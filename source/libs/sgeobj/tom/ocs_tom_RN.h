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
 * This code was generated from file source/libs/sgeobj/json/RN.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Range Element: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RN, Range Element
 *
 * The enumeration is the type of every field number of RN, so a parameter
 * of this type takes the field numbers of RN only.
 */
enum RN_Field {
   /**
    * @brief Lower Bound
    *
    * minimum or start value of an id range (e.g. 1)
    */
   RN_min = RN_LOWERBOUND,
   /**
    * @brief Upper Bound
    *
    * maximum or end value of an id range (e.g. 9)
    */
   RN_max,
   /**
    * @brief Step Size
    *
    * stepsize (e.g. 2)
    */
   RN_step,
};

namespace ocs::tom {

   /**
    * @brief Range Element
    *
    *  
    * Object holding values which define an id range
    * (e.g. 1-9:2 => 1, 3, 5, 7, 9).
    * Lists of such objects are held within job objects
    * (JB) to hold job array task ids.
    * Several functions may be used to access/modify/delete range
    * elements and range lists. You may find them in the 'SEE ALSO'
    * section below. It is highly advised to use these access functions
    * because they assure and require a defined structure of
    * elements and lists.
    *  
    * Range elements and lists stored in other objects fullfill
    * following conditions:
    *  
    *    - min <= max
    *    - step >= 1
    *    - real range elements (e.g. 1-9:2 instead of 1-10:2)
    *    - min-ids within range elements part of the same
    *      list are in ascending order: min_id(n) < min_id(n+1)
    *      (e.g. NOT 11-20:1; 1-9:2)
    *    - ids within range elements part of the same
    *      list are non-overlapping: max_id(n) < min_id(n+1)
    *      (e.g. 1-9:2; 11-20:1; 25-28:3)
    *  
    * @see gdi/range/range_list_calculate_union_set()
    * @see gdi/range/range_list_calculate_difference_set()
    * @see gdi/range/range_list_calculate_intersection_set() 
    * @see gdi/range/range_list_compress()
    * @see gdi/range/range_list_get_first_id()
    * @see gdi/range/range_list_get_last_id()
    * @see gdi/range/range_list_get_number_of_ids()
    * @see gdi/range/range_list_initialize()
    * @see gdi/range/range_list_insert_id()
    * @see gdi/range/range_list_is_id_within()
    * @see gdi/range/range_list_move_first_n_ids()
    * @see gdi/range/range_list_print_to_string()
    * @see gdi/range/range_list_remove_id()
    * @see gdi/range/range_correct_end()
    * @see gdi/range/range_get_all_ids()
    * @see gdi/range/range_get_number_of_ids()
    * @see gdi/range/range_is_overlapping()
    * @see gdi/range/range_is_id_within()
    * @see gdi/range/range_set_all_ids()
    * @see gdi/range/range_sort_uniq_compress()
    */
   struct RN {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RN_min, "RN_min", FieldType::Uint32, Flag::PrimaryKey | Flag::SpoolSublist},
         {RN_max, "RN_max", FieldType::Uint32, Flag::SpoolSublist},
         {RN_step, "RN_step", FieldType::Uint32, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RN belongs to RN */
   template <>
   struct TypeOf<RN_Field> {
      using type = RN;   ///< the object type
   };

} // namespace ocs::tom
