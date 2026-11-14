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
 * This code was generated from file source/libs/sgeobj/json/SPA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Commandline Argument: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SPA, Commandline Argument
 *
 * The enumeration is the type of every field number of SPA, so a parameter
 * of this type takes the field numbers of SPA only.
 */
enum SPA_Field {
   /**
    * @brief Option Number
    *
    * Defines which option was parsed.
    * The option numbers are defined as enum in source/common/sge_options.h, e.g.
    * the -a option has option number a_OPT.
    * Every option has a unique option number.
    */
   SPA_number = SPA_LOWERBOUND,
   /**
    * @brief Argument Type
    *
    * If an option has additional arguments the type of the argument,
    * as defined in source/libs/cull/cull_list.h, enum _enum_lMultiType, e.g.
    * lStringT for the -N option (defining a job name as string).
    */
   SPA_argtype,
   /**
    * @brief Switch
    *
    * The option as string, e.g. -N.
    */
   SPA_switch_val,
   /**
    * @brief Switch Argument
    *
    * Optional the argument to the option, e.g. for the -N switch the job name.
    */
   SPA_switch_arg,
   /**
    * @brief Occurence
    *
    * @todo Seems to define if an option has arguments or not.
    * Possible values are defined in source/common/parse_qsub.h (@todo it is *not* qsub specific, move it):
    * - BIT_SPA_OCC_NOARG
    * - BIT_SPA_OCC_ARG
    */
   SPA_occurrence,
   /**
    * @brief Parsed Double Argument
    *
    * If the option argument is a double then this value is the commandline argument (switch_arg) parsed to double.
    */
   SPA_argval_lDoubleT,
   /**
    * @brief Parsed Ulong Argument
    *
    * If the option argument is an ulong then this value is the commandline argument (switch_arg) parsed to ulong.
    */
   SPA_argval_lUlongT,
   /**
    * @brief Parsed Ulong64 Argument
    *
    * If the option argument is an ulong64 then this value is the commandline argument (switch_arg) parsed to ulong64.
    */
   SPA_argval_lUlong64T,
   /**
    * @brief Parsed Long Argument
    *
    * If the option argument is a long then this value is the commandline argument (switch_arg) parsed to long.
    */
   SPA_argval_lLongT,
   /**
    * @brief Parsed Char Argument
    *
    * If the option argument is a single bool then this value is the commandline argument (first character of switch_arg).
    * In parsing of the job submission command line it is used to store the scope information,
    * JRS_SCOPE_GLOBAL, JRS_SCOPE_MASTER, JRS_SCOPE_SLAVE.
    */
   SPA_argval_lBoolT,
   /**
    * @brief Parsed Int Argument
    *
    * If the option argument is an integer then this value is the commandline argument (switch_arg) parsed to integer.
    * In parsing of the job submission command line it is used to store the information if requests are hard or soft.
    */
   SPA_argval_lIntT,
   /**
    * @brief String Argument
    *
    * The commandline argument (same value as the switch_arg attribute)
    */
   SPA_argval_lStringT,
   /**
    * @brief Parsed List Argument
    *
    * If the option argument defines a list of items then this attribute contains the parsed list.
    * The type of its elements depends on the option, e.g. ST for -hold_jid, PN for -jsv, VA for -v.
    */
   SPA_argval_lListT,
};

namespace ocs::tom {

   /**
    * @brief Commandline Argument
    *
    * Used in clients for parsing the commandline.
    * One commandline argument (including data) is one SPA_Type object.
    */
   struct SPA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SPA_number, "SPA_number", FieldType::Uint32, Flag::None},
         {SPA_argtype, "SPA_argtype", FieldType::Uint32, Flag::None},
         {SPA_switch_val, "SPA_switch_val", FieldType::String, Flag::Hash},
         {SPA_switch_arg, "SPA_switch_arg", FieldType::String, Flag::None},
         {SPA_occurrence, "SPA_occurrence", FieldType::Uint32, Flag::None},
         {SPA_argval_lDoubleT, "SPA_argval_lDoubleT", FieldType::Double, Flag::None},
         {SPA_argval_lUlongT, "SPA_argval_lUlongT", FieldType::Uint32, Flag::None},
         {SPA_argval_lUlong64T, "SPA_argval_lUlong64T", FieldType::Uint64, Flag::None},
         {SPA_argval_lLongT, "SPA_argval_lLongT", FieldType::Int64, Flag::None},
         {SPA_argval_lBoolT, "SPA_argval_lBoolT", FieldType::Bool, Flag::None},
         {SPA_argval_lIntT, "SPA_argval_lIntT", FieldType::Int32, Flag::None},
         {SPA_argval_lStringT, "SPA_argval_lStringT", FieldType::String, Flag::None},
         {SPA_argval_lListT, "SPA_argval_lListT", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SPA belongs to SPA */
   template <>
   struct TypeOf<SPA_Field> {
      using type = SPA;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of SPA for the list library: a synonym for ocs::tom::SPA::descriptor */
inline constexpr const lDescr *SPA_Type = ocs::tom::SPA::descriptor.e;
