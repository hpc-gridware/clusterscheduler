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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"

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
   lSetList(queue, QU_pe_list, lCopyList("pe_list", st_list));
   CHECK(36, "QU_pe_list with ST_Type elements → true",  object_verify_cull(queue, QU_Type) == true);

   // T37: sub-list with wrong element type — QU_owner_list expects US_Type, not ST_Type → false
   lSetList(queue, QU_owner_list, lCopyList("owner_list", st_list));
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

   lList *alp = nullptr;
   lListElem *a = lCreateElem(UM_Type);
   lListElem *b = lCreateElem(UM_Type);

   // T44: two freshly allocated elements — all fields at their default values → no difference
   CHECK(44, "identical elements → false",            object_has_differences(a, &alp, b) == false);

   // T45: set a string field to different values → difference detected
   lSetString(a, UM_name, "alice");
   lSetString(b, UM_name, "bob");
   CHECK(45, "differing string field → true",         object_has_differences(a, &alp, b) == true);

   // T46: one nullptr, one valid element → true (one side has data, the other does not)
   CHECK(46, "nullptr vs valid → true",               object_has_differences(nullptr, &alp, a) == true);

   // T47: both nullptr → false (neither side has data; no difference)
   CHECK(47, "nullptr vs nullptr → false",            object_has_differences(nullptr, &alp, nullptr) == false);

   lFreeElem(&a);
   lFreeElem(&b);
   lFreeList(&alp);
}

// ---------------------------------------------------------------------------
// object_list_has_differences — list-level difference detection  [T48–T50]
//
// Compares elements in order.  Lists of different lengths differ immediately
// without element comparison.  Both nullptr is treated as no difference.
// ---------------------------------------------------------------------------

static void test_object_list_has_differences() {
   printf("\n--- object_list_has_differences ---\n");

   lList *alp = nullptr;
   lList *l1 = nullptr;
   lList *l2 = nullptr;
   lAddElemStr(&l1, UM_name, "alice", UM_Type);
   lAddElemStr(&l2, UM_name, "alice", UM_Type);

   // T48: lists with identical elements → false
   CHECK(48, "identical lists → false",               object_list_has_differences(l1, &alp, l2) == false);

   // T49: lists of different lengths → true (length check short-circuits element comparison)
   lAddElemStr(&l2, UM_name, "bob", UM_Type);
   CHECK(49, "different lengths → true",              object_list_has_differences(l1, &alp, l2) == true);

   // T50: both nullptr → false
   CHECK(50, "both nullptr → false",                  object_list_has_differences(nullptr, &alp, nullptr) == false);

   lFreeList(&l1);
   lFreeList(&l2);
   lFreeList(&alp);
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

   lList *alp = nullptr;
   lListElem *ep = lCreateElem(JB_Type);

   // T51: string field has a value → passes
   lSetString(ep, JB_job_name, "test_job");
   CHECK(51, "string field set → string_not_null passes",   object_verify_string_not_null(ep, &alp, JB_job_name) == true);
   lFreeList(&alp);

   // T52: string field is nullptr → fails and populates answer_list
   lSetString(ep, JB_job_name, nullptr);
   CHECK(52, "string field nullptr → string_not_null fails", object_verify_string_not_null(ep, &alp, JB_job_name) == false);
   lFreeList(&alp);

   // T53: ulong field is non-zero → passes
   lSetUlong(ep, JB_job_number, 1);
   CHECK(53, "ulong field 1 → ulong_not_null passes",       object_verify_ulong_not_null(ep, &alp, JB_job_number) == true);
   lFreeList(&alp);

   // T54: ulong field is 0 (default) → fails
   lSetUlong(ep, JB_job_number, 0);
   CHECK(54, "ulong field 0 → ulong_not_null fails",        object_verify_ulong_not_null(ep, &alp, JB_job_number) == false);
   lFreeList(&alp);

   // T55: ulong field is 0 → ulong_null passes
   CHECK(55, "ulong field 0 → ulong_null passes",           object_verify_ulong_null(ep, &alp, JB_job_number) == true);
   lFreeList(&alp);

   // T56: ulong field non-zero → ulong_null fails
   lSetUlong(ep, JB_job_number, 42);
   CHECK(56, "ulong field 42 → ulong_null fails",           object_verify_ulong_null(ep, &alp, JB_job_number) == false);
   lFreeList(&alp);

   // T57: double field is 0.0 (default) → double_null passes
   CHECK(57, "double field 0.0 → double_null passes",       object_verify_double_null(ep, &alp, JB_urg) == true);
   lFreeList(&alp);

   // T58: double field non-zero → double_null fails
   lSetDouble(ep, JB_urg, 0.5);
   CHECK(58, "double field 0.5 → double_null fails",        object_verify_double_null(ep, &alp, JB_urg) == false);
   lFreeList(&alp);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// object_verify_name — GDI name validity check  [T59–T60]
//
// Rejects names that start with a digit; characters not in QSUB_TABLE are
// also rejected.  Returns 0 on success, STATUS_EUNKNOWN on failure.
// ---------------------------------------------------------------------------

static void test_object_verify_name() {
   printf("\n--- object_verify_name ---\n");

   lList *alp = nullptr;
   lListElem *ep = lCreateElem(QU_Type);

   // T59: alphanumeric name with embedded punctuation is accepted
   lSetString(ep, QU_qname, "all.q");
   CHECK(59, "valid name → 0",                         object_verify_name(ep, &alp, QU_qname) == 0);
   lFreeList(&alp);

   // T60: name starting with a digit is always rejected regardless of other characters
   lSetString(ep, QU_qname, "1bad");
   CHECK(60, "leading digit → STATUS_EUNKNOWN",        object_verify_name(ep, &alp, QU_qname) == STATUS_EUNKNOWN);
   lFreeList(&alp);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_object");

   // suppress sge_log stderr output emitted by functions under test
   component_set_daemonized(true);

   lInit(nmv);

   lListElem *queue = lCreateElem(QU_Type);
   lList *st_list = nullptr;
   lAddElemStr(&st_list, ST_name, "test pe", ST_Type);

   test_object_has_type(queue);
   test_object_get_type(queue);
   test_object_get_subtype();
   test_object_get_primary_key();
   test_object_get_name();
   test_object_get_name_prefix();
   test_object_type_get_name();
   test_object_name_get_type();
   test_object_type_get_descr_and_key();
   test_object_verify_cull(queue, st_list);
   test_object_list_verify_cull(st_list);
   test_object_name_get_type_extended();
   test_object_has_differences();
   test_object_list_has_differences();
   test_object_verify_field_constraints();
   test_object_verify_name();

   lFreeElem(&queue);
   lFreeList(&st_list);

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
