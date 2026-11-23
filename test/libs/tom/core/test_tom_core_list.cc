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
 * @brief Unit tests for elements and lists of the list core in `libs/tom`, driven through the raw interface
 */

#include <cstdio>
#include <cstring>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SortCore.h"
#include "ocs_test_model.h"

// ---------------------------------------------------------------------------
// Local CULL type used by all tests
// ---------------------------------------------------------------------------

/** @brief A synthetic CULL object type, defined only for these tests
 *
 * One attribute per CULL data type, so the tests can exercise every type
 * without depending on a real object whose layout may change.
 */
enum {
   TEST_int = 1,   ///< an int attribute
   TEST_host,   ///< a host name attribute
   TEST_string,   ///< a string attribute
   TEST_double,   ///< a double attribute
   TEST_long,   ///< a long attribute
   TEST_ulong,   ///< an unsigned long attribute
   TEST_bool,   ///< a bool attribute
   TEST_list,   ///< a sublist attribute
   TEST_object,   ///< a sub-object attribute
   TEST_ref,   ///< a reference attribute
   TEST_ulong64,   ///< a 64 bit unsigned attribute
   TEST_ulong64_hashed,   ///< a 64 bit unsigned attribute that is looked up by value
   TEST_string_ukey   ///< a string attribute that is a unique key and looked up by value
};

LISTDEF(TEST_Type)
   SGE_INT    (TEST_int,    MT_DEFAULT)
   SGE_HOST   (TEST_host,   MT_DEFAULT)
   SGE_STRING (TEST_string, MT_DEFAULT)
   SGE_DOUBLE (TEST_double, MT_DEFAULT)
   SGE_LONG   (TEST_long,   MT_DEFAULT)
   SGE_ULONG  (TEST_ulong,  MT_DEFAULT)
   SGE_BOOL   (TEST_bool,   MT_DEFAULT)
   SGE_LIST   (TEST_list,   TEST_Type, MT_DEFAULT)
   SGE_OBJECT (TEST_object, TEST_Type, MT_DEFAULT)
   SGE_REF    (TEST_ref,    TEST_Type, MT_DEFAULT)
   SGE_ULONG64(TEST_ulong64, MT_DEFAULT)
   SGE_ULONG64(TEST_ulong64_hashed, MT_HASH)
   SGE_STRING (TEST_string_ukey, MT_UNIQUE | MT_HASH)
LISTEND(TEST_Type)

NAMEDEF(TEST_Name)
   NAME("TEST_int")
   NAME("TEST_host")
   NAME("TEST_string")
   NAME("TEST_double")
   NAME("TEST_long")
   NAME("TEST_ulong")
   NAME("TEST_bool")
   NAME("TEST_list")
   NAME("TEST_object")
   NAME("TEST_ref")
   NAME("TEST_ulong64")
   NAME("TEST_ulong64_hashed")
   NAME("TEST_string_ukey")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the synthetic type

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/** @brief Create an element of TEST_Type with an int value
 *
 * @param value the value of TEST_int
 * @return the owner of the element
 */
static ocs::tom::RawElementPtr make_int_elem(const std::int32_t value) {
   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   ep->set(TEST_int, value);
   return ep;
}

/** @brief Create an element of TEST_Type with a string value
 *
 * @param nm the string field
 * @param value the value
 * @return the owner of the element
 */
static ocs::tom::RawElementPtr make_str_elem(const int nm, const char *value) {
   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   ep->set(nm, value);
   return ep;
}

/** @brief The condition TEST_string == value
 *
 * @param value the value
 * @return the owner of the condition
 */
static ocs::tom::RawWherePtr string_is(const char *value) {
   return ocs::tom::where_compare(TEST_Type, TEST_string, ocs::tom::CompareOp::Equal, value);
}

/** @brief Sort a list by one field through a sort order of the core
 *
 * @param lp the list
 * @param nm the field
 * @param ascending true for ascending, false for descending
 */
static void sort_by(const ocs::tom::RawList lp, const int nm, const bool ascending) {
   const ocs::tom::SortKey keys[] = {{nm, ascending}};
   lSortOrder *order = ocs::tom::make_sort_order(lp.descr(), keys);
   ocs::tom::sort_list(lp, order);
   ocs::tom::free_sort_order(order);
}

// ---------------------------------------------------------------------------
// lWhat / lWhere  [T01–T02]
// ---------------------------------------------------------------------------

/** @brief The format strings of lWhat() and lWhere(); TOM builds conditions and selections without one */
static void test_what_where() {
   printf("\n--- lWhat / lWhere ---\n");

   // CHECK 1 removed: the format parser of lWhat() has no TOM counterpart (CS-3103)
   // CHECK 2 removed: the format parser of lWhere() has no TOM counterpart (CS-3103)
}

// ---------------------------------------------------------------------------
// Scalar field set/get  [T03–T11]
//
// Every field type has a set(nm, v) / get<V>(nm) pair. Each must round-trip
// the stored value exactly. A second set on the same field overwrites the
// first — the last written value is what get returns (T11).
// ---------------------------------------------------------------------------

/** @brief The raw setters and getters round-trip every field type */
static void test_field_access() {
   printf("\n--- scalar field set/get ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);

   // T03: integer field roundtrip
   ep->set(TEST_int, std::int32_t{42});
   CHECK(3, "set/get of an int field: roundtrip", ep->get<std::int32_t>(TEST_int) == 42);

   // T04: host field roundtrip
   ep->set(TEST_host, ocs::tom::Host("myhost.example.com"));
   CHECK(4, "set/get of a host field: roundtrip",
         strcmp(ep->get<ocs::tom::Host>(TEST_host), "myhost.example.com") == 0);

   // T05: string field roundtrip
   ep->set(TEST_string, static_cast<const char *>("hello"));
   CHECK(5, "set/get of a string field: roundtrip", strcmp(ep->get<const char *>(TEST_string), "hello") == 0);

   // T06: double field roundtrip (exact bit-for-bit — set and immediately get)
   ep->set(TEST_double, 3.14);
   CHECK(6, "set/get of a double field: roundtrip", ep->get<double>(TEST_double) == 3.14);

   // T07: long field roundtrip (negative value)
   ep->set(TEST_long, std::int64_t{-7});
   CHECK(7, "set/get of a long field: roundtrip", ep->get<std::int64_t>(TEST_long) == -7);

   // T08: unsigned long field roundtrip
   ep->set(TEST_ulong, std::uint32_t{99});
   CHECK(8, "set/get of an ulong field: roundtrip", ep->get<std::uint32_t>(TEST_ulong) == 99);

   // T09: bool field roundtrip — true
   ep->set(TEST_bool, true);
   CHECK(9, "set/get of a bool field: true", ep->get<bool>(TEST_bool) == true);

   // T10: bool field roundtrip — false (overwriting the true stored by T09)
   ep->set(TEST_bool, false);
   CHECK(10, "set/get of a bool field: false", ep->get<bool>(TEST_bool) == false);

   // T11: a second set overwrites the first; get returns the last value
   ep->set(TEST_int, std::int32_t{1});
   ep->set(TEST_int, std::int32_t{100});
   CHECK(11, "set of an int field overwrites: last value wins", ep->get<std::int32_t>(TEST_int) == 100);
}

// ---------------------------------------------------------------------------
// Embedded object  [T12–T13]
//
// An element can store another element as a sub-object via set(nm, owner).
// get<const lListElem *>() returns the stored pointer and the sub-object's own
// fields are accessible through it.
// ---------------------------------------------------------------------------

/** @brief An object field stores the element itself */
static void test_embedded_object() {
   printf("\n--- embedded object ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   ocs::tom::RawElementPtr obj = make_int_elem(50);
   const lListElem *obj_raw = obj.get();
   ep->set(TEST_object, std::move(obj));

   // T12: the getter returns the exact pointer that was stored
   CHECK(12, "get of an object field returns the stored pointer",
         ep->get<const lListElem *>(TEST_object) == obj_raw);

   // T13: the embedded object's own fields are accessible through the returned handle
   CHECK(13, "embedded object int accessible through the object field",
         ep->get<ocs::tom::ConstRawElement>(TEST_object).get<std::int32_t>(TEST_int) == 50);
}

// ---------------------------------------------------------------------------
// clone()  [T14–T16]
//
// clone() produces a deep copy of an element. The copy is a distinct
// allocation with the same field values; modifying one does not affect the other.
// ---------------------------------------------------------------------------

/** @brief clone() of an element */
static void test_copy_elem() {
   printf("\n--- clone() of an element ---\n");

   ocs::tom::RawElementPtr ep = make_int_elem(42);
   ep->set(TEST_string, static_cast<const char *>("hello"));

   ocs::tom::RawElementPtr copy = ep.clone();

   // T14: the copy is a distinct allocation (not the same pointer)
   CHECK(14, "clone() returns a different pointer", copy.get().raw() != ep.get().raw());

   // T15: integer field is preserved in the copy
   CHECK(15, "clone(): int field preserved", copy->get<std::int32_t>(TEST_int) == 42);

   // T16: string field is preserved in the copy
   CHECK(16, "clone(): string field preserved",
         copy->get<const char *>(TEST_string) != nullptr &&
         strcmp(copy->get<const char *>(TEST_string), "hello") == 0);
}

// ---------------------------------------------------------------------------
// copy_elem_partial()  [T17–T21]
//
// copy_elem_partial() copies a subset of fields from a source element into a
// pre-allocated destination element. The field selection is governed by a
// selection built with what_all() or what_of(). Fields not covered by the
// selection keep their default values (0 / nullptr) in the destination.
// ---------------------------------------------------------------------------

/** @brief copy_elem_partial() of the core, the former lCopyElemPartialPack() */
static void test_partial_pack() {
   printf("\n--- copy_elem_partial() ---\n");

   ocs::tom::RawElementPtr ep = make_int_elem(42);
   ep->set(TEST_string, static_cast<const char *>("hello"));
   ep->set(TEST_double, 3.14);

   // full copy: ALL fields
   auto full = ocs::tom::RawElementPtr::create(TEST_Type);
   ocs::tom::RawWhatPtr all_enp = ocs::tom::what_all();
   ocs::tom::copy_elem_partial(full.get(), ep.get(), all_enp.get(), true, false);

   // T17: a full copy has the int field
   CHECK(17, "full partial copy: int field copied", full->get<std::int32_t>(TEST_int) == 42);

   // T18: a full copy has the string field
   CHECK(18, "full partial copy: string field copied",
         full->get<const char *>(TEST_string) != nullptr &&
         strcmp(full->get<const char *>(TEST_string), "hello") == 0);

   // partial copy: only TEST_string and TEST_double
   auto part = ocs::tom::RawElementPtr::create(TEST_Type);
   const int part_fields[] = {TEST_string, TEST_double};
   ocs::tom::RawWhatPtr part_enp = ocs::tom::what_of(TEST_Type, part_fields);
   ocs::tom::copy_elem_partial(part.get(), ep.get(), part_enp.get(), true, false);

   // T19: the selected string field is present in the partial copy
   CHECK(19, "partial copy: string (selected) copied",
         part->get<const char *>(TEST_string) != nullptr &&
         strcmp(part->get<const char *>(TEST_string), "hello") == 0);

   // T20: the selected double field is present in the partial copy
   CHECK(20, "partial copy: double (selected) copied", part->get<double>(TEST_double) == 3.14);

   // T21: the unselected int field retains the default value 0 in the partial copy
   CHECK(21, "partial copy: int (not selected) is 0", part->get<std::int32_t>(TEST_int) == 0);
}

// ---------------------------------------------------------------------------
// List operations  [T22–T25]
//
// create / push_back / size / front cover the basic lifecycle of a list.
// Elements are owned by the list after push_back; the list owner frees them
// all.
// ---------------------------------------------------------------------------

/** @brief The basic lifecycle of a list */
static void test_list_ops() {
   printf("\n--- list operations ---\n");

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "test");

   // T22: a freshly created list has no elements
   CHECK(22, "fresh list has 0 elements", list->size() == 0);

   const lListElem *e1 = list->push_back(make_int_elem(10));

   // T23: after the first append the count is 1
   CHECK(23, "after first append, count is 1", list->size() == 1);

   list->push_back(make_int_elem(20));

   // T24: after the second append the count is 2
   CHECK(24, "after second append, count is 2", list->size() == 2);

   // T25: front() returns the first appended element
   CHECK(25, "front() returns first element", list->front().raw() == e1);
}

// ---------------------------------------------------------------------------
// Sub-list add and lookup  [T26–T27]
//
// get_or_create_list() and add_elem_str() of the core append an element to
// the sub-list of an element, setting a string field on the new sub-element,
// as lAddSubStr() did. find_first() searches that sub-list by string value and
// returns the matching element.
// ---------------------------------------------------------------------------

/** @brief Add to and look up in a sub-list */
static void test_sub_list() {
   printf("\n--- sub-list operations ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   lList *sub = ocs::tom::get_or_create_list(ep.get(), ocs::tom::field_pos(TEST_Type, TEST_list), nullptr, TEST_Type);
   ocs::tom::add_elem_str(sub, TEST_string, "needle", TEST_Type);

   const ocs::tom::ConstRawElement found =
           ep->get<ocs::tom::ConstRawList>(TEST_list).find_first(TEST_string, static_cast<const char *>("needle"));

   // T26: the added sub-element is found by its string value
   CHECK(26, "find_first() in the sub-list finds the element added by add_elem_str()", static_cast<bool>(found));

   // T27: the found element's string field matches the value that was added
   CHECK(27, "find_first() in the sub-list: found element has correct string value",
         found && strcmp(found.get<const char *>(TEST_string), "needle") == 0);
}

// ---------------------------------------------------------------------------
// field_pos()  [T28–T29]
//
// field_pos() maps a field ID (enum value) to its 0-based position in the
// descriptor array. Returns -1 for field IDs not present in the descriptor.
// ---------------------------------------------------------------------------

/** @brief field_pos() of the core */
static void test_get_pos_in_descr() {
   printf("\n--- field_pos() ---\n");

   // T28: a valid field ID yields a non-negative position
   CHECK(28, "field_pos(): valid field returns non-negative position",
         ocs::tom::field_pos(TEST_Type, TEST_string) >= 0);

   // T29: an unknown field ID returns -1
   CHECK(29, "field_pos(): invalid field returns -1", ocs::tom::field_pos(TEST_Type, 999) == -1);
}

// ---------------------------------------------------------------------------
// Condition-based search  [T30–T35]
//
// where_compare() builds a filter condition; find_first() of the core returns
// the first list element that satisfies it, or nullptr when none does.
// matches() tests a single element against a condition without iterating a
// list. make_logic() with LogicOp::Or combines two conditions with logical OR;
// it takes ownership of both inputs.
// ---------------------------------------------------------------------------

/** @brief Searching a list with a condition */
static void test_condition_search() {
   printf("\n--- condition-based search ---\n");

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "cond");
   const ocs::tom::RawElement e1 = list->push_back(make_str_elem(TEST_string, "alice"));
   const ocs::tom::RawElement e2 = list->push_back(make_str_elem(TEST_string, "bob"));

   // T30: a well-formed condition is not empty
   ocs::tom::RawWherePtr cond = string_is("alice");
   CHECK(30, "where_compare() returns a condition", cond != nullptr);

   // T31: find_first() returns the element whose field matches the condition
   CHECK(31, "find_first(): matching element found", ocs::tom::find_first(list.get(), cond.get()) == e1.raw());

   // T32: find_first() returns nullptr when no element satisfies the condition
   ocs::tom::RawWherePtr no_match = string_is("nobody");
   CHECK(32, "find_first(): no match returns nullptr", ocs::tom::find_first(list.get(), no_match.get()) == nullptr);

   // T33: matches() is true when the element satisfies the condition
   CHECK(33, "matches(): matching element gives true", e1.matches(cond.get()));

   // T34: matches() is false when the element does not satisfy the condition
   CHECK(34, "matches(): non-matching element gives false", !e2.matches(cond.get()));

   // T35: an OR condition matches elements satisfying either of two conditions
   ocs::tom::RawWherePtr or_cond(
           ocs::tom::make_logic(ocs::tom::LogicOp::Or, string_is("alice").release(), string_is("bob").release()));
   CHECK(35, "make_logic(Or): finds element matching second alternative",
         ocs::tom::find_first(list.get(), or_cond.get()) != nullptr && e2.matches(or_cond.get()));
}

// ---------------------------------------------------------------------------
// Element removal and insertion  [T36–T40]
//
// take() unlinks an element from a list without freeing it; the caller takes
// ownership. remove_elem() of the core unlinks and frees the element and sets
// the caller's pointer to nullptr. insert_after() inserts a new element
// immediately AFTER the given element; an empty handle inserts at the front of
// the list.
// ---------------------------------------------------------------------------

/** @brief take(), remove_elem() and insert_after() */
static void test_elem_removal() {
   printf("\n--- element removal and insertion ---\n");

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "rm");
   list->push_back(make_int_elem(1));
   const ocs::tom::RawElement e2 = list->push_back(make_int_elem(2));
   lListElem *e3 = list->push_back(make_int_elem(3));

   // T36: take() unlinks the element; count decreases but element is not freed
   ocs::tom::RawElementPtr taken = list->take(e2);
   CHECK(36, "take(): count decreases by 1", list->size() == 2 && taken);
   taken.reset();

   // T37: remove_elem() frees the element and decreases the count
   ocs::tom::remove_elem(list.get(), e3);
   CHECK(37, "remove_elem(): count decreases by 1", list->size() == 1);

   // T38: remove_elem() sets the caller's pointer to nullptr
   CHECK(38, "remove_elem(): caller pointer set to nullptr", e3 == nullptr);

   // T39: insert_after() inserts AFTER the given element (list is [e1] → insert ins after e1 → [e1, ins])
   const ocs::tom::RawElement ins = list->insert_after(list->front(), make_int_elem(99));
   CHECK(39, "insert_after(): element is inserted after the given element",
         list->front().next().raw() == ins.raw());

   // T40: after the insert the count is 2 (e1 + ins)
   CHECK(40, "insert_after(): count increases by 1", list->size() == 2);
}

// ---------------------------------------------------------------------------
// List traversal  [T41–T44]
//
// next() advances to the next element; back() returns the tail. remaining()
// counts elements strictly BEHIND the given element (exclusive — the element
// itself is not counted).
// ---------------------------------------------------------------------------

/** @brief next(), back() and remaining() */
static void test_traversal() {
   printf("\n--- traversal ---\n");

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "trav");
   list->push_back(make_int_elem(1));
   const ocs::tom::RawElement e2 = list->push_back(make_int_elem(2));
   const ocs::tom::RawElement e3 = list->push_back(make_int_elem(3));

   // T41: next() after front() returns the second element
   CHECK(41, "next() after front() returns second element", list->front().next().raw() == e2.raw());

   // T42: next() after the last element returns an empty handle
   CHECK(42, "next() after last element returns an empty handle", !list->back().next());

   // T43: back() returns the last appended element
   CHECK(43, "back() returns the last appended element", list->back().raw() == e3.raw());

   // T44: from e2, only e3 is behind it — count is 1 (exclusive of e2 itself)
   CHECK(44, "remaining() from second element is 1", ocs::tom::remaining(e2) == 1);
}

// ---------------------------------------------------------------------------
// String-key lookup  [T45–T46]
//
// find_first() searches a list for the first element whose named string field
// equals the given value. It returns nullptr when no element matches.
// ---------------------------------------------------------------------------

/** @brief find_first() by a string key */
static void test_elem_str_lookup() {
   printf("\n--- find_first() by a string ---\n");

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "str");
   list->push_back(make_str_elem(TEST_string, "alpha"));
   const ocs::tom::RawElement e2 = list->push_back(make_str_elem(TEST_string, "beta"));

   // T45: find_first() finds the element with the matching string value
   CHECK(45, "find_first(): finds element with matching string",
         list->find_first(TEST_string, static_cast<const char *>("beta")).raw() == e2.raw());

   // T46: find_first() returns nullptr when no element has the given value
   CHECK(46, "find_first(): returns nullptr for no match",
         !list->find_first(TEST_string, static_cast<const char *>("gamma")));
}

// ---------------------------------------------------------------------------
// clone() of a list  [T47–T49]
//
// clone() produces a deep copy of an entire list. The copy has the same
// number of elements; each element is a distinct allocation with the same
// field values as its source counterpart.
// ---------------------------------------------------------------------------

/** @brief clone() of a list */
static void test_copy_list() {
   printf("\n--- clone() of a list ---\n");

   auto src = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "src");
   src->push_back(make_int_elem(7));
   src->push_back(make_int_elem(8));

   ocs::tom::RawListPtr copy = src->clone();

   // T47: the copy has the same element count as the source
   CHECK(47, "clone(): copy has the 2 elements of the source", copy->size() == 2);

   // T48: elements in the copy are distinct allocations
   CHECK(48, "clone(): first element is a different pointer", copy->front().raw() != src->front().raw());

   // T49: int field values are preserved in the copy
   CHECK(49, "clone(): int field value preserved", copy->front().get<std::int32_t>(TEST_int) == 7);
}

// ---------------------------------------------------------------------------
// Reference field  [T50]
//
// The REF field stores a raw (unmanaged) pointer. set/get round-trip the
// value exactly; the list library does not dereference or free the stored
// pointer.
// ---------------------------------------------------------------------------

/** @brief A reference field round-trips the pointer */
static void test_ref_field() {
   printf("\n--- reference field ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   auto ref = ocs::tom::RawElementPtr::create(TEST_Type);
   ep->set(TEST_ref, static_cast<void *>(ref.get().raw()));

   // T50: get returns the exact pointer that was set
   CHECK(50, "set/get of a reference field: roundtrip returns same pointer",
         ep->get<void *>(TEST_ref) == ref.get().raw());
}

// ---------------------------------------------------------------------------
// Sorting and deduplication  [T51–T55]
//
// sort_list() of the core reorders list elements by a sort order;
// unique_by_key() removes elements whose named string field is a duplicate of
// an earlier element's value.
// ---------------------------------------------------------------------------

/** @brief sort_list() and unique_by_key() */
static void test_sort_and_uniq() {
   printf("\n--- sort and deduplication ---\n");

   // build an unsorted list: 30, 10, 20
   auto sort_list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "sort");
   sort_list->push_back(make_int_elem(30));
   sort_list->push_back(make_int_elem(10));
   sort_list->push_back(make_int_elem(20));

   sort_by(sort_list.get(), TEST_int, true);

   // T51: after ascending sort, front() has the smallest value
   CHECK(51, "ascending sort: front() has smallest int", sort_list->front().get<std::int32_t>(TEST_int) == 10);

   // T52: after ascending sort, back() has the largest value
   CHECK(52, "ascending sort: back() has largest int", sort_list->back().get<std::int32_t>(TEST_int) == 30);

   // build a list with a duplicate string: "alpha", "beta", "alpha"
   auto uniq_list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "uniq");
   uniq_list->push_back(make_str_elem(TEST_string, "alpha"));
   uniq_list->push_back(make_str_elem(TEST_string, "beta"));
   uniq_list->push_back(make_str_elem(TEST_string, "alpha"));

   ocs::tom::unique_by_key(uniq_list.get(), TEST_string);

   // T53: the duplicate is removed; only 2 elements remain
   CHECK(53, "unique_by_key(): duplicate removed (count = 2)", uniq_list->size() == 2);

   // T54: the unique value "beta" is still present after deduplication
   CHECK(54, "unique_by_key(): unique values preserved",
         static_cast<bool>(uniq_list->find_first(TEST_string, static_cast<const char *>("beta"))));

   // T55: the duplicated value "alpha" appears exactly once
   int alpha = 0;
   for (const ocs::tom::ConstRawElement ep : uniq_list.get()) {
      alpha += strcmp(ep.get<const char *>(TEST_string), "alpha") == 0 ? 1 : 0;
   }
   CHECK(55, "unique_by_key(): deduplicated value appears exactly once", alpha == 1);
}


// ---------------------------------------------------------------------------
// The atomic accessors
// ---------------------------------------------------------------------------

/*
 * CS-2685: a field written on a path that holds only a read lock. The value
 * carries no invariant with any other field, so what it needs is that no reader
 * can see it torn - not mutual exclusion. Relying on an aligned 64 bit store
 * being indivisible would be relying on something the language does not
 * guarantee and that does not hold on every platform this is built for.
 */
/** @brief The atomic load and store of the core on a 64 bit field */
static void test_atomic_ulong64() {
   printf("\n--- atomic 64 bit accessors ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   lMultiType &slot = ocs::tom::detail::value(ep.get(), ocs::tom::field_pos(TEST_Type, TEST_ulong64));

   // T56-T57: the plain round trip, and that the two kinds of accessor address
   // the same storage - a field must be readable by whichever of them the caller
   // happens to use
   ocs::tom::store_uint64_atomic(slot, 4242);
   CHECK(56, "store_uint64_atomic()/load_uint64_atomic(): round trip", ocs::tom::load_uint64_atomic(slot) == 4242);
   const bool plain_sees_atomic = ep->get<std::uint64_t>(TEST_ulong64) == 4242;
   ep->set(TEST_ulong64, std::uint64_t{7});
   CHECK(57, "the ordinary and the atomic accessor see the same field",
         plain_sees_atomic && ocs::tom::load_uint64_atomic(slot) == 7);

   // CHECK 58 removed: the run-time refusal of a hashed field by lSetUlong64Atomic() has no TOM counterpart; the
   // typed set_atomic<F>() does not compile for a hashed field (CS-3103)
   // CHECK 59 removed: lSetUlong64Atomic() on a field the element does not hold has no TOM counterpart (CS-3103)
   // CHECK 60 removed: lSetUlong64Atomic(nullptr) has no TOM counterpart (CS-3103)
}

// ---------------------------------------------------------------------------
// Integrity of a unique hashed key  [T61–T70]
//
// CS-2755. A list whose key field is MT_UNIQUE | MT_HASH answers two
// different things depending on how it is asked: a linear scan walks the
// elements, a keyed lookup goes through the hash table (lookup_first(),
// ocs_ElementCore.cc). The two must not disagree.
//
// What can be asked of a unique hash is not that every element is findable —
// two elements under one key cannot both be, since the table holds one entry
// per key. What can be asked is this:
//
//   every key that occurs in the list resolves to an element that is in the
//   list and carries that key.
//
// Once the duplicates are gone, that is the same statement as "every entry is
// findable". While they are there, it is the strongest form the structure
// allows, and it is exactly what breaks today: the key resolves to nothing.
// ---------------------------------------------------------------------------

/** @brief Does every key in the list resolve to a list element carrying it?
 *
 * Walks the list and looks each element's key up again through the hash. A
 * linear comparison would pass on a corrupt list, so the point of the check is
 * that the lookup and the walk are two different paths to the same answer.
 *
 * @param lp the list to check
 * @param nm the MT_UNIQUE | MT_HASH string field acting as the key
 * @return true when every key resolves to an element of lp that carries it
 */
static bool all_keys_resolve(const ocs::tom::ConstRawList lp, const int nm) {
   for (const ocs::tom::ConstRawElement ep : lp) {
      const char *key = ep.get<const char *>(nm);
      const ocs::tom::ConstRawElement found = lp.find_first(nm, key);

      if (!found) {
         return false;
      }
      // the hash may legitimately hand back the other element of a duplicate
      // pair, but never one carrying a different key
      if (strcmp(found.get<const char *>(nm), key) != 0) {
         return false;
      }
   }
   return true;
}

/** @brief Build a list of TEST_Type holding the given unique keys
 *
 * @param name the list name
 * @param keys the key values to append, in order
 * @param n how many of them
 * @return the owner of the new list
 */
static ocs::tom::RawListPtr make_keyed_list(const char *name, const char *const *keys, const int n) {
   auto lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, name);

   for (int i = 0; i < n; i++) {
      lp->push_back(make_str_elem(TEST_string_ukey, keys[i]));
   }
   return lp;
}

/** @brief Build a list holding one key twice, by moving a second list holding the key into the first
 *
 * @param name1 the name of the first list
 * @param name2 the name of the second list, freed by the move
 * @param keys the key held by both
 * @return the owner of the list
 */
static ocs::tom::RawListPtr make_duplicate_list(const char *name1, const char *name2, const char *const *keys) {
   ocs::tom::RawListPtr first = make_keyed_list(name1, &keys[0], 1);
   first->splice(make_keyed_list(name2, &keys[1], 1));
   return first;
}

/**
 * @brief T61-T70: a key held twice is a logic error at the lookup, and removing one of the pair leaves the
 *        other findable (CS-2755, CS-3052)
 *
 * The duplicates are created on purpose, which a debug build refuses at the
 * append (N-H-18); the checks are suspended here.
 *
 * @return nothing; CHECK records the results
 */
static void test_unique_key_integrity() {
   const ocs::tom::UniqueCheckSuspension suspension;
   printf("\n--- unique hashed key integrity (CS-2755) ---\n");

   static const char *const one_dup[] = {"h10", "h10"};
   static const char *const three[] = {"h10", "h20", "h30"};

   // T61: the state the ticket is about can be reached at all. href_list_add()
   // and its kind refuse a duplicate within one list, so in the product this
   // arises from concatenating two lists that are each clean on their own.
   ocs::tom::RawListPtr a = make_duplicate_list("a", "b", one_dup);
   CHECK(61, "splice(): two entries under one key end up in the list", a->size() == 2);

   // T62: with both still there a lookup of the key is a logic error (N-H-18, stage 2 of CS-3052); the core's
   // lookup throws
   bool lookup_throws = false;
   try {
      const void *iterator = nullptr;
      static_cast<void>(ocs::tom::lookup_first<ocs::tom::FieldType::String>(
         a.get(), ocs::tom::field_pos(TEST_Type, TEST_string_ukey), "h10", &iterator));
   } catch (const ocs::tom::FieldException &e) {
      lookup_throws = e.reason() == ocs::tom::FieldException::Reason::DuplicateKey;
   }
   CHECK(62, "duplicate present: a lookup of the key is a logic error", lookup_throws);

   // T63-T64: the repair. unique_by_key() drops one of the pair, which is
   // where the hash entry was lost: the survivor stays in the list and becomes
   // unfindable through the key.
   ocs::tom::unique_by_key(a.get(), TEST_string_ukey);
   CHECK(63, "unique_by_key(): one of the two is gone", a->size() == 1);
   CHECK(64, "unique_by_key(): the survivor is still findable by its key", all_keys_resolve(a.get(), TEST_string_ukey));
   CHECK(65, "unique_by_key(): the key resolves to the element that is actually in the list",
         a->find_first(TEST_string_ukey, static_cast<const char *>("h10")).raw() == a->front().raw());
   a.reset();

   // T66: remove the SECOND of the pair. It is the one the hash points at, so
   // this is the case where the entry legitimately goes and has to be handed on
   // to the element left behind.
   ocs::tom::RawListPtr c = make_duplicate_list("c", "d", one_dup);
   c->erase(c->back());
   CHECK(66, "erase() of the second duplicate: the first stays findable",
         all_keys_resolve(c.get(), TEST_string_ukey) && c->size() == 1);
   c.reset();

   // T67: remove the FIRST of the pair — the one the hash does NOT point at.
   // Deleting by key rather than by entry takes the other one's entry with it.
   ocs::tom::RawListPtr e = make_duplicate_list("e", "f", one_dup);
   e->erase(e->front());
   CHECK(67, "erase() of the first duplicate: the second stays findable",
         all_keys_resolve(e.get(), TEST_string_ukey) && e->size() == 1);
   e.reset();

   // T68: the same through take(), which removes the hash entries on its own
   // rather than by way of freeing the element
   ocs::tom::RawListPtr g = make_duplicate_list("g", "h", one_dup);
   ocs::tom::RawElementPtr taken = g->take(g->front());
   CHECK(68, "take() of the first duplicate: the second stays findable",
         all_keys_resolve(g.get(), TEST_string_ukey) && g->size() == 1);
   taken.reset();
   g.reset();

   // T69: the setter route. No concatenation anywhere — writing a key that is
   // already taken puts the list in the same state.
   ocs::tom::RawListPtr i = make_keyed_list("i", three, 2);              // h10, h20
   i->back().set(TEST_string_ukey, static_cast<const char *>("h10"));   // now h10, h10
   i->erase(i->back());
   CHECK(69, "set() onto an existing key: the remaining element stays findable",
         all_keys_resolve(i.get(), TEST_string_ukey) && i->size() == 1);
   i.reset();

   // T70: the ordinary case must not regress. Distinct keys, one removed: the
   // others resolve, the removed one does not.
   ocs::tom::RawListPtr j = make_keyed_list("j", three, 3);
   j->erase(j->front().next());
   CHECK(70, "distinct keys: removal takes exactly its own entry out of the hash",
         all_keys_resolve(j.get(), TEST_string_ukey) &&
         j->size() == 2 &&
         !j->find_first(TEST_string_ukey, static_cast<const char *>("h20")));
}

// ---------------------------------------------------------------------------
// is_free()  [T71–T75]
//
// Code outside the list library asks is_free() instead of reading the element
// status. An element is free from its creation until it is bound to a list or
// stored as a sub-object, and free again once it is taken out.
// ---------------------------------------------------------------------------

/**
 * @brief Check is_free() through the life of an element
 */
static void test_is_free_elem() {
   printf("\n--- is_free() ---\n");

   auto owner = ocs::tom::RawElementPtr::create(TEST_Type);
   const ocs::tom::RawElement ep = owner.get();
   // T71: a new element is free
   CHECK(71, "a created element is free", ocs::tom::is_free(ep));

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "free");
   list->push_back(std::move(owner));
   // T72: an element in a list is not free
   CHECK(72, "an appended element is not free", !ocs::tom::is_free(ep));

   owner = list->take(ep);
   // T73: taking it out makes it free again
   CHECK(73, "a taken element is free again", ocs::tom::is_free(ep));

   auto parent = ocs::tom::RawElementPtr::create(TEST_Type);
   parent->set(TEST_object, std::move(owner));
   // T74: a sub-object is not free
   CHECK(74, "a sub-object is not free", !ocs::tom::is_free(parent->get<ocs::tom::ConstRawElement>(TEST_object)));

   // T75: nullptr is not a free element
   CHECK(75, "nullptr is not free", !ocs::tom::is_free(nullptr));
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   // the lists here are short; with the threshold at 0 every lookup goes through the hash tables
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size}});
   ocs::tom::detail::hash_set_min_elems(0);

   test_what_where();
   test_field_access();
   test_embedded_object();
   test_copy_elem();
   test_partial_pack();
   test_list_ops();
   test_sub_list();
   test_get_pos_in_descr();
   test_condition_search();
   test_elem_removal();
   test_traversal();
   test_elem_str_lookup();
   test_copy_list();
   test_ref_field();
   test_sort_and_uniq();
   test_atomic_ulong64();
   test_unique_key_integrity();
   test_is_free_elem();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
