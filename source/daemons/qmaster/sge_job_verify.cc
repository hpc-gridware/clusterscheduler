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
 *  Copyright: 2008 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Verifying a submitted job before it is accepted
 */

#include <cerrno>
#include <optional>
#include <string>

#include "sge.h"

#include "uti/sge_bootstrap_env.h"
#include "uti/sge_log.h"
#include "uti/sge_monitor.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_security.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_conf.h"

#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_job_qmaster.h"
#include "symbols.h"
#include "msg_common.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CX.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_SU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "msg_qmaster.h"

#include <sge_str.h>

/**
 * @brief Reject a job with a global hard queue request and a master or slave hard queue request
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @return true when the job is acceptable
 */
static bool
sge_job_verify_global_master_slave_queues(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                          const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (job_get_hard_queue_list(jep)) {
      if (job_get_hard_queue_list(jep, JRS_SCOPE_MASTER)) {
         ERROR(MSG_JOB_GLOBALMASTERSLAVEQ_S, "master");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (ret && job_get_hard_queue_list(jep, JRS_SCOPE_SLAVE)) {
         ERROR(MSG_JOB_GLOBALMASTERSLAVEQ_S, "slave");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Reject a job requesting a resource in the global scope and in the master or slave scope
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @param soft true to check the global soft requests, false for the global hard requests
 * @return true when the job is acceptable
 */
static bool
sge_job_verify_global_master_slave_requests(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                            const ocs::tom::ConstElement<ocs::tom::JB> jep, const bool soft) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const auto global_requests = soft ? job_get_soft_resource_list(jep) : job_get_hard_resource_list(jep);

   if (global_requests) {
      const auto master_requests = job_get_hard_resource_list(jep, JRS_SCOPE_MASTER);
      const auto slave_requests = job_get_hard_resource_list(jep, JRS_SCOPE_SLAVE);
      for (const auto ep : global_requests) {
         const char *name = ep.get<CE_name>();
         if (centry_list_locate(master_requests, name)) {
            ERROR(MSG_JOB_GLOBALMASTERSLAVE_SSS, soft ? "soft" : "hard", name, "master");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         if (centry_list_locate(slave_requests, name)) {
            ERROR(MSG_JOB_GLOBALMASTERSLAVE_SSS, soft ? "soft" : "hard", name, "slave");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
      }
   }

   DRETURN(ret);
}

/** @brief Reject a parallel job whose global request conflicts with its master or slave request
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @return true when the job is acceptable
 */
bool
sge_job_verify_global_master_slave_allocation_rule(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                                   const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const char *global_rule = job_get_allocation_rule(jep, JRS_SCOPE_GLOBAL);
   const char *master_rule = job_get_allocation_rule(jep, JRS_SCOPE_MASTER);
   const char *slave_rule = job_get_allocation_rule(jep, JRS_SCOPE_SLAVE);

   if (global_rule != nullptr) {
      if (master_rule != nullptr) {
         ERROR(MSG_JOB_GLOBALMASTERSLAVEA_S, "master");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (slave_rule != nullptr) {
         ERROR(MSG_JOB_GLOBALMASTERSLAVEA_S, "slave");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   // It doesn't make sense to specify $pe_slots for master or slave scope - reject it.
   if (master_rule != nullptr && strcmp(master_rule, "$pe_slots") == 0) {
      ERROR(MSG_JOB_MASTERSLAVE_A_SS, master_rule, "master");
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (slave_rule != nullptr && strcmp(slave_rule, "$pe_slots") == 0) {
      ERROR(MSG_JOB_MASTERSLAVE_A_SS, slave_rule, "slave");
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   DRETURN(ret);
}

/** @brief Verify the per-job requests a slave task may not make
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @param centry_list see the brief above
 * @return true when the job is acceptable
 */
bool
sge_job_verify_slave_per_job_requests(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                      const ocs::tom::ConstElement<ocs::tom::JB> jep,
                                      const ocs::tom::ConstList<ocs::tom::CE> centry_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   // we only allow slave hard requests, no soft requests
   for (const auto request : job_get_hard_resource_list(jep, JRS_SCOPE_SLAVE)) {
      const char *name = request.get<CE_name>();
      if (name != nullptr) {
         const auto centry = centry_list_locate(centry_list, name);
         if (centry) {
            uint32_t consumable = centry.get<CE_consumable>();
            if (consumable == CONSUMABLE_JOB) {
               ERROR(MSG_JOB_SLAVEPERJOBREQUEST_S, name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
      }
   }

   DRETURN(ret);
}

/** @brief Verify the per-host resource requests of a parallel job
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @param master_centry_list see the brief above
 * @return true when the job is acceptable
 */
bool
sge_job_verify_per_host_requests(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                                 const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const auto master_request_list = job_get_hard_resource_list(jep, JRS_SCOPE_MASTER);
   const auto slave_request_list = job_get_hard_resource_list(jep, JRS_SCOPE_SLAVE);
   if (master_request_list && slave_request_list) {
      for (const auto master_request : master_request_list) {
         const char *name = master_request.get<CE_name>();
         if (name != nullptr) {
            const auto centry = centry_list_locate(master_centry_list, name);
            if (centry) {
               uint32_t consumable = centry.get<CE_consumable>();
               if (consumable == CONSUMABLE_HOST) {
                  if (slave_request_list.find_first<CE_name>(name)) {
                     ERROR(MSG_JOB_PERHOSTINBOTHMASTERSLAVE_S, name);
                     answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                     ret = false;
                     break;
                  }
               }
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Reject soft queue or soft resource requests in the master or slave scope
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @return true when the job is acceptable
 */
static bool
job_verify_soft_master_slave_requests(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                      const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);
   bool ret = true;

   const auto jrs_list = jep.get<JB_request_set_list>();
   if (jrs_list) {
      for (const auto jrs : jrs_list) {
         uint32_t scope = jrs.get<JRS_scope>();

         // we do not allow master and slave soft queue requests
         if (scope == JRS_SCOPE_MASTER || scope == JRS_SCOPE_SLAVE) {
            if (jrs.get<JRS_soft_queue_list>()) {
               ERROR(SFNMAX, MSG_JOB_MASTERSLAVESOFTQUEUE);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
            // we do not allow master and slave soft resource requests
            if (jrs.get<JRS_soft_resource_list>()) {
               ERROR(SFNMAX, MSG_JOB_MASTERSLAVESOFTREQ);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Reject master or slave scope requests of a job that is not parallel
 *
 * @param alpp receives messages for the caller
 * @param jep the job (`JB`)
 * @return true when the job is acceptable
 */
static bool
job_verify_non_pe_soft_master_slave_requests(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                             const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (jep.get<JB_pe>() == nullptr) {
      const auto jrs_list = jep.get<JB_request_set_list>();
      if (jrs_list) {
         for (const auto jrs : jrs_list) {
            if (jrs.get<JRS_scope>() != JRS_SCOPE_GLOBAL) {
               ERROR(SFNMAX, MSG_JOB_MASTERSLAVENONPE);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief refuse a resource map which is requested with different scope= in different scopes
 *
 * How far a same= constraint reaches is a property of the map and not of the request scope it
 * was written in: the master task and the slave tasks take their instances from the same maps,
 * and binding every host of the job to one group is either asked for or it is not. Two scopes
 * which name the same map and disagree about scope= therefore ask for two things at once, and
 * whichever the scheduler took would silently be the wrong one for the other scope.
 *
 * Only master against slave is looked at, because a request in the global scope together with
 * one in master or slave is already refused by sge_job_verify_global_master_slave_requests().
 * The comparison is against the effective value, so leaving scope= out in one scope and writing
 * scope=host in the other is not a conflict - both mean per host.
 *
 * @param alpp answer list which is filled in case of errors
 * @param jep  job carrying the request sets
 * @return true when every scope agrees about every map, false otherwise
 */
static bool
sge_job_verify_rsmap_scope_requests(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                    const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);
   bool ret = true;

   for (const auto jrs : jep.get<JB_request_set_list>()) {
      for (const auto request : jrs.get<JRS_hard_resource_list>()) {
         if (static_cast<ocs::CEntry::Type>(request.get<CE_valtype>()) !=
             ocs::CEntry::Type::RSMAP) {
            continue;
         }
         const char *name = request.get<CE_name>();
         const char *scope = centry_rsmap_request_is_job_scope(request)
                             ? RSMAP_REQUEST_PARAM_SCOPE_JOB : RSMAP_REQUEST_PARAM_SCOPE_HOST;

         // only the scopes behind this one, so that a pair is reported once and not twice
         for (auto other_jrs = jrs.next(); other_jrs; other_jrs = other_jrs.next()) {
            const auto other = other_jrs.get<JRS_hard_resource_list>().find_first<CE_name>(name);
            if (!other) {
               continue;
            }
            const char *other_scope = centry_rsmap_request_is_job_scope(other)
                                      ? RSMAP_REQUEST_PARAM_SCOPE_JOB
                                      : RSMAP_REQUEST_PARAM_SCOPE_HOST;
            if (strcmp(scope, other_scope) != 0) {
               ERROR(MSG_RSMAP_PARAM_SCOPE_CONFLICT_SSSS, name, RSMAP_REQUEST_PARAM_SCOPE,
                     scope, other_scope);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
         if (!ret) {
            break;
         }
      }
      if (!ret) {
         break;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Do all request set related checks
 *
 * Calls all the request set verification functions above:
 * - sge_job_verify_global_master_slave_queues()
 * - sge_job_verify_global_master_slave_requests()
 * - sge_job_verify_slave_per_job_requests()
 * - sge_job_verify_per_host_requests()
 * - job_verify_soft_master_slave_requests()
 * - job_verify_non_pe_soft_master_slave_requests()
 * - sge_job_verify_rsmap_scope_requests()
 *
 * @param alpp answer list which is filled in case of errors
 * @param jep job containing the request set
 * @param master_centry_list list of centry definitions
 *
 * @return true if all checks passed; false with `alpp` filled otherwise
 */
bool
job_verify_adjust_request_set(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                              const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   /* check for a non-parallel job that defines master or slave requests */
   if (ret) {
      ret = job_verify_non_pe_soft_master_slave_requests(alpp, jep);
   }

   // check for soft master or slave requests - we don't allow them (for now)
   if (ret) {
      ret = job_verify_soft_master_slave_requests(alpp, jep);
   }

   // verify that there are no requests on the same variable in global scope and one of master or slave
   if (ret) {
      if (!sge_job_verify_global_master_slave_requests(alpp, jep, false) ||
          !sge_job_verify_global_master_slave_requests(alpp, jep, true)) {
         ret = false;
      }
   }

   // check for slave requests of per job consumables (which are only granted to the master task)
   if (ret) {
      ret = sge_job_verify_slave_per_job_requests(alpp, jep, master_centry_list);
   }

   // verify that there are no hard queue requests in the global scope and one of master or slave
   if (ret) {
      ret = sge_job_verify_global_master_slave_queues(alpp, jep);
   }

   // verify that per host requests are not in both master and slave requests
   if (ret) {
      ret = sge_job_verify_per_host_requests(alpp, jep, master_centry_list);
   }

   // verify that there are no allocation rule specifications in global scope and one of master or slave
   if (ret) {
      ret = sge_job_verify_global_master_slave_allocation_rule(alpp, jep);
   }

   // verify that a resource map is not requested with a different scope= in different scopes
   if (ret) {
      ret = sge_job_verify_rsmap_scope_requests(alpp, jep);
   }

   DRETURN(ret);
}

/** @brief Verify a submitted job and fill in what it left unsaid
 *
 * @param jep the job (`JB`)
 * @param alpp receives messages for the caller
 * @param lpp the master list
 * @param packet the client request
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @return STATUS_OK when the job is acceptable, STATUS_ESYNTAX among others
 *         when a unique attribute holds a key twice
 */
int
sge_job_verify_adjust(const ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                      ocs::tom::ListData **lpp, ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int ret = STATUS_OK;

   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
   const auto master_user_list = ocs::DataStore::master_list<SGE_TYPE_USER>();
   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
   const auto master_suser_list = ocs::DataStore::master_list_rw<SGE_TYPE_SUSER>();

   if (!jep) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }

   /* check min_uid */
   if (ret == STATUS_OK) {
      if (packet->uid < mconf_get_min_uid()) {
         ERROR(MSG_JOB_UID2LOW_II, (int) packet->uid, (int) mconf_get_min_uid());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* check min_gid */
   if (ret == STATUS_OK) {
      if (packet->gid < mconf_get_min_gid()) {
         ERROR(MSG_JOB_GID2LOW_II, (int) packet->gid, (int) mconf_get_min_gid());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /*
    * adjust user and group
    *
    * we cannot rely on the information we got from the client
    * therefore we fill in the data we got from communication
    * library.
    */
   if (ret == STATUS_OK) {
      if (!job_set_owner_and_group(jep, packet->uid, packet->gid, packet->user, packet->group, packet->amount,
                                   packet->grp_array)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   // Check if there are binding parameters that are required but unset
   ocs::Job::binding_set_missing_defaults(jep, alpp);

   /*
    * fill name and shortcut for all requests
    * fill numeric values for all bool, time, memory and int type requests
    * use the master_CEntry_list for all fills
    * JB_hard/soft_resource_list points to a CE list
    */
   for (const auto jrs : jep.get<JB_request_set_list>()) {
      uint32_t scope = jrs.get<JRS_scope>();
      DPRINTF("request set of scope %s\n", job_scope_name(scope));

      const auto hard_resource_list = jrs.get<JRS_hard_resource_list>();
      if (centry_list_fill_request(hard_resource_list, alpp, master_centry_list, false, true, false)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (compress_ressources(alpp, hard_resource_list, SGE_OBJ_JOB)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (!centry_list_is_correct(hard_resource_list, alpp)) {
         ret = STATUS_EUNKNOWN;
         break;
      }

      const auto soft_resource_list = jrs.get<JRS_soft_resource_list>();
      if (centry_list_fill_request(soft_resource_list, alpp, master_centry_list, false, true, false)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (compress_ressources(alpp, soft_resource_list, SGE_OBJ_JOB)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (deny_soft_consumables(alpp, soft_resource_list, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (!centry_list_is_correct(soft_resource_list, alpp)) {
         ret = STATUS_EUNKNOWN;
         break;
      }

      const auto queue_list = jrs.get<JRS_hard_queue_list>();
      if (!qref_list_is_valid(queue_list, alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      const auto queue_list_qr = jrs.get<JRS_soft_queue_list>();
      if (!qref_list_is_valid(queue_list_qr, alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
         break;
      }

      // JRS_allocation_rule
      const char *allocation_rule = jrs.get<JRS_allocation_rule>();
      if (allocation_rule != nullptr) {
         if (!pe_validate_allocation_rule(alpp, allocation_rule)) {
            ret = STATUS_EUNKNOWN;
            break;
         }
      }
   }

   if (ret == STATUS_OK) {
      if (!job_verify_adjust_request_set(alpp, jep, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /* check for qsh without DISPLAY set */
   if (ret == STATUS_OK) {
      if (JOB_TYPE_IS_QSH(jep.get<JB_type>())) {
         ret = job_check_qsh_display(jep, alpp, false);
      }
   }

   /*
    * verify and adjust array job ids:
    *    JB_ja_structure, JB_ja_n_h_ids, JB_ja_u_h_ids, 
    *    JB_ja_s_h_ids, JB_ja_o_h_ids, JB_ja_a_h_ids, JB_ja_z_ids
    */
   if (ret == STATUS_OK) {
      job_check_correct_id_sublists(jep, alpp);
      if (answer_list_has_error(alpp)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /*
    * resolve host names contained in path names
    */
   if (ret == STATUS_OK) {
      int s1, s2, s3, s4;

      s1 = job_resolve_host_for_path_list(jep.get<JB_stdout_path_list>(), alpp);
      s2 = job_resolve_host_for_path_list(jep.get<JB_stdin_path_list>(), alpp);
      s3 = job_resolve_host_for_path_list(jep.get<JB_shell_list>(), alpp);
      s4 = job_resolve_host_for_path_list(jep.get<JB_stderr_path_list>(), alpp);
      if (s1 != STATUS_OK || s2 != STATUS_OK || s3 != STATUS_OK || s4 != STATUS_OK) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /* take care that non-binary jobs have a script */
   if (ret == STATUS_OK) {
      if ((!JOB_TYPE_IS_BINARY(jep.get<JB_type>()) &&
           !jep.get<JB_script_ptr>() && jep.get<JB_script_file>())) {
         ERROR(SFNMAX, MSG_JOB_NOSCRIPT);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* set the jobs submission time */
   if (ret == STATUS_OK) {
      jep.set<JB_submission_time>(sge_get_gmt64());
   }

   /* initialize the task template element and other sublists */
   if (ret == STATUS_OK) {
      jep.set<JB_ja_tasks>(nullptr);
      jep.set<JB_jid_successor_list>(nullptr);
      jep.set<JB_ja_ad_successor_list>(nullptr);
      if (!jep.get<JB_ja_template>()) {
         jep.emplace_back_in<JB_ja_template, JAT_task_number>(0);
      }
   }

   /* verify or set the account string */
   if (ret == STATUS_OK) {
      if (!jep.get<JB_account>()) {
         jep.set<JB_account>(DEFAULT_ACCOUNT);
      } else {
         if (verify_str_key(alpp, jep.get<JB_account>(), MAX_VERIFY_STRING,
                            "account string", QSUB_TABLE) != STATUS_OK) {
            ret = STATUS_EUNKNOWN;
         }
      }
   }

#if !defined(WITH_EXTENSIONS)
   if (ret == STATUS_OK) {
      if (jep.get<JB_binding>()) {
         // in OCS bsort is not available
         std::string binding_sort = ocs::Job::binding_get_sort(jep);
         if (binding_sort != NONE_STR) {
            ERROR(SFN2, MSG_SGETEXT_BINDING_NOT_AVAILABLE);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOTAVAILABLE, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOTAVAILABLE);
         }

         // in OCS binding start is not available
         ocs::BindingStart::Start binding_start = ocs::Job::binding_get_start(jep);
         if (binding_start != ocs::BindingStart::UNINITIALIZED && binding_start != ocs::BindingStart::NONE) {
            ERROR(SFN2, MSG_SGETEXT_BINDING_NOT_AVAILABLE);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOTAVAILABLE, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOTAVAILABLE);
         }

         // in OCS binding stop is not available
         ocs::BindingStop::Stop binding_stop = ocs::Job::binding_get_stop(jep);
         if (binding_stop != ocs::BindingStop::UNINITIALIZED && binding_stop != ocs::BindingStop::NONE) {
            ERROR(SFN2, MSG_SGETEXT_BINDING_NOT_AVAILABLE);
            answer_list_add(alpp, SGE_EVENT, STATUS_ENOTAVAILABLE, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ENOTAVAILABLE);
         }
      }
   }
#endif

   /* verify the job name */
   if (ret == STATUS_OK) {
      if (object_verify_name<JB_job_name>(jep, alpp)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /* is the max. size of array jobs exceeded? */
   if (ret == STATUS_OK) {
      uint32_t max_aj_tasks = mconf_get_max_aj_tasks();

      if (max_aj_tasks > 0) {
         uint32_t submit_size = range_list_get_number_of_ids(jep.get<JB_ja_structure>());

         if (submit_size > max_aj_tasks) {
            ERROR(MSG_JOB_MORETASKSTHAN_U, max_aj_tasks);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   // the instructions of -ac, -sc and -dc are applied to the context the job was sent with,
   // which is empty unless a GDI client sent one; the job keeps only the result (CS-3058)
   if (ret == STATUS_OK && jep.has<JB_context_change>() && jep.get<JB_context_change>()) {
      set_context(jep.get<JB_context_change>(), jep);
      jep.set<JB_context_change>(nullptr);
   }

#if 0
   // if verification failed so far, we don't need to continue
   if (ret != STATUS_OK) {
      //DRETURN(ret);
   }
#endif

   /*
    * Following block should only be executed once, when the job has no job id.
    *
    * At first, we try to find a job id which is not yet used. AFTER that we need
    * to set the submission time. Only this makes wure that forced separation 
    * in time is effective in case of job ID rollover.
    */
   /* ORDER IS IMPORTANT */
   if (jep.get<JB_job_number>() == 0) {
      uint32_t jid;

      do {
         jid = sge_get_job_number(monitor);
      } while (master_job_list.find_first<JB_job_number>(jid));
      jep.set<JB_job_number>(jid);
      jep.set<JB_submission_time>(sge_get_gmt64());
   }

   /*
    * JB_exec_file is the file qmaster spools the job script to, and execd writes and removes the script under
    * that name. It is always set here, never taken from the client (CS-3134): job_scripts/<job id> for a job
    * with a script, empty for one without (interactive jobs).
    */
   if (jep.get<JB_script_file>()) {
      dstring string = DSTRING_INIT;
      sge_dstring_sprintf(&string, "%s/" sge_u32, EXEC_DIR, jep.get<JB_job_number>());
      jep.set<JB_exec_file>(sge_dstring_get_string(&string));
      sge_dstring_free(&string);
   } else {
      jep.set<JB_exec_file>(nullptr);
   }

   /* check max_jobs */
   if (job_list_register_new_job(master_job_list, mconf_get_max_jobs(), 0)) {
      INFO(MSG_JOB_ALLOWEDJOBSPERCLUSTER, mconf_get_max_jobs());
      answer_list_add(alpp, SGE_EVENT, STATUS_NOTOK_DOAGAIN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_NOTOK_DOAGAIN);
   }

   if (jep.get<JB_verify_suitable_queues>() != JUST_VERIFY &&
       jep.get<JB_verify_suitable_queues>() != POKE_VERIFY) {
      if (suser_check_new_job(jep, mconf_get_max_u_jobs(), master_suser_list) != 0) {
         INFO(MSG_JOB_ALLOWEDJOBSPERUSER_UU, mconf_get_max_u_jobs(), suser_job_count(jep, master_suser_list));
         answer_list_add(alpp, SGE_EVENT, STATUS_NOTOK_DOAGAIN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_NOTOK_DOAGAIN);
      }
   }

   {
      const auto user_lists = mconf_get_user_lists();
      const auto xuser_lists = mconf_get_xuser_lists();
      const auto grp_list = grp_list_array2list(packet->amount, packet->grp_array);

      if (!sge_has_access_(packet->user, packet->group, *grp_list, *user_lists, *xuser_lists,
                           master_userset_list)) {
         ERROR(MSG_JOB_NOPERMS_SS, packet->user, packet->host);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /*
    * here we test (if requested) the parallel environment exists.
    * if not the job is refused
    */
   {
      const char *pe_name = nullptr;

      pe_name = jep.get<JB_pe>();
      if (pe_name) {
         if (!pe_list_find_matching(master_pe_list, pe_name)) {
            ERROR(MSG_JOB_PEUNKNOWN_S, pe_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         /* check pe_range */
         if (object_verify_pe_range(alpp, pe_name, jep.get<JB_pe_range>(), SGE_OBJ_JOB) != STATUS_OK) {
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }

   {
      uint32_t ckpt_attr = jep.get<JB_checkpoint_attr>();
      uint32_t ckpt_inter = jep.get<JB_checkpoint_interval>();
      const char *ckpt_name = jep.get<JB_checkpoint_name>();
      int ckpt_err = 0;

      /* request for non-existing ckpt object will be refused */
      if ((ckpt_name != nullptr)) {
         const auto ckpt_ep = master_ckpt_list.find_first<CK_name>(ckpt_name);
         if (!ckpt_ep) {
            ckpt_err = 1;
         } else if (!ckpt_attr) {
            ckpt_attr = sge_parse_checkpoint_attr(ckpt_ep.get<CK_when>());
            jep.set<JB_checkpoint_attr>(ckpt_attr);
         }
      }

      if (!ckpt_err) {
         if ((ckpt_attr & NO_CHECKPOINT) && (ckpt_attr & ~NO_CHECKPOINT)) {
            ckpt_err = 2;
         } else if (ckpt_name && (ckpt_attr & NO_CHECKPOINT)) {
            ckpt_err = 3;
         } else if ((!ckpt_name && (ckpt_attr & ~NO_CHECKPOINT))) {
            ckpt_err = 4;
         } else if (!ckpt_name && ckpt_inter) {
            ckpt_err = 5;
         }
      }

      if (ckpt_err) {
         switch (ckpt_err) {
            case 1:
               ERROR(MSG_JOB_CKPTUNKNOWN_S, ckpt_name);
               break;
            case 2:
            case 3:
               ERROR("%s", MSG_JOB_CKPTMINUSC);
               break;
            case 4:
            case 5:
               ERROR("%s", MSG_JOB_CKPTDENIED);
               break;
            default:
               ERROR("%s", MSG_JOB_CKPTDENIED);
               break;
         }
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   /* first check user permissions */
   {
      int has_permissions = 0;
      const auto grp_list = grp_list_array2list(packet->amount, packet->grp_array);

      for (const auto cqueue : master_cqueue_list) {
         for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
            if (sge_has_access(packet->user, packet->group, *grp_list, qinstance, master_userset_list)) {
               DPRINTF("job has access to queue " SFQ "\n", qinstance.get<QU_qname>());
               has_permissions = 1;
               break;
            }
         }
         if (has_permissions == 1) {
            break;
         }
      }
      if (has_permissions == 0) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NOTINANYQ_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      }
   }

   /* if enforce_user flag is "auto", add or update the user */
   {
      const std::optional<std::string> enforce_user = mconf_get_enforce_user();

      if (enforce_user && !strcasecmp(enforce_user->c_str(), "auto")) {
         int status = sge_add_auto_user(packet, task, packet->user, alpp, monitor);

         if (status != STATUS_OK) {
            DRETURN(status);
         }
      }

      /* ensure user exists if enforce_user flag is set */
      if (enforce_user && !strcasecmp(enforce_user->c_str(), "true") &&
          !master_user_list.find_first<UU_name>(packet->user)) {
         ERROR(MSG_JOB_USRUNKNOWN_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* set default project */
   if (!jep.get<JB_project>() && master_user_list) {
      if (const auto uep = master_user_list.find_first<UU_name>(packet->user)) {
         jep.set<JB_project>(uep.get<UU_default_project>());
      }
   }

   /* project */
   {
      int local_ret = job_verify_project(jep, alpp, packet->user, packet->group, jep.get<JB_grp_list>());
      if (local_ret != STATUS_OK) {
         DRETURN(local_ret);
      }
   }

   // check the given department or find a department for the user if none was specified
   if (const char *dept_name = jep.get<JB_department>(); dept_name != nullptr) {
      if (!job_is_valid_department(jep, alpp, dept_name, master_userset_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }
   } else {
      if (!job_set_department(jep, alpp, master_userset_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* 
    * If it is a deadline job the user has to be a deadline user
    */
   if (jep.get<JB_deadline>() > 0) {
      if (!user_is_deadline_user(packet, master_userset_list)) {
         ERROR(MSG_JOB_NODEADLINEUSER_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* Verify existence of ar, if ar exists */
   {
      uint32_t ar_id = jep.get<JB_ar>();

      if (ar_id != 0) {
         uint64_t ar_start_time, ar_end_time, job_duration;
         uint64_t now_time, job_execution_time;

         DPRINTF("job -ar " sge_u32"\n", ar_id);

         const auto ar = master_ar_list.find_first<AR_id>(ar_id);
         if (!ar) {
            ERROR(MSG_JOB_NOAREXISTS_U, ar_id);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         } else if ((ar.get<AR_state>() == AR_DELETED) ||
                    (ar.get<AR_state>() == AR_EXITED)) {
            ERROR(MSG_JOB_ARNOLONGERAVAILABE_U, ar_id);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
         /* fill the job and ar values */
         ar_start_time = ar.get<AR_start_time>();
         ar_end_time = ar.get<AR_end_time>();
         now_time = sge_get_gmt64();
         job_execution_time = jep.get<JB_execution_time>();

         /* execution before now is set to at least now */
         if (job_execution_time < now_time) {
            job_execution_time = now_time;
         }

         /* to be sure the execution time is NOT before AR start time */
         if (job_execution_time < ar_start_time) {
            job_execution_time = ar_start_time;
         }

         /* hard_resources h_rt limit */
         if (job_get_wallclock_limit(&job_duration, jep)) {
            DPRINTF("job -ar " sge_u64", ar_start_time " sge_u64", ar_end_time " sge_u64
                    ", job_execution_time " sge_u64", job duration " sge_u64" \n",
                    ar_id, ar_start_time, ar_end_time,
                    job_execution_time, job_duration);

            /* fit the timeframe */
            if (job_duration > (ar_end_time - ar_start_time)) {
               ERROR(MSG_JOB_HRTLIMITTOOLONG_U, ar_id);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_DENIED);
            }
            if ((job_execution_time + job_duration) > ar_end_time) {
               ERROR(MSG_JOB_HRTLIMITOVEREND_U, ar_id);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_DENIED);
            }
         }
      }
   }

   // no unique attribute may hold a key twice; repeated variables and requests are merged above (CS-3053)
   {
      const std::string job_name = std::string("job ") + std::to_string(jep.get<JB_job_number>());
      if (!ocs_object_verify_unique_keys(jep, alpp, job_name.c_str())) {
         DRETURN(STATUS_ESYNTAX);
      }
   }

   /* verify schedulability */
   {
      int ret = verify_suitable_queues(alpp, jep, nullptr, false);
      if (jep.get<JB_verify_suitable_queues>() == JUST_VERIFY ||
          jep.get<JB_verify_suitable_queues>() == POKE_VERIFY || ret != 0) {
         DRETURN(ret);
      }
   }

   /*
    * only operators and managers are allowed to submit
    * jobs with higher priority than 0 (=BASE_PRIORITY)
    */
   if (jep.get<JB_priority>() > BASE_PRIORITY &&
       !manop_is_operator(packet)) {
      ERROR(SFNMAX, MSG_JOB_NONADMINPRIO);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* checks on -hold_jid */
   if (job_verify_predecessors(jep, alpp)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* checks on -hold_jid_ad */
   if (job_verify_predecessors_ad(jep, alpp, packet->gdi_session)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   job_suc_pre(jep);

   job_suc_pre_ad(jep);

   DRETURN(ret);
}
