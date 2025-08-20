#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/EH.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Execution Host
*
* This is a host running an sge_execd.
*
*    SGE_HOST(EH_name) - unique name
*    Unique name of the execution host.
*    This name must be resolvable on all hosts dealing with it
*    (master host, the execution host itself, interactive and tightly integrated parallel jobs contacting it)
*
*    SGE_LIST(EH_scaling_list) - instructions for scaling of host load values
*    Used to scale host load values.
*    Contains pairs of load value names and doubles.
*
*    SGE_LIST(EH_consumable_config_list) - consumable resources
*    consumable resources of host
*
*    SGE_LIST(EH_usage_scaling_list) - scaling of usage values
*    defines scaling of job usage values reported by this execution host
*
*    SGE_LIST(EH_load_list) - list of load values
*    list of load values (e.g. load_avg) reported by the execution host
*
*    SGE_ULONG64(EH_lt_heard_from) - last heard from
*    timestamp when the sge_execd on the host last communicated with sge_qmaster
*
*    SGE_ULONG(EH_processors) - number of processors
*    number of processors of the execution host
*    actually the number of processor cores (@todo threads?)
*
*    SGE_LIST(EH_acl) - user access list
*    userset defining who can access the host
*
*    SGE_LIST(EH_xacl) - user no access list
*    userset defining who can not access the host
*
*    SGE_LIST(EH_prj) - project access list
*    project list defining which jobs of which projects can run on the host
*
*    SGE_LIST(EH_xprj) - project no access list
*    project list defining jobs of which projects can run not on the host
*
*    SGE_DOUBLE(EH_sort_value) - sort value based on load
*    sort value which is only used in the scheduler thread
*
*    SGE_ULONG(EH_reuse_me) - to be re-used
*    @todo field can be reused or removed
*
*    SGE_ULONG(EH_tagged) - tagging of hosts
*    used in scheduler to tag hosts
*
*    SGE_ULONG(EH_load_correction_factor) - @todo add summary
*    a value of 100 (stands for 1)
*    means the load values of this host
*    has to be increased fully by all
*    values from
*    conf.load_decay_adjustments only
*    scheduler local not spooled
*
*    SGE_ULONG(EH_seq_no) - host sequence number
*    suitability of this host for a job, scheduler only
*
*    SGE_STRING(EH_real_name) - real host name
*    in case of pseudo host: real name
*    @todo is this still used? Where?
*
*    SGE_ULONG(EH_sge_load) - SGEEE load
*    calculated from load values, scheduler only
*
*    SGE_DOUBLE(EH_sge_ticket_pct) - percentage of tickets
*    percentage of total SGEEE tickets, scheduler only
*
*    SGE_DOUBLE(EH_sge_load_pct) - percentage of load
*    percentage of total SGEEE tickets, scheduler only
*
*    SGE_ULONG(EH_featureset_id) - featureset id
*    supported feature-set id @todo still used?
*
*    SGE_LIST(EH_scaled_usage_list) - scaled usage
*    scaled usage for jobs on a host - used by sge_host_mon @todo: still used?
*
*    SGE_LIST(EH_scaled_usage_pct_list) - scaled usage percentage
*    scaled usage for jobs on a host - used by sge_host_mon @todo still used?
*
*    SGE_ULONG(EH_num_running_jobs) - number of running jobs
*    number of jobs running on a host - used by sge_host_mon @todo still used?
*
*    SGE_ULONG(EH_load_report_interval) - load report interval
*    used for caching from global/local configuration
*
*    SGE_LIST(EH_resource_utilization) - resource utilization
*    contains per consumable information about resource utilization for this host
*
*    SGE_LIST(EH_cached_complexes) - cached complexes
*    used in scheduler for caching built attributes
*
*    SGE_ULONG(EH_cache_version) - cache version
*    used to decide whether QU_cached_complexes needs a refresh
*
*    SGE_ULONG(EH_master_host) - master host
*    @todo no longer used, remove
*
*    SGE_ULONG(EH_reschedule_unknown) - timeout for rescheduling jobs
*    used for caching from global/local conf; timout after which jobs will be rescheduled automatically
*
*    SGE_LIST(EH_reschedule_unknown_list) - jobs which will be rescheduled
*    after the rundown of reschedule_unknown this list contains all jobs which will be rescheduled automatically
*
*    SGE_ULONG(EH_report_seqno) - sequence number of the last report
*    sequence number of the last report (job/load/..) qmaster received from the execd.
*    This seqno is used to detect old * reports, because reports are send * asynchronously
*    and we have no guarantee that they arrive in order at qmaster
*
*    SGE_LIST(EH_report_variables) - variables for reporting
*    @todo add description
*
*    SGE_LIST(EH_merged_report_variables) - merged variables for reporting
*    list of variables written to the report file, merged from global host and actual host
*
*    SGE_LIST(EH_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*    SGE_STRING(EH_internal_topology) - topology string stored internally
*    topology string stored internally, used by qmaster
*    contains hardware information (socket, cores, threads),
*    memory information (numa nodes and caches),
*    and attributes for those nodes (size, speed etc.)
*
*/

enum {
   EH_name = EH_LOWERBOUND,
   EH_scaling_list,
   EH_consumable_config_list,
   EH_usage_scaling_list,
   EH_load_list,
   EH_lt_heard_from,
   EH_processors,
   EH_acl,
   EH_xacl,
   EH_prj,
   EH_xprj,
   EH_sort_value,
   EH_reuse_me,
   EH_tagged,
   EH_load_correction_factor,
   EH_seq_no,
   EH_real_name,
   EH_sge_load,
   EH_sge_ticket_pct,
   EH_sge_load_pct,
   EH_featureset_id,
   EH_scaled_usage_list,
   EH_scaled_usage_pct_list,
   EH_num_running_jobs,
   EH_load_report_interval,
   EH_resource_utilization,
   EH_cached_complexes,
   EH_cache_version,
   EH_master_host,
   EH_reschedule_unknown,
   EH_reschedule_unknown_list,
   EH_report_seqno,
   EH_report_variables,
   EH_merged_report_variables,
   EH_joker,
   EH_internal_topology
};

LISTDEF(EH_Type)
   SGE_HOST(EH_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(EH_scaling_list, HS_Type, CULL_SPOOL)
   SGE_LIST(EH_consumable_config_list, CE_Type, CULL_SPOOL)
   SGE_LIST(EH_usage_scaling_list, HS_Type, CULL_SPOOL)
   SGE_LIST(EH_load_list, HL_Type, CULL_SPOOL)
   SGE_ULONG64(EH_lt_heard_from, CULL_DEFAULT)
   SGE_ULONG(EH_processors, CULL_SPOOL)
   SGE_LIST(EH_acl, US_Type, CULL_SPOOL)
   SGE_LIST(EH_xacl, US_Type, CULL_SPOOL)
   SGE_LIST(EH_prj, PR_Type, CULL_SPOOL)
   SGE_LIST(EH_xprj, PR_Type, CULL_SPOOL)
   SGE_DOUBLE(EH_sort_value, CULL_DEFAULT)
   SGE_ULONG(EH_reuse_me, CULL_DEFAULT)
   SGE_ULONG(EH_tagged, CULL_DEFAULT)
   SGE_ULONG(EH_load_correction_factor, CULL_DEFAULT)
   SGE_ULONG(EH_seq_no, CULL_DEFAULT)
   SGE_STRING(EH_real_name, CULL_DEFAULT)
   SGE_ULONG(EH_sge_load, CULL_DEFAULT)
   SGE_DOUBLE(EH_sge_ticket_pct, CULL_DEFAULT)
   SGE_DOUBLE(EH_sge_load_pct, CULL_DEFAULT)
   SGE_ULONG(EH_featureset_id, CULL_DEFAULT)
   SGE_LIST(EH_scaled_usage_list, UA_Type, CULL_DEFAULT)
   SGE_LIST(EH_scaled_usage_pct_list, UA_Type, CULL_DEFAULT)
   SGE_ULONG(EH_num_running_jobs, CULL_DEFAULT)
   SGE_ULONG(EH_load_report_interval, CULL_DEFAULT)
   SGE_LIST(EH_resource_utilization, RUE_Type, CULL_DEFAULT)
   SGE_LIST(EH_cached_complexes, CE_Type, CULL_DEFAULT)
   SGE_ULONG(EH_cache_version, CULL_DEFAULT)
   SGE_ULONG(EH_master_host, CULL_DEFAULT)
   SGE_ULONG(EH_reschedule_unknown, CULL_DEFAULT)
   SGE_LIST(EH_reschedule_unknown_list, RU_Type, CULL_DEFAULT)
   SGE_ULONG(EH_report_seqno, CULL_DEFAULT)
   SGE_LIST(EH_report_variables, STU_Type, CULL_SPOOL)
   SGE_LIST(EH_merged_report_variables, STU_Type, CULL_DEFAULT)
   SGE_LIST(EH_joker, VA_Type, CULL_SPOOL)
   SGE_STRING(EH_internal_topology, CULL_DEFAULT)
LISTEND

NAMEDEF(EHN)
   NAME("EH_name")
   NAME("EH_scaling_list")
   NAME("EH_consumable_config_list")
   NAME("EH_usage_scaling_list")
   NAME("EH_load_list")
   NAME("EH_lt_heard_from")
   NAME("EH_processors")
   NAME("EH_acl")
   NAME("EH_xacl")
   NAME("EH_prj")
   NAME("EH_xprj")
   NAME("EH_sort_value")
   NAME("EH_reuse_me")
   NAME("EH_tagged")
   NAME("EH_load_correction_factor")
   NAME("EH_seq_no")
   NAME("EH_real_name")
   NAME("EH_sge_load")
   NAME("EH_sge_ticket_pct")
   NAME("EH_sge_load_pct")
   NAME("EH_featureset_id")
   NAME("EH_scaled_usage_list")
   NAME("EH_scaled_usage_pct_list")
   NAME("EH_num_running_jobs")
   NAME("EH_load_report_interval")
   NAME("EH_resource_utilization")
   NAME("EH_cached_complexes")
   NAME("EH_cache_version")
   NAME("EH_master_host")
   NAME("EH_reschedule_unknown")
   NAME("EH_reschedule_unknown_list")
   NAME("EH_report_seqno")
   NAME("EH_report_variables")
   NAME("EH_merged_report_variables")
   NAME("EH_joker")
   NAME("EH_internal_topology")
NAMEEND

#define EH_SIZE sizeof(EHN)/sizeof(char *)


