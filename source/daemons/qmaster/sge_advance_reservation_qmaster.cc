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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Advance reservations: booking resources ahead of the jobs that will use them
 */

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "uti/ocs_Pattern.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_hostname.h"

#include "spool/sge_spooling.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_FreeAccess.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_ARA.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"

#include "sched/debit.h"
#include "sched/sge_resource_utilization.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_job_schedd.h"
#include "sched/sge_serf.h"
#include "sched/valid_queue_user.h"
#include "sched/sge_complex_schedd.h"

#include "evm/sge_event_master.h"
#include "evm/sge_queue_event_master.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_utility_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_qinstance_qmaster.h"
#include "mail.h"
#include "symbols.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "msg_daemons_common.h"
#include "ocs_AdvanceReservation.h"
#include "ocs_GrantedResources.h"
#include "sge_sched_thread_rsmap.h"

/** @brief The next advance reservation id, and whether it still has to be spooled
 *
 * The same arrangement as the job number: ids must not repeat across a restart,
 * so the counter is spooled, but not on every reservation.
 */
typedef struct {
   uint32_t ar_id;               ///< The next id to hand out
   bool changed;                 ///< The spooled value is out of date
   pthread_mutex_t ar_id_mutex;  ///< Guards both fields above
} ar_id_t;

ar_id_t ar_id_control = {0, false, PTHREAD_MUTEX_INITIALIZER};   ///< @copybrief ar_id_t

static bool
ar_reserve_queues(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::AR> ar, uint64_t gdi_session);

static uint32_t
sge_get_ar_id(monitoring_t *monitor);

static uint32_t
guess_highest_ar_id();

static void
sge_ar_send_mail(ocs::tom::ConstElement<ocs::tom::AR> ar, int type);

/** @brief Arm the timers for every reservation known at startup
 *
 * A reservation that should already have started while qmaster was down has to
 * be caught up rather than missed.
 *
 * @param answer_list receives messages for the caller
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 */
void
ar_initialize_timer(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   uint64_t now = sge_get_gmt64();

   const auto ar_master_list = ocs::DataStore::master_list_rw<SGE_TYPE_AR>();

   auto next_ar = ar_master_list.front();

   while (const auto ar = next_ar) {
      te_event_t ev = nullptr;

      next_ar = ar.next();

      if (now < ar.get<AR_start_time>()) {
         sge_ar_state_set_waiting(ar);

         ev = te_new_event(ar.get<AR_start_time>(), TYPE_AR_EVENT,
                           ONE_TIME_EVENT, ar.get<AR_id>(), AR_RUNNING, nullptr);
         te_add_event(ev);
         te_add_event(ev);
         te_free_event(&ev);

      } else if (now < ar.get<AR_end_time>()) {
         sge_ar_state_set_running(ar);

         ev = te_new_event(ar.get<AR_end_time>(), TYPE_AR_EVENT,
                           ONE_TIME_EVENT, ar.get<AR_id>(), AR_EXITED, nullptr);
         te_add_event(ev);
         te_free_event(&ev);
      } else {
         dstring buffer = DSTRING_INIT;
         uint32_t ar_id = ar.get<AR_id>();

         sge_ar_state_set_running(ar);

         sge_ar_remove_all_jobs(ar_id, 1, monitor, gdi_session);

         ar_do_reservation(ar, false, gdi_session);

         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_TERMINATED,
                                        "end time of AR reached",
                                        now);
         ocs::ReportingFileWriter::create_ar_acct_records(nullptr, ar, now);

         sge_dstring_sprintf(&buffer, sge_u32, ar_id);

         ar_master_list.erase(ar);

         spool_delete_object(answer_list, spool_get_default_context(),
                             SGE_TYPE_AR, sge_dstring_get_string(&buffer));

         sge_dstring_free(&buffer);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Verify the project requested for an advance reservation with qrsub -P.
 *
 * The checks are the same as for a job requesting a project: the project has to exist, the
 * user has to have access to it, and the projects and xprojects of the global configuration
 * have to allow it. An advance reservation without a project is not checked.
 *
 * @param ar The advance reservation, AR_project holds the requested project.
 * @param packet The GDI request, which holds the user, the group and the supplementary groups.
 * @param alpp The answer list which receives the error message.
 * @return true if no project is requested or the project is valid, false otherwise.
 */
static bool
ar_verify_project(const ocs::tom::ConstElement<ocs::tom::AR> ar, const ocs::gdi::Packet *packet,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   const char *project = ar.get<AR_project>();
   if (project == nullptr) {
      DRETURN(true);
   }

   // the project has to exist
   const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
   const auto pep = master_project_list.find_first<PR_name>(project);
   if (!pep) {
      ERROR(MSG_AR_PRJUNKNOWN_S, project);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // the user has to have access to the project
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto grp_list = grp_list_array2list(packet->amount, packet->grp_array);
   bool has_access = sge_has_access_(packet->user, packet->group, *grp_list, pep.get<PR_acl>(),
                                     pep.get<PR_xacl>(), master_userset_list) != 0;
   if (!has_access) {
      ERROR(MSG_AR_NOACCESS2PRJ4USER_SS, project, packet->user);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // the projects and xprojects of the global configuration have to allow the project
   const auto projects = mconf_get_projects();
   const auto xprojects = mconf_get_xprojects();
   bool allowed = (!xprojects || xprojects->find_first<PR_name>(project) == nullptr) &&
                  (!projects || projects->find_first<PR_name>(project) != nullptr);
   if (!allowed) {
      ERROR(MSG_AR_PRJNOTALLOWED_S, project);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   DRETURN(true);
}

/**
 * @brief Gdi callback function for adding modifing advance reservations
 *
 * This function is called from the framework that
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to add new advance reservation
 * objects.
 * Modifing is currently not supported.
 *
 * The sub-lists are replaced by copies of the request's (replace_sublist()); a keyed sub-command, which
 * qrsub does not send, is answered with a missing key, as ever.
 *
 * @param alpp the answer_list
 * @param new_ar if a new ar object will be created by this function, then new_ar is a newly initialized
 *               advance reservation
 * @param ar a reduced advance reservation that contains all of the requested values; its sub-lists are
 *           consumed
 * @param add 1 for add requests 0 for mod requests
 * @param packet the client request, carrying the requesting user and host
 * @param task the GDI task being answered
 * @param object structure of the GDI framework that contains additional informations to perform the request
 * @param sub_command GDI sub command
 * @param monitor monitoring structure
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param cmd the command being executed
 *
 * @return 0 on success STATUS_EUNKNOWN if an error occurred STATUS_NOTOK_DOAGAIN if a temporary error
 *
 * @note MT-NOTE: ar_mod() is not MT safe
 */
int ar_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
           const ocs::tom::Element<ocs::tom::AR> new_ar, const ocs::tom::Element<ocs::tom::AR> ar,
           const int add, const char *ruser, const char *rhost, gdi_object_t *object, const ocs::gdi::Command cmd,
           const ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t ar_id;
   uint32_t max_advance_reservations = mconf_get_max_advance_reservations();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();

   if (!ar_validate(ar, alpp, true, false, master_cqueue_list, master_hgroup_list, master_centry_list, master_ckpt_list,
                    master_pe_list, master_userset_list)) {
      goto AR_MOD_ERROR;
   }

   if (add) {
      /* get new ar ids until we find one that is not yet used */
      do {
         ar_id = sge_get_ar_id(monitor);
      } while (master_ar_list.find_first<AR_id>(ar_id));
      new_ar.set<AR_id>(ar_id);
      /*
      ** set the owner of new_ar, don't overwrite it with
      ** attr_mod_str(alpp, ar, new_ar, AR_owner, object->object_name);
      */
      new_ar.set<AR_owner>(ruser);
      new_ar.set<AR_group>(component_get_groupname());
   } else {
      ERROR(MSG_NOTYETIMPLEMENTED_S, "advance reservation modification");
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      goto AR_MOD_ERROR;
   }

   if (max_advance_reservations > 0 &&
       max_advance_reservations <= master_ar_list.size()) {
      ERROR(MSG_AR_MAXARSPERCLUSTER_U, max_advance_reservations);
      answer_list_add(alpp, SGE_EVENT, STATUS_NOTOK_DOAGAIN, ANSWER_QUALITY_ERROR);
      goto DOITAGAIN;
   }

   /*    AR_name, SGE_STRING */
   new_ar.copy_if_present<AR_name>(ar);
   /*   AR_account, SGE_STRING */
   new_ar.copy_if_present<AR_account>(ar);
   /*   AR_project, SGE_STRING */
   new_ar.copy_if_present<AR_project>(ar);
   if (!ar_verify_project(new_ar, packet, alpp)) {
      goto AR_MOD_ERROR;
   }
   /*   AR_submission_time, SGE_ULONG */
   new_ar.set<AR_submission_time>(sge_get_gmt64());
   /*   AR_start_time, SGE_ULONG          required */
   new_ar.copy_if_present<AR_start_time>(ar);
   /*   AR_end_time, SGE_ULONG            required */
   new_ar.copy_if_present<AR_end_time>(ar);
   /*   AR_duration, SGE_ULONG */
   new_ar.copy_if_present<AR_duration>(ar);
   /*   AR_verify, SGE_ULONG              just verify the reservation or final case */
   new_ar.copy_if_present<AR_verify>(ar);
   /*   AR_error_handling, SGE_ULONG      how to deal with soft and hard exceptions */
   new_ar.copy_if_present<AR_error_handling>(ar);
   /*   AR_state, SGE_ULONG               state of the AR */
   new_ar.set<AR_state>(AR_WAITING);
   /*   AR_checkpoint_name, SGE_STRING    Named checkpoint */
   new_ar.copy_if_present<AR_checkpoint_name>(ar);
   /*   AR_resource_list, SGE_LIST */
   replace_sublist<AR_resource_list>(alpp, new_ar, ar, sub_command, SGE_ATTR_COMPLEX_VALUES, SGE_OBJ_AR);
   /*   AR_queue_list, SGE_LIST */
   replace_sublist<AR_queue_list>(alpp, new_ar, ar, sub_command, SGE_ATTR_QUEUE_LIST, SGE_OBJ_AR);
   /*   AR_mail_options, SGE_ULONG   */
   new_ar.copy_if_present<AR_mail_options>(ar);
   /*   AR_mail_list, SGE_LIST */
   replace_sublist<AR_mail_list>(alpp, new_ar, ar, sub_command, SGE_ATTR_MAIL_LIST, SGE_OBJ_AR);
   /*   AR_pe, SGE_STRING */
   new_ar.copy_if_present<AR_pe>(ar);
   /*   AR_master_queue_list, SGE_LIST */
   replace_sublist<AR_master_queue_list>(alpp, new_ar, ar, sub_command, SGE_ATTR_QUEUE_LIST, SGE_OBJ_AR);
   /*   AR_pe_range, SGE_LIST */
   replace_sublist<AR_pe_range>(alpp, new_ar, ar, sub_command, SGE_ATTR_PE_LIST, SGE_OBJ_AR);
   /*   AR_allocation_rule, SGE_STRING */
   new_ar.copy_if_present<AR_allocation_rule>(ar);
   /*   AR_acl_list, SGE_LIST */
   replace_sublist<AR_acl_list>(alpp, new_ar, ar, sub_command, SGE_ATTR_USER_LISTS, SGE_OBJ_AR);
   /*   AR_xacl_list, SGE_LIST */
   replace_sublist<AR_xacl_list>(alpp, new_ar, ar, sub_command, SGE_ATTR_XUSER_LISTS, SGE_OBJ_AR);
   /*   AR_type, SGE_ULONG     */
   new_ar.copy_if_present<AR_type>(ar);
   // AR_binding
   attr_mod_obj_binding(alpp, new_ar, ar);

   /* try to reserve the queues */
   if (!ar_reserve_queues(alpp, new_ar, packet->gdi_session)) {
      goto AR_MOD_ERROR;
   }

   INFO(MSG_AR_GRANTED_U, ar_id);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(0);

   AR_MOD_ERROR:
DRETURN(STATUS_EUNKNOWN);
   DOITAGAIN:
DRETURN(STATUS_NOTOK_DOAGAIN);
}

/**
 * @brief Gdi callback funktion to spool an advance reservation
 *
 * This function is called from the framework that
 * add/modify/delete generic gdi objects.
 * After an object was modified/added successfully it
 * is necessary to spool the current state to the filesystem.
 *
 * @param alpp answer_list
 * @param ar element to spool
 * @param object structure from the GDI framework
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return [alpp] - error messages will be added to this list 0 - success STATUS_EEXIST - an error occurred
 *
 * @note MT-NOTE: ar_spool() is MT safe
 */
int ar_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
             const ocs::tom::ConstElement<ocs::tom::AR> ar, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   dstring buffer = DSTRING_INIT;

   sge_dstring_sprintf(&buffer, sge_u32, ar.get<AR_id>());
   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ar,
                                   sge_dstring_get_string(&buffer), SGE_TYPE_AR);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, sge_dstring_get_string(&buffer));
   }
   sge_dstring_free(&buffer);

   DRETURN(dbret ? 0 : 1);
}

/**
 * @brief Does something after a successfully add or modify request
 *
 * This function is called from the framework that
 * add/modify/delete generic gdi objects.
 * After an object was modified/added and spooled successfully
 * it is possibly necessary to perform additional tasks.
 * For example it is necessary to send some events to
 * other daemon.
 *
 * @param ar new added object
 * @param old_ep old object before modifications or an empty handle for add requests
 * @param object structure from the GDI framework
 * @param post what is left for after the last object of the request, unused
 * @param monitor monitoring structure
 * @param packet the client request
 * @param task the GDI task being answered; on an add with RETURN_NEW_VERSION its data list receives a copy
 *        of the new AR
 *
 * @return 0
 *
 * @note MT-NOTE: ar_success() is not MT safe
 */
int
ar_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::AR> ar,
           const ocs::tom::ConstElement<ocs::tom::AR> old_ep, gdi_object_t *object,
           [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   te_event_t ev;
   uint64_t timestamp = sge_get_gmt64();

   /* with old_ep it is possible to identify if it is an add or modify request */
   if (old_ep == nullptr) {
      ocs::ReportingFileWriter::create_new_ar_records(nullptr, ar, timestamp);
      ocs::ReportingFileWriter::create_ar_attribute_records(nullptr, ar, timestamp);
   } else {
      ocs::ReportingFileWriter::create_ar_attribute_records(nullptr, ar, timestamp);
   }

   /*
   ** return element with correct id: an add with RETURN_NEW_VERSION answers with a copy of every added AR,
   ** made before the state is set below. sge_c_gdi_add() has taken the requests out of task->data_list for
   ** such a request, so the list holds nothing but what is appended here, and frees them after its loop.
   */
   if (task != nullptr && old_ep == nullptr &&
       (task->sub_command & ocs::gdi::SubCommand::RETURN_NEW_VERSION) == ocs::gdi::SubCommand::RETURN_NEW_VERSION) {
      if (task->data_list == nullptr) {
         // stays raw: task->data_list carries any object type, given at run time by the target
         task->data_list = ocs::tom::ListPtr<ocs::tom::AR>::create("").release();
      }
      // only this hook fills the data list of such a request, so it holds ARs
      ocs::tom::List<ocs::tom::AR>::from_typed(task->data_list).push_back(ar.clone());
   }

   sge_ar_state_set_waiting(ar);

   /*
   ** send sgeE_AR_MOD/sgeE_AR_ADD event
   */
   sge_add_event(0, old_ep ? sgeE_AR_MOD : sgeE_AR_ADD, ar.get<AR_id>(), 0,
                 nullptr, nullptr, nullptr, ar, packet->gdi_session);

   /*
   ** add the timer to trigger the state change
    */
   ev = te_new_event(ar.get<AR_start_time>(), TYPE_AR_EVENT, ONE_TIME_EVENT, ar.get<AR_id>(),
                     AR_RUNNING, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN(0);
}

/**
 * @brief Removes advance reservation from master list
 *
 * This function removes a advance reservation from the master list and
 * performs the necessary cleanup.
 *
 * @param ep element that should be removed (ID)
 * @param alpp answer list
 * @param master_ar_list list from where the element should be removed (normally the master ar list)
 * @param packet the client request, carrying the user and host that invoked it
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 *
 * @return 0 on success STATUS_EUNKNOWN on failure
 *
 * @note MT-NOTE: ar_del() is not MT safe
 */
int
ar_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::ID> ep,
       ocs::tom::ListPtr<ocs::tom::AN> *alpp,
       const ocs::tom::List<ocs::tom::AR> master_ar_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool removed_one = false;
   bool has_manager_privileges = false;
   dstring buffer = DSTRING_INIT;
   ocs::tom::Where<ocs::tom::AR> ar_where;

   if (!ep) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no ar_del element, if ep has no ID_str */
   if (!ep.has<ID_str>()) {
      CRITICAL(MSG_SGETEXT_MISSINGFIELD_SS, ocs::tom::FieldTraits<ID_str>::info.name, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *id_str = ep.get<ID_str>();
   const auto user_list = ep.get<ID_user_list>();
   if (user_list) {
      for (const auto user : user_list) {
         const char *user_name = user.get<ST_name>();
         bool is_pattern = ocs::is_pattern(user_name);

         if (is_pattern && !manop_is_manager(packet)) {
            ERROR(MSG_SGETEXT_MUST_BE_MGR_TO_SS, packet->user, "modify all advance reservations");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            sge_dstring_free(&buffer);
            DRETURN(STATUS_EUNKNOWN);
         }

         auto new_where = is_pattern ? ocs::tom::field<AR_owner>().like(user_name)
                                     : ocs::tom::field<AR_owner>() == user_name;
         if (!ar_where) {
            ar_where = std::move(new_where);
         } else {
            ar_where = std::move(ar_where) || std::move(new_where);
         }
      }
   } else if (id_str != nullptr && !isdigit(id_str[0])) {
      /* CS-2863: no user list and a selection by name - plain or wildcard - selects only the
       * caller's own ARs. That is what qrdel(1) documents, and it is what qdel does for a job
       * name, down to this same isdigit() test (job_list_filter() in sge_job_qmaster.cc). The
       * restriction used to be added for a pattern only, so an exact name reached the ARs of
       * every user. Selecting by id is deliberately not restricted here - an id names one AR,
       * and the permission check below decides whether the caller may have it. A manager who
       * wants the ARs of all users asks for them with -u '*'. */
      auto new_where = ocs::tom::field<AR_owner>() == packet->user;
      if (!ar_where) {
         ar_where = std::move(new_where);
      } else {
         ar_where = std::move(ar_where) || std::move(new_where);
      }
   }

   if (id_str != nullptr && (strcmp(id_str, "0") != 0)) {
      char *dptr;
      ocs::tom::Where<ocs::tom::AR> new_where;

      uint32_t value = strtol(id_str, &dptr, 0);
      if (dptr[0] == '\0') {
         /* is numeric value */
         new_where = ocs::tom::field<AR_id>() == value;
      } else {
         bool error = false;
         if (isdigit(id_str[0])) {
            ERROR(MSG_OBJECT_INVALID_NAME_S, id_str);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            error = true;
         } else if (verify_str_key(alpp, id_str, MAX_VERIFY_STRING,
                                   ocs::tom::FieldTraits<AR_name>::info.name, KEY_TABLE) != STATUS_OK) {
            error = true;
         } else {
            new_where = ocs::tom::field<AR_name>().like(id_str);
         }

         if (error) {
            sge_dstring_free(&buffer);
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      if (!ar_where) {
         ar_where = std::move(new_where);
      } else {
         ar_where = std::move(ar_where) && std::move(new_where);
      }
   } else {
      id_str = nullptr;
   }

   if (id_str == nullptr && !user_list) {
      CRITICAL(MSG_SGETEXT_SPECIFYUSERORID_S, SGE_OBJ_AR);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (manop_is_manager(packet)) {
      has_manager_privileges = true;
   }

   uint64_t now = sge_get_gmt64();
   auto nxt = master_ar_list.front();
   while (const auto ar = nxt) {
      uint32_t ar_id = ar.get<AR_id>();

      nxt = ar.next();

      if (ar_where && !ar.matches(ar_where)) {
         continue;
      }

      removed_one = true;

      if (!has_manager_privileges && strcmp(packet->user, ar.get<AR_owner>())) {
         ERROR(MSG_DELETEPERMS_SSU, packet->user, SGE_OBJ_AR, ar_id);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOTOWNER, ANSWER_QUALITY_ERROR);
         continue;
      }

      sge_ar_state_set_deleted(ar);

      /* remove timer for this advance reservation */
      te_delete_one_time_event(TYPE_AR_EVENT, ar_id, AR_RUNNING, nullptr);
      te_delete_one_time_event(TYPE_AR_EVENT, ar_id, AR_EXITED, nullptr);

      sge_ar_send_mail(ar, MAIL_AT_EXIT);

      /* remove all jobs refering to the AR */
      if (sge_ar_remove_all_jobs(ar_id, ep.get<ID_force>(), monitor, packet->gdi_session)) {
         /* either all jobs were successfull removed or we had no jobs */

         /* unblock reserved queues */
         ar_do_reservation(ar, false, packet->gdi_session);

         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_DELETED,
                                        "AR deleted",
                                        now);
         ocs::ReportingFileWriter::create_ar_acct_records(nullptr, ar, now);

         gdil_del_all_orphaned(ar.get<AR_granted_slots>(), alpp, packet->gdi_session);

         master_ar_list.erase(ar);

         INFO(MSG_JOB_DELETEX_SSU, packet->user, SGE_OBJ_AR, ar_id);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

         sge_event_spool(alpp, 0, sgeE_AR_DEL,
                         ar_id, 0, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr, true, true, packet->gdi_session);
      } else {
         INFO(MSG_JOB_REGDELX_SSU, packet->user, SGE_OBJ_AR, ar_id);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         sge_event_spool(alpp, 0, sgeE_AR_MOD,
                         ar_id, 0, nullptr, nullptr, nullptr,
                         ar, nullptr, nullptr, true, true, packet->gdi_session);
      }

   }

   if (!removed_one) {
      if (id_str != nullptr) {
         sge_dstring_sprintf(&buffer, "%s", id_str);
         ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, SGE_OBJ_AR, sge_dstring_get_string(&buffer));
      } else {
         bool first = true;
         int umax = 5;

         sge_dstring_sprintf(&buffer, "%s", "");
         for (const auto user : user_list) {
            if (!first) {
               sge_dstring_append(&buffer, ",");
            } else {
               first = false;
            }
            if (umax == 0) {
               sge_dstring_append(&buffer, "...");
               break;
            }
            sge_dstring_append(&buffer, user.get<ST_name>());
            umax--;
         }
         ERROR(MSG_SGETEXT_THEREARENOXFORUSERS_SS, SGE_OBJ_AR, sge_dstring_get_string(&buffer));
      }

      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(STATUS_EEXIST);
   }

   sge_dstring_free(&buffer);
   DRETURN(0);
}


/**
 * @brief Returns the next possible unused id
 *
 * returns the next possible unused advance reservation id.
 *
 * @param monitor monitoring structure
 *
 * @return ar id
 *
 * @note MT-NOTE: sge_get_ar_id() is MT safe
 */
static uint32_t
sge_get_ar_id(monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t ar_id;
   bool is_store_ar = false;

   sge_mutex_lock("ar_id_mutex", "sge_get_ar_id", __LINE__, &ar_id_control.ar_id_mutex);

   if (ar_id_control.ar_id >= std::numeric_limits<uint32_t>::max()) {
      DPRINTF("highest ar number std::numeric_limits<uint32_t>::max() %d reached, starting over with 1\n", std::numeric_limits<uint32_t>::max());
      ar_id_control.ar_id = 0;
      is_store_ar = true;
   }
   ar_id_control.ar_id++;
   ar_id_control.changed = true;
   ar_id = ar_id_control.ar_id;

   sge_mutex_unlock("ar_id_mutex", "sge_get_ar_id", __LINE__, &ar_id_control.ar_id_mutex);

   if (is_store_ar) {
      sge_store_ar_id(nullptr, monitor);
   }

   DRETURN(ar_id);
}

/**
 * @brief Store ar id
 *
 * At qmaster shutdown it's necessary to store the latest highest ar id to
 * reinitialize the counter at the next qmaster start. This is done by a event
 * timer in specific intervall.
 *
 * @param anEvent event that triggered this function
 * @param monitor pointer to monitor (not used here)
 *
 * @note MT-NOTE: sge_store_ar_id() is not MT safe
 */
void
sge_store_ar_id(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t ar_id = 0;
   bool changed = false;

   sge_mutex_lock("ar_id_mutex", "sge_store_ar_id", __LINE__,
                  &ar_id_control.ar_id_mutex);
   if (ar_id_control.changed) {
      ar_id = ar_id_control.ar_id;
      ar_id_control.changed = false;
      changed = true;
   }
   sge_mutex_unlock("ar_id_mutex", "sge_store_ar_id", __LINE__,
                    &ar_id_control.ar_id_mutex);

   /* here we got a race condition that can (very unlikely)
      cause concurrent writing of the sequence number file  */
   if (changed) {
      FILE *fp = fopen(ARSEQ_NUM_FILE, "w");

      if (fp == nullptr) {
         ERROR(MSG_NOSEQFILECREATE_SSS, "ar", ARSEQ_NUM_FILE, strerror(errno));
      } else {
         FPRINTF((fp, sge_u32 "\n", ar_id));
         FCLOSE(fp);
      }
   }
   DRETURN_VOID;

   FPRINTF_ERROR:
   FCLOSE_ERROR:
   ERROR(MSG_NOSEQFILECLOSE_SSS, "ar", ARSEQ_NUM_FILE, strerror(errno));
   DRETURN_VOID;
}

/**
 * @brief Init ar id counter
 *
 * Called during startup and sets the advance reservation id counter.
 *
 * @note MT-NOTE: sge_init_ar_id() is MT safe
 */
void
sge_init_ar_id() {
   DENTER(TOP_LAYER);

   FILE *fp = nullptr;
   uint32_t ar_id = 0;
   uint32_t guess_ar_id = 0;

   if ((fp = fopen(ARSEQ_NUM_FILE, "r"))) {
      if (fscanf(fp, sge_u32, &ar_id) != 1) {
         ERROR(MSG_NOSEQNRREAD_SSS, "ar", ARSEQ_NUM_FILE, strerror(errno));
      }
      FCLOSE(fp);
      FCLOSE_ERROR:
      fp = nullptr;
   } else {
      WARNING(MSG_NOSEQFILEOPEN_SSS, "ar", ARSEQ_NUM_FILE, strerror(errno));
   }

   guess_ar_id = guess_highest_ar_id();
   ar_id = std::max(ar_id, guess_ar_id);

   sge_mutex_lock("ar_id_mutex", "sge_init_ar_id", __LINE__,
                  &ar_id_control.ar_id_mutex);
   ar_id_control.ar_id = ar_id;
   ar_id_control.changed = true;
   sge_mutex_unlock("ar_id_mutex", "sge_init_ar_id", __LINE__,
                    &ar_id_control.ar_id_mutex);

   DRETURN_VOID;
}

/**
 * @brief Guesses the histest ar id
 *
 * Iterates over all granted advance reservations in the cluster and determines
 * the highest id
 *
 * @return determined id
 *
 * @note MT-NOTE: guess_highest_ar_id() is MT safe
 */
static uint32_t
guess_highest_ar_id() {
   DENTER(TOP_LAYER);

   uint32_t maxid = 0;

   /* this function is called during qmaster startup and not while it is running,
      we do not need to monitor this lock */
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();

   for (const auto ar : master_ar_list) {
      maxid = std::max(maxid, ar.get<AR_id>());
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   DRETURN(maxid);
}

/**
 * @brief Advance reservation event handler
 *
 * Registered function in the times event framework. For every granted a trigger
 * for the start time of the advance reservation is registered. When the function is
 * executed at start time it regististers a additional timer for the end time of
 * the advance reservation.
 *
 * @param anEvent triggered timed event
 * @param monitor monitoring structure
 *
 * @note MT-NOTE: sge_ar_event_handler() is MT safe
 */
void
sge_ar_event_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint32_t ar_id = te_get_first_numeric_key(anEvent);
   uint32_t state = te_get_second_numeric_key(anEvent);
   te_event_t ev;

   /*
    To guarantee all jobs are removed from the cluster when AR end time is
    reached it is necessary to consider the DURATION_OFFSET for Advance Reservation also.
    This means all jobs submitted to a AR will have a resulting runtime limit of AR duration - DURATION_OFFSET.
    Jobs requesting a longer runtime will not be scheduled.
    The AR requester needs to keep this in mind when he creates a new AR.
    */
   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   const auto master_ar_list = ocs::DataStore::master_list_rw<SGE_TYPE_AR>();

   const auto ar = master_ar_list.find_first<AR_id>(ar_id);
   if (!ar) {
      ERROR(MSG_EVE_TE4AR_U, ar_id);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
      DRETURN_VOID;
   }

   if (state == AR_EXITED) {
      uint64_t timestamp = sge_get_gmt64();

      sge_ar_state_set_exited(ar);

      /* remove all jobs running in this AR */
      sge_ar_remove_all_jobs(ar_id, 1, monitor, ocs::SessionManager::GDI_SESSION_NONE);

      /* unblock reserved queues */
      ar_do_reservation(ar, false, ocs::SessionManager::GDI_SESSION_NONE);

      ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_TERMINATED,
                                     "end time of AR reached",
                                     timestamp);
      ocs::ReportingFileWriter::create_ar_acct_records(nullptr, ar, timestamp);

      sge_ar_send_mail(ar, MAIL_AT_EXIT);

      /* remove all orphaned queue intances, which are empty. */
      gdil_del_all_orphaned(ar.get<AR_granted_slots>(), nullptr, ocs::SessionManager::GDI_SESSION_NONE);

      /* remove the AR itself */
      DPRINTF("AR: exited, removing AR " sge_u32 "\n", ar_id);
      master_ar_list.erase(ar);
      sge_event_spool(nullptr, timestamp, sgeE_AR_DEL,
                      ar_id, 0, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, true, true, ocs::SessionManager::GDI_SESSION_NONE);

   } else {
      /* AR_RUNNING */
      DPRINTF("AR: started, changing state of AR " sge_u32 "\n", ar_id);

      sge_ar_state_set_running(ar);

      ev = te_new_event(ar.get<AR_end_time>(), TYPE_AR_EVENT, ONE_TIME_EVENT, ar_id, AR_EXITED, nullptr);
      te_add_event(ev);
      te_free_event(&ev);

      /* this info is not spooled */
      sge_add_event(0, sgeE_AR_MOD, ar_id, 0,
                    nullptr, nullptr, nullptr, ar, ocs::SessionManager::GDI_SESSION_NONE);

      ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_STARTTIME_REACHED,
                                     "start time of AR reached",
                                     sge_get_gmt64());

      sge_ar_send_mail(ar, MAIL_AT_BEGINNING);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

/**
 * @brief Selects the queues for reserving
 *
 * The function executes the scheduler code to select queues matching the
 * advance reservation request for reserving. The function works on temporary
 * lists and creates the AR_granted_slots list
 *
 * @param alpp answer list pointer pointer
 * @param ar ar object
 * @param gdi_session the GDI session of the request, for reading the data store consistently
 *
 * @return true on success, enough resources reservable false in verify mode or not enough resources available
 *
 * @note MT-NOTE: ar_reserve_queues() is not MT safe, needs GLOBAL_LOCK
 */
static bool
ar_reserve_queues(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<ocs::tom::AR> ar,
                  const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[SPLIT_LAST];
   ocs::tom::ListPtr<ocs::tom::JB> suspended_list;
   ocs::tom::ListPtr<ocs::tom::JB> running_list;

   int verify_mode = ar.get<AR_verify>();
   ocs::tom::ListPtr<ocs::tom::AN> talp;
   const auto ar_queue_request = ar.get<AR_queue_list>();
   const char *ar_pe_request = ar.get<AR_pe>();
   const char *ar_ckpt_request = ar.get<AR_checkpoint_name>();

   bool ret = true;
   int i = 0;
   sge_assignment_t a{};
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   const auto master_cal_list = ocs::DataStore::master_list<SGE_TYPE_CALENDAR>();

   /* These lists must be copied */
   auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>().clone("");
   auto master_exechost_list =
      ocs::DataStore::master_list<SGE_TYPE_EXECHOST>().clone("");

   dispatch_t result = DISPATCH_NEVER_CAT;


   auto dummy_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   if (ar.get<AR_acl_list>()) {
      dummy_job->set<JB_owner>("*");
      dummy_job->set<JB_group>("*");
   } else {
      dummy_job->set<JB_owner>(ar.get<AR_owner>());
      dummy_job->set<JB_group>(ar.get<AR_group>());
   }
   // the queues and hosts are selected for the project of the AR, see their projects and xprojects
   dummy_job->set<JB_project>(ar.get<AR_project>());

   assignment_init(&a, *dummy_job, nullptr, nullptr);
   a.host_list = *master_exechost_list;
   a.centry_list = master_centry_list;
   a.acl_list = master_userset_list;
   a.hgrp_list = master_hgroup_list;
   a.gep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(*master_exechost_list, SGE_GLOBAL_NAME));
   a.start = ar.get<AR_start_time>();
   a.duration = ar.get<AR_duration>();
   a.is_reservation = true;
   a.is_advance_reservation = true;
   a.now = sge_get_gmt64();

   /* 
    * Current scheduler code expects all queue instances in a plain list. We use 
    * a copy of all queue instances that needs to be free'd explicitely after 
    * deciding about assignment. This is because assignment_release() sees 
    * queue_list only as a list pointer.
    */
   auto queue_copy = ocs::tom::ListPtr<ocs::tom::QU>::create("");
   a.queue_list = queue_copy.get();

   /* imagine qs is empty */
   sconf_set_qs_state(QS_STATE_EMPTY);

   /* redirect scheduler monitoring into answer list */
   if (verify_mode == AR_JUST_VERIFY) {
      DPRINTF("AR Verify Mode\n");
      a.monitor_alpp = &talp;
   }

   for (const auto cqueue : master_cqueue_list) {
      const char *cqname = cqueue.get<CQ_name>();
      const auto qinstance_list = cqueue.get<CQ_qinstances>();

      if (cqueue_match_static(cqname, &a) != DISPATCH_OK) {
         continue;
      }

      // @todo some of the check below (pe, acl/xacl are already done in cqueue_match_static(). Do we need to repeat them on qinstance layer?
      for (const auto qinstance : qinstance_list) {
         const char *cal_name;

         /* skip orphaned queues */
         if (qinstance_state_is_orphaned(qinstance)) {
            continue;
         }

         /* we only have to consider requested queues */
         if (ar_queue_request) {
            if (qref_list_cq_rejected(ar_queue_request, cqname,
                                      qinstance.get<QU_qhostname>(), master_hgroup_list)) {
               continue;
            }
         }

         /* we only have to consider queues containing the requested pe */
         if (ar_pe_request != nullptr) {
            bool found = false;

            for (const auto pe_ref : qinstance.get<QU_pe_list>()) {
               if (pe_name_is_matching(pe_ref.get<ST_name>(), ar_pe_request)) {
                  found = true;
                  break;
               }
            }
            if (!found) {
               continue;
            }

         }

         /* we only have to consider queues containing the requested checkpoint object */
         if (ar_ckpt_request != nullptr) {
            if (!qinstance.find_first_in<QU_ckpt_list, ST_name>(ar_ckpt_request)) {
               continue;
            }
         }

         /* sort out queue that are calendar disabled in requested time frame */
         if ((cal_name = qinstance.get<QU_calendar>()) != nullptr) {
            const auto cal_ep = master_cal_list.find_first<CAL_name>(cal_name);

            if (!calendar_open_in_time_frame(cal_ep, ar.get<AR_start_time>(), ar.get<AR_duration>())) {
               /* skip queue */
               answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_AR_QUEUEDISABLEDINTIMEFRAME,
                                       qinstance.get<QU_full_name>());
               continue;
            }
         }
         /* sort out queues where not all users have access */
         if (ar.get<AR_acl_list>()) {
            if (!sge_ar_have_users_access(alpp, ar, qinstance.get<QU_full_name>(),
                                          qinstance.get<QU_acl>(),
                                          qinstance.get<QU_xacl>(),
                                          master_userset_list)) {
               continue;
            }
         }

         a.queue_list.push_back(qinstance.clone());
      }
   }

   /*
    * split jobs
    */
   {
      /* initialize all job lists */
      for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
         splitted_job_lists[i] = nullptr;
      }
      splitted_job_lists[SPLIT_SUSPENDED] = &suspended_list;
      splitted_job_lists[SPLIT_RUNNING] = &running_list;

      // the jobs are copied, the master job list stays the data store's and is not replaced
      split_jobs(master_job_list, mconf_get_max_aj_instances(), splitted_job_lists, true);
   }

   /*
    * prepare resource schedule
    */
   prepare_resource_schedules(*running_list,
                              *suspended_list,
                              *master_pe_list, a.host_list, a.queue_list,
                              ocs::tom::List<ocs::tom::RQS>(), a.centry_list, a.acl_list,
                              a.hgrp_list, ocs::tom::List<ocs::tom::AR>(), false, a.now);

   /* free generated job lists */
   running_list.reset();
   suspended_list.reset();

   dummy_job->set<JB_execution_time>(ar.get<AR_start_time>());
   dummy_job->set<JB_deadline>(ar.get<AR_end_time>());
   job_set_hard_resource_list(*dummy_job, ar.get<AR_resource_list>().clone());
   job_set_hard_queue_list(*dummy_job, ar.get<AR_queue_list>().clone());
   job_set_master_hard_queue_list(*dummy_job, ar.get<AR_master_queue_list>().clone());
   dummy_job->set<JB_type>(ar.get<AR_type>());
   dummy_job->set<JB_checkpoint_name>(ar.get<AR_checkpoint_name>());
   dummy_job->set<JB_binding>(ar.get<AR_binding>().clone());

   if (ar.get<AR_pe>()) {
      dummy_job->set<JB_pe>(ar.get<AR_pe>());
      dummy_job->set<JB_pe_range>(ar.get<AR_pe_range>().clone(""));

      // CS-2864: qrsub -par overrides the allocation_rule of the requested PE, exactly as qsub -par
      // does for a job. The dummy job carries the rule in its global request set, which is where
      // assignment_init_pe() already looks for it, so the scheduler needs no knowledge of ARs here.
      const char *allocation_rule = ar.get<AR_allocation_rule>();
      if (allocation_rule != nullptr) {
         job_set_allocation_rule(*dummy_job, allocation_rule, JRS_SCOPE_GLOBAL);
      }

      result = sge_select_parallel_environment(&a, *master_pe_list);
      if (result == DISPATCH_OK) {
         ar.set<AR_granted_pe>(a.pe.get<PE_name>());
         // We keep a copy of the granted pe, as we need to use exactly this pe with all its settings
         // for debiting and undebiting - the original PE from the master pe list might change!
         ar.set<AR_pe_object>(a.pe.clone());
      }
   } else {
      result = sge_sequential_assignment(&a);
   }

   /* stop redirection of scheduler monitoring messages */
   if (verify_mode == AR_JUST_VERIFY) {
      /* copy error msgs from talp into alpp */
      answer_list_append_list(alpp, &talp);
      a.monitor_alpp = nullptr;

      if (result == DISPATCH_OK) {
         if (!a.pe) {
            answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_JOB_VERIFYFOUNDQ);
         } else {
            answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_JOB_VERIFYFOUNDSLOTS_I, a.slots);
         }
      } else {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_INFO, MSG_JOB_NOSUITABLEQ_S,
                                 MSG_JOB_VERIFYVERIFY);
      }
      /* ret has to be false in verify mode, otherwise the framework adds the object to the master list */
      ret = false;
   } else {
      if (result != DISPATCH_OK) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_JOB_NOSUITABLEQ_S, SGE_OBJ_AR);
         ret = false;
      } else {
         ar.set<AR_granted_slots>(std::move(a.gdil));
         ar_initialize_resource_booking(ar);

         ar_do_reservation(ar, true, gdi_session);
      }
   }


   /* stop dreaming */
   sconf_set_qs_state(QS_STATE_FULL);

   queue_copy.reset();
   a.queue_list = nullptr;
   master_pe_list.reset();
   master_exechost_list.reset();
   dummy_job.reset();

   assignment_release(&a);

   DRETURN(ret);
}

/**
 * @brief Do the reservation in the selected queue instances
 *
 * This function does the (un)reserveration in the selected parallel environment
 * and the selected queue instances
 *
 * @param ar ar object (AR)
 * @param incslots increase or decrease usage: true to book the slots, false to give them back
 * @param gdi_session the session the change belongs to
 *
 * @return 0
 *
 * @note MT-NOTE: ar_do_reservation() is not MT safe
 *
 * @see `rqs_add_job_utilization()`
 */
int
ar_do_reservation(const ocs::tom::Element<ocs::tom::AR> ar, const bool incslots, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   uint64_t start_time = ar.get<AR_start_time>();
   uint64_t duration = ar.get<AR_duration>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_exechost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();

   auto dummy_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job_set_hard_resource_list(*dummy_job, ar.get<AR_resource_list>().clone());
   job_set_hard_queue_list(*dummy_job, ar.get<AR_queue_list>().clone());

   const ocs::tom::Element<ocs::tom::EH> global_host_ep(host_list_locate(master_exechost_list, SGE_GLOBAL_NAME));

   // the instances the reservation holds, so that they are taken on the real hosts for the
   // reservation's window and a job scheduled outside the reservation does not take them too
   const auto granted_resources = ar.get<AR_granted_resources_list>();

   const char *granted_pe = ar.get<AR_granted_pe>();
   ocs::tom::Element<ocs::tom::PE> pe;
   if (granted_pe != nullptr) {
      // for debiting / undebiting we need to use the granted PE with the settings it had at the time of scheduling
      // esp. for undebiting use the same settings as for debiting;
      // therefore, we keep a copy of the PE in AR_pe_object
      pe = ar.get<AR_pe_object>();
      if (!pe) {
         CRITICAL(MSG_AR_HAS_NO_PEOBJECT_US, ar.get<AR_id>(), granted_pe);
#if defined(ENABLE_DEBUG_CHECKS)
         ocs::TerminationManager::trigger_abort();
#endif
         // In product build we continue without PE - this is the old behavior but should never happen
      }
   }

   int pe_slots = 0;
   bool is_master_task = true;
   const char *last_hostname = nullptr;
   for (const auto gdil_ep : ar.get<AR_granted_slots>()) {
      const char *queue_name = gdil_ep.get<JG_qname>();
      const auto queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name);

      if (!queue) {
         ERROR(MSG_JOB_UNABLE2FINDQOFJOB_S, queue_name);
         is_master_task = false;
         continue;
      }

      const char *queue_hostname = queue.get<QU_qhostname>();
      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, queue_hostname);

      int tmp_slots;
      if (incslots) {
         tmp_slots = gdil_ep.get<JG_slots>();
      } else {
         tmp_slots = -gdil_ep.get<JG_slots>();
      }
      pe_slots += tmp_slots;

      /* reserve global host */
      if (rc_add_job_utilization(gdil_ep, *dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                 global_host_ep, master_centry_list, tmp_slots,
                                 SGE_GLOBAL_NAME, start_time, duration, GLOBAL_TAG,
                                 false, is_master_task, do_per_host_booking,
                                 granted_resources) != 0) {
         /* this info is not spooled */
         sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0,
                       SGE_GLOBAL_NAME, nullptr, nullptr, global_host_ep, gdi_session);
      }

      /* reserve exec host */
      const ocs::tom::Element<ocs::tom::EH> host_ep(host_list_locate(master_exechost_list, queue_hostname));
      if (rc_add_job_utilization(gdil_ep, *dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                 host_ep, master_centry_list, tmp_slots, queue_hostname,
                                 start_time, duration, HOST_TAG, false, is_master_task, do_per_host_booking,
                                 granted_resources) != 0) {
         /* this info is not spooled */
         sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0,
                       queue_hostname, nullptr, nullptr, host_ep, gdi_session);
      }

      /* reserve queue instance */
      rc_add_job_utilization(gdil_ep, *dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                             queue, master_centry_list, tmp_slots, queue_name,
                             start_time, duration, QUEUE_TAG, false, is_master_task, do_per_host_booking,
                             granted_resources);

      qinstance_increase_qversion(queue);
      /* this info is not spooled */
      qinstance_add_event(queue, sgeE_QINSTANCE_MOD, gdi_session);
      is_master_task = false;
   }

   if (granted_pe != nullptr) {
      // The booking of slots into the PE's resource diagram needs to be done on the actual PE from the master pe list.
      const auto master_pe_list = ocs::DataStore::master_list_rw<SGE_TYPE_PE>();
      const auto master_pe = master_pe_list.find_first<PE_name>(granted_pe);
      if (!master_pe) {
         CRITICAL(MSG_OBJ_UNABLE2FINDPE_S, granted_pe);
#if defined(ENABLE_DEBUG_CHECKS)
         ocs::TerminationManager::trigger_abort();
#endif
      } else {
         utilization_add(master_pe.get<PE_resource_utilization>().front(), start_time,
                         duration, pe_slots, 0, 0, PE_TAG, granted_pe,
                         SCHEDULING_RECORD_ENTRY_TYPE_RESERVING, false, false, ocs::tom::ConstList<ocs::tom::ST>(),
                         ocs::tom::ConstList<ocs::tom::RESL>());
         // the event has to carry the master PE, which holds the booking, not the AR's copy
         sge_add_event(0, sgeE_PE_MOD, 0, 0, granted_pe, nullptr, nullptr, master_pe, gdi_session);
      }
   }

   dummy_job.reset();

   DRETURN(0);
}

/**
 * @brief Does ckpt change breake an ar
 *
 * This function tests if a modification of a ckpt list in a qinstance is
 * allowed according to the advance reservations.
 * Input parameters are: the advance reservation master list, the name of the
 * qinstance which sould be modified and the ST string list of ckpt
 * names which represents the new setting for the qinstance.
 * If there is no reservation for this qinstance-ckpt combination or if
 * the reservation would be still valid after the modification then
 * the function returns 'false". Otherwise 'true'
 *
 * @param ar_master_list advance reservation master list
 * @param answer_list answer list which will contain the reason why a modification is not valid
 * @param qinstance_name name of a qinstance <`cqname@hostname`>
 * @param ckpt_string_list ST list containing ckpt names
 *
 * @return modification would breake at least one ar false - no ar will be broken if the ckpt list is modified
 *
 * @note MT-NOTE: ar_get_string_from_event() is MT safe
 */
bool
ar_list_has_reservation_due_to_ckpt(const ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *qinstance_name,
                                    const ocs::tom::ConstList<ocs::tom::ST> ckpt_string_list) {
   DENTER(TOP_LAYER);

   for (const auto ar : ar_master_list) {
      const char *ckpt_string = ar.get<AR_checkpoint_name>();

      if (ckpt_string != nullptr &&
          ar.get<AR_granted_slots>().find_first<JG_qname>(qinstance_name)) {
         if (!ckpt_string_list.find_first<ST_name>(ckpt_string)) {
            ERROR(MSG_PARSE_MOD_REJECTED_DUE_TO_AR_SSU, ckpt_string, SGE_ATTR_CKPT_LIST, ar.get<AR_id>());
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * @brief Does pe change breake an ar
 *
 * This function tests if a modification of a pe list in a qinstance is
 * allowed according to the advance reservations.
 * Input parameters are: the advance reservation master list, the name of the
 * qinstance which should be modified and the ST string list of pe
 * names which represents the new setting for the qinstance.
 * If there is no reservation for this qinstance-ckpt combination or if
 * the reservation would be still valid after the modification then
 * the function returns 'false". Otherwise 'true'
 *
 * @param ar_master_list advance reservation master list
 * @param answer_list answer list which will contain the reason why a modification is not valid
 * @param qinstance_name name of a qinstance <`cqname@hostname`>
 * @param pe_string_list ST list containing pe names
 *
 * @return modification would breake at least one ar false - no ar will be broken if the ckpt list is modified
 *
 * @note MT-NOTE: ar_get_string_from_event() is MT safe
 */
bool
ar_list_has_reservation_due_to_pe(const ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  const char *qinstance_name, const ocs::tom::ConstList<ocs::tom::ST> pe_string_list) {
   DENTER(TOP_LAYER);

   for (const auto ar : ar_master_list) {
      const char *pe_string = ar.get<AR_pe>();

      if (pe_string != nullptr &&
          ar.get<AR_granted_slots>().find_first<JG_qname>(qinstance_name)) {
         if (!pe_string_list.find_first<ST_name>(pe_string)) {
            ERROR(MSG_PARSE_MOD_REJECTED_DUE_TO_AR_SSU, pe_string, SGE_ATTR_PE_LIST, ar.get<AR_id>());
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * @brief Does PE change violate AR
 *
 * This function tests if a modification of slots entry in a pe is
 * allowed according to the advance reservations.
 * Input parameters are: the advance reservation master list, the name of the
 * pe which should be modified and the new slots value which should
 * be set in the pe which might vialote the advance reservations in
 * the system
 * If there is no reservation for this pe or if the new slots setting
 * does not violate the advance reservations in the system then this
 * function returns 'false'. Otherwise 'true'
 *
 * @param ar_master_list master advance reservation list
 * @param answer_list answer list
 * @param pe_name pe name
 * @param new_slots new slots setting for pe with 'pe_name'
 *
 * @return modification would break the ar's currently known false - modification is valid
 *
 * @note MT-NOTE: ar_list_has_reservation_for_pe_with_slots() is MT safe
 */
bool
ar_list_has_reservation_for_pe_with_slots(const ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                          const char *pe_name, const uint32_t new_slots) {
   DENTER(TOP_LAYER);

   bool ret = false;
   uint32_t max_res_slots = 0;

   for (const auto ar : ar_master_list) {
      const char *pe_string = ar.get<AR_pe>();

      if (pe_name != nullptr && pe_string != nullptr && strcmp(pe_string, pe_name) == 0) {
         for (const auto gs : ar.get<AR_granted_slots>()) {
            uint32_t slots = gs.get<JG_slots>();

            max_res_slots += slots;
         }
      }
   }
   if (max_res_slots > new_slots) {
      ERROR(MSG_PARSE_MOD_REJECTED_DUE_TO_AR_PE_SLOTS_U, max_res_slots);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief add a consumable to an AR resource's (queue or host) complex_values
 * @tparam F EH_consumable_config_list or QU_consumable_config_list, the list field of the target's type
 * @param target the target queue or host (from the AR_reserved_queues or AR_reserved_hosts)
 * @param source  the source queue or host (from the master queue or host list)
 * @param cr complex object
 * @param name name of the complex
 * @param doubleval capacity to store in the target object
 * @param stringval capacity formatted as string
 */
template <auto F>
static void
ar_add_consumable(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> target,
                  const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> source,
                  const ocs::tom::ConstElement<ocs::tom::CE> cr, const char *name, const double doubleval,
                  const char *stringval) {
   DENTER(TOP_LAYER);

   // is the complex defined in the source objects complex_values at all?
   if (source.template find_first_in<F, CE_name>(name)) {
      // make sure that the ??_consumable_config_list exists in target (host or queue);
      // we might already have the consumable in the list (for hosts having multiple qinstances / the global host)
      const auto cclist = target.template get_or_create<F>("");
      const auto ccobj = cclist.template find_first<CE_name>(name);
      if (!ccobj) {
         // if it did not exist, create it
         auto new_obj = cr.clone();
         new_obj->set<CE_doubleval>(doubleval);
         new_obj->set<CE_stringval>(stringval);
         cclist.push_back(std::move(new_obj));
      } else {
         // if it already existed, add the doubleval to the existing value and re-create the stringval
         ccobj.template add<CE_doubleval>(doubleval);
         double sum = ccobj.template get<CE_doubleval>();
         DSTRING_STATIC(dstr, 512);
         ccobj.template set<CE_stringval>(sge_dstring_sprintf(&dstr, "%f", sum));
      }
   }
   DRETURN_VOID;
}

/**
 * @brief give the reserved host's copy of a resource map the instances the reservation holds
 *
 * The copy carries the granted amount and, without this, no instances at all, which would make
 * the scheduler match a job inside the reservation against a map it cannot name anything in.
 *
 * @param ar_host the AR's copy of the exec host
 * @param name    name of the resource map
 * @param taken   the instances the reservation holds on that host
 */
static void
ar_set_reserved_rsmap(const ocs::tom::Element<ocs::tom::EH> ar_host, const char *name,
                      const ocs::tom::ConstList<ocs::tom::RESL> taken) {
   DENTER(TOP_LAYER);

   const auto ar_cr = ar_host.find_first_in_rw<EH_consumable_config_list, CE_name>(name);
   if (ar_cr) {
      ar_cr.set<CE_resource_map_list>(taken.clone("reserved_rsmap"));
   }

   DRETURN_VOID;
}

/**
 * @brief record which instances of a resource map the reservation holds
 *
 * The reservation is granted an amount; these are the instances that amount is made of. They
 * go to two places, for two readers:
 *
 *   the granted resource list of the AR, which is what qrstat reports and what a job running
 *   inside the reservation has to be granted from
 *
 *   the reserved host's copy of the map, so that the copy the scheduler matches against inside
 *   the reservation describes the instances rather than only their number
 *
 * The instances are taken from the exec host as the reservation sees it, less what it has
 * already taken for another queue instance on the same host - a host appears in the granted
 * slots once per queue instance, and the amount is added up across them.
 *
 * @param ar                     the reservation being booked
 * @param previous               the granted resource list the reservation carried before, the
 *                               spooled one when qmaster is starting up, an empty handle otherwise
 * @param granted_resources_list the AR's granted resource list, appended to, created if empty
 * @param ar_host                the AR's copy of the exec host
 * @param master_host            the exec host itself
 * @param name                   name of the resource map
 * @param host_name              name of the host
 * @param amount                 how many instances the reservation holds here
 */
static void
ar_add_rsmap_ids(const ocs::tom::ConstElement<ocs::tom::AR> ar, const ocs::tom::ConstList<ocs::tom::GRU> previous,
                 ocs::tom::ListPtr<ocs::tom::GRU> &granted_resources_list,
                 const ocs::tom::Element<ocs::tom::EH> ar_host, const ocs::tom::ConstElement<ocs::tom::EH> master_host,
                 const char *name, const char *host_name,
                 const double amount) {
   DENTER(TOP_LAYER);

   const auto definition = master_host.find_first_in<EH_consumable_config_list, CE_name>(name);
   if (!definition || amount <= 0) {
      // the map is not configured on this host, so there is nothing to name
      DRETURN_VOID;
   }
   // What is not free over the window this reservation covers: the instances other
   // reservations hold and the ones jobs hold, both of which the exec host's resource diagram
   // records for the time they are held. A reservation starting long after a job ends is
   // therefore not blocked by what that job holds now.
   const auto rue = master_host.find_first_in<EH_resource_utilization, RUE_name>(name);
   const auto held = utilization_rsmap_max(rue, sge_get_gmt64(), ar.get<AR_start_time>(),
                                           ar.get<AR_duration>());

   auto gru = gru_list_search(*granted_resources_list, name, host_name);
   if (!gru) {
      // name is not nullptr here, the lookup of the definition above has found it
      if (!granted_resources_list) {
         granted_resources_list = ocs::tom::ListPtr<ocs::tom::GRU>::create("");
      }
      gru = granted_resources_list->emplace_back<GRU_name>(name);
      gru.set<GRU_host>(ocs::tom::Host(host_name));
      gru.set<GRU_type>(static_cast<uint32_t>(ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE));

      // The instances a reservation holds are spooled with it, and this function runs again for
      // every reservation when qmaster starts up. Take the ones it already held rather than
      // choosing afresh - a job running inside the reservation was granted from that set, and
      // a restart must not move the reservation to a different one underneath it.
      const auto prev_gru = gru_list_search(previous, name, host_name);
      if (prev_gru && static_cast<ocs::GrantedResources::Type>(prev_gru.get<GRU_type>()) ==
                      ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
         for (const auto prev_ep : prev_gru.get<GRU_resource_map_list>()) {
            const char *id = prev_ep.get<RESL_value>();
            // an instance the map no longer defines is dropped, and made up for below
            if (!definition.find_first_in<CE_resource_map_list, RESL_value>(id)) {
               continue;
            }
            const auto resl = gru.emplace_back_in<GRU_resource_map_list, RESL_value>(id);
            if (resl) {
               resl.set<RESL_amount>(prev_ep.get<RESL_amount>());
               const auto props = prev_ep.get<RESL_properties>();
               if (props) {
                  resl.set<RESL_properties>(props.clone("granted_properties"));
               }
            }
         }
      }
   }
   // a host appears in the granted slots once per queue instance, and the amounts of those
   // entries add up, so what is taken here is whatever the total is still short of
   gru.set<GRU_amount>(gru.get<GRU_amount>() + amount);

   const auto taken = gru.get<GRU_resource_map_list>();
   uint32_t have = 0;
   for (const auto have_ep : taken) {
      have += have_ep.get<RESL_amount>();
   }
   const auto want = static_cast<uint32_t>(gru.get<GRU_amount>());
   if (have >= want) {
      // nothing more to name, which is the normal case once the instances have been reused
      ar_set_reserved_rsmap(ar_host, name, taken);
      DRETURN_VOID;
   }
   const uint32_t needed = want - have;

   const auto selected = centry_rsmap_select_instances(definition, *held, taken, needed);
   if (!selected) {
      // the reservation was granted an amount the map cannot name instances for, so matching
      // and this booking disagree. Say so rather than leaving the reservation looking complete.
      WARNING(MSG_AR_CANNOTNAMERSMAPIDS_SSU, name, host_name, needed);
      DRETURN_VOID;
   }

   for (const auto sel_ep : *selected) {
      const char *id = sel_ep.get<RESL_value>();

      // the AR's granted resources, for qrstat and for a job running inside the reservation
      auto resl = gru.find_first_in_rw<GRU_resource_map_list, RESL_value>(id);
      if (!resl) {
         resl = gru.emplace_back_in<GRU_resource_map_list, RESL_value>(id);
         const auto defined_ep = definition.find_first_in<CE_resource_map_list, RESL_value>(id);
         const auto src_props = defined_ep ? defined_ep.get<RESL_properties>()
                                           : ocs::tom::ConstList<ocs::tom::CE>();
         if (src_props) {
            resl.set<RESL_properties>(src_props.clone("granted_properties"));
         }
      }
      resl.add<RESL_amount>(sel_ep.get<RESL_amount>());
   }

   ar_set_reserved_rsmap(ar_host, name, gru.get<GRU_resource_map_list>());

   DRETURN_VOID;
}

/**
 * @brief add a non consumable complex to an AR resource's (queue or host) complex_values
 * @tparam F EH_consumable_config_list or QU_consumable_config_list, the list field of the target's type
 * @param target the target queue or host (from the AR_reserved_queues or AR_reserved_hosts)
 * @param source  the source queue or host (from the master queue or host list)
 * @param name name of the complex
 */
template <auto F>
static void
ar_add_non_consumable(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> target,
                      const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> source,
                      const char *name) {
   DENTER(TOP_LAYER);

   // is the complex defined in the source objects complex_values at all?
   const auto cr = source.template find_first_in<F, CE_name>(name);
   if (cr) {
      // make sure that the ??_consumable_config_list exists in target (host or queue);
      // we might already have the complex entry in the list (for hosts having multiple qinstances / the global host)
      const auto cclist = target.template get_or_create<F>("");
      if (!cclist.template find_first<CE_name>(name)) {
         // if it does not yet exist, add it
         cclist.push_back(cr.clone());
      }
   }

   DRETURN_VOID;
}

/**
 * @brief fetch the amount of resources requested for an AR
 * @param ar the AR object
 * @param cr the complex definition (for default requests)
 * @param cr_name  the complex/request name
 * @param request_value receives the requested amount, or the default request, if there is one
 * @return true if the resource was requested or has a default request, false otherwise
 */
static bool
ar_get_request_or_default(const ocs::tom::ConstElement<ocs::tom::AR> ar, const ocs::tom::ConstElement<ocs::tom::CE> cr,
                          const char *cr_name, double &request_value) {
   DENTER(TOP_LAYER);

   bool ret = false;

   const auto request = ar.find_first_in<AR_resource_list, CE_name>(cr_name);
   if (request) {
      // resource was explicitly requested
      request_value = request.get<CE_doubleval>();
      ret = true;
   } else {
      // resource was not requested but there might be a default request, parsed once by qmaster (CS-459)
      if (cr.get<CE_defaultval>() != nullptr) {
         request_value = cr.get<CE_default_doubleval>();
         ret = true;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Build the AR's private copy of the queues and hosts it reserved
 *
 * The granted slot list only names queue instances. Booking against the live
 * cluster configuration would make the reservation follow later configuration
 * changes, so the AR gets its own reduced copies of the reserved queues
 * (`AR_reserved_queues`) and hosts (`AR_reserved_hosts`) and debits the
 * requested resources into those. From here on the AR is accounted against
 * this snapshot, not against the queues themselves.
 *
 * @param ar the advance reservation (`AR`) to initialize
 */
void
ar_initialize_resource_booking(const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);

   const auto gdil = ar.get<AR_granted_slots>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_exechost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   dstring buffer = DSTRING_INIT;

   // the reduced layouts of the reserved queues and hosts, built once for the life of the process
   const auto queue_layout = ocs::tom::layout<QU_qhostname, QU_qname, QU_full_name, QU_job_slots,
                                              QU_consumable_config_list, QU_resource_utilization, QU_message_list,
                                              QU_state>();
   const auto host_layout = ocs::tom::layout<EH_name, EH_consumable_config_list, EH_resource_utilization>();

   // these are the AR's AR_reserved_queues and AR_reserved_hosts lists
   // the AR_reserved_hosts list gets a global host element
   ocs::tom::ListPtr<ocs::tom::QU> queue_list;
   auto host_list = ocs::tom::ListPtr<ocs::tom::EH>::create("", host_layout);
   const auto global_host = host_list->emplace_back<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME));
   const auto master_global_host = master_exechost_list.find_first<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME));

   // what the reservation held before this rebuild, see ar_add_rsmap_ids()
   const auto previous_granted_resources = ar.get<AR_granted_resources_list>();

   ocs::tom::ListPtr<ocs::tom::GRU> granted_resources_list;
   bool is_master_queue = true;
   const char *last_hostname = nullptr;
   for (const auto gep : gdil) {
      const char *queue_name = gep.get<JG_qname>();
      const ocs::uti::c_ptr<char> cqueue_name{cqueue_get_name_from_qinstance(queue_name)};
      const char *host_name = gep.get<JG_qhostname>();

      // add the binding_touse information (copy from JG to GRU)
      const auto binding_to_use_list = gep.get<JG_binding_to_use>();
      ocs::GrantedResources::add_binding_to_use(granted_resources_list, host_name, binding_to_use_list);

      // create queue with slot booking
      if (!queue_list) {
         queue_list = ocs::tom::ListPtr<ocs::tom::QU>::create("", queue_layout);
      }
      const auto queue = queue_list->emplace_back<QU_full_name>(queue_name);
      queue.set<QU_qname>(cqueue_name.get());
      queue.set<QU_qhostname>(ocs::tom::Host(host_name));
      uint32_t slots = gep.get<JG_slots>();
      queue.set<QU_job_slots>(slots);

      const auto master_cqueue = master_cqueue_list.find_first<CQ_name>(cqueue_name.get());
      const auto master_queue = cqueue_locate_qinstance(master_cqueue, host_name);

      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, host_name);

      // get or create host (one host can appear multiple times in gdil)
      auto host = host_list->find_first<EH_name>(ocs::tom::Host(host_name));
      if (!host) {
         host = host_list->emplace_back<EH_name>(ocs::tom::Host(host_name));
      }
      const auto master_host = master_exechost_list.find_first<EH_name>(ocs::tom::Host(host_name));

      // now we have queue, host and global host objects, fill in the granted complex_values

      // loop over the defined complex values and add them to the queue and hosts if
      // - they have been requested
      // - or they have a default request
      // - they are defined on the queue, host, global layer
      // - non-requested exclusive? no. they will have effect in the booking of the AR itself
      for (const auto cr : master_centry_list) {
         const char *cr_name = cr.get<CE_name>();
         uint32_t consumable = cr.get<CE_consumable>();
         if (consumable == CONSUMABLE_NO) {
            // non-consumable, add the definition from the master object (if defined on the layer)
            ar_add_non_consumable<EH_consumable_config_list>(global_host, master_global_host, cr_name);
            ar_add_non_consumable<EH_consumable_config_list>(host, master_host, cr_name);
            ar_add_non_consumable<QU_consumable_config_list>(queue, master_queue, cr_name);
         } else {
            double request_or_default = 0.0;
            bool requested = ar_get_request_or_default(ar, cr, cr_name, request_or_default);
            if (requested) {
               // there is something to book
               // it is a consumable, add the requested capacity to the booking (if defined on the layer)
               double doubleval;
               if (consumable == CONSUMABLE_YES) {
                  doubleval = request_or_default * slots;
               } else if (consumable == CONSUMABLE_JOB) {
                  if (!is_master_queue) {
                     continue;
                  }
                  doubleval = request_or_default;
               } else { // CONSUMABLE_HOST
                  if (!do_per_host_booking) {
                     continue;
                  }
                  doubleval = request_or_default;
               }
               const char *stringval = sge_dstring_sprintf(&buffer, "%f", doubleval);
               ar_add_consumable<EH_consumable_config_list>(global_host, master_global_host, cr, cr_name, doubleval,
                                                            stringval);
               ar_add_consumable<EH_consumable_config_list>(host, master_host, cr, cr_name, doubleval, stringval);
               ar_add_consumable<QU_consumable_config_list>(queue, master_queue, cr, cr_name, doubleval, stringval);

               // A resource map is held as particular instances, not only as an amount. Which
               // ones is decided here, for the same reason a job's are decided when it is
               // booked: nothing else knows what the reservation took, and a job which later
               // runs inside it has to be granted from that set rather than from whatever the
               // host has free (CS-2730).
               //
               // A map is looked for on both layers because it can be configured on either,
               // and a job takes it from the exec host when it is there and from the global
               // host when it is not (see gru_list_add_request()). Each call does nothing when
               // the map is not configured on the layer it is given.
               if (static_cast<ocs::CEntry::Type>(cr.get<CE_valtype>()) == ocs::CEntry::Type::RSMAP) {
                  ar_add_rsmap_ids(ar, previous_granted_resources, granted_resources_list,
                                   global_host, master_global_host, cr_name, SGE_GLOBAL_NAME,
                                   doubleval);
                  ar_add_rsmap_ids(ar, previous_granted_resources, granted_resources_list,
                                   host, master_host, cr_name, host_name, doubleval);
               }
            }
         }
      }

      /* initialize QU_state */
      if (qinstance_state_is_ambiguous(master_queue)) {
         ar.add<AR_qi_errors>(1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_AMBIGUOUS));
         qinstance_set_error(queue, QI_AMBIGUOUS, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_alarm(master_queue)) {
         ar.add<AR_qi_errors>(1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_ALARM));
         qinstance_set_error(queue, QI_ALARM, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_suspend_alarm(master_queue)) {
         ar.add<AR_qi_errors>(1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_SUSPEND_ALARM));
         qinstance_set_error(queue, QI_SUSPEND_ALARM, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_manual_disabled(master_queue)) {
         ar.add<AR_qi_errors>(1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_DISABLED));
         qinstance_set_error(queue, QI_DISABLED, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_unknown(master_queue)) {
         ar.add<AR_qi_errors>(1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_UNKNOWN));
         qinstance_set_error(queue, QI_UNKNOWN, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_error(master_queue)) {
         ar.add<AR_qi_errors>(1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_ERROR));
         qinstance_set_error(queue, QI_ERROR, sge_dstring_get_string(&buffer), true);
      }

      is_master_queue = false;
   }

   for (const auto queue : *queue_list) {
      // make sure all complex attributes are properly filled in
      centry_list_fill_config(queue.get<QU_consumable_config_list>(), master_centry_list);
      // ensure availability of implicit slot request
      qinstance_set_conf_slots_used(queue);
      // initialize QU_resource_utilization
      qinstance_debit_consumable(queue, ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::PE>(),
                                 master_centry_list, 0, true, true, nullptr);
   }
   for (const auto host : *host_list) {
      // make sure all complex attributes are properly filled in
      centry_list_fill_config(host.get<EH_consumable_config_list>(), master_centry_list);
      debit_host_consumable(ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::JAT>(),
                            *granted_resources_list, ocs::tom::ConstElement<ocs::tom::PE>(), host,
                            master_centry_list, 0, true, true, nullptr);
   }

   ar.set<AR_granted_resources_list>(std::move(granted_resources_list));
   ar.set<AR_reserved_queues>(std::move(queue_list));
   ar.set<AR_reserved_hosts>(std::move(host_list));

   sge_dstring_free(&buffer);

   DRETURN_VOID;
}

/**
 * @brief Removes all jobs of an AR
 *
 * The function deletes all jobs (and tasks) requested the advance
 * reservation
 *
 * @param ar_id advance reservation id
 * @param monitor monitoring structure
 * @param forced see the brief above
 * @param gdi_session the session the change belongs to
 *
 * @return true if all jobs of the AR could be removed
 *
 * @note MT-NOTE: sge_ar_remove_all_jobs() is not MT safe
 */
bool
sge_ar_remove_all_jobs(uint32_t ar_id, int forced, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
   auto nextjep = master_job_list.front();
   while (const auto jep = nextjep) {
      uint32_t task_number;
      uint32_t start = std::min(job_get_smallest_unenrolled_task_id(jep), job_get_smallest_enrolled_task_id(jep));
      uint32_t end = MAX(job_get_biggest_unenrolled_task_id(jep), job_get_biggest_enrolled_task_id(jep));

      // the job can be removed from the master job list below, so the next one is fetched first
      nextjep = jep.next();
      if (jep.get<JB_ar>() != ar_id) {
         continue;
      }

      DPRINTF("removing job %d\n", jep.get<JB_job_number>());
      DPRINTF(" ----> task_start = %d, task_end = %d\n", start, end);

      for (task_number = start;
           task_number <= end;
           task_number++) {

         if (job_is_ja_task_defined(jep, task_number)) {

            if (job_is_enrolled(jep, task_number)) {
               /* delete all enrolled pending tasks */
               DPRINTF("removing enrolled task %d.%d\n", jep.get<JB_job_number>(), task_number);
               const auto tmp_task = jep.find_first_in_rw<JB_ja_tasks, JAT_task_number>(task_number);

               /* 
                * if task is already in status deleted and was signaled
                * only recently and deletion is not forced, do nothing
                */
               if (ISSET(tmp_task.get<JAT_status>(), JFINISHED)) {
                  continue;
               }

               if (forced) {
                  sge_commit_job(jep, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED_EE,
                                 COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
               } else {
                  if (!ISSET(tmp_task.get<JAT_state>(), JDELETED)) {
                     // @todo CS-1093 we simply mark the job as deleted. When the next job report comes in for it
                     //       this will trigger a "... reports running job ... that was not supposed to be there..."
                     //       message (MSG_JOB_REPORTRUNFALSE_SUUSS) and the job will be signalled then.
                     //       Shouldn't we better signal the job here?
                     job_mark_job_as_deleted(jep, tmp_task);
                  }
                  ret = false;
               }
            } else {
               /* delete all unenrolled running tasks */
               DPRINTF("removing unenrolled task %d.%d\n", jep.get<JB_job_number>(), task_number);
               const auto tmp_task = job_get_ja_task_template_pending(jep, task_number);

               sge_commit_job(jep, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED,
                              COMMIT_NO_SPOOLING | COMMIT_UNENROLLED_TASK | COMMIT_NEVER_RAN,
                              monitor, gdi_session);
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Checks if the given calendar
 *
 * Iteraters over all existing Advance Reservations reserved queues and verifies
 * that the new calender does not invalidate the AR if the queue was reserved
 *
 * @param answer_list answer list
 * @param qinstance_name qinstance name the calendar was configured
 * @param cal_ep the calendar object (CAL)
 * @param master_ar_list master AR list
 *
 * @return true if conflicts false if OK
 *
 * @note MT-NOTE: sge_ar_list_conflicts_with_calendar() is MT safe
 */
bool
sge_ar_list_conflicts_with_calendar(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *qinstance_name,
                                    const ocs::tom::ConstElement<ocs::tom::CAL> cal_ep,
                                    const ocs::tom::ConstList<ocs::tom::AR> master_ar_list) {
   DENTER(TOP_LAYER);

   for (const auto ar : master_ar_list) {
      if (ar.get<AR_granted_slots>().find_first<JG_qname>(qinstance_name)) {
         uint64_t start_time = ar.get<AR_start_time>();
         uint64_t duration = ar.get<AR_duration>();

         if (!calendar_open_in_time_frame(cal_ep, start_time, duration)) {
            ERROR(MSG_PARSE_MOD2_REJECTED_DUE_TO_AR_SSU, cal_ep.get<CAL_name>(), SGE_ATTR_CALENDAR,
                  ar.get<AR_id>());
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * @brief Set ar in running state
 *
 * Sets the AR state to running. A running state can result in error state
 * if one of the reserved queues is unable to run a job. This is covered by the
 * function
 *
 * @param ar advance reservation object (AR)
 *
 * @note MT-NOTE: sge_ar_state_set_running() is MT safe
 *
 * @see #sge_ar_state_set_exited, #sge_ar_state_set_deleted, #sge_ar_state_set_waiting
 */
void
sge_ar_state_set_running(const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);

   uint32_t old_state = ar.get<AR_state>();

   if (old_state == AR_DELETED || old_state == AR_EXITED) {
      DRETURN_VOID;
   }

   if (sge_ar_has_errors(ar)) {
      ar.set<AR_state>(AR_ERROR);
      if (old_state != AR_WARNING && old_state != ar.get<AR_state>()) {
         /* state change from "running" to "error" */
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_UNSATISFIED, "AR resources unsatisfied", sge_get_gmt64());
         sge_ar_send_mail(ar, MAIL_AT_ABORT);
      } else if (old_state != ar.get<AR_state>()) {
         /* state change from "warning" to "error" */
         sge_ar_send_mail(ar, MAIL_AT_ABORT);
      }
   } else {
      ar.set<AR_state>(AR_RUNNING);
      if (old_state != AR_WAITING && old_state != ar.get<AR_state>()) {
         /* state change from "error" to "running" */
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_OK, "AR resources satisfied", sge_get_gmt64());
         sge_ar_send_mail(ar, MAIL_AT_ABORT);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Set ar in running state
 *
 * Sets the AR state to waiting. A waiting state can result in warning state
 * if one of the reserved queues is unable to run a job. This is covered by the
 * function
 *
 * @param ar advance reservation object (AR)
 *
 * @note MT-NOTE: sge_ar_state_set_waiting() is MT safe
 *
 * @see #sge_ar_state_set_exited, #sge_ar_state_set_deleted, #sge_ar_state_set_running
 */
void
sge_ar_state_set_waiting(const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);

   uint32_t old_state = ar.get<AR_state>();

   if (old_state == AR_DELETED || old_state == AR_EXITED) {
      DRETURN_VOID;
   }

   if (sge_ar_has_errors(ar)) {
      ar.set<AR_state>(AR_WARNING);
      if (old_state != ar.get<AR_state>()) {
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_UNSATISFIED, "AR resources unsatisfied", sge_get_gmt64());
      }
   } else {
      ar.set<AR_state>(AR_WAITING);
      if (old_state != ar.get<AR_state>()) {
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_OK, "AR resources satisfied", sge_get_gmt64());
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Sets AR into deleted state
 *
 * Sets the AR state to deleted
 *
 * @param ar advance reservation object (AR)
 *
 * @note MT-NOTE: sge_ar_state_set_deleted() is MT safe
 *
 * @see #sge_ar_state_set_exited, #sge_ar_state_set_waiting, #sge_ar_state_set_running
 */
void
sge_ar_state_set_deleted(const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);
   ar.set<AR_state>(AR_DELETED);
   DRETURN_VOID;
}

/**
 * @brief Sets AR into exited state
 *
 * Sets the AR state to deleted
 *
 * @param ar advance reservation object (AR)
 *
 * @note MT-NOTE: sge_ar_state_set_exited() is MT safe
 *
 * @see #sge_ar_state_set_deleted, #sge_ar_state_set_waiting, #sge_ar_state_set_running
 */
void
sge_ar_state_set_exited(const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);
   ar.set<AR_state>(AR_EXITED);
   DRETURN_VOID;
}

/**
 * @brief Set/unset all ARs reserved in a specific queue
 *
 * The function sets/unsets all ARs that reserved in a queue in the error state and
 * generates the error messages for qrstat -explain
 *
 * @param ar_list master advance reservation list
 * @param qname queue name
 * @param error_type error type
 * @param set_error set or unset
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: sge_ar_list_set_error_state() is MT safe
 */
void
sge_ar_list_set_error_state(const ocs::tom::List<ocs::tom::AR> ar_list, const char *qname, const uint32_t error_type,
                            const bool set_error, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   dstring buffer = DSTRING_INIT;

   for (const auto ar : ar_list) {
      const auto qinstance = ar.get<AR_reserved_queues>().find_first<QU_full_name>(qname);

      if (qinstance) {
         uint32_t old_errors = ar.get<AR_qi_errors>();
         uint32_t new_errors;

         if (set_error) {
            new_errors = old_errors + 1;
            sge_dstring_sprintf(&buffer, MSG_AR_RESERVEDQUEUEHASERROR_SS, qname,
                                qinstance_state_as_string(error_type));
         } else {
            new_errors = old_errors - 1;
         }
         ar.set<AR_qi_errors>(new_errors);

         qinstance_set_error(qinstance, error_type, sge_dstring_get_string(&buffer), set_error);

         /* update states */
         if (old_errors == 0 || new_errors == 0) {
            if ((ar.get<AR_state>() == AR_RUNNING || ar.get<AR_state>() == AR_ERROR)) {
               sge_ar_state_set_running(ar);
            } else {
               sge_ar_state_set_waiting(ar);
            }
            /* this info is not spooled */
            sge_add_event(0, sgeE_AR_MOD, ar.get<AR_id>(), 0,
                          nullptr, nullptr, nullptr, ar, gdi_session);
         }
      }
   }

   sge_dstring_free(&buffer);
   DRETURN_VOID;
}

/**
 * @brief Send mail for advance reservation state change
 *
 * Create and send mail for a specific event
 *
 * @param ar advance reservation object (AR)
 * @param type event type
 *
 * @note MT-NOTE: sge_ar_send_mail() is MT safe
 */
static void
sge_ar_send_mail(const ocs::tom::ConstElement<ocs::tom::AR> ar, const int type) {
   DENTER(TOP_LAYER);

   dstring buffer = DSTRING_INIT;
   dstring subject = DSTRING_INIT;
   dstring body = DSTRING_INIT;
   uint32_t ar_id;
   const char *ar_name;
   const char *mail_type = nullptr;

   if (!VALID(type, ar.get<AR_mail_options>())) {
      sge_dstring_append_mailopt(&buffer, type);
      DPRINTF("mailopt %s was not requested\n", sge_dstring_get_string(&buffer));
      sge_dstring_free(&subject);
      sge_dstring_free(&body);
      sge_dstring_free(&buffer);
      DRETURN_VOID;
   }

   ar_id = ar.get<AR_id>();
   ar_name = ar.get<AR_name>();

   switch (type) {
      case MAIL_AT_BEGINNING:
         sge_ctime64(ar.get<AR_start_time>(), &buffer);
         sge_dstring_sprintf(&subject, MSG_MAIL_ARSTARTEDSUBJ_US,
                             ar_id, ar_name ? ar_name : "none");
         sge_dstring_sprintf(&body, MSG_MAIL_ARSTARTBODY_USSS,
                             ar_id, ar_name ? ar_name : "none", ar.get<AR_owner>(),
                             sge_dstring_get_string(&buffer));
         mail_type = MSG_MAIL_TYPE_ARSTART;
         break;
      case MAIL_AT_EXIT:
         if (ar.get<AR_state>() == AR_DELETED) {
            sge_ctime64(sge_get_gmt64(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_ARDELETEDSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_ARDELETETBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", ar.get<AR_owner>(),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_ARDELETE;
         } else {
            sge_ctime64(ar.get<AR_end_time>(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_AREXITEDSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_AREXITBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", ar.get<AR_owner>(),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_AREND;
         }
         break;
      case MAIL_AT_ABORT:
         if (ar.get<AR_state>() == AR_ERROR) {
            sge_ctime64(sge_get_gmt64(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_ARERRORSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_ARERRORBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", ar.get<AR_owner>(),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_ARERROR;
         } else {
            sge_ctime64(sge_get_gmt64(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_AROKSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_AROKBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", ar.get<AR_owner>(),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_AROK;
         }
         break;
      default:
         /* should never happen */
         break;
   }

   mail_to_users(QMASTER, ar.get<AR_mail_list>(), sge_dstring_get_string(&subject), sge_dstring_get_string(&body),
             mail_type);

   sge_dstring_free(&buffer);
   sge_dstring_free(&subject);
   sge_dstring_free(&body);

   DRETURN_VOID;
}

/**
 * @brief Check whether a modification of the complex_values of a qinstance breaks an advance reservation
 *
 * Check if the modification of the complex_values of a qinstance
 * whould break existing advance reservations
 *
 * @param ar_master_list master AR list
 * @param answer_list answer list
 * @param qinstance qinstance with the new settings, its consumable actual list is rebuilt here
 * @param ce_master_list master centry list
 *
 * @return true if the modification is not allowed, false if the modification is allowed
 *
 * @note MT-NOTE: ar_list_has_reservation_due_to_qinstance_complex_attr() is
 *       MT safe
 */
bool
ar_list_has_reservation_due_to_qinstance_complex_attr(const ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                      const ocs::tom::Element<ocs::tom::QU> qinstance,
                                                      const ocs::tom::ConstList<ocs::tom::CE> ce_master_list) {
   DENTER(TOP_LAYER);

   for (const auto ar : ar_master_list) {
      const char *qinstance_name = qinstance.get<QU_full_name>();

      const auto gs = ar.get<AR_granted_slots>().find_first<JG_qname>(qinstance_name);
      if (gs) {
         for (const auto request : ar.get<AR_resource_list>()) {
            const char *ce_name = request.get<CE_name>();
            const auto ce = ce_master_list.find_first<CE_name>(ce_name);
            bool is_consumable = (ce.get<CE_consumable>() > 0) ? true : false;

            if (!is_consumable) {
               char text[2048];
               uint32_t slots = gs.get<JG_slots>();
               const auto current = qinstance.find_first_in<QU_consumable_config_list, CE_name>(ce_name);
               if (current) {
                  const auto copy = current.clone();
                  copy->set<CE_relop>(ce.get<CE_relop>());
                  copy->set<CE_pj_doubleval>(copy->get<CE_doubleval>());
                  copy->set<CE_pj_stringval>(copy->get<CE_stringval>());

                  if (compare_complexes(slots, request, *copy, text, false, true) == 0) {
                     ERROR(MSG_QUEUE_MODCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     DRETURN(true);
                  }
               }
            }
         }

         /* now it gets expensive. Before we can start the check at first we have to build the
            consumable config list. */
         qinstance_reinit_consumable_actual_list(qinstance, answer_list);
         const auto rue_list = qinstance.get<QU_resource_utilization>();

         for (const auto rue : rue_list) {
            const char *ce_name = rue.get<RUE_name>();
            const auto ce = ce_master_list.find_first<CE_name>(ce_name);
            bool is_consumable = (ce.get<CE_consumable>() > 0) ? true : false;

            if (is_consumable) {
               const auto rde_list = rue.get<RUE_utilized>();
               const auto cv = qinstance.find_first_in<QU_consumable_config_list, CE_name>(ce_name);

               if (!cv) {
                  ERROR(MSG_QUEUE_MODNOCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  DRETURN(true);
               } else {
                  double configured = cv.get<CE_doubleval>();

                  for (const auto rde : rde_list) {
                     double amount = rde.get<RDE_amount>();

                     if (amount > configured) {
                        ERROR(MSG_QUEUE_MODCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                        answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                        DRETURN(true);
                     }
                  }
               }
            }
         }
      }
   }
   DRETURN(false);
}

/**
 * @brief Check whether a modification of the complex_values of a host breaks an advance reservation
 *
 *  Check if the modification of the complex_values of a host
 *  whould break existing advance reservations.
 *
 * @param ar_master_list master AR list
 * @param answer_list AN list
 * @param host host with the new settings
 * @param ce_master_list master centry list
 *
 * @return true if the modification is not allowed, false if the modification is allowed
 *
 * @note MT-NOTE: ar_list_has_reservation_due_to_host_complex_attr() is MT
 *       safe
 */
bool
ar_list_has_reservation_due_to_host_complex_attr(const ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                 const ocs::tom::ConstElement<ocs::tom::EH> host,
                                                 const ocs::tom::ConstList<ocs::tom::CE> ce_master_list) {
   DENTER(TOP_LAYER);

   const char *hostname = host.get<EH_name>();

   for (const auto ar : ar_master_list) {
      for (const auto gs : ar.get<AR_granted_slots>()) {
         const char *gh = gs.get<JG_qhostname>();

         if (!sge_hostcmp(gh, hostname)) {
            const auto rue_list = host.get<EH_resource_utilization>();

            for (const auto request : ar.get<AR_resource_list>()) {
               const char *ce_name = request.get<CE_name>();
               const auto ce = ce_master_list.find_first<CE_name>(ce_name);
               bool is_consumable = (ce.get<CE_consumable>() > 0) ? true : false;

               if (!is_consumable) {
                  char text[2048];
                  uint32_t slots = gs.get<JG_slots>();
                  const auto old_current = host.find_first_in<EH_consumable_config_list, CE_name>(ce_name);
                  if (old_current) {
                     const auto current = old_current.clone();
                     current->set<CE_relop>(ce.get<CE_relop>());
                     current->set<CE_pj_doubleval>(current->get<CE_doubleval>());
                     current->set<CE_pj_stringval>(current->get<CE_stringval>());

                     if (compare_complexes(slots, request, *current, text, false, true) == 0) {
                        ERROR(MSG_QUEUE_MODCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                        answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                        DRETURN(true);
                     }
                  }
               }
            }
            for (const auto rue : rue_list) {
               const char *ce_name = rue.get<RUE_name>();
               const auto ce = ce_master_list.find_first<CE_name>(ce_name);
               bool is_consumable = (ce.get<CE_consumable>() > 0) ? true : false;

               if (is_consumable) {
                  const auto rde_list = rue.get<RUE_utilized>();
                  const auto cv = host.find_first_in<EH_consumable_config_list, CE_name>(ce_name);

                  if (!cv) {
                     ERROR(MSG_QUEUE_MODNOCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     DRETURN(true);
                  } else {
                     double configured = cv.get<CE_doubleval>();

                     for (const auto rde : rde_list) {
                        double amount = rde.get<RDE_amount>();

                        if (amount > configured) {
                           ERROR(MSG_QUEUE_MODCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                           answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                           DRETURN(true);
                        }
                     }
                  }
               }
            }
         }
      }
   }
   DRETURN(false);
}
