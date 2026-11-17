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
 * @brief The scheduler configuration object
 */
#include <cstdio>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_SC.h"

#include "sched/msg_schedd.h"

#include "spool/sge_spooling.h"

#include "sched_conf_qmaster.h"
#include "configuration_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"

/** @brief Read the scheduler configuration from the spool at startup
 *
 * @param aSpoolContext the spooling context to read through
 * @param anAnswer receives messages for the caller
 * @param gdi_session the session the change belongs to
 * @return STATUS_OK on success
 */
int
sge_read_sched_configuration(const lListElem *aSpoolContext, lList **anAnswer, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   // the spooling layer has no typed form yet; it reads SC_Type elements for SGE_TYPE_SCHEDD_CONF
   lList *spooled_conf = nullptr;
   spool_read_list(anAnswer, aSpoolContext, &spooled_conf, SGE_TYPE_SCHEDD_CONF);
   auto sched_conf = ocs::tom::ListPtr<ocs::tom::SC>::from_typed(spooled_conf);

   if (sched_conf->empty()) {
      auto default_conf = sconf_create_default();

      if (!sched_conf) {
         sched_conf = ocs::tom::ListPtr<ocs::tom::SC>::create("schedd_config_list");
      }

      const auto ep = sched_conf->push_back(std::move(default_conf));
      spool_write_object(anAnswer, spool_get_default_context(), ep, "schedd_conf", SGE_TYPE_SCHEDD_CONF);
   }

   if (!sconf_set_config(sched_conf, anAnswer)) {
      DRETURN(-1);
   }

   DRETURN(0);
}


/** @brief Replace the scheduler configuration
 *
 * Modify scheduler configuration. We have only one entry in
 * Master_Sched_Config_List. So we replace it with the new one.
 *
 * The element comes from a client and is checked to be a complete SC_Type
 * object, sub-lists included, before anything reads it. Every other source of
 * the configuration (spool, default, event mirror, full GDI get) yields a
 * complete element, so this is the only check the configuration module needs.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param confp the new configuration as sent by the client
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success, STATUS_ESYNTAX if the element is reduced or not of
 *         SC_Type or a unique attribute holds a key twice, STATUS_EUNKNOWN or -1
 *         on other errors
 */
int
sge_mod_sched_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::SC> confp,
                            lList **alpp, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   if (!confp || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
   if (!object_verify_cull(confp, SC_Type)) {
      answer_list_add(alpp, MSG_INCOMPLETE_SCHEDD_CONFIG, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_ESYNTAX);
   }
   if (!ocs_object_verify_unique_keys(confp, alpp, "scheduler configuration")) {
      DRETURN(STATUS_ESYNTAX);
   }
   auto temp_conf_list = ocs::tom::ListPtr<ocs::tom::SC>::create("sched config");

   ocs::tom::set<SC_weight_tickets_override>(confp, sconf_get_weight_tickets_override());

   const auto new_conf = temp_conf_list->push_back(confp.clone());

   /* just check and log; on success the list belongs to the master list, on failure it is freed here */
   if (!sconf_set_config(temp_conf_list, alpp)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   if (!sge_event_spool(alpp, 0, sgeE_SCHED_CONF, 0, 0, "schedd_conf", nullptr, nullptr,
                        new_conf, nullptr, nullptr, true, true, packet->gdi_session)) {
      answer_list_add(alpp, MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   INFO(MSG_SGETEXT_MODIFIEDINLIST_SSSS, ruser, rhost, "scheduler", "scheduler configuration");
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN(STATUS_OK);
}

/**
 * @brief sge_mod_sched_configuration() for an untyped caller
 *
 * The element is wrapped without a type check: the typed function rejects an element of
 * another type with object_verify_cull() and STATUS_ESYNTAX, as the raw function did.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param confp the new configuration as sent by the client
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return as the typed sge_mod_sched_configuration()
 */
int
sge_mod_sched_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *confp, lList **alpp,
                            char *ruser, char *rhost) {
   return sge_mod_sched_configuration(packet, task, ocs::tom::Element<ocs::tom::SC>::from_typed(confp), alpp,
                                      ruser, rhost);
}
