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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/****** SERF/-SERF_Interface *******************************************************
*  NAME
*     SERF -- Schedule entry recording facility
*
*  FUNCTION
*     The enlisted functions below allow for plugging in any module that 
*     records schedule entries be used that registers through sge_serf_init()
*     the following methods:
*
*        typedef void (*record_schedule_entry_func_t)(
*           u_long32 job_id, 
*           u_long32 ja_taskid, 
*           const char *state, 
*           u_long64 start_time,
*           u_long64 end_time,
*           char level_char, 
*           const char *object_name, 
*           const char *name, 
*           double utilization);
*
*        typedef void (*new_schedule_func_t)(u_long64 time);
*    
*  SEE ALSO
*     SERF/serf_init()
*     SERF/serf_record_entry()
*     SERF/serf_new_interval()
*     SERF/serf_get_active()
*     SERF/serf_set_active()
*     SERF/serf_exit()
*******************************************************************************/

#define SCHEDULING_RECORD_ENTRY_TYPE_RUNNING    "RUNNING"
#define SCHEDULING_RECORD_ENTRY_TYPE_SUSPENDED  "SUSPENDED"
#define SCHEDULING_RECORD_ENTRY_TYPE_PREEMPTING "MIGRATING"
#define SCHEDULING_RECORD_ENTRY_TYPE_STARTING   "STARTING"
#define SCHEDULING_RECORD_ENTRY_TYPE_RESERVING  "RESERVING"

typedef void (*record_schedule_entry_func_t)(u_long32 job_id, u_long32 ja_taskid, 
      const char *state, u_long64 start_time, u_long64 end_time, char level_char,
      const char *object_name, const char *name, double utilization);
typedef void (*new_schedule_func_t)();

void serf_init(record_schedule_entry_func_t, new_schedule_func_t);
void serf_record_entry(u_long32 job_id, u_long32 ja_taskid,
                       const char *state, u_long64 start_time, u_long64 end_time, u_long32 level,
                       const char *object_name, const char *name, double utilization);
void serf_new_interval();
void serf_exit();
