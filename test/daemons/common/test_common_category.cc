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

#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>
#include <sys/time.h>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_multitype.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_Category.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_range.h"

typedef struct {
   int        test_nr;        //< test number
   uint32_t   type;           //< the job type
   const char *project;       //< the job project
   const char *owner;         //< the job owner
   const char *group;         //< the job group
   const char *groups;        //< the job groups
   const char *ckpt;          //< the checkpointing
   const char *rqs;           //< the resource quota set
   const char *g_h_r;         //< global hard requested resources
   const char *g_s_r;         //< global soft requested resources
   const char *m_h_r;         //< master hard requested resources
   const char *s_h_r;         //< slave hard requested resources
   const char *g_h_q;         //< global hard queue list
   const char *m_h_q;         //< master hard queue list
   const char *s_h_q;         //< slave hard queue list
   const char *pe;            //< the requested pe
   int        is_access_list; //< if 1, generate an access list
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

static lList *test_create_access()
{
   lList *access_list = lCreateList("access", US_Type);

   if (access_list != nullptr) {
      for (int i = 0; AccessList[i] != nullptr; i++) {
         char *access_cp = strdup(AccessList[i]);
         char *iter_dash = nullptr;

         for (const char *access_str = strtok_r(access_cp, " ", &iter_dash); access_str; access_str = strtok_r(nullptr, " ", &iter_dash)) {
            lListElem *acc_elem = lCreateElem(US_Type);
            if (acc_elem != nullptr) {
               lList *users = lCreateList("user", UE_Type);
               lSetString(acc_elem, US_name, access_str);
               lSetList(acc_elem, US_entries, users);
               lSetBool(acc_elem, US_consider_with_categories, true);
               lAppendElem(access_list, acc_elem);

               for (access_str = strtok_r(nullptr, " ", &iter_dash); access_str; access_str = strtok_r(nullptr, " ", &iter_dash)) {
                  lListElem *user = lCreateElem(UE_Type);
                  lSetString(user, UE_name, access_str);
                  lAppendElem(users, user);
               }
            }
         }
         sge_free(&access_cp);
      }
   }

   return access_list;
}

static lList *test_create_project(const char *project)
{
   lList *project_list = nullptr;
   lListElem *prj = lAddElemStr(&project_list, PR_name, project, PR_Type);
   lSetBool(prj, PR_consider_with_categories, true);
   return project_list;
}

static lList *test_create_rqs()
{
   lList *rqs_list = lCreateList("my_rqs", RQS_Type);
   lListElem *rqs = lCreateElem(RQS_Type);
   lSetString(rqs, RQS_name, "Test_Name1");
   lSetBool(rqs, RQS_enabled, true);
   lList *rule_list = lCreateList("Rule_List", RQR_Type);
   lListElem *rule = lCreateElem(RQR_Type);
   lListElem *filter = lCreateElem(RQRF_Type);
   lSetBool(filter, RQRF_expand, true);
   lAddSubStr(filter, ST_name, "rqs_user", RQRF_scope, ST_Type);
   lSetObject(rule, RQR_filter_users, filter);
   lList *limit_list = lCreateList("limit_list", RQRL_Type);
   lListElem *limit = lCreateElem(RQRL_Type);
   lSetString(limit, RQRL_name, "slots");
   lSetString(limit, RQRL_value, "2*$num_proc");
   lAppendElem(limit_list, limit);
   lSetList(rule, RQR_limit, limit_list);
   lAppendElem(rule_list, rule);
   lSetList(rqs, RQS_rule, rule_list);
   lAppendElem(rqs_list, rqs);
   return rqs_list;
}

static lList *test_create_request(const char *requestStr, const int count)
{
   char *request_cp = nullptr;
   lList *requests = lCreateList("requests", CE_Type);
   if (requests != nullptr) {
      for (int i = 0; i < count; i++) {
         request_cp = strdup(requestStr);
         char *iter_dash = nullptr;
         for (const char *request_str = strtok_r(request_cp, " ", &iter_dash); request_str; request_str = strtok_r(nullptr, " ", &iter_dash)) {
            lListElem *request = lCreateElem(CE_Type);
            if (request != nullptr) {
               lSetString(request, CE_name, request_str);
               lSetString(request, CE_stringval, strtok_r(nullptr, " ", &iter_dash));
            } else {
               lFreeList(&requests);
               goto end;
            }
            lAppendElem(requests, request);
         }
         sge_free(&request_cp);
         request_cp = nullptr;
      }
   }
end:
   sge_free(&request_cp);
   return requests;
}

static lList *test_create_queue(const char *queueStr, const int count)
{
   char *queue_cp = nullptr;
   lList *queues = lCreateList("queues", QR_Type);
   if (queues != nullptr) {
      for (int i = 0; i < count; i++) {
         queue_cp = strdup(queueStr);
         char *iter_dash = nullptr;
         for (const char *queues_str = strtok_r(queue_cp, " ", &iter_dash); queues_str; queues_str = strtok_r(nullptr, " ", &iter_dash)) {
            lListElem *queue = lCreateElem(QR_Type);
            if (queue != nullptr) {
               lSetString(queue, QR_name, queues_str);
            } else {
               lFreeList(&queues);
               goto end;
            }
            lAppendElem(queues, queue);
         }
         sge_free(&queue_cp);
         queue_cp = nullptr;
      }
   }
end:
   sge_free(&queue_cp);
   return queues;
}

static void test_create_pe(const char *peStr, lListElem *job_elem)
{
   char *pe_cp = strdup(peStr);
   char *iter_dash = nullptr;

   for (const char *pe_str = strtok_r(pe_cp, " ", &iter_dash); pe_str; pe_str = strtok_r(nullptr, " ", &iter_dash)) {
      lSetString(job_elem, JB_pe, pe_str);
      pe_str = strtok_r(nullptr, " ", &iter_dash);
      lList *range = nullptr;
      range_list_parse_from_string(&range, nullptr, pe_str, false, false, INF_ALLOWED);
      if (range != nullptr) {
         lSetList(job_elem, JB_pe_range, range);
      } else {
         lSetString(job_elem, JB_pe, nullptr);
         printf("error generating pe object: %s\n", peStr);
      }
   }
   sge_free(&pe_cp);
}

static void test_create_groups(const char *groupStr, lListElem *job_elem)
{
   char *cp = strdup(groupStr);
   char *iter_dash = nullptr;
   for (const char *s = strtok_r(cp, " ", &iter_dash); s; s = strtok_r(nullptr, " ", &iter_dash)) {
      lAddSubStr(job_elem, ST_name, s, JB_grp_list, ST_Type);
   }
   sge_free(&cp);
}

static lListElem *test_create_job(const data_entry_t *test, const int count)
{
   lListElem *job = lCreateElem(JB_Type);

   if (job != nullptr) {
      lSetUlong(job, JB_type, test->type);

      if (test->project != nullptr) {
         lSetString(job, JB_project, test->project);
      }
      if (test->owner != nullptr) {
         lSetString(job, JB_owner, test->owner);
      }
      if (test->group != nullptr) {
         lSetString(job, JB_group, test->group);
      }
      if (test->ckpt != nullptr) {
         lSetString(job, JB_checkpoint_name, test->ckpt);
      }
      if (test->g_h_r != nullptr) {
         if (lList *requests = test_create_request(test->g_h_r, count); requests != nullptr) {
            job_set_hard_resource_list(job, requests);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->g_s_r != nullptr) {
         if (lList *requests = test_create_request(test->g_s_r, count); requests != nullptr) {
            job_set_soft_resource_list(job, requests);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->m_h_r != nullptr) {
         if (lList *requests = test_create_request(test->m_h_r, count); requests != nullptr) {
            job_set_resource_list(job, requests, JRS_SCOPE_MASTER, true);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->s_h_r != nullptr) {
         if (lList *requests = test_create_request(test->s_h_r, count); requests != nullptr) {
            job_set_resource_list(job, requests, JRS_SCOPE_SLAVE, true);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->g_h_q != nullptr) {
         if (lList *queues = test_create_queue(test->g_h_q, count); queues != nullptr) {
            job_set_hard_queue_list(job, queues);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->m_h_q != nullptr) {
         if (lList *queues = test_create_queue(test->m_h_q, count); queues != nullptr) {
            job_set_master_hard_queue_list(job, queues);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->s_h_q != nullptr) {
         if (lList *queues = test_create_queue(test->s_h_q, count); queues != nullptr) {
            job_set_queue_list(job, queues, JRS_SCOPE_SLAVE, true);
         } else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->pe != nullptr) {
         test_create_pe(test->pe, job);
      }
      if (test->groups != nullptr) {
         test_create_groups(test->groups, job);
      }
   }
end:
   return job;
}

// builds the category string for t and compares it against expected; returns true on match
static bool run_test(const data_entry_t *t, const char *expected)
{
   lListElem *job_elem = test_create_job(t, 1);
   if (job_elem == nullptr) {
      printf("failed to create job for test %d\n", t->test_nr);
      return false;
   }

   lList *access_list  = (t->is_access_list == 1) ? test_create_access()          : nullptr;
   lList *project_list = (t->project != nullptr)  ? test_create_project(t->project) : nullptr;
   lList *rqs_list     = (t->rqs != nullptr)      ? test_create_rqs()              : nullptr;

   dstring category_str = DSTRING_INIT;
   ocs::Category::build_string(&category_str, job_elem, access_list, project_list, rqs_list);

   const char *got = sge_dstring_get_string(&category_str);
   bool ok = (expected != nullptr && got != nullptr && strcmp(expected, got) == 0)
          || (expected == nullptr && got == nullptr);

   if (!ok) {
      printf("expected: <%s>\n", expected != nullptr ? expected : "<nullptr>");
      printf("got     : <%s>\n", got      != nullptr ? got      : "<nullptr>");
   }

   sge_dstring_free(&category_str);
   lFreeElem(&job_elem);
   lFreeList(&access_list);
   lFreeList(&project_list);
   lFreeList(&rqs_list);
   return ok;
}

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

int main(int /*argc*/, char * /*argv*/[])
{
   DENTER_MAIN(TOP_LAYER, "test_common_category");
   component_set_daemonized(true);
   lInit(nmv);

   char label[64];
   int id = 1;

   printf("\n--- category string tests ---\n");
   for (int i = 0; tests[i].test_nr != -1; i++) {
      snprintf(label, sizeof(label), "cat[%d]", tests[i].test_nr);
      CHECK(id++, label, run_test(&tests[i], result_category[i]));
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
