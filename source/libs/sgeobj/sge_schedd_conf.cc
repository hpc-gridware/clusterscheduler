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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The scheduler configuration: cached access to what `qconf -msconf` sets
 *
 * The configuration itself lives in the master list as a single CULL element.
 * Reading an attribute by name would cost a search on every dispatch decision,
 * so #config_pos_type caches each attribute's field position, plus the parsed
 * form of the attributes that would otherwise be re-parsed every time.
 *
 * Two kinds of state live here and must not be confused:
 *
 * - the configuration, guarded by the `Sched_Conf_Lock` mutex and shared by
 *   every thread, and
 * - #sc_state_t, which is **thread local** and holds what one scheduling run
 *   accumulates - counters, the message stores, and per run overrides such as
 *   the one #sconf_enable_schedd_job_info sets.
 *
 * @see sge_schedd_conf.h
 */

#include <cstring>
#include <pthread.h>
#include <limits>

#include "cull/cull.h"

#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "sched/msg_schedd.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/cull_parse_util.h"

#include "msg_common.h"
#include "uti/sge.h"

/******************************************************
 *
 * All configuration values are stored in one cull
 * master list: Master_Sched_Config_List. This list
 * has only one element: the config element (SC_Type).
 *
 * The configuratin is stored in a CULL list due to the
 * current configuration handling in the system. There
 * are methods outside this module, which need complete
 * access to the list. Therefore exist two methods to
 * access the whole configuration:
 *
 * - sconf_get_config_list
 *     (returns a pointer to the cull list
 *
 * - sconf_get_config
 *      returns a const pointer the config object.
 *
 * Both methods should only be used, when there is no
 * other way. Usualy the values in the configuration
 * should be accessed via access-function. The module
 * stores the position for each config element for
 * fast access, validates the configuration, when it
 * is changed, and pre-computes some values to return
 * them in the right way.
 *
 * If the configuration is changed directly in the CULL
 * list without using
 *                "sconf_set_config"
 * , the
 *               "sconf_validate_config"
 * needs to be called to ensure that the internal data
 * is updated.
 *
 * The access is not thread save. The current implementation
 * of the configuration does not allow an easy way to
 * make it thread save. But if only access functions
 * are used it should be easy to make this module thread
 * save, when the configuration is made thread save.
 *
 ******************************************************/



/**
 * @name Compiled in defaults of the scheduler configuration
 *
 * Not every attribute has its default here. The pairs whose second member
 * starts with `_` are the same value twice: once as the text an administrator
 * sees and once as the number the code computes with. **They have to be kept
 * in sync by hand.**
 * @{
 */
#define DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME "0:7:30"        ///< `load_adjustment_decay_time`, as text
#define _DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME 7*60+30        ///< the same, in seconds
#define DEFAULT_LOAD_FORMULA                "np_load_avg"   ///< `load_formula`: sort hosts by normalised load average
#define SCHEDULE_TIME                       "0:0:15"        ///< `schedule_interval`, as text
#define _SCHEDULE_TIME                      15              ///< the same, in seconds
#define REPRIORITIZE_INTERVAL               "0:0:0"         ///< `reprioritize_interval`, as text; 0 switches it off
#define REPRIORITIZE_INTERVAL_I             0               ///< the same, in seconds
#define MAXUJOBS                            0               ///< `maxujobs`: jobs per user; 0 is unlimited
#define MAXGJOBS                            0               ///< jobs in the cluster; 0 is unlimited
#define SCHEDD_JOB_INFO                     "true"          ///< `schedd_job_info`: keep a reason message per pending job
#define DEFAULT_DURATION                    INFINITY_STR    ///< `default_duration`, as text; must stay in sync with #DEFAULT_DURATION_I
#define DEFAULT_DURATION_I                  600             ///< the same, in seconds
#define DEFAULT_DURATION_OFFSET             60              ///< seconds added to a job's duration when reserving
/** @} */

/**
 * multithreading support, thread local
 **/

static pthread_key_t sc_state_key;

static pthread_once_t sc_once = PTHREAD_ONCE_INIT;

/* a scheduling configuration structure which is stored thread local */
/**
 * @brief What one scheduling thread accumulates during a run
 *
 * Thread local, so several scheduler threads never share it. `sc_state_init`
 * resets it at the start of a run.
 */
typedef struct {
   qs_state_t queue_state;              ///< whether queue state is fully evaluated or only approximated
   bool       global_load_correction;   ///< apply load correction for jobs that just started
   int        schedd_job_info;          ///< thread local override of `schedd_job_info`; see #sconf_get_schedd_job_info
   bool       host_order_changed;       ///< the host sort order has to be recomputed
   int        last_dispatch_type;       ///< what the previous dispatch was, so the next can reuse work
   int        search_alg[SCHEDD_PE_ALG_MAX]; ///< stores the weighting for the different algorithms
   int        scheduled_pe_jobs;        ///< counts the dispatched pe jobs
   int        scheduled_fast_jobs;      ///< counts the dispatched sequential jobs
   double     decay_constant;           ///< used in the share tree
   lListElem *sme;                      ///< Job scheduling informations store if not disabled
   lListElem *tmp_sme;                  ///< the same, for the run in progress
   bool mes_schedd_info;                ///< write scheduling information into logfile
   int log_schedd_info;                 ///< write scheduling information into logfile
}  sc_state_t;

/**
 * @brief Resets the thread local structure
 *
 * resets the thread local structure, which collects information during
 * a scheduling run.
 *
 * @param state the thread local structure
 *
 * @note MT-NOTE: sc_state_init() is MT safe
 */
static void sc_state_init(sc_state_t* state)
{
   state->queue_state = QS_STATE_FULL;
   state->global_load_correction = true;
   state->schedd_job_info = SCHEDD_JOB_INFO_FALSE;
   state->host_order_changed = true;
   state->last_dispatch_type = 0;
   state->search_alg[SCHEDD_PE_LOW_FIRST] = 0;
   state->search_alg[SCHEDD_PE_HIGH_FIRST] = 0;
   state->search_alg[SCHEDD_PE_BINARY] = 0;
   state->scheduled_fast_jobs = 0;
   state->scheduled_pe_jobs = 0;
   state->decay_constant = 0.0;
   /* temp data for scheduler messages */
   state->sme = nullptr;
   state->tmp_sme = nullptr;
   state->mes_schedd_info = false;
   state->log_schedd_info = 0;
}

static void sc_state_destroy(void* state)
{
   sge_free(&state);
}

static void
sc_thread_local_once_init()
{
   pthread_key_create(&sc_state_key, &sc_state_destroy);
}


static void sc_mt_init()
{
   pthread_once(&sc_once, sc_thread_local_once_init);
}

/// Runs the one time initialiser from its constructor, so the pthread key exists before main()
class ScThreadInit {
public:
   /// Triggers the one time initialiser
   ScThreadInit() {
      sc_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static ScThreadInit sc_obj{};

/*-----*/
/* end */
/*-----*/

/**
 * @brief Parses one entry of the scheduler `params` attribute
 *
 * Adds the parsed setting to `param_list` and validates it. Returns true when
 * the entry was understood, false with a message in `answer_list` otherwise.
 *
 * @see `sconf_eval_set_profiling`
 */
typedef bool (*setParam_func)(lList *param_list, lList **answer_list, const char* param);

/**
 * specifies an array of valid parameters and its validation functions
 */
typedef struct {
      const char* name;       ///< the key as it is written in `params`, e.g. `PROFILE`
      setParam_func setParam; ///< the function that parses and validates its value
}parameters_t;

/**
 * @brief Cached CULL field positions and precomputed settings
 *
 * Looking a field up by name costs a search, and the scheduler reads these on
 * every dispatch decision. The `int` members hold the position of each
 * attribute in the scheduler configuration element; the `c_*` members hold
 * values that would otherwise be parsed again on every read.
 *
 * @note Add a default here whenever a member is added, or `calc_pos` leaves it
 *       uninitialised.
 */
typedef struct{
   pthread_mutex_t  mutex;                           ///< guards every member below; the accessors take it, never the caller
   bool empty;                                       ///< marks this structure as empty or set

   int algorithm;                                    ///< position of the `algorithm` attribute in the scheduler configuration element
   int schedule_interval;                            ///< position of the `schedule_interval` attribute in the scheduler configuration element
   int maxujobs;                                     ///< position of the `maxujobs` attribute in the scheduler configuration element
   int queue_sort_method;                            ///< position of the `queue_sort_method` attribute in the scheduler configuration element
   int job_load_adjustments;                         ///< position of the `job_load_adjustments` attribute in the scheduler configuration element
   int load_adjustment_decay_time;                   ///< position of the `load_adjustment_decay_time` attribute in the scheduler configuration element
   int load_formula;                                 ///< position of the `load_formula` attribute in the scheduler configuration element
   int schedd_job_info;                              ///< position of the `schedd_job_info` attribute in the scheduler configuration element
   int flush_submit_sec;                             ///< position of the `flush_submit_sec` attribute in the scheduler configuration element
   int flush_finish_sec;                             ///< position of the `flush_finish_sec` attribute in the scheduler configuration element
   int params;                                       ///< position of the `params` attribute in the scheduler configuration element

   int reprioritize_interval;                        ///< position of the `reprioritize_interval` attribute in the scheduler configuration element
   int halftime;                                     ///< position of the `halftime` attribute in the scheduler configuration element
   int usage_weight_list;                            ///< position of the `usage_weight_list` attribute in the scheduler configuration element
   int compensation_factor;                          ///< position of the `compensation_factor` attribute in the scheduler configuration element
   int weight_user;                                  ///< position of the `weight_user` attribute in the scheduler configuration element
   int weight_project;                               ///< position of the `weight_project` attribute in the scheduler configuration element
   int weight_department;                            ///< position of the `weight_department` attribute in the scheduler configuration element
   int weight_job;                                   ///< position of the `weight_job` attribute in the scheduler configuration element
   int weight_tickets_functional;                    ///< position of the `weight_tickets_functional` attribute in the scheduler configuration element
   int weight_tickets_share;                         ///< position of the `weight_tickets_share` attribute in the scheduler configuration element

   int weight_tickets_override;                      ///< position of the `weight_tickets_override` attribute in the scheduler configuration element
   int share_override_tickets;                       ///< position of the `share_override_tickets` attribute in the scheduler configuration element
   int share_functional_shares;                      ///< position of the `share_functional_shares` attribute in the scheduler configuration element
   int max_functional_jobs_to_schedule;              ///< position of the `max_functional_jobs_to_schedule` attribute in the scheduler configuration element
   int report_pjob_tickets;                          ///< position of the `report_pjob_tickets` attribute in the scheduler configuration element
   int max_pending_tasks_per_job;                    ///< position of the `max_pending_tasks_per_job` attribute in the scheduler configuration element
   int halflife_decay_list;                          ///< position of the `halflife_decay_list` attribute in the scheduler configuration element
   int policy_hierarchy;                             ///< position of the `policy_hierarchy` attribute in the scheduler configuration element

   int weight_ticket;                                ///< position of the `weight_ticket` attribute in the scheduler configuration element
   int weight_waiting_time;                          ///< position of the `weight_waiting_time` attribute in the scheduler configuration element
   int weight_deadline;                              ///< position of the `weight_deadline` attribute in the scheduler configuration element
   int weight_urgency;                               ///< position of the `weight_urgency` attribute in the scheduler configuration element
   int max_reservation;                              ///< position of the `max_reservation` attribute in the scheduler configuration element
   int weight_priority;                              ///< position of the `weight_priority` attribute in the scheduler configuration element
   int default_duration;                             ///< position of the `default_duration` attribute in the scheduler configuration element

   int c_is_schedd_job_info;                         ///< cached `schedd_job_info` mode, so the common case needs no parsing
   uint32_t s_duration_offset;                       ///< cached `DURATION_OFFSET` from `params`, in seconds
   lList *c_schedd_job_info_range;                   ///< cached parsed range of `schedd_job_info`
   lList *c_halflife_decay_list;                     ///< cached parsed `halflife_decay_list`
   lList *c_params;                                  ///< cached parsed `params`
   uint32_t c_default_duration;                      ///< cached `default_duration`, in seconds

   bool new_config;                                  ///< identifies an update in the configuration
}config_pos_type;

static bool schedd_profiling = false;
static bool current_serf_do_monitoring = false;
static schedd_pe_algorithm  pe_algorithm = SCHEDD_PE_AUTO;

static bool calc_pos();

static void sconf_clear_pos();

static bool is_config_set();
static const char * get_algorithm();
static const char *get_schedule_interval_str();
static const char *get_load_adjustment_decay_time_str();
static const char *reprioritize_interval_str();
static const char *get_halflife_decay_list_str();
static const char *get_default_duration_str();
static const lList *get_usage_weight_list();
static const lList *get_job_load_adjustments();
static const char* get_load_formula();

static bool
sconf_eval_set_profiling(lList *param_list, lList **answer_list, const char* param);

static bool
sconf_eval_set_monitoring(lList *param_list, lList **answer_list, const char* param);

static bool
sconf_eval_set_duration_offset(lList *param_list, lList **answer_list, const char* param);

static bool
sconf_eval_set_pe_range_alg(lList *param_list, lList **answer_list, const char* param);

static char policy_hierarchy_enum2char(policy_type_t value);

static policy_type_t policy_hierarchy_char2enum(char character);

static int policy_hierarchy_verify_value(const char* value);

/* array structure. pre-init. Make sure that a default value is added
 * when the config_pos_type is edited
 */
static config_pos_type pos = {PTHREAD_MUTEX_INITIALIZER, true,
                       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1,
                       SCHEDD_JOB_INFO_UNDEF, 0, nullptr, nullptr, nullptr, std::numeric_limits<uint32_t>::max(),

                       false};

/*
 * a list of all valid "params" parameters
 *
 * The implementation has a problem. If an entry is removed, its setting is not
 * changed, but it should be turned off. This means we have to turn everything off,
 * before we work on the params
 */
/// The keys the scheduler `params` attribute accepts, and what parses each
const parameters_t params[] = {
   {"PROFILE",         sconf_eval_set_profiling},
   {"MONITOR",         sconf_eval_set_monitoring},
   {"DURATION_OFFSET", sconf_eval_set_duration_offset},
   {"PE_RANGE_ALG",    sconf_eval_set_pe_range_alg},
   {"NONE",            nullptr},
   {nullptr,           nullptr}
};

/// The letters a `policy_hierarchy` value is spelled with: override, functional, share tree
const char *const policy_hierarchy_chars = "OFS";

/* SG: TODO: should be const */
/// The two attributes a `job_load_adjustments` entry is parsed into
int load_adjustment_fields[] = { CE_name, CE_stringval, 0 };
/* SG: TODO: should be const */
/// The two attributes a `usage_weight_list` entry is parsed into
int usage_fields[] = { UA_name, UA_value, 0 };
/// Delimiters of the `name=value,name=value` lists parsed here
const char *delis[] = {"=", ",", ""};

/**
 * @brief Resets the position information
 *
 * is needed, when a new configuration is set
 * MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
 */
static void sconf_clear_pos(){

/* set config empty */
         pos.empty = true;

/* reset positions */
         pos.algorithm = -1;
         pos.schedule_interval = -1;
         pos.maxujobs =  -1;
         pos.queue_sort_method = -1;
         pos.job_load_adjustments = -1;
         pos.load_formula =  -1;
         pos.schedd_job_info = -1;
         pos.flush_submit_sec = -1;
         pos.flush_finish_sec =  -1;
         pos.params = -1;

         pos.reprioritize_interval= -1;
         pos.halftime = -1;
         pos.usage_weight_list = -1;
         pos.compensation_factor = -1;
         pos.weight_user = -1;
         pos.weight_project = -1;
         pos.weight_department = -1;
         pos.weight_job = -1;
         pos.weight_tickets_functional = -1;
         pos.weight_tickets_share = -1;
         pos.weight_tickets_override = -1;
         pos.share_override_tickets = -1;
         pos.share_functional_shares = -1;
         pos.max_functional_jobs_to_schedule = -1;
         pos.report_pjob_tickets = -1;
         pos.max_pending_tasks_per_job =  -1;
         pos.halflife_decay_list = -1;
         pos.policy_hierarchy = -1;

         pos.weight_ticket = -1;
         pos.weight_waiting_time = -1;
         pos.weight_deadline = -1;
         pos.weight_urgency = -1;
         pos.max_reservation = -1;
         pos.weight_priority = -1;
         pos.default_duration = -1;

/* reseting cached values */
         pos.c_is_schedd_job_info = SCHEDD_JOB_INFO_UNDEF;
         lFreeList(&(pos.c_schedd_job_info_range));
         lFreeList(&(pos.c_halflife_decay_list));
         lFreeList(&(pos.c_params));
         pos.c_default_duration = DEFAULT_DURATION_I;

}

/**
 * @brief MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
 */
static bool calc_pos()
{
   bool ret = true;

   DENTER(TOP_LAYER);

   if (pos.empty) {
      const lListElem *config = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

      if (config) {
         pos.empty = false;

         ret &= (pos.algorithm = lGetPosViaElem(config, SC_algorithm, SGE_NO_ABORT )) != -1;
         ret &= (pos.schedule_interval = lGetPosViaElem(config, SC_schedule_interval, SGE_NO_ABORT)) != -1;
         ret &= (pos.maxujobs = lGetPosViaElem(config, SC_maxujobs, SGE_NO_ABORT)) != -1;
         ret &= (pos.queue_sort_method = lGetPosViaElem(config, SC_queue_sort_method, SGE_NO_ABORT)) != -1;

         ret &= (pos.job_load_adjustments = lGetPosViaElem(config,SC_job_load_adjustments, SGE_NO_ABORT )) != -1;
         ret &= (pos.load_adjustment_decay_time = lGetPosViaElem(config, SC_load_adjustment_decay_time, SGE_NO_ABORT)) != -1;
         ret &= (pos.load_formula = lGetPosViaElem(config, SC_load_formula, SGE_NO_ABORT)) != -1;
         ret &= (pos.schedd_job_info = lGetPosViaElem(config, SC_schedd_job_info, SGE_NO_ABORT)) != -1;
         ret &= (pos.flush_submit_sec = lGetPosViaElem(config, SC_flush_submit_sec, SGE_NO_ABORT)) != -1;
         ret &= (pos.flush_finish_sec = lGetPosViaElem(config, SC_flush_finish_sec, SGE_NO_ABORT)) != -1;
         ret &= (pos.params = lGetPosViaElem(config, SC_params, SGE_NO_ABORT)) != -1;

         ret &= (pos.reprioritize_interval = lGetPosViaElem(config, SC_reprioritize_interval, SGE_NO_ABORT)) != -1;
         ret &= (pos.halftime = lGetPosViaElem(config, SC_halftime, SGE_NO_ABORT)) != -1;
         ret &= (pos.usage_weight_list = lGetPosViaElem(config, SC_usage_weight_list, SGE_NO_ABORT)) != -1;

         ret &= (pos.compensation_factor = lGetPosViaElem(config, SC_compensation_factor, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_user = lGetPosViaElem(config, SC_weight_user, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_project = lGetPosViaElem(config, SC_weight_project, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_department = lGetPosViaElem(config, SC_weight_department, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_job = lGetPosViaElem(config, SC_weight_job, SGE_NO_ABORT)) != -1;

         ret &= (pos.weight_tickets_functional = lGetPosViaElem(config, SC_weight_tickets_functional, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_tickets_share = lGetPosViaElem(config, SC_weight_tickets_share, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_tickets_override = lGetPosViaElem(config, SC_weight_tickets_override, SGE_NO_ABORT)) != -1;

         ret &= (pos.share_override_tickets = lGetPosViaElem(config, SC_share_override_tickets, SGE_NO_ABORT)) != -1;
         ret &= (pos.share_functional_shares = lGetPosViaElem(config, SC_share_functional_shares, SGE_NO_ABORT)) != -1;
         ret &= (pos.max_functional_jobs_to_schedule = lGetPosViaElem(config, SC_max_functional_jobs_to_schedule, SGE_NO_ABORT)) != -1;
         ret &= (pos.report_pjob_tickets = lGetPosViaElem(config, SC_report_pjob_tickets, SGE_NO_ABORT)) != -1;
         ret &= (pos.max_pending_tasks_per_job = lGetPosViaElem(config, SC_max_pending_tasks_per_job, SGE_NO_ABORT)) != -1;
         ret &= (pos.halflife_decay_list = lGetPosViaElem(config, SC_halflife_decay_list, SGE_NO_ABORT)) != -1;
         ret &= (pos.policy_hierarchy = lGetPosViaElem(config, SC_policy_hierarchy, SGE_NO_ABORT)) != -1;

         ret &= (pos.weight_ticket = lGetPosViaElem(config, SC_weight_ticket, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_waiting_time = lGetPosViaElem(config, SC_weight_waiting_time, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_deadline = lGetPosViaElem(config, SC_weight_deadline, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_urgency = lGetPosViaElem(config, SC_weight_urgency, SGE_NO_ABORT)) != -1;
         ret &= (pos.weight_priority = lGetPosViaElem(config, SC_weight_priority, SGE_NO_ABORT)) != -1;
         ret &= (pos.max_reservation = lGetPosViaElem(config, SC_max_reservation, SGE_NO_ABORT)) != -1;
         ret &= (pos.default_duration = lGetPosViaElem(config, SC_default_duration, SGE_NO_ABORT)) != -1;
      }
      else {
         ret = false;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Overwrites the existing configuration
 *
 * - validates the new configuration
 * - precalculates some values and caches them
 * - stores the position of each attribute in the structure
 * - and sets the new configuration, if the validation worked
 * If the new configuration is a nullptr pointer, the current configuration
 * if deleted.
 *
 * @param config new configuration (SC_Type)
 * @param answer_list error messages
 *
 * @return true, if it worked SG TODO: use a internal eval config function and not the external one.
 *
 * @note MT-NOTE: is MT-safe, uses LOCK_SCHED_CONF(write)
 */
bool sconf_set_config(lList **config, lList **answer_list)
{
   lList *store = nullptr;
   bool ret = true;
   lList **master_sconf_list = nullptr;

   DENTER(TOP_LAYER);

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   master_sconf_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_SCHEDD_CONF);

#if 0
   store = Master_Sched_Config_List;
#else
   store = *master_sconf_list;
#endif

   if (config) {
#if 0
      Master_Sched_Config_List = *config;
#endif
      *master_sconf_list = *config;

      sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
      ret = sconf_validate_config_(answer_list);
      sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

      if (ret) {
         lFreeList(&store);
         *config = nullptr;
      } else {
         *master_sconf_list = store;
         if (!*master_sconf_list) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_USE_DEFAULT_CONFIG);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING);

            *master_sconf_list = lCreateList("schedd config list", SC_Type);
            lAppendElem(*master_sconf_list, sconf_create_default());

         }
         sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
         sconf_validate_config_(nullptr);
         sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
      }
   } else {
      sconf_clear_pos();
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   DRETURN(ret);
}

/**
 * @brief Does the configured `load_formula` name only existing complex entries?
 *
 * @param[out] answer_list receives the reason the formula was rejected
 * @param centry_list the complex entries the formula may refer to
 *
 * @return true when the formula is usable
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
 */
bool sconf_is_valid_load_formula(lList **answer_list, const lList *centry_list)
{
   DENTER(TOP_LAYER);

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   const lListElem *schedd_conf = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
   const char *load_formula = lGetString(schedd_conf, SC_load_formula);
   sge_strip_blanks((char *)load_formula);
   bool is_valid = validate_load_formula(load_formula, answer_list, centry_list, SGE_ATTR_LOAD_FORMULA);

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   DRETURN(is_valid);
}

/**
 * @brief Returns a default configuration
 *
 * Creates a default configuration, but does not change the current
 * active configuration. A set config has to be used to make the
 * current configuration the active one.
 *
 * @return default configuration (SC_Type)
 *
 * @note MT-NOTE: is MT-safe, does not use global variables
 */
lListElem *sconf_create_default()
{
   lListElem *ep, *added;

   DENTER(TOP_LAYER);

   ep = lCreateElem(SC_Type);

   lSetString(ep, SC_algorithm, "default");
   lSetString(ep, SC_schedule_interval, SCHEDULE_TIME);
   lSetUlong(ep, SC_maxujobs, MAXUJOBS);

   lSetUlong(ep, SC_queue_sort_method, QSM_LOAD);

   added = lAddSubStr(ep, CE_name, "np_load_avg", SC_job_load_adjustments, CE_Type);
   lSetString(added, CE_stringval, "0.50");

   lSetString(ep, SC_load_adjustment_decay_time,
                     DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME);
   lSetString(ep, SC_load_formula, DEFAULT_LOAD_FORMULA);
   lSetString(ep, SC_schedd_job_info, SCHEDD_JOB_INFO);
   lSetUlong(ep, SC_flush_submit_sec, 0);
   lSetUlong(ep, SC_flush_finish_sec, 0);
   lSetString(ep, SC_params, "none");

   lSetString(ep, SC_reprioritize_interval, REPRIORITIZE_INTERVAL);
   lSetUlong(ep, SC_halftime, 168);

   added = lAddSubStr(ep, UA_name, USAGE_ATTR_CPU, SC_usage_weight_list, UA_Type);
   lSetDouble(added, UA_value, 1.00);
   added = lAddSubStr(ep, UA_name, USAGE_ATTR_MEM, SC_usage_weight_list, UA_Type);
   lSetDouble(added, UA_value, 0.0);
   added = lAddSubStr(ep, UA_name, USAGE_ATTR_IO, SC_usage_weight_list, UA_Type);
   lSetDouble(added, UA_value, 0.0);

   lSetDouble(ep, SC_compensation_factor, 5);
   lSetDouble(ep, SC_weight_user, 0.25);
   lSetDouble(ep, SC_weight_project, 0.25);
   lSetDouble(ep, SC_weight_department, 0.25);
   lSetDouble(ep, SC_weight_job, 0.25);
   lSetUlong(ep, SC_weight_tickets_functional, 0);
   lSetUlong(ep, SC_weight_tickets_share, 0);

   lSetBool(ep, SC_share_override_tickets, true);
   lSetBool(ep, SC_share_functional_shares, true);
   lSetUlong(ep, SC_max_functional_jobs_to_schedule, 200);
   lSetBool(ep, SC_report_pjob_tickets, true);
   lSetUlong(ep, SC_max_pending_tasks_per_job, 50);
   lSetString(ep, SC_halflife_decay_list, "none");
   lSetString(ep, SC_policy_hierarchy, policy_hierarchy_chars );

   lSetDouble(ep, SC_weight_ticket, 0.5);
   lSetDouble(ep, SC_weight_waiting_time, 0.278);
   lSetDouble(ep, SC_weight_deadline, 3600000 );
   lSetDouble(ep, SC_weight_urgency, 0.5 );
   lSetUlong(ep, SC_max_reservation, 0);
   lSetDouble(ep, SC_weight_priority, 0.0 );
   lSetString(ep, SC_default_duration, DEFAULT_DURATION);

   DRETURN(ep);
}

/**
 * @brief Does the scheduler configuration refer to a complex entry?
 *
 * Checks both `job_load_adjustments` and `load_formula`. Callers use it to
 * refuse deleting a complex entry that the scheduler still needs.
 *
 * @param centry the complex entry to look for
 *
 * @return true when either attribute refers to it
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
 */
bool sconf_is_centry_referenced(const lListElem *centry)
{
   bool ret = false;
   const lListElem *sc_ep = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (sc_ep != nullptr) {
      const char *name = lGetString(centry, CE_name);
      const lList *centry_list = lGetList(sc_ep, SC_job_load_adjustments);
      const lListElem *centry_ref = lGetElemStr(centry_list, CE_name, name);

      ret = ((centry_ref != nullptr)? true : false);

      if (!ret) {
         if (load_formula_is_centry_referenced(lGetString(sc_ep, SC_load_formula), centry)) {
            ret = true;
         }
      }
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return ret;
}

/**
 * @brief The `load_adjustment_decay_time` attribute of the scheduler configuration
 *
 * That is how long a load adjustment keeps decaying.
 *
 * @return the configured value
 *
 * @note MT-NOTE:  is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const char * get_load_adjustment_decay_time_str()
{
   const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (pos.load_adjustment_decay_time != -1) {
      return lGetPosString(sc_ep, pos.load_adjustment_decay_time );
   }
   else {
      return DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME;
   }
}

/**
 * @brief The `load_adjustment_decay_time` attribute of the scheduler configuration
 *
 * That is how long a load adjustment keeps decaying.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_load_adjustment_decay_time()
{
   uint32_t uval;
   const char *time = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   time = get_load_adjustment_decay_time_str();

   if (!extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, time, nullptr, 0, 0, true)) {
      uval = _DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return uval;
}

/**
 * @brief The `job_load_adjustments` attribute of the scheduler configuration
 *
 * That is load values a newly dispatched job is assumed to add.
 *
 * @return returns a copy, needs to be freed
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
lList *sconf_get_job_load_adjustments() {
   lList *load_adjustments = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   load_adjustments = lCopyList("load_adj_copy", get_job_load_adjustments());

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return load_adjustments;
}

/**
 * @brief The `load_formula` attribute of the scheduler configuration
 *
 * That is expression the hosts are sorted by.
 *
 * @return the configured value
 *
 * @note MT-NOTE:  is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const lList *get_job_load_adjustments()
{
   const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (pos.job_load_adjustments != -1) {
      return lGetPosList(sc_ep, pos.job_load_adjustments);
   } else {
      return nullptr;
   }
}


/**
 * @brief The `load_formula` attribute of the scheduler configuration
 *
 * That is the expression the hosts are sorted by.
 *
 * @return this is a copy of the load formula, the caller has to free it
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
char* sconf_get_load_formula() {
   char *formula = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   formula = sge_strdup(formula, get_load_formula());

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return formula;
}

/**
 * @brief The `queue_sort_method` attribute of the scheduler configuration
 *
 * That is in which order queues are tried when a job is dispatched.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const char* get_load_formula()
{
   const lListElem *sc_ep =  lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (pos.load_formula != -1) {
      return lGetPosString(sc_ep, pos.load_formula);
   }
   else {
      return DEFAULT_LOAD_FORMULA;
   }
}

/**
 * @brief The `queue_sort_method` attribute of the scheduler configuration
 *
 * That is in which order queues are tried when a job is dispatched.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_queue_sort_method()
{
   const lListElem *sc_ep =  nullptr;
   uint32_t sort_method = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.queue_sort_method != -1) {
      sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      sort_method = lGetPosUlong(sc_ep, pos.queue_sort_method);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return sort_method;
}

/**
 * @brief The `maxujobs` attribute of the scheduler configuration
 *
 * That is how many jobs one user may have running at a time; 0 is unlimited.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_maxujobs()
{
   uint32_t jobs = MAXUJOBS;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.maxujobs != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      jobs = lGetPosUlong(sc_ep, pos.maxujobs );
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return jobs;
}

/**
 * @brief The `schedule_interval` attribute of the scheduler configuration
 *
 * That is how often the scheduler runs.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is not MT-safe, the caller needs to hold the LOCK_SCHED_CONF(read)
 */
static const char *get_schedule_interval_str()
{
   if (pos.schedule_interval != -1) {
      const lListElem *sc_ep =  lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      if (sc_ep != nullptr) {
         return lGetPosString(sc_ep, pos.schedule_interval);
      } else {
         return nullptr;
      }
   }
   else {
      return SCHEDULE_TIME;
   }
}

/**
 * @brief The `schedule_interval` attribute of the scheduler configuration
 *
 * That is how often the scheduler runs.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_schedule_interval() {
   uint32_t uval = _SCHEDULE_TIME;
   const char *time = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   time = get_schedule_interval_str();
   if (!extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, time, nullptr, 0, 0, true) ) {
         uval = _SCHEDULE_TIME;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return uval;
}


/**
 * @brief The `reprioritize_interval` attribute of the scheduler configuration
 *
 * That is how often running jobs are repriorized.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const char *reprioritize_interval_str()
{
   if (pos.reprioritize_interval!= -1) {
      const lListElem *sc_ep =  lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      return lGetPosString(sc_ep, pos.reprioritize_interval);
   }
   else {
      return REPRIORITIZE_INTERVAL;
   }
}

/**
 * @brief The `reprioritize_interval` attribute of the scheduler configuration
 *
 * That is how often running jobs are repriorized.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_reprioritize_interval() {
   uint32_t uval = REPRIORITIZE_INTERVAL_I;
   const char *time = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   time = reprioritize_interval_str();

   if (!extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME,time, nullptr, 0 , 0, true)) {
      uval = REPRIORITIZE_INTERVAL_I;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return uval;
}

/**
 * @brief Start collecting scheduler reason messages on this thread
 *
 * The setting is thread local, so one scheduling run can collect messages
 * without changing what any other thread does.
 *
 * @note MT-NOTE: is thread save, uses thread local storage
 */
void sconf_enable_schedd_job_info()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->schedd_job_info = SCHEDD_JOB_INFO_TRUE;
}

/**
 * @brief Stop collecting scheduler reason messages on this thread
 *
 * @note MT-NOTE: is thread save, uses thread local storage
 */
void sconf_disable_schedd_job_info()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->schedd_job_info = SCHEDD_JOB_INFO_FALSE;
}

/**
 * @brief Returns the alg to use for pe-range jobs
 *
 * It checks for the alg. to use. If the user did not set a custom one, it
 * will evaluate the weights and return the most successful one.
 *
 * @return pe-range alg.
 *
 * @note MT-NOTE: sconf_best_pe_alg() is MT safe
 */
schedd_pe_algorithm sconf_best_pe_alg()
{
   schedd_pe_algorithm alg;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   alg = pe_algorithm;
   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (alg != SCHEDD_PE_AUTO) {
      return alg;
   }
   else {
      GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);

      if ((sc_state->search_alg[SCHEDD_PE_BINARY] >= sc_state->search_alg[SCHEDD_PE_LOW_FIRST]) &&
          (sc_state->search_alg[SCHEDD_PE_BINARY] >= sc_state->search_alg[SCHEDD_PE_HIGH_FIRST])) {
         return SCHEDD_PE_BINARY;
      }
      else if (sc_state->search_alg[SCHEDD_PE_HIGH_FIRST] >= sc_state->search_alg[SCHEDD_PE_LOW_FIRST]){
         return SCHEDD_PE_HIGH_FIRST;
      }
      else {
         return SCHEDD_PE_LOW_FIRST;
      }
   }
}

/**
 * @brief Updates the weights for the different algorithms
 *
 * updates the weights for the different algorithms. Since the alg. with
 * the bigest number is taken, the numbers are negative. It uses the running
 * averages to ensure, that the numbers are not getting to big and that the
 * scheduler can reakt to changes.
 *
 * @param runs number of runs it would have taken with the bin search alg.
 * @param current number of runs it took
 * @param max max runs
 *
 * @note MT-NOTE: sconf_update_pe_alg() is MT safe
 */
void sconf_update_pe_alg(int runs, int current, int max)
{
   const int HISTORY = 66;
   const int PRESENT = 34;

   if (max > 1) {
      int low_run = current+1;
      int high_run = max - current+1;
      GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);

      /* we calculate 2 digits behind the commma*/
      runs *= 100;
      low_run *= 100;
      high_run *= 100;

      sc_state->search_alg[SCHEDD_PE_BINARY]     = (sc_state->search_alg[SCHEDD_PE_BINARY]     * HISTORY) / 100;
      sc_state->search_alg[SCHEDD_PE_HIGH_FIRST] = (sc_state->search_alg[SCHEDD_PE_HIGH_FIRST] * HISTORY) / 100;
      sc_state->search_alg[SCHEDD_PE_LOW_FIRST]  = (sc_state->search_alg[SCHEDD_PE_LOW_FIRST]  * HISTORY) / 100;

      sc_state->search_alg[SCHEDD_PE_BINARY]     -= runs     * PRESENT / 100;
      sc_state->search_alg[SCHEDD_PE_LOW_FIRST]  -= low_run  * PRESENT / 100;
      sc_state->search_alg[SCHEDD_PE_HIGH_FIRST] -= high_run * PRESENT / 100;
   }
}

/**
 * @brief Current weighting of one parallel job search algorithm
 *
 * The counter is thread local, so each scheduling thread sees its own.
 *
 * @param alg the algorithm to ask about
 *
 * @return its weight; the scheduler picks the algorithm with the highest one
 */
int sconf_get_pe_alg_value(schedd_pe_algorithm alg)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->search_alg[alg];
}

/**
 * @brief Count one dispatched sequential job
 *
 * The counter is thread local, so each scheduling thread sees its own.
 */
void sconf_inc_fast_jobs()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->scheduled_fast_jobs++;
}

/**
 * @brief Sequential jobs dispatched in this scheduling run
 *
 * The counter is thread local, so each scheduling thread sees its own.
 *
 * @return the count since the last #sconf_reset_jobs
 */
int sconf_get_fast_jobs()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->scheduled_fast_jobs;
}

/**
 * @brief Count one dispatched parallel job
 *
 * The counter is thread local, so each scheduling thread sees its own.
 */
void sconf_inc_pe_jobs()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->scheduled_pe_jobs++;
}

/**
 * @brief Parallel jobs dispatched in this scheduling run
 *
 * The counter is thread local, so each scheduling thread sees its own.
 *
 * @return the count since the last #sconf_reset_jobs
 */
int sconf_get_pe_jobs()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->scheduled_pe_jobs;
}

/**
 * @brief Start counting dispatched jobs from zero again
 *
 * The counter is thread local, so each scheduling thread sees its own.
 */
void sconf_reset_jobs()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->scheduled_fast_jobs = 0;
   sc_state->scheduled_pe_jobs = 0;
}

/**
 * @brief Whether scheduler reason messages are collected, and for which jobs
 *
 * The configured `schedd_job_info` wins. Only when it is
 * `SCHEDD_JOB_INFO_FALSE` does the thread local setting - what
 * #sconf_enable_schedd_job_info changed - take effect, so a single scheduling
 * run can collect messages even though the cluster has them switched off.
 *
 * @return one of the `SCHEDD_JOB_INFO_*` values
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read) and local storage
 */
uint32_t sconf_get_schedd_job_info() {
   uint32_t info = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   info = pos.c_is_schedd_job_info;

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (info == SCHEDD_JOB_INFO_FALSE) {
      GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
      info = sc_state->schedd_job_info;
   }

   return info;
}

/**
 * @brief The `schedd_job_info` attribute of the scheduler configuration
 *
 * That is jobs the scheduler keeps a reason message for.
 *
 * @return returns a copy, needs to be freed
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
lList *sconf_get_schedd_job_info_range() {
   lList *range_copy = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   range_copy = lCopyList("copy_range", pos.c_schedd_job_info_range);

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return range_copy;
}

/**
 * @brief Does `schedd_job_info` ask for reason messages for this job?
 *
 * @param job_number the job to ask about
 *
 * @return true when the configured range contains it
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
bool sconf_is_id_in_schedd_job_info_range(uint32_t job_number)
{
   bool is_in_range = false;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   is_in_range = range_list_is_id_within(pos.c_schedd_job_info_range, job_number);

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is_in_range;
}

/**
 * @brief The `usage_weight_list` attribute of the scheduler configuration
 *
 * That is weight of each usage attribute in the share tree policy.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is not MT-safe, the caller needs the LOCK_SCHED_CONF(read)
 */
static const char * get_algorithm()
{
   if (pos.algorithm!= -1) {
      const lListElem *sc_ep =  lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      return lGetPosString(sc_ep, pos.algorithm);
   }
   else {
      return "default";
   }
}


/**
 * @brief The `usage_weight_list` attribute of the scheduler configuration
 *
 * That is the weight of each usage attribute in the share tree policy.
 *
 * @return returns a copy, needs to be freed
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
lList *sconf_get_usage_weight_list()
{
   lList *weight_list = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   weight_list = lCopyList("copy_weight", get_usage_weight_list());

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight_list;
}


/**
 * @brief The `weight_user` attribute of the scheduler configuration
 *
 * That is weight of the user share in the functional policy.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const lList *get_usage_weight_list()
{
   const lListElem *sc_ep =  lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (pos.usage_weight_list != -1) {
      return lGetPosList(sc_ep, pos.usage_weight_list );
   }
   else {
      return nullptr;
   }
}



/**
 * @brief The `weight_user` attribute of the scheduler configuration
 *
 * That is the weight of the user share in the functional policy.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_user()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_user!= -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_user);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_department` attribute of the scheduler configuration
 *
 * That is weight of the department share in the functional policy.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_department()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_department != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_department);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_project` attribute of the scheduler configuration
 *
 * That is weight of the project share in the functional policy.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_project()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_project != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_project);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_job` attribute of the scheduler configuration
 *
 * That is weight of the job share in the functional policy.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_job()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_job != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_job);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_tickets_share` attribute of the scheduler configuration
 *
 * That is total number of share tree tickets.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_weight_tickets_share()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_tickets_share != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosUlong(sc_ep, pos.weight_tickets_share );
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_tickets_functional` attribute of the scheduler configuration
 *
 * That is total number of functional tickets.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_weight_tickets_functional()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_tickets_functional != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosUlong(sc_ep, pos.weight_tickets_functional);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `halftime` attribute of the scheduler configuration
 *
 * That is half life of accumulated usage in the share tree policy, in hours.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_halftime()
{
   const lListElem *sc_ep = nullptr;
   uint32_t halftime = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.halftime != -1) {
      sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      halftime = lGetPosUlong(sc_ep, pos.halftime);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return halftime;
}


/**
 * @brief Set the `weight_tickets_override` attribute of the scheduler configuration
 *
 * That is total number of override tickets.
 *
 * @param active
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(write)
 */
void sconf_set_weight_tickets_override(uint32_t active)
{
   lListElem *sc_ep = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   sc_ep = lFirstRW(*ocs::DataStore::get_master_list_rw(SGE_TYPE_SCHEDD_CONF));

   if (pos.weight_tickets_override!= -1) {
      lSetPosUlong(sc_ep, pos.weight_tickets_override, active);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return;
}

/**
 * @brief The `weight_tickets_override` attribute of the scheduler configuration
 *
 * That is total number of override tickets.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_weight_tickets_override()
{
   uint32_t tickets = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_tickets_override!= -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      tickets = lGetPosUlong(sc_ep, pos.weight_tickets_override);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return tickets;

}

/**
 * @brief The `compensation_factor` attribute of the scheduler configuration
 *
 * That is how strongly the share tree policy compensates for past under-use.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_compensation_factor()
{
   double factor = 1;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.compensation_factor!= -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      factor = lGetPosDouble(sc_ep, pos.compensation_factor);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return factor;
}

/**
 * @brief The `weight_ticket` attribute of the scheduler configuration
 *
 * That is weight of the ticket term in the job priority.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_ticket()
{
   double  weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_ticket != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_ticket);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The three job priority weights, read in one lock
 *
 * Reading them separately would take the lock three times and could mix
 * weights from two configurations.
 *
 * @param[out] ticket weight of the ticket term
 * @param[out] urgency weight of the urgency term
 * @param[out] priority weight of the POSIX priority term
 *
 * @note All three are left untouched when the configuration does not have all
 *       three attributes.
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
 */
void sconf_get_weight_ticket_urgency_priority(double *ticket, double *urgency, double *priority)
{
   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_ticket != -1 && pos.weight_urgency != -1 && pos.weight_priority != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      *ticket = lGetPosDouble(sc_ep, pos.weight_ticket);
      *urgency = lGetPosDouble(sc_ep, pos.weight_urgency);
      *priority = lGetPosDouble(sc_ep, pos.weight_priority);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
}

/**
 * @brief The `weight_waiting_time` attribute of the scheduler configuration
 *
 * That is weight of the waiting time term in the job priority.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_waiting_time()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_waiting_time != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_waiting_time);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_deadline` attribute of the scheduler configuration
 *
 * That is weight of the deadline term in the job priority.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_deadline()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_deadline != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_deadline);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief The `weight_urgency` attribute of the scheduler configuration
 *
 * That is weight of the urgency term in the job priority.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_urgency()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_urgency != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_urgency);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}

/**
 * @brief Max reservation tuning parameter
 *
 * Tuning parameter.
 * Returns maximum number of reservations that should be done by
 * scheduler. If 0 is returned this no single job shall get a reservation
 * and assignments are to be made for 'now' only.
 *
 * @return Max. number of reservations
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_max_reservations() {
   uint32_t max_res = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (!pos.empty && (pos.max_reservation != -1)) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      max_res = lGetPosUlong(sc_ep, pos.max_reservation);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return max_res;
}

/**
 * @brief Return default_duration string
 *
 * Returns default duration string from scheduler configuration.
 *
 * @return the raw attribute text, not the parsed number
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const char *get_default_duration_str()
{
   const lListElem *sc_ep =  lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (pos.schedule_interval != -1) {
      return lGetPosString(sc_ep, pos.default_duration);
   }
   else {
      return DEFAULT_DURATION;
   }
}
/**
 * @brief The `weight_priority` attribute of the scheduler configuration
 *
 * That is weight of the POSIX priority term in the job priority.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
double sconf_get_weight_priority()
{
   double weight = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.weight_priority != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      weight = lGetPosDouble(sc_ep, pos.weight_priority);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return weight;
}


/**
 * @brief The `share_override_tickets` attribute of the scheduler configuration
 *
 * That is whether override tickets are shared among a job array.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
bool sconf_get_share_override_tickets()
{
   bool is_share = false;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.share_override_tickets != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      is_share = lGetPosBool(sc_ep, pos.share_override_tickets) ? true : false;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is_share;
}
/**
 * @brief The `share_functional_shares` attribute of the scheduler configuration
 *
 * That is whether functional shares are shared among a job array.
 *
 * @return the configured value
 */
bool sconf_get_share_functional_shares()
{
   bool is_share = true;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.share_functional_shares != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      is_share = lGetPosBool(sc_ep, pos.share_functional_shares) ? true : false;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is_share;
}

/**
 * @brief The `report_pjob_tickets` attribute of the scheduler configuration
 *
 * That is whether per job ticket values are reported to the master.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
bool sconf_get_report_pjob_tickets()
{
   bool is_report = true;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.report_pjob_tickets!= -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      is_report = lGetPosBool(sc_ep, pos.report_pjob_tickets) ? true : false;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is_report;
}

/**
 * @brief The `flush_submit_sec` attribute of the scheduler configuration
 *
 * That is seconds the scheduler waits after a submit before it runs.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_flush_submit_sec()
{
   const lListElem *sc_ep = nullptr;
   uint32_t flush_sec = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.flush_submit_sec != -1) {
      sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      if (sc_ep != nullptr) {
         flush_sec = lGetPosUlong(sc_ep, pos.flush_submit_sec);
      }
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return flush_sec;
}

/**
 * @brief The `flush_finish_sec` attribute of the scheduler configuration
 *
 * That is seconds the scheduler waits after a job finished before it runs.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_flush_finish_sec()
{
   const lListElem *sc_ep = nullptr;
   uint32_t flush_sec = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.flush_finish_sec!= -1) {
      sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      if (sc_ep != nullptr) {
         flush_sec = lGetPosUlong(sc_ep, pos.flush_finish_sec);
      }
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return flush_sec;
}


/**
 * @brief The `max_functional_jobs_to_schedule` attribute of the scheduler configuration
 *
 * That is how many pending jobs the functional policy ranks.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_max_functional_jobs_to_schedule()
{
   uint32_t amount = 200;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.max_functional_jobs_to_schedule != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      amount = lGetPosUlong(sc_ep, pos.max_functional_jobs_to_schedule);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return amount;
}

/**
 * @brief The `max_pending_tasks_per_job` attribute of the scheduler configuration
 *
 * That is how many tasks of one array job the scheduler considers.
 *
 * @return the configured value
 *
 * @note MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_max_pending_tasks_per_job()
{
   uint32_t max_pending = 50;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   if (pos.max_pending_tasks_per_job != -1) {
      const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
      max_pending = lGetPosUlong(sc_ep, pos.max_pending_tasks_per_job);
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return max_pending;
}

/**
 * @brief The `halflife_decay_list` attribute of the scheduler configuration
 *
 * That is per attribute half life overriding `halftime`.
 *
 * @return the configured value
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static const char *get_halflife_decay_list_str()
{
   const lListElem *sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   if (pos.halflife_decay_list != -1) {
      return lGetPosString(sc_ep, pos.halflife_decay_list);
   }
   else {
      return "none";
   }
}

/**
 * @brief The `halflife_decay_list` attribute of the scheduler configuration
 *
 * That is a per attribute half life overriding `halftime`.
 *
 * @return the configured value
 *
 * @note MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
 */
lList* sconf_get_halflife_decay_list(){
   lList *decay_list = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   decay_list = lCopyList("copy_decay_list",pos.c_halflife_decay_list);

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return decay_list;
}

/**
 * @brief Checks, if a configuration exists
 *
 * @return true, if a configuration exists
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
 */
static bool is_config_set()
{
   const lListElem *sc_ep = nullptr;

   if (*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)) {
      sc_ep = lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
   }

   return ((sc_ep != nullptr) ? true : false);
}

/**
 * @brief Checks, if a configuration exists
 *
 * @return true, if a configuration exists
 *
 * @note MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
 */
bool sconf_is()
{
   bool is = false;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   is = is_config_set();

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is;
}


/**
 * @brief Returns a config object
 *
 * @return a copy of the current config object NOTE DO NOT USE this method. ONLY when there is NO OTHER way. All config settings can be accessed via access function.
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
lListElem *sconf_get_config()
{
   lListElem *config = nullptr;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   config = lCopyElem(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)));

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return config;
}

/**
 * @brief Returns a pointer to the list, which contains
 *
 * @return a copy of the config list NOTE DO NOT USE this method. ONLY when there is NO OTHER way. All config settings can be accessed via access function. The config can be set via sconf_set_config(...) IMPORTANT If you modify the configuration by directly accessing, you have to call sconf_validate_config_ afterwards to ensure, that the caches reflect your changes.
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
lList *sconf_get_config_list()
{
   lList *copy_list = nullptr;

   DENTER(TOP_LAYER);
   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   copy_list = lCopyList("sched_conf_copy", *ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   DRETURN(copy_list);
}

/**
 * @brief Prints the current configuration to the INFO stream
 */
void sconf_print_config(){

   char tmp_buffer[1024];
   uint32_t uval;
   const char *s;
   const lList *lval= nullptr;
   double dval;

   DENTER(TOP_LAYER);

   if (!sconf_is()){
      ERROR(SFNMAX, MSG_SCONF_NO_CONFIG);
      DRETURN_VOID;
   }

   sconf_validate_config_(nullptr);

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   /* --- SC_algorithm */
   s = get_algorithm();
   INFO(MSG_ATTRIB_USINGXASY_SS , s, "algorithm");

   /* --- SC_schedule_interval */
   s = get_schedule_interval_str();
   INFO(MSG_ATTRIB_USINGXFORY_SS , s, "schedule_interval");

   /* --- SC_load_adjustment_decay_time */
   s = get_load_adjustment_decay_time_str();
   INFO(MSG_ATTRIB_USINGXFORY_SS, s, "load_adjustment_decay_time");

   /* --- SC_load_formula */
   INFO(MSG_ATTRIB_USINGXFORY_SS, get_load_formula(), "load_formula");

   /* --- SC_schedd_job_info */
   INFO(MSG_ATTRIB_USINGXFORY_SS, lGetString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)), SC_schedd_job_info), "schedd_job_info");

   /* --- SC_params */
   s=lGetString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)), SC_params);
   INFO(MSG_READ_PARAM_S, s);

   /* --- SC_reprioritize_interval */
   s = reprioritize_interval_str();
   INFO(MSG_ATTRIB_USINGXFORY_SS, s, "reprioritize_interval");

   /* --- SC_usage_weight_list */
   uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer), get_usage_weight_list(), usage_fields, delis, 0);
   INFO(MSG_ATTRIB_USINGXFORY_SS, tmp_buffer, "usage_weight_list");

   /* --- SC_halflife_decay_list_str */
   s = get_halflife_decay_list_str();
   INFO(MSG_ATTRIB_USINGXFORY_SS, s, "halflife_decay_list");

   /* --- SC_policy_hierarchy */
   s = lGetString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)), SC_policy_hierarchy);
   INFO(MSG_ATTRIB_USINGXFORY_SS, s, "policy_hierarchy");

   /* --- SC_job_load_adjustments */
   lval = get_job_load_adjustments();
   uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer), lval, load_adjustment_fields, delis, 0);
   INFO(MSG_ATTRIB_USINGXFORY_SS, tmp_buffer, "job_load_adjustments");

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   /* --- SC_maxujobs */
   uval = sconf_get_maxujobs();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "maxujobs");

   /* --- SC_queue_sort_method (was: SC_sort_seq_no) */
   uval = sconf_get_queue_sort_method();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "queue_sort_method");

   /* --- SC_flush_submit_sec */
   uval = sconf_get_flush_submit_sec();
   INFO(MSG_ATTRIB_USINGXFORY_US, uval, "flush_submit_sec");

   /* --- SC_flush_finish_sec */
   uval = sconf_get_flush_finish_sec();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval , "flush_finish_sec");

   /* --- SC_halftime */
   uval = sconf_get_halftime();
   INFO(MSG_ATTRIB_USINGXFORY_US ,  uval, "halftime");

   /* --- SC_compensation_factor */
   dval = sconf_get_compensation_factor();
   INFO(MSG_ATTRIB_USINGXFORY_6FS, dval, "compensation_factor");

   /* --- SC_weight_user */
   dval = sconf_get_weight_user();
   INFO(MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_user");

   /* --- SC_weight_project */
   dval = sconf_get_weight_project();
   INFO(MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_project");

   /* --- SC_weight_department */
   dval = sconf_get_weight_department();
   INFO(MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_department");

   /* --- SC_weight_job */
   dval = sconf_get_weight_job();
   INFO(MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_job");

   /* --- SC_weight_tickets_functional */
   uval = sconf_get_weight_tickets_functional();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "weight_tickets_functional");

   /* --- SC_weight_tickets_share */
   uval = sconf_get_weight_tickets_share();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "weight_tickets_share");

   /* --- SC_share_override_tickets */
   uval = sconf_get_share_override_tickets();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "share_override_tickets");

   /* --- SC_share_functional_shares */
   uval = sconf_get_share_functional_shares();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "share_functional_shares");

   /* --- SC_max_functional_jobs_to_schedule */
   uval = sconf_get_max_functional_jobs_to_schedule();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "max_functional_jobs_to_schedule");

   /* --- SC_report_job_tickets */
   uval = sconf_get_report_pjob_tickets();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "report_pjob_tickets");

   /* --- SC_max_pending_tasks_per_job */
   uval = sconf_get_max_pending_tasks_per_job();
   INFO(MSG_ATTRIB_USINGXFORY_US,  uval, "max_pending_tasks_per_job");

   /* --- SC_weight_ticket */
   dval = sconf_get_weight_ticket();
   INFO(MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_ticket");

   /* --- SC_weight_waiting_time */
   dval = sconf_get_weight_waiting_time();
   INFO(MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_waiting_time");

   /* --- SC_weight_deadline */
   dval = sconf_get_weight_deadline();
   INFO(MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_deadline");

   /* --- SC_weight_urgency */
   dval = sconf_get_weight_urgency();
   INFO(MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_urgency");

   /* --- SC_weight_priority */
   dval = sconf_get_weight_priority();
   INFO(MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_priority");

   /* --- SC_max_reservation */
   dval = sconf_get_max_reservations();
   INFO(MSG_ATTRIB_USINGXFORY_6FS,  dval, "max_reservation");

   DRETURN_VOID;
}

/**
 * @brief Has the scheduler configuration changed since it was last read?
 *
 * @return true while a new configuration has not been picked up yet
 *
 * @note MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
 */
bool sconf_is_new_config() {
   bool is_new_config = false;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   is_new_config = pos.new_config;

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is_new_config;
}

/**
 * @brief MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(write)
 */
void sconf_reset_new_config()
{
   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   pos.new_config = false;

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
}

/**
 * @brief Validates the current config
 *
 * validates the current config and updates the caches.
 *
 * @param answer_list error messages
 *
 * @return false for invalid scheduler configuration
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read/write)
 */
bool sconf_validate_config_(lList **answer_list)
{
   char tmp_buffer[1024], tmp_error[1024];
   uint32_t uval;
   const char *s;
   const lList *lval= nullptr;
   bool ret = true;
   uint32_t max_reservation = 0;

   DENTER(TOP_LAYER);

   if (!sconf_is()){
      DPRINTF("sconf_validate: no config to validate\n");
      DRETURN(true);
   }

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   sconf_clear_pos();

   pos.new_config = true;

   if (!calc_pos()){
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_INCOMPLETE_SCHEDD_CONFIG);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

 /* --- SC_params */
   {
      const char *sparams = lGetString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)), SC_params);
      char *s = nullptr;

      /* the implementation has a problem. If an entry is removed, its setting is not
         changed, but it should be turned off. This means we have to turn everything off,
         before we work on the params */
      schedd_profiling = false;
      current_serf_do_monitoring = false;
      pos.s_duration_offset = DEFAULT_DURATION_OFFSET;
      pe_algorithm = SCHEDD_PE_AUTO;

      if (sparams) {
         struct saved_vars_s *context = nullptr;

         if (pos.c_params == nullptr) {
            pos.c_params = lCreateList("params", PARA_Type);
         }
         for (s=sge_strtok_r(sparams, PARAMS_DELIMITER, &context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &context)) {
            int i = 0;
            bool added = false;
            for(i=0; params[i].name ;i++ ){
               if (!strncasecmp(s, params[i].name, sizeof(params[i].name)-1)){
                  if (params[i].setParam) {
                     ret &= params[i].setParam(pos.c_params, answer_list, s);
                  }
                  added = true;
               }
            }
            if (!added){
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_UNKNOWN_PARAM_S, s);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }
         sge_free_saved_vars(context);
      } else {
         lSetString(lFirstRW(*ocs::DataStore::get_master_list_rw(SGE_TYPE_SCHEDD_CONF)), SC_params, "none");
      }
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   max_reservation = sconf_get_max_reservations();

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   /* --- SC_algorithm */
   s = get_algorithm();
   if ( !s || (strcmp(s, "default") && strcmp(s, "simple_sched") && strncmp(s, "ext_", 4))) {
      if (!s)
         s = "not defined";
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_ALGORITHMNOVALIDNAME_S, s);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /* --- SC_schedule_interval */
   s = get_schedule_interval_str();
   if (!s || !extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, s, tmp_error, sizeof(tmp_error), 0, true) ) {
      if (!s)
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", "not defined");
      else
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", tmp_error);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret =  false;
   }

   /* --- SC_load_adjustment_decay_time */
   s = get_load_adjustment_decay_time_str();
   if (!s || !extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, s, tmp_error, sizeof(tmp_error), 0, true)) {
      if (!s) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", "not defined");
      }
      else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS, "load_adjustment_decay_time", tmp_error);
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

  /* --- SC_schedd_job_info */
   {
      char buf[4096];
      char* key = nullptr;
      int ikey = 0;
      lList *rlp=nullptr, *alp=nullptr;
      const char *schedd_info = lGetString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)), SC_schedd_job_info);

      if (schedd_info == nullptr){
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTRIB_SCHEDDJOBINFONOVALIDPARAM );
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      else {
         struct saved_vars_s *context = nullptr;
         strcpy(buf, schedd_info);
         /* on/off or watch a set of jobs */
         key = sge_strtok_r(buf, " \t", &context);
         if (!strcmp("true", key))
            ikey = SCHEDD_JOB_INFO_TRUE;
         else if (!strcmp("false", key))
            ikey = SCHEDD_JOB_INFO_FALSE;
         else if (!strcmp("job_list", key))
            ikey = SCHEDD_JOB_INFO_JOB_LIST;
         else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTRIB_SCHEDDJOBINFONOVALIDPARAM );
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         /* check list of groups */
         if (ikey == SCHEDD_JOB_INFO_JOB_LIST) {
            key = sge_strtok_r(nullptr, "\n", &context);
            range_list_parse_from_string(&rlp, &alp, key, false, false,
                                         INF_NOT_ALLOWED);
            if (rlp == nullptr) {
               lFreeList(&alp);
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTRIB_SCHEDDJOBINFONOVALIDJOBLIST);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
            else{
               pos.c_is_schedd_job_info = ikey;
               pos.c_schedd_job_info_range = rlp;
            }
         }
         else{
            pos.c_is_schedd_job_info = ikey;
         }
         sge_free_saved_vars(context);
      }
   }

   /* --- SC_reprioritize_interval */
   s = reprioritize_interval_str();
   if (s == nullptr || !extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, s, tmp_error,
                                              sizeof(tmp_error), 0, true)) {
      if (s == nullptr) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", "not defined");
      }
      else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "reprioritize_interval", tmp_error);
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /* --- SC_halflife_decay_list_str */
   {
      s = get_halflife_decay_list_str();
      if (s && (strcasecmp(s, "none") != 0)) {
         lList *halflife_decay_list = nullptr;
         lListElem *ep = nullptr;
         const char *s0 = nullptr;
         const char *s1 = nullptr;
         const char *s2 = nullptr;
         const char *s3 = nullptr;
         double value;
         struct saved_vars_s *sv1=nullptr;
         struct saved_vars_s *sv2=nullptr;
         s0 = s;
         for(s1=sge_strtok_r(s0, ":", &sv1); s1 != nullptr;
             s1=sge_strtok_r(nullptr, ":", &sv1)) {
            if (((s2=sge_strtok_r(s1, "=", &sv2)) != nullptr) &&
                ((s3=sge_strtok_r(nullptr, "=", &sv2)) != nullptr) &&
                (sscanf(s3, "%lf", &value) == 1)) {
               ep = lAddElemStr(&halflife_decay_list, UA_name, s2, UA_Type);
               lSetDouble(ep, UA_value, value);
            }
            sge_free_saved_vars(sv2);
         }
         sge_free_saved_vars(sv1);

         if (lGetNumberOfElem(halflife_decay_list) == 0) {
            answer_list_add(answer_list, MSG_GDI_INVALIDHALFLIFE_DECAY, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }

         pos.c_halflife_decay_list = halflife_decay_list;
      }
   }

   /* --- SC_policy_hierarchy */
   {
      const char *value_string = lGetString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)),
                                            SC_policy_hierarchy);
      if (value_string) {
         if (policy_hierarchy_verify_value(value_string) != 0) {
            answer_list_add(answer_list, MSG_GDI_INVALIDPOLICYSTRING, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
            lSetString(lFirstRW(*ocs::DataStore::get_master_list_rw(SGE_TYPE_SCHEDD_CONF)), SC_policy_hierarchy, policy_hierarchy_chars);
         }
      }
      else {
         if (!s)
         value_string = "not defined";
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "policy hierarchy", value_string);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   /* --- SC_max_reservation and SC_default_duration */
   {
      const char *s = get_default_duration_str();

      if (s == nullptr || !extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, s, tmp_error,
                                                 sizeof(tmp_error), 1, true) ) {
         if (s == nullptr) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "default_duration", "not defined");
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_XISNOTAY_SS , "default_duration", tmp_error);
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret =  false;
      } else {
         /* ensure we get a non-zero/non-infinity duration default in reservation scheduling mode */
         if (max_reservation != 0 && uval == 0) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_RR_REQUIRES_DEFAULT_DURATION);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
         } else {
            pos.c_default_duration = uval;
         }
      }
   }

   /* --- SC_usage_weight_list */
   if (uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer),
       get_usage_weight_list(), usage_fields, delis, 0) < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SCHEDD_USAGE_WEIGHT_LIST_S, tmp_error);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /* --- SC_job_load_adjustments */
   lval = get_job_load_adjustments();
   if (uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer), lval, load_adjustment_fields, delis, 0) < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SCHEDD_JOB_LOAD_ADJUSTMENTS_S, s);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   /* --- SC_load_formula */
   {
      const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
      if (master_centry_list != nullptr && !sconf_is_valid_load_formula(answer_list, master_centry_list)) {
         ret = false;
      }
   }

   /* --- max_pending_tasks_per_job */
   if (sconf_get_max_pending_tasks_per_job() == 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTRIB_WRONG_SETTING_SS, "max_pending_tasks_per_job", ">0");
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   DRETURN(ret);
}


/**
 * @brief Validate a given configuration
 *
 * @param answer_list error messages
 * @param config config to validate
 *
 * @return true, if the config is valid SG TODO: needs cleanup!!
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(write)
 */
bool sconf_validate_config(lList **answer_list, lList *config){
   const lList *store = nullptr;
   bool ret = true;

   DENTER(TOP_LAYER);

   if (config){
      sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
      store = *ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF);
      *ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF) = config;
      sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

      ret = sconf_validate_config_(answer_list);

      sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
      *ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF) = store;
      sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

      sconf_validate_config_(nullptr);
   }

   DRETURN(ret);
}

/**
 * @brief Verify a policy string
 *
 * The function tests whether the given policy string (value) is i
 * valid.
 *
 * @param value policy string
 *
 * @return 0 -> OK 1 -> ERROR: one char is at least twice in "value" 2 -> ERROR: invalid char in "value" 3 -> ERROR: value == nullptr
 *
 * @note MT-NOTE:  is MT safe, uses only the given data.
 */
static int policy_hierarchy_verify_value(const char* value)
{
   int ret = 0;

   DENTER(TOP_LAYER);

   if (value != nullptr) {
      if (strcmp(value, "") && strcasecmp(value, "NONE")) {
         int is_contained[POLICY_VALUES];
         int i;

         for (i = 0; i < POLICY_VALUES; i++) {
            is_contained[i] = 0;
         }

         for (i = 0; i < (int)strlen(value); i++) {
            char c = value[i];
            int index = policy_hierarchy_char2enum(c);

            if (is_contained[index]) {
               DPRINTF("character \'%c\' is contained at least twice\n", c);
               ret = 1;
               break;
            }

            is_contained[index] = 1;

            if (is_contained[INVALID_POLICY]) {
               DPRINTF("Invalid character \'%c\'\n", c);
               ret = 2;
               break;
            }
         }
      }
   }
   else {
      ret = 3;
   }

   DRETURN(ret);
}

/**
 * @brief Fill the policy array
 *
 * Fill the policy "array" according to the characters given by
 * "value".
 * value == "FODS":
 *    policy_hierarchy_t array[4] = {
 *        {FUNCTIONAL_POLICY, 1},
 *        {OVERRIDE_POLICY, 1},
 *        {DEADLINE_POLICY, 1},
 *        {SHARE_TREE_POLICY, 1}
 *    };
 * value == "FS":
 *    policy_hierarchy_t array[4] = {
 *        {FUNCTIONAL_POLICY, 1},
 *        {SHARE_TREE_POLICY, 1},
 *        {OVERRIDE_POLICY, 0},
 *        {DEADLINE_POLICY, 0}
 *    };
 * value == "OFS":
 *    policy_hierarchy_t hierarchy[4] = {
 *        {OVERRIDE_POLICY, 1},
 *        {FUNCTIONAL_POLICY, 1},
 *        {SHARE_TREE_POLICY, 1},
 *        {DEADLINE_POLICY, 0}
 *    };
 * value == "NONE":
 *    policy_hierarchy_t hierarchy[4] = {
 *        {OVERRIDE_POLICY, 0},
 *        {FUNCTIONAL_POLICY, 0},
 *        {SHARE_TREE_POLICY, 0},
 *        {DEADLINE_POLICY, 0}
 *    };
 *
 * @param[out] array receives one entry per policy, in the configured order;
 *                   it must have `POLICY_VALUES` entries
 *
 * @note The hierarchy is read from the `policy_hierarchy` attribute - `"NONE"`
 *       or any combination of the policy names' first letters, e.g. `"OFSD"`.
 *       An earlier version took it as a parameter instead.
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
 */
void sconf_ph_fill_array(policy_hierarchy_t array[])
{
   int is_contained[POLICY_VALUES];
   int index = 0;
   int i;
   const char *policy_hierarchy_string = nullptr;

   DENTER(TOP_LAYER);

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   policy_hierarchy_string = lGetPosString(lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)),
                                           pos.policy_hierarchy);

   for (i = 0; i < POLICY_VALUES; i++) {
      is_contained[i] = 0;
      array[i].policy = INVALID_POLICY;
   }
   if (policy_hierarchy_string != nullptr && strcmp(policy_hierarchy_string, "") &&
       strcasecmp(policy_hierarchy_string, "NONE")) {

      for (i = 0; i < (int)strlen(policy_hierarchy_string); i++) {
         char c = policy_hierarchy_string[i];
         policy_type_t enum_value = policy_hierarchy_char2enum(c);

         is_contained[enum_value] = 1;
         array[index].policy = enum_value;
         array[index].dependent = 1;
         index++;
      }
   }

   for (i = INVALID_POLICY + 1; i < LAST_POLICY_VALUE; i++) {
      if (!is_contained[i])  {
         array[index].policy = (policy_type_t)i;
         array[index].dependent = 0;
         index++;
      }
   }

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   DRETURN_VOID;
}

/**
 * @brief Return value for a policy char
 *
 * This function returns a enum value for the first letter of a
 * policy name.
 *
 * @param character "O", "F" or "S"
 *
 * @return enum value not thread safe, needs LOCK_SCHED_CONF(read)
 *
 * @note MT-NOTE:
 */
static policy_type_t policy_hierarchy_char2enum(char character)
{
   const char *pointer;
   policy_type_t ret;

   pointer = strchr(policy_hierarchy_chars, character);
   if (pointer != nullptr) {
      ret = (policy_type_t)((pointer - policy_hierarchy_chars) + 1);
   } else {
      ret = INVALID_POLICY;
   }
   return ret;
}


/**
 * @brief Print hierarchy array in the debug output
 *
 * @param array the hierarchy to print, as #sconf_ph_fill_array filled it
 */
void sconf_ph_print_array(policy_hierarchy_t array[])
{
   int i;

   DENTER(TOP_LAYER);

   for (i = INVALID_POLICY + 1; i < LAST_POLICY_VALUE; i++) {
      char character = policy_hierarchy_enum2char(array[i-1].policy);

      DPRINTF("policy: %c; dependent: %d\n", character, array[i-1].dependent);
   }

   DRETURN_VOID;
}

/**
 * @brief Return policy char for a value
 *
 * Returns the first letter of a policy name corresponding to the
 * enum "value".
 *
 * @param value enum value
 *
 * @return "O", "F", "S", "D" not thread safe, needs LOCK_SCHED_CONF(read)
 *
 * @note MT-NOTE:
 */
static char policy_hierarchy_enum2char(policy_type_t value)
{
   return policy_hierarchy_chars[value - 1];
}

/**
 * @brief Parse the `PROFILE=` entry of the scheduler `params` attribute
 *
 * Accepts `1`/`TRUE` and `0`/`FALSE`, case insensitively, and records the
 * result as a `profile` entry in the parsed parameter list. Anything else is
 * rejected.
 *
 * @param[in,out] param_list the parsed parameters, extended by one entry
 * @param[out] answer_list receives the message naming the bad setting
 * @param param the raw `PROFILE=...` text
 *
 * @return true when the setting was understood
 */
static bool sconf_eval_set_profiling(lList *param_list, lList **answer_list, const char* param){
   bool ret = true;
   lListElem *elem = nullptr;
   DENTER(TOP_LAYER);

   schedd_profiling = false;

   if (!strncasecmp(param, "PROFILE=1", sizeof("PROFILE=1")-1) ||
       !strncasecmp(param, "PROFILE=TRUE", sizeof("PROFILE=TRUE")-1) ) {
      schedd_profiling = true;
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "profile");
      lSetString(elem, PARA_value, "true");
   }
   else if (!strncasecmp(param, "PROFILE=0", sizeof("PROFILE=0")-1) ||
            !strncasecmp(param, "PROFILE=FALSE", sizeof("PROFILE=FALSE")-1) ) {
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "profile");
      lSetString(elem, PARA_value, "false");
   }
   else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INVALID_PARAM_SETTING_S, param);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (elem){
      lAppendElem(param_list, elem);
   }

   DRETURN(ret);
}

/**
 * @brief Control SERF on/off via MONITOR param
 *
 * The MONITOR param allows schedule entry recording facility module
 * be switched on/off.
 *
 * @param param_list
 * @param answer_list
 * @param param
 *
 * @return parsing error
 *
 * @note MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
 */
static bool sconf_eval_set_monitoring(lList *param_list, lList **answer_list, const char* param){
   bool ret = true;
   lListElem *elem = nullptr;
   const char mon_true[] = "MONITOR=TRUE", mon_one[] = "MONITOR=1";
   const char mon_false[] = "MONITOR=FALSE", mon_zero[] = "MONITOR=0";
   bool do_monitoring = false;

   DENTER(TOP_LAYER);

   if (!strncasecmp(param, mon_one, sizeof(mon_one)-1) ||
       !strncasecmp(param, mon_true, sizeof(mon_true)-1) ) {
      do_monitoring = true;
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "monitor");
      lSetString(elem, PARA_value, "true");
   }
   else if (!strncasecmp(param, mon_zero, sizeof(mon_zero)-1) ||
            !strncasecmp(param, mon_false, sizeof(mon_false)-1) ) {
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "monitor");
      lSetString(elem, PARA_value, "false");
   }
   else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INVALID_PARAM_SETTING_S, param);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (elem){
      lAppendElem(param_list, elem);
   }

   current_serf_do_monitoring = do_monitoring;

   DRETURN(ret);
}

static bool sconf_eval_set_duration_offset(lList *param_list, lList **answer_list, const char* param)
{
   uint32_t uval;
   char *s;

   if (!(s=strchr((char *)param, '=')) ||
       !extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::TIME, ++s, nullptr, 0, 0, true)) {
      pos.s_duration_offset = DEFAULT_DURATION_OFFSET;
      return false;
   }
   pos.s_duration_offset = uval;

   return true;
}

/**
 * @brief Parses the sched. param
 *
 * @return true, if successful
 *
 * @note MT-NOTE: sconf_eval_set_pe_range_alg() is not MT safe, caller needs LOCK_SCHED_CONF(write)
 */
static bool sconf_eval_set_pe_range_alg(lList *param_list, lList **answer_list, const char* param)
{
   char *s;

   DENTER(TOP_LAYER);

   if ((s=strchr((char *)param, '=')) != nullptr) {
      s++;
      if (strncasecmp(s, "auto", sizeof("auto")-1)  == 0) {
          pe_algorithm = SCHEDD_PE_AUTO;
      }
      else if (strncasecmp(s, "least", sizeof("least")-1)  == 0) {
          pe_algorithm = SCHEDD_PE_LOW_FIRST;
      }
      else if (strncasecmp(s, "bin", sizeof("bin")-1)  == 0) {
          pe_algorithm = SCHEDD_PE_BINARY;
      }
      else if (strncasecmp(s, "highest", sizeof("highest")-1)  == 0) {
          pe_algorithm = SCHEDD_PE_HIGH_FIRST;
      }
      else {
            DRETURN(false);
      }
      DRETURN(true);
   }

   DRETURN(false);
}

/*
   QS_STATE_FULL
      All debitations caused by running jobs are in effect.
   QS_STATE_EMPTY
      We ignore all debitations caused by running jobs.
      Ignore all but static load values.
*/
/**
 * @brief Set how the queue state is evaluated during this run
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param qs_state `QS_STATE_FULL` counts every debitation of running jobs;
 *                 `QS_STATE_EMPTY` ignores them and all but static load values
 */
void sconf_set_qs_state(qs_state_t qs_state)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->queue_state = qs_state;
}

/**
 * @brief How the queue state is evaluated during this run
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the current setting
 */
qs_state_t sconf_get_qs_state()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->queue_state;
}
/**
 * @brief Switch load correction for just started jobs on or off
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param flag true to apply the correction
 */
void sconf_set_global_load_correction(bool flag)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->global_load_correction = flag;
}
/**
 * @brief Is load correction for just started jobs applied?
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the current setting
 */
bool sconf_get_global_load_correction()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->global_load_correction;
}

/**
 * @brief The cached `default_duration`, in seconds
 *
 * Read from the cache rather than the configuration element, so a dispatch
 * decision does not have to parse the attribute again.
 *
 * @return the duration in seconds
 */
uint32_t sconf_get_default_duration()
{
   return pos.c_default_duration;
}

/**
 * @brief Does the host sort order have to be recomputed?
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return true when it is stale
 */
bool sconf_get_host_order_changed()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->host_order_changed;
}

/**
 * @brief Record whether the host sort order became stale
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param changed true when it has to be recomputed
 */
void sconf_set_host_order_changed(bool changed)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->host_order_changed = changed;
}

/**
 * @brief What the previous dispatch was
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the recorded type, which lets the next dispatch reuse work
 */
int sconf_get_last_dispatch_type()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->last_dispatch_type;
}

/**
 * @brief Record what this dispatch was
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param last the type to record
 */
void sconf_set_last_dispatch_type(int last)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->last_dispatch_type = last;
}

/**
 * @brief Set the share tree decay constant for this run
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param decay the constant, derived from `halftime`
 */
void sconf_set_decay_constant(double decay)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->decay_constant = decay;
}
/**
 * @brief The share tree decay constant of this run
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the constant
 */
double sconf_get_decay_constant()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->decay_constant;
}

/**
 * @brief Start or stop collecting scheduler reason messages on this thread
 *
 * Switching collection **on** is ignored while the messaging framework is not
 * initialised, i.e. while either message store is still nullptr.
 *
 * @param newval true to collect messages
 */
void sconf_set_mes_schedd_info(bool newval)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   if (newval) {
      if (sc_state->sme == nullptr || sc_state->tmp_sme == nullptr) {
         /* if one of the values is nullptr the messaging framework is initialized
            in this case just ignore the activate request */
         return;
      }
   }
   sc_state->mes_schedd_info = newval;
}

/**
 * @brief Are scheduler messages being collected?
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the current setting
 */
bool sconf_get_mes_schedd_info()
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->mes_schedd_info;
}

/**
 * @brief Switch writing scheduler messages to the log file on or off
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param bval non-zero to log them
 */
void schedd_mes_set_logging(int bval) {
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->log_schedd_info = bval;
}

/**
 * @brief Are scheduler messages written to the log file?
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return non-zero when they are
 */
int schedd_mes_get_logging() {
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->log_schedd_info;
}

/**
 * @brief The store the scheduler reason messages are collected in
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the element, or nullptr when messaging is not initialised
 */
lListElem *sconf_get_sme() {
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->sme;
}

/**
 * @brief Set the store the scheduler reason messages are collected in
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param sme the element to use
 */
void sconf_set_sme(lListElem *sme) {
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->sme = sme;
}

/**
 * @brief The message store of the run in progress
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @return the element, or nullptr when messaging is not initialised
 */
lListElem *sconf_get_tmp_sme() {
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   return sc_state->tmp_sme;
}

/**
 * @brief Set the message store of the run in progress
 *
 * The setting is thread local, so each scheduling thread has its own.
 *
 * @param sme the element to use
 */
void sconf_set_tmp_sme(lListElem *sme) {
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key);
   sc_state->tmp_sme = sme;
}

/**
 * @brief The cached `DURATION_OFFSET` from the scheduler `params`
 *
 * Seconds added to a job's duration when the scheduler reserves resources for
 * it, to absorb the time between the decision and the job actually starting.
 *
 * @return the offset in seconds
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
 */
uint32_t sconf_get_duration_offset()
{
   uint32_t offset = 0;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   offset = pos.s_duration_offset;

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return offset;
}

/**
 * @brief Retrieve whether SERF is active or not
 *
 * Returns whether SERF is active or not
 *
 * @return true = on false = off
 *
 * @note MT-NOTE: is MT safe, uses LOCK_SCHED_CONF(read)
 *
 * @note Actually belongs to sge_serf.c but this would cause a link dependency
 *       libsgeobj -> libschedd !!
 */
bool serf_get_active()
{
   bool is = false;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   is = current_serf_do_monitoring;

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return is;
}

/**
 * @brief Is scheduler profiling switched on?
 *
 * Set through the `PROFILE=` entry of the scheduler `params`.
 *
 * @return true while profiling is on
 *
 * @note MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
 */
bool sconf_get_profiling()
{
   bool profiling = false;

   sge_mutex_lock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);

   profiling = schedd_profiling;

   sge_mutex_unlock("Sched_Conf_Lock", "", __LINE__, &pos.mutex);
   return profiling;
}
