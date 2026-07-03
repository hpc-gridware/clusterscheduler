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

#include "basis_types.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_var.h"

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

/**
 * @brief Return the VA_variable string of the n-th element, or "" if absent.
 * @param[in] lp list to inspect (may be nullptr)
 * @param[in] n  zero-based element index
 * @return the element's VA_variable, or "" when missing
 */
static const char *
nth_var(lList *lp, int n) {
   const lListElem *ep = lFirst(lp);
   for (int i = 0; i < n && ep != nullptr; ++i) {
      ep = lNext(ep);
   }
   const char *s = (ep != nullptr) ? lGetString(ep, VA_variable) : nullptr;
   return (s != nullptr) ? s : "";
}

// ---------------------------------------------------------------------------

/**
 * @brief Regression tests for var_list_parse_from_string() [T01–T10].
 *
 * var_list_parse_from_string() parses an attacker-controlled comma-separated
 * variable list (qsub/qalter -v/-V/-ac/-dc, JSV job context) into a VA_Type
 * list.
 *
 * SECURITY REGRESSION (CS-2350, MEDIUM-SGEOBJ-002, CWE-476/CWE-617): a token
 * consisting only of '=' characters (e.g. "==") makes sge_strtok_r() return
 * nullptr. The code used to SGE_ASSERT() on it and then call strlen() on the
 * NULL pointer — abort() in debug builds, a NULL dereference under NDEBUG. The
 * fix rejects the token with return code 5. T02/T05/T06 exercise that path; on
 * the unpatched code they abort/segfault instead of returning.
 *
 * @return none; failures are accumulated in the file-local s_fail counter
 */
static void
test_var_list_parse() {
   // T01: a well-formed name=value pair parses successfully.
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "FOO=bar", 0);
      CHECK(1, "valid FOO=bar accepted", ret == 0 && strcmp(nth_var(lp, 0), "FOO") == 0);
      lFreeList(&lp);
   }

   // T02: RED->GREEN. An all-delimiter token ("==") must be rejected with an
   //      error, not asserted/dereferenced. Unpatched code aborts/segfaults here.
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "==", 0);
      CHECK(2, "all-delimiter token \"==\" rejected (no crash)", ret != 0);
      lFreeList(&lp);
   }

   // T03: a single '=' is likewise an empty name -> rejected.
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "=", 0);
      CHECK(3, "single \"=\" rejected", ret != 0);
      lFreeList(&lp);
   }

   // T04: a valid name with no value parses (value taken as none here).
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "FOO", 0);
      CHECK(4, "valid name without value accepted",
            ret == 0 && strcmp(nth_var(lp, 0), "FOO") == 0);
      lFreeList(&lp);
   }

   // T05: a malformed token *after* a valid one is still caught (mid-list).
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "FOO=bar,==", 0);
      CHECK(5, "malformed token mid-list rejected", ret != 0);
      lFreeList(&lp);
   }

   // T06: a malformed token *before* a valid one is caught on the first token.
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "==,FOO=bar", 0);
      CHECK(6, "malformed leading token rejected", ret != 0);
      lFreeList(&lp);
   }

   // T07/T08: multiple valid tokens produce one element each, in order.
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "A=1,B=2", 0);
      CHECK(7, "two valid tokens accepted", ret == 0 && lGetNumberOfElem(lp) == 2);
      CHECK(8, "elements kept in order",
            strcmp(nth_var(lp, 0), "A") == 0 && strcmp(nth_var(lp, 1), "B") == 0);
      lFreeList(&lp);
   }

   // T09: empty value after '=' is valid (name present, value empty).
   {
      lList *lp = nullptr;
      int ret = var_list_parse_from_string(&lp, "FOO=", 0);
      CHECK(9, "name with empty value accepted",
            ret == 0 && strcmp(nth_var(lp, 0), "FOO") == 0);
      lFreeList(&lp);
   }

   // T10: a nullptr list pointer is rejected without crashing.
   {
      int ret = var_list_parse_from_string(nullptr, "A=1", 0);
      CHECK(10, "nullptr lpp rejected", ret != 0);
   }
}

// ---------------------------------------------------------------------------

/**
 * @brief Module-test entry point.
 * @return 0 if all checks pass, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_var_list_parse();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
