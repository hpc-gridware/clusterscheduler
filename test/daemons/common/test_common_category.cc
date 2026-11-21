/*___INFO__MARK_BEGIN_NEW__*/
/*************************************************************************
 *
 *  Copyright 2003 Sun Microsystems, Inc.
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Unit tests for category in `daemons/common`
 */

#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>
#include <string>
#include <sys/time.h>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/ocs_Category.h"
#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_Element.h"

/** @brief One row of the category test table
 *
 * A category is the set of a job's scheduling-relevant properties, so two jobs
 * with the same category can be scheduled with one decision. Each row here is
 * one job description plus the category string it must produce.
 */
typedef struct {
   int        test_nr;        ///< test number
   uint32_t   type;           ///< the job type
   const char *project;       ///< the job project
   const char *owner;         ///< the job owner
   const char *group;         ///< the job group
   const char *groups;        ///< the job groups
   const char *ckpt;          ///< the checkpointing
   const char *rqs;           ///< the resource quota set
   const char *g_h_r;         ///< global hard requested resources
   const char *g_s_r;         ///< global soft requested resources
   const char *m_h_r;         ///< master hard requested resources
   const char *s_h_r;         ///< slave hard requested resources
   const char *g_h_q;         ///< global hard queue list
   const char *m_h_q;         ///< master hard queue list
   const char *s_h_q;         ///< slave hard queue list
   const char *pe;            ///< the requested pe
   int        is_access_list; ///< if 1, generate an access list
} data_entry_t;

// access list configuration: first token is the list name, rest are members
static const char *AccessList[] = {
   "test2_acc user test_user irgendwas @grp1",
   "test1_acc help user what-ever",
   "test0_acc nothing",
   nullptr
};

// one entry per test; see data_entry_t for field meanings
static data_entry_t tests[] = {
   {1,  128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {2,  128, "my_pr", "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {3,  128, nullptr, "user",         nullptr, nullptr,  "my_check", nullptr, nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {4,  128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {5,  128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  "arch test_arch lic 1 memory 1GB", nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {6,  128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, "arch test_arch lic 1 memory 1GB", nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {7,  128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          "arch test_arch lic 1 memory 1GB", nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {8,  128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB", nullptr,                      nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0},
   {9,  128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          nullptr,                       nullptr,  "my.q@test m1.q@what-ever test@*", nullptr,                       nullptr,    nullptr,     0},
   {10, 128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB", nullptr,                      nullptr,  "my.q@test m1.q@what-ever test@*", nullptr,                       nullptr,    nullptr,     0},
   {11, 128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        "my.q@test m1.q@what-ever test@*", nullptr,    nullptr,     0},
   {12, 128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB", nullptr,                      nullptr,  "my.q@test m1.q@what-ever test@*", "my.q@test m1.q@what-ever test@*", nullptr,    nullptr,     0},
   {13, 128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          "arch test_arch lic 1 memory 1GB", nullptr,  nullptr,                        nullptr,                        nullptr,    "my_pe 1-10", 0},
   {14, 128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB", nullptr,                      nullptr,  "my.q@test m1.q@what-ever test@*", "my.q@test m1.q@what-ever test@*", nullptr,    "my_pe 1-10", 0},
   {15, 128, nullptr, "user",         nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     1},
   {16, 128, "my_pr", "user",         nullptr, nullptr,  "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB", nullptr,                      nullptr,  "my.q@test m1.q@what-ever test@*", "my.q@test m1.q@what-ever test@*", nullptr,    "my_pe 1-10", 1},
   {17, 128, nullptr, "rqs_user",     nullptr, nullptr,  nullptr,   "my_rqs", nullptr,                          nullptr,                          nullptr,                       "mem 1G", nullptr,                        nullptr,                        "slave.q",  nullptr,     0},
   {18, 128, nullptr, "user_not_in_acl", "grp1", "grp1 grp2", nullptr, "my_rqs", nullptr,                      nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     1},

   {-1, 0,   nullptr, nullptr,        nullptr, nullptr,  nullptr,   nullptr,  nullptr,                          nullptr,                          nullptr,                       nullptr,  nullptr,                        nullptr,                        nullptr,    nullptr,     0}
};

// expected category string for each test (index matches tests[])
static const char *result_category[] = {
   "-",                                                                                                                                                                                                                                                                         // 1
   "-P my_pr",                                                                                                                                                                                                                                                                  // 2
   "-ckpt my_check",                                                                                                                                                                                                                                                            // 3
   "-ckpt my_check -P my_pr",                                                                                                                                                                                                                                                   // 4
   "-scope global -hard -l arch=test_arch,lic=1,memory=1GB",                                                                                                                                                                                                                   // 5
   "-scope global -hard -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",                                                                                                                                                                                           // 6
   "-scope global -soft -l arch=test_arch,lic=1,memory=1GB",                                                                                                                                                                                                                   // 7
   "-scope global -hard -l arch=test_arch,lic=1,memory=1GB -scope global -soft -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",                                                                                                                                    // 8
   "-scope global -hard -q m1.q@what-ever,my.q@test,test@*",                                                                                                                                                                                                                   // 9
   "-scope global -hard -q m1.q@what-ever,my.q@test,test@* -scope global -hard -l arch=test_arch,lic=1,memory=1GB -scope global -soft -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",                                                                            // 10
   "-scope master -hard -q m1.q@what-ever,my.q@test,test@*",                                                                                                                                                                                                                   // 11
   "-scope global -hard -q m1.q@what-ever,my.q@test,test@* -scope master -hard -q m1.q@what-ever,my.q@test,test@* -scope global -hard -l arch=test_arch,lic=1,memory=1GB -scope global -soft -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",                     // 12
   "-scope master -hard -l arch=test_arch,lic=1,memory=1GB -pe my_pe 1-10",                                                                                                                                                                                                    // 13
   "-scope global -hard -q m1.q@what-ever,my.q@test,test@* -scope master -hard -q m1.q@what-ever,my.q@test,test@* -scope global -hard -l arch=test_arch,lic=1,memory=1GB -scope global -soft -l arch=test_arch,lic=1,memory=1GB -pe my_pe 1-10 -ckpt my_check -P my_pr",    // 14
   "-U test2_acc,test1_acc",                                                                                                                                                                                                                                                    // 15
   "-U test2_acc,test1_acc -scope global -hard -q m1.q@what-ever,my.q@test,test@* -scope master -hard -q m1.q@what-ever,my.q@test,test@* -scope global -hard -l arch=test_arch,lic=1,memory=1GB -scope global -soft -l arch=test_arch,lic=1,memory=1GB -pe my_pe 1-10 -ckpt my_check -P my_pr", // 16
   "-u rqs_user -scope slave -hard -q slave.q -scope slave -hard -l mem=1G",                                                                                                                                                                                                   // 17
   "-U test2_acc",                                                                                                                                                                                                                                                              // 18
   nullptr
};

/**
 * @brief Build the access lists of AccessList
 * @return the access lists
 */
static ocs::tom::ListPtr<ocs::tom::US> test_create_access() {
   auto access_list = ocs::tom::ListPtr<ocs::tom::US>::create("access");

   if (access_list != nullptr) {
      for (int i = 0; AccessList[i] != nullptr; i++) {
         char *access_cp = strdup(AccessList[i]);
         char *iter_dash = nullptr;

         for (const char *access_str = strtok_r(access_cp, " ", &iter_dash); access_str;
              access_str = strtok_r(nullptr, " ", &iter_dash)) {
            auto acc_elem = ocs::tom::ElementPtr<ocs::tom::US>::create();
            if (acc_elem != nullptr) {
               auto users_ptr = ocs::tom::ListPtr<ocs::tom::UE>::create("user");
               const ocs::tom::List<ocs::tom::UE> users = *users_ptr;
               acc_elem->set<US_name>(access_str);
               acc_elem->set<US_entries>(std::move(users_ptr));
               acc_elem->set<US_consider_with_categories>(true);
               access_list->push_back(std::move(acc_elem));

               for (access_str = strtok_r(nullptr, " ", &iter_dash); access_str;
                    access_str = strtok_r(nullptr, " ", &iter_dash)) {
                  auto user = ocs::tom::ElementPtr<ocs::tom::UE>::create();
                  user->set<UE_name>(access_str);
                  users.push_back(std::move(user));
               }
            }
         }
         sge_free(&access_cp);
      }
   }

   return access_list;
}

/**
 * @brief Build a project list holding one project
 * @param project the project name
 * @return the project list
 */
static ocs::tom::ListPtr<ocs::tom::PR> test_create_project(const char *project) {
   auto project_list = ocs::tom::ListPtr<ocs::tom::PR>::create("projects");
   ocs::tom::Element<ocs::tom::PR> prj = project_list->emplace_back<PR_name>(project);
   prj.set<PR_consider_with_categories>(true);
   return project_list;
}

/**
 * @brief Build a resource quota set list with one rule limiting the user rqs_user
 * @return the resource quota set list
 */
static ocs::tom::ListPtr<ocs::tom::RQS> test_create_rqs() {
   auto rqs_list = ocs::tom::ListPtr<ocs::tom::RQS>::create("my_rqs");
   auto rqs = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   rqs->set<RQS_name>("Test_Name1");
   rqs->set<RQS_enabled>(true);
   auto rule_list = ocs::tom::ListPtr<ocs::tom::RQR>::create("Rule_List");
   auto rule = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
   auto filter = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
   filter->set<RQRF_expand>(true);
   filter->emplace_back_in<RQRF_scope, ST_name>("rqs_user");
   rule->set<RQR_filter_users>(std::move(filter));
   auto limit_list = ocs::tom::ListPtr<ocs::tom::RQRL>::create("limit_list");
   auto limit = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
   limit->set<RQRL_name>("slots");
   limit->set<RQRL_value>("2*$num_proc");
   limit_list->push_back(std::move(limit));
   rule->set<RQR_limit>(std::move(limit_list));
   rule_list->push_back(std::move(rule));
   rqs->set<RQS_rule>(std::move(rule_list));
   rqs_list->push_back(std::move(rqs));
   return rqs_list;
}

/**
 * @brief Build a resource request list from "name value" pairs
 * @param requestStr the blank separated pairs
 * @param count how often the pairs are repeated
 * @return the request list
 */
static ocs::tom::ListPtr<ocs::tom::CE> test_create_request(const char *requestStr, const int count) {
   char *request_cp = nullptr;
   auto requests = ocs::tom::ListPtr<ocs::tom::CE>::create("requests");
   if (requests != nullptr) {
      for (int i = 0; i < count; i++) {
         request_cp = strdup(requestStr);
         char *iter_dash = nullptr;
         for (const char *request_str = strtok_r(request_cp, " ", &iter_dash); request_str;
              request_str = strtok_r(nullptr, " ", &iter_dash)) {
            auto request = ocs::tom::ElementPtr<ocs::tom::CE>::create();
            if (request != nullptr) {
               request->set<CE_name>(request_str);
               request->set<CE_stringval>(strtok_r(nullptr, " ", &iter_dash));
            } else {
               requests.reset();
               goto end;
            }
            requests->push_back(std::move(request));
         }
         sge_free(&request_cp);
         request_cp = nullptr;
      }
   }
end:
   sge_free(&request_cp);
   return requests;
}

/**
 * @brief Build a queue reference list from blank separated queue names
 * @param queueStr the blank separated queue names
 * @param count how often the names are repeated
 * @return the queue reference list
 */
static ocs::tom::ListPtr<ocs::tom::QR> test_create_queue(const char *queueStr, const int count) {
   char *queue_cp = nullptr;
   auto queues = ocs::tom::ListPtr<ocs::tom::QR>::create("queues");
   if (queues != nullptr) {
      for (int i = 0; i < count; i++) {
         queue_cp = strdup(queueStr);
         char *iter_dash = nullptr;
         for (const char *queues_str = strtok_r(queue_cp, " ", &iter_dash); queues_str;
              queues_str = strtok_r(nullptr, " ", &iter_dash)) {
            auto queue = ocs::tom::ElementPtr<ocs::tom::QR>::create();
            if (queue != nullptr) {
               queue->set<QR_name>(queues_str);
            } else {
               queues.reset();
               goto end;
            }
            queues->push_back(std::move(queue));
         }
         sge_free(&queue_cp);
         queue_cp = nullptr;
      }
   }
end:
   sge_free(&queue_cp);
   return queues;
}

/**
 * @brief Give a job the parallel environment requests of "pe range" pairs
 * @param peStr the blank separated pairs
 * @param job_elem the job
 */
static void test_create_pe(const char *peStr, const ocs::tom::Element<ocs::tom::JB> job_elem) {
   char *pe_cp = strdup(peStr);
   char *iter_dash = nullptr;

   for (const char *pe_str = strtok_r(pe_cp, " ", &iter_dash); pe_str; pe_str = strtok_r(nullptr, " ", &iter_dash)) {
      job_elem.set<JB_pe>(pe_str);
      pe_str = strtok_r(nullptr, " ", &iter_dash);
      ocs::tom::ListPtr<ocs::tom::RN> range;
      range_list_parse_from_string(range, nullptr, pe_str, false, false, INF_ALLOWED);
      if (range != nullptr) {
         job_elem.set<JB_pe_range>(std::move(range));
      } else {
         job_elem.set<JB_pe>(nullptr);
         printf("error generating pe object: %s\n", peStr);
      }
   }
   sge_free(&pe_cp);
}

/**
 * @brief Give a job the blank separated supplementary groups
 * @param groupStr the blank separated group names
 * @param job_elem the job
 */
static void test_create_groups(const char *groupStr, const ocs::tom::Element<ocs::tom::JB> job_elem) {
   char *cp = strdup(groupStr);
   char *iter_dash = nullptr;
   for (const char *s = strtok_r(cp, " ", &iter_dash); s; s = strtok_r(nullptr, " ", &iter_dash)) {
      job_elem.emplace_back_in<JB_grp_list, ST_name>(s);
   }
   sge_free(&cp);
}

/**
 * @brief Build the job of a test entry
 * @param test the test entry
 * @param count how often the requests and queues are repeated
 * @return the job, or an empty handle if a part of it could not be built
 */
static ocs::tom::ElementPtr<ocs::tom::JB> test_create_job(const data_entry_t *test, const int count) {
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();

   if (job != nullptr) {
      job->set<JB_type>(test->type);

      if (test->project != nullptr) {
         job->set<JB_project>(test->project);
      }
      if (test->owner != nullptr) {
         job->set<JB_owner>(test->owner);
      }
      if (test->group != nullptr) {
         job->set<JB_group>(test->group);
      }
      if (test->ckpt != nullptr) {
         job->set<JB_checkpoint_name>(test->ckpt);
      }
      if (test->g_h_r != nullptr) {
         if (auto requests = test_create_request(test->g_h_r, count); requests != nullptr) {
            job_set_hard_resource_list(*job, std::move(requests));
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->g_s_r != nullptr) {
         if (auto requests = test_create_request(test->g_s_r, count); requests != nullptr) {
            job_set_soft_resource_list(*job, std::move(requests));
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->m_h_r != nullptr) {
         if (auto requests = test_create_request(test->m_h_r, count); requests != nullptr) {
            job_set_resource_list(*job, std::move(requests), JRS_SCOPE_MASTER, true);
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->s_h_r != nullptr) {
         if (auto requests = test_create_request(test->s_h_r, count); requests != nullptr) {
            job_set_resource_list(*job, std::move(requests), JRS_SCOPE_SLAVE, true);
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->g_h_q != nullptr) {
         if (auto queues = test_create_queue(test->g_h_q, count); queues != nullptr) {
            job_set_hard_queue_list(*job, std::move(queues));
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->m_h_q != nullptr) {
         if (auto queues = test_create_queue(test->m_h_q, count); queues != nullptr) {
            job_set_master_hard_queue_list(*job, std::move(queues));
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->s_h_q != nullptr) {
         if (auto queues = test_create_queue(test->s_h_q, count); queues != nullptr) {
            job_set_queue_list(*job, std::move(queues), JRS_SCOPE_SLAVE, true);
         } else {
            job.reset();
            goto end;
         }
      }
      if (test->pe != nullptr) {
         test_create_pe(test->pe, *job);
      }
      if (test->groups != nullptr) {
         test_create_groups(test->groups, *job);
      }
   }
end:
   return job;
}

/**
 * @brief Build the category string for a test entry and compare it against the expected one
 * @param t the test entry
 * @param expected the expected category string, or nullptr
 * @return true on a match
 */
static bool run_test(const data_entry_t *t, const char *expected) {
   auto job_elem = test_create_job(t, 1);
   if (job_elem == nullptr) {
      printf("failed to create job for test %d\n", t->test_nr);
      return false;
   }

   auto access_list  = (t->is_access_list == 1) ? test_create_access() : ocs::tom::ListPtr<ocs::tom::US>();
   auto project_list = (t->project != nullptr) ? test_create_project(t->project) : ocs::tom::ListPtr<ocs::tom::PR>();
   auto rqs_list     = (t->rqs != nullptr) ? test_create_rqs() : ocs::tom::ListPtr<ocs::tom::RQS>();

   dstring category_str = DSTRING_INIT;
   ocs::Category::build_string(&category_str, *job_elem, access_list, project_list, rqs_list);

   const char *got = sge_dstring_get_string(&category_str);
   bool ok = (expected != nullptr && got != nullptr && strcmp(expected, got) == 0)
          || (expected == nullptr && got == nullptr);

   if (!ok) {
      printf("expected: <%s>\n", expected != nullptr ? expected : "<nullptr>");
      printf("got     : <%s>\n", got      != nullptr ? got      : "<nullptr>");
   }

   sge_dstring_free(&category_str);
   return ok;
}

/**
 * @brief Build a normalized category string for a single mem_free=<val> request
 * @param val the requested value
 * @return the category string
 */
static std::string build_mem_cat(const char *val) {
   auto req = ocs::tom::ListPtr<ocs::tom::CE>::create("r");
   auto e_ptr = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   e_ptr->set<CE_name>("mem_free");
   e_ptr->set<CE_stringval>(val);
   e_ptr->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::MEM));
   e_ptr->set<CE_consumable>(CONSUMABLE_NO);
   const ocs::tom::Element<ocs::tom::CE> e = req->push_back(std::move(e_ptr));
   ocs::tom::ListPtr<ocs::tom::AN> al;
   centry_fill_and_check(e, &al, false, false);
   al.reset();
   dstring cat = DSTRING_INIT;
   sge_unparse_resource_list_dstring(&cat, *req, "-scope global -hard -l");
   std::string result(sge_dstring_get_string(&cat) ? sge_dstring_get_string(&cat) : "");
   sge_dstring_free(&cat);
   return result;
}

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_common_category");
   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   char label[64];
   int id = 1;

   printf("\n--- category string tests ---\n");
   for (int i = 0; tests[i].test_nr != -1; i++) {
      snprintf(label, sizeof(label), "cat[%d]", tests[i].test_nr);
      CHECK(id++, label, run_test(&tests[i], result_category[i]));
   }

   printf("\n--- memory normalization tests ---\n");
   // T19: 1K and 1024 are the same byte count → identical category string
   CHECK(id++, "mem_free=1K and mem_free=1024 produce identical category",
         build_mem_cat("1K") == build_mem_cat("1024"));
   // T20: 1k (1000 bytes) and 1K (1024 bytes) differ → distinct category strings
   CHECK(id++, "mem_free=1k and mem_free=1K produce distinct categories",
         build_mem_cat("1k") != build_mem_cat("1K"));

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
