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
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_module_test.h"

/**
 * @brief Return the text of the first answer-list element.
 * @param[in] answer_list answer list to inspect (may be nullptr/empty)
 * @return the first answer's text, or "" if the list is empty or has no text
 */
static const char *
first_answer_text(lList *answer_list) {
   const lListElem *aep = lFirst(answer_list);
   const char *text = (aep != nullptr) ? lGetString(aep, AN_text) : nullptr;
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
      lList *al = nullptr;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, "06151200");
      CHECK(1, "valid MMDDhhmm date accepted", ok);
      lFreeList(&al);
   }

   // T02: RED->GREEN. An input of exactly sizeof(stringT) bytes must be
   //      rejected by the length guard, *before* the copy. With the off-by-one
   //      it slipped through into the strcpy.
   {
      std::string s(cap, '9'); // exactly 2048 chars
      lList *al = nullptr;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, s.c_str());
      const char *msg = first_answer_text(al);
      CHECK(2, "len==sizeof(stringT) rejected (no overflow)", !ok);
      CHECK(3, "len==sizeof(stringT) rejected by length guard, not format check",
            strstr(msg, "exceeds maximum") != nullptr);
      lFreeList(&al);
   }

   // T04: an over-long input (sizeof+1) is likewise rejected by the guard.
   {
      std::string s(cap + 1, '9');
      lList *al = nullptr;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, s.c_str());
      const char *msg = first_answer_text(al);
      CHECK(4, "len>sizeof(stringT) rejected", !ok);
      CHECK(5, "len>sizeof(stringT) rejected by length guard",
            strstr(msg, "exceeds maximum") != nullptr);
      lFreeList(&al);
   }

   // T06: a string just under the buffer capacity (sizeof-1) must NOT be
   //      treated as "too long" — it fits with its terminator and proceeds to
   //      the format check (which rejects it as a malformed date). Guards
   //      against over-tightening the bound.
   {
      std::string s(cap - 1, '9');
      lList *al = nullptr;
      uint32_t t = 0;
      bool ok = ulong_parse_date_time_from_string(&t, &al, s.c_str());
      const char *msg = first_answer_text(al);
      CHECK(6, "len==sizeof(stringT)-1 not rejected as too long", !ok);
      CHECK(7, "len==sizeof(stringT)-1 reaches format check, not length guard",
            strstr(msg, "exceeds maximum") == nullptr);
      lFreeList(&al);
   }

   // T08: NULL and empty input are rejected without crashing.
   {
      lList *al = nullptr;
      uint32_t t = 0;
      bool ok_null = ulong_parse_date_time_from_string(&t, &al, nullptr);
      bool ok_empty = ulong_parse_date_time_from_string(&t, &al, "");
      CHECK(8, "NULL and empty input rejected", !ok_null && !ok_empty);
      lFreeList(&al);
   }
}

// ---------------------------------------------------------------------------

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame.
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   lInit(nmv);

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
   });
}
