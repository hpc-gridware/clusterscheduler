/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

/** @file
 * @brief The installed object model: field names, field lookup, sub-types and the static descriptors of the types
 *
 * The list library knows field numbers and descriptors, but not the object
 * types they belong to. The model of the object types is generated into
 * `libs/sgeobj/tom` (`ocs_tom_all.h`: `all_type_tables`, `all_sub_types`),
 * which the list library cannot depend on. So the program hands the model to
 * the list library once at start with install_model(); sgeobj offers the
 * instance (`ocs::tom::gcs_model()` in `sgeobj/tom/ocs_tom_model.h`).
 *
 * install_model() builds two read-only indexes over the tables: the object
 * types sorted by their base field number (a type's field numbers are the
 * consecutive range base .. base + size - 1, CS-439), and all fields sorted by
 * name. A lookup by field number is a binary search over the types,
 * O(log types); a lookup by name is a binary search over the fields, O(log n).
 * No static initializer is involved: an object of a static library is linked
 * only when referenced, so a registration by a static constructor would be
 * silently dropped.
 *
 * The model is process-wide. Installing it is idempotent and cheap when the
 * same model is installed already, so every program and every thread start
 * may call it. Readers never take a lock; an index once published is never
 * freed, so a reader holding it stays valid when another model is installed.
 *
 * The list library finds the static descriptor of a type
 * through type_table(), so a descriptor of a type the installed model does not
 * know is never recognised as static.
 */
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "tom/ocs_Types.h"
#include "tom/ocs_FieldTraits.h"

namespace ocs::tom {

   /**
    * @brief An object model: the field tables of all object types and the sub-types of their list and object fields
    *
    * Only views of tables that live for the whole run of the program (the
    * generated constexpr tables); install_model() keeps the views, not copies.
    */
   struct Model {
      std::span<const TypeTable> types;          ///< the field table of every object type
      std::span<const SubTypeEntry> sub_types;   ///< the sub-type of every list and object field that has one
   };

   bool install_model(const Model &model, std::string *reason = nullptr);

   void uninstall_model() noexcept;

   bool has_model() noexcept;

   const char *field_name(int nm) noexcept;

   int find_field(std::string_view full_name) noexcept;

   const FieldInfo *field_info(int nm) noexcept;

   const TypeTable *type_table(int nm) noexcept;

   const ocs::tom::FieldDescr *sub_type(int nm) noexcept;

   const char *type_name(const ocs::tom::FieldDescr *descr) noexcept;

} // namespace ocs::tom
