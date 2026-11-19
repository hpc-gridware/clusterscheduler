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
 * @brief Unit tests for object in `libs/sgeobj`
 */

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"
#include "cull/cull_pack.h"
#include "cull/cull_what.h"
#include "cull/cull_where.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_object_type.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pack.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "tom/ocs_Element.h"

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

// ---------------------------------------------------------------------------
// object_has_type — type identity check  [T01–T04]
//
// object_has_type() compares an element's descriptor pointer against the
// given descriptor and returns false for any nullptr argument.
// ---------------------------------------------------------------------------

static void test_object_has_type(lListElem *queue) {
   printf("\n--- object_has_type ---\n");

   // T01: element descriptor matches the expected type
   CHECK(1,  "queue matches QU_Type",          object_has_type(queue, QU_Type) == true);

   // T02: element descriptor does NOT match a different type
   CHECK(2,  "queue does not match JB_Type",   object_has_type(queue, JB_Type) == false);

   // T03: nullptr element → false without crash
   CHECK(3,  "nullptr element → false",        object_has_type(nullptr, QU_Type) == false);

   // T04: nullptr descriptor → false without crash
   CHECK(4,  "nullptr descr → false",          object_has_type(queue, nullptr) == false);
}

// ---------------------------------------------------------------------------
// object_get_type — retrieve the descriptor of an element  [T05–T06]
// ---------------------------------------------------------------------------

static void test_object_get_type(lListElem *queue) {
   printf("\n--- object_get_type ---\n");

   // T05: valid element returns its own descriptor
   CHECK(5,  "queue → QU_Type descriptor",     object_get_type(queue) == QU_Type);

   // T06: nullptr element → nullptr
   CHECK(6,  "nullptr element → nullptr",      object_get_type(nullptr) == nullptr);
}

// ---------------------------------------------------------------------------
// object_get_subtype — descriptor of an attribute's sub-list  [T07–T09]
//
// Returns the element descriptor for a list-type attribute; returns nullptr
// for scalar attributes and the NoName sentinel.
// ---------------------------------------------------------------------------

static void test_object_get_subtype() {
   printf("\n--- object_get_subtype ---\n");

   // T07: sub-list attribute QU_acl carries US_Type elements
   CHECK(7,  "QU_acl sub-type → US_Type",      object_get_subtype(QU_acl) == US_Type);

   // T08: scalar attribute has no sub-type
   CHECK(8,  "QU_qname (scalar) → nullptr",    object_get_subtype(QU_qname) == nullptr);

   // T09: NoName sentinel → nullptr
   CHECK(9,  "NoName → nullptr",               object_get_subtype(NoName) == nullptr);
}

// ---------------------------------------------------------------------------
// object_get_primary_key — key attribute nm for a descriptor  [T10–T11]
// ---------------------------------------------------------------------------

static void test_object_get_primary_key() {
   printf("\n--- object_get_primary_key ---\n");

   // T10: JB_Type is keyed by JB_job_number
   CHECK(10, "JB_Type key → JB_job_number",    object_get_primary_key(JB_Type) == JB_job_number);

   // T11: nullptr descriptor → NoName
   CHECK(11, "nullptr → NoName",               object_get_primary_key(nullptr) == NoName);
}

// ---------------------------------------------------------------------------
// object_get_name — human-readable type name from descriptor  [T12–T14]
//
// Searches object_base[] for a matching descriptor pointer and returns its
// registered type_name string.  Unrecognised descriptors return "unknown".
// nullptr maps to "unknown" without crashing.
// ---------------------------------------------------------------------------

static void test_object_get_name() {
   printf("\n--- object_get_name ---\n");

   // T12: JB_Type descriptor is registered as "JOB"
   CHECK(12, "JB_Type → \"JOB\"",              strcmp(object_get_name(JB_Type), "JOB") == 0);

   // T13: QU_Type descriptor is registered as "QINSTANCE"
   CHECK(13, "QU_Type → \"QINSTANCE\"",        strcmp(object_get_name(QU_Type), "QINSTANCE") == 0);

   // T14: nullptr → "unknown" (safe fallback, no crash)
   CHECK(14, "nullptr → \"unknown\"",          strcmp(object_get_name(nullptr), "unknown") == 0);
}

// ---------------------------------------------------------------------------
// object_get_name_prefix — CULL attribute name prefix  [T15–T18]
//
// Derives the prefix string from the first attribute in the descriptor
// (e.g. QU_qname → "QU_", JB_job_number → "JB_").
// Returns nullptr when either argument is nullptr.
// ---------------------------------------------------------------------------

static void test_object_get_name_prefix() {
   printf("\n--- object_get_name_prefix ---\n");

   dstring buf = DSTRING_INIT;

   // T15: QU_Type first attribute is QU_qname → prefix "QU_"
   CHECK(15, "QU_Type → \"QU_\"",              strcmp(object_get_name_prefix(QU_Type, &buf), "QU_") == 0);

   // T16: JB_Type first attribute is JB_job_number → prefix "JB_"
   CHECK(16, "JB_Type → \"JB_\"",              strcmp(object_get_name_prefix(JB_Type, &buf), "JB_") == 0);

   // T17: nullptr descriptor → nullptr
   CHECK(17, "nullptr descr → nullptr",        object_get_name_prefix(nullptr, &buf) == nullptr);

   // T18: nullptr buffer → nullptr
   CHECK(18, "nullptr buffer → nullptr",       object_get_name_prefix(JB_Type, nullptr) == nullptr);

   sge_dstring_free(&buf);
}

// ---------------------------------------------------------------------------
// object_type_get_name — type name from sge_object_type enum  [T19–T22]
//
// SGE_TYPE_ALL maps to the special name "default".
// Out-of-range values (SGE_TYPE_NONE) trigger an ERROR log and return "unknown".
// ---------------------------------------------------------------------------

static void test_object_type_get_name() {
   printf("\n--- object_type_get_name ---\n");

   // T19: known type returns its registered name
   CHECK(19, "SGE_TYPE_JOB → \"JOB\"",         strcmp(object_type_get_name(SGE_TYPE_JOB), "JOB") == 0);

   // T20: another registered type
   CHECK(20, "SGE_TYPE_CQUEUE → \"CQUEUE\"",   strcmp(object_type_get_name(SGE_TYPE_CQUEUE), "CQUEUE") == 0);

   // T21: SGE_TYPE_ALL sentinel → "default"
   CHECK(21, "SGE_TYPE_ALL → \"default\"",     strcmp(object_type_get_name(SGE_TYPE_ALL), "default") == 0);

   // T22: SGE_TYPE_NONE is out of range → "unknown" (error suppressed by daemonized flag)
   CHECK(22, "SGE_TYPE_NONE → \"unknown\"",    strcmp(object_type_get_name(SGE_TYPE_NONE), "unknown") == 0);
}

// ---------------------------------------------------------------------------
// object_name_get_type — reverse lookup from name string to type  [T23–T25]
//
// Lookup is case-insensitive.  Names in "NAME:key" form are supported by
// stripping the colon suffix.  Unrecognised names return SGE_TYPE_ALL.
// ---------------------------------------------------------------------------

static void test_object_name_get_type() {
   printf("\n--- object_name_get_type ---\n");

   // T23: exact name lookup
   CHECK(23, "\"JOB\" → SGE_TYPE_JOB",         object_name_get_type("JOB") == SGE_TYPE_JOB);

   // T24: another registered name
   CHECK(24, "\"CQUEUE\" → SGE_TYPE_CQUEUE",   object_name_get_type("CQUEUE") == SGE_TYPE_CQUEUE);

   // T25: unrecognised name → SGE_TYPE_ALL (not-found sentinel)
   CHECK(25, "unknown → SGE_TYPE_ALL",         object_name_get_type("NOSUCHTYPE") == SGE_TYPE_ALL);
}

// ---------------------------------------------------------------------------
// object_type_get_descr / object_type_get_key_nm  [T26–T31]
//
// object_type_get_descr() returns the CULL descriptor for a registered type,
// or nullptr for synthetic message types (SHUTDOWN, etc.) that carry no
// objects.
// object_type_get_key_nm() returns the primary-key attribute nm, or NoName
// for descriptor-less types.
// ---------------------------------------------------------------------------

static void test_object_type_get_descr_and_key() {
   printf("\n--- object_type_get_descr / object_type_get_key_nm ---\n");

   // T26: SGE_TYPE_JOB maps to the JB_Type descriptor
   CHECK(26, "SGE_TYPE_JOB descr → JB_Type",       object_type_get_descr(SGE_TYPE_JOB) == JB_Type);

   // T27: synthetic type SHUTDOWN carries no descriptor
   CHECK(27, "SGE_TYPE_SHUTDOWN descr → nullptr",   object_type_get_descr(SGE_TYPE_SHUTDOWN) == nullptr);

   // T28: SGE_TYPE_CQUEUE is keyed by CQ_name (string key)
   CHECK(28, "SGE_TYPE_CQUEUE key → CQ_name",       object_type_get_key_nm(SGE_TYPE_CQUEUE) == CQ_name);

   // T29: SGE_TYPE_JOB is keyed by JB_job_number (integer key)
   CHECK(29, "SGE_TYPE_JOB key → JB_job_number",    object_type_get_key_nm(SGE_TYPE_JOB) == JB_job_number);

   // T30: synthetic type with no object has NoName as key
   CHECK(30, "SGE_TYPE_SHUTDOWN key → NoName",      object_type_get_key_nm(SGE_TYPE_SHUTDOWN) == NoName);

   // T31: role type (CS-2027 era) is registered and keyed by RL_name
   CHECK(31, "SGE_TYPE_RL key → RL_name",           object_type_get_key_nm(SGE_TYPE_RL) == RL_name);
}

// ---------------------------------------------------------------------------
// ocs_object_types / ocs_with_object_type  [T67–T74]
//
// The compile-time table maps every sge_object_type to its object type and
// key field; the dispatcher calls a generic callable with the entry of a type
// known only at run time. The run-time table (object_type_get_descr(),
// object_type_get_key_nm()) is generated from the same tuple, so these checks
// make sure the dispatch picks the entry at the type's index.
// ---------------------------------------------------------------------------

static_assert(std::is_same_v<ocs_object_type_t<SGE_TYPE_JOB>::type, ocs::tom::JB>, "SGE_TYPE_JOB holds jobs");
static_assert(ocs_object_type_t<SGE_TYPE_JOB>::key == JB_job_number, "jobs are keyed by JB_job_number");
static_assert(ocs_object_type_t<SGE_TYPE_EXECHOST>::key == EH_name, "execution hosts are keyed by EH_name");
static_assert(ocs_object_type_t<SGE_TYPE_SCHEDD_CONF>::has_object && !ocs_object_type_t<SGE_TYPE_SCHEDD_CONF>::has_key,
              "the scheduler configuration has an object, but no key");
static_assert(!ocs_object_type_t<SGE_TYPE_SHUTDOWN>::has_object, "the shutdown carries no object");
static_assert(ocs_object_descriptions[SGE_TYPE_CQUEUE].descr == CQ_Type, "the run-time table is generated");

/** @brief What the dispatch test's callable reports about the entry it was called with */
struct DispatchedEntry {
   sge_object_type value;   ///< the entry's sge_object_type value
   const lDescr *descr;     ///< the descriptor of the entry's object type
   int key_nm;              ///< the entry's key field, NoName without a key
   const char *name;        ///< the entry's type name
};

/** @brief ocs_with_object_type() calls its callable with the entry of the type it is given [T67–T74] */
static void test_object_type_dispatch() {
   printf("\n--- ocs_with_object_type ---\n");

   bool descr_agrees = true;
   bool key_agrees = true;
   bool value_and_name_agree = true;
   bool key_in_descr = true;
   bool not_called_without_object = true;

   for (int i = SGE_TYPE_FIRST; i < SGE_TYPE_ALL; i++) {
      const auto type = static_cast<sge_object_type>(i);
      const auto dispatched = ocs_with_object_type(type, [](const auto &entry) {
         using E = ocs_object_type_entry_t<decltype(entry)>;
         return DispatchedEntry{E::value, E::type::descriptor.e, static_cast<int>(E::key), entry.name};
      });
      const lDescr *descr = object_type_get_descr(type);

      if (descr == nullptr) {
         if (dispatched) {
            printf("      %s carries no object but was dispatched\n", object_type_get_name(type));
            not_called_without_object = false;
         }
         continue;
      }
      if (!dispatched || dispatched->descr != descr) {
         printf("      %s dispatched to another descriptor\n", object_type_get_name(type));
         descr_agrees = false;
         continue;
      }
      if (dispatched->key_nm != object_type_get_key_nm(type)) {
         printf("      %s dispatched with another key field\n", object_type_get_name(type));
         key_agrees = false;
      }
      if (dispatched->value != type || strcmp(dispatched->name, object_type_get_name(type)) != 0) {
         printf("      %s dispatched to the entry of another value\n", object_type_get_name(type));
         value_and_name_agree = false;
      }
      if (dispatched->key_nm != NoName && lGetPosInDescr(descr, dispatched->key_nm) < 0) {
         printf("      %s: key field not in the descriptor\n", object_type_get_name(type));
         key_in_descr = false;
      }
   }

   // T67: every type with a descriptor dispatches to the object type of that descriptor
   CHECK(67, "dispatch → object type of object_type_get_descr()", descr_agrees);

   // T68: the dispatched key field is the one object_type_get_key_nm() names
   CHECK(68, "dispatch → key field of object_type_get_key_nm()", key_agrees);

   // T69: the dispatched entry is the one of the value, with its name
   CHECK(69, "dispatch → entry of the same value and name", value_and_name_agree);

   // T70: every key field is a field of its type's descriptor
   CHECK(70, "key field is in the descriptor", key_in_descr);

   // T71: a type without an object does not call the callable
   CHECK(71, "no dispatch for a type without an object", not_called_without_object);

   // T72: values outside the enumeration do not call the callable
   const auto unused = [](const auto &) { return 1; };
   CHECK(72, "no dispatch for SGE_TYPE_ALL and SGE_TYPE_NONE",
         !ocs_with_object_type(SGE_TYPE_ALL, unused) && !ocs_with_object_type(SGE_TYPE_NONE, unused));

   // T73: a callable returning nothing yields whether it was called
   int calls = 0;
   const auto count = [&calls](const auto &) { ++calls; };
   const bool job_called = ocs_with_object_type(SGE_TYPE_JOB, count);
   const bool shutdown_called = ocs_with_object_type(SGE_TYPE_SHUTDOWN, count);
   CHECK(73, "void callable → true if called", job_called && !shutdown_called && calls == 1);

   // T74: the typed body finds an element by the dispatched key field
   lList *cqueue_list = nullptr;
   ocs::tom::emplace_back<CQ_name>(&cqueue_list, "all.q");
   const auto found = ocs_with_object_type(SGE_TYPE_CQUEUE, [cqueue_list](const auto &entry) {
      using E = ocs_object_type_entry_t<decltype(entry)>;
      // nested: the traits of the key exist only for a type that has a key
      if constexpr (E::has_key) {
         if constexpr (ocs::tom::FieldTraits<E::key>::type == ocs::tom::FieldType::String) {
            return static_cast<bool>(ocs::tom::find_first<E::key>(cqueue_list, "all.q"));
         } else {
            return false;
         }
      } else {
         return false;
      }
   });
   CHECK(74, "typed lookup by the dispatched key field", found.value_or(false));
   lFreeList(&cqueue_list);
}

// ---------------------------------------------------------------------------
// object_verify_cull — structural type check on a single element  [T32–T37]
//
// With a nullptr descriptor the call verifies only that every sub-list in
// the element contains the right element type (as declared in the descriptor).
// When a descriptor is provided the element's own type must also match.
// ---------------------------------------------------------------------------

static void test_object_verify_cull(lListElem *queue, lList *st_list) {
   printf("\n--- object_verify_cull ---\n");

   // T32: both pointers nullptr → false
   CHECK(32, "nullptr elem, nullptr descr → false",  object_verify_cull(nullptr, nullptr) == false);

   // T33: valid element with nullptr descriptor → true (sub-list check only, no type guard)
   CHECK(33, "queue, nullptr descr → true",           object_verify_cull(queue, nullptr) == true);

   // T34: element type matches the given descriptor → true
   CHECK(34, "queue matches QU_Type → true",          object_verify_cull(queue, QU_Type) == true);

   // T35: element type does NOT match the given descriptor → false
   CHECK(35, "queue vs JB_Type → false",              object_verify_cull(queue, JB_Type) == false);

   // T36: sub-list with the correct element type (QU_pe_list expects ST_Type) → true
   ocs::tom::set<QU_pe_list>(queue, lCopyList("pe_list", st_list));
   CHECK(36, "QU_pe_list with ST_Type elements → true",  object_verify_cull(queue, QU_Type) == true);

   // T37: sub-list with wrong element type — QU_owner_list expects US_Type, not ST_Type → false
   ocs::tom::set<QU_owner_list>(queue, lCopyList("owner_list", st_list));
   CHECK(37, "QU_owner_list with ST_Type elements → false", object_verify_cull(queue, QU_Type) == false);
}

// ---------------------------------------------------------------------------
// object_list_verify_cull — structural type check on a list  [T38–T41]
// ---------------------------------------------------------------------------

static void test_object_list_verify_cull(lList *st_list) {
   printf("\n--- object_list_verify_cull ---\n");

   // T38: both pointers nullptr → false
   CHECK(38, "nullptr list, nullptr descr → false",   object_list_verify_cull(nullptr, nullptr) == false);

   // T39: valid list with nullptr descriptor → true (type inferred from elements)
   CHECK(39, "st_list, nullptr descr → true",         object_list_verify_cull(st_list, nullptr) == true);

   // T40: list element type matches the expected descriptor → true
   CHECK(40, "st_list matches ST_Type → true",        object_list_verify_cull(st_list, ST_Type) == true);

   // T41: list element type does NOT match the expected descriptor → false
   CHECK(41, "st_list vs QU_Type → false",            object_list_verify_cull(st_list, QU_Type) == false);
}

// ---------------------------------------------------------------------------
// object_name_get_type — additional coverage  [T42–T43]
//
// The lookup is case-insensitive.  Names in "NAME:key" form are handled by
// stripping everything from the first colon onwards before comparison.
// ---------------------------------------------------------------------------

static void test_object_name_get_type_extended() {
   printf("\n--- object_name_get_type (extended) ---\n");

   // T42: lookup is case-insensitive
   CHECK(42, "\"job\" (lowercase) → SGE_TYPE_JOB",   object_name_get_type("job") == SGE_TYPE_JOB);

   // T43: "NAME:key" form — colon suffix is stripped before comparison
   CHECK(43, "\"JOB:42\" → SGE_TYPE_JOB",            object_name_get_type("JOB:42") == SGE_TYPE_JOB);
}

// ---------------------------------------------------------------------------
// object_has_differences — field-by-field element comparison  [T44–T47]
//
// Returns true when any attribute value differs between the two elements.
// If one element is nullptr and the other is not, the result is true.
// When both elements are nullptr the result is false (no difference).
// ---------------------------------------------------------------------------

static void test_object_has_differences() {
   printf("\n--- object_has_differences ---\n");

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto a = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   auto b = ocs::tom::ElementPtr<ocs::tom::UE>::create();

   // T44: two freshly allocated elements — all fields at their default values → no difference
   CHECK(44, "identical elements → false",            object_has_differences(*a, &alp, *b) == false);

   // T45: set a string field to different values → difference detected
   ocs::tom::set<UE_name>(a, "alice");
   ocs::tom::set<UE_name>(b, "bob");
   CHECK(45, "differing string field → true",         object_has_differences(*a, &alp, *b) == true);

   // T46: one nullptr, one valid element → true (one side has data, the other does not)
   CHECK(46, "nullptr vs valid → true",               object_has_differences(nullptr, &alp, *a) == true);

   // T47: both nullptr → false (neither side has data; no difference)
   CHECK(47, "nullptr vs nullptr → false",            object_has_differences(nullptr, &alp, nullptr) == false);

   a.reset();
   b.reset();
   alp.reset();
}

// ---------------------------------------------------------------------------
// object_list_has_differences — list-level difference detection  [T48–T50]
//
// Compares elements in order.  Lists of different lengths differ immediately
// without element comparison.  Both nullptr is treated as no difference.
// ---------------------------------------------------------------------------

static void test_object_list_has_differences() {
   printf("\n--- object_list_has_differences ---\n");

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   lList *l1 = nullptr;
   lList *l2 = nullptr;
   ocs::tom::emplace_back<UE_name>(&l1, "alice");
   ocs::tom::emplace_back<UE_name>(&l2, "alice");

   // T48: lists with identical elements → false
   CHECK(48, "identical lists → false",               object_list_has_differences(l1, &alp, l2) == false);

   // T49: lists of different lengths → true (length check short-circuits element comparison)
   ocs::tom::emplace_back<UE_name>(&l2, "bob");
   CHECK(49, "different lengths → true",              object_list_has_differences(l1, &alp, l2) == true);

   // T50: both nullptr → false
   CHECK(50, "both nullptr → false",                  object_list_has_differences(nullptr, &alp, nullptr) == false);

   lFreeList(&l1);
   lFreeList(&l2);
   alp.reset();
}

// ---------------------------------------------------------------------------
// object_verify_string_not_null  [T51–T52]
// object_verify_ulong_not_null   [T53–T54]
// object_verify_ulong_null       [T55–T56]
// object_verify_double_null      [T57–T58]
//
// These helpers enforce field invariants during GDI validation.  Each fills
// an answer_list on failure; the list is freed after each check so failures
// do not accumulate.
// ---------------------------------------------------------------------------

static void test_object_verify_field_constraints() {
   printf("\n--- object_verify_{string,ulong,double}_null/not_null ---\n");

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::JB>::create();

   // T51: string field has a value → passes
   ocs::tom::set<JB_job_name>(ep, "test_job");
   CHECK(51, "string field set → string_not_null passes",   object_verify_string_not_null<JB_job_name>(*ep, &alp) == true);
   alp.reset();

   // T52: string field is nullptr → fails and populates answer_list
   ocs::tom::set<JB_job_name>(ep, nullptr);
   CHECK(52, "string field nullptr → string_not_null fails", object_verify_string_not_null<JB_job_name>(*ep, &alp) == false);
   alp.reset();

   // T53: ulong field is non-zero → passes
   ocs::tom::set<JB_job_number>(ep, 1);
   CHECK(53, "ulong field 1 → ulong_not_null passes",       object_verify_ulong_not_null<JB_job_number>(*ep, &alp) == true);
   alp.reset();

   // T54: ulong field is 0 (default) → fails
   ocs::tom::set<JB_job_number>(ep, 0);
   CHECK(54, "ulong field 0 → ulong_not_null fails",        object_verify_ulong_not_null<JB_job_number>(*ep, &alp) == false);
   alp.reset();

   // T55: ulong field is 0 → ulong_null passes
   CHECK(55, "ulong field 0 → ulong_null passes",           object_verify_ulong_null<JB_job_number>(*ep, &alp) == true);
   alp.reset();

   // T56: ulong field non-zero → ulong_null fails
   ocs::tom::set<JB_job_number>(ep, 42);
   CHECK(56, "ulong field 42 → ulong_null fails",           object_verify_ulong_null<JB_job_number>(*ep, &alp) == false);
   alp.reset();

   // T57: double field is 0.0 (default) → double_null passes
   CHECK(57, "double field 0.0 → double_null passes",       object_verify_double_null<JB_urg>(*ep, &alp) == true);
   alp.reset();

   // T58: double field non-zero → double_null fails
   ocs::tom::set<JB_urg>(ep, 0.5);
   CHECK(58, "double field 0.5 → double_null fails",        object_verify_double_null<JB_urg>(*ep, &alp) == false);
   alp.reset();

}

// ---------------------------------------------------------------------------
// object_verify_name — GDI name validity check  [T59–T60]
//
// Rejects names that start with a digit; characters not in QSUB_TABLE are
// also rejected.  Returns 0 on success, STATUS_EUNKNOWN on failure.
// ---------------------------------------------------------------------------

static void test_object_verify_name() {
   printf("\n--- object_verify_name ---\n");

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::QU>::create();

   // T59: alphanumeric name with embedded punctuation is accepted
   ocs::tom::set<QU_qname>(ep, "all.q");
   CHECK(59, "valid name → 0",                         object_verify_name(*ep, &alp, QU_qname) == 0);
   alp.reset();

   // T60: name starting with a digit is always rejected regardless of other characters
   ocs::tom::set<QU_qname>(ep, "1bad");
   CHECK(60, "leading digit → STATUS_EUNKNOWN",        object_verify_name(*ep, &alp, QU_qname) == STATUS_EUNKNOWN);
   alp.reset();

}

// ---------------------------------------------------------------------------
// spooled job of a shorter JB_Type — types grown at the end  [T61–T62]
//
// A job spooled by a release whose JB_Type ended four fields earlier is read
// into a full job; the added fields hold their initial values (N-W-6). The
// old special case for jobs in the unpack code is gone, the rule is generic.
// ---------------------------------------------------------------------------

/**
 * @brief Pack a job as a release with a JB_Type shorter by some fields would have spooled it
 *
 * The job is packed with a selection of the leading fields; clearing the reduced
 * mark of the packed descriptor turns it into the full type of the older release.
 *
 * @param pb the buffer to pack into
 * @param job the job
 * @param dropped how many fields at the end the older JB_Type lacks
 * @return true on success
 */
static bool pack_job_as_shorter(sge_pack_buffer *pb, const lListElem *job, const int dropped) {
   const int n = lCountDescr(JB_Type) - dropped;
   std::vector<int> nms;
   for (int i = 0; i < n; i++) {
      nms.push_back(JB_Type[i].nm);
   }
   nms.push_back(NoName);
   lEnumeration *what = lIntVector2What(JB_Type, nms.data());
   const size_t start = pb->bytes_used;
   const bool ok = cull_pack_elem_partial(pb, job, what, CULL_SPOOL) == PACK_SUCCESS;
   lFreeWhat(&what);
   // element: kind, field count, then (nm, mt) per field
   for (int i = 0; ok && i < n; i++) {
      char *mt_word = pb->head_ptr + start + 8 + 8 * i + 4;
      uint32_t mt;
      memcpy(&mt, mt_word, sizeof(mt));
      mt = htonl(ntohl(mt) & ~CULL_IS_REDUCED);
      memcpy(mt_word, &mt, sizeof(mt));
   }
   return ok;
}

/** @brief A job spooled with a JB_Type shorter by four fields is read into a full job [T61–T62] */
static void test_spooled_job_of_shorter_type() {
   printf("\n--- spooled job of a shorter JB_Type ---\n");

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   ocs::tom::set<JB_job_number>(job, 42);
   ocs::tom::set<JB_owner>(job, "owner");
   ocs::tom::set<JB_sync_options>(job, 5);

   lListElem *read = nullptr;
   int ret = PACK_FORMAT;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (pack_job_as_shorter(&pb, *job, 4)) {
         char *buf = sge_malloc(pb.bytes_used);
         memcpy(buf, pb.head_ptr, pb.bytes_used);
         sge_pack_buffer in;
         if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
            ret = cull_unpack_elem_partial(&in, &read, JB_Type, CULL_SPOOL);
         }
         clear_packbuffer(&in);
      }
      clear_packbuffer(&pb);
   }

   // T61: the job is read into the full JB_Type with its values
   CHECK(61, "shorter spooled job → full JB_Type",
         ret == PACK_SUCCESS && read != nullptr && lGetElemDescr(read) == JB_Type &&
         ocs::tom::get<JB_job_number>(read) == 42 && strcmp(ocs::tom::get<JB_owner>(read), "owner") == 0);

   // T62: a spooled field the older type lacked holds its initial value
   CHECK(62, "added spooled field → initial value", read != nullptr && ocs::tom::get<JB_sync_options>(read) == 0);

   lFreeElem(&read);
}

/**
 * @brief Send an element over the wire and read it back
 *
 * @param ep the element to pack
 * @param back receives the unpacked element, nullptr on error
 * @return the result of unpacking, or #PACK_FORMAT if packing failed
 */
static int pack_roundtrip(const lListElem *ep, lListElem **back) {
   int ret = PACK_FORMAT;
   *back = nullptr;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (cull_pack_elem(&pb, ep) == PACK_SUCCESS) {
         char *buf = sge_malloc(pb.bytes_used);
         memcpy(buf, pb.head_ptr, pb.bytes_used);
         sge_pack_buffer in;
         if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
            ret = cull_unpack_elem(&in, back, nullptr);
         }
         clear_packbuffer(&in);
      }
      clear_packbuffer(&pb);
   }
   return ret;
}

/** @brief A what and a where packed into PACK_Type elements survive the wire, as event clients send them [T63–T66] */
static void test_pack_type_roundtrip() {
   printf("\n--- PACK_Type over the wire ---\n");

   // T63: PACK_Type is registered in the name space
   CHECK(63, "PACK_id is known to the name space", strcmp(lNm2Str(PACK_id), "PACK_id") == 0);

   lEnumeration *what = lWhat("%T(%I %I)", QU_Type, QU_qname, QU_qhostname);
   auto what_elem = ocs_what_to_elem(what);
   lListElem *what_back = nullptr;
   const int what_ret = pack_roundtrip(*what_elem, &what_back);

   // T64: the element comes back with the static PACK_Type descriptor
   CHECK(64, "packed what → PACK_Type element",
         what_ret == PACK_SUCCESS && what_back != nullptr && lGetElemDescr(what_back) == PACK_Type);

   // T65: the what read from it selects the same fields
   lEnumeration *what_read =
      what_back != nullptr ? ocs_what_from_elem(ocs::tom::ConstElement<ocs::tom::PACK>(what_back)) : nullptr;
   CHECK(65, "what from the element → same fields",
         what_read != nullptr && lCountWhat(what_read, QU_Type) == 2 && lCountWhat(what, QU_Type) == 2);

   lCondition *where = lWhere("%T(%I==%s)", QU_Type, QU_qname, "all.q");
   auto where_elem = ocs_where_to_elem(where);
   lListElem *where_back = nullptr;
   const int where_ret = pack_roundtrip(*where_elem, &where_back);
   lCondition *where_read = where_ret == PACK_SUCCESS && where_back != nullptr
                               ? ocs_where_from_elem(ocs::tom::ConstElement<ocs::tom::PACK>(where_back))
                               : nullptr;

   // T66: the where read from it matches as the original does
   auto match = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   ocs::tom::set<QU_qname>(match, "all.q");
   auto other = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   ocs::tom::set<QU_qname>(other, "other.q");
   CHECK(66, "where from the element → same matches",
         where_read != nullptr && lCompare(*match, where_read) == 1 && lCompare(*other, where_read) == 0);

   other.reset();
   match.reset();
   lFreeWhere(&where_read);
   lFreeElem(&where_back);
   where_elem.reset();
   lFreeWhere(&where);
   lFreeWhat(&what_read);
   lFreeElem(&what_back);
   what_elem.reset();
   lFreeWhat(&what);
}

// ---------------------------------------------------------------------------
// object_parse_field_from_string — complex relational operator  [T75–T83]
//
// CS-3169: the relop is parsed from its text by walking the operators
// CMPLXEQ_OP .. CMPLXEXCL_OP. The out-of-range text "??" of map_op2str() and
// an unknown text must be rejected and must leave CE_relop unchanged.
// ---------------------------------------------------------------------------

/**
 * @brief Check that object_parse_field_from_string() accepts exactly the valid complex relops
 */
static void test_object_parse_centry_relop() {
   printf("\n--- object_parse_field_from_string (CE_relop) ---\n");
   auto centry = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   char label[128];
   int id = 75;

   // T75–T81: every operator is parsed from its text and stored
   for (uint32_t op = CMPLXEQ_OP; op <= CMPLXEXCL_OP; op++) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      const char *text = map_op2str(op);

      ocs::tom::set<CE_relop>(centry, 0);
      const bool parsed = object_parse_field_from_string(*centry, &alp, CE_relop, text);
      snprintf(label, sizeof(label), "relop \"%s\" is accepted and stored as %u", text, op);
      CHECK(id, label, parsed && ocs::tom::get<CE_relop>(centry) == op);
      id++;
   }

   // T82–T83: "??" (map_op2str() of an out-of-range value) and an unknown text are rejected
   for (const char *text : {"??", "="}) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;

      ocs::tom::set<CE_relop>(centry, CMPLXLE_OP);
      const bool parsed = object_parse_field_from_string(*centry, &alp, CE_relop, text);
      snprintf(label, sizeof(label), "invalid relop \"%s\" is rejected, CE_relop unchanged", text);
      CHECK(id, label, !parsed && ocs::tom::get<CE_relop>(centry) == CMPLXLE_OP);
      id++;
   }
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_object");

   // suppress sge_log stderr output emitted by functions under test
   component_set_daemonized(true);

   lInit(nmv);

   auto queue = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   lList *st_list = nullptr;
   ocs::tom::emplace_back<ST_name>(&st_list, "test pe");

   test_object_has_type(*queue);
   test_object_get_type(*queue);
   test_object_get_subtype();
   test_object_get_primary_key();
   test_object_get_name();
   test_object_get_name_prefix();
   test_object_type_get_name();
   test_object_name_get_type();
   test_object_type_get_descr_and_key();
   test_object_type_dispatch();
   test_object_verify_cull(*queue, st_list);
   test_object_list_verify_cull(st_list);
   test_object_name_get_type_extended();
   test_object_has_differences();
   test_object_list_has_differences();
   test_object_verify_field_constraints();
   test_object_verify_name();
   test_spooled_job_of_shorter_type();
   test_pack_type_roundtrip();
   test_object_parse_centry_relop();

   queue.reset();
   lFreeList(&st_list);

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
