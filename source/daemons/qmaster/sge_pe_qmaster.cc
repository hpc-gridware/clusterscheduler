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
 * @brief Parallel environments, and the slots booked against them
 */
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "uti/config_file.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "sched/sge_job_schedd.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_pe_qmaster.h"
#include "evm/sge_event_master.h"
#include "sge_userset_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"


static char object_name[] = "parallel environment";

static void pe_update_categories(ocs::tom::ConstElement<ocs::tom::PE> new_pe,
                                 ocs::tom::ConstElement<ocs::tom::PE> old_pe, uint64_t gdi_session);

/** @brief Apply one attribute change to a parallel environment
 *
 * The gdi_object_t::modifier for parallel environments; see sge_c_gdi.h for the sequence it is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param raw_new_pe the parallel environment as it is to become (PE_Type)
 * @param raw_pe the reduced element the client sent (PE_Type)
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
pe_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *raw_new_pe,
       lListElem *raw_pe, /* reduced */
       int add, const char *ruser, const char *rhost, gdi_object_t *object,
       ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
       monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // the signature is fixed by gdi_object_t::modifier; both elements are parallel environments
   // (the gdi object table and the listener's structure check of the request data guarantee it)
   const ocs::tom::Element<ocs::tom::PE> new_pe(raw_new_pe);
   const ocs::tom::Element<ocs::tom::PE> pe(raw_pe);
   int ret;
   const char *s, *pe_name;
   const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
   const ocs::tom::ConstList<ocs::tom::AR> master_ar_list(*ocs::DataStore::get_master_list(SGE_TYPE_AR));

   /* ---- PE_name */
   if (add) {
      // a nullptr name is refused by verify_obj_name() below
      ocs::tom::copy_if_present<PE_name>(new_pe, pe);
   }
   pe_name = ocs::tom::get<PE_name>(new_pe);

   /* Name has to be a valid filename without pathchanges */
   if (add && verify_obj_name(alpp, pe_name, MAX_VERIFY_STRING, MSG_OBJ_PE) != STATUS_OK) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ---- PE_slots */
   if (ocs::tom::has<PE_slots>(pe)) {
      uint32_t pe_slots = ocs::tom::get<PE_slots>(pe);

      if (pe_validate_slots(alpp, pe_slots, false) != STATUS_OK) {
         goto ERROR;
      }

      if (ar_list_has_reservation_for_pe_with_slots(master_ar_list, alpp, pe_name, pe_slots)) {
         goto ERROR;
      }
   }
   ocs::tom::copy_if_present<PE_slots>(new_pe, pe);

   /* ---- PE_control_slaves */
   ocs::tom::copy_if_present<PE_control_slaves>(new_pe, pe);

   /* ---- PE_job_is_first_task */
   ocs::tom::copy_if_present<PE_job_is_first_task>(new_pe, pe);

   /* ---- PE_user_list */
   if (ocs::tom::has<PE_user_list>(pe)) {
      DPRINTF("got new PE_user_list\n");
      /* check user_lists */
      normalize_sublist<PE_user_list>(pe);
      if (userset_list_validate_acl_list(ocs::tom::get<PE_user_list>(pe), alpp, master_userset_list) != STATUS_OK) {
         goto ERROR;
      }

      attr_mod_sub_list(alpp, new_pe, PE_user_list, US_name, pe, cmd, sub_command,
                        SGE_ATTR_USER_LISTS, SGE_OBJ_PE, 0, nullptr);
   }

   /* ---- PE_xuser_list */
   if (ocs::tom::has<PE_xuser_list>(pe)) {
      DPRINTF("got new QU_axcl\n");
      /* check xuser_lists */
      normalize_sublist<PE_xuser_list>(pe);
      if (userset_list_validate_acl_list(ocs::tom::get<PE_xuser_list>(pe), alpp, master_userset_list) != STATUS_OK) {
         goto ERROR;
      }
      attr_mod_sub_list(alpp, new_pe, PE_xuser_list, US_name, pe, cmd, sub_command,
                        SGE_ATTR_XUSER_LISTS, SGE_OBJ_PE, 0, nullptr);
   }

   if (ocs::tom::has<PE_xuser_list>(pe) ||
       ocs::tom::has<PE_user_list>(pe)) {
      if (multiple_occurances<US_name>(alpp, ocs::tom::get<PE_user_list>(new_pe),
                                       ocs::tom::get<PE_xuser_list>(new_pe), pe_name, object_name)) {
         goto ERROR;
      }
   }

   if (attr_mod_procedure<PE_start_proc_args>(alpp, pe, new_pe, "start_proc_args", pe_variables)) {
      goto ERROR;
   }
   if (attr_mod_procedure<PE_stop_proc_args>(alpp, pe, new_pe, "stop_proc_args", pe_variables)) {
      goto ERROR;
   }

   /* -------- PE_allocation_rule */
   if (ocs::tom::has<PE_allocation_rule>(pe)) {
      s = ocs::tom::get<PE_allocation_rule>(pe);
      if (!pe_validate_allocation_rule(alpp, s, true)) {
         DRETURN(STATUS_EEXIST);
      }
      ocs::tom::set<PE_allocation_rule>(new_pe, s);
   }

   /* -------- PE_urgency_slots */
   if (ocs::tom::has<PE_urgency_slots>(pe)) {
      s = ocs::tom::get<PE_urgency_slots>(pe);
      if (s == nullptr) {
         ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(PE_urgency_slots), "validate_pe");
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }

      if ((ret = pe_validate_urgency_slots(alpp, s)) != STATUS_OK) {
         DRETURN(ret);
      }
      ocs::tom::set<PE_urgency_slots>(new_pe, s);
   }

   /* ---- PE_accounting_summary */
   ocs::tom::copy_if_present<PE_accounting_summary>(new_pe, pe);
   /* ---- PE_ignore_slave_requests_on_master_host */
   ocs::tom::copy_if_present<PE_ignore_slave_requests_on_master_host>(new_pe, pe);
   /* ---- PE_master_forks_slaves */
   ocs::tom::copy_if_present<PE_master_forks_slaves>(new_pe, pe);
   /* ---- PE_daemon_forks_slaves */
   ocs::tom::copy_if_present<PE_daemon_forks_slaves>(new_pe, pe);

   /* -------- PE_resource_utilization */
   if (add) {
      if (pe_set_slots_used(new_pe, 0)) {
         ERROR(SFNMAX, MSG_MEM_MALLOC);
         answer_list_add(alpp, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EMALLOC);
      }
   }

   DRETURN(0);

   ERROR:
   DRETURN(STATUS_EUNKNOWN);
}

/** @brief Write a parallel environment to the spool
 *
 * The gdi_object_t::writer for parallel environments.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param pep the parallel environment
 * @param object the table entry for this object type
 * @return 0 on success
 */
int
pe_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *pep,
         gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   bool dbret;

   dbret = spool_write_object(&answer_list, spool_get_default_context(), pep,
                              ocs::tom::get<PE_name>(pep), SGE_TYPE_PE);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, ocs::tom::get<PE_name>(pep));
   }

   DRETURN(dbret ? 0 : 1);
}

/** @brief Announce a parallel environment change once it is safely spooled
 *
 * The gdi_object_t::on_success for parallel environments.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the object
 * @param old_ep the object as it was, or nullptr on add
 * @param object the table entry for this object type
 * @param ppList receives information for post processing
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int pe_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
               lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *pe_name;

   pe_name = ocs::tom::get<PE_name>(ep);

   pe_update_categories(ocs::tom::ConstElement<ocs::tom::PE>(ep), ocs::tom::ConstElement<ocs::tom::PE>(old_ep),
                        packet->gdi_session);

   sge_add_event(0, old_ep ? sgeE_PE_MOD : sgeE_PE_ADD, 0, 0, pe_name, nullptr, nullptr, ep, packet->gdi_session);

   DRETURN(0);
}

/** @brief Delete a parallel environment, unless a queue still references it
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param pep the parallel environment (`PE_Type`)
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_del_pe(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::PE> pep,
           ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list(SGE_TYPE_JOB));
   const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE));
   const ocs::tom::List<ocs::tom::PE> master_pe_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_PE));

   if (!pep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (!ocs::tom::has<PE_name>(pep)) {
      ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(PE_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *pe = ocs::tom::get<PE_name>(pep);
   if (!pe) {
      ERROR(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const auto ep = pe_list_locate(master_pe_list, pe);
   if (!ep) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, object_name, pe);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* 
    * Try to find references in other objects
    */
   {
      ocs::tom::ListPtr<ocs::tom::AN> local_answer_list;

      if (pe_is_referenced(ep, &local_answer_list, master_job_list, master_cqueue_list)) {
         const lListElem *answer = lFirst(local_answer_list);

         ERROR("denied: %s", ocs::tom::get<AN_text>(answer));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         local_answer_list.reset();
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* remove host file */
   if (!sge_event_spool(alpp, 0, sgeE_PE_DEL,
                        0, 0, pe, nullptr, nullptr, nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {
      ERROR(MSG_CANTSPOOL_SS, object_name, pe);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   pe_update_categories(nullptr, ep, packet->gdi_session);

   /* delete found pe element */
   master_pe_list.erase(ep);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, pe, object_name);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}


/** @brief Rebuild every parallel environment's slot booking from the running jobs
 *
 * @param pe_list the parallel environments
 */
void
debit_all_jobs_from_pes(const ocs::tom::List<ocs::tom::PE> pe_list) {
   DENTER(TOP_LAYER);
   const ocs::tom::ConstList<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list(SGE_TYPE_JOB));

   for (const auto pep : pe_list) {
      pe_set_slots_used(pep, 0);
      const char *pe_name = ocs::tom::get<PE_name>(pep);
      DPRINTF("debiting from pe %s:\n", pe_name);

      for (const auto jep : master_job_list) {
         if (ocs::tom::get<JB_pe>(jep) != nullptr) { /* is job  parallel */
            int slots = 0;
            for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
               if ((ISSET(ocs::tom::get<JAT_status>(jatep), JRUNNING) ||      /* is job running  */
                    ISSET(ocs::tom::get<JAT_status>(jatep), JTRANSFERING)) && /* or transfering  */
                   !sge_strnullcmp(pe_name, ocs::tom::get<JAT_granted_pe>(jatep))) {/* this pe         */
                  slots += sge_granted_slots(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep));
               }
            }
            pe_debit_slots(pep, slots, ocs::tom::get<JB_job_number>(jep));
         }
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Add the names of the usersets of a PE's user_lists and xuser_lists to a list, each once
 *
 * @param pe the PE (PE_Type)
 * @param acl the list of userset references (US_Type), created when it is empty and a name is added
 */
static void
ocs_pe_collect_usersets(const ocs::tom::ConstElement<ocs::tom::PE> pe, ocs::tom::ListPtr<ocs::tom::US> &acl) {
   DENTER(TOP_LAYER);

   for (const auto user_list : {ocs::tom::get<PE_user_list>(pe), ocs::tom::get<PE_xuser_list>(pe)}) {
      for (const auto ep : user_list) {
         const char *u = ocs::tom::get<US_name>(ep);
         if (!acl) {
            acl = ocs::tom::ListPtr<ocs::tom::US>::create("");
         }
         if (!acl->find_first<US_name>(u)) {
            acl->emplace_back<US_name>(u);
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Diff old/new PE usersets
 *
 * A diff new/old is made regarding PE acl/xacl.
 * Userset references are returned in new_acl/old_acl.
 * Names in both lists are removed from both; a list that becomes empty then is freed, as lDiffListStr() did.
 *
 * @param new_pe New PE (PE_Type), or an empty handle
 * @param old_pe Old PE (PE_Type), or an empty handle
 * @param new_acl New userset references (US_Type), or nullptr
 * @param old_acl Old userset references (US_Type), or nullptr
 *
 * @note MT-NOTE: pe_diff_usersets() is not MT safe
 */
void
pe_diff_usersets(const ocs::tom::ConstElement<ocs::tom::PE> new_pe, const ocs::tom::ConstElement<ocs::tom::PE> old_pe,
                 ocs::tom::ListPtr<ocs::tom::US> *new_acl, ocs::tom::ListPtr<ocs::tom::US> *old_acl) {
   DENTER(TOP_LAYER);

   if (old_pe && old_acl) {
      ocs_pe_collect_usersets(old_pe, *old_acl);
   }

   if (new_pe && new_acl) {
      ocs_pe_collect_usersets(new_pe, *new_acl);
   }

   if (new_acl != nullptr && old_acl != nullptr && *new_acl && *old_acl) {
      const std::size_t new_size = (*new_acl)->size();
      (*new_acl)->remove_common<US_name>(**old_acl);
      if ((*new_acl)->size() != new_size) {
         // lDiffListStr() freed a list that a removal emptied
         if ((*new_acl)->empty()) {
            new_acl->reset();
         }
         if ((*old_acl)->empty()) {
            old_acl->reset();
         }
      }
   }

   DRETURN_VOID;
}



/**
 * @brief Update categories wrts userset
 *
 * The userset information wrts categories is updated based
 *  on new/old PE configuration and events are sent upon changes.
 *
 * @param new_pe New PE (PE_Type), or an empty handle
 * @param old_pe Old PE (PE_Type), or an empty handle
 * @param gdi_session the GDI session of the request, for the events
 *
 * @note MT-NOTE: pe_update_categories() is not MT safe
 */
static void
pe_update_categories(const ocs::tom::ConstElement<ocs::tom::PE> new_pe,
                     const ocs::tom::ConstElement<ocs::tom::PE> old_pe, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::US> old_lp;
   ocs::tom::ListPtr<ocs::tom::US> new_lp;

   pe_diff_usersets(new_pe, old_pe, &new_lp, &old_lp);
   userset_update_categories(*new_lp, *old_lp, gdi_session);

   DRETURN_VOID;
}
