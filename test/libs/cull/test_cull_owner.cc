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
 * @brief Module test of the owner tag of a CULL element (CS-2995, HTL-01)
 *
 * The tag replaces the former element status: its two low bits hold the state, the
 * upper bits the list of a bound element. The test checks the layout, every state
 * transition, and that the pack format still carries the former status values.
 * The hash tables belong to the list that holds an element (HTL-02): the test
 * reads them directly, because a lookup through lGetElemStr() and its kind falls
 * back to a linear search and would hide a table that is wrong or missing.
 */

#include <cstdint>
#include <cstring>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_listP.h"
#include "cull/cull_hash.h"

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"

/** @brief The element type: hashed keys, a sublist and a sub-object */
enum {
   OW_name = 1, ///< unique hashed key
   OW_sub,      ///< a sublist of OWS_Type
   OW_obj,      ///< a sub-object of OWS_Type
   OW_group,    ///< non unique hashed key
   OW_count     ///< not hashed in the type, hashed later with cull_hash_new()
};

/** @brief The type of the sublist and of the sub-object */
enum {
   OWS_name = 51 ///< a string
};

LISTDEF(OW_Type)
   SGE_STRING(OW_name, CULL_DEFAULT | CULL_HASH | CULL_UNIQUE)
   SGE_LIST(OW_sub, OWS_Type, CULL_DEFAULT)
   SGE_OBJECT(OW_obj, OWS_Type, CULL_DEFAULT)
   SGE_ULONG(OW_group, CULL_DEFAULT | CULL_HASH)
   SGE_ULONG(OW_count, CULL_DEFAULT)
LISTEND

LISTDEF(OWS_Type)
   SGE_STRING(OWS_name, CULL_DEFAULT)
LISTEND

NAMEDEF(OW_Name)
   NAME("OW_name")
   NAME("OW_sub")
   NAME("OW_obj")
   NAME("OW_group")
   NAME("OW_count")
NAMEEND

NAMEDEF(OWS_Name)
   NAME("OWS_name")
NAMEEND

#define OW_Size sizeof(OW_Name) / sizeof(char *)   ///< number of fields of OW_Type
#define OWS_Size sizeof(OWS_Name) / sizeof(char *) ///< number of fields of OWS_Type

/** @brief The name space registering both types with CULL */
lNameSpace nmv[] = {
   {1, OW_Size, OW_Name, OW_Type},
   {51, OWS_Size, OWS_Name, OWS_Type},
   {0, 0, nullptr, nullptr}
};

/**
 * @brief Whether an element carries a state and, for a bound element, the list
 *
 * @param ep the element
 * @param kind the expected state
 * @param lp the expected list; nullptr for every state but BOUND_ELEM
 * @return true if state and list are as expected
 */
static bool is_owned(const lListElem *ep, cull_elem_kind kind, const lList *lp) {
   return ep != nullptr && elem_kind(ep) == kind && elem_list(ep) == lp;
}

/**
 * @brief Create an element of OW_Type with a sub-object and two sublist elements
 *
 * @param name the key
 * @return the free element
 */
static lListElem *make_elem(const char *name) {
   lListElem *ep = lCreateElem(OW_Type);
   lSetString(ep, OW_name, name);
   lListElem *obj = lCreateElem(OWS_Type);
   lSetString(obj, OWS_name, "object");
   lSetObject(ep, OW_obj, obj);
   lAddSubStr(ep, OWS_name, "s1", OW_sub, OWS_Type);
   lAddSubStr(ep, OWS_name, "s2", OW_sub, OWS_Type);
   return ep;
}

/**
 * @brief Pack an element and return the first word of the buffer, the status value on the wire
 *
 * @param ep the element
 * @param wire receives the packed status value
 * @return true if packing and reading back succeeded
 */
static bool packed_status(const lListElem *ep, uint32_t *wire) {
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024, false, false) != PACK_SUCCESS) {
      return false;
   }
   bool ok = cull_pack_elem(&pb, ep) == PACK_SUCCESS;
   if (ok) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      ok = init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS
           && unpackint(&in, wire) == PACK_SUCCESS;
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   return ok;
}

/**
 * @brief Pack an element and unpack it again, as a receiver does
 *
 * @param ep the element
 * @param epp receives the unpacked element
 * @return the unpack result, PACK_SUCCESS on success
 */
static int pack_unpack(const lListElem *ep, lListElem **epp) {
   sge_pack_buffer pb;
   int ret = init_packbuffer(&pb, 1024, false, false);
   if (ret == PACK_SUCCESS) {
      ret = cull_pack_elem(&pb, ep);
   }
   if (ret == PACK_SUCCESS) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      ret = init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false);
      if (ret == PACK_SUCCESS) {
         ret = cull_unpack_elem(&in, epp, lGetElemDescr(ep));
      }
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   return ret;
}

/**
 * @brief Look a string key up in the hash table a list holds for a field, not by a search
 *
 * @param lp the list
 * @param nm the hashed string field
 * @param key the key
 * @return the element the table holds for the key, or nullptr when the table has none or does not exist
 */
static const lListElem *table_str(const lList *lp, int nm, const char *key) {
   const int pos = lGetPosInDescr(lGetListDescr(lp), nm);
   const void *iterator = nullptr;
   return cull_hash_first(list_hash_table(lp, pos), key, mt_is_unique(lGetListDescr(lp)[pos].mt), &iterator);
}

/**
 * @brief Look an ulong key up in the hash table a list holds for a field, not by a search
 *
 * For a non unique table the first element of the key's chain is returned.
 *
 * @param lp the list
 * @param nm the hashed ulong field
 * @param key the key
 * @return the element the table holds for the key, or nullptr when the table has none or does not exist
 */
static const lListElem *table_ulong(const lList *lp, int nm, uint32_t key) {
   const int pos = lGetPosInDescr(lGetListDescr(lp), nm);
   const void *iterator = nullptr;
   return cull_hash_first(list_hash_table(lp, pos), &key, mt_is_unique(lGetListDescr(lp)[pos].mt), &iterator);
}

/**
 * @brief Whether a moved element is found under its new keys in the target only
 *
 * The element's unique name and its group are changed after the move; the
 * target's tables must hold it under the new keys, under the old name nothing
 * may be left, and the source's tables must not know it under either.
 *
 * @param ep the moved element, bound to @p target
 * @param source the list it came from
 * @param target the list it is in now
 * @param old_name its name before the change
 * @param new_name the name it gets
 * @param group the group it gets, unused by any other element of both lists
 * @return true if both tables of both lists are as expected
 */
static bool follows_owner(lListElem *ep, const lList *source, const lList *target, const char *old_name,
                          const char *new_name, uint32_t group) {
   lSetString(ep, OW_name, new_name);
   lSetUlong(ep, OW_group, group);
   return table_str(target, OW_name, new_name) == ep && table_str(target, OW_name, old_name) == nullptr
          && table_ulong(target, OW_group, group) == ep && table_str(source, OW_name, new_name) == nullptr
          && table_str(source, OW_name, old_name) == nullptr && table_ulong(source, OW_group, group) == nullptr;
}

/**
 * @brief OWNER-01: the element keeps its size and the tag holds the list
 *
 * @return nothing; CHECK records the results
 */
static void test_layout() {
   CHECK(1, "an element is 40 bytes", sizeof(lListElem) == 40);

   lList *lp = lCreateList("layout", OW_Type);
   lListElem *ep = make_elem("a");
   lAppendElem(lp, ep);
   CHECK(2, "a heap list leaves the two low bits of its address free",
         (reinterpret_cast<uintptr_t>(lp) & 3) == 0);
   CHECK(3, "the tag of a bound element yields its list", is_owned(ep, BOUND_ELEM, lp));
   lFreeList(&lp);
}

/**
 * @brief OWNER-02: every state transition sets the state and the list
 *
 * @return nothing; CHECK records the results
 */
static void test_transitions() {
   lList *lp = lCreateList("source", OW_Type);
   lListElem *a = make_elem("a");
   CHECK(4, "a created element is free", is_owned(a, FREE_ELEM, nullptr));

   lAppendElem(lp, a);
   lListElem *b = make_elem("b");
   lInsertElem(lp, nullptr, b);
   lListElem *c = make_elem("c");
   lInsertElem(lp, a, c);
   CHECK(5, "appended and inserted elements are bound to the list",
         is_owned(a, BOUND_ELEM, lp) && is_owned(b, BOUND_ELEM, lp) && is_owned(c, BOUND_ELEM, lp));

   const lListElem *s1 = lFirst(lGetList(a, OW_sub));
   CHECK(6, "a sublist element is bound to the sublist, a sub-object is an object",
         is_owned(s1, BOUND_ELEM, lGetList(a, OW_sub))
         && is_owned(lGetObject(a, OW_obj), OBJECT_ELEM, nullptr));

   // list order is b, a, c: move a and c into a new list
   lList *target = nullptr;
   lDechainList(lp, &target, a);
   CHECK(7, "lDechainList binds every moved element to the target and none stays with the source",
         target != nullptr && is_owned(a, BOUND_ELEM, target) && is_owned(c, BOUND_ELEM, target)
         && is_owned(b, BOUND_ELEM, lp) && lGetNumberOfElem(lp) == 1 && lGetNumberOfElem(target) == 2);

   lListElem *d = lDechainElem(target, c);
   CHECK(8, "a dechained element is free", is_owned(d, FREE_ELEM, nullptr));

   lListElem *obj = lDechainObject(a, OW_obj);
   CHECK(9, "a dechained sub-object is free", is_owned(obj, FREE_ELEM, nullptr));
   lSetObject(d, OW_obj, obj);
   CHECK(10, "an object set into a field is an object", is_owned(obj, OBJECT_ELEM, nullptr));

   lListElem *copy = lCopyElem(b);
   CHECK(11, "the copy of a bound element is free, its sub-object an object, its sublist elements bound "
             "to the copy's sublist",
         is_owned(copy, FREE_ELEM, nullptr) && is_owned(lGetObject(copy, OW_obj), OBJECT_ELEM, nullptr)
         && is_owned(lFirst(lGetList(copy, OW_sub)), BOUND_ELEM, lGetList(copy, OW_sub)));

   // free in every state: bound (through the lists), free, and the objects they hold
   lFreeElem(&copy);
   lFreeElem(&d);
   lFreeList(&target);
   lFreeList(&lp);
   CHECK(12, "elements are freed in every state", copy == nullptr && d == nullptr && target == nullptr
                                                   && lp == nullptr);
}

/**
 * @brief OWNER-03: the pack format still carries the former status values
 *
 * @return nothing; CHECK records the results
 */
static void test_pack() {
   lList *lp = lCreateList("pack", OW_Type);
   lListElem *bound = make_elem("bound");
   lAppendElem(lp, bound);
   lListElem *free_ep = make_elem("free");

   uint32_t w_free = 0;
   uint32_t w_bound = 0;
   uint32_t w_object = 0;
   CHECK(13, "free, bound and object elements are packed with the former status values 1, 2 and 8",
         packed_status(free_ep, &w_free) && w_free == 1 && packed_status(bound, &w_bound) && w_bound == 2
         && packed_status(lGetObject(bound, OW_obj), &w_object) && w_object == 8);

   lListElem *from_free = nullptr;
   lListElem *from_bound = nullptr;
   CHECK(14, "an unpacked free element is free, an unpacked bound element is transitionally bound",
         pack_unpack(free_ep, &from_free) == PACK_SUCCESS && is_owned(from_free, FREE_ELEM, nullptr)
         && pack_unpack(bound, &from_bound) == PACK_SUCCESS
         && is_owned(from_bound, TRANS_BOUND_ELEM, nullptr));

   uint32_t w_trans = 0;
   CHECK(15, "a transitionally bound element is packed with the former status value 4",
         packed_status(from_bound, &w_trans) && w_trans == 4);

   // a transitionally bound element may be appended; it is bound afterwards
   lList *lp2 = lCreateList("pack2", OW_Type);
   lAppendElem(lp2, from_bound);
   CHECK(16, "a transitionally bound element becomes bound when appended", is_owned(from_bound, BOUND_ELEM, lp2));

   // a whole list: every element is bound to the unpacked list
   sge_pack_buffer pb;
   lList *unpacked = nullptr;
   bool ok = init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS && cull_pack_list(&pb, lp) == PACK_SUCCESS;
   if (ok) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      ok = init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS
           && cull_unpack_list(&in, &unpacked) == PACK_SUCCESS;
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   CHECK(17, "the elements of an unpacked list are bound to it, their sub-objects are objects",
         ok && unpacked != nullptr && is_owned(lFirst(unpacked), BOUND_ELEM, unpacked)
         && is_owned(lGetObject(lFirst(unpacked), OW_obj), OBJECT_ELEM, nullptr));

   // a status value the format does not know is refused
   lListElem *bad = nullptr;
   ok = init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS && packint(&pb, 16) == PACK_SUCCESS;
   int ret = PACK_SUCCESS;
   if (ok) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      ok = init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS;
      if (ok) {
         ret = cull_unpack_elem(&in, &bad, OW_Type);
      }
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   CHECK(18, "an unknown status value is refused with PACK_FORMAT", ok && ret == PACK_FORMAT && bad == nullptr);

   lFreeElem(&free_ep);
   lFreeElem(&from_free);
   lFreeList(&unpacked);
   lFreeList(&lp2);
   lFreeList(&lp);
}

/**
 * @brief OWNER-04: hash maintenance follows the owning list
 *
 * @return nothing; CHECK records the results
 */
static void test_hash_follows_owner() {
   lList *source = lCreateList("source", OW_Type);
   for (const char *name : {"a", "b", "c", "d"}) {
      lListElem *ep = lCreateElem(OW_Type);
      lSetString(ep, OW_name, name);
      lSetUlong(ep, OW_group, 1);
      lAppendElem(source, ep);
   }
   CHECK(19, "a created list holds a table for each hashed field, filled by appending",
         source->nslots == 2 && table_str(source, OW_name, "a") == lFirst(source)
         && table_ulong(source, OW_group, 1) == lFirst(source)
         && list_hash_table(source, lGetPosInDescr(OW_Type, OW_count)) == nullptr);

   // dechain and append
   lList *target = lCreateList("target", OW_Type);
   lListElem *a = lDechainElem(source, lFirstRW(source));
   lAppendElem(target, a);
   CHECK(20, "after dechain and append a changed key is maintained in the target's tables only",
         follows_owner(a, source, target, "a", "a2", 20));

   // lDechainList moves c and d into a new list
   lList *moved = nullptr;
   lListElem *c = lNextRW(lFirstRW(source));
   lListElem *d = lNextRW(c);
   lDechainList(source, &moved, c);
   CHECK(21, "after lDechainList a changed key is maintained in the new list's tables only",
         moved != nullptr && follows_owner(c, source, moved, "c", "c2", 21)
         && follows_owner(d, source, moved, "d", "d2", 22) && table_str(source, OW_name, "b") == lFirst(source));

   // unpacking a list hashes its elements in the unpacked list
   sge_pack_buffer pb;
   lList *unpacked = nullptr;
   bool ok = init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS && cull_pack_list(&pb, moved) == PACK_SUCCESS;
   if (ok) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      ok = init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS
           && cull_unpack_list(&in, &unpacked) == PACK_SUCCESS;
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   lListElem *u = unpacked != nullptr ? lFirstRW(unpacked) : nullptr;
   const bool hashed_on_unpack = ok && u != nullptr && table_str(unpacked, OW_name, "c2") == u;
   if (u != nullptr) {
      lSetString(u, OW_name, "c3");
   }
   CHECK(22, "the elements of an unpacked list are maintained in that list's tables only",
         hashed_on_unpack && table_str(unpacked, OW_name, "c3") == u && table_str(unpacked, OW_name, "c2") == nullptr
         && table_str(moved, OW_name, "c3") == nullptr && table_str(moved, OW_name, "c2") == c);

   // a hash added to a live list on a field the type does not hash
   lSetUlong(lFirstRW(source), OW_count, 5);
   CHECK(23, "cull_hash_new adds a slot with a table over the existing elements, maintained afterwards",
         cull_hash_new(source, OW_count, false) == 1 && source->nslots == 3
         && table_ulong(source, OW_count, 5) == lFirst(source)
         && lSetUlong(lFirstRW(source), OW_count, 6) == 0 && table_ulong(source, OW_count, 6) == lFirst(source)
         && table_ulong(source, OW_count, 5) == nullptr);

   // sorting rebuilds the non unique chains in the new list order
   lSetUlong(c, OW_group, 9);
   lSetUlong(d, OW_group, 9);
   lPSortList(moved, "%I-", OW_name);
   CHECK(24, "after sorting the chain of a non unique key starts with the first element in list order",
         lFirst(moved) == d && table_ulong(moved, OW_group, 9) == d);

   lFreeList(&unpacked);
   lFreeList(&moved);
   lFreeList(&target);
   lFreeList(&source);
}

int main(int argc, char *argv[]) {
   lInit(nmv);

   return ocs::module_test::run(argc, argv, {
      {"OWNER-01", "element layout",
       "the owner tag replaces the element status without growing the element",
       {"check the size of an element", "bind an element to a heap list and read the list back from the tag"},
       "the element is 40 bytes, the list address leaves the tag bits free, the tag yields the list",
       {{"HTL-01", "covers"}}, {1, 2, 3}, test_layout},
      {"OWNER-02", "state transitions",
       "create, append, insert, lDechainList, dechain, object dechain and set, copy and free set the state "
       "and the list of every element involved",
       {"create elements with a sublist and a sub-object", "append and insert them",
        "move part of the list with lDechainList", "dechain an element and a sub-object",
        "set the sub-object into another element", "copy a bound element", "free everything"},
       "every element has the expected state, bound elements name the list that holds them",
       {{"HTL-01", "covers"}}, {4, 5, 6, 7, 8, 9, 10, 11, 12}, test_transitions},
      {"OWNER-03", "pack format of the state",
       "the state is packed with the former status values and unpacked as before",
       {"pack free, bound, object and transitionally bound elements and read the status value",
        "unpack free and bound elements", "append a transitionally bound element", "pack and unpack a list",
        "unpack an element with an unknown status value"},
       "the values are 1, 2, 8 and 4; unpacked bound elements are transitionally bound until appended; "
       "unpacked list elements are bound to the list; an unknown value is refused",
       {{"HTL-01", "covers"}, {"HTL-03", "covers"}}, {13, 14, 15, 16, 17, 18}, test_pack},
      {"OWNER-04", "hash maintenance follows the owning list",
       "the hash tables belong to the list that holds an element; a change of a hashed field is maintained "
       "in that list's tables, whichever way the element got there",
       {"create a list and append elements", "move one with lDechainElem and lAppendElem",
        "move two with lDechainList", "pack and unpack a list", "change a hashed field of each moved element",
        "add a hash on a field with cull_hash_new", "sort a list"},
       "each element is found through the tables of the list that holds it, under its new key, and not "
       "through the tables of the list it came from; the added and the sorted tables are maintained",
       {{"HTL-02", "covers"}}, {19, 20, 21, 22, 23, 24}, test_hash_follows_owner},
   });
}
