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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include "drmaa.h"

int main (int argc, char **argv) {
   char error[DRMAA_ERROR_STRING_BUFFER];
   int errnum = 0;
   drmaa_job_template_t *jt = nullptr;

   errnum = drmaa_init (nullptr, error, DRMAA_ERROR_STRING_BUFFER);

   if (errnum != DRMAA_ERRNO_SUCCESS) {
      fprintf (stderr, "Could not initialize the DRMAA library: %s\n", error);
      return 1;
   }

   errnum = drmaa_allocate_job_template (&jt, error, DRMAA_ERROR_STRING_BUFFER);

   if (errnum != DRMAA_ERRNO_SUCCESS) {
      fprintf (stderr, "Could not create job template: %s\n", error);
   }
   else {
      errnum = drmaa_set_attribute (jt, DRMAA_REMOTE_COMMAND, "sleeper.sh",
                                   error, DRMAA_ERROR_STRING_BUFFER);

      if (errnum != DRMAA_ERRNO_SUCCESS) {
         fprintf (stderr, "Could not set attribute \"%s\": %s\n",
                  DRMAA_REMOTE_COMMAND, error);
      }
      else {
         const char *args[2] = {"5", nullptr};
         
         errnum = drmaa_set_vector_attribute (jt, DRMAA_V_ARGV, args, error,
                                              DRMAA_ERROR_STRING_BUFFER);
      }
      
      if (errnum != DRMAA_ERRNO_SUCCESS) {
         fprintf (stderr, "Could not set attribute \"%s\": %s\n",
                  DRMAA_REMOTE_COMMAND, error);
      }
      else {
         drmaa_job_ids_t *ids = nullptr;
         int start = 1;
         int end = 30;
         int step = 2;

         errnum = drmaa_run_bulk_jobs (&ids, jt, start, end, step, error,
                                       DRMAA_ERROR_STRING_BUFFER);

         if (errnum != DRMAA_ERRNO_SUCCESS) {
            fprintf (stderr, "Could not submit job: %s\n", error);
         }
         else {
            char jobid[DRMAA_JOBNAME_BUFFER];
            const char *jobids[2] = {DRMAA_JOB_IDS_SESSION_ALL, nullptr};

            while (drmaa_get_next_job_id (ids, jobid, DRMAA_JOBNAME_BUFFER)
                                                     == DRMAA_ERRNO_SUCCESS) {
               printf ("A job task has been submitted with id %s\n", jobid);
            }
            
            errnum = drmaa_synchronize (jobids, DRMAA_TIMEOUT_WAIT_FOREVER,
                                        0, error, DRMAA_ERROR_STRING_BUFFER);
            
            if (errnum != DRMAA_ERRNO_SUCCESS) {
               fprintf (stderr, "Could not wait for jobs: %s\n", error);
            }
            else {
               char jobid[DRMAA_JOBNAME_BUFFER];
               int status = 0;
               drmaa_attr_values_t *rusage = nullptr;
               int count = 0;
               
               for (count = start; count < end; count += step) {
                  errnum = drmaa_wait (DRMAA_JOB_IDS_SESSION_ANY, jobid,
                                       DRMAA_JOBNAME_BUFFER, &status,
                                       DRMAA_TIMEOUT_WAIT_FOREVER, &rusage,
                                       error, DRMAA_ERROR_STRING_BUFFER);

                  if (errnum != DRMAA_ERRNO_SUCCESS) {
                     fprintf (stderr, "Could not wait for job: %s\n", error);
                  }
                  else {
                     char usage[DRMAA_ERROR_STRING_BUFFER];
                     int aborted = 0;

                     drmaa_wifaborted(&aborted, status, nullptr, 0);

                     if (aborted == 1) {
                        printf("Job %s never ran\n", jobid);
                     }
                     else {
                        int exited = 0;

                        drmaa_wifexited(&exited, status, nullptr, 0);

                        if (exited == 1) {
                           int exit_status = 0;

                           drmaa_wexitstatus(&exit_status, status, nullptr, 0);
                           printf("Job %s finished regularly with exit status %d\n",
                                  jobid, exit_status);
                        }
                        else {
                           int signaled = 0;

                           drmaa_wifsignaled(&signaled, status, nullptr, 0);

                           if (signaled == 1) {
                              char termsig[DRMAA_SIGNAL_BUFFER+1];

                              drmaa_wtermsig(termsig, DRMAA_SIGNAL_BUFFER, status, nullptr, 0);
                              printf("Job %s finished due to signal %s\n", jobid, termsig);
                           }
                           else {
                              printf("Job %s finished with unclear conditions\n", jobid);
                           }
                        } /* else */
                     } /* else */

                     printf ("Job Usage:\n");

                     while (drmaa_get_next_attr_value (rusage, usage, DRMAA_ERROR_STRING_BUFFER)
                                                                          == DRMAA_ERRNO_SUCCESS) {
                        printf ("  %s\n", usage);
                     }

                     drmaa_release_attr_values (rusage);
                  } /* else */
               } /* for */
            } /* else */
         } /* else */

         drmaa_release_job_ids (ids);
      } /* else */

      errnum = drmaa_delete_job_template (jt, error, DRMAA_ERROR_STRING_BUFFER);

      if (errnum != DRMAA_ERRNO_SUCCESS) {
         fprintf (stderr, "Could not delete job template: %s\n", error);
      }
   } /* else */

   errnum = drmaa_exit (error, DRMAA_ERROR_STRING_BUFFER);

   if (errnum != DRMAA_ERRNO_SUCCESS) {
      fprintf (stderr, "Could not shut down the DRMAA library: %s\n", error);
      return 1;
   }

   return 0;
}
