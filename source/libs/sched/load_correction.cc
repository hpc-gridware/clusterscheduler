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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of the scheduler's load and capacity correction
 */
#include <cstdio>
#include <cstring>

#include "uti/sge.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"

#include "schedd_monitor.h"
#include "load_correction.h"

/**
 * @brief Adds an artificial load for recently started jobs
 *
 * The load an execution host reports lags behind reality by minutes, so a job
 * that just started is invisible to the scheduler. For every job younger than
 * `decay_time` this function adds a correction to the host's
 * `EH_load_correction_factor`, linear in the age of the job:
 *
 *     correction(t) = 1 - t / decay_time
 *
 * so a job that just started contributes 1 and one whose decay time has
 * expired contributes 0. The value is multiplied by the number of slots the
 * job got on the host, and stored multiplied by 100 so that an integer field
 * can hold it.
 *
 * The factor is used to sort the hosts by load, to decide about the load
 * thresholds of the queues, and to resort the hosts after each scheduled job.
 *
 * @param[in]     running_jobs     the running jobs (`JB_Type`)
 * @param[in]     queue_list       the queue instances, to resolve the granted
 *                                 queue of a task
 * @param[in,out] host_list        the execution hosts, whose correction
 *                                 factors are updated
 * @param[in]     decay_time       age at which a job stops contributing
 * @param[in]     monitor_next_run whether the next run writes the scheduler
 *                                 monitoring log
 *
 * @return 0 on success, 1 if no queue list or host list was passed
 */
int correct_load(lList *running_jobs, lList *queue_list, lList *host_list,
                 uint64_t decay_time, bool monitor_next_run) {
   DENTER(TOP_LAYER);

   if (queue_list == nullptr || host_list == nullptr) {
      DRETURN(1);
   }

   lListElem *global_host = host_list_locate(host_list, SGE_GLOBAL_NAME);
   uint64_t now = sge_get_gmt64();

   for_each_rw_lv(job, running_jobs) {
      uint32_t job_id = ocs::tom::get<JB_job_number>(job);
      double global_lcf = 0.0;

      for (ocs::tom::Element<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
         uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
         uint64_t running_time = now - ocs::tom::get<JAT_start_time>(ja_task);
         double host_lcf = 0.0;

#if 1
         DPRINTF("JOB " sge_u32 "." sge_u32 " start_time = " sge_u64" running_time " sge_u64 " decay_time = " sge_u64"\n",
                 job_id, ja_task_id, ocs::tom::get<JAT_start_time>(ja_task), running_time, decay_time);
#endif
         if (running_time > decay_time) {
            continue;
         }
         ocs::tom::ConstList<ocs::tom::JG> granted_list = ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task);
         for (ocs::tom::ConstElement<ocs::tom::JG> granted_queue : granted_list) {
            const char *qnm = nullptr;
            const char *hnm = nullptr;
            lListElem *qep = nullptr;
            uint32_t slots;
            
            qnm = ocs::tom::get<JG_qname>(granted_queue);
            qep = qinstance_list_locate2(queue_list, qnm);
            if (qep == nullptr) {
               DPRINTF("Unable to find queue \"%s\" from gdil " "list of job " sge_u32 "." sge_u32"\n", qnm, job_id, ja_task_id);
               continue;
            }
           
            hnm=ocs::tom::get<JG_qhostname>(granted_queue); 
            ocs::tom::Element<ocs::tom::EH> hep = ocs::tom::find_first_rw<EH_name>(host_list, hnm);
            if (hep == nullptr) {
               DPRINTF("Unable to find host \"%s\" from gdil " "list of job " sge_u32 "." sge_u32"\n", hnm, job_id, ja_task_id);
               continue;
            } 

            /* To implement load correction we add values between
               1 (just started) and 0 (load_adjustment_decay_time expired)
               for each job slot in the exec host field 
               EH_load_correction_factor. This field is used later on to:
               - sort hosts concerning load
               - decide about load thresholds of queues
               - resort hosts for each scheduled job          */ 
            
            /* use linear function for additional load correction factor 
                                         t
               correction(t) = 1 - ---------------- 
                                    decay_time
            */
            host_lcf = 1 - ((double) running_time / (double) decay_time);
            global_lcf += host_lcf;

            /* multiply it for each slot on this host */
            slots = ocs::tom::get<JG_slots>(granted_queue);
            host_lcf *= slots;
            
            /* add this factor (multiplied with 100 for being able to use 
               uint32_t) */
            ocs::tom::set<EH_load_correction_factor>(hep, 
                      host_lcf * 100 + 
                      ocs::tom::get<EH_load_correction_factor>(hep));

#if 1
            DPRINTF("JOB " sge_u32 "." sge_u32 " [" sge_u32" slots] in queue %s increased lc of host "
                    "%s by " sge_u32" to " sge_u32"\n", job_id, ja_task_id, slots, qnm, hnm,
                    (uint32_t)(100*host_lcf), ocs::tom::get<EH_load_correction_factor>(hep));
#endif
            if (monitor_next_run) {
               char log_string[2048 + 1];
               snprintf(log_string, sizeof(log_string), "JOB " sge_u32"." sge_u32" [" sge_u32"] in queue " SFN
                          " increased absolute lc of host " SFN " by " sge_u32" to "
                          sge_u32"", job_id, ja_task_id, slots, qnm, hnm,
                          (uint32_t)(host_lcf*100), ocs::tom::get<EH_load_correction_factor>(hep));
               schedd_log(log_string, nullptr, true);
            }
         }
      }
      ocs::tom::set<EH_load_correction_factor>(global_host, 
                global_lcf * 100 + 
                ocs::tom::get<EH_load_correction_factor>(global_host));
   }

   DRETURN(0);
}


/**
 * @brief Load scaling and capacity correction for consumables
 *
 * For every consumable attribute that also has a load value the reported load
 * is scaled and the remaining capacity of the host is corrected accordingly.
 * That is what lets a consumable be tracked by its load value rather than
 * only by the bookings of the scheduler.
 *
 * @param[in,out] host_list        the execution hosts
 * @param[in]     centry_list      the complex entries (`CE_Type`)
 * @param[in]     load_adjustments the job load adjustments of the scheduler
 *                                 configuration (`CE_Type`); nullptr fetches
 *                                 them from the data store of the calling
 *                                 thread
 *
 * @return always 0
 */
int correct_capacities(lList *host_list, const lList *centry_list, const lList *load_adjustments) {
   DENTER(TOP_LAYER);
   lListElem *cep;
   ocs::tom::ConstElement<ocs::tom::CE> job_load;
   ocs::tom::ConstElement<ocs::tom::HS> scaling;
   ocs::tom::Element<ocs::tom::CE> total;
   ocs::tom::ConstElement<ocs::tom::RUE> inuse_rms;
   ocs::CEntry::Type type;
   uint32_t relop;
   double dval, inuse_ext, full_capacity, sc_factor;
   double load_correction;
   lList *local_load_adjustments = nullptr;
   const lList *job_load_adj_list = load_adjustments;
   if (job_load_adj_list == nullptr) {
      job_load_adj_list = local_load_adjustments = sconf_get_job_load_adjustments();
   }
 
   for_each_rw_lv(hep, host_list) {
      const char *host_name = ocs::tom::get<EH_name>(hep);

      for (ocs::tom::Element<ocs::tom::HL> ep : ocs::tom::get<EH_load_list>(hep)) {
         const char *attr_name = ocs::tom::get<HL_name>(ep);
 
         /* seach for appropriate complex attribute */
         if (!(cep=centry_list_locate(centry_list, attr_name)))
            continue;

         type = static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(cep));
         if (type != ocs::CEntry::Type::INT &&
             type != ocs::CEntry::Type::TIME &&
             type != ocs::CEntry::Type::MEM &&
             type != ocs::CEntry::Type::BOOL &&
             type != ocs::CEntry::Type::DOUBLE) {
            continue;
         }
        
         if (!parse_ulong_val(&dval, nullptr, type, ocs::tom::get<HL_value>(ep), nullptr, 0))
            continue;

         /* do load scaling */
         if ((scaling=ocs::tom::find_first_in<EH_scaling_list, HS_name>(hep, attr_name))) {
            char sval[20];
            sc_factor = ocs::tom::get<HS_value>(scaling);
            dval *= sc_factor;
            snprintf(sval, sizeof(sval), "%8.3f", dval);
            ocs::tom::set<HL_value>(ep, sval);
         }

         if (ocs::tom::get<CE_consumable>(cep) == CONSUMABLE_NO)
            continue;
         if (!(total=ocs::tom::find_first_in_rw<EH_consumable_config_list, CE_name>(hep, attr_name)))
            continue;
         if (!(inuse_rms=ocs::tom::find_first_in<EH_resource_utilization, RUE_name>(hep, attr_name)))
            continue;

         relop = ocs::tom::get<CE_relop>(cep);
         if (relop != CMPLXEQ_OP &&
             relop != CMPLXLT_OP &&
             relop != CMPLXLE_OP &&
             relop != CMPLXNE_OP)
            continue;

         /* do load correction */
         load_correction = 0;
         if ((job_load=ocs::tom::find_first<CE_name>(job_load_adj_list, attr_name))) {
            double lc_factor;
            const char *s = ocs::tom::get<CE_stringval>(job_load);

            if (parse_ulong_val(&load_correction, nullptr, type, s, nullptr, 0)) {
               lc_factor = ((double)ocs::tom::get<EH_load_correction_factor>(hep))/100.0;
               load_correction *= lc_factor;
               DPRINTF("%s:%s %s %8.3f %8.3f\n", host_name, attr_name, s, load_correction, lc_factor);
               dval -= load_correction;
            }
         }

         /* use scaled load value to deduce the amount */
         full_capacity = ocs::tom::get<CE_doubleval>(total);
         inuse_ext = full_capacity - ocs::tom::get<RUE_utilized_now>(inuse_rms) - dval;

         if (inuse_ext > 0.0) {
            ocs::tom::set<CE_doubleval>(total, full_capacity - inuse_ext);

            DPRINTF("%s:%s %8.3f --> %8.3f (ext: %8.3f = all %8.3f - ubC %8.3f - load %8.3f) lc = %8.3f\n",
                    host_name, attr_name, full_capacity, ocs::tom::get<CE_doubleval>(total),
                    inuse_ext, full_capacity, ocs::tom::get<RUE_utilized_now>(inuse_rms), dval, load_correction);
         } else {
            DPRINTF("ext: %8.3f <= 0\n", inuse_ext);
         }
      }
   }
   lFreeList(&local_load_adjustments);

   DRETURN(0);
}
