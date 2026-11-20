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
#include <string>
#include <type_traits>
#include <vector>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_test_api.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_object_type.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pack.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_PRO.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Raw.h"

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
// object_has_differences — field-by-field element comparison  [T44–T47, T100–T105]
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

   // T100/T101: a Uint64 field (CS-3189: it fell into the default branch and always counted as different)
   auto p = ocs::tom::ElementPtr<ocs::tom::PRO>::create();
   auto q = ocs::tom::ElementPtr<ocs::tom::PRO>::create();
   ocs::tom::set<PRO_vsize>(p, 4294967296ULL);
   ocs::tom::set<PRO_vsize>(q, 4294967296ULL);
   CHECK(100, "equal Uint64 field → false",            object_has_differences(*p, &alp, *q) == false);
   ocs::tom::set<PRO_vsize>(q, 4294967297ULL);
   CHECK(101, "differing Uint64 field → true",         object_has_differences(*p, &alp, *q) == true);

   // T102: host names compare as sge_hostcmp() does, so a host in another case is the same host
   auto h1 = ocs::tom::ElementPtr<ocs::tom::EH>::create();
   auto h2 = ocs::tom::ElementPtr<ocs::tom::EH>::create();
   ocs::tom::set<EH_name>(h1, ocs::tom::Host{"HostA"});
   ocs::tom::set<EH_name>(h2, ocs::tom::Host{"hosta"});
   CHECK(102, "host field in another case → false",   object_has_differences(*h1, &alp, *h2) == false);

   // T103/T104: a reduced element differs from the full one in both directions, two equal reductions do not
   lEnumeration *what = lWhat("%T(%I)", PRO_Type, PRO_vsize);
   auto reduced = ocs::tom::select(p.get().raw(), what);
   auto reduced_too = ocs::tom::select(p.get().raw(), what);
   lFreeWhat(&what);
   CHECK(103, "reduced vs full and full vs reduced → true",
         object_has_differences(reduced.get().raw(), &alp, *p) &&
            object_has_differences(*p, &alp, reduced.get().raw()));
   CHECK(104, "two equal reductions → false",
         object_has_differences(reduced.get().raw(), &alp, reduced_too.get().raw()) == false);

   // T105: a list field compares element by element
   auto j1 = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   auto j2 = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   ocs::tom::emplace_back_in<JB_env_list, VA_variable>(j1, "PATH");
   ocs::tom::emplace_back_in<JB_env_list, VA_variable>(j2, "PATH");
   const bool same_lists = !object_has_differences(*j1, &alp, *j2);
   ocs::tom::emplace_back_in<JB_env_list, VA_variable>(j1, "HOME");
   ocs::tom::emplace_back_in<JB_env_list, VA_variable>(j2, "LANG");
   CHECK(105, "equal list field → false, a differing sub-element → true",
         same_lists && object_has_differences(*j1, &alp, *j2));

   a.reset();
   b.reset();
   alp.reset();
}

// ---------------------------------------------------------------------------
// object_list_has_differences — list-level difference detection  [T48–T50, T106]
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

   // T106: an empty list and nullptr hold the same elements, none
   auto empty = ocs::tom::ListPtr<ocs::tom::UE>::create("empty");
   CHECK(106, "empty list vs nullptr → false",
         object_list_has_differences(empty.get().raw(), &alp, nullptr) == false &&
            object_list_has_differences(nullptr, &alp, empty.get().raw()) == false);

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
   CHECK(59, "valid name → 0",                         object_verify_name<QU_qname>(*ep, &alp) == 0);
   alp.reset();

   // T60: name starting with a digit is always rejected regardless of other characters
   ocs::tom::set<QU_qname>(ep, "1bad");
   CHECK(60, "leading digit → STATUS_EUNKNOWN",        object_verify_name<QU_qname>(*ep, &alp) == STATUS_EUNKNOWN);
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

   // T63: PACK_Type is known to the installed object model
   CHECK(63, "PACK_id is known to the object model", strcmp(ocs::tom::field_name(PACK_id), "PACK_id") == 0);

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
// ocs_object_parse_symbolic — complex relational operator  [T91–T99]
//
// CS-3169: the relop is parsed from its text by walking the operators
// CMPLXEQ_OP .. CMPLXEXCL_OP. The out-of-range text "??" of map_op2str() and
// an unknown text must be rejected and must not be stored.
// ---------------------------------------------------------------------------

/**
 * @brief Check that ocs_object_parse_symbolic() accepts exactly the valid complex relops
 */
static void test_object_parse_centry_relop() {
   printf("\n--- ocs_object_parse_symbolic (CE_relop) ---\n");
   char label[128];
   int id = 91;

   // T91–T97: every operator is parsed from its text, to be stored with its value
   for (uint32_t op = CMPLXEQ_OP; op <= CMPLXEXCL_OP; op++) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      const char *text = map_op2str(op);

      const auto parsed = ocs_object_parse_symbolic(&alp, ocs_object_symbolic::ce_relop, text);
      snprintf(label, sizeof(label), "relop \"%s\" is accepted and stored as %u", text, op);
      CHECK(id, label, parsed.ok && parsed.store && parsed.value == op);
      id++;
   }

   // T98–T99: "??" (map_op2str() of an out-of-range value) and an unknown text are rejected, nothing is stored
   for (const char *text : {"??", "="}) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;

      const auto parsed = ocs_object_parse_symbolic(&alp, ocs_object_symbolic::ce_relop, text);
      snprintf(label, sizeof(label), "invalid relop \"%s\" is rejected, nothing is stored", text);
      CHECK(id, label, !parsed.ok && !parsed.store);
      id++;
   }
}

// ---------------------------------------------------------------------------
// ocs_object_append_field / ocs_object_parse_field — attribute lists  [T75–T90]
// ---------------------------------------------------------------------------
// The generic round trip over the spooling records (test_spool_utilities) writes and reads a default element, so
// it never sees the cluster queue attribute lists, whose text is parsed into a sub-list and written back from it.

/**
 * @brief Parse a text into the value field of a cluster queue attribute and write it back, twice
 *
 * The text written for `input` must be `expected`, and `expected` parsed again must be written as itself, so the
 * written form is stable across a spool write and read.
 *
 * @tparam F the value field of the attribute type, e.g. ASTRLIST_value
 * @param input the text an administrator writes
 * @param expected the text the field is written as after parsing `input`
 * @return true if both texts parse and both are written as `expected`
 */
template <auto F>
static bool ocs_attr_value_round_trip(const char *input, const char *expected) {
   using T = typename ocs::tom::FieldTraits<F>::Type;

   bool ok = true;
   const char *text = input;
   for (int pass = 0; pass < 2 && ok; pass++) {
      // a copy: the parser of a complex entry list splits its argument in place
      std::string buffer{text};
      auto attr = ocs::tom::ElementPtr<T>::create();
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      dstring written = DSTRING_INIT;

      ok = ocs_object_parse_field<F>(attr.get(), &alp, buffer.data());
      const char *result = ok ? ocs_object_append_field<F>(attr.get(), &written) : nullptr;
      ok = result != nullptr && strcmp(result, expected) == 0;
      if (!ok) {
         printf("   %s: \"%s\" written as \"%s\", expected \"%s\"\n", ocs::tom::field_name(static_cast<int>(F)), text,
                result != nullptr ? result : "<parse error>", expected);
      }

      sge_dstring_free(&written);
      text = expected;
   }
   return ok;
}

/** @brief The attribute lists survive the typed parse and write: NONE, one entry, several entries [T75–T90] */
static void test_attr_value_round_trip() {
   printf("\n--- attribute lists: parse and write ---\n");

   // string lists (e.g. pe_list): names separated by blanks or commas, written separated by blanks
   CHECK(75, "ASTRLIST NONE", ocs_attr_value_round_trip<ASTRLIST_value>("NONE", "NONE"));
   CHECK(76, "ASTRLIST one entry", ocs_attr_value_round_trip<ASTRLIST_value>("make", "make"));
   CHECK(77, "ASTRLIST several entries", ocs_attr_value_round_trip<ASTRLIST_value>("make,mpi smp", "make mpi smp"));

   // user set lists (user_lists, xuser_lists)
   CHECK(78, "AUSRLIST NONE", ocs_attr_value_round_trip<AUSRLIST_value>("NONE", "NONE"));
   CHECK(79, "AUSRLIST one entry", ocs_attr_value_round_trip<AUSRLIST_value>("arusers", "arusers"));
   CHECK(80, "AUSRLIST several entries",
         ocs_attr_value_round_trip<AUSRLIST_value>("arusers,deadlineusers staff", "arusers deadlineusers staff"));

   // project lists (projects, xprojects)
   CHECK(81, "APRJLIST NONE", ocs_attr_value_round_trip<APRJLIST_value>("NONE", "NONE"));
   CHECK(82, "APRJLIST one entry", ocs_attr_value_round_trip<APRJLIST_value>("prj1", "prj1"));
   CHECK(83, "APRJLIST several entries", ocs_attr_value_round_trip<APRJLIST_value>("prj1, prj2", "prj1 prj2"));

   // complex entry lists (load_thresholds, complex_values): name=value pairs, written separated by commas
   CHECK(84, "ACELIST NONE", ocs_attr_value_round_trip<ACELIST_value>("NONE", "NONE"));
   CHECK(85, "ACELIST one entry", ocs_attr_value_round_trip<ACELIST_value>("np_load_avg=1.75", "np_load_avg=1.75"));
   CHECK(86, "ACELIST several entries",
         ocs_attr_value_round_trip<ACELIST_value>("h_vmem=1G, arch=lx-amd64", "h_vmem=1G,arch=lx-amd64"));

   // subordinate lists: queue wise with thresholds, and slot wise
   CHECK(87, "ASOLIST NONE", ocs_attr_value_round_trip<ASOLIST_value>("NONE", "NONE"));
   CHECK(88, "ASOLIST one entry", ocs_attr_value_round_trip<ASOLIST_value>("A.q=3", "A.q=3"));
   CHECK(89, "ASOLIST several entries, queue wise",
         ocs_attr_value_round_trip<ASOLIST_value>("A.q=3,B.q=4", "A.q=3, B.q=4"));
   CHECK(90, "ASOLIST several entries, slot wise",
         ocs_attr_value_round_trip<ASOLIST_value>("slots=4(A.q:1:lr, B.q:2)", "slots=4(A.q:1:lr, B.q:2:sr)"));
}

// ---------------------------------------------------------------------------
// object_get_type_name, object_unpack_elem_verify, ocs_object_find_repeated_keys, ocs_object_list_add_unique
// on raw handles  [T107–T118]
// ---------------------------------------------------------------------------

/**
 * @brief object_get_type_name() names an object by the primary key its descriptor holds [T107–T109]
 * @param queue a queue instance
 */
static void test_object_get_type_name(const ocs::tom::ConstElement<ocs::tom::QU> queue) {
   printf("\n--- object_get_type_name ---\n");

   CHECK(107, "queue instance → \"QINSTANCE\"", strcmp(object_get_type_name(queue), "QINSTANCE") == 0);
   CHECK(108, "nullptr → \"unknown\"", strcmp(object_get_type_name(nullptr), "unknown") == 0);

   auto st = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   CHECK(109, "an element of no registered object type → \"unknown\"",
         strcmp(object_get_type_name(*st), "unknown") == 0);
}

/**
 * @brief Pack an element and read it back through object_unpack_elem_verify()
 *
 * @param ep the element to pack
 * @param descr the type the unpacked element is checked against
 * @param answer_list receives the errors of object_unpack_elem_verify()
 * @param back receives the unpacked element, nullptr if it was rejected
 * @return the result of object_unpack_elem_verify(), false if packing failed
 */
static bool unpack_verify_roundtrip(const lListElem *ep, const lDescr *descr,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lListElem **back) {
   bool ret = false;
   *back = nullptr;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (cull_pack_elem(&pb, ep) == PACK_SUCCESS) {
         char *buf = sge_malloc(pb.bytes_used);
         memcpy(buf, pb.head_ptr, pb.bytes_used);
         sge_pack_buffer in;
         // the buffer takes buf over on every path, also when the header is refused
         if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
            ret = object_unpack_elem_verify(answer_list, &in, back, descr);
         }
         clear_packbuffer(&in);
      }
      clear_packbuffer(&pb);
   }
   return ret;
}

/**
 * @brief object_unpack_elem_verify() hands out a verified element and frees a rejected one [T110–T112]
 * @param queue a queue instance whose sub-lists are of their model types
 */
static void test_object_unpack_elem_verify(const ocs::tom::ConstElement<ocs::tom::QU> queue) {
   printf("\n--- object_unpack_elem_verify ---\n");

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   lListElem *back = nullptr;
   const bool accepted = unpack_verify_roundtrip(queue.raw(), QU_Type, &alp, &back);
   CHECK(110, "queue checked as QU_Type → true, the element is handed out",
         accepted && back != nullptr && lGetElemDescr(back) == QU_Type && !alp);
   lFreeElem(&back);

   const bool rejected = !unpack_verify_roundtrip(queue.raw(), JB_Type, &alp, &back);
   CHECK(111, "queue checked as JB_Type → false, the element is freed, one error",
         rejected && back == nullptr && alp && alp->size() == 1);
   alp.reset();

   lListElem *none = nullptr;
   CHECK(112, "no pack buffer → false, one error",
         !object_unpack_elem_verify(&alp, nullptr, &none, QU_Type) && none == nullptr && alp && alp->size() == 1);
}

/**
 * @brief ocs_object_find_repeated_keys() reports each repetition with its list, attribute, key and path
 *        [T113–T115]
 */
static void test_object_find_repeated_keys() {
   printf("\n--- ocs_object_find_repeated_keys ---\n");

   // a list with a repeated key on purpose; a debug build refuses it already at the append (N-H-18)
   const ocs::tom::UniqueCheckSuspension suspension;
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   lList *env = nullptr;
   ocs::tom::emplace_back<VA_variable>(&env, "A");
   ocs::tom::emplace_back<VA_variable>(&env, "B");
   ocs::tom::set<JB_env_list>(job, env);

   std::vector<ocs::RepeatedKey> found;
   ocs_object_find_repeated_keys(*job, found);
   CHECK(113, "distinct variables → nothing found", found.empty());

   lList *job_env = ocs::tom::get<JB_env_list>(job);
   ocs::tom::emplace_back<VA_variable>(&job_env, "A");
   ocs::tom::emplace_back<VA_variable>(&job_env, "A");
   ocs_object_find_repeated_keys(*job, found);
   CHECK(114, "\"A\" three times → two entries naming the list, VA_variable and the path JB_env_list",
         found.size() == 2 && found[0].list.raw() == job_env && found[0].nm == VA_variable && found[0].key == "A" &&
         found[0].path == "JB_env_list" && found[1].key == "A");

   found.clear();
   ocs_object_find_repeated_keys(nullptr, found);
   CHECK(115, "nullptr → nothing found", found.empty());
}

/**
 * @brief ocs_object_list_add_unique() on lists without a key: appended, or dropped for another layout, as
 *        lAddList() did [T116–T118]
 */
static void test_object_list_add_unique_layouts() {
   printf("\n--- ocs_object_list_add_unique, lists without a key ---\n");

   // ST has no unique string attribute, so its lists are appended as they are
   lList *target = nullptr;
   ocs::tom::emplace_back<ST_name>(&target, "a");
   lList *more = nullptr;
   ocs::tom::emplace_back<ST_name>(&more, "a");
   ocs_object_list_add_unique(&target, &more);
   CHECK(116, "same layout → appended, the source is consumed",
         more == nullptr && ocs::tom::size(target) == 2);

   lList *created = nullptr;
   lList *source = nullptr;
   ocs::tom::emplace_back<ST_name>(&source, "b");
   ocs_object_list_add_unique(&created, &source);
   CHECK(117, "no target → created with the source's elements", source == nullptr && ocs::tom::size(created) == 1);
   lFreeList(&created);

   // a reduced list of the same type has another layout: as lAddList() it is not joined, but freed
   lEnumeration *what = lWhat("%T(%I)", ST_Type, ST_name);
   ocs::tom::RawListPtr reduced = ocs::tom::select(target, nullptr, what);
   lList *reduced_raw = reduced.release();
   ocs_object_list_add_unique(&target, &reduced_raw);
   CHECK(118, "another layout → nothing appended, the source is consumed",
         reduced_raw == nullptr && ocs::tom::size(target) == 2);
   lFreeWhat(&what);
   lFreeList(&target);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_object");

   // suppress sge_log stderr output emitted by functions under test
   component_set_daemonized(true);

   ocs::tom::install_model(ocs::tom::gcs_model());

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
   test_attr_value_round_trip();
   test_object_get_type_name(*queue);
   // a fresh queue: the fixture above holds a deliberately wrong sub-list since T37
   const auto fresh_queue = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   test_object_unpack_elem_verify(*fresh_queue);
   test_object_find_repeated_keys();
   test_object_list_add_unique_layouts();

   queue.reset();
   lFreeList(&st_list);

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
