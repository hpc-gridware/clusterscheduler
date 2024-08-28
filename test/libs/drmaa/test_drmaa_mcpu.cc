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
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <ctime>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include "drmaa.h"

#include <uti/sge_log.h>
#include <uti/sge_stdlib.h>

#define CELL "default"
#define WD "/tmp"
#define CMD "/tmp/sleeper.sh"
#define CATEGORY "test"

int handle_code(int code, char *msg, int r, int t);
void *run(void *arg);

int main(int argc, char **argv) {
   int ret = DRMAA_ERRNO_SUCCESS;
   char error[DRMAA_ERROR_STRING_BUFFER + 1];
    
   int runs = 20;
   int threads = 50;
   int run_count = 0;
   int thread_count = 0;
   pthread_t *ids = nullptr;
    
   ids = (pthread_t *)sge_malloc(sizeof (pthread_t) * threads);
   SGE_ASSERT(ids != nullptr);
    
   ret = drmaa_init(CELL, error, DRMAA_ERROR_STRING_BUFFER);
   if (handle_code(ret, error, -1, 0) == 1) {
      exit(1);
   }
    
   for (run_count = 0; run_count < runs; run_count++) {
      printf("-1 0 STARTING RUN %d %ld\n", run_count, time(nullptr));
        
      for (thread_count = 0; thread_count < threads; thread_count++) {
         int *arg = (int *)sge_malloc(sizeof (int) * 2);

         SGE_ASSERT(arg != nullptr);
            
         arg[0] = run_count;
         arg[1] = thread_count;

         if (pthread_create(&ids[thread_count], nullptr, run, arg) != 0) {
            printf("%d %d EXCEPTION: Couldn't create thread %ld\n", run_count,
            thread_count, time(nullptr));
        }
      }
        
      for (thread_count = 0; thread_count < threads; thread_count++) {
         pthread_join(ids[thread_count], nullptr);
      }
        
      printf("-1 0 ENDING RUN %d %ld\n", run_count, time(nullptr));
   }
    
   ret = drmaa_exit(error, DRMAA_ERROR_STRING_BUFFER);
   handle_code(ret, error, -1, 0);

   return 0;
}

int handle_code(int code, char *msg, int r, int t) {
   if (code != DRMAA_ERRNO_SUCCESS) {
      printf("%d %d EXCEPTION: %s %ld\n", r, t, msg, time(nullptr));
      return 1;
   }
   return 0;
}

void *run(void *arg) {
   int ret = DRMAA_ERRNO_SUCCESS;
   char error[DRMAA_ERROR_STRING_BUFFER + 1];
    
   drmaa_job_template_t *jt = nullptr;
   int run = ((int *)arg)[0];
   int thread = ((int *)arg)[1];
   char jobid[DRMAA_JOBNAME_BUFFER + 1];
   int queued = 1;
   int running = 0;
   int status = -1;
    
   free(arg);
    
   ret = drmaa_allocate_job_template(&jt, error, DRMAA_ERROR_STRING_BUFFER);
   if (handle_code(ret, error, run, thread) == 1) {
      return nullptr;
   }
    
   ret = drmaa_set_attribute(jt, DRMAA_REMOTE_COMMAND, CMD, error, DRMAA_ERROR_STRING_BUFFER);
   if (handle_code(ret, error, run, thread) == 1) {
      return nullptr;
   }
    
   ret = drmaa_set_attribute(jt, DRMAA_WD, WD, error, DRMAA_ERROR_STRING_BUFFER);
   if (handle_code(ret, error, run, thread) == 1) {
      return nullptr;
   }
    
   ret = drmaa_set_attribute(jt, DRMAA_JOB_CATEGORY, CATEGORY, error, DRMAA_ERROR_STRING_BUFFER);
   if (handle_code(ret, error, run, thread) == 1) {
      return nullptr;
   }
    
   printf("%d %d SETUP complete %ld\n", run, thread, time(nullptr));
    
   ret = drmaa_run_job(jobid, DRMAA_JOBNAME_BUFFER, jt, error, DRMAA_ERROR_STRING_BUFFER);
   if (handle_code(ret, error, run, thread) == 1) {
      return nullptr;
   }
    
   printf("%d %d SUBMITTED jobid: %s %ld\n", run, thread, jobid, time(nullptr));
    
   ret = drmaa_delete_job_template(jt, error, DRMAA_ERROR_STRING_BUFFER);
   handle_code(ret, error, run, thread);
    
   while (queued) {
      ret = drmaa_wait(jobid, nullptr, 0, nullptr, 2, nullptr, error, DRMAA_ERROR_STRING_BUFFER);
        
      if (ret != DRMAA_ERRNO_EXIT_TIMEOUT) {
         if (handle_code(ret, error, run, thread) == 1) {
            return nullptr;
         }
      } else {
         printf ("%d %d TIMEOUT jobid: %s %ld\n", run, thread, jobid, time (nullptr));
      }
        
      ret = drmaa_job_ps(jobid, &status, error, DRMAA_ERROR_STRING_BUFFER);
      if (handle_code(ret, error, run, thread) == 1) {
         return nullptr;
      }
        
      queued = (status == DRMAA_PS_QUEUED_ACTIVE) ||
      (status == DRMAA_PS_SYSTEM_ON_HOLD) ||
      (status == DRMAA_PS_SYSTEM_ON_HOLD) ||
      (status == DRMAA_PS_USER_ON_HOLD) ||
      (status == DRMAA_PS_USER_SYSTEM_ON_HOLD);
   }
    
   printf("%d %d RUNNING jobid: %s %ld\n", run, thread, jobid, time(nullptr));
    
   running = 1;
    
   while (running == 1) {
      ret = drmaa_wait(jobid, nullptr, 0, nullptr, 60, nullptr, error, DRMAA_ERROR_STRING_BUFFER);

      if (ret != DRMAA_ERRNO_EXIT_TIMEOUT) {
         if (handle_code(ret, error, run, thread) == 1) {
            return nullptr;
         }
            
         running = 0;
            
         printf("%d %d FINISHED jobid: %s %ld\n", run, thread, jobid,
         time(nullptr));
      } else {
         printf ("%d %d TIMEOUT jobid: %s %ld\n", run, thread, jobid, time (nullptr));

         ret = drmaa_job_ps(jobid, &status, error, DRMAA_ERROR_STRING_BUFFER);
            
         if (handle_code(ret, error, run, thread) == 1) {
            return nullptr;
         }
            
         if (status != DRMAA_PS_RUNNING) {
            running = 0;

            printf("%d %d HUNG jobid: %s %ld\n", run, thread, jobid, time(nullptr));
         }
      }
   }
    
   return nullptr;
}
