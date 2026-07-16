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
#include "sgeobj/sge_usage.h"

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
 * Small helper: build a UA_Type element via the R8a helper and inspect it.
 * The R8a helper (usage_parse_value) produces a fresh UA_Type element with
 * UA_name set and either UA_value or UA_svalue populated according to the
 * discrimination rule described in CS-849 / origin AE3.
 *
 * expect_string == true means we expect UA_svalue set and UA_value 0.0
 * (svalue takes precedence). expect_string == false means we expect the
 * opposite: UA_svalue nullptr and UA_value set to expected_double.
 */
static bool
check_element(const char *variable, const char *value,
              bool expect_string, const char *expected_svalue,
              double expected_double) {
   lListElem *ep = usage_parse_value(variable, value);
   if (ep == nullptr) {
      printf("       usage_parse_value returned nullptr for value='%s'\n", value);
      return false;
   }
   const char *name = lGetString(ep, UA_name);
   if (name == nullptr || std::strcmp(name, variable) != 0) {
      printf("       UA_name='%s', expected '%s'\n", name ? name : "(nullptr)", variable);
      lFreeElem(&ep);
      return false;
   }
   const char *svalue = lGetString(ep, UA_svalue);
   double dvalue = lGetDouble(ep, UA_value);
   bool ok = true;
   if (expect_string) {
      if (svalue == nullptr) {
         printf("       expected UA_svalue='%s', got nullptr\n", expected_svalue);
         ok = false;
      } else if (std::strcmp(svalue, expected_svalue) != 0) {
         printf("       expected UA_svalue='%s', got '%s'\n", expected_svalue, svalue);
         ok = false;
      }
      // For string values UA_value is left at 0 (default) — not consulted downstream.
   } else {
      if (svalue != nullptr) {
         printf("       expected UA_svalue nullptr, got '%s'\n", svalue);
         ok = false;
      }
      if (dvalue != expected_double) {
         printf("       expected UA_value=%g, got %g\n", expected_double, dvalue);
         ok = false;
      }
   }
   lFreeElem(&ep);
   return ok;
}

// ---------------------------------------------------------------------------
// AE3 discrimination-rule regression tests.
//
// Every case from origin AE3 (see plan §Acceptance Examples) becomes one
// CHECK() line here. This is the CTest module test the plan U6 execution
// note requires: write RED, implement usage_parse_value() until GREEN.
// ---------------------------------------------------------------------------

int main(int, char *[]) {
   // Plain numeric
   CHECK(1, "42 -> numeric 42",
         check_element("x", "42", false, nullptr, 42.0));

   // Quoted numeric-looking string
   CHECK(2, "\"42\" -> string \"42\"",
         check_element("x", "\"42\"", true, "42", 0.0));

   // Bool true / false (case-insensitive)
   CHECK(3, "true -> numeric 1",
         check_element("x", "true", false, nullptr, 1.0));
   CHECK(4, "True -> numeric 1 (case-insensitive)",
         check_element("x", "True", false, nullptr, 1.0));
   CHECK(5, "FALSE -> numeric 0 (case-insensitive)",
         check_element("x", "FALSE", false, nullptr, 0.0));
   CHECK(6, "\"true\" -> string \"true\" (quoted stays string)",
         check_element("x", "\"true\"", true, "true", 0.0));

   // Plain string (no quotes, doesn't parse as double or bool)
   CHECK(7, "hello world -> string \"hello world\"",
         check_element("x", "hello world", true, "hello world", 0.0));

   // Scientific notation
   CHECK(8, "3.14e5 -> numeric 314000",
         check_element("x", "3.14e5", false, nullptr, 314000.0));
   CHECK(9, "\"3.14e5\" -> string \"3.14e5\"",
         check_element("x", "\"3.14e5\"", true, "3.14e5", 0.0));

   // Empty pair
   CHECK(10, "\"\" -> string of length 0",
         check_element("x", "\"\"", true, "", 0.0));

   // Unmatched opening quote — raw, leading quote kept as part of the string
   CHECK(11, "\"hello (unmatched) -> string \"\\\"hello\"",
         check_element("x", "\"hello", true, "\"hello", 0.0));

   // Mismatched quote types — raw, both kept
   CHECK(12, "\"hello' (mismatched) -> string \"\\\"hello'\"",
         check_element("x", "\"hello'", true, "\"hello'", 0.0));

   // Single-character stray quote
   CHECK(13, "\" (single char) -> string \"\\\"\"",
         check_element("x", "\"", true, "\"", 0.0));

   // Embedded escaped quotes — backslashes preserved literally, no escape processing
   CHECK(14, "\"he said \\\"hi\\\"\" -> string with backslashes preserved",
         check_element("x", "\"he said \\\"hi\\\"\"", true, "he said \\\"hi\\\"", 0.0));

   // Single-quoted string
   CHECK(15, "'hello' -> string \"hello\"",
         check_element("x", "'hello'", true, "hello", 0.0));

   // Zero, negative, decimal
   CHECK(16, "0 -> numeric 0",
         check_element("x", "0", false, nullptr, 0.0));
   CHECK(17, "-1.5 -> numeric -1.5",
         check_element("x", "-1.5", false, nullptr, -1.5));

   if (s_fail == 0) {
      printf("ALL PASS (17 tests)\n");
      return 0;
   }
   printf("FAIL (%d of 17)\n", s_fail);
   return 1;
}
