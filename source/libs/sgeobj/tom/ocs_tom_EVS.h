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
 * This code was generated from file source/libs/sgeobj/json/EVS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Event Subscription: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of EVS, Event Subscription
 *
 * The enumeration is the type of every field number of EVS, so a parameter
 * of this type takes the field numbers of EVS only.
 */
enum EVS_Field {
   /**
    * @brief Event Id
    *
    * The Id of the specific event from enumeration type ev_event (libs/sgeobj/sge_event.h), e.g.
    *   - sgeE_ADMIN_HOST_LIST
    *   - sgeE_ADMIN_HOST_ADD
    *   - sgeE_ADMIN_HOST_DEL
    *   - sgeE_ADMIN_HOST_MOD
    *   - sgeE_CALENDAR_LIST
    *   - ...
    */
   EVS_id = EVS_LOWERBOUND,
   /**
    * @brief Flush this Event
    *
    * Specifies if flushing of the event is enabled. This means that flushing of events is triggered
    * if this event will be delivered to a specific event client.
    * Flushing of event data means that the event are sent to the client earlier than forseen
    * by the event client's event delivery interval.
    */
   EVS_flush,
   /**
    * @brief Flushing Interval
    *
    * Flushing interval in seconds.
    * Events will be sent to the event client not later than current time + interval.
    */
   EVS_interval,
   /**
    * @brief Attribute Filter
    *
    * Enumeration defining which attributes of an object will be sent to the event client (reduced objects).
    * We can for example configure: We are only interested in job id, job name and job owner.
    */
   EVS_what,
   /**
    * @brief Object Filter
    *
    * Condition filtering objects to be sent.
    * We can for example configure: Send only events for jobs of user xyz.
    */
   EVS_where,
};

namespace ocs::tom {

   /**
    * @brief Event Subscription
    *
    * An object of the EventSubscription type specifies if a certain event is subscribed
    * and additional information for flushing and filtering
    * see also the documentation in source/libs/evc/sge_event_client.cc
    */
   struct EVS {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {EVS_id, "EVS_id", FieldType::Uint32, Flag::None},
         {EVS_flush, "EVS_flush", FieldType::Bool, Flag::None},
         {EVS_interval, "EVS_interval", FieldType::Uint32, Flag::None},
         {EVS_what, "EVS_what", FieldType::Object, Flag::None},
         {EVS_where, "EVS_where", FieldType::Object, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of EVS belongs to EVS */
   template <>
   struct TypeOf<EVS_Field> {
      using type = EVS;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of EVS for the list library: a synonym for ocs::tom::EVS::descriptor */
inline constexpr const lDescr *EVS_Type = ocs::tom::EVS::descriptor.e;
