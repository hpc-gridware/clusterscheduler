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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_c_report.h"

#include <cstring>

#include "uti/sge_bootstrap.h"
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

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "msg_qmaster.h"
#include "sge_c_gdi.h"
#include "sge_host_qmaster.h"
#include "configuration_qmaster.h"
#include "qmaster_to_execd.h"
#include "job_report_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "reschedule.h"

static int
update_license_data(lListElem *hep, lList *lp_lic, u_long64 gdi_session);


/****** sge_c_report() *******************************************************
*  NAME
*     sge_c_report() -- process execd load report
*
*  SYNOPSIS
*     void sge_c_report(char *rhost, char *commproc, int id, lList *report_list)
*
*  FUNCTION
*
*  INPUTS
*     char *rhost
*     char *commproc
*     int id
*     lList *report_list
*
*  RESULT
*     void - nothing
*
*  NOTES
*     MT-NOTE: sge_c_report() is MT safe
*
******************************************************************************/
void
sge_c_report(ocs::gdi::Packet *packet, ocs::gdi::Task *task, char *rhost, char *commproc, int id, lList *report_list, monitoring_t *monitor) {
   lListElem *hep = nullptr;
   u_long32 rep_type;
   lListElem *report;
   int ret = 0;
   u_long32 this_seqno, last_seqno;
   u_long32 rversion;
   sge_pack_buffer pb;
   bool is_pb_used = false;
   bool send_tag_new_conf = false;

   DENTER(TOP_LAYER);

   if (lGetNumberOfElem(report_list) == 0) {
      DPRINTF("received empty report\n");
      if (rhost != nullptr) {
         WARNING(MSG_QMASTER_RECEIVED_EMPTY_LOAD_REPORT_S, rhost);
      } else {
         WARNING(MSG_QMASTER_RECEIVED_EMPTY_LOAD_REPORT_S, "unknown");
      }
      DRETURN_VOID;
   }

#ifdef OBSERVE
   dstring rep_str = DSTRING_INIT;
   for_each_ep(report, report_list) {
      rep_type = lGetUlong(report, REP_type);

      switch (rep_type) {
      case NUM_REP_REPORT_LOAD:
         sge_dstring_sprintf_append(&rep_str, " NUM_REP_REPORT_LOAD");
         break;
      case NUM_REP_FULL_REPORT_LOAD:
         sge_dstring_sprintf_append(&rep_str, " NUM_REP_FULL_REPORT_LOAD");
         break;
      case NUM_REP_REPORT_CONF: 
         sge_dstring_sprintf_append(&rep_str, " NUM_REP_REPORT_CONF");
         break;
      case NUM_REP_REPORT_PROCESSORS:
         sge_dstring_sprintf_append(&rep_str, " NUM_REP_REPORT_PROCESSORS");
         break;
      case NUM_REP_REPORT_JOB:
         sge_dstring_sprintf_append(&rep_str, " NUM_REP_REPORT_JOB");
         break;
      default:
         sge_dstring_sprintf_append(&rep_str, " UNKNOWN");
         break;
      }
   }
   INFO("REPORT %s (%s)", sge_dstring_get_string(&rep_str), rhost);
   sge_dstring_free(&rep_str);
#endif

   /* accept reports only from execd's */
   if (strcmp(prognames[EXECD], commproc)) {
      ERROR(MSG_GOTSTATUSREPORTOFUNKNOWNCOMMPROC_S, commproc);
      DRETURN_VOID;
   }

   /* do not process load reports from old execution daemons */
   rversion = lGetUlong(lFirst(report_list), REP_version);
   if (!ocs::Version::do_versions_match(nullptr, rversion, rhost, commproc, id)) {
      DRETURN_VOID;
   }

   this_seqno = lGetUlong(lFirst(report_list), REP_seqno);

   /* need exec host for all types of reports */
   if (!(hep = host_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST), rhost))) {
      ERROR(MSG_GOTSTATUSREPORTOFUNKNOWNEXECHOST_S, rhost);
      DRETURN_VOID;
   }

   /* prevent old reports being proceeded 
      frequent loggings of outdated reports can be an indication 
      of too high message traffic arriving at qmaster */
   last_seqno = lGetUlong(hep, EH_report_seqno);

   if ((this_seqno < last_seqno && (last_seqno - this_seqno) <= 9000) &&
       !(last_seqno > 9990 && this_seqno < 10)) {
      /* this must be an old report, log and then ignore it */
      INFO(MSG_QMASTER_RECEIVED_OLD_LOAD_REPORT_UUS, this_seqno, last_seqno, rhost);
      DRETURN_VOID;
   }

   lSetUlong(hep, EH_report_seqno, this_seqno);

   /* RU: */
   /* tag all reschedule_unknown list entries we hope to 
      hear about in that job report */
   update_reschedule_unknown_list(hep, packet->gdi_session);

   /*
   ** process the reports one after the other
   ** usually there will be a load report
   ** and a configuration version report
   */
   for_each_rw(report, report_list) {
      rep_type = lGetUlong(report, REP_type);

      switch (rep_type) {
         case NUM_REP_REPORT_LOAD:
         case NUM_REP_FULL_REPORT_LOAD:
            MONITOR_ELOAD(monitor);
            /* Now handle execds load reports */
            if (lGetUlong64(hep, EH_lt_heard_from) == 0 && rep_type != NUM_REP_FULL_REPORT_LOAD) {
               host_notify_about_full_load_report(hep);
            } else {
               if (!is_pb_used) {
                  is_pb_used = true;
                  init_packbuffer(&pb, 1024);
               }
               sge_update_load_values(rhost, lGetListRW(report, REP_list), packet->gdi_session);

               if (mconf_get_simulate_execds()) {
                  const lList *master_exechost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
                  const lListElem *shep;
                  const lListElem *simhostElem = nullptr;

                  for_each_ep(shep, master_exechost_list) {
                     simhostElem = lGetSubStr(shep, CE_name, "load_report_host", EH_consumable_config_list);
                     if (simhostElem != nullptr) {
                        const char *real_host = lGetString(simhostElem, CE_stringval);
                        if (real_host != nullptr && sge_hostcmp(real_host, rhost) == 0) {
                           const char *sim_host = lGetHost(shep, EH_name);
                           lListElem *clp = nullptr;

                           DPRINTF("Copy load values of %s to simulated host %s\n", rhost, sim_host);

                           for_each_rw(clp, lGetList(report, REP_list)) {
                              if (strcmp(lGetHost(clp, LR_host), SGE_GLOBAL_NAME) != 0) {
                                 lSetHost(clp, LR_host, sim_host);
                              }
                           }
                           sge_update_load_values(sim_host, lGetListRW(report, REP_list), packet->gdi_session);
                        }
                     }
                  }
               }

               pack_ack(&pb, ACK_LOAD_REPORT, this_seqno, 0, nullptr);
            }
            break;
         case NUM_REP_REPORT_CONF:
            MONITOR_ECONF(monitor);
            if (sge_compare_configuration(hep, lGetList(report, REP_list)) != 0) {
               DPRINTF("%s: configuration on host %s is not up to date\n", __func__, rhost);
               send_tag_new_conf = true;
            }
            break;

         case NUM_REP_REPORT_PROCESSORS:
            /*
            ** save number of processors
            */
            MONITOR_EPROC(monitor);
            ret = update_license_data(hep, lGetListRW(report, REP_list), packet->gdi_session);
            if (ret) {
               ERROR(MSG_LICENCE_ERRORXUPDATINGLICENSEDATA_I, ret);
            }
            break;

         case NUM_REP_REPORT_JOB:
            MONITOR_EJOB(monitor);
            if (!is_pb_used) {
               is_pb_used = true;
               init_packbuffer(&pb, 1024);
            }
            process_job_report(report, hep, rhost, commproc, &pb, monitor, packet->gdi_session);
            break;

         default:
            DPRINTF("received invalid report type %ld\n", rep_type);
      }
   } /* end for_each */

   /* RU: */
   /* delete reschedule unknown list entries we heard about */
   delete_from_reschedule_unknown_list(hep, packet->gdi_session);

   if (is_pb_used) {
      if (pb_filled(&pb)) {
         lList *alp = nullptr;
         /* send all stuff packed during processing to execd */
         ocs::gdi::ClientServerBase::sge_gdi_send_any_request(0, nullptr, rhost, commproc, id, &pb, ocs::gdi::ClientServerBase::TAG_ACK_REQUEST, 0, &alp);
         MONITOR_MESSAGES_OUT(monitor);
         answer_list_output(&alp);
      }
      clear_packbuffer(&pb);
   }

   if (send_tag_new_conf) {
      if (host_notify_about_new_conf(hep) != 0) {
         ERROR(MSG_CONF_CANTNOTIFYEXECHOSTXOFNEWCONF_S, rhost);
      }
   }

   DRETURN_VOID;
} /* sge_c_report */

/*
** NAME
**   update_license_data
** PARAMETER
**   hep                 - pointer to host element, EH_Type
**   lp_lic              - list of license data, LIC_Type
**
** RETURN
**    0                  - ok
**   -1                  - nullptr pointer received for hep
**   -2                  - nullptr pointer received for lp_lic
** EXTERNAL
**   none
** DESCRIPTION
**   updates the number of processors in the host element
**   spools if it has changed
*/
static int
update_license_data(lListElem *hep, lList *lp_lic, u_long64 gdi_session) {
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
   u_long32 processors = lGetUlong(lFirst(lp_lic), LIC_processors);

   /*
   ** we spool, cf. cod_update_load_values()
   */
   if (processors != lGetUlong(hep, EH_processors)) {
      lList *answer_list = nullptr;

      lSetUlong(hep, EH_processors, processors);

      DPRINTF("%s has " sge_u32 " processors\n", lGetHost(hep, EH_name), processors);
      sge_event_spool(&answer_list, 0, sgeE_EXECHOST_MOD, 0, 0, lGetHost(hep, EH_name), nullptr, nullptr,
                      hep, nullptr, nullptr, true, true, gdi_session);
      answer_list_output(&answer_list);
   }

   DRETURN(0);
}

