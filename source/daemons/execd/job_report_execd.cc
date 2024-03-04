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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstdlib>
#include <cfloat>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"

#include "cull/cull.h"

#include "gdi/sge_gdi.h"

#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/sge_qinstance.h"

#include "sge_report_execd.h"
#include "job_report_execd.h"
#include "reaper_execd.h"
#include "execd_signal_queue.h"
#include "msg_execd.h"
#include "load_avg.h"

lList *jr_list = nullptr;
static bool flush_jr = false;
static int check_queue_limits = 0;

void sge_set_flush_jr_flag(bool value) {
   flush_jr = value;
}

bool sge_get_flush_jr_flag(void) {
   return flush_jr;
}

void 
flush_job_report(lListElem *jr)
{
   if (jr != nullptr) {
      lSetBool(jr, JR_flush, true);
      sge_set_flush_jr_flag(true);
   }
}

#if 0
void trace_jr()
{
   const lListElem *jr;

   DENTER(TOP_LAYER);

   DPRINTF(("--- JOB REPORT LIST ----------------\n"));
   for_each_ep(jr, jr_list) {
      const char *s;

      if ((s=lGetString(jr, JR_pe_task_id_str))) {
         DPRINTF(("Jobtask " sge_u32"." sge_u32" task %s\n", lGetUlong(jr, JR_job_number), lGetUlong(jr, JR_ja_task_number), s));
      } else {
         DPRINTF(("Jobtask " sge_u32"." sge_u32"\n", lGetUlong(jr, JR_job_number), lGetUlong(jr, JR_ja_task_number)));
      }   
   }
   DRETURN_VOID;
}
#endif

lListElem *add_job_report(u_long32 jobid, u_long32 jataskid, const char *petaskid, const lListElem *jep)
{
   lListElem *jr, *jatep = nullptr;
 
   DENTER(TOP_LAYER);

   if (jr_list == nullptr)
      jr_list = lCreateList("job report list", JR_Type);
  
   if (jr_list == nullptr || (jr=lCreateElem(JR_Type)) == nullptr) {
      ERROR(SFNMAX, MSG_JOB_TYPEMALLOC);
      DRETURN(nullptr);
   }

   lSetUlong(jr, JR_job_number, jobid);
   lSetUlong(jr, JR_ja_task_number, jataskid);
   if (petaskid != nullptr) {
      lSetString(jr, JR_pe_task_id_str, petaskid);
   }

   lAppendElem(jr_list, jr);
   DPRINTF(("adding job report for " sge_U32CFormat "." sge_U32CFormat "\n", sge_u32c(jobid), sge_u32c(jataskid)));

   if (jep != nullptr) {
      jatep = job_search_task(jep, nullptr, jataskid);
      if (jatep != nullptr) {
         lListElem *petep = nullptr;
         if (petaskid != nullptr) {
            petep = ja_task_search_pe_task(jatep, petaskid);
         }   
         job_report_init_from_job(jr, jep, jatep, petep);
      }
   }
 
   DRETURN(jr);
}

lListElem *
get_job_report(u_long32 job_id, u_long32 ja_task_id, const char *pe_task_id) 
{
   lListElem *jr;
   const void *iterator = nullptr;

   DENTER(TOP_LAYER);

   jr = lGetElemUlongFirstRW(jr_list, JR_job_number, job_id, &iterator);
   while (jr != nullptr) {
      if (lGetUlong(jr, JR_ja_task_number) == ja_task_id) {
         if (pe_task_id == nullptr) {
            break;
         } else {
            if (sge_strnullcmp(pe_task_id, lGetString(jr, JR_pe_task_id_str)) 
                == 0) {
                break;
            }
         }
      }
      jr = lGetElemUlongNextRW(jr_list, JR_job_number, job_id, &iterator);
   }

   DRETURN(jr);
}

void del_job_report(lListElem *jr)
{
   lRemoveElem(jr_list, &jr);
}

void cleanup_job_report(u_long32 jobid, u_long32 jataskid)
{
   lListElem *jr, *jr_next;
   const void *iterator = nullptr;

   DENTER(TOP_LAYER);

   /* get rid of job reports for all slave tasks */
   jr_next = lGetElemUlongFirstRW(jr_list, JR_job_number, jobid, &iterator);
   while ((jr = jr_next)) {
      jr_next = lGetElemUlongNextRW(jr_list, JR_job_number, jobid, &iterator);
      if (lGetUlong(jr, JR_ja_task_number) == jataskid) {
         const char *s = lGetString(jr, JR_pe_task_id_str);

         DPRINTF(("!!!! removing jobreport for " sge_u32"." sge_u32" task %s !!!!\n", jobid, jataskid, s?s:"master"));
         lRemoveElem(jr_list, &jr);
      }
   }

   DRETURN_VOID;
}

/* ------------------------------------------------------------
   NAME

      add_usage()
   
   DESCR

      Adds ulong attribute 'name' to the usage list of a 
      job report 'jr'. If no 'uval_as_str' or it is not
      convertable into a ulong 'uval_as_ulong' is used 
      as value for UA_value.

   RETURN      

      0 on success
      -1 on error
   ------------------------------------------------------------ */
/* JG: TODO (397): move to libs/gdi/sge_usage.* */   
int add_usage(lListElem *jr, const char *name, const char *val_as_str, double val) 
{
   lListElem *usage;

   DENTER(TOP_LAYER);

   if (!jr || !name) {
      DRETURN(-1);
   }

   /* check if we already have an usage value with this name */
   usage = lGetSubStrRW(jr, UA_name, name, JR_usage);
   if (!usage) {
      if (!(usage = lAddSubStr(jr, UA_name, name, JR_usage, UA_Type))) {
         DRETURN(-1);
      }
   }

   if (val_as_str) {
      char *p;
      double parsed;

      parsed = strtod(val_as_str, &p);
      if (p==val_as_str) {
         ERROR(MSG_PARSE_USAGEATTR_SSU, val_as_str, name, sge_u32c(lGetUlong(jr, JR_job_number))); /* use default value */
         lSetDouble(usage, UA_value, val); 
         DRETURN(-1);
      }
      val = parsed;
   }
      
   lSetDouble(usage, UA_value, val);

   DRETURN(0);
}


/* ------------------------------------------------------------

NAME 
   
   execd_c_ack()

DESCRIPTION
   
   These requests are triggered by our job report list
   that is sent periodically. They are responses of
   Qmaster in different cases. But they get sent as one
   message, to save communication. 

RETURN

   Typical dispatcher service function return values

   ------------------------------------------------------------ */
int do_ack(struct_msg_t *aMsg)
{
   u_long32 jobid, jataskid;
   lListElem *jr;
   lListElem *ack;
   const char *pe_task_id_str;

   DENTER(TOP_LAYER);
 
   DPRINTF(("------- GOT ACK'S ---------\n"));
 
   /* we get a bunch of ack's */
   while (pb_unused(&(aMsg->buf)) > 0) {

      if (cull_unpack_elem(&(aMsg->buf), &ack, nullptr)) {
         ERROR(SFNMAX, MSG_COM_UNPACKJOB);
         DRETURN(0);
      }

      switch (lGetUlong(ack, ACK_type)) {
 
         case ACK_JOB_EXIT:
/*
**          This is the answer of qmaster if we report a job as exiting
**          - job gets removed from job report list and from job list
**          - job gets cleaned from file system                       
**          - retry is triggered by next job report sent to qmaster 
**            containing this job as "exiting"                  
*/
            jobid = lGetUlong(ack, ACK_id);
            jataskid = lGetUlong(ack, ACK_id2);
            pe_task_id_str = lGetString(ack, ACK_str);

            DPRINTF(("remove exiting job " sge_u32"/" sge_u32"/%s\n",
                    jobid, jataskid, pe_task_id_str?pe_task_id_str:""));

            if ((jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
               remove_acked_job_exit(jobid, jataskid, pe_task_id_str, jr);
            } else {
               DPRINTF(("acknowledged job " sge_u32"." sge_u32" not found\n", jobid, jataskid));
            }

            break;
 
         case ACK_JOB_REPORT_RESEND:
/*
**          sent by qmaster instead of ACK_JOB_EXIT to make execd
**          resend the job report
*/
            jobid = lGetUlong(ack, ACK_id);
            jataskid = lGetUlong(ack, ACK_id2);
            pe_task_id_str = lGetString(ack, ACK_str);

            DPRINTF(("resending report for exiting job " sge_u32"/" sge_u32"/%s\n",
                    jobid, jataskid, pe_task_id_str?pe_task_id_str:""));

            if ((jr = get_job_report(jobid, jataskid, pe_task_id_str))) {
               flush_job_report(jr);
            } else {
               DPRINTF(("job requested to resend " sge_u32"." sge_u32" not found\n", jobid, jataskid));
            }

            break;
 
         case ACK_SIGNAL_JOB:
/*
**          This is the answer of qmaster
**          if we report a job as running
**          while qmaster does not know  
**          this job                     
**          - no "unknown job" is added  
**            to the job report          
**          - retry is triggered by next 
**            job report sent to qmaster 
**            containing this job as     
**            "running"                  
*/
            {
               u_long32 signo  = SGE_SIGKILL;

               jobid = lGetUlong(ack, ACK_id);
               jataskid = lGetUlong(ack, ACK_id2);

               if (signal_job(jobid, jataskid, signo)) {
                  lListElem *jr;
                  jr = get_job_report(jobid, jataskid, nullptr);
                  remove_acked_job_exit(jobid, jataskid, nullptr, jr);
                  job_unknown(jobid, jataskid, nullptr);
               }
            }
            break;

         case ACK_LOAD_REPORT:
            execd_merge_load_report(lGetUlong(ack, ACK_id));
            break;
 
/*
 * This is the answer of qmaster
 * when we report a slave job,
 * and the master task of this slave job has finished.
 * qmaster expects us to send a final slave report
 * (having JR_usage with at least a pseudo exit_status)
 * once all pe_tasks have finished.
 */
         case ACK_SIGNAL_SLAVE:
            jobid = lGetUlong(ack, ACK_id);
            jataskid = lGetUlong(ack, ACK_id2);

            execd_slave_job_exit(jobid, jataskid);
            break;

         default:
            ERROR(SFNMAX, MSG_COM_ACK_UNKNOWN1);
            break;
      }

      lFreeElem(&ack);
      /* 
       * delete job's spooling directory may take some time
       * (NFS directory case). We have to trigger communication
       * to be sure not to get communication timeouts when we have
       * to delete lot's of jobs at once. ( The trigger is done
       * NOT synchron which means that the commlib will return
       * when there is nothing to do
       */
      cl_commlib_trigger(cl_com_get_handle("execd", 1) ,0);
   }

   DRETURN(0);
}

void modify_queue_limits_flag_for_job(const char *qualified_hostname, lListElem *jep, bool increase)
{
   const lListElem *jatep;
   const lListElem *gdil_ep;

   for_each_ep(jatep, lGetList(jep, JB_ja_tasks)) {
      for_each_ep(gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
         double lim;
         lListElem *q;

         if (sge_hostcmp(qualified_hostname, lGetHost(gdil_ep, JG_qhostname)) 
             || !(q = lGetObject(gdil_ep, JG_queue))) {
            continue;
         }

         parse_ulong_val(&lim, nullptr, TYPE_TIM, lGetString(q, QU_s_cpu), nullptr, 0);
         if (lim != DBL_MAX) {
            if (increase) {
               check_queue_limits++;
            } else {
               check_queue_limits--;
            }
            break;
         }
         parse_ulong_val(&lim, nullptr, TYPE_TIM, lGetString(q, QU_h_cpu), nullptr, 0);
         if (lim != DBL_MAX) {
            if (increase) {
               check_queue_limits++;
            } else {
               check_queue_limits--;
            }
            break;
         }
         parse_ulong_val(&lim, nullptr, TYPE_TIM, lGetString(q, QU_s_vmem), nullptr, 0);
         if (lim != DBL_MAX) {
            if (increase) {
               check_queue_limits++;
            } else {
               check_queue_limits--;
            }
            break;
         }
         parse_ulong_val(&lim, nullptr, TYPE_TIM, lGetString(q, QU_h_vmem), nullptr, 0);
         if (lim != DBL_MAX) {
            if (increase) {
               check_queue_limits++;
            } else {
               check_queue_limits--;
            }
            break;
         }
      }
   }
}

bool check_for_queue_limits(void)
{
   bool ret = false;

   if (check_queue_limits != 0) {
      ret = true;
   }

   return ret;
}
