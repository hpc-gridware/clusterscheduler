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
 * @brief Unit tests for flatfile rsmap in `libs/spool`
 */

/* CS-1338: focused unit tests for read_CE_stringval_host — the flatfile
 * reader for RSMAP complex_values entries. Covers bare IDs (regression),
 * ranges, mixed range+ID input, the new per-instance characteristics
 * grammar, and the negative cases the reader must reject at syntax time
 * (validation is a separate layer; not exercised here). */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cull/cull_list.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_centry.h"
#include "spool/flatfile/sge_flatfile_obj_rsmap.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

namespace {

/* Small test harness — a compact TEST_ASSERT that logs the failing test's
 * name plus the failed expression, keeps counting rather than aborting, and
 * lets each test return non-zero if any assertion failed. */

static int g_failures = 0;
static const char *g_current = "?";

/** @brief Begin a named section, so a later #T_ASSERT names what it was checking
 * @param name what this section is about
 */
#define T_START(name) do { g_current = (name); printf("== %s ==\n", g_current); } while (0)
/** @brief Assert one condition within the current #T_START section
 * @param cond the condition that must hold
 */
#define T_ASSERT(cond) do {                                                    \
   if (!(cond)) {                                                              \
      fprintf(stderr, "[%s] assertion failed: %s (line %d)\n",                 \
              g_current, #cond, __LINE__);                                     \
      ++g_failures;                                                            \
      return 1;                                                                \
   }                                                                           \
} while (0)

/* Build a fresh CE_Type element ready for read_CE_stringval_host to fill in. */
static lListElem *
make_rsmap_ce(const char *name) {
   lListElem *ce = lCreateElem(CE_Type);
   lSetString(ce, CE_name, name);
   lSetUlong(ce, CE_valtype, static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   return ce;
}

/* read_CE_stringval_host expects a mutable buffer (sge_strtok_r modifies it),
 * so hand it a strdup and free after. */
static int
run_reader(lListElem *ce, const char *input, lList **alp) {
   char *buf = strdup(input);
   int rc = read_CE_stringval_host(ce, CE_stringval, buf, alp);
   free(buf);
   return rc;
}

/* Look up a property by name inside a RESL's properties list; returns nullptr
 * if absent. Only used by the GCS-only characteristics tests. */
#if defined(WITH_EXTENSIONS)
static const lListElem *
find_prop(const lListElem *resl, const char *name) {
   const lList *props = lGetList(resl, RESL_properties);
   if (props == nullptr) return nullptr;
   return lGetElemStr(props, CE_name, name);
}
#endif /* WITH_EXTENSIONS */

/* ------------------------------------------------------------------ */
/* positive: base grammar — must still parse exactly as before CS-1338  */
/* ------------------------------------------------------------------ */

static int
test_bare_ids() {
   T_START("bare_ids");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(A B)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr);
   T_ASSERT(lGetNumberOfElem(rm) == 2);

   const lListElem *r_a = lGetElemStr(rm, RESL_value, "A");
   const lListElem *r_b = lGetElemStr(rm, RESL_value, "B");
   T_ASSERT(r_a != nullptr && lGetUlong(r_a, RESL_amount) == 1);
   T_ASSERT(r_b != nullptr && lGetUlong(r_b, RESL_amount) == 1);
   T_ASSERT(lGetList(r_a, RESL_properties) == nullptr);
   T_ASSERT(lGetList(r_b, RESL_properties) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

static int
test_range_only() {
   T_START("range_only");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "3(1-3)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr);
   T_ASSERT(lGetNumberOfElem(rm) == 3);
   T_ASSERT(lGetElemStr(rm, RESL_value, "1") != nullptr);
   T_ASSERT(lGetElemStr(rm, RESL_value, "2") != nullptr);
   T_ASSERT(lGetElemStr(rm, RESL_value, "3") != nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

static int
test_mixed_range_and_names() {
   T_START("mixed_range_and_names");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "4(A B 1-2)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr);
   T_ASSERT(lGetNumberOfElem(rm) == 4);
   T_ASSERT(lGetElemStr(rm, RESL_value, "A") != nullptr);
   T_ASSERT(lGetElemStr(rm, RESL_value, "B") != nullptr);
   T_ASSERT(lGetElemStr(rm, RESL_value, "1") != nullptr);
   T_ASSERT(lGetElemStr(rm, RESL_value, "2") != nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* ------------------------------------------------------------------ */
/* per-instance characteristics grammar (GCS only)                      */
/*                                                                      */
/* Characteristics are a GCS-only feature: read_CE_stringval_host       */
/* rejects them outright in an OCS build, so the cases below only make  */
/* sense with extensions enabled. The OCS side is covered by            */
/* test_characteristics_rejected_in_ocs() at the end of this file.      */
/* ------------------------------------------------------------------ */
#if defined(WITH_EXTENSIONS)

static int
test_single_characteristic() {
   T_START("single_characteristic");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "1(gpu0[device=/dev/nvidia0])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr && lGetNumberOfElem(rm) == 1);

   const lListElem *r = lGetElemStr(rm, RESL_value, "gpu0");
   T_ASSERT(r != nullptr);
   const lListElem *dev = find_prop(r, "device");
   T_ASSERT(dev != nullptr);
   T_ASSERT(strcmp(lGetString(dev, CE_stringval), "/dev/nvidia0") == 0);
   /* reader stores only name+stringval verbatim; valtype must remain unset */
   T_ASSERT(lGetUlong(dev, CE_valtype) == 0);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

static int
test_multiple_characteristics() {
   T_START("multiple_characteristics");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce,
                       "1(gpu0[device=/dev/nvidia0,memory=80G,affinity_mask=SCCCCCCCCScccccccc])",
                       &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lListElem *r = lGetElemStr(lGetList(ce, CE_resource_map_list),
                                    RESL_value, "gpu0");
   T_ASSERT(r != nullptr);

   const lListElem *dev = find_prop(r, "device");
   const lListElem *mem = find_prop(r, "memory");
   const lListElem *aff = find_prop(r, "affinity_mask");
   T_ASSERT(dev != nullptr && strcmp(lGetString(dev, CE_stringval), "/dev/nvidia0") == 0);
   T_ASSERT(mem != nullptr && strcmp(lGetString(mem, CE_stringval), "80G") == 0);
   T_ASSERT(aff != nullptr && strcmp(lGetString(aff, CE_stringval), "SCCCCCCCCScccccccc") == 0);

   /* reader stores properties in file order; the writer sorts them at emit
    * time. Verify the count so the properties list length is exact. */
   T_ASSERT(lGetNumberOfElem(lGetList(r, RESL_properties)) == 3);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

static int
test_mixed_with_and_without_characteristics() {
   T_START("mixed_with_and_without_characteristics");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce,
                       "3(gpu0[device=/dev/nvidia0] gpu1 gpu2[device=/dev/nvidia2,memory=40G])",
                       &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr && lGetNumberOfElem(rm) == 3);

   const lListElem *r0 = lGetElemStr(rm, RESL_value, "gpu0");
   const lListElem *r1 = lGetElemStr(rm, RESL_value, "gpu1");
   const lListElem *r2 = lGetElemStr(rm, RESL_value, "gpu2");
   T_ASSERT(r0 != nullptr && r1 != nullptr && r2 != nullptr);

   T_ASSERT(lGetNumberOfElem(lGetList(r0, RESL_properties)) == 1);
   T_ASSERT(lGetList(r1, RESL_properties) == nullptr);
   T_ASSERT(lGetNumberOfElem(lGetList(r2, RESL_properties)) == 2);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* ------------------------------------------------------------------ */
/* negative: syntax errors the reader must reject                       */
/* ------------------------------------------------------------------ */

static int
test_reject_unclosed_bracket() {
   T_START("reject_unclosed_bracket");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "1(gpu0[device=/dev/nvidia0)", &alp) == 0);
   T_ASSERT(alp != nullptr);
   /* reader must clear the partial resource_map_list on failure so callers
    * don't observe half-parsed state */
   T_ASSERT(lGetList(ce, CE_resource_map_list) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

static int
test_reject_missing_eq() {
   T_START("reject_missing_eq");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "1(gpu0[device])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(lGetList(ce, CE_resource_map_list) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

static int
test_reject_characteristics_on_range() {
   T_START("reject_characteristics_on_range");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(1-2[device=/dev/nvidia0])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(lGetList(ce, CE_resource_map_list) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

#endif /* WITH_EXTENSIONS */

/* ------------------------------------------------------------------ */
/* duplicate-id semantics (CS-1338, Option A: all occurrences must match) */
/* ------------------------------------------------------------------ */

/* Regression: bare + bare duplicate. amount=2, no properties. */
static int
test_duplicate_bare() {
   T_START("duplicate_bare");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(gpu0 gpu0)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr && lGetNumberOfElem(rm) == 1);
   const lListElem *r = lGetElemStr(rm, RESL_value, "gpu0");
   T_ASSERT(r != nullptr);
   T_ASSERT(lGetUlong(r, RESL_amount) == 2);
   T_ASSERT(lGetList(r, RESL_properties) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* The remaining duplicate-id cases all involve characteristics, so they are
 * likewise GCS only. */
#if defined(WITH_EXTENSIONS)

/* Duplicate id with identical characteristics: stored once, amount=2. */
static int
test_duplicate_matching_characteristics() {
   T_START("duplicate_matching_characteristics");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(gpu0[memory=80G] gpu0[memory=80G])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lList *rm = lGetList(ce, CE_resource_map_list);
   T_ASSERT(rm != nullptr && lGetNumberOfElem(rm) == 1);
   const lListElem *r = lGetElemStr(rm, RESL_value, "gpu0");
   T_ASSERT(r != nullptr);
   T_ASSERT(lGetUlong(r, RESL_amount) == 2);
   const lList *props = lGetList(r, RESL_properties);
   T_ASSERT(props != nullptr && lGetNumberOfElem(props) == 1);
   const lListElem *mem = lGetElemStr(props, CE_name, "memory");
   T_ASSERT(mem != nullptr && strcmp(lGetString(mem, CE_stringval), "80G") == 0);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* Comparison is order-independent: same characteristics in different input
 * order are treated as identical. */
static int
test_duplicate_matching_reordered() {
   T_START("duplicate_matching_reordered");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(gpu0[a=1,b=2] gpu0[b=2,a=1])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lListElem *r = lGetElemStr(lGetList(ce, CE_resource_map_list),
                                    RESL_value, "gpu0");
   T_ASSERT(r != nullptr && lGetUlong(r, RESL_amount) == 2);
   T_ASSERT(lGetNumberOfElem(lGetList(r, RESL_properties)) == 2);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* Duplicate id with conflicting characteristics: reject. */
static int
test_reject_duplicate_conflict() {
   T_START("reject_duplicate_conflict");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(gpu0[memory=80G] gpu0[memory=40G])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(lGetList(ce, CE_resource_map_list) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* Whitespace inside a [...] block is stripped — this makes admin-typed
 * '\\' + <newline> line-continuation transparent. The flatfile scanner
 * turns that continuation into a single space; the reader must not treat
 * that space as an id-list separator. */
static int
test_whitespace_inside_brackets() {
   T_START("whitespace_inside_brackets");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   /* Simulate what the flatfile scanner delivers after resolving a
    * '\' + newline continuation:
    *     gpu0[device=/dev/nvidia0,\<newline>       memory=80G]
    * becomes a single space between ',' and 'memory' in the buffer. */
   T_ASSERT(run_reader(ce, "1(gpu0[device=/dev/nvidia0, memory=80G])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   const lListElem *r = lGetElemStr(lGetList(ce, CE_resource_map_list),
                                    RESL_value, "gpu0");
   T_ASSERT(r != nullptr && lGetUlong(r, RESL_amount) == 1);
   const lList *props = lGetList(r, RESL_properties);
   T_ASSERT(props != nullptr && lGetNumberOfElem(props) == 2);

   const lListElem *dev = lGetElemStr(props, CE_name, "device");
   const lListElem *mem = lGetElemStr(props, CE_name, "memory");
   T_ASSERT(dev != nullptr && strcmp(lGetString(dev, CE_stringval), "/dev/nvidia0") == 0);
   // whitespace must NOT leak into the characteristic name — 'memory', not ' memory'
   T_ASSERT(mem != nullptr && strcmp(lGetString(mem, CE_stringval), "80G") == 0);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

/* Bare + annotated occurrences of the same id are also a mismatch. */
static int
test_reject_duplicate_bare_vs_annotated() {
   T_START("reject_duplicate_bare_vs_annotated");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "2(gpu0[memory=80G] gpu0)", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(lGetList(ce, CE_resource_map_list) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}

#endif /* WITH_EXTENSIONS */

#if !defined(WITH_EXTENSIONS)
/* ------------------------------------------------------------------ */
/* OCS build: characteristics must be refused, not silently ignored     */
/* ------------------------------------------------------------------ */

/* An OCS build must reject any id carrying a characteristics block, and must
 * not leave a half-parsed resource map behind. Silently dropping the
 * characteristics would be worse than failing: a site could believe device
 * isolation was configured when nothing was applied. */
static int
test_characteristics_rejected_in_ocs() {
   T_START("characteristics_rejected_in_ocs");
   lListElem *ce = make_rsmap_ce("gpu");
   lList *alp = nullptr;

   T_ASSERT(run_reader(ce, "1(gpu0[device=/dev/nvidia0])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(lGetList(ce, CE_resource_map_list) == nullptr);

   lFreeElem(&ce);
   lFreeList(&alp);

   /* the plain grammar keeps working in OCS */
   ce = make_rsmap_ce("gpu");
   alp = nullptr;
   T_ASSERT(run_reader(ce, "2(gpu0 gpu1)", &alp) == 1);
   T_ASSERT(alp == nullptr);
   T_ASSERT(lGetNumberOfElem(lGetList(ce, CE_resource_map_list)) == 2);

   lFreeElem(&ce);
   lFreeList(&alp);
   return 0;
}
#endif /* !WITH_EXTENSIONS */

}  /* anonymous namespace */

int main(int, char **) {
   DENTER_MAIN(TOP_LAYER, "test_ff_rsmap");
   lInit(nmv);

   /* base grammar - available in both OCS and GCS */
   test_bare_ids();
   test_range_only();
   test_mixed_range_and_names();
   test_duplicate_bare();

#if defined(WITH_EXTENSIONS)
   /* per-instance characteristics - GCS only */
   test_single_characteristic();
   test_multiple_characteristics();
   test_mixed_with_and_without_characteristics();
   test_reject_unclosed_bracket();
   test_reject_missing_eq();
   test_reject_characteristics_on_range();
   test_duplicate_matching_characteristics();
   test_duplicate_matching_reordered();
   test_reject_duplicate_conflict();
   test_reject_duplicate_bare_vs_annotated();
   test_whitespace_inside_brackets();
#else
   test_characteristics_rejected_in_ocs();
#endif

   if (g_failures > 0) {
      fprintf(stderr, "\n=== %d test(s) FAILED ===\n", g_failures);
      sge_exit(1);
   }
   printf("\n=== all tests passed ===\n");
   DRETURN(EXIT_SUCCESS);
}
