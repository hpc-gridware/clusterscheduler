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
 * This code was generated from file source/libs/sgeobj/json/PA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Path Alias: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of PA, Path Alias
 *
 * The enumeration is the type of every field number of PA, so a parameter
 * of this type takes the field numbers of PA only.
 */
enum PA_Field {
   /**
    * @brief Original Path
    *
    * The original path before applying aliasing.
    */
   PA_origin = PA_LOWERBOUND,
   /**
    * @brief Submit Host
    *
    * The host from which the job was submitted.
    */
   PA_submit_host,
   /**
    * @brief Exec Host
    *
    * The destination execution host.
    */
   PA_exec_host,
   /**
    * @brief Translation
    *
    * The path after applying path aliasing.
    */
   PA_translation,
};

namespace ocs::tom {

   /**
    * @brief Path Alias
    *
    * Object holding information necessary to realize path aliasing.
    * Path aliasing is configured in $SGE_ROOT/$SGE_CELL/sge_aliases and/or $HOME/.sge_aliases
    * See man page sge_aliases.5
    */
   struct PA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PA_origin, "PA_origin", FieldType::String, Flag::PrimaryKey | Flag::Spool},
         {PA_submit_host, "PA_submit_host", FieldType::Host, Flag::Spool},
         {PA_exec_host, "PA_exec_host", FieldType::Host, Flag::Spool},
         {PA_translation, "PA_translation", FieldType::String, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PA belongs to PA */
   template <>
   struct TypeOf<PA_Field> {
      using type = PA;   ///< the object type
   };

} // namespace ocs::tom
