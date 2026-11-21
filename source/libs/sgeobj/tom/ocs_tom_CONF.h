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
 * This code was generated from file source/libs/sgeobj/json/CONF.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Host Configuration: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of CONF, Host Configuration
 *
 * The enumeration is the type of every field number of CONF, so a parameter
 * of this type takes the field numbers of CONF only.
 */
enum CONF_Field {
   /**
    * @brief Host Name
    *
    * Name of the host the configuration object refers to, or global for the global configuration.
    */
   CONF_name = CONF_LOWERBOUND,
   /**
    * @brief Configuration Version
    *
    * Each configuration object has a version number which is increased with every change.
    */
   CONF_version,
   /**
    * @brief Configuration Entries
    *
    * A configuration consists of multiple configuration entries of CF.
    */
   CONF_entries,
};

namespace ocs::tom {

   struct CF;

   /**
    * @brief Host Configuration
    *
    * Contains configuration options for hosts (execution hosts but also a global configuration for the sge_qmaster).
    * Host specific configurations inherit values from the global configuration.
    * @todo there is an overlap with the exec host type (EH), can this be unified?
    */
   struct CONF {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CONF_name, "CONF_name", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {CONF_version, "CONF_version", FieldType::Uint32, Flag::Spool},
         {CONF_entries, "CONF_entries", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CONF belongs to CONF */
   template <>
   struct TypeOf<CONF_Field> {
      using type = CONF;   ///< the object type
   };

   /** @brief CONF_entries holds elements of CF */
   template <>
   struct SubType<CONF_entries> {
      using type = CF;   ///< the sub-type
   };

} // namespace ocs::tom
