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
 * @brief Module test of the attribute modification helpers of the qmaster (sge_utility_qmaster)
 */

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

#include "gdi/ocs_gdi_Command.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_ARA.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "msg_qmaster.h"
#include "sge_utility_qmaster.h"

/**
 * @brief Report the result of one check
 * @param number the check's number
 * @param what what the check asserts
 * @param ok whether it holds
 * @return 0 if it holds, 1 otherwise
 */
static int
check(const int number, const char *what, const bool ok) {
   printf("%s CHECK %d: %s\n", ok ? "ok    " : "FAILED", number, what);
   return ok ? 0 : 1;
}

/**
 * @brief An advance reservation whose access list holds the given user names
 * @param names the names, in this order
 * @param with_list false for an advance reservation without an access list
 * @return the advance reservation
 */
static ocs::tom::ElementPtr<ocs::tom::AR>
make_ar(const std::initializer_list<const char *> names, const bool with_list = true) {
   auto ar = ocs::tom::ElementPtr<ocs::tom::AR>::create();
   if (with_list) {
      const auto acl = ocs::tom::get_or_create<AR_acl_list>(ar.get(), "acl");
      for (const char *name : names) {
         acl.emplace_back<ARA_name>(name);
      }
   }
   return ar;
}

/**
 * @brief The user names of an advance reservation's access list
 * @param ar the advance reservation
 * @return the names separated by commas, "null" for an unset list
 */
static std::string
acl_of(const ocs::tom::ConstElement<ocs::tom::AR> ar) {
   const auto acl = ar.get<AR_acl_list>();
   if (!acl) {
      return "null";
   }
   std::string names;
   for (const auto ep : acl) {
      if (!names.empty()) {
         names += ",";
      }
      names += ep.get<ARA_name>();
   }
   return names;
}

/**
 * @brief A resource quota set whose rule list holds rules with the given names
 * @param names the rule names, in this order
 * @return the resource quota set
 */
static ocs::tom::ElementPtr<ocs::tom::RQS>
make_rqs(const std::initializer_list<const char *> names) {
   auto rqs = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   const auto rules = ocs::tom::get_or_create<RQS_rule>(rqs.get(), "rules");
   for (const char *name : names) {
      rules.emplace_back<RQR_name>(name);
   }
   return rqs;
}

/**
 * @brief The rule names of a resource quota set
 * @param rqs the resource quota set
 * @return the names separated by commas, "null" for an unset rule list
 */
static std::string
rules_of(const ocs::tom::ConstElement<ocs::tom::RQS> rqs) {
   const auto rules = rqs.get<RQS_rule>();
   if (!rules) {
      return "null";
   }
   std::string names;
   for (const auto ep : rules) {
      if (!names.empty()) {
         names += ",";
      }
      names += ep.get<RQR_name>();
   }
   return names;
}

/**
 * @brief Whether an answer list holds exactly the given number of missing key answers and nothing else
 * @param answers the answer list
 * @param count the expected number
 * @return true if every answer is the error MSG_OBJECT_VALUEMISSING with STATUS_ESEMANTIC
 */
static bool
only_missing_keys(const ocs::tom::ListPtr<ocs::tom::AN> &answers, const std::size_t count) {
   const auto list = answers.get();
   if (list.size() != count) {
      return false;
   }
   for (const auto answer : list) {
      const char *text = answer.get<AN_text>();
      if (text == nullptr || strcmp(text, MSG_OBJECT_VALUEMISSING) != 0 ||
          answer.get<AN_status>() != STATUS_ESEMANTIC ||
          answer.get<AN_quality>() != static_cast<uint32_t>(ANSWER_QUALITY_ERROR)) {
         return false;
      }
   }
   return true;
}

/**
 * @brief replace_sublist<F>() does what the run-time attr_mod_sub_list() did for the AR and RQS callers
 * @return the number of failed checks
 */
static int
test_replace_sublist() {
   int failed = 0;

   // SET: the request's list replaces the object's as a copy, the request keeps its own
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a"});
      auto request = make_ar({"b", "c"});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(), ocs::gdi::SubCommand::SET,
                                                    "user_lists", "advance_reservation");
      failed += check(1, "SET replaces the sub-list by a copy of the request's and answers nothing",
                      ret && !answers && acl_of(dst.get()) == "b,c" && acl_of(request.get()) == "b,c");
   }

   // SET: an empty list and no list both leave the object without a list; SET_ALL is a SET
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a"});
      auto request = make_ar({});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(), ocs::gdi::SubCommand::SET,
                                                    "user_lists", "advance_reservation");
      auto dst2 = make_ar({"a"});
      auto request2 = make_ar({}, false);
      const bool ret2 = replace_sublist<AR_acl_list>(&answers, dst2.get(), request2.get(),
                                                     ocs::gdi::SubCommand::SET_ALL, "user_lists",
                                                     "advance_reservation");
      failed += check(2, "SET of an empty list or of none stores an unset list",
                      ret && ret2 && !answers && acl_of(dst.get()) == "null" && acl_of(dst2.get()) == "null");
   }

   // a request without the list field changes nothing and answers nothing
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a"});
      auto request = ocs::tom::ElementPtr<ocs::tom::AR>::create(ocs::tom::layout<AR_name>());
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(), ocs::gdi::SubCommand::SET,
                                                    "user_lists", "advance_reservation");
      failed += check(3, "a request without the sub-list gives false and leaves the object as it was",
                      !ret && !answers && acl_of(dst.get()) == "a");
   }

   // a keyed sub-command: one missing key per held element, or per request element if none is held
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a", "b", "c"});
      auto request = make_ar({"x"});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(),
                                                    ocs::gdi::SubCommand::CHANGE, "user_lists",
                                                    "advance_reservation");
      failed += check(4, "CHANGE answers a missing key for every held element and changes nothing",
                      !ret && only_missing_keys(answers, 3) && acl_of(dst.get()) == "a,b,c" &&
                      acl_of(request.get()) == "x");

      ocs::tom::ListPtr<ocs::tom::AN> answers2;
      auto dst2 = make_ar({}, false);
      auto request2 = make_ar({"x", "y"});
      const bool ret2 = replace_sublist<AR_acl_list>(&answers2, dst2.get(), request2.get(),
                                                     ocs::gdi::SubCommand::APPEND, "user_lists",
                                                     "advance_reservation");
      failed += check(5, "APPEND to an unset list answers a missing key for every request element",
                      !ret2 && only_missing_keys(answers2, 2) && acl_of(dst2.get()) == "null");

      ocs::tom::ListPtr<ocs::tom::AN> answers3;
      auto dst3 = make_ar({});
      auto request3 = make_ar({});
      const bool ret3 = replace_sublist<AR_acl_list>(&answers3, dst3.get(), request3.get(),
                                                     ocs::gdi::SubCommand::SET_ALL | ocs::gdi::SubCommand::REMOVE,
                                                     "user_lists", "advance_reservation");
      failed += check(6, "a keyed sub-command with an empty request list answers nothing and unsets an empty list",
                      ret3 && !answers3 && acl_of(dst3.get()) == "null");
   }

   // the number of missing keys follows the held elements, not the larger of the two lists
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a"});
      auto request = make_ar({"x", "y", "z"});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(),
                                                    ocs::gdi::SubCommand::CHANGE, "user_lists",
                                                    "advance_reservation");
      failed += check(7, "CHANGE with one held and three request elements answers exactly one missing key",
                      !ret && only_missing_keys(answers, 1) && acl_of(dst.get()) == "a" &&
                      acl_of(request.get()) == "x,y,z");
   }

   // a held list that is set but empty counts as no held element: one missing key per request element.
   // make_ar({}) stores such a list: get_or_create() sets the field to an empty list, nothing normalizes it.
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({});
      const bool held_set_empty = dst->get<AR_acl_list>() &&
                                  acl_of(dst.get()).empty();
      auto request = make_ar({"x", "y"});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(),
                                                    ocs::gdi::SubCommand::CHANGE, "user_lists",
                                                    "advance_reservation");
      failed += check(8, "CHANGE on a set but empty held list answers a missing key for every request element",
                      held_set_empty && !ret && only_missing_keys(answers, 2) && acl_of(dst.get()).empty());
   }

   // a keyed sub-command with an empty request list keeps a held list that has elements
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a"});
      auto request = make_ar({});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(),
                                                    ocs::gdi::SubCommand::CHANGE, "user_lists",
                                                    "advance_reservation");
      failed += check(9, "CHANGE with an empty request list gives true, answers nothing and keeps the held list",
                      ret && !answers && acl_of(dst.get()) == "a");
   }

   // SET_ALL combined with a keyed flag is keyed: nothing is replaced
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_ar({"a", "b"});
      auto request = make_ar({"x"});
      const bool ret = replace_sublist<AR_acl_list>(&answers, dst.get(), request.get(),
                                                    ocs::gdi::SubCommand::SET_ALL | ocs::gdi::SubCommand::CHANGE,
                                                    "user_lists", "advance_reservation");
      failed += check(10, "SET_ALL|CHANGE answers missing keys, gives false and replaces nothing",
                      !ret && only_missing_keys(answers, 2) && acl_of(dst.get()) == "a,b");
   }

   // the resource quota set path: SET_ALL replaces the rules, SET_ALL|CHANGE answers a missing key per rule
   {
      ocs::tom::ListPtr<ocs::tom::AN> answers;
      auto dst = make_rqs({"r1"});
      auto request = make_rqs({"r2", "r3"});
      const bool ret = replace_sublist<RQS_rule>(&answers, dst.get(), request.get(), ocs::gdi::SubCommand::SET_ALL,
                                                 "limit", "resource_quota");
      ocs::tom::ListPtr<ocs::tom::AN> answers2;
      auto dst2 = make_rqs({"r1", "r2"});
      auto request2 = make_rqs({"r3"});
      const bool ret2 = replace_sublist<RQS_rule>(&answers2, dst2.get(), request2.get(),
                                                  ocs::gdi::SubCommand::SET_ALL | ocs::gdi::SubCommand::CHANGE,
                                                  "limit", "resource_quota");
      failed += check(11, "RQS_rule: SET_ALL replaces the rules by a copy, SET_ALL|CHANGE only answers missing keys",
                      ret && !answers && rules_of(dst.get()) == "r2,r3" && rules_of(request.get()) == "r2,r3" &&
                      !ret2 && only_missing_keys(answers2, 2) && rules_of(dst2.get()) == "r1,r2");
   }

   return failed;
}

/**
 * @brief Run the module test
 * @param argc unused
 * @param argv unused
 * @return the number of failed checks
 */
int
main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   const int failed = test_replace_sublist();

   if (failed == 0) {
      printf("PASS: test solved!\n");
   } else {
      printf("FAILED: %d test(s) NOT solved!\n", failed);
   }
   return failed;
}
