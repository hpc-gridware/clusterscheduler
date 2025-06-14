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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "uti/sge_bitfield.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_parse_num_par.h"
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
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/ocs_DataStore.h"

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

typedef struct {
   u_long32 ar_id;
   bool changed;
   pthread_mutex_t ar_id_mutex;
} ar_id_t;

ar_id_t ar_id_control = {0, false, PTHREAD_MUTEX_INITIALIZER};

static bool
ar_reserve_queues(lList **alpp, lListElem *ar, u_long64 gdi_session);

static u_long32
sge_get_ar_id(monitoring_t *monitor);

static u_long32
guess_highest_ar_id();

static void
sge_ar_send_mail(lListElem *ar, int type);

void
ar_initialize_timer(lList **answer_list, monitoring_t *monitor, u_long64 gdi_session) {
   lListElem *ar, *next_ar;
   u_long64 now = sge_get_gmt64();

   DENTER(TOP_LAYER);

   lList *ar_master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_AR);

   next_ar = lFirstRW(ar_master_list);

   while ((ar = next_ar)) {
      te_event_t ev = nullptr;

      next_ar = lNextRW(ar);

      if (now < lGetUlong64(ar, AR_start_time)) {
         sge_ar_state_set_waiting(ar);

         ev = te_new_event(lGetUlong64(ar, AR_start_time), TYPE_AR_EVENT,
                           ONE_TIME_EVENT, lGetUlong(ar, AR_id), AR_RUNNING, nullptr);
         te_add_event(ev);
         te_add_event(ev);
         te_free_event(&ev);

      } else if (now < lGetUlong64(ar, AR_end_time)) {
         sge_ar_state_set_running(ar);

         ev = te_new_event(lGetUlong64(ar, AR_end_time), TYPE_AR_EVENT,
                           ONE_TIME_EVENT, lGetUlong(ar, AR_id), AR_EXITED, nullptr);
         te_add_event(ev);
         te_free_event(&ev);
      } else {
         dstring buffer = DSTRING_INIT;
         u_long32 ar_id = lGetUlong(ar, AR_id);

         sge_ar_state_set_running(ar);

         sge_ar_remove_all_jobs(ar_id, 1, monitor, gdi_session);

         ar_do_reservation(ar, false, gdi_session);

         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_TERMINATED,
                                        "end time of AR reached",
                                        now);
         ocs::ReportingFileWriter::create_ar_acct_records(nullptr, ar, now);

         sge_dstring_sprintf(&buffer, sge_u32, ar_id);

         lRemoveElem(ar_master_list, &ar);

         spool_delete_object(answer_list, spool_get_default_context(),
                             SGE_TYPE_AR, sge_dstring_get_string(&buffer), true);

         sge_dstring_free(&buffer);
      }
   }
   DRETURN_VOID;
}

/****** sge_advance_reservation_qmaster/ar_mod() *******************************
*  NAME
*     ar_mod() -- gdi callback function for adding modifing advance reservations
*
*  SYNOPSIS
*     int ar_mod(sge_gdi_ctx_class_t *ctx, lList **alpp, lListElem *new_ar, 
*     lListElem *ar, int add, const char *ruser, const char *rhost, 
*     gdi_object_t *object, int sub_command, monitoring_t *monitor) 
*
*  FUNCTION
*     This function is called from the framework that
*     add/modify/delete generic gdi objects.
*     The purpose of this function is it to add new advance reservation
*     objects.
*     Modifing is currently not supported.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - gdi context pointer
*     lList **alpp             - the answer_list
*     lListElem *new_ar        - if a new ar object will be created by this
*                                function, then new_ar is a newly initialized
*                                CULL object.
*     lListElem *ar            - a reduced ar object that contains all of the
*                                requested values
*     int add                  - 1 for add requests
*                                0 for mod requests
*     const char *ruser        - username who invoked this GDI request
*     const char *rhost        - hostname of where the GDI request was invoked
*     gdi_object_t *object     - structure of the GDI framework that contains
*                                additional informations to perform the request
*     int sub_command          - GDI sub command
*     monitoring_t *monitor    - monitoring structure
*
*  RESULT
*     int - 0 on success
*           STATUS_EUNKNOWN if an error occurred
*           STATUS_NOTOK_DOAGAIN if a temporary error
*
*  NOTES
*     MT-NOTE: ar_mod() is not MT safe 
*******************************************************************************/
int ar_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_ar, lListElem *ar, int add, const char *ruser,
           const char *rhost, gdi_object_t *object, ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_command, monitoring_t *monitor) {
   u_long32 ar_id;
   u_long32 max_advance_reservations = mconf_get_max_advance_reservations();
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_ckpt_list = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);

   DENTER(TOP_LAYER);

   if (!ar_validate(ar, alpp, true, false, master_cqueue_list, master_hgroup_list, master_centry_list, master_ckpt_list,
                    master_pe_list, master_userset_list)) {
      goto ERROR;
   }

   if (add) {
      /* get new ar ids until we find one that is not yet used */
      do {
         ar_id = sge_get_ar_id(monitor);
      } while (ar_list_locate(master_ar_list, ar_id));
      lSetUlong(new_ar, AR_id, ar_id);
      /*
      ** set the owner of new_ar, don't overwrite it with
      ** attr_mod_str(alpp, ar, new_ar, AR_owner, object->object_name);
      */
      lSetString(new_ar, AR_owner, ruser);
      lSetString(new_ar, AR_group, component_get_groupname());
   } else {
      ERROR(MSG_NOTYETIMPLEMENTED_S, "advance reservation modification");
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      goto ERROR;
   }

   if (max_advance_reservations > 0 &&
       max_advance_reservations <= lGetNumberOfElem(master_ar_list)) {
      ERROR(MSG_AR_MAXARSPERCLUSTER_U, max_advance_reservations);
      answer_list_add(alpp, SGE_EVENT, STATUS_NOTOK_DOAGAIN, ANSWER_QUALITY_ERROR);
      goto DOITAGAIN;
   }

   /*    AR_name, SGE_STRING */
   attr_mod_zerostr(ar, new_ar, AR_name, object->object_name);
   /*   AR_account, SGE_STRING */
   attr_mod_zerostr(ar, new_ar, AR_account, object->object_name);
   /*   AR_submission_time, SGE_ULONG */
   lSetUlong64(new_ar, AR_submission_time, sge_get_gmt64());
   /*   AR_start_time, SGE_ULONG          required */
   attr_mod_ulong64(ar, new_ar, AR_start_time, object->object_name);
   /*   AR_end_time, SGE_ULONG            required */
   attr_mod_ulong64(ar, new_ar, AR_end_time, object->object_name);
   /*   AR_duration, SGE_ULONG */
   attr_mod_ulong64(ar, new_ar, AR_duration, object->object_name);
   /*   AR_verify, SGE_ULONG              just verify the reservation or final case */
   attr_mod_ulong(ar, new_ar, AR_verify, object->object_name);
   /*   AR_error_handling, SGE_ULONG      how to deal with soft and hard exceptions */
   attr_mod_ulong(ar, new_ar, AR_error_handling, object->object_name);
   /*   AR_state, SGE_ULONG               state of the AR */
   lSetUlong(new_ar, AR_state, AR_WAITING);
   /*   AR_checkpoint_name, SGE_STRING    Named checkpoint */
   attr_mod_zerostr(ar, new_ar, AR_checkpoint_name, object->object_name);
   /*   AR_resource_list, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_resource_list, AR_name, ar, cmd, sub_command, SGE_ATTR_COMPLEX_VALUES, SGE_OBJ_AR, 0,
                     nullptr);
   /*   AR_queue_list, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_queue_list, AR_name, ar, cmd, sub_command, SGE_ATTR_QUEUE_LIST, SGE_OBJ_AR, 0, nullptr);
   /*   AR_mail_options, SGE_ULONG   */
   attr_mod_ulong(ar, new_ar, AR_mail_options, object->object_name);
   /*   AR_mail_list, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_mail_list, AR_name, ar, cmd, sub_command, SGE_ATTR_MAIL_LIST, SGE_OBJ_AR, 0, nullptr);
   /*   AR_pe, SGE_STRING */
   attr_mod_zerostr(ar, new_ar, AR_pe, object->object_name);
   /*   AR_master_queue_list, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_master_queue_list, AR_name, ar, cmd, sub_command, SGE_ATTR_QUEUE_LIST, SGE_OBJ_AR, 0,
                     nullptr);
   /*   AR_pe_range, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_pe_range, AR_name, ar, cmd, sub_command, SGE_ATTR_PE_LIST, SGE_OBJ_AR, 0, nullptr);
   /*   AR_acl_list, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_acl_list, AR_name, ar, cmd, sub_command, SGE_ATTR_USER_LISTS, SGE_OBJ_AR, 0, nullptr);
   /*   AR_xacl_list, SGE_LIST */
   attr_mod_sub_list(alpp, new_ar, AR_xacl_list, AR_name, ar, cmd, sub_command, SGE_ATTR_XUSER_LISTS, SGE_OBJ_AR, 0, nullptr);
   /*   AR_type, SGE_ULONG     */
   attr_mod_ulong(ar, new_ar, AR_type, object->object_name);

   /* try to reserve the queues */
   if (!ar_reserve_queues(alpp, new_ar, packet->gdi_session)) {
      goto ERROR;
   }

   INFO(MSG_AR_GRANTED_U, ar_id);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(0);

   ERROR:
DRETURN(STATUS_EUNKNOWN);
   DOITAGAIN:
DRETURN(STATUS_NOTOK_DOAGAIN);
}

/****** sge_advance_reservation_qmaster/ar_spool() *****************************
*  NAME
*     ar_spool() -- gdi callback funktion to spool an advance reservation
*
*  SYNOPSIS
*     int ar_spool(sge_gdi_ctx_class_t *ctx, lList **alpp, lListElem *ep, 
*     gdi_object_t *object) 
*
*  FUNCTION
*     This function is called from the framework that
*     add/modify/delete generic gdi objects.
*     After an object was modified/added successfully it
*     is necessary to spool the current state to the filesystem.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - GDI context
*     lList **alpp             - answer_list
*     lListElem *ep            - element to spool
*     gdi_object_t *object     - structure from the GDI framework
*
*  RESULT
*     [alpp] - error messages will be added to this list
*     0 - success
*     STATUS_EEXIST - an error occurred
*
*  NOTES
*     MT-NOTE: ar_spool() is MT safe 
*******************************************************************************/
int ar_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *ep, gdi_object_t *object) {
   lList *answer_list = nullptr;
   dstring buffer = DSTRING_INIT;

   DENTER(TOP_LAYER);

   sge_dstring_sprintf(&buffer, sge_u32, lGetUlong(ep, AR_id));
   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ep,
                                   sge_dstring_get_string(&buffer), SGE_TYPE_AR, true);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, sge_dstring_get_string(&buffer));
   }
   sge_dstring_free(&buffer);

   DRETURN(dbret ? 0 : 1);
}

/****** sge_advance_reservation_qmaster/ar_success() ***************************
*  NAME
*     ar_success() -- does something after a successfully add or modify request
*
*  SYNOPSIS
*     int ar_success(sge_gdi_ctx_class_t *ctx, lListElem *ep, lListElem 
*     *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor) 
*
*  FUNCTION
*     This function is called from the framework that
*     add/modify/delete generic gdi objects.
*     After an object was modified/added and spooled successfully 
*     it is possibly necessary to perform additional tasks.
*     For example it is necessary to send some events to
*     other daemon.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - GDI context
*     lListElem *ep            - new added object
*     lListElem *old_ep        - old object before modifications or nullptr
*                                for add requests
*     gdi_object_t *object     - structure from the GDI framework
*     lList **ppList           - ???
*     monitoring_t *monitor    - monitoring structure
*
*  RESULT
*     int - 0
*
*  NOTES
*     MT-NOTE: ar_success() is not MT safe 
*******************************************************************************/
int
ar_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   te_event_t ev;
   u_long64 timestamp = sge_get_gmt64();

   /* with old_ep it is possible to identify if it is an add or modify request */
   if (old_ep == nullptr) {
      ocs::ReportingFileWriter::create_new_ar_records(nullptr, ep, timestamp);
      ocs::ReportingFileWriter::create_ar_attribute_records(nullptr, ep, timestamp);
   } else {
      ocs::ReportingFileWriter::create_ar_attribute_records(nullptr, ep, timestamp);
   }

   /*
   ** return element with correct id
   */
   if (ppList != nullptr) {
      if (*ppList == nullptr) {
         *ppList = lCreateList("", AR_Type);
      }
      lAppendElem(*ppList, lCopyElem(ep));
   }

   sge_ar_state_set_waiting(ep);

   /*
   ** send sgeE_AR_MOD/sgeE_AR_ADD event
   */
   sge_add_event(0, old_ep ? sgeE_AR_MOD : sgeE_AR_ADD, lGetUlong(ep, AR_id), 0,
                 nullptr, nullptr, nullptr, ep, packet->gdi_session);

   /*
   ** add the timer to trigger the state change
    */
   ev = te_new_event(lGetUlong64(ep, AR_start_time), TYPE_AR_EVENT, ONE_TIME_EVENT, lGetUlong(ep, AR_id),
                     AR_RUNNING, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN(0);
}

/****** sge_advance_reservation_qmaster/ar_del() *******************************
*  NAME
*     ar_del() -- removes advance reservation from master list
*
*  SYNOPSIS
*     int ar_del(sge_gdi_ctx_class_t *ctx, lListElem *ep, lList **alpp, lList 
*     **ar_list, char *ruser, char *rhost) 
*
*  FUNCTION
*     This function removes a advance reservation from the master list and
*     performs the necessary cleanup.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - GDI context
*     lListElem *ep            - element that should be removed (ID_Type)
*     lList **alpp             - answer list
*     lList **ar_list          - list from where the element should be removed
*                                (normally a reference to the master ar list)
*     char *ruser              - user who invoked this GDI request
*     char *rhost              - host where the request was invoked
*
*  RESULT
*     int - 0 on success
*           STATUS_EUNKNOWN on failure
*
*  NOTES
*     MT-NOTE: ar_del() is not MT safe 
*******************************************************************************/
int
ar_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, lList **master_ar_list, monitoring_t *monitor) {
   lListElem *ar, *nxt;
   bool removed_one = false;
   bool has_manager_privileges = false;
   dstring buffer = DSTRING_INIT;
   lCondition *ar_where = nullptr;
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);

   DENTER(TOP_LAYER);

   if (!ep) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no ar_del element, if ep has no ID_str */
   if (lGetPosViaElem(ep, ID_str, SGE_NO_ABORT) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(ID_str), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *id_str = lGetString(ep, ID_str);
   const lList *user_list = lGetList(ep, ID_user_list);
   if (user_list != nullptr) {
      const lListElem *user;

      for_each_ep(user, user_list) {
         const char *user_name = lGetString(user, ST_name);
         bool is_pattern = sge_is_pattern(user_name);

         if (is_pattern && !manop_is_manager(packet, master_manager_list)) {
            ERROR(MSG_SGETEXT_MUST_BE_MGR_TO_SS, packet->user, "modify all advance reservations");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            sge_dstring_free(&buffer);
            lFreeWhere(&ar_where);
            DRETURN(STATUS_EUNKNOWN);
         }

         lCondition *new_where;
         if (is_pattern) {
            new_where = lWhere("%T(%I p= %s)", AR_Type, AR_owner, user_name);
         } else {
            new_where = lWhere("%T(%I == %s)", AR_Type, AR_owner, user_name);
         }
         if (ar_where == nullptr) {
            ar_where = new_where;
         } else {
            ar_where = lOrWhere(ar_where, new_where);
         }
      }
   } else if (sge_is_pattern(id_str)) {
      /* if no userlist and wildcard jobs was requested only delete the own ars */
      lCondition *new_where = nullptr;
      new_where = lWhere("%T(%I == %s)", AR_Type, AR_owner, packet->user);
      if (ar_where == nullptr) {
         ar_where = new_where;
      } else {
         ar_where = lOrWhere(ar_where, new_where);
      }
   }

   if (id_str != nullptr && (strcmp(id_str, "0") != 0)) {
      char *dptr;
      lCondition *new_where = nullptr;

      u_long32 value = strtol(id_str, &dptr, 0);
      if (dptr[0] == '\0') {
         /* is numeric value */
         new_where = lWhere("%T(%I==%u)", AR_Type, AR_id, value);
      } else {
         bool error = false;
         if (isdigit(id_str[0])) {
            ERROR(MSG_OBJECT_INVALID_NAME_S, id_str);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            error = true;
         } else if (verify_str_key(alpp, id_str, MAX_VERIFY_STRING,
                                   lNm2Str(AR_name), KEY_TABLE) != STATUS_OK) {
            error = true;
         } else {
            new_where = lWhere("%T(%I p= %s)", AR_Type, AR_name, id_str);
         }

         if (error) {
            sge_dstring_free(&buffer);
            lFreeWhere(&new_where);
            lFreeWhere(&ar_where);
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      if (!ar_where) {
         ar_where = new_where;
      } else {
         ar_where = lAndWhere(ar_where, new_where);
      }
   } else {
      id_str = nullptr;
   }

   if (id_str == nullptr && user_list == nullptr) {
      CRITICAL(MSG_SGETEXT_SPECIFYUSERORID_S, SGE_OBJ_AR);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      lFreeWhere(&ar_where);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (manop_is_manager(packet, master_manager_list)) {
      has_manager_privileges = true;
   }

   u_long64 now = sge_get_gmt64();
   nxt = lFirstRW(*master_ar_list);
   while ((ar = nxt)) {
      u_long32 ar_id = lGetUlong(ar, AR_id);

      nxt = lNextRW(ar);

      if ((ar_where != nullptr) && !lCompare(ar, ar_where)) {
         continue;
      }

      removed_one = true;

      if (!has_manager_privileges && strcmp(packet->user, lGetString(ar, AR_owner))) {
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
      if (sge_ar_remove_all_jobs(ar_id, lGetUlong(ep, ID_force), monitor, packet->gdi_session)) {
         /* either all jobs were successfull removed or we had no jobs */

         /* unblock reserved queues */
         ar_do_reservation(ar, false, packet->gdi_session);

         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_DELETED,
                                        "AR deleted",
                                        now);
         ocs::ReportingFileWriter::create_ar_acct_records(nullptr, ar, now);

         gdil_del_all_orphaned(lGetList(ar, AR_granted_slots), alpp, packet->gdi_session);

         lRemoveElem(*master_ar_list, &ar);

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
         const lListElem *user;
         bool first = true;
         int umax = 5;

         sge_dstring_sprintf(&buffer, "%s", "");
         for_each_ep(user, user_list) {
            if (!first) {
               sge_dstring_append(&buffer, ",");
            } else {
               first = false;
            }
            if (umax == 0) {
               sge_dstring_append(&buffer, "...");
               break;
            }
            sge_dstring_append(&buffer, lGetString(user, ST_name));
            umax--;
         }
         ERROR(MSG_SGETEXT_THEREARENOXFORUSERS_SS, SGE_OBJ_AR, sge_dstring_get_string(&buffer));
      }

      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      lFreeWhere(&ar_where);
      DRETURN(STATUS_EEXIST);
   }

   sge_dstring_free(&buffer);
   lFreeWhere(&ar_where);
   DRETURN(0);
}

/****** sge_advance_reservation_qmaster/sge_get_ar_id() ************************
*  NAME
*     sge_get_ar_id() -- returns the next possible unused id
*
*  SYNOPSIS
*     static u_long32 sge_get_ar_id(sge_gdi_ctx_class_t *ctx, monitoring_t 
*     *monitor) 
*
*  FUNCTION
*     returns the next possible unused advance reservation id.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - gdi context
*     monitoring_t *monitor    - monitoring structure
*
*  RESULT
*     static u_long32 - ar id
*
*  NOTES
*     MT-NOTE: sge_get_ar_id() is MT safe 
*******************************************************************************/
static u_long32
sge_get_ar_id(monitoring_t *monitor) {
   u_long32 ar_id;
   bool is_store_ar = false;

   DENTER(TOP_LAYER);

   sge_mutex_lock("ar_id_mutex", "sge_get_ar_id", __LINE__, &ar_id_control.ar_id_mutex);

   if (ar_id_control.ar_id >= MAX_SEQNUM) {
      DPRINTF("highest ar number MAX_SEQNUM %d reached, starting over with 1\n", MAX_SEQNUM);
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

/****** sge_advance_reservation_qmaster/sge_store_ar_id() **********************
*  NAME
*     sge_store_ar_id() -- store ar id
*
*  SYNOPSIS
*     void sge_store_ar_id(sge_gdi_ctx_class_t *ctx, te_event_t anEvent, 
*     monitoring_t *monitor) 
*
*  FUNCTION
*     At qmaster shutdown it's necessary to store the latest highest ar id to
*     reinitialize the counter at the next qmaster start. This is done by a event
*     timer in specific intervall.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - GDI context
*     te_event_t anEvent       - event that triggered this function
*     monitoring_t *monitor    - pointer to monitor (not used here)
*
*  NOTES
*     MT-NOTE: sge_store_ar_id() is not MT safe 
*******************************************************************************/
void
sge_store_ar_id(te_event_t anEvent, monitoring_t *monitor) {
   u_long32 ar_id = 0;
   bool changed = false;

   DENTER(TOP_LAYER);

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

/****** sge_advance_reservation_qmaster/sge_init_ar_id() ***********************
*  NAME
*     sge_init_ar_id() -- init ar id counter
*
*  SYNOPSIS
*     void sge_init_ar_id() 
*
*  FUNCTION
*     Called during startup and sets the advance reservation id counter. 
*
*  NOTES
*     MT-NOTE: sge_init_ar_id() is MT safe 
*******************************************************************************/
void
sge_init_ar_id() {
   FILE *fp = nullptr;
   u_long32 ar_id = 0;
   u_long32 guess_ar_id = 0;

   DENTER(TOP_LAYER);

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
   ar_id = MAX(ar_id, guess_ar_id);

   sge_mutex_lock("ar_id_mutex", "sge_init_ar_id", __LINE__,
                  &ar_id_control.ar_id_mutex);
   ar_id_control.ar_id = ar_id;
   ar_id_control.changed = true;
   sge_mutex_unlock("ar_id_mutex", "sge_init_ar_id", __LINE__,
                    &ar_id_control.ar_id_mutex);

   DRETURN_VOID;
}

/****** sge_advance_reservation_qmaster/guess_highest_ar_id() ******************
*  NAME
*     guess_highest_ar_id() -- guesses the histest ar id
*
*  SYNOPSIS
*     static u_long32 guess_highest_ar_id() 
*
*  FUNCTION
*     Iterates over all granted advance reservations in the cluster and determines
*     the highest id
*
*  RESULT
*     static u_long32 - determined id
*
*  NOTES
*     MT-NOTE: guess_highest_ar_id() is MT safe 
*******************************************************************************/
static u_long32
guess_highest_ar_id() {
   const lListElem *ar;
   u_long32 maxid = 0;
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);

   DENTER(TOP_LAYER);

   /* this function is called during qmaster startup and not while it is running,
      we do not need to monitor this lock */
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   ar = lFirst(master_ar_list);
   if (ar) {
      int pos;
      pos = lGetPosViaElem(ar, AR_id, SGE_NO_ABORT);

      for_each_ep(ar, master_ar_list) {
         maxid = MAX(maxid, lGetPosUlong(ar, pos));
      }
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   DRETURN(maxid);
}

/****** sge_advance_reservation_qmaster/sge_ar_event_handler() *****************
*  NAME
*     sge_ar_event_handler() -- advance reservation event handler
*
*  SYNOPSIS
*     void sge_ar_event_handler(sge_gdi_ctx_class_t *ctx, te_event_t anEvent, 
*     monitoring_t *monitor) 
*
*  FUNCTION
*     Registered function in the times event framework. For every granted a trigger
*     for the start time of the advance reservation is registered. When the function is
*     executed at start time it regististers a additional timer for the end time of
*     the advance reservation.
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - GDI context
*     te_event_t anEvent       - triggered timed event
*     monitoring_t *monitor    - monitoring structure
*
*  NOTES
*     MT-NOTE: sge_ar_event_handler() is MT safe 
*******************************************************************************/
void
sge_ar_event_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   lListElem *ar;
   u_long32 ar_id = te_get_first_numeric_key(anEvent);
   u_long32 state = te_get_second_numeric_key(anEvent);
   te_event_t ev;

   /*
    To guarantee all jobs are removed from the cluster when AR end time is
    reached it is necessary to consider the DURATION_OFFSET for Advance Reservation also.
    This means all jobs submitted to a AR will have a resulting runtime limit of AR duration - DURATION_OFFSET.
    Jobs requesting a longer runtime will not be scheduled.
    The AR requester needs to keep this in mind when he creates a new AR.
    */
   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   lList *master_ar_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_AR);

   if (!(ar = ar_list_locate(master_ar_list, ar_id))) {
      ERROR(MSG_EVE_TE4AR_U, ar_id);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
      DRETURN_VOID;
   }

   if (state == AR_EXITED) {
      u_long64 timestamp = sge_get_gmt64();

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
      gdil_del_all_orphaned(lGetList(ar, AR_granted_slots), nullptr, ocs::SessionManager::GDI_SESSION_NONE);

      /* remove the AR itself */
      DPRINTF("AR: exited, removing AR " sge_u32 "\n", ar_id);
      lRemoveElem(master_ar_list, &ar);
      sge_event_spool(nullptr, timestamp, sgeE_AR_DEL,
                      ar_id, 0, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, true, true, ocs::SessionManager::GDI_SESSION_NONE);

   } else {
      /* AR_RUNNING */
      DPRINTF("AR: started, changing state of AR " sge_u32 "\n", ar_id);

      sge_ar_state_set_running(ar);

      ev = te_new_event(lGetUlong64(ar, AR_end_time), TYPE_AR_EVENT, ONE_TIME_EVENT, ar_id, AR_EXITED, nullptr);
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

/****** sge_advance_reservation_qmaster/ar_reserve_queues() ********************
*  NAME
*     ar_reserve_queues() -- selects the queues for reserving 
*
*  SYNOPSIS
*     static bool ar_reserve_queues(lList **alpp, lListElem *ar) 
*
*  FUNCTION
*     The function executes the scheduler code to select queues matching the
*     advance reservation request for reserving. The function works on temporary
*     lists and creates the AR_granted_slots list
*
*  INPUTS
*     lList **alpp  - answer list pointer pointer
*     lListElem *ar - ar object
*
*  RESULT
*     static bool - true on success, enough resources reservable
*                   false in verify mode or not enough resources available
*
*  NOTES
*     MT-NOTE: ar_reserve_queues() is not MT safe, needs GLOBAL_LOCK
*******************************************************************************/
static bool
ar_reserve_queues(lList **alpp, lListElem *ar, u_long64 gdi_session) {
   lList **splitted_job_lists[SPLIT_LAST];
   lList *suspended_list = nullptr;                   /* JB_Type */
   lList *running_list = nullptr;                     /* JB_Type */

   int verify_mode = lGetUlong(ar, AR_verify);
   lList *talp = nullptr;
   const lList *ar_queue_request = lGetList(ar, AR_queue_list);
   const char *ar_pe_request = lGetString(ar, AR_pe);
   const char *ar_ckpt_request = lGetString(ar, AR_checkpoint_name);

   const lListElem *cqueue = nullptr;
   bool ret = true;
   int i = 0;
   lListElem *dummy_job = lCreateElem(JB_Type);
   sge_assignment_t a = SGE_ASSIGNMENT_INIT;
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_cal_list = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);

   /* These lists must be copied */
   lList *master_pe_list = lCopyList("", *ocs::DataStore::get_master_list(SGE_TYPE_PE));
   lList *master_exechost_list = lCopyList("", *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST));

   dispatch_t result = DISPATCH_NEVER_CAT;

   DENTER(TOP_LAYER);

   if (lGetList(ar, AR_acl_list) != nullptr) {
      lSetString(dummy_job, JB_owner, "*");
      lSetString(dummy_job, JB_group, "*");
   } else {
      lSetString(dummy_job, JB_owner, lGetString(ar, AR_owner));
      lSetString(dummy_job, JB_group, lGetString(ar, AR_group));
   }

   assignment_init(&a, dummy_job, nullptr, nullptr);
   a.host_list = master_exechost_list;
   a.centry_list = master_centry_list;
   a.acl_list = master_userset_list;
   a.hgrp_list = master_hgroup_list;
   a.gep = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);
   a.start = lGetUlong64(ar, AR_start_time);
   a.duration = lGetUlong64(ar, AR_duration);
   a.is_reservation = true;
   a.is_advance_reservation = true;
   a.now = sge_get_gmt64();

   /* 
    * Current scheduler code expects all queue instances in a plain list. We use 
    * a copy of all queue instances that needs to be free'd explicitely after 
    * deciding about assignment. This is because assignment_release() sees 
    * queue_list only as a list pointer.
    */
   a.queue_list = lCreateList("", QU_Type);

   /* imagine qs is empty */
   sconf_set_qs_state(QS_STATE_EMPTY);

   /* redirect scheduler monitoring into answer list */
   if (verify_mode == AR_JUST_VERIFY) {
      DPRINTF("AR Verify Mode\n");
      a.monitor_alpp = &talp;
   }

   for_each_ep(cqueue, master_cqueue_list) {
      const char *cqname = lGetString(cqueue, CQ_name);
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      const lListElem *qinstance;

      if (cqueue_match_static(cqname, &a) != DISPATCH_OK) {
         continue;
      }

      // @todo some of the check below (pe, acl/xacl are already done in cqueue_match_static(). Do we need to repeat them on qinstance layer?
      for_each_ep(qinstance, qinstance_list) {
         const char *cal_name;

         /* skip orphaned queues */
         if (qinstance_state_is_orphaned(qinstance)) {
            continue;
         }

         /* we only have to consider requested queues */
         if (ar_queue_request != nullptr) {
            if (qref_list_cq_rejected(ar_queue_request, cqname,
                                      lGetHost(qinstance, QU_qhostname), master_hgroup_list)) {
               continue;
            }
         }

         /* we only have to consider queues containing the requested pe */
         if (ar_pe_request != nullptr) {
            bool found = false;
            const lListElem *pe_ref;

            for_each_ep(pe_ref, lGetList(qinstance, QU_pe_list)) {
               if (pe_name_is_matching(lGetString(pe_ref, ST_name), ar_pe_request)) {
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
            if (lGetSubStr(qinstance, ST_name, ar_ckpt_request, QU_ckpt_list) == nullptr) {
               continue;
            }
         }

         /* sort out queue that are calendar disabled in requested time frame */
         if ((cal_name = lGetString(qinstance, QU_calendar)) != nullptr) {
            const lListElem *cal_ep = lGetElemStrRW(master_cal_list, CAL_name, cal_name);

            if (!calendar_open_in_time_frame(cal_ep, lGetUlong64(ar, AR_start_time), lGetUlong64(ar, AR_duration))) {
               /* skip queue */
               answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_AR_QUEUEDISABLEDINTIMEFRAME,
                                       lGetString(qinstance, QU_full_name));
               continue;
            }
         }
         /* sort out queues where not all users have access */
         if (lGetList(ar, AR_acl_list) != nullptr) {
            if (!sge_ar_have_users_access(alpp, ar, lGetString(qinstance, QU_full_name),
                                          lGetList(qinstance, QU_acl),
                                          lGetList(qinstance, QU_xacl),
                                          master_userset_list)) {
               continue;
            }
         }

         lAppendElem(a.queue_list, lCopyElem(qinstance));
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

      /* split job lists must be freed */
      split_jobs(&master_job_list, mconf_get_max_aj_instances(), splitted_job_lists, true);
   }

   /*
    * prepare resource schedule
    */
   prepare_resource_schedules(*(splitted_job_lists[SPLIT_RUNNING]),
                              *(splitted_job_lists[SPLIT_SUSPENDED]),
                              master_pe_list, a.host_list, a.queue_list,
                              nullptr, a.centry_list, a.acl_list,
                              a.hgrp_list, nullptr, false, a.now);

   /* free generated job lists */
   lFreeList(splitted_job_lists[SPLIT_RUNNING]);
   lFreeList(splitted_job_lists[SPLIT_SUSPENDED]);

   lSetUlong64(dummy_job, JB_execution_time, lGetUlong64(ar, AR_start_time));
   lSetUlong64(dummy_job, JB_deadline, lGetUlong64(ar, AR_end_time));
   job_set_hard_resource_list(dummy_job, lCopyList(nullptr, lGetList(ar, AR_resource_list)));
   job_set_hard_queue_list(dummy_job, lCopyList(nullptr, lGetList(ar, AR_queue_list)));
   job_set_master_hard_queue_list(dummy_job, lCopyList(nullptr, lGetList(ar, AR_master_queue_list)));
   lSetUlong(dummy_job, JB_type, lGetUlong(ar, AR_type));
   lSetString(dummy_job, JB_checkpoint_name, lGetString(ar, AR_checkpoint_name));


   if (lGetString(ar, AR_pe)) {
      lSetString(dummy_job, JB_pe, lGetString(ar, AR_pe));
      lSetList(dummy_job, JB_pe_range, lCopyList("", lGetList(ar, AR_pe_range)));

      result = sge_select_parallel_environment(&a, master_pe_list);
      if (result == DISPATCH_OK) {
         lSetString(ar, AR_granted_pe, lGetString(a.pe, PE_name));
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
         lSetList(ar, AR_granted_slots, a.gdil);
         ar_initialize_resource_booking(ar);
         a.gdil = nullptr;

         ar_do_reservation(ar, true, gdi_session);
      }
   }

   /* stop dreaming */
   sconf_set_qs_state(QS_STATE_FULL);

   lFreeList(&(a.queue_list));
   lFreeList(&master_pe_list);
   lFreeList(&master_exechost_list);
   lFreeElem(&dummy_job);

   assignment_release(&a);

   DRETURN(ret);
}

/****** sge_advance_reservation_qmaster/ar_do_reservation() ********************
*  NAME
*     ar_do_reservation() -- do the reservation in the selected queue instances
*
*  SYNOPSIS
*     int ar_do_reservation(lListElem *ar, bool incslots) 
*
*  FUNCTION
*     This function does the (un)reserveration in the selected parallel environment
*     and the selected queue instances
*
*  INPUTS
*     lListElem *ar - ar object (AR_Type)
*     bool incslots - increase or decrease usage
*
*  RESULT
*     int - 0
*
*  NOTES
*     MT-NOTE: ar_do_reservation() is not MT safe 
*
*  SEE ALSO
*     sge_resource_utilization/rqs_add_job_utilization()
*******************************************************************************/
int
ar_do_reservation(lListElem *ar, bool incslots, u_long64 gdi_session) {
   lListElem *dummy_job = lCreateElem(JB_Type);
   int pe_slots = 0;
   int tmp_slots;
   const char *granted_pe = lGetString(ar, AR_granted_pe);
   u_long64 start_time = lGetUlong64(ar, AR_start_time);
   u_long64 duration = lGetUlong64(ar, AR_duration);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_exechost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   bool is_master_task = true;

   DENTER(TOP_LAYER);

   job_set_hard_resource_list(dummy_job, lCopyList(nullptr, lGetList(ar, AR_resource_list)));
   job_set_hard_queue_list(dummy_job, lCopyList(nullptr, lGetList(ar, AR_queue_list)));

   lListElem *global_host_ep = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);

   lListElem *pe = nullptr;
   if (granted_pe != nullptr) {
      pe = pe_list_locate(master_pe_list, granted_pe);
      if (pe == nullptr) {
         ERROR(MSG_OBJ_UNABLE2FINDPE_S, granted_pe);
      }
   }

   const lListElem *gdil_ep;
   const char *last_hostname = nullptr;
   for_each_ep(gdil_ep, lGetList(ar, AR_granted_slots)) {
      const char *queue_name = lGetString(gdil_ep, JG_qname);
      lListElem *queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name);

      if (queue == nullptr) {
         ERROR(MSG_JOB_UNABLE2FINDQOFJOB_S, queue_name);
         is_master_task = false;
         continue;
      }

      const char *queue_hostname = lGetHost(queue, QU_qhostname);
      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, queue_hostname);

      if (!incslots) {
         tmp_slots = -lGetUlong(gdil_ep, JG_slots);
      } else {
         tmp_slots = lGetUlong(gdil_ep, JG_slots);
      }

      pe_slots += tmp_slots;

      /* reserve global host */
      if (rc_add_job_utilization(dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                 global_host_ep, master_centry_list, tmp_slots,
                                 EH_consumable_config_list, EH_resource_utilization,
                                 SGE_GLOBAL_NAME, start_time, duration, GLOBAL_TAG,
                                 false, is_master_task, do_per_host_booking) != 0) {
         /* this info is not spooled */
         sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0,
                       SGE_GLOBAL_NAME, nullptr, nullptr, global_host_ep, gdi_session);
      }

      /* reserve exec host */
      lListElem *host_ep = host_list_locate(master_exechost_list, queue_hostname);
      if (rc_add_job_utilization(dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                 host_ep, master_centry_list, tmp_slots, EH_consumable_config_list,
                                 EH_resource_utilization, queue_hostname, start_time,
                                 duration, HOST_TAG, false, is_master_task, do_per_host_booking) != 0) {
         /* this info is not spooled */
         sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0,
                       queue_hostname, nullptr, nullptr, host_ep, gdi_session);
      }

      /* reserve queue instance */
      rc_add_job_utilization(dummy_job, pe, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                             queue, master_centry_list, tmp_slots, QU_consumable_config_list,
                             QU_resource_utilization, queue_name, start_time, duration,
                             QUEUE_TAG, false, is_master_task, do_per_host_booking);

      qinstance_increase_qversion(queue);
      /* this info is not spooled */
      qinstance_add_event(queue, sgeE_QINSTANCE_MOD, gdi_session);
      is_master_task = false;
   }

   if (pe != nullptr) {
      utilization_add(lFirstRW(lGetList(pe, PE_resource_utilization)), start_time,
                      duration, pe_slots, 0, 0, PE_TAG, granted_pe,
                      SCHEDULING_RECORD_ENTRY_TYPE_RESERVING, false, false);
      sge_add_event(0, sgeE_PE_MOD, 0, 0, granted_pe, nullptr, nullptr, pe, gdi_session);
   }

   lFreeElem(&dummy_job);

   DRETURN(0);
}

/****** libs/sgeobj/ar_list_has_reservation_due_to_ckpt() **********************
*  NAME
*     ar_list_has_reservation_due_to_ckpt() -- does ckpt change breake an ar 
*
*  SYNOPSIS
*     bool ar_list_has_reservation_due_to_ckpt(lList *ar_master_list, 
*                                              lList **answer_list,
*                                              const char *qinstance_name, 
*                                              lList *ckpt_string_list) 
*
*  FUNCTION
*     This function tests if a modification of a ckpt list in a qinstance is
*     allowed according to the advance reservations. 
*
*     Input parameters are: the advance reservation master list, the name of the
*     qinstance which sould be modified and the ST_Type string list of ckpt
*     names which represents the new setting for the qinstance.
*
*     If there is no reservation for this qinstance-ckpt combination or if 
*     the reservation would be still valid after the modification then 
*     the function returns 'false". Otherwise 'true' 
*
*  INPUTS
*     lList *ar_master_list      - advance reservation master list
*     lList **answer_list        - answer list which will contain the reason why a 
*                                  modification is not valid
*     const char *qinstance_name - name of a qinstance <cqname@hostname>
*     lList *ckpt_string_list    - ST_Type list containing ckpt names 
*
*  RESULT
*     boolean
*        true - modification would breake at least one ar
*        false - no ar will be broken if the ckpt list is modified 
*
*  NOTES
*     MT-NOTE: ar_get_string_from_event() is MT safe 
*******************************************************************************/
bool
ar_list_has_reservation_due_to_ckpt(const lList *ar_master_list, lList **answer_list,
                                    const char *qinstance_name, lList *ckpt_string_list) {
   const lListElem *ar;

   DENTER(TOP_LAYER);

   for_each_ep(ar, ar_master_list) {
      const char *ckpt_string = lGetString(ar, AR_checkpoint_name);

      if (ckpt_string != nullptr && lGetElemStr(lGetList(ar, AR_granted_slots), JG_qname, qinstance_name)) {
         if (lGetElemStr(ckpt_string_list, ST_name, ckpt_string) == nullptr) {
            ERROR(MSG_PARSE_MOD_REJECTED_DUE_TO_AR_SSU, ckpt_string, SGE_ATTR_CKPT_LIST, lGetUlong(ar, AR_id));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/****** libs/sgeobj/ar_list_has_reservation_due_to_pe() **********************
*  NAME
*     ar_list_has_reservation_due_to_pe() -- does pe change breake an ar 
*
*  SYNOPSIS
*     bool ar_list_has_reservation_due_to_pe(lList *ar_master_list, 
*                                            lList **answer_list,
*                                            const char *qinstance_name, 
*                                            lList *pe_string_list) 
*
*  FUNCTION
*     This function tests if a modification of a pe list in a qinstance is
*     allowed according to the advance reservations. 
*
*     Input parameters are: the advance reservation master list, the name of the
*     qinstance which should be modified and the ST_Type string list of pe 
*     names which represents the new setting for the qinstance.
*
*     If there is no reservation for this qinstance-ckpt combination or if 
*     the reservation would be still valid after the modification then 
*     the function returns 'false". Otherwise 'true' 
*
*  INPUTS
*     lList *ar_master_list      - advance reservation master list
*     lList **answer_list        - answer list which will contain the reason why a 
*                                  modification is not valid
*     const char *qinstance_name - name of a qinstance <cqname@hostname>
*     lList *pe_string_list    - ST_Type list containing pe names 
*
*  RESULT
*     boolean
*        true - modification would breake at least one ar
*        false - no ar will be broken if the ckpt list is modified 
*
*  NOTES
*     MT-NOTE: ar_get_string_from_event() is MT safe 
*******************************************************************************/
bool
ar_list_has_reservation_due_to_pe(const lList *ar_master_list, lList **answer_list, const char *qinstance_name,
                                  lList *pe_string_list) {
   const lListElem *ar;

   DENTER(TOP_LAYER);

   for_each_ep(ar, ar_master_list) {
      const char *pe_string = lGetString(ar, AR_pe);

      if (pe_string != nullptr && lGetElemStr(lGetList(ar, AR_granted_slots), JG_qname, qinstance_name)) {
         if (lGetElemStr(pe_string_list, ST_name, pe_string) == nullptr) {
            ERROR(MSG_PARSE_MOD_REJECTED_DUE_TO_AR_SSU, pe_string, SGE_ATTR_PE_LIST, lGetUlong(ar, AR_id));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/****** sgeobj/ar_list_has_reservation_for_pe_with_slots() ********************
*  NAME
*     ar_list_has_reservation_for_pe_with_slots() -- Does PE change violate AR 
*
*  SYNOPSIS
*     bool 
*     ar_list_has_reservation_for_pe_with_slots(lList *ar_master_list, 
*                                               lList **answer_list, 
*                                               const char *pe_name, 
*                                               u_long32 new_slots) 
*
*  FUNCTION
*     This function tests if a modification of slots entry in a pe is
*     allowed according to the advance reservations. 
*
*     Input parameters are: the advance reservation master list, the name of the
*     pe which should be modified and the new slots value which should
*     be set in the pe which might vialote the advance reservations in
*     the system
*
*     If there is no reservation for this pe or if the new slots setting
*     does not violate the advance reservations in the system then this
*     function returns 'false'. Otherwise 'true'
*
*  INPUTS
*     lList *ar_master_list - master advance reservation list 
*     lList **answer_list   - answer list 
*     const char *pe_name   - pe name 
*     u_long32 new_slots    - new slots setting for pe with 'pe_name' 
*
*  RESULT
*     bool 
*        true - modification would break the ar's currently known
*        false - modification is valid
*
*  NOTES
*     MT-NOTE: ar_list_has_reservation_for_pe_with_slots() is MT safe 
*******************************************************************************/
bool
ar_list_has_reservation_for_pe_with_slots(const lList *ar_master_list, lList **answer_list, const char *pe_name,
                                          u_long32 new_slots) {
   bool ret = false;
   const lListElem *ar;
   const lListElem *gs;
   u_long32 max_res_slots = 0;

   DENTER(TOP_LAYER);

   for_each_ep(ar, ar_master_list) {
      const char *pe_string = lGetString(ar, AR_pe);

      if (pe_name != nullptr && pe_string != nullptr && strcmp(pe_string, pe_name) == 0) {
         for_each_ep(gs, lGetList(ar, AR_granted_slots)) {
            u_long32 slots = lGetUlong(gs, JG_slots);

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
 * @param target the target queue or host (from the AR_reserved_queues or AR_reserved_hosts)
 * @param source  the source queue or host (from the master queue or host list)
 * @param nm EH_consumable_config_list or QU_consumable_config_list
 * @param cr complex object
 * @param name name of the complex
 * @param doubleval capacity to store in the target object
 * @param stringval capacity formatted as string
 */
static void
ar_add_consumable(lListElem *target, const lListElem *source, int nm, const lListElem *cr, const char *name, double doubleval, const char *stringval) {
   DENTER(TOP_LAYER);

   // is the complex defined in the source objects complex_values at all?
   const lListElem *master_cr = lGetSubStr(source, CE_name, name, nm);
   if (master_cr != nullptr) {
      // make sure that the ??_consumable_config_list exists in target (host or queue)
      lListElem *ccobj = nullptr;
      lList *cclist = lGetListRW(target, nm);
      if (cclist == nullptr) {
         cclist = lCreateList("", CE_Type);
         lSetList(target, nm, cclist);
      } else {
         // we might already have the consumable in the list (for hosts having multiple qinstances / the global host)
         ccobj = lGetElemStrRW(cclist, CE_name, name);
      }
      if (ccobj == nullptr) {
         // if it did not exist, create it
         ccobj = lCopyElem(cr);
         lSetDouble(ccobj, CE_doubleval, doubleval);
         lSetString(ccobj, CE_stringval, stringval);
         lAppendElem(cclist, ccobj);
      } else {
         // if it already existed, add the doubleval to the existing value and re-create the stringval
         lAddDouble(ccobj, CE_doubleval, doubleval);
         double sum = lGetDouble(ccobj, CE_doubleval);
         DSTRING_STATIC(dstr, 512);
         lSetString(ccobj, CE_stringval, sge_dstring_sprintf(&dstr, "%f", sum));
      }
   }
   DRETURN_VOID;
}

/**
 * @brief add a non consumable complex to an AR resource's (queue or host) complex_values
 * @param target the target queue or host (from the AR_reserved_queues or AR_reserved_hosts)
 * @param source  the source queue or host (from the master queue or host list)
 * @param nm EH_consumable_config_list or QU_consumable_config_list
 * @param name name of the complex
 */
static void
ar_add_non_consumable(lListElem *target, const lListElem *source, int nm, const char *name) {
   // is the complex defined in the source objects complex_values at all?
   const lListElem *cr = lGetSubStr(source, CE_name, name, nm);
   if (cr != nullptr) {
      // make sure that the ??_consumable_config_list exists in target (host or queue)
      lListElem *ccobj = nullptr;
      lList *cclist = lGetListRW(target, nm);
      if (cclist == nullptr) {
         cclist = lCreateList("", CE_Type);
         lSetList(target, nm, cclist);
      } else {
         // we might already have the complex entry in the list (for hosts having multiple qinstances / the global host)
         ccobj = lGetElemStrRW(cclist, CE_name, name);
      }
      if (ccobj == nullptr) {
         // if it does not yet exist, add it
         lAppendElem(cclist, lCopyElem(cr));
      }
   }
}

/**
 * @brief fetch the amount of resources requested for an AR
 * @param ar the AR object
 * @param cr the complex definition (for default requests)
 * @param cr_name  the complex/request name
 * @return 0.0 if the resource was not requested, else the requested amount
 */
static bool
ar_get_request_or_default(const lListElem *ar, const lListElem *cr, const char *cr_name, double &request_value) {
   double ret = false;

   const lListElem *request = lGetSubStr(ar, CE_name, cr_name, AR_resource_list);
   if (request != nullptr) {
      // resource was explicitly requested
      request_value = lGetDouble(request, CE_doubleval);
      ret = true;
   } else {
      // resource was not requested but there might be a default request
      const char *default_string = lGetString(cr, CE_defaultval);
      double default_double;
      if (default_string != nullptr && parse_ulong_val(&default_double, NULL, lGetUlong(cr, CE_valtype), default_string, NULL, 0) != 0) {
         request_value = default_double;
         ret = true;
      }
   }

   return ret;
}

/****** sge_advance_reservation_qmaster/ar_initialize_resource_booking() *******
*  \brief Initialize reserved queue structure.
*
*  \details
*  The function creates the resource booking lists (AR_reserved_queues and AR_reserved_hosts)
*  that store the necessary data to debit jobs in an AR. The elements of the lists are
*  reduced elements of QU_Type and EH_Type.
*
*  \param[in] ar  Advance reservation that should be initialized.
*
*  \note
*  MT-NOTE: ar_initialize_resource_booking() is not MT safe.
*******************************************************************************/
void
ar_initialize_resource_booking(lListElem *ar) {
   DENTER(TOP_LAYER);

   const lListElem *gep;
   const lList *gdil = lGetList(ar, AR_granted_slots);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_exechost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
   dstring buffer = DSTRING_INIT;

   static int queue_fields[] {
      QU_qhostname,
      QU_qname,
      QU_full_name,
      QU_job_slots,
      QU_consumable_config_list,
      QU_resource_utilization,
      QU_message_list,
      QU_state,
      NoName
   };

   static int host_fields[] {
      EH_name,
      EH_consumable_config_list,
      EH_resource_utilization,
      NoName
   };

   lEnumeration *queue_what = lIntVector2What(QU_Type, queue_fields);
   lEnumeration *host_what = lIntVector2What(EH_Type, host_fields);

   lDescr *queue_descr = nullptr;
   lDescr *host_descr = nullptr;
   lReduceDescr(&queue_descr, QU_Type, queue_what);
   lReduceDescr(&host_descr, EH_Type, host_what);
   lFreeWhat(&queue_what);
   lFreeWhat(&host_what);

   // these are the AR's AR_reserved_queues and AR_reserved_hosts lists
   // the AR_reserved_hosts list gets a global host element
   lList *queue_list = nullptr;
   lList *host_list = nullptr;
   lListElem *global_host = lAddElemHost(&host_list, EH_name, SGE_GLOBAL_NAME, host_descr);
   const lListElem *master_global_host = lGetElemHost(master_exechost_list, EH_name, SGE_GLOBAL_NAME);

   bool is_master_queue = true;
   const char *last_hostname = nullptr;
   for_each_ep(gep, gdil) {
      const char *queue_name = lGetString(gep, JG_qname);
      char *cqueue_name = cqueue_get_name_from_qinstance(queue_name);
      const char *host_name = lGetHost(gep, JG_qhostname);

      // create queue with slot booking
      lListElem *queue = lAddElemStr(&queue_list, QU_full_name, queue_name, queue_descr);
      lSetString(queue, QU_qname, cqueue_name);
      lSetHost(queue, QU_qhostname, host_name);
      u_long32 slots = lGetUlong(gep, JG_slots);
      lSetUlong(queue, QU_job_slots, slots);

      const lListElem *master_cqueue = cqueue_list_locate(master_cqueue_list, cqueue_name);
      const lListElem *master_queue = cqueue_locate_qinstance(master_cqueue, host_name);

      bool do_per_host_booking = host_do_per_host_booking(&last_hostname, host_name);

      // get or create host (one host can appear multiple times in gdil)
      lListElem *host = lGetElemHostRW(host_list, EH_name, host_name);
      if (host == nullptr) {
         host = lAddElemHost(&host_list, EH_name, host_name, host_descr);
      }
      const lListElem *master_host = lGetElemHost(master_exechost_list, EH_name, host_name);

      // now we have queue, host and global host objects, fill in the granted complex_values

      // loop over the defined complex values and add them to the queue and hosts if
      // - they have been requested
      // - or they have a default request
      // - they are defined on the queue, host, global layer
      // - non-requested exclusive? no. they will have effect in the booking of the AR itself
      const lListElem *cr;
      for_each_ep (cr, master_centry_list) {
         const char *cr_name = lGetString(cr, CE_name);
         u_long32 consumable = lGetUlong(cr, CE_consumable);
         if (consumable == CONSUMABLE_NO) {
            // non-consumable, add the definition from the master object (if defined on the layer)
            ar_add_non_consumable(global_host, master_global_host, EH_consumable_config_list, cr_name);
            ar_add_non_consumable(host, master_host, EH_consumable_config_list, cr_name);
            ar_add_non_consumable(queue, master_queue, QU_consumable_config_list, cr_name);
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
               ar_add_consumable(global_host, master_global_host, EH_consumable_config_list, cr, cr_name, doubleval, stringval);
               ar_add_consumable(host, master_host, EH_consumable_config_list, cr, cr_name, doubleval, stringval);
               ar_add_consumable(queue, master_queue, QU_consumable_config_list, cr, cr_name, doubleval, stringval);
            }
         }
      }

      /* initialize QU_state */
      if (qinstance_state_is_ambiguous(master_queue)) {
         lAddUlong(ar, AR_qi_errors, 1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_AMBIGUOUS));
         qinstance_set_error(queue, QI_AMBIGUOUS, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_alarm(master_queue)) {
         lAddUlong(ar, AR_qi_errors, 1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_ALARM));
         qinstance_set_error(queue, QI_ALARM, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_suspend_alarm(master_queue)) {
         lAddUlong(ar, AR_qi_errors, 1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_SUSPEND_ALARM));
         qinstance_set_error(queue, QI_SUSPEND_ALARM, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_manual_disabled(master_queue)) {
         lAddUlong(ar, AR_qi_errors, 1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_DISABLED));
         qinstance_set_error(queue, QI_DISABLED, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_unknown(master_queue)) {
         lAddUlong(ar, AR_qi_errors, 1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_UNKNOWN));
         qinstance_set_error(queue, QI_UNKNOWN, sge_dstring_get_string(&buffer), true);
      }
      if (qinstance_state_is_error(master_queue)) {
         lAddUlong(ar, AR_qi_errors, 1);
         sge_dstring_sprintf(&buffer, "reserved queue %s is %s", queue_name,
                             qinstance_state_as_string(QI_ERROR));
         qinstance_set_error(queue, QI_ERROR, sge_dstring_get_string(&buffer), true);
      }

      sge_free(&cqueue_name);
      is_master_queue = false;
   }

   lListElem *queue;
   for_each_rw(queue, queue_list) {
      // make sure all complex attributes are properly filled in
      centry_list_fill_config(lGetListRW(queue, QU_consumable_config_list), master_centry_list);
      // ensure availability of implicit slot request
      qinstance_set_conf_slots_used(queue);
      // initialize QU_resource_utilization
      qinstance_debit_consumable(queue, nullptr, nullptr, master_centry_list, 0, true, true, nullptr);
   }
   lListElem *host;
   for_each_rw(host, host_list) {
      // make sure all complex attributes are properly filled in
      centry_list_fill_config(lGetListRW(host, EH_consumable_config_list), master_centry_list);
      debit_host_consumable(nullptr, nullptr, nullptr, host, master_centry_list, 0, true, true, nullptr);
   }

   lSetList(ar, AR_reserved_queues, queue_list);
   lSetList(ar, AR_reserved_hosts, host_list);

   sge_free(&queue_descr);
   sge_free(&host_descr);
   sge_dstring_free(&buffer);

   DRETURN_VOID;
}

/****** sge_advance_reservation_qmaster/sge_ar_remove_all_jobs() ***************
*  NAME
*     sge_ar_remove_all_jobs() -- removes all jobs of an AR
*
*  SYNOPSIS
*     void sge_ar_remove_all_jobs(sge_gdi_ctx_class_t *ctx, u_long32 
*     ar_id, monitoring_t *monitor) 
*
*  FUNCTION
*     The function deletes all jobs (and tasks) requested the advance
*     reservation
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t *ctx - context handler
*     u_long32 ar_id           - advance reservation id
*     monitoring_t *monitor    - monitoring structure
*
*  NOTES
*     MT-NOTE: sge_ar_remove_all_jobs() is not MT safe 
*******************************************************************************/
bool
sge_ar_remove_all_jobs(u_long32 ar_id, int forced, monitoring_t *monitor, u_long64 gdi_session) {
   lListElem *nextjep, *jep;
   lListElem *tmp_task;
   bool ret = true;

   DENTER(TOP_LAYER);

   nextjep = lFirstRW(*ocs::DataStore::get_master_list(SGE_TYPE_JOB));
   while ((jep = nextjep)) {
      u_long32 task_number;
      u_long32 start = MIN(job_get_smallest_unenrolled_task_id(jep), job_get_smallest_enrolled_task_id(jep));
      u_long32 end = MAX(job_get_biggest_unenrolled_task_id(jep), job_get_biggest_enrolled_task_id(jep));

      nextjep = lNextRW(jep);
      if (lGetUlong(jep, JB_ar) != ar_id) {
         continue;
      }

      DPRINTF("removing job %d\n", lGetUlong(jep, JB_job_number));
      DPRINTF(" ----> task_start = %d, task_end = %d\n", start, end);

      for (task_number = start;
           task_number <= end;
           task_number++) {

         if (job_is_ja_task_defined(jep, task_number)) {

            if (job_is_enrolled(jep, task_number)) {
               /* delete all enrolled pending tasks */
               DPRINTF("removing enrolled task %d.%d\n", lGetUlong(jep, JB_job_number), task_number);
               tmp_task = lGetSubUlongRW(jep, JAT_task_number, task_number, JB_ja_tasks);

               /* 
                * if task is already in status deleted and was signaled
                * only recently and deletion is not forced, do nothing
                */
               if (ISSET(lGetUlong(tmp_task, JAT_status), JFINISHED)) {
                  continue;
               }

               if (forced) {
                  sge_commit_job(jep, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED_EE,
                                 COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
               } else {
                  if (!ISSET(lGetUlong(tmp_task, JAT_state), JDELETED)) {
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
               DPRINTF("removing unenrolled task %d.%d\n", lGetUlong(jep, JB_job_number), task_number);
               tmp_task = job_get_ja_task_template_pending(jep, task_number);

               sge_commit_job(jep, tmp_task, nullptr, COMMIT_ST_FINISHED_FAILED,
                              COMMIT_NO_SPOOLING | COMMIT_UNENROLLED_TASK | COMMIT_NEVER_RAN,
                              monitor, gdi_session);
            }
         }
      }
   }

   DRETURN(ret);
}

/****** sge_advance_reservation_qmaster/sge_ar_list_conflicts_with_calendar() ******
*  NAME
*     sge_ar_list_conflicts_with_calendar() -- checks if the given calendar
*                                              conflicts with AR open time frame
*
*  SYNOPSIS
*     bool sge_ar_list_conflicts_with_calendar(lList **answer_list, const char 
*     *qinstance_name, lListElem *cal_ep, lList *master_ar_list) 
*
*  FUNCTION
*     Iteraters over all existing Advance Reservations reserved queues and verifies
*     that the new calender does not invalidate the AR if the queue was reserved
*
*  INPUTS
*     lList **answer_list        - answer list
*     const char *qinstance_name - qinstance name the calendar was configured
*     lListElem *cal_ep          - the calendar object (CAL_Type)
*     lList *master_ar_list      - master AR list
*
*  RESULT
*     bool - true if conflicts
*            false if OK
*
*  NOTES
*     MT-NOTE: sge_ar_list_conflicts_with_calendar() is MT safe 
*******************************************************************************/
bool
sge_ar_list_conflicts_with_calendar(lList **answer_list, const char *qinstance_name, const lListElem *cal_ep,
                                    const lList *master_ar_list) {
   const lListElem *ar;

   DENTER(TOP_LAYER);

   for_each_ep(ar, master_ar_list) {
      if (lGetElemStr(lGetList(ar, AR_granted_slots), JG_qname, qinstance_name)) {
         u_long64 start_time = lGetUlong64(ar, AR_start_time);
         u_long64 duration = lGetUlong64(ar, AR_duration);

         if (!calendar_open_in_time_frame(cal_ep, start_time, duration)) {
            ERROR(MSG_PARSE_MOD2_REJECTED_DUE_TO_AR_SSU, lGetString(cal_ep, CAL_name), SGE_ATTR_CALENDAR, lGetUlong(ar, AR_id));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/****** sge_advance_reservation_qmaster/sge_ar_state_set_running() *************
*  NAME
*     sge_ar_state_set_running() -- set ar in running state
*
*  SYNOPSIS
*     void sge_ar_state_set_running(lListElem *ar) 
*
*  FUNCTION
*     Sets the AR state to running. A running state can result in error state
*     if one of the reserved queues is unable to run a job. This is covered by the
*     function
*
*  INPUTS
*     lListElem *ar - advance reservation object (AR_Type)
*
*  NOTES
*     MT-NOTE: sge_ar_state_set_running() is MT safe 
*
*  SEE ALSO
*     sge_advance_reservation_qmaster/sge_ar_state_set_exited()
*     sge_advance_reservation_qmaster/sge_ar_state_set_deleted()
*     sge_advance_reservation_qmaster/sge_ar_state_set_waiting()
*******************************************************************************/
void
sge_ar_state_set_running(lListElem *ar) {
   u_long32 old_state = lGetUlong(ar, AR_state);

   if (old_state == AR_DELETED || old_state == AR_EXITED) {
      return;
   }

   if (sge_ar_has_errors(ar)) {
      lSetUlong(ar, AR_state, AR_ERROR);
      if (old_state != AR_WARNING && old_state != lGetUlong(ar, AR_state)) {
         /* state change from "running" to "error" */
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_UNSATISFIED, "AR resources unsatisfied", sge_get_gmt64());
         sge_ar_send_mail(ar, MAIL_AT_ABORT);
      } else if (old_state != lGetUlong(ar, AR_state)) {
         /* state change from "warning" to "error" */
         sge_ar_send_mail(ar, MAIL_AT_ABORT);
      }
   } else {
      lSetUlong(ar, AR_state, AR_RUNNING);
      if (old_state != AR_WAITING && old_state != lGetUlong(ar, AR_state)) {
         /* state change from "error" to "running" */
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_OK, "AR resources satisfied", sge_get_gmt64());
         sge_ar_send_mail(ar, MAIL_AT_ABORT);
      }
   }
}

/****** sge_advance_reservation_qmaster/sge_ar_state_set_waiting() *************
*  NAME
*     sge_ar_state_set_waiting() -- set ar in running state
*
*  SYNOPSIS
*     void sge_ar_state_set_waiting(lListElem *ar) 
*
*  FUNCTION
*     Sets the AR state to waiting. A waiting state can result in warning state
*     if one of the reserved queues is unable to run a job. This is covered by the
*     function
*
*  INPUTS
*     lListElem *ar - advance reservation object (AR_Type)
*
*  NOTES
*     MT-NOTE: sge_ar_state_set_waiting() is MT safe 
*
*  SEE ALSO
*     sge_advance_reservation_qmaster/sge_ar_state_set_exited()
*     sge_advance_reservation_qmaster/sge_ar_state_set_deleted()
*     sge_advance_reservation_qmaster/sge_ar_state_set_running()
*******************************************************************************/
void
sge_ar_state_set_waiting(lListElem *ar) {
   u_long32 old_state = lGetUlong(ar, AR_state);

   if (old_state == AR_DELETED || old_state == AR_EXITED) {
      return;
   }

   if (sge_ar_has_errors(ar)) {
      lSetUlong(ar, AR_state, AR_WARNING);
      if (old_state != lGetUlong(ar, AR_state)) {
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_UNSATISFIED, "AR resources unsatisfied", sge_get_gmt64());
      }
   } else {
      lSetUlong(ar, AR_state, AR_WAITING);
      if (old_state != lGetUlong(ar, AR_state)) {
         ocs::ReportingFileWriter::create_ar_log_records(nullptr, ar, ARL_OK, "AR resources satisfied", sge_get_gmt64());
      }
   }
}

/****** sge_advance_reservation_qmaster/sge_ar_state_set_deleted() *************
*  NAME
*     sge_ar_state_set_deleted() -- sets AR into deleted state
*
*  SYNOPSIS
*     void sge_ar_state_set_deleted(lListElem *ar) 
*
*  FUNCTION
*     Sets the AR state to deleted
*
*  INPUTS
*     lListElem *ar - advance reservation object (AR_Type)
*
*  NOTES
*     MT-NOTE: sge_ar_state_set_deleted() is MT safe 
*
*  SEE ALSO
*     sge_advance_reservation_qmaster/sge_ar_state_set_exited()
*     sge_advance_reservation_qmaster/sge_ar_state_set_waiting()
*     sge_advance_reservation_qmaster/sge_ar_state_set_running()
*******************************************************************************/
void
sge_ar_state_set_deleted(lListElem *ar) {
   lSetUlong(ar, AR_state, AR_DELETED);
}

/****** sge_advance_reservation_qmaster/sge_ar_state_set_exited() **************
*  NAME
*     sge_ar_state_set_exited() -- sets AR into exited state
*
*  SYNOPSIS
*     void sge_ar_state_set_exited(lListElem *ar) 
*
*  FUNCTION
*     Sets the AR state to deleted
*
*  INPUTS
*     lListElem *ar - advance reservation object (AR_Type)
*
*  NOTES
*     MT-NOTE: sge_ar_state_set_exited() is MT safe 
*
*  SEE ALSO
*     sge_advance_reservation_qmaster/sge_ar_state_set_deleted()
*     sge_advance_reservation_qmaster/sge_ar_state_set_waiting()
*     sge_advance_reservation_qmaster/sge_ar_state_set_running()
*******************************************************************************/
void
sge_ar_state_set_exited(lListElem *ar) {
   lSetUlong(ar, AR_state, AR_EXITED);
}

/****** sge_advance_reservation_qmaster/sge_ar_list_set_error_state() **********
*  NAME
*     sge_ar_list_set_error_state() -- Set/unset all ARs reserved in a specific queue
*                                      into error state
*
*  SYNOPSIS
*     void sge_ar_list_set_error_state(lList *ar_list, const char *qname, 
*     u_long32 error_type, bool send_events, bool set_error) 
*
*  FUNCTION
*     The function sets/unsets all ARs that reserved in a queue in the error state and
*     generates the error messages for qrstat -explain
*     
*
*  INPUTS
*     lList *ar_list      - master advance reservation list
*     const char *qname   - queue name
*     u_long32 error_type - error type
*     bool send_events    - send events?
*     bool set_error      - set or unset
*
*  NOTES
*     MT-NOTE: sge_ar_list_set_error_state() is MT safe 
*******************************************************************************/
void
sge_ar_list_set_error_state(lList *ar_list, const char *qname, u_long32 error_type, bool set_error, u_long64 gdi_session) {
   lListElem *ar;
   dstring buffer = DSTRING_INIT;

   DENTER(TOP_LAYER);

   for_each_rw(ar, ar_list) {
      lListElem *qinstance;
      const lList *granted_slots = lGetList(ar, AR_reserved_queues);

      if ((qinstance = lGetElemStrRW(granted_slots, QU_full_name, qname)) != nullptr) {
         u_long32 old_errors = lGetUlong(ar, AR_qi_errors);
         u_long32 new_errors;

         if (set_error) {
            new_errors = old_errors + 1;
            sge_dstring_sprintf(&buffer, MSG_AR_RESERVEDQUEUEHASERROR_SS, qname,
                                qinstance_state_as_string(error_type));
         } else {
            new_errors = old_errors - 1;
         }
         lSetUlong(ar, AR_qi_errors, new_errors);

         qinstance_set_error(qinstance, error_type, sge_dstring_get_string(&buffer), set_error);

         /* update states */
         if (old_errors == 0 || new_errors == 0) {
            if ((lGetUlong(ar, AR_state) == AR_RUNNING || lGetUlong(ar, AR_state) == AR_ERROR)) {
               sge_ar_state_set_running(ar);
            } else {
               sge_ar_state_set_waiting(ar);
            }
            /* this info is not spooled */
            sge_add_event(0, sgeE_AR_MOD, lGetUlong(ar, AR_id), 0,
                          nullptr, nullptr, nullptr, ar, gdi_session);
         }
      }
   }

   sge_dstring_free(&buffer);
   DRETURN_VOID;
}

/****** sge_advance_reservation_qmaster/sge_ar_send_mail() *********************
*  NAME
*     sge_ar_send_mail() -- send mail for advance reservation state change
*
*  SYNOPSIS
*     static void sge_ar_send_mail(lListElem *ar, int type) 
*
*  FUNCTION
*     Create and send mail for a specific event
*
*  INPUTS
*     lListElem *ar - advance reservation object (AR_Type)
*     int type      - event type
*
*  NOTES
*     MT-NOTE: sge_ar_send_mail() is MT safe 
*******************************************************************************/
static void
sge_ar_send_mail(lListElem *ar, int type) {
   dstring buffer = DSTRING_INIT;
   dstring subject = DSTRING_INIT;
   dstring body = DSTRING_INIT;
   u_long32 ar_id;
   const char *ar_name;
   const char *mail_type = nullptr;

   DENTER(TOP_LAYER);

   if (!VALID(type, lGetUlong(ar, AR_mail_options))) {
      sge_dstring_append_mailopt(&buffer, type);
      DPRINTF("mailopt %s was not requested\n", sge_dstring_get_string(&buffer));
      sge_dstring_free(&subject);
      sge_dstring_free(&body);
      sge_dstring_free(&buffer);
      DRETURN_VOID;
   }

   ar_id = lGetUlong(ar, AR_id);
   ar_name = lGetString(ar, AR_name);

   switch (type) {
      case MAIL_AT_BEGINNING:
         sge_ctime64(lGetUlong64(ar, AR_start_time), &buffer);
         sge_dstring_sprintf(&subject, MSG_MAIL_ARSTARTEDSUBJ_US,
                             ar_id, ar_name ? ar_name : "none");
         sge_dstring_sprintf(&body, MSG_MAIL_ARSTARTBODY_USSS,
                             ar_id, ar_name ? ar_name : "none", lGetString(ar, AR_owner),
                             sge_dstring_get_string(&buffer));
         mail_type = MSG_MAIL_TYPE_ARSTART;
         break;
      case MAIL_AT_EXIT:
         if (lGetUlong(ar, AR_state) == AR_DELETED) {
            sge_ctime64(sge_get_gmt64(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_ARDELETEDSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_ARDELETETBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", lGetString(ar, AR_owner),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_ARDELETE;
         } else {
            sge_ctime64(lGetUlong64(ar, AR_end_time), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_AREXITEDSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_AREXITBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", lGetString(ar, AR_owner),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_AREND;
         }
         break;
      case MAIL_AT_ABORT:
         if (lGetUlong(ar, AR_state) == AR_ERROR) {
            sge_ctime64(sge_get_gmt64(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_ARERRORSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_ARERRORBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", lGetString(ar, AR_owner),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_ARERROR;
         } else {
            sge_ctime64(sge_get_gmt64(), &buffer);
            sge_dstring_sprintf(&subject, MSG_MAIL_AROKSUBJ_US,
                                ar_id, ar_name ? ar_name : "none");
            sge_dstring_sprintf(&body, MSG_MAIL_AROKBODY_USSS,
                                ar_id, ar_name ? ar_name : "none", lGetString(ar, AR_owner),
                                sge_dstring_get_string(&buffer));
            mail_type = MSG_MAIL_TYPE_AROK;
         }
         break;
      default:
         /* should never happen */
         break;
   }

   cull_mail(QMASTER, lGetList(ar, AR_mail_list), sge_dstring_get_string(&subject), sge_dstring_get_string(&body),
             mail_type);

   sge_dstring_free(&buffer);
   sge_dstring_free(&subject);
   sge_dstring_free(&body);

   DRETURN_VOID;
}

/****** sge_advance_reservation_qmaster/ar_list_has_reservation_due_to_qinstance_complex_attr() ******
*  NAME
*     ar_list_has_reservation_due_to_qinstance_complex_attr() -- check
*        if change of complex values is valid concerning ar 
*
*  SYNOPSIS
*     bool ar_list_has_reservation_due_to_qinstance_complex_attr(
*        lList *ar_master_list, 
*        lList **answer_list, 
*        lListElem *qinstance, 
*        lList *ce_master_list) 
*
*  FUNCTION
*     Check if the modification of the complex_values of a qinstance
*     whould break existing advance reservations 
*
*  INPUTS
*     lList *ar_master_list - master AR list 
*     lList **answer_list   - answer list 
*     lListElem *qinstance  - qinstance 
*     lList *ce_master_list - master centry list 
*
*  RESULT
*     bool 
*        true - modification is not allowed
*        false - modification is allowed
*
*  NOTES
*     MT-NOTE: ar_list_has_reservation_due_to_qinstance_complex_attr() is  
*     MT safe 
*******************************************************************************/
bool
ar_list_has_reservation_due_to_qinstance_complex_attr(const lList *ar_master_list, lList **answer_list,
                                                      lListElem *qinstance, const lList *ce_master_list) {
   const lListElem *ar;
   const lListElem *gs;

   DENTER(TOP_LAYER);

   for_each_ep(ar, ar_master_list) {
      const char *qinstance_name = lGetString(qinstance, QU_full_name);

      if ((gs = lGetElemStr(lGetList(ar, AR_granted_slots), JG_qname, qinstance_name))) {

         const lListElem *rue = nullptr;
         lListElem *request;
         const lList *rue_list;

         for_each_rw(request, lGetList(ar, AR_resource_list)) {
            const char *ce_name = lGetString(request, CE_name);
            const lListElem *ce = lGetElemStr(ce_master_list, CE_name, ce_name);
            bool is_consumable = (lGetUlong(ce, CE_consumable) > 0) ? true : false;

            if (!is_consumable) {
               char text[2048];
               u_long32 slots = lGetUlong(gs, JG_slots);
               lListElem *current = lGetSubStrRW(qinstance, CE_name, ce_name, QU_consumable_config_list);
               if (current != nullptr) {
                  current = lCopyElem(current);
                  lSetUlong(current, CE_relop, lGetUlong(ce, CE_relop));
                  lSetDouble(current, CE_pj_doubleval, lGetDouble(current, CE_doubleval));
                  lSetString(current, CE_pj_stringval, lGetString(current, CE_stringval));

                  if (compare_complexes(slots, request, current, text, false, true) == 0) {
                     ERROR(MSG_QUEUE_MODCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     lFreeElem(&current);
                     DRETURN(true);
                  }
                  lFreeElem(&current);
               }
            }
         }

         /* now it gets expensive. Before we can start the check at first we have to build the
            consumable config list. */
         qinstance_reinit_consumable_actual_list(qinstance, answer_list);
         rue_list = lGetList(qinstance, QU_resource_utilization);

         for_each_ep(rue, rue_list) {
            const char *ce_name = lGetString(rue, RUE_name);
            const lListElem *ce = lGetElemStr(ce_master_list, CE_name, ce_name);
            bool is_consumable = (lGetUlong(ce, CE_consumable) > 0) ? true : false;

            if (is_consumable) {
               const lListElem *rde = nullptr;
               const lList *rde_list = lGetList(rue, RUE_utilized);
               const lListElem *cv = lGetSubStr(qinstance, CE_name, ce_name, QU_consumable_config_list);

               if (cv == nullptr) {
                  ERROR(MSG_QUEUE_MODNOCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  DRETURN(true);
               } else {
                  double configured = lGetDouble(cv, CE_doubleval);

                  for_each_ep(rde, rde_list) {
                     double amount = lGetDouble(rde, RDE_amount);

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

/****** sge_advance_reservation_qmaster/ar_list_has_reservation_due_to_host_complex_attr() ******
*  NAME
*     ar_list_has_reservation_due_to_host_complex_attr() -- check
*        if change of complex values is valid concerning ar 
*
*  SYNOPSIS
*     bool ar_list_has_reservation_due_to_host_complex_attr(
*        lList  *ar_master_list, 
*        lList **answer_list, 
*        lListElem *host, 
*        lList *ce_master_list) 
*
*  FUNCTION
*      Check if the modification of the complex_values of a host
*      whould break existing advance reservations.
*
*  INPUTS
*     lList *ar_master_list - master AR list 
*     lList **answer_list   - AN_Type list 
*     lListElem *host       - host 
*     lList *ce_master_list - master centry list 
*
*  RESULT
*     bool 
*        true - modification is not allowed
*        false - modification is allowed
*
*  NOTES
*     MT-NOTE: ar_list_has_reservation_due_to_host_complex_attr() is MT 
*     safe 
*******************************************************************************/
bool
ar_list_has_reservation_due_to_host_complex_attr(const lList *ar_master_list, lList **answer_list,
                                                 lListElem *host, const lList *ce_master_list) {
   const lListElem *ar = nullptr;
   const char *hostname = lGetHost(host, EH_name);

   DENTER(TOP_LAYER);

   for_each_ep(ar, ar_master_list) {
      const lListElem *gs = nullptr;

      for_each_ep(gs, lGetList(ar, AR_granted_slots)) {
         const char *gh = lGetHost(gs, JG_qhostname);

         if (!sge_hostcmp(gh, hostname)) {
            const lListElem *rue;
            lListElem *request;
            const lList *rue_list = lGetList(host, EH_resource_utilization);

            for_each_rw(request, lGetList(ar, AR_resource_list)) {
               const char *ce_name = lGetString(request, CE_name);
               const lListElem *ce = lGetElemStr(ce_master_list, CE_name, ce_name);
               bool is_consumable = (lGetUlong(ce, CE_consumable) > 0) ? true : false;

               if (!is_consumable) {
                  char text[2048];
                  u_long32 slots = lGetUlong(gs, JG_slots);
                  const lListElem *old_current = lGetSubStr(host, CE_name, ce_name, EH_consumable_config_list);
                  if (old_current != nullptr) {
                     lListElem *current = lCopyElem(old_current);
                     lSetUlong(current, CE_relop, lGetUlong(ce, CE_relop));
                     lSetDouble(current, CE_pj_doubleval, lGetDouble(current, CE_doubleval));
                     lSetString(current, CE_pj_stringval, lGetString(current, CE_stringval));

                     if (compare_complexes(slots, request, current, text, false, true) == 0) {
                        ERROR(MSG_QUEUE_MODCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                        answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                        lFreeElem(&current);
                        DRETURN(true);
                     }
                     lFreeElem(&current);
                  }
               }
            }
            for_each_ep(rue, rue_list) {
               const char *ce_name = lGetString(rue, RUE_name);
               const lListElem *ce = lGetElemStr(ce_master_list, CE_name, ce_name);
               bool is_consumable = (lGetUlong(ce, CE_consumable) > 0) ? true : false;

               if (is_consumable) {
                  const lListElem *rde = nullptr;
                  const lList *rde_list = lGetList(rue, RUE_utilized);
                  const lListElem *cv = lGetSubStr(host, CE_name, ce_name, EH_consumable_config_list);

                  if (cv == nullptr) {
                     ERROR(MSG_QUEUE_MODNOCMPLXDENYDUETOAR_SS, ce_name, SGE_ATTR_COMPLEX_VALUES);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     DRETURN(true);
                  } else {
                     double configured = lGetDouble(cv, CE_doubleval);

                     for_each_ep(rde, rde_list) {
                        double amount = lGetDouble(rde, RDE_amount);

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
