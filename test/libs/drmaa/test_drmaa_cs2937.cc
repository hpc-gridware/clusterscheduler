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

/*
 * CS-2937 probe. Times drmaa_init() and drmaa_exit() of a session, which is
 * all the reported reproducer does.
 *
 * For a user who is not a manager, drmaa_exit() takes about 30 seconds while
 * the qmaster logs
 *
 *    denied: "<user>" must be manager to initiate TRIGGER operation
 *
 * For a manager it returns at once. The reason is the way the session shuts
 * its event client down: japi_exit() calls japi_stop_event_client(), which
 * sends a GDI TRIGGER request for the SGE_EV_LIST target
 * (japi.cc, ocs::gdi::Client::gdi_kill() with EVENTCLIENT_KILL). Since the
 * permission check for TRIGGER requests became "manager, unless the target is
 * the cluster queue list" the qmaster rejects that request for an ordinary
 * user, the event client is never told to shut down, and the session waits
 * for its event client thread until the thread notices on its own.
 *
 * What the thread waits for is the event delivery interval, which is why
 * setting SGE_JAPI_EDTIME to 1 shortens the exit to about a second and is the
 * workaround for the reported case. The probe therefore prints the value it
 * runs with, and is meant to be run twice, once with it and once without.
 *
 * Usage:
 *   test_drmaa_cs2937 [njobs [sleep_seconds]]
 *
 * <njobs>          Number of /bin/sleep jobs submitted before the session is
 *                  closed again. Default 0 - the reported reproducer submits
 *                  nothing at all, and the delay does not depend on it. Use a
 *                  job count to mirror a submitting wrapper; the jobs are not
 *                  waited for.
 * <sleep_seconds>  Runtime of those jobs. Default 5.
 *
 * The last line of the output is meant for a test driver:
 *
 *   RESULT init=0.012 run=0.000 exit=30.174
 *
 * The probe needs a live cluster, so it is built but not registered with
 * CTest.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "drmaa.h"

static double
now_s() {
   struct timespec ts{};
   clock_gettime(CLOCK_REALTIME, &ts);
   return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

/*
 * Submit njobs sleeper jobs and return the time it took. The job ids are
 * printed but not kept: the probe does not reap them, exactly as the reported
 * wrapper does not.
 */
static double
submit_jobs(int njobs, const char *sleep_secs, int *failed) {
   char err[DRMAA_ERROR_STRING_BUFFER];
   char jobid[DRMAA_JOBNAME_BUFFER];
   drmaa_job_template_t *jt = nullptr;
   const char *args[2];
   double t0 = now_s();

   *failed = 0;
   if (njobs <= 0) {
      return 0.0;
   }

   if (drmaa_allocate_job_template(&jt, err, sizeof(err)) != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_allocate_job_template: %s\n", err);
      *failed = 1;
      return now_s() - t0;
   }
   drmaa_set_attribute(jt, DRMAA_REMOTE_COMMAND, "/bin/sleep", err, sizeof(err));
   args[0] = sleep_secs;
   args[1] = nullptr;
   drmaa_set_vector_attribute(jt, DRMAA_V_ARGV, args, err, sizeof(err));
   drmaa_set_attribute(jt, DRMAA_OUTPUT_PATH, ":/dev/null", err, sizeof(err));
   drmaa_set_attribute(jt, DRMAA_JOIN_FILES, "y", err, sizeof(err));

   for (int i = 0; i < njobs; i++) {
      int rc = drmaa_run_job(jobid, sizeof(jobid), jt, err, sizeof(err));
      if (rc != DRMAA_ERRNO_SUCCESS) {
         fprintf(stderr, "drmaa_run_job: %s\n", err);
         *failed = 1;
         break;
      }
      printf("submitted               : %s\n", jobid);
   }

   drmaa_delete_job_template(jt, err, sizeof(err));

   return now_s() - t0;
}

int
main(int argc, char *argv[]) {
   char err[DRMAA_ERROR_STRING_BUFFER];
   const int njobs = (argc > 1) ? atoi(argv[1]) : 0;
   const char *sleep_secs = (argc > 2) ? argv[2] : "5";
   const char *edtime = getenv("SGE_JAPI_EDTIME");
   double t_init, t_run, t_exit, t0;
   int submit_failed = 0;

   printf("SGE_JAPI_EDTIME         : %s\n", edtime != nullptr ? edtime : "(unset)");
   printf("jobs                    : %d x /bin/sleep %s\n", njobs > 0 ? njobs : 0, sleep_secs);
   fflush(stdout);

   t0 = now_s();
   if (drmaa_init(nullptr, err, sizeof(err)) != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_init: %s\n", err);
      return 1;
   }
   t_init = now_s() - t0;
   printf("init                    : %8.3f s\n", t_init);
   fflush(stdout);

   t_run = submit_jobs(njobs, sleep_secs, &submit_failed);
   printf("run                     : %8.3f s\n", t_run);
   fflush(stdout);

   t0 = now_s();
   if (drmaa_exit(err, sizeof(err)) != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_exit: %s\n", err);
      return 1;
   }
   t_exit = now_s() - t0;
   printf("exit                    : %8.3f s\n", t_exit);

   // the line a test driver reads
   printf("\nRESULT init=%.3f run=%.3f exit=%.3f\n", t_init, t_run, t_exit);

   return submit_failed ? 1 : 0;
}
