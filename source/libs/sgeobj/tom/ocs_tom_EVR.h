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
 * This code was generated from file source/libs/sgeobj/json/EVR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Event Master Request: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of EVR, Event Master Request
 *
 * The enumeration is the type of every field number of EVR, so a parameter
 * of this type takes the field numbers of EVR only.
 */
enum EVR_Field {
   /**
    * @brief Operation
    *
    * Which request this is: `EVR_ADD_EVC`, `EVR_MOD_EVC`, `EVR_DEL_EVC` or `EVR_ADD_EVENT`. See `sge_event.h`.
    */
   EVR_operation = EVR_LOWERBOUND,
   /**
    * @brief Timestamp
    *
    * When the request was posted, so the event master can order and age requests.
    */
   EVR_timestamp,
   /**
    * @brief Event Client Id
    *
    * Which event client the request is about.
    */
   EVR_event_client_id,
   /**
    * @brief Event Number
    *
    * The client's event serial the request refers to.
    */
   EVR_event_number,
   /**
    * @brief Session
    *
    * The session the request belongs to, so a client sees its own changes.
    */
   EVR_session,
   /**
    * @brief Event Client
    *
    * The event client object (`EV`), for the register and modify operations.
    */
   EVR_event_client,
   /**
    * @brief Events
    *
    * The events to deliver (`ET`), for `EVR_ADD_EVENT`.
    */
   EVR_event_list,
};

namespace ocs::tom {

   struct ET;
   struct EV;

   /**
    * @brief Event Master Request
    *
    * One request queued for the event master thread.
    * Every change to the event master - a client registering, changing its subscription, leaving, or an event being added - is posted as one of these rather than applied directly, so the callers never block on the event master's lock.
    */
   struct EVR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {EVR_operation, "EVR_operation", FieldType::Uint32, Flag::None},
         {EVR_timestamp, "EVR_timestamp", FieldType::Uint64, Flag::None},
         {EVR_event_client_id, "EVR_event_client_id", FieldType::Uint32, Flag::None},
         {EVR_event_number, "EVR_event_number", FieldType::Uint32, Flag::None},
         {EVR_session, "EVR_session", FieldType::String, Flag::None},
         {EVR_event_client, "EVR_event_client", FieldType::Object, Flag::None},
         {EVR_event_list, "EVR_event_list", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of EVR belongs to EVR */
   template <>
   struct TypeOf<EVR_Field> {
      using type = EVR;   ///< the object type
   };

   /** @brief EVR_event_client holds elements of EV */
   template <>
   struct SubType<EVR_event_client> {
      using type = EV;   ///< the sub-type
   };

   /** @brief EVR_event_list holds elements of ET */
   template <>
   struct SubType<EVR_event_list> {
      using type = ET;   ///< the sub-type
   };

} // namespace ocs::tom
