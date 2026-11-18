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
 * @brief Execution, submit and admin hosts
 */
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_AutoAddHosts.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Session.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_HS.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_LR.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "tom/ocs_List.h"

#include "comm/commlib.h"

#include "sched/sge_job_schedd.h"
#include "sched/sge_resource_utilization.h"
#include "sched/sge_serf.h"
#include "sched/debit.h"
#include "sched/valid_queue_user.h"

#include "spool/sge_spooling.h"

#include "ocs_ReportingFileWriter.h"
#include "symbols.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "sge_host_qmaster.h"
#include "sge_hgroup_qmaster.h"
#include "evm/sge_event_master.h"
#include "configuration_qmaster.h"
#include "sge_c_gdi.h"
#include "mail.h"
#include "ocs_CEntry.h"
#include "sge_cqueue_qmaster.h"
#include "sge_hgroup_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "reschedule.h"
#include "sge_qinstance_qmaster.h"
#include "sge_utility_qmaster.h"
#include "qmaster_to_execd.h"
#include "sge_persistence_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_job_enforce_limit.h"

static void
exec_host_change_queue_version(const char *exechost_name, uint64_t gdi_session);

static void
master_kill_execds(ocs::gdi::Packet *packet, ocs::gdi::Task *task);

static void
host_trash_nonstatic_load_values(ocs::tom::Element<ocs::tom::EH> host);

static void
notify(ocs::tom::Element<ocs::tom::EH> lel, ocs::gdi::Packet *packet, ocs::gdi::Task *task,
       int kill_jobs, int force);

static int
verify_scaling_list(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ConstElement<ocs::tom::EH> host);

static void
host_update_categories(ocs::tom::ConstElement<ocs::tom::EH> new_hep, ocs::tom::ConstElement<ocs::tom::EH> old_hep,
                       uint64_t gdi_session);

static int
attr_mod_threshold(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::EH> ep,
                   ocs::tom::Element<ocs::tom::EH> new_ep,
                   ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, const char *attr_name,
                   const char *object_name);

/** @brief Arm the timers this host needs: load cleanup and reschedule-unknown
 *
 * Note the misspelling in the name; it is part of the interface now.
 */
void
host_initalitze_timer() {
   DENTER(TOP_LAYER);
   const ocs::tom::List<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));

   /* initiate timer for all hosts because they start in 'unknown' state */
   if (master_ehost_list) {
      // host_list_locate() has a raw form only; the list holds execution hosts
      /* get "global" element pointer */
      const ocs::tom::ConstElement<ocs::tom::EH> global_host_elem(host_list_locate(master_ehost_list,
                                                                                    SGE_GLOBAL_NAME));

      /* get "template" element pointer */
      const ocs::tom::ConstElement<ocs::tom::EH> template_host_elem(host_list_locate(master_ehost_list,
                                                                                      SGE_TEMPLATE_NAME));

      for (const auto host : master_ehost_list) {
         if ((host != global_host_elem) && (host != template_host_elem)) {
            reschedule_add_additional_time(sge_gmt32_to_gmt64(load_report_interval(host)));
            reschedule_unknown_trigger(host);
            reschedule_add_additional_time(0);
         }
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Trash old load values
 *
 * Trash old load values in "host" element
 *
 * @param host EH_Type element
 */
static void
host_trash_nonstatic_load_values(const ocs::tom::Element<ocs::tom::EH> host) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::HL> load_attr;
   ocs::tom::List<ocs::tom::HL> load_attr_list = ocs::tom::get<EH_load_list>(host);
   ocs::tom::Element<ocs::tom::HL> next_load_attr = load_attr_list.front();
   while ((load_attr = next_load_attr)) {
      next_load_attr = load_attr.next();
      if (!ocs::tom::get<HL_is_static>(load_attr)) {
         load_attr_list.erase(load_attr);
      }
   }
   DRETURN_VOID;
}

/* ------------------------------------------ 

   adds the host to host_list with type 

   -1 error 
   0 ok
   1 host does exist

 */
/** @brief Create a host object of a given kind, if it does not exist yet
 *
 * An execution host reporting in for the first time, or a submit host named in
 * a request, is created rather than rejected.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param hostname the host
 * @param target which host list to add it to
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
sge_add_host_of_type(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *hostname, ocs::gdi::Target target, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int ret;
   int dataType;
   int pos;
   lListElem *ep;
   gdi_object_t *object;
   lList *ppList = nullptr;
   const char *username = component_get_username();
   const char *qualified_hostname = component_get_qualified_hostname();

   if (hostname == nullptr) {
      DRETURN(-1);
   }

   object = get_gdi_object(target);

   /* prepare template
    * the object type and its key field are known only at run time (gdi_object_t), so this stays generic */
   ep = lCreateElem(object->type);
   pos = lGetPosInDescr(object->type, object->key_nm);
   dataType = lGetPosType(object->type, pos);
   switch (dataType) {
      case lStringT:
         lSetString(ep, object->key_nm, hostname);
         break;
      case lHostT:
         lSetHost(ep, object->key_nm, hostname);
         break;
      default:
      DPRINTF("sge_add_host_of_type: unexpected datatype\n");
   }
   ret = sge_gdi_add_mod_generic(packet, task, nullptr, ep, 1, object, username,
                                 qualified_hostname, ocs::gdi::Command::NONE,
                                 ocs::gdi::SubCommand::NONE, &ppList, monitor);
   lFreeElem(&ep);
   lFreeList(&ppList);

   DRETURN((ret == STATUS_OK) ? 0 : -1);
}

/** @brief Create host objects for every host named in a host group but missing
 * @param packet the client request
 * @param task the GDI task being answered
 * @param this_list the host list to add to
 * @param answer_list receives messages for the caller
 * @param href_list the host references to satisfy
 * @param monitor for monitoring qmaster threads
 * @return true on success
 */
bool
host_list_add_missing_href(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                           const ocs::tom::ConstList<ocs::tom::EH> this_list,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const ocs::tom::ConstList<ocs::tom::HR> href_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   bool ret = true;
   for (const auto href : href_list) {
      const char *hostname = ocs::tom::get<HR_name>(href);

      if (host_list_locate(this_list, hostname) == nullptr) {
         ret &= (sge_add_host_of_type(packet, task, hostname, ocs::gdi::Target::EH_LIST, monitor) == 0);
      }
   }
   DRETURN(ret);
}

/* ------------------------------------------------------------

   sge_del_host - deletes a host from the host_list

   if the invoking process is the qmaster the host list is
   spooled to disk

*/
/** @brief Delete a host object
 * @param packet the client request
 * @param task the GDI task being answered
 * @param hep the host
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param target which host list it belongs to
 * @param master_hgroup_list the host groups, to check the host is not referenced
 * @param monitor for monitoring qmaster threads
 * @return STATUS_OK on success
 */
int sge_del_host(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::EH> hep,
                 ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost, const ocs::gdi::Target target,
                 const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::EH> ep;
   const char *host;
   char unique[CL_MAXHOSTNAMELEN];
   ocs::tom::List<ocs::tom::EH> host_list = nullptr;
   const char *name = nullptr;
   int ret;
   const ocs::tom::List<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));
   const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE));

   if (!hep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   switch (target) {
      case ocs::gdi::Target::EH_LIST:
         host_list = master_ehost_list;
         name = "execution host";
         break;
      default:
      DRETURN(STATUS_EUNKNOWN);
   }
   /* ep is no host element, if ep has no EH_name */
   if (!ocs::tom::has<EH_name>(hep)) {
      ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(EH_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   host = ocs::tom::get<EH_name>(hep);
   if (!host) {
      ERROR(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   ret = sge_resolve_hostname(host, unique, EH_name);
   if (ret != CL_RETVAL_OK) {
      /* 
       * Due to CR 6319231, IZ 1760 this is allowed 
       */
      ;
   }

   /* check if host is in host list (host_list_locate() has a raw form only, the list holds execution hosts) */
   if ((ep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(host_list, unique))) == nullptr) {
      /* may be host was not the unique hostname.
         Get the unique hostname and try to find it again. */
      if (getuniquehostname(host, unique, 0) != CL_RETVAL_OK) {
         ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, host);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      /* again check if host is in host list. This time use the unique
         hostname */
      if ((ep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(host_list, unique))) == nullptr) {
         ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, name, host);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   if (target == ocs::gdi::Target::EH_LIST &&
       host_is_referenced(hep, alpp, master_cqueue_list, master_hgroup_list)) {
      answer_list_log(alpp, false, true);
      DRETURN(STATUS_ESEMANTIC);
   }

   if (target == ocs::gdi::Target::EH_LIST && !strcasecmp(unique, "global")) {
      ERROR(SFNMAX, MSG_OBJ_DELGLOBALHOST);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_ESEMANTIC);
   }

   /* remove host file and send event */
   switch (target) {
      case ocs::gdi::Target::EH_LIST: {
         // a deleted host does not report, rescheduled jobs must not wait for their old runs
         reschedule_clear_deferred_starts_of_host(ep, packet->gdi_session);

         ocs::tom::ListPtr<ocs::tom::AN> answer_list;
         sge_event_spool(&answer_list, 0, sgeE_EXECHOST_DEL,
                         0, 0, ocs::tom::get<EH_name>(ep), nullptr, nullptr,
                         nullptr, nullptr, nullptr, true, true, packet->gdi_session);
         answer_list_output(&answer_list);
         host_update_categories(nullptr, ep, packet->gdi_session);

         break;
      }
      case ocs::gdi::Target::NO_TARGET:
         break;
      case ocs::gdi::Target::CQ_LIST:
         break;
      case ocs::gdi::Target::JB_LIST:
         break;
      case ocs::gdi::Target::EV_LIST:
         break;
      case ocs::gdi::Target::CE_LIST:
         break;
      case ocs::gdi::Target::ORDER_LIST:
         break;
      case ocs::gdi::Target::MASTER_EVENT:
         break;
      case ocs::gdi::Target::CONF_LIST:
         break;
      case ocs::gdi::Target::PE_LIST:
         break;
      case ocs::gdi::Target::SC_LIST:
         break;
      case ocs::gdi::Target::UU_LIST:
         break;
      case ocs::gdi::Target::US_LIST:
         break;
      case ocs::gdi::Target::PR_LIST:
         break;
      case ocs::gdi::Target::STN_LIST:
         break;
      case ocs::gdi::Target::CK_LIST:
         break;
      case ocs::gdi::Target::CAL_LIST:
         break;
      case ocs::gdi::Target::SME_LIST:
         break;
      case ocs::gdi::Target::HGRP_LIST:
         break;
      case ocs::gdi::Target::RQS_LIST:
         break;
      case ocs::gdi::Target::AR_LIST:
         break;
      case ocs::gdi::Target::DUMMY_LIST:
         break;
      case ocs::gdi::Target::CAT_LIST:
         break;
      case ocs::gdi::Target::RL_LIST:
         break;
      case ocs::gdi::Target::PROCEDURE:
         break;
   }

   /* delete found host element */
   host_list.erase(ep);

   /* CS-2438 chunk 7: @exec_hosts follows the exec host list.
    *
    * CS-2753: noted, not done here. The rebuild is O(n) in the execution host
    * list and it used to run after every single removal, which made "qconf -de"
    * with a thousand names quadratic. sge_c_gdi_process_in_worker() does it once
    * when the request is through, by which time the list has lost every host the
    * request deletes -- which is what the rebuild has to see. */
   if (target == ocs::gdi::Target::EH_LIST && task != nullptr) {
      task->exec_hostgroup_out_of_sync = true;
   }

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, unique, name);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}


/* ------------------------------------------------------------ */

/** @brief Apply one attribute change to a host
 *
 * The gdi_object_t::modifier for hosts; see sge_c_gdi.h for the sequence it
 * is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param new_host the host as it is to become
 * @param ep the reduced element the client sent
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param object the table entry for hosts
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
host_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *new_host,
         lListElem *ep, int add, const char *ruser, const char *rhost, gdi_object_t *object,
         ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *host;
   int nm;
   int pos;
   int dataType;
   bool changed = false;
   bool update_qversion = false;
   const ocs::tom::ConstList<ocs::tom::PR> master_project_list(*ocs::DataStore::get_master_list(SGE_TYPE_PROJECT));
   const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
   const ocs::tom::ConstList<ocs::tom::CE> master_centry_list(*ocs::DataStore::get_master_list(SGE_TYPE_CENTRY));
   const ocs::tom::ConstList<ocs::tom::AR> master_ar_list(*ocs::DataStore::get_master_list(SGE_TYPE_AR));

   // the key field comes from the gdi_object_t table at run time, the name handling stays generic
   nm = object->key_nm;

   /* ---- [EAS]H_name */
   if (add) {
      if (attr_mod_str(alpp, ep, new_host, nm, object->object_name)) {
         goto ERROR;
      }
   }
   pos = lGetPosViaElem(new_host, nm, SGE_NO_ABORT);
   dataType = lGetPosType(lGetElemDescr(new_host), pos);
   if (dataType == lHostT) {
      host = lGetHost(new_host, nm);
   } else {
      host = lGetString(new_host, nm);
   }

   /* On add, reject a host name that is unsafe as a spool path component (e.g.
    * "../../tmp/x"): host names skip verify_str_key() and are used verbatim as a
    * spool-file key, so validate at the input boundary too (CS-2364, CWE-22).
    * The flatfile spool layer has a matching defence-in-depth guard. */
   if (add && !verify_host_name(alpp, host)) {
      goto ERROR;
   }

   if (nm == EH_name) {
      bool acl_changed = false;
      // with EH_name as the key both elements are execution hosts (the request data is checked)
      const ocs::tom::Element<ocs::tom::EH> reduced(ep);
      const ocs::tom::Element<ocs::tom::EH> new_eh(new_host);

      /* ---- EH_scaling_list */
      if (ocs::tom::has<EH_scaling_list>(reduced)) {
         attr_mod_sub_list(alpp, new_eh, EH_scaling_list, HS_name, reduced, cmd,
                           sub_command, SGE_ATTR_LOAD_SCALING, SGE_OBJ_EXECHOST, 0, &changed);
         if (verify_scaling_list(alpp, new_eh) != STATUS_OK) {
            goto ERROR;
         }
         if (changed) {
            update_qversion = true;
         }
      }

      /* ---- EH_consumable_config_list */
      if (ocs::tom::has<EH_consumable_config_list>(reduced)) {
         if (attr_mod_threshold(alpp, reduced, new_eh, cmd, sub_command, SGE_ATTR_COMPLEX_VALUES, SGE_OBJ_EXECHOST)) {
            goto ERROR;
         }
         if (changed) {
            update_qversion = true;
         }
      }

      /* ---- EH_acl */
      if (ocs::tom::has<EH_acl>(reduced)) {
         DPRINTF("got new EH_acl\n");
         /* check acl list */
         if (userset_list_validate_acl_list(ocs::tom::get<EH_acl>(reduced), alpp, master_userset_list) != STATUS_OK) {
            goto ERROR;
         }
         attr_mod_sub_list(alpp, new_eh, EH_acl, US_name, reduced, cmd,
                           sub_command, SGE_ATTR_USER_LISTS, SGE_OBJ_EXECHOST, 0, &changed);
         if (changed) {
            acl_changed = true;
            update_qversion = true;
         }
      }

      /* ---- EH_xacl */
      if (ocs::tom::has<EH_xacl>(reduced)) {
         DPRINTF("got new EH_xacl\n");
         /* check xacl list */
         if (userset_list_validate_acl_list(ocs::tom::get<EH_xacl>(reduced), alpp, master_userset_list) != STATUS_OK) {
            goto ERROR;
         }
         attr_mod_sub_list(alpp, new_eh, EH_xacl, US_name, reduced, cmd,
                           sub_command, SGE_ATTR_XUSER_LISTS,
                           SGE_OBJ_EXECHOST, 0, &changed);
         if (changed) {
            acl_changed = true;
            update_qversion = true;
         }
      }


      /* ---- EH_prj */
      if (ocs::tom::has<EH_prj>(reduced)) {
         DPRINTF("got new EH_prj\n");
         /* check prj list */
         if (verify_project_list(alpp, ocs::tom::get<EH_prj>(reduced), master_project_list, "projects",
                                 object->object_name,
                                 host) != STATUS_OK) {
            goto ERROR;
         }
         attr_mod_sub_list(alpp, new_eh, EH_prj, PR_name, reduced, cmd,
                           sub_command, SGE_ATTR_PROJECTS,
                           SGE_OBJ_EXECHOST, 0, &changed);
         if (changed) {
            update_qversion = true;
         }
      }

      /* ---- EH_xprj */
      if (ocs::tom::has<EH_xprj>(reduced)) {
         DPRINTF("got new EH_xprj\n");
         /* check xprj list */
         if (verify_project_list(alpp, ocs::tom::get<EH_xprj>(reduced), master_project_list, "xprojects",
                                 object->object_name,
                                 host) != STATUS_OK) {
            goto ERROR;
         }
         attr_mod_sub_list(alpp, new_eh, EH_xprj, PR_name, reduced, cmd,
                           sub_command, SGE_ATTR_XPROJECTS,
                           SGE_OBJ_EXECHOST, 0, &changed);
         if (changed) {
            update_qversion = true;
         }
      }

      /* ---- EH_usage_scaling_list */
      if (ocs::tom::has<EH_usage_scaling_list>(reduced)) {
         attr_mod_sub_list(alpp, new_eh, EH_usage_scaling_list, HS_name, reduced, cmd,
                           sub_command, SGE_ATTR_USAGE_SCALING, SGE_OBJ_EXECHOST, 0, &changed);
         if (changed) {
            update_qversion = true;
         }
      }

      if (ocs::tom::has<EH_report_variables>(reduced)) {
         attr_mod_sub_list(alpp, new_eh, EH_report_variables, STU_name, reduced, cmd,
                           sub_command, "report_variables",
                           SGE_OBJ_EXECHOST, 0, nullptr);


         /* check if all report_variables are valid complex variables */
         for (ocs::tom::ConstElement<ocs::tom::STU> var : ocs::tom::get<EH_report_variables>(reduced)) {
            const char *name = ocs::tom::get<STU_name>(var);
            if (centry_list_locate(master_centry_list, name) == nullptr) {
               ERROR(MSG_SGETEXT_UNKNOWN_RESOURCE_S, name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               goto ERROR;
            }
         }
      }

      if (acl_changed) {
         for (const auto ar : master_ar_list) {
            if (ocs::tom::find_first<JG_qhostname>(ocs::tom::get<AR_granted_slots>(ar), host)) {
               if (!sge_ar_have_users_access(nullptr, ar, host, ocs::tom::get<EH_acl>(new_eh),
                                             ocs::tom::get<EH_xacl>(new_eh), master_userset_list)) {
                  ERROR(MSG_PARSE_MOD3_REJECTED_DUE_TO_AR_SU, SGE_ATTR_USER_LISTS, ocs::tom::get<AR_id>(ar));
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  goto ERROR;
               }
            }
         }
      }
   }

   if (update_qversion) {
      exec_host_change_queue_version(host, packet->gdi_session);
   }

   DRETURN(0);
   ERROR:
DRETURN(STATUS_EUNKNOWN);
}

/** @brief Write a host to the spool
 *
 * The gdi_object_t::writer for hosts.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param ep the host
 * @param object the table entry for hosts
 * @return 0 on success
 */
int
host_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *ep,
           gdi_object_t *object) {
   DENTER(TOP_LAYER);

   // the signature is fixed by gdi_object_t::writer, and the key field comes from the table at run time

   const char *key;
   int pos = lGetPosViaElem(ep, object->key_nm, SGE_NO_ABORT);
   if (lGetPosType(lGetElemDescr(ep), pos) == lHostT) {
      key = lGetHost(ep, object->key_nm);
   } else {
      key = lGetString(ep, object->key_nm);
   }

   sge_object_type host_type = SGE_TYPE_EXECHOST;
   switch (object->key_nm) {
      case EH_name:
         host_type = SGE_TYPE_EXECHOST;
         break;
   }

   int ret = 0;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ep, key, host_type);
   answer_list_output(&answer_list);
   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S,
                              key);
      ret = 1;
   }

   DRETURN(ret);
}

/**
 * @brief Rebuild `@exec_hosts` from the exec host list
 *
 * CS-2438 chunk 7. `@exec_hosts` is derived, not administered: it mirrors
 * the execution host list and is read-only for every role including manager.
 * This is the one place that fills it.
 * A full rebuild, not an incremental add/remove of the one host that
 * changed. Exec hosts are added and deleted rarely -- this is nowhere near a
 * hot path -- and a rebuild converges even if some future code path forgets
 * to call it, whereas a missed incremental update stays wrong until the next
 * qmaster restart. For a derived list, self-correcting beats cheap.
 * "global" and "template" are excluded: they are pseudo-hosts carrying
 * default values, not machines, and a queue instance on them makes no sense.
 * Not spooled, deliberately. The membership is derived from EH_LIST, which
 * IS spooled, and it is rebuilt at every qmaster startup -- writing it to
 * disk on every exec host add would be I/O for state that is reconstructed
 * anyway. The group object itself was spooled once when chunk 1 seeded it.
 *
 * @param monitor likewise
 * @param send_events false during qmaster startup, where the event system is not serving yet and setup_qmaster.cc rebuilds every cache once afterwards anyway
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return true if the membership changed
 *
 * @note MT-NOTE: call under the write lock -- it modifies the HGRP master list
 */
bool
host_sync_exec_hostgroup(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                         bool send_events) {
   DENTER(TOP_LAYER);

   const uint64_t gdi_session = (packet != nullptr) ? packet->gdi_session
                                                    : ocs::SessionManager::GDI_SESSION_NONE;

   const ocs::tom::List<ocs::tom::HGRP> master_hgroup_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_HGROUP));
   const ocs::tom::Element<ocs::tom::HGRP> hgrp = hgroup_list_locate(master_hgroup_list, EXEC_HOSTGROUP);

   if (hgrp == nullptr) {
      /* not seeded yet -- setup_qmaster.cc creates it and calls us afterwards */
      DRETURN(false);
   }

   const ocs::tom::ConstList<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST));
   ocs::tom::ListPtr<ocs::tom::HR> wanted;

   for (const auto ehost : master_ehost_list) {
      const char *name = ocs::tom::get<EH_name>(ehost);

      if (name == nullptr || strcmp(name, SGE_GLOBAL_NAME) == 0 || strcmp(name, SGE_TEMPLATE_NAME) == 0) {
         continue;
      }
      ocs_href_list_emplace(wanted, name);
   }

   /*
    * Leave the object alone when nothing changed. Every exec host modify comes
    * through host_success() too, and an unconditional lSetList() would emit an
    * HGROUP_MOD event -- and a cache recomputation for every referencing group
    * -- on each "qconf -Me", for a membership that did not move.
    */
   ocs::tom::ConstList<ocs::tom::HR> current = ocs::tom::get<HGRP_host_list>(hgrp);
   bool changed = (current.size() != wanted.get().size());

   if (!changed) {
      for (const auto href : wanted.get()) {
         if (href_list_locate(current, ocs::tom::get<HR_name>(href)) == nullptr) {
            changed = true;
            break;
         }
      }
   }

   if (!changed) {
      DRETURN(false);
   }

   /*
    * Queue instances have to follow the membership, exactly as they do when an
    * administrator edits an ordinary host group. hgroup_mod() gets that from the
    * cqueue machinery below; a plain lSetList() here would not, and a cluster
    * queue whose hostlist names @exec_hosts would silently stop gaining queue
    * instances for newly added exec hosts -- a desync that does NOT heal, since
    * startup reads the queues and their instances back from the spool.
    *
    * The structure mirrors hgroup_mod(): per referencing queue, resolve its
    * hostlist before and after the change, reduce the raw add/remove sets to the
    * hosts that queue actually gains or loses, and hand those to
    * cqueue_handle_qinstances() on a copy that hgroup_commit() swaps in at the
    * end. Where hgroup_mod() dechains its not-yet-installed copy in and out of
    * the master list to get the two views, this group is modified in place, so
    * the same effect comes from swapping the new member list in for the length
    * of one resolution.
    */
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   ocs::tom::ListPtr<ocs::tom::HR> add_hosts;
   ocs::tom::ListPtr<ocs::tom::HR> rem_hosts;
   bool ret;
   {
      ocs::tom::ListPtr<ocs::tom::HR> add_groups;
      ocs::tom::ListPtr<ocs::tom::HR> rem_groups;
      ret = href_list_find_diff(wanted.get(), &answer_list, current, &add_hosts, &rem_hosts,
                                &add_groups, &rem_groups);

      /* @exec_hosts holds plain hosts only -- it never nests */
   }

   /* the groups whose resolution changes: this one, plus everything above it */
   ocs::tom::ListPtr<ocs::tom::HR> occupant_groups;
   if (ret) {
      ret &= href_list_add(occupant_groups, &answer_list, EXEC_HOSTGROUP);
      ret &= hgroup_find_all_referencees(hgrp, &answer_list, master_hgroup_list, &occupant_groups);
   }

   const ocs::tom::List<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE));

   for (const auto cqueue : master_cqueue_list) {
      if (!ret) {
         break;
      }
      if (!cqueue_is_a_href_referenced(cqueue, occupant_groups.get(), true)) {
         continue;
      }

      const ocs::tom::ConstList<ocs::tom::HR> cq_hostlist = cqueue_get_hostlist(cqueue, master_hgroup_list);
      ocs::tom::ListPtr<ocs::tom::HR> before_mod_list;
      ocs::tom::ListPtr<ocs::tom::HR> after_mod_list;
      ocs::tom::ListPtr<ocs::tom::HR> real_add_hosts;
      ocs::tom::ListPtr<ocs::tom::HR> real_rem_hosts;

      ret &= href_list_find_all_references(cq_hostlist, &answer_list, master_hgroup_list,
                                           &before_mod_list, nullptr);

      /* swap the new membership in just long enough to resolve the "after" view */
      wanted = hgrp.exchange<HGRP_host_list>(std::move(wanted));
      ret &= href_list_find_all_references(cq_hostlist, &answer_list, master_hgroup_list,
                                           &after_mod_list, nullptr);
      wanted = hgrp.exchange<HGRP_host_list>(std::move(wanted));

      if (ret) {
         /* a host this queue keeps reaching by another route is not really lost,
          * and one it already reached is not really gained */
         ret &= href_list_compare(rem_hosts.get(), &answer_list, after_mod_list.get(), &real_rem_hosts,
                                  nullptr, nullptr, nullptr);
         ret &= href_list_compare(add_hosts.get(), &answer_list, before_mod_list.get(), &real_add_hosts,
                                  nullptr, nullptr, nullptr);
      }

      if (ret) {
         const ocs::tom::List<ocs::tom::CQ> pending = ocs::tom::get_or_create<HGRP_cqueue_list>(hgrp, "");
         const ocs::tom::Element<ocs::tom::CQ> new_cqueue = pending.push_back(cqueue.clone());

         {

            /*
             * hgrp as the "reduced element": cqueue_handle_qinstances() only
             * probes it for cluster queue attributes the client changed, and an
             * HGRP element yields none -- which is the truth here. No queue
             * attribute moved, only the set of hosts. refresh_all_values is true
             * for the same reason hgroup_mod() sets it: the hostlist changed, so
             * a previously unambiguous attribute value may now be ambiguous.
             */
            ret &= cqueue_handle_qinstances(packet, task, new_cqueue, &answer_list, hgrp,
                                            real_add_hosts.get(), real_rem_hosts.get(), true, monitor,
                                            master_hgroup_list, master_cqueue_list);
         }
      }
   }

   occupant_groups.reset();
   add_hosts.reset();
   rem_hosts.reset();

   if (!ret) {
      /* leave the group untouched -- a partial update is worse than none */
      hgroup_rollback_cqueues(hgrp);
      answer_list_output(&answer_list);
      ERROR(MSG_HGRP_RESERVED_NOSYNC_S, EXEC_HOSTGROUP);
      DRETURN(false);
   }

   ocs::tom::set<HGRP_host_list>(hgrp, std::move(wanted));   /* takes ownership */

   ocs::tom::ListPtr<ocs::tom::HR> referencees;
   hgroup_refresh_caches(hgrp, master_hgroup_list, referencees);

   /*
    * CS-2680, N-C-1. The refresh above follows references, and that is not
    * enough any more: a group carrying a matcher depends on the execution host
    * list because that list is the set its patterns are resolved against, even
    * when it references nothing that mirrors it. This is the step that lets a
    * node which has just registered appear in its groups without anyone editing
    * one.
    */
   hgroup_refresh_matcher_caches(master_hgroup_list, referencees);

   if (send_events) {
      /* the pending queue copies are not part of the group's own event */
      auto pending = hgrp.exchange<HGRP_cqueue_list>(ocs::tom::ListPtr<ocs::tom::CQ>());
      sge_add_event(0, sgeE_HGROUP_MOD, 0, 0, EXEC_HOSTGROUP, nullptr, nullptr, hgrp, gdi_session);
      static_cast<void>(hgrp.exchange<HGRP_cqueue_list>(std::move(pending)));

      hgroup_send_referencee_events(referencees.get(), master_hgroup_list, gdi_session);
   }
   referencees.reset();

   /*
    * Spool the queue instances the change created or altered, before committing
    * them. The GDI route gets this from hgroup_spool(), which walks the same
    * HGRP_cqueue_list; this path does not go through the GDI object table, so it
    * has to do it itself. Without it a queue instance born from an exec host add
    * lives only in memory and is gone after the next qmaster start -- and the
    * matching delete then tries to unlink a spool file that was never written.
    *
    * The group itself is deliberately NOT spooled here: its membership is
    * derived from the execution host list and rebuilt at every startup.
    */
   {
      ocs::tom::ConstList<ocs::tom::CQ> pending_queues = ocs::tom::get<HGRP_cqueue_list>(hgrp);
      dstring key_dstring = DSTRING_INIT;
      ocs::tom::ListPtr<ocs::tom::AN> spool_answer_list;
      bool spool_ok = spool_transaction(&spool_answer_list, spool_get_default_context(), STC_begin);

      answer_list_output(&spool_answer_list);

      for (const auto cq : pending_queues) {
         const char *cqname = ocs::tom::get<CQ_name>(cq);

         if (!spool_ok) {
            break;
         }
         for (const auto qinstance : ocs::tom::get<CQ_qinstances>(cq)) {
            const uint32_t tag = ocs::tom::get<QU_tag>(qinstance);

            if (tag == SGE_QI_TAG_ADD || tag == SGE_QI_TAG_MOD) {
               const char *key = sge_dstring_sprintf(&key_dstring, "%s/%s", cqname,
                                                     static_cast<const char *>(ocs::tom::get<QU_qhostname>(qinstance)));

               if (!spool_write_object(&spool_answer_list, spool_get_default_context(),
                                       qinstance, key, SGE_TYPE_QINSTANCE)) {
                  answer_list_output(&spool_answer_list);
                  spool_ok = false;
                  break;
               }
            }
         }
      }

      spool_transaction(&spool_answer_list, spool_get_default_context(),
                        spool_ok ? STC_commit : STC_rollback);
      answer_list_output(&spool_answer_list);
      sge_dstring_free(&key_dstring);

      if (!spool_ok) {
         WARNING(MSG_HGRP_RESERVED_NOSYNC_S, EXEC_HOSTGROUP);
      }
   }

   /* installs the modified queues and sends their QINSTANCE add/mod/del events */
   hgroup_commit(hgrp, gdi_session);

   answer_list_output(&answer_list);

   DRETURN(true);
}

/** @brief Announce a host change once it is safely spooled
 *
 * The gdi_object_t::on_success for hosts.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the host as it now is
 * @param old_ep the host as it was, or nullptr on add
 * @param object the table entry for hosts
 * @param ppList receives information for post processing
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
host_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *raw_ep, lListElem *raw_old_ep,
             gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   const ocs::tom::List<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));

   // the signature is fixed by gdi_object_t::on_success, the key field comes from the table at run time
   switch (object->key_nm) {
      case EH_name: {
         // with EH_name as the key both elements are execution hosts
         const ocs::tom::Element<ocs::tom::EH> ep(raw_ep);
         const ocs::tom::ConstElement<ocs::tom::EH> old_ep(raw_old_ep);
         const char *host = ocs::tom::get<EH_name>(ep);
         int global_host = !strcmp(SGE_GLOBAL_NAME, host);

         if (global_host) {
            host_list_merge(master_ehost_list);
         } else {
            const ocs::tom::ConstElement<ocs::tom::EH> global_ep =
                    ocs::tom::find_first<EH_name>(master_ehost_list, SGE_GLOBAL_NAME);
            host_merge(ep, global_ep);
         }

         host_update_categories(ep, old_ep, packet->gdi_session);
         sge_add_event(0, old_ep ? sgeE_EXECHOST_MOD : sgeE_EXECHOST_ADD, 0, 0, host, nullptr, nullptr, ep,
                       packet->gdi_session);

         /* CS-2438 chunk 7: @exec_hosts follows the exec host list. Marked for
          * MOD too, not just ADD -- the sync is a no-op when the membership did
          * not move, and that is cheaper than reasoning about which modify paths
          * can rename a host.
          *
          * CS-2753: marked rather than done, see sge_del_host(). The add side had
          * the same problem: one rebuild per element, even though "qconf -Ae" hands
          * the qmaster every host in a single request. */
         if (task != nullptr) {
            task->exec_hostgroup_out_of_sync = true;
         }
      }
         break;

   }

   DRETURN(0);
}

/* ------------------------------------------------------------ */

/** @brief Mark a host as unreachable and everything on it as unknown
 *
 * Called when the execution daemon stops answering. The jobs are not deleted -
 * the host may come back with them still running - but the queue instances go
 * into the unknown state so the scheduler stops sending work there.
 *
 * @param hep the execution host
 * @param gdi_session the session the change belongs to
 */
void
sge_mark_unheard(const ocs::tom::Element<ocs::tom::EH> hep, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   const char *host;
   const ocs::tom::List<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE));

   host = ocs::tom::get<EH_name>(hep);

   if (cl_com_remove_known_endpoint_from_name(host, to_cstr(EXECD), 1) == CL_RETVAL_OK) {
      DEBUG("set %s/%s/%d to unheard\n", host, to_cstr(EXECD), 1);
   }

   if (ocs::tom::get<EH_lt_heard_from>(hep) != 0) {
      host_trash_nonstatic_load_values(hep);
      cqueue_list_set_unknown_state(master_cqueue_list, host, true, true, gdi_session);

      ocs::tom::set<EH_lt_heard_from>(hep, 0);

      // the host does not report any more, rescheduled jobs must not wait for their old runs
      reschedule_clear_deferred_starts_of_host(hep, gdi_session);

      /* add a trigger to enforce limits when they are exceeded */
      sge_host_add_enforce_limit_trigger(host);

      /* hedeby depends on this event */
      sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0, host, nullptr, nullptr, hep, gdi_session);
   }

   DRETURN_VOID;
}

/* ----------------------------------------

   updates global and host specific load values
   using the load report list lp
*/
/** @brief Store the load values an execution host just reported
 * @param rhost the host as it identified itself
 * @param real_host its resolved name
 * @param lp the reported load values
 * @param gdi_session the session the change belongs to
 */
void
sge_update_load_values(const char *rhost, const char* real_host, const ocs::tom::List<ocs::tom::LR> lp,
                       const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::EH> *hepp = nullptr;
   ocs::tom::Element<ocs::tom::EH> global_ep = nullptr;
   ocs::tom::Element<ocs::tom::EH> host_ep = nullptr;
   ocs::tom::Element<ocs::tom::EH> real_ep = nullptr;
   bool statics_changed = false;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const ocs::tom::List<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE));
   const ocs::tom::ConstList<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST));

   /* JG: TODO: this time should better come with the report.
    *           it is the time when the reported values were valid.
    */
   uint64_t now = sge_get_gmt64();

   ocs::tom::Element<ocs::tom::EH> host_ep_eh = ocs::tom::find_first_rw<EH_name>(master_ehost_list, rhost);
   if (host_ep_eh == nullptr) {
      /* report from unknown host arrived, ignore it */
      DRETURN_VOID;
   }

   // get real host element if we handle a simulated host
   if (real_host != nullptr) {
      real_ep = ocs::tom::find_first_rw<EH_name>(master_ehost_list, real_host);
   }

   /* 
    * if rhost is unknown set him to known
    */
   if (ocs::tom::get<EH_lt_heard_from>(host_ep_eh) == 0) {
      cqueue_list_set_unknown_state(master_cqueue_list, rhost, true, false, gdi_session);

      /* remove a trigger to enforce limits when they are exceeded */
      sge_host_remove_enforce_limit_trigger(rhost);

      ocs::tom::set<EH_lt_heard_from>(host_ep_eh, sge_get_gmt64());
   }

   host_ep = nullptr;
   /* loop over all received load values */
   for (const auto ep : lp) {
      /* get name, value and other info */
      const char *name = ocs::tom::get<LR_name>(ep);
      const char *value = ocs::tom::get<LR_value>(ep);
      const char *host = ocs::tom::get<LR_host>(ep);
      uint32_t global = ocs::tom::get<LR_global>(ep);
      uint32_t is_static = ocs::tom::get<LR_is_static>(ep);

      /* erroneous load report */
      if (!name || !value || !host) {
         continue;
      }

      /* handle global or exec host? */
      if (global) {
         hepp = &global_ep;
      } else {
         hepp = &host_ep;
      }

      // The topology string that we receive from execd contains more information than we need
      // Filter the information and only keep the topology string as load value
      // Store the full string in the host element
      if (strcmp(LOAD_ATTR_TOPOLOGY, name) == 0) {
         // If we have a real host (simulated host case), get the topology from the internally stored full string
         if (real_ep != nullptr) {
            value = ocs::tom::get<EH_internal_topology>(real_ep);
         }

         // Keep the original value in the host element and a product-specific version as load value
         ocs::TopologyString topology(value);
         ocs::tom::set<EH_internal_topology>(*hepp, topology.to_string(true, true, true, false, false, false).c_str());
         ocs::tom::set<LR_value>(ep, topology.to_product_topology_string().c_str());
         value = ocs::tom::get<LR_value>(ep);
      }

      /* update load value list of reported host */
      if (*hepp == nullptr || sge_hostcmp(host, ocs::tom::get<EH_name>(*hepp)) != 0) {
         if (*hepp != nullptr) {
            /* we have a host change, send events for the previous one */
            sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD,
                            0, 0, ocs::tom::get<EH_name>(*hepp), nullptr, nullptr,
                            host_ep, nullptr, nullptr, true, statics_changed, gdi_session);
            ocs::ReportingFileWriter::create_host_records(&answer_list, *hepp, now);
            statics_changed = false;
         }

         /* get the new host */
         *hepp = ocs::tom::find_first_rw<EH_name>(master_ehost_list, host);
         if (*hepp == nullptr) {
            INFO(MSG_CANT_ASSOCIATE_LOAD_SS, rhost, host);
            continue;
         }
      }

      if (is_static == 2) {
         /* remove old load value */
         const ocs::tom::Element<ocs::tom::HL> lep = ocs::tom::find_first_in_rw<EH_load_list, HL_name>(*hepp, name);

         if (lep) {
            ocs::tom::get<EH_load_list>(*hepp).erase(lep);
         }
      } else {
         /* add a new load value */
         ocs::tom::Element<ocs::tom::HL> lep = ocs::tom::find_first_in_rw<EH_load_list, HL_name>(*hepp, name);
         if (lep == nullptr) {
            lep = ocs::tom::emplace_back_in<EH_load_list, HL_name>(*hepp, name);
            DPRINTF("%s: adding load value: " SFQ " = " SFQ "\n", host, name, value);
            if (is_static == 1) {
               statics_changed = true;
            }
         } else {
            /* replace an existing load value */
            if (is_static == 1) {
               if (sge_strnullcmp(value, ocs::tom::get<HL_value>(lep)) != 0) {
                  statics_changed = true;
               }
            }
         }

         /* copy value */
         ocs::tom::set<HL_value>(lep, value);
         ocs::tom::set<HL_last_update>(lep, now);
         ocs::tom::set<HL_is_static>(lep, is_static);
      }
   }

   /*
   ** if static load values (eg arch) have changed
   ** then spool
   */
   if (hepp != nullptr && *hepp != nullptr) {
      sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD,
                      0, 0, ocs::tom::get<EH_name>(*hepp), nullptr, nullptr,
                      *hepp, nullptr, nullptr, true, statics_changed, gdi_session);

      ocs::ReportingFileWriter::create_host_records(&answer_list, *hepp, now);
   }

   if (global_ep) {
      sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD,
                      0, 0, SGE_GLOBAL_NAME, nullptr, nullptr,
                      global_ep, nullptr, nullptr, true, false, gdi_session);
      ocs::ReportingFileWriter::create_host_records(&answer_list, global_ep, now);
   }
   answer_list_output(&answer_list);

   DRETURN_VOID;
}


/* ----------------------------------------

   trash old load values 
   
*/
/** @brief Drop load values that have outlived their configured lifetime
 *
 * A value nobody refreshes must not go on influencing scheduling for ever;
 * registered as a #TYPE_LOAD_VALUE_CLEANUP_EVENT.
 *
 * @param anEvent the timed event that fired
 * @param monitor for monitoring qmaster threads
 */
void
sge_load_value_cleanup_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *host;
   uint64_t timeout;
   uint64_t now = sge_get_gmt64();
   bool simulate_execds = mconf_get_simulate_execds();

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   const ocs::tom::List<ocs::tom::EH> master_exechost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));
   const ocs::tom::List<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE));

   // host_list_locate() has a raw form only; the list holds execution hosts
   /* get "global" element pointer */
   const ocs::tom::ConstElement<ocs::tom::EH> global_host_elem(host_list_locate(master_exechost_list,
                                                                                 SGE_GLOBAL_NAME));
   /* get "template" element pointer */
   const ocs::tom::ConstElement<ocs::tom::EH> template_host_elem(host_list_locate(master_exechost_list,
                                                                                   SGE_TEMPLATE_NAME));
   /* take each host including the "global" host */
   for (const auto hep : master_exechost_list) {
      unsigned long last_heard;
      if (hep == template_host_elem || hep == global_host_elem) {
         continue;
      }

      host = ocs::tom::get<EH_name>(hep);

      /* do not trash load values of simulated hosts */
      if (simulate_execds) {
         ocs::tom::ConstElement<ocs::tom::CE> load_report_host = ocs::tom::find_first_in<EH_consumable_config_list, CE_name>(hep, "load_report_host");
         if (load_report_host != nullptr) {
            const char *real_host = ocs::tom::get<CE_stringval>(load_report_host);
            if (real_host != nullptr && sge_hostcmp(real_host, host) != 0) {
               DPRINTF("skip trashing load values for host %s simulated by %s\n", host, real_host);
               continue;
            }
         }
      }

      if (ocs::tom::get<EH_lt_heard_from>(hep) == 0) {
         /* host is already unknown, nothing to trash */
         continue;
      }

      cl_commlib_get_last_message_time((cl_com_get_handle(to_cstr(QMASTER), 0)),
                                       host, to_cstr(EXECD), 1, &last_heard);


      timeout = host_unheard_timeout(hep);
      if (now <= sge_gmt32_to_gmt64((uint32_t)last_heard) + timeout) {
         continue;
         /* host is known, nothing to trash */
      }

      ocs::tom::set<EH_lt_heard_from>(hep, 0);

      // the host does not report any more, rescheduled jobs must not wait for their old runs
      reschedule_clear_deferred_starts_of_host(hep, ocs::SessionManager::GDI_SESSION_NONE);

      /* take each load value */
      host_trash_nonstatic_load_values(hep);

      /* set all queues residing at this host in unknown state */
      cqueue_list_set_unknown_state(master_cqueue_list, host, true, true, ocs::SessionManager::GDI_SESSION_NONE);

      /* add a trigger to enforce limits when they are exceeded */
      sge_host_add_enforce_limit_trigger(host);

      /* hedeby depends on this event */
      sge_add_event(0, sgeE_EXECHOST_MOD, 0, 0, host, nullptr, nullptr, hep, ocs::SessionManager::GDI_SESSION_NONE);

      /* initiate timer for this host because they turn into 'unknown' state */
      reschedule_unknown_trigger(hep);
   }

   mconf_set_new_config(false);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

/** @brief How often this host is expected to report its load
 * @param hep the execution host
 * @return the interval in seconds
 */
uint32_t
load_report_interval(const ocs::tom::Element<ocs::tom::EH> hep) {
   DENTER(TOP_LAYER);

   uint32_t timeout;
   const char *host;

   host = ocs::tom::get<EH_name>(hep);

   /* cache load report interval in exec host to prevent host name resolving each epoch */
   timeout = ocs::tom::get<EH_load_report_interval>(hep);
   if (mconf_is_new_config() || timeout == 0) {
      const auto conf_entry = ocs_get_configuration_entry_by_name(host, "load_report_time");

      if (conf_entry != nullptr) {
         if (parse_ulong_val(nullptr, &timeout, ocs::CEntry::Type::TIME, ocs::tom::get<CF_value>(conf_entry), nullptr, 0) == 0) {
            ERROR(MSG_OBJ_LOADREPORTIVAL_SS, host, ocs::tom::get<CF_value>(conf_entry));
            timeout = 120;
         }
      }

      DPRINTF("%s: load value timeout for host %s is " sge_u32 "\n", __func__, host, timeout);

      ocs::tom::set<EH_load_report_interval>(hep, timeout);
   }

   DRETURN(timeout);
}

/**
 * @brief How long may sge_qmaster not hear from the execd of a host?
 *
 * An idle sge_execd contacts sge_qmaster only once per load_report_time. A host is taken as
 * unheard when sge_qmaster has not heard from it for 3 load report intervals, but not before
 * max_unheard has passed.
 *
 * @param hep the execution host (EH_Type), its load report interval is cached in it
 * @return the timeout in microseconds
 */
uint64_t
host_unheard_timeout(const ocs::tom::Element<ocs::tom::EH> hep) {
   DENTER(TOP_LAYER);

   uint64_t timeout = sge_gmt32_to_gmt64(std::max(load_report_interval(hep) * 3,
                                                  mconf_get_max_unheard()));

   DRETURN(timeout);
}

/** @brief Increase the version of the queue instances on a host whose configuration changed
 *
 * @param exechost_name the host, or "global" for every queue instance
 * @param gdi_session the session the change belongs to
 */
static void
exec_host_change_queue_version(const char *exechost_name, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool change_all = (strcasecmp(exechost_name, SGE_GLOBAL_NAME) == 0) ? true : false;
   const ocs::tom::List<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE));

   for (const auto cqueue : master_cqueue_list) {
      ocs::tom::List<ocs::tom::QU> qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
      ocs::tom::Element<ocs::tom::QU> qinstance = nullptr;
      ocs::tom::Element<ocs::tom::QU> next_qinstance = nullptr;
      const void *iterator = nullptr;

      if (change_all) {
         next_qinstance = qinstance_list.front();
      } else {
         next_qinstance = ocs::tom::find_first_rw<QU_qhostname>(qinstance_list, exechost_name, &iterator);
      }
      while ((qinstance = next_qinstance)) {
         const char *name;
         ocs::tom::ListPtr<ocs::tom::AN> answer_list;

         if (change_all) {
            next_qinstance = qinstance.next();
            name = SGE_GLOBAL_NAME;
         } else {
            next_qinstance = ocs::tom::find_next_rw<QU_qhostname>(qinstance_list, exechost_name, &iterator);
            name = exechost_name;
         }
         DPRINTF(SFQ " has changed. Increasing qversion of" SFQ "\n", name, ocs::tom::get<QU_full_name>(qinstance));
         qinstance_increase_qversion(qinstance);
         sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD,
                         0, 0, ocs::tom::get<QU_qname>(qinstance),
                         ocs::tom::get<QU_qhostname>(qinstance), nullptr,
                         qinstance, nullptr, nullptr, true, false, gdi_session);
         answer_list_output(&answer_list);
      }
   }

   DRETURN_VOID;
}

/****
 **** ocs::gdi::Client::sge_gdi_kill_exechost
 ****
 **** prepares the killing of an exechost (or all).
 **** Actutally only the permission is checked here
 **** and master_kill_execds is called to do the work.
 ****/
/** @brief Handle a request to shut execution daemons down
 * @param packet the client request
 * @param task the GDI task being answered
 */
void
sge_gdi_kill_exechost(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
   DENTER(GDI_LAYER);

   if (!manop_is_manager(packet)) {
      ERROR(SFNMAX, MSG_OBJ_SHUTDOWNPERMS);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
      DRETURN_VOID;
   }

   master_kill_execds(packet, task);
   DRETURN_VOID;
}

/** @brief Tell execution daemons to terminate
 *
 * The request data is an ID_Type list. If the first ID_str is nullptr, every
 * execd is killed; otherwise every ID_str names an execd to kill. If ID_force
 * is 1, the jobs are killed too, if it is 0 they are not.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 */
static void
master_kill_execds(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
   DENTER(TOP_LAYER);

   int kill_jobs;
   char host[CL_MAXHOSTNAMELEN];
   const char *hostname;
   const ocs::tom::List<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));
   // the request data is structure checked in the listener (CS-3125)
   const ocs::tom::ConstList<ocs::tom::ID> data_list(task->data_list);

   if (ocs::tom::get<ID_str>(data_list.front()) == nullptr) {
      /* this means, we have to kill every execd. */

      kill_jobs = ocs::tom::get<ID_force>(data_list.front()) ? 1 : 0;
      /* walk over exechost list and send every exechosts execd a
         notification */
      for (const auto lel : master_ehost_list) {
         hostname = ocs::tom::get<EH_name>(lel);
         if (strcmp(hostname, SGE_TEMPLATE_NAME) && strcmp(hostname, SGE_GLOBAL_NAME)) {
            notify(lel, packet, task, kill_jobs, 0);

            /* RU: */
            /* initiate timer for this host which turns into unknown state */
            reschedule_unknown_trigger(lel);
         }
      }
      if (task->answer_list->size() == 0) {
         /* no exechosts have been killed */
         DPRINTF(MSG_SGETEXT_NOEXECHOSTS);
         INFO(SFNMAX, MSG_SGETEXT_NOEXECHOSTS);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
   } else {
      /* only specified exechosts should be killed. */

      /* walk over list with execd's to kill */
      for (const auto rep : data_list) {
         if ((getuniquehostname(ocs::tom::get<ID_str>(rep), host, 0)) != CL_RETVAL_OK) {
            WARNING(MSG_SGETEXT_CANTRESOLVEHOST_S, ocs::tom::get<ID_str>(rep));
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            // host_list_locate() has a raw form only; the list holds execution hosts
            const ocs::tom::Element<ocs::tom::EH> lel(host_list_locate(master_ehost_list, host));
            if (lel) {
               kill_jobs = ocs::tom::get<ID_force>(rep) ? 1 : 0;
               /*
               ** if a host name is given, then a kill is forced
               ** this means that even if the host is unheard we try
               ** to kill it
               */
               notify(lel, packet, task, kill_jobs, 1);
               /* RU: */
               /* initiate timer for this host which turns into unknown state */
               reschedule_unknown_trigger(lel);
            } else {
               WARNING(MSG_SGETEXT_ISNOEXECHOST_S, host);
               answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
            }
         }
      }
   }
   DRETURN_VOID;
}


/** @brief Notify sge_execd on a host to shut down
 *
 * @param lel the execution host
 * @param packet the client request
 * @param task the GDI task being answered
 * @param kill_jobs whether the jobs on the host are killed too
 * @param force whether to try even when the execd is not known to be alive
 */
static void
notify(const ocs::tom::Element<ocs::tom::EH> lel, ocs::gdi::Packet *packet, ocs::gdi::Task *task,
       const int kill_jobs, const int force) {
   DENTER(TOP_LAYER);

   const char *action_str = kill_jobs ? MSG_NOTIFY_SHUTDOWNANDKILL : MSG_NOTIFY_SHUTDOWN;
   const char *hostname = ocs::tom::get<EH_name>(lel);

   unsigned long last_heard_from;
   cl_commlib_get_last_message_time((cl_com_get_handle(to_cstr(QMASTER), 0)),
                                    hostname, to_cstr(EXECD), 1, &last_heard_from);
   bool execd_alive = last_heard_from > 0;

   if (!force && !execd_alive) {
      WARNING(MSG_OBJ_NOEXECDONHOST_S, hostname);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
   } else {
      int result = host_notify_about_kill(lel, kill_jobs);
      if (result != 0) {
         INFO(MSG_COM_NONOTIFICATION_SSS, action_str, (execd_alive ? "" : MSG_OBJ_UNKNOWN), hostname);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EDENIED2HOST, ANSWER_QUALITY_ERROR);
      } else {
         INFO(MSG_COM_NOTIFICATION_SSS, action_str, (execd_alive ? "" : MSG_OBJ_UNKNOWN), hostname);
         answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
      DPRINTF(SGE_EVENT);
   }

   if (kill_jobs) {
      char sge_mail_subj[1024];
      char sge_mail_body[1024];

      /* mark killed jobs as deleted */
      const ocs::tom::List<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB));
      for (const auto jep : master_job_list) {
         ocs::tom::ConstList<ocs::tom::MR> mail_users = ocs::tom::get<JB_mail_list>(jep);
         uint32_t mail_options = ocs::tom::get<JB_mail_options>(jep);

         for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
            ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep);
            if (gdil != nullptr) {
               if (sge_hostcmp(ocs::tom::get<JG_qhostname>(gdil.front()), hostname) == 0) {
                  /*   send mail to users if requested                  */
                  if (VALID(MAIL_AT_ABORT, mail_options) && !(ocs::tom::get<JAT_state>(jatep) & JDELETED)) {
                     lUlong jep_JB_job_number;
                     const char *jep_JB_job_name;

                     jep_JB_job_number = ocs::tom::get<JB_job_number>(jep);
                     jep_JB_job_name = ocs::tom::get<JB_job_name>(jep);

                     snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_JOBKILLEDSUBJ_US,
                              jep_JB_job_number, jep_JB_job_name);
                     snprintf(sge_mail_body, sizeof(sge_mail_body), MSG_MAIL_JOBKILLEDBODY_USS,
                              jep_JB_job_number, jep_JB_job_name, hostname);
                     cull_mail(QMASTER, mail_users, sge_mail_subj, sge_mail_body, "job abortion");
                  }

                  /* this job has the killed exechost as master host */
                  uint32_t state = ocs::tom::get<JAT_state>(jatep);
                  SETBIT(JDELETED, state);
                  ocs::tom::set<JAT_state>(jatep, state);

                  /* spool job and propagate state change to event clients */
                  ocs::tom::ListPtr<ocs::tom::AN> answer_list;
                  sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, ocs::tom::get<JB_job_number>(jep),
                                  ocs::tom::get<JAT_task_number>(jatep), nullptr, nullptr, nullptr,
                                  jep, jatep, nullptr, true, true, 0);
                  answer_list_output(&answer_list);
               }
            }
         }
      }
   }

   sge_mark_unheard(lel, packet->gdi_session);

   DRETURN_VOID;
}


/** @brief Marks an execution host as one that added itself to the cluster
 *
 * The time of the registration is stored in EH_auto_added_time, which is spooled, so that the
 * host is still known to have been added automatically after a restart of the master daemon.
 * A host which an administrator added keeps the value 0.
 *
 * @param hep the execution host to mark
 * @param gdi_session the GDI session of the execution daemon registration
 */
static void
host_mark_as_auto_added(const ocs::tom::Element<ocs::tom::EH> hep, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   ocs::tom::set<EH_auto_added_time>(hep, sge_get_gmt64());

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD, 0, 0, ocs::tom::get<EH_name>(hep),
                   nullptr, nullptr, hep, nullptr, nullptr, true, true, gdi_session);
   answer_list_output(&answer_list);

   DRETURN_VOID;
}

/** @brief Adds an execution host which added itself to the cluster to a host group
 *
 * The host group is created if it does not exist yet, else the host is appended to its host list.
 * Queue instances for the new host appear through the host group resolution of the cluster queues
 * which use the host group, nothing has to be done for them here.
 *
 * @param packet the GDI packet of the execution daemon registration
 * @param task the GDI task of the execution daemon registration
 * @param hostname name of the host to add
 * @param hgroup_name name of the host group, including the leading @ character
 * @param monitor for monitoring qmaster threads
 * @return true if the host is part of the host group, false on error
 */
static bool
host_add_to_hgroup(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *hostname,
                   const char *hgroup_name, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list(*ocs::DataStore::get_master_list(SGE_TYPE_HGROUP));
   const ocs::tom::ConstElement<ocs::tom::HGRP> old_hgroup = hgroup_list_locate(master_hgroup_list, hgroup_name);
   int add = old_hgroup == nullptr ? 1 : 0;
   ocs::tom::ListPtr<ocs::tom::HR> href_list;

   if (add == 0) {
      // the host can already be part of the host group, e.g. when it was removed from the exec host
      // list but not from the host group
      if (ocs::tom::find_first<HR_name>(ocs::tom::get<HGRP_host_list>(old_hgroup), hostname) != nullptr) {
         DPRINTF("host " SFQ " is already part of host group " SFQ "\n", hostname, hgroup_name);
         DRETURN(true);
      }
      href_list = ocs::tom::get<HGRP_host_list>(old_hgroup).clone();
   }
   ocs_href_list_emplace(href_list, hostname);

   bool ret = true;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   auto hgroup = hgroup_create(&answer_list, hgroup_name, std::move(href_list), true);
   if (hgroup == nullptr) {
      ret = false;
   } else {
      lList *tmp_list = nullptr;
      gdi_object_t *object = get_gdi_object(ocs::gdi::Target::HGRP_LIST);

      if (sge_gdi_add_mod_generic(packet, task, &answer_list, *hgroup, add, object,
                                  component_get_username(), component_get_qualified_hostname(),
                                  ocs::gdi::Command::NONE, ocs::gdi::SubCommand::NONE,
                                  &tmp_list, monitor) != STATUS_OK) {
         ret = false;
      }
      lFreeList(&tmp_list);
   }
   answer_list_output(&answer_list);

   DRETURN(ret);
}

/** @brief Takes a host out of every host group which names it
 *
 * The host group itself is kept even when it becomes empty: an administrator may have built
 * cluster queues on it, and those would lose their configuration with it. The reserved host
 * groups are left alone, they mirror the host lists and are maintained by the master daemon.
 *
 * @param packet the GDI packet of the request
 * @param task the GDI task of the request
 * @param hostname name of the host to take out
 * @param monitor for monitoring qmaster threads
 * @return true if the host is in no host group any more, false if one of them could not be changed
 */
static bool
host_remove_from_hgroups(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *hostname,
                         monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool ret = true;

   // the list itself is re-read on every pass below, because modifying a host group replaces the
   // element and may even replace the list
   const lList **master_hgroup_list = ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);

   // the host groups to change are collected first: the list is modified while we work on it
   std::vector<std::string> hgroup_names;
   for (const auto hgroup : ocs::tom::ConstList<ocs::tom::HGRP>(*master_hgroup_list)) {
      const char *hgroup_name = ocs::tom::get<HGRP_name>(hgroup);

      if (hgroup_is_reserved(hgroup_name)) {
         continue;
      }
      if (ocs::tom::find_first<HR_name>(ocs::tom::get<HGRP_host_list>(hgroup), hostname) != nullptr) {
         hgroup_names.emplace_back(hgroup_name);
      }
   }

   for (const std::string &hgroup_name : hgroup_names) {
      const ocs::tom::ConstElement<ocs::tom::HGRP> old_hgroup =
              hgroup_list_locate(ocs::tom::ConstList<ocs::tom::HGRP>(*master_hgroup_list), hgroup_name.c_str());
      if (old_hgroup == nullptr) {
         continue;
      }

      // the new host list is the old one without this host
      auto href_list = ocs::tom::get<HGRP_host_list>(old_hgroup).clone();
      const ocs::tom::Element<ocs::tom::HR> href = ocs::tom::find_first_rw<HR_name>(href_list.get(), hostname);
      if (href) {
         href_list.get().erase(href);
      }

      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      auto hgroup = hgroup_create(&answer_list, hgroup_name.c_str(), std::move(href_list), true);
      if (hgroup == nullptr) {
         ret = false;
      } else {
         lList *tmp_list = nullptr;
         gdi_object_t *object = get_gdi_object(ocs::gdi::Target::HGRP_LIST);

         if (sge_gdi_add_mod_generic(packet, task, &answer_list, *hgroup, 0, object,
                                     component_get_username(), component_get_qualified_hostname(),
                                     ocs::gdi::Command::NONE, ocs::gdi::SubCommand::NONE,
                                     &tmp_list, monitor) != STATUS_OK) {
            ret = false;
         }
         lFreeList(&tmp_list);
      }
      answer_list_output(&answer_list);
   }

   DRETURN(ret);
}

/** @brief Whether jobs are running on an execution host
 *
 * @param hostname name of the execution host
 * @return true if at least one slot of one of its queue instances is in use
 */
static bool
host_has_running_jobs(const char *hostname) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE));

   for (const auto cqueue : master_cqueue_list) {
      const ocs::tom::ConstElement<ocs::tom::QU> qinstance =
              ocs::tom::find_first_in<CQ_qinstances, QU_qhostname>(cqueue, hostname);

      if (qinstance != nullptr && qinstance_slots_used(qinstance) > 0) {
         DRETURN(true);
      }
   }

   DRETURN(false);
}

/** @brief Remove execution hosts that added themselves and went silent
 *
 * A host which added itself through one of the qmaster_params AUTO_ADD_HOSTS rules carries the
 * time of its registration in EH_auto_added_time. When the qmaster_params AUTO_REMOVE_HOSTS_AFTER
 * names a time and such a host has been unknown for that long, it is taken out of its host groups
 * and deleted from the execution host list, which removes its queue instances with it. This is
 * what keeps a cloud node which disappeared without a shutdown from showing up in qhost and qstat
 * forever.
 *
 * A host which an administrator added is never removed, however long it has been unreachable, and
 * a host which still has jobs running is kept until they are gone. A host which cannot be deleted,
 * because it is still referenced somewhere, loses its marker, so that the attempt is not repeated
 * on every tick - the same way the cleanup of the automatically created users gives up.
 *
 * EH_lt_heard_from is not a rolling timestamp: it holds the time at which a host became known and
 * is set back to 0 once the master daemon declares the host unknown, which happens after
 * max_unheard. It therefore says whether a host is alive but not for how long it has been gone,
 * so the moment a host is first seen unknown is remembered here. That also gives the behaviour a
 * restart of the master daemon needs: the memory starts out empty, so the time is measured from
 * the first run of this handler rather than from a last contact nobody recorded.
 *
 * @param anEvent the timed event that fired
 * @param monitor for monitoring qmaster threads
 */
void
sge_automatic_host_cleanup_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // when each host was first seen unknown, by host name. Only this handler touches it, and it
   // runs in the timer thread alone
   static std::map<std::string, uint64_t> unknown_since;

   uint32_t remove_after = mconf_get_auto_remove_hosts_after();
   if (remove_after == 0) {
      // nothing is removed, and what we remembered is of no use when it is switched on again
      unknown_since.clear();
      DRETURN_VOID;
   }

   uint64_t now = sge_get_gmt64();
   uint64_t limit = sge_gmt32_to_gmt64(remove_after);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);
   const ocs::tom::List<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));

   // not a range-for: hosts are deleted from the list we walk
   ocs::tom::Element<ocs::tom::EH> next = master_ehost_list.front();
   while (next != nullptr) {
      const ocs::tom::Element<ocs::tom::EH> hep = next;
      next = hep.next();

      const char *hostname = ocs::tom::get<EH_name>(hep);

      // a host which an administrator added carries no time and is never removed
      if (ocs::tom::get<EH_auto_added_time>(hep) == 0) {
         unknown_since.erase(hostname);
         continue;
      }

      // a host we have heard from is alive, whenever that was
      if (ocs::tom::get<EH_lt_heard_from>(hep) != 0) {
         unknown_since.erase(hostname);
         continue;
      }

      auto it = unknown_since.find(hostname);
      if (it == unknown_since.end()) {
         DPRINTF("host " SFQ " is unknown, starting to count\n", hostname);
         unknown_since[hostname] = now;
         continue;
      }
      if (now < it->second + limit) {
         continue;
      }

      if (host_has_running_jobs(hostname)) {
         INFO(MSG_AUTOREMOVEHOSTS_BUSY_S, hostname);
         continue;
      }

      ocs::gdi::Packet packet;
      ocs::gdi::Task task;
      packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;

      uint64_t unknown_for = now - it->second;
      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      bool removed = false;
      if (host_remove_from_hgroups(&packet, &task, hostname, monitor)) {
         char ruser[] = "qmaster";
         char rhost[CL_MAXHOSTNAMELEN];
         strncpy(rhost, component_get_qualified_hostname(), sizeof(rhost) - 1);
         rhost[sizeof(rhost) - 1] = '\0';

         const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list(
                 *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP));
         removed = sge_del_host(&packet, &task, hep, &answer_list, ruser, rhost,
                                ocs::gdi::Target::EH_LIST, master_hgroup_list,
                                monitor) == STATUS_OK;
      }
      answer_list_output(&answer_list);

      if (removed) {
         INFO(MSG_AUTOREMOVEHOSTS_REMOVED_SU, hostname,
              static_cast<uint32_t>(sge_gmt64_to_gmt32(unknown_for)));
         unknown_since.erase(hostname);
      } else {
         // do not try again on every tick, treat the host as one an administrator added
         ERROR(MSG_AUTOREMOVEHOSTS_FAILED_S, hostname);
         ocs::tom::set<EH_auto_added_time>(hep, 0);
         unknown_since.erase(hostname);
      }
   }
   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

/****
 **** sge_execd_startedup
 ****
 **** gdi call for old request starting_up.
 ****/
/** @brief Register an execution daemon that has just started
 *
 * A restart is not the same as a first start: on a restart the jobs qmaster
 * believes are running there have to be reconciled with what the daemon
 * actually found, which is what @p is_restart selects.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param host the execution host
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param target which host list the host belongs to
 * @param monitor for monitoring qmaster threads
 * @param is_restart whether the daemon is re-registering
 * @return STATUS_OK on success
 */
int
sge_execd_startedup(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::EH> host,
                    ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost, const ocs::gdi::Target target,
                    monitoring_t *monitor, const bool is_restart) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::EH> hep;
   dstring ds;
   char buffer[256];
   const ocs::tom::List<ocs::tom::EH> master_ehost_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST));
   const ocs::tom::List<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE));

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   if (!host || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   bool host_was_added = false;
   // host_list_locate() has a raw form only; the list holds execution hosts
   hep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(master_ehost_list, rhost));
   if (!hep) {
      if (sge_add_host_of_type(packet, task, rhost, ocs::gdi::Target::EH_LIST, monitor) < 0) {
         ERROR(MSG_OBJ_INVALIDHOST_S, rhost);
         answer_list_add(alpp, SGE_EVENT, STATUS_DENIED, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_DENIED);
      }

      hep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(master_ehost_list, rhost));
      if (!hep) {
         ERROR(MSG_OBJ_NOADDHOST_S, rhost);
         answer_list_add(alpp, SGE_EVENT, STATUS_DENIED, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_DENIED);
      }
      host_was_added = true;
   }

   ocs::tom::set<EH_report_seqno>(hep, 0);

   // a host which was not known before and which matches one of the rules of the qmaster_params
   // AUTO_ADD_HOSTS added itself to the cluster - mark it as automatically added and put it into
   // the host group which the matching rule names
   if (host_was_added) {
      std::string hgroup_name;

      if (ocs::AutoAddHosts::match_host(rhost, packet->ip, hgroup_name)) {
         INFO(MSG_AUTOADDHOSTS_HOST_S, rhost);
         host_mark_as_auto_added(hep, packet->gdi_session);

         if (!hgroup_name.empty()) {
            if (host_add_to_hgroup(packet, task, rhost, hgroup_name.c_str(), monitor)) {
               INFO(MSG_AUTOADDHOSTS_HGROUP_SS, rhost, hgroup_name.c_str());
            } else {
               // the host itself is added, rejecting the registration would not help anybody
               ERROR(MSG_AUTOADDHOSTS_HGROUPFAILED_SS, rhost, hgroup_name.c_str());
            }
         }
      }
   }

   /*
    * reinit state of all qinstances at this host according to initial_state
    */
   if (!is_restart) {
      for (const auto cqueue : master_cqueue_list) {
         ocs::tom::ConstList<ocs::tom::QU> qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
         ocs::tom::Element<ocs::tom::QU> qinstance = ocs::tom::find_first_rw<QU_qhostname>(qinstance_list, rhost);

         if (qinstance != nullptr) {
            if (sge_qmaster_qinstance_set_initial_state(qinstance, packet->gdi_session)) {
               ocs::tom::ListPtr<ocs::tom::AN> answer_list;

               qinstance_increase_qversion(qinstance);
               sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, ocs::tom::get<QU_qname>(qinstance),
                               rhost, nullptr, qinstance, nullptr, nullptr, true, true, packet->gdi_session);
               answer_list_output(&answer_list);
            }
         }
      }
   }

   DPRINTF("=====>STARTING_UP: %s %s on >%s< is starting up\n",
           feature_get_product_name(FS_SHORT_VERSION, &ds), "execd", rhost);

   INFO(MSG_LOG_REGISTER_SS, "execd", rhost);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_ERROR);

   DRETURN(STATUS_OK);
}



/** @brief Check that every load scaling of a host names a known complex attribute
 *
 * @param answer_list receives messages for the caller
 * @param host the execution host
 * @return STATUS_OK, or STATUS_EUNKNOWN for an unknown attribute
 */
static int
verify_scaling_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstElement<ocs::tom::EH> host) {
   DENTER(TOP_LAYER);
   bool ret = true;
   const ocs::tom::ConstList<ocs::tom::CE> master_centry_list(*ocs::DataStore::get_master_list(SGE_TYPE_CENTRY));

   for (const auto hs_elem : ocs::tom::get<EH_scaling_list>(host)) {
      const char *name = ocs::tom::get<HS_name>(hs_elem);
      const ocs::tom::ConstElement<ocs::tom::CE> centry = centry_list_locate(master_centry_list, name);

      if (centry == nullptr) {
         ERROR(MSG_OBJ_NOSCALING4HOST_SS, name, static_cast<const char *>(ocs::tom::get<EH_name>(host)));
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
         break;
      }
   }
   DRETURN(ret ? STATUS_OK : STATUS_EUNKNOWN);
}

/**
 * @brief Diff exechost sublists
 *
 * Makes a diff userset/project sublists of an exec host.
 *
 * @param new_host New exec host (EH_Type)
 * @param old_host Old exec host (EH_Type)
 * @param snm1 First exec host sublist field
 * @param snm2 Second exec host sublist field
 * @param key_nm Field with key in sublist
 * @param dp Type for outgoing sublist arguments
 * @param new_sublist List of new references
 * @param old_sublist List of old references
 *
 * @note MT-NOTE: host_diff_sublist() is MT safe
 */
static void
host_diff_sublist(const lListElem *new_host, const lListElem *old_host, int snm1, int snm2, int key_nm,
                  const lDescr *dp, lList **new_sublist, lList **old_sublist) {
   const lListElem *ep;
   const char *p;

   /* collect 'old' entries in 'old_sublist' */
   if (old_host && old_sublist) {
      for_each_ep(ep, lGetList(old_host, snm1)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*old_sublist, key_nm, p))
            lAddElemStr(old_sublist, key_nm, p, dp);
      }
      for_each_ep(ep, lGetList(old_host, snm2)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*old_sublist, key_nm, p))
            lAddElemStr(old_sublist, key_nm, p, dp);
      }
   }

   /* collect 'new' entries in 'new_sublist' */
   if (new_host && new_sublist) {
      for_each_ep(ep, lGetList(new_host, snm1)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*new_sublist, key_nm, p))
            lAddElemStr(new_sublist, key_nm, p, dp);
      }
      for_each_ep(ep, lGetList(new_host, snm2)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*new_sublist, key_nm, p))
            lAddElemStr(new_sublist, key_nm, p, dp);
      }
   }
}


/**
 * @brief Diff old/new exec host projects
 *
 * A diff new/old is made regarding exec host projects/xprojects.
 * Project references are returned in new_prj/old_prj.
 *
 * @param new_host New exec host (EH_Type)
 * @param old_host Old exec host (EH_Type)
 * @param new_prj New project references (US_Type)
 * @param old_prj Old project references (US_Type)
 *
 * @note MT-NOTE: host_diff_projects() is not MT safe
 */
void
host_diff_projects(const ocs::tom::ConstElement<ocs::tom::EH> new_host,
                   const ocs::tom::ConstElement<ocs::tom::EH> old_host, lList **new_prj, lList **old_prj) {
   // the lists accumulate over several calls of the caller, so the out-parameters stay raw
   host_diff_sublist(new_host, old_host, EH_prj, EH_xprj, PR_name, PR_Type, new_prj, old_prj);
   lDiffListStr(PR_name, new_prj, old_prj);
}

/**
 * @brief Diff old/new exec host usersets
 *
 * A diff new/old is made regarding exec host acl/xacl.
 * Userset references are returned in new_acl/old_acl.
 *
 * @param new_host New exec host (EH_Type)
 * @param old_host Old exec host (EH_Type)
 * @param new_acl New userset references (US_Type)
 * @param old_acl Old userset references (US_Type)
 *
 * @note MT-NOTE: host_diff_usersets() is not MT safe
 */
void
host_diff_usersets(const ocs::tom::ConstElement<ocs::tom::EH> new_host,
                   const ocs::tom::ConstElement<ocs::tom::EH> old_host, lList **new_acl, lList **old_acl) {
   // the lists accumulate over several calls of the caller, so the out-parameters stay raw
   host_diff_sublist(new_host, old_host, EH_acl, EH_xacl, US_name, US_Type, new_acl, old_acl);
   lDiffListStr(US_name, new_acl, old_acl);
}


/**
 * @brief Update categories wrts userset/project
 *
 * The userset/project information wrts categories is updated based
 * on new/old exec host configuration and events are sent upon
 * changes.
 *
 * @param new_hep New exec host (EH_Type)
 * @param old_hep Old exec host (EH_Type)
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: host_update_categories() is not MT safe
 */
static void
host_update_categories(const ocs::tom::ConstElement<ocs::tom::EH> new_hep,
                       const ocs::tom::ConstElement<ocs::tom::EH> old_hep, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   {
      // host_diff_projects() fills raw out-parameters, the lists are owned right after
      lList *raw_new = nullptr;
      lList *raw_old = nullptr;
      host_diff_projects(new_hep, old_hep, &raw_new, &raw_old);
      const ocs::tom::ListPtr<ocs::tom::PR> new_lp(raw_new);
      const ocs::tom::ListPtr<ocs::tom::PR> old_lp(raw_old);
      project_update_categories(new_lp.get(), old_lp.get(), gdi_session);
   }

   {
      lList *raw_new = nullptr;
      lList *raw_old = nullptr;
      host_diff_usersets(new_hep, old_hep, &raw_new, &raw_old);
      const ocs::tom::ListPtr<ocs::tom::US> new_lp(raw_new);
      const ocs::tom::ListPtr<ocs::tom::US> old_lp(raw_old);
      userset_update_categories(new_lp.get(), old_lp.get(), gdi_session);
   }
   DRETURN_VOID;
}

/**
 * @brief Modify the threshold configuration sublist
 *
 * Validation tries to find each element of the ep element in the threshold identified by nm.
 * Elements which already exist here are copied into sublist of new_ep.
 *
 * @param alpp The answer list
 * @param ep The source object element
 * @param new_ep The target object element
 * @param sub_command The add, modify, remove command
 * @param attr_name The attribute name
 * @param object_name The target object name
 *
 * @return 0 if success
 *
 * @note MT-NOTE: attr_mod_threshold() is MT safe
 */
static int
attr_mod_threshold(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<ocs::tom::EH> ep,
                   const ocs::tom::Element<ocs::tom::EH> new_ep,
                   const ocs::gdi::Command cmd, const ocs::gdi::SubCommand sub_command, const char *attr_name,
                   const char *object_name) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::CE> master_centry_list(*ocs::DataStore::get_master_list(SGE_TYPE_CENTRY));
   const ocs::tom::List<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB));
   const ocs::tom::ConstList<ocs::tom::AR> master_ar_list(*ocs::DataStore::get_master_list(SGE_TYPE_AR));
   const ocs::tom::ConstList<ocs::tom::PE> master_pe_list(*ocs::DataStore::get_master_list(SGE_TYPE_PE));

   /* ---- attribute EH_consumable_config_list */
   if (ocs::tom::has<EH_consumable_config_list>(ep)) {
      DPRINTF("got new %s\n", attr_name);

      // reject duplicate entries
      if (ocs::CEntry::has_duplicates(ocs::tom::get<EH_consumable_config_list>(ep), alpp, "complex_values")) {
         DRETURN(STATUS_EUNKNOWN);
      }

      /* check if corresponding complex attributes exist (the field is passed as a run-time number) */
      if (ensure_attrib_available(alpp, ep, EH_consumable_config_list, master_centry_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      /* work on a temporary element to allow rollback on error */
      auto tmp_elem = new_ep.clone();

      /* the attr_mod_sub_list return boolean and there is stored in the int value, attention true=1 */
      if (!attr_mod_sub_list(alpp, *tmp_elem, EH_consumable_config_list, CE_name, ep, cmd,
                             sub_command, attr_name, object_name, 0, nullptr)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      // ensure that slots-attribute is part of the list
      if (host_ensure_slots_are_defined(*tmp_elem, ocs::tom::get<EH_processors>(new_ep))) {
         DRETURN(STATUS_EUNKNOWN);
      }

      // fill missing attributes in EH_consumable_config_list
      if (centry_list_fill_request(ocs::tom::get<EH_consumable_config_list>(*tmp_elem), alpp, master_centry_list, true,
                                   false, false)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      // an RSMAP written as a bare amount gets the ids 0 to amount-1 here - has to happen before
      // the resources are debited below, which books the utilization per id
      if (!centry_list_rsmap_expand_implicit_ids(alpp,
                                                 ocs::tom::get<EH_consumable_config_list>(*tmp_elem))) {
         DRETURN(STATUS_EUNKNOWN);
      }

      // debit resources
      {
         const char *host = ocs::tom::get<EH_name>(*tmp_elem);
         int global_host = !strcmp(SGE_GLOBAL_NAME, host);

         // initialize booking
         ocs::tom::set<EH_resource_utilization>(*tmp_elem, nullptr);
         debit_host_consumable(ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::JAT>(),
                               ocs::tom::ConstList<ocs::tom::GRU>(), ocs::tom::ConstElement<ocs::tom::PE>(),
                               *tmp_elem, master_centry_list, 0, true, true, nullptr);

         // do the resource booking
         for (const auto jep : master_job_list) {
            for (const ocs::tom::ConstElement<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
               ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatep);
               int slots;
               bool is_master_task = false;
               const void *iterator = nullptr;
               const ocs::tom::ConstElement<ocs::tom::PE> pe = ocs::tom::get<JAT_pe_object>(jatep);
               ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(jatep);

               if (global_host || (gdil.front() == ocs::tom::find_first<JG_qhostname>(gdil, host, &iterator))) {
                  is_master_task = true;
               }

               slots = nslots_granted(gdil, global_host ? nullptr : host);

               if (slots > 0) {
                  // do_per_host_booking is true, we book on one host once
                  debit_host_consumable(jep, jatep, granted_resources_list, pe, *tmp_elem, master_centry_list, slots,
                                        is_master_task, true, nullptr);
               }
            }
         }

         for (const auto ar_ep : master_ar_list) {
            ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<AR_granted_slots>(ar_ep);
            const void *iterator = nullptr;
            ocs::tom::ConstElement<ocs::tom::JG> gdil_ep = ocs::tom::find_first<JG_qhostname>(gdil, host, &iterator);

            // in case the master task is running on this host, need to book per JOB consumables
            bool is_master_task = false;
            if (gdil_ep == gdil.front()) {
               is_master_task = true;
            }

            // we book per host consumables once
            bool do_per_host_booking = true;

            const char *pe_name = ocs::tom::get<AR_granted_pe>(ar_ep);
            ocs::tom::ConstElement<ocs::tom::PE> pe = nullptr;
            if (pe_name != nullptr) {
               pe = pe_list_locate(master_pe_list, pe_name);
            }

            if (gdil_ep != nullptr) {
               auto dummy_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
               job_set_hard_resource_list(*dummy_job, ocs::tom::get<AR_resource_list>(ar_ep).clone());

               while (gdil_ep != nullptr) {
                  rc_add_job_utilization(gdil_ep, *dummy_job, pe, 0,
                                         SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                         *tmp_elem, master_centry_list,
                                         ocs::tom::get<JG_slots>(gdil_ep), host, ocs::tom::get<AR_start_time>(ar_ep),
                                         ocs::tom::get<AR_duration>(ar_ep), HOST_TAG, false, is_master_task,
                                         do_per_host_booking, ocs::tom::get<AR_granted_resources_list>(ar_ep));
                  is_master_task = false;
                  do_per_host_booking = false;
                  gdil_ep = ocs::tom::find_next<JG_qhostname>(gdil, host, &iterator);
               }
            }
         }
      }

      if (ar_list_has_reservation_due_to_host_complex_attr(master_ar_list, alpp,
                                                           *tmp_elem, master_centry_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      /* copy back the consumable config and resource utilization lists to new exec host object */
      {
         auto t = tmp_elem->exchange<EH_consumable_config_list>(ocs::tom::ListPtr<ocs::tom::CE>());
         t = new_ep.exchange<EH_consumable_config_list>(std::move(t));
         static_cast<void>(tmp_elem->exchange<EH_consumable_config_list>(std::move(t)));

         auto u = tmp_elem->exchange<EH_resource_utilization>(ocs::tom::ListPtr<ocs::tom::RUE>());
         u = new_ep.exchange<EH_resource_utilization>(std::move(u));
         static_cast<void>(tmp_elem->exchange<EH_resource_utilization>(std::move(u)));
      }
   }

   DRETURN(0);
}





