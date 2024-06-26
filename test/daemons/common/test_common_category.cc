/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>
#include <sys/time.h>

#include "uti/sge_profiling.h"

#include "cull/cull_multitype.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_range.h"

#include "category.h"

typedef struct {
   int        test_nr;                 /* test number */
   u_long32   type;                    /* the job type */
   const char *project;                /* the job project */
   const char *owner;                  /* the job owner */
   const char *group;                  /* the job group */
   const char *checkpointing;          /* the checkpointing */
   const char *rqs;                    /* the resource quota set */
   const char *hard_resource_list;     /* the hard requested resources */
   const char *soft_resource_list;     /* the soft requested resources */
   const char *hard_queue_list;        /* the hard requested queues */
   const char *hard_master_queue_list; /* hard master queue list */
   const char *pe;                     /* the requested pe */
   int        is_access_list;          /* if 1, generate a access list */
}data_entry_t;


/*
 * This describes the acces list configuration. Each line is one acces list. The first
 * item is the access_list name, the others are the users in the access list
 */
static const char *AccessList[] = {"test2_acc user test_user irgendwas",
                             "test1_acc help user what-ever",
                             "test0_acc nothing",
                             nullptr};

/**
 *
 * Test setup:
 * Each line specifies one test. se data_entry_t documentation the meaning of each element. Please ensure
 * that for each line you have also 1 result_category line with the expected category string
 *
 **/
static data_entry_t tests[] = { {1, 128, nullptr, "user", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0},
                                {2, 128, "my_pr", "user", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0},
                                {3, 128, nullptr, "user", nullptr, "my_check", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0},
                                {4, 128, "my_pr", "user", nullptr, "my_check", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0},
                                {5, 128, nullptr, "user", nullptr, nullptr, nullptr, "arch test_arch lic 1 memory 1GB", nullptr, nullptr, nullptr, nullptr, 0},
                                {6, 128, "my_pr", "user", nullptr, "my_check", nullptr, "arch test_arch lic 1 memory 1GB", nullptr, nullptr, nullptr, nullptr, 0},
                                {7, 128, nullptr, "user", nullptr, nullptr, nullptr, nullptr, "arch test_arch lic 1 memory 1GB", nullptr, nullptr, nullptr, 0},
                                {8, 128, "my_pr", "user", nullptr, "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB",
                                    nullptr, nullptr, nullptr, 0},
                                {9, 128, nullptr, "user", nullptr, nullptr, nullptr, nullptr, nullptr, "my.q@test m1.q@what-ever test@*", nullptr, nullptr, 0},
                                {10, 128, "my_pr", "user", nullptr, "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB",
                                    "my.q@test m1.q@what-ever test@*", nullptr, nullptr, 0},
                                {11, 128, nullptr, "user", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "my.q@test m1.q@what-ever test@*", nullptr, 0},
                                {12, 128, "my_pr", "user", nullptr, "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB",
                                    "my.q@test m1.q@what-ever test@*", "my.q@test m1.q@what-ever test@*", nullptr, 0},
                                {13, 128, nullptr, "user", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "my_pe 1-10", 0},
                                {14, 128, "my_pr", "user", nullptr, "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB",
                                    "my.q@test m1.q@what-ever test@*", "my.q@test m1.q@what-ever test@*", "my_pe 1-10", 0},
                                {15, 128, nullptr, "user", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 1},
                                {16, 128, "my_pr", "user", nullptr, "my_check", nullptr, "arch test_arch lic 1 memory 1GB", "arch test_arch lic 1 memory 1GB",
                                    "my.q@test m1.q@what-ever test@*", "my.q@test m1.q@what-ever test@*", "my_pe 1-10", 1},
                                {17, 128, nullptr, "rqs_user", nullptr, nullptr, "my_rqs", nullptr, nullptr, nullptr, nullptr, nullptr, 0},
                                {18, 128, nullptr, "user", nullptr, nullptr, "my_rqs", nullptr, nullptr, nullptr, nullptr, nullptr, 0},

/* stop entry */                {-1,  0, nullptr,   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0}
                              };  

/**
 * result strings
 **/
static const char *result_category[] = { nullptr,
                                   "-P my_pr",
                                   "-ckpt my_check",
                                   "-ckpt my_check -P my_pr",
                                   "-l arch=test_arch,lic=1,memory=1GB",
                                   "-l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",
                                   "-soft -l arch=test_arch,lic=1,memory=1GB",
                                   "-l arch=test_arch,lic=1,memory=1GB -soft -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",
                                   "-q m1.q@what-ever,my.q@test,test@*",
                                   "-q m1.q@what-ever,my.q@test,test@* -l arch=test_arch,lic=1,memory=1GB -soft -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",
                                   "-masterq m1.q@what-ever,my.q@test,test@*",
                                   "-q m1.q@what-ever,my.q@test,test@* -masterq m1.q@what-ever,my.q@test,test@* -l arch=test_arch,lic=1,memory=1GB -soft -l arch=test_arch,lic=1,memory=1GB -ckpt my_check -P my_pr",
                                   "-pe my_pe 1-10",
                                   "-q m1.q@what-ever,my.q@test,test@* -masterq m1.q@what-ever,my.q@test,test@* -l arch=test_arch,lic=1,memory=1GB -soft -l arch=test_arch,lic=1,memory=1GB -pe my_pe 1-10 -ckpt my_check -P my_pr",
                                   "-U test2_acc,test1_acc",
                                   "-U test2_acc,test1_acc -q m1.q@what-ever,my.q@test,test@* -masterq m1.q@what-ever,my.q@test,test@* -l arch=test_arch,lic=1,memory=1GB -soft -l arch=test_arch,lic=1,memory=1GB -pe my_pe 1-10 -ckpt my_check -P my_pr",
                                   "-u rqs_user",
                                   nullptr,
                                   nullptr
                                 };

/****** test_category/test_create_access() *************************************
*  NAME
*     test_create_access() -- creates an access list from AccessList
*
*  SYNOPSIS
*     lList* test_create_access() 
*
*  RESULT
*     lList* - nullptr or valid acces list
*
*  NOTES
*     MT-NOTE: test_create_access() is not MT safe 
*
*******************************************************************************/
static lList *test_create_access()
{
   lList *access_list = nullptr;

   access_list = lCreateList("access", US_Type);
   
   if (access_list != nullptr) {
      int i;

      for (i = 0; AccessList[i] != nullptr; i++) {
         char *access_cp = nullptr;
         char *access_str = nullptr;
         char *iter_dash = nullptr;

         access_cp = strdup(AccessList[i]);
        
         for (access_str = strtok_r(access_cp, " ", &iter_dash); access_str; access_str = strtok_r(nullptr, " ", &iter_dash)) {
            lListElem *acc_elem = nullptr;

            acc_elem = lCreateElem(US_Type);
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
         if (access_cp != nullptr) {
            sge_free(&access_cp);
         }
      }
   }
   return access_list;
}

/****** test_category/test_create_project() *************************************
*  NAME
*     test_create_project() -- creates an project list with the passed project name
*
*  SYNOPSIS
*     lList* test_create_project(const char *project) 
*
*  RESULT
*     lList* - nullptr or valid project list
*
*  NOTES
*     MT-NOTE: test_create_project() is not MT safe 
*
*******************************************************************************/
static lList *test_create_project(const char *project)
{
   lList *project_list = nullptr;
   lListElem *prj;
   prj = lAddElemStr(&project_list, PR_name, project, PR_Type);
   lSetBool(prj, PR_consider_with_categories, true);
   return project_list;
}

static lList *test_create_rqs()
{
   lList* rqs_list = lCreateList("my_rqs", RQS_Type);
   lListElem* rqs;
   lList* rule_list;
   lListElem* rule;
   lListElem* filter;
   lListElem* limit;
   lList * limit_list;
   
   rqs = lCreateElem(RQS_Type);
   lSetString(rqs, RQS_name, "Test_Name1");
   lSetBool(rqs, RQS_enabled, true);
   rule_list = lCreateList("Rule_List", RQR_Type);

   rule = lCreateElem(RQR_Type);
      filter = lCreateElem(RQRF_Type);
      lSetBool(filter, RQRF_expand, true);
      lAddSubStr(filter, ST_name, "rqs_user", RQRF_scope, ST_Type);

      lSetObject(rule, RQR_filter_users, filter);

      limit_list = lCreateList("limit_list", RQRL_Type);
      limit = lCreateElem(RQRL_Type);
      lSetString(limit, RQRL_name, "slots");
      lSetString(limit, RQRL_value, "2*$num_proc");
      lAppendElem(limit_list, limit);
      lSetList(rule, RQR_limit, limit_list);
   lAppendElem(rule_list, rule);
   lSetList(rqs, RQS_rule, rule_list);
   lAppendElem(rqs_list, rqs);


   return rqs_list;
}

/****** test_category/test_create_request() ************************************
*  NAME
*     test_create_request() -- creats a request list from the request string
*
*  SYNOPSIS
*     lList* test_create_request(const char *requestStr, int count) 
*
*  INPUTS
*     const char *requestStr - request string
*     int count              - how many times the request string should be used
*                              (multiplyer, needs to between 1 and ...)
*
*  RESULT
*     lList* - nullptr or request list
*
*  NOTES
*     MT-NOTE: test_create_request() is MT safe 
*
*******************************************************************************/
static lList *test_create_request(const char *requestStr, int count) 
{
   lList *requests = nullptr;
   char *request_cp = nullptr;
   char *iter_dash = nullptr;

   requests = lCreateList("requests", CE_Type);

   if (requests != nullptr) {
       int i;
       for (i = 0; i < count; i++) {
          char *request_str;

          request_cp = strdup(requestStr);
          
          for (request_str = strtok_r(request_cp, " ", &iter_dash); request_str; request_str = strtok_r(nullptr, " ", &iter_dash)) {
            lListElem *request = nullptr;
            
            request = lCreateElem(CE_Type);
            
            if (request != nullptr) {
               lSetString(request, CE_name, request_str);
               lSetString(request, CE_stringval, strtok_r(nullptr, " ", &iter_dash));
            }
            else {
               lFreeList(&requests);
               goto end;
            }
            lAppendElem(requests, request); 
          }
         if (request_cp != nullptr) {
            sge_free(&request_cp);
         }
       }
   }
end:
   if (request_cp != nullptr) {
      sge_free(&request_cp);
   }
   return requests;
}

/****** test_category/test_create_queue() **************************************
*  NAME
*     test_create_queue() -- creates a request queue list from the queue string
*
*  SYNOPSIS
*     lList* test_create_queue(const char *queueStr, int count) 
*
*  INPUTS
*     const char *queueStr - the queue string used as a bases
*     int count            -  how many times the request string should be used
*                             (multiplyer, needs to between 1 and ...)
*
*  RESULT
*     lList* - nullptr or valid queue request list
*
*  NOTES
*     MT-NOTE: test_create_queue() is MT safe 
*
*******************************************************************************/
static lList *test_create_queue(const char *queueStr, int count) 
{
   lList *queues = nullptr;
   char *queue_cp = nullptr;
   char *iter_dash = nullptr;

   queues = lCreateList("queues", QR_Type);

   if (queues != nullptr) {
       int i;
       for (i = 0; i < count; i++) {
          char *queues_str;
          queue_cp = strdup(queueStr);
          for (queues_str = strtok_r(queue_cp, " ", &iter_dash); queues_str; queues_str = strtok_r(nullptr, " ", &iter_dash)) {
            lListElem *queue = nullptr;
            
            queue = lCreateElem(QR_Type);
            
            if (queue != nullptr) {
               lSetString(queue, QR_name, queues_str);
            }
            else {
               lFreeList(&queues);
               goto end;
            }
            lAppendElem(queues, queue); 
          }
          if (queue_cp != nullptr) {
            sge_free(&queue_cp);
          }
       }
   }
end:
   if (queue_cp != nullptr) {
      sge_free(&queue_cp); 
   }
   return queues;
}

/****** test_category/test_create_pe() *****************************************
*  NAME
*     test_create_pe() -- adds a pe object to the job
*
*  SYNOPSIS
*     void test_create_pe(const char *peStr, lListElem *job_elem) 
*
*  INPUTS
*     const char *peStr   - string representation of the pe object
*     lListElem *job_elem - job object
*
*  NOTES
*     MT-NOTE: test_create_pe() is MT safe 
*
*******************************************************************************/
static void test_create_pe(const char *peStr, lListElem *job_elem) 
{
   lList *range= nullptr;
   char *pe_cp = strdup(peStr);
   char *iter_dash = nullptr;
   char *pe_str;

    for (pe_str = strtok_r(pe_cp, " ", &iter_dash); pe_str; pe_str= strtok_r(nullptr, " ", &iter_dash)) {

      lSetString(job_elem, JB_pe, pe_str);
      
      pe_str= strtok_r(nullptr, " ", &iter_dash);
      range_list_parse_from_string(&range, nullptr, pe_str, false, false, INF_ALLOWED);
      if (range != nullptr) {
         lSetList(job_elem, JB_pe_range, range);             
      }
      else {
         lSetString(job_elem, JB_pe, nullptr);
         printf("error generating pe object: %s\n", peStr);

      }

    }
   if (pe_cp != nullptr) {
      sge_free(&pe_cp); 
   }
}


/****** test_category/test_create_job() ****************************************
*  NAME
*     test_create_job() -- creates a job object
*
*  SYNOPSIS
*     lListElem* test_create_job(data_entry_t *test, int count) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     data_entry_t *test - string representation of a job
*     int count          - multiplier for the requests
*
*  RESULT
*     lListElem* - nullptr or valid job object
*
*  NOTES
*     MT-NOTE: test_create_job() is MT safe 
*
*******************************************************************************/
static lListElem *test_create_job(data_entry_t *test, int count) 
{
   lListElem *job = nullptr;

   job = lCreateElem(JB_Type);

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
      if (test->checkpointing != nullptr) {
         lSetString(job, JB_checkpoint_name, test->checkpointing);
      }
      if (test->hard_resource_list != nullptr) {
         lList *requests = test_create_request(test->hard_resource_list, count);
         if (requests != nullptr) {
            lSetList(job, JB_hard_resource_list, requests);
         }
         else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->soft_resource_list != nullptr) {
         lList *requests = test_create_request(test->soft_resource_list, count);
         if (requests != nullptr) {
            lSetList(job, JB_soft_resource_list, requests);
         }
         else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->hard_queue_list != nullptr) {
         lList *queues = test_create_queue(test->hard_queue_list, count);
         if (queues != nullptr) {
            lSetList(job, JB_hard_queue_list, queues);
         }
         else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->hard_master_queue_list != nullptr) {
         lList *queues = test_create_queue(test->hard_master_queue_list, count);
         if (queues != nullptr) {
            lSetList(job, JB_master_hard_queue_list, queues);
         }
         else {
            lFreeElem(&job);
            goto end;
         }
      }
      if (test->pe != nullptr) {
         test_create_pe(test->pe, job);
      }
   }
end:
   return job;
}

/****** test_category/test_performance() ***************************************
*  NAME
*     test_performance() -- messures and outputs the time neede for n category strings
*
*  SYNOPSIS
*     double test_performance(lListElem *job_elem, int max, lList* access_list) 
*
*  INPUTS
*     lListElem *job_elem - job object
*     int max             - number of generated category strings
*     lList* access_list  - access list or nullptr
*
*  RESULT
*     double - time needed for the run
*
*  NOTES
*     MT-NOTE: test_performance() is MT safe 
*
*******************************************************************************/
static double test_performance(lListElem *job_elem, int max, lList* access_list, const lList *project_list, const lList *rqs_list) 
{
   int i;
   dstring category_str = DSTRING_INIT;
   struct timeval before;
   struct timeval after;
   double time_new;
   
   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      sge_build_job_category_dstring(&category_str, job_elem, access_list, project_list, nullptr, rqs_list);
      sge_dstring_clear(&category_str);
   }
   gettimeofday(&after, nullptr);
   sge_dstring_free(&category_str);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new/1000000);

   printf("tested %d category creations: new: %.2fs\n", max, time_new);

   return time_new;
}

/****** test_category/test() ***************************************************
*  NAME
*     test() -- executes one test including a performance test run
*
*  SYNOPSIS
*     int test(data_entry_t *test, char *result, int count) 
*
*  INPUTS
*     data_entry_t *test - one test setup
*     char *result       - expected category
*     int count          - test number
*
*  RESULT
*     int - 0 okay, 1 failed
*
*  NOTES
*     MT-NOTE: test() is MT safe 
*
*******************************************************************************/
static int test(data_entry_t *test, const char *result, int count)
{
   int ret = 0;
   lListElem *job_elem = nullptr;
   lList *access_list = nullptr;
   lList *project_list = nullptr;
   lList *rqs_list = nullptr;

   printf("\ntest %d:\n-------\n", test->test_nr);
   
   job_elem = test_create_job(test, 1);

   if (test->is_access_list == 1) {
      access_list = test_create_access();
   }
   if (test->project) {
      project_list = test_create_project(test->project);
   }
   if (test->rqs) {
      rqs_list = test_create_rqs();
   }

   if (job_elem != nullptr) {
       dstring category_str = DSTRING_INIT;

       sge_build_job_category_dstring(&category_str, job_elem, access_list, project_list, nullptr, rqs_list);

       printf("got     : <%s>\n", sge_dstring_get_string(&category_str)!=nullptr?sge_dstring_get_string(&category_str):"<nullptr>");

       if (result != nullptr && sge_dstring_get_string(&category_str) != nullptr) {
         if (strcmp(result, sge_dstring_get_string(&category_str)) == 0) {
         } else {
            ret = 1;
            printf("expected: <%s>\n", result!=nullptr? result:"<nullptr>");
         }
       } else if (result == nullptr &&  sge_dstring_get_string(&category_str) == nullptr) {
       } else {
         ret = 1;
         printf("expected: <%s>\n", result!=nullptr? result:"<nullptr>");
       }
       
       if (ret == 0) {
         int i;
         int max = 10000;
         printf(" => category outputs match\n");
         lFreeElem(&job_elem);
         for (i = 1; i <= 500; i*=6) {
            printf("test with %dx :", i);
            job_elem = test_create_job(test, i);
            if (job_elem != nullptr) {
               double time = test_performance(job_elem, max, access_list, nullptr, rqs_list);
               if (time > 1) {
                  max /= 10;
               }
               lFreeElem(&job_elem);
            } else {
               printf("failed to create job\n");
               ret = 1;
               break;
            }
         }
       }
       else {
         printf(" => test failed\n");
       }
       
       sge_dstring_free(&category_str);
   }
   else {
      printf("failed to create job for test %d\n", count);
      ret = 1;
   }
   lFreeElem(&job_elem);
   lFreeList(&access_list);
   lFreeList(&project_list);
   lFreeList(&rqs_list);
   return ret;
}

/****** test_sge_calendar/main() ***********************************************
*  NAME
*     main() -- calendar test
*
*  SYNOPSIS
*     int main(int argc, char* argv[]) 
*
*  FUNCTION
*     calendar test
*
*  INPUTS
*     int argc     - nr. of args 
*     char* argv[] - args
*
*  RESULT
*     int -  nr of failed tests
*
*******************************************************************************/
int main(int argc, char* argv[])
{
   int test_counter = 0;
   int failed = 0;

   lInit(nmv);
   
   printf("==> category test <==\n");
   printf("---------------------\n");


   while (tests[test_counter].test_nr != -1) {
      if (test(&(tests[test_counter]), 
               result_category[test_counter], 
               test_counter) != 0) {
         failed++; 
      }   
      test_counter++;
   }

   if (failed == 0) {
      printf("\n==> All tests are okay <==\n");
   }
   else {
      printf("\n==> %d/%d test(s) failed <==\n", failed, test_counter);
   }
   
   return failed;
}
