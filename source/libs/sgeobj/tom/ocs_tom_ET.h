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
 * This code was generated from file source/libs/sgeobj/json/ET.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Event Type: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of ET, Event Type
 *
 * The enumeration is the type of every field number of ET, so a parameter
 * of this type takes the field numbers of ET only.
 */
enum ET_Field {
   /**
    * @brief unique ID for an event within qmaster.
    *
    * A unique number. IDs are always increasing for generated events. No wrap around.
    * It is used in qmaster to synchronize activities between transaction processing happening
    * in different threads. Session table contains the biggest ID of the last RW request that
    * was handled in any thread for that session.
    */
   ET_unique_id = ET_LOWERBOUND,
   /**
    * @brief serial number
    *
    * A unique serial number.
    * It is used to acknowledge receipt of a list of events
    * (up to a specific event represented by the serial number)
    * and also to detect inconsistencies in the event protocol, e.g. a missing event.
    */
   ET_number,
   /**
    * @brief event generation time
    *
    * Date and time (microseconds since epoch) when the event was generated.
    */
   ET_timestamp,
   /**
    * @brief event type
    *
    * The type of the event. Values of enumeration type ev_event (libs/sgeobj/sge_event.h), e.g.
    *   - sgeE_ADMIN_HOST_LIST
    *   - sgeE_ADMIN_HOST_ADD
    *   - sgeE_ADMIN_HOST_DEL
    *   - sgeE_ADMIN_HOST_MOD
    *   - sgeE_CALENDAR_LIST
    *   - ...
    */
   ET_type,
   /**
    * @brief first integer key
    *
    * An int key for use by a specific event type, e.g. job id in a sgeE_JOB_ADD event.
    */
   ET_intkey,
   /**
    * @brief second integer key
    *
    * A second int key for use by a specific event type, e.g. ja_task id in a sgeE_JATASK_MOD event.
    */
   ET_intkey2,
   /**
    * @brief first string key
    *
    * A string key for use by a specific event type, e.g. the complex variable name in a sgeE_CENTRY_DEL event.
    */
   ET_strkey,
   /**
    * @brief second string key
    *
    * A second string key for use by a specific event type, e.g.
    * in a sgeE_QINSTANCE_ADD event the first key is the cluster queue name, the second key is the host name
    */
   ET_strkey2,
   /**
    * @brief new version of the data
    *
    * A list containing the new object(s). The list type depends on the event type.
    * @todo we could split this into two fields, one for lists and one for individual objects
    */
   ET_new_version,
};

namespace ocs::tom {

   /**
    * @brief Event Type
    *
    * An object of this type represents a single event.
    * An event has a key and a value
    * The key can consist of multiple integer or string fields, e.g.
    * job id, array task id, pe task id
    * The value represents the new version of data.
    * It can be a list of objects or an individual object
    * An event has a unique serial number.
    */
   struct ET {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ET_unique_id, "ET_unique_id", FieldType::Uint64, Flag::PrimaryKey},
         {ET_number, "ET_number", FieldType::Uint32, Flag::None},
         {ET_timestamp, "ET_timestamp", FieldType::Uint64, Flag::None},
         {ET_type, "ET_type", FieldType::Uint32, Flag::None},
         {ET_intkey, "ET_intkey", FieldType::Uint32, Flag::None},
         {ET_intkey2, "ET_intkey2", FieldType::Uint32, Flag::None},
         {ET_strkey, "ET_strkey", FieldType::String, Flag::None},
         {ET_strkey2, "ET_strkey2", FieldType::String, Flag::None},
         {ET_new_version, "ET_new_version", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ET belongs to ET */
   template <>
   struct TypeOf<ET_Field> {
      using type = ET;   ///< the object type
   };

} // namespace ocs::tom
