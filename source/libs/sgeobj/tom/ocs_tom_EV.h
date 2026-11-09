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
 * This code was generated from file source/libs/sgeobj/json/EV.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Event Client: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of EV, Event Client
 *
 * The enumeration is the type of every field number of EV, so a parameter
 * of this type takes the field numbers of EV only.
 */
enum EV_Field {
   /**
    * @brief event client id
    *
    * Unique id requested by client or given by qmaster
    */
   EV_id = EV_LOWERBOUND,
   /**
    * @brief event client name
    *
    * name of event client (non unique)
    */
   EV_name,
   /**
    * @brief event client address: host name
    *
    * Commlib address: The host name where the event client is running
    */
   EV_host,
   /**
    * @brief event client address: commproc
    *
    * Commlib address: The process name, e.g. qsub for the qsub -sync y event client
    */
   EV_commproc,
   /**
    * @brief event client address: commid
    *
    * Commlib address: A unique id assigned by the sge_qmaster commlib
    */
   EV_commid,
   /**
    * @brief user id
    *
    * The user id of the user who started the event client.
    */
   EV_uid,
   /**
    * @brief event delivery interval
    *
    * The time interval in seconds in which an event package is delivered to the client.
    */
   EV_d_time,
   /**
    * @brief subscribed events
    *
    * a list of subscribed events
    */
   EV_subscribed,
   /**
    * @brief event client changed?
    *
    * true, if any configuration information of the event client has been changed.
    * Requires then updating the event client information in the event master
    */
   EV_changed,
   /**
    * @brief busy handling
    *
    * Defines how the event master shall deal with busy event clients.
    */
   EV_busy_handling,
   /**
    * @brief session key
    *
    * Session key used tfor filtering subscribed events, used with job submission via the DRMAA interface
    */
   EV_session,
   /**
    * @brief last heard from
    *
    * Timestamp (microseconds since epoch) of the last communication between event client and event master.
    */
   EV_last_heard_from,
   /**
    * @brief last send time
    *
    * Timestamp (microseconds since epoch) when the last event package was sent to the event client.
    */
   EV_last_send_time,
   /**
    * @brief next send time
    *
    * Timestamp (microseconds since epoch) when the next event package shall be sent to the event client.
    */
   EV_next_send_time,
   /**
    * @brief next event serial number
    *
    * Serial number of the next event which will be sent to the event client.
    */
   EV_next_number,
   /**
    * @brief busy
    *
    * true if the event client is considered busy, else false.
    * no events will be sent to a busy client
    */
   EV_busy,
   /**
    * @brief events to be sent
    *
    * List of events which will next be delivered to the event client.
    */
   EV_events,
   /**
    * @brief subscription array
    *
    * Subscription information used in event master only.
    */
   EV_sub_array,
   /**
    * @brief event client state
    *
    * State of the event client, e.g. connected, closing, terminated.
    */
   EV_state,
   /**
    * @brief update function
    *
    * Pointer to an update function used for updating internal event clients (threads in sge_qmaster).
    */
   EV_update_function,
   /**
    * @brief update function argument
    *
    * Pointer that is passed to the update_function when it is called.
    * The pointer can be specified during mirror/evc initialize.
    */
   EV_update_function_arg,
};

namespace ocs::tom {

   struct ET;
   struct EVS;

   /**
    * @brief Event Client
    *
    * An object of the event client type represents one event client.
    * For more information about the event client interface see the documentation in
    * source/libs/evc/sge_event_client.cc
    */
   struct EV {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {EV_id, "EV_id", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Hash},
         {EV_name, "EV_name", FieldType::String, Flag::None},
         {EV_host, "EV_host", FieldType::Host, Flag::None},
         {EV_commproc, "EV_commproc", FieldType::String, Flag::None},
         {EV_commid, "EV_commid", FieldType::Uint32, Flag::None},
         {EV_uid, "EV_uid", FieldType::Uint32, Flag::None},
         {EV_d_time, "EV_d_time", FieldType::Uint32, Flag::None},
         {EV_subscribed, "EV_subscribed", FieldType::List, Flag::None},
         {EV_changed, "EV_changed", FieldType::Bool, Flag::None},
         {EV_busy_handling, "EV_busy_handling", FieldType::Uint32, Flag::None},
         {EV_session, "EV_session", FieldType::String, Flag::None},
         {EV_last_heard_from, "EV_last_heard_from", FieldType::Uint64, Flag::None},
         {EV_last_send_time, "EV_last_send_time", FieldType::Uint64, Flag::None},
         {EV_next_send_time, "EV_next_send_time", FieldType::Uint64, Flag::None},
         {EV_next_number, "EV_next_number", FieldType::Uint32, Flag::None},
         {EV_busy, "EV_busy", FieldType::Uint32, Flag::None},
         {EV_events, "EV_events", FieldType::List, Flag::None},
         {EV_sub_array, "EV_sub_array", FieldType::Ref, Flag::NoTransfer},
         {EV_state, "EV_state", FieldType::Uint32, Flag::None},
         {EV_update_function, "EV_update_function", FieldType::Ref, Flag::NoTransfer},
         {EV_update_function_arg, "EV_update_function_arg", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of EV belongs to EV */
   template <>
   struct TypeOf<EV_Field> {
      using type = EV;   ///< the object type
   };

   /** @brief EV_subscribed holds elements of EVS */
   template <>
   struct SubType<EV_subscribed> {
      using type = EVS;   ///< the sub-type
   };

   /** @brief EV_events holds elements of ET */
   template <>
   struct SubType<EV_events> {
      using type = ET;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of EV for the list library: a synonym for ocs::tom::EV::descriptor */
inline constexpr const lDescr *EV_Type = ocs::tom::EV::descriptor.e;
