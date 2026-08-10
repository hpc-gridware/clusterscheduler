/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of JAPI - Cluster Scheduler's API for job submission and control
 *
 * ## The session
 *
 * japi_init() opens a session and starts the **event client thread**
 * (japi_implementation_thread()). That thread registers as an event client at
 * qmaster, subscribes to the job events of this session - selected by the
 * session key, see japi_open_session() - and maintains
 * `Master_japi_job_list`, a local copy of the state of every job submitted
 * through the session. It is that list which lets japi_wait() and
 * japi_synchronize() block until a job has really finished instead of
 * polling qmaster.
 *
 * A session can also run **without** the event client thread. japi_init()
 * takes an `enable_wait` flag, and when it is false the submitting calls
 * work but japi_wait() and japi_synchronize() do not; japi_enable_job_wait()
 * starts the thread later.
 *
 * ## Threading
 *
 * All of the session state below is global and shared between the application
 * threads and the event client thread, so each variable has a mutex and the
 * `JAPI_LOCK_*` macros are how it is taken. `japi_threads_in_session` counts
 * the application threads currently inside a call that depends on
 * `Master_japi_job_list`, which is how japi_exit() knows when it may free it.
 *
 * ## Relation to DRMAA
 *
 * `drmaa.cc` implements the DRMAA specification on top of this file. Almost
 * every `drmaa_*` function checks its arguments, calls the `japi_*` function
 * of the same name and maps the result onto a DRMAA error code, so the
 * behaviour is described here and the specification conformance there.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <pthread.h>
#include <pwd.h>

#include "uti/ocs_cond.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge.h"
#include "uti/sge_stdlib.h"

#include "japi/drmaa.h"
#include "japi/japi.h"
#include "japi/msg_japi.h"
#include "japi/japiP.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_usage.h"

#include "cull/cull_list.h"

#include "comm/commlib.h"

#include "evc/sge_event_client.h"

#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_Client.h"

/** Guards the one time initialisation done by japi_once_init() */
static pthread_once_t japi_once_control = PTHREAD_ONCE_INIT;


/**
 * The event client thread. japi_init() starts it and japi_exit() joins it, so
 * this handle is how the two control its lifetime.
 */
static pthread_t japi_event_client_thread;

/* ---- japi_ec_return_value ------------------------------ */
/**
 * @brief The answer list the event client thread reports its result through
 *
 * The thread has no caller to return to, so it stores its diagnosis here and
 * japi_exit() picks it up.
 */
struct japi_ec_alp_data_t {
   lList*           japi_ec_alp;   ///< Answer list produced by the event client thread
   pthread_mutex_t  mutex;         ///< Guards `japi_ec_alp`
};
static struct japi_ec_alp_data_t japi_ec_alp_struct = {nullptr, PTHREAD_MUTEX_INITIALIZER };
/** Takes the mutex guarding the event client thread's answer list */
#define JAPI_LOCK_EC_ALP(japi_ec_alp_data_t)      sge_mutex_lock("EC_ALP", __func__, __LINE__, &(japi_ec_alp_data_t.mutex))
/** Releases the mutex taken by #JAPI_LOCK_EC_ALP */
#define JAPI_UNLOCK_EC_ALP(japi_ec_alp_data_t)    sge_mutex_unlock("EC_ALP", __func__, __LINE__, &(japi_ec_alp_data_t.mutex))

/* ---- japi_session --------------------------------- */

/**
 * @brief State of the JAPI session
 *
 * Read and written only under #JAPI_LOCK_SESSION.
 */
enum {
   JAPI_SESSION_ACTIVE,          ///< japi_init() succeeded, JAPI calls are allowed
   JAPI_SESSION_INITIALIZING,    ///< japi_init() is running, no other call may enter
   JAPI_SESSION_SHUTTING_DOWN,   ///< japi_exit() is running, no new call may enter
   JAPI_SESSION_INACTIVE         ///< No session, the state before japi_init() and after japi_exit()
};
/** State of the session, see #JAPI_SESSION_ACTIVE */
static int japi_session = JAPI_SESSION_INACTIVE;
/** Guards #japi_session and #japi_session_key */
static pthread_mutex_t japi_session_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Takes the mutex guarding the session state */
#define JAPI_LOCK_SESSION()      sge_mutex_lock("SESSION", __func__, __LINE__, &japi_session_mutex)
/** Releases the mutex taken by #JAPI_LOCK_SESSION */
#define JAPI_UNLOCK_SESSION()    sge_mutex_unlock("SESSION", __func__, __LINE__, &japi_session_mutex)

/* ---- japi_ec_state ------------------------------------- */

/**
 * @brief State of the event client thread
 *
 * japi_init() waits here for the thread to come up and japi_exit() for it to
 * go down, and every blocking call watches it so that it returns when the
 * thread dies - otherwise a japi_exit() in another thread would leave a
 * japi_wait() blocked forever.
 */
enum {
   JAPI_EC_DOWN,        ///< The thread is not running
   JAPI_EC_UP,          ///< The thread is registered at qmaster and delivering events
   JAPI_EC_RESTARTING,  ///< The thread lost its registration and is registering again
   JAPI_EC_STARTING,    ///< The thread was created and is registering
   JAPI_EC_FINISHING,   ///< The thread is shutting down
   JAPI_EC_FAILED       ///< The thread gave up, see the answer list in `japi_ec_alp_struct`
};

/** State of the event client thread, see #JAPI_EC_DOWN */
static int japi_ec_state = JAPI_EC_DOWN;

/** Guards #japi_ec_state */
static pthread_mutex_t japi_ec_state_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Takes the mutex guarding the event client thread state */
#define JAPI_LOCK_EC_STATE()      sge_mutex_lock("japi_ec_state_mutex", __func__, __LINE__, &japi_ec_state_mutex)
/** Releases the mutex taken by #JAPI_LOCK_EC_STATE */
#define JAPI_UNLOCK_EC_STATE()    sge_mutex_unlock("japi_ec_state_mutex", __func__, __LINE__, &japi_ec_state_mutex)

/**
 * Signalled on every transition of #japi_ec_state; japi_init() waits on it for
 * the event client thread to be up and running.
 */
static pthread_cond_t japi_ec_state_starting_cv = PTHREAD_COND_INITIALIZER;

/* ---- japi_ec_id ------------------------------------------ */
/**
 * Event client id assigned by qmaster. Written by the event client thread and
 * read by japi_exit() to deregister the client.
 */
static uint32_t japi_ec_id = 0;

/* ---- Master_japi_job_list -------------------------------- */
/**
 * @brief State of all jobs of this session
 *
 * japi_run_job() and japi_run_bulk_jobs() add the jobs they submit, the event
 * client thread stores the finish information, and japi_wait() and
 * japi_synchronize() remove a job again when it is reaped. This list is what
 * makes waiting for a job possible without polling qmaster.
 */
static lList *Master_japi_job_list = nullptr;

/** Guards `Master_japi_job_list` */
static pthread_mutex_t Master_japi_job_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Takes the mutex guarding `Master_japi_job_list` */
#define JAPI_LOCK_JOB_LIST()     sge_mutex_lock("Master_japi_job_list_mutex", __func__, __LINE__, &Master_japi_job_list_mutex)
/** Releases the mutex taken by #JAPI_LOCK_JOB_LIST */
#define JAPI_UNLOCK_JOB_LIST()   sge_mutex_unlock("Master_japi_job_list_mutex", __func__, __LINE__, &Master_japi_job_list_mutex)

/** Signalled by the event client thread each time a job or task has finished */
static pthread_cond_t Master_japi_job_list_finished_cv = PTHREAD_COND_INITIALIZER;

/* ---- japi_threads_in_session ------------------------------ */

/**
 * @brief Number of application threads currently inside a JAPI call
 *
 * Every call that touches `Master_japi_job_list` raises the counter on entry
 * and lowers it again on exit, see japi_inc_threads() and japi_dec_threads().
 * japi_exit() waits for it to reach zero before it frees the list.
 */
int japi_threads_in_session = 0;

/** Guards #japi_threads_in_session */
static pthread_mutex_t japi_threads_in_session_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Takes the mutex guarding #japi_threads_in_session */
#define JAPI_LOCK_REFCOUNTER()   sge_mutex_lock("japi_threads_in_session_mutex", __func__, __LINE__, &japi_threads_in_session_mutex)
/** Releases the mutex taken by #JAPI_LOCK_REFCOUNTER */
#define JAPI_UNLOCK_REFCOUNTER() sge_mutex_unlock("japi_threads_in_session_mutex", __func__, __LINE__, &japi_threads_in_session_mutex)

/** Signalled when #japi_threads_in_session drops to zero, awaited by japi_exit() */
static pthread_cond_t japi_threads_in_session_cv = PTHREAD_COND_INITIALIZER;

/* ---- globals ------------------------------------- */
/**
 * @brief Key identifying this session at qmaster
 *
 * Passed on event client registration so that the session sees only the job
 * events of its own jobs. Guarded by `japi_session_mutex` and assumed not to
 * change while a session is active.
 */
char *japi_session_key = nullptr;
/** Session key used when the application did not ask for a persistent session */
static const char *JAPI_SINGLE_SESSION_KEY = "JAPI_SSK";
/** Component name this library registers with, see japi_init() */
static volatile ProgName prog_number = JAPI;
/** The thread that called japi_init(); only it may call japi_exit() */
static pthread_t init_thread = 0;
/** Callback for the error messages of the event client thread, see #error_handler_t */
static error_handler_t error_handler = nullptr;
/**
 * Whether delegated file staging is enabled in the cluster configuration, -1
 * while unknown. Read it through japi_is_delegated_file_staging_enabled(),
 * which takes the mutex.
 */
static int japi_delegated_file_staging_is_enabled = -1;
/**
 * False once the first session of this process was opened. Only japi_init()
 * uses it, so it needs no mutex.
 */
static bool virgin_session = true;

/** Number of jobs japi_exit() deletes in one GDI request */
#define MAX_JOBS_TO_DELETE 500


static int japi_open_session(const char *username, const char *unqualified_hostname, const char *key_in, dstring *key_out, dstring *diag);
#ifdef ENABLE_PERSISTENT_JAPI_SESSIONS
static int japi_close_session(const char*username, const dstring *key, dstring *diag);
#endif
static void *japi_implementation_thread(void *);
static int japi_parse_jobid(const char *jobid_str, uint32_t *jobid, uint32_t *taskid,
   bool *is_array, dstring *diag);
static int japi_send_job(lListElem **job, uint32_t *jobid, dstring *diag);
static int japi_add_job(uint32_t jobid, uint32_t start, uint32_t end, uint32_t incr,
      bool is_array, dstring *diag);
static int japi_synchronize_jobids_retry(const char *jobids[], bool dispose);
static int japi_wait_retry(lList *japi_job_list, int wait4any, uint32_t jobid,
                           uint32_t taskid, bool is_array_task, int event_mask,
                           uint32_t *wjobidp, uint32_t *wtaskidp,
                           bool *wis_task_arrayp, int *wait_status,
                           int *wevent, lList **rusagep);
static int japi_gdi_control_error2japi_error(lListElem *aep, dstring *diag, int drmaa_control_action);
static int japi_sync_job_tasks(lListElem *japi_job, lListElem *sge_job);
static int japi_clean_up_jobs (int flag, dstring *diag);
static int japi_read_dynamic_attributes(dstring *diag);
static int do_gdi_delete (lList **id_list, int action, bool delete_all,
                          dstring *diag);
static int japi_stop_event_client(const char *default_cell);


static void japi_use_library_signals() {
   /* simply ignore SIGPIPE */
   signal (SIGPIPE, SIG_IGN);
}


static void japi_once_init() {
   /* enable rmon monitoring */
   rmon_mopen();
}


static void japi_inc_threads(const char *func) {
   DENTER(TOP_LAYER);
   JAPI_LOCK_REFCOUNTER();
   japi_threads_in_session++;
   DPRINTF("%s(): japi_threads_in_session++ %d\n", func, japi_threads_in_session);
   JAPI_UNLOCK_REFCOUNTER();
   DRETURN_VOID;
}

static void japi_dec_threads(const char *func) {
   DENTER(TOP_LAYER);
   JAPI_LOCK_REFCOUNTER();
   if (--japi_threads_in_session == 0)
      pthread_cond_signal(&japi_threads_in_session_cv);
   DPRINTF("%s(): japi_threads_in_session-- %d\n", func, japi_threads_in_session);
   JAPI_UNLOCK_REFCOUNTER();
   DRETURN_VOID;
}


/**
 * @brief Per thread library initialization
 *
 * Do all per thread initialization required for libraries JAPI builds
 * upon.
 *
 * @param diag returns diagnosis information - on error
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTES: japi_init_mt() is MT safe
 */
int japi_init_mt(dstring *diag) {
   DENTER(TOP_LAYER);

   lList *alp = nullptr;
   ocs::gdi::ErrorValue gdi_errno;

   /* current major assumptions are
      - code is not compiled with -DCRYPTO
      - code is not compiled with -DKERBEROS
      - neither AFS nor DCE/KERBEROS security may be used */

   /* as long as signal handling is not restored japi_init_mt() is
      good place to install library signal handling */
   japi_use_library_signals();

   /*
   ** TODO: return error reason in diag
   */
   gdi_errno = ocs::gdi::ClientBase::setup_and_enroll(prog_number, MAIN_THREAD, &alp);
   if ((gdi_errno != ocs::gdi::AE_OK) && (gdi_errno != ocs::gdi::AE_ALREADY_SETUP)) {
      answer_to_dstring(lFirst(alp), diag);
      lFreeList(&alp);
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }
   lFreeList(&alp);

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Initialize JAPI library
 *
 * Initialize JAPI library and create a new JAPI session. This
 * routine must be called before any other JAPI calls, except for
 * japi_version(). Initializes internal data structures.  Also registers
 * with qmaster using the event client mechanism if the enable_wait parameter
 * is set to true.  If enable_wait is set to false, japi_enable_job_wait()
 * must be called before calling japi_wait() or japi_synchronize().
 * If enable_wait is set to true, a second thread is spawned as an event client,
 * which imposes threading and synchronization overhead.  If japi_wait() and
 * japi_synchronize() are not needed, JAPI can be made much lighter weight
 * by setting enable_wait to false.
 *
 * @param contact 'Contact' is an implementation dependent string which may be used to specify which DRM system to use. If 'contact' is nullptr, the default DRM system will be used.
 * @param session_key_in if non nullptr japi_init() tries to restart a former session using this session key.
 * @param my_prog_num the index into prognames to use when registering with the qmaster.  See ocs::gdi::ClientBase::setup_and_enroll().
 * @param enable_wait Whether to start up in mutli-threaded mode to allow japi_wait() and japi_synchronize() to function. When true, a new session is created (if needed), and the event client thread is started.  When false, no session string is set, and the event client is not started. When false, japi_synchronize() and japi_wait() will return DRMAA_ERRNO_NO_ACTIVE_SESSION. If enable_wait is set to false, job waiting can be explicitly enabled later by calling the japi_enable_job_wait() function.
 * @param handler A callback to be used for error messages from the event client thread.  When enable_wait is false, handler should be set to nullptr.  The callback should not free the error message after processing it.
 * @param session_key_out Returns session key of new session - on success.
 * @param diag Returns diagnosis information - on failure
 *
 * @return DRMAA error codes
 *
 * @note japi_session_mutex
 *
 *       MT-NOTE: japi_init() is MT safe
 */
int japi_init(const char *contact, const char *session_key_in,
              dstring *session_key_out, ProgName my_prog_num, bool enable_wait,
              error_handler_t handler, dstring *diag) {
   DENTER(TOP_LAYER);

   int ret;
   cl_com_handle_t* handle = nullptr;

   ocs::uti::condition_initialize(&japi_ec_state_starting_cv);
   ocs::uti::condition_initialize(&Master_japi_job_list_finished_cv);
   ocs::uti::condition_initialize(&japi_threads_in_session_cv);

   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_INACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_ALREADY_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_ALREADY_ACTIVE_SESSION);
   }

   japi_session = JAPI_SESSION_INITIALIZING;

   /* Bugfix: Issuezilla 1076
    * We set this here so that the enable_job_wait() can be certain about
    * whether init called it or not. */
   init_thread = pthread_self();
   JAPI_UNLOCK_SESSION();

   pthread_once(&japi_once_control, japi_once_init);

   if (my_prog_num > 0) {
      prog_number = my_prog_num;
   }

   /* per thread initialization */
   if (japi_init_mt(diag) != DRMAA_ERRNO_SUCCESS) {
      japi_session = JAPI_SESSION_INACTIVE;
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* Bugfix: Issuezilla 1025
    * The problem is that the commlib handle was being created in japi_mt_init()
    * even when there was no actual need of a communications channel.  The
    * reason the handle is created at all is that if it is not, calling
    * japi_init() followed by japi_exit() followed by japi_init() again would
    * result in functions like japi_run_job() getting a dead handle.  Once the
    * handle is closed, it has to be explicitly reopened.  (Because this is
    * really only an init issue, it's safe to move this code in japi_init().)
    * The answer is to not create the handle the first time japi_init() is
    * called.  Since the handle hasn't been closed yet, it doesn't need to be
    * explicitly created.  If japi_init() gets called more than once, it's fair
    * to assume that later calls will be doing something more than just
    * initializing to prep for outputting usage information.  At least, that's
    * how it looks right now. */
   /* Besides, it looks like creating the handle wasn't the real problem.  The
    * real problem was the call to read_dynamic_attributes() from japi_init().
    * This bug fix is still a good idea, though. */
   /* No need to worry about locking for this global since it is only used in
    * japi_init(), and only one thread may be in japi_init() at a time. */
   if (!virgin_session) {
      lList *answer_list = nullptr;
      int commlib_error = CL_RETVAL_OK;
      const char *component_name = component_get_component_name();
      handle = cl_com_get_handle(component_name, 0);
      if (handle == nullptr) {
         /* check if master is alive */
         commlib_error = ocs::gdi::ClientBase::prepare_enroll(&answer_list);
         if (commlib_error == CL_RETVAL_OK) {
            commlib_error = ocs::gdi::ClientBase::gdi_is_alive(&answer_list);
         }
         handle = cl_com_get_handle(component_name, 0);
      }
      if (handle == nullptr) {
         sge_dstring_sprintf (diag, MSG_JAPI_NO_HANDLE_S, cl_get_error_text(commlib_error));
         DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
      }
   } else {
      virgin_session = false;
   }

   if (enable_wait) {
      const char *username = component_get_username();
      const char *unqualified_hostname = component_get_unqualified_hostname();

      /* spawn implementation thread japi_implementation_thread() */
      ret = japi_enable_job_wait(username, unqualified_hostname, session_key_in, session_key_out, handler, diag);
   } else {
      /* This doesn't need to be protected by a lock because by definition we
       * only get here if there are no other threads. */
      japi_session_key = (char *)JAPI_SINGLE_SESSION_KEY;
      ret = DRMAA_ERRNO_SUCCESS;
   }

   JAPI_LOCK_SESSION();
   if (ret == DRMAA_ERRNO_SUCCESS) {
      japi_session = JAPI_SESSION_ACTIVE;
   } else {
      japi_session = JAPI_SESSION_INACTIVE;
   }
   JAPI_UNLOCK_SESSION();

   DRETURN(ret);
}

/**
 * @brief Do setup required for doing job waits
 *
 * Does all of the required setup to be able to use the japi_wait() and
 * japi_synchronize() calls.  This includes starting up the event client
 * thread and establishing a session.
 * If japi_init() was called with enable_wait set to false, this method must
 * be called before japi_wait() or japi_synchronize() can be used.
 * This is useful if, for example, when one doesn't know for sure whether
 * japi_wait() will be needed at the time japi_init() is called.  The
 * overhead associated with starting and stopping the event client thread and
 * creating and destroying a session can thereby be avoided.
 *
 * @param[in]  username             Name of the user the session belongs to
 * @param[in]  unqualified_hostname Short name of the host the session runs on;
 *                                  together with `username` it identifies the
 *                                  owner of a session that is to be restarted
 * @param[in]  session_key_in       If non nullptr japi_enable_job_wait() tries
 *                                  to restart a former session using this
 *                                  session key
 * @param[in]  handler              Callback for the error messages of the
 *                                  event client thread. When nullptr, the
 *                                  thread generates no error messages. The
 *                                  callback must not free the message
 * @param[out] session_key_out      Returns the session key of the new session
 *                                  - on success
 * @param[out] diag                 Returns diagnosis information - on failure
 *
 * @return DRMAA error codes
 *
 * @note japi_session_mutex -> japi_ec_state_mutex
 *
 *       MT-NOTE: japi_enable_job_wait() is MT safe
 */
int japi_enable_job_wait(const char *username, const char *unqualified_hostname, const char *session_key_in, dstring *session_key_out,
                         error_handler_t handler, dstring *diag) {
   DENTER(TOP_LAYER);

   int i = 0;
   int ret = DRMAA_ERRNO_SUCCESS;
   pthread_attr_t attr;

   JAPI_LOCK_SESSION();
   /* JAPI_SESSION_INITIALIZING if we're called from japi_init() or
    * JAPI_SESSION_ACTIVE if we're called from the client code directly. */
   if (japi_session != JAPI_SESSION_INITIALIZING && japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }
   /* Bugfix: Issuezilla 1076
    * japi_init() sets init_thread to the calling thread's thread id.
    * That means that if the id doesn't match, japi_init() isn't the thread
    * that called this function. */
   /* When init_thread is set in japi_init(), it's guarded by the session
    * mutex, so we know there's no race condition here. */
   if (japi_session == JAPI_SESSION_INITIALIZING && init_thread != pthread_self()) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_ALREADY_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_ALREADY_ACTIVE_SESSION);
   }

   JAPI_LOCK_EC_STATE();
   if (japi_ec_state != JAPI_EC_DOWN) {
      JAPI_UNLOCK_EC_STATE();
      JAPI_UNLOCK_SESSION();
      sge_dstring_copy_string(diag, MSG_JAPI_EVENT_CLIENT_ALREADY_STARTED);
      /* If the state is not JAPI_EC_DOWN, return
       * DRMAA_ERRNO_ALREADY_ACTIVE_SESSION because we don't have a better
       * error code to return.  We really need to give JAPI it's own error
       * codes instead of leaning on DRMAA. */
      /* This also applies to finishing because the event client must already
       * be running to be stopping.  Ideally we would return a more specific
       * error code, but for the moment, this is the best I can do. */
      DRETURN(DRMAA_ERRNO_ALREADY_ACTIVE_SESSION);
   }

   /* Note that we're in the process of starting up so that other calls to this
    * function fail. */
   if (!session_key_in) {
      japi_ec_state = JAPI_EC_STARTING;
   } else {
      japi_ec_state = JAPI_EC_RESTARTING;
   }

   /* It's safe to unlock both locks here because we have the ec
    * state set so that no other functions can disturb us. */
   JAPI_UNLOCK_EC_STATE();
   JAPI_UNLOCK_SESSION();

   /* (re)open JAPI session associated with JAPI session key */
   ret = japi_open_session(username, unqualified_hostname, session_key_in, session_key_out, diag);

   if (ret != DRMAA_ERRNO_SUCCESS) {
      JAPI_LOCK_EC_STATE();
      japi_ec_state = JAPI_EC_DOWN;
      JAPI_UNLOCK_EC_STATE();

      /* diag was set by japi_open_session() */
      DRETURN(ret);
   }

   JAPI_LOCK_SESSION();
   if (japi_session_key == JAPI_SINGLE_SESSION_KEY) {
      /* japi_init() was called with enable_wait set to false */
      japi_session_key = strdup(sge_dstring_get_string(session_key_out));
   }
   else {
      /* japi_init() was called with enable_wait set to true */
      japi_session_key = sge_strdup(japi_session_key, sge_dstring_get_string(session_key_out));
   }
   JAPI_UNLOCK_SESSION();

   sge_dstring_free(session_key_out);

   /* Set handler for dealing with error messages from event client thread. */
   error_handler = handler;

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

   /* I'm locking the EC_STATE here so that there is no race condition with the
    * event client thread. */
   JAPI_LOCK_EC_STATE();
   DPRINTF("Waiting for event client to start up\n");

   i = pthread_create(&japi_event_client_thread, &attr,
                      japi_implementation_thread, nullptr);

   if (i != 0) {
      japi_ec_state = JAPI_EC_DOWN;
      JAPI_UNLOCK_EC_STATE();

      if (diag != nullptr) {
         sge_dstring_sprintf(diag, MSG_JAPI_EC_THREAD_NOT_STARTED_S,
                             strerror(errno));
      }

      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* wait until event client thread is operable or gives up */

   /* We wait for !JAPI_EC_STARTING here instead of JAPI_EC_UP because the
    * event client thread may not succeed in starting up. */
   while (japi_ec_state == JAPI_EC_STARTING || japi_ec_state == JAPI_EC_RESTARTING ) {
      pthread_cond_wait(&japi_ec_state_starting_cv, &japi_ec_state_mutex);
   }

   if (japi_ec_state == JAPI_EC_UP) {
      JAPI_UNLOCK_EC_STATE();

      DPRINTF("Event client has been started\n");
      ret = DRMAA_ERRNO_SUCCESS;
   }
   else if (japi_ec_state == JAPI_EC_FAILED) {
      const lListElem *aep = nullptr;

      japi_ec_state = JAPI_EC_DOWN;
      JAPI_UNLOCK_EC_STATE();

      ret = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
      if (pthread_join(japi_event_client_thread, nullptr)) {
         DPRINTF("japi_init(): pthread_join returned\n");
      }

      /* We know that japi_session_key is a copy of a string at this point. */
      sge_free(&japi_session_key);

      /* return error context from event client thread if there is such */
      JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
      aep = lFirst(japi_ec_alp_struct.japi_ec_alp);
      if (aep != nullptr) {
         answer_to_dstring(aep, diag);
      }
      else {
         japi_standard_error(DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE, diag);
      }
      JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
   }
   else {
      JAPI_UNLOCK_EC_STATE();
      /* else japi_ec_state == JAPI_EC_DOWN which means the thread was shut
       * down by japi_exit() before it could register as an event client.  In
       * this case, we just quietly exit as though everything worked, which
       * techincally it did.  We just triggered a shortcut that prevents the
       * event client thread from starting up completely just to be shut
       * down. */
      ret = DRMAA_ERRNO_SUCCESS;
   }

   pthread_attr_destroy(&attr);

   DRETURN(ret);
}


/**
 * @brief Create or reopen JAPI session
 *
 * A JAPI session is created or reopend, depending on the value of key_in.
 * The session key of the opend session is returned.
 *
 * @param key_in If 'key' is non nullptr it is used to reopen the JAPI session. Otherwise a new session is always created.
 * @param key_out Returns session key of the session that was opened on success.
 * @param diag Diagnosis information - on failure.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_open_session() is MT safe
 */
static int japi_open_session(const char *username, const char *unqualified_hostname, const char *key_in, dstring *key_out, dstring *diag) {
   DENTER(TOP_LAYER);

#ifdef ENABLE_PERSISTENT_JAPI_SESSIONS
   struct passwd pw_struct, *pwd;
   char buffer[2048];
   char tmp_session_path_buffer[SGE_PATH_MAX];
   dstring tmp_session_path;
#endif

   if (key_in == nullptr) {
      char tmp_session_key_buffer[SGE_PATH_MAX];
      dstring tmp_session_key;
      unsigned int id = 0;

      /* seed random function */
      id = sge_gmt64_to_gmt32(sge_get_gmt64());

      sge_dstring_init(&tmp_session_key, tmp_session_key_buffer, sizeof(tmp_session_key_buffer));

      /* a unique session key must be found if we got no session key */
      id = rand_r((unsigned int *)&id);

      /* a session key is built from <unqualified hostname>.<pid>.<number> */
      sge_dstring_sprintf(&tmp_session_key, "%s." pid_t_fmt ".%.6d",
                          unqualified_hostname, getpid(),
                          id);

      DPRINTF("created new session using generated \"%s\" as JAPI session key\n",
              sge_dstring_get_string(&tmp_session_key));
      sge_dstring_copy_dstring(key_out, &tmp_session_key);
   } else {
      sge_dstring_copy_string(key_out, key_in);
   }

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Optionally close JAPI session and shutdown JAPI library
 *
 * Disengage from JAPI library and allow the JAPI library to perform
 * any necessary internal clean up, and end the JAPI session. Whether the
 * jobs of the session survive is decided by `flag`; with
 * #JAPI_EXIT_NO_FLAG queued and running jobs remain queued and running.
 *
 * @param[in]  flag What happens to the jobs of the session, one of
 *                  #JAPI_EXIT_NO_FLAG, #JAPI_EXIT_KILL_ALL or
 *                  #JAPI_EXIT_KILL_PENDING
 * @param[out] diag diagnosis information - on error
 *
 * @return DRMAA error codes
 *
 * @note japi_session_mutex -> japi_threads_in_session_mutex
 *
 *       MT-NOTE: japi_exit() is MT safe
 */
int japi_exit(int flag, dstring *diag) {
   DENTER(TOP_LAYER);

   int cl_errno;
   cl_com_handle_t* handle = nullptr;
   const char *default_cell = nullptr;

   DPRINTF("entering japi_exit() at " sge_u64"\n", sge_get_gmt64());

   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   japi_session = JAPI_SESSION_SHUTTING_DOWN;
   JAPI_UNLOCK_SESSION();

   /* be sure that the context exists, therefore after test for active session */
   default_cell = bootstrap_get_sge_cell();

   /* do not destroy session state until last japi call
      depending on it is finished */
   JAPI_LOCK_REFCOUNTER();

   if (japi_threads_in_session > 0) {
      /* signal all application threads waiting for a job to finish */
      pthread_cond_broadcast(&Master_japi_job_list_finished_cv);

      while (japi_threads_in_session > 0) {
         pthread_cond_wait(&japi_threads_in_session_cv, &japi_threads_in_session_mutex);
      }
   }

   JAPI_UNLOCK_REFCOUNTER();

   /* per thread initialization */
   if (japi_init_mt(diag) != DRMAA_ERRNO_SUCCESS) {
      japi_session = JAPI_SESSION_INACTIVE;
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* Here's how this stop process works:
    * o Kill any pending jobs
    * o Wait for the event client thread to die
    * o Close the comm lib connection
    * o Free the job list
    * o Close the session
    */

   /* First we clean up the pending job(s). */
   japi_clean_up_jobs (flag, diag);

   /*
    * notify event client about shutdown
    *
    * Currently this is done by using the sge_gsi_kill_eventclient() call.  As
    * a backup, we also set japi_ec_state accordingly.
    */
   JAPI_LOCK_EC_STATE();
   DPRINTF("Notify event client about shutdown\n");
   if ((japi_ec_state == JAPI_EC_UP) || (japi_ec_state == JAPI_EC_STARTING) || (japi_ec_state == JAPI_EC_RESTARTING)) {
      int my_state = japi_ec_state;
      /* If the event client thread is running, it will check the state at the
       * beginning of every cycle.  If the state is set to JAPI_EC_FINISHING
       * it will exit. */
      /* If the event client thread is still starting up, we can shotcut its
       * start-up by setting the state to JAPI_EC_FINISHING without having to
       * first let it come up and then bring it down. */
      japi_ec_state = JAPI_EC_FINISHING;
      JAPI_UNLOCK_EC_STATE();

      if (my_state == JAPI_EC_UP) {
         japi_stop_event_client(default_cell);
      }

      DPRINTF (("Waiting for event client to terminate.\n"));
      pthread_join (japi_event_client_thread, nullptr);
      japi_ec_state = JAPI_EC_DOWN;
   } else {
      JAPI_UNLOCK_EC_STATE();
   }

   /* If it's down, we're fine.  It can't be finishing because only one
    * thread can be in japi_exit() at a time. */

   /* Make certain nothing is still hanging around. */
   pthread_cond_broadcast (&japi_ec_state_starting_cv);

   /*
    * Try to disconnect from commd
    * this will fail if the thread never made any commd communiction
    *
    * When DRMAA calls were made by multiple threads other
    * sge_commd commprocs remain registered. To unregister also
    * these commprocs a list of open commprocs per process is
    * required to implement kind of a all_thread_leave_commproc().
    * This function would then be called here instead.
    */
   /* There's two ways for us to get here.  The first is that we successfully
    * unregistered the event client and signaled the event client thread.  In
    * case, it's possible that the connection is in an unstable state.  However,
    * we don't actually care because all we're doing is closing it, and if the
    * thread was holding any locks, they were released when it died.  The second
    * way is for the unregister to have failed.  In this case, we had to
    * ask the GDI to ask the event client to shutdown.  If we've gotten here,
    * the event client thread is stopped and no further communications are
    * needed. */

   /*
    * disconnect from commd
    */
   DPRINTF (("Before commlib shutdown\n"));
   handle = cl_com_get_handle(component_get_component_name(), 0);
   cl_errno = cl_commlib_shutdown_handle(handle, false);
   if (cl_errno != CL_RETVAL_OK) {
      sge_dstring_sprintf(diag, MSG_JAPI_CANNOT_CLOSE_COMMLIB_S, cl_get_error_text(cl_errno));
   }
   DPRINTF (("After commlib shutdown\n"));

   /* We have to wait to free the job list until any waiting or syncing threads
    * have exited.  Otherwise, they may think their jobs exited badly. */
   JAPI_LOCK_JOB_LIST();
   lFreeList(&Master_japi_job_list);
   JAPI_UNLOCK_JOB_LIST();

   /* Session is not inactive until the session has been closed (or not).  If
    * I set the session to inactive earlier, another japi_init() could try to
    * open the same session before we could close it.  The result would be that
    * the session would be successfully opened by japi_init() and then quietly
    * closed by japi_exit() causing all kinds of headaches.
    * The same goes for the communications socket. */
   JAPI_LOCK_SESSION();
   if (japi_session_key != JAPI_SINGLE_SESSION_KEY) {
      sge_free(&japi_session_key);
   }
   else {
      japi_session_key = nullptr;
   }

   japi_session = JAPI_SESSION_INACTIVE;
   JAPI_UNLOCK_SESSION();

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Allocate a string vector
 *
 * Allocate a string vector iterator. Two different variations are
 * supported:
 *    JAPI_ITERATOR_BULK_JOBS
 *        Provides bulk job id strings in a memory efficient fashion.
 *    JAPI_ITERATOR_STRINGS
 *        Implements a simple string list.
 *
 * @param type JAPI_ITERATOR_BULK_JOBS or JAPI_ITERATOR_STRINGS
 *
 * @return the iterator
 *
 * @note MT-NOTE: japi_allocate_string_vector() is MT safe
 *       should be moved to drmaa.c
 */
drmaa_attr_values_t *japi_allocate_string_vector(int type) {
   drmaa_attr_values_t *iter;

   if (!(iter = (drmaa_attr_values_t *)sge_malloc(sizeof(drmaa_attr_values_t)))) {
      return nullptr;
   }
   iter->iterator_type = type;

   switch (type) {
   case JAPI_ITERATOR_BULK_JOBS:
      iter->it.ji.jobid    = 0;
      iter->it.ji.start    = 0;
      iter->it.ji.end      = 0;
      iter->it.ji.incr     = 0;
      iter->it.ji.next_pos = 0;
      break;
   case JAPI_ITERATOR_STRINGS:
      iter->it.si.strings = nullptr;
      iter->it.si.next_pos = nullptr;
      break;
   default:
      sge_free(&iter);
   }

   return iter;
}

/**
 * @brief Return next entry of a string vector
 *
 * DRMAA_ERRNO_NO_MORE_ELEMENTS is returned for an empty string
 * vector. The next entry of a string vector is returned.
 *
 * @param iter The string vector
 * @param val Returns next string value - on success.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_string_vector_get_next() is MT safe
 */
int japi_string_vector_get_next(drmaa_attr_values_t *iter, dstring *val) {
   DENTER(TOP_LAYER);

   if (!iter) {
      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   switch (iter->iterator_type) {
   case JAPI_ITERATOR_BULK_JOBS:
      if (iter->it.ji.next_pos > iter->it.ji.end) {
         DRETURN(DRMAA_ERRNO_NO_MORE_ELEMENTS);
      }
      if (val != nullptr) {
         sge_dstring_sprintf(val, "%ld.%d", iter->it.ji.jobid, iter->it.ji.next_pos);
      }

      iter->it.ji.next_pos += iter->it.ji.incr;
      DRETURN(DRMAA_ERRNO_SUCCESS);
   case JAPI_ITERATOR_STRINGS:
      if (!iter->it.si.next_pos) {
         DRETURN(DRMAA_ERRNO_NO_MORE_ELEMENTS);
      }

      if (val != nullptr) {
         sge_dstring_copy_string(val, lGetString(iter->it.si.next_pos, ST_name));
      }

      iter->it.si.next_pos = lNextRW(iter->it.si.next_pos);
      DRETURN(DRMAA_ERRNO_SUCCESS);
   default:
      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }
}

/**
 * @brief Return number of entries of a string
 *
 * Returns the total number of elements in the string vector.
 *
 * @param[in]  values The string vector
 * @param[out] size   Returns the number of entries
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_string_vector_get_num() is MT safe
 */
int japi_string_vector_get_num(drmaa_attr_values_t *values, int *size) {
   DENTER(TOP_LAYER);

   if ((values == nullptr) || (size == nullptr)) {
      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   switch (values->iterator_type) {
      case JAPI_ITERATOR_BULK_JOBS:
         /* 1-7:3 = [1,4,7] => (7 - 1) / 3 + 1 = 3 */
         *size = (values->it.ji.end - values->it.ji.start) / values->it.ji.incr + 1;
         DRETURN(DRMAA_ERRNO_SUCCESS);
      case JAPI_ITERATOR_STRINGS:
         *size = lGetNumberOfElem(values->it.si.strings);
         DRETURN(DRMAA_ERRNO_SUCCESS);
      default:
         DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }
}

/**
 * @brief Release all resources of a string vector
 *
 * Release all resources of a string vector.
 *
 * @param iter to be released
 *
 * @note MT-NOTE: japi_delete_string_vector() is MT safe
 *       should be moved to drmaa.c
 */
void japi_delete_string_vector(drmaa_attr_values_t *iter) {
   if (!iter)
      return;

   switch (iter->iterator_type) {
   case JAPI_ITERATOR_BULK_JOBS:
      break;
   case JAPI_ITERATOR_STRINGS:
      lFreeList(&(iter->it.si.strings));
      break;
   default:
      break;
   }
   sge_free(&iter);

   return;
}

/**
 * @brief Send job to qmaster using GDI
 *
 * The job passed is sent to qmaster using GDI. The jobid is returned.
 *
 * @param job the job (JB_Type)
 * @param jobid destination for resulting jobid
 * @param diag diagnosis information
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_send_job() is MT safe
 */
static int japi_send_job(lListElem **sge_job_template, uint32_t *jobid, dstring *diag) {
   DENTER(TOP_LAYER);
   lList *job_lp, *alp;
   lListElem *job;
   int result = DRMAA_ERRNO_SUCCESS;


   job_lp = lCreateList(nullptr, JB_Type);
   job = lCopyElem(*sge_job_template);
   lAppendElem(job_lp, job);

   /*
    * Set owner and group so that information will be available in
    * client JSV scripts
    */
   int amount;
   ocs_grp_elem_t *grp_array;
   component_get_supplementray_groups(&amount, &grp_array);
   job_set_owner_and_group(job, component_get_uid(), component_get_gid(),
                           component_get_username(), component_get_groupname(),
                           amount, grp_array);

   /* use GDI to submit job for this session */
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::ADD,
                 ocs::gdi::SubCommand::RETURN_NEW_VERSION, &job_lp, nullptr, nullptr);

   /* reinitialize 'job' with pointer to new version from qmaster */
   lFreeElem(sge_job_template);
   if ((*sge_job_template = lFirstRW(job_lp))) {
      *jobid = lGetUlong(*sge_job_template, JB_job_number);
   }

   lDechainElem(job_lp, *sge_job_template);
   lFreeList(&job_lp);

   if (!lFirst(alp)) {
      lFreeList(&alp);
      sge_dstring_copy_string(diag, MSG_JAPI_BAD_GDI_ANSWER_LIST);
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /*
    *  We simply put all answer messages into the diag buffer.
    *  Each single answer message is at first added without newline
    *  characters. Then a newline is added to delimit two messages.
    */
   for_each_ep_lv(aep, alp) {
      uint32_t quality;
      quality = lGetUlong(aep, AN_quality);

      if (quality == ANSWER_QUALITY_ERROR) {
         uint32_t answer_status = lGetUlong(aep, AN_status);

         if ((answer_status == STATUS_NOQMASTER) ||
             (answer_status == STATUS_NOCOMMD)) {
            result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
         } else if (answer_status == STATUS_NOTOK_DOAGAIN) {
            result = DRMAA_ERRNO_TRY_LATER;
         } else {
            result = DRMAA_ERRNO_DENIED_BY_DRM;
         }
      }

      answer_to_dstring(aep, diag);
      if (lNext(aep)) {
         sge_dstring_append(diag, "\n");
      }
   }
   lFreeList(&alp);

   DRETURN(result);
}


/**
 * @brief Add job/bulk job to library session data
 *
 * Add the job/bulk job to the library session data.
 *
 * @param jobid the jobid
 * @param start start index
 * @param end end index
 * @param incr increment
 * @param is_array true for array/bulk jobs false otherwise
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTES: japi_add_job() is not MT safe due to
 *       Master_japi_job_list
 */
static int japi_add_job(uint32_t jobid, uint32_t start, uint32_t end, uint32_t incr,
                        bool is_array, dstring *diag) {
   DENTER(TOP_LAYER);

   lListElem *japi_job;

   japi_job = lGetElemUlongRW(Master_japi_job_list, JJ_jobid, jobid);
   if (japi_job != nullptr) {
      /* job may not yet exist */
      sge_dstring_sprintf(diag, MSG_JAPI_JOB_ALREADY_EXISTS_S, jobid);
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* add job to library session data
      -  all tasks in JJ_not_yet_finished_ids
      -  no task in JJ_finished_jobs */
   japi_job = lAddElemUlong(&Master_japi_job_list, JJ_jobid, jobid, JJ_Type);
   object_set_range_id(japi_job, JJ_not_yet_finished_ids, start, end, incr);

   /* mark it as array job */
   if (is_array) {
      uint32_t job_type;
      job_type = lGetUlong(japi_job, JJ_type);
      JOB_TYPE_SET_ARRAY(job_type);
      lSetUlong(japi_job, JJ_type, job_type);
   }

   DRETURN(DRMAA_ERRNO_SUCCESS);
}


/**
 * @brief Submit a job using a SGE job template
 *
 * The job described in the SGE job template is submitted. The id
 * of the job is returned.
 *
 * @param sge_job_template SGE job template. Might be modified by JSV
 * @param job_id SGE jobid as string - on success.
 * @param diag diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note japi_session_mutex -> japi_threads_in_session_mutex
 *       Master_japi_job_list_mutex
 *       japi_threads_in_session_mutex
 *
 *       MT-NOTE: japi_run_job() is MT safe
 *       Would be better to return job_id as uint32_t.
 */
int japi_run_job(dstring *job_id, lListElem **sge_job_template, dstring *diag) {
   DENTER(TOP_LAYER);

   uint32_t jobid = 0;
   int drmaa_errno;
   const char *s;

   /* ensure japi_init() was called */
   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   /* ensure job list still is consistent when we add the job id of the submitted job later on */
   japi_inc_threads(__func__);

   JAPI_UNLOCK_SESSION();

   /* per thread initialization */
   if (japi_init_mt(diag) != DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_init_mt() */
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* tag job with JAPI session key */
   lSetString(*sge_job_template, JB_session, japi_session_key);

   JAPI_LOCK_JOB_LIST();

   /* send job to qmaster using GDI */
   drmaa_errno = japi_send_job(sge_job_template, &jobid, diag);
   if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
      JAPI_UNLOCK_JOB_LIST();
      japi_dec_threads(__func__);
      /* diag written by japi_send_job() */
      DRETURN(drmaa_errno);
   }

   /* add job array to library session data */
   drmaa_errno = japi_add_job(jobid, 1, 1, 1, false, diag);

   JAPI_UNLOCK_JOB_LIST();

   /* this is just a dirty hook for testing purposes
      need this to enforce certain error conditions */
   if ((s=getenv("SGE_DELAY_AFTER_SUBMIT"))) {
      int seconds = atoi(s);
      DPRINTF("sleeping %d seconds\n", seconds);
      sleep(seconds);
      DPRINTF("slept %d seconds\n", seconds);
   }

   japi_dec_threads(__func__);
   if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
      /* diag written by japi_add_job() */
      DRETURN(drmaa_errno);
   }

   /* return jobid as string */
   if (job_id)
      sge_dstring_sprintf(job_id, "%ld", jobid);

   DRETURN(DRMAA_ERRNO_SUCCESS);
}


/**
 * @brief Submit a bulk of jobs
 *
 * Submit the SGE job template as array job.
 *
 * @param[in]  sge_job_template SGE job template
 * @param[in]  start            array job start index
 * @param[in]  end              array job end index
 * @param[in]  incr             array job increment
 * @param[out] jobidsp          a string array of jobids - on success
 * @param[out] diag             Returns diagnosis information - on error
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_run_bulk_jobs() is MT safe
 *       Would be better to return job_id instead of drmaa_attr_values_t.
 */
int japi_run_bulk_jobs(drmaa_attr_values_t **jobidsp, lListElem **sge_job_template,
                       int start, int end, int incr, dstring *diag) {
   DENTER(TOP_LAYER);

   drmaa_attr_values_t *jobids;
   uint32_t jobid = 0;
   int drmaa_errno;

   /* check arguments */
   if (start > end || !incr) {
      japi_standard_error(DRMAA_ERRNO_INVALID_ARGUMENT, diag);
      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   /* ensure japi_init() was called */
   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   japi_inc_threads(__func__);

   JAPI_UNLOCK_SESSION();

   /* per thread initialization */
   if (japi_init_mt(diag)!=DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_drmaa_job2sge_job() */
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* tag job with JAPI session key */
   if (japi_session_key != nullptr) {
      lSetString(*sge_job_template, JB_session, japi_session_key);
   }

   JAPI_LOCK_JOB_LIST();

   /* send job to qmaster using GDI */
   drmaa_errno = japi_send_job(sge_job_template, &jobid, diag);
   if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
      JAPI_UNLOCK_JOB_LIST();
      japi_dec_threads(__func__);
      /* diag written by japi_send_job() */
      DRETURN(drmaa_errno);
   }

   /* add job array to library session data */
   drmaa_errno = japi_add_job(jobid, start, end, incr, true, diag);

   JAPI_UNLOCK_JOB_LIST();

   japi_dec_threads(__func__);
   if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
      /* diag written by japi_add_job() */
      DRETURN(drmaa_errno);
   }

   if (!(jobids = japi_allocate_string_vector(JAPI_ITERATOR_BULK_JOBS))) {
      japi_standard_error(DRMAA_ERRNO_NO_MEMORY, diag);
      DRETURN(DRMAA_ERRNO_NO_MEMORY);
   }

   /* initialize jobid iterator to be returned */
   jobids->it.ji.jobid    = jobid;
   jobids->it.ji.start    = start;
   jobids->it.ji.end      = end;
   jobids->it.ji.incr     = incr;
   jobids->it.ji.next_pos = start;

   /* return jobids */
   *jobidsp = jobids;

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Helper function for composing GDI request
 *
 * Adds a reduced job structure to the request list that causes the job/task
 * be hold/released when it is used with ocs::gdi::Client::sge_gdi(SGE_JB_LIST, SGE_GDI_MOD).
 *
 * @param gdi_action the GDI action to be performed
 * @param request_list the request list we operate on
 * @param jobid the jobid
 * @param taskid the taskid
 * @param array true in case of an arry job
 * @param diag diagnosis information in case of an error
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_user_hold_add_jobid() is MT safe
 */
static int japi_user_hold_add_jobid(uint32_t gdi_action, lList **request_list,
                                    uint32_t jobid, uint32_t taskid, bool array,
                                    dstring *diag) {
   DENTER(TOP_LAYER);

   const lDescr job_descr[] = {
         {JB_job_number, lUlongT | CULL_IS_REDUCED, nullptr},
         {JB_verify_suitable_queues, lUlongT | CULL_IS_REDUCED, nullptr},
         {JB_ja_tasks, lListT | CULL_IS_REDUCED, nullptr},
         {JB_ja_structure, lListT | CULL_IS_REDUCED, nullptr},
         {NoName, lEndT | CULL_IS_REDUCED, nullptr}
   };
   const lDescr task_descr[] = {
         {JAT_task_number, lUlongT | CULL_IS_REDUCED, nullptr},
         {JAT_hold, lUlongT | CULL_IS_REDUCED, nullptr},
         {NoName, lEndT | CULL_IS_REDUCED, nullptr}
   };
   lListElem *jep = nullptr;
   lListElem *tep = nullptr;

   if (!array) {
      taskid = 0;
   }

   /* ensure JB_Type structure exists */
   jep = lGetElemUlongRW(*request_list, JB_job_number, jobid);

   if (jep == nullptr) {
      jep = lAddElemUlong(request_list, JB_job_number, jobid, job_descr);
   }

   /* ensure JAT_Type structure exists */
   if (lGetSubUlong(jep, JAT_task_number, taskid, JB_ja_tasks) != nullptr) {
      /* taskid is referenced twice */
      if (diag != nullptr) {
         sge_dstring_sprintf(diag, MSG_JAPI_TASK_REF_TWICE_UU,
               taskid, jobid);
      }

      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   tep = lAddSubUlong(jep, JAT_task_number, taskid, JB_ja_tasks, task_descr);

   /* set action */
   lSetUlong(tep, JAT_hold, gdi_action);

   if (taskid != 0) {
      lList *tlp = nullptr;
      lXchgList(jep, JB_ja_structure, &tlp);
      range_list_insert_id(&tlp, nullptr, taskid);
      lXchgList(jep, JB_ja_structure, &tlp);
   }

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Apply control operation on JAPI jobs
 *
 * Apply control operation to the job specified. If 'jobid' is
 * DRMAA_JOB_IDS_SESSION_ALL, then this routine acts on all jobs
 * *submitted* during this DRMAA session.
 * This routine returns once the action has been acknowledged, but
 * does not necessarily wait until the action has been completed.
 *
 * @param[in]  jobid_str    The job id or DRMAA_JOB_IDS_SESSION_ALL
 * @param[in]  drmaa_action The action to be performed, one of
 *                          #DRMAA_CONTROL_SUSPEND (stop the job, `qmod -s`),
 *                          #DRMAA_CONTROL_RESUME (restart it, `qmod -us`),
 *                          #DRMAA_CONTROL_HOLD (put it on hold, `qhold`),
 *                          #DRMAA_CONTROL_RELEASE (release the hold, `qrls`) or
 *                          #DRMAA_CONTROL_TERMINATE (kill it, `qdel`)
 * @param[out] diag         Returns diagnosis information - on error
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_control() is MT safe
 *       Would be good to have japi_control() operate on a vector of jobids.
 *       Would be good to interface also operations qmod -r and qmod -c.
 */
int japi_control(const char *jobid_str, int drmaa_action, dstring *diag) {
   DENTER(TOP_LAYER);

   int drmaa_errno;
   uint32_t jobid, taskid;
   bool array;
   lList *alp = nullptr;

   /* ensure japi_init() was called */
   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   japi_inc_threads(__func__);

   JAPI_UNLOCK_SESSION();

   /* per thread initialization */
   if (japi_init_mt(diag)!=DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_drmaa_job2sge_job() */
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* use GDI to implement control operations */
   switch (drmaa_action) {
   case DRMAA_CONTROL_SUSPEND:
   case DRMAA_CONTROL_RESUME:
      {
         lList *ref_list = nullptr;

         if (!strcmp(jobid_str, DRMAA_JOB_IDS_SESSION_ALL)) {
            JAPI_LOCK_JOB_LIST();
            for_each_ep_lv(japi_job, Master_japi_job_list) {
               jobid = lGetUlong(japi_job, JJ_jobid);
               if (!JOB_TYPE_IS_ARRAY(lGetUlong(japi_job, JJ_type))) {
                  char buffer[1024];
                  dstring job_task_specifier;

                  sge_dstring_init(&job_task_specifier, buffer, sizeof(buffer));
                  sge_dstring_sprintf(&job_task_specifier, sge_u32, jobid);
                  lAddElemStr(&ref_list, ST_name, sge_dstring_get_string (&job_task_specifier), ST_Type);
               } else {
                  for_each_ep_lv(range, lGetList(japi_job, JJ_not_yet_finished_ids)) {
                     char buffer[1024];
                     dstring job_task_specifier;
                     uint32_t start, end, step;

                     sge_dstring_init(&job_task_specifier, buffer, sizeof(buffer));
                     sge_dstring_sprintf(&job_task_specifier, sge_u32 ".", jobid);
                     range_get_all_ids(range, &start, &end, &step);
                     range_to_dstring(start, end, step, &job_task_specifier, false, false, false);
                     lAddElemStr(&ref_list, ST_name, sge_dstring_get_string(&job_task_specifier), ST_Type);
                  }
               }
            }
            JAPI_UNLOCK_JOB_LIST();
         } else {
            /* just ensure jobid can be parsed */
            if (japi_parse_jobid(jobid_str, &jobid, &taskid, &array, diag)) {
               japi_dec_threads(__func__);
               /* diag written by japi_parse_jobid() */
               lFreeList(&ref_list);
               DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
            }
            lAddElemStr(&ref_list, ST_name, jobid_str, ST_Type);
         }

         if (ref_list) {
            lList *id_list = nullptr;

            if (drmaa_action == DRMAA_CONTROL_SUSPEND) {
               id_list_build_from_str_list(&id_list, &alp, ref_list,
                                           QI_DO_SUSPEND, 0);
            } else {
               id_list_build_from_str_list(&id_list, &alp, ref_list,
                                           QI_DO_UNSUSPEND, 0);
            }
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::TRIGGER,
                          ocs::gdi::SubCommand::NONE, &id_list, nullptr, nullptr);
            lFreeList(&id_list);
            lFreeList(&ref_list);

            for_each_rw_lv(aep, alp) {
               if (lGetUlong(aep, AN_status) != STATUS_OK) {
                  int ret = DRMAA_ERRNO_SUCCESS;

                  japi_dec_threads(__func__);
                  ret = japi_gdi_control_error2japi_error(aep, diag, drmaa_action);
                  lFreeList(&alp);

                  DRETURN(ret);
               }
            }
            lFreeList(&alp);
         }
      }
      break;

   case DRMAA_CONTROL_HOLD:
   case DRMAA_CONTROL_RELEASE:
      {
         lList *alp = nullptr;
         lList *request_list = nullptr;
         uint32_t gdi_action;

         /* set action */
         if (drmaa_action == DRMAA_CONTROL_HOLD) {
            gdi_action = MINUS_H_TGT_USER|MINUS_H_CMD_ADD;
         }
         else {
            gdi_action = MINUS_H_TGT_USER|MINUS_H_CMD_SUB;
         }

         if (strcmp(jobid_str, DRMAA_JOB_IDS_SESSION_ALL) == 0) {
            JAPI_LOCK_JOB_LIST();
            for_each_ep_lv(japi_job, Master_japi_job_list) {
               jobid = lGetUlong(japi_job, JJ_jobid);

               if (!JOB_TYPE_IS_ARRAY(lGetUlong(japi_job, JJ_type))) {
                  drmaa_errno = japi_user_hold_add_jobid(gdi_action,
                                                         &request_list,
                                                         jobid, 0, false, diag);

                  if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
                     /* diag written by japi_user_hold_add_jobid() */
                     JAPI_UNLOCK_JOB_LIST();
                     japi_dec_threads(__func__);
                     lFreeList(&request_list);
                     DRETURN(drmaa_errno);
                  }
               } else {
                  for_each_ep_lv(range, lGetList(japi_job, JJ_not_yet_finished_ids)) {
                     uint32_t min, max, step;

                     range_get_all_ids(range, &min, &max, &step);

                     for (taskid = min; taskid <= max; taskid += step) {
                        drmaa_errno = japi_user_hold_add_jobid(gdi_action,
                                                               &request_list,
                                                               jobid,
                                                               taskid,
                                                               true,
                                                               diag);

                        if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
                           /* diag written by japi_user_hold_add_jobid() */
                           JAPI_UNLOCK_JOB_LIST();
                           japi_dec_threads(__func__);
                           lFreeList(&request_list);
                           DRETURN(drmaa_errno);
                        }
                     }
                  }
               }
            }
            JAPI_UNLOCK_JOB_LIST();
         } else {
            if (japi_parse_jobid(jobid_str, &jobid, &taskid, &array, diag)) {
               japi_dec_threads(__func__);
               /* diag written by japi_parse_jobid() */
               lFreeList(&request_list);
               DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
            }

            drmaa_errno = japi_user_hold_add_jobid(gdi_action, &request_list,
                                                   jobid, taskid, array, diag);

            if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
               japi_dec_threads(__func__);
               /* diag written by japi_user_hold_add_jobid() */
               lFreeList(&request_list);
               DRETURN(drmaa_errno);
            }
         }

         if (request_list) {
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::MOD,
                          ocs::gdi::SubCommand::NONE, &request_list, nullptr, nullptr);
            lFreeList(&request_list);

            for_each_rw_lv(aep, alp) {
               if (lGetUlong(aep, AN_status) != STATUS_OK) {
                  int ret = DRMAA_ERRNO_SUCCESS;

                  japi_dec_threads(__func__);
                  ret = japi_gdi_control_error2japi_error(aep, diag, drmaa_action);
                  lFreeList(&alp);

                  DRETURN(ret);
               }
            }

            lFreeList(&alp);
         }
      }
      break;

   case DRMAA_CONTROL_TERMINATE:
      {
         lList *id_list = nullptr;
         lListElem *id_entry;

         if (strcmp(jobid_str, DRMAA_JOB_IDS_SESSION_ALL) == 0) {
            bool done = false;
            int count = 0;
            char buffer[1024];
            dstring job_task_specifier;
            const lListElem *japi_job = nullptr;

            sge_dstring_init(&job_task_specifier, buffer, sizeof(buffer));

            JAPI_LOCK_JOB_LIST();
            japi_job = lFirst(Master_japi_job_list);

            while (!done) {
               count = 0;

               while (japi_job != nullptr) {
                  jobid = lGetUlong(japi_job, JJ_jobid);
                  /* This overwrites the previous contents of the dstring. */
                  sge_dstring_sprintf(&job_task_specifier, sge_u32, jobid);

                  id_entry = lAddElemStr(&id_list, ID_str, sge_dstring_get_string(&job_task_specifier), ID_Type);
                  if (JOB_TYPE_IS_ARRAY(lGetUlong(japi_job, JJ_type))) {
                     lSetList(id_entry, ID_ja_structure, lCopyList(nullptr, lGetList(japi_job, JJ_not_yet_finished_ids)));
                  }

                  /* japi_job starts out as the first element in the master job
                   * list.  Every time through this loop, we move to the next
                   * element.  We do this before the check for maximum num of
                   * jobs to delete so that the next time we come to this loop,
                   * japi_job will already point to the right job.  This saves
                   * us some initializer logic before the loop. */
                  japi_job = lNext (japi_job);

                  /* Stop when we reach the deletion limit. */
                  if (++count >= MAX_JOBS_TO_DELETE) {
                     break;
                  }
               } /* while */

               /* If we exhausted the list before reaching the job limit, we're
                * done. */
               if (count < MAX_JOBS_TO_DELETE) {
                  DPRINTF("Deleting %d jobs\n", count);
                  done = true;
               }
               else {
                  DPRINTF("Deleting %d jobs\n", MAX_JOBS_TO_DELETE);
               }

               if (id_list) {
                  int ret = DRMAA_ERRNO_SUCCESS;
                  lList *idlp = nullptr;

                  /* Look for jobs from any user. */
                  for_each_rw_lv(idp, id_list) {
                     idlp = lGetListRW(idp, ID_user_list);

                     if (idlp == nullptr) {
                        idlp = lCreateList ("User List", ST_Type);
                        lSetList(idp, ID_user_list, idlp);
                     }

                     lAddElemStr(&idlp, ST_name, "*", ST_Type);
                  }

                  /* This function frees id_list */
                  ret = do_gdi_delete(&id_list, drmaa_action, true, diag);

                  if (ret != DRMAA_ERRNO_SUCCESS) {
                     JAPI_UNLOCK_JOB_LIST();
                     japi_dec_threads(__func__);
                     lFreeList(&id_list);
                     DRETURN(ret);
                  }
               } /* if */
            } /* while */
            JAPI_UNLOCK_JOB_LIST();
         } /* if */
         else {
            char buffer[1024];
            dstring job_task_specifier;
            sge_dstring_init(&job_task_specifier, buffer, sizeof(buffer));

            if (japi_parse_jobid(jobid_str, &jobid, &taskid, &array, diag)) {
               japi_dec_threads(__func__);
               /* diag written by japi_parse_jobid() */
               lFreeList(&id_list);
               DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
            }

            sge_dstring_sprintf(&job_task_specifier, sge_u32, jobid);
            id_entry = lAddElemStr(&id_list, ID_str, sge_dstring_get_string(&job_task_specifier), ID_Type);
            if (array) {
               lList *tlp = nullptr;
               lXchgList(id_entry, ID_ja_structure, &tlp);
               range_list_insert_id(&tlp, nullptr, taskid);
               lXchgList(id_entry, ID_ja_structure, &tlp);
            }
         } /* else */

         if (id_list) {
            int ret = DRMAA_ERRNO_SUCCESS;
            lList *idlp = nullptr;

            /* Look for jobs from any user. */
            for_each_rw_lv(idp, id_list) {
               idlp = lGetListRW(idp, ID_user_list);

               if (idlp == nullptr) {
                  idlp = lCreateList ("User List", ST_Type);
                  lSetList(idp, ID_user_list, idlp);
               }

               lAddElemStr(&idlp, ST_name, "*", ST_Type);
            }

            /* This function frees id_list */
            ret = do_gdi_delete(&id_list, drmaa_action, false, diag);

            if (ret != DRMAA_ERRNO_SUCCESS) {
               japi_dec_threads(__func__);
               lFreeList(&id_list);
               DRETURN(ret);
            }
         } /* if */
      }
      break;

   default:
      japi_dec_threads(__func__);
      japi_standard_error(DRMAA_ERRNO_INVALID_ARGUMENT, diag);
      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   japi_dec_threads(__func__);

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Outcome of one attempt of the retry helpers to reap a task
 *
 * Returned by japi_wait_retry() and the japi_synchronize_*_retry() functions
 * to tell their caller whether it has to wait for the next event or can
 * return to the application.
 */
enum {
   JAPI_WAIT_ALLFINISHED, ///< There is nothing more to wait for
   JAPI_WAIT_UNFINISHED,  ///< There are still unfinished tasks
   JAPI_WAIT_FINISHED,    ///< A finished task was found and reaped
   JAPI_WAIT_INVALID,     ///< The specified task does not exist
   JAPI_WAIT_TIMEOUT      ///< The timeout expired before the condition was met
};

static int japi_gdi_control_error2japi_error(lListElem *aep, dstring *diag, int drmaa_control_action) {
   DENTER(TOP_LAYER);

   int ret, gdi_error;

   answer_to_dstring(aep, diag);
   switch ((gdi_error=lGetUlong(aep, AN_status))) {
   case STATUS_EEXIST:
      ret = DRMAA_ERRNO_INVALID_JOB;
      break;
   case STATUS_EDENIED2HOST:
   case STATUS_ENOMGR:
   case STATUS_ENOOPR:
   case STATUS_ENOTOWNER:
      ret = DRMAA_ERRNO_AUTH_FAILURE;
      break;
   case STATUS_NOQMASTER:
   case STATUS_NOCOMMD:
      ret = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
      break;
   case STATUS_ESEMANTIC:
      switch (drmaa_control_action) {
      case DRMAA_CONTROL_SUSPEND:
         ret = DRMAA_ERRNO_SUSPEND_INCONSISTENT_STATE;
         break;
      case DRMAA_CONTROL_RESUME:
         ret = DRMAA_ERRNO_RESUME_INCONSISTENT_STATE;
         break;
      case DRMAA_CONTROL_HOLD:
         ret = DRMAA_ERRNO_HOLD_INCONSISTENT_STATE;
         break;
      case DRMAA_CONTROL_RELEASE:
         ret = DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE;
         break;
      case DRMAA_CONTROL_TERMINATE:
         /* job termination never fails due to the wrong job state */
         ret = DRMAA_ERRNO_INVALID_JOB;
         break;
      default:
         ret = DRMAA_ERRNO_INTERNAL_ERROR;
         break;
      }
      break;
   default:
      ret = DRMAA_ERRNO_INTERNAL_ERROR;
      break;
   }
   DPRINTF("mapping GDI error code %d to DRMAA error code %d\n", gdi_error, ret);
   DRETURN(ret);
}

/**
 * @brief Synchronize with jobs to finish w/ and w/o reaping
 *
 * Wait until all jobs specified by 'job_ids' have finished
 * execution. When DRMAA_JOB_IDS_SESSION_ALL is used as jobid
 * one can synchronize with all jobs that were submitted during this
 * JAPI session. A timeout can be specified to prevent blocking
 * indefinitely. If the call exits before timeout all the jobs have
 * been waited on or there was an interrupt. If the invocation exits
 * on timeout, the return code is DRMAA_ERRNO_EXIT_TIMEOUT. The dispose
 * parameter specifies whether job finish information shall be reaped.
 * This method requires the event client to have been started, either by
 * passing enable_wait as true to japi_init() or by calling
 * japi_enable_job_wait().
 *
 * @param[in]  job_ids  nullptr terminated list of job ids to wait for, or a
 *                      list whose only entry is DRMAA_JOB_IDS_SESSION_ALL to
 *                      wait for all jobs of the session
 * @param[in]  timeout  timeout in seconds, #DRMAA_TIMEOUT_WAIT_FOREVER for
 *                      infinite waiting or #DRMAA_TIMEOUT_NO_WAIT to return
 *                      immediately
 * @param[in]  dispose  Whether job finish information shall be reaped
 * @param[out] diag     Diagnosis information - on error
 *
 * @return DRMAA error codes
 *
 * @note japi_session_mutex -> japi_threads_in_session_mutex
 *
 *       MT-NOTE: japi_synchronize() is MT safe
 *       The caller must check system time before and after this call
 *       in order to check how much time has passed. This should be improved.
 */
int japi_synchronize(const char *job_ids[], signed long timeout, bool dispose, dstring *diag) {
   DENTER(TOP_LAYER);

   bool sync_all = false;
   int drmaa_errno, i;
   int wait_result;
   const char **sync_job_ids = nullptr;
   lList *sync_list = nullptr;

   if (timeout < DRMAA_TIMEOUT_WAIT_FOREVER) {
      sge_dstring_sprintf (diag, MSG_JAPI_NEGATIVE_TIMEOUT);

      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   /* ensure japi_init() was called */
   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   JAPI_LOCK_EC_STATE();
   if (japi_ec_state != JAPI_EC_UP) {
      JAPI_UNLOCK_EC_STATE();
      JAPI_UNLOCK_SESSION();
      sge_dstring_copy_string(diag, MSG_JAPI_NO_EVENT_CLIENT);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }
   JAPI_UNLOCK_EC_STATE();

   /* ensure job list still is consistent when we wait jobs later on */
   japi_inc_threads(__func__);

   JAPI_UNLOCK_SESSION();

   /* per thread initialization */
   if (japi_init_mt(diag)!=DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_drmaa_job2sge_job() */
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* wait(?) until specified jobs have finished according to library session data */

   /* synchronize with *all* jobs submitted during this session ? */
   for (i=0; job_ids[i] != nullptr; i++) {
      if (!strcmp(job_ids[i], DRMAA_JOB_IDS_SESSION_ALL)) {
         sync_all = true;
         break;
      }
      else {
         if ((drmaa_errno=japi_parse_jobid(job_ids[i], nullptr, nullptr, nullptr,
                     diag))!=DRMAA_ERRNO_SUCCESS) {
            japi_dec_threads(__func__);
            /* diag written by japi_parse_jobid() */
            DRETURN(drmaa_errno);
         }
      }
   }

   JAPI_LOCK_JOB_LIST();

   /* If we're synchronizing against all jobs, make a new job id list from the
    * running jobs in the master job list. */
   if (sync_all) {
      uint32_t id = 0;
      int count = 0;
      sync_list = lCreateList ("Synchronize Job List", ST_Type);

      for_each_ep_lv(ep, Master_japi_job_list) {
         const lList *not_yet_finished = nullptr;
         const lListElem *range;
         uint32_t task_id = 0;
         uint32_t min = 0;
         uint32_t max = 0;
         uint32_t step = 0;

         not_yet_finished = lGetList(ep, JJ_not_yet_finished_ids);

         /* If we're supposed to dispose of the job info, we need to include
          * all jobs currently in the session.  If we're not disposing, though,
          * we can save time by not including completed jobs. */
         if (!dispose) {
            /* If there are task ids not yet finished, this job is running or
             * waiting to be run, and we want to wait for it.  Otherwise, we
             * don't. */
            if (lGetNumberOfElem(not_yet_finished) == 0) {
               continue;
            }
         }

         id = lGetUlong(ep, JJ_jobid);

         /* handle all unfinished tasks */
         for_each_ep(range, not_yet_finished) {
            range_get_all_ids(range, &min, &max, &step);

            for (task_id = min; task_id <= max; task_id += step) {
               /* The largest number representable by 64 unsigned bits is 19
                * characters long. */
               char char_id[40];
               snprintf (char_id, 40, sge_u32 "." sge_u32, id, task_id);

               DPRINTF ("Synchronize All: adding %s to id list\n", char_id);
               lAddElemStr (&sync_list, ST_name, char_id, ST_Type);
            }
         }
      }

      /* We have to add one to the size of the array for the nullptr terminator. */
      sync_job_ids = (const char**)sge_malloc (sizeof (char *) * (lGetNumberOfElem (sync_list) + 1));
      SGE_ASSERT(sync_job_ids != nullptr);

      for_each_ep_lv(ep, sync_list) {
         sync_job_ids[count] = lGetString (ep, ST_name);
         count++;
      }

      sync_job_ids[count] = nullptr;
   }
   /* Otherwise just use the id list we were passed. */
   else {
      sync_job_ids = job_ids;
   }

   while ((wait_result = japi_synchronize_jobids_retry(sync_job_ids, dispose) == JAPI_WAIT_UNFINISHED)) {

      /* must return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE when event client
         thread was shutdown during japi_wait() use japi_session */
      /* has japi_exit() been called meanwhile ? */
      JAPI_LOCK_SESSION();
      if (japi_session != JAPI_SESSION_ACTIVE) {
         JAPI_UNLOCK_SESSION();
         JAPI_UNLOCK_JOB_LIST();
         japi_dec_threads(__func__);
         japi_standard_error(DRMAA_ERRNO_EXIT_TIMEOUT, diag);

         /* If we created a new job list, we have to free it. */
         if (sync_all) {
            /* The sync_job_ids is an array of pointers to the ST_name elements
             * of the sync_list.  That means that if we free the sync_list, we
             * don't have to free the individual elements of the
             * sync_job_ids. */
            lFreeList(&sync_list);
            sge_free(&sync_job_ids);
         }

         DRETURN(DRMAA_ERRNO_EXIT_TIMEOUT);
      }
      JAPI_UNLOCK_SESSION();

      if (timeout != DRMAA_TIMEOUT_WAIT_FOREVER) {
         if (ocs::uti::condition_timedwait(&Master_japi_job_list_finished_cv,
                  &Master_japi_job_list_mutex, timeout) == ETIMEDOUT) {
            DPRINTF("got a timeout while waiting for job(s) to finish\n");
            wait_result = JAPI_WAIT_TIMEOUT;
            break;
         }
      } else {
         pthread_cond_wait(&Master_japi_job_list_finished_cv, &Master_japi_job_list_mutex);
      }
   }

   JAPI_UNLOCK_JOB_LIST();

   if (wait_result == JAPI_WAIT_TIMEOUT)
      drmaa_errno = DRMAA_ERRNO_EXIT_TIMEOUT;
   else
      drmaa_errno = DRMAA_ERRNO_SUCCESS;

   japi_dec_threads(__func__);

   /* If we created a new job list, we have to free it. */
   if (sync_all) {
      /* The sync_job_ids is an array of pointers to the ST_name elements of the
       * sync_list.  That means that if we free the sync_list, we don't have to
       * free the individual elements of the sync_job_ids. */
      lFreeList(&sync_list);
      sge_free(&sync_job_ids);
   }

   DRETURN(drmaa_errno);
}

/**
 * @brief Look whether particular jobs finished
 *
 * The Master_japi_job_list is searched to investigate whether particular
 * jobs specified in job_ids finshed. If dispose is true job finish
 * information is also removed during this operation.
 *
 * @param dispose should job finish information be removed
 *
 * @return JAPI_WAIT_ALLFINISHED = there is nothing more to wait for JAPI_WAIT_UNFINISHED  = there are still unfinished tasks
 *
 * @note japi_synchronize_jobids_retry() does no error checking with the job_ids
 *       passed. Assumption is this was ensured before japi_synchronize_jobids_retry()
 *       is called.
 *       MT-NOTE: due to acess to Master_japi_job_list japi_synchronize_jobids_retry()
 *       MT-NOTE: is not MT safe; only one instance may be called at a time!
 */
static int japi_synchronize_jobids_retry(const char *job_ids[], bool dispose) {
   DENTER(TOP_LAYER);

   int i;
   lListElem *japi_job;
   const lList *not_yet_finished;

   /*
    * We simply iterate over all jobids and do the wait operation
    * for each of them.
    */
   for (i=0; job_ids[i] != nullptr; i++) {
      uint32_t jobid, taskid;
      bool is_array;

      /* assumption is all job_ids can be parsed w/o error by japi_parse_jobid()
         this must be ensured before japi_synchronize_jobids_retry() is called */
      japi_parse_jobid(job_ids[i], &jobid, &taskid, &is_array, nullptr);

      japi_job = lGetElemUlongRW(Master_japi_job_list, JJ_jobid, jobid);

      if (japi_job == nullptr) {
         DPRINTF("synchronized with " sge_u32 "." sge_u32"\n", jobid, taskid);
         continue;
      }

      not_yet_finished = lGetList(japi_job, JJ_not_yet_finished_ids);
      if (not_yet_finished && range_list_is_id_within(not_yet_finished, taskid)) {
         DPRINTF("job " sge_u32 "." sge_u32" is a still unfinished task\n", jobid, taskid);
         DRETURN(JAPI_WAIT_UNFINISHED);
      }

      DPRINTF("synchronized with " sge_u32 "." sge_u32 "\n", jobid, taskid);
      if (dispose) {
         /* remove corresponding entry in JJ_finished_tasks */
         lDelSubUlong(japi_job, JJAT_task_id, taskid, JJ_finished_tasks);
         DPRINTF("dispose job finish information for job " sge_u32 " task " sge_u32 "\n", jobid, taskid);
         if (!lGetList(japi_job, JJ_finished_tasks) && !not_yet_finished) {
            /* remove JAPI job if no longer needed */
            lRemoveElem(Master_japi_job_list, &japi_job);
         }
      }
   }

   DRETURN(JAPI_WAIT_ALLFINISHED);
}


/**
 * @brief Wait for job(s) to finish and reap job finish info
 *
 * This routine waits for a job with job_id to fail or finish execution. Passing a special string
 * DRMAA_JOB_IDS_SESSION_ANY instead job_id waits for any job. If such a job was
 * successfully waited its job_id is returned as a second parameter. This routine is
 * modeled on wait3 POSIX routine. To prevent
 * blocking indefinitely in this call the caller could use timeout specifying
 * after how many seconds to time out in this call.
 * If the call exits before timeout the job has been waited on
 * successfully or there was an interrupt.
 * If the invocation exits on timeout, the return code is DRMAA_ERRNO_EXIT_TIMEOUT.
 * The caller should check system time before and after this call
 * in order to check how much time has passed.
 * The routine reaps jobs on a successful call, so any subsequent calls
 * to japi_wait() should fail returning an error DRMAA_ERRNO_INVALID_JOB meaning
 * that the job has been already reaped. This error is the same as if the job was
 * unknown. Failing due to an elapsed timeout has an effect that it is possible to
 * issue japi_wait() multiple times for the same job_id.
 * This method requires the event client to have been started, either by
 * passing enable_wait as true to japi_init() or by calling
 * japi_enable_job_wait().
 *
 * @param[in]  job_id     job id of the job to wait for, or
 *                        DRMAA_JOB_IDS_SESSION_ANY to wait for any job
 * @param[in]  timeout    timeout in seconds, #DRMAA_TIMEOUT_WAIT_FOREVER for
 *                        infinite waiting or #DRMAA_TIMEOUT_NO_WAIT to return
 *                        immediately
 * @param[in]  event_mask Which events to listen for: #JAPI_JOB_START,
 *                        #JAPI_JOB_FINISH or the two or-ed together
 * @param[out] waited_job returns the job id of the job that was waited for
 * @param[out] stat       returns the job finish information - exit status,
 *                        signal and the like - to be decoded with
 *                        japi_wifexited() and friends
 * @param[out] event      returns the event that actually occurred. When
 *                        `event_mask` includes #JAPI_JOB_START this has to be
 *                        checked: a rejected immediate job makes japi_wait()
 *                        return DRMAA_ERRNO_SUCCESS for a #JAPI_JOB_FINISH
 *                        event even though only #JAPI_JOB_START was asked for
 * @param[out] rusage     returns the resource usage of the job run, when
 *                        waiting for #JAPI_JOB_FINISH
 * @param[out] diag       diagnosis information in case japi_wait() fails
 *
 * @return DRMAA_ERRNO_SUCCESS Job finished. DRMAA_ERRNO_EXIT_TIMEOUT No job end within specified time. DRMAA_ERRNO_INVALID_JOB The job id specified was invalid or DRMAA_JOB_IDS_SESSION_ANY has been specified and all jobs of this session have already finished. DRMAA_ERRNO_NO_ACTIVE_SESSION No active session. DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE DRMAA_ERRNO_AUTH_FAILURE DRMAA_ERRNO_NO_RUSAGE
 *
 * @note japi_session_mutex -> japi_threads_in_session_mutex
 *       Master_japi_job_list_mutex -> japi_ec_state_mutex
 *
 *       MT-NOTE: japi_wait() is MT safe
 *       Would be good to also return information about job failures in
 *       JJAT_failed_text.
 *       Would be good to enhance japi_wait() in a way allowing not only to
 *       wait for job finish events but also other events that have an meaning
 *       for the end user, e.g. job scheduled, job started, job rescheduled.
 */
int japi_wait(const char *job_id, dstring *waited_job, int *stat,
              signed long timeout, int event_mask, int *event,
              drmaa_attr_values_t **rusage, dstring *diag) {
   DENTER(TOP_LAYER);

   uint32_t jobid = 0;
   uint32_t taskid = 0;
   int wait4any = 0;
   bool is_array_task = false;
   int drmaa_errno, wait_result;
   bool waited_is_task_array = false;
   uint32_t waited_jobid = 0, waited_taskid = 0;
   bool got_usage_info = false;
   bool evc_killed = false;

   if (timeout < DRMAA_TIMEOUT_WAIT_FOREVER) {
      sge_dstring_sprintf (diag, MSG_JAPI_NEGATIVE_TIMEOUT);

      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   /* ensure japi_init() was called */
   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   JAPI_LOCK_EC_STATE();
   if (japi_ec_state != JAPI_EC_UP) {
      JAPI_UNLOCK_EC_STATE();
      JAPI_UNLOCK_SESSION();
      sge_dstring_copy_string(diag, MSG_JAPI_NO_EVENT_CLIENT);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }
   JAPI_UNLOCK_EC_STATE();

   /* ensure job list still is consistent when we wait jobs later on */
   japi_inc_threads(__func__);

   JAPI_UNLOCK_SESSION();

   /* per thread initialization */
   if (japi_init_mt(diag)!=DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_init_mt() */
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   /* check wait conditions */
   if (!strcmp(job_id, DRMAA_JOB_IDS_SESSION_ANY)) {
      wait4any = 1;
   }
   else {
      wait4any = 0;
      if ((drmaa_errno = japi_parse_jobid(job_id, &jobid, &taskid, &is_array_task, diag))
                           != DRMAA_ERRNO_SUCCESS) {
         japi_dec_threads(__func__);
         /* diag written by japi_parse_jobid() */
         DRETURN(drmaa_errno);
      }
   }

   {
      lList *rusagep = nullptr;

      JAPI_LOCK_JOB_LIST();

      while ((wait_result = japi_wait_retry(Master_japi_job_list, wait4any, jobid,
                                          taskid, is_array_task, event_mask, &waited_jobid,
                                          &waited_taskid, &waited_is_task_array,
                                          stat, event, &rusagep)) == JAPI_WAIT_UNFINISHED) {

         const lListElem *aep = nullptr;
         /* has japi_exit() been called meanwhile ? */
         JAPI_LOCK_SESSION();
         if (japi_session != JAPI_SESSION_ACTIVE) {
            JAPI_UNLOCK_SESSION();
            JAPI_UNLOCK_JOB_LIST();
            japi_dec_threads(__func__);
            japi_standard_error(DRMAA_ERRNO_EXIT_TIMEOUT, diag);
            DRETURN(DRMAA_ERRNO_EXIT_TIMEOUT); /* could also return something else here */
         }
         JAPI_UNLOCK_SESSION();

         JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
         aep = lFirst(japi_ec_alp_struct.japi_ec_alp);
         /* return error context from event client thread if there is such */
         if (aep != nullptr) {
            sge_dstring_clear(diag);
            answer_to_dstring(aep, diag);
            evc_killed = true;
            JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
            break;
         }
         JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);

         if (timeout != DRMAA_TIMEOUT_WAIT_FOREVER) {
            if (ocs::uti::condition_timedwait(&Master_japi_job_list_finished_cv,
                     &Master_japi_job_list_mutex, timeout) == ETIMEDOUT) {
               DPRINTF("got a timeout while waiting for job(s) to finish\n");
               wait_result = JAPI_WAIT_TIMEOUT;
               break;
            }
         } else {
            pthread_cond_wait(&Master_japi_job_list_finished_cv, &Master_japi_job_list_mutex);
         }
      } /* while */

      JAPI_UNLOCK_JOB_LIST();

      /* Build a drmaa_attr_values_t from the rusage list */
      if ((event_mask & JAPI_JOB_FINISH) && (rusage != nullptr)) {
         lList *slp = nullptr;
         lListElem *sep = nullptr;
         char buffer[256];

         if (rusagep != nullptr) {
            slp = lCreateList ("Usage List", ST_Type);
            got_usage_info = true;

            *rusage = japi_allocate_string_vector (JAPI_ITERATOR_STRINGS);

            for_each_ep_lv(uep, rusagep) {
               sep = lCreateElem (ST_Type);
               lAppendElem (slp, sep);

               snprintf(buffer, sizeof(buffer), "%s=%.4f", lGetString (uep, UA_name), lGetDouble (uep, UA_value));
               lSetString (sep, ST_name, buffer);
            }

            (*rusage)->iterator_type = JAPI_ITERATOR_STRINGS;
            (*rusage)->it.si.strings = slp;
            (*rusage)->it.si.next_pos = lFirstRW(slp);
         }
      }

      japi_dec_threads(__func__);

      lFreeList(&rusagep);
   }

   if (wait_result == JAPI_WAIT_INVALID) {
      japi_standard_error(DRMAA_ERRNO_INVALID_JOB, diag);
      DRETURN(DRMAA_ERRNO_INVALID_JOB);
   }
   if (wait_result == JAPI_WAIT_TIMEOUT) {
      japi_standard_error(DRMAA_ERRNO_EXIT_TIMEOUT, diag);
      DRETURN(DRMAA_ERRNO_EXIT_TIMEOUT);
   }

   /* copy jobid of finished job into buffer provided by caller */
   if (wait_result==JAPI_WAIT_FINISHED && waited_job) {
      if (waited_is_task_array) {
         sge_dstring_sprintf(waited_job, "%ld.%d", waited_jobid, waited_taskid);
      } else {
         sge_dstring_sprintf(waited_job, "%ld", waited_jobid);
      }
   }

   if (wait_result != JAPI_WAIT_FINISHED) {
      if (evc_killed) {
         DRETURN(DRMAA_ERRNO_INVALID_JOB);
      }
      japi_standard_error(DRMAA_ERRNO_INVALID_JOB, diag);
      DRETURN(DRMAA_ERRNO_INVALID_JOB);
   }

   if ((event_mask & JAPI_JOB_FINISH) && (rusage != nullptr) && !got_usage_info) {
      japi_standard_error (DRMAA_ERRNO_NO_RUSAGE, diag);
      DRETURN(DRMAA_ERRNO_NO_RUSAGE);
   }
   else {
      DRETURN(DRMAA_ERRNO_SUCCESS);
   }
}

/**
 * @brief Seek for job_id in JJ_finished_jobs of all jobs
 *
 * Search the passed japi_job_list for finished jobs matching the wait4any/
 * jobid/taskid condition.
 *
 * @param japi_job_list The JJ_Type japi joblist that is searched.
 * @param wait4any 0 any finished job/task is fine
 * @param jobid specifies which job is searched
 * @param taskid specifies which task is searched
 * @param is_array_task true if it is an array taskid
 * @param event_mask the events to wait for
 * @param wjobidp destination for jobid of waited job
 * @param wtaskidp destination for taskid of waited job
 * @param wis_task_arrayp destination for taskid of waited job
 * @param wait_status destination for status that is finally returned by japi_wait()
 * @param wevent destination for actual event received
 * @param rusagep desitnation for rusage info of waited job
 *
 * @return JAPI_WAIT_ALLFINISHED = there is nothing more to wait for JAPI_WAIT_UNFINISHED  = no job/task finished, but there are still unfinished tasks JAPI_WAIT_FINISHED    = got a finished task
 *
 * @note MT-NOTE: japi_wait_retry() is MT safe
 */
static int japi_wait_retry(lList *japi_job_list, int wait4any, uint32_t jobid,
                           uint32_t taskid, bool is_array_task, int event_mask,
                           uint32_t *wjobidp, uint32_t *wtaskidp,
                           bool *wis_task_arrayp, int *wait_status, int *wevent,
                           lList **rusagep) {
   DENTER(TOP_LAYER);

   lListElem *job = nullptr;
   lListElem *task = nullptr;
   int actual_event = 0;
   int return_value = JAPI_WAIT_UNFINISHED;

   /* seek for job_id in JJ_finished_jobs of all jobs */
   if (event_mask & JAPI_JOB_FINISH) {
      if (wait4any) {
         int not_yet_reaped = 0;

         for_each_rw(job, japi_job_list) {
            task = lFirstRW(lGetList(job, JJ_finished_tasks));

            if (task != nullptr) {
               break;
            }

            /* This comes after the break because if we have a non-nullptr task,
             * we don't bother looking at not_yet_reaped. */
            if (lGetList(job, JJ_not_yet_finished_ids) != nullptr) {
               not_yet_reaped = 1;
            }
         }

         if ((task == nullptr) || (job == nullptr)) {
            if (not_yet_reaped) {
               return_value = JAPI_WAIT_UNFINISHED;
            } else {
               return_value = JAPI_WAIT_ALLFINISHED;
            }
         }
         else {
            return_value = JAPI_WAIT_FINISHED;
         }
      } /* if wait4any */
      else {
         job = lGetElemUlongRW(japi_job_list, JJ_jobid, jobid);
         if (job == nullptr) {
            return_value = JAPI_WAIT_ALLFINISHED;
         }
         else {
            /* for non-array jobs no task id may have been specified */
            if (!JOB_TYPE_IS_ARRAY(lGetUlong(job, JJ_type)) && taskid != 1) {
               return_value = JAPI_WAIT_INVALID;
            }
            else {
               task = lGetSubUlongRW(job, JJAT_task_id, taskid, JJ_finished_tasks);
               if (!task) {
                  if (range_list_is_id_within(lGetList(job, JJ_not_yet_finished_ids), taskid)) {
                     return_value = JAPI_WAIT_UNFINISHED;
                  } else {
                     return_value = JAPI_WAIT_ALLFINISHED;
                  }
               }
               else {
                  return_value = JAPI_WAIT_FINISHED;
               }
            }
         }
      }
   }

   if (return_value != JAPI_WAIT_UNFINISHED) {
      *wevent = JAPI_JOB_FINISH;
      actual_event = JAPI_JOB_FINISH;
   }
   else if (event_mask & JAPI_JOB_START) {
      if (wait4any) {
         bool still_running = false;
         bool failed = false;

         for_each_rw (job, japi_job_list) {
            /* If there's a task in the started list, that counts. */
            if (lFirst (lGetList (job, JJ_started_task_ids)) != nullptr) {
               break;
            }
            /* A task in the finished list when the started list is empty counts
             * as a failure. */
            else if (lFirst (lGetList (job, JJ_finished_tasks)) != nullptr) {
               failed = true;
               break;
            }

            /* A task in the not yet finished list means we wait. */
            if (lGetList(job, JJ_not_yet_finished_ids) != nullptr) {
               still_running = true;
            }
         }

         if (failed) {
            return_value = JAPI_WAIT_FINISHED;
            *wevent = JAPI_JOB_FINISH;
            actual_event = JAPI_JOB_START;
         }
         else if ((job == nullptr) && still_running) {
            return_value = JAPI_WAIT_UNFINISHED;
         }
         else if (job == nullptr) {
            return_value = JAPI_WAIT_ALLFINISHED;
            *wevent = JAPI_JOB_START;
            actual_event = JAPI_JOB_START;
         }
         else {
            return_value = JAPI_WAIT_FINISHED;
            *wevent = JAPI_JOB_START;
            actual_event = JAPI_JOB_START;
         }
      }
      else {
         job = lGetElemUlongRW(japi_job_list, JJ_jobid, jobid);

         if (!job) {
            return_value = JAPI_WAIT_ALLFINISHED;
            *wevent = JAPI_JOB_START;
            actual_event = JAPI_JOB_START;
         }
         else {
            /* for non-array jobs no task id may have been specified */
            if (!JOB_TYPE_IS_ARRAY(lGetUlong(job, JJ_type)) && taskid != 1) {
               return_value = JAPI_WAIT_INVALID;
            }
            else {
               if (range_list_is_id_within(lGetList(job, JJ_started_task_ids), taskid)) {
                  return_value = JAPI_WAIT_FINISHED;
                  *wevent = JAPI_JOB_START;
                  actual_event = JAPI_JOB_START;
               }
               else if (!range_list_is_id_within (lGetList (job, JJ_not_yet_finished_ids), taskid)) {
                  task = lGetSubUlongRW(job, JJAT_task_id, taskid, JJ_finished_tasks);

                  if (task == nullptr) {
                     return_value = JAPI_WAIT_ALLFINISHED;
                     *wevent = JAPI_JOB_START;
                     actual_event = JAPI_JOB_START;
                  }
                  else {
                     /* This is a special case.  If the task makes it into the
                      * finished list without making it into the started list,
                      * the job was rejected before being started.  In this case
                      * there's no need to wait any longer, so we return
                      * JAPI_WAIT_FINISHED, but we set the wevent to
                      * JAPI_JOB_FINISH to show that it wasn't the job start
                      * event that caused the wait to end. */
                     return_value = JAPI_WAIT_FINISHED;
                     *wevent = JAPI_JOB_FINISH;
                     actual_event = JAPI_JOB_START;
                  }
               }
               else {
                  return_value = JAPI_WAIT_UNFINISHED;
               }
            }
         }
      }
   }

   if (return_value != JAPI_WAIT_FINISHED) {
      DRETURN(return_value);
   }

   /* return all kinds of job finish information */
   *wjobidp = lGetUlong(job, JJ_jobid);
   if (JOB_TYPE_IS_ARRAY(lGetUlong(job, JJ_type))) {
      *wis_task_arrayp = true;

      /* For the job start event, the task is nullptr at this point */
      if (actual_event == JAPI_JOB_START) {
         *wtaskidp = 1;
      }
      else {
         *wtaskidp = lGetUlong(task, JJAT_task_id);
      }
   } else {
      *wis_task_arrayp = false;
   }

   if (actual_event == JAPI_JOB_FINISH) {
      if (wait_status) {
         *wait_status = lGetUlong(task, JJAT_stat);
      }

      if (rusagep != nullptr) {
         const lList *usage = lGetList (task, JJAT_rusage);

         if (usage != nullptr) {
            lList *usage_copy = lCopyList ("Usage List", usage);

            if (*rusagep == nullptr) {
               *rusagep = usage_copy;
            }
            else {
               lAddList(*rusagep, &usage_copy);
            }
         }
      }
   }

   if (*wevent == JAPI_JOB_FINISH) {
      /* remove reaped jobs from library session data */
      lRemoveElem(lGetListRW(job, JJ_finished_tasks), &task);
      if (range_list_is_empty(lGetList(job, JJ_not_yet_finished_ids))
         && lGetNumberOfElem(lGetList(job, JJ_finished_tasks))==0) {
         lRemoveElem(Master_japi_job_list, &job);
      }
   }

   DRETURN(JAPI_WAIT_FINISHED);
}


/**
 * @brief Bit masks used to assemble a combined DRMAA state
 *
 * A DRMAA job state is one of these queued/running bits or-ed with the
 * suspend bits that apply:
 *
 * <pre>
 *    DRMAA_PS_QUEUED_ACTIVE         DRMAA_PS_SUBSTATE_PENDING
 *
 *    DRMAA_PS_SYSTEM_ON_HOLD        DRMAA_PS_SUBSTATE_PENDING |
 *                                   DRMAA_PS_SUBSTATE_SYSTEM_SUSP
 *
 *    DRMAA_PS_USER_ON_HOLD          DRMAA_PS_SUBSTATE_PENDING |
 *                                   DRMAA_PS_SUBSTATE_USER_SUSP
 *
 *    DRMAA_PS_USER_SYSTEM_ON_HOLD   DRMAA_PS_SUBSTATE_PENDING |
 *                                   DRMAA_PS_SUBSTATE_SYSTEM_SUSP |
 *                                   DRMAA_PS_SUBSTATE_USER_SUSP
 *
 *    DRMAA_PS_RUNNING               DRMAA_PS_SUBSTATE_RUNNING
 *
 *    DRMAA_PS_SYSTEM_SUSPENDED      DRMAA_PS_SUBSTATE_RUNNING |
 *                                   DRMAA_PS_SUBSTATE_SYSTEM_SUSP
 *
 *    DRMAA_PS_USER_SUSPENDED        DRMAA_PS_SUBSTATE_RUNNING |
 *                                   DRMAA_PS_SUBSTATE_USER_SUSP
 *
 *    DRMAA_PS_USER_SYSTEM_SUSPENDED DRMAA_PS_SUBSTATE_RUNNING |
 *                                   DRMAA_PS_SUBSTATE_SYSTEM_SUSP |
 *                                   DRMAA_PS_SUBSTATE_USER_SUSP
 * </pre>
 */
enum {
   DRMAA_PS_SUBSTATE_PENDING        = 0x10,   ///< The job has not started yet
   DRMAA_PS_SUBSTATE_RUNNING        = 0x20,   ///< The job is running
   DRMAA_PS_SUBSTATE_SYSTEM_SUSP    = 0x01,   ///< The job is held or suspended by the system
   DRMAA_PS_SUBSTATE_USER_SUSP      = 0x02    ///< The job is held or suspended by the user
};

/**
 * @brief Map Cluster Scheduler state into DRMAA state
 *
 * All Cluster Scheduler state information is used and combined into a DRMAA
 * job state.
 *
 * @param job the job (JB_Type)
 * @param is_array_task if false jobid is considered the job id of a seq. job, if true jobid and taskid must fit to an existing array task.
 * @param jobid the jobid of a seq. job or an array job
 * @param taskid the array task id in case of array jobs, 1 otherwise
 * @param remote_ps destination of DRMAA job state
 * @param diag diagnosis information
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_sge_state_to_drmaa_state() is MT safe
 */
static int
japi_sge_state_to_drmaa_state(const lListElem *job, bool is_array_task, uint32_t jobid,
                              uint32_t taskid, int *remote_ps, dstring *diag) {
   DENTER(TOP_LAYER);

   bool task_finished = false;
   lListElem *ja_task = nullptr;

   if (job == nullptr) {
      task_finished = true;
   }
   else {
      ja_task = job_search_task(job, nullptr, taskid);

      if ((ja_task != nullptr) && (lGetUlong(ja_task, JAT_status) == JFINISHED)) {
         task_finished = true;
      } else {
         if (ja_task == nullptr) {
            if (!range_list_is_id_within(lGetList(job, JB_ja_n_h_ids), taskid) &&
                !range_list_is_id_within(lGetList(job, JB_ja_u_h_ids), taskid) &&
                !range_list_is_id_within(lGetList(job, JB_ja_s_h_ids), taskid) &&
                !range_list_is_id_within(lGetList(job, JB_ja_o_h_ids), taskid) &&
                !range_list_is_id_within(lGetList(job, JB_ja_a_h_ids), taskid))
               task_finished = true;
         }
      }
   }

   /*
    * The reason for this job no longer being available at qmaster might
    * be it is done or failed. The JAPI job list contains such information
    * if the job was not yet waited. For a job that is not found there either
    * we return DRMAA_ERRNO_INVALID_JOB.
    */
   if (task_finished) {
      lListElem *japi_job = nullptr;
      const lListElem *japi_task = nullptr;

      DPRINTF("Job " sge_u32 "." sge_u32 " is finished.\n", jobid, taskid);

      JAPI_LOCK_JOB_LIST();

      japi_job = lGetElemUlongRW(Master_japi_job_list, JJ_jobid, jobid);

      if (japi_job != nullptr) {
         uint32_t wait_status;

         /*
          * When the job/task has already been deleted at qmaster side but
          * the event reporting this is not yet arrived at JAPI library
          * the task is still contained in the not_yet_finished list.
          *
          * We can assume that the job will be finished or failed, but
          * we can't know which one. Or we could presume the job was running,
          * but what if it was pending and then deleted using qdel?
          */
         if (range_list_is_id_within(lGetList(japi_job, JJ_not_yet_finished_ids), taskid)) {
            JAPI_UNLOCK_JOB_LIST();
            DPRINTF("Job " sge_u32 "." sge_u32 " is actually in unknown state.\n", jobid, taskid);
            *remote_ps = DRMAA_PS_UNDETERMINED;
            DRETURN(DRMAA_ERRNO_SUCCESS);
         }

         japi_task = lGetSubUlong(japi_job, JJAT_task_id, taskid, JJ_finished_tasks);

         if (japi_task != nullptr) {
            wait_status = lGetUlong(japi_task, JJAT_stat);
            DPRINTF("wait_status(" sge_u32 "/" sge_u32") = " sge_u32 "\n", jobid, taskid, wait_status);

            if (SGE_GET_NEVERRAN(wait_status)) {
               *remote_ps = DRMAA_PS_FAILED;
            } else {
               *remote_ps = DRMAA_PS_DONE;
            }

            JAPI_UNLOCK_JOB_LIST();
            DRETURN(DRMAA_ERRNO_SUCCESS);
         }
      }

      if ((japi_job == nullptr) || (japi_task == nullptr)) {
         JAPI_UNLOCK_JOB_LIST();
         japi_standard_error(DRMAA_ERRNO_INVALID_JOB, diag);
         DRETURN(DRMAA_ERRNO_INVALID_JOB);
      }

      /*
       * JJAT_stat must indicate whether the job finished or failed
       * at this point we simply assume it finished successfully
       * when it is found in the finished tasks list
       */

      JAPI_UNLOCK_JOB_LIST();
      *remote_ps = DRMAA_PS_DONE;
      DRETURN(DRMAA_ERRNO_SUCCESS);
   }

   if (!is_array_task) {
      /* reject "jobid" without taskid for array jobs */
      if (JOB_TYPE_IS_ARRAY(lGetUlong(job, JB_type))) {
         japi_standard_error(DRMAA_ERRNO_INVALID_JOB, diag);
         DRETURN(DRMAA_ERRNO_INVALID_JOB);
      }
   } else {
      /* reject "jobid.taskid" for non-array jobs and ensure taskid exists in job array */
      if (!JOB_TYPE_IS_ARRAY(lGetUlong(job, JB_type)) || !job_is_ja_task_defined(job, taskid)) {
         japi_standard_error(DRMAA_ERRNO_INVALID_JOB, diag);
         DRETURN(DRMAA_ERRNO_INVALID_JOB);
      }
   }

   if (ja_task != nullptr) {
      /* the state of enrolled tasks can directly be determined */
      uint32_t ja_task_status = lGetUlong(ja_task, JAT_status);
      uint32_t ja_task_state = lGetUlong(ja_task, JAT_state);
      uint32_t ja_task_hold = lGetUlong(ja_task, JAT_hold);

      DPRINTF("Job " sge_u32 "." sge_u32 " status=" sge_x32 " state=" sge_x32 " hold=" sge_x32 "\n",
              jobid, taskid, ja_task_status, ja_task_state, ja_task_hold);

      /* ERROR */
      if (ja_task_state & JERROR) {
         *remote_ps = DRMAA_PS_FAILED;
         DRETURN(DRMAA_ERRNO_SUCCESS);
      }

      /* PENDING & HOLD */
      if ((ja_task_status == JIDLE) || ((ja_task_state & JHELD) != 0)) {
         *remote_ps = DRMAA_PS_SUBSTATE_PENDING;

         /*
          * Only one hold state (-h u) is considered USER HOLD.
          * Others are also user's hold but only this hold state
          * can be released using japi_control().
          */
         if ((ja_task_hold & MINUS_H_TGT_USER))
            *remote_ps |= DRMAA_PS_SUBSTATE_USER_SUSP;

         /*
          * These hold states are considered SYSTEM HOLD. Some of
          * them (WAITING_DUE_TO_TIME, WAITING_DUE_TO_PREDECESSOR )
          * actually are the user's hold but DRMAA user interface does
          * not know these hold * conditions.
          */
         if ((ja_task_hold & (MINUS_H_TGT_OPERATOR|MINUS_H_TGT_SYSTEM|MINUS_H_TGT_JA_AD)) ||
             (lGetUlong64(job, JB_execution_time) > sge_get_gmt64()) ||
             lGetList(job, JB_jid_predecessor_list))
            *remote_ps |= DRMAA_PS_SUBSTATE_SYSTEM_SUSP;

         DRETURN(DRMAA_ERRNO_SUCCESS);
      }

      /* RUNNING */
      *remote_ps = DRMAA_PS_SUBSTATE_RUNNING;

      /*
       * Only the qmod -s <jobid> suspension is a USER SUSPEND
       * other suspension can be controlled only by the admins.
       */
      if ((ja_task_state & JSUSPENDED)) {
         *remote_ps |= DRMAA_PS_SUBSTATE_USER_SUSP;
      }

      /*
       * A SYSTEM SUSPEND can be
       *   - suspended due to suspend threshold
       *   - suspended because queue is qmod -s <queue> suspended
       *   - suspended because queue is suspended on subordinate
       *   - suspended because queue is suspended by calendar
       */
      if ((ja_task_state & JSUSPENDED_ON_THRESHOLD) ||
          (ja_task_state & JSUSPENDED_ON_SUBORDINATE) ||
          (ja_task_state & JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
         *remote_ps |= DRMAA_PS_SUBSTATE_SYSTEM_SUSP;
      }

      DRETURN(DRMAA_ERRNO_SUCCESS);
   }

   /* not yet enrolled tasks are always PENDING */
   *remote_ps = DRMAA_PS_SUBSTATE_PENDING;

   if (range_list_is_id_within(lGetList(job, JB_ja_u_h_ids), taskid)) {
      *remote_ps |= DRMAA_PS_SUBSTATE_USER_SUSP;
   }
   if (range_list_is_id_within(lGetList(job, JB_ja_s_h_ids), taskid) ||
       range_list_is_id_within(lGetList(job, JB_ja_o_h_ids), taskid) ||
       range_list_is_id_within(lGetList(job, JB_ja_a_h_ids), taskid)  ||
       (lGetUlong64(job, JB_execution_time) > sge_get_gmt64()) ||
                    lGetList(job, JB_jid_predecessor_list)) {
      *remote_ps |= DRMAA_PS_SUBSTATE_SYSTEM_SUSP;
   }

   DRETURN(DRMAA_ERRNO_SUCCESS);
}


/**
 * @brief Get job and the queue via GDI for job status
 *
 * We use GDI GET to get jobs status. Additionally also the queue list
 * must be retrieved because the (queue) system suspend state is kept in
 * the queue where the job runs.
 *
 * @param jobid the jobs id
 * @param retrieved_job_list resulting job list
 * @param diag diagnosis info
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTES: japi_get_job() is MT safe
 */
static int japi_get_job(uint32_t jobid, lList **retrieved_job_list, dstring *diag) {
   DENTER(TOP_LAYER);

   lList *alp = nullptr;
   const lListElem *aep = nullptr;
   int jb_id = 0;
   ocs::gdi::Request gdi_multi{};
   lCondition *job_selection = nullptr;
   lEnumeration *job_fields = nullptr;
   uint32_t quality = 0;

   /* prepare GDI GET JOB selection */
   job_selection = lWhere("%T(%I==%u)", JB_Type, JB_job_number, jobid);
   job_fields = lWhat("%T(%I%I%I%I%I%I%I%I%I%I%I)", JB_Type,
         JB_job_number,
         JB_type,
         JB_ja_structure,
         JB_ja_n_h_ids,
         JB_ja_u_h_ids,
         JB_ja_s_h_ids,
         JB_ja_o_h_ids,
         JB_ja_a_h_ids,
         JB_ja_tasks,
         JB_jid_predecessor_list,
         JB_execution_time);

   if (!job_selection || !job_fields) {
      japi_standard_error(DRMAA_ERRNO_NO_MEMORY, diag);
      DRETURN(DRMAA_ERRNO_NO_MEMORY);
   }

   jb_id = gdi_multi.request(&alp, ocs::gdi::Mode::SEND, ocs::gdi::Target::JB_LIST, ocs::gdi::Command::GET,
                             ocs::gdi::SubCommand::NONE, nullptr, job_selection, job_fields, true);
   gdi_multi.wait();
   lFreeWhere(&job_selection);
   lFreeWhat(&job_fields);

   gdi_multi.get_response(&alp, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE,
                          ocs::gdi::Target::JB_LIST, jb_id, retrieved_job_list);
   aep = lFirst(alp);
   if (aep == nullptr) {
      sge_dstring_copy_string(diag, MSG_JAPI_BAD_GDI_ANSWER_LIST);
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   quality = lGetUlong(aep, AN_quality);

   if (quality == ANSWER_QUALITY_ERROR) {
      answer_to_dstring(aep, diag);
      lFreeList(&alp);
      DRETURN(DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE);
   }

   lFreeList(&alp);

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Parse jobid string
 *
 * The string is parsed. Jobid and task id are returned, also
 * it is returned whether the id appears to be an array taskid.
 *
 * @param job_id_str the jobid string
 * @param jp destination for jobid
 * @param tp destination for taskid
 * @param ap was it an array task
 * @param diag diagnosis
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_parse_jobid() is MT safe
 */
static int japi_parse_jobid(const char *job_id_str, uint32_t *jp, uint32_t *tp,
                            bool *ap, dstring *diag) {
   DENTER(TOP_LAYER);

   uint32_t jobid, taskid;
   bool is_array_task;

   /* parse jobid/taskid */
   if (strchr(job_id_str, '.')) {
      if (sscanf(job_id_str, sge_u32"." sge_u32, &jobid, &taskid) != 2) {
         sge_dstring_sprintf(diag, MSG_JAPI_BAD_BULK_JOB_ID_S, job_id_str);
         DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
      }
/*       DPRINTF("parsing jobid.taskid: %ld.%ld\n", jobid, taskid); */
      is_array_task = true;
   } else {
      if (sscanf(job_id_str, sge_u32, &jobid) != 1) {
         sge_dstring_sprintf(diag, MSG_JAPI_BAD_JOB_ID_S, job_id_str);
         DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
      }
/*       DPRINTF("parsing jobid: %ld\n", jobid); */
      taskid = 1;
      is_array_task = false;
   }

   if (jp)
      *jp = jobid;
   if (tp)
      *tp = taskid;
   if (ap)
      *ap = is_array_task;

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Get job status
 *
 * Get the program status of the job identified by 'job_id'.
 * The possible values returned in 'remote_ps' and their meanings are:
 * DRMAA_PS_UNDETERMINED = 00H : process status cannot be determined,
 * DRMAA_PS_QUEUED_ACTIVE = 10H : job is queued and active,
 * DRMAA_PS_SYSTEM_ON_HOLD = 11H : job is queued and in system hold,
 * DRMAA_PS_USER_ON_HOLD = 12H : job is queued and in user hold,
 * DRMAA_PS_USER_SYSTEM_ON_HOLD = 13H : job is queued and in user and system hold,
 * DRMAA_PS_RUNNING = 20H : job is running,
 * DRMAA_PS_SYSTEM_SUSPENDED = 21H : job is system suspended,
 * DRMAA_PS_USER_SUSPENDED = 22H : job is user suspended,
 * DRMAA_PS_USER_SYSTEM_SUSPENDED = 23H : job is user and system suspended,
 * DRMAA_PS_DONE = 30H : job finished normally, and
 * DRMAA_PS_FAILED = 40H : job finished, but failed.
 *
 * @param job_id_str A job id
 * @param remote_ps Returns the job state - on success
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_job_ps() is MT safe
 *       Would be good to enhance drmaa_job_ps() to operate on an array of
 *       jobids.
 *       Would be good to have DRMAA_JOB_IDS_SESSION_ALL supported with
 *       drama_job_ps().
 *
 *       This function should be changed in a way that local JAPI-internal
 *       information is evaluated at first and no GDI request is done if
 *       this isn't necessary:
 *
 *       (1) A GDI request isn't acutally required for argument checking
 *       to prevent "jobid" being passed for array jobs or "jobid.taskid"
 *       be passed for non-array jobs. This is true at least for jobs
 *       that were submitted during the session which can be assumed the
 *       majority. Argument checking can be done based on JJ_type.
 *
 *       (2) A GDI request isn't actually required if job finish event
 *       already arrived at JAPI.
 *
 *       in these cases GDI request could be saved. This would help
 *       improving qmaster availability.
 */
int japi_job_ps(const char *job_id_str, int *remote_ps, dstring *diag) {
   DENTER(TOP_LAYER);

   uint32_t jobid, taskid;
   lList *retrieved_job_list = nullptr;
   lList *retrieved_cqueue_list = nullptr;
   int drmaa_errno;
   bool is_array_task;

   /* check arguments */
   if (!job_id_str || !remote_ps) {
      japi_standard_error(DRMAA_ERRNO_INVALID_ARGUMENT, diag);
      DRETURN(DRMAA_ERRNO_INVALID_ARGUMENT);
   }

   /* ensure japi_init() was called */
   JAPI_LOCK_SESSION();
   if (japi_session != JAPI_SESSION_ACTIVE) {
      JAPI_UNLOCK_SESSION();
      japi_standard_error(DRMAA_ERRNO_NO_ACTIVE_SESSION, diag);
      DRETURN(DRMAA_ERRNO_NO_ACTIVE_SESSION);
   }

   /* ensure job list still is consistent when we must access it later on
      to retrieve state information */
   japi_inc_threads(__func__);

   JAPI_UNLOCK_SESSION();

   /* per thread initialization */
   if (japi_init_mt(diag)!=DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_drmaa_job2sge_job() */
      DRETURN(DRMAA_ERRNO_INTERNAL_ERROR);
   }

   DPRINTF("japi_job_ps1(" SFQ ")\n", job_id_str);
   if ((drmaa_errno=japi_parse_jobid(job_id_str, &jobid, &taskid,
         &is_array_task, diag)) !=DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_parse_jobid() */
      DRETURN(drmaa_errno);
   }

   DPRINTF("japi_job_ps2(" SFQ ")\n", job_id_str);

   drmaa_errno = japi_get_job(jobid, &retrieved_job_list, diag);
   if (drmaa_errno != DRMAA_ERRNO_SUCCESS) {
      japi_dec_threads(__func__);
      /* diag written by japi_get_job() */
      DRETURN(drmaa_errno);
   }

   DPRINTF("japi_job_ps3(" SFQ ")\n", job_id_str);

   drmaa_errno = japi_sge_state_to_drmaa_state(lFirst(retrieved_job_list),
                                               is_array_task, jobid, taskid,
                                               remote_ps, diag);

   /* inactive code sample for retrieving master node information of a running job */
#if 0
   if (node) {
      const lListElem *ja_task, *master_node;
      switch (*remote_ps) {
      case DRMAA_PS_RUNNING:
      case DRMAA_PS_SYSTEM_SUSPENDED:
      case DRMAA_PS_USER_SUSPENDED:
      case DRMAA_PS_USER_SYSTEM_SUSPENDED:
         if ((ja_task = job_search_task(lFirst(retrieved_job_list), nullptr, taskid)) &&
            (master_node = lFirst(lGetList(ja_task, JAT_granted_destin_identifier_list))))
            sge_dstring_copy_string(node, lGetHost(master_node, JG_qhostname));
         else
            sge_dstring_copy_string(node, "<unknown>");
         break;
      default:
         break;
      }
   }
#endif

   japi_dec_threads(__func__);

   lFreeList(&retrieved_job_list);
   lFreeList(&retrieved_cqueue_list);

   DRETURN(drmaa_errno);
}

/**
 * @brief Did the job ever run?
 *
 * Evaluates into 'aborted' a non-zero value if 'stat' was returned for
 * a JAPI job that ended before entering the running state.
 *
 * @param stat 'stat' value returned by japi_wait()
 * @param aborted Returns 1 if the job was aborted, 0 otherwise - on success.
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_wifaborted() is MT safe
 *
 * @see #japi_wait
 */
int japi_wifaborted(int *aborted, int stat, dstring *diag) {
   *aborted = SGE_GET_NEVERRAN(stat)?1:0;
   return DRMAA_ERRNO_SUCCESS;
}


/**
 * @brief Has job exited?
 *
 * Allows to investigate whether a job has exited regularly.
 * If 'exited' returns 1 the exit status can be retrieved using
 * japi_wexitstatus().
 *
 * @param stat 'stat' value returned by japi_wait()
 * @param exited Returns 1 if the job exited, 0 otherwise - on success.
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_wifexited() is MT safe
 *
 * @see #japi_wexitstatus
 */
int japi_wifexited(int *exited, int stat, dstring *diag) {
   *exited = SGE_GET_WEXITED(stat)?1:0;
   return DRMAA_ERRNO_SUCCESS;
}

/**
 * @brief Get jobs exit status
 *
 * Retrieves the exit status of a job assumed it exited regularly
 * according japi_wifexited().
 *
 * @param stat 'stat' value returned by japi_wait()
 * @param exit_status Returns the jobs exit status - on success.
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_wexitstatus() is MT safe
 *
 * @see #japi_wifexited
 */
int japi_wexitstatus(int *exit_status, int stat, dstring *diag) {
   *exit_status = SGE_GET_WEXITSTATUS(stat);
   return DRMAA_ERRNO_SUCCESS;
}


/**
 * @brief Did the job die through a signal
 *
 * Allows to investigate whether a job died through a signal.
 * If 'signaled' returns 1 the signal can be retrieved using
 * japi_wtermsig().
 *
 * @param stat 'stat' value returned by japi_wait()
 * @param signaled Returns 1 if the job died through a signal, 0 otherwise - on success.
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_wifsignaled() is MT safe
 *
 * @see #japi_wtermsig
 */
int japi_wifsignaled(int *signaled, int stat, dstring *diag) {
   *signaled = SGE_GET_WSIGNALED(stat)?1:0;
   return DRMAA_ERRNO_SUCCESS;
}


/**
 * @brief Retrieve the signal a job died through
 *
 * Retrieves the signal of a job assumed it died through a signal
 * according japi_wifsignaled().
 *
 * @param stat 'stat' value returned by japi_wait()
 * @param sig Returns signal the job died through in string form (e.g. "SIGKILL")
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_wtermsig() is MT safe
 *       Would be better to directly SGE signal value, instead of a string.
 *
 * @see #japi_wifsignaled
 */
int japi_wtermsig(dstring *sig, int stat, dstring *diag) {
   uint32_t sge_sig = SGE_GET_WSIGNAL(stat);
   sge_dstring_sprintf(sig, "SIG%s", sge_sig2str(sge_sig));
   return DRMAA_ERRNO_SUCCESS;
}


/**
 * @brief Did job core dump?
 *
 * If drmaa_wifsignaled() indicates a job died through a signal this function
 * evaluates into 'core_dumped' a non-zero value if a core image of the terminated
 * job was created.
 *
 * @param stat 'stat' value returned by japi_wait()
 * @param core_dumped Returns 1 if a core image was created, 0 otherwises - on success.
 * @param diag Returns diagnosis information - on error.
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_wifcoredump() is MT safe
 */
int japi_wifcoredump(int *core_dumped, int stat, dstring *diag) {
   *core_dumped = SGE_GET_WCOREDUMP(stat)?1:0;
   return DRMAA_ERRNO_SUCCESS;
}

/**
 * @brief Provide standard diagnosis message
 *
 * @param drmaa_errno DRMAA error code
 * @param diag diagnosis message
 *
 * @note MT-NOTE: japi_standard_error() is MT safe
 */
void japi_standard_error(int drmaa_errno, dstring *diag) {
   if (diag != nullptr) {
      sge_dstring_copy_string(diag, japi_strerror(drmaa_errno));
   }
}


/**
 * @brief JAPI strerror()
 *
 * Returns readable text version of errno (constant string)
 *
 * @param drmaa_errno DRMAA error code
 *
 * @return A string describing the DRMAA error case for valid DRMAA error code and nullptr otherwise.
 *
 * @note MT-NOTE: japi_strerror() is MT safe
 */
const char *japi_strerror(int drmaa_errno) {
   const struct error_text_s {
      int drmaa_errno;
      const char *str;
   } error_text[] = {
      /* -------------- these are relevant to all sections ---------------- */
      { DRMAA_ERRNO_SUCCESS, "Routine returned normally with success." },
      { DRMAA_ERRNO_INTERNAL_ERROR, "Unexpected or internal DRMAA error like memory allocation, system call failure, etc." },
      { DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE, "Could not contact DRM system" },
      { DRMAA_ERRNO_AUTH_FAILURE, "The specified request is not processed successfully due to authorization failure." },
      { DRMAA_ERRNO_INVALID_ARGUMENT, "The input value for an argument is invalid." },
      { DRMAA_ERRNO_NO_ACTIVE_SESSION, "No active session" },
      { DRMAA_ERRNO_NO_MEMORY, "failed allocating memory" },

      /* -------------- init and exit specific --------------- */
      { DRMAA_ERRNO_INVALID_CONTACT_STRING, "Initialization failed due to invalid contact string." },
      { DRMAA_ERRNO_DEFAULT_CONTACT_STRING_ERROR, "DRMAA could not use the default contact string to connect to DRM system." },
      { DRMAA_ERRNO_NO_DEFAULT_CONTACT_STRING_SELECTED, "No default contact string was provided or selected." },
      { DRMAA_ERRNO_DRMS_INIT_FAILED, "Initialization failed due to failure to init DRM system." },
      { DRMAA_ERRNO_ALREADY_ACTIVE_SESSION, "Initialization failed due to existing DRMAA session." },
      { DRMAA_ERRNO_DRMS_EXIT_ERROR, "DRM system disengagement failed." },

   /* ---------------- job attributes specific -------------- */
      { DRMAA_ERRNO_INVALID_ATTRIBUTE_FORMAT, "The format for the job attribute value is invalid." },
      { DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE, "The value for the job attribute is invalid." },
      { DRMAA_ERRNO_CONFLICTING_ATTRIBUTE_VALUES, "The value of this attribute is conflicting with a previously set attributes." },

   /* --------------------- job submission specific -------------- */
      { DRMAA_ERRNO_TRY_LATER, "Could not pass job now to DRM system. A retry may succeed however (saturation)." },
      { DRMAA_ERRNO_DENIED_BY_DRM, "The DRM system rejected the job. The job will never be accepted due to DRM configuration or job template settings." },

   /* ------------------------------- job control specific ---------------- */
      { DRMAA_ERRNO_INVALID_JOB, "The job specified by the 'jobid' does not exist." },
      { DRMAA_ERRNO_RESUME_INCONSISTENT_STATE, "The job has not been suspended. The RESUME request will not be processed." },
      { DRMAA_ERRNO_SUSPEND_INCONSISTENT_STATE, "The job has not been running, and it cannot be suspended." },
      { DRMAA_ERRNO_HOLD_INCONSISTENT_STATE, "The job cannot be moved to a HOLD state." },
      { DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE, "The job is not in a HOLD state." },
      { DRMAA_ERRNO_EXIT_TIMEOUT, "time-out condition" },
      { DRMAA_ERRNO_NO_RUSAGE, "no usage information was returned for the completed job" },
      { DRMAA_ERRNO_NO_MORE_ELEMENTS, "no more elements are contained in the opaque string vector" },

      { DRMAA_NO_ERRNO, nullptr }
   };

   int i;

   for (i=0; error_text[i].drmaa_errno != DRMAA_NO_ERRNO; i++) {
      if (drmaa_errno == error_text[i].drmaa_errno) {
         return error_text[i].str;
      }
   }
   return nullptr;
}

/**
 * @brief Return current contact information
 *
 * Current contact information for DRM system
 *
 * @param[out] contact Returns a string similar to the 'contact' of japi_init()
 * @param[out] diag    Returns diagnosis information - on error
 *
 * @return DRMAA error code
 *
 * @note MT-NOTES: japi_get_contact() is MT safe
 *
 * @see #japi_init
 */
int japi_get_contact(dstring *contact, dstring *diag) {
   DENTER(TOP_LAYER);

   int japi_errno = DRMAA_ERRNO_SUCCESS;

   if ((contact != nullptr) && (diag != nullptr)) {
      JAPI_LOCK_SESSION();
      if ((japi_session_key != nullptr) &&
          (japi_session_key != JAPI_SINGLE_SESSION_KEY)) {
         sge_dstring_sprintf(contact, "session=%s", japi_session_key);
      }
      JAPI_UNLOCK_SESSION();
   }
/* This will change the previous behavior for this method, so we have to make it
 * specific to the new library version. */
   else if (contact == nullptr) {
      japi_errno = DRMAA_ERRNO_INVALID_ARGUMENT;
      japi_standard_error(japi_errno, diag);
   }

   DRETURN(japi_errno);
}

/**
 * @brief Return DRMAA version the JAPI library is compliant to
 *
 * Return DRMAA version the JAPI library is compliant to.
 *
 * @param[out] major Returns the major version number
 * @param[out] minor Returns the minor version number
 *
 * @note MT-NOTE: japi_version() is MT safe
 *
 * @warning The function body is empty - it writes neither `major` nor `minor`,
 *          so both stay at whatever the caller passed in. drmaa_version() is
 *          the only caller and sets the numbers itself before calling.
 */
void japi_version(unsigned int *major, unsigned int *minor) {
}


/**
 * @brief Returns SGE system implementation information. The output contain the DRM
 *
 * @param drm Returns DRM name - on success
 * @param diag Returns diagnssis information - on error.
 * @param me Me.wo progname
 *
 * @return DRMAA error codes
 *
 * @note MT-NOTE: japi_get_drm_system() is MT safe
 */
int japi_get_drm_system(dstring *drm, dstring *diag, ProgName me) {
   dstring buffer = DSTRING_INIT;
   pthread_once(&japi_once_control, japi_once_init);

   /* Set application prog number */
   prog_number = me;

   /* per thread initialization */
   if (japi_init_mt(diag)!=DRMAA_ERRNO_SUCCESS) {
      return DRMAA_ERRNO_INTERNAL_ERROR;
   }

   sge_dstring_copy_string(drm, feature_get_product_name(FS_SHORT_VERSION, &buffer));
   sge_dstring_free(&buffer);
   return DRMAA_ERRNO_SUCCESS;
}


/**
 * @brief Do event subscription for job list
 *
 * Event subscription for job list can be very costly. It requires
 * qmaster to copy the entire job list temporarily at the time when
 * an event is registered. For that reason subscribing the job list
 * was factorized out, so that it can be done only when required.
 * Subscribing the job list event is required only in cases
 * (a) when the client event client connection breaks down e.g.
 *     due to qmaster be shut-down and restarted
 * (b) when a JAPI session is restarted e.g when DRMAA is used
 *
 * @param japi_session_key JAPI session key
 * @param evc event client object
 *
 * @note MT-NOTE: japi_subscribe_job_list() is MT safe
 */
static void japi_subscribe_job_list(const char *japi_session_key, sge_evc_class_t *evc) {
   const int job_nm[] = {
      JB_job_number,
      JB_project,
      JB_type,
      JB_ja_tasks,
      JB_ja_structure,
      JB_ja_n_h_ids,
      JB_ja_u_h_ids,
      JB_ja_s_h_ids,
      JB_ja_o_h_ids,
      JB_ja_template,
      NoName
   };

   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   lListElem *where_el = nullptr;
   lListElem *what_el = nullptr;

   evc->ec_subscribe(evc, sgeE_JOB_LIST);

   where = lWhere("%T(%I==%s)", JB_Type, JB_session, japi_session_key);
   what = lIntVector2What(JB_Type, job_nm);

   where_el = lWhereToElem(where);
   what_el = lWhatToElem(what);

   evc->ec_mod_subscription_where(evc, sgeE_JOB_LIST, what_el, where_el);

   lFreeWhere(&where);
   lFreeWhat(&what);
   if (where_el) {
      lFreeElem(&where_el);
   }

   if (what_el) {
      lFreeElem(&what_el);
   }

   return;
}

/**
 * @brief Control flow implementation thread
 *
 * @note MT-NOTE: japi_implementation_thread() is MT safe
 */
static void *japi_implementation_thread(void *a_user_data_pointer) {
   DENTER(TOP_LAYER);

   lList *alp = nullptr, *event_list = nullptr;
   char buffer[1024];
   dstring buffer_wrapper;
   bool stop_ec = false;
   int parameter, ed_time = 30;
   const char *s;
   bool restarting;
   bool job_list_subscribed = false;
   bool up_and_running = false;
   bool qmaster_bound = false; /* Whether we ever successfully connected to the
                                  qmaster. */
   bool disconnected = false; /* Whether we are currently connected to the
                                 qmaster. */
   static sge_evc_class_t *evc = nullptr;
   ocs::gdi::ErrorValue gdi_errno = ocs::gdi::ErrorValue::AE_OK;

   /* Check EC state before we bother starting.  This also prevents the event
    * client thread from having a race condition with japi_enable_job_wait(). */
   JAPI_LOCK_EC_STATE();
   if (japi_ec_state != JAPI_EC_STARTING && japi_ec_state != JAPI_EC_RESTARTING ) {
      JAPI_UNLOCK_EC_STATE();
      lFreeList(&alp);
      goto SetupFailed;
   }
   restarting = (japi_ec_state == JAPI_EC_RESTARTING)?true:false;
   JAPI_UNLOCK_EC_STATE();

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

   gdi_errno = ocs::gdi::ClientBase::setup_and_enroll(prog_number, MAIN_THREAD, &alp);
   if ((gdi_errno != ocs::gdi::ErrorValue::AE_OK) && (gdi_errno != ocs::gdi::ErrorValue::AE_ALREADY_SETUP)) {
      const lListElem *aep = lFirst(alp);
      DPRINTF("error: ocs::gdi::ClientBase::setup_and_enroll() failed with gdi_error %d for event client thread\n", gdi_errno);
      if (aep) {
         JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
         answer_list_add(&(japi_ec_alp_struct.japi_ec_alp), lGetString(aep, AN_text),
               lGetUlong(aep, AN_status), (answer_quality_t)lGetUlong(aep, AN_quality));
         JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
      }
      lFreeList(&alp);
      goto SetupFailed;
   }

   /* JAPI parameters passed through environment */
   if ((s=getenv("SGE_JAPI_EDTIME"))) {
      parameter = atoi(s);
      if (parameter > 0) {
         ed_time = parameter;
      }
   }
   /* register at qmaster as event client */
   DPRINTF("registering as event client ...\n");
   evc = sge_evc_class_create(EV_ID_ANY, &alp, nullptr);
   if (evc == nullptr) {
      const lListElem *aep = lFirst(alp);
      if (aep) {
         JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
         answer_list_add(&(japi_ec_alp_struct.japi_ec_alp), lGetString(aep, AN_text),
               lGetUlong(aep, AN_status), (answer_quality_t)lGetUlong(aep, AN_quality));
         JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
      }
      lFreeList(&alp);
      goto SetupFailed;
   }

   evc->ec_set_edtime(evc, ed_time);
   evc->ec_set_busy_handling(evc, EV_BUSY_UNTIL_ACK);
   evc->ec_set_session(evc, japi_session_key);

   /* subscription of the entire job list at start-up
      required only for session reconnect (DRMAA) */
   if (restarting) {
      japi_subscribe_job_list(japi_session_key, evc);
      evc->ec_mark4registration(evc);
      job_list_subscribed = true;
   }

   evc->ec_subscribe(evc, sgeE_JOB_FINISH);
   evc->ec_set_flush(evc, sgeE_JOB_FINISH, true, 0);

   evc->ec_subscribe(evc, sgeE_JATASK_MOD);
   evc->ec_set_flush(evc, sgeE_JATASK_MOD, true, 0);

   evc->ec_subscribe(evc, sgeE_SHUTDOWN);
   evc->ec_set_flush(evc, sgeE_SHUTDOWN, true, 0);

/*    sgeE_QMASTER_GOES_DOWN  ??? */

   /* Check again before we commit to this. */
   JAPI_LOCK_EC_STATE();
   if (japi_ec_state != JAPI_EC_STARTING && japi_ec_state != JAPI_EC_RESTARTING ) {
      JAPI_UNLOCK_EC_STATE();
      lFreeList(&alp);
      goto SetupFailed;
   }

   if (!evc->ec_register(evc, false, &alp)) {
      const lListElem *aep = lFirst(alp);
      DPRINTF("error: ec_register() failed\n");
      if (aep) {
         JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
         answer_list_add(&(japi_ec_alp_struct.japi_ec_alp), lGetString(aep, AN_text),
               lGetUlong(aep, AN_status), (answer_quality_t)lGetUlong(aep, AN_quality));
         JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
      }
      JAPI_UNLOCK_EC_STATE();
      lFreeList(&alp);
      goto SetupFailed;
   }
   japi_ec_id = evc->ec_get_id(evc);
   JAPI_UNLOCK_EC_STATE();

   cl_com_set_synchron_receive_timeout(cl_com_get_handle(component_get_component_name(), 0),ed_time*2);

   while (!stop_ec) {
      /* read events and add relevant information into library session data */
      int ec_get_ret = evc->ec_get(evc, &event_list, false);
      if (!ec_get_ret) {
         evc->ec_mark4registration(evc);

         DPRINTF (("Sleeping 10 seconds before trying to register again.\n"));
         sleep(10);
      } else {
         /* We need to check that we japi_exit() didn't wake us up to die. */
         JAPI_LOCK_EC_STATE();
         if (japi_ec_state == JAPI_EC_FINISHING) {
            JAPI_UNLOCK_EC_STATE();
            DPRINTF (("Received stop request while waiting for events.\n"));
            lFreeList(&event_list);
            break;
         }
         JAPI_UNLOCK_EC_STATE();

         DTRACE;

         /* Bug Fix: Issuezilla #826
          * The first part of this bug fix is to keep the event client thread
          * from dying when the qmaster goes down.  In distinguish between
          * failures that represent the qmaster going down and failures that
          * represent other errors, such as the qmaster never having been up,
          * we note here that we were able to communication with the qmaster
          * at least once before we started having problems. */
         qmaster_bound = true;

         DTRACE;

         /* If we think we're disconnected, print a message saying we've
          * reconnected, and note that we're not disconnected. */
         if (disconnected) {
            if (error_handler != nullptr) {
               error_handler (MSG_JAPI_RECONNECTED);
            }
            if (!job_list_subscribed) {
               japi_subscribe_job_list(japi_session_key, evc);
               job_list_subscribed = true;
            }

            DPRINTF ((MSG_JAPI_RECONNECTED));
            disconnected = false;
         }

         DTRACE;

         for_each_ep_lv(event, event_list) {
            uint32_t type, intkey, intkey2;
            type = lGetUlong(event, ET_type);
            intkey = lGetUlong(event, ET_intkey);
            intkey2 = lGetUlong(event, ET_intkey2);

            DPRINTF("\tEvent: %s intkey %d intkey2 %d\n", event_text(event, &buffer_wrapper), intkey, intkey2);

            /* maintain library session data */
            if (type == sgeE_JOB_LIST) {
               lList *sge_job_list = lGetListRW(event, ET_new_version);
               uint32_t jobid, taskid;
               int finished_tasks = 0;

               DPRINTF (("Handling job list event\n"));
               JAPI_LOCK_JOB_LIST();

               /* - check every session job
                  - no longer existing jobs must be moved to JJ_finished_jobs
                  - TODO: actually we had to return DRMAA_ERRNO_NO_RUSAGE when japi_wait() is
                          called for such a job. Must enhance JJAT_Type to reflect the case when
                          no stat and rusage are known */
               for_each_rw_lv(japi_job, Master_japi_job_list) {
                  jobid = lGetUlong(japi_job, JJ_jobid);

                  lListElem *sge_job;
                  if (!(sge_job = lGetElemUlongRW(sge_job_list, JB_job_number, jobid))) {
                     while ((taskid = range_list_get_first_id(lGetList(japi_job, JJ_not_yet_finished_ids), nullptr))) {
                        /* remove task from not yet finished job id list */
                        object_delete_range_id(japi_job, nullptr, JJ_not_yet_finished_ids, taskid);

                        /* add entry to the finished tasks */
                        DPRINTF("adding finished task " sge_u32 " for job " sge_u32 " existing not any longer\n", taskid, jobid);
                        lAddSubUlong(japi_job, JJAT_task_id, taskid, JJ_finished_tasks, JJAT_Type);
                        finished_tasks++;

                     } /* while */
                  } /* if (sge_job == nullptr) */
                  else {
                     finished_tasks = japi_sync_job_tasks (japi_job, sge_job);
                     /* So that we know which jobs have been seen, we remove this
                      * job from the list. */
                     lRemoveElem(sge_job_list, &sge_job);
                  } /* else */
               } /* for_each */

               /* Now add any left over jobs to the master job list. */
               for_each_rw_lv(sge_job, sge_job_list) {
                  lList *task_list = nullptr;

                  lListElem *japi_job = lAddElemUlong(&Master_japi_job_list, JJ_jobid,
                                                  lGetUlong(sge_job, JB_job_number), JJ_Type);
                  lSetUlong(japi_job, JJ_type, lGetUlong(sge_job, JB_type));
                  lXchgList(sge_job, JB_ja_structure, &task_list);
                  lSetList(japi_job, JJ_not_yet_finished_ids, task_list);
                  finished_tasks = japi_sync_job_tasks (japi_job, sge_job);
               }

               /* signal all application threads waiting for a job to finish */
               if (finished_tasks)
                  pthread_cond_broadcast(&Master_japi_job_list_finished_cv);

               JAPI_UNLOCK_JOB_LIST();

            } /* if type == sgeE_JOB_LIST */
            else if (type == sgeE_JOB_FINISH) {
               /* - move job/task to JJ_finished_jobs */
               lListElem *japi_job, *japi_task;
               uint32_t wait_status;
               const char *err_str;
               const lListElem *jr = lFirst(lGetList(event, ET_new_version));

               DPRINTF (("Handling job finish event\n"));

               wait_status = lGetUlong(jr, JR_wait_status);
               err_str = lGetString(jr, JR_err_str);
               if (SGE_GET_NEVERRAN(wait_status)) {
                  DPRINTF("JOB_FINISH: %d.%d job never ran: %s\n", intkey, intkey2, err_str);
               } else {
                  if (SGE_GET_WEXITED(wait_status)) {
                     DPRINTF("JOB_FINISH: %d.%d exited with exit status %d\n", intkey, intkey2, SGE_GET_WEXITSTATUS(wait_status));
                  }
                  if (SGE_GET_WSIGNALED(wait_status)) {
                     DPRINTF("JOB_FINISH: %d.%d died through signal %s%s\n", intkey, intkey2, sge_sig2str(SGE_GET_WSIGNAL(wait_status)), SGE_GET_WCOREDUMP(wait_status)?"(core dumped)":"");
                  }
               }

               JAPI_LOCK_JOB_LIST();

               japi_job = lGetElemUlongRW(Master_japi_job_list, JJ_jobid, intkey);
               if (japi_job != nullptr) {
                  if (range_list_is_id_within(lGetList(japi_job, JJ_not_yet_finished_ids), intkey2)) {
                     const lList *usage = nullptr;

                     /* remove task from not yet finished job id list */
                     object_delete_range_id(japi_job, nullptr, JJ_not_yet_finished_ids, intkey2);

                     /* add an entry to the finished tasks */
                     DPRINTF("adding finished task %ld for job %ld\n", intkey2, intkey);
                     japi_task = lAddSubUlong(japi_job, JJAT_task_id, intkey2, JJ_finished_tasks, JJAT_Type);
                     lSetUlong(japi_task, JJAT_stat, wait_status);
                     lSetString(japi_task, JJAT_failed_text, err_str);

                     usage = lGetList (jr, JR_usage);

                     if (usage != nullptr)  {
                        lSetList(japi_task, JJAT_rusage, lCopyList ("job usage", usage));
                     }

                     /* signal all application threads waiting for a job event */
                     pthread_cond_broadcast(&Master_japi_job_list_finished_cv);
                  } /* if range_list_is_id_within() */
               } /* if japi_job != nullptr */
               else {
                  DPRINTF("ignoring event on unknown job " sge_u32 "\n", intkey);
               }

               JAPI_UNLOCK_JOB_LIST();
            } /* else if type == sgeE_JOB_FINISH */
            else if (type == sgeE_JATASK_MOD) {
               /* - add task to JJ_started_task_ids */
               lListElem *japi_job;
               const lList *jat = lGetList(event, ET_new_version);
               const lListElem *ep = lFirst(jat);
               u_long job_status = lGetUlong(ep, JAT_status);
               bool task_running = (job_status==JRUNNING || job_status==JTRANSFERING);

               if (task_running) {
                  DPRINTF("Handling task modify event\n");

                  JAPI_LOCK_JOB_LIST();

                  japi_job = lGetElemUlongRW(Master_japi_job_list, JJ_jobid, intkey);
                  if (japi_job != nullptr) {
                     if (!range_list_is_id_within(lGetList (japi_job, JJ_started_task_ids), intkey2)) {
                        lList *range = nullptr;

                        lXchgList(japi_job, JJ_started_task_ids, &range);

                        if (range == nullptr) {
                           range = lCreateList ("started tasks", RN_Type);
                        }

                        /* add an entry to the started tasks */
                        DPRINTF("adding started task %ld for job %ld\n", intkey2, intkey);
                        range_list_insert_id (&range, &alp, intkey2);
                        JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
                        range_list_sort_uniq_compress(range, &(japi_ec_alp_struct.japi_ec_alp), true);
                        JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
                        lXchgList(japi_job, JJ_started_task_ids, &range);

                        /* signal all application threads waiting for a job event */
                        pthread_cond_broadcast (&Master_japi_job_list_finished_cv);
                     }
                  } /* if japi_job != nullptr */
                  else {
                     DPRINTF("ignoring event on unknown job " sge_u32 "\n", intkey);
                  }

                  JAPI_UNLOCK_JOB_LIST();
               } /* if task_running */
            } /* else if type == sgeE_JATASK_MOD */
            /* Bug Fix: Issuezilla #826
             * Since we only want to stop when explicitly told to, we have to
             * draw a distinction between SHUTDOWN and QMASTER_GOES_DOWN.
             * On SHUTDOWN, we exit the event client thread.
             * On QMASTER_GOES_DOWN, * we may eventually want to issue a warning message.
             */
            else if (type == sgeE_SHUTDOWN) {
               JAPI_LOCK_JOB_LIST();
               DPRINTF (("Received shutdown message\n"));
               stop_ec = true;
               qmaster_bound = false;
               JAPI_LOCK_EC_ALP(japi_ec_alp_struct);
               answer_list_add(&(japi_ec_alp_struct.japi_ec_alp), MSG_JAPI_KILLED_EVENT_CLIENT,
                  STATUS_ERROR1, ANSWER_QUALITY_CRITICAL);
               JAPI_UNLOCK_EC_ALP(japi_ec_alp_struct);
               pthread_cond_broadcast (&Master_japi_job_list_finished_cv);
               JAPI_UNLOCK_JOB_LIST();
            } else if (type == sgeE_ACK_TIMEOUT) {
               /*
                * Print a message that we are timed out at qmaster
                * and we have to reconnect.
                */
               DPRINTF("got sgeE_ACK_TIMEOUT event\n");

               disconnected = true;

               if (error_handler != nullptr) {
                  error_handler(MSG_JAPI_QMASTER_TIMEDOUT);
               }
            } else if (type == sgeE_QMASTER_GOES_DOWN) {
               /* Print a message that qmaster is down and note that we are
                * disconnected. */
               if (error_handler != nullptr) {
                  error_handler(MSG_JAPI_QMASTER_DOWN);
               }

               DPRINTF(SFNMAX "\n", MSG_JAPI_QMASTER_DOWN);
               disconnected = true;
            }
         } /* for_each */
         lFreeList(&event_list);

         if (!up_and_running) {
            /* set japi_ec_state to JAPI_EC_UP and notify initialization thread */
            DPRINTF("signalling event client thread is up and running\n");

            JAPI_LOCK_EC_STATE();
            japi_ec_state = JAPI_EC_UP;
            DPRINTF("EC STATE is now %d\n", japi_ec_state);
               pthread_cond_signal(&japi_ec_state_starting_cv);
            JAPI_UNLOCK_EC_STATE();
            up_and_running = true;
         }
      } /* else */

      if (!stop_ec) {
         /* has japi_exit() been called meanwhile ? */
         JAPI_LOCK_EC_STATE();
         if (japi_ec_state == JAPI_EC_FINISHING) {
            stop_ec = true;
         }
         JAPI_UNLOCK_EC_STATE();
      }

      /* Bug Fix: Issuezilla #826
       * Here we have to make sure that we only give up if we've never actually
       * connected to the qmaster.  At some point we should probably implement
       * some kind of timeout to keep clients from waiting indefinitely for a
       * qmaster that may never come back. */
      if ((ec_get_ret == 0) && !stop_ec && !qmaster_bound) {
         /* Print a message that there's a communication problem */
         if (error_handler != nullptr) {
            error_handler (MSG_JAPI_EC_GET_PROBLEM);
         }

         DPRINTF ((MSG_JAPI_EC_GET_PROBLEM));
         stop_ec = true;
      }
      else if ((ec_get_ret == 0) && !stop_ec && !disconnected) {
         /* Print a message that the qmaster is unavailable and note that we're
            disconnected. */
         if (error_handler != nullptr) {
            error_handler (MSG_JAPI_DISCONNECTED);
         }

         DPRINTF ((MSG_JAPI_DISCONNECTED));
         disconnected = true;
      }
   } /* while */

   /* Unregister event client */
   DPRINTF("unregistering from qmaster ...\n");
   if (evc->ec_deregister(evc)==false) {
      DPRINTF("failed unregistering event client from qmaster.\n");
   } else {
      DPRINTF("... unregistered.\n");
   }

   JAPI_LOCK_EC_STATE();
   /* We have to check here whether the event client ever got the first job list
    * event.  If not, being here counts as a failure. */
   /* The only non-error states here are JAPI_EC_UP="success" and
    * JAPI_EC_FINISHING="aborted by main thread." */
   if ((japi_ec_state == JAPI_EC_UP) || (japi_ec_state == JAPI_EC_FINISHING)) {
      japi_ec_state = JAPI_EC_DOWN;
   } else {
      japi_ec_state = JAPI_EC_FAILED;
   }

   japi_ec_id = 0;
   /* We signal here because it's possible that we started up ok but failed on
    * the first ec_get to get the job list event.  In that case, the main thread
    * will still be waiting for the event client to signal start up. */
   pthread_cond_signal(&japi_ec_state_starting_cv);
   JAPI_UNLOCK_EC_STATE();

   /* signal all application threads waiting for a job event */
   pthread_cond_broadcast (&Master_japi_job_list_finished_cv);

   pthread_exit(nullptr);

   DRETURN(nullptr);

SetupFailed:
   JAPI_LOCK_EC_STATE();
   japi_ec_state = JAPI_EC_FAILED;
   pthread_cond_signal(&japi_ec_state_starting_cv);
   JAPI_UNLOCK_EC_STATE();
   DRETURN(nullptr);
}


/**
 * @brief Adjusts JAPI job structure tasks to match the
 *
 * Iterates through the JAPI job structure's JJ_not_yet_finished_task_ids
 * list and moves finished jobs into the JJ_finished_tasks list.
 *
 * @return The number of finished tasks
 *
 * @note MT-NOTES: japi_sync_job_tasks() is MT safe.
 */
static int japi_sync_job_tasks(lListElem *japi_job, lListElem *sge_job) {
   DENTER(TOP_LAYER);
   lList *range_list_copy = nullptr;
   lListElem *task = nullptr;
   uint32_t min = 0;
   uint32_t max = 0;
   uint32_t step = 0;
   uint32_t taskid = 0;
   int finished_tasks = 0;

   /*
    * We must iterate over all taskid's in the JJ_not_yet_finished_ids list.
    * Depending on the tasks state as the reported by qmaster entries
    * are removed from the JJ_not_yet_finished_ids list in this loop.
    * For this reason we operate on a copy to implement the loop.
    */
   range_list_copy = lCopyList(nullptr, lGetList(japi_job, JJ_not_yet_finished_ids));

   /* keep all tasks in 'not yet finished list' if tasks are
      still running or not yet running */
   for_each_ep_lv(range, range_list_copy) {
      range_get_all_ids(range, &min, &max, &step);

      for (taskid = min; taskid <= max; taskid += step) {
         task = job_search_task(sge_job, nullptr, taskid);
         if (task != nullptr) {
            DPRINTF("task " sge_u32 "." sge_u32" contained in enrolled task list\n", lGetUlong(japi_job, JJ_jobid), taskid);

            if ((lGetUlong(task, JAT_status) & JFINISHED) != 0) {
               DPRINTF("task " sge_u32 "." sge_u32 " is finished\n", lGetUlong(japi_job, JJ_jobid), taskid);
            }
            /* This is a potentially problematic animal.  DRMAA has a problem
             * with jobs in error state as DRMAA has no error state.  This case
             * should never occur, but if it does, this should be the correct
             * way to handle it.  However, because the code in complex, I'm not
             * 100% certain. */
            else if ((lGetUlong(task, JAT_state) & JERROR) != 0) {
               DPRINTF("task " sge_u32 "." sge_u32 " has failed\n", lGetUlong(japi_job, JJ_jobid), taskid);
            }
            else {
               continue;
            }
         } else if (range_list_is_id_within(lGetList(sge_job, JB_ja_n_h_ids), taskid) ||
                  range_list_is_id_within(lGetList(sge_job, JB_ja_u_h_ids), taskid) ||
                  range_list_is_id_within(lGetList(sge_job, JB_ja_s_h_ids), taskid) ||
                  range_list_is_id_within(lGetList(sge_job, JB_ja_o_h_ids), taskid)) {
            DPRINTF("task " sge_u32 "." sge_u32" is still pending\n", lGetUlong(japi_job, JJ_jobid), taskid);
            continue;
         } else {
            DPRINTF("task " sge_u32 "." sge_u32" presumably has finished meanwhile\n", lGetUlong(japi_job, JJ_jobid), taskid);
         }

         /* remove task from not yet finished job id list */
         object_delete_range_id(japi_job, nullptr, JJ_not_yet_finished_ids,
                                taskid);
         /* add entry to the finished tasks */
         DPRINTF("adding finished task %ld for job %ld which still exists\n", taskid, lGetUlong(japi_job, JJ_jobid));
         lAddSubUlong(japi_job, JJAT_task_id, taskid, JJ_finished_tasks, JJAT_Type);
         finished_tasks++;
      } /* for */
   } /* for_each */

   lFreeList(&range_list_copy);
   DRETURN(finished_tasks);
}

/**
 * @brief Stops jobs still running in the session
 *
 * Deletes jobs running in the session when flag is set to JAPI_EXIT_KILL_ALL
 * or JAPI_EXIT_KILL_PENDING.
 *
 * @return 0 = OK, 1 = Error
 *
 * @note MT-NOTES: japi_clean_up_jobs() is MT safe (assumptions)
 */
static int japi_clean_up_jobs(int flag, dstring *diag) {
   DENTER(TOP_LAYER);

   const lListElem *japi_job = nullptr;
   lListElem *id_entry = nullptr;
   lList *id_list = nullptr, *alp = nullptr;
   uint32_t jobid;
   int ret = DRMAA_ERRNO_SUCCESS;
   bool done = false;
   int count = 0;
   char buffer[1024];
   dstring job_task_specifier;

   sge_dstring_init(&job_task_specifier, buffer, sizeof(buffer));

   /* If there are any pending jobs, and a flag is set, kill them. */
   if ((flag == JAPI_EXIT_KILL_PENDING) || (flag == JAPI_EXIT_KILL_ALL)) {
      if (flag == JAPI_EXIT_KILL_PENDING) {
         DPRINTF (("Stopping all pending jobs in this session.\n"));
      }
      else if (flag == JAPI_EXIT_KILL_ALL) {
         DPRINTF (("Stopping all jobs in this session.\n"));
      }

      JAPI_LOCK_JOB_LIST();
      japi_job = lFirst (Master_japi_job_list);

      while (!done) {
         count = 0;

         while (japi_job != nullptr) {
            jobid = lGetUlong(japi_job, JJ_jobid);

            DPRINTF("Stopping job %ld\n", jobid);

            sge_dstring_sprintf(&job_task_specifier, sge_u32, jobid);
            id_entry = lAddElemStr(&id_list, ID_str, sge_dstring_get_string(&job_task_specifier), ID_Type);

            if (JOB_TYPE_IS_ARRAY(lGetUlong(japi_job, JJ_type))) {
               /* Kill every task in the not yet finished list.  Some of the tasks
                * may have finished since we killed the event client, but that's
                * ok. If we can't stop a job, we just move on to the next one. */
               if (flag == JAPI_EXIT_KILL_PENDING) {
                  lList *del_list = nullptr;

                  range_list_calculate_difference_set (&del_list, &alp,
                                    lGetList(japi_job, JJ_not_yet_finished_ids),
                                    lGetList(japi_job, JJ_started_task_ids));
                  lSetList(id_entry, ID_ja_structure, del_list);
               }
               /* Kill every task that is in the not yet finished list but not in
                * the started list.  Same as above for tasks we can't kill. */
               else if (flag == JAPI_EXIT_KILL_ALL) {
                  lSetList(id_entry, ID_ja_structure, lCopyList(nullptr,
                                  lGetList(japi_job, JJ_not_yet_finished_ids)));
               }
            }

            /* japi_job starts out as the first element in the master job
             * list.  Every time through this loop, we move to the next
             * element.  We do this before the check for maximum num of
             * jobs to delete so that the next time we come to this loop,
             * japi_job will already point to the right job.  This saves
             * us some initializer logic before the loop. */
            japi_job = lNext (japi_job);

            if (++count >= MAX_JOBS_TO_DELETE) {
               break; /* while */
            }
         } /* while */

         if (count < MAX_JOBS_TO_DELETE) {
            DPRINTF("Deleting %d jobs\n", count);
            done = true;
         }
         else {
            DPRINTF("Deleting %d jobs\n", MAX_JOBS_TO_DELETE);
         }

         if (id_list) {
            /* This function frees id_list. */
            ret = do_gdi_delete (&id_list, DRMAA_CONTROL_TERMINATE, true, diag);

            if (ret != DRMAA_ERRNO_SUCCESS) {
               break; /* while */
            }
         } /* if */
      } /* while */
      JAPI_UNLOCK_JOB_LIST();
   } /* if */

   DRETURN(ret);
}


/**
 * @brief Return current contact information
 *
 * Check if japi_init was already called.
 *
 * @param diag returns diagnosis information - on error
 *
 * @return DRMAA_ERRNO_SUCCESS if japi_init was already called, DRMAA_ERRNO_NO_ACTIVE_SESSION if japi_init was not called, DRMAA_ERRNO_INTERNAL_ERROR if an unexpected error occurs.
 *
 * @note MT-NOTES: japi_was_init_called() is MT safe
 */
int japi_was_init_called(dstring *diag) {
   DENTER(TOP_LAYER);

   int ret = DRMAA_ERRNO_SUCCESS;

   /* per thread initialization */
   /* diag written by japi_init_mt() */
   ret = japi_init_mt(diag);

   if (ret == DRMAA_ERRNO_SUCCESS) {
      /* ensure japi_init() was called */
      JAPI_LOCK_SESSION();

      if (japi_session != JAPI_SESSION_ACTIVE) {
         ret = DRMAA_ERRNO_NO_ACTIVE_SESSION;
      }

      JAPI_UNLOCK_SESSION();
   }

   if (ret != DRMAA_ERRNO_SUCCESS) {
      japi_standard_error(ret, diag);
   }

   DRETURN(ret);
}

/**
 * @brief Is file staging enabled, i.e
 *
 * Returns if delegated file staging is enabled.
 *
 * @param[out] diag Returns diagnosis information - on error
 *
 * @return true if delegated file staging is enabled, else false
 *
 * @note MT-NOTES: japi_is_delegated_file_staging_enabled() is MT safe
 */
bool japi_is_delegated_file_staging_enabled(dstring *diag) {
   DENTER(TOP_LAYER);

   bool ret = false;

   JAPI_LOCK_SESSION();
   if (japi_delegated_file_staging_is_enabled == -1) {
      /* This function call does a GDI call, meaning it could take a while,
       * leaving the session mutex locked.  However, this only happens once.
       * The less noticable way to make this call is to call it from
       * japi_init().  The problem there, however, is documented as Issuezilla
       * bug #1025.  This is the next best solution and doesn't appear to cause
       * any noticable problems. */
      japi_read_dynamic_attributes (diag);
   }

   ret = (japi_delegated_file_staging_is_enabled == 1) ? true : false;
   JAPI_UNLOCK_SESSION();

   DRETURN(ret);
}

/**
 * @brief Read the 'dynamic' attributes from
 *
 * Reads from the DRM configuration, which 'dynamic' attributes are enabled.
 *
 * @param diag returns diagnosis information - on error
 *
 * @return DRMAA_ERRNO_SUCCES on success, DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE, DRMAA_ERRNO_INVALID_ARGUMENT on error.
 *
 * @note MT-NOTES: japi_read_dynamic_attributes() is not MT safe.  It assumes that
 *       the calling thread holds the session mutex.
 */
static int japi_read_dynamic_attributes(dstring *diag) {
   DENTER(TOP_LAYER);

   int        ret=0;
   int        drmaa_errno=DRMAA_ERRNO_SUCCESS;
   const lList      *pSubList;
   lListElem  *config = nullptr;
   const lListElem  *ep = nullptr;
   const char *pStr = nullptr;

   ret = ocs::gdi::Client::gdi_get_configuration(SGE_GLOBAL_NAME, &config, nullptr);

   if (ret<0) {
      switch( ret ) {
         case -2:
         case -4:
         case -6:
         case -7:
         case -8:
            drmaa_errno = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
            break;
         case -1:
         case -3:
            drmaa_errno = DRMAA_ERRNO_INVALID_ARGUMENT;
            break;
         case -5:
            /* -5 there is no global configuration
             * This means that "delegated_file_staging" is not set.
             * This is not an error for us, not set means default value.
             */
            drmaa_errno = DRMAA_ERRNO_SUCCESS;
            break;
      }

      japi_standard_error(drmaa_errno, diag);
      DRETURN(drmaa_errno);
   }

   pSubList = lGetList(config, CONF_entries);
   if (pSubList != nullptr) {
      ep = lGetElemStr(pSubList, CF_name, "delegated_file_staging");
      if (ep != nullptr) {
         pStr = lGetString(ep, CF_value);

         if (strcasecmp( pStr, "true") ==0) {
            japi_delegated_file_staging_is_enabled = 1;
         }
         else {
            japi_delegated_file_staging_is_enabled = 0;
         }
      }
   }

   lFreeElem(&config);
   DRETURN(drmaa_errno);
}

/**
 * @brief Delete the job list
 *
 * Deletes all the jobs in the job id list, converts and GDI errors into
 * DRMAA errors, and frees the job id list.
 *
 * @param id_list List of job ids to delete.  Gets freed.
 * @param action The action that caused this delete
 * @param delete_all Whether this call is deleting all jobs in the session
 * @param diag returns diagnosis information - on error
 *
 * @return DRMAA_ERRNO_SUCCES on success, DRMAA error code on error.
 *
 * @note MT-NOTES: do_gdi_delete() is MT safe
 */
static int do_gdi_delete(lList **id_list, int action, bool delete_all, dstring *diag) {
   DENTER(TOP_LAYER);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::DEL, ocs::gdi::SubCommand::NONE,
                                          id_list, nullptr, nullptr);
   lFreeList(id_list);

   for_each_rw_lv(aep, alp) {
      int status = lGetUlong(aep, AN_status);

   /* If we're doing a bulk delete (i.e. deleting all jobs in the session), we
    * have a problem in that the list we have of the jobs in out session could
    * be out of sync with reality.  That means we may try to delete a job that
    * no longer exists.  Since we're just trying to kill all the jobs, it's not
    * an error if the job doesn't exist when we try to delete it.  Therefore,
    * if we see such as error, we ignore it.  Otherwise, a busy system will
    * return a DRMAA_ERRNO_INVALID_JOB error by every control(ALL, TERM). */
      if ((status != STATUS_OK) && !(delete_all && (status == STATUS_EEXIST))) {
         int ret = japi_gdi_control_error2japi_error(aep, diag, action);
         lFreeList(&alp);
         DRETURN(ret);
      }
   }

   lFreeList(&alp);

   DRETURN(DRMAA_ERRNO_SUCCESS);
}

/**
 * @brief Stops the event client
 *
 * Uses the Event Master interface to send a SHUTDOWN event to the event
 * client.
 *
 * @return 0 = OK, 1 = Error
 *
 * @note MT-NOTES: japi_stop_event_client() is MT safe (assumptions)
 */
static int japi_stop_event_client(const char *default_cell) {
   DENTER(TOP_LAYER);

   lList *alp = nullptr;
   lList *id_list = nullptr;
   char id_string[25];

   DPRINTF (("Requesting that GDI kill our event client.\n"));
   snprintf(id_string, sizeof(id_string)-1, sge_u32, japi_ec_id);
   lAddElemStr(&id_list, ID_str, id_string, ID_Type);
   alp = ocs::gdi::Client::gdi_kill(id_list, EVENTCLIENT_KILL);
   lFreeList(&id_list);
   lFreeList(&alp);

   DRETURN(0);
}
