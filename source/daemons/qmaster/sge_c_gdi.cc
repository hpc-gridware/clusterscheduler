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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The generic GDI request path: one table, every configuration object
 */

#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "tom/ocs_Types.h"

#include "sgeobj/ocs_AutoAddHosts.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/tom/ocs_tom_EV.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PERM.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "gdi/ocs_gdi_Command.h"

#include "sge_follow.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_thread_scheduler.h"
#include "sge_c_gdi.h"
#include "sge_c_gdi_procedure.h"

#include "sge_host_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "sge_centry_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_role_qmaster.h"
#include "sge_resource_quota_qmaster.h"
#include "configuration_qmaster.h"
#include "evm/sge_event_master.h"
#include "evm/msg_evmlib.h"
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
sge_c_gdi_get_in_worker(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

static void
sge_c_gdi_get_in_listener(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

static void
sge_c_gdi_add(ocs::gdi::Packet *packet, ocs::gdi::Task *task, gdi_object_t *ao,
              ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

static void
sge_c_gdi_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

static void
sge_c_gdi_mod(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task,
              ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

static void
sge_c_gdi_copy(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task,
               ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

static void
sge_c_gdi_trigger_in_listener(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

static void
sge_c_gdi_trigger_in_worker(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                            monitoring_t *monitor);

static void
sge_c_gdi_permcheck(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                    monitoring_t *monitor);

static void
sge_gdi_do_permcheck(ocs::gdi::Packet *packet, ocs::gdi::Task *task);

static void
sge_gdi_shutdown_event_client(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

static void
sge_gdi_tigger_thread_state_transition(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                       monitoring_t *monitor);

static int
get_client_id(ocs::tom::ConstElement<ocs::tom::ID> anElem, int *anID);

static void
trigger_scheduler_monitoring(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

static bool
sge_task_check_get_perm_host(ocs::gdi::Packet *packet, ocs::gdi::Task *task);

static void
ocs_gdi_add_mod_list(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::RawList requests, int add,
                     gdi_object_t *object, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
                     ocs_gdi_post_processing *post,
                     monitoring_t *monitor);

/*
 * Prevent these functions made inline by compiler. This is
 * necessary for Solaris 10 dtrace pid provider to work.
 */
#ifdef SOLARIS
#pragma no_inline(sge_c_gdi_permcheck, sge_c_gdi_trigger, sge_c_gdi_copy, sge_c_gdi_get, sge_c_gdi_del, sge_c_gdi_mod, sge_c_gdi_add, sge_c_gdi_copy)
#endif

/* ------------------------------ generic gdi objects --------------------- */
/*
 * The run-time part of each object type. The typed modifier, writer and on_success of a type are bound to it at
 * compile time by ocs_gdi_hooks below, and called directly by ocs_gdi_add_mod().
 */
/* *INDENT-OFF* */
static gdi_object_t gdi_object[] = {
   {ocs::gdi::Target::CAL_LIST,     CAL_name,  ocs::tom::CAL::descriptor.e,  "calendar",                SGE_TYPE_CALENDAR},
   {ocs::gdi::Target::EV_LIST,      0,         nullptr,                      "event",                   SGE_TYPE_NONE},
   {ocs::gdi::Target::EH_LIST,      EH_name,   ocs::tom::EH::descriptor.e,   "exechost",                SGE_TYPE_EXECHOST},
   {ocs::gdi::Target::CQ_LIST,      CQ_name,   ocs::tom::CQ::descriptor.e,   "cluster queue",           SGE_TYPE_CQUEUE},
   {ocs::gdi::Target::JB_LIST,      0,         nullptr,                      "job",                     SGE_TYPE_JOB},
   {ocs::gdi::Target::CE_LIST,      CE_name,   ocs::tom::CE::descriptor.e,   "complex entry",           SGE_TYPE_CENTRY},
   {ocs::gdi::Target::ORDER_LIST,   0,         nullptr,                      "order",                   SGE_TYPE_NONE},
   {ocs::gdi::Target::MASTER_EVENT, 0,         nullptr,                      "master event",            SGE_TYPE_NONE},
   {ocs::gdi::Target::PE_LIST,      PE_name,   ocs::tom::PE::descriptor.e,   "parallel environment",    SGE_TYPE_PE},
   {ocs::gdi::Target::CONF_LIST,    0,         nullptr,                      "configuration",           SGE_TYPE_NONE},
   {ocs::gdi::Target::SC_LIST,      0,         nullptr,                      "scheduler configuration", SGE_TYPE_NONE},
   {ocs::gdi::Target::UU_LIST,      UU_name,   ocs::tom::UU::descriptor.e,   "user",                    SGE_TYPE_USER},
   {ocs::gdi::Target::US_LIST,      US_name,   ocs::tom::US::descriptor.e,   "userset",                 SGE_TYPE_USERSET},
   {ocs::gdi::Target::PR_LIST,      PR_name,   ocs::tom::PR::descriptor.e,   "project",                 SGE_TYPE_PROJECT},
   {ocs::gdi::Target::STN_LIST,     0,         nullptr,                      "sharetree",               SGE_TYPE_SHARETREE},
   {ocs::gdi::Target::CK_LIST,      CK_name,   ocs::tom::CK::descriptor.e,   "checkpoint interface",    SGE_TYPE_CKPT},
   {ocs::gdi::Target::SME_LIST,     0,         nullptr,                      "schedd info",             SGE_TYPE_JOB_SCHEDD_INFO},
   {ocs::gdi::Target::RQS_LIST,     RQS_name,  ocs::tom::RQS::descriptor.e,  "resource quota set",      SGE_TYPE_RQS},
   {ocs::gdi::Target::HGRP_LIST,    HGRP_name, ocs::tom::HGRP::descriptor.e, "host group",              SGE_TYPE_HGROUP},
   {ocs::gdi::Target::AR_LIST,      AR_id,     ocs::tom::AR::descriptor.e,   "advance reservation",     SGE_TYPE_AR},
   {ocs::gdi::Target::DUMMY_LIST,   0,         nullptr,                      "general request",         SGE_TYPE_NONE},
   {ocs::gdi::Target::CAT_LIST,     CT_id,     nullptr,                      "category",                SGE_TYPE_CATEGORY},
   {ocs::gdi::Target::RL_LIST,      RL_name,   ocs::tom::RL::descriptor.e,   "role",                    SGE_TYPE_RL},
   {ocs::gdi::Target::PROCEDURE,    0,         nullptr,                      "procedure",               SGE_TYPE_CATEGORY},
   {ocs::gdi::Target::NO_TARGET,    0,         nullptr,                      nullptr,                   SGE_TYPE_NONE}
};

/* *INDENT-ON* */

/**
 * @brief The typed functions of one object type with a generic add and modify; undefined for the other types
 *
 * Each specialization derives from ocs_gdi_hooks_of and names the master list of the type, its modifier, its
 * writer and its on_success. The types are exactly those of #OcsGdiGenericType.
 *
 * @tparam T the object type
 */
template <OcsGdiGenericType T>
struct ocs_gdi_hooks;

/**
 * @brief The members of an ocs_gdi_hooks specialization
 *
 * @tparam L the master list of the type, an #sge_object_type value
 * @tparam M the modifier: validates the request and applies it to the new object; 0 on success
 * @tparam W the writer: spools the new object; 0 on success
 * @tparam S the on_success: announces the stored object
 */
template <sge_object_type L, auto M, auto W, auto S>
struct ocs_gdi_hooks_of {
   static constexpr sge_object_type list_type = L;   ///< the master list of the type
   static constexpr auto modifier = M;               ///< validates the request and applies it to the new object
   static constexpr auto writer = W;                 ///< spools the new object
   static constexpr auto on_success = S;             ///< announces the stored object
};

/* *INDENT-OFF* */
template <> struct ocs_gdi_hooks<ocs::tom::CAL>
   : ocs_gdi_hooks_of<SGE_TYPE_CALENDAR, calendar_mod, calendar_spool, calendar_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::EH>
   : ocs_gdi_hooks_of<SGE_TYPE_EXECHOST, host_mod, host_spool, host_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::CQ>
   : ocs_gdi_hooks_of<SGE_TYPE_CQUEUE, cqueue_mod, cqueue_spool, cqueue_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::CE>
   : ocs_gdi_hooks_of<SGE_TYPE_CENTRY, centry_mod, centry_spool, centry_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::PE>
   : ocs_gdi_hooks_of<SGE_TYPE_PE, pe_mod, pe_spool, pe_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::UU>
   : ocs_gdi_hooks_of<SGE_TYPE_USER, userprj_mod<ocs::tom::UU>, userprj_spool<ocs::tom::UU>,
                      userprj_success<ocs::tom::UU>> {};
template <> struct ocs_gdi_hooks<ocs::tom::US>
   : ocs_gdi_hooks_of<SGE_TYPE_USERSET, userset_mod, userset_spool, userset_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::PR>
   : ocs_gdi_hooks_of<SGE_TYPE_PROJECT, userprj_mod<ocs::tom::PR>, userprj_spool<ocs::tom::PR>,
                      userprj_success<ocs::tom::PR>> {};
template <> struct ocs_gdi_hooks<ocs::tom::CK>
   : ocs_gdi_hooks_of<SGE_TYPE_CKPT, ckpt_mod, ckpt_spool, ckpt_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::RQS>
   : ocs_gdi_hooks_of<SGE_TYPE_RQS, rqs_mod, rqs_spool, rqs_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::HGRP>
   : ocs_gdi_hooks_of<SGE_TYPE_HGROUP, hgroup_mod, hgroup_spool, hgroup_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::AR>
   : ocs_gdi_hooks_of<SGE_TYPE_AR, ar_mod, ar_spool, ar_success> {};
template <> struct ocs_gdi_hooks<ocs::tom::RL>
   : ocs_gdi_hooks_of<SGE_TYPE_RL, role_mod, role_spool, role_success> {};
/* *INDENT-ON* */

/** @brief Release every master list, at shutdown */
void sge_clean_lists() {
   int i = 0;

   for (; gdi_object[i].target != ocs::gdi::Target::NO_TARGET; i++) {
      if (gdi_object[i].list_type != SGE_TYPE_NONE) {
         // the slot holds a list of the type of the table entry, known only at run time; freed, the slot holds
         // nullptr afterwards
         ocs::DataStore::free_master_list(gdi_object[i].list_type);
      }
   }

}

/** @brief Handle the part of a request that the listener thread can do alone
 *
 * A read-only request that can be answered from the listener's own view never
 * reaches a worker at all, which is what keeps `qstat` cheap on a busy master.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param monitor for monitoring qmaster threads
 * @param has_next whether further tasks follow in the same packet
 * @return true when the request was fully answered here
 */
bool
sge_c_gdi_process_in_listener(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list, monitoring_t *monitor, bool has_next) {
   DENTER(TOP_LAYER);

   const char *target_name = nullptr;
   gdi_object_t *ao = get_gdi_object(task->target);
   if (ao != nullptr) {
      target_name = ao->object_name;
   }
   if (ao == nullptr || target_name == nullptr) {
      target_name = MSG_UNKNOWN_OBJECT;
   }

   const auto operation = task->command;
   const std::string cmd_string = to_string(task->command);
   const char *operation_name = cmd_string.c_str();

   DPRINTF("GDI %s %s (%s/%s) (%s/%d/%s/%d)\n", operation_name, target_name, packet->host, packet->commproc,
           packet->user, (int) packet->uid, packet->group, (int) packet->gid);

   sge_pack_buffer *pb = &(packet->pb);
   switch (operation) {
      case ocs::gdi::Command::TRIGGER:
         MONITOR_LIS_GDI_TRIG(monitor);
         sge_c_gdi_trigger_in_listener(packet, task, monitor);
         packet->pack_task(task, answer_list, pb, has_next);
         DRETURN(true);
      case ocs::gdi::Command::PERMCHECK:
         MONITOR_LIS_GDI_PERM(monitor);
         sge_c_gdi_permcheck(packet, task, monitor);
         packet->pack_task(task, answer_list, pb, has_next);
         DRETURN(true);
      case ocs::gdi::Command::GET:
         MONITOR_LIS_GDI_GET(monitor);
         sge_c_gdi_get_in_listener(ao, packet, task, monitor);
         packet->pack_task(task, answer_list, pb, has_next);
         DRETURN(true);
      default:
         // requests that are not handled in listener will be processed in a worker thread
         DRETURN(false);
   }

   DRETURN(false);
}

/** @brief May this client execute this request at all?
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @return true when the request is permitted
 *
 * @note MT-NOTE: sge_c_gdi_check_execution_permission() is NOT MT safe on its own.
 *
 *       CS-2633. The checks below resolve the reserved manager/operator usersets
 *       and the reserved @admin_hosts/@submit_hosts host groups in the data store
 *       of the calling thread. The mirror thread of that store removes and frees
 *       elements while merging events, so THE CALLER MUST HOLD THE READ LOCK of
 *       the store it is bound to - LOCK_LISTENER in the listener threads.
 *
 *       Do not move the lock in here. The helpers this dispatches to are also
 *       reached from sge_c_gdi_get_in_listener(), which already runs under that
 *       lock. LOCK_LISTENER is a FIFO reader/writer lock (sge_lock_fifo.cc) in
 *       which a reader queues as soon as a writer is waiting, so acquiring it
 *       recursively deadlocks the thread against the mirror.
 */
bool
sge_c_gdi_check_execution_permission(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
   DENTER(TOP_LAYER);

   switch (const ocs::gdi::Command cmd = task->command) {
      case ocs::gdi::Command::GET:
      case ocs::gdi::Command::GET_PROCEDURE:
         DRETURN(sge_task_check_get_perm_host(packet, task));
      case ocs::gdi::Command::ADD:
      case ocs::gdi::Command::MOD:
      case ocs::gdi::Command::COPY:
      case ocs::gdi::Command::DEL:
      case ocs::gdi::Command::MOD_PROCEDURE:
         /* MOD_PROCEDURE belongs here and not beside GET_PROCEDURE above: a
          * procedure that may change state has to pass the same two checks a
          * MOD does. It can do so because such a request carries the object
          * list it writes as its target, not Target::PROCEDURE - which owns no
          * permission of its own, so checking that would have refused every
          * write procedure through the fail-closed default of
          * sge_chck_mod_perm_user(). What the client claims there is not
          * believed: sge_c_gdi_procedure() refuses a request whose target or
          * command disagrees with the dispatch entry of the procedure it names,
          * so a caller cannot pick a weaker permission by naming a different
          * object than the procedure actually writes. */
         if (!sge_chck_mod_perm_user(packet, &(task->answer_list), task->target)) {
            DRETURN(false);
         }
         if (!sge_chck_mod_perm_host(packet, &(task->answer_list), task->target, cmd)) {
            DRETURN(false);
         }
         DRETURN(true);
      case ocs::gdi::Command::TRIGGER:
         // queue and job owners are allowed to trigger queues/jobs
         // this is done via qmod command on the cluster queue list
         //
         // the event client list is checked per client instead of here: the owner of an event
         // client may shut it down without being a manager, which is what a JAPI or DRMAA
         // session does when it ends. sge_gdi_shutdown_event_client() and the event master do
         // that check, and they keep the scheduler and "all clients at once" manager only.
         //
         // other trigger requests must have been initiated by a manager
         if (task->target != ocs::gdi::Target::CQ_LIST &&
             task->target != ocs::gdi::Target::EV_LIST) {
            if (!manop_is_manager(packet)) {
               ERROR(MSG_SGETEXT_MUSTBEMANAGERFOROP_SS, packet->user, to_string(cmd).c_str());
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         }
         if (!sge_chck_mod_perm_host(packet, &(task->answer_list), task->target, cmd)) {
            DRETURN(false);
         }
         DRETURN(true);
      case ocs::gdi::Command::PERMCHECK:
      default:
         // no checks required anyone can do that
         break;
   }
   DRETURN(true);
}

/* ------------------------------------------------------------ */
/** @brief Handle a request that needs a worker thread
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param monitor for monitoring qmaster threads
 * @param has_next whether further tasks follow in the same packet
 */
void
sge_c_gdi_process_in_worker(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list, monitoring_t *monitor, bool has_next) {
   DENTER(TOP_LAYER);

   const char *target_name = nullptr;
   gdi_object_t *ao = get_gdi_object(task->target);
   if (ao != nullptr) {
      target_name = ao->object_name;
   }
   if (ao == nullptr || target_name == nullptr) {
      target_name = MSG_UNKNOWN_OBJECT;
   }

   const ocs::gdi::Command command = task->command;
   const ocs::gdi::SubCommand sub_command = task->sub_command;
   const std::string cmd_string = to_string(task->command);
   const char *operation_name = cmd_string.c_str();

   DPRINTF("GDI %s %s (%s/%s) (%s/%d/%s/%d)\n", operation_name, target_name, packet->host, packet->commproc,
           packet->user, (int) packet->uid, packet->group, (int) packet->gid);

   sge_pack_buffer *pb = &(packet->pb);
   switch (command) {
      case ocs::gdi::Command::GET:
         MONITOR_GDI_GET(monitor);
         sge_c_gdi_get_in_worker(ao, packet, task, monitor);
         break;
      case ocs::gdi::Command::ADD:
         MONITOR_GDI_ADD(monitor);
         sge_c_gdi_add(packet, task, ao, command, sub_command, monitor);
         break;
      case ocs::gdi::Command::DEL:
         MONITOR_GDI_DEL(monitor);
         sge_c_gdi_del(packet, task, command, sub_command, monitor);
         break;
      case ocs::gdi::Command::MOD:
         MONITOR_GDI_MOD(monitor);
         sge_c_gdi_mod(ao, packet, task, command, sub_command, monitor);
         break;
      case ocs::gdi::Command::COPY:
         MONITOR_GDI_CP(monitor);
         sge_c_gdi_copy(ao, packet, task, sub_command, monitor);
         break;
      case ocs::gdi::Command::TRIGGER:
         MONITOR_GDI_TRIG(monitor);
         sge_c_gdi_trigger_in_worker(packet, task, monitor);
         break;
      case ocs::gdi::Command::PERMCHECK:
         MONITOR_GDI_PERM(monitor);
         sge_c_gdi_permcheck(packet, task, monitor);
         break;
      // The two procedure commands share a dispatch, which refuses a procedure
      // whose entry declares the other one, but they are counted apart: a read
      // is served from a reader thread, a modification serialises the daemon
      // under the global write lock.
      // @todo CS-1913 the remaining monitoring work for procedures is the
      //       per-procedure breakdown and timing; the two request counters are
      //       in place since CS-2822.
      case ocs::gdi::Command::GET_PROCEDURE:
         MONITOR_GDI_PROC_GET(monitor);
         sge_c_gdi_procedure(ao, packet, task, command, sub_command, monitor);
         break;
      case ocs::gdi::Command::MOD_PROCEDURE:
         MONITOR_GDI_PROC_MOD(monitor);
         sge_c_gdi_procedure(ao, packet, task, command, sub_command, monitor);
         break;
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_SGETEXT_UNKNOWNOP);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   }

   /*
    * CS-2753: bring @exec_hosts back in line with the execution host list, once
    * for the whole request.
    *
    * The rebuild walks the execution host list, drives the queue instances of
    * every referencing cluster queue and emits an event carrying the entire
    * membership. Doing that per element made adding or deleting N hosts cost
    * O(N^2): measured on a cluster of 1000 simulated hosts, one "qconf -de" took
    * 67 s, of which only 13 s was the round trips.
    *
    * Here, not after the element loops of sge_c_gdi_add()/_del()/_mod(): the
    * execution host list is also written outside EH_LIST requests -- by
    * host_list_add_missing_href() under an HGRP_LIST request, and by an execd
    * reporting in for the first time. The flag catches all of them.
    *
    * Safe at this point: the worker holds LOCK_GLOBAL for writing around the
    * whole task loop, the reader thread never dispatches anything but GET here,
    * and sge_commit() has not run yet, so the queue instances the rebuild spools
    * are committed with the rest of the request.
    */
   if (task->exec_hostgroup_out_of_sync) {
      task->exec_hostgroup_out_of_sync = false;
      host_sync_exec_hostgroup(packet, task, monitor, true);
   }

   // send the response
   packet->pack_task(task, answer_list, pb, has_next);

   DRETURN_VOID;
}

/** @brief Answer the GET requests a listener thread handles itself: event clients and active threads
 *
 * @param ao      the table entry of the target, unused
 * @param packet  the client request
 * @param task    the GDI task being answered; its data list receives the result
 * @param monitor for monitoring qmaster threads, unused
 */
static void
sge_c_gdi_get_in_listener(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // for get requests we do not need what the client might have sent to us
   task->reset_data_list();

   // check the permission
   if (!sge_task_check_get_perm_host(packet, task)) {
      DRETURN_VOID;
   }

   // handle he get request
   switch (task->target) {
      case ocs::gdi::Target::EV_LIST:
         task->reset_data_list(ocs::tom::RawListPtr(
               sge_select_event_clients("", task->condition.get(), task->enumeration.get())));
         task->do_select_pack_simultaneous = false;
         answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
         DRETURN_VOID;
      case ocs::gdi::Target::DUMMY_LIST:
         // the data list of a GDI task is untyped (libs/gdi)
         task->reset_data_list(ocs::tom::RawListPtr(ocs_get_active_thread_list()));
         task->do_select_pack_simultaneous = false;
         answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
         DRETURN_VOID;
      default:
         DRETURN_VOID;
   }
   DRETURN_VOID;
}

/**
 * @brief Answer the GET requests a worker thread handles: the configurations and every master list
 *
 * For an external client the selection is postponed: the task's data list points to the master list, and
 * pack_task() selects from it straight into the send buffer. An internal request gets a selected copy.
 *
 * @param ao      the table entry of the target, nullptr for a target without one
 * @param packet  the client request
 * @param task    the GDI task being answered; its data list receives the result
 * @param monitor for monitoring qmaster threads, unused
 */
static void
sge_c_gdi_get_in_worker(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   /* Whatever client sent with this get request - we don't need it */
   task->reset_data_list();

   switch (task->target) {
      case ocs::gdi::Target::CONF_LIST:
         task->reset_data_list(sge_get_configuration(task->condition.get(), task->enumeration.get()));
         task->do_select_pack_simultaneous = false;
         answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
         break;
      case ocs::gdi::Target::SC_LIST: /* TODO EB: move this into the scheduler configuration,
                                    and pack the list right away */ {
         const auto conf = sconf_get_config_list();
         // the old contract of lSelectHashPack(): the condition and the selection of the client are not checked
         // against the type; with a selection a misfit gives no list instead of an exception and the result is
         // named "", without one the list is copied under its own name and the condition is not applied
         task->reset_data_list(ocs::tom::select_as("", conf.get(), task->condition.get(), task->enumeration.get(),
                                                   ocs::tom::Hashing::Off));
         task->do_select_pack_simultaneous = false;
         answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
         break;
      }
      case ocs::gdi::Target::PROCEDURE:
         DPRINTF("Returning procedure information is not implemented yet\n");
         answer_list_add(&(task->answer_list), "Returning procedure information is not implemented yet", STATUS_OK, ANSWER_QUALITY_END);
         break;
      default:
         /*
          * Issue 1365
          * If the scheduler is not available the information in the job info
          * messages are outdated. In this case we have to reject the request.
          */
         if (task->target == ocs::gdi::Target::SME_LIST && !sge_has_event_client(EV_ID_SCHEDD)) {
            answer_list_add(&(task->answer_list), MSG_SGETEXT_JOBINFOMESSAGESOUTDATED, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         } else if (ao == nullptr || ao->list_type == SGE_TYPE_NONE) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         } else {
            // the type of the master list is given at run time by the target
            const ocs::tom::RawList data_source = ocs::DataStore::master_list_raw_rw(ao->list_type);

            DPRINTF("Got list with %zu elements\n", data_source.size());

            if (packet->is_intern_request) {
               /* intern requests need no pb so it is not necessary to postpone the operation */
               // the old contract of lSelectHashPack(), as for SC_LIST above
               task->reset_data_list(ocs::tom::select_as("", data_source.raw(), task->condition.get(),
                                                         task->enumeration.get(), ocs::tom::Hashing::Off));
               task->do_select_pack_simultaneous = false;

               /*
                * DIRTY HACK: The "ok" message should be removed from the answer list
                * 05/21/2007 quality was ANSWER_QUALITY_INFO but this results in "ok"
                * messages on qconf side
                */
               answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);

            } else {
               /* lSelect will be postponed till packing */
               task->data_list = data_source.raw();
               task->do_select_pack_simultaneous = true;

               /*
                * answer list creation is also done during packing!!!!
                */
            }
         }
   }
   DRETURN_VOID;
}

/**
 * @brief Handles the ADD command of a GDI request.
 *
 * Dispatches on the request target. Event clients, jobs and the scheduler
 * configuration each have their own registration path; every other target is
 * handled by the generic object add below.
 *
 * MT-NOTE: sge_c_gdi_add() is MT safe
 *
 * @param[in]     packet       GDI packet the task belongs to; carries the
 *                             authentication and address infos of the sender
 * @param[in,out] task         GDI task; its data_list holds the objects to add
 *                             and receives the answers
 * @param[in]     ao           descriptor of the object type to add, used by the
 *                             generic path
 * @param[in]     cmd          the GDI command, passed on to the generic path
 * @param[in]     sub_command  GDI sub command, e.g. RETURN_NEW_VERSION
 * @param[in]     monitor      monitoring structure of the calling thread
 *
 * @return void
 */
static void
sge_c_gdi_add(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
              gdi_object_t *ao, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::RTIC> ticket_orders;
   bool reprioritize_tickets = sconf_get_reprioritize_interval() > 0;

   if (task->target == ocs::gdi::Target::EV_LIST) {
      bool return_new_version =
              (sub_command & ocs::gdi::SubCommand::RETURN_NEW_VERSION) == ocs::gdi::SubCommand::RETURN_NEW_VERSION;

      /*
       * The registered event clients are returned in task->data_list, the very
       * list the requests arrive in. Take the requests aside first: otherwise
       * sge_add_event_client() assigns the new EV_id to the request element and
       * then appends the registered client with that same id to the list it is
       * a member of - two elements under a key that EV_id declares unique, and
       * the walk below would run into the appended elements as well.
       */
      // the request data is structure checked by the listener (CS-3125): an EV_LIST add carries EV; adopt()
      // checks before it takes the list over, so on a throw raw_requests frees it
      ocs::tom::RawListPtr raw_requests = task->take_data_list();
      auto requests = ocs::tom::adopt<ocs::tom::EV>(std::move(raw_requests));

      /* is thread save. the global lock is used when needed */
      for (const auto ep : *requests) {
         /* fill address infos from request into event client that must be added */
         ep.set<EV_host>(ocs::tom::Host(packet->host));
         ep.set<EV_commproc>(packet->commproc);
         ep.set<EV_commid>(packet->commproc_id);

         /* fill in authentication infos from request */
         ep.set<EV_uid>(packet->uid);
         if (!event_client_verify(ep, &(task->answer_list), true)) {
            ERROR(MSG_QMASTER_INVALIDEVENTCLIENT_SSS, packet->user, packet->commproc, packet->host);
         } else {
            sge_add_event_client(packet, ep, &(task->answer_list),
                                 return_new_version ? &(task->data_list) : nullptr,
                                 (event_client_update_func_t) nullptr, nullptr);
         }
      }

      if (!return_new_version) {
         /* nothing was returned, so the requests go back as they always did; otherwise requests frees them */
         task->reset_data_list(ocs::tom::RawListPtr(std::move(requests)));
      }
   } else if (task->target == ocs::gdi::Target::JB_LIST) {
      const bool return_new_version =
              (sub_command & ocs::gdi::SubCommand::RETURN_NEW_VERSION) == ocs::gdi::SubCommand::RETURN_NEW_VERSION;
      /* With RETURN_NEW_VERSION sge_gdi_add_job() appends a copy of every added job to task->data_list, which
       * goes back to the client. The requests are taken out of task->data_list before the loop, so that the
       * copies are neither walked by the loop as further submissions nor mixed with the requests (CS-3161);
       * after it the requests not taken over go back in front of the copies, the others are freed. */
      // the request data is structure checked by the listener (CS-3125): a JB_LIST add carries JB
      ocs::tom::RawListPtr raw_requests = return_new_version ? task->take_data_list() : nullptr;
      auto requests = ocs::tom::adopt<ocs::tom::JB>(std::move(raw_requests));
      const auto jobs = return_new_version ? requests.get() : ocs::tom::List<ocs::tom::JB>(task->data_list);

      auto ep = jobs.front();
      while (ep) { /* is thread save. the global lock is used, when needed */
         const auto next = ep.next();

         // sge_gdi_add_job() takes the job over and may free it
         auto job = jobs.extract(ep);

         /* fill address infos from request into event client that must be added */
         if (!job_verify_submitted_job(*job, &(task->answer_list))) {
            ERROR(MSG_QMASTER_INVALIDJOBSUBMISSION_SSS, packet->user, packet->commproc, packet->host);
         } else {
            /* submit needs to know user and group */
            sge_gdi_add_job(job, &(task->answer_list), return_new_version ? &(task->data_list) : nullptr,
                            packet, task, monitor);
         }
         // a job freed by sge_gdi_add_job() is not put back, as lInsertElem() of a null element did nothing
         if (job) {
            jobs.push_front(std::move(job));
         }
         ep = next;
      }

      /* The requests sge_gdi_add_job() did not take over - a job only verified (qsub -w v|p) or a rejected one -
       * go back to the client in front of the copies of the added jobs, as before CS-3161: qsub -w v reads
       * its job from the response. */
      if (return_new_version && requests && !requests->empty()) {
         if (task->data_list != nullptr) {
            // sge_gdi_add_job() appended copies of the added jobs (JB)
            requests->splice(ocs::tom::ListPtr<ocs::tom::JB>::from_typed(task->take_data_list().release()));
         }
         task->reset_data_list(ocs::tom::RawListPtr(std::move(requests)));
      }
   } else if (task->target == ocs::gdi::Target::SC_LIST) {
      // the request data is structure checked by the listener (CS-3125): an SC_LIST add carries SC
      auto ep = ocs::tom::List<ocs::tom::SC>(task->data_list).front();
      while (ep) {
         const auto next = ep.next();

         sge_mod_sched_configuration(packet, task, ep, &(task->answer_list), packet->user, packet->host);
         ep = next;
      }
   } else {
      // the element type of the data list is given at run time by the target: orders, share tree nodes and
      // execution hosts are walked typed, every other target dispatches once on its object type
      bool is_scheduler_resync = false;

      /* With RETURN_NEW_VERSION the requests are not returned: the client gets what ar_success() appends to
       * task->data_list instead, nullptr when nothing was appended. The requests are therefore taken out of
       * task->data_list before the loop, so that the appended objects are neither visited by the loop nor
       * mixed with the requests, and freed after it. */
      const bool return_new_version =
              (sub_command & ocs::gdi::SubCommand::RETURN_NEW_VERSION) == ocs::gdi::SubCommand::RETURN_NEW_VERSION;
      // owns the requests when they are taken out of the task, see above; freed when the add is done
      ocs::tom::RawListPtr taken_requests = return_new_version ? task->take_data_list() : nullptr;
      const ocs::tom::RawList requests =
            return_new_version ? taken_requests.get() : ocs::tom::RawList(task->data_list);

      if (task->target == ocs::gdi::Target::ORDER_LIST) {
         // the request data is structure checked by the listener (CS-3125): an ORDER_LIST add carries OR
         auto next = ocs::tom::List<ocs::tom::OR>(requests).front();
         while (next) {
            const auto ep = next;
            next = ep.next();

            switch (sge_follow_order(ep, packet->user, packet->host,
                                     reprioritize_tickets ? &ticket_orders : nullptr, monitor, packet->gdi_session)) {
               case STATUS_OK :
               case 0 : /* everything went fine */
                  break;
               case -2 :
                  is_scheduler_resync = true;
               case -1 :
               case -3 :
                  /* stop the order processing */
                  // the count of the elements after one has no typed form; the raw one counts them
                  WARNING("Skipping remaining " sge_u32 " orders", static_cast<uint32_t>(ocs::tom::remaining(ep)));
                  next = nullptr;
                  break;

               default :
               DPRINTF("--> FAILED: unexpected state from in the order processing <--\n");
                  break;
            }
         }

         set_next_stree_spooling_time();
         answer_list_add(&(task->answer_list), "OK\n", STATUS_OK, ANSWER_QUALITY_INFO);
      } else if (task->target == ocs::gdi::Target::STN_LIST) {
         // the request data is structure checked by the listener (CS-3125): an STN_LIST add carries STN
         auto ep = ocs::tom::List<ocs::tom::STN>::from_typed(requests).front();
         while (ep) {
            const auto next = ep.next();
            sge_add_sharetree(packet, task, ep, &(task->answer_list), packet->user, packet->host);
            ep = next;
         }
      } else if (ao == nullptr) {
         // a target without a table entry has no element type; every request is refused
         for (auto n = requests.size(); n > 0; n--) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         }
      } else if (task->target == ocs::gdi::Target::EH_LIST && !strcmp(to_cstr(EXECD), packet->commproc)) {
         const bool is_restart = sub_command == ocs::gdi::SubCommand::EXECD_RESTART;

         // the request data is structure checked by the listener (CS-3125): an EH_LIST add carries EH
         auto ep = ocs::tom::List<ocs::tom::EH>::from_typed(requests).front();
         while (ep) {
            const auto next = ep.next();
            sge_execd_startedup(packet, task, ep, &(task->answer_list), packet->user, packet->host, task->target,
                                monitor, is_restart);
            ep = next;
         }
      } else {
         // no on_success hook leaves anything for after an add
         ocs_gdi_add_mod_list(packet, task, requests, 1, ao, cmd, sub_command, nullptr, monitor);
      }

      if (is_scheduler_resync) {
         sge_resync_schedd(monitor, packet->gdi_session); /* ask for a total update */
      }

      // task->data_list holds the added ARs appended by ar_success(), see above; taken_requests frees the requests
   }

   if (reprioritize_tickets && ticket_orders) {
      distribute_ticket_orders(ticket_orders, monitor);
      ticket_orders.reset();
   }

   DRETURN_VOID;
}

/** @brief Refuse the delete of one object of a list that cannot be deleted from, see the header
 *
 * @param alpp receives the refusal
 */
void
ocs_gdi_del_unsupported(ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(GDI_LAYER);
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPNOIMPFORTARGET_S, "sge_c_gdi_del");
   answer_list_add(alpp, SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   DRETURN_VOID;
}

/** @brief Delete a whole object list the way a GDI delete without data does, see the header
 *
 * @param packet the client request
 * @param task   the GDI task being answered
 * @param target the object list; only the share tree can be deleted as a whole
 * @param alpp   receives what the delete has to say
 */
void
ocs_gdi_del_list(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::gdi::Target target,
                 ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(GDI_LAYER);
   if (target == ocs::gdi::Target::STN_LIST) {
      sge_del_sharetree(packet, task, alpp, packet->user, packet->host);
   } else {
      ocs_gdi_del_unsupported(alpp);
   }
   DRETURN_VOID;
}

/** @brief Delete one object the way a GDI delete does, see the header
 *
 * The per-object half of sge_c_gdi_del(), split out so that the qconf delete
 * procedure (CS-2983) deletes through exactly the functions, the checks and the
 * messages a GDI delete uses.
 *
 * @tparam T      the object type
 * @param packet  the client request
 * @param task    the GDI task being answered
 * @param ep      the object to delete, carrying its name
 * @param alpp    receives what the delete has to say
 * @param monitor for monitoring qmaster threads; only the delete of an execution host uses it
 */
template <OcsGdiDeletableType T>
void
ocs_gdi_del_object(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<T> ep,
                   ocs::tom::ListPtr<ocs::tom::AN> *alpp, [[maybe_unused]] monitoring_t *monitor) {
   DENTER(GDI_LAYER);
   if constexpr (std::same_as<T, ocs::tom::EH>) {
      sge_del_host(packet, task, ep, alpp, packet->user, packet->host, ocs::gdi::Target::EH_LIST,
                   ocs::DataStore::master_list<SGE_TYPE_HGROUP>(), monitor);
   } else if constexpr (std::same_as<T, ocs::tom::CQ>) {
      cqueue_del(packet, task, ep, alpp, packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::CE>) {
      sge_del_centry(packet, task, ep, alpp, packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::PE>) {
      sge_del_pe(packet, task, ep, alpp, packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::CONF>) {
      sge_del_configuration(packet, task, ep, alpp, packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::UU>) {
      sge_del_userprj(packet, task, ep, alpp, ocs::DataStore::master_list_rw<SGE_TYPE_USER>(), packet->user,
                      packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::US>) {
      sge_del_userset(packet, task, ep, alpp, ocs::DataStore::master_list_rw<SGE_TYPE_USERSET>(), packet->user,
                      packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::PR>) {
      sge_del_userprj(packet, task, ep, alpp, ocs::DataStore::master_list_rw<SGE_TYPE_PROJECT>(), packet->user,
                      packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::RQS>) {
      rqs_del(packet, task, ep, alpp, ocs::DataStore::master_list_rw<SGE_TYPE_RQS>(), packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::CK>) {
      sge_del_ckpt(packet, task, ep, alpp, packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::CAL>) {
      sge_del_calendar(packet, task, ep, alpp, packet->user, packet->host);
   } else if constexpr (std::same_as<T, ocs::tom::HGRP>) {
      hgroup_del(packet, task, ep, alpp, packet->user, packet->host);
   } else {
      static_assert(std::same_as<T, ocs::tom::RL>, "every type of OcsGdiDeletableType has a delete");
      sge_del_role(packet, task, ep, alpp, packet->user, packet->host);
   }
   DRETURN_VOID;
}

/* *INDENT-OFF* */
template void ocs_gdi_del_object<ocs::tom::EH>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::EH>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::CQ>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::CQ>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::CE>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::CE>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::PE>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::PE>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::CONF>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                                 ocs::tom::ConstElement<ocs::tom::CONF>,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::UU>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::UU>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::US>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::US>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::PR>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::PR>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::RQS>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                                ocs::tom::ConstElement<ocs::tom::RQS>,
                                                ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::CK>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::CK>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::CAL>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                                ocs::tom::ConstElement<ocs::tom::CAL>,
                                                ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::HGRP>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                                 ocs::tom::ConstElement<ocs::tom::HGRP>,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
template void ocs_gdi_del_object<ocs::tom::RL>(ocs::gdi::Packet *, ocs::gdi::Task *,
                                               ocs::tom::ConstElement<ocs::tom::RL>,
                                               ocs::tom::ListPtr<ocs::tom::AN> *, monitoring_t *);
/* *INDENT-ON* */

/** @brief Delete every object of a GDI delete request of one object type, front to back
 *
 * @tparam T      the object type of the target
 * @param packet  the client request
 * @param task    the GDI task being answered; its data list holds elements of type T (CS-3125)
 * @param monitor for monitoring qmaster threads
 */
template <OcsGdiDeletableType T>
static void
ocs_gdi_del_each(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(GDI_LAYER);
   // the request data is structure checked by the listener (CS-3125): a delete carries elements of the target type
   for (const auto ep : ocs::tom::ConstList<T>(task->data_list)) {
      ocs_gdi_del_object<T>(packet, task, ep, &(task->answer_list), monitor);
   }
   DRETURN_VOID;
}

/** @brief Carry out a GDI delete request, one object of its data list after the other
 *
 * The element type of the data list is fixed per target by the listener (CS-3125): ID elements for jobs and
 * advance reservations, elements of the target type otherwise. The target is dispatched once, the data list is
 * walked typed.
 *
 * MT safe.
 *
 * @param packet      the client request
 * @param task        the GDI task being answered; an empty data list deletes the whole list
 * @param cmd         the command of the request
 * @param sub_command what kind of delete this is
 * @param monitor     for monitoring qmaster threads
 */
void
sge_c_gdi_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::gdi::Command cmd,
              const ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(GDI_LAYER);

   if (task->data_list == nullptr) {
      /* delete whole list */
      ocs_gdi_del_list(packet, task, task->target, &(task->answer_list));
      DRETURN_VOID;
   }

   switch (task->target) {
      case ocs::gdi::Target::JB_LIST:
         // the request data is structure checked by the listener (CS-3125): a job delete carries ID elements
         for (const auto ep : ocs::tom::List<ocs::tom::ID>(task->data_list)) {
            sge_gdi_del_job(packet, task, ep, &(task->answer_list), cmd, sub_command, monitor);
         }
         break;
      case ocs::gdi::Target::AR_LIST:
         // the request data is structure checked by the listener (CS-3125): an AR delete carries ID elements
         for (const auto ep : ocs::tom::ConstList<ocs::tom::ID>(task->data_list)) {
            ar_del(packet, task, ep, &(task->answer_list), ocs::DataStore::master_list_rw<SGE_TYPE_AR>(), monitor);
         }
         break;
      case ocs::gdi::Target::EH_LIST:
         ocs_gdi_del_each<ocs::tom::EH>(packet, task, monitor);
         break;
      case ocs::gdi::Target::CQ_LIST:
         ocs_gdi_del_each<ocs::tom::CQ>(packet, task, monitor);
         break;
      case ocs::gdi::Target::CE_LIST:
         ocs_gdi_del_each<ocs::tom::CE>(packet, task, monitor);
         break;
      case ocs::gdi::Target::PE_LIST:
         ocs_gdi_del_each<ocs::tom::PE>(packet, task, monitor);
         break;
      case ocs::gdi::Target::CONF_LIST:
         ocs_gdi_del_each<ocs::tom::CONF>(packet, task, monitor);
         break;
      case ocs::gdi::Target::UU_LIST:
         ocs_gdi_del_each<ocs::tom::UU>(packet, task, monitor);
         break;
      case ocs::gdi::Target::US_LIST:
         ocs_gdi_del_each<ocs::tom::US>(packet, task, monitor);
         break;
      case ocs::gdi::Target::PR_LIST:
         ocs_gdi_del_each<ocs::tom::PR>(packet, task, monitor);
         break;
      case ocs::gdi::Target::RQS_LIST:
         ocs_gdi_del_each<ocs::tom::RQS>(packet, task, monitor);
         break;
      case ocs::gdi::Target::CK_LIST:
         ocs_gdi_del_each<ocs::tom::CK>(packet, task, monitor);
         break;
      case ocs::gdi::Target::CAL_LIST:
         ocs_gdi_del_each<ocs::tom::CAL>(packet, task, monitor);
         break;
      case ocs::gdi::Target::HGRP_LIST:
         ocs_gdi_del_each<ocs::tom::HGRP>(packet, task, monitor);
         break;
      case ocs::gdi::Target::RL_LIST:
         ocs_gdi_del_each<ocs::tom::RL>(packet, task, monitor);
         break;
      default:
         // the element type of a target without a delete is not fixed; every element is refused
         for (auto n = ocs::tom::ConstRawList(task->data_list).size(); n > 0; n--) {
            ocs_gdi_del_unsupported(&(task->answer_list));
         }
         break;
   }

   DRETURN_VOID;
}

/**
 * @brief Carry out a GDI copy request; only jobs can be copied
 *
 * The target is dispatched once: the data list of a job copy is walked typed, every element of another target is
 * refused.
 *
 * @param ao          the table entry of the target, unused
 * @param packet      the client request
 * @param task        the GDI task being answered; its data list holds the objects to copy
 * @param sub_command GDI sub command; RETURN_NEW_VERSION is not honoured for COPY
 * @param monitor     for monitoring qmaster threads
 *
 * @note MT-NOTE: sge_c_gdi_copy() is MT safe
 */
static void sge_c_gdi_copy([[maybe_unused]] gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                           [[maybe_unused]] const ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   if (task->target == ocs::gdi::Target::JB_LIST) {
      // the request data is structure checked by the listener (CS-3125): a JB_LIST copy carries JB
      for (const auto ep : ocs::tom::List<ocs::tom::JB>(task->data_list)) {
         /* gdi_copy_job uses the global lock internal */
         // RETURN_NEW_VERSION is not honoured for COPY: the old code handed the answer list in as the job return
         // list, which put a JB element into the AN answer list; no client sends it with COPY
         sge_gdi_copy_job(ep, &(task->answer_list), nullptr, packet, task, monitor);
      }
   } else {
      // the element type of another target is not fixed for COPY; every element is refused
      for (auto n = ocs::tom::ConstRawList(task->data_list).size(); n > 0; n--) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
      }
   }
   DRETURN_VOID;
}

/* ------------------------------------------------------------ */

/**
 * @brief Answer a PERMCHECK request: which roles the calling user and host have
 *
 * Whatever the client sent is replaced by one PERM element; its condition and selection are ignored.
 *
 * @param packet the client request, which names user and host
 * @param task   the GDI task being answered; its data list receives the result
 */
static void
sge_gdi_do_permcheck(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
   DENTER(TOP_LAYER);

   const char *hostname = packet->host;
   const char *username = packet->user;

   // create list with element and fill in username, hostname and corresponding permission
   auto perm_list = ocs::tom::ListPtr<ocs::tom::PERM>::create("");
   const auto ep = perm_list->emplace_back<PERM_username>(username);
   // whatever client sent - we do not need that, instead we use data from commlib stored in packet
   task->reset_data_list(ocs::tom::RawListPtr(std::move(perm_list)));
   ep.set<PERM_host>(ocs::tom::Host(hostname));

   // add user roles
   bool is_manager = manop_is_manager(packet);
   bool is_operator = manop_is_operator(packet);
   ep.set<PERM_is_manager>(is_manager);
   ep.set<PERM_is_operator>(is_operator);

   // add host roles -- through the same helpers as the checks above, so that
   // what permcheck REPORTS and what the permission checks ENFORCE cannot drift
   // apart. This site used a direct lGetElemHost() on the retired admin/submit
   // host lists, which host_list_locate() resolved to for those list types.
   bool is_admin_host = host_is_admin_host(hostname);
   bool is_submit_host = host_is_submit_host(hostname);
   ep.set<PERM_is_admin_host>(is_admin_host);
   ep.set<PERM_is_submit_host>(is_submit_host);

   // show the results
   DPRINTF("manager/operator/admin_host/submit_host: %s/%s/%s/%s\n",
           is_manager ? "true" : "false",
           is_operator ? "true" : "false",
           is_admin_host ? "true" : "false",
           is_submit_host ? "true" : "false");

   // client might have sent ocs::tom::WhereData and ocs::tom::WhatData (where/what) but we ignore it for this kind of
   // request

   // report success
   answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
   DRETURN_VOID;
}

static void
sge_c_gdi_permcheck(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   switch (task->target) {
      case ocs::gdi::Target::DUMMY_LIST:
         sge_gdi_do_permcheck(packet, task);
         break;
      default:
         WARNING(MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   }
   DRETURN_VOID;
}

static void
sge_c_gdi_trigger_in_listener(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(GDI_LAYER);
   ocs::gdi::Target target = task->target;
   switch (target) {
      case ocs::gdi::Target::MASTER_EVENT:
         // shutdown request for qmaster (qconf -km)
         if (!host_is_admin_host(packet->host)) {
            ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN_VOID;
         }
         sge_gdi_kill_master(packet, task);
         DRETURN_VOID;
      case ocs::gdi::Target::SC_LIST:
         // trigger scheduling (qconf -tsm)
         if (!host_is_admin_host(packet->host)) {
            ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
            DRETURN_VOID;
         }
         trigger_scheduler_monitoring(packet, task, monitor);
         DRETURN_VOID;
      case ocs::gdi::Target::EV_LIST:
         // shutdown of event client (qconf -kec)
         sge_gdi_shutdown_event_client(packet, task, monitor);
         answer_list_log(&(task->answer_list), false, true);
         DRETURN_VOID;
      case ocs::gdi::Target::DUMMY_LIST:
         // start stop of scheduler (qconf -ks | -kt scheduler | -at scheduler)
         sge_gdi_tigger_thread_state_transition(packet, task, monitor);
         answer_list_log(&(task->answer_list), false, true);
         DRETURN_VOID;
      default:
         // unknown operation for a listener thread
         WARNING(MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
   }
   DRETURN_VOID;
}

static void
sge_c_gdi_trigger_in_worker(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(GDI_LAYER);
   switch (task->target) {
      case ocs::gdi::Target::EH_LIST:
         // shutdown of execd (qconf -ke)
         sge_gdi_kill_exechost(packet, task);
         DRETURN_VOID;
      case ocs::gdi::Target::CQ_LIST:
      case ocs::gdi::Target::JB_LIST:
         // state changes of queues and jobs
         sge_gdi_qmod(packet, task, monitor);
         DRETURN_VOID;
      default:
         // unknown operation for a worker thread
         WARNING(MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
   }
   DRETURN_VOID;
}

/**
 * @brief Start or stop a qmaster thread on request; only the scheduler thread supports it
 *
 * @param packet  the client request
 * @param task    the GDI task being answered; its data list holds ID elements naming the thread
 *                and the action
 * @param monitor for monitoring qmaster threads
 */
static void
sge_gdi_tigger_thread_state_transition(ocs::gdi::Packet *packet,
                                       ocs::gdi::Task *task,
                                       monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> *answer_list = &(task->answer_list);
   // every element is read as ID below, as the free access on the raw element did
   for (const auto elem : ocs::tom::List<ocs::tom::ID>(task->data_list)) {
      const char *name = elem.get<ID_str>();
      sge_thread_state_transitions_t action = (sge_thread_state_transitions_t) elem.get<ID_action>();

      if (name != nullptr) {
         if (strcasecmp(name, to_cstr(SCHEDD_THREAD)) == 0) {
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

/**
 * @brief Shutdown event client
 *
 * Shutdown event clients by client id. tasks data_list does contain a list of
 * client id's. This is a list of 'ID' elements.
 *
 * @param packet the client request
 * @param task the GDI task being answered; its data list holds the ID elements
 * @param monitor the monitoring structure
 *
 * @note MT-NOTE: ocs::gdi::Client::sge_gdi_shutdown_event_client() is NOT MT safe.
 */
static void
sge_gdi_shutdown_event_client(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // every element is read as ID by get_client_id(), as the free access on the raw element did
   for (const auto elem : ocs::tom::List<ocs::tom::ID>(task->data_list)) {
      ocs::tom::ListPtr<ocs::tom::AN> local_alp;
      int client_id = EV_ID_ANY;
      int res = -1;

      if (get_client_id(elem, &client_id) != 0) {
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         continue;
      }

      if (client_id == EV_ID_SCHEDD && !host_is_admin_host(packet->host)) {
         ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
         continue;
      } else if (!host_is_configured_submit_host(packet->host) && !host_is_admin_host(packet->host)) {
         // Deliberately not host_is_submit_host(): ALLOW_ANY_SUBMITHOSTS grants what a
         // submit host may do with its own work, and killing another client's event
         // client is not that. This check stays with the configured submit hosts.
         ERROR(MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
         continue;
      }

      /* thread shutdown */
      if (client_id == EV_ID_SCHEDD) {
         // the permission check for TRIGGER requests lets the event client list through so that
         // the owner of a dynamic event client can shut it down. The scheduler is not owned by
         // the requester, so qconf -ks stays a manager operation and is checked here - the
         // event master does the same check for a single client and for all of them at once.
         if (!manop_is_manager(packet)) {
            answer_list_add(&(task->answer_list), MSG_COM_NOSHUTDOWNPERMS, STATUS_DENIED,
                            ANSWER_QUALITY_ERROR);
            continue;
         }
         sge_scheduler_terminate(nullptr);
      }

      if (client_id == EV_ID_ANY) {
         res = sge_shutdown_dynamic_event_clients(packet, &local_alp, monitor);
      } else {
         res = sge_shutdown_event_client(packet, client_id, &local_alp);
      }

      if ((res == EINVAL) && (client_id == EV_ID_SCHEDD)) {
         local_alp.reset();
         answer_list_add(&(task->answer_list), MSG_COM_NOSCHEDDREGMASTER, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      } else {
         answer_list_append_list(&(task->answer_list), &local_alp);
      }
   }

   DRETURN_VOID;
} /* ocs::gdi::Client::sge_gdi_shutdown_event_client() */

/**
 * @brief Get client id from ID element
 *
 * Get client id from ID element. The client id is converted to an
 * integer and stored in 'anID'.
 *
 * @param anElem ID element
 * @param anID will contain client id on return
 *
 * @return EINVAL if the client id could not be extracted, 0 otherwise
 *
 * @note MT-NOTE: get_client_id() is MT safe.
 *
 *       Using 'errno' to check for 'strtol' error situations is recommended
 *       by POSIX.
 */
static int get_client_id(const ocs::tom::ConstElement<ocs::tom::ID> anElem, int *anID) {
   DENTER(TOP_LAYER);

   const char *id = nullptr;

   if ((id = anElem.get<ID_str>()) == nullptr) {
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

/**
 * @brief Trigger scheduler monitoring
 *
 * Trigger scheduler monitoring.
 *
 * @note MT-NOTE: trigger_scheduler_monitoring() is MT safe, using global lock
 */
static void
trigger_scheduler_monitoring(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                             monitoring_t *monitor) {
   DENTER(GDI_LAYER);

   if (!manop_is_manager(packet)) {
      WARNING(SFNMAX, MSG_COM_NOSCHEDMONPERMS);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_WARNING);
      DRETURN_VOID;
   }
   if (!sge_add_event_for_client(EV_ID_SCHEDD, 0, sgeE_SCHEDDMONITOR, 0, 0, nullptr, nullptr, nullptr, nullptr, packet->gdi_session)) {
      WARNING(SFNMAX, MSG_COM_NOSCHEDDREGMASTER);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      DRETURN_VOID;
   }

   INFO(MSG_COM_SCHEDMON_SS, packet->user, packet->host);
   answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN_VOID;
} /* trigger_scheduler_monitoring() */

/**
 * @brief Carry out a GDI modify request, one object of its data list after the other
 *
 * The element type of the data list is given at run time by the target. The targets with a modify of their
 * own (configuration, event clients, scheduler configuration, jobs, share tree) are dispatched once and walked
 * typed; every other target with a table entry dispatches once on its object type and is walked typed
 * (ocs_gdi_add_mod_list()). What the on_success hooks leave for after the last object is carried out once,
 * after the loop.
 *
 * @param ao          the table entry of the target, nullptr if it has no generic handling
 * @param packet      the client request
 * @param task        the GDI task being answered; its data list holds the modification instructions
 * @param command     the GDI command
 * @param sub_command GDI sub command, e.g. RETURN_NEW_VERSION
 * @param monitor     for monitoring qmaster threads
 *
 * @note MT-NOTE: sge_c_gdi_mod() is MT safe
 */
static void sge_c_gdi_mod(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                          ocs::gdi::Command command, ocs::gdi::SubCommand sub_command,
                          monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   ocs_gdi_post_processing post;

   const ocs::gdi::Target target = task->target;
   const bool own_mod = target == ocs::gdi::Target::CONF_LIST || target == ocs::gdi::Target::EV_LIST ||
                        target == ocs::gdi::Target::SC_LIST || target == ocs::gdi::Target::JB_LIST ||
                        target == ocs::gdi::Target::STN_LIST;
   if (!own_mod && ao != nullptr) {
      // every other target with a table entry dispatches once on its object type
      ocs_gdi_add_mod_list(packet, task, task->data_list, 0, ao, command, sub_command, &post, monitor);
   } else {
      // the request data is structure checked by the listener (CS-3125): a modify carries the type of the target
      switch (target) {
         case ocs::gdi::Target::CONF_LIST:
            for (const auto ep : ocs::tom::List<ocs::tom::CONF>(task->data_list)) {
               sge_mod_configuration(ep, &(task->answer_list), packet->user, packet->host, packet->gdi_session);
            }
            break;
         case ocs::gdi::Target::EV_LIST:
            for (const auto ev : ocs::tom::List<ocs::tom::EV>(task->data_list)) {
               /* fill address infos from request into event client that must be added */
               ev.set<EV_host>(ocs::tom::Host(packet->host));
               ev.set<EV_commproc>(packet->commproc);
               ev.set<EV_commid>(packet->commproc_id);

               /* fill in authentication infos from request */
               ev.set<EV_uid>(packet->uid);
               if (!event_client_verify(ev, &(task->answer_list), false)) {
                  ERROR(MSG_QMASTER_INVALIDEVENTCLIENT_SSS, packet->user, packet->commproc, packet->host);
               } else {
                  sge_mod_event_client(ev, &(task->answer_list), packet->user, packet->host);
               }
            }
            break;
         case ocs::gdi::Target::SC_LIST:
            for (const auto ep : ocs::tom::List<ocs::tom::SC>(task->data_list)) {
               sge_mod_sched_configuration(packet, task, ep, &(task->answer_list), packet->user, packet->host);
            }
            break;
         case ocs::gdi::Target::JB_LIST:
            for (const auto ep : ocs::tom::List<ocs::tom::JB>(task->data_list)) {
               sge_gdi_mod_job(packet, task, ep, &(task->answer_list), sub_command);
            }
            break;
         case ocs::gdi::Target::STN_LIST:
            for (const auto ep : ocs::tom::List<ocs::tom::STN>(task->data_list)) {
               sge_mod_sharetree(packet, task, ep, &(task->answer_list), packet->user, packet->host);
            }
            break;
         default:
            // a target without a table entry has no element type; every element is refused
            for (auto n = ocs::tom::ConstRawList(task->data_list).size(); n > 0; n--) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
            }
            break;
      }
   }

   /* postprocessing for the list of requests: centry_success() collects the names of the changed complex
    * entries, their debitation is rebuilt once for the whole request */
   if (!post.redebit_centries->empty()) {
      DPRINTF("rebuilding consumable debitation\n");
      centry_redebit_consumables(post.redebit_centries.get(), packet->gdi_session);
   }

   DRETURN_VOID;
}

bool
sge_chck_mod_perm_user(const ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::gdi::Target target) {
   DENTER(TOP_LAYER);


   /* check permissions of user */
   switch (const auto tar = target) {
      case ocs::gdi::Target::ORDER_LIST:
      case ocs::gdi::Target::EH_LIST:
      case ocs::gdi::Target::CQ_LIST:
      case ocs::gdi::Target::CE_LIST:
      case ocs::gdi::Target::PE_LIST:
      case ocs::gdi::Target::CONF_LIST:
      case ocs::gdi::Target::SC_LIST:
      case ocs::gdi::Target::UU_LIST:
      case ocs::gdi::Target::PR_LIST:
      case ocs::gdi::Target::STN_LIST:
      case ocs::gdi::Target::CK_LIST:
      case ocs::gdi::Target::CAL_LIST:
      case ocs::gdi::Target::HGRP_LIST:
      case ocs::gdi::Target::RQS_LIST:
      case ocs::gdi::Target::RL_LIST:
      case ocs::gdi::Target::MASTER_EVENT:
         /* user must be a manager */
         if (!manop_is_manager(packet)) {
            ERROR(MSG_SGETEXT_MUSTBEMANAGERFORTAR_SS, packet->user, to_string(tar).c_str());
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;

      case ocs::gdi::Target::US_LIST:
         /* user must be a manager (CS-2973): access lists grant rights - the
          * arusers and deadlineusers lists, and every list a queue, a project,
          * a parallel environment or a host admits users by - so an operator
          * who could change them could give himself access he does not have */
         if (!manop_is_manager(packet)) {
            ERROR(MSG_SGETEXT_MUSTBEMANAGERFORTAR_SS, packet->user, to_string(tar).c_str());
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;

      case ocs::gdi::Target::JB_LIST:

         /*
            what checking could we do here ?

            we had to check if there is a queue configured for scheduling
            of jobs of this group/user. If there is no such queue we
            had to deny submitting.

            Other checkings need to be done in stub functions.

         */
         break;

      case ocs::gdi::Target::EV_LIST:
         /*
            an event client can be started by any user - it can only
            read objects like SGE_GDI_GET
            delete requires more info - is done in ocs::gdi::Client::sge_gdi_kill_eventclient
         */
         break;
      case ocs::gdi::Target::AR_LIST: {
         const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();

         // only managers and ARUSERS are allowed to create AR's
         if (!manop_is_manager(packet) && !user_is_ar_user(packet, master_userset_list)) {
            ERROR(MSG_SGETEXT_MUSTBEMANAGERORUSER_SS, packet->user, AR_USERS);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         break;
      }
      default:
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_OPNOIMPFORTARGET_S, __func__);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
         DRETURN(false);
   }

   DRETURN(true);
}

/** @brief checks modify-permissions of host for a given target
 *
 * Qmaster internal requests are not checked for permissions
 *
 * The qmaster_params ALLOW_ANY_SUBMITHOSTS lets any host which can reach the qmaster
 * count as a submit host. Nothing here needs to ask for it separately: wherever a
 * submit host is accepted, such a host is accepted too, and wherever an admin host is
 * required it is not.
 *
 * @param packet - packet to check
 * @param alpp - answer list pointer
 * @param target - target to check
 * @param command - the GDI command of the request
 *
 * @return true if permission is granted, false otherwise
 */
bool
sge_chck_mod_perm_host(const ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                       const ocs::gdi::Target target, const ocs::gdi::Command command) {
   DENTER(TOP_LAYER);

   if (!packet->is_intern_request) {
      bool is_admin_host = host_is_admin_host(packet->host);
      bool is_submit_host = host_is_submit_host(packet->host);

      switch (target) {
         case ocs::gdi::Target::EH_LIST: {
            const auto master_exec_host_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
            bool is_exec_host = host_list_locate(master_exec_host_list, packet->host) != nullptr;
            bool is_execd = strcmp(packet->commproc, to_cstr(EXECD)) == 0;

            // a host which is not known yet may add itself to the cluster if one of the rules
            // of the qmaster_params AUTO_ADD_HOSTS matches it
            //
            // this is limited to an execd adding an exec host: the name of the host which is
            // added is taken from the sending host in sge_execd_startedup(), therefore such a
            // request can add the sending host and no other one, and it grants neither admin
            // nor submit host rights
            bool may_add_itself = false;
            if (is_execd && !is_exec_host && command == ocs::gdi::Command::ADD) {
               std::string hostgroup;

               may_add_itself = ocs::AutoAddHosts::match_host(packet->host, packet->ip, hostgroup);
            }

            // host must be either admin host
            // or exec host and request has to come from execd
            // or a host which may add itself
            if (!(is_admin_host || (is_exec_host && is_execd) || may_add_itself)) {
               ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
               answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
            break;
         }
         case ocs::gdi::Target::EV_LIST:
            // host must be admin host or submit host
               if (!is_submit_host && !is_admin_host) {
                  ERROR(MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
                  answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
                  DRETURN(false);
               }
         break;
         case ocs::gdi::Target::JB_LIST:
         case ocs::gdi::Target::AR_LIST:
            // host must be a submit host
            if (!is_submit_host) {
               ERROR(MSG_SGETEXT_NOSUBMITHOST_S, packet->host);
               answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         break;
         case ocs::gdi::Target::CQ_LIST:
            if (command == ocs::gdi::Command::TRIGGER) {
               // trigger on cluster queue list is qmod (e.g., -s),
               // must be submit or admin host
               if (!is_submit_host && !is_admin_host) {
                  ERROR(MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
                  answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
                  DRETURN(false);
               }
            } else {
               // all other commands on cluster queue list are configuration commands,
               // need to be admin host
               if (!is_admin_host) {
                  ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
                  answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
                  DRETURN(false);
               }
            }
            break;
         default:
            // for all other targets, the host must be an admin host
            if (!is_admin_host) {
               ERROR(MSG_SGETEXT_NOADMINHOST_S, packet->host);
               answer_list_add(alpp, SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         break;
      }
   }

   DRETURN(true);
}

/** @brief checks get-permissions of host for a given target
 *
 * Qmaster internal requests are not checked for permissions
 *
 * The qmaster_params ALLOW_ANY_SUBMITHOSTS lets any host which can reach the qmaster
 * count as a submit host here as well. A host which may place work in the cluster has
 * to be able to look at it afterwards, otherwise it can submit a job and never learn
 * what became of it.
 *
 * @param packet - packet to check
 * @param task - task to check
 * @param monitor - monitoring structure
 *
 * @return true if permission is granted, false otherwise
 */
static bool
sge_task_check_get_perm_host(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
   DENTER(TOP_LAYER);

   // only external requests need to be checked
   if (!packet->is_intern_request) {
      const bool is_admin_host = host_is_admin_host(packet->host);
      const bool is_submit_host = host_is_submit_host(packet->host);

      switch (task->target) {
         case ocs::gdi::Target::CONF_LIST: {
            const auto master_exec_host_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
            bool is_exec_host = host_list_locate(master_exec_host_list, packet->host) != nullptr;

            // host must be either admin/submit or exec host
            if (!is_admin_host && !is_submit_host && !is_exec_host) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
            break;
         }
         default:
            // for all other targets host must be an admin host or submit host
               if (!is_admin_host && !is_submit_host) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NOSUBMITORADMINHOST_S, packet->host);
                  answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
                  DRETURN(false);
               }
         break;
      }
   }

   DRETURN(true);
}


/* ------------------------------ generic add and modify --------------------- */

/**
 * @brief Refuse an add or modify without an object or without a table entry
 *
 * @param alpp receives the refusal
 * @param function the generic function, for the message
 * @return STATUS_EUNKNOWN
 */
static int
ocs_gdi_add_mod_null(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *function) {
   DENTER(TOP_LAYER);
   CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, function);
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Refuse an add or modify whose request does not carry the key field of the table entry
 *
 * @param alpp receives the refusal
 * @param object the table entry
 * @param function the generic function, for the message
 * @return STATUS_EUNKNOWN
 */
static int
ocs_gdi_add_mod_missing_key(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const gdi_object_t *object,
                            const char *function) {
   DENTER(TOP_LAYER);
   CRITICAL(MSG_SGETEXT_MISSINGFIELD_SS, ocs::tom::field_name(object->key_nm), function);
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Refuse an add or modify of an object type that has no generic add and modify
 *
 * Says what the untyped generic path said for such a table entry: the request does not carry the key field of
 * the entry, which is none for most of these entries (key field 0).
 *
 * @param alpp receives the refusal
 * @param object the table entry, nullptr is refused as a null pointer
 * @return STATUS_EUNKNOWN
 */
int
ocs_gdi_add_mod_unsupported(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const gdi_object_t *object) {
   DENTER(TOP_LAYER);
   const int ret = object == nullptr ? ocs_gdi_add_mod_null(alpp, __func__)
                                     : ocs_gdi_add_mod_missing_key(alpp, object, __func__);
   DRETURN(ret);
}

/**
 * @brief Refuse an add or modify of an execution host whose name does not resolve
 *
 * @param alpp receives the refusal
 * @param host the host name of the request, or nullptr
 * @return STATUS_EUNKNOWN
 */
static int
ocs_gdi_add_mod_unresolved_host(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *host) {
   DENTER(TOP_LAYER);
   ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, host ? host : "nullptr");
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Check the name of the request and whether its object exists as the command needs it
 *
 * An add needs an object that does not exist yet, a modify one that does.
 *
 * @param alpp receives the refusal
 * @param object the table entry
 * @param add 1 for add, 0 for modify
 * @param name the key of the request as text, nullptr if it has none
 * @param exists whether the master list holds an object with that key
 * @return STATUS_OK if the request may go on, STATUS_EEXIST otherwise
 */
static int
ocs_gdi_add_mod_check_existence(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const gdi_object_t *object, const int add,
                                const char *name, const bool exists) {
   DENTER(TOP_LAYER);
   if (name == nullptr) {
      answer_list_add(alpp, MSG_OBJ_NAME_MISSING, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* prevent duplicates / modifying non existing objects */
   if ((exists && add) || (!exists && !add)) {
      ERROR(add ? MSG_SGETEXT_ALREADYEXISTS_SS : MSG_SGETEXT_DOESNOTEXIST_SS, object->object_name, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }
   DRETURN(STATUS_OK);
}

/**
 * @brief Refuse an add or modify for which the new object could not be created
 *
 * @param alpp receives the refusal
 * @return STATUS_EEXIST
 */
static int
ocs_gdi_add_mod_no_memory(ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   ERROR(SFNMAX, MSG_MEM_MALLOC);
   answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
   DRETURN(STATUS_EEXIST);
}

/**
 * @brief Pass on what a failed modifier said
 *
 * Only the last message is passed on, the ones before may be success messages that are no longer true; an
 * advance reservation passes on all of them.
 *
 * @param alpp receives the messages, nullptr to drop them
 * @param tmp_alp what the modifier said, taken over
 * @param pass_all true to pass on every message
 * @return STATUS_EUNKNOWN
 */
static int
ocs_gdi_add_mod_modifier_failed(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ListPtr<ocs::tom::AN> &&tmp_alp,
                                const bool pass_all) {
   DENTER(TOP_LAYER);
   auto answers = std::move(tmp_alp);
   if (alpp != nullptr && answers) {
      if (!*alpp) {
         *alpp = ocs::tom::ListPtr<ocs::tom::AN>::create("answer");
      }

      if (pass_all) {
         (*alpp)->splice(*answers);
      } else if (const auto failure = answers->back()) {
         (*alpp)->push_back(answers->extract(failure));
      }
   }
   answers.reset();
   DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Whether no unique attribute of the new object holds a key twice (CS-3053)
 *
 * Checked after the modifier, so that the merges which collapse a repeated key on purpose (host groups) keep
 * working.
 *
 * @param new_obj the new object; the check is descriptor-generic and takes it raw
 * @param alpp receives the refusal
 * @param object the table entry
 * @param name the key of the object as text
 * @return true if the object holds every key once
 */
static bool
ocs_gdi_add_mod_unique_keys(const ocs::tom::ElementData *new_obj, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                            const gdi_object_t *object, const char *name) {
   DENTER(TOP_LAYER);
   const std::string object_name = std::string(object->object_name) + " " + name;
   const bool ret = ocs_object_verify_unique_keys(new_obj, alpp, object_name.c_str());
   DRETURN(ret);
}

/**
 * @brief Pass on what the modifier said about an object that is spooled now
 *
 * @param alpp receives the messages, nullptr to drop them
 * @param tmp_alp what the modifier said, taken over
 */
static void
ocs_gdi_add_mod_take_answers(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ListPtr<ocs::tom::AN> &&tmp_alp) {
   DENTER(TOP_LAYER);
   auto answers = std::move(tmp_alp);
   if (alpp != nullptr) {
      if (*alpp == nullptr) {
         *alpp = ocs::tom::ListPtr<ocs::tom::AN>::create("answer");
      }

      /* copy tmp_alp to alpp */
      (*alpp)->splice(*answers);
   }
   answers.reset();
   DRETURN_VOID;
}

/**
 * @brief Confirm an add or modify that is done, unless the client asked for the new version instead
 *
 * @param alpp receives the confirmation
 * @param object the table entry
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param name the key of the object as text
 * @param sub_command what kind of modification this is
 * @return STATUS_OK
 */
static int
ocs_gdi_add_mod_done(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const gdi_object_t *object, const int add,
                     const char *ruser, const char *rhost, const char *name, const ocs::gdi::SubCommand sub_command) {
   DENTER(TOP_LAYER);
   if ((sub_command & ocs::gdi::SubCommand::RETURN_NEW_VERSION) != ocs::gdi::SubCommand::RETURN_NEW_VERSION) {
      INFO(add ? MSG_SGETEXT_ADDEDTOLIST_SSSS : MSG_SGETEXT_MODIFIEDINLIST_SSSS, ruser, rhost, name,
           object->object_name);

      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }
   DRETURN(STATUS_OK);
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
/**
 * @brief Add or modify one configuration object of a type with a generic add and modify
 *
 * The sequence every object type shares: validate each attribute through the modifier of the type, spool
 * through its writer, then let its on_success announce it (ocs_gdi_hooks). Compiled once per object type, so
 * the lookup by key, the copy and the update of the master list are typed; what does not depend on the type -
 * the messages, the answer handling, the check for repeated keys - is in shared functions.
 *
 * @tparam T the object type
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller, may be nullptr
 * @param request the object as the client wants it to become, possibly reduced; must carry the key field
 * @param add 1 for add, 0 for modify
 * @param object the table entry of @p T, for its name in messages and for the hooks
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param post receives what the on_success hook leaves for after the last object of the request, nullptr
 *        when the caller has no such step
 * @param monitor for monitoring qmaster threads
 * @return STATUS_OK on success, an error status otherwise
 */
template <OcsGdiGenericType T>
int
ocs_gdi_add_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                const ocs::tom::Element<T> request, const int add, gdi_object_t *object, const char *ruser,
                const char *rhost, const ocs::gdi::Command cmd, const ocs::gdi::SubCommand sub_command,
                ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   using H = ocs_gdi_hooks<T>;
   using E = ocs_object_type_t<H::list_type>;
   static_assert(std::is_same_v<typename E::type, T> && E::has_key, "the master list holds T, and T has a key");
   constexpr auto key = E::key;
   constexpr ocs::tom::FieldType key_type = ocs::tom::FieldTraits<key>::type;

   /* DO COMMON CHECKS AND SEARCH OLD OBJECT */
   if (!request || object == nullptr) {
      DRETURN(ocs_gdi_add_mod_null(alpp, __func__));
   }

   /* the request is no element of this type if it does not contain the primary key attribute; a table entry of
    * another master list is refused the same way, it does not describe T */
   if (object->list_type != H::list_type || !request.template has<key>()) {
      DRETURN(ocs_gdi_add_mod_missing_key(alpp, object, __func__));
   }

   /* resolve host name in case of objects with hostnames as key before searching for the objects */
   if constexpr (std::is_same_v<T, ocs::tom::EH>) {
      if (ocs_resolve_host<key>(request) != CL_RETVAL_OK) {
         DRETURN(ocs_gdi_add_mod_unresolved_host(alpp, request.template get<key>().c_str()));
      }
   }

   /* get and verify the primary key */
   char ds_buffer[256];
   dstring buffer;
   sge_dstring_init(&buffer, ds_buffer, sizeof(ds_buffer));
   const char *name;
   ocs::tom::Element<T> old_obj;
   // read access to the slot, as master_list() gives it; the object found is changed further on
   const auto master_list = ocs::DataStore::master_list_rw<H::list_type>(true);
   if constexpr (key_type == ocs::tom::FieldType::Uint32) {
      const uint32_t id = request.template get<key>();
      sge_dstring_sprintf(&buffer, sge_u32, id);
      name = sge_dstring_get_string(&buffer);
      old_obj = master_list.template find_first<key>(id);
   } else if constexpr (key_type == ocs::tom::FieldType::Host) {
      const auto host = request.template get<key>();
      name = host.c_str();
      old_obj = master_list.template find_first<key>(host);
   } else {
      static_assert(key_type == ocs::tom::FieldType::String, "the key is a number, a host or a string");
      name = request.template get<key>();
      old_obj = master_list.template find_first<key>(name);
   }

   if (const int status = ocs_gdi_add_mod_check_existence(alpp, object, add, name, static_cast<bool>(old_obj));
       status != STATUS_OK) {
      DRETURN(status);
   }

   /* create a new object (add case), or make a copy of the old object (mod case) */
   auto new_obj = add ? ocs::tom::ElementPtr<T>::create() : old_obj.clone();
   if (!new_obj) {
      DRETURN(ocs_gdi_add_mod_no_memory(alpp));
   }

   /* modify the new object base on information in the request; on error the new object is disposed of */
   ocs::tom::ListPtr<ocs::tom::AN> tmp_alp;
   if (H::modifier(packet, task, &tmp_alp, new_obj.get(), request, add, ruser, rhost, object, cmd, sub_command,
                   monitor) != 0) {
      DRETURN(ocs_gdi_add_mod_modifier_failed(alpp, std::move(tmp_alp), std::is_same_v<T, ocs::tom::AR>));
   }

   /* no unique attribute of the resulting object may hold a key twice (CS-3053) */
   if (!ocs_gdi_add_mod_unique_keys(new_obj.get(), alpp, object, name)) {
      DRETURN(STATUS_ESYNTAX);
   }

   /* write on file */
   if (H::writer(packet, task, alpp, new_obj.get(), object) != 0) {
      DRETURN(STATUS_EUNKNOWN);
   }

   ocs_gdi_add_mod_take_answers(alpp, std::move(tmp_alp));

   /* replace the old object by the new one */
   ocs::tom::ElementPtr<T> old_owned;
   if (old_obj) {
      /* chain out the old object */
      old_owned = ocs::DataStore::master_list_rw<H::list_type>().extract(old_obj);
   }
   /* chain in new object; ensure our global list exists */
   const auto stored =
      ocs::DataStore::master_list_create<H::list_type>(object->object_name).push_back(std::move(new_obj));

   /* once we successfully added/modified the object, do final steps (on_success callback) */
   H::on_success(packet, task, stored, old_owned.get(), object, post, monitor);
   old_owned.reset();

   DRETURN(ocs_gdi_add_mod_done(alpp, object, add, ruser, rhost, name, sub_command));
}

/* *INDENT-OFF* */
// the instantiations for the callers outside this file, one per type of OcsGdiGenericType
template int ocs_gdi_add_mod<ocs::tom::CAL>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::CAL>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::EH>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::EH>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::CQ>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::CQ>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::CE>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::CE>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::PE>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::PE>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::UU>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::UU>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::US>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::US>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::PR>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::PR>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::CK>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::CK>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::RQS>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::RQS>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::HGRP>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::HGRP>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::AR>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::AR>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
template int ocs_gdi_add_mod<ocs::tom::RL>(ocs::gdi::Packet *, ocs::gdi::Task *, ocs::tom::ListPtr<ocs::tom::AN> *,
   ocs::tom::Element<ocs::tom::RL>, int, gdi_object_t *, const char *, const char *, ocs::gdi::Command,
   ocs::gdi::SubCommand, ocs_gdi_post_processing *, monitoring_t *);
/* *INDENT-ON* */

/**
 * @brief Call a callable with the object type of a table entry, if that type has a generic add and modify
 *
 * Dispatches once on the master list type (ocs_with_object_type()); the callable is compiled only for the
 * types of #OcsGdiGenericType.
 *
 * @tparam F a generic callable taking `std::type_identity<T>`
 * @param list_type the master list of the table entry
 * @param f the callable
 * @return true if it was called, false for a type without a generic add and modify or without an object
 */
template <typename F>
static bool
ocs_gdi_with_generic_type(const sge_object_type list_type, F &&f) {
   DENTER(TOP_LAYER);
   const bool ret = ocs_with_object_type(list_type, [&f](const auto &entry) {
      using T = typename ocs_object_type_entry_t<decltype(entry)>::type;
      if constexpr (OcsGdiGenericType<T>) {
         f(std::type_identity<T>{});
         return true;
      } else {
         return false;
      }
   }).value_or(false);
   DRETURN(ret);
}

/**
 * @brief Add or modify every object of a request list through the generic path of its table entry
 *
 * Dispatches once on the object type of the table entry and walks the requests typed. The next request is
 * fetched before the current one is handled; the generic path does not change the request list. A table entry
 * without a generic add and modify refuses every request.
 *
 * @param packet the client request
 * @param task the GDI task being answered; receives the answers
 * @param requests the requests, of the type of the table entry
 * @param add 1 for add, 0 for modify
 * @param object the table entry of the target
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param post receives what the on_success hooks leave for after the last object, or nullptr
 * @param monitor for monitoring qmaster threads
 */
static void
ocs_gdi_add_mod_list(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::RawList requests, const int add,
                     gdi_object_t *object, const ocs::gdi::Command cmd, const ocs::gdi::SubCommand sub_command,
                     ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   const bool generic = ocs_gdi_with_generic_type(object->list_type, [&](auto tag) {
      using T = typename decltype(tag)::type;
      // the request data is structure checked by the listener (CS-3125): it carries the type of the table entry
      auto ep = ocs::tom::List<T>::from_typed(requests).front();
      while (ep) {
         const auto next = ep.next();
         ocs_gdi_add_mod<T>(packet, task, &(task->answer_list), ep, add, object, packet->user, packet->host, cmd,
                            sub_command, post, monitor);
         ep = next;
      }
   });
   if (!generic) {
      // the element type is given at run time by the target, and every request is refused
      for (auto n = requests.size(); n > 0; n--) {
         ocs_gdi_add_mod_unsupported(&(task->answer_list), object);
      }
   }
   DRETURN_VOID;
}

/*
 * MT-NOTE: get_gdi_object() is MT safe
 */
/** @brief The table entry for one GDI target
 * @param target the object type
 * @return its entry, or nullptr when the target has no generic handling
 */
gdi_object_t *get_gdi_object(ocs::gdi::Target target) {
   DENTER(TOP_LAYER);

   for (int i = 0; gdi_object[i].target != ocs::gdi::Target::NO_TARGET; i++) {
      if (target == gdi_object[i].target) {
         DRETURN(&gdi_object[i]);
      }
   }

   DRETURN(nullptr);
}

