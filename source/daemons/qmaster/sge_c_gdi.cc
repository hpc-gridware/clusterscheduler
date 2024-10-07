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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <cstdlib>
#include <cerrno>

#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_manop.h"

#include "gdi/version.h"
#include "gdi/sge_gdi_packet_pb_cull.h"
#include "gdi/sge_gdi_packet.h"

#include "sge_follow.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_thread_scheduler.h"
#include "sge_c_gdi.h"
#include "sge_host_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "sge_manop_qmaster.h"
#include "sge_centry_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_resource_quota_qmaster.h"
#include "configuration_qmaster.h"
#include "evm/sge_event_master.h"
#include "sched_conf_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_ckpt_qmaster.h"
#include "sge_hgroup_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "sge_thread_utility.h"
#include "sge_qmod_qmaster.h"
#include "sge_qmaster_threads.h"
#include "msg_common.h"
#include "msg_qmaster.h"

static void
sge_c_gdi_get_in_worker(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor);

static void
sge_c_gdi_get_in_listener(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor);

static void
sge_c_gdi_add(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, gdi_object_t *ao,
              int sub_command, monitoring_t *monitor);

static void
sge_c_gdi_del(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, int sub_command, monitoring_t *monitor);

static void
sge_c_gdi_mod(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
              int sub_command, monitoring_t *monitor);

static void
sge_c_gdi_copy(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
               int sub_command, monitoring_t *monitor);

static void
sge_c_gdi_trigger_in_listener(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor);

static void
sge_c_gdi_trigger_in_worker(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                            monitoring_t *monitor);

static void
sge_c_gdi_permcheck(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                    monitoring_t *monitor);

static void
sge_gdi_do_permcheck(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task);

static void
sge_c_gdi_replace(gdi_object_t *ao, sge_gdi_packet_class_t *packet,
                  sge_gdi_task_class_t *task, int sub_command, monitoring_t *monitor);


static void
sge_gdi_shutdown_event_client(const sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor);

static void
sge_gdi_tigger_thread_state_transition(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                                       monitoring_t *monitor);

static int
get_client_id(lListElem *, int *);

static void
trigger_scheduler_monitoring(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor);

static bool
sge_chck_mod_perm_user(const sge_gdi_packet_class_t *packet, lList **alpp, u_long32 target);

static bool
sge_task_check_get_perm_host(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor);

static bool
sge_chck_mod_perm_host(const sge_gdi_packet_class_t *packet, lList **alpp, u_long32 target);

static int
schedd_mod(lList **alpp, lListElem *modp, lListElem *ep, int add, const char *ruser,
           const char *rhost, gdi_object_t *object, int sub_command, monitoring_t *monitor);

/*
 * Prevent these functions made inline by compiler. This is
 * necessary for Solaris 10 dtrace pid provider to work.
 */
#ifdef SOLARIS
#pragma no_inline(sge_c_gdi_permcheck, sge_c_gdi_trigger, sge_c_gdi_copy, sge_c_gdi_get, sge_c_gdi_del, sge_c_gdi_mod, sge_c_gdi_add, sge_c_gdi_copy)
#endif

/* ------------------------------ generic gdi objects --------------------- */
/* *INDENT-OFF* */
static gdi_object_t gdi_object[] = {
        {SGE_CAL_LIST,     CAL_name,  CAL_Type,  "calendar",                SGE_TYPE_CALENDAR,        calendar_mod, calendar_spool, calendar_update_queue_states},
        {SGE_EV_LIST,      0,         nullptr,   "event",                   SGE_TYPE_NONE,            nullptr,      nullptr,        nullptr},
        {SGE_AH_LIST,      AH_name,   AH_Type,   "adminhost",               SGE_TYPE_ADMINHOST,       host_mod,     host_spool,     host_success},
        {SGE_SH_LIST,      SH_name,   SH_Type,   "submithost",              SGE_TYPE_SUBMITHOST,      host_mod,     host_spool,     host_success},
        {SGE_EH_LIST,      EH_name,   EH_Type,   "exechost",                SGE_TYPE_EXECHOST,        host_mod,     host_spool,     host_success},
        {SGE_CQ_LIST,      CQ_name,   CQ_Type,   "cluster queue",           SGE_TYPE_CQUEUE,          cqueue_mod,   cqueue_spool,   cqueue_success},
        {SGE_JB_LIST,      0,         nullptr,   "job",                     SGE_TYPE_JOB,             nullptr,      nullptr,        nullptr},
        {SGE_CE_LIST,      CE_name,   CE_Type,   "complex entry",           SGE_TYPE_CENTRY,          centry_mod,   centry_spool,   centry_success},
        {SGE_ORDER_LIST,   0,         nullptr,   "order",                   SGE_TYPE_NONE,            nullptr,      nullptr,        nullptr},
        {SGE_MASTER_EVENT, 0,         nullptr,   "master event",            SGE_TYPE_NONE,            nullptr,      nullptr,        nullptr},
        {SGE_UM_LIST,      0,         nullptr,   "manager",                 SGE_TYPE_MANAGER,         nullptr,      nullptr,        nullptr},
        {SGE_UO_LIST,      0,         nullptr,   "operator",                SGE_TYPE_OPERATOR,        nullptr,      nullptr,        nullptr},
        {SGE_PE_LIST,      PE_name,   PE_Type,   "parallel environment",    SGE_TYPE_PE,              pe_mod,       pe_spool,       pe_success},
        {SGE_CONF_LIST,    0,         nullptr,   "configuration",           SGE_TYPE_NONE,            nullptr,      nullptr,        nullptr},
        {SGE_SC_LIST,      0,         nullptr,   "scheduler configuration", SGE_TYPE_NONE,            schedd_mod,   nullptr,        nullptr},
        {SGE_UU_LIST,      UU_name,   UU_Type,   "user",                    SGE_TYPE_USER,            userprj_mod,  userprj_spool,  userprj_success},
        {SGE_US_LIST,      US_name,   US_Type,   "userset",                 SGE_TYPE_USERSET,         userset_mod,  userset_spool,  userset_success},
        {SGE_PR_LIST,      PR_name,   PR_Type,   "project",                 SGE_TYPE_PROJECT,         userprj_mod,  userprj_spool,  userprj_success},
        {SGE_STN_LIST,     0,         nullptr,   "sharetree",               SGE_TYPE_SHARETREE,       nullptr,      nullptr,        nullptr},
        {SGE_CK_LIST,      CK_name,   CK_Type,   "checkpoint interface",    SGE_TYPE_CKPT,            ckpt_mod,     ckpt_spool,     ckpt_success},
        {SGE_SME_LIST,     0,         nullptr,   "schedd info",             SGE_TYPE_JOB_SCHEDD_INFO, nullptr,      nullptr,        nullptr},
        {SGE_ZOMBIE_LIST,  0,         nullptr,   "job zombie list",         SGE_TYPE_ZOMBIE,          nullptr,      nullptr,        nullptr},
        {SGE_RQS_LIST,     RQS_name,  RQS_Type,  "resource quota set",      SGE_TYPE_RQS,             rqs_mod,      rqs_spool,      rqs_success},
        {SGE_HGRP_LIST,    HGRP_name, HGRP_Type, "host group",              SGE_TYPE_HGROUP,          hgroup_mod,   hgroup_spool,   hgroup_success},
        {SGE_AR_LIST,      AR_id,     AR_Type,   "advance reservation",     SGE_TYPE_AR,              ar_mod,       ar_spool,       ar_success},
        {SGE_DUMMY_LIST,   0,         nullptr,   "general request",         SGE_TYPE_NONE,            nullptr,      nullptr,        nullptr},
        {0,                0,         nullptr,   nullptr,                   SGE_TYPE_NONE,            nullptr,      nullptr,        nullptr}
};

/* *INDENT-ON* */

void sge_clean_lists() {
   int i = 0;

   for (; gdi_object[i].target != 0; i++) {
      if (gdi_object[i].list_type != SGE_TYPE_NONE) {
         lList **master_list = ocs::DataStore::get_master_list_rw(gdi_object[i].list_type);
         lFreeList(master_list);
      }
   }

}

#if 1

/* EB: TODO: CLEANUP: should be replaced with sge_gdi_packet_verify_version() */

/*
 * MT-NOTE: verify_request_version() is MT safe
 */
int verify_request_version(lList **alpp, u_long32 version, char *host, char *commproc, int id) {
   const char *client_version = nullptr;
   dstring ds;
   char buffer[256];
   const vdict_t *vp, *vdict = GRM_GDI_VERSION_ARRAY;

   DENTER(TOP_LAYER);

   if (version == GRM_GDI_VERSION) {
      DRETURN(0);
   }

   for (vp = &vdict[0]; vp->version; vp++) {
      if (version == vp->version) {
         client_version = vp->release;
         break;
      }
   }

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   if (client_version) {
      WARNING(MSG_GDI_WRONG_GDI_SSISS, host, commproc, id, client_version, feature_get_product_name(FS_VERSION, &ds));
   } else {
      WARNING(MSG_GDI_WRONG_GDI_SSIUS, host, commproc, id, sge_u32c(version), feature_get_product_name(FS_VERSION, &ds));
   }
   answer_list_add(alpp, SGE_EVENT, STATUS_EVERSION, ANSWER_QUALITY_ERROR);

   DRETURN(1);
}

#endif

bool
sge_c_gdi_process_in_listener(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                              lList **answer_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *target_name = nullptr;
   gdi_object_t *ao = get_gdi_object(task->target);
   if (ao != nullptr) {
      target_name = ao->object_name;
   }
   if (ao == nullptr || target_name == nullptr) {
      target_name = MSG_UNKNOWN_OBJECT;
   }

   int operation = SGE_GDI_GET_OPERATION(task->command);
   const char *operation_name = sge_gdi_task_get_operation_name(task);

   DPRINTF("GDI %s %s (%s/%s/%d) (%s/%d/%s/%d)\n", operation_name, target_name, packet->host, packet->commproc,
           (int) task->id, packet->user, (int) packet->uid, packet->group, (int) packet->gid);

   sge_pack_buffer *pb = &(packet->pb);
   switch (operation) {
      case SGE_GDI_TRIGGER:
#if 0
         MONITOR_GDI_TRIG(monitor);
#endif
         sge_c_gdi_trigger_in_listener(packet, task, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         DRETURN(true);
      case SGE_GDI_PERMCHECK:
#if 0
         MONITOR_GDI_PERM(monitor);
#endif
         sge_c_gdi_permcheck(packet, task, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         DRETURN(true);
      case SGE_GDI_GET:
#if 0
         MONITOR_GDI_GET(monitor);
#endif
         sge_c_gdi_get_in_listener(ao, packet, task, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         DRETURN(true);
      default:
         // requests that are not handled in listener will be processed in a worker thread
         DRETURN(false);
   }

   DRETURN(false);
}

bool
sge_c_gdi_check_execution_permission(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                                     monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   int operation = SGE_GDI_GET_OPERATION(task->command);
   switch (operation) {
      case SGE_GDI_GET:
         DRETURN(sge_task_check_get_perm_host(packet, task, monitor));
      case SGE_GDI_ADD:
      case SGE_GDI_MOD:
      case SGE_GDI_COPY:
      case SGE_GDI_REPLACE:
      case SGE_GDI_DEL: {
         if (!sge_chck_mod_perm_user(packet, &(task->answer_list), task->target)) {
            DRETURN(false);
         }
         if (!sge_chck_mod_perm_host(packet, &(task->answer_list), task->target)) {
            DRETURN(false);
         }
         DRETURN(true);
      }
      case SGE_GDI_TRIGGER:
      {
         const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
         const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);

         // operators are allowed to trigger rescheduling requests
         // other trigger requests must have been initiated by a manager
         if (task->target == SGE_CQ_LIST) {
            if (!manop_is_operator(packet, master_manager_list, master_operator_list)) {
               ERROR(MSG_SGETEXT_MUSTBEOPERATOR_S, packet->user);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         } else {
            if (!manop_is_manager(packet, master_manager_list)) {
               ERROR(MSG_SGETEXT_MUSTBEMANAGER_S, packet->user);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         }
         if (!sge_chck_mod_perm_host(packet, &(task->answer_list), task->target)) {
            DRETURN(false);
         }
         DRETURN(true);
      }
      case SGE_GDI_PERMCHECK:
      default:
         // no checks required anyone can do that
         break;
   }
   DRETURN(true);
}

/* ------------------------------------------------------------ */
void
sge_c_gdi_process_in_worker(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                            lList **answer_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *target_name = nullptr;
   gdi_object_t *ao = get_gdi_object(task->target);
   if (ao != nullptr) {
      target_name = ao->object_name;
   }
   if (ao == nullptr || target_name == nullptr) {
      target_name = MSG_UNKNOWN_OBJECT;
   }

   int sub_command = SGE_GDI_GET_SUBCOMMAND(task->command);
   int operation = SGE_GDI_GET_OPERATION(task->command);
   const char *operation_name = sge_gdi_task_get_operation_name(task);

#ifdef OBSERVE
   dstring target_dstr = DSTRING_INIT;
   if (task->target == SGE_ORDER_LIST) {
      sge_dstring_sprintf(&target_dstr, "%s", target_name);

      lListElem *order;
      for_each_ep(order, task->data_list) {
         switch (lGetUlong(order, OR_type)) {
            case ORT_start_job:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_start_job");
               break;
            case ORT_tickets:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_tickets");
               break;
            case ORT_ptickets:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_ptickets");
               break;
            case ORT_remove_job:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_remove_job");
               break;
            case ORT_update_project_usage:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_update_project_usage");
               break;
            case ORT_update_user_usage:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_update_user_usage");
               break;
            case ORT_share_tree:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_share_tree");
               break;
            case ORT_remove_immediate_job:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_remove_immediate_job");
               break;
            case ORT_sched_conf:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_sched_conf");
               break;
            case ORT_suspend_on_threshold:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_suspend_on_threshold");
               break;
            case ORT_unsuspend_on_threshold:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_unsuspend_on_threshold");
               break;
            case ORT_job_schedd_info:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_job_schedd_info");
               break;
            case ORT_clear_pri_info:
               sge_dstring_sprintf_append(&target_dstr, " %s", "ORT_clear_pri_info");
               break;
            default:
               sge_dstring_sprintf_append(&target_dstr, " %s", "UNKNOWN");
               break;
         }
      }
   } else {
      sge_dstring_sprintf(&target_dstr, "%s", target_name);
   }
   INFO("GDI %s %s (%s/%s/%d) (%s/%d/%s/%d)", operation_name, sge_dstring_get_string(&target_dstr), packet->host, packet->commproc, (int)task->id, packet->user, (int)packet->uid, packet->group, (int)packet->gid);
   sge_dstring_free(&target_dstr);
#else
   DPRINTF("GDI %s %s (%s/%s/%d) (%s/%d/%s/%d)\n", operation_name, target_name, packet->host, packet->commproc,
           (int) task->id, packet->user, (int) packet->uid, packet->group, (int) packet->gid);
#endif

   sge_pack_buffer *pb = &(packet->pb);
   switch (operation) {
      case SGE_GDI_GET:
         MONITOR_GDI_GET(monitor);
         sge_c_gdi_get_in_worker(ao, packet, task, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_ADD:
         MONITOR_GDI_ADD(monitor);
         sge_c_gdi_add(packet, task, ao, sub_command, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_DEL:
         MONITOR_GDI_DEL(monitor);
         sge_c_gdi_del(packet, task, sub_command, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_MOD:
         MONITOR_GDI_MOD(monitor);
         sge_c_gdi_mod(ao, packet, task, sub_command, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_COPY:
         MONITOR_GDI_CP(monitor);
         sge_c_gdi_copy(ao, packet, task, sub_command, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_TRIGGER:
         MONITOR_GDI_TRIG(monitor);
         sge_c_gdi_trigger_in_worker(packet, task, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_PERMCHECK:
         MONITOR_GDI_PERM(monitor);
         sge_c_gdi_permcheck(packet, task, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      case SGE_GDI_REPLACE:
         MONITOR_GDI_REPLACE(monitor);
         sge_c_gdi_replace(ao, packet, task, sub_command, monitor);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_UNKNOWNOP);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         sge_gdi_packet_pack_task(packet, task, answer_list, pb);
         break;
   }

   DRETURN_VOID;
}

static void
sge_c_gdi_get_in_listener(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // for get requests we do not need what the client might have sent to us
   lFreeList(&(task->data_list));

   // check the permission
   if (!sge_task_check_get_perm_host(packet, task, monitor)) {
      DRETURN_VOID;
   }

   // handle he get request
   switch (task->target) {
      case SGE_EV_LIST:
         task->data_list = sge_select_event_clients("", task->condition, task->enumeration);
         task->do_select_pack_simultaneous = false;
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_OKNL);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_END);
         DRETURN_VOID;
      case SGE_DUMMY_LIST:
         task->data_list = get_active_thread_list();
         task->do_select_pack_simultaneous = false;
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_OKNL);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_END);
         DRETURN_VOID;
      default:
         DRETURN_VOID;
   }
   DRETURN_VOID;
}

static void
sge_c_gdi_get_in_worker(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   /* Whatever client sent with this get request - we don't need it */
   lFreeList(&(task->data_list));

   switch (task->target) {
      case SGE_CONF_LIST:
         task->data_list = sge_get_configuration(task->condition, task->enumeration);
         task->do_select_pack_simultaneous = false;
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_OKNL);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_END);
         DRETURN_VOID;
      case SGE_SC_LIST: /* TODO EB: move this into the scheduler configuration,
                                    and pack the list right away */
      {
         lList *conf = nullptr;

         conf = sconf_get_config_list();
         task->data_list = lSelectHashPack("", conf, task->condition, task->enumeration, false, nullptr);
         task->do_select_pack_simultaneous = false;
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_OKNL);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_END);
         lFreeList(&conf);
      }
         DRETURN_VOID;
      default:

         /*
          * Issue 1365
          * If the scheduler is not available the information in the job info
          * messages are outdated. In this case we have to reject the request.
          */
         if (task->target == SGE_SME_LIST &&
             !sge_has_event_client(EV_ID_SCHEDD)) {
            answer_list_add(&(task->answer_list), MSG_SGETEXT_JOBINFOMESSAGESOUTDATED,
                            STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         } else if (ao == nullptr || ao->list_type == SGE_TYPE_NONE) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         } else {
            lList *data_source = *ocs::DataStore::get_master_list_rw(ao->list_type);

            if (packet->is_intern_request) {
               /* intern requests need no pb so it is not necessary to postpone the operation */
               task->data_list = lSelectHashPack("", data_source, task->condition,
                                                 task->enumeration, false, nullptr);
               task->do_select_pack_simultaneous = false;

               /*
                * DIRTY HACK: The "ok" message should be removed from the answer list
                * 05/21/2007 qualitiy was ANSWER_QUALITY_INFO but this results in "ok"
                * messages on qconf side
                */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_OKNL);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_END);

            } else {
               /* lSelect will be postponed till packing */
               task->data_list = data_source;
               task->do_select_pack_simultaneous = true;

               /*
                * answer list creation is also done during packing!!!!
                */
            }
         }

   }
   DRETURN_VOID;
}

/*
 * MT-NOTE: sge_c_gdi_add() is MT safe
 */
static void
sge_c_gdi_add(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
              gdi_object_t *ao, int sub_command, monitoring_t *monitor) {
   lListElem *ep;
   lList *ticket_orders = nullptr;
   bool reprioritize_tickets = (mconf_get_reprioritize() == 1) ? true : false;

   DENTER(TOP_LAYER);

   if (task->target == SGE_EV_LIST) {
      lListElem *next;

      next = lFirstRW(task->data_list);
      while ((ep = next) != nullptr) {/* is thread save. the global lock is used, when needed */
         next = lNextRW(ep);

         /* fill address infos from request into event client that must be added */
         lSetHost(ep, EV_host, packet->host);
         lSetString(ep, EV_commproc, packet->commproc);
         lSetUlong(ep, EV_commid, packet->commproc_id);

         /* fill in authentication infos from request */
         lSetUlong(ep, EV_uid, packet->uid);
         if (!event_client_verify(ep, &(task->answer_list), true)) {
            ERROR(MSG_QMASTER_INVALIDEVENTCLIENT_SSS, packet->user, packet->commproc, packet->host);
         } else {
            mconf_set_max_dynamic_event_clients(
                    sge_set_max_dynamic_event_clients(mconf_get_max_dynamic_event_clients()));

            sge_add_event_client(packet, ep, &(task->answer_list),
                                 (sub_command & SGE_GDI_RETURN_NEW_VERSION) ? &(task->data_list) : nullptr,
                                 (event_client_update_func_t) nullptr, nullptr);
         }
      }
   } else if (task->target == SGE_JB_LIST) {
      lListElem *next;

      next = lFirstRW(task->data_list);
      while ((ep = next) != nullptr) { /* is thread save. the global lock is used, when needed */
         next = lNextRW(ep);

         lDechainElem(task->data_list, ep);

         /* fill address infos from request into event client that must be added */
         if (!job_verify_submitted_job(ep, &(task->answer_list))) {
            ERROR(MSG_QMASTER_INVALIDJOBSUBMISSION_SSS, packet->user, packet->commproc, packet->host);
         } else {
            /* submit needs to know user and group */
            sge_gdi_add_job(&ep, &(task->answer_list),
                            (sub_command & SGE_GDI_RETURN_NEW_VERSION) ?
                            &(task->data_list) : nullptr,
                            packet, task, monitor);
         }
         lInsertElem(task->data_list, nullptr, ep);
      }
   } else if (task->target == SGE_SC_LIST) {
      lListElem *next;

      next = lFirstRW(task->data_list);
      while ((ep = next) != nullptr) {
         next = lNextRW(ep);

         sge_mod_sched_configuration(ep, &(task->answer_list), packet->user, packet->host);
      }
   } else {
      bool is_scheduler_resync = false;
      lList *tmp_list = nullptr;
      lListElem *next;

      if (task->target == SGE_ORDER_LIST) {
         sge_set_commit_required();
      }

      next = lFirstRW(task->data_list);
      while ((ep = next) != nullptr) {
         next = lNextRW(ep);

         /* add each element */
         switch (task->target) {

            case SGE_ORDER_LIST:
               switch (sge_follow_order(ep, packet->user, packet->host,
                                        reprioritize_tickets ? &ticket_orders : nullptr, monitor)) {
                  case STATUS_OK :
                  case 0 : /* everything went fine */
                     break;
                  case -2 :
                     is_scheduler_resync = true;
                  case -1 :
                  case -3 :
                     /* stop the order processing */
                     WARNING("Skipping remaining " sge_uu32 " orders", lGetNumberOfRemainingElem(ep));
                     next = nullptr;
                     break;

                  default :
                  DPRINTF("--> FAILED: unexpected state from in the order processing <--\n");
                     break;
               }
               break;
            case SGE_UM_LIST:
            case SGE_UO_LIST:
               sge_add_manop(ep, &(task->answer_list), packet->user, packet->host, task->target);
               break;

            case SGE_STN_LIST:
               sge_add_sharetree(ep, ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE), &(task->answer_list),
                                 packet->user, packet->host);
               break;

            default:
               if (!ao) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
                  answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
                  break;
               }

               if (task->target == SGE_EH_LIST && !strcmp(prognames[EXECD], packet->commproc)) {
                  bool is_restart = false;

                  if (sub_command == SGE_GDI_EXECD_RESTART) {
                     is_restart = true;
                  }

                  sge_execd_startedup(ep, &(task->answer_list), packet->user,
                                      packet->host, task->target, monitor, is_restart);
               } else {
                  sge_gdi_add_mod_generic(&(task->answer_list), ep, 1, ao, packet->user, packet->host,
                                          sub_command, &tmp_list, monitor);
               }
               break;
         }
      } /* while loop */

      if (task->target == SGE_ORDER_LIST) {
         sge_commit();
         sge_set_next_spooling_time();
         answer_list_add(&(task->answer_list), "OK\n", STATUS_OK, ANSWER_QUALITY_INFO);
      }

      if (is_scheduler_resync) {
         sge_resync_schedd(monitor); /* ask for a total update */
      }

      /*
      ** tmp_list contains the changed AR element, set in ar_success
      */
      if (SGE_GDI_IS_SUBCOMMAND_SET(sub_command, SGE_GDI_RETURN_NEW_VERSION)) {
         lFreeList(&(task->data_list));
         task->data_list = tmp_list;
         tmp_list = nullptr;
      }

      lFreeList(&tmp_list);
   }

   if (reprioritize_tickets && ticket_orders != nullptr) {
      distribute_ticket_orders(ticket_orders, monitor);
      lFreeList(&ticket_orders);
      DPRINTF("DISTRIBUTED NEW PRIORITIZE TICKETS\n");
   } else {
      /* tickets not needed at execd's if no repriorization is done */
      DPRINTF("NO TICKET DELIVERY\n");
   }

   DRETURN_VOID;
}

/*
 * MT-NOTE: sge_c_gdi-del() is MT safe
 */
void
sge_c_gdi_del(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, int sub_command, monitoring_t *monitor) {
   lListElem *ep;

   DENTER(GDI_LAYER);

   if (task->data_list == nullptr) {
      /* delete whole list */

      switch (task->target) {
         case SGE_STN_LIST:
            sge_del_sharetree(ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE), &(task->answer_list),
                              packet->user, packet->host);
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
            break;
      }
   } else {
      sge_set_commit_required();

      for_each_rw(ep, task->data_list) {
         /* try to remove the element */
         switch (task->target) {
            case SGE_AH_LIST:
            case SGE_SH_LIST:
            case SGE_EH_LIST:
               sge_del_host(ep, &(task->answer_list), packet->user, packet->host, task->target,
                            *ocs::DataStore::get_master_list_rw(SGE_TYPE_HGROUP));
               break;

            case SGE_CQ_LIST:
               cqueue_del(ep, &(task->answer_list), packet->user, packet->host);
               break;

            case SGE_JB_LIST:
               sge_gdi_del_job(packet, ep, &(task->answer_list), sub_command, monitor);
               break;

            case SGE_CE_LIST:
               sge_del_centry(ep, &(task->answer_list), packet->user, packet->host);
               break;

            case SGE_PE_LIST:
               sge_del_pe(ep, &(task->answer_list), packet->user, packet->host);
               break;

            case SGE_UM_LIST:
            case SGE_UO_LIST:
               sge_del_manop(ep, &(task->answer_list), packet->user, packet->host, task->target);
               break;

            case SGE_CONF_LIST:
               sge_del_configuration(ep, &(task->answer_list), packet->user, packet->host);
               break;

            case SGE_UU_LIST:
               sge_del_userprj(ep, &(task->answer_list), ocs::DataStore::get_master_list_rw(SGE_TYPE_USER),
                               packet->user, packet->host, 1);
               break;

            case SGE_US_LIST:
               sge_del_userset(ep, &(task->answer_list), ocs::DataStore::get_master_list_rw(SGE_TYPE_USERSET),
                               packet->user, packet->host);
               break;

            case SGE_PR_LIST:
               sge_del_userprj(ep, &(task->answer_list), ocs::DataStore::get_master_list_rw(SGE_TYPE_PROJECT),
                               packet->user, packet->host, 0);
               break;

            case SGE_RQS_LIST:
               rqs_del(ep, &(task->answer_list), ocs::DataStore::get_master_list_rw(SGE_TYPE_RQS), packet->user,
                       packet->host);
               break;

            case SGE_CK_LIST:
               sge_del_ckpt(ep, &(task->answer_list), packet->user, packet->host);
               break;

            case SGE_CAL_LIST:
               sge_del_calendar(ep, &(task->answer_list), packet->user, packet->host);
               break;
            case SGE_HGRP_LIST:
               hgroup_del(ep, &(task->answer_list), packet->user, packet->host);
               break;
            case SGE_AR_LIST:
               ar_del(packet, ep, &(task->answer_list), ocs::DataStore::get_master_list_rw(SGE_TYPE_AR), monitor);
               break;
            default:
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
               break;
         } /* switch target */

      } /* for_each element */

      sge_commit();
   }

   DRETURN_VOID;
}

/*
 * MT-NOTE: sge_c_gdi_copy() is MT safe
 */
static void sge_c_gdi_copy(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                           int sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   lListElem *ep;
   for_each_rw (ep, task->data_list) {
      switch (task->target) {
         case SGE_JB_LIST:
            /* gdi_copy_job uses the global lock internal */
            sge_gdi_copy_job(ep, &(task->answer_list),
                             (sub_command & SGE_GDI_RETURN_NEW_VERSION) ? &(task->answer_list) : nullptr,
                             packet, task, monitor);
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
            break;
      }
   }
   DRETURN_VOID;
}

/* ------------------------------------------------------------ */

static void
sge_gdi_do_permcheck(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task) {
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   const lList *master_admin_host_list = *ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST);
   const lList *master_submit_host_list = *ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST);

   DENTER(TOP_LAYER);

   // whatever client sent - we do not need that, instead we use data from commlib stored in packet
   lFreeList(&task->data_list);
   const char *hostname = packet->host;
   const char *username = packet->user;

   // create list with element and fill in username, hostname and corresponding permission
   lListElem *ep = lAddElemStr(&task->data_list, PERM_username, username, PERM_Type);
   lSetHost(ep, PERM_host, hostname);

   // add user roles
   bool is_manager = manop_is_manager(packet, master_manager_list);
   bool is_operator = manop_is_operator(packet, master_manager_list, master_operator_list);
   lSetBool(ep, PERM_is_manager, is_manager);
   lSetBool(ep, PERM_is_operator, is_operator);

   // add host roles
   bool is_admin_host = (lGetElemHost(master_admin_host_list, AH_name, hostname) != nullptr);
   bool is_submit_host = (lGetElemHost(master_submit_host_list, SH_name, hostname) != nullptr);
   lSetBool(ep, PERM_is_admin_host, is_admin_host);
   lSetBool(ep, PERM_is_submit_host, is_submit_host);

   // show the results
   DPRINTF("manager/operator/admin_host/submit_host: %s/%s/%s/%s\n",
           is_manager ? "true" : "false",
           is_operator ? "true" : "false",
           is_admin_host ? "true" : "false",
           is_submit_host ? "true" : "false");

   // client might have sent lCondition and lEnumeration (where/what) but we ignore it for this kind of request

   // report success
   answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
   DRETURN_VOID;
}

static void
sge_c_gdi_permcheck(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   switch (task->target) {
      case SGE_DUMMY_LIST:
         sge_gdi_do_permcheck(packet, task);
         break;
      default:
         WARNING(SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   }
   DRETURN_VOID;
}

void sge_c_gdi_replace(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                       int sub_command, monitoring_t *monitor) {
   lList *tmp_list = nullptr;
   lListElem *ep = nullptr;

   DENTER(GDI_LAYER);

   switch (task->target) {
      case SGE_RQS_LIST: {
         if (rqs_replace_request_verify(&(task->answer_list), task->data_list) != true) {
            DRETURN_VOID;
         }
         /* delete all currently defined rule sets */
         ep = lFirstRW(*ocs::DataStore::get_master_list(SGE_TYPE_RQS));
         while (ep != nullptr) {
            rqs_del(ep, &(task->answer_list), ocs::DataStore::get_master_list_rw(SGE_TYPE_RQS), packet->user,
                    packet->host);
            ep = lFirstRW(*ocs::DataStore::get_master_list(SGE_TYPE_RQS));
         }

         for_each_rw (ep, task->data_list) {
            sge_gdi_add_mod_generic(&(task->answer_list), ep, 1, ao, packet->user, packet->host, SGE_GDI_SET_ALL,
                                    &tmp_list, monitor);
         }
         lFreeList(&tmp_list);
      }
         break;
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         break;
   }
   DRETURN_VOID;
}

static void
sge_c_gdi_trigger_in_listener(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   DENTER(GDI_LAYER);
   u_long32 target = task->target;
   switch (target) {
      case SGE_MASTER_EVENT:
         // shutdown request for qmaster (qconf -km)
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host)) {
            ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN_VOID;
         }
         sge_gdi_kill_master(packet, task);
         DRETURN_VOID;
      case SGE_SC_LIST:
         // trigger scheduling (qconf -tsm)
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host)) {
            ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN_VOID;
         }
         trigger_scheduler_monitoring(packet, task, monitor);
         DRETURN_VOID;
      case SGE_EV_LIST:
         // shutdown of event client (qconf -kec)
         sge_gdi_shutdown_event_client(packet, task, monitor);
         answer_list_log(&(task->answer_list), false, true);
         DRETURN_VOID;
      case SGE_DUMMY_LIST:
         // start stop of scheduler (qconf -ks | -kt scheduler | -at scheduler)
         sge_gdi_tigger_thread_state_transition(packet, task, monitor);
         answer_list_log(&(task->answer_list), false, true);
         DRETURN_VOID;
      default:
         // unknown operation for a listener thread
         WARNING(SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
   }
   DRETURN_VOID;
}

static void
sge_c_gdi_trigger_in_worker(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   DENTER(GDI_LAYER);
   u_long32 target = task->target;
   switch (target) {
      case SGE_EH_LIST:
         // shutdown of execd (qconf -ke)
         sge_gdi_kill_exechost(packet, task);
         DRETURN_VOID;
      case SGE_CQ_LIST:
      case SGE_JB_LIST:
         // state changes of queues and jobs
         sge_set_commit_required();
         sge_gdi_qmod(packet, task, monitor);
         sge_commit();
         DRETURN_VOID;
      default:
         // unknown operation for a worker thread
         WARNING(SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
   }
   DRETURN_VOID;
}

static void
sge_gdi_tigger_thread_state_transition(sge_gdi_packet_class_t *packet,
                                       sge_gdi_task_class_t *task,
                                       monitoring_t *monitor) {
   lListElem *elem = nullptr; /* ID_Type */
   lList **answer_list = &(task->answer_list);

   DENTER(TOP_LAYER);
   for_each_rw(elem, task->data_list) {
      const char *name = lGetString(elem, ID_str);
      sge_thread_state_transitions_t action = (sge_thread_state_transitions_t) lGetUlong(elem, ID_action);

      if (name != nullptr) {
         if (strcasecmp(name, threadnames[SCHEDD_THREAD]) == 0) {
            if (action == SGE_THREAD_TRIGGER_START) {
               sge_scheduler_initialize(answer_list);
            } else if (action == SGE_THREAD_TRIGGER_STOP) {
               sge_scheduler_terminate(answer_list);
            } else {
               ERROR(MSG_TRIGGER_STATENOTSUPPORTED_DS, action, name);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
            }
         } else {
            ERROR(MSG_TRIGGER_NOTSUPPORTED_S, name);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }
      } else {
         ERROR(MSG_TRIGGER_NOTSUPPORTED_S, "");
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      }
   }
   DRETURN_VOID;
}

/****** qmaster/sge_c_gdi_process_in_worker/sge_gdi_shutdown_event_client() **********************
*  NAME
*     sge_gdi_shutdown_event_client() -- shutdown event client
*
*  SYNOPSIS
*     static void
*     sge_gdi_shutdown_event_client(sge_gdi_ctx_class_t *ctx,
*                                   sge_gdi_packet_class_t *packet,
*                                   sge_gdi_task_class_t *task,
*                                   monitoring_t *monitor)
*
*  FUNCTION
*     Shutdown event clients by client id. tasks data_list does contain a list of
*     client id's. This is a list of 'ID_Type' elements.
*
*  INPUTS
*     sge_gdi_ctx_class_t *ctx - context
*     sge_gdi_packet_class_t *packet - request packet
*     sge_gdi_task_class_t *task - request task
*     monitoring_t *monitor - the monitoring structure
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_gdi_shutdown_event_client() is NOT MT safe.
*
*******************************************************************************/
static void
sge_gdi_shutdown_event_client(const sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   lListElem *elem = nullptr; /* ID_Type */

   DENTER(TOP_LAYER);

   for_each_rw(elem, task->data_list) {
      lList *local_alp = nullptr;
      int client_id = EV_ID_ANY;
      int res = -1;

      if (get_client_id(elem, &client_id) != 0) {
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         continue;
      }

      if (client_id == EV_ID_SCHEDD &&
          !host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host)) {
         ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
         continue;
      } else if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST), packet->host)
                 && !host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host)) {
         ERROR(MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
         continue;
      }

      /* thread shutdown */
      if (client_id == EV_ID_SCHEDD) {
         sge_scheduler_terminate(nullptr);
      }

      if (client_id == EV_ID_ANY) {
         res = sge_shutdown_dynamic_event_clients(packet, &local_alp, monitor);
      } else {
         res = sge_shutdown_event_client(packet, client_id, &local_alp);
      }

      if ((res == EINVAL) && (client_id == EV_ID_SCHEDD)) {
         lFreeList(&local_alp);
         answer_list_add(&(task->answer_list), MSG_COM_NOSCHEDDREGMASTER, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      } else {
         answer_list_append_list(&(task->answer_list), &local_alp);
      }
   }

   DRETURN_VOID;
} /* sge_gdi_shutdown_event_client() */

/****** qmaster/sge_c_gdi_process_in_worker/get_client_id() **************************************
*  NAME
*     get_client_id() -- get client id from ID_Type element.
*
*  SYNOPSIS
*     static int get_client_id(lListElem *anElem, int *anID)
*
*  FUNCTION
*     Get client id from ID_Type element. The client id is converted to an
*     integer and stored in 'anID'.
*
*  INPUTS
*     lListElem *anElem - ID_Type element
*     int *anID         - will contain client id on return
*
*  RESULT
*     EINVAL - failed to extract client id.
*     0      - otherwise
*
*  NOTES
*     MT-NOTE: get_client_id() is MT safe.
*
*     Using 'errno' to check for 'strtol' error situations is recommended
*     by POSIX.
*
*******************************************************************************/
static int get_client_id(lListElem *anElem, int *anID) {
   const char *id = nullptr;

   DENTER(TOP_LAYER);

   if ((id = lGetString(anElem, ID_str)) == nullptr) {
      DRETURN(EINVAL);
   }

   errno = 0; /* errno is thread local */

   *anID = strtol(id, nullptr, 0);

   if (errno != 0) {
      ERROR(MSG_GDI_EVENTCLIENTIDFORMAT_S, id);
      DRETURN(EINVAL);
   }

   DRETURN(0);
} /* get_client_id() */

/****** qmaster/sge_c_gdi_process_in_worker/trigger_scheduler_monitoring() ***********************
*  NAME
*     trigger_scheduler_monitoring() -- trigger scheduler monitoring
*
*  SYNOPSIS
*     static void
*     trigger_scheduler_monitoring(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
*                                  monitoring_t *monitor)
*
*  FUNCTION
*     Trigger scheduler monitoring.
*
*  INPUTS
*     sge_gdi_packet_class_t *packet - request packet
*     sge_gdi_task_class_t *task - request task
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: trigger_scheduler_monitoring() is MT safe, using global lock
*
*  SEE ALSO
*     qconf -tsm
*
*******************************************************************************/
static void
trigger_scheduler_monitoring(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task,
                             monitoring_t *monitor) {
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);

   DENTER(GDI_LAYER);

   if (!manop_is_manager(packet, master_manager_list)) {
      WARNING(SFNMAX, MSG_COM_NOSCHEDMONPERMS);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_WARNING);
      DRETURN_VOID;
   }
   if (!sge_add_event_for_client(EV_ID_SCHEDD, 0, sgeE_SCHEDDMONITOR, 0, 0, nullptr, nullptr, nullptr, nullptr)) {
      WARNING(SFNMAX, MSG_COM_NOSCHEDDREGMASTER);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      DRETURN_VOID;
   }

   INFO(MSG_COM_SCHEDMON_SS, packet->user, packet->host);
   answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN_VOID;
} /* trigger_scheduler_monitoring() */

/*
 * MT-NOTE: sge_c_gdi_mod() is MT safe
 */
static void sge_c_gdi_mod(gdi_object_t *ao, sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, int sub_command,
                          monitoring_t *monitor) {
   lListElem *ep;
   lList *tmp_list = nullptr;
   bool is_locked = false;

   DENTER(TOP_LAYER);

   for_each_rw(ep, task->data_list) {
      if (task->target == SGE_CONF_LIST) {
         sge_mod_configuration(ep, &(task->answer_list), packet->user, packet->host);
      } else if (task->target == SGE_EV_LIST) {
         /* fill address infos from request into event client that must be added */
         lSetHost(ep, EV_host, packet->host);
         lSetString(ep, EV_commproc, packet->commproc);
         lSetUlong(ep, EV_commid, packet->id);

         /* fill in authentication infos from request */
         lSetUlong(ep, EV_uid, packet->uid);
         if (!event_client_verify(ep, &(task->answer_list), false)) {
            ERROR(MSG_QMASTER_INVALIDEVENTCLIENT_SSS, packet->user, packet->commproc, packet->host);
         } else {
            sge_mod_event_client(ep, &(task->answer_list), packet->user, packet->host);
         }
      } else if (task->target == SGE_SC_LIST) {
         sge_mod_sched_configuration(ep, &(task->answer_list), packet->user, packet->host);
      } else {
         if (!is_locked) {
            sge_set_commit_required();
            is_locked = true;
         }

         switch (task->target) {
            case SGE_JB_LIST:
               sge_gdi_mod_job(packet, ep, &(task->answer_list), sub_command);
               break;

            case SGE_STN_LIST:
               sge_mod_sharetree(ep, ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE),
                                 &(task->answer_list), packet->user, packet->host);
               break;
            default:
               if (ao == nullptr) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
                  answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
                  break;
               }
               sge_gdi_add_mod_generic(&(task->answer_list), ep, 0, ao, packet->user, packet->host, sub_command,
                                       &tmp_list, monitor);
               break;
         }
      }
   } /* for_each */

   if (is_locked) {
      sge_commit();
   }

   /* postprocessing for the list of requests */
   if (lGetNumberOfElem(tmp_list) != 0) {
      switch (task->target) {
         case SGE_CE_LIST:
         DPRINTF("rebuilding consumable debitation\n");
            centry_redebit_consumables(tmp_list);
            break;
      }
   }

   lFreeList(&tmp_list);

   DRETURN_VOID;
}

static bool
sge_chck_mod_perm_user(const sge_gdi_packet_class_t *packet, lList **alpp, u_long32 target) {
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);

   DENTER(TOP_LAYER);

   /* check permissions of user */
   switch (target) {

      case SGE_ORDER_LIST:
      case SGE_AH_LIST:
      case SGE_SH_LIST:
      case SGE_EH_LIST:
      case SGE_CQ_LIST:
      case SGE_CE_LIST:
      case SGE_UO_LIST:
      case SGE_UM_LIST:
      case SGE_PE_LIST:
      case SGE_CONF_LIST:
      case SGE_SC_LIST:
      case SGE_UU_LIST:
      case SGE_PR_LIST:
      case SGE_STN_LIST:
      case SGE_CK_LIST:
      case SGE_CAL_LIST:
      case SGE_USER_MAPPING_LIST:
      case SGE_HGRP_LIST:
      case SGE_RQS_LIST:
      case SGE_MASTER_EVENT:
         /* user must be a manager */
         if (!manop_is_manager(packet, master_manager_list)) {
            ERROR(MSG_SGETEXT_MUSTBEMANAGER_S, packet->user);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;

      case SGE_US_LIST:
         /* user must be a operator */
         if (!manop_is_operator(packet, master_manager_list, master_operator_list)) {
            ERROR(MSG_SGETEXT_MUSTBEOPERATOR_S, packet->user);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;

      case SGE_JB_LIST:

         /*
            what checking could we do here ?

            we had to check if there is a queue configured for scheduling
            of jobs of this group/user. If there is no such queue we
            had to deny submitting.

            Other checkings need to be done in stub functions.

         */
         break;

      case SGE_EV_LIST:
         /*
            an event client can be started by any user - it can only
            read objects like SGE_GDI_GET
            delete requires more info - is done in sge_gdi_kill_eventclient
         */
         break;
      case SGE_AR_LIST: {
         const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);

         // only managers and ARUSERS are allowed to create AR's
         if (!manop_is_manager(packet, master_manager_list) && !user_is_ar_user(packet, master_userset_list)) {
            ERROR(MSG_SGETEXT_MUSTBEMANAGERORUSER_SS, packet->user, AR_USERS);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;
      }
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN(false);
   }

   DRETURN(true);
}

static bool
sge_chck_mod_perm_host(const sge_gdi_packet_class_t *packet, lList **alpp, u_long32 target) {
   DENTER(TOP_LAYER);

   /* check permissions of host */
   switch (target) {

      case SGE_ORDER_LIST:
      case SGE_AH_LIST:
      case SGE_UO_LIST:
      case SGE_UM_LIST:
      case SGE_SH_LIST:
      case SGE_CQ_LIST:
      case SGE_CE_LIST:
      case SGE_PE_LIST:
      case SGE_CONF_LIST:
      case SGE_SC_LIST:
      case SGE_UU_LIST:
      case SGE_US_LIST:
      case SGE_PR_LIST:
      case SGE_STN_LIST:
      case SGE_CK_LIST:
      case SGE_CAL_LIST:
      case SGE_USER_MAPPING_LIST:
      case SGE_HGRP_LIST:
      case SGE_RQS_LIST:
      case SGE_MASTER_EVENT:
      case SGE_DUMMY_LIST:

         /* host must be SGE_AH_LIST */
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host)) {
            ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;

      case SGE_EH_LIST:

         /* host must be either admin host or exec host and execd */

         if (!(host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host) ||
               (host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST), packet->host) &&
                !strcmp(packet->commproc, prognames[EXECD])))) {
            ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;

      case SGE_JB_LIST:
         /* host must be SGE_SH_LIST */
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST), packet->host)) {
            ERROR(MSG_SGETEXT_NOSUBMITHOST_S, packet->host);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;
      case SGE_EV_LIST:
         /* to start an event client or if an event client
            performs modify requests on itself
            it must be on a submit or an admin host
          */
         if ((!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST), packet->host))
             && (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), packet->host))) {
            ERROR(MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;
      case SGE_AR_LIST:
         /* host must be SGE_SH_LIST */
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST), packet->host)) {
            ERROR(MSG_SGETEXT_NOSUBMITHOST_S, packet->host);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN(false);
   }

   DRETURN(true);
}


/* EB: TODO: ST: skip execution of this function if it is internal GDI request */
/* EB: TODO: ST: move usage of this code into listener in future */
static bool
sge_task_check_get_perm_host(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, monitoring_t *monitor) {
   bool ret = true;
   u_long32 target;
   char *host = nullptr;

   DENTER(TOP_LAYER);

   target = task->target;
   host = packet->host;

   /* check permissions of host */
   switch (target) {
      case SGE_ORDER_LIST:
      case SGE_EV_LIST:
      case SGE_AH_LIST:
      case SGE_UO_LIST:
      case SGE_UM_LIST:
      case SGE_SH_LIST:
      case SGE_CQ_LIST:
      case SGE_CE_LIST:
      case SGE_PE_LIST:
      case SGE_SC_LIST:
      case SGE_UU_LIST:
      case SGE_US_LIST:
      case SGE_PR_LIST:
      case SGE_STN_LIST:
      case SGE_CK_LIST:
      case SGE_CAL_LIST:
      case SGE_USER_MAPPING_LIST:
      case SGE_HGRP_LIST:
      case SGE_EH_LIST:
      case SGE_JB_LIST:
      case SGE_ZOMBIE_LIST:
      case SGE_SME_LIST:
      case SGE_RQS_LIST:
      case SGE_AR_LIST:
      case SGE_DUMMY_LIST:
         /* host must be admin or submit host */
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), host) &&
             !host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST), host)) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NOSUBMITORADMINHOST_S, host);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         break;
      case SGE_CONF_LIST:
         /* host must be admin or submit host or exec host */
         if (!host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_ADMINHOST), host) &&
             !host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_SUBMITHOST), host) &&
             !host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST), host)) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NOSUBMITORADMINHOST_S, host);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         break;
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_OPNOIMPFORTARGET);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         ret = false;
         return ret;
   }

   DRETURN(ret);
}


/*
   this is our strategy:

   do common checks and search old object
   make a copy of the old object (this will become the new object)
   modify new object using reduced object as instruction
      on error: dispose new object
   store new object to disc
      on error: dispose new object
   on success create events
   replace old object by new queue
*/
int
sge_gdi_add_mod_generic(lList **alpp, lListElem *instructions, int add, gdi_object_t *object, const char *ruser,
                        const char *rhost, int sub_command, lList **tmp_list, monitoring_t *monitor) {
   int pos;
   int dataType;
   const char *name;
   lList *tmp_alp = nullptr;
   lListElem *new_obj = nullptr;
   lListElem *old_obj;

   dstring buffer;
   char ds_buffer[256];

   DENTER(TOP_LAYER);

   sge_dstring_init(&buffer, ds_buffer, sizeof(ds_buffer));

   /* DO COMMON CHECKS AND SEARCH OLD OBJECT */
   if (!instructions || !object) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no element of this type, if ep doesn't contain the the primary key attribute */
   if (lGetPosViaElem(instructions, object->key_nm, SGE_NO_ABORT) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(object->key_nm), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /*
    * resolve host name in case of objects with hostnames as key
    * before searching for the objects
    */
   if (object->key_nm == EH_name ||
       object->key_nm == AH_name ||
       object->key_nm == SH_name) {
      if (sge_resolve_host(instructions, object->key_nm) != CL_RETVAL_OK) {
         const char *host = lGetHost(instructions, object->key_nm);
         ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, host ? host : "nullptr");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* get and verify the primary key */
   pos = lGetPosViaElem(instructions, object->key_nm, SGE_NO_ABORT);
   dataType = lGetPosType(lGetElemDescr(instructions), pos);
   if (dataType == lUlongT) {
      u_long32 id = lGetUlong(instructions, object->key_nm);
      sge_dstring_sprintf(&buffer, sge_u32, id);
      name = sge_dstring_get_string(&buffer);

      old_obj = lGetElemUlongRW(*ocs::DataStore::get_master_list(object->list_type), object->key_nm, id);
   } else if (dataType == lHostT) {
      name = lGetHost(instructions, object->key_nm);
      old_obj = lGetElemHostRW(*ocs::DataStore::get_master_list(object->list_type), object->key_nm, name);
   } else {
      name = lGetString(instructions, object->key_nm);
      old_obj = lGetElemStrRW(*ocs::DataStore::get_master_list(object->list_type), object->key_nm, name);
   }

   if (name == nullptr) {
      answer_list_add(alpp, MSG_OBJ_NAME_MISSING,
                      STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* prevent duplicates / modifying non existing objects */
   if ((old_obj && add) || (!old_obj && !add)) {
      ERROR(add ? MSG_SGETEXT_ALREADYEXISTS_SS : MSG_SGETEXT_DOESNOTEXIST_SS, object->object_name, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* create a new object (add case), or make a copy of the old object (mod case) */
   if (!(new_obj = (add
                    ? lCreateElem(object->type)
                    : lCopyElem(old_obj)))) {
      ERROR(SFNMAX, MSG_MEM_MALLOC);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* modify the new object base on information in the request */
   if (object->modifier(&tmp_alp, new_obj, instructions, add, ruser, rhost, object, sub_command, monitor) != 0) {

      if (alpp) {
         /* ON ERROR: DISPOSE NEW OBJECT */
         /* failure: just append last elem in tmp_alp
            elements before may contain invalid success messages */
         if (tmp_alp) {
            if (!*alpp) {
               *alpp = lCreateList("answer", AN_Type);
            }

            if (object->type == AR_Type) {
               lAppendList(*alpp, tmp_alp);
               lFreeList(&tmp_alp);
            } else {
               lListElem *failure = lLastRW(tmp_alp);

               lDechainElem(tmp_alp, failure);
               lAppendElem(*alpp, failure);
               lFreeList(&tmp_alp);
            }
         }
      }
      lFreeElem(&new_obj);
      DRETURN(STATUS_EUNKNOWN);
   }


   /* write on file */
   if (object->writer(alpp, new_obj, object)) {
      lFreeElem(&new_obj);
      lFreeList(&tmp_alp);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (alpp != nullptr) {
      if (*alpp == nullptr) {
         *alpp = lCreateList("answer", AN_Type);
      }

      /* copy tmp_alp to alpp */
      lAppendList(*alpp, tmp_alp);
   }
   lFreeList(&tmp_alp);

   {
      lList **master_list = ocs::DataStore::get_master_list_rw(object->list_type);

      /* chain out the old object */
      if (old_obj) {
         lDechainElem(*master_list, old_obj);
      }

      /* ensure our global list exists */
      if (*master_list == nullptr) {
         *master_list = lCreateList(object->object_name, object->type);
      }

      /* chain in new object */
      lAppendElem(*master_list, new_obj);
   }

   /* once we successfully added/modified the object, do final steps (on_success callback) */
   if (object->on_success) {
      object->on_success(new_obj, old_obj, object, tmp_list, monitor);
   }

   lFreeElem(&old_obj);

   if (!SGE_GDI_IS_SUBCOMMAND_SET(sub_command, SGE_GDI_RETURN_NEW_VERSION)) {
      INFO(add ? MSG_SGETEXT_ADDEDTOLIST_SSSS : MSG_SGETEXT_MODIFIEDINLIST_SSSS, ruser, rhost, name, object->object_name);

      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   DRETURN(STATUS_OK);
}

/*
 * MT-NOTE: get_gdi_object() is MT safe
 */
gdi_object_t *get_gdi_object(u_long32 target) {
   int i;

   DENTER(TOP_LAYER);

   for (i = 0; gdi_object[i].target; i++) {
      if (target == gdi_object[i].target) {
         DRETURN(&gdi_object[i]);
      }
   }

   DRETURN(nullptr);
}

static int schedd_mod(lList **alpp, lListElem *modp, lListElem *ep, int add, const char *ruser,
                      const char *rhost, gdi_object_t *object, int sub_command,
                      monitoring_t *monitor) {
   int ret;
   DENTER(TOP_LAYER);

   ret = sconf_validate_config_(alpp) ? 0 : 1;

   DRETURN(ret);
}

