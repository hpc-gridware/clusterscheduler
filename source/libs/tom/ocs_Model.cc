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
 * @brief The installed object model: the indexes behind field_name(), find_field(), sub_type() and type_name()
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstring>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "uti/sge_log.h"

#include "tom/msg_tom.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Model.h"

namespace ocs::tom {

   namespace {

      /** @brief One object type of the installed model, found by the range of its field numbers */
      struct TypeEntry {
         int base;                  ///< the field number of the type's first field
         int size;                  ///< the number of fields; the type holds base .. base + size - 1
         const TypeTable *table;    ///< the type's table in the model
         std::size_t first_sub;     ///< index of the type's first field in Index::subs
      };

      /** @brief One field of the installed model, found by its name */
      struct NameEntry {
         std::string_view name;     ///< the full field name, e.g. "JB_job_number"
         int nm;                    ///< the field number
      };

      /** @brief The read-only indexes over one installed model; built once, never changed, never freed */
      struct Index {
         Model model;                       ///< the model the index was built from
         std::vector<TypeEntry> types;      ///< all types, sorted by base
         std::vector<NameEntry> names;      ///< all fields, sorted by name
         std::vector<const lDescr *> subs;  ///< the sub-type of every field, nullptr if none; at first_sub + pos
      };

      std::atomic<const Index *> installed{nullptr};   ///< the installed model, nullptr if none
      std::mutex build_mutex;                          ///< serializes install_model() and uninstall_model()
      std::vector<const Index *> *built = nullptr;     ///< every index built so far, reused on a re-install; leaked

      /**
       * @brief Whether an index was built from the given model
       * @param index the index
       * @param model the model
       * @return true if both views of the model are the same as those of the index
       */
      bool same_model(const Index &index, const Model &model) noexcept {
         return index.model.types.data() == model.types.data() && index.model.types.size() == model.types.size() &&
                index.model.sub_types.data() == model.sub_types.data() &&
                index.model.sub_types.size() == model.sub_types.size();
      }

      /**
       * @brief Find the object type whose field number range holds a field number
       * @param index the index to search
       * @param nm the field number
       * @return the type, or nullptr if no type holds @p nm
       */
      const TypeEntry *find_type(const Index &index, const int nm) noexcept {
         const auto it = std::upper_bound(index.types.begin(), index.types.end(), nm,
                                          [](const int n, const TypeEntry &e) { return n < e.base; });
         if (it == index.types.begin()) {
            return nullptr;
         }
         const TypeEntry &type = *std::prev(it);
         return nm < type.base + type.size ? &type : nullptr;
      }

      /**
       * @brief Check the field table of one object type against itself and against its descriptor
       *
       * The type needs a name, a descriptor and at least one field; every field
       * needs a name, and the field numbers must be consecutive. The descriptor
       * must hold the table's fields in the same order, each with the field
       * number and the type of the list library the table declares, and end
       * after the last of them.
       *
       * @param table the field table of the type
       * @param[out] reason receives the violated rule if the table is not consistent
       * @return true if the table is consistent
       * @throw std::bad_alloc if memory for @p reason is exhausted
       */
      bool check_table(const TypeTable &table, std::string &reason) {
         const std::string type = table.name != nullptr ? table.name : "(unnamed)";
         if (table.name == nullptr) {
            reason = "a type has no name";
            return false;
         }
         if (table.descr == nullptr || table.fields == nullptr || table.size == 0) {
            reason = "type " + type + " has no descriptor or no fields";
            return false;
         }
         const int base = table.fields[0].nm;
         // the position of a field as text, built on the error path only
         const auto where = [&type](const std::size_t i) { return "type " + type + ", field " + std::to_string(i); };
         for (std::size_t i = 0; i < table.size; i++) {
            const FieldInfo &field = table.fields[i];
            if (field.name == nullptr) {
               reason = where(i) + " (number " + std::to_string(field.nm) + ") has no name";
               return false;
            }
            if (field.nm != base + static_cast<int>(i)) {
               reason = where(i) + " (" + field.name + ") has the number " + std::to_string(field.nm) +
                        ", expected " + std::to_string(base + static_cast<int>(i)) +
                        ": the field numbers are not consecutive";
               return false;
            }
            const lDescr &entry = table.descr[i];
            if (entry.nm != field.nm || mt_get_type(entry.mt) != mt_type_of(field.type)) {
               reason = where(i) + " (" + field.name + "): the descriptor holds field " + std::to_string(entry.nm) +
                        " of type " + type_name(mt_get_type(entry.mt)) + ", the table field " +
                        std::to_string(field.nm) + " of type " + type_name(mt_type_of(field.type));
               return false;
            }
         }
         if (table.descr[table.size].nm != NoName || mt_get_type(table.descr[table.size].mt) != MT_END) {
            reason = "type " + type + ": the descriptor has more entries than the " + std::to_string(table.size) +
                     " fields of the table";
            return false;
         }
         return true;
      }

      /**
       * @brief Build the indexes over a model and check it for consistency
       *
       * Consistent means: the model has at least one type; every type passes
       * check_table(); no type name occurs twice; the ranges of the types do not
       * overlap; no field name occurs twice; every sub-type entry names a field
       * of the model that is a list or an object field, and no field has two
       * entries. The generator guarantees all of this for the generated model,
       * so a violation means a hand-made or damaged model.
       *
       * @param model the model
       * @param[out] reason receives the first violated rule if the model is not consistent
       * @return the index, or nullptr if the model is not consistent
       * @throw std::bad_alloc if memory is exhausted
       */
      const Index *build_index(const Model &model, std::string &reason) {
         if (model.types.empty()) {
            reason = "the model has no types";
            return nullptr;
         }
         auto index = std::make_unique<Index>(Index{model, {}, {}, {}});
         std::size_t n_fields = 0;
         for (const TypeTable &table : model.types) {
            if (!check_table(table, reason)) {
               return nullptr;
            }
            index->types.push_back({table.fields[0].nm, static_cast<int>(table.size), &table, n_fields});
            n_fields += table.size;
         }

         std::vector<std::string_view> type_names;
         type_names.reserve(model.types.size());
         for (const TypeTable &table : model.types) {
            type_names.emplace_back(table.name);
         }
         std::sort(type_names.begin(), type_names.end());
         if (const auto twice = std::adjacent_find(type_names.begin(), type_names.end());
             twice != type_names.end()) {
            reason = "the type name " + std::string{*twice} + " occurs twice";
            return nullptr;
         }

         std::sort(index->types.begin(), index->types.end(),
                   [](const TypeEntry &a, const TypeEntry &b) { return a.base < b.base; });
         for (std::size_t i = 1; i < index->types.size(); i++) {
            const TypeEntry &low = index->types[i - 1];
            const TypeEntry &high = index->types[i];
            if (low.base + low.size > high.base) {
               reason = "the field numbers of type " + std::string{low.table->name} + " (" +
                        std::to_string(low.base) + " .. " + std::to_string(low.base + low.size - 1) +
                        ") overlap those of type " + high.table->name + " (from " + std::to_string(high.base) + ")";
               return nullptr;
            }
         }

         index->names.reserve(n_fields);
         for (const TypeEntry &type : index->types) {
            for (int i = 0; i < type.size; i++) {
               index->names.push_back({type.table->fields[i].name, type.table->fields[i].nm});
            }
         }
         std::sort(index->names.begin(), index->names.end(),
                   [](const NameEntry &a, const NameEntry &b) { return a.name < b.name; });
         if (const auto twice = std::adjacent_find(index->names.begin(), index->names.end(),
                                                   [](const NameEntry &a, const NameEntry &b) {
                                                      return a.name == b.name;
                                                   });
             twice != index->names.end()) {
            reason = "the field name " + std::string{twice->name} + " occurs twice (numbers " +
                     std::to_string(twice->nm) + " and " + std::to_string(std::next(twice)->nm) + ")";
            return nullptr;
         }

         index->subs.assign(n_fields, nullptr);
         std::vector<bool> has_entry(n_fields, false);
         for (const SubTypeEntry &entry : model.sub_types) {
            const TypeEntry *type = find_type(*index, entry.nm);
            if (type == nullptr) {
               reason = "a sub-type entry names the field number " + std::to_string(entry.nm) +
                        ", which no type of the model holds";
               return nullptr;
            }
            const std::size_t pos = static_cast<std::size_t>(entry.nm - type->base);
            const FieldInfo &field = type->table->fields[pos];
            if (field.type != FieldType::List && field.type != FieldType::Object) {
               reason = std::string{"a sub-type entry names the field "} + field.name +
                        ", which is neither a list nor an object field";
               return nullptr;
            }
            if (has_entry[type->first_sub + pos]) {
               reason = std::string{"the field "} + field.name + " has more than one sub-type entry";
               return nullptr;
            }
            has_entry[type->first_sub + pos] = true;
            index->subs[type->first_sub + pos] = entry.sub;
         }
         return index.release();
      }

   } // namespace

   /**
    * @brief Install the object model the name lookups of the typed object model read
    *
    * Called once at start of a program or a thread; calling it again with the same
    * model costs one atomic load. Installing another model replaces the
    * installed one; the replaced indexes stay valid for readers that still
    * hold them. An index built once is reused when the same model is
    * installed again.
    *
    * @param model the model, whose tables must live for the rest of the program
    * @param[out] reason if not nullptr, receives the violated rule when the model is rejected; left as it is
    *             when the model is installed. For the module test, which checks that each inconsistent model
    *             is rejected for the rule it breaks.
    * @return true if the model is installed; false if it is not consistent (see build_index()), in which case the
    *         violated rule is logged as critical and the installed model is left as it was
    * @throw std::bad_alloc if memory for the indexes is exhausted
    */
   bool install_model(const Model &model, std::string *reason) {
      if (const Index *current = installed.load(std::memory_order_acquire);
          current != nullptr && same_model(*current, model)) {
         return true;
      }
      std::lock_guard<std::mutex> guard(build_mutex);
      if (built == nullptr) {
         built = new std::vector<const Index *>;
      }
      const Index *index = nullptr;
      for (const Index *candidate : *built) {
         if (same_model(*candidate, model)) {
            index = candidate;
            break;
         }
      }
      if (index == nullptr) {
         std::string rule;
         index = build_index(model, rule);
         if (index == nullptr) {
            CRITICAL(MSG_TOM_MODELREJECTED_S, rule.c_str());
            if (reason != nullptr) {
               *reason = rule;
            }
            return false;
         }
         built->push_back(index);
      }
      installed.store(index, std::memory_order_release);
      return true;
   }

   /**
    * @brief Remove the installed model, so that the name lookups find nothing
    *
    * For tests of the behaviour without a model. The indexes stay allocated
    * for readers that still hold them and for a later re-install.
    */
   void uninstall_model() noexcept {
      std::lock_guard<std::mutex> guard(build_mutex);
      installed.store(nullptr, std::memory_order_release);
   }

   /**
    * @brief Whether an object model is installed
    * @return true if install_model() succeeded and uninstall_model() was not called since
    */
   bool has_model() noexcept {
      return installed.load(std::memory_order_acquire) != nullptr;
   }

   /**
    * @brief The table entry of a field: number, name, type and flags
    *
    * O(log types): a binary search over the field number ranges of the types.
    *
    * @param nm the field number
    * @return the entry, or nullptr if no model is installed or no type of the model has the field
    */
   const FieldInfo *field_info(const int nm) noexcept {
      const Index *index = installed.load(std::memory_order_acquire);
      if (index == nullptr) {
         return nullptr;
      }
      const TypeEntry *type = find_type(*index, nm);
      return type != nullptr ? &type->table->fields[nm - type->base] : nullptr;
   }

   /**
    * @brief The field table of the object type whose field number range holds a field
    *
    * O(log types): a binary search over the field number ranges of the types.
    * The list library reads the static descriptor of a type from it
    * (`TypeTable::descr`), the position of the field in the type is
    * `nm - fields[0].nm`.
    *
    * @param nm the field number
    * @return the table, or nullptr if no model is installed or no type of the model holds the field
    */
   const TypeTable *type_table(const int nm) noexcept {
      const Index *index = installed.load(std::memory_order_acquire);
      if (index == nullptr) {
         return nullptr;
      }
      const TypeEntry *type = find_type(*index, nm);
      return type != nullptr ? type->table : nullptr;
   }

   /**
    * @brief The full name of a field, e.g. "JB_job_number"
    *
    * O(log types). For a field the installed model does not know, or without
    * a model, the text "nm <n>" is returned instead. It is written into one of
    * four buffers of the calling thread, used in turn: no other thread can
    * overwrite it, and it stays valid until the same thread has asked for four
    * further unknown names. A known
    * name is a constant of the model and valid for the whole run.
    *
    * @param nm the field number
    * @return the name, never nullptr
    */
   const char *field_name(const int nm) noexcept {
      if (const FieldInfo *info = field_info(nm); info != nullptr) {
         return info->name;
      }
      static constexpr std::size_t n_buffers = 4;
      static constexpr std::size_t buffer_size = 16;   // "nm " + "-2147483648" + '\0'
      thread_local std::array<std::array<char, buffer_size>, n_buffers> buffers{};
      thread_local std::size_t next = 0;
      auto &buffer = buffers[next];
      next = (next + 1) % n_buffers;
      std::memcpy(buffer.data(), "nm ", 3);
      const auto [end, ec] = std::to_chars(buffer.data() + 3, buffer.data() + buffer.size() - 1, nm);
      *(ec == std::errc{} ? end : buffer.data() + 3) = '\0';
      return buffer.data();
   }

   /**
    * @brief The field number of a field name
    *
    * Only the full name is accepted ("JB_job_number").
    * O(log n) over all fields of the model.
    *
    * @param full_name the full field name
    * @return the field number, or NoName if no model is installed or the model has no field of that name
    */
   int find_field(const std::string_view full_name) noexcept {
      const Index *index = installed.load(std::memory_order_acquire);
      if (index == nullptr) {
         return NoName;
      }
      const auto it = std::lower_bound(index->names.begin(), index->names.end(), full_name,
                                       [](const NameEntry &e, const std::string_view n) { return e.name < n; });
      return it != index->names.end() && it->name == full_name ? it->nm : NoName;
   }

   /**
    * @brief The sub-type of a list or object field, as the model names it
    *
    * O(log types).
    *
    * @param nm the field number
    * @return the full descriptor of the sub-type, or nullptr if no model is installed, the field is unknown, is no
    *         list or object field, or is an untyped list or object field (one the model names no sub-type for, so
    *         that it may hold elements of any type)
    */
   const lDescr *sub_type(const int nm) noexcept {
      const Index *index = installed.load(std::memory_order_acquire);
      if (index == nullptr) {
         return nullptr;
      }
      const TypeEntry *type = find_type(*index, nm);
      return type != nullptr ? index->subs[type->first_sub + static_cast<std::size_t>(nm - type->base)] : nullptr;
   }

   /**
    * @brief The name of the object type of a descriptor, e.g. "JB"
    *
    * A full and a reduced descriptor of a type keep all entries of the type in
    * field order (a reduced one marks the absent fields only), so the first
    * entry names the type's first field. O(log types).
    *
    * @param descr a full or reduced descriptor, or nullptr
    * @return the type's name, or nullptr if no model is installed, @p descr is nullptr or empty, or its first entry
    *         is not the first field of a type of the model
    */
   const char *type_name(const lDescr *descr) noexcept {
      const Index *index = installed.load(std::memory_order_acquire);
      if (index == nullptr || descr == nullptr || descr[0].nm == NoName) {
         return nullptr;
      }
      const TypeEntry *type = find_type(*index, descr[0].nm);
      return type != nullptr && type->base == descr[0].nm ? type->table->name : nullptr;
   }

} // namespace ocs::tom
