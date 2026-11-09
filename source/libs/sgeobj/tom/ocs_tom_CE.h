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
 * This code was generated from file source/libs/sgeobj/json/CE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Complex Entry: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CE, Complex Entry
 *
 * The enumeration is the type of every field number of CE, so a parameter
 * of this type takes the field numbers of CE only.
 */
enum CE_Field {
   /**
    * @brief Full Name
    *
    * Full name of a complex variable.
    */
   CE_name = CE_LOWERBOUND,
   /**
    * @brief Shortcut Name
    *
    * Shortcut for the complex variable name which can be used as an alternative to the name,
    * e.g. in job submission with qsub or when querying resources with qstat -F.
    */
   CE_shortcut,
   /**
    * @brief Variable Type
    *
    * Type of the complex variable defined in common/basis_types.h, e.g.
    *   - TYPE_INT
    *   - TYPE_STR
    *   - TYPE_TIM
    *   - TYPE_MEM
    *   - ...
    * @todo instead of defines, should we better use an enum?
    */
   CE_valtype,
   /**
    * @brief String Value
    *
    * Value of the complex variable as string, from old docs: non overwritten value.
    */
   CE_stringval,
   /**
    * @brief Double Value
    *
    * Value of the complex variable as double, from old docs: parsed CE_stringval
    */
   CE_doubleval,
   /**
    * @brief Relational Operator
    *
    * Relational operator used in comparison of complex variables (e.g. against requests).
    * Defined in libs/sgeobj/sge_centry.h, e.g.
    * CMPLXEQ_OP
    * CMPLXGE_OP
    * CMPLXGT_OP
    * ...
    */
   CE_relop,
   /**
    * @brief Consumable Flag
    *
    * Defines if a complex variable is consumable and if it is a per job or per slot consumable.
    * Defined in libs/sgeobj/sge_centry.h, possible values are
    * CONSUMABLE_NO
    * CONSUMABLE_YES
    * CONSUMABLE_JOB
    * CONSUMABLE_HOST
    */
   CE_consumable,
   /**
    * @brief Default Value
    *
    * Default value (default request) as string.
    */
   CE_defaultval,
   /**
    * @brief Monitoring Facility
    *
    * @todo add description
    */
   CE_dominant,
   /**
    * @brief Per Job String Value
    *
    * Per job string value, @todo add more information
    */
   CE_pj_stringval,
   /**
    * @brief Per Job Double Value
    *
    * Per job double values, parsed from CE_stringval (?)
    */
   CE_pj_doubleval,
   /**
    * @brief Per Job Monitoring Facility
    *
    * @todo add description
    */
   CE_pj_dominant,
   /**
    * @brief Requestable
    *
    * Whether a job may ask for this complex, and whether asking is mandatory: `NO`, `YES` or `FORCED`.
    */
   CE_requestable,
   /**
    * @brief Variable Is Tagged
    *
    * Used for tagging variables, e.g. during the scheduling process.
    */
   CE_tagged,
   /**
    * @brief Urgency Weighting Factor
    *
    * Static Urgency Weighting Factor.
    */
   CE_urgency_weight,
   /**
    * @brief Resource Map List
    *
    * on host level only: RSMAP id list
    */
   CE_resource_map_list,
};

namespace ocs::tom {

   struct RESL;

   /**
    * @brief Complex Entry
    *
    * A complex entry contains a complex variable, both its definition as well as a possible current value
    * A complex variable is used for defining all types of attributes in Cluster Scheduler
    * as well as defining fixed values of resources and the capacity of consumable resources.
    * @todo should be we better split definition and values into two objects?
    */
   struct CE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CE_name, "CE_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool | Flag::SpoolSublist},
         {CE_shortcut, "CE_shortcut", FieldType::String, Flag::Unique | Flag::Hash | Flag::Spool},
         {CE_valtype, "CE_valtype", FieldType::Uint32, Flag::Spool},
         {CE_stringval, "CE_stringval", FieldType::String, Flag::Spool | Flag::SpoolSublist},
         {CE_doubleval, "CE_doubleval", FieldType::Double, Flag::None},
         {CE_relop, "CE_relop", FieldType::Uint32, Flag::Spool},
         {CE_consumable, "CE_consumable", FieldType::Uint32, Flag::Spool},
         {CE_defaultval, "CE_defaultval", FieldType::String, Flag::Spool},
         {CE_dominant, "CE_dominant", FieldType::Uint32, Flag::None},
         {CE_pj_stringval, "CE_pj_stringval", FieldType::String, Flag::None},
         {CE_pj_doubleval, "CE_pj_doubleval", FieldType::Double, Flag::None},
         {CE_pj_dominant, "CE_pj_dominant", FieldType::Uint32, Flag::None},
         {CE_requestable, "CE_requestable", FieldType::Uint32, Flag::Hash | Flag::Spool},
         {CE_tagged, "CE_tagged", FieldType::Uint32, Flag::None},
         {CE_urgency_weight, "CE_urgency_weight", FieldType::String, Flag::Spool},
         {CE_resource_map_list, "CE_resource_map_list", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CE belongs to CE */
   template <>
   struct TypeOf<CE_Field> {
      using type = CE;   ///< the object type
   };

   /** @brief CE_resource_map_list holds elements of RESL */
   template <>
   struct SubType<CE_resource_map_list> {
      using type = RESL;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of CE for the list library: a synonym for ocs::tom::CE::descriptor */
inline constexpr const lDescr *CE_Type = ocs::tom::CE::descriptor.e;
