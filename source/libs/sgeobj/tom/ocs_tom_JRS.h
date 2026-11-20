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
 * This code was generated from file source/libs/sgeobj/json/JRS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Request Set: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JRS, Job Request Set
 *
 * The enumeration is the type of every field number of JRS, so a parameter
 * of this type takes the field numbers of JRS only.
 */
enum JRS_Field {
   /**
    * @brief Scope
    *
    * Request scope: global, master, slaves.
    */
   JRS_scope = JRS_LOWERBOUND,
   /**
    * @brief Hard Resource List
    *
    * List of hard resource requests, from qsub -l name=value request.
    */
   JRS_hard_resource_list,
   /**
    * @brief Soft Resource List
    *
    * List of soft resource requests, from qsub -soft -l name=value request.
    */
   JRS_soft_resource_list,
   /**
    * @brief Hard Queue List
    *
    * List of hard queue requests, from qsub -q dest_identifier request.
    */
   JRS_hard_queue_list,
   /**
    * @brief Soft Queue List
    *
    * List of soft queue requests, from qsub -soft -q dest_identifier request.
    */
   JRS_soft_queue_list,
   /**
    * @brief Allocation Rule
    *
    * Overwrites the allocation rule of the parallel environment for this scope.
    */
   JRS_allocation_rule,
   /**
    * @brief Ignore Slave Requests on Master Host
    *
    * Overwrites the ignore_slave_requests_on_master_host option of the parallel environment for this scope.
    */
   JRS_ignore_slave_requests_on_master_host,
};

namespace ocs::tom {

   struct CE;
   struct QR;

   /**
    * @brief Job Request Set
    *
    * A job request set contains job requests (currently: queue requests and resource requests)
    * valid for a specific scope of parallel tasks (global, master, slaves).
    * Sequential jobs can have a single job request set, the global one.
    * Parallel jobs can have up to 3 request sets, global requests, requests for the master task,
    * requests for the slave tasks.
    * None of them must exist, every one can exist, depending on requests
    */
   struct JRS {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JRS_scope, "JRS_scope", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Spool},
         {JRS_hard_resource_list, "JRS_hard_resource_list", FieldType::List, Flag::Spool},
         {JRS_soft_resource_list, "JRS_soft_resource_list", FieldType::List, Flag::Spool},
         {JRS_hard_queue_list, "JRS_hard_queue_list", FieldType::List, Flag::Spool},
         {JRS_soft_queue_list, "JRS_soft_queue_list", FieldType::List, Flag::Spool},
         {JRS_allocation_rule, "JRS_allocation_rule", FieldType::String, Flag::Spool},
         {JRS_ignore_slave_requests_on_master_host, "JRS_ignore_slave_requests_on_master_host", FieldType::Bool, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JRS belongs to JRS */
   template <>
   struct TypeOf<JRS_Field> {
      using type = JRS;   ///< the object type
   };

   /** @brief JRS_hard_resource_list holds elements of CE */
   template <>
   struct SubType<JRS_hard_resource_list> {
      using type = CE;   ///< the sub-type
   };

   /** @brief JRS_soft_resource_list holds elements of CE */
   template <>
   struct SubType<JRS_soft_resource_list> {
      using type = CE;   ///< the sub-type
   };

   /** @brief JRS_hard_queue_list holds elements of QR */
   template <>
   struct SubType<JRS_hard_queue_list> {
      using type = QR;   ///< the sub-type
   };

   /** @brief JRS_soft_queue_list holds elements of QR */
   template <>
   struct SubType<JRS_soft_queue_list> {
      using type = QR;   ///< the sub-type
   };

} // namespace ocs::tom
