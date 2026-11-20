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
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Complex entries: the resources a cluster knows about
 */
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_STU.h"

#include "sched/debit.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_c_gdi.h"
#include "sge_persistence_qmaster.h"
#include "sge_centry_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"


/** @brief Apply one attribute change to a complex entry
 *
 * Copies every attribute the (possibly reduced) request element holds into the complex
 * entry; the slots entry keeps its fixed type, relop, requestable, consumable and default.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param ce the complex entry to change
 * @param reduced the request element, possibly reduced
 * @param add 1 for add, 0 for modify
 * @param remote_user the requesting user
 * @param remote_host the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success, STATUS_EUNKNOWN on error
 */
int
centry_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
           const ocs::tom::Element<ocs::tom::CE> ce, const ocs::tom::ConstElement<ocs::tom::CE> reduced, const int add,
           const char *remote_user, const char *remote_host, gdi_object_t *object, const ocs::gdi::Command cmd,
           const ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool ret = true;
   bool is_slots_attr = false;

   double dval;
   char error_msg[200];

   /*
    * At least the centry name has to be available (CE_name)
    */
   if (ret && ocs::tom::has<CE_name>(reduced)) {
      const char *name = ocs::tom::get<CE_name>(reduced);

      DPRINTF("Got CE_name: " SFQ "\n", name);
      ocs::tom::set<CE_name>(ce, name);
      if (!strcmp(SGE_ATTR_SLOTS, name)) {
         is_slots_attr = true;
      }
   }

   /*
    * Shortcut (CE_shortcut)
    */
   if (ret && ocs::tom::has<CE_shortcut>(reduced)) {
      const char *shortcut = ocs::tom::get<CE_shortcut>(reduced);

      DPRINTF("Got CE_shortcut: " SFQ "\n", shortcut ? shortcut : "-NA-");
      ocs::tom::set<CE_shortcut>(ce, shortcut);
   }

   /*
    * Type (CE_valtype)
    */
   if (ret && ocs::tom::has<CE_valtype>(reduced)) {
      uint32_t type = ocs::tom::get<CE_valtype>(reduced);

      if (is_slots_attr) {
         type = static_cast<uint32_t>(ocs::CEntry::Type::INT);
      }
      DPRINTF("Got CE_valtype: " sge_u32 "\n", type);
      ocs::tom::set<CE_valtype>(ce, type);
   }

   /*
    * Operator (CE_relop)
    */
   if (ret && ocs::tom::has<CE_relop>(reduced)) {
      uint32_t relop = ocs::tom::get<CE_relop>(reduced);

      if (is_slots_attr) {
         relop = CMPLXLE_OP;
      }
      DPRINTF("Got CE_relop: " sge_u32 "\n", relop);
      ocs::tom::set<CE_relop>(ce, relop);
   }

   /*
    * Requestable (CE_request)
    */
   if (ret && ocs::tom::has<CE_requestable>(reduced)) {
      uint32_t request = ocs::tom::get<CE_requestable>(reduced);

      if (is_slots_attr) {
         request = REQU_YES;
      }
      DPRINTF("Got CE_requestable: " sge_u32 "\n", request);
      ocs::tom::set<CE_requestable>(ce, request);
   }

   /*
    * Consumable (CE_consumable)
    */
   if (ret && ocs::tom::has<CE_consumable>(reduced)) {
      uint32_t consumable = ocs::tom::get<CE_consumable>(reduced);

      if (is_slots_attr) {
         consumable = CONSUMABLE_YES;
      }
      DPRINTF("Got CE_consumable: " sge_u32 "\n", consumable);
      ocs::tom::set<CE_consumable>(ce, consumable);
   }

   /*
    * Default (CE_defaultval)
    */
   if (ret && ocs::tom::has<CE_defaultval>(reduced)) {
      const char *defaultval = ocs::tom::get<CE_defaultval>(reduced);

      if (is_slots_attr) {
         defaultval = "1";
      }
      DPRINTF("Got CE_defaultval: " SFQ "\n", defaultval ? defaultval : "-NA-");
      ocs::tom::set<CE_defaultval>(ce, defaultval);
   }

   /*
    * Default (CE_urgency_weight)
    */
   if (ret && ocs::tom::has<CE_urgency_weight>(reduced)) {
      const char *urgency_weight = ocs::tom::get<CE_urgency_weight>(reduced);
      DPRINTF("Got CE_defaultval: " SFQ "\n", urgency_weight ? urgency_weight : "-NA-");

      /* Check that the entry is valid */
      const char *attrname = ocs::tom::get<CE_name>(reduced);
      if (!parse_ulong_val(&dval, nullptr, ocs::CEntry::Type::DOUBLE, urgency_weight, error_msg, 199)) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_INVALID_CENTRY_PARSE_URGENCY_SS, attrname, error_msg);
         ret = false;
      }

      ocs::tom::set<CE_urgency_weight>(ce, urgency_weight);
   }

   if (ret) {
      ret = centry_elem_validate(ce, ocs::tom::ConstList<ocs::tom::CE>(), answer_list, false);
   }

   /*
    * A complex definition carries no value - CE_stringval and CE_doubleval belong to the
    * complex_values of a host or a queue and to a job request, which are built from the same
    * type. Clearing them repairs a definition an earlier version left a value in, which would
    * otherwise make every "qconf -Mc" report the entry as modified.
    */
   if (ret) {
      ocs::tom::set<CE_stringval>(ce, nullptr);
      ocs::tom::set<CE_doubleval>(ce, 0);
   }

   if (ret) {
      DRETURN(0);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

/** @brief Write a complex entry to the spool
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param centry the complex entry to write
 * @param object the table entry for this object type
 * @return 0 on success, 1 if writing failed
 */
int
centry_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
             const ocs::tom::ConstElement<ocs::tom::CE> centry, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   const bool dbret = spool_write_object(&answer_list, spool_get_default_context(), centry,
                                         ocs::tom::get<CE_name>(centry), SGE_TYPE_CENTRY);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, ocs::tom::get<CE_name>(centry));
   }

   DRETURN(dbret ? 0 : 1);
}

/* ------------------------------------------------------------ */

/**
 * @brief Apply the consequences of an added or modified complex entry
 *
 * Called after a centry has been written. A changed complex may alter what
 * every job's resource requests mean, so the resource debitations of all
 * running jobs are re-done against all hosts and queues, and the change is
 * announced as an event. See the `@bug` below for the cost of that.
 *
 * @param centry the new or modified complex entry
 * @param old_centry the previous version, or an empty handle when the entry was added
 * @param object the GDI object description for complex entries
 * @param packet the client request
 * @param task the GDI task being answered
 * @param post collects in `redebit_centries` the names of the complex entries whose consumables are to be
 *        rebuilt (the list is created on first use), or nullptr
 * @param monitor for monitoring qmaster threads
 *
 * @return 0 on success
 *
 * @note MT-NOTE: centry_success() is not MT safe
 *
 * @bug This function is the cause for huge overhead with processing complex
 *      entry change requests: Each change with complex configuration causes
 *      debitations for ALL resources be re-done with all hosts and queues
 *      based on per job resource requests. There should be a chance to notably
 *      lower resource consumption doing this only for those complexes where
 *      changes actually occurred.
 *
 *      There is no need sort centry list each time a change occurs (fixed).
 *
 *      Reporting needs to be updated only for the changed complex entry (minor issue).
 *
 *      No updates have to be done for the ADD operation - the centry cannot be
 *      referenced anywhere at ADD time (fixed).
 *
 *      Update is only required, if the consumable attribute changed or
 *      the centry is a consumable and the default request changed (fixed).
 *
 *      The whole update mechanism wouldn't be required, if the granted resources
 *      would be stored in the job object for running jobs. It is only required,
 *      as the granted resources are not stored in the job and therefore a change
 *      of the default request would result in a wrong number of resources freed
 *      for the consumable at job end.
 *      Storing the granted resources in the job object would have further
 *      advantages: - qstat -j would display the default requests for consumables
 *      - qstat -j would display granted soft requests
 *      - soft requests on consumables could be enabled
 */
int
centry_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CE> centry,
               const ocs::tom::ConstElement<ocs::tom::CE> old_centry, gdi_object_t *object,
               ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool rebuild_consumables = false;

   sge_add_event(0, old_centry ? sgeE_CENTRY_MOD : sgeE_CENTRY_ADD, 0, 0,
                 ocs::tom::get<CE_name>(centry), nullptr, nullptr, centry, packet->gdi_session);

   if (old_centry) {
      /*
       * If a complex has become a consumable, or
       * is no longer a consumable, or
       * it is a consumable and the default value has changed,
       * queue / host values for these consumables have to be rebuilt.
       */
      const uint32_t consumable = ocs::tom::get<CE_consumable>(centry);
      const uint32_t old_consumable = ocs::tom::get<CE_consumable>(old_centry);
      if (consumable != old_consumable) {
         rebuild_consumables = true;
      } else if (consumable) {
         const char *default_request = ocs::tom::get<CE_defaultval>(centry);
         const char *old_default_request = ocs::tom::get<CE_defaultval>(old_centry);
         if (sge_strnullcmp(default_request, old_default_request) != 0) {
            rebuild_consumables = true;
         }
      }
   }

   // a nullptr name adds nothing, as the free emplace_back() on the raw list did
   if (const char *name = ocs::tom::get<CE_name>(centry); rebuild_consumables && post != nullptr && name != nullptr) {
      if (post->redebit_centries == nullptr) {
         post->redebit_centries = ocs::tom::ListPtr<ocs::tom::STU>::create("");
      }
      post->redebit_centries->emplace_back<STU_name>(name);
   }

   DRETURN(0);
}

/** @brief Delete a complex entry, unless something still requests it
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param centry the complex entry (`CE`), only its name is used
 * @param answer_list receives messages for the caller
 * @param remote_user the requesting user
 * @param remote_host the requesting host
 * @return STATUS_OK on success, STATUS_EUNKNOWN on error
 */
int
sge_del_centry(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::CE> centry,
               ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char *remote_user, char *remote_host) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const auto master_centry_list = ocs::DataStore::master_list_rw<SGE_TYPE_CENTRY>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();

   if (centry || remote_user != nullptr || remote_host != nullptr) {
      const char *name = ocs::tom::get<CE_name>(centry);

      if (name != nullptr) {
         ocs::tom::ListPtr<ocs::tom::AN> local_answer_list;
         const auto tmp_centry = centry_list_locate(master_centry_list, name);

         /* check if its a build in value */
         if (get_rsrc(name, true, nullptr, nullptr, nullptr, nullptr) == 0 ||
             get_rsrc(name, false, nullptr, nullptr, nullptr, nullptr) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_DEL_S, name);
            ret = false;
         }

         if (ret) {
            if (tmp_centry) {
               if (!centry_is_referenced(tmp_centry, &local_answer_list, master_cqueue_list, master_ehost_list,
                                         master_rqs_list)) {
                  if (sge_event_spool(answer_list, 0, sgeE_CENTRY_DEL,
                                      0, 0, name, nullptr, nullptr,
                                      nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {

                     master_centry_list.erase(tmp_centry);
                     INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, remote_user, remote_host, name, MSG_OBJ_CPLX);
                     answer_list_add(answer_list, SGE_EVENT,
                                     STATUS_OK, ANSWER_QUALITY_INFO);
                  } else {
                     ERROR(MSG_CANTSPOOL_SS, "complex entry", name);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                     ret = false;
                  }
               } else {
                  const auto answer = local_answer_list->front();

                  ERROR("denied: %s", ocs::tom::get<AN_text>(answer));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                  local_answer_list.reset();
                  ret = false;
               }
            } else {
               ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_CPLX, name);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }
      } else {
         CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, ocs::tom::FieldTraits<CE_name>::info.name, __func__);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   } else {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   if (ret) {
      DRETURN(STATUS_OK);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}


/**
 * @brief Increase the version of every queue instance and announce it and every execution host
 *
 * @param gdi_version the GDI session the change belongs to
 */
static void
sge_change_queue_version_centry(const uint64_t gdi_version) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const auto master_ehost_list = ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();

   for (const auto cqueue : master_cqueue_list) {
      for (const auto qinstance : ocs::tom::get<CQ_qinstances>(cqueue)) {
         qinstance_increase_qversion(qinstance);

         sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD,
                         0, 0, ocs::tom::get<QU_qname>(qinstance),
                         ocs::tom::get<QU_qhostname>(qinstance), nullptr,
                         qinstance, nullptr, nullptr, true, false, gdi_version);
      }
   }
   for (const auto ep : master_ehost_list) {
      sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD,
                      0, 0, ocs::tom::get<EH_name>(ep), nullptr, nullptr,
                      ep, nullptr, nullptr, true, false, gdi_version);
   }
   answer_list_output(&answer_list);

   DRETURN_VOID;
}

/**
 * @brief Recompute consumable debitation
 *
 * Recomputes the complete consumable debitation for all queues, hosts and
 * jobs.
 *
 * @param centries the names (`STU`) of the complex entries that actually require recomputation;
 *                 not evaluated yet, every consumable is recomputed
 * @param gdi_version the GDI session the change belongs to
 *
 * @note MT-NOTE: centry_redebit_consumables() maybe not MT safe (the functions
 *       qinstance_debit_consumable and host_debit_consumable have no MT-NOTE).
 *
 *       TODO: This function could be highly optimized by taking into account the
 *       centry list passed as parameter.
 *       This would not only increase performance by only recomputing the
 *       debitation for only the changed centries (and spooling only the actually
 *       affected queues instead of all), but also reduce the number of scheduling
 *       decisions trashed due to a changed queue version number.
 */
void
centry_redebit_consumables(const ocs::tom::ConstList<ocs::tom::STU> centries, const uint64_t gdi_version) {
   DENTER(TOP_LAYER);

   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
   const auto master_ehost_list = ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>();
   const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();

   /* throw away all old actual values lists and rebuild them from scratch */
   for (const auto cqueue : master_cqueue_list) {
      for (const auto qinstance : ocs::tom::get<CQ_qinstances>(cqueue)) {
         ocs::tom::set<QU_resource_utilization>(qinstance, nullptr);
         qinstance_debit_consumable(qinstance, ocs::tom::ConstElement<ocs::tom::JB>(),
                                    ocs::tom::ConstElement<ocs::tom::PE>(), master_centry_list, 0, true, true,
                                    nullptr);
      }
   }
   for (const auto hep : master_ehost_list) {
      ocs::tom::set<EH_resource_utilization>(hep, nullptr);
      debit_host_consumable(ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::JAT>(),
                            ocs::tom::ConstList<ocs::tom::GRU>(), ocs::tom::ConstElement<ocs::tom::PE>(), hep,
                            master_centry_list, 0, true, true, nullptr);
   }

   /*
    * completely rebuild resource utilization of
    * all queues and execution hosts
    * change versions of corresponding queues
    */
   for (const auto jep : master_job_list) {
      for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         bool master_task = true;
         const char *last_hostname = nullptr;
         const auto pe = ocs::tom::get<JAT_pe_object>(jatep);
         const auto granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(jatep);

         bool do_per_global_host_booking = true;
         const auto global_host = ocs::tom::find_first_rw<EH_name>(master_ehost_list, SGE_GLOBAL_NAME);
         for (const auto gdil : ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) {
            const auto qep = cqueue_list_locate_qinstance(master_cqueue_list, ocs::tom::get<JG_qname>(gdil));
            if (!qep) {
               /* should never happen */
               master_task = false;
               continue;
            }

            const int qslots = ocs::tom::get<JG_slots>(gdil);
            debit_host_consumable(jep, jatep, granted_resources_list, pe, global_host, master_centry_list, qslots,
                                  master_task, do_per_global_host_booking, nullptr);

            const char *gdil_host = ocs::tom::get<JG_qhostname>(gdil);
            const bool do_per_host_booking = host_do_per_host_booking(&last_hostname, gdil_host);
            const char *queue_host = ocs::tom::get<QU_qhostname>(qep);
            debit_host_consumable(jep, jatep, granted_resources_list, pe,
                                  ocs::tom::find_first_rw<EH_name>(master_ehost_list, queue_host),
                                  master_centry_list, qslots, master_task, do_per_host_booking, nullptr);
            qinstance_debit_consumable(qep, jep, pe, master_centry_list, qslots, master_task, do_per_host_booking,
                                       nullptr);
            master_task = false;
            do_per_global_host_booking = false;
         }
      }
   }

   sge_change_queue_version_centry(gdi_version);

   /* changing complex attributes can change consumables.
    * dump queue and host consumables to reporting file.
    */
   {
      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      const uint64_t now = sge_get_gmt64();

      /* dump all queue consumables */
      for (const auto cqueue : master_cqueue_list) {
         for (const auto qinstance : ocs::tom::get<CQ_qinstances>(cqueue)) {
            const char *hostname = ocs::tom::get<QU_qhostname>(qinstance);
            const auto host = ocs::tom::find_first<EH_name>(master_ehost_list, hostname);
            ocs::ReportingFileWriter::create_queue_consumable_records(&answer_list, host, qinstance,
                                                                      ocs::tom::ConstElement<ocs::tom::JB>(), now);
         }
      }
      answer_list_output(&answer_list);
      /* dump all host consumables */
      for (const auto hep : master_ehost_list) {
         ocs::ReportingFileWriter::create_host_consumable_records(&answer_list, hep,
                                                                  ocs::tom::ConstElement<ocs::tom::JB>(), now);
      }
      answer_list_output(&answer_list);
   }

   DRETURN_VOID;
}

