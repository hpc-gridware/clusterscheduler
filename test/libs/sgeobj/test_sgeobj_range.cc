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
#include <initializer_list>

#include "cull/cull.h"

#include "uti/sge_component.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_range.h"

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
// range_get_all_ids  [T01–T03]
//
// range_get_all_ids() reads the (min, max, step) triple from an RN_Type
// element.  On nullptr input all three output parameters are zeroed.
// A freshly created (zeroed) element also produces (0, 0, 0).
// ---------------------------------------------------------------------------

static void test_range_get_all_ids() {
   printf("\n--- range_get_all_ids ---\n");
   uint32_t mn, mx, st;

   // T01: nullptr range — outputs are zeroed without crashing
   mn = 1; mx = 6; st = 2;
   range_get_all_ids(nullptr, &mn, &mx, &st);
   CHECK(1, "nullptr → min=0 max=0 step=0", mn == 0 && mx == 0 && st == 0);

   // T02: freshly created (zeroed) element — also produces (0, 0, 0)
   {
      lListElem *r = lCreateElem(RN_Type);
      mn = 1; mx = 6; st = 2;
      range_get_all_ids(r, &mn, &mx, &st);
      CHECK(2, "zeroed element → min=0 max=0 step=0", mn == 0 && mx == 0 && st == 0);
      lFreeElem(&r);
   }

   // T03: set [1, 6, 2] then read back — roundtrip is exact
   {
      lListElem *r = lCreateElem(RN_Type);
      range_set_all_ids(r, 1, 6, 2);
      range_get_all_ids(r, &mn, &mx, &st);
      CHECK(3, "set [1,6,2] → get [1,6,2]", mn == 1 && mx == 6 && st == 2);
      lFreeElem(&r);
   }
}

// ---------------------------------------------------------------------------
// range_set_all_ids  [T04–T06]
//
// range_set_all_ids() stores (min, max, step) into an RN_Type element.
// When min == max the step is normalised to 1 regardless of what was passed.
// nullptr input is a documented no-op.
// ---------------------------------------------------------------------------

static void test_range_set_all_ids() {
   printf("\n--- range_set_all_ids ---\n");

   // T04: nullptr range — no crash, call is silently ignored
   range_set_all_ids(nullptr, 1, 6, 2);
   CHECK(4, "nullptr — no crash", true);

   // T05: normal range [1, 6, 2] round-trips correctly
   {
      lListElem *r = lCreateElem(RN_Type);
      uint32_t mn, mx, st;
      range_set_all_ids(r, 1, 6, 2);
      range_get_all_ids(r, &mn, &mx, &st);
      CHECK(5, "set [1,6,2] stores correctly", mn == 1 && mx == 6 && st == 2);
      lFreeElem(&r);
   }

   // T06: when min == max the step is normalised to 1 (single-element range)
   {
      lListElem *r = lCreateElem(RN_Type);
      uint32_t mn, mx, st;
      range_set_all_ids(r, 5, 5, 2);
      range_get_all_ids(r, &mn, &mx, &st);
      CHECK(6, "min==max → step normalised to 1", mn == 5 && mx == 5 && st == 1);
      lFreeElem(&r);
   }
}

// ---------------------------------------------------------------------------
// range_get_number_of_ids  [T07–T09]
//
// range_get_number_of_ids() returns 1 + (max - min) / step using integer
// division.  The stored 'max' is a field value, not necessarily the last
// element on the step grid.  For [1, 6, 2] the ids are 1, 3, 5 → 3 ids.
// ---------------------------------------------------------------------------

static void test_range_get_number_of_ids() {
   printf("\n--- range_get_number_of_ids ---\n");

   // T07: [1, 6, 2] — ids are 1, 3, 5 → 3 ids
   {
      lListElem *r = lCreateElem(RN_Type);
      range_set_all_ids(r, 1, 6, 2);
      CHECK(7, "[1,6,2] → 3 ids", range_get_number_of_ids(r) == 3);
      lFreeElem(&r);
   }

   // T08: [1, 1, 1] — single element → 1 id
   {
      lListElem *r = lCreateElem(RN_Type);
      range_set_all_ids(r, 1, 1, 1);
      CHECK(8, "[1,1,1] → 1 id", range_get_number_of_ids(r) == 1);
      lFreeElem(&r);
   }

   // T09: [2, 10, 2] — ids are 2, 4, 6, 8, 10 → 5 ids
   {
      lListElem *r = lCreateElem(RN_Type);
      range_set_all_ids(r, 2, 10, 2);
      CHECK(9, "[2,10,2] → 5 ids", range_get_number_of_ids(r) == 5);
      lFreeElem(&r);
   }
}

// ---------------------------------------------------------------------------
// range_is_id_within  [T10–T14]
//
// range_is_id_within() returns true when id satisfies:
//   id >= min  &&  id <= max  &&  (id - min) % step == 0
//
// For [1, 6, 2] the members on the step grid are 1, 3, 5.
// Note: max=6 is the stored field; 6 itself is NOT a member because
// (6 - 1) % 2 == 1 ≠ 0.
// ---------------------------------------------------------------------------

static void test_range_is_id_within() {
   printf("\n--- range_is_id_within ---\n");
   lListElem *r = lCreateElem(RN_Type);
   range_set_all_ids(r, 1, 6, 2);   // members: 1, 3, 5

   // T10: first element (min) is within range
   CHECK(10, "[1,6,2]: id=1 is within",   range_is_id_within(r, 1) == true);

   // T11: last grid element (5) is within range
   CHECK(11, "[1,6,2]: id=5 is within",   range_is_id_within(r, 5) == true);

   // T12: id=6 is the stored max but falls off the step grid — NOT within
   CHECK(12, "[1,6,2]: id=6 not on grid → not within", range_is_id_within(r, 6) == false);

   // T13: id beyond max is not within range
   CHECK(13, "[1,6,2]: id=7 beyond max → not within", range_is_id_within(r, 7) == false);

   // T14: nullptr range — returns false without crashing
   CHECK(14, "nullptr range → false",     range_is_id_within(nullptr, 1) == false);

   lFreeElem(&r);
}

// ---------------------------------------------------------------------------
// range_containes_id_less_than  [T15–T17]
//
// range_containes_id_less_than() returns true when range.min < id.
// The check is on the stored start value only — no step grid traversal.
// ---------------------------------------------------------------------------

static void test_range_containes_id_less_than() {
   printf("\n--- range_containes_id_less_than ---\n");
   lListElem *r = lCreateElem(RN_Type);
   range_set_all_ids(r, 1, 6, 2);   // min = 1

   // T15: min (1) is less than id (3) → true
   CHECK(15, "[1,6,2]: id=3 → start < id → true",
         range_containes_id_less_than(r, 3) == true);

   // T16: min (1) equals id (1) — NOT less than → false
   CHECK(16, "[1,6,2]: id=1 → start == id → false",
         range_containes_id_less_than(r, 1) == false);

   // T17: nullptr range — returns false without crashing
   CHECK(17, "nullptr range → false",
         range_containes_id_less_than(nullptr, 3) == false);

   lFreeElem(&r);
}

// ---------------------------------------------------------------------------
// range_correct_end  [T18–T19]
//
// range_correct_end() adjusts the stored max so it falls exactly on the step
// grid.  If (max - min) % step != 0, max is lowered to min + floor((max-min)/step)*step.
// A max that is already aligned is left unchanged.
// ---------------------------------------------------------------------------

static void test_range_correct_end() {
   printf("\n--- range_correct_end ---\n");
   uint32_t mn, mx, st;

   // T18: [1, 6, 2] — 6 is off the grid (5 is the true last) → corrected to [1, 5, 2]
   {
      lListElem *r = lCreateElem(RN_Type);
      range_set_all_ids(r, 1, 6, 2);
      range_correct_end(r);
      range_get_all_ids(r, &mn, &mx, &st);
      CHECK(18, "[1,6,2] corrected to [1,5,2]", mn == 1 && mx == 5 && st == 2);
      lFreeElem(&r);
   }

   // T19: [1, 7, 2] — 7 is already on the grid (1,3,5,7) → unchanged
   {
      lListElem *r = lCreateElem(RN_Type);
      range_set_all_ids(r, 1, 7, 2);
      range_correct_end(r);
      range_get_all_ids(r, &mn, &mx, &st);
      CHECK(19, "[1,7,2] already aligned — unchanged", mn == 1 && mx == 7 && st == 2);
      lFreeElem(&r);
   }
}

// ---------------------------------------------------------------------------
// range_list_insert_id / range_list_is_id_within  [T20–T23]
//
// range_list_insert_id() adds a single id to a range list, creating or
// extending range elements as needed.  range_list_is_id_within() checks
// membership across all ranges in the list.
// ---------------------------------------------------------------------------

static void test_range_list_insert_and_query() {
   printf("\n--- range_list_insert_id / range_list_is_id_within ---\n");
   lList *rl = nullptr;
   lList *al = nullptr;

   // T20: list starts empty — id not found before insert
   CHECK(20, "empty list: id 5 not found", range_list_is_id_within(rl, 5) == false);

   // T21: insert id 5 — now found in the list
   range_list_insert_id(&rl, &al, 5);
   CHECK(21, "after insert 5: id 5 found", range_list_is_id_within(rl, 5) == true);

   // T22: insert id 7 — both 5 and 7 are found; 6 is not
   range_list_insert_id(&rl, &al, 7);
   CHECK(22, "after insert 7: id 7 found", range_list_is_id_within(rl, 7) == true);
   CHECK(23, "id 6 not in list (was not inserted)", range_list_is_id_within(rl, 6) == false);

   lFreeList(&rl);
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_is_empty  [T24–T25]
//
// range_list_is_empty() returns true for a nullptr or zero-element list.
// ---------------------------------------------------------------------------

static void test_range_list_is_empty() {
   printf("\n--- range_list_is_empty ---\n");
   lList *al = nullptr;

   // T24: nullptr list — considered empty
   CHECK(24, "nullptr list is empty", range_list_is_empty(nullptr) == true);

   // T25: list with one id — not empty
   {
      lList *rl = nullptr;
      range_list_insert_id(&rl, &al, 3);
      CHECK(25, "list with id 3 is not empty", range_list_is_empty(rl) == false);
      lFreeList(&rl);
   }

   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_get_first_id / range_list_get_last_id  [T26–T27]
//
// These functions return the start of the first range element and the end of
// the last range element respectively.  For an empty list they return 0.
// ---------------------------------------------------------------------------

static void test_range_list_first_last() {
   printf("\n--- range_list_get_first_id / range_list_get_last_id ---\n");
   lList *al = nullptr;

   // build list: insert 1, 3, 5 → after compress these form a range [1-5:2]
   lList *rl = nullptr;
   range_list_insert_id(&rl, &al, 1);
   range_list_insert_id(&rl, &al, 3);
   range_list_insert_id(&rl, &al, 5);

   // T26: first id is 1
   CHECK(26, "first id is 1", range_list_get_first_id(rl, &al) == 1);

   // T27: last id is 5
   CHECK(27, "last id is 5", range_list_get_last_id(rl, &al) == 5);

   lFreeList(&rl);
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_get_number_of_ids  [T28–T29]
//
// Returns the total count of distinct ids across all ranges in the list.
// An empty or nullptr list returns 0.
// ---------------------------------------------------------------------------

static void test_range_list_number_of_ids() {
   printf("\n--- range_list_get_number_of_ids ---\n");
   lList *al = nullptr;

   // T28: nullptr list — 0 ids
   CHECK(28, "nullptr list → 0 ids", range_list_get_number_of_ids(nullptr) == 0);

   // T29: list with ids 1, 2, 3, 4, 5 inserted individually → 5 ids
   {
      lList *rl = nullptr;
      for (uint32_t i = 1; i <= 5; i++) {
         range_list_insert_id(&rl, &al, i);
      }
      CHECK(29, "5 inserted ids → count 5", range_list_get_number_of_ids(rl) == 5);
      lFreeList(&rl);
   }

   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_parse_from_string  [T30–T33]
//
// Parses a human-readable range string (e.g. "1-5:2", "3", "1-3") into an
// RN_Type list.  Returns true on success, false on error.
// ---------------------------------------------------------------------------

static void test_range_list_parse_from_string() {
   printf("\n--- range_list_parse_from_string ---\n");
   lList *al = nullptr;

   // T30: simple single id "3" — parses to a one-element list containing id 3
   {
      lList *rl = nullptr;
      bool ok = range_list_parse_from_string(&rl, &al, "3", false, true, false);
      CHECK(30, "\"3\" parses successfully", ok == true);
      CHECK(31, "\"3\" → contains id 3", range_list_is_id_within(rl, 3) == true);
      lFreeList(&rl);
      lFreeList(&al);
   }

   // T32: stepped range "1-5:2" — ids are 1, 3, 5 → 3 ids total
   {
      lList *rl = nullptr;
      bool ok = range_list_parse_from_string(&rl, &al, "1-5:2", false, true, false);
      CHECK(32, "\"1-5:2\" parses successfully", ok == true);
      CHECK(33, "\"1-5:2\" → 3 ids (1,3,5)", range_list_get_number_of_ids(rl) == 3);
      lFreeList(&rl);
      lFreeList(&al);
   }
}

// ---------------------------------------------------------------------------
// range_is_id_within — id between grid points  [T34]
//
// Additional scenario: an id that falls within [min, max] but is not on the
// step grid must be rejected.  This complements T12 (id == stored max).
// ---------------------------------------------------------------------------

static void test_range_is_id_within_extended() {
   printf("\n--- range_is_id_within (step-grid check) ---\n");
   lListElem *r = lCreateElem(RN_Type);
   range_set_all_ids(r, 1, 6, 2);   // members: 1, 3, 5

   // T34: id=2 is in [1,6] but not on the step grid → false
   CHECK(34, "[1,6,2]: id=2 in bounds but off grid → false",
         range_is_id_within(r, 2) == false);

   lFreeElem(&r);
}

// ---------------------------------------------------------------------------
// range_list_remove_id  [T35–T37]
//
// range_list_remove_id() removes a single id from a range list, splitting or
// shrinking the affected range element as necessary:
//   - single-element range → range element deleted, list freed when empty
//   - id at start → start advances by one step
//   - id in the middle → range is split into two separate elements
// ---------------------------------------------------------------------------

static void test_range_list_remove_id() {
   printf("\n--- range_list_remove_id ---\n");
   lList *al = nullptr;

   // T35: remove the only id from a list → list becomes nullptr (freed)
   {
      lList *rl = nullptr;
      range_list_insert_id(&rl, &al, 5);
      range_list_remove_id(&rl, &al, 5);
      CHECK(35, "remove only id → list is nullptr", rl == nullptr);
   }

   // T36: remove start of a multi-id range → start advances by step
   {
      lList *rl = nullptr;
      for (uint32_t i = 1; i <= 5; i++) {
         range_list_insert_id(&rl, &al, i);
      }
      // list is 1-5:1; remove id 1 → 2-5:1
      range_list_remove_id(&rl, &al, 1);
      CHECK(36, "remove start: id 1 gone",  range_list_is_id_within(rl, 1) == false);
      CHECK(37, "remove start: id 2 still present", range_list_is_id_within(rl, 2) == true);
      lFreeList(&rl);
   }

   // T37: remove id from middle of range → range splits; both sides remain
   {
      lList *rl = nullptr;
      for (uint32_t i = 1; i <= 5; i++) {
         range_list_insert_id(&rl, &al, i);
      }
      // list is 1-5:1; remove id 3 → {1,2} and {4,5}
      range_list_remove_id(&rl, &al, 3);
      CHECK(38, "remove middle: id 3 gone",         range_list_is_id_within(rl, 3) == false);
      CHECK(39, "remove middle: id 2 still present", range_list_is_id_within(rl, 2) == true);
      CHECK(40, "remove middle: id 4 still present", range_list_is_id_within(rl, 4) == true);
      lFreeList(&rl);
   }

   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_sort_uniq_compress  [T41–T42]
//
// range_list_sort_uniq_compress() sorts the list by start id, removes
// overlapping/duplicate ids, and merges adjacent ranges.  After the call
// the list is in canonical (sorted, non-overlapping, compressed) form.
// ---------------------------------------------------------------------------

static void test_range_list_sort_uniq_compress() {
   printf("\n--- range_list_sort_uniq_compress ---\n");
   lList *al = nullptr;

   // Build an unordered list: insert 5, 3, 1 individually
   lList *rl = nullptr;
   range_list_insert_id(&rl, &al, 5);
   range_list_insert_id(&rl, &al, 3);
   range_list_insert_id(&rl, &al, 1);

   range_list_sort_uniq_compress(rl, &al, true);

   // T41: all three ids are still present after compression
   CHECK(41, "after compress: id 1 present", range_list_is_id_within(rl, 1) == true);
   CHECK(42, "after compress: id 5 present", range_list_is_id_within(rl, 5) == true);

   // T42: total count is preserved (no ids lost or duplicated)
   CHECK(43, "after compress: 3 ids total", range_list_get_number_of_ids(rl) == 3);

   lFreeList(&rl);
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_get_average  [T44–T45]
//
// range_list_get_average() returns the arithmetic mean of all ids in the list.
// An optional upperbound caps the max of each range before averaging.
// Returns 0 for a nullptr or empty list.
// ---------------------------------------------------------------------------

static void test_range_list_get_average() {
   printf("\n--- range_list_get_average ---\n");
   lList *al = nullptr;

   // T44: average of ids 1,2,3,4,5 → 3.0
   {
      lList *rl = nullptr;
      for (uint32_t i = 1; i <= 5; i++) {
         range_list_insert_id(&rl, &al, i);
      }
      double avg = range_list_get_average(rl, 0);
      CHECK(44, "average of 1-5 → 3.0", avg == 3.0);
      lFreeList(&rl);
   }

   // T45: nullptr list → average is 0
   CHECK(45, "nullptr list → average 0", range_list_get_average(nullptr, 0) == 0.0);

   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// Set operations  [T46–T51]
//
// range_list_calculate_union_set()       — all ids from both lists
// range_list_calculate_difference_set()  — ids in list1 that are not in list2
// range_list_calculate_intersection_set()— ids present in both lists
// ---------------------------------------------------------------------------

static lList *make_range_list(std::initializer_list<uint32_t> ids) {
   lList *rl = nullptr;
   lList *al = nullptr;
   for (uint32_t id : ids) {
      range_list_insert_id(&rl, &al, id);
   }
   lFreeList(&al);
   return rl;
}

static void test_set_operations() {
   printf("\n--- set operations ---\n");
   lList *al = nullptr;

   // union: {1,2} ∪ {4,5} → {1,2,4,5}
   {
      lList *a = make_range_list({1, 2});
      lList *b = make_range_list({4, 5});
      lList *u = nullptr;
      range_list_calculate_union_set(&u, &al, a, b);
      CHECK(46, "union {1,2}+{4,5}: id 1 present", range_list_is_id_within(u, 1) == true);
      CHECK(47, "union {1,2}+{4,5}: id 4 present", range_list_is_id_within(u, 4) == true);
      CHECK(48, "union {1,2}+{4,5}: 4 ids total",  range_list_get_number_of_ids(u) == 4);
      lFreeList(&a); lFreeList(&b); lFreeList(&u);
   }

   // difference: {1,2,3,4,5} − {2,4} → {1,3,5}
   {
      lList *a = make_range_list({1, 2, 3, 4, 5});
      lList *b = make_range_list({2, 4});
      lList *d = nullptr;
      range_list_calculate_difference_set(&d, &al, a, b);
      CHECK(49, "diff {1-5}-{2,4}: id 3 present", range_list_is_id_within(d, 3) == true);
      CHECK(50, "diff {1-5}-{2,4}: id 2 absent",  range_list_is_id_within(d, 2) == false);
      CHECK(51, "diff {1-5}-{2,4}: 3 ids total",  range_list_get_number_of_ids(d) == 3);
      lFreeList(&a); lFreeList(&b); lFreeList(&d);
   }

   // intersection: {1,2,3} ∩ {2,3,4} → {2,3}
   {
      lList *a = make_range_list({1, 2, 3});
      lList *b = make_range_list({2, 3, 4});
      lList *i = nullptr;
      range_list_calculate_intersection_set(&i, &al, a, b);
      CHECK(52, "intersect {1-3}∩{2-4}: id 2 present", range_list_is_id_within(i, 2) == true);
      CHECK(53, "intersect {1-3}∩{2-4}: id 1 absent",  range_list_is_id_within(i, 1) == false);
      CHECK(54, "intersect {1-3}∩{2-4}: 2 ids total",  range_list_get_number_of_ids(i) == 2);
      lFreeList(&a); lFreeList(&b); lFreeList(&i);
   }

   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_containes_id_less_than  [T55–T56]
//
// List-level wrapper: returns true if any range in the list has start < id.
// ---------------------------------------------------------------------------

static void test_range_list_containes_id_less_than() {
   printf("\n--- range_list_containes_id_less_than ---\n");
   lList *al = nullptr;

   lList *rl = make_range_list({3, 4, 5});

   // T55: list start (3) is less than id 5 → true
   CHECK(55, "list start 3 < id 5 → true",
         range_list_containes_id_less_than(rl, 5) == true);

   // T56: list start (3) equals id 3 — not strictly less than → false
   CHECK(56, "list start 3 == id 3 → false",
         range_list_containes_id_less_than(rl, 3) == false);

   lFreeList(&rl);
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_get_first_id / range_list_get_last_id — nullptr list  [T57]
//
// Both functions return 0 and add an error to the answer list when the list
// is nullptr or empty.
// ---------------------------------------------------------------------------

static void test_range_list_first_last_nullptr() {
   printf("\n--- range_list_get_first_id / last — nullptr ---\n");
   lList *al = nullptr;

   // T57: nullptr list → first id is 0
   CHECK(57, "nullptr list → first id 0", range_list_get_first_id(nullptr, &al) == 0);

   // T58: nullptr list → last id is 0
   CHECK(58, "nullptr list → last id 0", range_list_get_last_id(nullptr, &al) == 0);

   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// range_list_parse_from_string — additional cases  [T59–T62]
// ---------------------------------------------------------------------------

static void test_range_list_parse_extended() {
   printf("\n--- range_list_parse_from_string (extended) ---\n");
   lList *al = nullptr;

   // T59: contiguous range "1-3" (no explicit step) → ids 1, 2, 3 → 3 ids
   {
      lList *rl = nullptr;
      bool ok = range_list_parse_from_string(&rl, &al, "1-3", false, true, false);
      CHECK(59, "\"1-3\" parses successfully", ok == true);
      CHECK(60, "\"1-3\" → 3 ids", range_list_get_number_of_ids(rl) == 3);
      CHECK(61, "\"1-3\" → id 2 present", range_list_is_id_within(rl, 2) == true);
      lFreeList(&rl);
      lFreeList(&al);
   }

   // T62: a single unparseable token is treated as "undefined" (returns true, empty list)
   //      — documents current behavior; this mirrors the NONE sentinel convention
   {
      lList *rl = nullptr;
      bool ok = range_list_parse_from_string(&rl, &al, "abc", false, true, false);
      CHECK(62, "single unparseable token treated as undefined → true", ok == true);
      CHECK(63, "undefined parse → list is empty", range_list_is_empty(rl));
      lFreeList(&rl);
      lFreeList(&al);
   }

   // T64: valid first token followed by an invalid second token → parse fails
   //      (the second token may not be undefined after a valid first range)
   {
      lList *rl = nullptr;
      bool ok = range_list_parse_from_string(&rl, &al, "1,abc", false, true, false);
      CHECK(64, "valid then invalid token → parse fails", ok == false);
      lFreeList(&rl);
      lFreeList(&al);
   }
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   component_set_daemonized(true);   // suppress sge_log stderr output during tests
   lInit(nmv);

   test_range_get_all_ids();
   test_range_set_all_ids();
   test_range_get_number_of_ids();
   test_range_is_id_within();
   test_range_containes_id_less_than();
   test_range_correct_end();
   test_range_list_insert_and_query();
   test_range_list_is_empty();
   test_range_list_first_last();
   test_range_list_number_of_ids();
   test_range_list_parse_from_string();
   test_range_is_id_within_extended();
   test_range_list_remove_id();
   test_range_list_sort_uniq_compress();
   test_range_list_get_average();
   test_set_operations();
   test_range_list_containes_id_less_than();
   test_range_list_first_last_nullptr();
   test_range_list_parse_extended();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
