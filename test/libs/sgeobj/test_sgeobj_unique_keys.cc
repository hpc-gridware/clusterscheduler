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
 * @brief Module test of the duplicate keys in unique attributes built from input (CS-3053, CS-3058)
 */

#include <cstdlib>
#include <cstring>
#include <string>

#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_var.h"
#include "tom/ocs_ElementCore.h"

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
   // builds lists with a repeated key on purpose, as input for the check; a debug build refuses them already at
   // the append (N-H-18), so the check is suspended here
   const ocs::tom::UniqueCheckSuspension suspension;
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
   lAddElemHost(&hosts, HR_name, "h2", HR_Type);
   const bool distinct_hosts = ocs_object_list_verify_unique_keys(hosts, &answer_list, "test");
   lAddElemHost(&hosts, HR_name, "H1", HR_Type);
   CHECK(4, "host keys compare like the hash table of the list: H1 repeats h1, named as it is given",
         distinct_hosts && !ocs_object_list_verify_unique_keys(hosts, &answer_list, "test") &&
         answer_contains(answer_list, "\"H1\""));
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
   // builds lists with a repeated key on purpose, as input for the check; a debug build refuses them already at
   // the append (N-H-18), so the check is suspended here
   const ocs::tom::UniqueCheckSuspension suspension;
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
   {
      // a list with a repeated name on purpose, as input for the removal; suspended as above
      const ocs::tom::UniqueCheckSuspension suspension;
      lAddElemStr(&acl, ARA_name, "u1", ARA_Type);
      lAddElemStr(&acl, ARA_name, "u2", ARA_Type);
      lAddElemStr(&acl, ARA_name, "u1", ARA_Type);
      ocs_object_list_remove_repeated_keys(acl, ARA_name);
   }
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
 * @brief Append one context switch to a request, built the way the parser builds it
 * @param[in,out] change the context change request
 * @param marker the marker of the switch: "+" for -ac, "-" for -dc, "=" for -sc
 * @param vars the variables of the switch, "name[=value],..."
 * @return nothing
 */
static void
add_context_switch(lList **change, const char *marker, const char *vars) {
   lList *option = nullptr;
   var_list_parse_from_string(&option, vars, 0);
   lListElem *ep = lCreateElem(VA_Type);
   lSetString(ep, VA_variable, marker);
   lInsertElem(option, nullptr, ep);
   ocs_context_change_add(change, option);
   lFreeList(&option);
}

/**
 * @brief Whether a job's context holds a variable with a value
 * @param job the job
 * @param name the variable
 * @param value the expected value
 * @return true if the variable is in JB_context with that value
 */
static bool
context_has(const lListElem *job, const char *name, const char *value) {
   const lListElem *var = lGetSubStr(job, VA_variable, name, JB_context);
   return var != nullptr && lGetString(var, VA_value) != nullptr && strcmp(lGetString(var, VA_value), value) == 0;
}

/**
 * @brief The instructions of -ac, -sc and -dc repeat markers and names without a unique key
 * @return nothing; CHECK records the results
 */
static void
test_context_change() {
   // -ac a=1 -ac b=2 -dc a -ac a=3
   lList *change = nullptr;
   add_context_switch(&change, "+", "a=1");
   add_context_switch(&change, "+", "b=2");
   add_context_switch(&change, "-", "a");
   add_context_switch(&change, "+", "a=3");
   const lListElem *a = lGetElemStr(change, CX_variable, "a");
   CHECK(15, "the request holds four markers and a three times; a lookup of a finds an element",
         lGetNumberOfElem(change) == 8 && a != nullptr);

   lListElem *job = lCreateElem(JB_Type);
   lSetList(job, JB_context_change, change);
   lList *answer_list = nullptr;
   CHECK(16, "the central check accepts a job holding the request",
         ocs_object_verify_unique_keys(job, &answer_list, "job 1"));
   lFreeList(&answer_list);

   set_context(lGetList(job, JB_context_change), job);
   CHECK(17, "the request yields the context b=2,a=3",
         lGetNumberOfElem(lGetList(job, JB_context)) == 2 && context_has(job, "b", "2") && context_has(job, "a", "3"));

   // the context loses its last variable and gets a new one
   lList *readd = nullptr;
   add_context_switch(&readd, "=", "a=1");
   add_context_switch(&readd, "-", "a");
   add_context_switch(&readd, "+", "c=1");
   set_context(readd, job);
   CHECK(18, "a variable added after the context became empty is kept",
         lGetNumberOfElem(lGetList(job, JB_context)) == 1 && context_has(job, "c", "1"));
   lFreeList(&readd);

   // above the hash threshold the markers and a repeated name are hashed without a unique key
   lList *big = nullptr;
   for (int i = 0; i < 20; i++) {
      const std::string var = "x" + std::to_string(i) + "=" + std::to_string(i);
      add_context_switch(&big, "+", var.c_str());
   }
   add_context_switch(&big, "-", "x0");
   add_context_switch(&big, "+", "x0=new");
   const bool found = lGetElemStr(big, CX_variable, "+") != nullptr && lGetElemStr(big, CX_variable, "x0") != nullptr;
   set_context(big, job);
   CHECK(19, "above the hash threshold the lookups work; c and the 20 added variables remain, x0 with its new value",
         lGetNumberOfElem(big) > OCS_CULL_HASH_MIN_ELEMS && found &&
         lGetNumberOfElem(lGetList(job, JB_context)) == 21 && context_has(job, "x0", "new"));
   lFreeList(&big);

   // a request without a marker replaces the context
   lList *plain = nullptr;
   lList *vars = nullptr;
   var_list_parse_from_string(&vars, "d=4", 0);
   ocs_context_change_add(&plain, vars);
   lFreeList(&vars);
   set_context(plain, job);
   CHECK(20, "a request without a marker replaces the context",
         lGetNumberOfElem(lGetList(job, JB_context)) == 1 && context_has(job, "d", "4"));
   lFreeList(&plain);
   lFreeElem(&job);

   // qsh adds PARENT after the switches; a trailing -dc must not delete it
   lListElem *child = lCreateElem(JB_Type);
   lList *child_change = nullptr;
   add_context_switch(&child_change, "+", "e=5");
   add_context_switch(&child_change, "-", "e");
   lSetList(child, JB_context_change, child_change);
   setenv("JOB_ID", "42", 1);
   job_add_parent_id_to_context(child);
   unsetenv("JOB_ID");
   lList *effective = ocs_context_get_effective(child);
   const lListElem *parent = lGetElemStr(effective, VA_variable, CONTEXT_PARENT);
   CHECK(21, "PARENT given after a trailing -dc is in the effective context",
         lGetNumberOfElem(effective) == 1 && parent != nullptr && strcmp(lGetString(parent, VA_value), "42") == 0);
   lFreeList(&effective);
   lFreeElem(&child);

   // qstat -j works on a reduced job that does not contain JB_context_change
   lList *jobs = lCreateList("jobs", JB_Type);
   lListElem *full = lAddElemUlong(&jobs, JB_job_number, 1, JB_Type);
   lList *context = nullptr;
   lAddElemStr(&context, VA_variable, "f", VA_Type);
   lSetList(full, JB_context, context);
   lEnumeration *what = lWhat("%T(%I %I)", JB_Type, JB_job_number, JB_context);
   lList *reduced = lSelect("reduced", jobs, nullptr, what);
   lList *reduced_effective = ocs_context_get_effective(lFirst(reduced));
   CHECK(22, "the effective context of a reduced job without the change request is its context",
         lGetNumberOfElem(reduced_effective) == 1 && lGetElemStr(reduced_effective, VA_variable, "f") != nullptr);
   lFreeList(&reduced_effective);
   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&jobs);
}

/**
 * @brief Add a variable to a list of variables
 * @param lpp the list, created if nullptr
 * @param name the name
 * @param value the value
 */
static void
add_variable(lList **lpp, const char *name, const char *value) {
   lListElem *var = lAddElemStr(lpp, VA_variable, name, VA_Type);
   lSetString(var, VA_value, value);
}

/**
 * @brief The repair of repeated keys in a spooled object, as an earlier version wrote it (N-H-18)
 * @return nothing; CHECK records the results
 */
static void
test_repair() {
   // builds lists with a repeated key on purpose, as input for the check; a debug build refuses them already at
   // the append (N-H-18), so the check is suspended here
   const ocs::tom::UniqueCheckSuspension suspension;
   lListElem *job = lCreateElem(JB_Type);
   lSetUlong(job, JB_job_number, 7);

   lList *env = nullptr;
   add_variable(&env, "A", "1");
   add_variable(&env, "B", "2");
   add_variable(&env, "A", "3");
   lSetList(job, JB_env_list, env);

   lListElem *request_set = lAddSubUlong(job, JRS_scope, 0, JB_request_set_list, JRS_Type);
   lList *queues = nullptr;
   lAddElemStr(&queues, QR_name, "all.q", QR_Type);
   lAddElemStr(&queues, QR_name, "b.q", QR_Type);
   lAddElemStr(&queues, QR_name, "all.q", QR_Type);
   lSetList(request_set, JRS_hard_queue_list, queues);
   lList *resources = nullptr;
   lSetString(lAddElemStr(&resources, CE_name, "h_vmem", CE_Type), CE_stringval, "1G");
   lSetString(lAddElemStr(&resources, CE_name, "h_vmem", CE_Type), CE_stringval, "2G");
   lSetList(request_set, JRS_hard_resource_list, resources);

   std::string repairs;
   lList *answer_list = nullptr;
   const bool repaired = ocs_object_repair_repeated_keys(job, repairs, &answer_list, "job 7");
   const lListElem *a = lGetElemStr(lGetList(job, JB_env_list), VA_variable, "A");
   const lList *repaired_queues = lGetList(request_set, JRS_hard_queue_list);
   const lList *repaired_resources = lGetList(request_set, JRS_hard_resource_list);
   CHECK(23, "variables and resources keep the last value, a repeated queue is dropped, the first stays",
         repaired && answer_list == nullptr && lGetNumberOfElem(lGetList(job, JB_env_list)) == 2 && a != nullptr &&
         strcmp(lGetString(a, VA_value), "3") == 0 && lGetNumberOfElem(repaired_queues) == 2 &&
         strcmp(lGetString(lFirst(repaired_queues), QR_name), "all.q") == 0 &&
         lGetNumberOfElem(repaired_resources) == 1 &&
         strcmp(lGetString(lFirst(repaired_resources), CE_stringval), "2G") == 0);
   CHECK(24, "the repairs name the path of each list and the key",
         repairs.find("JB_env_list \"A\"") != std::string::npos &&
         repairs.find("JB_request_set_list/JRS_hard_queue_list \"all.q\"") != std::string::npos &&
         repairs.find("JB_request_set_list/JRS_hard_resource_list \"h_vmem\"") != std::string::npos);

   repairs.clear();
   CHECK(25, "a repaired object has nothing left to repair",
         ocs_object_repair_repeated_keys(job, repairs, &answer_list, "job 7") && repairs.empty());

   lListElem *userset = lCreateElem(US_Type);
   lSetString(userset, US_name, "staff");
   lAddSubStr(userset, UE_name, "u1", US_entries, UE_Type);
   lAddSubStr(userset, UE_name, "u2", US_entries, UE_Type);
   lAddSubStr(userset, UE_name, "u1", US_entries, UE_Type);
   CHECK(26, "a user set keeps a repeated user once",
         ocs_object_repair_repeated_keys(userset, repairs, &answer_list, "userset staff") &&
         lGetNumberOfElem(lGetList(userset, US_entries)) == 2 && repairs == "US_entries \"u1\"");

   lAddSubUlong(job, JAT_task_number, 1, JB_ja_tasks, JAT_Type);
   lAddSubUlong(job, JAT_task_number, 1, JB_ja_tasks, JAT_Type);
   repairs.clear();
   CHECK(27, "a repeated array task is not repaired; the error names the job, the list and the key",
         !ocs_object_repair_repeated_keys(job, repairs, &answer_list, "job 7") && repairs.empty() &&
         lGetNumberOfElem(lGetList(job, JB_ja_tasks)) == 2 && answer_contains(answer_list, "job 7, JB_ja_tasks") &&
         answer_contains(answer_list, "JAT_task_number"));

   lFreeList(&answer_list);
   lFreeElem(&userset);
   lFreeElem(&job);
}

/**
 * @brief Build a list of one string attribute from names
 * @param names the names, nullptr terminated
 * @param nm the attribute
 * @param descr the type of the list
 * @return the list
 */
static lList *
names_list(const char *const names[], const int nm, const lDescr *descr) {
   lList *lp = nullptr;
   for (int i = 0; names[i] != nullptr; i++) {
      lAddElemStr(&lp, nm, names[i], descr);
   }
   return lp;
}

/**
 * @brief The entries of several switches are merged without ever holding a key twice (N-H-18)
 *
 * Runs with the debug check armed: a merge that held a key twice for a moment aborts the test.
 *
 * @return nothing; CHECK records the results
 */
static void
test_merge() {
   const char *const first_queues[] = {"a.q", "b.q", nullptr};
   const char *const more_queues[] = {"c.q", "a.q", nullptr};
   const char *const last_queues[] = {"c.q", nullptr};
   lList *queues = names_list(first_queues, QR_name, QR_Type);
   lList *more = names_list(more_queues, QR_name, QR_Type);
   lList *last = names_list(last_queues, QR_name, QR_Type);
   ocs_object_list_add_unique(&queues, &more);
   ocs_object_list_add_unique(&queues, &last);
   CHECK(28, "-q a.q,b.q -q c.q,a.q -q c.q holds a.q, b.q and c.q once, in their first positions",
         more == nullptr && last == nullptr && lGetNumberOfElem(queues) == 3 &&
         strcmp(lGetString(lFirst(queues), QR_name), "a.q") == 0 &&
         strcmp(lGetString(lLast(queues), QR_name), "c.q") == 0);
   lFreeList(&queues);

   lList *resources = nullptr;
   lList *switch_1 = nullptr;
   lSetString(lAddElemStr(&switch_1, CE_name, "h_vmem", CE_Type), CE_stringval, "1G");
   lSetString(lAddElemStr(&switch_1, CE_name, "h_rt", CE_Type), CE_stringval, "60");
   lList *switch_2 = nullptr;
   lSetString(lAddElemStr(&switch_2, CE_name, "h_vmem", CE_Type), CE_stringval, "2G");
   ocs_object_list_add_unique(&resources, &switch_1);
   ocs_object_list_add_unique(&resources, &switch_2);
   const lListElem *h_vmem = lGetElemStr(resources, CE_name, "h_vmem");
   CHECK(29, "-l h_vmem=1G,h_rt=60 -l h_vmem=2G holds h_vmem once, with 2G",
         lGetNumberOfElem(resources) == 2 && h_vmem != nullptr && strcmp(lGetString(h_vmem, CE_stringval), "2G") == 0);
   lFreeList(&resources);

   lList *env = nullptr;
   lList *vars = nullptr;
   add_variable(&vars, "A", "1");
   ocs_object_list_add_unique(&env, &vars);
   add_variable(&vars, "A", "2");
   ocs_object_list_add_unique(&env, &vars);
   CHECK(30, "a variable merged twice keeps its last value",
         lGetNumberOfElem(env) == 1 && strcmp(lGetString(lFirst(env), VA_value), "2") == 0);
   lFreeList(&env);

   const char *const acl_names[] = {"u1", "u2", nullptr};
   const char *const more_names[] = {"u1", nullptr};
   lList *acl = names_list(acl_names, ARA_name, ARA_Type);
   lList *more_acl = names_list(more_names, ARA_name, ARA_Type);
   ocs_object_list_add_unique(&acl, &more_acl);
   CHECK(31, "an access list merged with a name it holds keeps it once", lGetNumberOfElem(acl) == 2);
   lFreeList(&acl);

   lList *ids = nullptr;
   lList *more_ids = nullptr;
   lAddElemUlong(&ids, ULNG_value, 1, ULNG_Type);
   lAddElemUlong(&more_ids, ULNG_value, 2, ULNG_Type);
   ocs_object_list_add_unique(&ids, &more_ids);
   CHECK(32, "a list without a unique string attribute is appended as it is", lGetNumberOfElem(ids) == 2);
   lFreeList(&ids);
}

/**
 * @brief A resource request holding one value of the string attribute arch, by its full name or its shortcut
 * @param lpp the request list, created if needed
 * @param name arch or a
 * @param value the requested value
 */
static void
add_arch_request(lList **lpp, const char *name, const char *value) {
   lSetString(lAddElemStr(lpp, CE_name, name, CE_Type), CE_stringval, value);
}

/**
 * @brief The value of the only arch request of a list, if it holds exactly one request under its full name
 * @param lp the request list
 * @return the requested value, or nullptr if the list does not hold exactly one request named arch
 */
static const char *
only_arch_value(const lList *lp) {
   if (lGetNumberOfElem(lp) != 1 || strcmp(lGetString(lFirst(lp), CE_name), "arch") != 0) {
      return nullptr;
   }
   return lGetString(lFirst(lp), CE_stringval);
}

/**
 * @brief Code paths the inventory run of N-H-18 found holding a key twice, each run with the debug check armed
 *
 * A resource requested by its shortcut and by its full name, the targets of qconf -mattr naming two instances of
 * one cluster queue, and a limit enforced twice on the same task.
 *
 * @return nothing; CHECK records the results
 */
static void
test_inventory() {
   lList *master_centry_list = nullptr;
   lListElem *arch = lAddElemStr(&master_centry_list, CE_name, "arch", CE_Type);
   lSetString(arch, CE_shortcut, "a");
   lSetUlong(arch, CE_valtype, static_cast<uint32_t>(ocs::CEntry::Type::STR));
   lSetUlong(arch, CE_relop, CMPLXEQ_OP);
   lSetUlong(arch, CE_requestable, REQU_YES);
   lSetUlong(arch, CE_consumable, CONSUMABLE_NO);

   lList *request = nullptr;
   add_arch_request(&request, "a", "lx-amd64");
   add_arch_request(&request, "arch", "lx-arm64");
   lList *answers = nullptr;
   int ret = centry_list_fill_request(request, &answers, master_centry_list, false, true, false);
   const char *value = only_arch_value(request);
   CHECK(33, "-l a=lx-amd64,arch=lx-arm64 holds arch once, with the later value",
         ret == 0 && value != nullptr && strcmp(value, "lx-arm64") == 0);
   lFreeList(&request);
   lFreeList(&answers);

   add_arch_request(&request, "arch", "lx-amd64");
   add_arch_request(&request, "a", "lx-arm64");
   ret = centry_list_fill_request(request, &answers, master_centry_list, false, true, false);
   value = only_arch_value(request);
   CHECK(34, "-l arch=lx-amd64,a=lx-arm64 holds arch once, with the later value",
         ret == 0 && value != nullptr && strcmp(value, "lx-arm64") == 0);
   lFreeList(&request);
   lFreeList(&answers);
   lFreeList(&master_centry_list);

   // qconf -mattr queue slots 4 all.q@h1 all.q@h2 all.q: one request per target, each with a default entry
   lList *targets = nullptr;
   for (const char *target : {"all.q@h1", "all.q@h2", "all.q"}) {
      lListElem *cqueue = lAddElemStr(&targets, CQ_name, target, CQ_Type);
      lListElem *slots = lAddSubHost(cqueue, AULNG_href, HOSTREF_DEFAULT, CQ_job_slots, AULNG_Type);
      lSetUlong(slots, AULNG_value, 4);
   }
   bool ok = cqueue_xattr_pre_gdi(targets, &answers);
   const lList *slots = lGetNumberOfElem(targets) == 1 ? lGetList(lFirst(targets), CQ_job_slots) : nullptr;
   CHECK(35, "the targets all.q@h1, all.q@h2 and all.q become one request for all.q with h1, h2 and the default",
         ok && slots != nullptr && strcmp(lGetString(lFirst(targets), CQ_name), "all.q") == 0 &&
         lGetNumberOfElem(slots) == 3 && lGetElemHost(slots, AULNG_href, "h1") != nullptr &&
         lGetElemHost(slots, AULNG_href, "h2") != nullptr &&
         lGetElemHost(slots, AULNG_href, HOSTREF_DEFAULT) != nullptr);
   lFreeList(&targets);
   lFreeList(&answers);

   // the qmaster enforces a limit on a task, and then again on the same task
   lListElem *job = lCreateElem(JB_Type);
   lSetUlong(job, JB_job_number, 1);
   lSetUlong64(job, JB_submission_time, 1000);
   lListElem *ja_task = lCreateElem(JAT_Type);
   lSetUlong(ja_task, JAT_task_number, 1);
   lSetUlong64(ja_task, JAT_start_time, 2000);
   for (uint64_t now : {3000, 4000}) {
      lListElem *report = lCreateElem(JR_Type);
      job_report_init_from_job_with_usage(report, job, ja_task, nullptr, now);
      lFreeElem(&report);
   }
   const lList *usage = lGetList(ja_task, JAT_scaled_usage_list);
   const lListElem *end_time = lGetElemStr(usage, UA_name, "end_time");
   CHECK(36, "a limit enforced twice leaves each usage entry once, end_time from the later enforcement",
         lGetNumberOfElem(usage) == 4 && end_time != nullptr && lGetDouble(end_time, UA_value) == 4000.0);
   lFreeElem(&ja_task);
   lFreeElem(&job);
}

/**
 * @brief Run the scenarios of the duplicate keys in unique attributes
 * @param argc number of arguments
 * @param argv the arguments; the module test frame takes its own options from them
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   lInit(nmv);
   // the parsers run with the debug check of N-H-18 armed: they must not hold a repeated key even for a moment; the
   // cases that build such lists on purpose suspend the check themselves
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
      {"UNQ-05", "raw context",
       "the instructions of -ac, -sc and -dc repeat markers and names in a list without a unique key",
       {"build the request of -ac a=1 -ac b=2 -dc a -ac a=3, check it and apply it to a job",
        "empty the context and add to it again",
        "build a request above the hash threshold and look up a marker and a repeated name",
        "apply a request without a marker",
        "add PARENT after a trailing -dc",
        "take the effective context of a reduced job as qstat -j has it"},
       "the request passes the central check; the context is b=2,a=3, then c=1, then 21 variables, then d=4; "
       "PARENT survives the -dc; a reduced job without the request does not abort",
       {{"CLN-04", "covers"}}, {15, 16, 17, 18, 19, 20, 21, 22}, test_context_change},
      {"UNQ-06", "spool repair",
       "a spooled object with repeated keys is repaired as the input paths repair them",
       {"repair a job with A=1,B=2,A=3, the queue all.q twice and h_vmem twice in a request set",
        "repair it again",
        "repair a user set naming a user twice",
        "repair a job holding array task 1 twice"},
       "variables and resources keep their last value, a repeated queue or user is dropped, the repairs name "
       "the lists; a repaired object has nothing left; a repeated task is reported with the job and the list",
       {{"SPL-01", "covers"}}, {23, 24, 25, 26, 27}, test_repair},
      {"UNQ-07", "merging switches",
       "the entries of several switches are merged by the rule of their type, never holding a key twice",
       {"merge -q a.q,b.q with c.q,a.q and c.q", "merge -l h_vmem=1G,h_rt=60 with h_vmem=2G",
        "merge A=1 with A=2", "merge an access list with a name it holds", "merge two ulong lists"},
       "queues and access lists keep the first entry, resources and variables the last value; a list without "
       "a unique string attribute is appended; the debug check never fires",
       {{"CLN-03", "covers"}}, {28, 29, 30, 31, 32}, test_merge},
      {"UNQ-08", "inventory findings",
       "code paths the inventory run found holding a key twice keep it once",
       {"fill -l a=lx-amd64,arch=lx-arm64 and -l arch=lx-amd64,a=lx-arm64, a being the shortcut of arch",
        "prepare qconf -mattr queue slots 4 for all.q@h1, all.q@h2 and all.q",
        "enforce a limit twice on the same task"},
       "the later request of arch is kept; the targets become one request for all.q with three references; "
       "each usage entry is held once with the values of the later enforcement; the debug check never fires",
       {{"LEG-02", "prepares"}}, {33, 34, 35, 36}, test_inventory},
   });
}
