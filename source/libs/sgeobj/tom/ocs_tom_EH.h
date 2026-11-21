#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Execution Host: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of EH, Execution Host
 *
 * The enumeration is the type of every field number of EH, so a parameter
 * of this type takes the field numbers of EH only.
 */
enum EH_Field {
   /**
    * @brief unique name
    *
    * Unique name of the execution host.
    * This name must be resolvable on all hosts dealing with it
    * (master host, the execution host itself, interactive and tightly integrated parallel jobs contacting it)
    */
   EH_name = EH_LOWERBOUND,
   /**
    * @brief instructions for scaling of host load values
    *
    * Used to scale host load values.
    * Contains pairs of load value names and doubles.
    */
   EH_scaling_list,
   /**
    * @brief consumable resources
    *
    * consumable resources of host
    */
   EH_consumable_config_list,
   /**
    * @brief scaling of usage values
    *
    * defines scaling of job usage values reported by this execution host
    */
   EH_usage_scaling_list,
   /**
    * @brief list of load values
    *
    * list of load values (e.g. load_avg) reported by the execution host
    */
   EH_load_list,
   /**
    * @brief last heard from
    *
    * timestamp when the sge_execd on the host last communicated with sge_qmaster
    */
   EH_lt_heard_from,
   /**
    * @brief number of processors
    *
    * number of processors of the execution host
    * actually the number of processor cores (@todo threads?)
    */
   EH_processors,
   /**
    * @brief user access list
    *
    * userset defining who can access the host
    */
   EH_acl,
   /**
    * @brief user no access list
    *
    * userset defining who can not access the host
    */
   EH_xacl,
   /**
    * @brief project access list
    *
    * project list defining which jobs of which projects can run on the host
    */
   EH_prj,
   /**
    * @brief project no access list
    *
    * project list defining jobs of which projects can run not on the host
    */
   EH_xprj,
   /**
    * @brief sort value based on load
    *
    * sort value which is only used in the scheduler thread
    */
   EH_sort_value,
   /**
    * @brief tagging of hosts
    *
    * used in scheduler to tag hosts
    */
   EH_tagged,
   /**
    * @brief Load Correction Factor
    *
    * Artificial load added for jobs that have just started, decaying over `load_adjustment_decay_time`. Stops the scheduler overloading a host by dispatching to it faster than the reported load can rise.
    */
   EH_load_correction_factor,
   /**
    * @brief host sequence number
    *
    * suitability of this host for a job, scheduler only
    */
   EH_seq_no,
   /**
    * @brief SGEEE load
    *
    * calculated from load values, scheduler only
    */
   EH_sge_load,
   /**
    * @brief percentage of tickets
    *
    * percentage of total SGEEE tickets, scheduler only
    */
   EH_sge_ticket_pct,
   /**
    * @brief percentage of load
    *
    * percentage of total SGEEE tickets, scheduler only
    */
   EH_sge_load_pct,
   /**
    * @brief load report interval
    *
    * used for caching from global/local configuration
    */
   EH_load_report_interval,
   /**
    * @brief resource utilization
    *
    * contains per consumable information about resource utilization for this host
    */
   EH_resource_utilization,
   /**
    * @brief cached complexes
    *
    * used in scheduler for caching built attributes
    */
   EH_cached_complexes,
   /**
    * @brief cache version
    *
    * used to decide whether QU_cached_complexes needs a refresh
    */
   EH_cache_version,
   /**
    * @brief timeout for rescheduling jobs
    *
    * used for caching from global/local conf; timout after which jobs will be rescheduled automatically
    */
   EH_reschedule_unknown,
   /**
    * @brief jobs which will be rescheduled
    *
    * after the rundown of reschedule_unknown this list contains all jobs which will be rescheduled automatically
    */
   EH_reschedule_unknown_list,
   /**
    * @brief sequence number of the last report
    *
    * sequence number of the last report (job/load/..) qmaster received from the execd.
    * This seqno is used to detect old * reports, because reports are send * asynchronously
    * and we have no guarantee that they arrive in order at qmaster
    */
   EH_report_seqno,
   /**
    * @brief variables for reporting
    *
    * @todo add description
    */
   EH_report_variables,
   /**
    * @brief merged variables for reporting
    *
    * list of variables written to the report file, merged from global host and actual host
    */
   EH_merged_report_variables,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   EH_joker,
   /**
    * @brief topology string stored internally
    *
    * topology string stored internally, used by qmaster
    * contains hardware information (socket, cores, threads),
    * memory information (numa nodes and caches),
    * and attributes for those nodes (size, speed etc.)
    */
   EH_internal_topology,
   /**
    * @brief time at which the host added itself to the cluster
    *
    * The time at which the host added itself to the cluster, when its execution
    * daemon registered and a rule of the qmaster_params AUTO_ADD_HOSTS matched it.
    * 0 for a host which an administrator added.
    */
   EH_auto_added_time,
};

namespace ocs::tom {

   struct CE;
   struct HL;
   struct HS;
   struct PR;
   struct RU;
   struct RUE;
   struct STU;
   struct US;
   struct VA;

   /**
    * @brief Execution Host
    *
    * This is a host running an sge_execd.
    */
   struct EH {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {EH_name, "EH_name", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {EH_scaling_list, "EH_scaling_list", FieldType::List, Flag::Spool},
         {EH_consumable_config_list, "EH_consumable_config_list", FieldType::List, Flag::Spool},
         {EH_usage_scaling_list, "EH_usage_scaling_list", FieldType::List, Flag::Spool},
         {EH_load_list, "EH_load_list", FieldType::List, Flag::Spool},
         {EH_lt_heard_from, "EH_lt_heard_from", FieldType::Uint64, Flag::None},
         {EH_processors, "EH_processors", FieldType::Uint32, Flag::Spool},
         {EH_acl, "EH_acl", FieldType::List, Flag::Spool},
         {EH_xacl, "EH_xacl", FieldType::List, Flag::Spool},
         {EH_prj, "EH_prj", FieldType::List, Flag::Spool},
         {EH_xprj, "EH_xprj", FieldType::List, Flag::Spool},
         {EH_sort_value, "EH_sort_value", FieldType::Double, Flag::None},
         {EH_tagged, "EH_tagged", FieldType::Uint32, Flag::None},
         {EH_load_correction_factor, "EH_load_correction_factor", FieldType::Uint32, Flag::None},
         {EH_seq_no, "EH_seq_no", FieldType::Uint32, Flag::None},
         {EH_sge_load, "EH_sge_load", FieldType::Uint32, Flag::None},
         {EH_sge_ticket_pct, "EH_sge_ticket_pct", FieldType::Double, Flag::None},
         {EH_sge_load_pct, "EH_sge_load_pct", FieldType::Double, Flag::None},
         {EH_load_report_interval, "EH_load_report_interval", FieldType::Uint32, Flag::None},
         {EH_resource_utilization, "EH_resource_utilization", FieldType::List, Flag::None},
         {EH_cached_complexes, "EH_cached_complexes", FieldType::List, Flag::None},
         {EH_cache_version, "EH_cache_version", FieldType::Uint32, Flag::None},
         {EH_reschedule_unknown, "EH_reschedule_unknown", FieldType::Uint32, Flag::None},
         {EH_reschedule_unknown_list, "EH_reschedule_unknown_list", FieldType::List, Flag::Spool},
         {EH_report_seqno, "EH_report_seqno", FieldType::Uint32, Flag::None},
         {EH_report_variables, "EH_report_variables", FieldType::List, Flag::Spool},
         {EH_merged_report_variables, "EH_merged_report_variables", FieldType::List, Flag::None},
         {EH_joker, "EH_joker", FieldType::List, Flag::Spool},
         {EH_internal_topology, "EH_internal_topology", FieldType::String, Flag::None},
         {EH_auto_added_time, "EH_auto_added_time", FieldType::Uint64, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of EH belongs to EH */
   template <>
   struct TypeOf<EH_Field> {
      using type = EH;   ///< the object type
   };

   /** @brief EH_scaling_list holds elements of HS */
   template <>
   struct SubType<EH_scaling_list> {
      using type = HS;   ///< the sub-type
   };

   /** @brief EH_consumable_config_list holds elements of CE */
   template <>
   struct SubType<EH_consumable_config_list> {
      using type = CE;   ///< the sub-type
   };

   /** @brief EH_usage_scaling_list holds elements of HS */
   template <>
   struct SubType<EH_usage_scaling_list> {
      using type = HS;   ///< the sub-type
   };

   /** @brief EH_load_list holds elements of HL */
   template <>
   struct SubType<EH_load_list> {
      using type = HL;   ///< the sub-type
   };

   /** @brief EH_acl holds elements of US */
   template <>
   struct SubType<EH_acl> {
      using type = US;   ///< the sub-type
   };

   /** @brief EH_xacl holds elements of US */
   template <>
   struct SubType<EH_xacl> {
      using type = US;   ///< the sub-type
   };

   /** @brief EH_prj holds elements of PR */
   template <>
   struct SubType<EH_prj> {
      using type = PR;   ///< the sub-type
   };

   /** @brief EH_xprj holds elements of PR */
   template <>
   struct SubType<EH_xprj> {
      using type = PR;   ///< the sub-type
   };

   /** @brief EH_resource_utilization holds elements of RUE */
   template <>
   struct SubType<EH_resource_utilization> {
      using type = RUE;   ///< the sub-type
   };

   /** @brief EH_cached_complexes holds elements of CE */
   template <>
   struct SubType<EH_cached_complexes> {
      using type = CE;   ///< the sub-type
   };

   /** @brief EH_reschedule_unknown_list holds elements of RU */
   template <>
   struct SubType<EH_reschedule_unknown_list> {
      using type = RU;   ///< the sub-type
   };

   /** @brief EH_report_variables holds elements of STU */
   template <>
   struct SubType<EH_report_variables> {
      using type = STU;   ///< the sub-type
   };

   /** @brief EH_merged_report_variables holds elements of STU */
   template <>
   struct SubType<EH_merged_report_variables> {
      using type = STU;   ///< the sub-type
   };

   /** @brief EH_joker holds elements of VA */
   template <>
   struct SubType<EH_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
