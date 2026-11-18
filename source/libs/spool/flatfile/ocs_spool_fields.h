#pragma once
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
 * @brief The flatfile spooling field tables as compile-time typed tables
 *
 * A field table says which fields of an object type the flatfile writer and reader handle, under which
 * name, with which width, how a sublist is laid out and which callbacks render or parse a field. Every
 * table is a tuple of #OcsSpoolField entries; an entry carries its field and its sub-table as template
 * arguments, so its typed callbacks take the element as `ocs::tom::Element<T>` and the code touching the
 * field's value is compiled per field from ocs::tom::FieldTraits.
 *
 * The writer and the reader in sge_flatfile.cc walk a #spooling_field array, because their callers select,
 * copy, align and patch the rows at run time (qconf_copy_fields(), spool_flatfile_align_list(), the
 * builders in sge_flatfile_obj.cc, the views of libs/procedure). #ocs_spool_view turns a typed table into
 * that array at compile time - a pack expansion over the tuple - and gives every row a pointer to the
 * #ocs_spool_field_ops record of its field (spool/ocs_spool_field_info.h), which is where the walkers find
 * the field's type, its renderings and its shared accessors. A row built at run time from a descriptor
 * (spool_get_fields_to_spool()) or from a field number (the share tree's recursion row) takes a record made
 * the same way from ocs_spool_field_ops_of(), so every row the walkers see has its ops.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "basis_types.h"

#include "uti/sge.h"
#include "uti/sge_dstring.h"

#include "cull/cull.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_userset.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/ocs_spool_field_info.h"

struct spool_flatfile_instr;

/** @brief The type of #ocs_spool_no_table, the sub-table of an entry without one */
struct ocs_spool_no_table_t {};

/** @brief The sub-table of an entry without one */
inline constexpr ocs_spool_no_table_t ocs_spool_no_table{};

/**
 * @brief One row of a typed field table
 *
 * @tparam F the field, of the object type of the table
 * @tparam Sub the table its sublist is written and read with, #ocs_spool_no_table for none; a table may name a
 *             sub-table for a field that is not a list, as the old arrays did - the walkers ignore it there
 */
template <auto F, const auto &Sub = ocs_spool_no_table>
struct OcsSpoolField {
   /// the object type of the field
   using type = typename ocs::tom::FieldTraits<F>::Type;
   /// a callback parsing the field's value, returns false if the value is rejected
   using read_fn = bool (*)(ocs::tom::Element<type>, const char *, ocs::tom::ListPtr<ocs::tom::AN> *);
   /// a callback rendering the field's value, returns false if nothing was rendered
   using write_fn = bool (*)(ocs::tom::ConstElement<type>, dstring *, ocs::tom::ListPtr<ocs::tom::AN> *);

   /// the field
   static constexpr auto field = F;
   /// the sub-table
   static constexpr const auto &sub_table = Sub;
   /// whether the entry names a sub-table
   static constexpr bool has_sub_table = !std::is_same_v<std::remove_cvref_t<decltype(Sub)>, ocs_spool_no_table_t>;

   const char *name;                                ///< the name in the text, nullptr for a nameless field
   int width;                                       ///< the width of the name column, 0 for unaligned
   const spool_flatfile_instr *sub_instr = nullptr; ///< the layout of the sublist, nullptr for the default
   read_fn read = nullptr;                          ///< parses the value instead of the default conversion
   write_fn write = nullptr;                        ///< renders the value instead of the default conversion
   unsigned need = 0;                               ///< the variant bits a builder must select for the row
};

/**
 * @brief A typed read callback in the signature of #spooling_field::read_func
 *
 * The field number is not read: the callback knows its field.
 *
 * @tparam T the object type of the callback's field
 * @tparam Cb the typed callback
 * @param ep the element, holding the callback's field
 * @param nm the field, unused
 * @param buffer the text of the value
 * @param answer_list receives the messages
 * @return 1 if the value was accepted, 0 otherwise
 */
template <typename T, auto Cb>
int ocs_spool_read_thunk(lListElem *ep, [[maybe_unused]] const int nm, const char *buffer,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   // the walkers call a row's callback only for an element holding the row's field; checked as the old callbacks were
   return Cb(ocs::tom::Element<T>(ep), buffer, answer_list) ? 1 : 0;
}

/**
 * @brief A typed write callback in the signature of #spooling_field::write_func
 *
 * The field number is not read: the callback knows its field.
 *
 * @tparam T the object type of the callback's field
 * @tparam Cb the typed callback
 * @param ep the element, holding the callback's field
 * @param nm the field, unused
 * @param buffer receives the text, appended
 * @param answer_list receives the messages
 * @return 1 if something was rendered, 0 otherwise
 */
template <typename T, auto Cb>
int ocs_spool_write_thunk(const lListElem *ep, [[maybe_unused]] const int nm, dstring *buffer,
                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   // the walkers call a row's callback only for an element holding the row's field; checked as the old callbacks were
   return Cb(ocs::tom::ConstElement<T>(ep), buffer, answer_list) ? 1 : 0;
}

/**
 * @brief The number of entries of a typed table
 * @tparam Table the table, a tuple of OcsSpoolField entries
 */
template <const auto &Table>
inline constexpr std::size_t ocs_spool_table_size = std::tuple_size_v<std::remove_cvref_t<decltype(Table)>>;

template <const auto &Table>
constexpr spooling_field *ocs_spool_view_rows();

/**
 * @brief The row of one entry of a typed table
 * @tparam Table the table
 * @tparam I the index of the entry
 * @return the row; it owns neither its name nor its sub-table
 */
template <const auto &Table, std::size_t I>
consteval spooling_field ocs_spool_make_row() {
   constexpr const auto &entry = std::get<I>(Table);
   using E = std::remove_cvref_t<decltype(entry)>;

   spooling_field row{};
   row.nm = E::field;
   row.width = entry.width;
   row.name = entry.name;
   row.free_name = false;
   if constexpr (E::has_sub_table) {
      row.sub_fields = ocs_spool_view_rows<E::sub_table>();
   }
   row.free_sub_fields = false;
   row.clientdata = entry.sub_instr;
   if constexpr (entry.read != nullptr) {
      row.read_func = &ocs_spool_read_thunk<typename E::type, entry.read>;
   }
   if constexpr (entry.write != nullptr) {
      row.write_func = &ocs_spool_write_thunk<typename E::type, entry.write>;
   }
   row.ops = &ocs_spool_ops<E::field>;
   return row;
}

/**
 * @brief The rows of a typed table, terminated by a `NoName` row
 *
 * The terminating row has the width of the first entry, as the hand written arrays had.
 *
 * @tparam Table the table
 * @tparam I the indices of its entries
 * @return the rows
 */
template <const auto &Table, std::size_t... I>
consteval std::array<spooling_field, sizeof...(I) + 1> ocs_spool_make_view(std::index_sequence<I...>) {
   spooling_field end{};
   end.nm = NoName;
   if constexpr (sizeof...(I) > 0) {
      end.width = std::get<0>(Table).width;
   }
   return {ocs_spool_make_row<Table, I>()..., end};
}

/**
 * @brief The run-time view of a typed table: the #spooling_field array its run-time users take
 *
 * Constant initialised, so it exists before any static constructor runs. It is mutable because the callers
 * align the shared arrays in place, as they did with the hand written ones.
 *
 * @tparam Table the table
 */
template <const auto &Table>
constinit std::array<spooling_field, ocs_spool_table_size<Table> + 1> ocs_spool_view =
      ocs_spool_make_view<Table>(std::make_index_sequence<ocs_spool_table_size<Table>>{});

/**
 * @brief The rows of the run-time view of a typed table
 *
 * Declared before the view so that a row can point at the view of its sub-table.
 *
 * @tparam Table the table
 * @return the first row
 */
template <const auto &Table>
constexpr spooling_field *ocs_spool_view_rows() {
   return ocs_spool_view<Table>.data();
}

/**
 * @brief The variant bits each entry of a typed table needs
 * @tparam Table the table
 * @tparam I the indices of its entries
 * @return the bits, in the order of the entries
 */
template <const auto &Table, std::size_t... I>
consteval std::array<unsigned, sizeof...(I)> ocs_spool_make_needs(std::index_sequence<I...>) {
   return {std::get<I>(Table).need...};
}

/**
 * @brief The variant bits each entry of a typed table needs, in the order of the entries
 * @tparam Table the table, a tuple of OcsSpoolField entries
 */
template <const auto &Table>
inline constexpr std::array<unsigned, ocs_spool_table_size<Table>> ocs_spool_needs =
      ocs_spool_make_needs<Table>(std::make_index_sequence<ocs_spool_table_size<Table>>{});

/**
 * @brief The rows of a table view a variant selects, as a heap array (see the definition)
 * @param view the rows of the table, terminated by a NoName row
 * @param needs the variant bits each row needs
 * @param size the number of rows without the terminator
 * @param variant the variant bits the caller has
 * @return the selected rows and the terminator, allocated with sge_malloc()
 */
spooling_field *ocs_spool_select_rows(const spooling_field *view, const unsigned *needs, size_t size,
                                      unsigned variant);

/**
 * @brief The rows of a typed table a variant selects, as a heap array
 *
 * A row is selected when the variant holds every bit its entry needs; the order of the table is kept.
 *
 * @tparam Table the table
 * @param variant the variant bits
 * @return the rows and the terminating row, to be freed with #spool_free_spooling_fields or sge_free()
 */
template <const auto &Table>
spooling_field *ocs_spool_select(const unsigned variant) {
   return ocs_spool_select_rows(ocs_spool_view<Table>.data(), ocs_spool_needs<Table>.data(),
                                ocs_spool_table_size<Table>, variant);
}
