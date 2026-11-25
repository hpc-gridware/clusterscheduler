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
 * @brief Unit tests for ulong in `libs/sgeobj`
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "basis_types.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ulong.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

/**
 * @brief Return the text of the first answer-list element.
 * @param[in] answer_list answer list to inspect (may be nullptr/empty)
 * @return the first answer's text, or "" if the list is empty or has no text
 */
static const char *
first_answer_text(const ocs::tom::ConstList<ocs::tom::AN> answer_list) {
   const auto aep = answer_list.front();
   const char *text = aep ? aep.get<AN_text>() : nullptr;
   return (text != nullptr) ? text : "";
}

// ---------------------------------------------------------------------------

/**
 * @brief Regression tests for ulong_parse_date_time_from_string() [T01–T08].
 *
 * ulong_parse_date_time_from_string() parses an attacker-controlled
 * [[CC]YY]MMDDhhmm[.SS] date/time string (qsub/qalter -a, qacct -b/-e, DRMAA)
 * into seconds since the epoch. The string is copied into a fixed-size stack
 * buffer of sizeof(stringT) (= MAX_STRING_SIZE = 2048) bytes.
 *
 * SECURITY REGRESSION (CS-2349, MEDIUM-SGEOBJ-001, CWE-193/CWE-787): the length
 * guard used to be 'strlen(string) > sizeof(stringT)', which let a string of
 * length exactly 2048 through, after which strcpy() wrote 2049 bytes (data + NUL)
 * into the 2048-byte buffer — a one-byte out-of-bounds stack write. The fix
 * rejects 'strlen(string) >= sizeof(stringT)'.
 *
 * The single-NUL overflow is not deterministically observable, but the boundary
 * fix has a clean behavioural discriminator: an input of exactly 2048 bytes is
 * now rejected by the length guard ("Starttime ... exceeds maximum"), whereas
 * the buggy code passed the guard, overflowed, and only later rejected the value
 * as a malformed date ("Invalid format of date/hour-minute field"). T03
 * therefore fails against the unpatched code.
 *
 * @return nothing; CHECK records the results
 */
static void
test_parse_date_time() {
   const size_t cap = sizeof(stringT); // 2048

   // T01: a well-formed MMDDhhmm value parses successfully.
   {
      ocs::tom::ListPtr<ocs::tom::AN> al;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, "06151200");
      CHECK(1, "valid MMDDhhmm date accepted", ok);
      al.reset();
   }

   // T02: RED->GREEN. An input of exactly sizeof(stringT) bytes must be
   //      rejected by the length guard, *before* the copy. With the off-by-one
   //      it slipped through into the strcpy.
   {
      std::string s(cap, '9'); // exactly 2048 chars
      ocs::tom::ListPtr<ocs::tom::AN> al;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, s.c_str());
      const char *msg = first_answer_text(al);
      CHECK(2, "len==sizeof(stringT) rejected (no overflow)", !ok);
      CHECK(3, "len==sizeof(stringT) rejected by length guard, not format check",
            strstr(msg, "exceeds maximum") != nullptr);
      al.reset();
   }

   // T04: an over-long input (sizeof+1) is likewise rejected by the guard.
   {
      std::string s(cap + 1, '9');
      ocs::tom::ListPtr<ocs::tom::AN> al;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, s.c_str());
      const char *msg = first_answer_text(al);
      CHECK(4, "len>sizeof(stringT) rejected", !ok);
      CHECK(5, "len>sizeof(stringT) rejected by length guard",
            strstr(msg, "exceeds maximum") != nullptr);
      al.reset();
   }

   // T06: a string just under the buffer capacity (sizeof-1) must NOT be
   //      treated as "too long" — it fits with its terminator and proceeds to
   //      the format check (which rejects it as a malformed date). Guards
   //      against over-tightening the bound.
   {
      std::string s(cap - 1, '9');
      ocs::tom::ListPtr<ocs::tom::AN> al;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, s.c_str());
      const char *msg = first_answer_text(al);
      CHECK(6, "len==sizeof(stringT)-1 not rejected as too long", !ok);
      CHECK(7, "len==sizeof(stringT)-1 reaches format check, not length guard",
            strstr(msg, "exceeds maximum") == nullptr);
      al.reset();
   }

   // T08: NULL and empty input are rejected without crashing.
   {
      ocs::tom::ListPtr<ocs::tom::AN> al;
      uint32_t t = 0;
      bool ok_null = ulong_parse_date_time_from_string(&t, &al, nullptr);
      bool ok_empty = ulong_parse_date_time_from_string(&t, &al, "");
      CHECK(8, "NULL and empty input rejected", !ok_null && !ok_empty);
      al.reset();
   }
}

// ---------------------------------------------------------------------------

/**
 * @brief Priority and binding amount reject a value that is not a number [T09–T20] (CS-3143)
 *
 * ulong_parse_priority() and ulong_parse_binding_amount() compared the end pointer of strtol() with the
 * address of the output variable instead of with the input, so a text without digits parsed as 0 and
 * `qsub -p abc` was accepted with priority 0. Trailing characters were accepted as well.
 *
 * @return nothing; CHECK records the results
 */
static void
test_parse_priority_and_binding_amount() {
   /**
    * @brief Parse a priority
    * @param text the text
    * @param value receives the parsed value
    * @return true when it was accepted without an error answer, false when it was rejected with one
    */
   auto priority = [](const char *text, int &value) {
      ocs::tom::ListPtr<ocs::tom::AN> al;
      const bool ok = ulong_parse_priority(&al, &value, text);
      return ok && al == nullptr;
   };
   /**
    * @brief Parse a binding amount
    * @param text the text
    * @param value receives the parsed value
    * @return true when it was accepted without an error answer, false otherwise
    */
   auto amount = [](const char *text, int &value) {
      ocs::tom::ListPtr<ocs::tom::AN> al;
      const bool ok = ulong_parse_binding_amount(&al, &value, text);
      return ok && al == nullptr;
   };

   int value = 99;
   CHECK(9, "priority 10 is accepted", priority("10", value) && value == 10);
   CHECK(10, "priority -1023 is accepted", priority("-1023", value) && value == -1023);
   CHECK(11, "priority 0 is accepted", priority("0", value) && value == 0);
   CHECK(12, "priority abc is rejected", !priority("abc", value));
   CHECK(13, "an empty priority is rejected", !priority("", value));
   CHECK(14, "priority 10abc is rejected", !priority("10abc", value));
   CHECK(15, "priority 1025 is rejected", !priority("1025", value));

   CHECK(16, "binding amount 4 is accepted", amount("4", value) && value == 4);
   CHECK(17, "binding amount abc is rejected", !amount("abc", value));
   CHECK(18, "an empty binding amount is rejected", !amount("", value));
   CHECK(19, "binding amount 4x is rejected", !amount("4x", value));
   CHECK(20, "binding amount -1 is rejected", !amount("-1", value));
}

// ---------------------------------------------------------------------------

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame.
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   return ocs::module_test::run(argc, argv, {
      {"ULONG-01", "date/time parsing at the buffer boundary (CS-2349)",
       "ulong_parse_date_time_from_string() rejects an input of sizeof(stringT) bytes or more by its "
       "length guard, before the copy, and still parses a valid date",
       {"parse a valid MMDDhhmm date",
        "parse an input of exactly sizeof(stringT) bytes and one of sizeof(stringT)+1 bytes",
        "parse an input of sizeof(stringT)-1 bytes",
        "parse nullptr and an empty string"},
       "the valid date is accepted, both over-long inputs are rejected by the length guard, the "
       "input that fits reaches the format check, nullptr and the empty string are rejected",
       {}, {1, 2, 3, 4, 5, 6, 7, 8}, test_parse_date_time},
      {"ULONG-02", "priority and binding amount that are not a number (CS-3143)",
       "ulong_parse_priority() and ulong_parse_binding_amount() reject a text that is not a whole number, "
       "also one with trailing characters, instead of taking it as 0",
       {"parse valid priorities and binding amounts",
        "parse abc, an empty text and a number with trailing characters",
        "parse values out of range"},
       "the valid values are accepted, everything else is rejected with an error answer",
       {}, {9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}, test_parse_priority_and_binding_amount},
   });
}
