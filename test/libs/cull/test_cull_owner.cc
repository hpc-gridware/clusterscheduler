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
 * reads them directly, because a lookup through find_first() and its kind falls
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

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SortCore.h"

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"
#include "ocs_test_model.h"

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
LISTEND(OW_Type)

LISTDEF(OWS_Type)
   SGE_STRING(OWS_name, CULL_DEFAULT)
LISTEND(OWS_Type)

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
 * @return the owner of the free element
 */
static ocs::tom::RawElementPtr make_elem(const char *name) {
   auto ep = ocs::tom::RawElementPtr::create(OW_Type);
   ep->set(OW_name, name);
   auto obj = ocs::tom::RawElementPtr::create(OWS_Type);
   obj->set(OWS_name, static_cast<const char *>("object"));
   ep->set(OW_obj, std::move(obj));
   auto sub = ocs::tom::RawListPtr::create(OWS_Type, ocs::tom::Hashing::On, "OW_sub");
   for (const char *key : {"s1", "s2"}) {
      auto sub_elem = ocs::tom::RawElementPtr::create(OWS_Type);
      sub_elem->set(OWS_name, key);
      sub->push_back(std::move(sub_elem));
   }
   ep->set(OW_sub, std::move(sub));
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
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_elem(&pb.raw(), ep, nullptr, 0);
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      *wire = ocs::tom::get_uint32(&in.raw());
   } catch (const ocs::tom::PackException &) {
      return false;
   }
   return true;
}

/**
 * @brief Pack an element and unpack it again, as a receiver does
 *
 * @param ep the element
 * @param out receives the owner of the unpacked element
 * @return the unpack result, PACK_SUCCESS on success
 */
static int pack_unpack(const lListElem *ep, ocs::tom::RawElementPtr &out) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_elem(&pb.raw(), ep, nullptr, 0);
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      out = ocs::tom::unpack_raw_elem(&in.raw(), ep->descr, 0);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a list and unpack it again, as a receiver does
 *
 * @param lp the list
 * @param out receives the owner of the unpacked list
 * @return true on success
 */
static bool pack_unpack_list(const lList *lp, ocs::tom::RawListPtr &out) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_list(&pb.raw(), lp, nullptr, 0);
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      out = ocs::tom::unpack_raw_list(&in.raw(), 0);
   } catch (const ocs::tom::PackException &) {
      return false;
   }
   return true;
}

/**
 * @brief Look a string key up in the hash table a list holds for a field, not by a search
 *
 * @param lp the list
 * @param nm the hashed string field
 * @param key the key
 * The table is built by this lookup if the list has none yet (the threshold is
 * 0 in this test, see main()).
 *
 * @return the element the table holds for the key, or nullptr when the table has none or cannot be built
 */
static const lListElem *table_str(const lList *lp, int nm, const char *key) {
   const int pos = ocs::tom::field_pos(lp->descr, nm);
   const void *iterator = nullptr;
   return cull_hash_first(ocs_cull_hash_lookup_table(lp, pos), key, mt_is_unique(lp->descr[pos].mt), &iterator);
}

/**
 * @brief Look an ulong key up in the hash table a list holds for a field, not by a search
 *
 * For a non unique table the first element of the key's chain is returned.
 *
 * @param lp the list
 * @param nm the hashed ulong field
 * @param key the key
 * The table is built by this lookup if the list has none yet.
 *
 * @return the element the table holds for the key, or nullptr when the table has none or cannot be built
 */
static const lListElem *table_ulong(const lList *lp, int nm, uint32_t key) {
   const int pos = ocs::tom::field_pos(lp->descr, nm);
   const void *iterator = nullptr;
   return cull_hash_first(ocs_cull_hash_lookup_table(lp, pos), &key, mt_is_unique(lp->descr[pos].mt), &iterator);
}

/**
 * @brief Whether a moved element is found under its new keys in the target only
 *
 * The tables of both lists are built first, so that the change of the
 * element's unique name and its group after the move has to be maintained in
 * them; the target's tables must hold it under the new keys, under the old
 * name nothing may be left, and the source's tables must not know it under
 * either.
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
   table_str(target, OW_name, old_name);
   table_ulong(target, OW_group, group);
   table_str(source, OW_name, old_name);
   table_ulong(source, OW_group, group);
   ocs::tom::set(ep, OW_name, new_name);
   ocs::tom::set(ep, OW_group, group);
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

   auto lp = ocs::tom::RawListPtr::create(OW_Type, ocs::tom::Hashing::On, "layout");
   const lListElem *ep = lp->push_back(make_elem("a"));
   CHECK(2, "a heap list leaves the two low bits of its address free",
         (reinterpret_cast<uintptr_t>(lp.get().raw()) & 3) == 0);
   CHECK(3, "the tag of a bound element yields its list", is_owned(ep, BOUND_ELEM, lp.get()));
}

/**
 * @brief OWNER-02: every state transition sets the state and the list
 *
 * @return nothing; CHECK records the results
 */
static void test_transitions() {
   auto lp = ocs::tom::RawListPtr::create(OW_Type, ocs::tom::Hashing::On, "source");
   ocs::tom::RawElementPtr a_owner = make_elem("a");
   CHECK(4, "a created element is free", is_owned(a_owner.get(), FREE_ELEM, nullptr));

   const ocs::tom::RawElement a = lp->push_back(std::move(a_owner));
   const ocs::tom::RawElement b = lp->insert_after(ocs::tom::ConstRawElement(), make_elem("b"));
   const ocs::tom::RawElement c = lp->insert_after(a, make_elem("c"));
   CHECK(5, "appended and inserted elements are bound to the list",
         is_owned(a, BOUND_ELEM, lp.get()) && is_owned(b, BOUND_ELEM, lp.get()) && is_owned(c, BOUND_ELEM, lp.get()));

   const ocs::tom::ConstRawList a_sub = a.get<ocs::tom::ConstRawList>(OW_sub);
   CHECK(6, "a sublist element is bound to the sublist, a sub-object is an object",
         is_owned(a_sub.front(), BOUND_ELEM, a_sub)
         && is_owned(a.get<const lListElem *>(OW_obj), OBJECT_ELEM, nullptr));

   // list order is b, a, c: move a and c into a new list
   ocs::tom::RawListPtr target(ocs::tom::split_from(lp.get(), a, "source"));
   CHECK(7, "split_from() binds every moved element to the target and none stays with the source",
         target && is_owned(a, BOUND_ELEM, target.get()) && is_owned(c, BOUND_ELEM, target.get())
         && is_owned(b, BOUND_ELEM, lp.get()) && lp->size() == 1 && target->size() == 2);

   ocs::tom::RawElementPtr d = target->take(c);
   CHECK(8, "a taken element is free", is_owned(d.get(), FREE_ELEM, nullptr));

   ocs::tom::RawElementPtr obj = a.take(OW_obj);
   const lListElem *obj_raw = obj.get();
   CHECK(9, "a taken sub-object is free", is_owned(obj_raw, FREE_ELEM, nullptr));
   d->set(OW_obj, std::move(obj));
   CHECK(10, "an object set into a field is an object", is_owned(obj_raw, OBJECT_ELEM, nullptr));

   ocs::tom::RawElementPtr copy = b.clone();
   const ocs::tom::ConstRawList copy_sub = copy->get<ocs::tom::ConstRawList>(OW_sub);
   CHECK(11, "the copy of a bound element is free, its sub-object an object, its sublist elements bound "
             "to the copy's sublist",
         is_owned(copy.get(), FREE_ELEM, nullptr)
         && is_owned(copy->get<const lListElem *>(OW_obj), OBJECT_ELEM, nullptr)
         && is_owned(copy_sub.front(), BOUND_ELEM, copy_sub));

   // free in every state: bound (through the lists), free, and the objects they hold
   copy.reset();
   d.reset();
   target.reset();
   lp.reset();
   CHECK(12, "elements are freed in every state", !copy && !d && !target && !lp);
}

/**
 * @brief OWNER-03: the pack format still carries the former status values
 *
 * @return nothing; CHECK records the results
 */
static void test_pack() {
   auto lp = ocs::tom::RawListPtr::create(OW_Type, ocs::tom::Hashing::On, "pack");
   const ocs::tom::RawElement bound = lp->push_back(make_elem("bound"));
   ocs::tom::RawElementPtr free_ep = make_elem("free");

   uint32_t w_free = 0;
   uint32_t w_bound = 0;
   uint32_t w_object = 0;
   CHECK(13, "free, bound and object elements are packed with the former status values 1, 2 and 8",
         packed_status(free_ep.get(), &w_free) && w_free == 1 && packed_status(bound, &w_bound) && w_bound == 2
         && packed_status(bound.get<const lListElem *>(OW_obj), &w_object) && w_object == 8);

   ocs::tom::RawElementPtr from_free;
   ocs::tom::RawElementPtr from_bound;
   CHECK(14, "an unpacked free element is free, an unpacked bound element is transitionally bound",
         pack_unpack(free_ep.get(), from_free) == PACK_SUCCESS && is_owned(from_free.get(), FREE_ELEM, nullptr)
         && pack_unpack(bound, from_bound) == PACK_SUCCESS
         && is_owned(from_bound.get(), TRANS_BOUND_ELEM, nullptr));

   uint32_t w_trans = 0;
   CHECK(15, "a transitionally bound element is packed with the former status value 4",
         packed_status(from_bound.get(), &w_trans) && w_trans == 4);

   // a transitionally bound element may be appended; it is bound afterwards
   auto lp2 = ocs::tom::RawListPtr::create(OW_Type, ocs::tom::Hashing::On, "pack2");
   const lListElem *trans = from_bound.get();
   bool appended = false;
   if (from_bound) {
      static_cast<void>(lp2->push_back(std::move(from_bound)));
      appended = true;
   }
   CHECK(16, "a transitionally bound element becomes bound when appended",
         appended && is_owned(trans, BOUND_ELEM, lp2.get()));

   // a whole list: every element is bound to the unpacked list
   ocs::tom::RawListPtr unpacked;
   const bool ok = pack_unpack_list(lp.get(), unpacked);
   CHECK(17, "the elements of an unpacked list are bound to it, their sub-objects are objects",
         ok && unpacked && is_owned(unpacked->front(), BOUND_ELEM, unpacked.get())
         && is_owned(unpacked->front().get<const lListElem *>(OW_obj), OBJECT_ELEM, nullptr));

   // a status value the format does not know is refused
   ocs::tom::RawElementPtr bad;
   int ret = PACK_SUCCESS;
   bool prepared = false;
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::put_uint32(&pb.raw(), 16);
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      prepared = true;
      bad = ocs::tom::unpack_raw_elem(&in.raw(), OW_Type, 0);
   } catch (const ocs::tom::PackException &e) {
      ret = e.code();
   }
   CHECK(18, "an unknown status value is refused with PACK_FORMAT", prepared && ret == PACK_FORMAT && !bad);
}

/**
 * @brief OWNER-04: hash maintenance follows the owning list
 *
 * @return nothing; CHECK records the results
 */
static void test_hash_follows_owner() {
   auto source_owner = ocs::tom::RawListPtr::create(OW_Type, ocs::tom::Hashing::On, "source");
   const ocs::tom::RawList source = source_owner.get();
   for (const char *name : {"a", "b", "c", "d"}) {
      auto ep = ocs::tom::RawElementPtr::create(OW_Type);
      ep->set(OW_name, name);
      ep->set(OW_group, std::uint32_t{1});
      source.push_back(std::move(ep));
   }
   CHECK(19, "the first lookups build a table for each hashed field of a created list",
         source.raw()->nslots == 2 && table_str(source, OW_name, "a") == source.front().raw()
         && table_ulong(source, OW_group, 1) == source.front().raw()
         && list_hash_table(source, ocs::tom::field_pos(OW_Type, OW_count)) == nullptr);

   // take and append
   auto target = ocs::tom::RawListPtr::create(OW_Type, ocs::tom::Hashing::On, "target");
   const ocs::tom::RawElement a = target->push_back(source.take(source.front()));
   CHECK(20, "after take and append a changed key is maintained in the target's tables only",
         follows_owner(a, source, target.get(), "a", "a2", 20));

   // split_from() moves c and d into a new list
   const ocs::tom::RawElement c = source.front().next();
   const ocs::tom::RawElement d = c.next();
   ocs::tom::RawListPtr moved(ocs::tom::split_from(source, c, "source"));
   CHECK(21, "after split_from() a changed key is maintained in the new list's tables only",
         moved && follows_owner(c, source, moved.get(), "c", "c2", 21)
         && follows_owner(d, source, moved.get(), "d", "d2", 22)
         && table_str(source, OW_name, "b") == source.front().raw());

   // unpacking a list hashes its elements in the unpacked list
   ocs::tom::RawListPtr unpacked;
   const bool ok = pack_unpack_list(moved.get(), unpacked);
   const ocs::tom::RawElement u = unpacked ? unpacked->front() : ocs::tom::RawElement();
   const bool hashed_on_unpack = ok && u && table_str(unpacked.get(), OW_name, "c2") == u.raw();
   if (u) {
      u.set(OW_name, static_cast<const char *>("c3"));
   }
   CHECK(22, "the elements of an unpacked list are maintained in that list's tables only",
         hashed_on_unpack && table_str(unpacked.get(), OW_name, "c3") == u.raw()
         && table_str(unpacked.get(), OW_name, "c2") == nullptr
         && table_str(moved.get(), OW_name, "c3") == nullptr && table_str(moved.get(), OW_name, "c2") == c.raw());

   // a hash added to a live list on a field the type does not hash
   source.front().set(OW_count, std::uint32_t{5});
   const bool hash_added = cull_hash_new(source, OW_count, false) == 1 && source.raw()->nslots == 3
                           && table_ulong(source, OW_count, 5) == source.front().raw();
   source.front().set(OW_count, std::uint32_t{6});
   CHECK(23, "cull_hash_new adds a slot whose table, built by the first lookup, is maintained afterwards",
         hash_added && table_ulong(source, OW_count, 6) == source.front().raw()
         && table_ulong(source, OW_count, 5) == nullptr);

   // sorting rebuilds the non unique chains in the new list order
   c.set(OW_group, std::uint32_t{9});
   d.set(OW_group, std::uint32_t{9});
   const ocs::tom::SortKey keys[] = {{OW_name, false}};
   lSortOrder *order = ocs::tom::make_sort_order(OW_Type, keys);
   ocs::tom::sort_list(moved.get(), order);
   ocs::tom::free_sort_order(order);
   CHECK(24, "after sorting the chain of a non unique key starts with the first element in list order",
         moved->front().raw() == d.raw() && table_ulong(moved.get(), OW_group, 9) == d.raw());

   // the tables left the descriptor: an entry is field number, type and slot offset (CS-439), no table
   CHECK(25, "a descriptor entry is 12 bytes", sizeof(lDescr) == 12);
}

/**
 * @brief Run the scenarios of this test
 * @param argc the number of arguments
 * @param argv the arguments, handed to the module test driver
 * @return the exit status of the module test driver
 */
int main(int argc, char *argv[]) {
   // the lists here are short; the maintenance of their tables is what is tested
   ocs::module_test::install_test_model({{"OW", OW_Type, OW_Name, OW_Size}, {"OWS", OWS_Type, OWS_Name, OWS_Size}});
   ocs_cull_hash_set_min_elems(0);

   return ocs::module_test::run(argc, argv, {
      {"OWNER-01", "element layout",
       "the owner tag replaces the element status without growing the element",
       {"check the size of an element", "bind an element to a heap list and read the list back from the tag"},
       "the element is 40 bytes, the list address leaves the tag bits free, the tag yields the list",
       {{"HTL-01", "covers"}}, {1, 2, 3}, test_layout},
      {"OWNER-02", "state transitions",
       "create, append, insert, split_from, take, object take and set, copy and free set the state "
       "and the list of every element involved",
       {"create elements with a sublist and a sub-object", "append and insert them",
        "move part of the list with split_from", "take an element and a sub-object",
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
       {"create a list, append elements and look them up", "move one with take and push_back",
        "move two with split_from", "pack and unpack a list", "change a hashed field of each moved element",
        "add a hash on a field with cull_hash_new", "sort a list", "check the size of a descriptor entry"},
       "each element is found through the tables of the list that holds it, under its new key, and not "
       "through the tables of the list it came from; the added and the sorted tables are maintained; a "
       "descriptor entry holds no table and is 12 bytes",
       {{"HTL-02", "covers"}}, {19, 20, 21, 22, 23, 24, 25}, test_hash_follows_owner},
   });
}
