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
 * @brief Module test of the single-pass select-and-pack path in `libs/cull`
 *
 * The qmaster answers a GDI GET request with a where and a what by walking the
 * master list once and packing what it selects straight into the pack buffer
 * (lSelectHashPack() with a pack buffer, ocs_gdi_Packet.cc). Nothing else in
 * the module tests exercises that path. These scenarios pin what it delivers
 * against the two-step path it replaces -- lSelect() followed by
 * cull_pack_list() -- and cover the pieces it is built from: lPartialDescr(),
 * the counting mode of the pack buffer, cull_pack_list_partial(), reduced lists,
 * free elements and sub-objects in the round trip, and the hash tables of an
 * unpacked list. They are the safety net for changing the pack format.
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

#include "cull/cull.h"
#include "cull/cull_whatP.h"

#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "ocs_module_test.h"

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

/** @brief The name space registering both types with CULL */
lNameSpace nmv[] = {
   {1, SP_Size, SP_Name, SP_Type},
   {51, SPS_Size, SPS_Name, SPS_Type},
   {0, 0, nullptr, nullptr}
};

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
   const lDescr *da = lGetElemDescr(a);
   const lDescr *db = lGetElemDescr(b);
   const int n = lCountDescr(da);
   if (n != lCountDescr(db)) {
      fprintf(stderr, "elements have %d and %d fields\n", n, lCountDescr(db));
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
      bool same = true;
      switch (mt_get_type(da[i].mt)) {
         case lStringT:
            same = sge_strnullcmp(lGetPosString(a, i), lGetPosString(b, i)) == 0;
            break;
         case lHostT:
            same = sge_strnullcmp(lGetPosHost(a, i), lGetPosHost(b, i)) == 0;
            break;
         case lUlongT:
            same = lGetPosUlong(a, i) == lGetPosUlong(b, i);
            break;
         case lUlong64T:
            same = lGetPosUlong64(a, i) == lGetPosUlong64(b, i);
            break;
         case lDoubleT:
            same = lGetPosDouble(a, i) == lGetPosDouble(b, i);
            break;
         case lBoolT:
            same = lGetPosBool(a, i) == lGetPosBool(b, i);
            break;
         case lIntT:
            same = lGetPosInt(a, i) == lGetPosInt(b, i);
            break;
         case lLongT:
            same = lGetPosLong(a, i) == lGetPosLong(b, i);
            break;
         case lListT:
            same = list_equal(lGetPosList(a, i), lGetPosList(b, i));
            break;
         case lObjectT:
            same = elem_equal(lGetPosObject(a, i), lGetPosObject(b, i));
            break;
         default:
            break;
      }
      if (!same) {
         fprintf(stderr, "field %s (type %d) differs\n", lNm2Str(da[i].nm), mt_get_type(da[i].mt));
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
   if (lGetNumberOfElem(a) != lGetNumberOfElem(b)) {
      fprintf(stderr, "list %s has %d elements, list %s has %d\n", lGetListName(a), lGetNumberOfElem(a),
              lGetListName(b), lGetNumberOfElem(b));
      return false;
   }
   const lListElem *ea = lFirst(a);
   const lListElem *eb = lFirst(b);
   while (ea != nullptr && eb != nullptr) {
      if (!elem_equal(ea, eb)) {
         return false;
      }
      ea = lNext(ea);
      eb = lNext(eb);
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
   for (const auto f : ocs_cull_present_fields(dp)) {
      if (names[i] == NoName || dp[f.pos].nm != names[i]) {
         return false;
      }
      i++;
   }
   return names[i] == NoName;
}

/**
 * @brief Build the source list
 *
 * SOURCE_SIZE elements e0..e5, SP_group = i % 3, each with a sublist of SUB_SIZE
 * elements s<i>-<k>; the even ones carry a sub-object, the odd ones none.
 *
 * @return the new list, owned by the caller
 */
static lList *make_source() {
   lList *lp = lCreateList("source", SP_Type);
   for (int i = 0; i < SOURCE_SIZE; i++) {
      const std::string n = std::to_string(i);
      lListElem *ep = lAddElemStr(&lp, SP_name, ("e" + n).c_str(), SP_Type);
      lSetUlong(ep, SP_group, i % 3);
      lSetHost(ep, SP_host, ("host" + n).c_str());
      lSetDouble(ep, SP_value, i * 1.5);
      lSetBool(ep, SP_flag, i % 2 == 0);
      lSetUlong64(ep, SP_big, (static_cast<uint64_t>(1) << 40) + i);
      lSetString(ep, SP_spool, ("spool" + n).c_str());
      for (int k = 0; k < SUB_SIZE; k++) {
         const std::string nk = n + "-" + std::to_string(k);
         lListElem *sub = lAddSubStr(ep, SPS_name, ("s" + nk).c_str(), SP_sub, SPS_Type);
         lSetUlong(sub, SPS_count, i * 10 + k);
         lSetString(sub, SPS_note, ("note" + nk).c_str());
      }
      if (i % 2 == 0) {
         lListElem *obj = lCreateElem(SPS_Type);
         lSetString(obj, SPS_name, ("o" + n).c_str());
         lSetUlong(obj, SPS_count, i);
         lSetString(obj, SPS_note, "object");
         lSetObject(ep, SP_obj, obj);
      }
   }
   return lp;
}

/**
 * @brief The what of a typical GET request: a few fields, a nested sublist and sub-object
 * @return the new enumeration, owned by the caller
 */
static lEnumeration *make_nested_what() {
   return lWhat("%T(%I %I %I -> %T(%I %I) %I -> %T(%I))", SP_Type, SP_name, SP_group,
                SP_sub, SPS_Type, SPS_name, SPS_count,
                SP_obj, SPS_Type, SPS_note);
}

/**
 * @brief Make a pack buffer ready for packing, or for counting only
 * @param pb the buffer
 * @param just_count true for the counting mode
 * @return true on success
 */
static bool pb_init(sge_pack_buffer *pb, bool just_count) {
   return init_packbuffer(pb, 1024, just_count, false) == PACK_SUCCESS;
}

/**
 * @brief Unpack a list from what a pack buffer holds, as the receiver does
 *
 * The bytes are copied into a buffer of their own, so the sending buffer is
 * left untouched and the receiver sees exactly what went over the wire.
 *
 * @param pb the packed buffer
 * @param lpp receives the unpacked list
 * @param flags 0 for cull_unpack_list(), else the flags for cull_unpack_list_partial()
 * @return the unpack result, PACK_SUCCESS on success
 */
static int unpack_list_from(const sge_pack_buffer *pb, lList **lpp, int flags = 0) {
   char *buf = sge_malloc(pb->bytes_used);
   memcpy(buf, pb->head_ptr, pb->bytes_used);
   sge_pack_buffer in;
   int ret = init_packbuffer_from_buffer(&in, buf, pb->bytes_used, false);
   if (ret != PACK_SUCCESS) {
      sge_free(&buf);
      return ret;
   }
   ret = flags == 0 ? cull_unpack_list(&in, lpp) : cull_unpack_list_partial(&in, lpp, flags);
   clear_packbuffer(&in);
   return ret;
}

/**
 * @brief Pack a whole list with cull_pack_list() and unpack it again
 * @param lp the list
 * @param lpp receives the unpacked list
 * @return the unpack result, PACK_SUCCESS on success
 */
static int pack_unpack_list(const lList *lp, lList **lpp) {
   sge_pack_buffer pb;
   if (!pb_init(&pb, false)) {
      return PACK_ENOMEM;
   }
   int ret = cull_pack_list(&pb, lp);
   if (ret == PACK_SUCCESS) {
      ret = unpack_list_from(&pb, lpp);
   }
   clear_packbuffer(&pb);
   return ret;
}

/**
 * @brief Pack one element and unpack it again, as the receiver does
 * @param ep the element
 * @param epp receives the unpacked element
 * @return the unpack result, PACK_SUCCESS on success
 */
static int pack_unpack_elem(const lListElem *ep, lListElem **epp) {
   sge_pack_buffer pb;
   if (!pb_init(&pb, false)) {
      return PACK_ENOMEM;
   }
   int ret = cull_pack_elem(&pb, ep);
   if (ret == PACK_SUCCESS) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      ret = init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false);
      if (ret == PACK_SUCCESS) {
         ret = cull_unpack_elem(&in, epp, lGetElemDescr(ep));
         clear_packbuffer(&in);
      } else {
         sge_free(&buf);
      }
   }
   clear_packbuffer(&pb);
   return ret;
}

/**
 * @brief Run lSelectHashPack() into a fresh pack buffer and unpack the result
 * @param slp the source list
 * @param cp the where, may be nullptr
 * @param what the what, may be nullptr
 * @param lpp receives the unpacked list
 * @param bytes receives the number of bytes lSelectHashPack() added, the buffer head excluded
 * @return the unpack result, PACK_SUCCESS on success
 */
static int select_pack_unpack(const lList *slp, const lCondition *cp, const lEnumeration *what, lList **lpp,
                              size_t *bytes) {
   sge_pack_buffer pb;
   if (!pb_init(&pb, false)) {
      return PACK_ENOMEM;
   }
   const size_t head = pb.bytes_used;
   lSelectHashPack("", slp, cp, what, false, &pb);
   *bytes = pb.bytes_used - head;
   const int ret = unpack_list_from(&pb, lpp);
   clear_packbuffer(&pb);
   return ret;
}

/**
 * @brief Count the bytes lSelectHashPack() packs, in the counting mode of the buffer
 *
 * Only the bytes it adds are counted: the head that init_packbuffer() writes in
 * normal mode is not counted in counting mode.
 * @param slp the source list
 * @param cp the where, may be nullptr
 * @param what the what, may be nullptr
 * @return the number of bytes added, or 0 on error
 */
static size_t select_pack_count(const lList *slp, const lCondition *cp, const lEnumeration *what) {
   sge_pack_buffer pb;
   if (!pb_init(&pb, true)) {
      return 0;
   }
   const size_t head = pb.bytes_used;
   lSelectHashPack("", slp, cp, what, false, &pb);
   const size_t bytes = pb.bytes_used - head;
   clear_packbuffer(&pb);
   return bytes;
}

// ---------------------------------------------------------------------------
// scenarios
// ---------------------------------------------------------------------------

/**
 * @brief SELPACK-01: single-pass select-and-pack delivers what lSelect() and cull_pack_list() deliver
 * @return nothing; CHECK records the results
 */
static void test_select_pack_equals_select() {
   lList *source = make_source();
   lCondition *where = lWhere("%T(%I==%u)", SP_Type, SP_group, 1);
   lEnumeration *what = make_nested_what();

   lList *packed = nullptr;
   size_t bytes = 0;
   const int ret = select_pack_unpack(source, where, what, &packed, &bytes);
   CHECK(1, "lSelectHashPack into a pack buffer unpacks with cull_unpack_list", ret == PACK_SUCCESS && packed != nullptr);

   lList *selected = lSelect("selected", source, where, what);
   CHECK(2, "both paths select the two elements of group 1",
         lGetNumberOfElem(packed) == 2 && lGetNumberOfElem(selected) == 2 &&
         lGetElemStr(packed, SP_name, "e1") != nullptr && lGetElemStr(packed, SP_name, "e4") != nullptr);

   static const int top[] = {SP_name, SP_group, SP_sub, SP_obj, NoName};
   static const int sub[] = {SPS_name, SPS_count, NoName};
   CHECK(3, "the unpacked list has exactly the fields of the what, in its order",
         descr_names_are(lGetListDescr(packed), top) && descr_names_are(lGetListDescr(selected), top));

   const lListElem *first = lFirst(packed);
   CHECK(4, "the sublists are reduced to the nested what as well",
         first != nullptr && lGetNumberOfElem(lGetList(first, SP_sub)) == SUB_SIZE &&
         descr_names_are(lGetListDescr(lGetList(first, SP_sub)), sub));

   CHECK(5, "the single-pass result equals the lSelect result field by field", list_equal(packed, selected));

   // the two-step path the single pass replaces: lSelect, then pack the reduced list
   lList *two_step = nullptr;
   pack_unpack_list(selected, &two_step);
   CHECK(6, "the single-pass result equals lSelect followed by cull_pack_list", list_equal(packed, two_step));

   lFreeList(&two_step);
   lFreeList(&selected);
   lFreeList(&packed);
   lFreeWhat(&what);
   lFreeWhere(&where);
   lFreeList(&source);
}

/**
 * @brief SELPACK-02: the counting mode of the pack buffer predicts the packed size exactly
 * @return nothing; CHECK records the results
 */
static void test_select_pack_counting() {
   lList *source = make_source();
   lCondition *where = lWhere("%T(%I==%u)", SP_Type, SP_group, 1);
   lEnumeration *what = make_nested_what();

   lList *lp = nullptr;
   size_t bytes = 0;
   select_pack_unpack(source, where, what, &lp, &bytes);
   CHECK(7, "with where and nested what, counting gives the packed size", bytes > 0 &&
         select_pack_count(source, where, what) == bytes);
   lFreeList(&lp);

   select_pack_unpack(source, nullptr, nullptr, &lp, &bytes);
   CHECK(8, "without where and what, counting gives the packed size", bytes > 0 &&
         select_pack_count(source, nullptr, nullptr) == bytes);
   lFreeList(&lp);

   lFreeWhat(&what);
   lFreeWhere(&where);
   lFreeList(&source);
}

/**
 * @brief SELPACK-03: select-and-pack with nothing to select
 * @return nothing; CHECK records the results
 */
static void test_select_pack_empty() {
   lList *source = make_source();
   lCondition *none = lWhere("%T(%I==%u)", SP_Type, SP_group, 7);
   lEnumeration *what = make_nested_what();

   lList *lp = nullptr;
   size_t bytes = 0;
   int ret = select_pack_unpack(source, none, what, &lp, &bytes);
   static const int top[] = {SP_name, SP_group, SP_sub, SP_obj, NoName};
   CHECK(9, "a where matching nothing packs an empty list of the reduced type",
         ret == PACK_SUCCESS && lp != nullptr && lGetNumberOfElem(lp) == 0 &&
         descr_names_are(lGetListDescr(lp), top));
   lFreeList(&lp);

   lList *selected = lSelect("selected", source, none, what);
   CHECK(10, "lSelect returns no list at all for the same where", selected == nullptr);
   lFreeList(&selected);

   // a list in the result shows that the unpack overwrites it; the unpack does not free it
   lList *dummy = lCreateList("dummy", SP_Type);
   lp = dummy;
   ret = select_pack_unpack(nullptr, nullptr, what, &lp, &bytes);
   CHECK(11, "a missing source list packs as a missing list", ret == PACK_SUCCESS && lp == nullptr);
   lFreeList(&lp);
   lFreeList(&dummy);

   lFreeWhat(&what);
   lFreeWhere(&none);
   lFreeList(&source);
}

/**
 * @brief SELPACK-04: select-and-pack of whole elements
 * @return nothing; CHECK records the results
 */
static void test_select_pack_all_fields() {
   lList *source = make_source();

   lList *lp = nullptr;
   size_t bytes = 0;
   int ret = select_pack_unpack(source, nullptr, nullptr, &lp, &bytes);
   CHECK(12, "without where and what the whole list arrives unchanged",
         ret == PACK_SUCCESS && list_equal(source, lp));
   lFreeList(&lp);

   lCondition *where = lWhere("%T(%I==%u)", SP_Type, SP_group, 2);
   lEnumeration *all = lWhat("%T(ALL)", SP_Type);
   ret = select_pack_unpack(source, where, all, &lp, &bytes);
   lList *selected = lSelect("selected", source, where, all);
   CHECK(13, "a where with what ALL packs the full elements lSelect selects",
         ret == PACK_SUCCESS && lGetNumberOfElem(lp) == 2 && list_equal(lp, selected));
   lFreeList(&selected);
   lFreeList(&lp);

   lFreeWhat(&all);
   lFreeWhere(&where);
   lFreeList(&source);
}

/**
 * @brief SELPACK-05: lPartialDescr builds the reduced descriptor at the full length of the type
 * @return nothing; CHECK records the results
 */
static void test_partial_descr() {
   lEnumeration *what = lWhat("%T(%I %I %I)", SP_Type, SP_spool, SP_name, SP_group);
   const int n = lCountWhat(what, SP_Type);
   std::vector<lDescr> dp(SP_Size + 1);
   int index = 0;
   const int ret = lPartialDescr(what, SP_Type, dp.data(), &index);

   // lWhat brings the fields into the order of the descriptor (CS-2997); the result has the full length (CS-439)
   const int pos_name = lGetPosInDescr(SP_Type, SP_name);
   const int pos_group = lGetPosInDescr(SP_Type, SP_group);
   const int pos_spool = lGetPosInDescr(SP_Type, SP_spool);
   std::vector<std::pair<int, int>> present;
   for (const auto f : ocs_cull_present_fields(dp.data())) {
      present.emplace_back(f.pos, f.slot);
   }
   CHECK(14, "lPartialDescr keeps the length of the type and gives the fields their slots in type order",
         ret == 0 && n == 3 && index == static_cast<int>(SP_Size) && lCountPresent(dp.data()) == 3 &&
         present == (std::vector<std::pair<int, int>>{{pos_name, 0}, {pos_group, 1}, {pos_spool, 2}}));

   bool reduced = true;
   for (size_t i = 0; i <= SP_Size; i++) {
      reduced = reduced && (dp[i].mt & CULL_IS_REDUCED) != 0;
   }
   CHECK(15, "every field and the end mark carry CULL_IS_REDUCED", reduced);

   CHECK(16, "the field attributes of the source are kept",
         mt_get_type(dp[pos_name].mt) == lStringT && (dp[pos_name].mt & CULL_HASH) != 0 &&
         (dp[pos_name].mt & CULL_UNIQUE) != 0 && (dp[pos_group].mt & CULL_HASH) != 0 &&
         (dp[pos_group].mt & CULL_UNIQUE) == 0 && (dp[pos_spool].mt & CULL_SPOOL) != 0);
   lFreeWhat(&what);

   lEnumeration *all = lWhat("%T(ALL)", SP_Type);
   std::vector<lDescr> dp_all(SP_Size + 1);
   index = 0;
   const int ret_all = lPartialDescr(all, SP_Type, dp_all.data(), &index);
   CHECK(17, "what ALL copies the descriptor without marking it reduced",
         ret_all == 0 && index == static_cast<int>(SP_Size) && lCompListDescr(dp_all.data(), SP_Type) == 0 &&
         (dp_all[0].mt & CULL_IS_REDUCED) == 0);
   lFreeWhat(&all);

   lDescr dummy[1];
   index = 0;
   CHECK(18, "lPartialDescr rejects a missing what", lPartialDescr(nullptr, SP_Type, dummy, &index) == -1);
}

/**
 * @brief SELPACK-06: cull_pack_list_partial with flags and with a what
 * @return nothing; CHECK records the results
 */
static void test_pack_list_partial() {
   lList *source = make_source();
   sge_pack_buffer pb;

   lList *lp = nullptr;
   int ret = PACK_FORMAT;
   if (pb_init(&pb, false)) {
      if (cull_pack_list_partial(&pb, source, nullptr, CULL_SPOOL) == PACK_SUCCESS) {
         ret = unpack_list_from(&pb, &lp, CULL_SPOOL);
      }
      clear_packbuffer(&pb);
   }
   const lListElem *e2 = lFirst(lp) != nullptr ? lNext(lNext(lFirst(lp))) : nullptr;
   CHECK(19, "with CULL_SPOOL only the spooled fields arrive, the others stay empty",
         ret == PACK_SUCCESS && lGetNumberOfElem(lp) == SOURCE_SIZE && e2 != nullptr &&
         sge_strnullcmp(lGetString(e2, SP_spool), "spool2") == 0 && lGetString(e2, SP_name) == nullptr &&
         lGetUlong(e2, SP_group) == 0 && lGetObject(e2, SP_obj) == nullptr);
   CHECK(20, "the flags reach into the sublist",
         e2 != nullptr && lGetNumberOfElem(lGetList(e2, SP_sub)) == SUB_SIZE &&
         sge_strnullcmp(lGetString(lFirst(lGetList(e2, SP_sub)), SPS_name), "s2-0") == 0 &&
         lGetString(lFirst(lGetList(e2, SP_sub)), SPS_note) == nullptr);
   lFreeList(&lp);

   lEnumeration *what = make_nested_what();
   ret = PACK_FORMAT;
   if (pb_init(&pb, false)) {
      if (cull_pack_list_partial(&pb, source, what, 0) == PACK_SUCCESS) {
         ret = unpack_list_from(&pb, &lp);
      }
      clear_packbuffer(&pb);
   }
   lList *selected = lSelect("selected", source, nullptr, what);
   CHECK(21, "with a what it packs what lSelect selects", ret == PACK_SUCCESS && list_equal(lp, selected));
   lFreeList(&selected);
   lFreeList(&lp);
   lFreeWhat(&what);
   lFreeList(&source);
}

/**
 * @brief SELPACK-07: reduced lists, free elements and sub-objects in the pack round trip
 * @return nothing; CHECK records the results
 */
static void test_reduced_and_free() {
   lList *source = make_source();
   lEnumeration *what = make_nested_what();
   lList *reduced = lSelect("reduced", source, nullptr, what);

   lList *lp = nullptr;
   int ret = pack_unpack_list(reduced, &lp);
   CHECK(22, "a reduced list survives the round trip",
         ret == PACK_SUCCESS && lGetNumberOfElem(lp) == SOURCE_SIZE && list_equal(reduced, lp));
   const lDescr *dp = lGetListDescr(lp);
   CHECK(23, "the unpacked list is still marked reduced", dp != nullptr && (dp[0].mt & CULL_IS_REDUCED) != 0);
   lFreeList(&lp);

   // a free element, not bound to any list, with a sub-object
   lListElem *free_ep = lCopyElem(lFirst(source));
   lListElem *copy = nullptr;
   ret = pack_unpack_elem(free_ep, &copy);
   CHECK(24, "a free element with a sub-object survives the round trip",
         lIsFreeElem(free_ep) && ret == PACK_SUCCESS && elem_equal(free_ep, copy) &&
         lGetObject(copy, SP_obj) != nullptr);
   CHECK(25, "the unpacked element is free, its sub-object is bound to it",
         copy != nullptr && lIsFreeElem(copy) && !lIsFreeElem(lGetObject(copy, SP_obj)));
   lFreeElem(&copy);
   lFreeElem(&free_ep);

   // an element without sub-object and with an empty sublist
   lListElem *odd = lCopyElem(lNext(lFirst(source)));
   lSetList(odd, SP_sub, lCreateList("empty", SPS_Type));
   ret = pack_unpack_elem(odd, &copy);
   CHECK(26, "a missing sub-object stays missing, an empty sublist stays an empty list",
         ret == PACK_SUCCESS && copy != nullptr && lGetObject(copy, SP_obj) == nullptr &&
         lGetList(copy, SP_sub) != nullptr && lGetNumberOfElem(lGetList(copy, SP_sub)) == 0);
   lFreeElem(&copy);
   lFreeElem(&odd);

   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&source);
}

/**
 * @brief Do all keys of a unique hashed field resolve to their element?
 * @param lp the list
 * @param nm the field
 * @return true if every element is found through its key
 */
static bool all_keys_found(const lList *lp, int nm) {
   const lListElem *ep;
   for_each_ep(ep, lp) {
      if (lGetElemStr(lp, nm, lGetString(ep, nm)) != ep) {
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
 * @return the number of elements found through the hash iterator
 */
static int count_by_ulong(const lList *lp, int nm, lUlong value) {
   const void *iterator = nullptr;
   int count = 0;
   for (const lListElem *ep = lGetElemUlongFirst(lp, nm, value, &iterator); ep != nullptr;
        ep = lGetElemUlongNext(lp, nm, value, &iterator)) {
      count++;
   }
   return count;
}

/**
 * @brief SELPACK-08: the hash tables of an unpacked list
 * @return nothing; CHECK records the results
 */
static void test_hash_after_unpack() {
   lList *source = make_source();

   lList *lp = nullptr;
   int ret = pack_unpack_list(source, &lp);
   const lDescr *dp = lGetListDescr(lp);
   CHECK(27, "the unpacked list may be hashed on both hashed fields and has no table before a lookup",
         ret == PACK_SUCCESS && dp != nullptr && lp->nslots == 2 && list_slots(lp) == nullptr &&
         list_hash_table(lp, lGetPosInDescr(dp, SP_name)) == nullptr &&
         list_hash_table(lp, lGetPosInDescr(dp, SP_group)) == nullptr);
   CHECK(28, "every unique key resolves to its element", all_keys_found(lp, SP_name));
   CHECK(29, "the non unique hash yields every element of a group",
         count_by_ulong(lp, SP_group, 0) == 2 && count_by_ulong(lp, SP_group, 1) == 2 &&
         count_by_ulong(lp, SP_group, 2) == 2);
   const lList *sub = lGetList(lGetElemStr(lp, SP_name, "e3"), SP_sub);
   CHECK(30, "the sublists are hashed as well",
         sub != nullptr && all_keys_found(sub, SPS_name) &&
         list_hash_table(sub, lGetPosInDescr(lGetListDescr(sub), SPS_name)) != nullptr);
   lFreeList(&lp);

   // the reduced list the single pass delivers
   lCondition *where = lWhere("%T(%I<%u)", SP_Type, SP_group, 2);
   lEnumeration *what = make_nested_what();
   size_t bytes = 0;
   ret = select_pack_unpack(source, where, what, &lp, &bytes);
   CHECK(31, "keys and groups resolve in the reduced list the single pass delivers",
         ret == PACK_SUCCESS && lGetNumberOfElem(lp) == 4 && all_keys_found(lp, SP_name) &&
         count_by_ulong(lp, SP_group, 0) == 2 && count_by_ulong(lp, SP_group, 1) == 2 &&
         all_keys_found(lGetList(lFirst(lp), SP_sub), SPS_name));
   lFreeList(&lp);
   lFreeWhat(&what);
   lFreeWhere(&where);
   lFreeList(&source);
}

/**
 * @brief SELPACK-09: a what on the full type applied to a reduction (CS-439, N-B-11, N-W-11)
 * @return nothing; CHECK records the results
 */
static void test_full_what_on_reduction() {
   lList *source = make_source();
   lEnumeration *reduce = lWhat("%T(%I %I %I)", SP_Type, SP_name, SP_group, SP_spool);
   lList *reduced = lSelect("reduced", source, nullptr, reduce);
   lEnumeration *what = lWhat("%T(%I %I)", SP_Type, SP_group, SP_name);

   static const int names[] = {SP_name, SP_group, NoName};
   lList *selected = reduced != nullptr ? lSelect("selected", reduced, nullptr, what) : nullptr;
   const lListElem *first = selected != nullptr ? lFirst(selected) : nullptr;
   CHECK(32, "a what on the full type selects from a reduced list that holds its fields",
         selected != nullptr && lGetNumberOfElem(selected) == SOURCE_SIZE &&
         descr_names_are(lGetListDescr(selected), names) && first != nullptr &&
         strcmp(lGetString(first, SP_name), "e0") == 0 && lGetUlong(first, SP_group) == 0);

   lList *packed = nullptr;
   size_t bytes = 0;
   const int ret = reduced != nullptr ? select_pack_unpack(reduced, nullptr, what, &packed, &bytes) : PACK_FORMAT;
   CHECK(33, "select-and-pack in one pass delivers the same from the reduced list",
         ret == PACK_SUCCESS && list_equal(packed, selected));

   lListElem *full = lCreateElem(SP_Type);
   const bool copied = reduced != nullptr && lCopyElemPartialPack(full, lFirst(reduced), what, true, nullptr) == 0;
   CHECK(34, "a partial copy from a reduced element into a full one keeps the positions",
         copied && strcmp(lGetString(full, SP_name), "e0") == 0 && lGetUlong(full, SP_group) == 0 &&
         lGetString(full, SP_spool) == nullptr);
   lFreeElem(&full);

   lEnumeration *absent = lWhat("%T(%I %I)", SP_Type, SP_name, SP_value);
   lList *none = reduced != nullptr ? lSelect("none", reduced, nullptr, absent) : nullptr;
   lList *none_packed = nullptr;
   const int absent_ret =
      reduced != nullptr ? select_pack_unpack(reduced, nullptr, absent, &none_packed, &bytes) : PACK_SUCCESS;
   CHECK(35, "a field the reduction does not hold is rejected by select and by select-and-pack",
         none == nullptr && (absent_ret != PACK_SUCCESS || none_packed == nullptr));
   lFreeList(&none);
   lFreeList(&none_packed);
   lFreeWhat(&absent);

   lEnumeration *neg = reduced != nullptr ? lWhat("%T(!(%I))", lGetListDescr(reduced), SP_group) : nullptr;
   static const int neg_names[] = {SP_name, SP_spool, NoName};
   lList *neg_selected = neg != nullptr ? lSelect("neg", reduced, nullptr, neg) : nullptr;
   CHECK(36, "a negated what on a reduced descriptor selects its other present fields",
         neg != nullptr && lCountWhat(neg, lGetListDescr(reduced)) == 2 && neg_selected != nullptr &&
         descr_names_are(lGetListDescr(neg_selected), neg_names));
   lFreeList(&neg_selected);
   lFreeWhat(&neg);

   // a sender's position is not trusted: the receiver computes it from the field name
   lEnumeration *forged = lWhat("%T(%I)", SP_Type, SP_group);
   forged[0].pos = 0;
   sge_pack_buffer pb;
   lEnumeration *received = nullptr;
   if (pb_init(&pb, false)) {
      if (cull_pack_enum(&pb, forged) == PACK_SUCCESS) {
         char *buf = sge_malloc(pb.bytes_used);
         memcpy(buf, pb.head_ptr, pb.bytes_used);
         sge_pack_buffer in;
         if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
            cull_unpack_enum(&in, &received);
         }
         clear_packbuffer(&in);
      }
      clear_packbuffer(&pb);
   }
   CHECK(37, "an unpacked what takes the positions of the local type",
         received != nullptr && received[0].nm == SP_group && received[0].pos == lGetPosInDescr(SP_Type, SP_group));
   lFreeWhat(&received);
   lFreeWhat(&forged);

   lFreeList(&packed);
   lFreeList(&selected);
   lFreeWhat(&what);
   lFreeList(&reduced);
   lFreeWhat(&reduce);
   lFreeList(&source);
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
   lInit(nmv, 0);

   return ocs::module_test::run(argc, argv, {
      {"SELPACK-01", "single-pass select-and-pack equals the two-step path",
       "lSelectHashPack() with a where and a nested what, packing into a pack buffer, delivers to the "
       "receiver what lSelect() delivers and what lSelect() followed by cull_pack_list() delivers",
       {"select with a where on a hashed field and a what with a nested sublist and sub-object, into a pack buffer",
        "unpack it with cull_unpack_list()",
        "select the same with lSelect(), and pack and unpack that result",
        "compare the three lists field by field"},
       "all three lists hold the same two elements with exactly the fields of the what, sublists reduced too",
       {{"CLN-03", "covers"}}, {1, 2, 3, 4, 5, 6}, test_select_pack_equals_select},
      {"SELPACK-02", "the counting mode predicts the packed size",
       "lSelectHashPack() into a pack buffer in counting mode counts exactly the bytes it packs in "
       "normal mode",
       {"pack with where and nested what, then count the same", "pack and count without where and what"},
       "the counted size equals the packed size in both cases",
       {{"CLN-03", "covers"}}, {7, 8}, test_select_pack_counting},
      {"SELPACK-03", "select-and-pack with nothing to select",
       "a where matching nothing and a missing source list are packed so the receiver can tell them apart",
       {"select-and-pack with a where matching no element", "lSelect with the same where",
        "select-and-pack a missing source list"},
       "the receiver gets an empty list of the reduced type for the first, lSelect returns no list, the "
       "receiver gets no list for the missing source",
       {{"CLN-03", "covers"}}, {9, 10, 11}, test_select_pack_empty},
      {"SELPACK-04", "select-and-pack of whole elements",
       "without a what, and with what ALL, select-and-pack delivers the elements with all their fields",
       {"select-and-pack without where and what", "select-and-pack with a where and what ALL, and lSelect the same"},
       "the first gives the source list unchanged, the second the same list as lSelect",
       {{"CLN-03", "covers"}}, {12, 13}, test_select_pack_all_fields},
      {"SELPACK-05", "lPartialDescr builds the reduced descriptor",
       "lPartialDescr() takes the fields of a what in descriptor order, marks them reduced and keeps their attributes",
       {"build a descriptor from a what of three fields in non-descriptor order",
        "build one from what ALL", "call it without a what"},
       "the fields come in type order, at the length of the type, with CULL_IS_REDUCED and their hash, unique and "
       "spool attributes; what ALL "
       "gives the unmarked source descriptor; a missing what is rejected",
       {{"CLN-03", "covers"}}, {14, 15, 16, 17, 18}, test_partial_descr},
      {"SELPACK-06", "cull_pack_list_partial with flags and with a what",
       "cull_pack_list_partial() packs only the flagged fields, sublists included, or the fields of a what",
       {"pack the source list with CULL_SPOOL and unpack it with the same flags",
        "pack it with a nested what and compare with lSelect"},
       "only the spooled fields arrive, down into the sublist; with a what the result equals lSelect",
       {{"CLN-03", "covers"}}, {19, 20, 21}, test_pack_list_partial},
      {"SELPACK-07", "reduced lists, free elements and sub-objects in the round trip",
       "a reduced list, a free element with a sub-object, and an element without sub-object and with an "
       "empty sublist survive packing and unpacking",
       {"pack and unpack a list returned by lSelect", "pack and unpack a free element with a sub-object",
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
        "select a field the reduction does not hold", "negate a field on the reduced descriptor",
        "pack a what with a wrong position and unpack it"},
       "the held fields are selected, packed and copied as from the full list, an absent field is rejected, the "
       "negation keeps the other present fields, and the unpacked what has the local position",
       {{"FLD-01", "partly"}}, {32, 33, 34, 35, 36, 37}, test_full_what_on_reduction},
   });
}
