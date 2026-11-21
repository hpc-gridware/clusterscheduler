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
 * This code was generated from file source/libs/sgeobj/json/ID.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Id for Action: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of ID, Job Id for Action
 *
 * The enumeration is the type of every field number of ID, so a parameter
 * of this type takes the field numbers of ID only.
 */
enum ID_Field {
   /**
    * @brief Id String
    *
    * Id as string, e.g. job name or job number.
    */
   ID_str = ID_LOWERBOUND,
   /**
    * @brief Array Task Structure
    *
    * Used to transport array task specific information, e.g. status, from qalter to sge_qmaster.
    */
   ID_ja_structure,
   /**
    * @brief Action
    *
    * Used by qconf to transport specific actions to sge_qmaster, e.g. start thread, stop thread, clear queue.
    */
   ID_action,
   /**
    * @brief Force
    *
    * Used to express that a certain action shall be forced, e.g. from qdel -f option.
    */
   ID_force,
   /**
    * @brief User List
    *
    * Restricts actions to specific users, e.g. from qdel -u option.
    */
   ID_user_list,
};

namespace ocs::tom {

   struct RN;
   struct ST;

   /**
    * @brief Job Id for Action
    *
    * The ID is used to select specific jobs, e.g. by job number or by job name.
    * It is used by client commands like qalter, qconf, qdel, qrdel, ...
    * @todo the same goal could be achieved by using a lWhere condition, why do we have this type in the first place?
    */
   struct ID {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ID_str, "ID_str", FieldType::String, Flag::None},
         {ID_ja_structure, "ID_ja_structure", FieldType::List, Flag::None},
         {ID_action, "ID_action", FieldType::Uint32, Flag::None},
         {ID_force, "ID_force", FieldType::Uint32, Flag::None},
         {ID_user_list, "ID_user_list", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ID belongs to ID */
   template <>
   struct TypeOf<ID_Field> {
      using type = ID;   ///< the object type
   };

   /** @brief ID_ja_structure holds elements of RN */
   template <>
   struct SubType<ID_ja_structure> {
      using type = RN;   ///< the sub-type
   };

   /** @brief ID_user_list holds elements of ST */
   template <>
   struct SubType<ID_user_list> {
      using type = ST;   ///< the sub-type
   };

} // namespace ocs::tom
