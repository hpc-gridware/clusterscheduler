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
 * @brief Module test of the single-pass select-and-pack path of the list core in `libs/tom`
 *
 * The qmaster answers a GDI GET request with a where and a what by walking the
 * master list once and packing what it selects straight into the pack buffer
 * (select_pack(), formerly lSelectHashPack() with a pack buffer,
 * ocs_gdi_Packet.cc). Nothing else in the module tests exercises that path.
 * These scenarios pin what it delivers against the two-step path it replaces
 * -- select_as() (formerly lSelect()) followed by pack_list() -- and cover the
 * pieces it is built from: partial_descr(), the counting mode of the pack
 * buffer, pack_list() with flags and a what, reduced lists, free elements and
 * sub-objects in the round trip, and the hash tables of an unpacked list. They
 * are the safety net for changing the pack format.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_WhatP.h"
#include "tom/ocs_Descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"

#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "ocs_module_test.h"
#include "ocs_test_model.h"

/** @brief The selected-from type: one field per CULL type the path handles */
enum {
   SP_name = 1, ///< unique hashed key
   SP_group,    ///< non unique hashed ulong
   SP_host,     ///< a host name
   SP_value,    ///< a double
   SP_flag,     ///< a bool
   SP_big,      ///< a 64-bit ulong
   SP_spool,    ///< the only scalar field marked for spooling
   SP_sub,      ///< a sublist of SPS_Type
   SP_obj       ///< a sub-object of SPS_Type
};

/** @brief The type of the sublist and of the sub-object */
enum {
   SPS_name = 51, ///< unique hashed key
   SPS_count,     ///< an ulong
   SPS_note       ///< a string
};

LISTDEF(SP_Type)
   SGE_STRING(SP_name, CULL_DEFAULT | CULL_HASH | CULL_UNIQUE)
   SGE_ULONG(SP_group, CULL_DEFAULT | CULL_HASH)
   SGE_HOST(SP_host, CULL_DEFAULT)
   SGE_DOUBLE(SP_value, CULL_DEFAULT)
   SGE_BOOL(SP_flag, CULL_DEFAULT)
   SGE_ULONG64(SP_big, CULL_DEFAULT)
   SGE_STRING(SP_spool, CULL_DEFAULT | CULL_SPOOL)
   SGE_LIST(SP_sub, SPS_Type, CULL_DEFAULT | CULL_SPOOL)
   SGE_OBJECT(SP_obj, SPS_Type, CULL_DEFAULT)
LISTEND(SP_Type)

LISTDEF(SPS_Type)
   SGE_STRING(SPS_name, CULL_DEFAULT | CULL_HASH | CULL_UNIQUE | CULL_SPOOL)
   SGE_ULONG(SPS_count, CULL_DEFAULT | CULL_SPOOL)
   SGE_STRING(SPS_note, CULL_DEFAULT)
LISTEND(SPS_Type)

NAMEDEF(SP_Name)
   NAME("SP_name")
   NAME("SP_group")
   NAME("SP_host")
   NAME("SP_value")
   NAME("SP_flag")
   NAME("SP_big")
   NAME("SP_spool")
   NAME("SP_sub")
   NAME("SP_obj")
NAMEEND

NAMEDEF(SPS_Name)
   NAME("SPS_name")
   NAME("SPS_count")
   NAME("SPS_note")
NAMEEND

#define SP_Size sizeof(SP_Name) / sizeof(char *)   ///< number of fields of SP_Type
#define SPS_Size sizeof(SPS_Name) / sizeof(char *) ///< number of fields of SPS_Type

/// number of elements of the source list
static constexpr int SOURCE_SIZE = 6;
/// number of sublist elements per source element
static constexpr int SUB_SIZE = 3;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static bool list_equal(const lList *a, const lList *b);

/**
 * @brief Compare two elements field by field, sublists and sub-objects included
 *
 * Both elements must have the same fields in the same order; the values are
 * compared by type. A reference is not transferred, so it is not compared.
 *
 * @param a the first element, may be nullptr
 * @param b the second element, may be nullptr
 * @return true if both are nullptr or carry the same fields and values
 */
static bool elem_equal(const lListElem *a, const lListElem *b) {
   if (a == nullptr || b == nullptr) {
      return a == b;
   }
   const lDescr *da = ocs::tom::ConstRawElement(a).descr();
   const lDescr *db = ocs::tom::ConstRawElement(b).descr();
   const int n = ocs::tom::descr_length(da);
   if (n != ocs::tom::descr_length(db)) {
      fprintf(stderr, "elements have %d and %d fields\n", n, ocs::tom::descr_length(db));
      return false;
   }
   for (int i = 0; i < n; i++) {
      if (da[i].nm != db[i].nm || mt_get_type(da[i].mt) != mt_get_type(db[i].mt) ||
          (da[i].off == OCS_CULL_ABSENT) != (db[i].off == OCS_CULL_ABSENT)) {
         return false;
      }
      if (da[i].off == OCS_CULL_ABSENT) {
         continue;
      }
      const int nm = da[i].nm;
      bool same = true;
      switch (mt_get_type(da[i].mt)) {
         case MT_STRING:
            same = sge_strnullcmp(ocs::tom::get<const char *>(a, nm), ocs::tom::get<const char *>(b, nm)) == 0;
            break;
         case MT_HOST:
            same = sge_strnullcmp(ocs::tom::get<ocs::tom::Host>(a, nm), ocs::tom::get<ocs::tom::Host>(b, nm)) == 0;
            break;
         case MT_UINT32:
            same = ocs::tom::get<std::uint32_t>(a, nm) == ocs::tom::get<std::uint32_t>(b, nm);
            break;
         case MT_UINT64:
            same = ocs::tom::get<std::uint64_t>(a, nm) == ocs::tom::get<std::uint64_t>(b, nm);
            break;
         case MT_DOUBLE:
            same = ocs::tom::get<double>(a, nm) == ocs::tom::get<double>(b, nm);
            break;
         case MT_BOOL:
            same = ocs::tom::get<bool>(a, nm) == ocs::tom::get<bool>(b, nm);
            break;
         case MT_INT32:
            same = ocs::tom::get<std::int32_t>(a, nm) == ocs::tom::get<std::int32_t>(b, nm);
            break;
         case MT_INT64:
            same = ocs::tom::get<std::int64_t>(a, nm) == ocs::tom::get<std::int64_t>(b, nm);
            break;
         case MT_LIST:
            same = list_equal(ocs::tom::get<const lList *>(a, nm), ocs::tom::get<const lList *>(b, nm));
            break;
         case MT_OBJECT:
            same = elem_equal(ocs::tom::get<const lListElem *>(a, nm), ocs::tom::get<const lListElem *>(b, nm));
            break;
         default:
            break;
      }
      if (!same) {
         fprintf(stderr, "field %s (type %d) differs\n", ocs::tom::field_name(nm), mt_get_type(da[i].mt));
         return false;
      }
   }
   return true;
}

/**
 * @brief Compare two lists element by element, in order
 * @param a the first list, may be nullptr
 * @param b the second list, may be nullptr
 * @return true if both are nullptr or hold equal elements in the same order
 */
static bool list_equal(const lList *a, const lList *b) {
   if (a == nullptr || b == nullptr) {
      return a == b;
   }
   const ocs::tom::ConstRawList la(a);
   const ocs::tom::ConstRawList lb(b);
   if (la.size() != lb.size()) {
      fprintf(stderr, "list %s has %zu elements, list %s has %zu\n", ocs::tom::list_name(a), la.size(),
              ocs::tom::list_name(b), lb.size());
      return false;
   }
   ocs::tom::ConstRawElement ea = la.front();
   ocs::tom::ConstRawElement eb = lb.front();
   while (ea && eb) {
      if (!elem_equal(ea, eb)) {
         return false;
      }
      ea = ea.next();
      eb = eb.next();
   }
   return true;
}

/**
 * @brief Does a descriptor hold exactly these fields, in this slot order?
 * @param dp the descriptor
 * @param names the expected names, terminated by NoName
 * @return true if the fields that have a value and their order match
 */
static bool descr_names_are(const lDescr *dp, const int *names) {
   if (dp == nullptr) {
      return false;
   }
   int i = 0;
   for (const auto f : ocs::tom::present_fields(dp)) {
      if (names[i] == NoName || dp[f.pos].nm != names[i]) {
         return false;
      }
      i++;
   }
   return names[i] == NoName;
}

/**
 * @brief The descriptor of a list
 * @param lp the list, may be empty
 * @return the descriptor, nullptr for no list
 */
static const lDescr *descr_of(const ocs::tom::ConstRawList lp) {
   return lp ? lp.descr() : nullptr;
}

/**
 * @brief Build the source list
 *
 * SOURCE_SIZE elements e0..e5, SP_group = i % 3, each with a sublist of SUB_SIZE
 * elements s<i>-<k>; the even ones carry a sub-object, the odd ones none.
 * The elements and the sublist elements are added with add_elem_str() of the
 * core, as lAddElemStr() and lAddSubStr() added them.
 *
 * @return the owner of the new list
 */
static ocs::tom::RawListPtr make_source() {
   auto owner = ocs::tom::RawListPtr::create(SP_Type, ocs::tom::Hashing::On, "source");
   lList *lp = owner.get();
   const int sub_pos = ocs::tom::field_pos(SP_Type, SP_sub);
   for (int i = 0; i < SOURCE_SIZE; i++) {
      const std::string n = std::to_string(i);
      const ocs::tom::RawElement ep = ocs::tom::add_elem_str(lp, SP_name, ("e" + n).c_str(), SP_Type);
      ep.set(SP_group, static_cast<std::uint32_t>(i % 3));
      ep.set(SP_host, ocs::tom::Host(("host" + n).c_str()));
      ep.set(SP_value, i * 1.5);
      ep.set(SP_flag, i % 2 == 0);
      ep.set(SP_big, (static_cast<std::uint64_t>(1) << 40) + i);
      ep.set(SP_spool, ("spool" + n).c_str());
      lList *sub_list = ocs::tom::get_or_create_list(ep, sub_pos, nullptr, SPS_Type);
      for (int k = 0; k < SUB_SIZE; k++) {
         const std::string nk = n + "-" + std::to_string(k);
         const ocs::tom::RawElement sub = ocs::tom::add_elem_str(sub_list, SPS_name, ("s" + nk).c_str(), SPS_Type);
         sub.set(SPS_count, static_cast<std::uint32_t>(i * 10 + k));
         sub.set(SPS_note, ("note" + nk).c_str());
      }
      if (i % 2 == 0) {
         auto obj = ocs::tom::RawElementPtr::create(SPS_Type);
         obj->set(SPS_name, ("o" + n).c_str());
         obj->set(SPS_count, static_cast<std::uint32_t>(i));
         obj->set(SPS_note, static_cast<const char *>("object"));
         ep.set(SP_obj, std::move(obj));
      }
   }
   return owner;
}

/**
 * @brief The selection of some fields of a type, without nested selections
 * @param descr the descriptor of the type
 * @param fields the fields
 * @return the owner of the selection
 */
static ocs::tom::RawWhatPtr what_of(const lDescr *descr, const std::initializer_list<int> fields) {
   return ocs::tom::what_of(descr, std::span<const int>(fields.begin(), fields.size()));
}

/**
 * @brief The what of a typical GET request: a few fields, a nested sublist and sub-object
 *
 * Built with make_what() of the core, as lWhat("%T(%I %I %I -> %T(%I %I) %I -> %T(%I))") built it.
 *
 * @return the owner of the selection
 */
static ocs::tom::RawWhatPtr make_nested_what() {
   const ocs::tom::WhatEntry entries[] = {
      {SP_name, nullptr},
      {SP_group, nullptr},
      {SP_sub, what_of(SPS_Type, {SPS_name, SPS_count}).release()},
      {SP_obj, what_of(SPS_Type, {SPS_note}).release()}};
   return ocs::tom::RawWhatPtr(ocs::tom::make_what(SP_Type, entries));
}

/**
 * @brief The condition SP_group <op> value
 * @param op the comparison
 * @param value the value
 * @return the owner of the condition
 */
static ocs::tom::RawWherePtr group_is(const ocs::tom::CompareOp op, const std::uint32_t value) {
   return ocs::tom::where_compare(SP_Type, SP_group, op, value);
}

/**
 * @brief Select as lSelect() did: no list for no match
 * @param name the name of the result
 * @param lp the source list
 * @param where the where, may be nullptr
 * @param what the what, may be nullptr
 * @return the owner of the result, empty for no match or an error
 */
static ocs::tom::RawListPtr select_old(const char *name, const lList *lp, const lCondition *where,
                                        const lEnumeration *what) {
   return ocs::tom::select_as(name, lp, where, what, ocs::tom::Hashing::On);
}

/**
 * @brief Unpack a list from what a pack buffer holds, as the receiver does
 *
 * The bytes are copied into a buffer of their own, so the sending buffer is
 * left untouched and the receiver sees exactly what went over the wire.
 *
 * @param pb the packed buffer
 * @param out receives the owner of the unpacked list
 * @param flags the flags of unpack_raw_list(), 0 for all fields
 * @return the unpack result, PACK_SUCCESS on success
 */
static int unpack_list_from(ocs::tom::PackBuffer &pb, ocs::tom::RawListPtr &out, const int flags = 0) {
   try {
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      out = ocs::tom::unpack_raw_list(&in.raw(), flags);
   } catch (const ocs::tom::PackException &e) {
      out.reset();
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a whole list with pack_list() and unpack it again
 * @param lp the list
 * @param out receives the owner of the unpacked list
 * @return the unpack result, PACK_SUCCESS on success
 */
static int pack_unpack_list(const lList *lp, ocs::tom::RawListPtr &out) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_list(&pb.raw(), lp, nullptr, 0);
      return unpack_list_from(pb, out);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/**
 * @brief Pack one element and unpack it again, as the receiver does
 * @param ep the element
 * @param out receives the owner of the unpacked element
 * @return the unpack result, PACK_SUCCESS on success
 */
static int pack_unpack_elem(const lListElem *ep, ocs::tom::RawElementPtr &out) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_elem(&pb.raw(), ep, nullptr, 0);
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      out = ocs::tom::unpack_raw_elem(&in.raw(), ocs::tom::ConstRawElement(ep).descr(), 0);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Run select_pack() into a fresh pack buffer and unpack the result
 * @param slp the source list
 * @param cp the where, may be nullptr
 * @param what the what, may be nullptr
 * @param out receives the owner of the unpacked list
 * @param bytes receives the number of bytes select_pack() added, the buffer head excluded
 * @return the unpack result, PACK_SUCCESS on success
 */
static int select_pack_unpack(const lList *slp, const lCondition *cp, const lEnumeration *what,
                              ocs::tom::RawListPtr &out, size_t *bytes) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      const size_t head = pb.size();
      ocs::tom::select_pack(&pb.raw(), "", slp, cp, what);
      *bytes = pb.size() - head;
      return unpack_list_from(pb, out);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/**
 * @brief Count the bytes select_pack() packs, in the counting mode of the buffer
 *
 * Only the bytes it adds are counted: the head that a buffer to pack into gets
 * in normal mode is not counted in counting mode.
 * @param slp the source list
 * @param cp the where, may be nullptr
 * @param what the what, may be nullptr
 * @return the number of bytes added, or 0 on error
 */
static size_t select_pack_count(const lList *slp, const lCondition *cp, const lEnumeration *what) {
   try {
      ocs::tom::PackBuffer pb = ocs::tom::PackBuffer::counting();
      const size_t head = pb.size();
      ocs::tom::select_pack(&pb.raw(), "", slp, cp, what);
      return pb.size() - head;
   } catch (const ocs::tom::PackException &) {
      return 0;
   }
}

// ---------------------------------------------------------------------------
// scenarios
// ---------------------------------------------------------------------------

/**
 * @brief SELPACK-01: single-pass select-and-pack delivers what select_as() and pack_list() deliver
 * @return nothing; CHECK records the results
 */
static void test_select_pack_equals_select() {
   ocs::tom::RawListPtr source = make_source();
   ocs::tom::RawWherePtr where = group_is(ocs::tom::CompareOp::Equal, 1);
   ocs::tom::RawWhatPtr what = make_nested_what();

   ocs::tom::RawListPtr packed;
   size_t bytes = 0;
   const int ret = select_pack_unpack(source.get(), where.get(), what.get(), packed, &bytes);
   CHECK(1, "select_pack into a pack buffer unpacks with unpack_raw_list", ret == PACK_SUCCESS && packed);

   ocs::tom::RawListPtr selected = select_old("selected", source.get(), where.get(), what.get());
   CHECK(2, "both paths select the two elements of group 1",
         packed->size() == 2 && selected->size() == 2 &&
         packed->find_first(SP_name, static_cast<const char *>("e1")) &&
         packed->find_first(SP_name, static_cast<const char *>("e4")));

   static const int top[] = {SP_name, SP_group, SP_sub, SP_obj, NoName};
   static const int sub[] = {SPS_name, SPS_count, NoName};
   CHECK(3, "the unpacked list has exactly the fields of the what, in its order",
         descr_names_are(descr_of(packed.get()), top) && descr_names_are(descr_of(selected.get()), top));

   const ocs::tom::ConstRawElement first = packed->front();
   CHECK(4, "the sublists are reduced to the nested what as well",
         first && first.get<ocs::tom::ConstRawList>(SP_sub).size() == SUB_SIZE &&
         descr_names_are(descr_of(first.get<ocs::tom::ConstRawList>(SP_sub)), sub));

   CHECK(5, "the single-pass result equals the select_as result field by field",
         list_equal(packed.get(), selected.get()));

   // the two-step path the single pass replaces: select_as, then pack the reduced list
   ocs::tom::RawListPtr two_step;
   pack_unpack_list(selected.get(), two_step);
   CHECK(6, "the single-pass result equals select_as followed by pack_list", list_equal(packed.get(), two_step.get()));
}

/**
 * @brief SELPACK-02: the counting mode of the pack buffer predicts the packed size exactly
 * @return nothing; CHECK records the results
 */
static void test_select_pack_counting() {
   ocs::tom::RawListPtr source = make_source();
   ocs::tom::RawWherePtr where = group_is(ocs::tom::CompareOp::Equal, 1);
   ocs::tom::RawWhatPtr what = make_nested_what();

   ocs::tom::RawListPtr lp;
   size_t bytes = 0;
   select_pack_unpack(source.get(), where.get(), what.get(), lp, &bytes);
   CHECK(7, "with where and nested what, counting gives the packed size", bytes > 0 &&
         select_pack_count(source.get(), where.get(), what.get()) == bytes);
   lp.reset();

   select_pack_unpack(source.get(), nullptr, nullptr, lp, &bytes);
   CHECK(8, "without where and what, counting gives the packed size", bytes > 0 &&
         select_pack_count(source.get(), nullptr, nullptr) == bytes);
}

/**
 * @brief SELPACK-03: select-and-pack with nothing to select
 * @return nothing; CHECK records the results
 */
static void test_select_pack_empty() {
   ocs::tom::RawListPtr source = make_source();
   ocs::tom::RawWherePtr none = group_is(ocs::tom::CompareOp::Equal, 7);
   ocs::tom::RawWhatPtr what = make_nested_what();

   ocs::tom::RawListPtr lp;
   size_t bytes = 0;
   int ret = select_pack_unpack(source.get(), none.get(), what.get(), lp, &bytes);
   static const int top[] = {SP_name, SP_group, SP_sub, SP_obj, NoName};
   CHECK(9, "a where matching nothing packs an empty list of the reduced type",
         ret == PACK_SUCCESS && lp && lp->size() == 0 && descr_names_are(descr_of(lp.get()), top));
   lp.reset();

   ocs::tom::RawListPtr selected = select_old("selected", source.get(), none.get(), what.get());
   CHECK(10, "select_as returns no list at all for the same where", !selected);

   ret = select_pack_unpack(nullptr, nullptr, what.get(), lp, &bytes);
   CHECK(11, "a missing source list packs as a missing list", ret == PACK_SUCCESS && !lp);
}

/**
 * @brief SELPACK-04: select-and-pack of whole elements
 * @return nothing; CHECK records the results
 */
static void test_select_pack_all_fields() {
   ocs::tom::RawListPtr source = make_source();

   ocs::tom::RawListPtr lp;
   size_t bytes = 0;
   int ret = select_pack_unpack(source.get(), nullptr, nullptr, lp, &bytes);
   CHECK(12, "without where and what the whole list arrives unchanged",
         ret == PACK_SUCCESS && list_equal(source.get(), lp.get()));
   lp.reset();

   ocs::tom::RawWherePtr where = group_is(ocs::tom::CompareOp::Equal, 2);
   ocs::tom::RawWhatPtr all = ocs::tom::what_all();
   ret = select_pack_unpack(source.get(), where.get(), all.get(), lp, &bytes);
   ocs::tom::RawListPtr selected = select_old("selected", source.get(), where.get(), all.get());
   CHECK(13, "a where with what ALL packs the full elements select_as selects",
         ret == PACK_SUCCESS && lp->size() == 2 && list_equal(lp.get(), selected.get()));
}

/**
 * @brief SELPACK-05: partial_descr builds the reduced descriptor at the full length of the type
 * @return nothing; CHECK records the results
 */
static void test_partial_descr() {
   ocs::tom::RawWhatPtr what = what_of(SP_Type, {SP_spool, SP_name, SP_group});
   const int n = ocs::tom::count_what(what.get(), SP_Type);
   std::vector<lDescr> dp(SP_Size + 1);
   int index = 0;
   const int ret = ocs::tom::partial_descr(what.get(), SP_Type, dp.data(), &index);

   // the selection brings the fields into the order of the descriptor (CS-2997); the result has the full length
   // (CS-439)
   const int pos_name = ocs::tom::field_pos(SP_Type, SP_name);
   const int pos_group = ocs::tom::field_pos(SP_Type, SP_group);
   const int pos_spool = ocs::tom::field_pos(SP_Type, SP_spool);
   std::vector<std::pair<int, int>> present;
   for (const auto f : ocs::tom::present_fields(dp.data())) {
      present.emplace_back(f.pos, f.slot);
   }
   CHECK(14, "partial_descr keeps the length of the type and gives the fields their slots in type order",
         ret == 0 && n == 3 && index == static_cast<int>(SP_Size) && lCountPresent(dp.data()) == 3 &&
         present == (std::vector<std::pair<int, int>>{{pos_name, 0}, {pos_group, 1}, {pos_spool, 2}}));

   bool reduced = true;
   for (size_t i = 0; i <= SP_Size; i++) {
      reduced = reduced && (dp[i].mt & CULL_IS_REDUCED) != 0;
   }
   CHECK(15, "every field and the end mark carry CULL_IS_REDUCED", reduced);

   CHECK(16, "the field attributes of the source are kept",
         mt_get_type(dp[pos_name].mt) == MT_STRING && (dp[pos_name].mt & CULL_HASH) != 0 &&
         (dp[pos_name].mt & CULL_UNIQUE) != 0 && (dp[pos_group].mt & CULL_HASH) != 0 &&
         (dp[pos_group].mt & CULL_UNIQUE) == 0 && (dp[pos_spool].mt & CULL_SPOOL) != 0);

   ocs::tom::RawWhatPtr all = ocs::tom::what_all();
   std::vector<lDescr> dp_all(SP_Size + 1);
   index = 0;
   const int ret_all = ocs::tom::partial_descr(all.get(), SP_Type, dp_all.data(), &index);
   CHECK(17, "what ALL copies the descriptor without marking it reduced",
         ret_all == 0 && index == static_cast<int>(SP_Size) &&
         ocs::tom::layout_difference(dp_all.data(), SP_Type) == 0 && (dp_all[0].mt & CULL_IS_REDUCED) == 0);

   lDescr dummy[1];
   index = 0;
   CHECK(18, "partial_descr rejects a missing what", ocs::tom::partial_descr(nullptr, SP_Type, dummy, &index) == -1);
}

/**
 * @brief SELPACK-06: pack_list with flags and with a what
 * @return nothing; CHECK records the results
 */
static void test_pack_list_partial() {
   ocs::tom::RawListPtr source = make_source();

   ocs::tom::RawListPtr lp;
   int ret = PACK_FORMAT;
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_list(&pb.raw(), source.get(), nullptr, CULL_SPOOL);
      ret = unpack_list_from(pb, lp, CULL_SPOOL);
   } catch (const ocs::tom::PackException &e) {
      ret = e.code();
   }
   const ocs::tom::ConstRawElement e2 = lp ? lp->front().next().next() : ocs::tom::ConstRawElement();
   CHECK(19, "with CULL_SPOOL only the spooled fields arrive, the others stay empty",
         ret == PACK_SUCCESS && lp->size() == SOURCE_SIZE && e2 &&
         sge_strnullcmp(e2.get<const char *>(SP_spool), "spool2") == 0 && e2.get<const char *>(SP_name) == nullptr &&
         e2.get<std::uint32_t>(SP_group) == 0 && e2.get<const lListElem *>(SP_obj) == nullptr);
   CHECK(20, "the flags reach into the sublist",
         e2 && e2.get<ocs::tom::ConstRawList>(SP_sub).size() == SUB_SIZE &&
         sge_strnullcmp(e2.get<ocs::tom::ConstRawList>(SP_sub).front().get<const char *>(SPS_name), "s2-0") == 0 &&
         e2.get<ocs::tom::ConstRawList>(SP_sub).front().get<const char *>(SPS_note) == nullptr);
   lp.reset();

   ocs::tom::RawWhatPtr what = make_nested_what();
   ret = PACK_FORMAT;
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_list(&pb.raw(), source.get(), what.get(), 0);
      ret = unpack_list_from(pb, lp);
   } catch (const ocs::tom::PackException &e) {
      ret = e.code();
   }
   ocs::tom::RawListPtr selected = select_old("selected", source.get(), nullptr, what.get());
   CHECK(21, "with a what it packs what select_as selects",
         ret == PACK_SUCCESS && list_equal(lp.get(), selected.get()));
}

/**
 * @brief SELPACK-07: reduced lists, free elements and sub-objects in the pack round trip
 * @return nothing; CHECK records the results
 */
static void test_reduced_and_free() {
   ocs::tom::RawListPtr source = make_source();
   ocs::tom::RawWhatPtr what = make_nested_what();
   ocs::tom::RawListPtr reduced = select_old("reduced", source.get(), nullptr, what.get());

   ocs::tom::RawListPtr lp;
   int ret = pack_unpack_list(reduced.get(), lp);
   CHECK(22, "a reduced list survives the round trip",
         ret == PACK_SUCCESS && lp->size() == SOURCE_SIZE && list_equal(reduced.get(), lp.get()));
   const lDescr *dp = descr_of(lp.get());
   CHECK(23, "the unpacked list is still marked reduced", dp != nullptr && (dp[0].mt & CULL_IS_REDUCED) != 0);
   lp.reset();

   // a free element, not bound to any list, with a sub-object
   ocs::tom::RawElementPtr free_ep = source->front().clone();
   ocs::tom::RawElementPtr copy;
   ret = pack_unpack_elem(free_ep.get(), copy);
   CHECK(24, "a free element with a sub-object survives the round trip",
         ocs::tom::is_free(free_ep.get()) && ret == PACK_SUCCESS && elem_equal(free_ep.get(), copy.get()) &&
         copy->get<const lListElem *>(SP_obj) != nullptr);
   CHECK(25, "the unpacked element is free, its sub-object is bound to it",
         copy && ocs::tom::is_free(copy.get()) && !ocs::tom::is_free(copy->get<ocs::tom::ConstRawElement>(SP_obj)));
   copy.reset();
   free_ep.reset();

   // an element without sub-object and with an empty sublist
   ocs::tom::RawElementPtr odd = source->front().next().clone();
   odd->set(SP_sub, ocs::tom::RawListPtr::create(SPS_Type, ocs::tom::Hashing::On, "empty"));
   ret = pack_unpack_elem(odd.get(), copy);
   CHECK(26, "a missing sub-object stays missing, an empty sublist stays an empty list",
         ret == PACK_SUCCESS && copy && copy->get<const lListElem *>(SP_obj) == nullptr &&
         copy->get<const lList *>(SP_sub) != nullptr && copy->get<ocs::tom::ConstRawList>(SP_sub).size() == 0);
}

/**
 * @brief Do all keys of a unique hashed field resolve to their element?
 * @param lp the list
 * @param nm the field
 * @return true if every element is found through its key
 */
static bool all_keys_found(const ocs::tom::ConstRawList lp, const int nm) {
   for (const ocs::tom::ConstRawElement ep : lp) {
      if (lp.find_first(nm, ep.get<const char *>(nm)).raw() != ep.raw()) {
         return false;
      }
   }
   return true;
}

/**
 * @brief Count the elements a non unique hashed ulong field yields for one value
 * @param lp the list
 * @param nm the field
 * @param value the value
 * @return the number of elements found through the hash iterator of the core
 */
static int count_by_ulong(const ocs::tom::ConstRawList lp, const int nm, const std::uint32_t value) {
   if (!lp) {
      return 0;
   }
   const int pos = ocs::tom::field_pos(lp.descr(), nm);
   const void *iterator = nullptr;
   int count = 0;
   for (const lListElem *ep = ocs::tom::lookup_first<ocs::tom::FieldType::Uint32>(lp, pos, value, &iterator);
        ep != nullptr; ep = ocs::tom::lookup_next<ocs::tom::FieldType::Uint32>(lp, pos, value, &iterator)) {
      count++;
   }
   return count;
}

/**
 * @brief SELPACK-08: the hash tables of an unpacked list
 * @return nothing; CHECK records the results
 */
static void test_hash_after_unpack() {
   ocs::tom::RawListPtr source = make_source();

   ocs::tom::RawListPtr lp;
   int ret = pack_unpack_list(source.get(), lp);
   const lDescr *dp = descr_of(lp.get());
   CHECK(27, "the unpacked list may be hashed on both hashed fields and has no table before a lookup",
         ret == PACK_SUCCESS && dp != nullptr && lp->raw()->nslots == 2 && list_slots(lp.get()) == nullptr &&
         list_hash_table(lp.get(), ocs::tom::field_pos(dp, SP_name)) == nullptr &&
         list_hash_table(lp.get(), ocs::tom::field_pos(dp, SP_group)) == nullptr);
   CHECK(28, "every unique key resolves to its element", all_keys_found(lp.get(), SP_name));
   CHECK(29, "the non unique hash yields every element of a group",
         count_by_ulong(lp.get(), SP_group, 0) == 2 && count_by_ulong(lp.get(), SP_group, 1) == 2 &&
         count_by_ulong(lp.get(), SP_group, 2) == 2);
   const ocs::tom::ConstRawElement e3 = lp->find_first(SP_name, static_cast<const char *>("e3"));
   const ocs::tom::ConstRawList sub = e3 ? e3.get<ocs::tom::ConstRawList>(SP_sub) : ocs::tom::ConstRawList();
   CHECK(30, "the sublists are hashed as well",
         sub && all_keys_found(sub, SPS_name) &&
         list_hash_table(sub, ocs::tom::field_pos(sub.descr(), SPS_name)) != nullptr);
   lp.reset();

   // the reduced list the single pass delivers
   ocs::tom::RawWherePtr where = group_is(ocs::tom::CompareOp::Lower, 2);
   ocs::tom::RawWhatPtr what = make_nested_what();
   size_t bytes = 0;
   ret = select_pack_unpack(source.get(), where.get(), what.get(), lp, &bytes);
   CHECK(31, "keys and groups resolve in the reduced list the single pass delivers",
         ret == PACK_SUCCESS && lp->size() == 4 && all_keys_found(lp.get(), SP_name) &&
         count_by_ulong(lp.get(), SP_group, 0) == 2 && count_by_ulong(lp.get(), SP_group, 1) == 2 &&
         all_keys_found(lp->front().get<ocs::tom::ConstRawList>(SP_sub), SPS_name));
}

/**
 * @brief SELPACK-09: a what on the full type applied to a reduction (CS-439, N-B-11, N-W-11)
 * @return nothing; CHECK records the results
 */
static void test_full_what_on_reduction() {
   ocs::tom::RawListPtr source = make_source();
   ocs::tom::RawWhatPtr reduce = what_of(SP_Type, {SP_name, SP_group, SP_spool});
   ocs::tom::RawListPtr reduced = select_old("reduced", source.get(), nullptr, reduce.get());
   ocs::tom::RawWhatPtr what = what_of(SP_Type, {SP_group, SP_name});

   static const int names[] = {SP_name, SP_group, NoName};
   ocs::tom::RawListPtr selected =
           reduced ? select_old("selected", reduced.get(), nullptr, what.get()) : ocs::tom::RawListPtr();
   const ocs::tom::ConstRawElement first = selected ? selected->front() : ocs::tom::ConstRawElement();
   CHECK(32, "a what on the full type selects from a reduced list that holds its fields",
         selected && selected->size() == SOURCE_SIZE && descr_names_are(descr_of(selected.get()), names) && first &&
         strcmp(first.get<const char *>(SP_name), "e0") == 0 && first.get<std::uint32_t>(SP_group) == 0);

   ocs::tom::RawListPtr packed;
   size_t bytes = 0;
   const int ret = reduced ? select_pack_unpack(reduced.get(), nullptr, what.get(), packed, &bytes) : PACK_FORMAT;
   CHECK(33, "select-and-pack in one pass delivers the same from the reduced list",
         ret == PACK_SUCCESS && list_equal(packed.get(), selected.get()));

   auto full = ocs::tom::RawElementPtr::create(SP_Type);
   bool copied = false;
   if (reduced) {
      try {
         ocs::tom::copy_elem_partial(full.get(), reduced->front(), what.get(), true, false);
         copied = true;
      } catch (const ocs::RuntimeException &) {
         copied = false;
      }
   }
   CHECK(34, "a partial copy from a reduced element into a full one keeps the positions",
         copied && strcmp(full->get<const char *>(SP_name), "e0") == 0 && full->get<std::uint32_t>(SP_group) == 0 &&
         full->get<const char *>(SP_spool) == nullptr);
   full.reset();

   ocs::tom::RawWhatPtr absent = what_of(SP_Type, {SP_name, SP_value});
   ocs::tom::RawListPtr none =
           reduced ? select_old("none", reduced.get(), nullptr, absent.get()) : ocs::tom::RawListPtr();
   ocs::tom::RawListPtr none_packed;
   const int absent_ret =
      reduced ? select_pack_unpack(reduced.get(), nullptr, absent.get(), none_packed, &bytes) : PACK_SUCCESS;
   CHECK(35, "a field the reduction does not hold is rejected by select and by select-and-pack",
         !none && (absent_ret != PACK_SUCCESS || !none_packed));

   // a sender's position is not trusted: the receiver computes it from the field name
   ocs::tom::RawWhatPtr forged = what_of(SP_Type, {SP_group});
   forged.get()[0].pos = 0;
   ocs::tom::RawWhatPtr received;
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_what(&pb.raw(), forged.get());
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      received = ocs::tom::RawWhatPtr(ocs::tom::unpack_what(&in.raw()));
   } catch (const ocs::tom::PackException &) {
      received.reset();
   }
   CHECK(37, "an unpacked what takes the positions of the local type",
         received && received.get()[0].nm == SP_group &&
         received.get()[0].pos == ocs::tom::field_pos(SP_Type, SP_group));
}

// ---------------------------------------------------------------------------

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame.
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   // the lists here are short; the tables built on them by a lookup are what is tested
   ocs::module_test::install_test_model({{"SP", SP_Type, SP_Name, SP_Size}, {"SPS", SPS_Type, SPS_Name, SPS_Size}});
   ocs::tom::detail::hash_set_min_elems(0);

   return ocs::module_test::run(argc, argv, {
      {"SELPACK-01", "single-pass select-and-pack equals the two-step path",
       "select_pack() with a where and a nested what, packing into a pack buffer, delivers to the "
       "receiver what select_as() delivers and what select_as() followed by pack_list() delivers",
       {"select with a where on a hashed field and a what with a nested sublist and sub-object, into a pack buffer",
        "unpack it with unpack_raw_list()",
        "select the same with select_as(), and pack and unpack that result",
        "compare the three lists field by field"},
       "all three lists hold the same two elements with exactly the fields of the what, sublists reduced too",
       {{"CLN-03", "covers"}}, {1, 2, 3, 4, 5, 6}, test_select_pack_equals_select},
      {"SELPACK-02", "the counting mode predicts the packed size",
       "select_pack() into a pack buffer in counting mode counts exactly the bytes it packs in "
       "normal mode",
       {"pack with where and nested what, then count the same", "pack and count without where and what"},
       "the counted size equals the packed size in both cases",
       {{"CLN-03", "covers"}}, {7, 8}, test_select_pack_counting},
      {"SELPACK-03", "select-and-pack with nothing to select",
       "a where matching nothing and a missing source list are packed so the receiver can tell them apart",
       {"select-and-pack with a where matching no element", "select_as with the same where",
        "select-and-pack a missing source list"},
       "the receiver gets an empty list of the reduced type for the first, select_as returns no list, the "
       "receiver gets no list for the missing source",
       {{"CLN-03", "covers"}}, {9, 10, 11}, test_select_pack_empty},
      {"SELPACK-04", "select-and-pack of whole elements",
       "without a what, and with what ALL, select-and-pack delivers the elements with all their fields",
       {"select-and-pack without where and what", "select-and-pack with a where and what ALL, and select_as the same"},
       "the first gives the source list unchanged, the second the same list as select_as",
       {{"CLN-03", "covers"}}, {12, 13}, test_select_pack_all_fields},
      {"SELPACK-05", "partial_descr builds the reduced descriptor",
       "partial_descr() takes the fields of a what in descriptor order, marks them reduced and keeps their attributes",
       {"build a descriptor from a what of three fields in non-descriptor order",
        "build one from what ALL", "call it without a what"},
       "the fields come in type order, at the length of the type, with CULL_IS_REDUCED and their hash, unique and "
       "spool attributes; what ALL "
       "gives the unmarked source descriptor; a missing what is rejected",
       {{"CLN-03", "covers"}}, {14, 15, 16, 17, 18}, test_partial_descr},
      {"SELPACK-06", "pack_list with flags and with a what",
       "pack_list() packs only the flagged fields, sublists included, or the fields of a what",
       {"pack the source list with CULL_SPOOL and unpack it with the same flags",
        "pack it with a nested what and compare with select_as"},
       "only the spooled fields arrive, down into the sublist; with a what the result equals select_as",
       {{"CLN-03", "covers"}}, {19, 20, 21}, test_pack_list_partial},
      {"SELPACK-07", "reduced lists, free elements and sub-objects in the round trip",
       "a reduced list, a free element with a sub-object, and an element without sub-object and with an "
       "empty sublist survive packing and unpacking",
       {"pack and unpack a list returned by select_as", "pack and unpack a free element with a sub-object",
        "pack and unpack an element without sub-object and with an empty sublist"},
       "every value arrives, the list stays reduced, the element stays free and its sub-object bound, a "
       "missing sub-object stays missing and an empty sublist stays empty",
       {{"CLN-03", "covers"}}, {22, 23, 24, 25, 26}, test_reduced_and_free},
      {"SELPACK-08", "hash tables of an unpacked list",
       "an unpacked list, the full one as well as the reduced one of the single pass, finds its elements "
       "through unique and non unique keys, also in sublists",
       {"pack and unpack the source list", "look every element up by its unique key and by its group",
        "do the same for a sublist and for the result of select-and-pack"},
       "the lists have no table before the first lookup, which builds it; every lookup finds the right elements",
       {{"CLN-03", "covers"}}, {27, 28, 29, 30, 31}, test_hash_after_unpack},
      {"SELPACK-09", "a what on the full type applied to a reduction",
       "positions are those of the type in every descriptor, so a what built on the full type selects from "
       "reductions that hold its fields, and a receiver takes the positions of its own type",
       {"reduce a list to three fields", "select, select-and-pack and copy two of them with a what on the full type",
        "select a field the reduction does not hold", "pack a what with a wrong position and unpack it"},
       "the held fields are selected, packed and copied as from the full list, an absent field is rejected, and "
       "the unpacked what has the local position",
       {{"FLD-01", "partly"}}, {32, 33, 34, 35, 37}, test_full_what_on_reduction},
   });
}
