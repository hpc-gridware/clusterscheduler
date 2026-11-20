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
 * This code was generated from file source/libs/sgeobj/json/RQRL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule Limit: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RQRL, Resource Quota Rule Limit
 *
 * The enumeration is the type of every field number of RQRL, so a parameter
 * of this type takes the field numbers of RQRL only.
 */
enum RQRL_Field {
   /**
    * @brief Resource Name
    *
    * The complex attribute being limited, e.g. `slots`.
    */
   RQRL_name = RQRL_LOWERBOUND,
   /**
    * @brief Configured Value
    *
    * The limit as configured, still as text. For a dynamic limit this is the formula rather than a number.
    */
   RQRL_value,
   /**
    * @brief Value Type
    *
    * The complex attribute's type, copied from `CE_valtype`, which decides how RQRL_value is parsed.
    */
   RQRL_type,
   /**
    * @brief Evaluated Value
    *
    * The limit as a number. For a static limit this is RQRL_value parsed; for a dynamic one it is the formula evaluated against the host currently being considered.
    */
   RQRL_dvalue,
   /**
    * @brief Usage
    *
    * What is booked against this limit right now (`RUE`), one entry per name when the filter expands.
    */
   RQRL_usage,
   /**
    * @brief Dynamic
    *
    * The limit is a formula over host complexes rather than a constant, so RQRL_dvalue has to be recomputed per host instead of read once.
    */
   RQRL_dynamic,
};

namespace ocs::tom {

   struct RUE;

   /**
    * @brief Resource Quota Rule Limit
    *
    * One resource a rule limits, the configured ceiling, and what is currently booked against it.
    */
   struct RQRL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RQRL_name, "RQRL_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {RQRL_value, "RQRL_value", FieldType::String, Flag::Spool},
         {RQRL_type, "RQRL_type", FieldType::Uint32, Flag::Spool},
         {RQRL_dvalue, "RQRL_dvalue", FieldType::Double, Flag::Spool},
         {RQRL_usage, "RQRL_usage", FieldType::List, Flag::None},
         {RQRL_dynamic, "RQRL_dynamic", FieldType::Bool, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RQRL belongs to RQRL */
   template <>
   struct TypeOf<RQRL_Field> {
      using type = RQRL;   ///< the object type
   };

   /** @brief RQRL_usage holds elements of RUE */
   template <>
   struct SubType<RQRL_usage> {
      using type = RUE;   ///< the sub-type
   };

} // namespace ocs::tom
