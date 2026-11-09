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
 * This code was generated from file source/libs/sgeobj/json/LR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Load Report: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of LR, Load Report
 *
 * The enumeration is the type of every field number of LR, so a parameter
 * of this type takes the field numbers of LR only.
 */
enum LR_Field {
   /**
    * @brief Load Variable Name
    *
    * Name of the load variable. In order for the load value to be processed in sge_qmaster
    * a complex variable with this name must have been configured.
    */
   LR_name = LR_LOWERBOUND,
   /**
    * @brief Load Variable Value
    *
    * Value of the variable on a specific host.
    */
   LR_value,
   /**
    * @brief Is Global
    *
    * Specifies if it is a global load variable.
    * 1 means that it is a global load value (host is global in this case),
    * 0 means that it is a host specific load value.
    * @todo: make it a boolean
    */
   LR_global,
   /**
    * @brief Is Static
    *
    * Specifies if it is a static load variable.
    * Static load variables represent seldomly changing variables, e.g. arch, n_proc, mem_total.
    * 0 means a non static load value
    * 1 means a static load value
    * 2 is a special internal value: remove the load value
    */
   LR_is_static,
   /**
    * @brief Host Name
    *
    * Name of the host on which the load value is valid. Specific host name or keyword global for global load values.
    */
   LR_host,
};

namespace ocs::tom {

   /**
    * @brief Load Report
    *
    * A LoadReport object represents the value for a single load variable on a specific host.
    */
   struct LR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {LR_name, "LR_name", FieldType::String, Flag::Hash},
         {LR_value, "LR_value", FieldType::String, Flag::None},
         {LR_global, "LR_global", FieldType::Uint32, Flag::None},
         {LR_is_static, "LR_is_static", FieldType::Uint32, Flag::None},
         {LR_host, "LR_host", FieldType::Host, Flag::Hash},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of LR belongs to LR */
   template <>
   struct TypeOf<LR_Field> {
      using type = LR;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of LR for the list library: a synonym for ocs::tom::LR::descriptor */
inline constexpr const lDescr *LR_Type = ocs::tom::LR::descriptor.e;
