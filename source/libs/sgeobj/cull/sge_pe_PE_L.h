#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/*
 * This code was generated from file source/libs/sgeobj/json/PE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

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
*    SGE_BOOL(PE_master_forks_slaves) - Master Forks Slaves
*    For tightly integrated parallel jobs.
*    Specifies if slave tasks on the master host are started via qrsh -inherit or if they are started
*    by the master task process itself (via fork/exec or as threads).
*
*    SGE_BOOL(PE_daemon_forks_slaves) - Daemon Forks Slaves
*    For tightly integrated parallel jobs.
*    Specifies if slave tasks on slave host are started individually via qrsh -inherit or if a single
*    process is started via qrsh -inherit which then starts the slave tasks (via fork/exec or as threads).
*
*    SGE_BOOL(PE_ignore_slave_requests_on_master_host) - Daemon Forks Slaves
*    For tightly integrated parallel jobs.
*    When slave tasks are started on the master host, this flag specifies if slave specific requests
*    shall be applied to these slave tasks or not.
*
*    SGE_LIST(PE_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
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
   PE_accounting_summary,
   PE_master_forks_slaves,
   PE_daemon_forks_slaves,
   PE_ignore_slave_requests_on_master_host,
   PE_joker
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
   SGE_BOOL(PE_master_forks_slaves, CULL_SPOOL)
   SGE_BOOL(PE_daemon_forks_slaves, CULL_SPOOL)
   SGE_BOOL(PE_ignore_slave_requests_on_master_host, CULL_SPOOL)
   SGE_LIST(PE_joker, VA_Type, CULL_SPOOL)
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
   NAME("PE_master_forks_slaves")
   NAME("PE_daemon_forks_slaves")
   NAME("PE_ignore_slave_requests_on_master_host")
   NAME("PE_joker")
NAMEEND

#define PE_SIZE sizeof(PEN)/sizeof(char *)


