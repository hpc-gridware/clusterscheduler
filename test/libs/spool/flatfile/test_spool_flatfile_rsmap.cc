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
#include <string>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_centry.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "spool/flatfile/sge_flatfile_obj_rsmap.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"
#include "uti/sge_unistd.h"
#include "sgeobj/sge_answer.h"

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

/**
 * @brief Build a fresh CE element ready for read_CE_stringval_host to fill in
 * @param name the name of the complex entry
 * @return the complex entry
 */
static ocs::tom::ElementPtr<ocs::tom::CE> make_rsmap_ce(const char *name) {
   auto ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ce->set<CE_name>(name);
   ce->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   return ce;
}

/**
 * @brief Read a resource map value into a complex entry
 *
 * read_CE_stringval_host expects a mutable buffer (sge_strtok_r modifies it),
 * so hand it a strdup and free after.
 *
 * @param ce the complex entry to fill
 * @param input the value
 * @param alp the answer list
 * @return 1 if the reader accepted the value, 0 otherwise
 */
static int
run_reader(const ocs::tom::Element<ocs::tom::CE> ce, const char *input, ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   char *buf = strdup(input);
   int rc = read_CE_stringval_host(ce, buf, alp) ? 1 : 0;
   free(buf);
   return rc;
}

/**
 * @brief Write a complex entry back out the way the flatfile spooler does
 *
 * Returns what it produced, so that a value can be read in and written out again.
 *
 * @param ce the complex entry
 * @return the written value
 */
static std::string
run_writer(const ocs::tom::ConstElement<ocs::tom::CE> ce) {
   dstring buffer = DSTRING_INIT;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   write_CE_stringval_host(ce, &buffer, &alp);
   std::string out{sge_dstring_get_string(&buffer) == nullptr ? "" : sge_dstring_get_string(&buffer)};
   sge_dstring_free(&buffer);
   alp.reset();
   return out;
}

/**
 * @brief Read a value and write it straight back out
 *
 * What comes out has to be something the reader accepts again and which describes the same map.
 * @param input the value
 * @return what the writer produced, or "<reader rejected>"
 */
static std::string
round_trip(const char *input) {
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   if (run_reader(*ce, input, &alp) != 1) {
      ce.reset();
      alp.reset();
      return "<reader rejected>";
   }
   std::string out = run_writer(*ce);
   ce.reset();
   alp.reset();
   return out;
}

#if defined(WITH_EXTENSIONS)
/**
 * @brief Look up a property by name inside the properties list of a resource map entry
 *
 * Only used by the GCS-only characteristics tests.
 *
 * @param resl the resource map entry
 * @param name the name of the property
 * @return the property, empty if absent
 */
static ocs::tom::ConstElement<ocs::tom::CE>
find_prop(const ocs::tom::ConstElement<ocs::tom::RESL> resl, const char *name) {
   ocs::tom::ConstList<ocs::tom::CE> props = resl.get<RESL_properties>();
   if (props == nullptr) return nullptr;
   return props.find_first<CE_name>(name);
}
#endif /* WITH_EXTENSIONS */

/* ------------------------------------------------------------------ */
/* writing: what is read in has to come back out unchanged              */
/* ------------------------------------------------------------------ */

/* An id which occurs more than once describes N-way sharing of one device,
 * and the count is part of the configuration. Writing has to repeat the id,
 * not collapse the map to the set of distinct ids. */
static int
test_write_repeated_id() {
   T_START("write_repeated_id");
   T_ASSERT(round_trip("4(0 0 0 1)") == "4(0 0 0 1)");
   T_ASSERT(round_trip("4(0 0 1 1)") == "4(0 0 1 1)");
   T_ASSERT(round_trip("3(A A B)") == "3(A A B)");
   return 0;
}

/* Ids which occur equally often may share a range, because the written form
 * repeats the whole range once per occurrence. Ids which occur a different
 * number of times may not. */
static int
test_write_range_compaction() {
   T_START("write_range_compaction");
   T_ASSERT(round_trip("4(1-4)") == "4(1-4)");
   T_ASSERT(round_trip("6(0 0 1 1 2 2)") == "6(0-2 0-2)");
   T_ASSERT(round_trip("5(0 0 1 2 2)") == "5(0 0 1 2 2)");
   T_ASSERT(round_trip("2(A B)") == "2(A B)");
   return 0;
}

/* ------------------------------------------------------------------ */
/* positive: base grammar — must still parse exactly as before CS-1338  */
/* ------------------------------------------------------------------ */

/**
 * @brief Resource map ids given as bare names
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_bare_ids() {
   T_START("bare_ids");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(A B)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr);
   T_ASSERT(rm.size() == 2);

   ocs::tom::ConstElement<ocs::tom::RESL> r_a = rm.find_first<RESL_value>("A");
   ocs::tom::ConstElement<ocs::tom::RESL> r_b = rm.find_first<RESL_value>("B");
   T_ASSERT(r_a != nullptr && r_a.get<RESL_amount>() == 1);
   T_ASSERT(r_b != nullptr && r_b.get<RESL_amount>() == 1);
   T_ASSERT(r_a.get<RESL_properties>() == nullptr);
   T_ASSERT(r_b.get<RESL_properties>() == nullptr);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Resource map ids given as a range
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_range_only() {
   T_START("range_only");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "3(1-3)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr);
   T_ASSERT(rm.size() == 3);
   T_ASSERT(rm.find_first<RESL_value>("1") != nullptr);
   T_ASSERT(rm.find_first<RESL_value>("2") != nullptr);
   T_ASSERT(rm.find_first<RESL_value>("3") != nullptr);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Resource map ids given as a range and as names
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_mixed_range_and_names() {
   T_START("mixed_range_and_names");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "4(A B 1-2)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr);
   T_ASSERT(rm.size() == 4);
   T_ASSERT(rm.find_first<RESL_value>("A") != nullptr);
   T_ASSERT(rm.find_first<RESL_value>("B") != nullptr);
   T_ASSERT(rm.find_first<RESL_value>("1") != nullptr);
   T_ASSERT(rm.find_first<RESL_value>("2") != nullptr);

   ce.reset();
   alp.reset();
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

/**
 * @brief A resource map id with one characteristic
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_single_characteristic() {
   T_START("single_characteristic");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "1(gpu0[device=/dev/nvidia0])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr && rm.size() == 1);

   const auto r = rm.find_first<RESL_value>("gpu0");
   T_ASSERT(r != nullptr);
   const auto dev = find_prop(r, "device");
   T_ASSERT(dev != nullptr);
   T_ASSERT(strcmp(ocs::tom::get<CE_stringval>(dev), "/dev/nvidia0") == 0);
   /* reader stores only name+stringval verbatim; valtype must remain unset */
   T_ASSERT(ocs::tom::get<CE_valtype>(dev) == 0);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief A resource map id with several characteristics
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_multiple_characteristics() {
   T_START("multiple_characteristics");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce,
                       "1(gpu0[device=/dev/nvidia0,memory=80G,affinity_mask=SCCCCCCCCScccccccc])",
                       &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstElement<ocs::tom::RESL> r = ocs::tom::find_first<RESL_value>(ce->get<CE_resource_map_list>(), "gpu0");
   T_ASSERT(r != nullptr);

   const auto dev = find_prop(r, "device");
   const auto mem = find_prop(r, "memory");
   const auto aff = find_prop(r, "affinity_mask");
   T_ASSERT(dev != nullptr && strcmp(ocs::tom::get<CE_stringval>(dev), "/dev/nvidia0") == 0);
   T_ASSERT(mem != nullptr && strcmp(ocs::tom::get<CE_stringval>(mem), "80G") == 0);
   T_ASSERT(aff != nullptr && strcmp(ocs::tom::get<CE_stringval>(aff), "SCCCCCCCCScccccccc") == 0);

   /* reader stores properties in file order; the writer sorts them at emit
    * time. Verify the count so the properties list length is exact. */
   T_ASSERT(r.get<RESL_properties>().size() == 3);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Resource map ids with and without characteristics in one value
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_mixed_with_and_without_characteristics() {
   T_START("mixed_with_and_without_characteristics");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce,
                       "3(gpu0[device=/dev/nvidia0] gpu1 gpu2[device=/dev/nvidia2,memory=40G])",
                       &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr && rm.size() == 3);

   ocs::tom::ConstElement<ocs::tom::RESL> r0 = rm.find_first<RESL_value>("gpu0");
   ocs::tom::ConstElement<ocs::tom::RESL> r1 = rm.find_first<RESL_value>("gpu1");
   ocs::tom::ConstElement<ocs::tom::RESL> r2 = rm.find_first<RESL_value>("gpu2");
   T_ASSERT(r0 != nullptr && r1 != nullptr && r2 != nullptr);

   T_ASSERT(r0.get<RESL_properties>().size() == 1);
   T_ASSERT(r1.get<RESL_properties>() == nullptr);
   T_ASSERT(r2.get<RESL_properties>().size() == 2);

   ce.reset();
   alp.reset();
   return 0;
}

/* ------------------------------------------------------------------ */
/* negative: syntax errors the reader must reject                       */
/* ------------------------------------------------------------------ */

/**
 * @brief A characteristics block without its closing bracket is rejected
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_reject_unclosed_bracket() {
   T_START("reject_unclosed_bracket");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "1(gpu0[device=/dev/nvidia0)", &alp) == 0);
   T_ASSERT(alp != nullptr);
   /* reader must clear the partial resource_map_list on failure so callers
    * don't observe half-parsed state */
   T_ASSERT(!ce->get<CE_resource_map_list>());

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief A characteristic without '=' is rejected
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_reject_missing_eq() {
   T_START("reject_missing_eq");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "1(gpu0[device])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(!ce->get<CE_resource_map_list>());

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Characteristics on a range of ids are rejected
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_reject_characteristics_on_range() {
   T_START("reject_characteristics_on_range");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(1-2[device=/dev/nvidia0])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(!ce->get<CE_resource_map_list>());

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief A bare number is a range of exactly one id, and there it names a single instance
 *
 * it takes characteristics like any other identifier. Only a range covering more than one is refused.
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_characteristics_on_numeric_id() {
   T_START("characteristics_on_numeric_id");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(0[device=/dev/nvidia0] 1[device=/dev/nvidia1])", &alp) == 1);
   T_ASSERT(ce->get<CE_resource_map_list>().size() == 2);

   ocs::tom::ConstElement<ocs::tom::RESL> resl = ce->find_first_in<CE_resource_map_list, RESL_value>("0");
   T_ASSERT(resl != nullptr);
   T_ASSERT(resl.get<RESL_properties>().size() == 1);
   T_ASSERT(strcmp(ocs::tom::get<CE_stringval>(resl.get<RESL_properties>().front()),
                   "/dev/nvidia0") == 0);

   ce.reset();
   alp.reset();

   /* a range of one written as a range is still a single instance */
   ce = make_rsmap_ce("gpu");
   T_ASSERT(run_reader(*ce, "1(3-3[device=/dev/nvidia3])", &alp) == 1);
   T_ASSERT((ce->find_first_in<CE_resource_map_list, RESL_value>("3") != nullptr));

   ce.reset();
   alp.reset();
   return 0;
}

#endif /* WITH_EXTENSIONS */

/* ------------------------------------------------------------------ */
/* duplicate-id semantics (CS-1338, Option A: all occurrences must match) */
/* ------------------------------------------------------------------ */

/**
 * @brief Regression: a bare id given twice is stored once with amount 2 and no properties
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_duplicate_bare() {
   T_START("duplicate_bare");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(gpu0 gpu0)", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr && rm.size() == 1);
   ocs::tom::ConstElement<ocs::tom::RESL> r = rm.find_first<RESL_value>("gpu0");
   T_ASSERT(r != nullptr);
   T_ASSERT(r.get<RESL_amount>() == 2);
   T_ASSERT(r.get<RESL_properties>() == nullptr);

   ce.reset();
   alp.reset();
   return 0;
}

/* The remaining duplicate-id cases all involve characteristics, so they are
 * likewise GCS only. */
#if defined(WITH_EXTENSIONS)

/**
 * @brief Duplicate id with identical characteristics
 *
 * stored once, amount=2.
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_duplicate_matching_characteristics() {
   T_START("duplicate_matching_characteristics");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(gpu0[memory=80G] gpu0[memory=80G])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstList<ocs::tom::RESL> rm = ce->get<CE_resource_map_list>();
   T_ASSERT(rm != nullptr && rm.size() == 1);
   ocs::tom::ConstElement<ocs::tom::RESL> r = rm.find_first<RESL_value>("gpu0");
   T_ASSERT(r != nullptr);
   T_ASSERT(r.get<RESL_amount>() == 2);
   ocs::tom::ConstList<ocs::tom::CE> props = r.get<RESL_properties>();
   T_ASSERT(props != nullptr && props.size() == 1);
   ocs::tom::ConstElement<ocs::tom::CE> mem = props.find_first<CE_name>("memory");
   T_ASSERT(mem != nullptr && strcmp(mem.get<CE_stringval>(), "80G") == 0);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Comparison is order-independent
 *
 * same characteristics in different input order are treated as identical.
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_duplicate_matching_reordered() {
   T_START("duplicate_matching_reordered");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(gpu0[a=1,b=2] gpu0[b=2,a=1])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstElement<ocs::tom::RESL> r = ocs::tom::find_first<RESL_value>(ce->get<CE_resource_map_list>(), "gpu0");
   T_ASSERT(r != nullptr && r.get<RESL_amount>() == 2);
   T_ASSERT(r.get<RESL_properties>().size() == 2);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Duplicate id with conflicting characteristics
 *
 * reject.
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_reject_duplicate_conflict() {
   T_START("reject_duplicate_conflict");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(gpu0[memory=80G] gpu0[memory=40G])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(!ce->get<CE_resource_map_list>());

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Whitespace inside a [...] block is stripped — this makes admin-typed '\\' + <newline> line-continuation
 *        transparent
 *
 * The flatfile scanner turns that continuation into a single space; the reader must not treat that space as an
 * id-list separator.
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_whitespace_inside_brackets() {
   T_START("whitespace_inside_brackets");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   /* Simulate what the flatfile scanner delivers after resolving a
    * '\' + newline continuation:
    *     gpu0[device=/dev/nvidia0,\<newline>       memory=80G]
    * becomes a single space between ',' and 'memory' in the buffer. */
   T_ASSERT(run_reader(*ce, "1(gpu0[device=/dev/nvidia0, memory=80G])", &alp) == 1);
   T_ASSERT(alp == nullptr);

   ocs::tom::ConstElement<ocs::tom::RESL> r = ocs::tom::find_first<RESL_value>(ce->get<CE_resource_map_list>(), "gpu0");
   T_ASSERT(r != nullptr && r.get<RESL_amount>() == 1);
   ocs::tom::ConstList<ocs::tom::CE> props = r.get<RESL_properties>();
   T_ASSERT(props != nullptr && props.size() == 2);

   ocs::tom::ConstElement<ocs::tom::CE> dev = props.find_first<CE_name>("device");
   ocs::tom::ConstElement<ocs::tom::CE> mem = props.find_first<CE_name>("memory");
   T_ASSERT(dev != nullptr && strcmp(dev.get<CE_stringval>(), "/dev/nvidia0") == 0);
   // whitespace must NOT leak into the characteristic name — 'memory', not ' memory'
   T_ASSERT(mem != nullptr && strcmp(mem.get<CE_stringval>(), "80G") == 0);

   ce.reset();
   alp.reset();
   return 0;
}

/**
 * @brief Bare + annotated occurrences of the same id are also a mismatch
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_reject_duplicate_bare_vs_annotated() {
   T_START("reject_duplicate_bare_vs_annotated");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "2(gpu0[memory=80G] gpu0)", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(!ce->get<CE_resource_map_list>());

   ce.reset();
   alp.reset();
   return 0;
}

#endif /* WITH_EXTENSIONS */

#if !defined(WITH_EXTENSIONS)
/* ------------------------------------------------------------------ */
/* OCS build: characteristics must be refused, not silently ignored     */
/* ------------------------------------------------------------------ */

/**
 * @brief An OCS build must reject any id carrying a characteristics block, and must not leave a half-parsed resource
 *        map behind
 *
 * Silently dropping the characteristics would be worse than failing: a site could believe device isolation was
 * configured when nothing was applied.
 * @return 0 if every assertion held, 1 otherwise
 */
static int
test_characteristics_rejected_in_ocs() {
   T_START("characteristics_rejected_in_ocs");
   auto ce = make_rsmap_ce("gpu");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   T_ASSERT(run_reader(*ce, "1(gpu0[device=/dev/nvidia0])", &alp) == 0);
   T_ASSERT(alp != nullptr);
   T_ASSERT(!ce->get<CE_resource_map_list>());

   ce.reset();
   alp.reset();

   /* the plain grammar keeps working in OCS */
   ce = make_rsmap_ce("gpu");
   alp = nullptr;
   T_ASSERT(run_reader(*ce, "2(gpu0 gpu1)", &alp) == 1);
   T_ASSERT(alp == nullptr);
   T_ASSERT(ce->get<CE_resource_map_list>().size() == 2);

   ce.reset();
   alp.reset();
   return 0;
}
#endif /* !WITH_EXTENSIONS */

}  /* anonymous namespace */

int main(int, char **) {
   DENTER_MAIN(TOP_LAYER, "test_ff_rsmap");
   ocs::tom::install_model(ocs::tom::gcs_model());

   /* base grammar - available in both OCS and GCS */
   test_bare_ids();
   test_range_only();
   test_mixed_range_and_names();
   test_duplicate_bare();
   test_write_repeated_id();
   test_write_range_compaction();

#if defined(WITH_EXTENSIONS)
   /* per-instance characteristics - GCS only */
   test_single_characteristic();
   test_multiple_characteristics();
   test_mixed_with_and_without_characteristics();
   test_reject_unclosed_bracket();
   test_reject_missing_eq();
   test_reject_characteristics_on_range();
   test_characteristics_on_numeric_id();
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
