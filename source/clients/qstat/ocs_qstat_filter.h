#pragma once
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

#include "ocs_QStatModel.h"
#include "sgeobj/sge_daemonize.h"

#include "ocs_QStatParameter.h"

/* ------------- Cluster Queue Summary -------------------------------------- */
/* qstat -g c                                                                 */

typedef struct cqueue_summary_str cqueue_summary_t;

struct cqueue_summary_str {
   double load;
   bool   is_load_available;
   u_long32 used;
   u_long32 resv;
   u_long32 total;
   u_long32 temp_disabled; 
   u_long32 available; 
   u_long32 manual_intervention;
   u_long32 suspend_manual; 
   u_long32 suspend_threshold; 
   u_long32 suspend_on_subordinate;
   u_long32 suspend_calendar; 
   u_long32 unknown, load_alarm;
   u_long32 disabled_manual; 
   u_long32 disabled_calendar; 
   u_long32 ambiguous;
   u_long32 orphaned, error;
};

typedef struct cqueue_summary_handler_str cqueue_summary_handler_t;

struct cqueue_summary_handler_str {
   void *ctx;

   int (*report_started)(cqueue_summary_handler_t *thiz, lList **alpp, ocs::QStatParameter &parameter);
   int (*report_finished)(cqueue_summary_handler_t *thiz, lList **alpp);
   
   int (*report_cqueue)(cqueue_summary_handler_t *thiz, const char* cqname, cqueue_summary_t *summary, lList **alpp, ocs::QStatParameter &parameter);
   
   int (*destroy)(cqueue_summary_handler_t *thiz);
};

int qstat_cqueue_summary(cqueue_summary_handler_t *handler, lList **alpp, ocs::QStatParameter &parameter, ocs::QStatModel &model);

/* ---------------- QStat queue/job handling ---------------------------------*/

typedef struct queue_summary_str queue_summary_t;

struct queue_summary_str {
   
   const char* queue_type;
   
   u_long32    used_slots;
   u_long32    resv_slots;
   u_long32    total_slots;
   
   const char* arch;
   const char* state;
   
   const char* load_avg_str;
   bool has_load_value;
   bool has_load_value_from_object;
   double load_avg;

   bool has_access;                 //< qstat user has access to the queue that should be displayed
};

typedef struct job_summary_str job_summary_t;

struct job_summary_str {
   bool print_jobid;
   int priority;
   double nurg;
   double urg;
   double nppri;
   double nprior;
   double ntckts;
   double rrcontr;
   double wtcontr;
   double dlcontr;
   const char* name;
   const char* user;
   const char* project;
   const char* department;
   char state[8];
   u_long64 submit_time;
   u_long64 start_time;
   u_long64 deadline;
   
   bool   has_cpu_usage;
   u_long32 cpu_usage;
   bool   has_mem_usage;
   double mem_usage;
   bool   has_io_usage;
   double io_usage;
   
   bool   is_zombie;
   u_long override_tickets;
   bool   is_queue_assigned;
   u_long tickets;
   u_long otickets;
   u_long ftickets;
   u_long stickets;

   double share;
   const char* queue;
   const char* master;
   u_long32 slots;
   bool is_array;
   bool is_running;
   const char* task_id;
   
};

typedef enum {
   JOB_ADDITIONAL_INFO_ERROR = 0,
   CHECKPOINT_ENV  = 1,
   MASTER_QUEUE    = 2,
   FULL_JOB_NAME   = 3
} job_additional_info_t;

typedef struct task_summary_str task_summary_t;

struct task_summary_str {
   const char* task_id;
   const char* state;
   bool has_cpu_usage;
   double cpu_usage;
   bool has_mem_usage;
   double mem_usage;
   bool has_io_usage;
   double io_usage;
   bool is_running;
   bool has_exit_status;
   u_long32 exit_status;
};

typedef struct job_handler_str job_handler_t;

struct job_handler_str {

  void *ctx;

  int(*report_job)(job_handler_t* handler, u_long32 jid, job_summary_t *summary, lList **alpp, ocs::QStatParameter &parameter, ocs::QStatModel &model);
  
  int (*report_sub_tasks_started)(job_handler_t* handler, lList **alpp);
  int (*report_sub_task)(job_handler_t* handler, task_summary_t *summary, lList **alpp);
  int (*report_sub_tasks_finished)(job_handler_t* handler, lList **alpp);
  
  int (*report_additional_info)(job_handler_t *handler, job_additional_info_t name, const char* value, lList **alpp);
  
  int (*report_requested_pe)(job_handler_t *handler, const char* pe_name, const char* pe_range, lList **alpp);
  int (*report_granted_pe)(job_handler_t *handler, const char* pe_name, int pe_slots, lList **alpp);
  
  int (*report_request)(job_handler_t* handler, const char* name, const char* value, lList **alpp);
  
  int (*report_hard_resources_started)(job_handler_t* handler, int scope, lList **alpp);
  int (*report_hard_resource)(job_handler_t *handler, int scope, const char* name, const char* value, double uc, lList **alpp);
  int (*report_hard_resources_finished)(job_handler_t* handler, lList **alpp);

  int (*report_soft_resources_started)(job_handler_t* handler, int scope, lList **alpp);
  /* RH TODO: the soft resource/request has no contribution => remove the parameter uc */
  int (*report_soft_resource)(job_handler_t *handler, int scope, const char* name, const char* value, double uc, lList **alpp);
  int (*report_soft_resources_finished)(job_handler_t* handler, lList **alpp);
  
  int (*report_hard_requested_queues_started)(job_handler_t *handler, int scope, lList **alpp);
  int (*report_hard_requested_queue)(job_handler_t *handler, int scope, const char* name, lList **alpp);
  int (*report_hard_requested_queues_finished)(job_handler_t *handler, lList **alpp);
  
  int (*report_soft_requested_queues_started)(job_handler_t *handler, int scope, lList **alpp);
  int (*report_soft_requested_queue)(job_handler_t *handler, int scope, const char* name, lList **alpp);
  int (*report_soft_requested_queues_finished)(job_handler_t *handler, lList **alpp);
  
  int (*report_predecessors_requested_started)(job_handler_t* handler, lList **alpp);
  int (*report_predecessor_requested)(job_handler_t* handler, const char* name, lList **alpp);
  int (*report_predecessors_requested_finished)(job_handler_t* handler, lList **alpp);
  
  int (*report_predecessors_started)(job_handler_t* handler, lList **alpp);
  int (*report_predecessor)(job_handler_t* handler, u_long32 jid, lList **alpp);
  int (*report_predecessors_finished)(job_handler_t* handler, lList **alpp);
  
  int (*report_ad_predecessors_requested_started)(job_handler_t* handler, lList **alpp);
  int (*report_ad_predecessor_requested)(job_handler_t* handler, const char* name, lList **alpp);
  int (*report_ad_predecessors_requested_finished)(job_handler_t* handler, lList **alpp);
  
  int (*report_ad_predecessors_started)(job_handler_t* handler, lList **alpp);
  int (*report_ad_predecessor)(job_handler_t* handler, u_long32 jid, lList **alpp);
  int (*report_ad_predecessors_finished)(job_handler_t* handler, lList **alpp);

  int (*report_binding_started)(job_handler_t* handler, lList **alpp);
  int (*report_binding)(job_handler_t *handler, const char *binding, lList **alpp);
  int (*report_binding_finished)(job_handler_t* handler, lList **alpp);

  int (*report_job_finished)(job_handler_t* handler, u_long32 jid, lList **alpp);
};

typedef struct qstat_handler_str qstat_handler_t;

struct qstat_handler_str {
  void *ctx;

  int (*report_started)(qstat_handler_t* handler, lList** alpp);
  int (*report_finished)(qstat_handler_t* hanlder, lList** alpp);
  
  int (*report_queue_started)(qstat_handler_t *handler, const char* qname, lList **alpp, ocs::QStatParameter &parameter);
  int (*report_queue_summary)(qstat_handler_t *handler, const char* qname,  queue_summary_t *summary, lList **alpp, ocs::QStatParameter &parameter);
  int (*report_queue_load_alarm)(qstat_handler_t* handler, const char* qname, const char* reason, lList **alpp);
  int (*report_queue_suspend_alarm)(qstat_handler_t* handler, const char* qname, const char* reason, lList **alpp);
  int (*report_queue_message)(qstat_handler_t* handler, const char* qname, const char *message, lList **alpp);
  
  int (*report_queue_resource)(qstat_handler_t* handler, const char* dom, const char* name, const char* value, const char *details, lList **alpp);
  
  job_handler_t job_handler;
  
  int (*report_queue_jobs_started)(qstat_handler_t *handler, const char* qname, lList **alpp);
  int (*report_queue_jobs_finished)(qstat_handler_t *handler, const char* qname, lList **alpp, ocs::QStatParameter &parameter);

  int (*report_queue_finished)(qstat_handler_t* handler, const char* qname, lList **alpp, ocs::QStatParameter &parameter);

  int (*report_pending_jobs_started)(qstat_handler_t *handler, lList **alpp, ocs::QStatParameter &parameter);
  int (*report_pending_jobs_finished)(qstat_handler_t *handler, lList **alpp);
  int (*report_finished_jobs_started)(qstat_handler_t *handler, lList **alpp, ocs::QStatParameter &parameter);
  int (*report_finished_jobs_finished)(qstat_handler_t *handler, lList **alpp);
  int (*report_error_jobs_started)(qstat_handler_t *handler, lList **alpp, ocs::QStatParameter &parameter);
  int (*report_error_jobs_finished)(qstat_handler_t *handler, lList **alpp);
  int (*report_zombie_jobs_started)(qstat_handler_t *handler, lList **alpp);
  int (*report_zombie_jobs_finished)(qstat_handler_t *handler, lList **alpp);
  
  
  int (*destroy)(qstat_handler_t* handler);
};



int qstat_no_group(qstat_handler_t* handler, lList **alpp, ocs::QStatParameter &parameter, ocs::QStatModel &model);
