/*___INFO__MARK_BEGIN_NEW__*/
/*************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
 ************************************************************************/
/*___INFO__MARK_END_NEW__*/

/*
 * Unit tests for sge_eval_expression().
 *
 * Interactive mode: test_eval_expression <expr> <value>
 *   Evaluates one expression against one value and prints TRUE/FALSE/ERROR.
 *
 * Batch mode (no args): runs all CHECK scenarios and prints PASS/FAIL summary.
 */

#include <cstdio>
#include <cstring>

#include "ocs_CEntry.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_eval_expression.h"

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

// Returns true when sge_strtolower(a) == sge_strtolower(b)
static bool tolower_eq(const char *a, const char *b) {
   char *t1 = strdup(a);
   char *t2 = strdup(b);
   sge_strtolower(t1, 255);
   sge_strtolower(t2, 255);
   bool result = (strcmp(t1, t2) == 0);
   sge_free(&t1);
   sge_free(&t2);
   return result;
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_eval_expression");

   if (argc == 3) {
      // interactive single-expression evaluation
      int ret = sge_eval_expression(ocs::CEntry::Type::RESTR, argv[1], argv[2], nullptr);
      const char *result = (ret == -1) ? "ERROR" : (ret == 0) ? "TRUE" : "FALSE";
      fprintf(stdout, "eval_expr(%s,%s) => %s\n", argv[1], argv[2], result);
      DRETURN(ret);
   }

   // suppress sge_log stderr output from ERROR() calls inside sge_eval_expression
   component_set_daemonized(true);

   int id = 1;

   printf("\n--- tolower ---\n");
   // sge_strtolower is idempotent and case-insensitive on already-lower input
   CHECK(id++, "tolower: identical mixed-case strings",  tolower_eq("TESTDATA*[]dda", "TESTDATA*[]dda"));
   CHECK(id++, "tolower: upper vs lower",                tolower_eq("TESTDATA*[]dda", "testdata*[]dda"));
   CHECK(id++, "tolower: lower vs upper",                tolower_eq("TESTDATA*[]dda", "TESTDATA*[]DDA"));

   printf("\n--- STR: AND ---\n");
   // "a & b" requires both a and b simultaneously; a single-token value can never satisfy both
   CHECK(id++, "STR: 'a & b' vs 'a' -> false",   sge_eval_expression(ocs::CEntry::Type::STR, "a & b",    "a", nullptr) == 1);
   CHECK(id++, "STR: 'a & b' vs 'b' -> false",   sge_eval_expression(ocs::CEntry::Type::STR, "a & b",    "b", nullptr) == 1);
   CHECK(id++, "STR: 'a* & b*' vs 'a' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "a* & b*",  "a", nullptr) == 1);
   CHECK(id++, "STR: 'a* & b*' vs 'b' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "a* & b*",  "b", nullptr) == 1);

   printf("\n--- STR: AND NOT ---\n");
   // "a & !b": matches a-only, not b
   CHECK(id++, "STR: 'a & !b' vs 'a' -> true",    sge_eval_expression(ocs::CEntry::Type::STR, "a & !b",   "a", nullptr) == 0);
   CHECK(id++, "STR: 'a & !b' vs 'b' -> false",   sge_eval_expression(ocs::CEntry::Type::STR, "a & !b",   "b", nullptr) == 1);
   CHECK(id++, "STR: 'a* & !b*' vs 'a' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "a* & !b*", "a", nullptr) == 0);
   CHECK(id++, "STR: 'a* & !b*' vs 'b' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "a* & !b*", "b", nullptr) == 1);

   printf("\n--- STR: identity ---\n");
   // single token and wildcard variant
   CHECK(id++, "STR: 'a' vs 'a' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "a",  "a", nullptr) == 0);
   CHECK(id++, "STR: 'a*' vs 'a' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "a*", "a", nullptr) == 0);

   printf("\n--- STR: NOT AND ---\n");
   // "!a & b": matches b-only, not a
   CHECK(id++, "STR: '!a & b' vs 'a' -> false",   sge_eval_expression(ocs::CEntry::Type::STR, "!a & b",   "a", nullptr) == 1);
   CHECK(id++, "STR: '!a & b' vs 'b' -> true",    sge_eval_expression(ocs::CEntry::Type::STR, "!a & b",   "b", nullptr) == 0);
   CHECK(id++, "STR: '!a* & b*' vs 'a' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "!a* & b*", "a", nullptr) == 1);
   CHECK(id++, "STR: '!a* & b*' vs 'b' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "!a* & b*", "b", nullptr) == 0);

   printf("\n--- STR: XOR ((!a&b)|(a&!b)) ---\n");
   // symmetric difference: matches exactly one of a or b
   CHECK(id++, "STR: XOR vs 'a' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "(!a & b) | (a & !b)",   "a", nullptr) == 0);
   CHECK(id++, "STR: XOR vs 'b' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "(!a & b) | (a & !b)",   "b", nullptr) == 0);
   CHECK(id++, "STR: XOR* vs 'a' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "(!a* & b*) | (a* & !b*)", "a", nullptr) == 0);
   CHECK(id++, "STR: XOR* vs 'b' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "(!a* & b*) | (a* & !b*)", "b", nullptr) == 0);

   printf("\n--- STR: OR ---\n");
   // "a | b": matches either a or b
   CHECK(id++, "STR: 'a | b' vs 'a' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "a | b",   "a", nullptr) == 0);
   CHECK(id++, "STR: 'a | b' vs 'b' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "a | b",   "b", nullptr) == 0);
   CHECK(id++, "STR: 'a* | b*' vs 'a' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "a* | b*", "a", nullptr) == 0);
   CHECK(id++, "STR: 'a* | b*' vs 'b' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "a* | b*", "b", nullptr) == 0);

   printf("\n--- STR: NOR (!(a|b)) ---\n");
   // "!(a | b)": matches neither a nor b
   CHECK(id++, "STR: NOR vs 'a' -> false",  sge_eval_expression(ocs::CEntry::Type::STR, "!(a | b)",   "a", nullptr) == 1);
   CHECK(id++, "STR: NOR vs 'b' -> false",  sge_eval_expression(ocs::CEntry::Type::STR, "!(a | b)",   "b", nullptr) == 1);
   CHECK(id++, "STR: NOR* vs 'a' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "!(a* | b*)", "a", nullptr) == 1);
   CHECK(id++, "STR: NOR* vs 'b' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "!(a* | b*)", "b", nullptr) == 1);

   printf("\n--- STR: XNOR ((!a|b)&(a|!b)) ---\n");
   // logical equivalence: neither a alone nor b alone
   CHECK(id++, "STR: XNOR vs 'a' -> false",  sge_eval_expression(ocs::CEntry::Type::STR, "(!a | b) & (a | !b)",   "a", nullptr) == 1);
   CHECK(id++, "STR: XNOR vs 'b' -> false",  sge_eval_expression(ocs::CEntry::Type::STR, "(!a | b) & (a | !b)",   "b", nullptr) == 1);
   CHECK(id++, "STR: XNOR* vs 'a' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "(!a* | b*) & (a* | !b*)", "a", nullptr) == 1);
   CHECK(id++, "STR: XNOR* vs 'b' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "(!a* | b*) & (a* | !b*)", "b", nullptr) == 1);

   printf("\n--- STR: OR NOT ---\n");
   // "a | !b": matches a or anything that is not b
   CHECK(id++, "STR: 'a | !b' vs 'a' -> true",    sge_eval_expression(ocs::CEntry::Type::STR, "a | !b",   "a", nullptr) == 0);
   CHECK(id++, "STR: 'a | !b' vs 'b' -> false",   sge_eval_expression(ocs::CEntry::Type::STR, "a | !b",   "b", nullptr) == 1);
   CHECK(id++, "STR: 'a* | !b*' vs 'a' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "a* | !b*", "a", nullptr) == 0);
   CHECK(id++, "STR: 'a* | !b*' vs 'b' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "a* | !b*", "b", nullptr) == 1);

   printf("\n--- STR: NOT OR ---\n");
   // "!a | b": matches b or anything that is not a
   CHECK(id++, "STR: '!a | b' vs 'a' -> false",  sge_eval_expression(ocs::CEntry::Type::STR, "!a | b",   "a", nullptr) == 1);
   CHECK(id++, "STR: '!a | b' vs 'b' -> true",   sge_eval_expression(ocs::CEntry::Type::STR, "!a | b",   "b", nullptr) == 0);
   CHECK(id++, "STR: '!a* | b*' vs 'a' -> false", sge_eval_expression(ocs::CEntry::Type::STR, "!a* | b*", "a", nullptr) == 1);
   CHECK(id++, "STR: '!a* | b*' vs 'b' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "!a* | b*", "b", nullptr) == 0);

   printf("\n--- STR: NAND (!(a&b)) ---\n");
   // "!(a & b)": true unless the value satisfies both a and b simultaneously
   CHECK(id++, "STR: NAND vs 'a' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "!(a & b)",   "a", nullptr) == 0);
   CHECK(id++, "STR: NAND vs 'b' -> true",  sge_eval_expression(ocs::CEntry::Type::STR, "!(a & b)",   "b", nullptr) == 0);
   CHECK(id++, "STR: NAND* vs 'a' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "!(a* & b*)", "a", nullptr) == 0);
   CHECK(id++, "STR: NAND* vs 'b' -> true", sge_eval_expression(ocs::CEntry::Type::STR, "!(a* & b*)", "b", nullptr) == 0);

   printf("\n--- CSTR: regular expressions ---\n");
   // CSTR matches are case-insensitive
   CHECK(id++, "CSTR: 'solaris' vs 'solaris' -> true",                 sge_eval_expression(ocs::CEntry::Type::CSTR, "solaris",  "solaris",  nullptr) == 0);
   CHECK(id++, "CSTR: '!solaris' vs 'solaris' -> false",               sge_eval_expression(ocs::CEntry::Type::CSTR, "!solaris", "solaris",  nullptr) == 1);
   CHECK(id++, "CSTR: '*amd64&sol*' vs 'sol-amd64' -> true",           sge_eval_expression(ocs::CEntry::Type::CSTR, "*amd64&sol*", "sol-amd64", nullptr) == 0);
   CHECK(id++, "CSTR: sol-*64|linux*,!sol-sparc vs 'sol-sparc64' -> true",
         sge_eval_expression(ocs::CEntry::Type::CSTR, "(sol-*64|linux*)&!sol-sparc", "sol-sparc64", nullptr) == 0);
   CHECK(id++, "CSTR: sol-*64|linux*,!sol-sparc vs 'sol-sparc' -> false",
         sge_eval_expression(ocs::CEntry::Type::CSTR, "(sol-*64|linux*)&!sol-sparc", "sol-sparc", nullptr) == 1);
   // complex negated expression
   CHECK(id++, "CSTR: complex negation vs 'sol-sparc' -> true",
         sge_eval_expression(ocs::CEntry::Type::CSTR, "!(sola*|lin*|hp*)&!sola*&!*sparc64&(!sole*|!lin*|!hp*)", "sol-sparc", nullptr) == 0);
   CHECK(id++, "CSTR: triple-nested parens vs 'test' -> true",
         sge_eval_expression(ocs::CEntry::Type::CSTR, "(((test)))", "test", nullptr) == 0);
   CHECK(id++, "CSTR: triple-nested parens with AND vs 'test' -> false",
         sge_eval_expression(ocs::CEntry::Type::CSTR, "(((test)&pet*))", "test", nullptr) == 1);

   printf("\n--- STR/CSTR: error cases ---\n");
   // invalid syntax must return -1; unexpected tokens after valid expression also error
   CHECK(id++, "STR: trailing '!&' after expression -> error",
         sge_eval_expression(ocs::CEntry::Type::STR, "(sol-*64|linux|hp*)&!sol-sparc!&", "sol-sparc", nullptr) == -1);
   // space-separated tokens: parser treats first token only, 'a b c' vs blank returns no-match
   CHECK(id++, "STR: space-separated 'a b c' vs blank -> false",
         sge_eval_expression(ocs::CEntry::Type::STR, "a b c", "      ", nullptr) == 1);
   CHECK(id++, "STR: 'a|b c' -> error (space after valid binary expression)",
         sge_eval_expression(ocs::CEntry::Type::STR, "a|b c", "a", nullptr) == -1);
   CHECK(id++, "STR: 'a&' -> error (missing right operand)",
         sge_eval_expression(ocs::CEntry::Type::STR, "a&", "a", nullptr) == -1);
   CHECK(id++, "STR: 'a|' -> error (missing right operand)",
         sge_eval_expression(ocs::CEntry::Type::STR, "a|", "a", nullptr) == -1);
   CHECK(id++, "STR: 'a&a&' -> error (trailing operator)",
         sge_eval_expression(ocs::CEntry::Type::STR, "a&a&", "a", nullptr) == -1);
   CHECK(id++, "STR: 'a|a|' -> error (trailing operator)",
         sge_eval_expression(ocs::CEntry::Type::STR, "a|a|", "a", nullptr) == -1);
   CHECK(id++, "STR: '(a b c' -> error (unclosed paren)",
         sge_eval_expression(ocs::CEntry::Type::STR, "(a b c", "a", nullptr) == -1);
   CHECK(id++, "STR: 'a)&b' -> error (unexpected closing paren)",
         sge_eval_expression(ocs::CEntry::Type::STR, "a)&b", "a", nullptr) == -1);
   CHECK(id++, "STR: '(a)&b)|c' -> error (unmatched closing paren)",
         sge_eval_expression(ocs::CEntry::Type::STR, "(a)&b)|c", "a", nullptr) == -1);

   printf("\n--- CSTR: case sensitivity ---\n");
   // CSTR folds case: lowercase expr matches uppercase value and vice versa
   CHECK(id++, "CSTR: 'a' vs 'A' -> true",       sge_eval_expression(ocs::CEntry::Type::CSTR, "a",       "A", nullptr) == 0);
   CHECK(id++, "CSTR: 'A' vs 'a' -> true",       sge_eval_expression(ocs::CEntry::Type::CSTR, "A",       "a", nullptr) == 0);
   CHECK(id++, "CSTR: 'a*' vs 'A' -> true",      sge_eval_expression(ocs::CEntry::Type::CSTR, "a*",      "A", nullptr) == 0);
   CHECK(id++, "CSTR: 'A*' vs 'a' -> true",      sge_eval_expression(ocs::CEntry::Type::CSTR, "A*",      "a", nullptr) == 0);
   CHECK(id++, "CSTR: 'a&b|a' vs 'A' -> true",   sge_eval_expression(ocs::CEntry::Type::CSTR, "a&b|a",   "A", nullptr) == 0);
   CHECK(id++, "CSTR: 'A&B|A' vs 'a' -> true",   sge_eval_expression(ocs::CEntry::Type::CSTR, "A&B|A",   "a", nullptr) == 0);
   CHECK(id++, "CSTR: 'a*&b*|a*' vs 'A' -> true", sge_eval_expression(ocs::CEntry::Type::CSTR, "a*&b*|a*", "A", nullptr) == 0);
   CHECK(id++, "CSTR: 'A*&B*|A*' vs 'a' -> true", sge_eval_expression(ocs::CEntry::Type::CSTR, "A*&B*|A*", "a", nullptr) == 0);

   printf("\n--- HOST: hostname matching ---\n");
   // sge_hostcmp() reads the bootstrap file via ocs::Bootstrap; skip if SGE_ROOT is not set
   if (getenv("SGE_ROOT") == nullptr) {
      printf("skipped — SGE_ROOT not set (sge_hostcmp requires bootstrap file)\n");
      id += 4;
   } else {
      // HOST uses sge_hostmatch/sge_hostcmp (case-insensitive, FQDN-aware)
      CHECK(id++, "HOST: 'Latte*' vs 'latte3.czech.sun.com' -> true",
            sge_eval_expression(ocs::CEntry::Type::HOST, "Latte*", "latte3.czech.sun.com", nullptr) == 0);
      CHECK(id++, "HOST: 'latte* & !*3.czech.sun.com' vs 'latte3.czech.sun.com' -> false",
            sge_eval_expression(ocs::CEntry::Type::HOST, "latte* & !*3.czech.sun.com", "latte3.czech.sun.com", nullptr) == 1);
      CHECK(id++, "HOST: 'Latte* | Mocca*' vs 'latte3.czech.sun.com' -> true",
            sge_eval_expression(ocs::CEntry::Type::HOST, "Latte* | Mocca*", "latte3.czech.sun.com", nullptr) == 0);
      // long expression with many alternatives — stress test parser stack depth
      CHECK(id++, "HOST: exhaustive negation vs 'bla' -> false",
            sge_eval_expression(ocs::CEntry::Type::HOST,
               "!(a*|b*|c*|d*|e*|f*|g*|h*|i*|j*|k*|l*|m*|n*|o*|p*|q*|r*|s*|t*|u*|v*|w*|x*|y*|z*|"
               "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
               "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa*)",
               "bla", nullptr) == 1);
   }

   printf("\n--- RESTR: restricted string ---\n");
   // RESTR behaves like STR: case-sensitive strcmp/fnmatch, no case folding
   CHECK(id++, "RESTR: 'a' vs 'a' -> true (exact match)",
         sge_eval_expression(ocs::CEntry::Type::RESTR, "a", "a", nullptr) == 0);
   CHECK(id++, "RESTR: 'a' vs 'A' -> false (case-sensitive, unlike CSTR)",
         sge_eval_expression(ocs::CEntry::Type::RESTR, "a", "A", nullptr) == 1);
   CHECK(id++, "RESTR: 'a*' vs 'abc' -> true (wildcard)",
         sge_eval_expression(ocs::CEntry::Type::RESTR, "a*", "abc", nullptr) == 0);
   CHECK(id++, "RESTR: 'a|b' vs 'b' -> true (OR expression)",
         sge_eval_expression(ocs::CEntry::Type::RESTR, "a|b", "b", nullptr) == 0);
   CHECK(id++, "RESTR: 'a&' vs 'a' -> error (invalid syntax)",
         sge_eval_expression(ocs::CEntry::Type::RESTR, "a&", "a", nullptr) == -1);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
