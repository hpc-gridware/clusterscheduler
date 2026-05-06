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

#include <cmath>
#include <cstdio>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/cull/sge_all_listsL.h"

#include "sort_hosts.h"

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

// Floating-point comparison with a small absolute tolerance
static bool feq(double a, double b) {
   return fabs(a - b) < 1e-9;
}

// ---------------------------------------------------------------------------
// validate_load_formula — valid formulas  [T01–T09]
//
// validate_load_formula() checks that a load formula string is syntactically
// valid and references only known complex attributes.  A valid formula:
//   - starts with an attribute name (optionally prefixed with '$')
//   - is followed by zero or more operator-operand pairs (+, -, *)
//   - operands may be numeric constants or '$'-prefixed attribute names
//   - must NOT start with a numeric literal
//   - must NOT use a comma as decimal separator
//   - must NOT reference undefined or reserved attribute names (e.g. "none")
//
// The test cluster resource: num_proc = 4 (double)
// ---------------------------------------------------------------------------

static void test_validate_valid(lList **alpp, const lList *centry_list) {
   printf("\n--- validate_load_formula (valid) ---\n");

   // T01: bare attribute name is the simplest valid formula
   CHECK(1,  "bare attribute name",             validate_load_formula("num_proc",         alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T02: '$'-prefixed attribute name is also valid (shell-variable style)
   CHECK(2,  "$-prefixed attribute name",        validate_load_formula("$num_proc",        alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T03: multiply $name by an integer constant
   CHECK(3,  "$name * integer",                  validate_load_formula("$num_proc*2",      alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T04: multiply $name by a fractional constant
   CHECK(4,  "$name * fractional constant",      validate_load_formula("$num_proc*0.5",    alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T05: multiply without '$' prefix — both forms are accepted
   CHECK(5,  "name * integer (no $ prefix)",     validate_load_formula("num_proc*2",       alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T06: addition of an integer constant
   CHECK(6,  "name + integer",                   validate_load_formula("num_proc+1",       alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T07: subtraction of an integer constant
   CHECK(7,  "$name - integer",                  validate_load_formula("$num_proc-2",      alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T08: addition of a fractional constant
   CHECK(8,  "$name + fractional",               validate_load_formula("$num_proc+0.1",    alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);

   // T09: leading constant followed by attribute — allowed as long as attribute comes first in value terms
   CHECK(9,  "constant + $name + fractional",    validate_load_formula("1+$num_proc+0.1",  alpp, centry_list, "load_formula") == true);
   lFreeList(alpp);
}

// ---------------------------------------------------------------------------
// validate_load_formula — invalid formulas  [T10–T12]
// ---------------------------------------------------------------------------

static void test_validate_invalid(lList **alpp, const lList *centry_list) {
   printf("\n--- validate_load_formula (invalid) ---\n");

   // T10: formula starting with a numeric literal is rejected
   //      (the parser requires the formula to begin with an attribute name)
   CHECK(10, "leading numeric rejected (2*num_proc)",   validate_load_formula("2*num_proc",      alpp, centry_list, "load_formula") == false);
   lFreeList(alpp);

   // T11: comma as decimal separator is invalid — only '.' is accepted
   CHECK(11, "comma decimal separator rejected",        validate_load_formula("2,0+num_proc",    alpp, centry_list, "load_formula") == false);
   lFreeList(alpp);

   // T12: "none" is a reserved keyword; it is not a valid attribute reference
   CHECK(12, "reserved keyword 'none' rejected",        validate_load_formula("none",            alpp, centry_list, "load_formula") == false);
   lFreeList(alpp);
}

// ---------------------------------------------------------------------------
// scaled_mixed_load — formula evaluation  [T13–T21]
//
// scaled_mixed_load() evaluates a load formula against a host's resource
// values and returns the result as a double.
//
// Test setup: num_proc = 4.0
// ---------------------------------------------------------------------------

static void test_scaled_mixed_load(lListElem *host, const lList *centry_list) {
   printf("\n--- scaled_mixed_load ---\n");
   double val;

   // T13: bare attribute reference evaluates to the attribute's value
   val = scaled_mixed_load("num_proc", nullptr, host, centry_list);
   CHECK(13, "num_proc → 4.0",         feq(val, 4.0));

   // T14: '$'-prefixed reference evaluates to the same value
   val = scaled_mixed_load("$num_proc", nullptr, host, centry_list);
   CHECK(14, "$num_proc → 4.0",        feq(val, 4.0));

   // T15: multiply by 2 → 8.0
   val = scaled_mixed_load("$num_proc*2", nullptr, host, centry_list);
   CHECK(15, "$num_proc*2 → 8.0",      feq(val, 8.0));

   // T16: multiply by 0.5 → 2.0
   val = scaled_mixed_load("$num_proc*0.5", nullptr, host, centry_list);
   CHECK(16, "$num_proc*0.5 → 2.0",    feq(val, 2.0));

   // T17: multiply without '$' prefix — same result
   val = scaled_mixed_load("num_proc*2", nullptr, host, centry_list);
   CHECK(17, "num_proc*2 → 8.0",       feq(val, 8.0));

   // T18: add integer constant
   val = scaled_mixed_load("num_proc+1", nullptr, host, centry_list);
   CHECK(18, "num_proc+1 → 5.0",       feq(val, 5.0));

   // T19: subtract integer constant
   val = scaled_mixed_load("$num_proc-2", nullptr, host, centry_list);
   CHECK(19, "$num_proc-2 → 2.0",      feq(val, 2.0));

   // T20: add fractional constant
   val = scaled_mixed_load("$num_proc+0.1", nullptr, host, centry_list);
   CHECK(20, "$num_proc+0.1 → 4.1",    feq(val, 4.1));

   // T21: leading constant + attribute + fractional constant
   val = scaled_mixed_load("1+$num_proc+0.1", nullptr, host, centry_list);
   CHECK(21, "1+$num_proc+0.1 → 5.1",  feq(val, 5.1));
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sched_load_formula");

   lInit(nmv);

   // build a single-entry complex with num_proc = 4
   lList *centry_list = lCreateList("", CE_Type);
   lListElem *centry = lCreateElem(CE_Type);
   lSetString(centry, CE_name, "num_proc");
   lSetString(centry, CE_stringval, "4");
   lSetDouble(centry, CE_doubleval, 4);
   lAppendElem(centry_list, centry);

   // build a host element carrying the complex values
   lList *host_centry_list = lCreateList("", CE_Type);
   lAppendElem(host_centry_list, lCopyElem(centry));
   lListElem *host = lCreateElem(EH_Type);
   lSetList(host, EH_consumable_config_list, host_centry_list);

   lList *answer_list = nullptr;

   test_validate_valid(&answer_list, centry_list);
   test_validate_invalid(&answer_list, centry_list);
   test_scaled_mixed_load(host, centry_list);

   lFreeList(&centry_list);
   lFreeElem(&host);

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
