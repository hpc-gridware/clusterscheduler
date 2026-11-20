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
 * This code was generated from file source/libs/sgeobj/json/CF.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Configuration Element: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CF, Configuration Element
 *
 * The enumeration is the type of every field number of CF, so a parameter
 * of this type takes the field numbers of CF only.
 */
enum CF_Field {
   /**
    * @brief Configuration Name
    *
    * Name of the configuration element (the left column of qconf -sconf).
    */
   CF_name = CF_LOWERBOUND,
   /**
    * @brief Configuration Value
    *
    * The value of a configuration element (the right column of qconf -sconf).
    */
   CF_value,
   /**
    * @brief Configuration Sublist
    *
    * Recursive Sublist of CF. @todo is it still required? It is only used in libs/gdi/sge_qtcsh.cc.
    */
   CF_sublist,
   /**
    * @brief Local Configuration
    *
    * Is it a local configuration (true) or the global configuration (false).
    */
   CF_local,
};

namespace ocs::tom {

   /**
    * @brief Configuration Element
    *
    * A configuration element contains one line of a configuration, see qconf -sconf, e.g.
    * - execd_spool_dir
    * - mailer
    * - xterm
    * - load_sensor
    * - ...
    */
   struct CF {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CF_name, "CF_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {CF_value, "CF_value", FieldType::String, Flag::SpoolSublist},
         {CF_sublist, "CF_sublist", FieldType::List, Flag::None},
         {CF_local, "CF_local", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CF belongs to CF */
   template <>
   struct TypeOf<CF_Field> {
      using type = CF;   ///< the object type
   };

} // namespace ocs::tom
