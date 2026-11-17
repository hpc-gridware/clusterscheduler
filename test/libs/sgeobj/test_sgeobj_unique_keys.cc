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
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_var.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CX.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_US.h"
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
   const char *answer = ocs::tom::get<AN_text>(lFirst(answer_list));
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
   ocs::tom::emplace_back<VA_variable>(&vars, "A");
   ocs::tom::emplace_back<VA_variable>(&vars, "B");
   CHECK(1, "distinct string keys pass", ocs_object_list_verify_unique_keys(vars, &answer_list, "test"));

   ocs::tom::emplace_back<VA_variable>(&vars, "A");
   const bool rejected = !ocs_object_list_verify_unique_keys(vars, &answer_list, "test");
   CHECK(2, "a repeated string key is rejected, naming the key and the attribute",
         rejected && answer_contains(answer_list, "\"A\"") && answer_contains(answer_list, "VA_variable"));
   lFreeList(&answer_list);
   lFreeList(&vars);

   auto no_keys = ocs::tom::ListPtr<ocs::tom::VA>::create("no keys");
   lAppendElem(*no_keys, lCreateElem(VA_Type));
   lAppendElem(*no_keys, lCreateElem(VA_Type));
   CHECK(3, "elements without a key are skipped", ocs_object_list_verify_unique_keys(*no_keys, &answer_list, "test"));
   no_keys.reset();

   lList *hosts = nullptr;
   ocs::tom::emplace_back<HR_name>(&hosts, "h1");
   ocs::tom::emplace_back<HR_name>(&hosts, "h2");
   const bool distinct_hosts = ocs_object_list_verify_unique_keys(hosts, &answer_list, "test");
   ocs::tom::emplace_back<HR_name>(&hosts, "H1");
   CHECK(4, "host keys compare like the hash table of the list: H1 repeats h1, named as it is given",
         distinct_hosts && !ocs_object_list_verify_unique_keys(hosts, &answer_list, "test") &&
         answer_contains(answer_list, "\"H1\""));
   lFreeList(&answer_list);
   lFreeList(&hosts);

   lList *ulongs = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&ulongs, 1);
   ocs::tom::emplace_back<ULNG_value>(&ulongs, 2);
   const bool distinct = ocs_object_list_verify_unique_keys(ulongs, &answer_list, "test");
   ocs::tom::emplace_back<ULNG_value>(&ulongs, 1);
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
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   lList *env = nullptr;
   ocs::tom::emplace_back<VA_variable>(&env, "A");
   ocs::tom::emplace_back<VA_variable>(&env, "B");
   ocs::tom::set<JB_env_list>(job, env);
   CHECK(6, "a job with distinct variables passes", ocs_object_verify_unique_keys(*job, &answer_list, "job 1"));

   lList *job_env = ocs::tom::get<JB_env_list>(job);
   ocs::tom::emplace_back<VA_variable>(&job_env, "B");
   const bool rejected = !ocs_object_verify_unique_keys(*job, &answer_list, "job 1");
   CHECK(7, "a repeated variable in the job's environment is rejected, naming the job",
         rejected && answer_contains(answer_list, "job 1"));
   lFreeList(&answer_list);
}

/**
 * @brief The parsers keep a repeated variable once, with its last value
 * @return nothing; CHECK records the results
 */
static void
test_variable_parser() {
   lList *vars = nullptr;
   const int ret = var_list_parse_from_string(&vars, "A=1,B=2,A=3", 0);
   ocs::tom::ConstElement<ocs::tom::VA> a = ocs::tom::find_first<VA_variable>(vars, "A");
   CHECK(8, "-v A=1,B=2,A=3 holds A once, with the value 3",
         ret == 0 && lGetNumberOfElem(vars) == 2 && a != nullptr && strcmp(ocs::tom::get<VA_value>(a), "3") == 0);

   const int ret2 = var_list_parse_from_string(&vars, "A=4", 0);
   ocs::tom::ConstElement<ocs::tom::VA> a_va = ocs::tom::find_first<VA_variable>(vars, "A");
   CHECK(9, "a later -v adds to the same list by name",
         ret2 == 0 && lGetNumberOfElem(vars) == 2 && strcmp(ocs::tom::get<VA_value>(a_va), "4") == 0);
   lFreeList(&vars);

   const int ret3 = var_list_parse_from_string(&vars, "A,A", 0);
   CHECK(10, "a name without a value, given twice, is kept once without a value",
         ret3 == 0 && lGetNumberOfElem(vars) == 1 && ocs::tom::get<VA_value>(lFirst(vars)) == nullptr);
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
         ret == 0 && lGetNumberOfElem(qrefs) == 2 && strcmp(ocs::tom::get<QR_name>(lFirst(qrefs)), "a.q") == 0);
   qref_list_add(&qrefs, nullptr, "b.q");
   CHECK(12, "adding a reference the list holds changes nothing", lGetNumberOfElem(qrefs) == 2);
   lFreeList(&qrefs);

   lList *acl = nullptr;
   {
      // a list with a repeated name on purpose, as input for the removal; suspended as above
      const ocs::tom::UniqueCheckSuspension suspension;
      ocs::tom::emplace_back<ARA_name>(&acl, "u1");
      ocs::tom::emplace_back<ARA_name>(&acl, "u2");
      ocs::tom::emplace_back<ARA_name>(&acl, "u1");
      ocs_object_list_remove_repeated_keys(acl, ARA_name);
   }
   CHECK(13, "repeated names are removed, the first of each stays",
         lGetNumberOfElem(acl) == 2 && strcmp(ocs::tom::get<ARA_name>(lLast(acl)), "u2") == 0);
   lFreeList(&acl);

   lList *answer_list = nullptr;
   const auto [parsed, ulongs] = ulong_list_parse_from_string(&answer_list, "1,2,1", ",");
   CHECK(14, "a ulong list given 1,2,1 holds 1 once", parsed && ulongs->size() == 2);
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
   auto ep = ocs::tom::ElementPtr<ocs::tom::VA>::create();
   ocs::tom::set<VA_variable>(ep, marker);
   lInsertElem(option, nullptr, std::move(ep));
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
   ocs::tom::ConstElement<ocs::tom::VA> var = ocs::tom::find_first_in<JB_context, VA_variable>(job, name);
   return var != nullptr && ocs::tom::get<VA_value>(var) != nullptr && strcmp(ocs::tom::get<VA_value>(var), value) == 0;
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
   const lListElem *a = ocs::tom::find_first<CX_variable>(change, "a");
   CHECK(15, "the request holds four markers and a three times; a lookup of a finds an element",
         lGetNumberOfElem(change) == 8 && a != nullptr);

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   ocs::tom::set<JB_context_change>(job, change);
   lList *answer_list = nullptr;
   CHECK(16, "the central check accepts a job holding the request",
         ocs_object_verify_unique_keys(*job, &answer_list, "job 1"));
   lFreeList(&answer_list);

   set_context(ocs::tom::get<JB_context_change>(job), *job);
   CHECK(17, "the request yields the context b=2,a=3",
         lGetNumberOfElem(ocs::tom::get<JB_context>(job)) == 2 && context_has(*job, "b", "2") && context_has(*job, "a", "3"));

   // the context loses its last variable and gets a new one
   lList *readd = nullptr;
   add_context_switch(&readd, "=", "a=1");
   add_context_switch(&readd, "-", "a");
   add_context_switch(&readd, "+", "c=1");
   set_context(readd, *job);
   CHECK(18, "a variable added after the context became empty is kept",
         lGetNumberOfElem(ocs::tom::get<JB_context>(job)) == 1 && context_has(*job, "c", "1"));
   lFreeList(&readd);

   // above the hash threshold the markers and a repeated name are hashed without a unique key
   lList *big = nullptr;
   for (int i = 0; i < 20; i++) {
      const std::string var = "x" + std::to_string(i) + "=" + std::to_string(i);
      add_context_switch(&big, "+", var.c_str());
   }
   add_context_switch(&big, "-", "x0");
   add_context_switch(&big, "+", "x0=new");
   const bool found = static_cast<const lListElem *>(ocs::tom::find_first<CX_variable>(big, "+")) != nullptr && static_cast<const lListElem *>(ocs::tom::find_first<CX_variable>(big, "x0")) != nullptr;
   set_context(big, *job);
   CHECK(19, "above the hash threshold the lookups work; c and the 20 added variables remain, x0 with its new value",
         lGetNumberOfElem(big) > OCS_CULL_HASH_MIN_ELEMS && found &&
         lGetNumberOfElem(ocs::tom::get<JB_context>(job)) == 21 && context_has(*job, "x0", "new"));
   lFreeList(&big);

   // a request without a marker replaces the context
   lList *plain = nullptr;
   lList *vars = nullptr;
   var_list_parse_from_string(&vars, "d=4", 0);
   ocs_context_change_add(&plain, vars);
   lFreeList(&vars);
   set_context(plain, *job);
   CHECK(20, "a request without a marker replaces the context",
         lGetNumberOfElem(ocs::tom::get<JB_context>(job)) == 1 && context_has(*job, "d", "4"));
   lFreeList(&plain);
   job.reset();

   // qsh adds PARENT after the switches; a trailing -dc must not delete it
   auto child = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   lList *child_change = nullptr;
   add_context_switch(&child_change, "+", "e=5");
   add_context_switch(&child_change, "-", "e");
   ocs::tom::set<JB_context_change>(child, child_change);
   setenv("JOB_ID", "42", 1);
   job_add_parent_id_to_context(*child);
   unsetenv("JOB_ID");
   lList *effective = ocs_context_get_effective(*child).release();
   ocs::tom::ConstElement<ocs::tom::VA> parent = ocs::tom::find_first<VA_variable>(effective, CONTEXT_PARENT);
   CHECK(21, "PARENT given after a trailing -dc is in the effective context",
         lGetNumberOfElem(effective) == 1 && parent != nullptr && strcmp(ocs::tom::get<VA_value>(parent), "42") == 0);
   lFreeList(&effective);
   child.reset();

   // qstat -j works on a reduced job that does not contain JB_context_change
   lList *jobs = lCreateList("jobs", JB_Type);
   ocs::tom::Element<ocs::tom::JB> full = ocs::tom::emplace_back<JB_job_number>(&jobs, 1);
   lList *context = nullptr;
   ocs::tom::emplace_back<VA_variable>(&context, "f");
   ocs::tom::set<JB_context>(full, context);
   lEnumeration *what = lWhat("%T(%I %I)", JB_Type, JB_job_number, JB_context);
   lList *reduced = lSelect("reduced", jobs, nullptr, what);
   lList *reduced_effective = ocs_context_get_effective(lFirst(reduced));
   CHECK(22, "the effective context of a reduced job without the change request is its context",
         lGetNumberOfElem(reduced_effective) == 1 && static_cast<const lListElem *>(ocs::tom::find_first<VA_variable>(reduced_effective, "f")) != nullptr);
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
   ocs::tom::Element<ocs::tom::VA> var = ocs::tom::emplace_back<VA_variable>(lpp, name);
   ocs::tom::set<VA_value>(var, value);
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
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   ocs::tom::set<JB_job_number>(job, 7);

   lList *env = nullptr;
   add_variable(&env, "A", "1");
   add_variable(&env, "B", "2");
   add_variable(&env, "A", "3");
   ocs::tom::set<JB_env_list>(job, env);

   ocs::tom::Element<ocs::tom::JRS> request_set = ocs::tom::emplace_back_in<JB_request_set_list, JRS_scope>(job, 0);
   lList *queues = nullptr;
   ocs::tom::emplace_back<QR_name>(&queues, "all.q");
   ocs::tom::emplace_back<QR_name>(&queues, "b.q");
   ocs::tom::emplace_back<QR_name>(&queues, "all.q");
   ocs::tom::set<JRS_hard_queue_list>(request_set, queues);
   lList *resources = nullptr;
   ocs::tom::set<CE_stringval>(ocs::tom::emplace_back<CE_name>(&resources, "h_vmem"), "1G");
   ocs::tom::set<CE_stringval>(ocs::tom::emplace_back<CE_name>(&resources, "h_vmem"), "2G");
   ocs::tom::set<JRS_hard_resource_list>(request_set, resources);

   std::string repairs;
   lList *answer_list = nullptr;
   const bool repaired = ocs_object_repair_repeated_keys(*job, repairs, &answer_list, "job 7");
   ocs::tom::ConstElement<ocs::tom::VA> a = ocs::tom::find_first<VA_variable>(ocs::tom::get<JB_env_list>(job), "A");
   ocs::tom::ConstList<ocs::tom::QR> repaired_queues = ocs::tom::get<JRS_hard_queue_list>(request_set);
   ocs::tom::ConstList<ocs::tom::CE> repaired_resources = ocs::tom::get<JRS_hard_resource_list>(request_set);
   CHECK(23, "variables and resources keep the last value, a repeated queue is dropped, the first stays",
         repaired && answer_list == nullptr && lGetNumberOfElem(ocs::tom::get<JB_env_list>(job)) == 2 && a != nullptr &&
         strcmp(ocs::tom::get<VA_value>(a), "3") == 0 && lGetNumberOfElem(repaired_queues) == 2 &&
         strcmp(ocs::tom::get<QR_name>(lFirst(repaired_queues)), "all.q") == 0 &&
         lGetNumberOfElem(repaired_resources) == 1 &&
         strcmp(ocs::tom::get<CE_stringval>(lFirst(repaired_resources)), "2G") == 0);
   CHECK(24, "the repairs name the path of each list and the key",
         repairs.find("JB_env_list \"A\"") != std::string::npos &&
         repairs.find("JB_request_set_list/JRS_hard_queue_list \"all.q\"") != std::string::npos &&
         repairs.find("JB_request_set_list/JRS_hard_resource_list \"h_vmem\"") != std::string::npos);

   repairs.clear();
   CHECK(25, "a repaired object has nothing left to repair",
         ocs_object_repair_repeated_keys(*job, repairs, &answer_list, "job 7") && repairs.empty());

   auto userset = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(userset, "staff");
   ocs::tom::emplace_back_in<US_entries, UE_name>(userset, "u1");
   ocs::tom::emplace_back_in<US_entries, UE_name>(userset, "u2");
   ocs::tom::emplace_back_in<US_entries, UE_name>(userset, "u1");
   CHECK(26, "a user set keeps a repeated user once",
         ocs_object_repair_repeated_keys(*userset, repairs, &answer_list, "userset staff") &&
         lGetNumberOfElem(ocs::tom::get<US_entries>(userset)) == 2 && repairs == "US_entries \"u1\"");

   ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(job, 1);
   ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(job, 1);
   repairs.clear();
   CHECK(27, "a repeated array task is not repaired; the error names the job, the list and the key",
         !ocs_object_repair_repeated_keys(*job, repairs, &answer_list, "job 7") && repairs.empty() &&
         lGetNumberOfElem(ocs::tom::get<JB_ja_tasks>(job)) == 2 && answer_contains(answer_list, "job 7, JB_ja_tasks") &&
         answer_contains(answer_list, "JAT_task_number"));

   lFreeList(&answer_list);
   userset.reset();
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
         strcmp(ocs::tom::get<QR_name>(lFirst(queues)), "a.q") == 0 &&
         strcmp(ocs::tom::get<QR_name>(lLast(queues)), "c.q") == 0);
   lFreeList(&queues);

   lList *resources = nullptr;
   lList *switch_1 = nullptr;
   ocs::tom::set<CE_stringval>(ocs::tom::emplace_back<CE_name>(&switch_1, "h_vmem"), "1G");
   ocs::tom::set<CE_stringval>(ocs::tom::emplace_back<CE_name>(&switch_1, "h_rt"), "60");
   lList *switch_2 = nullptr;
   ocs::tom::set<CE_stringval>(ocs::tom::emplace_back<CE_name>(&switch_2, "h_vmem"), "2G");
   ocs_object_list_add_unique(&resources, &switch_1);
   ocs_object_list_add_unique(&resources, &switch_2);
   ocs::tom::ConstElement<ocs::tom::CE> h_vmem = ocs::tom::find_first<CE_name>(resources, "h_vmem");
   CHECK(29, "-l h_vmem=1G,h_rt=60 -l h_vmem=2G holds h_vmem once, with 2G",
         lGetNumberOfElem(resources) == 2 && h_vmem != nullptr && strcmp(ocs::tom::get<CE_stringval>(h_vmem), "2G") == 0);
   lFreeList(&resources);

   lList *env = nullptr;
   lList *vars = nullptr;
   add_variable(&vars, "A", "1");
   ocs_object_list_add_unique(&env, &vars);
   add_variable(&vars, "A", "2");
   ocs_object_list_add_unique(&env, &vars);
   CHECK(30, "a variable merged twice keeps its last value",
         lGetNumberOfElem(env) == 1 && strcmp(ocs::tom::get<VA_value>(lFirst(env)), "2") == 0);
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
   ocs::tom::emplace_back<ULNG_value>(&ids, 1);
   ocs::tom::emplace_back<ULNG_value>(&more_ids, 2);
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
   ocs::tom::set<CE_stringval>(ocs::tom::emplace_back<CE_name>(lpp, name), value);
}

/**
 * @brief The value of the only arch request of a list, if it holds exactly one request under its full name
 * @param lp the request list
 * @return the requested value, or nullptr if the list does not hold exactly one request named arch
 */
static const char *
only_arch_value(const lList *lp) {
   if (lGetNumberOfElem(lp) != 1 || strcmp(ocs::tom::get<CE_name>(lFirst(lp)), "arch") != 0) {
      return nullptr;
   }
   return ocs::tom::get<CE_stringval>(lFirst(lp));
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
   ocs::tom::Element<ocs::tom::CE> arch = ocs::tom::emplace_back<CE_name>(&master_centry_list, "arch");
   ocs::tom::set<CE_shortcut>(arch, "a");
   ocs::tom::set<CE_valtype>(arch, static_cast<uint32_t>(ocs::CEntry::Type::STR));
   ocs::tom::set<CE_relop>(arch, CMPLXEQ_OP);
   ocs::tom::set<CE_requestable>(arch, REQU_YES);
   ocs::tom::set<CE_consumable>(arch, CONSUMABLE_NO);

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
      lListElem *cqueue = ocs::tom::emplace_back<CQ_name>(&targets, target);
      lListElem *slots = lAddSubHost(cqueue, AULNG_href, HOSTREF_DEFAULT, CQ_job_slots, AULNG_Type);
      ocs::tom::set<AULNG_value>(slots, 4);
   }
   bool ok = cqueue_xattr_pre_gdi(targets, &answers);
   const lList *slots = lGetNumberOfElem(targets) == 1 ? ocs::tom::get<CQ_job_slots>(lFirst(targets)) : nullptr;
   CHECK(35, "the targets all.q@h1, all.q@h2 and all.q become one request for all.q with h1, h2 and the default",
         ok && slots != nullptr && strcmp(ocs::tom::get<CQ_name>(lFirst(targets)), "all.q") == 0 &&
         lGetNumberOfElem(slots) == 3 && static_cast<const lListElem *>(ocs::tom::find_first<AULNG_href>(slots, "h1")) != nullptr &&
         static_cast<const lListElem *>(ocs::tom::find_first<AULNG_href>(slots, "h2")) != nullptr &&
         static_cast<const lListElem *>(ocs::tom::find_first<AULNG_href>(slots, HOSTREF_DEFAULT)) != nullptr);
   lFreeList(&targets);
   lFreeList(&answers);

   // the qmaster enforces a limit on a task, and then again on the same task
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   ocs::tom::set<JB_job_number>(job, 1);
   ocs::tom::set<JB_submission_time>(job, 1000);
   auto ja_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   ocs::tom::set<JAT_task_number>(ja_task, 1);
   ocs::tom::set<JAT_start_time>(ja_task, 2000);
   for (uint64_t now : {3000, 4000}) {
      auto report = ocs::tom::ElementPtr<ocs::tom::JR>::create();
      job_report_init_from_job_with_usage(*report, *job, *ja_task, nullptr, now);
   }
   ocs::tom::ConstList<ocs::tom::UA> usage = ocs::tom::get<JAT_scaled_usage_list>(ja_task);
   ocs::tom::ConstElement<ocs::tom::UA> end_time = ocs::tom::find_first<UA_name>(usage, "end_time");
   CHECK(36, "a limit enforced twice leaves each usage entry once, end_time from the later enforcement",
         lGetNumberOfElem(usage) == 4 && end_time != nullptr && ocs::tom::get<UA_value>(end_time) == 4000.0);
   ja_task.reset();
   job.reset();

   // qsub -q '*@@*': a host that belongs to several matching host groups
   lList *hgroups = nullptr;
   const char *const members[][3] = {{"@a", "h1", "h2"}, {"@b", "h2", "h3"}};
   for (const auto &m : members) {
      ocs::tom::Element<ocs::tom::HGRP> hgroup = ocs::tom::emplace_back<HGRP_name>(&hgroups, m[0]);
      ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(hgroup, m[1]);
      ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(hgroup, m[2]);
   }
   lList *hosts = nullptr;
   ok = hgroup_list_find_matching_and_resolve(hgroups, &answers, "@*", &hosts);
   CHECK(37, "resolving @* over @a (h1, h2) and @b (h2, h3) collects h1, h2 and h3 once each",
         ok && lGetNumberOfElem(hosts) == 3);
   lFreeList(&hosts);
   lFreeList(&hgroups);
   lFreeList(&answers);
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
        "enforce a limit twice on the same task",
        "resolve the host group pattern @* over two groups sharing a host"},
       "the later request of arch is kept; the targets become one request for all.q with three references; "
       "each usage entry is held once with the values of the later enforcement; a shared host is collected once; "
       "the debug check never fires",
       {{"LEG-02", "prepares"}}, {33, 34, 35, 36, 37}, test_inventory},
   });
}
