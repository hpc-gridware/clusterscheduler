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
 * This code was generated from file source/libs/sgeobj/json/REF.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Generic Reference (unused): object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of REF, Generic Reference (unused)
 *
 * The enumeration is the type of every field number of REF, so a parameter
 * of this type takes the field numbers of REF only.
 */
enum REF_Field {
   /**
    * @brief Referenced Element
    *
    * Part of the unused REF object; see the object description.
    */
   REF_ref = REF_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Generic Reference (unused)
    *
    * A bare pointer to another element, for building temporary reference lists.
    * @warning No attribute of this type is read or written anywhere in either repository, and `REF` itself is referenced nowhere. Left in place because removing a type changes the spooled object model.
    */
   struct REF {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {REF_ref, "REF_ref", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of REF belongs to REF */
   template <>
   struct TypeOf<REF_Field> {
      using type = REF;   ///< the object type
   };

} // namespace ocs::tom
