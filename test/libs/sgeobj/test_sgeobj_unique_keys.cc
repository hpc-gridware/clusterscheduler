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

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
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
#include "sgeobj/sge_answer.h"

/**
 * @brief Whether the first entry of an answer list contains a text
 * @param answer_list the answer list
 * @param text the text to look for
 * @return true if the list has an entry and its text contains the given one
 */
static bool
answer_contains(const ocs::tom::ConstList<ocs::tom::AN> answer_list, const char *text) {
   const char *answer = answer_list.front().get<AN_text>();
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
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   auto vars = ocs::tom::ListPtr<ocs::tom::VA>::create("vars");
   vars->emplace_back<VA_variable>("A");
   vars->emplace_back<VA_variable>("B");
   CHECK(1, "distinct string keys pass", ocs_object_list_verify_unique_keys(*vars, &answer_list, "test"));

   vars->emplace_back<VA_variable>("A");
   const bool rejected = !ocs_object_list_verify_unique_keys(*vars, &answer_list, "test");
   CHECK(2, "a repeated string key is rejected, naming the key and the attribute",
         rejected && answer_contains(answer_list, "\"A\"") && answer_contains(answer_list, "VA_variable"));
   answer_list.reset();
   vars.reset();

   auto no_keys = ocs::tom::ListPtr<ocs::tom::VA>::create("no keys");
   no_keys->push_back(ocs::tom::ElementPtr<ocs::tom::VA>::create());
   no_keys->push_back(ocs::tom::ElementPtr<ocs::tom::VA>::create());
   CHECK(3, "elements without a key are skipped", ocs_object_list_verify_unique_keys(*no_keys, &answer_list, "test"));
   no_keys.reset();

   auto hosts = ocs::tom::ListPtr<ocs::tom::HR>::create("hosts");
   hosts->emplace_back<HR_name>(ocs::tom::Host("h1"));
   hosts->emplace_back<HR_name>(ocs::tom::Host("h2"));
   const bool distinct_hosts = ocs_object_list_verify_unique_keys(*hosts, &answer_list, "test");
   hosts->emplace_back<HR_name>(ocs::tom::Host("H1"));
   CHECK(4, "host keys compare like the hash table of the list: H1 repeats h1, named as it is given",
         distinct_hosts && !ocs_object_list_verify_unique_keys(*hosts, &answer_list, "test") &&
         answer_contains(answer_list, "\"H1\""));
   answer_list.reset();
   hosts.reset();

   auto ulongs = ocs::tom::ListPtr<ocs::tom::ULNG>::create("ulongs");
   ulongs->emplace_back<ULNG_value>(1);
   ulongs->emplace_back<ULNG_value>(2);
   const bool distinct = ocs_object_list_verify_unique_keys(*ulongs, &answer_list, "test");
   ulongs->emplace_back<ULNG_value>(1);
   CHECK(5, "a repeated ulong key is rejected",
         distinct && !ocs_object_list_verify_unique_keys(*ulongs, &answer_list, "test"));
   answer_list.reset();
   ulongs.reset();
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
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   auto env = ocs::tom::ListPtr<ocs::tom::VA>::create("env");
   env->emplace_back<VA_variable>("A");
   env->emplace_back<VA_variable>("B");
   job->set<JB_env_list>(std::move(env));
   CHECK(6, "a job with distinct variables passes", ocs_object_verify_unique_keys(*job, &answer_list, "job 1"));

   job->get<JB_env_list>().emplace_back<VA_variable>("B");
   const bool rejected = !ocs_object_verify_unique_keys(*job, &answer_list, "job 1");
   CHECK(7, "a repeated variable in the job's environment is rejected, naming the job",
         rejected && answer_contains(answer_list, "job 1"));
   answer_list.reset();
}

/**
 * @brief The parsers keep a repeated variable once, with its last value
 * @return nothing; CHECK records the results
 */
static void
test_variable_parser() {
   ocs::tom::ListPtr<ocs::tom::VA> vars;
   const int ret = var_list_parse_from_string(vars, "A=1,B=2,A=3", 0);
   ocs::tom::ConstElement<ocs::tom::VA> a = vars->find_first<VA_variable>("A");
   CHECK(8, "-v A=1,B=2,A=3 holds A once, with the value 3",
         ret == 0 && vars->size() == 2 && a != nullptr && strcmp(a.get<VA_value>(), "3") == 0);

   const int ret2 = var_list_parse_from_string(vars, "A=4", 0);
   ocs::tom::ConstElement<ocs::tom::VA> a_va = vars->find_first<VA_variable>("A");
   CHECK(9, "a later -v adds to the same list by name",
         ret2 == 0 && vars->size() == 2 && strcmp(a_va.get<VA_value>(), "4") == 0);
   vars.reset();

   const int ret3 = var_list_parse_from_string(vars, "A,A", 0);
   CHECK(10, "a name without a value, given twice, is kept once without a value",
         ret3 == 0 && vars->size() == 1 && vars->front().get<VA_value>() == nullptr);
   vars.reset();
}

/**
 * @brief Set-like lists keep a repeated name once
 * @return nothing; CHECK records the results
 */
static void
test_set_lists() {
   lList *raw_qrefs = nullptr;
   const int ret = cull_parse_destination_identifier_list(&raw_qrefs, "a.q,b.q,a.q");
   ocs::tom::ListPtr<ocs::tom::QR> qrefs(raw_qrefs);
   CHECK(11, "-q a.q,b.q,a.q holds a.q once, in its first position",
         ret == 0 && qrefs->size() == 2 && strcmp(qrefs->front().get<QR_name>(), "a.q") == 0);
   qref_list_add(&qrefs, nullptr, "b.q");
   CHECK(12, "adding a reference the list holds changes nothing", qrefs->size() == 2);

   auto acl = ocs::tom::ListPtr<ocs::tom::ARA>::create("acl");
   {
      // a list with a repeated name on purpose, as input for the removal; suspended as above
      const ocs::tom::UniqueCheckSuspension suspension;
      acl->emplace_back<ARA_name>("u1");
      acl->emplace_back<ARA_name>("u2");
      acl->emplace_back<ARA_name>("u1");
      ocs_object_list_remove_repeated_keys(*acl, ARA_name);
   }
   CHECK(13, "repeated names are removed, the first of each stays",
         acl->size() == 2 && strcmp(acl->back().get<ARA_name>(), "u2") == 0);
   acl.reset();

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const auto [parsed, ulongs] = ulong_list_parse_from_string(&answer_list, "1,2,1", ",");
   CHECK(14, "a ulong list given 1,2,1 holds 1 once", parsed && ulongs->size() == 2);
   answer_list.reset();
}

/**
 * @brief Append one context switch to a request, built the way the parser builds it
 * @param[in,out] change the context change request, created with the first instruction
 * @param marker the marker of the switch: "+" for -ac, "-" for -dc, "=" for -sc
 * @param vars the variables of the switch, "name[=value],..."
 * @return nothing
 */
static void
add_context_switch(ocs::tom::ListPtr<ocs::tom::CX> &change, const char *marker, const char *vars) {
   ocs::tom::ListPtr<ocs::tom::VA> option;
   var_list_parse_from_string(option, vars, 0);
   auto ep = ocs::tom::ElementPtr<ocs::tom::VA>::create();
   ep->set<VA_variable>(marker);
   option->push_front(std::move(ep));
   ocs_context_change_add(change, option);
}

/**
 * @brief Whether a job's context holds a variable with a value
 * @param job the job
 * @param name the variable
 * @param value the expected value
 * @return true if the variable is in JB_context with that value
 */
static bool
context_has(const ocs::tom::ConstElement<ocs::tom::JB> job, const char *name, const char *value) {
   const auto var = job.find_first_in<JB_context, VA_variable>(name);
   return var != nullptr && var.get<VA_value>() != nullptr && strcmp(var.get<VA_value>(), value) == 0;
}

/**
 * @brief The instructions of -ac, -sc and -dc repeat markers and names without a unique key
 * @return nothing; CHECK records the results
 */
static void
test_context_change() {
   // -ac a=1 -ac b=2 -dc a -ac a=3
   ocs::tom::ListPtr<ocs::tom::CX> change;
   add_context_switch(change, "+", "a=1");
   add_context_switch(change, "+", "b=2");
   add_context_switch(change, "-", "a");
   add_context_switch(change, "+", "a=3");
   const auto a = change->find_first<CX_variable>("a");
   CHECK(15, "the request holds four markers and a three times; a lookup of a finds an element",
         change->size() == 8 && a != nullptr);

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_context_change>(std::move(change));
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   CHECK(16, "the central check accepts a job holding the request",
         ocs_object_verify_unique_keys(*job, &answer_list, "job 1"));
   answer_list.reset();

   set_context(job->get<JB_context_change>(), *job);
   CHECK(17, "the request yields the context b=2,a=3",
         job->get<JB_context>().size() == 2 && context_has(*job, "b", "2") && context_has(*job, "a", "3"));

   // the context loses its last variable and gets a new one
   ocs::tom::ListPtr<ocs::tom::CX> readd;
   add_context_switch(readd, "=", "a=1");
   add_context_switch(readd, "-", "a");
   add_context_switch(readd, "+", "c=1");
   set_context(*readd, *job);
   CHECK(18, "a variable added after the context became empty is kept",
         job->get<JB_context>().size() == 1 && context_has(*job, "c", "1"));

   // above the hash threshold the markers and a repeated name are hashed without a unique key
   ocs::tom::ListPtr<ocs::tom::CX> big;
   for (int i = 0; i < 20; i++) {
      const std::string var = "x" + std::to_string(i) + "=" + std::to_string(i);
      add_context_switch(big, "+", var.c_str());
   }
   add_context_switch(big, "-", "x0");
   add_context_switch(big, "+", "x0=new");
   const bool found = big->find_first<CX_variable>("+") != nullptr && big->find_first<CX_variable>("x0") != nullptr;
   set_context(*big, *job);
   CHECK(19, "above the hash threshold the lookups work; c and the 20 added variables remain, x0 with its new value",
         big->size() > TOM_HASH_MIN_ELEMS && found &&
         job->get<JB_context>().size() == 21 && context_has(*job, "x0", "new"));

   // a request without a marker replaces the context
   ocs::tom::ListPtr<ocs::tom::CX> plain;
   ocs::tom::ListPtr<ocs::tom::VA> vars;
   var_list_parse_from_string(vars, "d=4", 0);
   ocs_context_change_add(plain, vars);
   vars.reset();
   set_context(*plain, *job);
   CHECK(20, "a request without a marker replaces the context",
         job->get<JB_context>().size() == 1 && context_has(*job, "d", "4"));
   job.reset();

   // qsh adds PARENT after the switches; a trailing -dc must not delete it
   auto child = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   ocs::tom::ListPtr<ocs::tom::CX> child_change;
   add_context_switch(child_change, "+", "e=5");
   add_context_switch(child_change, "-", "e");
   child->set<JB_context_change>(std::move(child_change));
   setenv("JOB_ID", "42", 1);
   job_add_parent_id_to_context(*child);
   unsetenv("JOB_ID");
   auto effective = ocs_context_get_effective(*child);
   ocs::tom::ConstElement<ocs::tom::VA> parent = effective->find_first<VA_variable>(CONTEXT_PARENT);
   CHECK(21, "PARENT given after a trailing -dc is in the effective context",
         effective->size() == 1 && parent != nullptr && strcmp(parent.get<VA_value>(), "42") == 0);
   effective.reset();
   child.reset();

   // qstat -j works on a reduced job that does not contain JB_context_change
   auto jobs = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   ocs::tom::Element<ocs::tom::JB> full = jobs->emplace_back<JB_job_number>(1);
   auto context = ocs::tom::ListPtr<ocs::tom::VA>::create("context");
   context->emplace_back<VA_variable>("f");
   full.set<JB_context>(std::move(context));
   auto reduced = jobs->select(ocs::tom::what<JB_job_number, JB_context>());
   auto reduced_effective = ocs_context_get_effective(reduced->front());
   CHECK(22, "the effective context of a reduced job without the change request is its context",
         reduced_effective->size() == 1 &&
         reduced_effective->find_first<VA_variable>("f") != nullptr);
   reduced_effective.reset();
   reduced.reset();
   jobs.reset();
}

/**
 * @brief Add a variable to a list of variables
 * @param[in,out] lp the list, created if empty
 * @param name the name
 * @param value the value
 */
static void
add_variable(ocs::tom::ListPtr<ocs::tom::VA> &lp, const char *name, const char *value) {
   if (!lp) {
      lp = ocs::tom::ListPtr<ocs::tom::VA>::create("variables");
   }
   lp->emplace_back<VA_variable>(name).set<VA_value>(value);
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
   job->set<JB_job_number>(7);

   ocs::tom::ListPtr<ocs::tom::VA> env;
   add_variable(env, "A", "1");
   add_variable(env, "B", "2");
   add_variable(env, "A", "3");
   job->set<JB_env_list>(std::move(env));

   ocs::tom::Element<ocs::tom::JRS> request_set = job->emplace_back_in<JB_request_set_list, JRS_scope>(0);
   auto queues = ocs::tom::ListPtr<ocs::tom::QR>::create("queues");
   queues->emplace_back<QR_name>("all.q");
   queues->emplace_back<QR_name>("b.q");
   queues->emplace_back<QR_name>("all.q");
   request_set.set<JRS_hard_queue_list>(std::move(queues));
   auto resources = ocs::tom::ListPtr<ocs::tom::CE>::create("resources");
   resources->emplace_back<CE_name>("h_vmem").set<CE_stringval>("1G");
   resources->emplace_back<CE_name>("h_vmem").set<CE_stringval>("2G");
   request_set.set<JRS_hard_resource_list>(std::move(resources));

   std::string repairs;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const bool repaired = ocs_object_repair_repeated_keys(*job, repairs, &answer_list, "job 7");
   ocs::tom::ConstElement<ocs::tom::VA> a = job->get<JB_env_list>().find_first<VA_variable>("A");
   ocs::tom::ConstList<ocs::tom::QR> repaired_queues = request_set.get<JRS_hard_queue_list>();
   ocs::tom::ConstList<ocs::tom::CE> repaired_resources = request_set.get<JRS_hard_resource_list>();
   CHECK(23, "variables and resources keep the last value, a repeated queue is dropped, the first stays",
         repaired && answer_list == nullptr && job->get<JB_env_list>().size() == 2 && a != nullptr &&
         strcmp(a.get<VA_value>(), "3") == 0 && repaired_queues.size() == 2 &&
         strcmp(repaired_queues.front().get<QR_name>(), "all.q") == 0 &&
         repaired_resources.size() == 1 &&
         strcmp(repaired_resources.front().get<CE_stringval>(), "2G") == 0);
   CHECK(24, "the repairs name the path of each list and the key",
         repairs.find("JB_env_list \"A\"") != std::string::npos &&
         repairs.find("JB_request_set_list/JRS_hard_queue_list \"all.q\"") != std::string::npos &&
         repairs.find("JB_request_set_list/JRS_hard_resource_list \"h_vmem\"") != std::string::npos);

   repairs.clear();
   CHECK(25, "a repaired object has nothing left to repair",
         ocs_object_repair_repeated_keys(*job, repairs, &answer_list, "job 7") && repairs.empty());

   auto userset = ocs::tom::ElementPtr<ocs::tom::US>::create();
   userset->set<US_name>("staff");
   userset->emplace_back_in<US_entries, UE_name>("u1");
   userset->emplace_back_in<US_entries, UE_name>("u2");
   userset->emplace_back_in<US_entries, UE_name>("u1");
   CHECK(26, "a user set keeps a repeated user once",
         ocs_object_repair_repeated_keys(*userset, repairs, &answer_list, "userset staff") &&
         userset->get<US_entries>().size() == 2 && repairs == "US_entries \"u1\"");

   job->emplace_back_in<JB_ja_tasks, JAT_task_number>(1);
   job->emplace_back_in<JB_ja_tasks, JAT_task_number>(1);
   repairs.clear();
   CHECK(27, "a repeated array task is not repaired; the error names the job, the list and the key",
         !ocs_object_repair_repeated_keys(*job, repairs, &answer_list, "job 7") && repairs.empty() &&
         job->get<JB_ja_tasks>().size() == 2 && answer_contains(answer_list, "job 7, JB_ja_tasks") &&
         answer_contains(answer_list, "JAT_task_number"));

   answer_list.reset();
   userset.reset();
}

/**
 * @brief Build a list of one string attribute from names
 * @tparam F the attribute; its type is the type of the list
 * @param names the names, nullptr terminated
 * @return the list, empty for no name
 */
template <auto F>
static ocs::tom::ListPtr<typename ocs::tom::FieldTraits<F>::Type>
names_list(const char *const names[]) {
   using T = typename ocs::tom::FieldTraits<F>::Type;
   ocs::tom::ListPtr<T> lp;
   for (int i = 0; names[i] != nullptr; i++) {
      if (!lp) {
         lp = ocs::tom::ListPtr<T>::create("names");
      }
      lp->template emplace_back<F>(names[i]);
   }
   return lp;
}

/**
 * @brief Merge a list into another with ocs_object_list_add_unique(), which takes both lists by raw reference
 * @tparam T the object type of both lists
 * @param[in,out] dst the target list, created if it is empty
 * @param[in,out] src the elements to move; empty afterwards
 */
template <typename T>
static void
add_unique(ocs::tom::ListPtr<T> &dst, ocs::tom::ListPtr<T> &src) {
   lList *dst_raw = dst.release();
   lList *src_raw = src.release();
   ocs_object_list_add_unique(&dst_raw, &src_raw);
   dst = ocs::tom::ListPtr<T>::from_typed(dst_raw);
   src = ocs::tom::ListPtr<T>::from_typed(src_raw);
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
   auto queues = names_list<QR_name>(first_queues);
   auto more = names_list<QR_name>(more_queues);
   auto last = names_list<QR_name>(last_queues);
   add_unique(queues, more);
   add_unique(queues, last);
   CHECK(28, "-q a.q,b.q -q c.q,a.q -q c.q holds a.q, b.q and c.q once, in their first positions",
         more == nullptr && last == nullptr && queues->size() == 3 &&
         strcmp(queues->front().get<QR_name>(), "a.q") == 0 && strcmp(queues->back().get<QR_name>(), "c.q") == 0);
   queues.reset();

   ocs::tom::ListPtr<ocs::tom::CE> resources;
   auto switch_1 = ocs::tom::ListPtr<ocs::tom::CE>::create("switch_1");
   switch_1->emplace_back<CE_name>("h_vmem").set<CE_stringval>("1G");
   switch_1->emplace_back<CE_name>("h_rt").set<CE_stringval>("60");
   auto switch_2 = ocs::tom::ListPtr<ocs::tom::CE>::create("switch_2");
   switch_2->emplace_back<CE_name>("h_vmem").set<CE_stringval>("2G");
   add_unique(resources, switch_1);
   add_unique(resources, switch_2);
   const auto h_vmem = resources->find_first<CE_name>("h_vmem");
   CHECK(29, "-l h_vmem=1G,h_rt=60 -l h_vmem=2G holds h_vmem once, with 2G",
         resources->size() == 2 && h_vmem != nullptr && strcmp(h_vmem.get<CE_stringval>(), "2G") == 0);
   resources.reset();

   ocs::tom::ListPtr<ocs::tom::VA> env;
   ocs::tom::ListPtr<ocs::tom::VA> vars;
   add_variable(vars, "A", "1");
   add_unique(env, vars);
   add_variable(vars, "A", "2");
   add_unique(env, vars);
   CHECK(30, "a variable merged twice keeps its last value",
         env->size() == 1 && strcmp(env->front().get<VA_value>(), "2") == 0);
   env.reset();

   const char *const acl_names[] = {"u1", "u2", nullptr};
   const char *const more_names[] = {"u1", nullptr};
   auto acl = names_list<ARA_name>(acl_names);
   auto more_acl = names_list<ARA_name>(more_names);
   add_unique(acl, more_acl);
   CHECK(31, "an access list merged with a name it holds keeps it once", acl->size() == 2);
   acl.reset();

   auto ids = ocs::tom::ListPtr<ocs::tom::ULNG>::create("ids");
   auto more_ids = ocs::tom::ListPtr<ocs::tom::ULNG>::create("more_ids");
   ids->emplace_back<ULNG_value>(1);
   more_ids->emplace_back<ULNG_value>(2);
   add_unique(ids, more_ids);
   CHECK(32, "a list without a unique string attribute is appended as it is", ids->size() == 2);
   ids.reset();
}

/**
 * @brief A resource request holding one value of the string attribute arch, by its full name or its shortcut
 * @param lp the request list, created if needed
 * @param name arch or a
 * @param value the requested value
 */
static void
add_arch_request(ocs::tom::ListPtr<ocs::tom::CE> &lp, const char *name, const char *value) {
   if (!lp) {
      lp = ocs::tom::ListPtr<ocs::tom::CE>::create("request");
   }
   lp->emplace_back<CE_name>(name).set<CE_stringval>(value);
}

/**
 * @brief The value of the only arch request of a list, if it holds exactly one request under its full name
 * @param lp the request list
 * @return the requested value, or nullptr if the list does not hold exactly one request named arch
 */
static const char *
only_arch_value(const ocs::tom::ConstList<ocs::tom::CE> lp) {
   if (lp.size() != 1 || strcmp(lp.front().get<CE_name>(), "arch") != 0) {
      return nullptr;
   }
   return lp.front().get<CE_stringval>();
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
   auto master_centry_list = ocs::tom::ListPtr<ocs::tom::CE>::create("master_centry_list");
   ocs::tom::Element<ocs::tom::CE> arch = master_centry_list->emplace_back<CE_name>("arch");
   arch.set<CE_shortcut>("a");
   arch.set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::STR));
   arch.set<CE_relop>(CMPLXEQ_OP);
   arch.set<CE_requestable>(REQU_YES);
   arch.set<CE_consumable>(CONSUMABLE_NO);

   ocs::tom::ListPtr<ocs::tom::CE> request;
   add_arch_request(request, "a", "lx-amd64");
   add_arch_request(request, "arch", "lx-arm64");
   ocs::tom::ListPtr<ocs::tom::AN> answers;
   int ret = centry_list_fill_request(request, &answers, master_centry_list, false, true, false);
   const char *value = only_arch_value(request);
   CHECK(33, "-l a=lx-amd64,arch=lx-arm64 holds arch once, with the later value",
         ret == 0 && value != nullptr && strcmp(value, "lx-arm64") == 0);
   request.reset();
   answers.reset();

   add_arch_request(request, "arch", "lx-amd64");
   add_arch_request(request, "a", "lx-arm64");
   ret = centry_list_fill_request(request, &answers, master_centry_list, false, true, false);
   value = only_arch_value(request);
   CHECK(34, "-l arch=lx-amd64,a=lx-arm64 holds arch once, with the later value",
         ret == 0 && value != nullptr && strcmp(value, "lx-arm64") == 0);
   request.reset();
   answers.reset();
   master_centry_list.reset();

   // qconf -mattr queue slots 4 all.q@h1 all.q@h2 all.q: one request per target, each with a default entry
   auto targets = ocs::tom::ListPtr<ocs::tom::CQ>::create("targets");
   for (const char *target : {"all.q@h1", "all.q@h2", "all.q"}) {
      const auto cqueue = targets->emplace_back<CQ_name>(target);
      const ocs::tom::Element<ocs::tom::AULNG> slot =
         cqueue.emplace_back_in<CQ_job_slots, AULNG_href>(ocs::tom::Host(HOSTREF_DEFAULT));
      slot.set<AULNG_value>(4);
   }
   bool ok = cqueue_xattr_pre_gdi(*targets, &answers);
   const ocs::tom::ConstList<ocs::tom::CQ> target_list(targets);
   const ocs::tom::ConstList<ocs::tom::AULNG> slots =
      target_list.size() == 1 ? target_list.front().get<CQ_job_slots>() : ocs::tom::ConstList<ocs::tom::AULNG>();
   CHECK(35, "the targets all.q@h1, all.q@h2 and all.q become one request for all.q with h1, h2 and the default",
         ok && slots != nullptr && strcmp(target_list.front().get<CQ_name>(), "all.q") == 0 &&
         slots.size() == 3 && slots.find_first<AULNG_href>(ocs::tom::Host("h1")) != nullptr &&
         slots.find_first<AULNG_href>(ocs::tom::Host("h2")) != nullptr &&
         slots.find_first<AULNG_href>(ocs::tom::Host(HOSTREF_DEFAULT)) != nullptr);
   targets.reset();
   answers.reset();

   // the qmaster enforces a limit on a task, and then again on the same task
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(1);
   job->set<JB_submission_time>(1000);
   auto ja_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create();
   ja_task->set<JAT_task_number>(1);
   ja_task->set<JAT_start_time>(2000);
   for (uint64_t now : {3000, 4000}) {
      auto report = ocs::tom::ElementPtr<ocs::tom::JR>::create();
      job_report_init_from_job_with_usage(*report, *job, *ja_task, nullptr, now);
   }
   ocs::tom::ConstList<ocs::tom::UA> usage = ja_task->get<JAT_scaled_usage_list>();
   ocs::tom::ConstElement<ocs::tom::UA> end_time = usage.find_first<UA_name>("end_time");
   CHECK(36, "a limit enforced twice leaves each usage entry once, end_time from the later enforcement",
         usage.size() == 4 && end_time != nullptr && end_time.get<UA_value>() == 4000.0);
   ja_task.reset();
   job.reset();

   // qsub -q '*@@*': a host that belongs to several matching host groups
   auto hgroups = ocs::tom::ListPtr<ocs::tom::HGRP>::create("hgroups");
   const char *const members[][3] = {{"@a", "h1", "h2"}, {"@b", "h2", "h3"}};
   for (const auto &m : members) {
      ocs::tom::Element<ocs::tom::HGRP> hgroup = hgroups->emplace_back<HGRP_name>(ocs::tom::Host(m[0]));
      hgroup.emplace_back_in<HGRP_host_list, HR_name>(m[1]);
      hgroup.emplace_back_in<HGRP_host_list, HR_name>(m[2]);
   }
   ocs::tom::ListPtr<ocs::tom::HR> hosts;
   ok = hgroup_list_find_matching_and_resolve(hgroups, &answers, "@*", &hosts);
   CHECK(37, "resolving @* over @a (h1, h2) and @b (h2, h3) collects h1, h2 and h3 once each",
         ok && hosts->size() == 3);
   hosts.reset();
   hgroups.reset();
   answers.reset();
}

/**
 * @brief Run the scenarios of the duplicate keys in unique attributes
 * @param argc number of arguments
 * @param argv the arguments; the module test frame takes its own options from them
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
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
