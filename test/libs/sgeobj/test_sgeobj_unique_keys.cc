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
 * @brief Module test of the duplicate keys in unique attributes built from input (CS-3053)
 */

#include <cstring>

#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_var.h"

#include "ocs_module_test.h"

/**
 * @brief Whether the first entry of an answer list contains a text
 * @param answer_list the answer list
 * @param text the text to look for
 * @return true if the list has an entry and its text contains the given one
 */
static bool
answer_contains(const lList *answer_list, const char *text) {
   const char *answer = lGetString(lFirst(answer_list), AN_text);
   return answer != nullptr && strstr(answer, text) != nullptr;
}

/**
 * @brief The central check on flat lists of string, host and ulong keys
 * @return nothing; CHECK records the results
 */
static void
test_flat_lists() {
   lList *answer_list = nullptr;

   lList *vars = nullptr;
   lAddElemStr(&vars, VA_variable, "A", VA_Type);
   lAddElemStr(&vars, VA_variable, "B", VA_Type);
   CHECK(1, "distinct string keys pass", ocs_object_list_verify_unique_keys(vars, &answer_list, "test"));

   lAddElemStr(&vars, VA_variable, "A", VA_Type);
   const bool rejected = !ocs_object_list_verify_unique_keys(vars, &answer_list, "test");
   CHECK(2, "a repeated string key is rejected, naming the key and the attribute",
         rejected && answer_contains(answer_list, "\"A\"") && answer_contains(answer_list, "VA_variable"));
   lFreeList(&answer_list);
   lFreeList(&vars);

   lList *no_keys = lCreateList("no keys", VA_Type);
   lAppendElem(no_keys, lCreateElem(VA_Type));
   lAppendElem(no_keys, lCreateElem(VA_Type));
   CHECK(3, "elements without a key are skipped", ocs_object_list_verify_unique_keys(no_keys, &answer_list, "test"));
   lFreeList(&no_keys);

   lList *hosts = nullptr;
   lAddElemHost(&hosts, HR_name, "h1", HR_Type);
   lAddElemHost(&hosts, HR_name, "H1", HR_Type);
   const bool exact = ocs_object_list_verify_unique_keys(hosts, &answer_list, "test");
   lAddElemHost(&hosts, HR_name, "h1", HR_Type);
   CHECK(4, "host keys compare by exact string, like the hash table of the list",
         exact && !ocs_object_list_verify_unique_keys(hosts, &answer_list, "test"));
   lFreeList(&answer_list);
   lFreeList(&hosts);

   lList *ulongs = nullptr;
   lAddElemUlong(&ulongs, ULNG_value, 1, ULNG_Type);
   lAddElemUlong(&ulongs, ULNG_value, 2, ULNG_Type);
   const bool distinct = ocs_object_list_verify_unique_keys(ulongs, &answer_list, "test");
   lAddElemUlong(&ulongs, ULNG_value, 1, ULNG_Type);
   CHECK(5, "a repeated ulong key is rejected",
         distinct && !ocs_object_list_verify_unique_keys(ulongs, &answer_list, "test"));
   lFreeList(&answer_list);
   lFreeList(&ulongs);
}

/**
 * @brief The central check walks sub-lists of an object
 * @return nothing; CHECK records the results
 */
static void
test_nested() {
   lList *answer_list = nullptr;
   lListElem *job = lCreateElem(JB_Type);
   lList *env = nullptr;
   lAddElemStr(&env, VA_variable, "A", VA_Type);
   lAddElemStr(&env, VA_variable, "B", VA_Type);
   lSetList(job, JB_env_list, env);
   CHECK(6, "a job with distinct variables passes", ocs_object_verify_unique_keys(job, &answer_list, "job 1"));

   lList *job_env = lGetListRW(job, JB_env_list);
   lAddElemStr(&job_env, VA_variable, "B", VA_Type);
   const bool rejected = !ocs_object_verify_unique_keys(job, &answer_list, "job 1");
   CHECK(7, "a repeated variable in the job's environment is rejected, naming the job",
         rejected && answer_contains(answer_list, "job 1"));
   lFreeList(&answer_list);
   lFreeElem(&job);
}

/**
 * @brief The parsers keep a repeated variable once, with its last value
 * @return nothing; CHECK records the results
 */
static void
test_variable_parser() {
   lList *vars = nullptr;
   const int ret = var_list_parse_from_string(&vars, "A=1,B=2,A=3", 0);
   const lListElem *a = lGetElemStr(vars, VA_variable, "A");
   CHECK(8, "-v A=1,B=2,A=3 holds A once, with the value 3",
         ret == 0 && lGetNumberOfElem(vars) == 2 && a != nullptr && strcmp(lGetString(a, VA_value), "3") == 0);

   const int ret2 = var_list_parse_from_string(&vars, "A=4", 0);
   a = lGetElemStr(vars, VA_variable, "A");
   CHECK(9, "a later -v adds to the same list by name",
         ret2 == 0 && lGetNumberOfElem(vars) == 2 && strcmp(lGetString(a, VA_value), "4") == 0);
   lFreeList(&vars);

   const int ret3 = var_list_parse_from_string(&vars, "A,A", 0);
   CHECK(10, "a name without a value, given twice, is kept once without a value",
         ret3 == 0 && lGetNumberOfElem(vars) == 1 && lGetString(lFirst(vars), VA_value) == nullptr);
   lFreeList(&vars);
}

/**
 * @brief Set-like lists keep a repeated name once
 * @return nothing; CHECK records the results
 */
static void
test_set_lists() {
   lList *qrefs = nullptr;
   const int ret = cull_parse_destination_identifier_list(&qrefs, "a.q,b.q,a.q");
   CHECK(11, "-q a.q,b.q,a.q holds a.q once, in its first position",
         ret == 0 && lGetNumberOfElem(qrefs) == 2 && strcmp(lGetString(lFirst(qrefs), QR_name), "a.q") == 0);
   qref_list_add(&qrefs, nullptr, "b.q");
   CHECK(12, "adding a reference the list holds changes nothing", lGetNumberOfElem(qrefs) == 2);
   lFreeList(&qrefs);

   lList *acl = nullptr;
   lAddElemStr(&acl, ARA_name, "u1", ARA_Type);
   lAddElemStr(&acl, ARA_name, "u2", ARA_Type);
   lAddElemStr(&acl, ARA_name, "u1", ARA_Type);
   ocs_object_list_remove_repeated_keys(acl, ARA_name);
   CHECK(13, "repeated names are removed, the first of each stays",
         lGetNumberOfElem(acl) == 2 && strcmp(lGetString(lLast(acl), ARA_name), "u2") == 0);
   lFreeList(&acl);

   lList *ulongs = nullptr;
   lList *answer_list = nullptr;
   const bool parsed = ulong_list_parse_from_string(&ulongs, &answer_list, "1,2,1", ",");
   CHECK(14, "a ulong list given 1,2,1 holds 1 once", parsed && lGetNumberOfElem(ulongs) == 2);
   lFreeList(&ulongs);
   lFreeList(&answer_list);
}

/**
 * @brief Run the scenarios of the duplicate keys in unique attributes
 * @param argc number of arguments
 * @param argv the arguments; the module test frame takes its own options from them
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   lInit(nmv);
   return ocs::module_test::run(argc, argv, {
      {"UNQ-01", "flat lists",
       "the central check finds a repeated key in a unique attribute of a list",
       {"check lists of variables, host references and ulong values, with and without a repeated key",
        "check a list whose elements have no key"},
       "a repeated key is rejected with the key and the attribute named; keys without a value are skipped",
       {{"CLN-03", "covers"}}, {1, 2, 3, 4, 5}, test_flat_lists},
      {"UNQ-02", "nested",
       "the central check walks the sub-lists of an object",
       {"check a job whose environment holds a variable twice"},
       "the job is rejected and the message names it",
       {{"CLN-03", "covers"}}, {6, 7}, test_nested},
      {"UNQ-03", "variables",
       "a variable given twice is kept once, with its last value",
       {"parse -v A=1,B=2,A=3, then A=4 into the same list",
        "parse a name without a value twice"},
       "A is held once with the last value; a name without a value stays without one",
       {{"CLN-03", "covers"}}, {8, 9, 10}, test_variable_parser},
      {"UNQ-04", "sets",
       "queue references, access lists and ulong lists keep a repeated name once",
       {"parse -q a.q,b.q,a.q and add b.q again",
        "remove repeated names from an access list",
        "parse the ulong list 1,2,1"},
       "each name is held once, in its first position",
       {{"CLN-03", "covers"}}, {11, 12, 13, 14}, test_set_lists},
   });
}
