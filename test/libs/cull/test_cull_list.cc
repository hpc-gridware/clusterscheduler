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

#include <cstdio>
#include <cstring>

#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_list.h"
#include "cull/cull_multitype.h"

// ---------------------------------------------------------------------------
// Local CULL type used by all tests
// ---------------------------------------------------------------------------

enum {
   TEST_int = 1,
   TEST_host,
   TEST_string,
   TEST_double,
   TEST_long,
   TEST_ulong,
   TEST_bool,
   TEST_list,
   TEST_object,
   TEST_ref
};

LISTDEF(TEST_Type)
   SGE_INT    (TEST_int,    CULL_DEFAULT)
   SGE_HOST   (TEST_host,   CULL_DEFAULT)
   SGE_STRING (TEST_string, CULL_DEFAULT)
   SGE_DOUBLE (TEST_double, CULL_DEFAULT)
   SGE_LONG   (TEST_long,   CULL_DEFAULT)
   SGE_ULONG  (TEST_ulong,  CULL_DEFAULT)
   SGE_BOOL   (TEST_bool,   CULL_DEFAULT)
   SGE_LIST   (TEST_list,   TEST_Type, CULL_DEFAULT)
   SGE_OBJECT (TEST_object, TEST_Type, CULL_DEFAULT)
   SGE_REF    (TEST_ref,    TEST_Type, CULL_DEFAULT)
LISTEND

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
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)

lNameSpace nmv[] = {
   {1, TEST_Size, TEST_Name, TEST_Type},
   {0, 0, nullptr, nullptr}
};

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

// ---------------------------------------------------------------------------
// lWhat / lWhere  [T01–T02]
//
// lWhat() builds an enumeration (field selection) from a format string.
// lWhere() builds a filter condition from a format string.
// Both must return nullptr for invalid or unsupported patterns rather than
// crashing or producing a corrupt object.
// ---------------------------------------------------------------------------

static void test_what_where() {
   printf("\n--- lWhat / lWhere ---\n");

   // T01: a syntactically malformed lWhat pattern must return nullptr
   lEnumeration *enp = lWhat("%T(%I%I->%T(%I%T))", TEST_Type,
                              TEST_int, TEST_int, TEST_Type, TEST_int, TEST_host);
   CHECK(1, "malformed lWhat pattern returns nullptr", enp == nullptr);
   if (enp != nullptr) { lFreeWhat(&enp); }

   // T02: "ALL" is not a valid WHERE condition and must return nullptr
   lCondition *cond = lWhere("%T(ALL)", TEST_Type);
   CHECK(2, "lWhere with ALL token returns nullptr", cond == nullptr);
   if (cond != nullptr) { lFreeWhere(&cond); }
}

// ---------------------------------------------------------------------------
// Scalar field set/get  [T03–T11]
//
// Every CULL field type has a lSetXxx / lGetXxx pair. Each must round-trip
// the stored value exactly. A second lSetXxx on the same field overwrites the
// first — the last written value is what lGetXxx returns (T11).
// ---------------------------------------------------------------------------

static void test_field_access() {
   printf("\n--- scalar field set/get ---\n");

   lListElem *ep = lCreateElem(TEST_Type);

   // T03: integer field roundtrip
   lSetInt(ep, TEST_int, 42);
   CHECK(3, "lSetInt/lGetInt roundtrip", lGetInt(ep, TEST_int) == 42);

   // T04: host field roundtrip
   lSetHost(ep, TEST_host, "myhost.example.com");
   CHECK(4, "lSetHost/lGetHost roundtrip", strcmp(lGetHost(ep, TEST_host), "myhost.example.com") == 0);

   // T05: string field roundtrip
   lSetString(ep, TEST_string, "hello");
   CHECK(5, "lSetString/lGetString roundtrip", strcmp(lGetString(ep, TEST_string), "hello") == 0);

   // T06: double field roundtrip (exact bit-for-bit — set and immediately get)
   lSetDouble(ep, TEST_double, 3.14);
   CHECK(6, "lSetDouble/lGetDouble roundtrip", lGetDouble(ep, TEST_double) == 3.14);

   // T07: long field roundtrip (negative value)
   lSetLong(ep, TEST_long, -7);
   CHECK(7, "lSetLong/lGetLong roundtrip", lGetLong(ep, TEST_long) == -7);

   // T08: unsigned long field roundtrip
   lSetUlong(ep, TEST_ulong, 99);
   CHECK(8, "lSetUlong/lGetUlong roundtrip", lGetUlong(ep, TEST_ulong) == 99);

   // T09: bool field roundtrip — true
   lSetBool(ep, TEST_bool, true);
   CHECK(9, "lSetBool/lGetBool: true", lGetBool(ep, TEST_bool) == true);

   // T10: bool field roundtrip — false (overwriting the true stored by T09)
   lSetBool(ep, TEST_bool, false);
   CHECK(10, "lSetBool/lGetBool: false", lGetBool(ep, TEST_bool) == false);

   // T11: a second lSetInt overwrites the first; lGetInt returns the last value
   lSetInt(ep, TEST_int, 1);
   lSetInt(ep, TEST_int, 100);
   CHECK(11, "lSetInt overwrites: last value wins", lGetInt(ep, TEST_int) == 100);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// Embedded object (lSetObject / lGetObject)  [T12–T13]
//
// A CULL element can store another element as a sub-object via lSetObject.
// lGetObject returns the stored pointer and the sub-object's own fields are
// accessible through it.
// ---------------------------------------------------------------------------

static void test_embedded_object() {
   printf("\n--- embedded object ---\n");

   lListElem *ep  = lCreateElem(TEST_Type);
   lListElem *obj = lCreateElem(TEST_Type);
   lSetInt(obj, TEST_int, 50);
   lSetObject(ep, TEST_object, obj);

   // T12: lGetObject returns the exact pointer that was stored
   CHECK(12, "lGetObject returns stored pointer", lGetObject(ep, TEST_object) == obj);

   // T13: the embedded object's own fields are accessible through the returned pointer
   CHECK(13, "embedded object int accessible via lGetObject",
         lGetInt(lGetObject(ep, TEST_object), TEST_int) == 50);

   lFreeElem(&ep); // frees ep; obj lifetime is managed by CULL after lSetObject
}

// ---------------------------------------------------------------------------
// lCopyElem  [T14–T16]
//
// lCopyElem() produces a deep copy of an element. The copy is a distinct
// allocation with the same field values; modifying one does not affect the other.
// ---------------------------------------------------------------------------

static void test_copy_elem() {
   printf("\n--- lCopyElem ---\n");

   lListElem *ep = lCreateElem(TEST_Type);
   lSetInt(ep, TEST_int, 42);
   lSetString(ep, TEST_string, "hello");

   lListElem *copy = lCopyElem(ep);

   // T14: the copy is a distinct allocation (not the same pointer)
   CHECK(14, "lCopyElem returns different pointer", copy != ep);

   // T15: integer field is preserved in the copy
   CHECK(15, "lCopyElem: int field preserved", lGetInt(copy, TEST_int) == 42);

   // T16: string field is preserved in the copy
   CHECK(16, "lCopyElem: string field preserved",
         lGetString(copy, TEST_string) != nullptr &&
         strcmp(lGetString(copy, TEST_string), "hello") == 0);

   lFreeElem(&copy);
   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// lCopyElemPartialPack  [T17–T21]
//
// lCopyElemPartialPack() copies a subset of fields from a source element into
// a pre-allocated destination element. The field selection is governed by an
// lEnumeration built with lWhat. Fields not covered by the enumeration keep
// their default values (0 / nullptr) in the destination.
// ---------------------------------------------------------------------------

static void test_partial_pack() {
   printf("\n--- lCopyElemPartialPack ---\n");

   lListElem *ep = lCreateElem(TEST_Type);
   lSetInt(ep, TEST_int, 42);
   lSetString(ep, TEST_string, "hello");
   lSetDouble(ep, TEST_double, 3.14);

   // full copy: ALL fields
   lListElem *full = lCreateElem(TEST_Type);
   lEnumeration *all_enp = lWhat("%T(ALL)", TEST_Type);
   int index = 0;
   lCopyElemPartialPack(full, &index, ep, all_enp, true, nullptr);

   // T17: a full copy has the int field
   CHECK(17, "full partial-pack: int field copied", lGetInt(full, TEST_int) == 42);

   // T18: a full copy has the string field
   CHECK(18, "full partial-pack: string field copied",
         lGetString(full, TEST_string) != nullptr &&
         strcmp(lGetString(full, TEST_string), "hello") == 0);

   lFreeElem(&full);
   lFreeWhat(&all_enp);

   // partial copy: only TEST_string and TEST_double
   lListElem *part = lCreateElem(TEST_Type);
   lEnumeration *part_enp = lWhat("%T(%I %I)", TEST_Type, TEST_string, TEST_double);
   index = lGetPosInDescr(TEST_Type, TEST_string);
   lCopyElemPartialPack(part, &index, ep, part_enp, true, nullptr);

   // T19: the selected string field is present in the partial copy
   CHECK(19, "partial-pack: string (selected) copied",
         lGetString(part, TEST_string) != nullptr &&
         strcmp(lGetString(part, TEST_string), "hello") == 0);

   // T20: the selected double field is present in the partial copy
   CHECK(20, "partial-pack: double (selected) copied",
         lGetDouble(part, TEST_double) == 3.14);

   // T21: the unselected int field retains the default value 0 in the partial copy
   CHECK(21, "partial-pack: int (not selected) is 0", lGetInt(part, TEST_int) == 0);

   lFreeElem(&part);
   lFreeWhat(&part_enp);
   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// List operations  [T22–T25]
//
// lCreateList / lAppendElem / lGetNumberOfElem / lFirst cover the basic
// lifecycle of a CULL list. Elements are owned by the list after lAppendElem;
// lFreeList frees them all.
// ---------------------------------------------------------------------------

static void test_list_ops() {
   printf("\n--- list operations ---\n");

   lList *list = lCreateList("test", TEST_Type);

   // T22: a freshly created list has no elements
   CHECK(22, "fresh list has 0 elements", lGetNumberOfElem(list) == 0);

   lListElem *e1 = lCreateElem(TEST_Type);
   lSetInt(e1, TEST_int, 10);
   lAppendElem(list, e1);

   // T23: after the first append the count is 1
   CHECK(23, "after first append, count is 1", lGetNumberOfElem(list) == 1);

   lListElem *e2 = lCreateElem(TEST_Type);
   lSetInt(e2, TEST_int, 20);
   lAppendElem(list, e2);

   // T24: after the second append the count is 2
   CHECK(24, "after second append, count is 2", lGetNumberOfElem(list) == 2);

   // T25: lFirst returns the first appended element
   CHECK(25, "lFirst returns first element", lFirst(list) == e1);

   lFreeList(&list); // frees e1 and e2
}

// ---------------------------------------------------------------------------
// lAddSubStr / lGetSubStr  [T26–T27]
//
// lAddSubStr appends an element to a named sub-list of an element, setting
// a string field on the new sub-element. lGetSubStr searches that sub-list
// by string value and returns the matching element.
// ---------------------------------------------------------------------------

static void test_sub_list() {
   printf("\n--- sub-list operations ---\n");

   lListElem *ep = lCreateElem(TEST_Type);
   lAddSubStr(ep, TEST_string, "needle", TEST_list, TEST_Type);

   const lListElem *found = lGetSubStr(ep, TEST_string, "needle", TEST_list);

   // T26: the added sub-element is found by its string value
   CHECK(26, "lGetSubStr finds the element added by lAddSubStr", found != nullptr);

   // T27: the found element's string field matches the value that was added
   CHECK(27, "lGetSubStr: found element has correct string value",
         found != nullptr && strcmp(lGetString(found, TEST_string), "needle") == 0);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// lGetPosInDescr  [T28–T29]
//
// lGetPosInDescr maps a field ID (enum value) to its 0-based position in the
// descriptor array. Returns -1 for field IDs not present in the descriptor.
// ---------------------------------------------------------------------------

static void test_get_pos_in_descr() {
   printf("\n--- lGetPosInDescr ---\n");

   // T28: a valid field ID yields a non-negative position
   CHECK(28, "lGetPosInDescr: valid field returns non-negative position",
         lGetPosInDescr(TEST_Type, TEST_string) >= 0);

   // T29: an unknown field ID returns -1
   CHECK(29, "lGetPosInDescr: invalid field returns -1",
         lGetPosInDescr(TEST_Type, 999) == -1);
}

// ---------------------------------------------------------------------------
// Condition-based search  [T30–T35]
//
// lWhere() builds a filter condition; lFindFirstRW() returns the first list
// element that satisfies it, or nullptr when none does. lCompare() tests a
// single element against a condition without iterating a list. lOrWhere()
// combines two conditions with logical OR; it takes ownership of both inputs.
// ---------------------------------------------------------------------------

static void test_condition_search() {
   printf("\n--- condition-based search ---\n");

   lList *list = lCreateList("cond", TEST_Type);
   lListElem *e1 = lCreateElem(TEST_Type);
   lSetString(e1, TEST_string, "alice");
   lAppendElem(list, e1);
   lListElem *e2 = lCreateElem(TEST_Type);
   lSetString(e2, TEST_string, "bob");
   lAppendElem(list, e2);

   // T30: a well-formed lWhere condition returns a non-null condition object
   lCondition *cond = lWhere("%T(%I==%s)", TEST_Type, TEST_string, "alice");
   CHECK(30, "well-formed lWhere returns non-null condition", cond != nullptr);

   // T31: lFindFirstRW returns the element whose field matches the condition
   lListElem *found = lFindFirstRW(list, cond);
   CHECK(31, "lFindFirstRW: matching element found", found == e1);

   // T32: lFindFirstRW returns nullptr when no element satisfies the condition
   lCondition *no_match = lWhere("%T(%I==%s)", TEST_Type, TEST_string, "nobody");
   CHECK(32, "lFindFirstRW: no match returns nullptr", lFindFirstRW(list, no_match) == nullptr);
   lFreeWhere(&no_match);

   // T33: lCompare returns non-zero when the element satisfies the condition
   CHECK(33, "lCompare: matching element returns non-zero", lCompare(e1, cond) != 0);

   // T34: lCompare returns 0 when the element does not satisfy the condition
   CHECK(34, "lCompare: non-matching element returns 0", lCompare(e2, cond) == 0);
   lFreeWhere(&cond);

   // T35: lOrWhere matches elements satisfying either of two conditions
   lCondition *c1 = lWhere("%T(%I==%s)", TEST_Type, TEST_string, "alice");
   lCondition *c2 = lWhere("%T(%I==%s)", TEST_Type, TEST_string, "bob");
   lCondition *or_cond = lOrWhere(c1, c2); // consumes c1 and c2
   CHECK(35, "lOrWhere: finds element matching second alternative",
         lFindFirstRW(list, or_cond) != nullptr);
   lFreeWhere(&or_cond);

   lFreeList(&list);
}

// ---------------------------------------------------------------------------
// Element removal and insertion  [T36–T40]
//
// lDechainElem() unlinks an element from a list without freeing it; the
// caller takes ownership and must free it. lRemoveElem() unlinks and frees
// the element and sets the caller's pointer to nullptr. lInsertElem() inserts
// a new element immediately AFTER the given element; passing nullptr inserts
// at the front of the list.
// ---------------------------------------------------------------------------

static void test_elem_removal() {
   printf("\n--- element removal and insertion ---\n");

   lList *list = lCreateList("rm", TEST_Type);
   lListElem *e1 = lCreateElem(TEST_Type); lSetInt(e1, TEST_int, 1); lAppendElem(list, e1);
   lListElem *e2 = lCreateElem(TEST_Type); lSetInt(e2, TEST_int, 2); lAppendElem(list, e2);
   lListElem *e3 = lCreateElem(TEST_Type); lSetInt(e3, TEST_int, 3); lAppendElem(list, e3);

   // T36: lDechainElem unlinks the element; count decreases but element is not freed
   lDechainElem(list, e2);
   CHECK(36, "lDechainElem: count decreases by 1", lGetNumberOfElem(list) == 2);
   lFreeElem(&e2); // caller owns the decheined element

   // T37: lRemoveElem frees the element and decreases the count
   lRemoveElem(list, &e3);
   CHECK(37, "lRemoveElem: count decreases by 1", lGetNumberOfElem(list) == 1);

   // T38: lRemoveElem sets the caller's pointer to nullptr
   CHECK(38, "lRemoveElem: caller pointer set to nullptr", e3 == nullptr);

   // T39: lInsertElem inserts AFTER the given element (list is [e1] → insert ins after e1 → [e1, ins])
   lListElem *ins = lCreateElem(TEST_Type); lSetInt(ins, TEST_int, 99);
   lInsertElem(list, lFirstRW(list), ins);
   CHECK(39, "lInsertElem: element is inserted after the given element",
         lNext(lFirst(list)) == ins);

   // T40: after the insert the count is 2 (e1 + ins)
   CHECK(40, "lInsertElem: count increases by 1", lGetNumberOfElem(list) == 2);

   lFreeList(&list);
}

// ---------------------------------------------------------------------------
// List traversal  [T41–T44]
//
// lNext() advances to the next element; lLast() returns the tail; lPrev()
// steps backward. lGetNumberOfRemainingElem() counts elements strictly BEHIND
// the given element (exclusive — the element itself is not counted).
// ---------------------------------------------------------------------------

static void test_traversal() {
   printf("\n--- traversal ---\n");

   lList *list = lCreateList("trav", TEST_Type);
   lListElem *e1 = lCreateElem(TEST_Type); lSetInt(e1, TEST_int, 1); lAppendElem(list, e1);
   lListElem *e2 = lCreateElem(TEST_Type); lSetInt(e2, TEST_int, 2); lAppendElem(list, e2);
   lListElem *e3 = lCreateElem(TEST_Type); lSetInt(e3, TEST_int, 3); lAppendElem(list, e3);

   // T41: lNext after lFirst returns the second element
   CHECK(41, "lNext after lFirst returns second element", lNext(lFirst(list)) == e2);

   // T42: lNext after the last element returns nullptr
   CHECK(42, "lNext after last element returns nullptr", lNext(lLast(list)) == nullptr);

   // T43: lLast returns the last appended element
   CHECK(43, "lLast returns the last appended element", lLast(list) == e3);

   // T44: from e2, only e3 is behind it — count is 1 (exclusive of e2 itself)
   CHECK(44, "lGetNumberOfRemainingElem from second element is 1",
         lGetNumberOfRemainingElem(e2) == 1);

   lFreeList(&list);
}

// ---------------------------------------------------------------------------
// String-key lookup  [T45–T46]
//
// lGetElemStr() searches a list for the first element whose named string field
// equals the given value. It returns nullptr when no element matches.
// ---------------------------------------------------------------------------

static void test_elem_str_lookup() {
   printf("\n--- lGetElemStr ---\n");

   lList *list = lCreateList("str", TEST_Type);
   lListElem *e1 = lCreateElem(TEST_Type); lSetString(e1, TEST_string, "alpha"); lAppendElem(list, e1);
   lListElem *e2 = lCreateElem(TEST_Type); lSetString(e2, TEST_string, "beta");  lAppendElem(list, e2);

   // T45: lGetElemStr finds the element with the matching string value
   CHECK(45, "lGetElemStr: finds element with matching string", lGetElemStr(list, TEST_string, "beta") == e2);

   // T46: lGetElemStr returns nullptr when no element has the given value
   CHECK(46, "lGetElemStr: returns nullptr for no match", lGetElemStr(list, TEST_string, "gamma") == nullptr);

   lFreeList(&list);
}

// ---------------------------------------------------------------------------
// lCopyList  [T47–T49]
//
// lCopyList() produces a deep copy of an entire list. The copy has the same
// number of elements; each element is a distinct allocation with the same
// field values as its source counterpart.
// ---------------------------------------------------------------------------

static void test_copy_list() {
   printf("\n--- lCopyList ---\n");

   lList *src = lCreateList("src", TEST_Type);
   lListElem *e1 = lCreateElem(TEST_Type); lSetInt(e1, TEST_int, 7); lAppendElem(src, e1);
   lListElem *e2 = lCreateElem(TEST_Type); lSetInt(e2, TEST_int, 8); lAppendElem(src, e2);

   lList *copy = lCopyList("copy", src);

   // T47: the copy has the same element count as the source
   CHECK(47, "lCopyList: copy has same element count", lGetNumberOfElem(copy) == lGetNumberOfElem(src));

   // T48: elements in the copy are distinct allocations
   CHECK(48, "lCopyList: first element is a different pointer", lFirst(copy) != lFirst(src));

   // T49: int field values are preserved in the copy
   CHECK(49, "lCopyList: int field value preserved", lGetInt(lFirst(copy), TEST_int) == 7);

   lFreeList(&copy);
   lFreeList(&src);
}

// ---------------------------------------------------------------------------
// lSetRef / lGetRef  [T50]
//
// The REF field stores a raw (unmanaged) pointer. lSetRef/lGetRef round-trip
// the value exactly; CULL does not dereference or free the stored pointer.
// ---------------------------------------------------------------------------

static void test_ref_field() {
   printf("\n--- lSetRef / lGetRef ---\n");

   lListElem *ep  = lCreateElem(TEST_Type);
   lListElem *ref = lCreateElem(TEST_Type);
   lSetRef(ep, TEST_ref, ref);

   // T50: lGetRef returns the exact pointer that was passed to lSetRef
   CHECK(50, "lSetRef/lGetRef: roundtrip returns same pointer",
         lGetRef(ep, TEST_ref) == ref);

   lFreeElem(&ref);
   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// Sorting and deduplication  [T51–T55]
//
// lPSortList() reorders list elements by a field; the format "%I+" means
// ascending, "%I-" means descending. lUniqStr() removes elements whose
// named string field is a duplicate of an earlier element's value.
// ---------------------------------------------------------------------------

static void test_sort_and_uniq() {
   printf("\n--- sort and deduplication ---\n");

   // build an unsorted list: 30, 10, 20
   lList *sort_list = lCreateList("sort", TEST_Type);
   lListElem *s1 = lCreateElem(TEST_Type); lSetInt(s1, TEST_int, 30); lAppendElem(sort_list, s1);
   lListElem *s2 = lCreateElem(TEST_Type); lSetInt(s2, TEST_int, 10); lAppendElem(sort_list, s2);
   lListElem *s3 = lCreateElem(TEST_Type); lSetInt(s3, TEST_int, 20); lAppendElem(sort_list, s3);

   lPSortList(sort_list, "%I+", TEST_int);

   // T51: after ascending sort, lFirst has the smallest value
   CHECK(51, "lPSortList ascending: lFirst has smallest int",
         lGetInt(lFirst(sort_list), TEST_int) == 10);

   // T52: after ascending sort, lLast has the largest value
   CHECK(52, "lPSortList ascending: lLast has largest int",
         lGetInt(lLast(sort_list), TEST_int) == 30);

   lFreeList(&sort_list);

   // build a list with a duplicate string: "alpha", "beta", "alpha"
   lList *uniq_list = lCreateList("uniq", TEST_Type);
   lListElem *u1 = lCreateElem(TEST_Type); lSetString(u1, TEST_string, "alpha"); lAppendElem(uniq_list, u1);
   lListElem *u2 = lCreateElem(TEST_Type); lSetString(u2, TEST_string, "beta");  lAppendElem(uniq_list, u2);
   lListElem *u3 = lCreateElem(TEST_Type); lSetString(u3, TEST_string, "alpha"); lAppendElem(uniq_list, u3);

   lUniqStr(uniq_list, TEST_string);

   // T53: the duplicate is removed; only 2 elements remain
   CHECK(53, "lUniqStr: duplicate removed (count = 2)", lGetNumberOfElem(uniq_list) == 2);

   // T54: the unique value "beta" is still present after deduplication
   CHECK(54, "lUniqStr: unique values preserved",
         lGetElemStr(uniq_list, TEST_string, "beta") != nullptr);

   // T55: the duplicated value "alpha" appears exactly once
   const lListElem *found = lGetElemStr(uniq_list, TEST_string, "alpha");
   CHECK(55, "lUniqStr: deduplicated value appears exactly once",
         found != nullptr && lGetElemStr(uniq_list, TEST_string, "alpha") == found);

   lFreeList(&uniq_list);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

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

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
