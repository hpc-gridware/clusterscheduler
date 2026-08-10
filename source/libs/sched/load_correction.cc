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
                  uint64_t decay_time, bool monitor_next_run)
{
   DENTER(TOP_LAYER);

   if (queue_list == nullptr || host_list == nullptr) {
      DRETURN(1);
   }

   lListElem *global_host = host_list_locate(host_list, SGE_GLOBAL_NAME);
   uint64_t now = sge_get_gmt64();

   for_each_rw_lv(job, running_jobs) {
      uint32_t job_id = lGetUlong(job, JB_job_number);
      double global_lcf = 0.0;

      for_each_rw_lv(ja_task, lGetList(job, JB_ja_tasks)) {
         uint32_t ja_task_id = lGetUlong(ja_task, JAT_task_number);
         uint64_t running_time = now - lGetUlong64(ja_task, JAT_start_time);
         const lList *granted_list = nullptr;
         double host_lcf = 0.0;

#if 1
         DPRINTF("JOB " sge_u32 "." sge_u32 " start_time = " sge_u64" running_time " sge_u64 " decay_time = " sge_u64"\n",
                 job_id, ja_task_id, lGetUlong64(ja_task, JAT_start_time), running_time, decay_time);
#endif
         if (running_time > decay_time) {
            continue;
         }
         granted_list = lGetList(ja_task, JAT_granted_destin_identifier_list);
         for_each_ep_lv(granted_queue, granted_list) {
            const char *qnm = nullptr;
            const char *hnm = nullptr;
            lListElem *qep = nullptr;
            lListElem *hep = nullptr;
            uint32_t slots;
            
            qnm = lGetString(granted_queue, JG_qname);
            qep = qinstance_list_locate2(queue_list, qnm);
            if (qep == nullptr) {
               DPRINTF("Unable to find queue \"%s\" from gdil " "list of job " sge_u32 "." sge_u32"\n", qnm, job_id, ja_task_id);
               continue;
            }
           
            hnm=lGetHost(granted_queue, JG_qhostname); 
            hep = lGetElemHostRW(host_list, EH_name, hnm);
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
            slots = lGetUlong(granted_queue, JG_slots);
            host_lcf *= slots;
            
            /* add this factor (multiplied with 100 for being able to use 
               uint32_t) */
            lSetUlong(hep, EH_load_correction_factor, 
                      host_lcf * 100 + 
                      lGetUlong(hep, EH_load_correction_factor));

#if 1
            DPRINTF("JOB " sge_u32 "." sge_u32 " [" sge_u32" slots] in queue %s increased lc of host "
                    "%s by " sge_u32" to " sge_u32"\n", job_id, ja_task_id, slots, qnm, hnm,
                    (uint32_t)(100*host_lcf), lGetUlong(hep, EH_load_correction_factor));
#endif
            if (monitor_next_run) {
               char log_string[2048 + 1];
               snprintf(log_string, sizeof(log_string), "JOB " sge_u32"." sge_u32" [" sge_u32"] in queue " SFN
                          " increased absolute lc of host " SFN " by " sge_u32" to "
                          sge_u32"", job_id, ja_task_id, slots, qnm, hnm,
                          (uint32_t)(host_lcf*100), lGetUlong(hep, EH_load_correction_factor));
               schedd_log(log_string, nullptr, true);
            }
         }
      }
      lSetUlong(global_host, EH_load_correction_factor, 
                global_lcf * 100 + 
                lGetUlong(global_host, EH_load_correction_factor));
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
 * @param[in,out] host_list   the execution hosts
 * @param[in]     centry_list the complex entries (`CE_Type`)
 *
 * @return always 0
 */
int 
correct_capacities(lList *host_list, const lList *centry_list) 
{
   DENTER(TOP_LAYER);
   lListElem *cep;
   const lListElem *job_load;
   const lListElem *scaling;
   lListElem *total;
   const lListElem *inuse_rms;
   ocs::CEntry::Type type;
   uint32_t relop;
   double dval, inuse_ext, full_capacity, sc_factor;
   double load_correction;
   lList* job_load_adj_list = nullptr;

   job_load_adj_list = sconf_get_job_load_adjustments();
 
   for_each_rw_lv(hep, host_list) {
      const char *host_name = lGetHost(hep, EH_name);

      for_each_rw_lv(ep, lGetList(hep, EH_load_list)) {
         const char *attr_name = lGetString(ep, HL_name);
 
         /* seach for appropriate complex attribute */
         if (!(cep=centry_list_locate(centry_list, attr_name)))
            continue;

         type = static_cast<ocs::CEntry::Type>(lGetUlong(cep, CE_valtype));
         if (type != ocs::CEntry::Type::INT &&
             type != ocs::CEntry::Type::TIME &&
             type != ocs::CEntry::Type::MEM &&
             type != ocs::CEntry::Type::BOOL &&
             type != ocs::CEntry::Type::DOUBLE) {
            continue;
         }
        
         if (!parse_ulong_val(&dval, nullptr, type, lGetString(ep, HL_value), nullptr, 0))
            continue;

         /* do load scaling */
         if ((scaling=lGetSubStr(hep, HS_name, attr_name, EH_scaling_list))) {
            char sval[20];
            sc_factor = lGetDouble(scaling, HS_value);
            dval *= sc_factor;
            snprintf(sval, sizeof(sval), "%8.3f", dval);
            lSetString(ep, HL_value, sval);
         }

         if (lGetUlong(cep, CE_consumable) == CONSUMABLE_NO)
            continue;
         if (!(total=lGetSubStrRW(hep, CE_name, attr_name, EH_consumable_config_list)))
            continue;
         if (!(inuse_rms=lGetSubStr(hep, RUE_name, attr_name, EH_resource_utilization)))
            continue;

         relop = lGetUlong(cep, CE_relop);
         if (relop != CMPLXEQ_OP &&
             relop != CMPLXLT_OP &&
             relop != CMPLXLE_OP &&
             relop != CMPLXNE_OP)
            continue;

         /* do load correction */
         load_correction = 0;
         if ((job_load=lGetElemStr(job_load_adj_list, CE_name, attr_name))) {
            double lc_factor;
            const char *s = lGetString(job_load, CE_stringval);

            if (parse_ulong_val(&load_correction, nullptr, type, s, nullptr, 0)) {
               lc_factor = ((double)lGetUlong(hep, EH_load_correction_factor))/100.0;
               load_correction *= lc_factor;
               DPRINTF("%s:%s %s %8.3f %8.3f\n", host_name, attr_name, s, load_correction, lc_factor);
               dval -= load_correction;
            }
         }

         /* use scaled load value to deduce the amount */
         full_capacity = lGetDouble(total, CE_doubleval);
         inuse_ext = full_capacity - lGetDouble(inuse_rms, RUE_utilized_now) - dval;

         if (inuse_ext > 0.0) {
            lSetDouble(total, CE_doubleval, full_capacity - inuse_ext);

            DPRINTF("%s:%s %8.3f --> %8.3f (ext: %8.3f = all %8.3f - ubC %8.3f - load %8.3f) lc = %8.3f\n",
                    host_name, attr_name, full_capacity, lGetDouble(total, CE_doubleval),
                    inuse_ext, full_capacity, lGetDouble(inuse_rms, RUE_utilized_now), dval, load_correction);
         } else {
            DPRINTF("ext: %8.3f <= 0\n", inuse_ext);
         }
      }
   }
   lFreeList(&job_load_adj_list);

   DRETURN(0);
}
