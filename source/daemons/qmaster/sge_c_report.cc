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
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Receiving load and job reports from the execution hosts
 */

#include "sge_c_report.h"

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_hostname.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Version.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_LIC.h"
#include "sgeobj/tom/ocs_tom_LR.h"
#include "sgeobj/tom/ocs_tom_REP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "msg_qmaster.h"
#include "sge_c_gdi.h"
#include "sge_host_qmaster.h"
#include "configuration_qmaster.h"
#include "qmaster_to_execd.h"
#include "job_report_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "reschedule.h"
#include "tom/ocs_PackCore.h"

static int
update_license_data(ocs::tom::Element<ocs::tom::EH> hep, ocs::tom::ConstList<ocs::tom::LIC> lp_lic,
                    uint64_t gdi_session);


/**
 * @brief Process execd load report
 *
 * @param packet the GDI packet the report arrived in
 * @param task the GDI task within that packet
 * @param rhost the host the execution daemon sent the report from
 * @param commproc the commlib component name of the sender (`execd`)
 * @param id the commlib component id of the sender
 * @param report_list the reports (`REP`) to process
 * @param monitor for monitoring qmaster threads
 *
 * The signature stays raw while the callers outside this module pass the raw data list of a packet.
 *
 * @note MT-NOTE: sge_c_report() is MT safe
 */
void
sge_c_report(ocs::gdi::Packet *packet, [[maybe_unused]] ocs::gdi::Task *task, char *rhost, char *commproc,
             const int id, lList *report_list, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // the reports arrive from an execd and were unpacked with the REP descriptor; from_typed keeps the old
   // tolerance and checks nothing more on this path
   const auto reports = ocs::tom::List<ocs::tom::REP>::from_typed(report_list);
   ocs::tom::Element<ocs::tom::EH> hep;
   uint32_t rep_type;
   int ret = 0;
   uint32_t this_seqno, last_seqno;
   uint32_t rversion;
   sge_pack_buffer pb;
   bool is_pb_used = false;
   bool send_tag_new_conf = false;

   if (reports.empty()) {
      DPRINTF("received empty report\n");
      if (rhost != nullptr) {
         WARNING(MSG_QMASTER_RECEIVED_EMPTY_LOAD_REPORT_S, rhost);
      } else {
         WARNING(MSG_QMASTER_RECEIVED_EMPTY_LOAD_REPORT_S, "unknown");
      }
      DRETURN_VOID;
   }

   /* accept reports only from execd's */
   if (strcmp(to_cstr(EXECD), commproc)) {
      ERROR(MSG_GOTSTATUSREPORTOFUNKNOWNCOMMPROC_S, commproc);
      DRETURN_VOID;
   }

   /* do not process load reports from old execution daemons */
   rversion = reports.front().get<REP_version>();
   if (!ocs::Version::do_versions_match(nullptr, rversion, rhost, commproc, id)) {
      DRETURN_VOID;
   }

   this_seqno = reports.front().get<REP_seqno>();

   /* need exec host for all types of reports */
   if (!(hep = ocs::tom::Element<ocs::tom::EH>(host_list_locate(ocs::DataStore::master_list<SGE_TYPE_EXECHOST>(),
                                                                rhost)))) {
      ERROR(MSG_GOTSTATUSREPORTOFUNKNOWNEXECHOST_S, rhost);
      DRETURN_VOID;
   }

   /* prevent old reports being proceeded 
      frequent loggings of outdated reports can be an indication 
      of too high message traffic arriving at qmaster */
   last_seqno = hep.get<EH_report_seqno>();

   if ((this_seqno < last_seqno && (last_seqno - this_seqno) <= 9000) &&
       !(last_seqno > 9990 && this_seqno < 10)) {
      /* this must be an old report, log and then ignore it */
      INFO(MSG_QMASTER_RECEIVED_OLD_LOAD_REPORT_UUS, this_seqno, last_seqno, rhost);
      DRETURN_VOID;
   }

   hep.set<EH_report_seqno>(this_seqno);

   /* RU: */
   /* tag all reschedule_unknown list entries we hope to 
      hear about in that job report */
   update_reschedule_unknown_list(hep, packet->gdi_session);

   // Does this message contain the reports of all jobs on the host? Only then the reschedule
   // unknown list entries of jobs without a report may be removed. An sge_execd of an older
   // version does not tell, its reports are handled as before.
   bool old_execd = !reports.front().has<REP_complete>();
   bool all_jobs_reported = old_execd;

   /*
   ** process the reports one after the other
   ** usually there will be a load report
   ** and a configuration version report
   */
   for (const auto report : reports) {
      rep_type = report.get<REP_type>();

      switch (rep_type) {
         case NUM_REP_REPORT_LOAD:
         case NUM_REP_FULL_REPORT_LOAD:
            MONITOR_ELOAD(monitor);
            /* Now handle execds load reports */
            if (hep.get<EH_lt_heard_from>() == 0 && rep_type != NUM_REP_FULL_REPORT_LOAD) {
               host_notify_about_full_load_report(hep);
            } else {
               if (!is_pb_used) {
                  is_pb_used = true;
                  ocs::tom::try_init_buffer(&pb, 1024);
               }
               // REP_list names no sub-type in the model; a load report holds LR elements, unpacked with their
               // descriptor, from_typed keeps the old tolerance
               sge_update_load_values(rhost, nullptr, ocs::tom::List<ocs::tom::LR>::from_typed(
                                         report.get<REP_list>()), packet->gdi_session);

               if (mconf_get_simulate_execds()) {
                  const auto master_exechost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();

                  for (const auto shep : master_exechost_list) {
                     const auto simhostElem =
                        shep.get<EH_consumable_config_list>().find_first<CE_name>("load_report_host");
                     if (simhostElem) {
                        const char *real_host = simhostElem.get<CE_stringval>();
                        if (real_host != nullptr && sge_hostcmp(real_host, rhost) == 0) {
                           const char *sim_host = shep.get<EH_name>();

                           DPRINTF("Copy load values of %s to simulated host %s\n", rhost, sim_host);

                           // REP_list names no sub-type in the model; a load report holds LR elements, unpacked
                           // with their descriptor, from_typed keeps the old tolerance
                           const auto load_list =
                              ocs::tom::List<ocs::tom::LR>::from_typed(report.get<REP_list>());
                           for (const auto clp : load_list) {
                              if (strcmp(clp.get<LR_host>(), SGE_GLOBAL_NAME) != 0) {
                                 clp.set<LR_host>(ocs::tom::Host(sim_host));
                              }
                           }
                           sge_update_load_values(sim_host, real_host, load_list, packet->gdi_session);
                        }
                     }
                  }
               }

               pack_ack(&pb, ACK_LOAD_REPORT, this_seqno, 0, nullptr);
            }
            break;
         case NUM_REP_REPORT_CONF:
            MONITOR_ECONF(monitor);
            // REP_list names no sub-type in the model; a configuration report holds CONF elements
            if (sge_compare_configuration(hep, ocs::tom::ConstList<ocs::tom::CONF>::from_typed(
                                                  report.get<REP_list>())) != 0) {
               DPRINTF("%s: configuration on host %s is not up to date\n", __func__, rhost);
               send_tag_new_conf = true;
            }
            break;

         case NUM_REP_REPORT_PROCESSORS:
            /*
            ** save number of processors
            */
            MONITOR_EPROC(monitor);
            // REP_list names no sub-type in the model; a processor report holds LIC elements
            ret = update_license_data(hep,
                                      ocs::tom::ConstList<ocs::tom::LIC>::from_typed(report.get<REP_list>()),
                                      packet->gdi_session);
            if (ret) {
               ERROR(MSG_LICENCE_ERRORXUPDATINGLICENSEDATA_I, ret);
            }
            break;

         case NUM_REP_REPORT_JOB:
            MONITOR_EJOB(monitor);
            if (!is_pb_used) {
               is_pb_used = true;
               ocs::tom::try_init_buffer(&pb, 1024);
            }
            process_job_report(report, hep, rhost, commproc, &pb, monitor, packet->gdi_session);
            if (!old_execd && report.get<REP_complete>()) {
               all_jobs_reported = true;
            }
            break;

         default:
            DPRINTF("received invalid report type %ld\n", rep_type);
      }
   } /* end for_each */

   /* RU: */
   /* delete reschedule unknown list entries we heard about */
   delete_from_reschedule_unknown_list(hep, all_jobs_reported, packet->gdi_session);

   if (is_pb_used) {
      if (ocs::tom::buffer_filled(&pb)) {
         ocs::tom::ListPtr<ocs::tom::AN> alp;
         /* send all stuff packed during processing to execd */
         ocs::gdi::ClientServerBase::sge_gdi_send_any_request(0, nullptr, rhost, commproc, id, &pb, ocs::gdi::ClientServerBase::TAG_ACK_REQUEST, 0, &alp);
         MONITOR_MESSAGES_OUT(monitor);
         answer_list_output(&alp);
      }
      ocs::tom::clear_buffer(&pb);
   }

   if (send_tag_new_conf) {
      if (host_notify_about_new_conf(hep) != 0) {
         ERROR(MSG_CONF_CANTNOTIFYEXECHOSTXOFNEWCONF_S, rhost);
      }
   }

   DRETURN_VOID;
} /* sge_c_report */

/**
 * @brief Update the number of processors in the host element, spool it if it has changed
 * @param hep the host
 * @param lp_lic the license data; only the first element is evaluated
 * @param gdi_session the session the change belongs to
 * @return 0 - ok, -1 - empty handle for hep, -2 - empty handle for lp_lic
 */
static int
update_license_data(const ocs::tom::Element<ocs::tom::EH> hep, const ocs::tom::ConstList<ocs::tom::LIC> lp_lic,
                    const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   if (!hep) {
      DRETURN(-1);
   }

   /*
   ** if it was clear what to do in this case we could return 0
   */
   if (!lp_lic) {
      DRETURN(-2);
   }

   /*
   ** at the moment only the first element is evaluated
   */
   uint32_t processors = lp_lic.front().get<LIC_processors>();

   /*
   ** we spool, cf. cod_update_load_values()
   */
   if (processors != hep.get<EH_processors>()) {
      ocs::tom::ListPtr<ocs::tom::AN> answer_list;

      // set the processors attribute
      hep.set<EH_processors>(processors);

      // adapt slots value but do this only if it was not set by a manager already
      host_ensure_slots_are_defined(hep, processors);

      const auto slots_complex = hep.find_first_in_rw<EH_consumable_config_list, CE_name>(SGE_ATTR_SLOTS);
      if (!slots_complex) {
         const auto new_slots_complex =
            hep.emplace_back_in<EH_consumable_config_list, CE_name>(SGE_ATTR_SLOTS);
         new_slots_complex.set<CE_doubleval>(processors);
         new_slots_complex.set<CE_stringval>(std::to_string(processors).c_str());
      } else {
         double val_uninitialized = std::numeric_limits<int>::max();
         double val = slots_complex.get<CE_doubleval>();

         if (val == val_uninitialized) {
            slots_complex.set<CE_doubleval>(processors);
            slots_complex.set<CE_stringval>(std::to_string(processors).c_str());
         }
      }

      DPRINTF("%s has " sge_u32 " processors\n", hep.get<EH_name>().c_str(), processors);
      sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD, 0, 0, hep.get<EH_name>(), nullptr, nullptr,
                      hep, nullptr, nullptr, true, true, gdi_session);
      answer_list_output(&answer_list);
   }

   DRETURN(0);
}

