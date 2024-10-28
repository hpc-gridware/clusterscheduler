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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *  Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_thread_ctrl.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "comm/lists/cl_thread.h"
#include "comm/lists/cl_errors.h"
#include "comm/cl_commlib.h"

#include "sgeobj/sge_daemonize.h"
#include "gdi/sge_gdi.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/ocs_Session.h"
#include "sgeobj/cull/sge_event_request_EVR_L.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "configuration_qmaster.h"   /* TODO: bad dependency!! */
#include "evm/ocs_event_master.h"
#include "evm/sge_event_master.h"
#include "uti/sge.h"

#include "msg_common.h"
#include "msg_evmlib.h"
#include "msg_qmaster.h"

/*
 ***** transaction handling implementation ************
 *
 * Well, one cannot really call the transaction implementation
 * transaction handling. First of all, there is no role
 * back. Second, there is only one transaction in the 
 * whole system, one no way to distinguish between the 
 * events added by the thread, which opend the transaction, 
 * and other threads just adding events.
 *
 * We need the current implementation for the scheduler
 * protocol. All gdi requets have to be atomic from the
 * scheduler point of view. Therefore we have a second
 * (the transaction) list to add events, and put them
 * into the send queue, when the gdi request has been
 * handled.
 * 
 * Important variables:
 *  pthread_mutex_t  transaction_mutex;  
 *  lList            *transaction_events;
 *  pthread_mutex_t  t_add_event_mutex;
 *  bool             is_transaction;   
 *
 * related methods:
 *  sge_set_commit_required()
 *  sge_commit() 
 *
 ******************************************************
 */


/*
 ***** subscription_t definition **********************
 *
 * This is a subscription entry in a two dimensional 
 * definition. The first dimension is for the event
 * clients and changes when the event client subscription
 * changes. The second dimension is of fixed size and 
 * contains one element for each posible event.
 *
 ******************************************************
 */
typedef struct {
      bool         subscription; /* true -> the event is subscribed           */
      bool         blocked;      /* true -> no events will be accepted before */
                                 /*   the total update is issued                */
      bool         flush;        /* true -> flush is set                      */
      u_long32     flush_time;   /* seconds how much the event can be delayed */
      lCondition   *where;       /* where filter                              */
      lDescr       *descr;       /* target list descriptor                    */
      lEnumeration *what;        /* limits the target element                 */
} subscription_t;

/****** Eventclient/Server/-Event_Client_Server_Defines ************************
*  NAME
*     Defines -- Constants used in the module
*
*  SYNOPSIS
*     #define EVENT_DELIVERY_INTERVAL_S 1 
*     #define EVENT_DELIVERY_INTERVAL_N 0 
*     #define EVENT_ACK_MIN_TIMEOUT 600
*     #define EVENT_ACK_MAX_TIMEOUT 1200
*
*  FUNCTION
*     EVENT_DELIVERY_INTERVAL_S is the event delivery interval. It is set in seconds.
*     EVENT_DELIVERY_INTERVAL_N same thing but in nano seconds.
*
*     EVENT_ACK_MIN/MAX_TIMEOUT is the minimum/maximum timeout value for an event
*     client sending the acknowledge for the delivery of events.
*     The real timeout value depends on the event delivery interval for the 
*     event client (10 * event delivery interval).
*******************************************************************************/
#define EVENT_DELIVERY_INTERVAL_S 1
#define EVENT_DELIVERY_INTERVAL_N 0
#define EVENT_ACK_MIN_TIMEOUT 600
#define EVENT_ACK_MAX_TIMEOUT 1200

/*
 *******************************************************************
 *
 * The next three array are important for lists, which can be
 * subscribed by the event client and which contain a sub-list
 * that can be subscribed by itself again (such as the: job list
 * with the JAT_Task sub list, or the cluster queue list with the
 * queue instances sub-list)
 * All lists, which follow the same structure have to be defined
 * in the special construct.
 *
 * EVENT_LIST:
 *  Contains all events for the main list, which delivers also
 *  the sub-list. 
 *
 * FIELD_LIST:
 *  Contains all attributes in the main list, which contain the
 *  sub-list in question.
 *
 * SOURCE_LIST:
 *  Contains the sub-scription events for the sub-list, which also
 *  contains the filter for the sub-list.
 *
 *
 * This construct and its functions are limited to one sub-scribable
 * sub-list per main list. If multiple sub-lists can be subsribed, the
 * construct has to be extended.
 *
 * ISSUES:
 *  1416: the sgeE_PETASK_* list events are not handled correctly during
 *        total update. It works fine with all other events. The problem
 *        is, that an update of the jobs filters for job and ja_task, but
 *        not for PE tasks.
 *
 * SEE ALSO:
 *     evm/sge_event_master/list_select()
 *     evm/sge_event_master/elem_select() 
 *  and
 *     evm/sge_event_master/add_list_event
 *     evm/sge_event_master/add_event
 *
 ********************************************************************
 */
#define LIST_MAX 3

const int EVENT_LIST[LIST_MAX][6] = {
   {sgeE_JOB_LIST, sgeE_JOB_ADD, sgeE_JOB_DEL, sgeE_JOB_MOD, sgeE_JOB_MOD_SCHED_PRIORITY, -1},
   {sgeE_CQUEUE_LIST, sgeE_CQUEUE_ADD, sgeE_CQUEUE_DEL, sgeE_CQUEUE_MOD, -1, -1},
   {sgeE_JATASK_ADD, sgeE_JATASK_DEL, sgeE_JATASK_MOD, -1, -1, -1 }
};

const int FIELD_LIST[LIST_MAX][3] = {
   {JB_ja_tasks, JB_ja_template, -1},
   {CQ_qinstances, -1, -1},
   {JAT_task_list, -1, -1}
};

const int SOURCE_LIST[LIST_MAX][3] = {
   {sgeE_JATASK_MOD, sgeE_JATASK_ADD, -1},
   {sgeE_QINSTANCE_ADD, sgeE_QINSTANCE_MOD, -1},
   {sgeE_PETASK_ADD, -1, -1}
};

/*
 *****************************************************
 *
 * The next two array are needed for blocking events
 * for a given client, when a total update is pending
 * for it.
 *
 * The total_update_events array defines all list events,
 * which are used during a total update.
 *
 * The block_events contain all events, which are blocked
 * during a total update.
 *
 *  SEE ALSO:
 *   blockEvents
 *   total_update 
 *   add_list_event_direct
 *****************************************************
 */

#define total_update_eventsMAX 21

const int total_update_events[total_update_eventsMAX + 1] = {sgeE_ADMINHOST_LIST,
                                       sgeE_CALENDAR_LIST,
                                       sgeE_CKPT_LIST,
                                       sgeE_CENTRY_LIST,
                                       sgeE_CONFIG_LIST,
                                       sgeE_EXECHOST_LIST,
                                       sgeE_JOB_LIST,
                                       sgeE_JOB_SCHEDD_INFO_LIST,
                                       sgeE_MANAGER_LIST,
                                       sgeE_OPERATOR_LIST,
                                       sgeE_PE_LIST,
                                       sgeE_CQUEUE_LIST,
                                       sgeE_SCHED_CONF,
                                       sgeE_SUBMITHOST_LIST,
                                       sgeE_USERSET_LIST,
                                       sgeE_NEW_SHARETREE,
                                       sgeE_PROJECT_LIST,
                                       sgeE_USER_LIST,
                                       sgeE_HGROUP_LIST,
                                       sgeE_RQS_LIST,
                                       sgeE_AR_LIST,
                                       -1};

const int block_events[total_update_eventsMAX][9] = {
   {sgeE_ADMINHOST_ADD, sgeE_ADMINHOST_DEL, sgeE_ADMINHOST_MOD, -1, -1, -1, -1, -1, -1}, 
   {sgeE_CALENDAR_ADD,  sgeE_CALENDAR_DEL,  sgeE_CALENDAR_MOD,  -1, -1, -1, -1, -1, -1},
   {sgeE_CKPT_ADD,      sgeE_CKPT_DEL,      sgeE_CKPT_MOD,      -1, -1, -1, -1, -1, -1},
   {sgeE_CENTRY_ADD,    sgeE_CENTRY_DEL,    sgeE_CENTRY_MOD,    -1, -1, -1, -1, -1, -1}, 
   {sgeE_CONFIG_ADD,    sgeE_CONFIG_DEL,    sgeE_CONFIG_MOD,    -1, -1, -1, -1, -1, -1}, 
   {sgeE_EXECHOST_ADD,  sgeE_EXECHOST_DEL,  sgeE_EXECHOST_MOD,  -1, -1, -1, -1, -1, -1},
   {sgeE_JOB_ADD, sgeE_JOB_DEL, sgeE_JOB_MOD, sgeE_JOB_MOD_SCHED_PRIORITY, sgeE_JOB_USAGE, sgeE_JOB_FINAL_USAGE, sgeE_JOB_FINISH, -1, -1}, 
   {sgeE_JOB_SCHEDD_INFO_ADD, sgeE_JOB_SCHEDD_INFO_DEL, sgeE_JOB_SCHEDD_INFO_MOD, -1, -1, -1, -1, -1, -1},
   {sgeE_MANAGER_ADD,         sgeE_MANAGER_DEL,         sgeE_MANAGER_MOD,         -1, -1, -1, -1, -1, -1}, 
   {sgeE_OPERATOR_ADD,        sgeE_OPERATOR_DEL,        sgeE_OPERATOR_MOD,        -1, -1, -1, -1, -1, -1},
   {sgeE_PE_ADD,              sgeE_PE_DEL,              sgeE_PE_MOD,              -1, -1, -1, -1, -1, -1},
   {sgeE_CQUEUE_ADD,          sgeE_CQUEUE_DEL,          sgeE_CQUEUE_MOD, sgeE_QINSTANCE_ADD, sgeE_QINSTANCE_DEL, sgeE_QINSTANCE_MOD, sgeE_QINSTANCE_SOS, sgeE_QINSTANCE_USOS, -1}, 
   {-1, -1, -1, -1, -1, -1, -1, -1},
   {sgeE_SUBMITHOST_ADD,      sgeE_SUBMITHOST_DEL,      sgeE_SUBMITHOST_MOD,      -1, -1, -1, -1, -1, -1},
   {sgeE_USERSET_ADD,         sgeE_USERSET_DEL,         sgeE_USERSET_MOD,         -1, -1, -1, -1, -1, -1},
   {-1, -1, -1, -1, -1, -1, -1, -1}, 
   {sgeE_PROJECT_ADD,         sgeE_PROJECT_DEL,         sgeE_PROJECT_MOD,         -1, -1, -1, -1, -1, -1},
   {sgeE_USER_ADD,            sgeE_USER_DEL,            sgeE_USER_MOD,            -1, -1, -1, -1, -1, -1},
   {sgeE_HGROUP_ADD,          sgeE_HGROUP_DEL,          sgeE_HGROUP_MOD,          -1, -1, -1, -1, -1, -1},
   {sgeE_RQS_ADD,             sgeE_RQS_DEL,             sgeE_RQS_MOD,             -1, -1, -1, -1, -1, -1},
   {sgeE_AR_ADD,              sgeE_AR_DEL,              sgeE_AR_MOD,              -1, -1, -1, -1, -1, -1}
};


/*
 *******************************************************************
 *
 * Some events have to be delivered even so they have no data left
 * after filtering for them. These are for example all update list
 * events. 
 * The ensure, that is is done as fast as posible, we define an 
 * array of the size of the number of events, we have a init function
 * which sets the events which will be updated. To add a new event
 * one has only to update that function.
 * Events which do not contain any data are not affected. They are
 * always delivered.
 *
 * SEE ALSO:
 * - Array:
 *    SEND_EVENTS
 *
 * - Init function:
 *    evm/sge_event_master/sge_init_send_events()
 *
 ******************************************************************
 */

static bool SEND_EVENTS[sgeE_EVENTSIZE];

event_master_control_t Event_Master_Control = {
   PTHREAD_MUTEX_INITIALIZER,       /* mutex */
   PTHREAD_COND_INITIALIZER,        /* cond_var */
   PTHREAD_MUTEX_INITIALIZER,       /* cond_mutex */
   false,                           /* delivery_signaled */
   0,                               /* max_event_clients */
   false,                           /* is_prepare_shutdown */
   nullptr,                            /* clients */
   nullptr,                            /* client_ids */
   nullptr,                            /* requests */
   PTHREAD_MUTEX_INITIALIZER,       /* request_mutex */
   0                                /* transaction_key */
};

static void       init_send_events(); 
static void       flush_events(lListElem*, int);
static void       total_update(lListElem*, u_long64 gdi_session);
static void       build_subscription(lListElem*);
static void       remove_event_client(lListElem **client, bool lock_event_master);
static void       check_send_new_subscribed_list(const subscription_t*, 
                                                 const subscription_t*, lListElem*,
                                                 ev_event event);
static int        eventclient_subscribed(const lListElem *, ev_event, const char*);
static int        purge_event_list(lList* aList, u_long32 event_number); 

static lListElem* sge_create_event(u_long32, u_long64, ev_event, u_long32, u_long32, const char*, const char*, lList*);
static bool       add_list_event_for_client(u_long32, u_long64, ev_event, u_long32, u_long32, const char*, const char*, const char*, lList*, u_long64);
static void       add_list_event_direct(lListElem *event_client, lListElem *event, bool copy_event);
static void       total_update_event(lListElem *event_client, ev_event type, bool new_subscription, u_long64 gdi_session);
static bool       list_select(subscription_t*, int, lList**, lList*, const lCondition*, const lEnumeration*, const lDescr*, bool);
static lListElem* elem_select(subscription_t*, lListElem*, const int[], const lCondition*, const lEnumeration*, const lDescr*, int);    
static lListElem* eventclient_list_locate_by_adress(const char*, const char*, u_long32);
static const lDescr* getDescriptorL(subscription_t*, const lList*, int);
static lListElem* get_event_client(u_long32 id);
static u_long32   allocate_new_dynamic_id(lList **answer_list);
static void       free_dynamic_id(lList **answer_list, u_long32 id);

static void blockEvents(lListElem *event_client, ev_event ev_type, bool isBlock);

static void
sge_event_master_destroy_transaction_store(void *transaction_store)
{
   event_master_transaction_t *t_store = (event_master_transaction_t *)transaction_store;
   lFreeList(&(t_store->transaction_requests));
   sge_free(&t_store);
}

static void
sge_event_master_init_transaction_store(event_master_transaction_t *t_store)
{
   t_store->is_transaction = false;
   t_store->transaction_requests = lCreateListHash("Event Master Requests", EVR_Type, false);
}

void sge_cleanup_event_master_control(void *arg) {
   // there shouldn't be anybody left accessing the event master control anymore, but just to be sure
   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   // remove leftover event clients
   lListElem *event_client;
   while ((event_client = lFirstRW(Event_Master_Control.clients)) != nullptr) {
      DEBUG("event client " sge_u32 " (state: " sge_u32 ") is still registered at event master shutdown",
            lGetUlong(event_client, EV_id), lGetUlong(event_client, EV_state));
      remove_event_client(&event_client, false);
   }

   // free data structures and pthread resources
   lFreeList(&Event_Master_Control.clients);
   lFreeList(&Event_Master_Control.client_ids);
   lFreeList(&Event_Master_Control.requests);
   pthread_cond_destroy(&Event_Master_Control.cond_var);
   pthread_mutex_destroy(&Event_Master_Control.cond_mutex);
   pthread_mutex_destroy(&Event_Master_Control.request_mutex);

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   pthread_mutex_destroy(&Event_Master_Control.mutex);
}

/****** Eventclient/Server/sge_add_event_client() ******************************
*  NAME
*     sge_add_event_client() -- register a new event client
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     int 
*     sge_add_event_client(lListElem *clio, lList **alpp, lList **eclpp, 
*     char *ruser, char *rhost) 
*
*  FUNCTION
*     Registeres a new event client. 
*     If it requested a dynamic id, a new id is created and assigned.
*     If it is a special client(with fixed id) and an event client
*     with this id already exists, the old instance is deleted and the
*     new one registered.
*     If the registration succeeds, the event client is sent all data
*     (sgeE*_LIST events) according to its subscription.
*
*  INPUTS
*     lListElem *clio - the event client object used as registration data
*     lList **alpp    - answer list pointer for answer to event client
*     lList **eclpp   - list pointer to return new event client object
*     char *ruser     - user that tries to register an event client
*     char *rhost     - host on which the event client runs
*     event_client_update_func_t update_func - for internal event clients
*     monitoring_t monitor - monitoring handle
*
*  RESULT
*     int - AN_status value. STATUS_OK on success, else error code
*
*  NOTES
*     MT-NOTE: sge_add_event_client() is MT safe, it uses the global lock and
*              internal ones.
*
*******************************************************************************/
static void sge_event_master_process_add_event_client(const lListElem *request, monitoring_t *monitor)
{
   /* to be implemented later on - handling the internal event clients could become a little bit tricky */
}

int sge_add_event_client(const sge_gdi_packet_class_t *packet, lListElem *clio, lList **alpp, lList **eclpp,
                         event_client_update_func_t update_func, void *update_func_arg)
{
   lListElem *ep = nullptr;
   u_long32 id;
   u_long32 ed_time;
   const char *name;
   const char *host;
   const char *commproc;
   u_long32 commproc_id;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);

   DENTER(TOP_LAYER);

   id = lGetUlong(clio, EV_id);
   name = lGetString(clio, EV_name);
   ed_time = lGetUlong(clio, EV_d_time);
   host = lGetHost(clio, EV_host);
   commproc = lGetString(clio, EV_commproc);
   commproc_id = lGetUlong(clio, EV_commid);

   /* an event client must have a name */
   if (name == nullptr) {
      name = "unnamed";
      lSetString(clio, EV_name, name);
   }

   /* check event client object structure */
   if (lCompListDescr(lGetElemDescr(clio), EV_Type) != 0) {
      ERROR(SFNMAX, MSG_EVE_INCOMPLETEEVENTCLIENT);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_DENIED);
   }

   /* EV_ID_ANY is 0, therefore the compare is always true (Irix complained) */
   if (id >= EV_ID_FIRST_DYNAMIC) { /* invalid request */
      ERROR(MSG_EVE_ILLEGALIDREGISTERED_U, sge_u32c(id));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_ESEMANTIC);
   }

   if (lGetBool(clio, EV_changed) && lGetList(clio, EV_subscribed) == nullptr) {
      ERROR(SFNMAX, MSG_EVE_INVALIDSUBSCRIPTION);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_ESEMANTIC);
   }

   /* Acquire the event master mutex - we access the event client list */
   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   if (Event_Master_Control.is_prepare_shutdown) {
      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
      ERROR(SFNMAX, MSG_EVE_QMASTERISGOINGDOWN);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);      
      DRETURN(STATUS_ESEMANTIC);
   }

   if (id == EV_ID_ANY) {   /* qmaster shall give id dynamically */
      /* Try to find an event client with the very same commd
         address triplet. If it exists the "new" event client must
         be the result of a reconnect after a timeout that happend at
         client side. We delete the old event client. */
      ep = eventclient_list_locate_by_adress(host, commproc, commproc_id);

      if (ep != nullptr) {
         ERROR(MSG_EVE_CLIENTREREGISTERED_SSSU, name, host, commproc, sge_u32c(commproc_id));

         /* delete old event client entry, and we already hold the lock! */
         remove_event_client(&ep, false);
      }

      /* Otherwise, get a new dynamic event client id */
      id = allocate_new_dynamic_id(alpp);

      if (id == 0) {
         sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
         DRETURN(STATUS_ESEMANTIC);
      }

      INFO(MSG_EVE_REG_SUU, name, sge_u32c(id), sge_u32c(ed_time));

      /* Set the new id for this client. */
      lSetUlong(clio, EV_id, id);
   }

   /* special event clients: we allow only one instance */
   /* if it already exists, delete the old one and register the new one */
   if (id > EV_ID_ANY && id < EV_ID_FIRST_DYNAMIC) {
      /*n
      ** we allow addition of a priviledged event client
      ** for internal clients (==> update_func != nullptr)
      ** and manager/operator
      */
      if (update_func == nullptr && !manop_is_manager(packet, master_manager_list)) {
         sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
         ERROR(SFNMAX, MSG_WRONG_USER_FORFIXEDID);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESEMANTIC);
      }

      if ((ep = get_event_client(id)) != nullptr) {
         /* we already have this special client */
         ERROR(MSG_EVE_CLIENTREREGISTERED_SSSU, name, host, commproc, sge_u32c(commproc_id));

         /* delete old event client entry, and we already have the mutex! */
         remove_event_client(&ep, false);
      } else {
         INFO(MSG_EVE_REG_SUU, name, sge_u32c(id), sge_u32c(ed_time));
      }
   }

   ep = lCopyElem(clio);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   lSetRef(ep, EV_update_function, (void*) update_func);
   lSetRef(ep, EV_update_function_arg, update_func_arg);
#pragma GCC diagnostic pop
   lSetBool(clio, EV_changed, false);

   lAppendElem(Event_Master_Control.clients, ep);

   lSetUlong(ep, EV_next_number, 1);

   /* register this contact */
   u_long64 now = sge_get_gmt64();
   lSetUlong64(ep, EV_last_send_time, 0);
   lSetUlong64(ep, EV_next_send_time, now + sge_gmt32_to_gmt64(lGetUlong(ep, EV_d_time)));
   lSetUlong64(ep, EV_last_heard_from, now);
   lSetUlong(ep, EV_state, EV_connected);

   /* return new event client object to internal event client */
   if (eclpp != nullptr) {
      lListElem *ret_el = lCopyElem(ep);
      if (*eclpp == nullptr) {
         *eclpp = lCreateListHash("new event client", EV_Type, true);
      }
      lSetBool(ret_el, EV_changed, false);
      lAppendElem(*eclpp, ret_el);
   }

   /* Start with no pending events. */
   build_subscription(ep);

   /* build events for total update */
   total_update(ep, packet->gdi_session);

   /* flush initial list events */
   flush_events(ep, 0);

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   INFO(MSG_SGETEXT_ADDEDTOLIST_SSSS, packet->user, packet->host, name, MSG_EVE_EVENTCLIENT);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN(STATUS_OK);
} /* sge_event_master_process_add_event_client() */

/****** Eventclient/Server/sge_mod_event_client() ******************************
*  NAME
*     sge_mod_event_client() -- modify event client
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     int 
*     sge_mod_event_client(lListElem *clio, lList **alpp, lList **eclpp, 
*     char *ruser, char *rhost) 
*
*  FUNCTION
*     An event client object is modified.
*     It is possible to modify the event delivery time and
*     the subscription.
*     If the subscription is changed, and new sgeE*_LIST events are subscribed,
*     these lists are sent to the event client.
*
*  INPUTS
*     lListElem *clio - object containing the data to change
*     lList **alpp    - answer list pointer
*     char *ruser     - user that triggered the modify action
*     char *rhost     - host that triggered the modify action
*
*  RESULT
*     int - AN_status code. STATUS_OK on success, else error code
*
*  NOTES
*     MT-NOTE: sge_mod_event_client() is MT safe, uses internal locks
*
*  SEE ALSO
*     evm_mod_func_t
*
*******************************************************************************/
int
sge_mod_event_client(lListElem *clio, lList **alpp, char *ruser, char *rhost)
{
   lListElem *evr = nullptr;

   DENTER(TOP_LAYER);

   if (clio == nullptr) {
      ERROR("nullptr element passed to sge_mod_event_client");
      abort();
      DRETURN(STATUS_ESEMANTIC);
   }

   evr = lCreateElem(EVR_Type);
   lSetUlong(evr, EVR_operation, EVR_MOD_EVC);
   lSetUlong64(evr, EVR_timestamp, sge_get_gmt64());
   lSetObject(evr, EVR_event_client, lCopyElem(clio));

   sge_mutex_lock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);
   lAppendElem(Event_Master_Control.requests, evr);
   sge_mutex_unlock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);

   DEBUG(MSG_SGETEXT_MODIFIEDINLIST_SSSS, ruser, rhost, lGetString(clio, EV_name), MSG_EVE_EVENTCLIENT);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   sge_event_master_flush_requests();

   DRETURN(STATUS_OK);
}

/****** Eventclient/Server/sge_event_master_process_mod_event_client() ********
*  NAME
*     sge_mod_event_client() -- modify event client
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     int 
*     sge_event_master_process_mod_event_client(lListElem *clio, lList **alpp, 
*                                               lList **eclpp, char *ruser, 
*                                               char *rhost) 
*
*  FUNCTION
*     An event client object is modified.
*     It is possible to modify the event delivery time and
*     the subscription.
*     If the subscription is changed, and new sgeE*_LIST events are subscribed,
*     these lists are sent to the event client.
*
*  INPUTS
*     lListElem *clio - object containing the data to change
*     lList **alpp    - answer list pointer
*     char *ruser     - user that triggered the modify action
*     char *rhost     - host that triggered the modify action
*
*  RESULT
*     int - AN_status code. STATUS_OK on success, else error code
*
*  NOTES
*     MT-NOTE: sge_mod_event_client() is NOT MT safe.
*
*******************************************************************************/
static void
sge_event_master_process_mod_event_client(const lListElem *request, monitoring_t *monitor)
{
   lListElem *event_client = nullptr;
   u_long32 id;
   u_long32 busy;
   u_long32 busy_handling;
   u_long32 ev_d_time;
   lListElem *clio = nullptr;
   cl_thread_settings_t *thread_config = nullptr;

   DENTER(TOP_LAYER);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), monitor);

   clio = lGetObject(request, EVR_event_client);

   /* try to find event_client */
   id = lGetUlong(clio, EV_id);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   event_client = get_event_client(id);

   if (event_client == nullptr) {
      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
      ERROR(MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(id), "modify");
      DRETURN_VOID;
   }

   /* these parameters can be changed */
   busy = lGetUlong(clio, EV_busy);
   ev_d_time = lGetUlong(clio, EV_d_time);
   busy_handling = lGetUlong(clio, EV_busy_handling);

   /* check for validity */
   if (ev_d_time < 1) {
      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
      ERROR(MSG_EVE_INVALIDINTERVAL_U, sge_u32c(ev_d_time));
      DRETURN_VOID;
   }

   if (lGetBool(clio, EV_changed) && lGetList(clio, EV_subscribed) == nullptr) {
      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
      ERROR(SFNMAX, MSG_EVE_INVALIDSUBSCRIPTION);
      DRETURN_VOID;
   }

   /* event delivery interval changed.
    * We have to update the next delivery time to
    * next_delivery_time - old_interval + new_interval
    */
   if (ev_d_time != lGetUlong(event_client, EV_d_time)) {
      lSetUlong64(event_client, EV_next_send_time,
                lGetUlong64(event_client, EV_next_send_time) -
                        sge_gmt32_to_gmt64(lGetUlong(event_client, EV_d_time) + ev_d_time));
      lSetUlong(event_client, EV_d_time, ev_d_time);
   }

   /* subscription changed */
   if (lGetBool(clio, EV_changed)) {
      subscription_t *new_sub = nullptr;
      subscription_t *old_sub = nullptr;
      build_subscription(clio);
      new_sub = (subscription_t *)lGetRef(clio, EV_sub_array);
      old_sub = (subscription_t *)lGetRef(event_client, EV_sub_array);


      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_ADMINHOST_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CALENDAR_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CKPT_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CENTRY_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CONFIG_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_EXECHOST_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_JOB_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_JOB_SCHEDD_INFO_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_MANAGER_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_OPERATOR_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_NEW_SHARETREE);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_PE_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_PROJECT_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CQUEUE_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_SCHED_CONF);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_SUBMITHOST_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_USER_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_USERSET_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_HGROUP_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_RQS_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_AR_LIST);

#if 0
/* JG: TODO: better use lXchgList? */
      {
         lList *tmp_list = nullptr;
         lXchgList(clio, EV_subscribed, &tmp_list);
         lXchgList(event_client, EV_subscribed, &tmp_list);
         lXchgList(clio, EV_subscribed, &tmp_list);
      }
#else
      lSetList(event_client, EV_subscribed, lCopyList("", lGetList(clio, EV_subscribed)));
#endif
      lSetRef(event_client, EV_sub_array, new_sub);
      lSetRef(clio, EV_sub_array, nullptr);
      if (old_sub) {
         int i;
         for (i=0; i<sgeE_EVENTSIZE; i++){
            lFreeWhere(&(old_sub[i].where));
            lFreeWhat(&(old_sub[i].what));
            if (old_sub[i].descr){
               cull_hash_free_descr(old_sub[i].descr);
               sge_free(&(old_sub[i].descr));
            }
         } 
         sge_free(&old_sub);
      }
   }

   /* busy state changed */
   if (busy != lGetUlong(event_client, EV_busy)) {
      lSetUlong(event_client, EV_busy, busy);
   }
   /* busy_handling changed */
   if (busy_handling != lGetUlong(event_client, EV_busy_handling)) {
      DPRINTF("EVM: event client %s changes to " sge_U32CFormat "\n", lGetString(event_client, EV_name), lGetUlong(event_client, EV_busy_handling));
      lSetUlong(event_client, EV_busy_handling, busy_handling);
   }

   MONITOR_EDT_MOD(monitor);

   thread_config = cl_thread_get_thread_config();
   DEBUG(MSG_SGETEXT_MODIFIEDINLIST_SSSS, thread_config ? thread_config->thread_name : "-NA-", "master host", lGetString(event_client, EV_name), MSG_EVE_EVENTCLIENT);

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   DRETURN_VOID;
} /* sge_event_master_process_mod_event_client() */

/****** evm/sge_event_master/sge_remove_event_client() *************************
*  NAME
*     sge_remove_event_client() -- remove event client 
*
*  SYNOPSIS
*     void sge_remove_event_client(u_long32 event_client_id) 
*
*  FUNCTION
*     Remove event client. Fetch event client from event client list.
*     Only sets status to "terminated",
*     it will be removed later on in ......................
*
*  INPUTS
*     u_long32 event_client_id - event client id 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_remove_event_client() is MT safe, uses internal locks 
*
*  SEE ALSO
*     evm_remove_func_t
*
*******************************************************************************/
void
sge_remove_event_client(u_long32 event_client_id) {
   lListElem *client;

   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DPRINTF("sge_remove_event_client id = %d\n", (int) event_client_id);

   client = get_event_client(event_client_id);

   if (client == nullptr) {
      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
      ERROR(MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(event_client_id), "remove");
      DRETURN_VOID;
   }
   lSetUlong(client, EV_state, EV_terminated);

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   DRETURN_VOID;
} /* sge_remove_event_client() */


/****** sge_event_master/sge_set_max_dynamic_event_clients() *******************
*  NAME
*     sge_set_max_dynamic_event_clients() -- set max number of dyn. event clients
*
*  SYNOPSIS
*     void sge_set_max_dynamic_event_clients(u_long32 max) 
*
*  FUNCTION
*     Sets max number of dynamic event clients. If the new value is larger than
*     the maximum number of used file descriptors for communication this value
*     is set to the max. number of file descriptors minus some reserved file
*     descriptors. (10 for static event clients, 9 for execd, 10 for file 
*     descriptors used by application (to write files, etc.) ).
*
*     At least one dynamic event client is allowed.
*
*  INPUTS
*     u_long32 max - number of dynamic event clients
*
*  NOTES
*     MT-NOTE: sge_set_max_dynamic_event_clients() is MT safe 
*
*******************************************************************************/
u_long32
sge_set_max_dynamic_event_clients(u_long32 new_value) {
   u_long32 max = new_value;

   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   /* Set the max event clients if it changed. */
   if (max != Event_Master_Control.max_event_clients) {
      /* check max. file descriptors of qmaster communication handle */
      cl_com_handle_t *handle = cl_com_get_handle("qmaster", 1);
      if (handle != nullptr) {
         u_long32 max_allowed_value = 0;
         unsigned long max_file_handles = 0;

         cl_com_get_max_connections(handle, &max_file_handles);
         if (max_file_handles >= EVENT_MASTER_MIN_FREE_DESCRIPTORS) {
            max_allowed_value = (u_long32)max_file_handles - EVENT_MASTER_MIN_FREE_DESCRIPTORS;
         } else {
            max_allowed_value = 1;
         }

         if (max > max_allowed_value) {
            max = max_allowed_value;
            WARNING(MSG_CONF_NR_DYNAMIC_EVENT_CLIENT_EXCEEDS_MAX_FILEDESCR_U, sge_u32c(max));
         }
      }
   }

   /* check max again - it might have changed due to commlib max_file_handles restrictions */
   if (max != Event_Master_Control.max_event_clients) {
      lList *answer_list = nullptr;
      lListElem *new_range;
      const lListElem *event_client;

      /* If the new max is lower than the old max, then lowering the maximum
       * prevents new event clients, but allows the old ones there to drain off naturally.
       */
      Event_Master_Control.max_event_clients = max;
      INFO(MSG_SET_MAXDYNEVENTCLIENT_U, sge_u32c(max));

      /* we have to rebuild the event client id range list */
      lFreeList(&Event_Master_Control.client_ids);
      range_list_initialize(&Event_Master_Control.client_ids, &answer_list);
      new_range = lCreateElem(RN_Type);
      range_set_all_ids(new_range, EV_ID_FIRST_DYNAMIC, max - 1 + EV_ID_FIRST_DYNAMIC, 1);
      lAppendElem(Event_Master_Control.client_ids, new_range);

      /* and we have to remove the ids of our existing event clients from the range */
      for_each_ep(event_client, Event_Master_Control.clients) {
         u_long32 event_client_id = lGetUlong(event_client, EV_id);
         /* only for dynamic event clients */
         if (event_client_id >= EV_ID_FIRST_DYNAMIC) {
            /* 
             * the event clients id might not be in the new range,
             * if the number of dynamic event clients has been reduced 
             */
            if (range_list_is_id_within(Event_Master_Control.client_ids, event_client_id)) {
               range_list_remove_id(&Event_Master_Control.client_ids, &answer_list, event_client_id);
            }
         }
      }

      /* compress the range list to reduce fragmentation */
      range_list_compress(Event_Master_Control.client_ids);

      /* output any errors that might have occured */
      answer_list_output(&answer_list);

   } /* if */
   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DRETURN(max);
}

/****** sge_event_master/sge_get_max_dynamic_event_clients() *******************
*  NAME
*     sge_get_max_dynamic_event_clients() -- get max dynamic event clients nr
*
*  SYNOPSIS
*     u_long32 sge_get_max_dynamic_event_clients(u_long32 max) 
*
*  FUNCTION
*     Returns the actual value of max. dynamic event clients allowed.
*
*  RESULT
*     u_long32 - max value
*
*  NOTES
*     MT-NOTE: sge_get_max_dynamic_event_clients() is MT save
*
*******************************************************************************/
u_long32
sge_get_max_dynamic_event_clients() {
   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   u_long32 ret = Event_Master_Control.max_event_clients;
   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DRETURN(ret);
}

/**
* @brief Get the number of event clients
*
* @return u_long32 - number of event clients (length of the client list)
*/
u_long32
sge_get_num_event_clients() {
   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   u_long32 ret = lGetNumberOfElem(Event_Master_Control.clients);
   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DRETURN(ret);
}

/****** Eventclient/Server/sge_has_event_client() ******************************
*  NAME
*     sge_has_event_client() -- Is a event client registered
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*    bool sge_has_event_client(u_long32 event_client_id)
*
*  FUNCTION
*    Searches if the event client list, if a client
*    with this id is available
*
*  INPUTS
*     u_long32 event_client_id  - id of the event client
*
*  RESULT
*     bool - TRUE if client is in the event client list
*
*  NOTES
*     MT-NOTE: sge_has_event_client() is MT safe, it uses the internal locks
*
*******************************************************************************/
bool
sge_has_event_client(u_long32 event_client_id) {
   bool ret;
   
   DENTER(TOP_LAYER);
   
   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   ret = (get_event_client(event_client_id) != nullptr) ? true : false;
   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DRETURN(ret);
}

/****** evm/sge_event_master/sge_select_event_clients() ************************
*  NAME
*     sge_select_event_clients() -- select event clients 
*
*  SYNOPSIS
*     lList* sge_select_event_clients(const char *list_name, const lCondition 
*     *where, const lEnumeration *what) 
*
*  FUNCTION
*     Select event clients.  
*
*  INPUTS
*     const char *list_name       - name of the result list returned. 
*     const lCondition *where    - where condition 
*     const lEnumeration *what - what enumeration
*
*  RESULT
*     lList* - list with elements of type 'EV_Type'.
*
*  NOTES
*     MT-NOTE: sge_select_event_clients() is MT safe 
*     MT-NOTE:
*     MT-NOTE: The elements contained in the result list are copies of the
*     MT-NOTE: respective event client list elements.
*
*******************************************************************************/
lList*
sge_select_event_clients(const char *list_name, const lCondition *where, const lEnumeration *what)
{
   lList *lst = nullptr;

   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   if (Event_Master_Control.clients != nullptr) {
      lst = lSelect(list_name, Event_Master_Control.clients, where, what);
   }
   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DRETURN(lst);
} /* sge_select_event_clients() */

/****** evm/sge_event_master/sge_shutdown_event_client() ***********************
*  NAME
*     sge_shutdown_event_client() -- shutdown an event client 
*
*  SYNOPSIS
*     int sge_shutdown_event_client(u_long32 event_client_id, const char* anUser, 
*     uid_t anUID) 
*
*  FUNCTION
*     Shutdown an event client. Send the event client denoted by 'event_client_id' 
*     a shutdown event.
*
*     Shutting down an event client is only permitted if 'anUser' does have
*     manager privileges OR is the owner of event client 'event_client_id'.
*
*  INPUTS
*     u_long32 event_client_id - event client ID 
*     const char* anUser - user which did request this operation 
*     uid_t anUID        - user id of request user
*     lList **alpp       - answer list for info and errors
*
*  RESULT
*     EPERM - operation not permitted  
*     ESRCH - client with given client id is unknown
*     0     - otherwise
*
*  NOTES
*     MT-NOTE: sge_shutdown_event_client() is MT safe, it uses the global lock
*              and internal ones.
*
*******************************************************************************/
int
sge_shutdown_event_client(const sge_gdi_packet_class_t *packet, u_long32 event_client_id, lList **alpp) {
   lListElem *client = nullptr;
   int ret = 0;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);

   DENTER(TOP_LAYER);

   if (event_client_id <= EV_ID_ANY) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(event_client_id), "shutdown");
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(EINVAL);
   }

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   client = get_event_client(event_client_id);

   if (client != nullptr) {
      if (!manop_is_manager(packet, master_manager_list) && (packet->uid != lGetUlong(client, EV_uid))) {
         sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
         answer_list_add(alpp, MSG_COM_NOSHUTDOWNPERMS, STATUS_DENIED,
                         ANSWER_QUALITY_ERROR);
         DRETURN(EPERM);
      }

      add_list_event_for_client(event_client_id, 0, sgeE_SHUTDOWN, 0, 0, nullptr, nullptr, nullptr, nullptr, packet->gdi_session);
      /* Print out a message about the event. */
      if (event_client_id == EV_ID_SCHEDD) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_COM_KILLED_SCHEDULER);
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_COM_SHUTDOWNNOTIFICATION_SUS, lGetString(client, EV_name),
                  sge_u32c(lGetUlong(client, EV_id)), lGetHost(client, EV_host));
      }
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(event_client_id), "shutdown");
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = EINVAL;
   }

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   DRETURN(ret);
} /* sge_shutdown_event_client */

/****** evm/sge_event_master/sge_shutdown_dynamic_event_clients() **************
*  NAME
*     sge_shutdown_dynamic_event_clients() -- shutdown all dynamic event clients
*
*  SYNOPSIS
*     int sge_shutdown_dynamic_event_clients(const char *anUser) 
*
*  FUNCTION
*     Shutdown all dynamic event clients. Each dynamic event client known will
*     be send a shutdown event.
*
*     An event client is a dynamic event client if it's client id is greater
*     than or equal to 'EV_ID_FIRST_DYNAMIC'. 
*
*     Shutting down all dynamic event clients is only permitted if 'anUser' does
*     have manager privileges.
*
*  INPUTS
*     const char *anUser - user which did request this operation 
*     lList **alpp       - answer list for info and errors
*
*  RESULT
*     EPERM - operation not permitted 
*     0     - otherwise
*
*  NOTES
*     MT-NOTES: sge_shutdown_dynamic_event_clients() is MT safe, it uses the
*               global_lock and internal ones.
*
*******************************************************************************/
int sge_shutdown_dynamic_event_clients(const sge_gdi_packet_class_t *packet, lList **alpp, monitoring_t *monitor)
{
   const lListElem *client;
   int id = 0;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);

   DENTER(TOP_LAYER);

   if (!manop_is_manager(packet, master_manager_list)) {
      answer_list_add(alpp, MSG_COM_NOSHUTDOWNPERMS, STATUS_DENIED, ANSWER_QUALITY_ERROR);
      DRETURN(EPERM);
   }

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   for_each_ep(client, Event_Master_Control.clients) {
      id = lGetUlong(client, EV_id);

      /* Ignore clients with static ids. */
      if (id < EV_ID_FIRST_DYNAMIC) {
         continue;
      }

      sge_add_event_for_client(id, 0, sgeE_SHUTDOWN, 0, 0, nullptr, nullptr, nullptr, nullptr, packet->gdi_session);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_COM_SHUTDOWNNOTIFICATION_SUS, lGetString(client, EV_name),
               sge_u32c(id), lGetHost(client, EV_host));
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   } /* for_each */

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   DRETURN(0);
} /* sge_shutdown_dynamic_event_clients() */

/****** Eventclient/Server/sge_add_event() *************************************
*  NAME
*     sge_add_event() -- add an object as event
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     void
*     sge_add_event(u_long64 timestamp, ev_event type,
*                   u_long32 intkey, u_long32 intkey2,
*                   const char *strkey, const char *strkey2,
*                   const char *session, lListElem *element)
*
*  FUNCTION
*     Adds an object to the list of events to deliver. Called, if an event
*     occurs to that object, e.g. it was added to Cluster Scheduler, modified or
*     deleted.
*
*     Internally, a list with that single object is created and passed to
*     sge_add_list_event().
*
*  INPUTS
*     u_long64 timestamp      - event creation time, 0 -> use current time
*     ev_event type           - the event id
*     u_long32 intkey         - additional data
*     u_long32 intkey2        - additional data
*     const char *strkey      - additional data
*     const char *strkey2     - additional data
*     const char *session     - events session key
*     lListElem *element      - the object to deliver as event
*
*  NOTES
*     MT-NOTE: sge_add_event() is NOT MT safe.
*
*******************************************************************************/
bool
sge_add_event(u_long64 timestamp, ev_event type, u_long32 intkey, u_long32 intkey2, const char *strkey,
              const char *strkey2, const char *session, lListElem *element, u_long64 gdi_session)
{
   return sge_add_event_for_client(EV_ID_ANY, timestamp, type, intkey, intkey2, strkey, strkey2, session, element, gdi_session);
}
/****** sge_event_master/sge_add_event_for_client() ****************************
*  NAME
*     sge_add_event_for_client() -- add an event for a given object
*
*  SYNOPSIS
*     bool
*     sge_add_event_for_client(u_long32 event_client_id, u_long64 timestamp, ev_event type,
*                              u_long32 intkey, u_long32 intkey2,
*                              const char *strkey, const char *strkey2,
*                              const char *session, lListElem *element)
*
*  FUNCTION
*     Add an event for a given event client.
*
*  INPUTS
*     u_long32 event_client_id   - event client id
*     u_long64 timestamp         - event creation time, 0 -> use current time
*     ev_event type              - event id
*     u_long32 intkey            - 1st numeric key
*     u_long32 intkey2           - 2nd numeric key
*     const char *strkey         - 1st alphanumeric key
*     const char *strkey2        - 2nd alphanumeric key
*     const char *session        - event session
*     lListElem *element         - object to be delivered with the event
*
*  NOTES
*     MT-NOTE: sge_add_event_for_client() is MT safe
*
*******************************************************************************/
bool sge_add_event_for_client(u_long32 event_client_id, u_long64 timestamp, ev_event type,
                              u_long32 intkey, u_long32 intkey2,
                              const char *strkey, const char *strkey2,
                              const char *session, lListElem *element, u_long64 gdi_session)
{
   lList *lp = nullptr;
   bool ret = false;

   DENTER(TOP_LAYER);

   if (element != nullptr) {
      lList *temp_sub_lp = nullptr;
      int sub_list_elem = 0;

      /* ignore the sublist in case of the following events. We have
       * extra events to handle the sub-lists
       */
      if (type == sgeE_JOB_MOD) {
         sub_list_elem = JB_ja_tasks;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      } else if (type == sgeE_CQUEUE_MOD) {
         sub_list_elem = CQ_qinstances;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      } else if (type == sgeE_JATASK_MOD) {
         sub_list_elem = JAT_task_list;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }

      lp = lCreateListHash("Events", lGetElemDescr(element), false);
      lAppendElem(lp, lCopyElemHash(element, false));

      /* restore the original event object */
      if (temp_sub_lp != nullptr) {
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
   }

   ret = add_list_event_for_client(event_client_id, timestamp, type, intkey, intkey2,
                                   strkey, strkey2, session, lp, gdi_session);

   DRETURN(ret);
}

/****** Eventclient/Server/sge_add_list_event() ********************************
*  NAME
*     sge_add_list_event() -- add a list as event
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     void
*     sge_add_list_event(u_long64 timestamp, ev_event type,
*                        u_long32 intkey, u_long32 intkey2,
*                        const char *strkey, const char *strkey2,
*                        const char *session, lList *list)
*
*  FUNCTION
*     Adds a list of objects to the list of events to deliver, e.g. the
*     sgeE*_LIST events.
*
*  INPUTS
*     u_long64 timestamp      - event creation time, 0 -> use current time
*     ev_event type           - the event id
*     u_long32 intkey         - additional data
*     u_long32 intkey2        - additional data
*     const char *strkey      - additional data
*     const char *strkey2     - additional data
*     const char *session     - events session key
*     lList *list             - the list to deliver as event
*
*  NOTES
*     MT-NOTE: sge_add_list_event() is MT safe.
*
*******************************************************************************/
bool sge_add_list_event(u_long64 timestamp, ev_event type,
                        u_long32 intkey, u_long32 intkey2,
                        const char *strkey, const char *strkey2,
                        const char *session, lList *list, u_long64 gdi_session)
{
   bool ret;
   lList *lp = nullptr;

   if (list != nullptr) {
      lListElem *element = nullptr;

      lp = lCreateListHash("Events", lGetListDescr(list), false);
      if (lp == nullptr) {
         return false;
      }
      for_each_rw (element, list) {
         lList *temp_sub_lp = nullptr;
         int sub_list_elem = 0;

         /* ignore the sublist in case of the following events. We have
          * extra events to handle the sub-lists
          */
         if (type == sgeE_JOB_MOD) {
            sub_list_elem = JB_ja_tasks;
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         } else if (type == sgeE_CQUEUE_MOD) {
            sub_list_elem = CQ_qinstances;
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         } else if (type == sgeE_JATASK_MOD) {
            sub_list_elem = JAT_task_list;
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         }

         lAppendElem(lp, lCopyElemHash(element, false));

         /* restore the original event object */
         if (temp_sub_lp != nullptr) {
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         }
      }
   }

   ret = add_list_event_for_client(EV_ID_ANY, timestamp, type, intkey, intkey2,
                                   strkey, strkey2, session, lp, gdi_session);
   return ret;
}

/****** Eventclient/Server/sge_create_event() ********************************
*  NAME
*     sge_add_list_event() -- add a list as event
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     static lListElem* sge_create_event(u_long32    event_client_id,
*                                        u_long32    number,
*                                        u_long64    timestamp,
*                                        ev_event    type,
*                                        u_long32    intkey,
*                                        u_long32    intkey2,
*                                        const char *strkey,
*                                        const char *strkey2,
*                                        const char *session,
*                                        lList      *list)
*
*  FUNCTION
*     Create ET_Type element and fill in the specified parameters.
*     The caller is responsible for freeing the memory again.
*
*  INPUTS
*     u_long32 event_client_id - event client id
*     u_long32    number       - the event number
*     u_long64 timestamp       - event creation time, 0 -> use current time
*     ev_event type            - the event id
*     u_long32 intkey          - additional data
*     u_long32 intkey2         - additional data
*     const char *strkey       - additional data
*     const char *strkey2      - additional data
*     const char *session      - events session key
*     lList *list              - the list to deliver as event
*
*  NOTES
*     MT-NOTE: sge_add_list_event() is MT safe.
*
*******************************************************************************/
static lListElem*
sge_create_event(u_long32 number, u_long64 timestamp, ev_event type, u_long32 intkey, u_long32 intkey2,
                 const char *strkey, const char *strkey2, lList *list) {
   lListElem *etp = nullptr;        /* event object */

   DENTER(TOP_LAYER);

   /* an event needs a timestamp */
   if (timestamp == 0) {
      timestamp = sge_get_gmt64();
   }

   etp = lCreateElem(ET_Type);
   lSetUlong(etp, ET_number, number);
   lSetUlong(etp, ET_type, type);
   lSetUlong64(etp, ET_timestamp, timestamp);
   lSetUlong(etp, ET_intkey, intkey);
   lSetUlong(etp, ET_intkey2, intkey2);
   lSetString(etp, ET_strkey, strkey);
   lSetString(etp, ET_strkey2, strkey2);
   lSetList(etp, ET_new_version, list);

   // A unique ID is needed later, when we are sure that the event will be delivered.
   // Here we do not know that because the event might be part of a action or transaction
   // that is not commited later on.
   u_long64 unique_id = 0;
   lSetUlong64(etp, ET_unique_id, unique_id);

   DRETURN(etp);
}

static void
add_unique_id_to_evr(lListElem *evr, u_long64 unique_id) {
   lListElem *ev;
   for_each_rw(ev, lGetList(evr, EVR_event_list)) {
      lSetUlong64(ev, ET_unique_id, unique_id);
   }
}

static void
add_list_event_for_client_after_commit(lListElem *evr, lList *evr_list, u_long64 gdi_session) {
   DENTER(TOP_LAYER);

   // Either we get a single evr or a list but not both
   bool single_evr = (evr != nullptr);

   // Find the next unique ID for the event
   u_long64 unique_id = oge_get_next_unique_event_id();

   // Add a unique ID to to events part of the commited evr or evr_list.
   if (single_evr) {
      add_unique_id_to_evr(evr, unique_id);
   } else {
      lListElem *evr;

      for_each_rw(evr, evr_list) {
         add_unique_id_to_evr(evr, unique_id);
      }
   }

   // Process the unique ID for the event that was triggered
   ocs::SessionManager::set_write_unique_id(gdi_session, unique_id);

   // Add the request to the event master request list
   sge_mutex_lock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);
   if (single_evr) {
      lAppendElem(Event_Master_Control.requests, evr);
   } else {
      lAppendList(Event_Master_Control.requests, evr_list);
   }
   sge_mutex_unlock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);

   sge_event_master_flush_requests();
   DRETURN_VOID;
}

/****** Eventclient/Server/add_list_event_for_client() *************************
*  NAME
*     add_list_event_for_client() -- add a list as event
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     void
*     add_list_event_for_client(u_long32 event_client_id, u_long64 timestamp,
*                               ev_event type, u_long32 intkey,
*                               u_long32 intkey2, const char *strkey,
*                               const char *session, lList *list)
*
*  FUNCTION
*     Adds a list of objects to the list of events to deliver, e.g. the
*     sgeE*_LIST events, to a specific client.  No checking is done to make
*     sure that the client id is valid.  That is the responsibility of the
*     calling function.
*
*  INPUTS
*     u_long32 event_client_id      - the id of the recipient
*     u_long64 timestamp      - time stamp in gmt for the even; if 0 is passed,
*                               sge_add_list_event will insert the actual time
*     ev_event type           - the event id
*     u_long32 intkey         - additional data
*     u_long32 intkey2        - additional data
*     const char *strkey      - additional data
*     const char *session     - events session key
*     lList *list             - the list to deliver as event
*
*  RESULTS
*     Whether the event was added successfully.
*
*  NOTES
*     MT-NOTE: add_list_event_for_client() is MT safe.
*
*******************************************************************************/
static bool
add_list_event_for_client(u_long32 event_client_id, u_long64 timestamp, ev_event type, u_long32 intkey,
                          u_long32 intkey2, const char *strkey, const char *strkey2, const char *session,
                          lList *list, u_long64 gdi_session) {
   lListElem *evr = nullptr;        /* event request object */
   lList *etlp = nullptr;           /* event list */
   lListElem *etp = nullptr;        /* event object */

   DENTER(TOP_LAYER);

   /* an event needs a timestamp */
   if (timestamp == 0) {
      timestamp = sge_get_gmt64();
   }

   evr = lCreateElem(EVR_Type);
   lSetUlong(evr, EVR_operation, EVR_ADD_EVENT);
   lSetUlong64(evr, EVR_timestamp, timestamp);
   lSetUlong(evr, EVR_event_client_id, event_client_id);
   lSetString(evr, EVR_session, session);

   etlp = lCreateListHash("Event_List", ET_Type, false);
   lSetList(evr, EVR_event_list, etlp);

   /*
    * Create a new event elem (The event number is added when
    * qmaster adds the event to the event client data structure)
    */
   etp = sge_create_event(0, timestamp, type, intkey, intkey2, strkey, strkey2, list);
   lAppendElem(etlp, etp);


   /*
    * if we have a transaction open, add to the transaction
    * otherwise into the event master request list
    * need a new C block, as the GET_SPECIFIC macro declares new variables
    */
   {
      GET_SPECIFIC(event_master_transaction_t, t_store, sge_event_master_init_transaction_store, Event_Master_Control.transaction_key);
      if (t_store->is_transaction) {
         lAppendElem(t_store->transaction_requests, evr);
      } else {
         add_list_event_for_client_after_commit(evr, nullptr, gdi_session);
      }
   }

   DRETURN(true);
}

/* add an event from the request list to the event clients which subscribed it */
static void
sge_event_master_process_send(const lListElem *request, monitoring_t *monitor) {
   lListElem *event_client = nullptr;
   lListElem *event = nullptr;
   lList *event_list = nullptr;
   u_long32 ec_id = 0;
   const char *session = nullptr;
   ev_event type = sgeE_ALL_EVENTS;

   DENTER(TOP_LAYER);

   ec_id = lGetUlong(request, EVR_event_client_id);
   session = lGetString(request, EVR_session);
   event_list = lGetListRW(request, EVR_event_list);

   MONITOR_EDT_NEW(monitor);

   if (ec_id == EV_ID_ANY) {
      DPRINTF("Processing event for all clients\n");

      event = lFirstRW(event_list);
      while (event != nullptr) {
         bool added = false;
         event = lDechainElem(event_list, event);
         type = (ev_event)lGetUlong(event, ET_type);

         sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
         for_each_rw (event_client, Event_Master_Control.clients) {
            ec_id = lGetUlong(event_client, EV_id);

            DPRINTF("Preparing event for client %ld\n", ec_id);

            if (eventclient_subscribed(event_client, type, session)) {
               added = true;
               add_list_event_direct(event_client, event, true);
               MONITOR_EDT_ADDED(monitor);
            }
         } /* for_each */
         sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

         if (!added) {
            MONITOR_EDT_SKIP(monitor);
         }
         lFreeElem(&event);
         event = lFirstRW(event_list);
      } /* while */
   } else {
      DPRINTF("Processing event for client %d.\n", ec_id);

      sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

      event_client = get_event_client(ec_id);

      /* Skip bad client ids.  Events with bad client ids will be freed
       * when send is freed since we don't dechain them. */
      if (event_client != nullptr) {
         event = lFirstRW(event_list);

         while (event != nullptr) {
            event = lDechainElem(event_list, event);
            type = (ev_event)lGetUlong(event, ET_type);

            if (eventclient_subscribed(event_client, type, session)) {
               add_list_event_direct(event_client, event, false);
               MONITOR_EDT_ADDED(monitor);
               /* We can't free the event when we're done because it now belongs
                * to send_events(). */
            } else {
               MONITOR_EDT_SKIP(monitor);
               lFreeElem(&event);
            }
            event = lFirstRW(event_list);
         } /* while */
      } /* if */

      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   } /* else */
   DRETURN_VOID;
} /* process_sends() */

/****** Eventclient/Server/sge_handle_event_ack() ******************************
*  NAME
*     sge_handle_event_ack() -- acknowledge event delivery
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     void
*     sge_handle_event_ack(u_long32 event_client_id, ev_event event_number)
*
*  FUNCTION
*     After the server sent events to an event client, it has to acknowledge
*     their receipt.
*     Acknowledged events are deleted from the list of events to deliver,
*     otherwise they will be resent after the next event delivery interval.
*     If the handling of a busy state of the event client is enabled and set to
*     EV_BUSY_UNTIL_ACK, the event client will be set to "not busy".
*
*  INPUTS
*     u_long32 event_client_id - event client sending acknowledge
*     ev_event event_number   - serial number of the last event to acknowledge
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: sge_handle_event_ack() is MT safe.
*
*
*******************************************************************************/
bool
sge_handle_event_ack(u_long32 event_client_id, u_long32 event_number)
{
   lListElem *evr = nullptr;

   DENTER(TOP_LAYER);

   evr = lCreateElem(EVR_Type);
   lSetUlong(evr, EVR_operation, EVR_ACK_EVENT);
   lSetUlong64(evr, EVR_timestamp, sge_get_gmt64());
   lSetUlong(evr, EVR_event_client_id, event_client_id);
   lSetUlong(evr, EVR_event_number, event_number);

   sge_mutex_lock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);
   lAppendElem(Event_Master_Control.requests, evr);
   sge_mutex_unlock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);

   sge_event_master_flush_requests();
   DRETURN(true);
}

static void
sge_event_master_process_ack(const lListElem *request, monitoring_t *monitor)
{
   lListElem *client;
   u_long32 event_client_id;

   DENTER(TOP_LAYER);

   event_client_id = lGetUlong(request, EVR_event_client_id);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   client = get_event_client(event_client_id);

   if (client == nullptr) {
      ERROR(MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(event_client_id), "process acknowledgements");
   } else {
      u_long32 event_number = lGetUlong(request, EVR_event_number);
      u_long64 timestamp = lGetUlong64(request, EVR_timestamp);
      lList *list = lGetListRW(client, EV_events);
      int res = purge_event_list(list, event_number);

      MONITOR_EDT_ACK(monitor);
      if (res > 0) {
         DPRINTF("%s: purged %d acknowledged events\n", __func__, res);
      }

      lSetUlong64(client, EV_last_heard_from, timestamp); /* note time of ack */

      switch (lGetUlong(client, EV_busy_handling)) {
         case EV_BUSY_UNTIL_ACK:
            lSetUlong(client, EV_busy, 0); /* clear busy state */
            break;
         default:
            break;
      }
   } /* else */

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   DRETURN_VOID;
} /* sge_handle_event_ack() */

/****** evm/sge_event_master/sge_deliver_events_immediately() ******************
*  NAME
*     sge_deliver_events_immediately() -- deliver events immediately
*
*  SYNOPSIS
*     void sge_deliver_events_immediately(u_long32 event_client_id)
*
*  FUNCTION
*     Deliver all events for the event client denoted by 'event_client_id'
*     immediately.
*
*  INPUTS
*     u_long32 event_client_id - event client id
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_deliver_events_immediately() is NOT MT safe.
*
*******************************************************************************/
void
sge_deliver_events_immediately(u_long32 event_client_id)
{
   lListElem *client = nullptr;

   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   if ((client = get_event_client(event_client_id)) == nullptr) {
      ERROR(MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(event_client_id), "deliver events immediately");
   } else {
      flush_events(client, 0);
      sge_event_master_flush_requests(true);
   }

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   DRETURN_VOID;
} /* sge_deliver_event_immediately() */

/****** evm/sge_event_master/sge_resync_schedd() *******************************
*  NAME
*     sge_resync_schedd() -- resync schedd
*
*  SYNOPSIS
*     int sge_resync_schedd()
*
*  FUNCTION
*     Does a total update (send all lists) to schedd and outputs an error
*     message.
*
*  INPUTS
*     void - none
*
*  RESULT
*     0 - resync successful
*    -1 - otherwise
*
*  NOTES
*     MT-NOTE: sge_resync_schedd() in NOT MT safe.
*
*******************************************************************************/
int
sge_resync_schedd(monitoring_t *monitor, u_long64 gdi_session)
{
   lListElem *client;
   int ret = -1;
   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   if ((client = get_event_client(EV_ID_SCHEDD)) != nullptr) {
      ERROR(MSG_EVE_REINITEVENTCLIENT_S, lGetString(client, EV_name));

      total_update(client, gdi_session);

      ret = 0;
   } else {
      ERROR(MSG_EVE_UNKNOWNEVCLIENT_US, sge_u32c(EV_ID_SCHEDD), "resynchronize");
      ret = -1;
   }

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   DRETURN(ret);
} /* sge_resync_schedd() */

/****** evm/sge_event_master/sge_event_master_init() **************************
*  NAME
*     sge_event_master_init() -- event master initialization
*
*  SYNOPSIS
*     static void sge_event_master_init()
*
*  FUNCTION
*     Initialize the event master control structure. Initialize permanent
*     event array.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_event_master_init() is not MT safe
*
*******************************************************************************/
void
sge_event_master_init() {
   DENTER(TOP_LAYER);

   Event_Master_Control.clients = lCreateListHash("EV_Clients", EV_Type, true);
   Event_Master_Control.requests = lCreateListHash("Event Master Requests", EVR_Type, false);
   pthread_key_create(&(Event_Master_Control.transaction_key), sge_event_master_destroy_transaction_store);

   init_send_events();

   /* Initialize the range list for event client ids */
   {
      lList *answer_list = nullptr;
      range_list_initialize(&Event_Master_Control.client_ids, &answer_list);
      answer_list_output(&answer_list);
   }

   DRETURN_VOID;
}

/****** evm/sge_event_master/init_send_events() ********************************
*  NAME
*     init_send_events() -- sets the events, that should allways be delivered
*
*  SYNOPSIS
*     void init_send_events()
*
*  FUNCTION
*     sets the events, that should allways be delivered
*
*  NOTES
*     MT-NOTE: init_send_events() is not MT safe
*
*******************************************************************************/
static void
init_send_events() {
   DENTER(TOP_LAYER);

   memset(SEND_EVENTS, false, sizeof(bool) * sgeE_EVENTSIZE);

   SEND_EVENTS[sgeE_ADMINHOST_LIST] = true;
   SEND_EVENTS[sgeE_CALENDAR_LIST] = true;
   SEND_EVENTS[sgeE_CKPT_LIST] = true;
   SEND_EVENTS[sgeE_CENTRY_LIST] = true;
   SEND_EVENTS[sgeE_CONFIG_LIST] = true;
   SEND_EVENTS[sgeE_EXECHOST_LIST] = true;
   SEND_EVENTS[sgeE_JOB_LIST] = true;
   SEND_EVENTS[sgeE_JOB_SCHEDD_INFO_LIST] = true;
   SEND_EVENTS[sgeE_MANAGER_LIST] = true;
   SEND_EVENTS[sgeE_OPERATOR_LIST] = true;
   SEND_EVENTS[sgeE_PE_LIST] = true;
   SEND_EVENTS[sgeE_PROJECT_LIST] = true;
   SEND_EVENTS[sgeE_QMASTER_GOES_DOWN] = true;
   SEND_EVENTS[sgeE_ACK_TIMEOUT] = true;
   SEND_EVENTS[sgeE_CQUEUE_LIST] = true;
   SEND_EVENTS[sgeE_SUBMITHOST_LIST] = true;
   SEND_EVENTS[sgeE_USER_LIST] = true;
   SEND_EVENTS[sgeE_USERSET_LIST] = true;
   SEND_EVENTS[sgeE_HGROUP_LIST] = true;
   SEND_EVENTS[sgeE_RQS_LIST] = true;
   SEND_EVENTS[sgeE_AR_LIST] = true;

   DRETURN_VOID;
} /* init_send_events() */


/****** sge_event_master/sge_event_master_wait_next() ******************************
*  NAME
*     sge_event_master_wait_next() -- waits for a weakup
*
*  SYNOPSIS
*     void sge_event_master_wait_next()
*
*  FUNCTION
*     waits for a weakup
*
*  NOTES
*     MT-NOTE: is MT safe
*
*******************************************************************************/
void
sge_event_master_wait_next()
{
   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_cond_mutex", __func__, __LINE__, &Event_Master_Control.cond_mutex);

   if (!Event_Master_Control.delivery_signaled) {
      time_t current_time = sge_gmt64_to_time_t(sge_get_gmt64());
      struct timespec ts;
      ts.tv_sec = current_time + EVENT_DELIVERY_INTERVAL_S;
      ts.tv_nsec = EVENT_DELIVERY_INTERVAL_N;
      pthread_cond_timedwait(&Event_Master_Control.cond_var, &Event_Master_Control.cond_mutex, &ts);
   }

   Event_Master_Control.delivery_signaled = false;

   sge_mutex_unlock("event_master_cond_mutex", __func__, __LINE__, &Event_Master_Control.cond_mutex);


   DRETURN_VOID;
}

/****** sge_event_master/remove_event_client() *********************************
*  NAME
*     remove_event_client() -- removes an event client
*
*  SYNOPSIS
*     static void
*     remove_event_client(lListElem **client, int event_client_id, bool lock_event_master)
*
*  FUNCTION
*     removes an event client, marks the index as dirty and frees the memory
*
*  INPUTS
*     lListElem **client     - event client to remove
*     int event_client_id    - event client id to remove
*     bool lock_event_master - shall the function acquire the event_master mutex
*
*  NOTES
*     MT-NOTE: remove_event_client() is not MT safe
*     - it locks the event master mutex to modify the event client list
*     - it assumes that the event client is locked before this method is called
*
*******************************************************************************/
static void
remove_event_client(lListElem **client, bool lock_event_master) {
   DENTER(TOP_LAYER);

   u_long32 event_client_id = lGetUlong(*client, EV_id);
   INFO(MSG_EVE_UNREG_SU, lGetString(*client, EV_name), event_client_id);

   subscription_t *old_sub = nullptr;
   old_sub = (subscription_t *)lGetRef(*client, EV_sub_array);
   if (old_sub) {
      /* now free event client subscription data */
      for (int i = 0; i < sgeE_EVENTSIZE; i++) {
         lFreeWhere(&old_sub[i].where);
         lFreeWhat(&old_sub[i].what);

         if (old_sub[i].descr) {
            cull_hash_free_descr(old_sub[i].descr);
            sge_free(&(old_sub[i].descr));
         }
      }

      sge_free(&old_sub);
      lSetRef(*client, EV_sub_array, nullptr);
   }

   if (lock_event_master) {
      sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   }

   lRemoveElem(Event_Master_Control.clients, client);
   if (event_client_id >= EV_ID_FIRST_DYNAMIC) {
      lList *answer_list = nullptr;
      free_dynamic_id(&answer_list, event_client_id);
      answer_list_output(&answer_list);
   }

   if (lock_event_master) {
      sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   }

   DRETURN_VOID;
}

/****** evm/sge_event_master/sge_event_master_send_events() ******************
*  NAME
*     sge_event_master_send_events() -- send events to event clients
*
*  SYNOPSIS
*     static void send_events()
*
*  FUNCTION
*     Loop over all event clients and send events due. If an event client did
*     time out, it will be removed from the list of registered event clients.
*
*     Events will be delivered only, if the so called 'busy handling' of a
*     client does allow it. Events will be delivered as a report (REP_Type)
*     with a report list of type ET_Type.
*
*  INPUTS
*     lListElem *report  - a report, has to be part of the report list. All
*                          fields have to be init, except for REP_list element.
*     lList *report_list - a pre-init report list
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: send_events() is MT safe
*     MT-NOTE:
*     MT-NOTE: After all events for all clients have been sent. This function
*     MT-NOTE: will wait on the condition variable 'Event_Master_Control.cond_var'
*
*******************************************************************************/
void
sge_event_master_send_events(lListElem *report, lList *report_list, monitoring_t *monitor)
{
   u_long32 timeout;
   u_long32 busy_handling;
   u_long32 scheduler_timeout = mconf_get_scheduler_timeout();
   lListElem *event_client, *next_event_client;
   int ret;
   int commid;
   int deliver_interval;
   u_long32 ec_id = 0;
   event_client_update_func_t update_func = nullptr;
   void *update_func_arg = nullptr;

   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);

   u_long64 now = sge_get_gmt64();
   event_client = lFirstRW(Event_Master_Control.clients);
   while (event_client != nullptr) {
      const char *host = nullptr;
      const char *commproc = nullptr;
      bool do_remove = false;

      next_event_client = lNextRW(event_client);

      ec_id = lGetUlong(event_client, EV_id);

      /* is the event client in state terminated? remove it */
      if (lGetUlong(event_client, EV_state) == EV_terminated) {
         remove_event_client(&event_client, false);
         /* we removed a client, continue with the next one */
         event_client = next_event_client;
         continue;
      }

      /* extract address of event client      */
      /* Important:                           */
      /*   host and commproc have to be freed */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      update_func = (event_client_update_func_t) lGetRef(event_client, EV_update_function);
      update_func_arg = lGetRef(event_client, EV_update_function_arg);
#pragma GCC diagnostic pop

      host = lGetHost(event_client, EV_host);
      commproc = lGetString(event_client, EV_commproc);
      commid = lGetUlong(event_client, EV_commid);

      deliver_interval = lGetUlong(event_client, EV_d_time);
      busy_handling = lGetUlong(event_client, EV_busy_handling);

      /* someone turned the clock back */
      if (lGetUlong64(event_client, EV_last_heard_from) > now) {
         WARNING(MSG_SYSTEM_SYSTEMHASBEENMODIFIEDXSECONDS_I, (int)sge_gmt64_to_gmt32(now - lGetUlong64(event_client, EV_last_heard_from)));
         lSetUlong64(event_client, EV_last_heard_from, now);
         lSetUlong64(event_client, EV_next_send_time, now + sge_gmt32_to_gmt64(deliver_interval));
      } else if (lGetUlong64(event_client, EV_last_send_time) > now) {
         lSetUlong64(event_client, EV_last_send_time, now);
      }

      /* if set, use qmaster_params SCHEDULER_TIMEOUT */
      if (ec_id == EV_ID_SCHEDD && scheduler_timeout > 0) {
         timeout = scheduler_timeout;
      } else {
         /* is the ack timeout expired ? */
         timeout = 10 * deliver_interval;

         if (timeout < EVENT_ACK_MIN_TIMEOUT) {
            timeout = EVENT_ACK_MIN_TIMEOUT;
         } else if (timeout > EVENT_ACK_MAX_TIMEOUT) {
            timeout = EVENT_ACK_MAX_TIMEOUT;
         }
      }

      /*
       * Remove timed-out clients after flushing events for the targeted client.
       * Flush event sgeE_ACK_TIMEOUT for out-timed clients. Clients might re-connect
       * when getting sgeE_ACK_TIMEOUT event.
       */
      if (now > (lGetUlong64(event_client, EV_last_heard_from) + sge_gmt32_to_gmt64(timeout))) {
         lListElem *new_event = nullptr;
         lList* tmp_event_list = nullptr;
         lUlong tmp_cur_event_nr = 0;
         dstring buffer_wrapper;
         char buffer[256];

         DPRINTF("EVC timeout (%d s) (part 1/2)\n", timeout);
         WARNING(MSG_COM_ACKTIMEOUT4EV_ISIS, (int) timeout, commproc, (int) commid, host);

         /* yes, we have to remove this client after sending the sgeE_ACK_TIMEOUT event */
         do_remove = true;

         /* Create new ACK_TIMEOUT event and add it directly to the client event list */
         tmp_cur_event_nr = lGetUlong(event_client, EV_next_number);
         new_event = sge_create_event(tmp_cur_event_nr, now, sgeE_ACK_TIMEOUT, 0, 0, nullptr, nullptr, nullptr);
         tmp_event_list = lGetListRW(event_client, EV_events);

         if (tmp_event_list != nullptr) {
            lAppendElem(tmp_event_list, new_event);
            DPRINTF("Added sgeE_ACK_TIMEOUT to already existing event report list\n");
         } else {
            tmp_event_list = lCreateListHash("Events", ET_Type, false);
            lAppendElem(tmp_event_list, new_event);
            lSetList(event_client, EV_events, tmp_event_list);
            DPRINTF("Created new Events list with sgeE_ACK_TIMEOUT event\n");
         }

         /* We have to set the correct next event number */
         lSetUlong(event_client, EV_next_number, tmp_cur_event_nr + 1);

         /* We log the new added sgeE_ACK_TIMEOUT event */
         sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));
         DPRINTF("%d %s\n", ec_id, event_text(new_event, &buffer_wrapper));

         /* Set next send time to now, we want to deliver it */
         lSetUlong64(event_client, EV_next_send_time, now);
      }

      /* do we have to deliver events ? */
      if (now >= lGetUlong64(event_client, EV_next_send_time)) {
         if (!lGetUlong(event_client, EV_busy) || do_remove) {
            lList *lp = nullptr;

            /* put only pointer in report - dont copy */
            lXchgList(event_client, EV_events, &lp);
            lXchgList(report, REP_list, &lp);

            if (update_func != nullptr) {
               update_func(ec_id, nullptr, report_list, update_func_arg);
               ret = CL_RETVAL_OK;
            } else {
               ret = report_list_send(report_list, host, commproc, commid, 0);
               MONITOR_MESSAGES_OUT(monitor);
            }

            /* on failure retry is triggered automatically */
            if (ret == CL_RETVAL_OK) {
               now = sge_get_gmt64();

               switch (busy_handling) {
                  case EV_BUSY_UNTIL_RELEASED:
                  case EV_BUSY_UNTIL_ACK:
                     lSetUlong(event_client, EV_busy, 1);
                     break;
                  default:
                     /* EV_BUSY_NO_HANDLING */
                     break;
               }

               lSetUlong64(event_client, EV_last_send_time, now);
            }

            /* We reset this time even if the report list send failed because we
             * want to give failed clients a break before trying them again. */
            lSetUlong64(event_client, EV_next_send_time, now + sge_gmt32_to_gmt64(deliver_interval));

            /* don't delete sent events - deletion is triggerd by ack's */
            lXchgList(report, REP_list, &lp);
            lXchgList(event_client, EV_events, &lp);
         } else {
            MONITOR_EDT_BUSY(monitor);
         }
      } /*if */

      /*
       * if we have to remove the event client because of timeout we do it now, because
       * sgeE_ACK_TIMEOUT event was delivered.
       */
      if (do_remove) {
         DPRINTF("REMOVE EVC because of timeout (%d s) (part 2/2)\n", timeout);
         ERROR(MSG_COM_ACKTIMEOUT4EV_SIS, commproc, (int) commid, host);
         remove_event_client(&event_client, false);
      }

      event_client = next_event_client;
   } /* while */

   sge_mutex_unlock("event_master_mutex", __func__, __LINE__, &Event_Master_Control.mutex);
   DRETURN_VOID;
} /* send_events() */

static void
flush_events(lListElem *event_client, int interval) {
   u_long64 next_send = 0;
   u_long64 now = sge_get_gmt64();

   DENTER(TOP_LAYER);

   SGE_ASSERT(event_client != nullptr);

   next_send = lGetUlong64(event_client, EV_next_send_time);
   next_send = MIN(next_send, now + sge_gmt32_to_gmt64(interval));

   lSetUlong64(event_client, EV_next_send_time, next_send);

   if (now >= next_send) {
      sge_event_master_flush_requests();
   }

   DPRINTF("%s: %s %d\tNOW: " sge_u64 " NEXT FLUSH: " sge_u64 " (%s,%s,%d)\n",
           __func__,
           ((lGetString(event_client, EV_name) != nullptr) ? lGetString(event_client, EV_name) : "<null>"),
           lGetUlong(event_client, EV_id),
           now,
           next_send,
           ((lGetHost(event_client, EV_host) != nullptr) ? lGetHost(event_client, EV_host) : "<null>"),
           ((lGetString(event_client, EV_commproc) != nullptr) ? lGetString(event_client, EV_commproc) : "<null>"),
           lGetUlong(event_client, EV_commid));

   DRETURN_VOID;
} /* flush_events() */

/****** Eventclient/Server/total_update() **************************************
*  NAME
*     total_update() -- send all data to eventclient
*
*  SYNOPSIS
*     static void
*     total_update(lListElem *event_client)
*
*  FUNCTION
*     Sends all complete lists it subscribed to an eventclient.
*     If the event client receives a complete list instead of single events,
*     it should completely update it's database.
*
*  INPUTS
*     lListElem *event_client - the event client to update
*
*  NOTES
*     MT-NOTE: total_update() is MT safe, IF the function is invoked with
*     MT-NOTE: 'LOCK_EVENT_CLIENT_LST' locked! This is in accordance with
*     MT-NOTE: the acquire/release protocol as defined by the Cluster Scheduler
*     MT-NOTE: Locking API.
*     MT-NOTE: the method also locks the global lock. One has to make sure,
*     MT-NOTE: that no calling method has that lock already.
*
*  SEE ALSO
*     libs/lck/sge_lock.h
*     libs/lck/sge_lock.c
*
*******************************************************************************/
static void
total_update(lListElem *event_client, u_long64 gdi_session)
{
   DENTER(TOP_LAYER);

   blockEvents(event_client, sgeE_ALL_EVENTS, true);

   sge_set_commit_required();

   total_update_event(event_client, sgeE_ADMINHOST_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_CALENDAR_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_CKPT_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_CENTRY_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_CONFIG_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_EXECHOST_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_JOB_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_JOB_SCHEDD_INFO_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_MANAGER_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_OPERATOR_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_PE_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_CQUEUE_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_SCHED_CONF, false, gdi_session);
   total_update_event(event_client, sgeE_SUBMITHOST_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_USERSET_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_NEW_SHARETREE, false, gdi_session);
   total_update_event(event_client, sgeE_PROJECT_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_USER_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_HGROUP_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_RQS_LIST, false, gdi_session);
   total_update_event(event_client, sgeE_AR_LIST, false, gdi_session);

   sge_commit(gdi_session);

   DRETURN_VOID;
} /* total_update() */


/****** evm/sge_event_master/build_subscription() ******************************
*  NAME
*     build_subscription() -- generates an array out of the cull registration
*                                 structure
*
*  SYNOPSIS
*     static void build_subscription(lListElem *event_el)
*
*  FUNCTION
*      generates an array out of the cull registration
*      structure. The array contains all event elements and each of them
*      has an identifier, if it is subscribed or not. Before that is done, it is
*      tested, the EV_changed flag is set. If not, the function simply returns.
*
*
*  INPUTS
*     lListElem *event_el - the event element, which event structure will be transformed
*
*******************************************************************************/
static void build_subscription(lListElem *event_el)
{
   const lList *subscription = lGetList(event_el, EV_subscribed);
   const lListElem *sub_el = nullptr;
   subscription_t *sub_array = nullptr;
   subscription_t *old_sub_array = nullptr;
   int i = 0;

   DENTER(TOP_LAYER);

   if (!lGetBool(event_el, EV_changed)) {
      DRETURN_VOID;
   }

   DPRINTF("rebuild event mask for client(id): %s(" sge_u32")\n", lGetString(event_el, EV_name), lGetUlong(event_el, EV_id));

   sub_array = (subscription_t *) sge_malloc(sizeof(subscription_t) * sgeE_EVENTSIZE);
   SGE_ASSERT(sub_array != nullptr);
   memset(sub_array, 0, sizeof(subscription_t) * sgeE_EVENTSIZE);

   for (i = 0; i < sgeE_EVENTSIZE; i++) {
      sub_array[i].subscription = EV_NOT_SUBSCRIBED;
      sub_array[i].blocked = false;
   }

   for_each_ep(sub_el, subscription) {
      const lListElem *temp = nullptr;
      u_long32 event = lGetUlong(sub_el, EVS_id);

      sub_array[event].subscription = EV_SUBSCRIBED;
      sub_array[event].flush = lGetBool(sub_el, EVS_flush) ? true : false;
      sub_array[event].flush_time = lGetUlong(sub_el, EVS_interval);

      if ((temp = lGetObject(sub_el, EVS_where))) {
         sub_array[event].where = lWhereFromElem(temp);
      }

      if ((temp = lGetObject(sub_el, EVS_what))) {
         sub_array[event].what = lWhatFromElem(temp);
      }
   }

   old_sub_array = (subscription_t *)lGetRef(event_el, EV_sub_array);

   if (old_sub_array != nullptr) {
      for (int j = 0; j < sgeE_EVENTSIZE; j++) {
         lFreeWhere(&(old_sub_array[j].where));
         lFreeWhat(&(old_sub_array[j].what));
         if (old_sub_array[j].descr){
            cull_hash_free_descr(old_sub_array[j].descr);
            sge_free(&(old_sub_array[j].descr));
         }
      }
      sge_free(&old_sub_array);
   }

   lSetRef(event_el, EV_sub_array, sub_array);
   lSetBool(event_el, EV_changed, false);

   DRETURN_VOID;
} /* build_subscription() */

/****** Eventclient/Server/check_send_new_subscribed_list() ********************
*  NAME
*     check_send_new_subscribed_list() -- check suscription for new list events
*
*  SYNOPSIS
*     static void
*     check_send_new_subscribed_list(const subscription_t *old_subscription,
*                                    const subscription_t *new_subscription,
*                                    lListElem *event_client,
*                                    ev_event event)
*
*  FUNCTION
*     Checks, if sgeE*_LIST events have been added to the subscription of a
*     certain event client. If yes, send these lists to the event client.
*
*  INPUTS
*     const subscription_t *old_subscription - former subscription
*     const subscription_t *new_subscription - new subscription
*     lListElem *event_client                - the event client object
*     ev_event event                         - the event to check
*
*  SEE ALSO
*     Eventclient/Server/total_update_event()
*
*******************************************************************************/
static void
check_send_new_subscribed_list(const subscription_t *old_subscription,
                               const subscription_t *new_subscription,
                               lListElem *event_client, ev_event event)
{
   if ((new_subscription[event].subscription == EV_SUBSCRIBED) &&
      (old_subscription[event].subscription == EV_NOT_SUBSCRIBED)) {
      total_update_event(event_client, event, true, GDI_SESSION_NONE);
   }
}

/****** Eventclient/Server/eventclient_subscribed() ************************
*  NAME
*     eventclient_subscribed() -- has event client subscribed an event?
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     int
*     eventclient_subscribed(const lListElem *event_client, ev_event event)
*
*  FUNCTION
*     Checks if the given event client has a certain event subscribed.
*     For event clients that use session filtering additional conditions
*     must be fulfilled otherwise the event counts not as subscribed.
*
*  INPUTS
*     const lListElem *event_client - event client to check
*     ev_event event                - event to check
*     const char *session           - session key of this event
*
*  RESULT
*     int - 0 = not subscribed, 1 = subscribed
*
*  SEE ALSO
*     Eventclient/-Session filtering
*******************************************************************************/
static int eventclient_subscribed(const lListElem *event_client, ev_event event,
                                  const char *session)
{
   const subscription_t *subscription = nullptr;
   const char *ec_session = nullptr;

   DENTER(TOP_LAYER);

   SGE_ASSERT(event_client != nullptr);

   if (event_client == nullptr) {
      DRETURN(0);
   }

   subscription = (subscription_t *)lGetRef(event_client, EV_sub_array);
   ec_session = lGetString(event_client, EV_session);

   if (subscription == nullptr) {
      DPRINTF("No subscription!\n");
      DRETURN(0);
   }

   if (ec_session) {
      if (session) {
         /* events that belong to a specific session are not subscribed
            in case the event client is not interested in that session */
         if (strcmp(session, ec_session)) {
            DPRINTF("Event session does not match client session\n");
            DRETURN(0);
         }
      } else {
         /* events that do not belong to a specific session are not
            subscribed if the event client is interested in events of a
            specific session.
            The only exception are list events, because list events do not
            belong to a specific session. These events require filtering on
            a more fine grained level */
         if (!IS_TOTAL_UPDATE_EVENT(event)) {
            DRETURN(0);
         }
      }
   }
   if (subscription[event].subscription == EV_SUBSCRIBED && !subscription[event].blocked) {
      DRETURN(1);
   }

   DRETURN(0);
}

/****** evm/sge_event_master/purge_event_list() ********************************
*  NAME
*     purge_event_list() -- purge event list
*
*  SYNOPSIS
*     static int purge_event_list(lList* aList, ev_event event_number)
*
*  FUNCTION
*     Remove all events from 'aList' which do have an event id less than or
*     equal to 'event_number'.
*
*  INPUTS
*     lList* aList     - event list
*     ev_event event_number - event
*
*  RESULT
*     int - number of events purged.
*
*  NOTES
*     MT-NOTE: purge_event_list() is NOT MT safe.
*     MT-NOTE:
*     MT-NOTE: Do not call this function without having 'aList' locked!
*
*  BUGS
*     BUGBUG-AD: If 'event_number' == 0, not events will be purged. However zero is
*     BUGBUG-AD: also the id of 'sgeE_ALL_EVENTS'. Is this behaviour correct?
*
*******************************************************************************/
static int purge_event_list(lList *event_list, u_long32 event_number)
{
   int purged = 0, pos = 0;
   lListElem *ev = nullptr;

   DENTER(TOP_LAYER);

   if (event_number == 0) {
      DRETURN(0);
   }

   pos = lGetPosInDescr(ET_Type, ET_number);
   ev = lFirstRW(event_list);

   while (ev != nullptr) {
      lListElem *tmp = ev;

      ev = lNextRW(ev);

      if (lGetPosUlong(tmp, pos) > event_number) {
         break;
      }

      lRemoveElem(event_list, &tmp);
      purged++;
   }

   DRETURN(purged);
} /* remove_events_from_client() */

static void add_list_event_direct(lListElem *event_client, lListElem *event,
                                  bool copy_event)
{
   lList *lp = nullptr;
   lList *clp = nullptr;
   lListElem *ep = nullptr;
   ev_event type = (ev_event)lGetUlong(event, ET_type);
   subscription_t *subscription = nullptr;
   char buffer[1024];
   dstring buffer_wrapper;
   u_long32 i = 0;
   const lCondition *selection = nullptr;
   const lEnumeration *fields = nullptr;
   const lDescr *descr = nullptr;
   bool internal_client = false;

   DENTER(TOP_LAYER);

   SGE_ASSERT(event_client != nullptr);

   if (lGetUlong(event_client, EV_state) != EV_connected) {
      /* the event client is not connected anymore, so we are not
         adding new events to it*/
      if (!copy_event) {
         lFreeElem(&event);
      }
      DRETURN_VOID;
   }

   /* detect internal event clients */
   if (lGetRef(event_client, EV_update_function) != nullptr) {
      internal_client = true;
   }

   /* if the total updates blocked the client, we have to unblock this list */
   blockEvents(event_client, type, false);

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

   /* Pull out payload for selecting */
   lXchgList(event, ET_new_version, &lp);

   /* If the list is nullptr, no need to bother with any of this.  Plus, if we
    * did do this part with a nullptr list, the check for a clp with no
    * elements would kick us out. */
   if (lp != nullptr) {
      subscription = (subscription_t *)lGetRef(event_client, EV_sub_array);
      selection = subscription[type].where;
      fields = subscription[type].what;

#if 0
      DPRINTF("deliver event: %d with where filter=%s and what filter=%s\n",
              type, selection?"true":"false", fields?"true":"false");
#endif

      if (fields) {
         descr = getDescriptorL(subscription, lp, type);

#if 0
         DPRINTF("Reducing event data\n");
#endif

         if (!list_select(subscription, type, &clp, lp, selection, fields,
                          descr, internal_client)) {
            clp = lSelectDPack("updating list", lp, selection, descr,
                               fields, internal_client, nullptr, nullptr);
         }

         /* no elements in the event list, no need for an event */
         if (!SEND_EVENTS[type] && lGetNumberOfElem(clp) == 0) {
            if (clp != nullptr) {
               lFreeList(&clp);
            }

            /* we are not making a copy, so we have to restore the old element */
            lXchgList(event, ET_new_version, &lp);

            if (!copy_event) {
               lFreeElem(&event);
            }

            DPRINTF("Skipping event because it has no content for this client.\n");
            DRETURN_VOID;
         }

         /* If we're not making a copy, we have to free the original list.  If
          * we are making a copy, freeing the list is the responsibility of the
          * calling function. */
         if (!copy_event) {
            lFreeList(&lp);
         }
      } else if (copy_event) {
         /* If there's no what clause, and we want a copy, we copy the list */
#if 0
         DPRINTF("Copying event data\n");
#endif
         clp = lCopyListHash(lGetListName(lp), lp, internal_client);
      } else {
         /* If there's no what clause, and we don't want to copy, we just reuse
          * the original list. */
         clp = lp;
         if (internal_client) {
            cull_hash_create_hashtables(clp);
         }
         /* Make sure lp is clear for the next part. */
         lp = nullptr;
      }
   } /* if */

   /* If we're making a copy, copy the event and swap the orignial list
    * back into the original event */
   if (copy_event) {
#if 0
      DPRINTF("Copying event\n");
#endif
      ep = lCopyElemHash(event, false);

      lXchgList(event, ET_new_version, &lp);
   } else {
      /* If we're not making a copy, reuse the original event. */
      ep = event;
   }

   /* Swap the new list into the working event. */
   lXchgList(ep, ET_new_version, &clp);

   /* fill in event number and increment
      EV_next_number of event recipient */
   i = lGetUlong(event_client, EV_next_number);
   lSetUlong(event_client, EV_next_number, (i + 1));
   lSetUlong(ep, ET_number, i);

   /* build a new event list if not exists */
   lp = lGetListRW(event_client, EV_events);

   if (lp == nullptr) {
      lp=lCreateListHash("Events", ET_Type, false);
      lSetList(event_client, EV_events, lp);
   }

   /* chain in new event */
   lAppendElem(lp, ep);

   DPRINTF("%d %s\n", lGetUlong(event_client, EV_id), event_text(ep, &buffer_wrapper));

   /* check if event clients wants flushing */
   subscription = (subscription_t *)lGetRef(event_client, EV_sub_array);

   if (type == sgeE_QMASTER_GOES_DOWN) {
      Event_Master_Control.is_prepare_shutdown = true;
      lSetUlong(event_client, EV_busy, 0); /* can't be too busy for shutdown */
      flush_events(event_client, 0);
   } else if (type == sgeE_SHUTDOWN) {
      flush_events(event_client, 0);
      /* the event client should be shutdown, so we do not add any events to it, after
         the shutdown event */
      lSetUlong(event_client, EV_state, EV_closing);
   } else if (subscription[type].flush) {
      DPRINTF("flushing event client\n");
      flush_events(event_client, subscription[type].flush_time);
   }

   DRETURN_VOID;
}

/****** Eventclient/Server/total_update_event() *******************************
*  NAME
*     total_update_event() -- create a total update event
*
*  SYNOPSIS
*     static void
*     total_update_event(lListElem *event_client, ev_event type)
*
*  FUNCTION
*     Creates an event delivering a certain list of objects for an event client.
*     For event clients that have subscribed a session list filtering can be done
*     here.
*
*  INPUTS
*     lListElem *event_client          - event client to receive the list
*     ev_event type                    - event describing the list to update
*
*******************************************************************************/
static void total_update_event(lListElem *event_client, ev_event type, bool new_subscription, u_long64 gdi_session)
{
   const lList *lp = nullptr; /* lp should be set, if we have to make a copy */
   lList *copy_lp = nullptr; /* copy_lp should be used for a copy of the org. list */
   char buffer[1024];
   dstring buffer_wrapper;
   u_long32 id;

   DENTER(TOP_LAYER);

   SGE_ASSERT(event_client != nullptr);

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));
   id = lGetUlong(event_client, EV_id);

   /* This test bothers me.  Technically, the GDI thread should just drop the
    * event in the send queue and forget about it.  However, doing this test
    * here could prevent the queuing of events that will later be determined
    * to be useless. */
   if (new_subscription || eventclient_subscribed(event_client, type, nullptr)) {
      switch (type) {
         case sgeE_ADMINHOST_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST);
            break;
         case sgeE_CALENDAR_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);
            break;
         case sgeE_CKPT_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);
            break;
         case sgeE_CENTRY_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
            break;
         case sgeE_CONFIG_LIST:
            /* sge_get_configuration() returns a copy already, we do not need to make
               one later */
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_CONFIG);
            break;
         case sgeE_EXECHOST_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
            break;
         case sgeE_JOB_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
            break;
         case sgeE_JOB_SCHEDD_INFO_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_JOB_SCHEDD_INFO);
            break;
         case sgeE_MANAGER_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
            break;
         case sgeE_NEW_SHARETREE:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_SHARETREE);
            break;
         case sgeE_OPERATOR_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
            break;
         case sgeE_PE_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
            break;
         case sgeE_PROJECT_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
            break;
         case sgeE_CQUEUE_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
            break;
         case sgeE_SCHED_CONF:
            copy_lp = sconf_get_config_list();
            break;
         case sgeE_SUBMITHOST_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST);
            break;
         case sgeE_USER_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
            break;
         case sgeE_USERSET_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
            break;
         case sgeE_HGROUP_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
            break;
         case sgeE_RQS_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
            break;
         case sgeE_AR_LIST:
            lp = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
            break;
         default:
            WARNING(MSG_EVE_TOTALUPDATENOTHANDLINGEVENT_I, type);
            DRETURN_VOID;
      } /* switch */

      if (lp != nullptr) {
         copy_lp = lCopyListHash(lGetListName(lp), lp, false);
      }

      /* 'send_events()' will free the copy of 'lp' */
      add_list_event_for_client(id, 0, type, 0, 0, nullptr, nullptr, nullptr, copy_lp, GDI_SESSION_NONE);
   } /* if */

   DRETURN_VOID;
} /* total_update_event() */


/****** evm/sge_event_master/list_select() *************************************
*  NAME
*     list_select() -- makes a reduced job list duplication
*
*  SYNOPSIS
*     static bool list_select(subscription_t *subscription, int type, lList
*     **reduced_lp, lList *lp, const lCondition *selection, const lEnumeration
*     *fields, const lDescr *descr, bool do_hash)
*
*  FUNCTION
*     Only works on job events. All others are ignored. The job events
*     need some special handling and this is done in this function. The
*     JAT_Type list can be subscribed by its self and it is also part
*     of the JB_Type. If a JAT_Type filter is set, this function also
*     filters the JAT_Type lists in the JB_Type lists.
*
*  INPUTS
*     subscription_t *subscription - subscription array
*     int type                     - event type
*     lList **reduced_lp           - target list (has to be an empty list)
*     lList *lp                    - source list (will be modified)
*     const lCondition *selection  - where filter
*     const lEnumeration *fields   - what filter
*     const lDescr *descr          - reduced descriptor
*     bool do_hash                 - create hash tables in the target list
*
*  RESULT
*     static bool - true, if it was a job event
*******************************************************************************/
static bool list_select(subscription_t *subscription, int type,
                        lList **reduced_lp, lList *lp,
                        const lCondition *selection, const lEnumeration *fields,
                        const lDescr *descr, bool do_hash)
{
   bool ret = false;
   int entry_counter;
   int event_counter;

   DENTER(TOP_LAYER);

   for (entry_counter = 0; entry_counter < LIST_MAX; entry_counter++) {
      event_counter = -1;
      while (EVENT_LIST[entry_counter][++event_counter] != -1) {
         if (type == EVENT_LIST[entry_counter][event_counter]) {
            int sub_type = -1;
            int i = -1;

            while (SOURCE_LIST[entry_counter][++i] != -1) {
               if (subscription[SOURCE_LIST[entry_counter][i]].what) {
                  sub_type = SOURCE_LIST[entry_counter][i];
                  break;
               }
            }

            if (sub_type != -1) {
               lListElem *element = nullptr;
               lListElem *reduced_el = nullptr;

               ret = true;
               *reduced_lp = lCreateListHash("update", descr, do_hash);

               for_each_rw (element, lp) {
                  reduced_el = elem_select(subscription, element,
                               FIELD_LIST[entry_counter], selection,
                               fields, descr, sub_type);

                  lAppendElem(*reduced_lp, reduced_el);
               }
            } else {
               DPRINTF("no sub type filter specified\n");
            }
            goto end;
         } /* end if */
      } /* end while */
   } /* end for */
end:
   DRETURN(ret);
}

/****** sge_event_master/elem_select() ******************************************
*  NAME
*     elem_select() -- makes a reduced copy of an element with reducing sublists
*                      as well
*
*  SYNOPSIS
*     static lListElem *elem_select(subscription_t *subscription, lListElem *element,
*                              const int ids[], const lCondition *selection,
*                              const lEnumeration *fields, const lDescr *dp, int sub_type)
*
*  FUNCTION
*     The function will apply the given filters for the element. Before the element
*     is reduced, all attribute sub lists named in "ids" will be removed from the list and
*     reduced. The reduced sub lists will be added the the reduced element and the original
*     element will be restored. The sub-lists will only be reduced, if the reduced element
*     still contains their attributes.
*
*  INPUTS
*     subscription_t *subscription - subscription array
*     lListElem *element           - the element to reduce
*     const int ids[]              - attribute with sublists to be reduced as well
*     const lCondition *selection  - where filter
*     const lEnumeration *fields   - what filter
*     const lDescr *descr          - reduced descriptor
*     int sub_type                 - list type of the sublists.
*
*  RESULT
*     bool - the reduced element, or nullptr if something went wrong
*
*  NOTE:
*  MT-NOTE: works only on the variables -> thread save
*
*******************************************************************************/
static lListElem *elem_select(subscription_t *subscription, lListElem *element,
                              const int ids[], const lCondition *selection,
                              const lEnumeration *fields, const lDescr *dp, int sub_type)
{
   const lCondition *sub_selection = nullptr;
   const lEnumeration *sub_fields = nullptr;
   const lDescr *sub_descr = nullptr;
   lList **sub_list;
   lListElem *el = nullptr;
   int counter;

   DENTER(TOP_LAYER);

   if (element == nullptr) {
      DRETURN(nullptr);
   }

   if (sub_type <= sgeE_ALL_EVENTS || sub_type >= sgeE_EVENTSIZE) {
      /* TODO: SG: add error message */
      DPRINTF("wrong event sub type\n");
      DRETURN(nullptr);
   }

   /* get the filters for the sub lists */
   if (sub_type >= 0) {
      sub_selection = subscription[sub_type].where;
      sub_fields = subscription[sub_type].what;
   }

   if (sub_fields) { /* do we have a sub list filter, otherwise ... */
      int ids_size = 0;

      /* allocate memory to store the sub-lists, which should be handeled special */
      while (ids[ids_size] != -1) {
         ids_size++;
      }
      sub_list = (lList **)sge_malloc(ids_size * sizeof(lList*));
      memset(sub_list, 0 , ids_size * sizeof(lList*));

      /* remove the sub-lists from the main element */
      for (counter = 0; counter < ids_size; counter ++) {
         lXchgList(element, ids[counter], &(sub_list[counter]));
      }

      /* get descriptor for reduced sub-lists */
      if (!sub_descr) {
         for (counter = 0; counter < ids_size; counter ++) {
            if (sub_list[counter]) {
               sub_descr = getDescriptorL(subscription, sub_list[counter], sub_type);
               break;
            }
         }
      }

      /* copy the main list */
      if (!fields) {
         /* there might be no filter for the main element, but for the sub-lists */
         el = lCopyElemHash(element, false);
      } else if (!dp) {
         /* for some reason, we did not get a descriptor for the target element */
         el = lSelectElemPack(element, selection, fields, false, nullptr);
      } else {
         el = lSelectElemDPack(element, selection, dp, fields, false, nullptr, nullptr);
      }

      /* if we have a new reduced main element */
      if (el) {
         /* copy the sub-lists, if they are still part of the reduced main element */
         for (counter = 0; counter < ids_size; counter ++) {
            if (sub_list[counter] && (lGetPosViaElem(el, ids[counter], SGE_NO_ABORT) != -1)) {
               lSetList(el, ids[counter],
                        lSelectDPack("", sub_list[counter], sub_selection,
                                     sub_descr, sub_fields, false, nullptr, nullptr));
            }
         }
      }

      /* restore the old sub_list */
      for (counter = 0; counter < ids_size; counter ++) {
         lXchgList(element, ids[counter], &(sub_list[counter]));
      }

      sge_free(&sub_list);
   } else {
      DPRINTF("no sub filter specified\n");
      el = lSelectElemDPack(element, selection, dp, fields, false, nullptr, nullptr);
   }

   DRETURN(el);
}

/****** Eventclient/Server/eventclient_list_locate() **************************
*  NAME
*     eventclient_list_locate_by_adress() -- search event client by adress
*
*  SYNOPSIS
*     #include "evm/sge_event_master.h"
*
*     lListElem *
*     eventclient_list_locate_by_adress(const char *host,
*                     const char *commproc, u_long32 id)
*
*  FUNCTION
*     Searches the event client list for an event client with the
*     specified commlib adress.
*     Returns a pointer to the event client object or
*     nullptr, if no such event client is registered.
*
*  INPUTS
*     const char *host     - hostname of the event client to search
*     const char *commproc - commproc of the event client to search
*     u_long32 id          - id of the event client to search
*
*  RESULT
*     lListElem* - event client object or nullptr.
*
*  NOTES
*
*******************************************************************************/
static lListElem *
eventclient_list_locate_by_adress(const char *host, const char *commproc,
                                  u_long32 id)
{
   lListElem *ep;

   DENTER(TOP_LAYER);

   for_each_rw(ep, Event_Master_Control.clients) {
      if (lGetUlong(ep, EV_commid) == id &&
          !sge_hostcmp(lGetHost(ep, EV_host), host) &&
          !strcmp(lGetString(ep, EV_commproc), commproc)) {
         break;
      }
   }

   DRETURN(ep);
}

/****** sge_event_master/getDescriptorL() **************************************
*  NAME
*     getDescriptorL() -- returns a reduced desciptor
*
*  SYNOPSIS
*     static const lDescr* getDescriptorL(subscription_t *subscription, const
*     lList* list, int type)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     subscription_t *subscription - subscription array
*     const lList* list            - source list
*     int type                     - event type
*
*  RESULT
*     static const lDescr* - reduced descriptor or nullptr, if no what exists
*
*  NOTE
*   MT-NOTE: thread save, works only on the submitted variables.
*******************************************************************************/
static const lDescr* getDescriptorL(subscription_t *subscription,
                                    const lList* list, int type)
{
   const lDescr *dp = nullptr;

   if (subscription[type].what) {
      if (!(dp = subscription[type].descr)) {
         subscription[type].descr = lGetReducedDescr(lGetListDescr(list),
                                                     subscription[type].what);
         dp = subscription[type].descr;
      }
   }

   return dp;
}

/****** sge_event_master/get_event_client() ************************************
*  NAME
*     get_event_client() -- gets the event client from the list
*
*  SYNOPSIS
*     static lListElem *get_event_client(u_long32 id)
*
*  FUNCTION
*     Returns the event client with the given id, or nullptr if no such event
*     client exists.
*
*  INPUTS
*     u_long32 id - the client id
*
*  RESULT
*     The event client with the given id, or nullptr if no such event client
*     exists.
*
*  NOTE
*     MT-NOTE: NOT thread safe.  Requires caller to hold Event_Master_Control.mutex.
*******************************************************************************/
static lListElem *get_event_client(u_long32 id)
{
   return lGetElemUlongRW(Event_Master_Control.clients, EV_id, id);
}

/****** sge_event_master/allocate_new_dynamic_id() *******************************
*  NAME
*     allocate_new_dynamic_id() -- gets a new dynamic id
*
*  SYNOPSIS
*     static u_long32 allocate_new_dynamic_id()
*
*  FUNCTION
*     Returns the next available dynamic event client id.  The id returned will
*     be between EV_ID_FIRST_DYNAMIC and Event_Master_Control.max_event_clients +
*     EV_ID_FIRST_DYNAMIC.
*
*  RESULTS
*     The next available dynamic event client id.
*
*  NOTE
*     MT-NOTE: allocate_new_dynamic_id() is thread safe,
*              when the caller holds Event_Master_Control.mutex.
*******************************************************************************/
static u_long32
allocate_new_dynamic_id(lList **answer_list)
{
   u_long32 id = 0;

   DENTER(TOP_LAYER);

   if (range_list_is_empty(Event_Master_Control.client_ids)) {
      ERROR(MSG_TO_MANY_DYNAMIC_EC_U, sge_u32c(Event_Master_Control.max_event_clients));
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
   } else {
      id = range_list_get_first_id(Event_Master_Control.client_ids, answer_list);
      if (id != 0) {
         range_list_remove_id(&Event_Master_Control.client_ids, answer_list, id);
         /* compress the range list to reduce fragmentation
          * JG: TODO: do we need to compress here?
          */
         range_list_compress(Event_Master_Control.client_ids);
      }
   }

   DRETURN(id);
}

static void
free_dynamic_id(lList **answer_list, u_long32 id)
{
   if (id < Event_Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC) {
      range_list_insert_id(&Event_Master_Control.client_ids, answer_list, id);

      /* compress the range list to reduce fragmentation */
      range_list_compress(Event_Master_Control.client_ids);
   }
}

/****** sge_event_master/sge_commit() ****************************************
*  NAME
*     sge_commit() -- Commit the queued events
*
*  SYNOPSIS
*     bool sge_commit()
*
*  FUNCTION
*     Sends any events that this thread currently has queued and clears the
*     queue.
*
*  RESULTS
*     Whether the call succeeded.
*
*  NOTE
*     MT-NOTE: sge_commit is thread safe.
*******************************************************************************/
bool
sge_commit(u_long64 gdi_session) {
   DENTER(TOP_LAYER);
   bool ret = true;

   GET_SPECIFIC(event_master_transaction_t, t_store, sge_event_master_init_transaction_store, Event_Master_Control.transaction_key);
   if (t_store->is_transaction) {
      t_store->is_transaction = false;

      if (lGetNumberOfElem(t_store->transaction_requests) > 0) {
         add_list_event_for_client_after_commit(nullptr, t_store->transaction_requests, gdi_session);
      }
   } else {
      WARNING("attempting to commit an event master transaction, but no transaction is open");
      ret = false;
   }

   DRETURN(ret);
}

/****** sge_event_master/blockEvents() ****************************************
*  NAME
*     blockEvents() -- blocks or unblocks events
*
*  SYNOPSIS
*     void blockEvents(lListElem *event_client, int ev_type, bool isBlock)
*
*  FUNCTION
*     In case that global update events have to be send, this function
*     blocks all events for that list, or unblocks it.
*
*  INPUT
*    lListElem *event_client : event client to modify
*    ev_event ev_type : the event_lists to unblock or -1
*    bool isBlock : true: block events, false: unblock
*
*  NOTE
*     MT-NOTE: sge_commit is thread safe.
*******************************************************************************/
static void blockEvents(lListElem *event_client, ev_event ev_type, bool isBlock) {
   subscription_t *sub_array = (subscription_t *)lGetRef(event_client, EV_sub_array);

   if (sub_array != nullptr) {
      int i = -1;
      if (ev_type == sgeE_ALL_EVENTS) { /* block all subscribed events, for which are list events subscribed */
         while (total_update_events[++i] != -1) {
            if (sub_array[total_update_events[i]].subscription == EV_SUBSCRIBED) {
               int y = -1;
               while (block_events[i][++y] != -1) {
                  sub_array[block_events[i][y]].blocked = isBlock;
               }
            }
         }
      } else {
         while (total_update_events[++i] != -1) {
            if (total_update_events[i] == ev_type) {
               int y = -1;
               while (block_events[i][++y] != -1) {
                 sub_array[block_events[i][y]].blocked = isBlock;
               }
               break;
            }
         }
      }
   }
}

/**
* @brief flush all requests
*
* Triggers immediate processing of all requests.
* After processing requests the events are delivered to clients.
*
* @note The former name of the function (set_flush) suggested that events were immediately flushed.
*       This is not the case! First all requests are processed, only then the events are delivered.
*       Processing the events can take some time (e.g. when a total update of a client is requested.
*       We might want to set a flag to signal that the existing events shall be delivered first.
*
* @param force - if true, the delivery (the condition variable) is signalled again, even if it was already signaled
*/
void sge_event_master_flush_requests(bool force) {
   DENTER(TOP_LAYER);

   sge_mutex_lock("event_master_cond_mutex", __func__, __LINE__, &Event_Master_Control.cond_mutex);
   if (!Event_Master_Control.delivery_signaled || force) {
      Event_Master_Control.delivery_signaled = true;
      pthread_cond_signal(&Event_Master_Control.cond_var);
   }
   sge_mutex_unlock("event_master_cond_mutex", __func__, __LINE__, &Event_Master_Control.cond_mutex);

   DRETURN_VOID;
}

/****** sge_event_master/sge_set_commit_required() *****************************
*  NAME
*     sge_set_commit_required() -- Require commits (make multipe object changes atomic)
*
*  SYNOPSIS
*     void sge_set_commit_required()
*
*  FUNCTION
*  Enables transactions on events. So far a rollback is not supported. It allows to accumulate
*  events, while multiple objects are modified and events are issued and to submit them as
*  one event package. There can only be one event session open at a time. The transaction_mutex
*  will block multiple calles to this method.
*  The method cannot be called recursivly, and sge_commit has to be called to close the transaction.
*
*
*  NOTE
*     MT-NOTE: sge_set_commit_required is thread safe.  Transactional event
*     processing is handled for each thread individually.
*******************************************************************************/
void sge_set_commit_required()
{
   DENTER(TOP_LAYER);

   /* need a new C block, as the GET_SPECIFIC macro declares new variables */
   {
      GET_SPECIFIC(event_master_transaction_t, t_store, sge_event_master_init_transaction_store, Event_Master_Control.transaction_key);
      if (t_store->is_transaction) {
         WARNING("attempting to open a new event master transaction, but we already have a transaction open");
      } else {
         t_store->is_transaction = true;
      }
   }

   DRETURN_VOID;
}

void sge_event_master_process_requests(monitoring_t *monitor)
{
   lList *requests = nullptr;

   DENTER(TOP_LAYER);

   /*
    * get the request list
    * put a new empty list in place to allow new requests while we process the old ones
    */
   sge_mutex_lock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);

   if (lGetNumberOfElem(Event_Master_Control.requests) > 0) {
      requests = Event_Master_Control.requests;
      Event_Master_Control.requests = lCreateListHash("Event Master Requests", EVR_Type, false);
   }

   sge_mutex_unlock("event_master_request_mutex", __func__, __LINE__, &Event_Master_Control.request_mutex);

   /* if there have been any requests - process them */
   if (requests != nullptr) {
      lListElem *request = nullptr;

      while ((request = lFirstRW(requests)) != nullptr) {
#if 0
         DPRINTF("processing event master request: %d\n", lGetUlong(request, EVR_operation));
#endif
         switch (lGetUlong(request, EVR_operation)) {
            case EVR_ADD_EVC:
               sge_event_master_process_add_event_client(request, monitor);
               break;
            case EVR_MOD_EVC:
               sge_event_master_process_mod_event_client(request, monitor);
               break;
            case EVR_DEL_EVC:
               break;
            case EVR_ADD_EVENT:
               sge_event_master_process_send(request, monitor);
               break;
            case EVR_ACK_EVENT:
               sge_event_master_process_ack(request, monitor);
               break;
         }

         lRemoveElem(requests, &request);
      }

      lFreeList(&requests);
   }
}

