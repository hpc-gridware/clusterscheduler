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


namespace ocs {
   class QStatDefaultViewBase;
}
int qstat_no_group(lList **alpp, ocs::QStatParameter &parameter, ocs::QStatModel &model, ocs::QStatDefaultViewBase &view);

void calc_longest_queue_length(ocs::QStatParameter &parameter, ocs::QStatModel &model);

int job_handle_resources(const lList* cel, lList* centry_list, int slots, int scope,
                         lList **alpp, bool is_hard_resource, ocs::QStatDefaultViewBase &view);
