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
 * This code was generated from file source/libs/sgeobj/json/LS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Load Sensor: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of LS, Load Sensor
 *
 * The enumeration is the type of every field number of LS, so a parameter
 * of this type takes the field numbers of LS only.
 */
enum LS_Field {
   /**
    * @brief Name
    *
    * Name of this load sensor, no hashing, we only have few loadsensors/host.
    * Currently used names:
    *   - extern
    *   - IDLE_LOADSENSOR_NAME = qidle
    *   - GNU_LOADSENSOR_NAME = qloadsensor
    * @todo what's the purpose of the name?
    */
   LS_name = LS_LOWERBOUND,
   /**
    * @brief Command
    *
    * Absolute path of the load sensor script / binary.
    */
   LS_command,
   /**
    * @brief Pid
    *
    * Pid of the load sensor process.
    */
   LS_pid,
   /**
    * @brief Stdin File Handle
    *
    * stdin filehandle to the loadsensor process (type FILE *)
    */
   LS_in,
   /**
    * @brief Stdout File Handle
    *
    * stdout filehandle of the loadsensor process (type FILE *)
    */
   LS_out,
   /**
    * @brief Stderr File Handle
    *
    * stderr filehandle of the loadsensor process (type FILE *)
    */
   LS_err,
   /**
    * @brief Has to restart
    *
    * Do we have to restart the load sensor script?
    */
   LS_has_to_restart,
   /**
    * @brief Tag
    *
    * Tag for internal use (@todo 1 means it is running, 0 it isn't running? Verify.)
    */
   LS_tag,
   /**
    * @brief Incomplete Values
    *
    * Current values we got from the load sensor script.
    */
   LS_incomplete,
   /**
    * @brief Complete Values
    *
    * Last complete set of the load sensor's values.
    */
   LS_complete,
   /**
    * @brief Last Modification Time
    *
    * Last modification time of the load sensor script.
    * If the script is modified, then the load sensor will be re-started.
    */
   LS_last_mod,
};

namespace ocs::tom {

   struct LR;

   /**
    * @brief Load Sensor
    *
    * The attributes of this element show the state of a load sensor.
    * A list of these elements is used in the execd.
    */
   struct LS {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {LS_name, "LS_name", FieldType::String, Flag::None},
         {LS_command, "LS_command", FieldType::String, Flag::None},
         {LS_pid, "LS_pid", FieldType::String, Flag::None},
         {LS_in, "LS_in", FieldType::Ref, Flag::NoTransfer},
         {LS_out, "LS_out", FieldType::Ref, Flag::NoTransfer},
         {LS_err, "LS_err", FieldType::Ref, Flag::NoTransfer},
         {LS_has_to_restart, "LS_has_to_restart", FieldType::Bool, Flag::None},
         {LS_tag, "LS_tag", FieldType::Uint32, Flag::None},
         {LS_incomplete, "LS_incomplete", FieldType::List, Flag::None},
         {LS_complete, "LS_complete", FieldType::List, Flag::None},
         {LS_last_mod, "LS_last_mod", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of LS belongs to LS */
   template <>
   struct TypeOf<LS_Field> {
      using type = LS;   ///< the object type
   };

   /** @brief LS_incomplete holds elements of LR */
   template <>
   struct SubType<LS_incomplete> {
      using type = LR;   ///< the sub-type
   };

   /** @brief LS_complete holds elements of LR */
   template <>
   struct SubType<LS_complete> {
      using type = LR;   ///< the sub-type
   };

} // namespace ocs::tom
