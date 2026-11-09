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
 * This code was generated from file source/libs/sgeobj/json/PERM.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Object used to request given permissions of user and host.: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of PERM, Object used to request given permissions of user and host.
 *
 * The enumeration is the type of every field number of PERM, so a parameter
 * of this type takes the field numbers of PERM only.
 */
enum PERM_Field {
   /**
    * @brief true if manager
    *
    * true if user has manager role
    */
   PERM_is_manager = PERM_LOWERBOUND,
   /**
    * @brief true if operator
    *
    * true if user has operator role
    */
   PERM_is_operator,
   /**
    * @brief true if admin host
    *
    * true if the specified host is an admin host
    */
   PERM_is_admin_host,
   /**
    * @brief true if submit host
    *
    * true if the specified host is a submit host
    */
   PERM_is_submit_host,
   /**
    * @brief hostname
    *
    * name of the host with the specified permissions
    */
   PERM_host,
   /**
    * @brief username
    *
    * username with the specified permissions
    */
   PERM_username,
};

namespace ocs::tom {

   /**
    * @brief Object used to request given permissions of user and host.
    *
    * Object used to request given permissions of user and host.
    */
   struct PERM {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PERM_is_manager, "PERM_is_manager", FieldType::Bool, Flag::None},
         {PERM_is_operator, "PERM_is_operator", FieldType::Bool, Flag::None},
         {PERM_is_admin_host, "PERM_is_admin_host", FieldType::Bool, Flag::None},
         {PERM_is_submit_host, "PERM_is_submit_host", FieldType::Bool, Flag::None},
         {PERM_host, "PERM_host", FieldType::Host, Flag::None},
         {PERM_username, "PERM_username", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PERM belongs to PERM */
   template <>
   struct TypeOf<PERM_Field> {
      using type = PERM;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of PERM for the list library: a synonym for ocs::tom::PERM::descriptor */
inline constexpr const lDescr *PERM_Type = ocs::tom::PERM::descriptor.e;
