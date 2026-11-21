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
 * This code was generated from file source/libs/sgeobj/json/AR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Advance Reservation: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of AR, Advance Reservation
 *
 * The enumeration is the type of every field number of AR, so a parameter
 * of this type takes the field numbers of AR only.
 */
enum AR_Field {
   /**
    * @brief AR Id
    *
    * A unique identifier for the advance reservation, similar to the job id.
    */
   AR_id = AR_LOWERBOUND,
   /**
    * @brief AR Name
    *
    * A name for the advance reservation.
    */
   AR_name,
   /**
    * @brief Account
    *
    * The account to which the advance reservation is charged.
    */
   AR_account,
   /**
    * @brief Owner
    *
    * The owner of the advance reservation (the user who submitted the AR).
    */
   AR_owner,
   /**
    * @brief Group
    *
    * The UNIX group of the advance reservation owner.
    */
   AR_group,
   /**
    * @brief Submission Time
    *
    * The time when the AR was submitted in microseconds since epoch.
    */
   AR_submission_time,
   /**
    * @brief Start Time
    *
    * The start time of the AR in microseconds since epoch
    */
   AR_start_time,
   /**
    * @brief End Time
    *
    * The end time of the AR in microseconds since epoch
    */
   AR_end_time,
   /**
    * @brief Duration
    *
    * The duration of the AR in microseconds
    */
   AR_duration,
   /**
    * @brief Verify
    *
    * From qrsub -w v|e.
    */
   AR_verify,
   /**
    * @brief Error Handling
    *
    * From qrsub -he yes/no.
    */
   AR_error_handling,
   /**
    * @brief State
    *
    * The state of the advance reservation.
    */
   AR_state,
   /**
    * @brief Checkpoint Name
    *
    * Checkpointing environment jobs running in the AR can use.
    */
   AR_checkpoint_name,
   /**
    * @brief Resource List
    *
    * The list of resources requested by the advance reservation (qrsub -l).
    * Just one hard resource list. @todo The -scope feature is still missing.
    */
   AR_resource_list,
   /**
    * @brief Resource Utilization
    *
    * The utilization of resources by jobs running in the AR.
    */
   AR_resource_utilization,
   /**
    * @brief Queue List
    *
    * The list of queues requested by the advance reservation (qrsub -q).
    * Just one hard queue list. @todo The -scope feature is still missing.
    */
   AR_queue_list,
   /**
    * @brief Granted Slots
    *
    * The queue instances and slot counts the reservation holds (`JG`), the same shape as a job's granted destination identifier list. Set once when the reservation is confirmed.
    */
   AR_granted_slots,
   /**
    * @brief Reserved Hosts
    *
    * Will hold the list of hosts which are reserved for the advance reservation
    * together with the amount of granted consumables in EH_consumable_resources.
    * @todo not yet implemented, see CS-430
    */
   AR_reserved_hosts,
   /**
    * @brief Reserved Queues
    *
    * Holds the list of queues which are reserved for the advance reservation
    * together with the amount of granted consumables in QU_consumable_resources.
    */
   AR_reserved_queues,
   /**
    * @brief Mail Options
    *
    * Mail options for the advance reservation from qrsub -m.
    */
   AR_mail_options,
   /**
    * @brief Mail List
    *
    * Mail list for the advance reservation from qrsub -M.
    */
   AR_mail_list,
   /**
    * @brief Parallel Environment
    *
    * The parallel environment which is requested by the advance reservation from qrsub -pe.
    */
   AR_pe,
   /**
    * @brief PE Range
    *
    * The number of slots (can be a range) which are requested for the advance reservation.
    */
   AR_pe_range,
   /**
    * @brief Granted PE
    *
    * The parallel environment which was granted to the advance reservation based on the pe request.
    */
   AR_granted_pe,
   /**
    * @brief Master Queue List
    *
    * The list of possible master queues requested by the advance reservation (qrsub -masterq).
    */
   AR_master_queue_list,
   /**
    * @brief ACL List
    *
    * The acl_list defines which users may use the advance reservation.
    */
   AR_acl_list,
   /**
    * @brief XACL List
    *
    * The xacl_list defines which users may not use the advance reservation.
    */
   AR_xacl_list,
   /**
    * @brief Type
    *
    * Holds the information from the qrsub -now option (immediate or batch).
    */
   AR_type,
   /**
    * @brief QI Errors
    *
    * Number of queue instances which are in some error state.
    */
   AR_qi_errors,
   /**
    * @brief Request Set List
    *
    * @todo placeholder for implementing the -scope switch
    * List of request sets. 0 .. 3 request sets can exist: global, master, slave.
    * Sequential ARs can have a single job request set, the global one.
    * Parallel ARs can have up to 3 request sets: global requests,
    * requests for the master task, requests for the slave tasks.
    */
   AR_request_set_list,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   AR_joker,
   /**
    * @brief Granted Resources
    *
    * List of granted resources, currently these are granted bindings only.
    */
   AR_granted_resources_list,
   /**
    * @brief Binding Strategy
    *
    * Binding strategy for the advance reservation
    */
   AR_binding,
   /**
    * @brief Project
    *
    * Project the AR was submitted to.
    */
   AR_project,
   /**
    * @brief PE Object
    *
    * PE object granted to this AR (PE) when it was scheduled.
    */
   AR_pe_object,
   /**
    * @brief Allocation Rule
    *
    * The PE allocation rule the advance reservation reserves its slots with (qrsub -par).
    * It overrides the allocation_rule of the requested PE, in the same way as the -par
    * switch of qsub overrides it for a job. Only set when the AR requests a PE.
    */
   AR_allocation_rule,
};

namespace ocs::tom {

   struct ARA;
   struct BN;
   struct CE;
   struct EH;
   struct GRU;
   struct JG;
   struct JRS;
   struct MR;
   struct PE;
   struct QR;
   struct QU;
   struct RN;
   struct RUE;
   struct VA;

   /**
    * @brief Advance Reservation
    *
    * An object of this type represents an advance reservation.
    * An advance reservation reserves (consumable) resources for a certain time period.
    * Jobs which are submitted into the AR can use these resources.
    * See also man pages qrsub.1, qrstat.1, and qrdel.1
    */
   struct AR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {AR_id, "AR_id", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {AR_name, "AR_name", FieldType::String, Flag::Spool},
         {AR_account, "AR_account", FieldType::String, Flag::Spool},
         {AR_owner, "AR_owner", FieldType::String, Flag::Spool},
         {AR_group, "AR_group", FieldType::String, Flag::Spool},
         {AR_submission_time, "AR_submission_time", FieldType::Uint64, Flag::Spool},
         {AR_start_time, "AR_start_time", FieldType::Uint64, Flag::Spool},
         {AR_end_time, "AR_end_time", FieldType::Uint64, Flag::Spool},
         {AR_duration, "AR_duration", FieldType::Uint64, Flag::Spool},
         {AR_verify, "AR_verify", FieldType::Uint32, Flag::Spool},
         {AR_error_handling, "AR_error_handling", FieldType::Uint32, Flag::Spool},
         {AR_state, "AR_state", FieldType::Uint32, Flag::Spool},
         {AR_checkpoint_name, "AR_checkpoint_name", FieldType::String, Flag::Spool},
         {AR_resource_list, "AR_resource_list", FieldType::List, Flag::Spool},
         {AR_resource_utilization, "AR_resource_utilization", FieldType::List, Flag::None},
         {AR_queue_list, "AR_queue_list", FieldType::List, Flag::Spool},
         {AR_granted_slots, "AR_granted_slots", FieldType::List, Flag::Spool},
         {AR_reserved_hosts, "AR_reserved_hosts", FieldType::List, Flag::Spool},
         {AR_reserved_queues, "AR_reserved_queues", FieldType::List, Flag::Spool},
         {AR_mail_options, "AR_mail_options", FieldType::Uint32, Flag::Spool},
         {AR_mail_list, "AR_mail_list", FieldType::List, Flag::Spool},
         {AR_pe, "AR_pe", FieldType::String, Flag::Spool},
         {AR_pe_range, "AR_pe_range", FieldType::List, Flag::Spool},
         {AR_granted_pe, "AR_granted_pe", FieldType::String, Flag::Spool},
         {AR_master_queue_list, "AR_master_queue_list", FieldType::List, Flag::Spool},
         {AR_acl_list, "AR_acl_list", FieldType::List, Flag::Spool},
         {AR_xacl_list, "AR_xacl_list", FieldType::List, Flag::Spool},
         {AR_type, "AR_type", FieldType::Uint32, Flag::Spool},
         {AR_qi_errors, "AR_qi_errors", FieldType::Uint32, Flag::None},
         {AR_request_set_list, "AR_request_set_list", FieldType::List, Flag::Spool},
         {AR_joker, "AR_joker", FieldType::List, Flag::Spool},
         {AR_granted_resources_list, "AR_granted_resources_list", FieldType::List, Flag::Spool},
         {AR_binding, "AR_binding", FieldType::Object, Flag::Spool},
         {AR_project, "AR_project", FieldType::String, Flag::Spool},
         {AR_pe_object, "AR_pe_object", FieldType::Object, Flag::Spool},
         {AR_allocation_rule, "AR_allocation_rule", FieldType::String, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of AR belongs to AR */
   template <>
   struct TypeOf<AR_Field> {
      using type = AR;   ///< the object type
   };

   /** @brief AR_resource_list holds elements of CE */
   template <>
   struct SubType<AR_resource_list> {
      using type = CE;   ///< the sub-type
   };

   /** @brief AR_resource_utilization holds elements of RUE */
   template <>
   struct SubType<AR_resource_utilization> {
      using type = RUE;   ///< the sub-type
   };

   /** @brief AR_queue_list holds elements of QR */
   template <>
   struct SubType<AR_queue_list> {
      using type = QR;   ///< the sub-type
   };

   /** @brief AR_granted_slots holds elements of JG */
   template <>
   struct SubType<AR_granted_slots> {
      using type = JG;   ///< the sub-type
   };

   /** @brief AR_reserved_hosts holds elements of EH */
   template <>
   struct SubType<AR_reserved_hosts> {
      using type = EH;   ///< the sub-type
   };

   /** @brief AR_reserved_queues holds elements of QU */
   template <>
   struct SubType<AR_reserved_queues> {
      using type = QU;   ///< the sub-type
   };

   /** @brief AR_mail_list holds elements of MR */
   template <>
   struct SubType<AR_mail_list> {
      using type = MR;   ///< the sub-type
   };

   /** @brief AR_pe_range holds elements of RN */
   template <>
   struct SubType<AR_pe_range> {
      using type = RN;   ///< the sub-type
   };

   /** @brief AR_master_queue_list holds elements of QR */
   template <>
   struct SubType<AR_master_queue_list> {
      using type = QR;   ///< the sub-type
   };

   /** @brief AR_acl_list holds elements of ARA */
   template <>
   struct SubType<AR_acl_list> {
      using type = ARA;   ///< the sub-type
   };

   /** @brief AR_xacl_list holds elements of ARA */
   template <>
   struct SubType<AR_xacl_list> {
      using type = ARA;   ///< the sub-type
   };

   /** @brief AR_request_set_list holds elements of JRS */
   template <>
   struct SubType<AR_request_set_list> {
      using type = JRS;   ///< the sub-type
   };

   /** @brief AR_joker holds elements of VA */
   template <>
   struct SubType<AR_joker> {
      using type = VA;   ///< the sub-type
   };

   /** @brief AR_granted_resources_list holds elements of GRU */
   template <>
   struct SubType<AR_granted_resources_list> {
      using type = GRU;   ///< the sub-type
   };

   /** @brief AR_binding holds elements of BN */
   template <>
   struct SubType<AR_binding> {
      using type = BN;   ///< the sub-type
   };

   /** @brief AR_pe_object holds elements of PE */
   template <>
   struct SubType<AR_pe_object> {
      using type = PE;   ///< the sub-type
   };

} // namespace ocs::tom
