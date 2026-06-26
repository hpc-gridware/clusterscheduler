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
#include <string>

#include "basis_types.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"

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
// verify_str_key  [T01–T32]
//
// verify_str_key() validates GCS object names against a table-specific rule set.
//
// KEY_TABLE rules:
//   - The string must not be nullptr.
//   - The string must not exceed str_length characters (MAX_VERIFY_STRING=512).
//   - The first character must not be '.' or '#'.
//   - The string must not contain any of: \n \t \r space / : ' " \ [ ] { } | ( ) @ % ,
//   - The string must not match a reserved keyword: NONE, ALL, TEMPLATE (case-insensitive).
//   - The optional 'exceptions' parameter whitelists specific forbidden mid-characters.
//
// QSUB_TABLE rules differ: no forbidden leading characters; '*' and '?' are forbidden;
// space, '%', '|', ',', '"', and parentheses are allowed.
//
// Scenarios T01–T02 cover forbidden leading characters (KEY_TABLE).
// Scenarios T03–T09 cover a representative subset of forbidden mid-characters (KEY_TABLE).
// Scenarios T10–T13 cover reserved keywords including case-insensitive matching.
// Scenario  T14     covers the maximum length limit.
// Scenario  T15     covers null input.
// Scenarios T16–T18 cover valid (accepted) names.
// Scenario  T19     covers the exceptions parameter.
// Scenarios T20–T24 cover the remaining untested forbidden mid-characters (KEY_TABLE).
// Scenario  T25     documents empty-string behavior (accepted — no rule rejects it).
// Scenarios T26–T27 cover mixed-case keyword rejection.
// Scenarios T28–T32 cover QSUB_TABLE-specific rules.
// ---------------------------------------------------------------------------

static void test_verify_str_key() {
   printf("\n--- verify_str_key ---\n");
   lList *al = nullptr;

   // T01: leading '.' is forbidden in KEY_TABLE names
   CHECK(1,  "leading dot rejected",   verify_str_key(&al, ".leadingdot",  MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T02: leading '#' is forbidden in KEY_TABLE names
   CHECK(2,  "leading hash rejected",  verify_str_key(&al, "#leadinghash", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T03: '@' is a forbidden mid-character (used as hostgroup prefix in GCS syntax)
   CHECK(3,  "@ in middle rejected",   verify_str_key(&al, "name@host",    MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T04: '%' is a forbidden mid-character (format string hazard)
   CHECK(4,  "% in middle rejected",   verify_str_key(&al, "bla%sfoo",     MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T05: space is a forbidden mid-character (breaks shell tokenisation)
   CHECK(5,  "space in middle rejected", verify_str_key(&al, "name space",  MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T06: '/' is a forbidden mid-character (path separator)
   CHECK(6,  "/ in middle rejected",   verify_str_key(&al, "path/name",    MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T07: ':' is a forbidden mid-character (queue instance separator)
   CHECK(7,  ": in middle rejected",   verify_str_key(&al, "queue:host",   MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T08: '|' is a forbidden mid-character (shell pipe / alternation syntax)
   CHECK(8,  "| in middle rejected",   verify_str_key(&al, "foo|bar",      MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T09: ',' is a forbidden mid-character (list separator in GCS option strings)
   CHECK(9,  ", in middle rejected",   verify_str_key(&al, "foo,bar",      MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T10: "NONE" is a reserved keyword (exact case)
   CHECK(10, "keyword NONE rejected",     verify_str_key(&al, "NONE",     MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T11: "ALL" is a reserved keyword (exact case)
   CHECK(11, "keyword ALL rejected",      verify_str_key(&al, "ALL",      MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T12: "TEMPLATE" is a reserved keyword (exact case)
   CHECK(12, "keyword TEMPLATE rejected", verify_str_key(&al, "TEMPLATE", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T13: keyword matching is case-insensitive; "none" must also be rejected
   CHECK(13, "keyword none (lowercase) rejected", verify_str_key(&al, "none", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T14: a string longer than MAX_VERIFY_STRING (512) characters is rejected
   std::string too_long(MAX_VERIFY_STRING + 1, 'x');
   CHECK(14, "string > 512 chars rejected", verify_str_key(&al, too_long.c_str(), MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T15: a nullptr string is rejected without crashing
   CHECK(15, "nullptr rejected", verify_str_key(&al, nullptr, MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T16: a plain alphanumeric name with no forbidden characters is accepted
   CHECK(16, "plain name accepted", verify_str_key(&al, "validname123", MAX_VERIFY_STRING, "test", KEY_TABLE) == STATUS_OK);
   lFreeList(&al);

   // T17: "EMPLATE" shares a prefix with the keyword "TEMPLATE" but is not a keyword
   CHECK(17, "EMPLATE (not a keyword) accepted", verify_str_key(&al, "EMPLATE", MAX_VERIFY_STRING, "test", KEY_TABLE) == STATUS_OK);
   lFreeList(&al);

   // T18: a string of exactly MAX_VERIFY_STRING characters is accepted (boundary)
   std::string max_len(MAX_VERIFY_STRING, 'a');
   CHECK(18, "string of exactly 512 chars accepted", verify_str_key(&al, max_len.c_str(), MAX_VERIFY_STRING, "test", KEY_TABLE) == STATUS_OK);
   lFreeList(&al);

   // T19: passing '@' in the exceptions string whitelists it for this call
   CHECK(19, "@ allowed when listed in exceptions", verify_str_key(&al, "name@host", MAX_VERIFY_STRING, "test", KEY_TABLE, "@") == STATUS_OK);
   lFreeList(&al);

   // T20: single quote is a forbidden mid-character
   CHECK(20, "single quote in middle rejected", verify_str_key(&al, "it's", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T21: backslash is a forbidden mid-character (escape sequence hazard)
   CHECK(21, "backslash in middle rejected", verify_str_key(&al, "path\\name", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T22: square bracket is a forbidden mid-character (shell glob syntax)
   CHECK(22, "[ in middle rejected", verify_str_key(&al, "name[0]", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T23: curly brace is a forbidden mid-character (shell expansion syntax)
   CHECK(23, "{ in middle rejected", verify_str_key(&al, "name{a}", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T24: parenthesis is a forbidden mid-character (shell subshell syntax)
   CHECK(24, "( in middle rejected", verify_str_key(&al, "name(x)", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T25: an empty string passes all checks (no rule explicitly rejects it); document current behavior
   CHECK(25, "empty string accepted", verify_str_key(&al, "", MAX_VERIFY_STRING, "test", KEY_TABLE) == STATUS_OK);
   lFreeList(&al);

   // T26: keyword matching is case-insensitive — mixed-case "Template" must be rejected
   CHECK(26, "keyword Template (mixed case) rejected", verify_str_key(&al, "Template", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T27: keyword matching is case-insensitive — mixed-case "All" must be rejected
   CHECK(27, "keyword All (mixed case) rejected", verify_str_key(&al, "All", MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T28: QSUB_TABLE forbids '*' (wildcard would corrupt qsub -q patterns)
   CHECK(28, "QSUB_TABLE: * rejected", verify_str_key(&al, "queue*", MAX_VERIFY_STRING, "test", QSUB_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T29: QSUB_TABLE forbids '?' (wildcard character)
   CHECK(29, "QSUB_TABLE: ? rejected", verify_str_key(&al, "queue?", MAX_VERIFY_STRING, "test", QSUB_TABLE) != STATUS_OK);
   lFreeList(&al);

   // T30: QSUB_TABLE does not forbid space — it appears in qsub option values
   CHECK(30, "QSUB_TABLE: space allowed", verify_str_key(&al, "my queue", MAX_VERIFY_STRING, "test", QSUB_TABLE) == STATUS_OK);
   lFreeList(&al);

   // T31: QSUB_TABLE does not forbid '%' — it appears in job name patterns
   CHECK(31, "QSUB_TABLE: % allowed", verify_str_key(&al, "job%d", MAX_VERIFY_STRING, "test", QSUB_TABLE) == STATUS_OK);
   lFreeList(&al);

   // T32: QSUB_TABLE has no forbidden leading characters — leading '.' is accepted
   CHECK(32, "QSUB_TABLE: leading dot allowed", verify_str_key(&al, ".hidden", MAX_VERIFY_STRING, "test", QSUB_TABLE) == STATUS_OK);
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// verify_host_name  [T33–T42]
//
// SECURITY REGRESSION (CS-2364, LOW-SPOOL-002, CWE-22): host names become spool
// file keys (EXECHOST_DIR/<name>) and are NOT run through verify_str_key(), so
// verify_host_name() must reject names that are unsafe as a single path
// component — a '/' anywhere or a leading '.' (".", "..", "../x", ".hidden").
// Before the fix those were accepted (return true); T35-T40 fail against the
// unpatched verify_host_name().
// ---------------------------------------------------------------------------
static void test_verify_host_name() {
   printf("\n--- verify_host_name ---\n");
   lList *al = nullptr;

   // T33/T34: ordinary names accepted (incl. an FQDN with '-' and '.')
   CHECK(33, "plain hostname accepted",      verify_host_name(&al, "myhost"));
   CHECK(34, "FQDN with '-' and '.' accepted", verify_host_name(&al, "host-01.example.com"));

   // T35-T40: path-traversal / unsafe names rejected
   CHECK(35, "traversal '../../tmp/x' rejected", !verify_host_name(&al, "../../tmp/x"));
   CHECK(36, "absolute '/etc/passwd' rejected",  !verify_host_name(&al, "/etc/passwd"));
   CHECK(37, "'..' rejected",                    !verify_host_name(&al, ".."));
   CHECK(38, "'.' rejected",                     !verify_host_name(&al, "."));
   CHECK(39, "leading-dot '.hidden' rejected",   !verify_host_name(&al, ".hidden"));
   CHECK(40, "embedded slash 'a/b' rejected",    !verify_host_name(&al, "a/b"));

   // T41/T42: nullptr / empty rejected
   CHECK(41, "nullptr rejected",  !verify_host_name(&al, nullptr));
   CHECK(42, "empty rejected",    !verify_host_name(&al, ""));

   lFreeList(&al);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_verify_str_key();
   test_verify_host_name();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
