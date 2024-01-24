#ifndef SGE_PE_L_H
#define SGE_PE_L_H
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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Parallel Environment
*
* Defines the runtime environment for running shared memory or distributed memory parallelized applications
*
*    SGE_STRING(PE_name) - Name
*    Name of the pe.
*
*    SGE_ULONG(PE_slots) - Total Slots
*    Total number of slots which can be occupied by jobs running in the PE.
*
*    SGE_LIST(PE_user_list) - User List
*    US_type; list of allowed users.
*
*    SGE_LIST(PE_xuser_list) - XUser List
*    US_Type; list of not allowed users.
*
*    SGE_STRING(PE_start_proc_args) - Start Procedure
*    Cmd line sequence for starting the pe.
*
*    SGE_STRING(PE_stop_proc_args) - Stop Procedure
*    Cmd line sequence for stopping the pe.
*
*    SGE_STRING(PE_allocation_rule) - Allocation Rule
*    Defines how a job is distributed over multiple host.
*    This can be a fixed number of processors (slots) per machine
*    or one of the following special allocation rules:
*    $pe_slots: Allocate only slots on a single host.
*    $fill_up: Fill up slots on one host, then switch to the next host, ...
*    $round_robin. Occupy one slot of a host, then switch to the next one, ...
*
*    SGE_BOOL(PE_control_slaves) - Control Slaves
*    Defines whether (false) the job is executed in so called loose integration
*    (slave execution daemons do not know about the job, no job control, no accounting)
*    or (true) if it is executed in the tight integration, where execution daemons know about slave tasks
*    and slave tasks are started under OGE control (vial qrsh -inherit).
*    Tight integration provides full job control and we get accounting information for the whole job
*    including slave tasks on remote hosts.
*
*    SGE_BOOL(PE_job_is_first_task) - Job Is First Task
*    
*    When set to true then the job script also counts as task (a 4 times parallel job can then
*    comprise of the master task plus 3 slave tasks.
*    When set to true then the master task does not consume a slot. This is used to take into
*    account that the master task in many cases consumes very little resources, e.g.
*    it just does an mpirun call spawning slave tasks and then waits for the slave tasks to finish.
*    A 4 times parallel job can then comprise of the master task plus 4 slave tasks.
*
*    SGE_LIST(PE_resource_utilization) - Resource Utilization
*    Sub list of RUE_Type used to store resources (slots) currently in use by running parallel jobs.
*
*    SGE_STRING(PE_urgency_slots) - Urgency Slots
*    Specifies what slot amount shall be used when computing jobs
*    static urgency in case of jobs with slot range PE requests.
*    The actual problem is that when determining the urgency number
*    the number of slots finally assigned is not yet known. The following
*    settings are supported: min/max/avg/<fixed integer>
*
*    SGE_BOOL(PE_accounting_summary) - Accounting Summary
*    For tightly integrated parallel jobs.
*    Specifies if a single accounting record is written for the whole job,
*    or if every task (master task and slave tasks) gets an individual accounting record.
*
*/

enum {
   PE_name = PE_LOWERBOUND,
   PE_slots,
   PE_user_list,
   PE_xuser_list,
   PE_start_proc_args,
   PE_stop_proc_args,
   PE_allocation_rule,
   PE_control_slaves,
   PE_job_is_first_task,
   PE_resource_utilization,
   PE_urgency_slots,
   PE_accounting_summary
};

LISTDEF(PE_Type)
   SGE_STRING(PE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(PE_slots, CULL_SPOOL)
   SGE_LIST(PE_user_list, US_Type, CULL_SPOOL)
   SGE_LIST(PE_xuser_list, US_Type, CULL_SPOOL)
   SGE_STRING(PE_start_proc_args, CULL_SPOOL)
   SGE_STRING(PE_stop_proc_args, CULL_SPOOL)
   SGE_STRING(PE_allocation_rule, CULL_SPOOL)
   SGE_BOOL(PE_control_slaves, CULL_SPOOL)
   SGE_BOOL(PE_job_is_first_task, CULL_SPOOL)
   SGE_LIST(PE_resource_utilization, RUE_Type, CULL_DEFAULT)
   SGE_STRING(PE_urgency_slots, CULL_SPOOL)
   SGE_BOOL(PE_accounting_summary, CULL_SPOOL)
LISTEND

NAMEDEF(PEN)
   NAME("PE_name")
   NAME("PE_slots")
   NAME("PE_user_list")
   NAME("PE_xuser_list")
   NAME("PE_start_proc_args")
   NAME("PE_stop_proc_args")
   NAME("PE_allocation_rule")
   NAME("PE_control_slaves")
   NAME("PE_job_is_first_task")
   NAME("PE_resource_utilization")
   NAME("PE_urgency_slots")
   NAME("PE_accounting_summary")
NAMEEND

#define PE_SIZE sizeof(PEN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
