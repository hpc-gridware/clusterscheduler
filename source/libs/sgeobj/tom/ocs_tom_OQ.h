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
 * This code was generated from file source/libs/sgeobj/json/OQ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue Info in an Order: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of OQ, Queue Info in an Order
 *
 * The enumeration is the type of every field number of OQ, so a parameter
 * of this type takes the field numbers of OQ only.
 */
enum OQ_Field {
   /**
    * @brief Slots
    *
    * The number of slots a job occupies in the queue / queue instance.
    */
   OQ_slots = OQ_LOWERBOUND,
   /**
    * @brief Qinstance Name
    *
    * The full queue instance name (cluster queue name + @ + host name).
    */
   OQ_dest_queue,
   /**
    * @brief Qinstance Version
    *
    * Version of the queue instance. Is used to detect if the queue has been modified during scheduling.
    * In this case the scheduling result is skipped.
    */
   OQ_dest_version,
   /**
    * @brief Total Tickets
    *
    * Total SGEEE tickets for slots.
    */
   OQ_ticket,
   /**
    * @brief Override Tickets
    *
    * Total SGEEE override tickets.
    */
   OQ_oticket,
   /**
    * @brief Functional Tickets
    *
    * Total SGEEE functional tickets.
    */
   OQ_fticket,
   /**
    * @brief Sharetree Tickets
    *
    * Total SGEEE sharetree tickets.
    */
   OQ_sticket,
   /**
    * @brief Binding that should be used
    *
    * One entry for sequential jobs or multiple entries for PE jobs in case of host/task specific binding
    */
   OQ_binding_to_use,
   /**
    * @brief Granted RSMAP IDs
    *
    * List of RSMAP IDs granted to this job / specific tasks of a parallel job,
    * which are running on the specific queue instance / host described by this object.
    */
   OQ_granted_rsmaps,
};

namespace ocs::tom {

   struct RESL;
   struct ST;

   /**
    * @brief Queue Info in an Order
    *
    * Job start orders and also orders for reprioritization contain per queue information.
    * One or more objects of type OrderQueue are contained in the queuelist attribute of an order.
    * This information will be copied to the JAT_granted_destin_identifier_list of a running
    * job (array task).
    */
   struct OQ {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {OQ_slots, "OQ_slots", FieldType::Uint32, Flag::None},
         {OQ_dest_queue, "OQ_dest_queue", FieldType::String, Flag::None},
         {OQ_dest_version, "OQ_dest_version", FieldType::Uint32, Flag::None},
         {OQ_ticket, "OQ_ticket", FieldType::Double, Flag::None},
         {OQ_oticket, "OQ_oticket", FieldType::Double, Flag::None},
         {OQ_fticket, "OQ_fticket", FieldType::Double, Flag::None},
         {OQ_sticket, "OQ_sticket", FieldType::Double, Flag::None},
         {OQ_binding_to_use, "OQ_binding_to_use", FieldType::List, Flag::None},
         {OQ_granted_rsmaps, "OQ_granted_rsmaps", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of OQ belongs to OQ */
   template <>
   struct TypeOf<OQ_Field> {
      using type = OQ;   ///< the object type
   };

   /** @brief OQ_binding_to_use holds elements of ST */
   template <>
   struct SubType<OQ_binding_to_use> {
      using type = ST;   ///< the sub-type
   };

   /** @brief OQ_granted_rsmaps holds elements of RESL */
   template <>
   struct SubType<OQ_granted_rsmaps> {
      using type = RESL;   ///< the sub-type
   };

} // namespace ocs::tom
