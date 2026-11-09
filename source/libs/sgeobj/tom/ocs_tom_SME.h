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
 * This code was generated from file source/libs/sgeobj/json/SME.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job SchedulingInfo: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SME, Job SchedulingInfo
 *
 * The enumeration is the type of every field number of SME, so a parameter
 * of this type takes the field numbers of SME only.
 */
enum SME_Field {
   /**
    * @brief Message List
    *
    * Job specific messages explaining why certain requests of a job cannot be granted.
    */
   SME_message_list = SME_LOWERBOUND,
   /**
    * @brief Global Message List
    *
    * Global messages which apply to all jobs, e.g. queues being disabled.
    */
   SME_global_message_list,
};

namespace ocs::tom {

   struct MES;

   /**
    * @brief Job SchedulingInfo
    *
    * Root object for scheduler messages enabled by scheduler config attribute schedd_job_info.
    */
   struct SME {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SME_message_list, "SME_message_list", FieldType::List, Flag::None},
         {SME_global_message_list, "SME_global_message_list", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SME belongs to SME */
   template <>
   struct TypeOf<SME_Field> {
      using type = SME;   ///< the object type
   };

   /** @brief SME_message_list holds elements of MES */
   template <>
   struct SubType<SME_message_list> {
      using type = MES;   ///< the sub-type
   };

   /** @brief SME_global_message_list holds elements of MES */
   template <>
   struct SubType<SME_global_message_list> {
      using type = MES;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of SME for the list library: a synonym for ocs::tom::SME::descriptor */
inline constexpr const lDescr *SME_Type = ocs::tom::SME::descriptor.e;
