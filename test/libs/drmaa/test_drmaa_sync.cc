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

#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include "japi/drmaa.h"

static void *submit_thread(void *arg);
static void *sync_thread(void *arg);

static int count = -1;
static pthread_mutex_t japi_session_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_COUNT()   pthread_mutex_lock(&japi_session_mutex)                                 
#define UNLOCK_COUNT() pthread_mutex_unlock(&japi_session_mutex)

int main(int argc, char *argv[])
{
   pthread_t tid1, tid2;
   
   drmaa_init (nullptr, nullptr, 0);
   
   pthread_create (&tid1, nullptr, submit_thread, argv[1]);
   pthread_create (&tid2, nullptr, sync_thread, nullptr);
   
   sleep (60);
   drmaa_exit (nullptr, 0);
   
   pthread_join (tid1, nullptr);
   pthread_join (tid2, nullptr);
   
   return count;
}

static void *submit_thread(void *arg)
{
   drmaa_job_template_t *jt = nullptr;
   char jobid[DRMAA_JOBNAME_BUFFER];
   const char *job_argv[2] = {"5", nullptr};
   int check_count = 1;
   
   drmaa_allocate_job_template (&jt, nullptr, 0);
   drmaa_set_attribute (jt, DRMAA_REMOTE_COMMAND, (char *)arg, nullptr, 0);
   drmaa_set_vector_attribute(jt, DRMAA_V_ARGV, job_argv, nullptr, 0);
   drmaa_set_attribute (jt, DRMAA_OUTPUT_PATH, ":" DRMAA_PLACEHOLDER_HD "/DRMAA_JOB", nullptr, 0);
   drmaa_set_attribute (jt, DRMAA_JOIN_FILES, "y", nullptr, 0);
   
   while (drmaa_run_job (jobid, DRMAA_JOBNAME_BUFFER, jt, nullptr, 0) == DRMAA_ERRNO_SUCCESS) {
      if (check_count) {
         LOCK_COUNT();
      
         if (count < 0) {
            count = 0;
         }
         
         UNLOCK_COUNT();
         
         check_count = 0;
      }
   }
   
   drmaa_delete_job_template (jt, nullptr, 0);
   
   return (void *)nullptr;
}

static void *sync_thread(void *arg)
{
   const char *jobids[2] = {DRMAA_JOB_IDS_SESSION_ALL, nullptr};
   int dispose = 0;
   
   while (drmaa_synchronize (jobids, DRMAA_TIMEOUT_WAIT_FOREVER, dispose, nullptr, 0) == DRMAA_ERRNO_SUCCESS) {
      LOCK_COUNT();
      
      if (count >= 0) {
         count++;
      }
      
      UNLOCK_COUNT();
      
      dispose = !dispose;
   }
   
   return (void *)nullptr;
}
