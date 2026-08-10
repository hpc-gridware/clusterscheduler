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
 * @brief CS-2447 probe: measures how long drmaa_wait() takes to return after a job has actually ended
 */

/*
 * CS-2447 probe. Measures how long drmaa_wait() takes to return *after a job
 * has actually ended*, i.e. with the job runtime taken out.
 *
 * The ticket reports drmaa_wait() round trips of 17-50 s that scale with
 * SGE_JAPI_EDTIME. That should not happen: JAPI subscribes sgeE_JOB_FINISH
 * with a flush delay of 0 (japi_implementation_thread() in
 * source/libs/japi/japi.cc), so sge_qmaster flushes the event as soon as a
 * worker thread generates it, independently of the event delivery interval
 * that SGE_JAPI_EDTIME controls.
 *
 * All jobs are submitted before any of them is reaped, so their runtimes
 * overlap and a slow job cannot hide the latency of one that finished
 * earlier. Reaping uses DRMAA_JOB_IDS_SESSION_ANY, i.e. completion order.
 *
 * TWO LATENCIES ARE REPORTED, because neither is trustworthy alone:
 *
 *   wait_lat  = drmaa_wait() returned - end_time
 *       Exact about when the job ended, but end_time is stamped by
 *       sge_shepherd on the EXECUTION host (source/daemons/shepherd/
 *       sge_fileio.cc, and sge_execd in reaper_execd.cc) while the wait
 *       return is stamped on the submit host. Any clock offset between the
 *       two goes straight into this number, and an execution host whose
 *       clock runs ahead produces a NEGATIVE latency. With jobs spread over
 *       several execution hosts each contributes its own offset, so this
 *       column scatters by whatever the cluster's clock skew happens to be.
 *       The runtime column stays consistent regardless, because start_time
 *       and end_time come from the same host clock and the skew cancels.
 *
 *   local_lat = drmaa_wait() returned - (submitted + sleep_seconds)
 *       Uses the submit host clock only, so skew cannot corrupt it. It is an
 *       upper bound: it also contains dispatch and scheduling delay, because
 *       a job does not start the instant it is submitted.
 *
 * To remove the skew entirely, pin all jobs to one execution host with the
 * native specification argument - wait_lat then carries a single constant
 * offset instead of one per host.
 *
 * Usage:
 *   test_drmaa_cs2447 [sleep_seconds] [njobs] [native_spec]
 *
 * <sleep_seconds>  Duration of the /bin/sleep job. Default 10.
 * <njobs>          Number of jobs submitted in parallel. Default 5. Keep it
 *                  at or below the number of free slots, otherwise the
 *                  surplus queues and inflates local_lat.
 *
 * sleep_seconds must exceed the time the cluster needs to dispatch the whole
 * batch, otherwise the jobs never overlap and local_lat measures dispatch
 * rather than event latency. The probe reports the observed start spread and
 * warns when that condition is violated.
 * <native_spec>    Passed verbatim as DRMAA_NATIVE_SPECIFICATION, e.g.
 *                  "-l h=somehost" to pin all jobs to one execution host.
 *
 * The sweep that settles the ticket:
 *
 *                       test_drmaa_cs2447 10 5 "-l h=somehost"
 *   SGE_JAPI_EDTIME=6   test_drmaa_cs2447 10 5 "-l h=somehost"
 *   SGE_JAPI_EDTIME=30  test_drmaa_cs2447 10 5 "-l h=somehost"
 *
 * Clock skew is constant per host, so it cancels out of the *change* between
 * runs. If the medians stay flat while SGE_JAPI_EDTIME rises, the flushed
 * subscription works and the reported "wall ~= EDTIME + 11 s" relationship
 * does not reproduce. Note that SGE_JAPI_EDTIME values at or below the event
 * client's own flush delay are rejected in drmaa_init() with "event flush
 * delay may not be greater than event delivery time"; that is expected and
 * is not specific to a release.
 *
 * The probe needs a live cluster, so it is built but not registered with
 * CTest.
 */

#include <algorithm>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "drmaa.h"

/** @brief What is measured for one job
 *
 * The ticket is about the delay between a job *ending* and `drmaa_wait()`
 * saying so, so both clocks are needed: `t_*` are taken in this process,
 * `start_time` and `end_time` come from the job's rusage and are the execution
 * host's. The interesting number is `t_wait_ret - end_time`, which takes the
 * job's own runtime out of the measurement.
 */
typedef struct {
   char   jobid[DRMAA_JOBNAME_BUFFER];   ///< the job this record is about
   double t_submitted;    ///< when `drmaa_run_job()` returned, local clock
   double t_wait_ret;     ///< when `drmaa_wait()` returned for it, local clock
   double start_time;     ///< when it started, from rusage, execution host clock
   double end_time;       ///< when it ended, from rusage, execution host clock
   bool   reaped;         ///< `drmaa_wait()` has already returned this job
} job_rec_t;

static double
now_s() {
   struct timespec ts{};
   clock_gettime(CLOCK_REALTIME, &ts);
   return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

/*
 * Usage values are doubles whose unit has varied between formats
 * (sge_get_gmt64() delivers microseconds). Normalise a suspected epoch
 * timestamp to seconds so the derived latency stays meaningful.
 */
static double
to_seconds(double v) {
   if (v > 1e14) return v / 1e6;   /* microseconds */
   if (v > 1e11) return v / 1e3;   /* milliseconds */
   return v;                        /* seconds */
}

static void
print_stats(const char *label, const char *explanation, std::vector<double> &v) {
   if (v.empty()) {
      return;
   }
   std::sort(v.begin(), v.end());
   double sum = 0.0;
   for (double d : v) {
      sum += d;
   }
   printf("\n%s\n", label);
   if (explanation != nullptr) {
      printf("%s\n", explanation);
   }
   printf("  n %zu   min %.3f   median %.3f   mean %.3f   max %.3f\n",
          v.size(), v.front(), v[v.size() / 2], sum / static_cast<double>(v.size()), v.back());
}

int
main(int argc, char *argv[]) {
   char err[DRMAA_ERROR_STRING_BUFFER];
   const char *sleep_secs = (argc > 1) ? argv[1] : "10";
   int njobs = (argc > 2) ? atoi(argv[2]) : 5;
   const char *native_spec = (argc > 3) ? argv[3] : nullptr;
   const char *edtime = getenv("SGE_JAPI_EDTIME");
   const double sleep_d = atof(sleep_secs);
   drmaa_job_template_t *jt = nullptr;
   const char *args[2];
   double t0, t1, t_all0, t_all1;
   int rc;

   if (njobs < 1) {
      njobs = 1;
   }
   std::vector<job_rec_t> jobs(static_cast<size_t>(njobs));
   std::vector<double> wait_lat;
   std::vector<double> local_lat;

   printf("SGE_JAPI_EDTIME         : %s\n", edtime != nullptr ? edtime : "(unset)");
   printf("job                     : /bin/sleep %s   x%d job(s), submitted in parallel\n",
          sleep_secs, njobs);
   printf("native specification    : %s\n\n",
          native_spec != nullptr ? native_spec : "(none - jobs may spread over hosts)");
   fflush(stdout);

   t0 = now_s();
   rc = drmaa_init(nullptr, err, sizeof(err));
   t1 = now_s();
   if (rc != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_init failed (rc=%d): %s\n", rc, err);
      return 1;
   }
   printf("drmaa_init              : %8.3f s\n", t1 - t0);

   /* ---- submit everything first, so the jobs run concurrently ---------- */

   if ((rc = drmaa_allocate_job_template(&jt, err, sizeof(err))) != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_allocate_job_template: %s\n", err);
      drmaa_exit(err, sizeof(err));
      return 1;
   }
   drmaa_set_attribute(jt, DRMAA_REMOTE_COMMAND, "/bin/sleep", err, sizeof(err));
   args[0] = sleep_secs;
   args[1] = nullptr;
   drmaa_set_vector_attribute(jt, DRMAA_V_ARGV, args, err, sizeof(err));
   /* leave no stdout/stderr files behind */
   drmaa_set_attribute(jt, DRMAA_OUTPUT_PATH, ":/dev/null", err, sizeof(err));
   drmaa_set_attribute(jt, DRMAA_JOIN_FILES, "y", err, sizeof(err));
   if (native_spec != nullptr) {
      if (drmaa_set_attribute(jt, DRMAA_NATIVE_SPECIFICATION, native_spec,
                              err, sizeof(err)) != DRMAA_ERRNO_SUCCESS) {
         fprintf(stderr, "drmaa_set_attribute(native_specification): %s\n", err);
      }
   }

   t_all0 = now_s();
   size_t n = 0;
   for (int i = 0; i < njobs; i++) {
      rc = drmaa_run_job(jobs[n].jobid, sizeof(jobs[n].jobid), jt, err, sizeof(err));
      if (rc != DRMAA_ERRNO_SUCCESS) {
         fprintf(stderr, "drmaa_run_job (#%d): %s\n", i + 1, err);
         continue;
      }
      jobs[n].t_submitted = now_s();
      n++;
   }
   drmaa_delete_job_template(jt, err, sizeof(err));

   if (n == 0) {
      fprintf(stderr, "no job could be submitted\n");
      drmaa_exit(err, sizeof(err));
      return 1;
   }
   jobs.resize(n);
   printf("submitted               : %8.3f s   for %zu job(s)\n\n", now_s() - t_all0, n);
   fflush(stdout);

   /* ---- reap in completion order --------------------------------------- */

   for (size_t i = 0; i < n; i++) {
      char jobid_out[DRMAA_JOBNAME_BUFFER];
      char attr[DRMAA_ATTR_BUFFER];
      drmaa_attr_values_t *rusage = nullptr;
      double start_time = 0.0, end_time = 0.0;
      int stat = 0;

      rc = drmaa_wait(DRMAA_JOB_IDS_SESSION_ANY, jobid_out, sizeof(jobid_out),
                      &stat, DRMAA_TIMEOUT_WAIT_FOREVER, &rusage, err, sizeof(err));
      const double t_ret = now_s();

      if (rc != DRMAA_ERRNO_SUCCESS) {
         fprintf(stderr, "drmaa_wait: %s\n", err);
         break;
      }

      if (rusage != nullptr) {
         while (drmaa_get_next_attr_value(rusage, attr, sizeof(attr)) == DRMAA_ERRNO_SUCCESS) {
            if (strncmp(attr, "start_time=", 11) == 0) {
               start_time = to_seconds(atof(attr + 11));
            } else if (strncmp(attr, "end_time=", 9) == 0) {
               end_time = to_seconds(atof(attr + 9));
            }
         }
         drmaa_release_attr_values(rusage);
      }

      auto it = std::find_if(jobs.begin(), jobs.end(), [&](const job_rec_t &r) {
         return !r.reaped && strcmp(r.jobid, jobid_out) == 0;
      });
      if (it == jobs.end()) {
         fprintf(stderr, "warning: reaped unknown job id %s\n", jobid_out);
         continue;
      }

      it->reaped     = true;
      it->t_wait_ret = t_ret;
      it->start_time = start_time;
      it->end_time   = end_time;
   }
   t_all1 = now_s();

   /* ---- report ---------------------------------------------------------- */

   std::vector<const job_rec_t *> order;
   for (const auto &r : jobs) {
      if (r.reaped) {
         order.push_back(&r);
      }
   }
   std::sort(order.begin(), order.end(),
             [](const job_rec_t *a, const job_rec_t *b) { return a->t_wait_ret < b->t_wait_ret; });

   printf("%-10s %11s %8s %14s %14s\n",
          "job id", "runtime[s]", "reaped#", "wait_lat[s]", "local_lat[s]");
   printf("%-10s %11s %8s %14s %14s\n",
          "------", "----------", "-------", "-----------", "------------");

   for (size_t i = 0; i < order.size(); i++) {
      const job_rec_t *r = order[i];
      const double runtime = (r->end_time > 0.0 && r->start_time > 0.0)
                             ? r->end_time - r->start_time : -1.0;
      const double ll = r->t_wait_ret - (r->t_submitted + sleep_d);

      local_lat.push_back(ll);

      if (r->end_time > 0.0) {
         const double l = r->t_wait_ret - r->end_time;
         wait_lat.push_back(l);
         printf("%-10s %11.3f %8zu %14.3f %14.3f\n", r->jobid, runtime, i + 1, l, ll);
      } else {
         printf("%-10s %11.3f %8zu %14s %14.3f\n", r->jobid, runtime, i + 1, "n/a", ll);
      }
   }

   if (wait_lat.empty()) {
      printf("\nno end_time in any rusage - cannot compute wait_lat\n");
   } else {
      const double lo = *std::min_element(wait_lat.begin(), wait_lat.end());
      const double hi = *std::max_element(wait_lat.begin(), wait_lat.end());

      print_stats("wait_lat  = drmaa_wait() return - end_time",
                  "            (execution host clock; affected by clock skew)", wait_lat);
      if (lo < 0.0) {
         printf("  NOTE: negative values mean an execution host clock is AHEAD of this\n");
         printf("        host - end_time is stamped by sge_shepherd on the execution host.\n");
      }
      if (hi - lo > 0.25 && native_spec == nullptr) {
         printf("  NOTE: spread of %.3f s across jobs suggests they ran on hosts whose\n", hi - lo);
         printf("        clocks differ. Pin them to one host to remove this, e.g.\n");
         printf("        test_drmaa_cs2447 %s %d \"-l h=<somehost>\"\n", sleep_secs, njobs);
      }
   }

   print_stats("local_lat = drmaa_wait() return - (submitted + sleep)",
               "            (submit host clock only, immune to skew; also contains\n"
               "             dispatch and queueing delay)", local_lat);

   /* ---- start spread: is the measurement even valid? --------------------
    *
    * The whole point of submitting everything before reaping anything is
    * that the jobs run concurrently and end together, so what is measured
    * is event latency rather than the cluster working through a queue. If
    * dispatching the batch takes longer than a job runs, the first jobs
    * finish before the last ones start, the batch never overlaps, and
    * local_lat degenerates into a measurement of dispatch time - it
    * contains the full start delay of every job by construction.
    *
    * That is easy to hit on a small or loaded machine (~80 ms/job of
    * dispatch has been observed on a laptop VM), and it silently produces
    * a latency that grows with the job count, which is exactly the false
    * positive this warning exists to prevent. */
   {
      double min_start = 0.0, max_start = 0.0, min_end = 0.0;
      bool have = false;

      for (const auto &r : jobs) {
         if (!r.reaped || r.start_time <= 0.0) {
            continue;
         }
         if (!have) {
            min_start = max_start = r.start_time;
            min_end = r.end_time;
            have = true;
         } else {
            if (r.start_time < min_start) min_start = r.start_time;
            if (r.start_time > max_start) max_start = r.start_time;
            if (r.end_time > 0.0 && (min_end <= 0.0 || r.end_time < min_end)) min_end = r.end_time;
         }
      }

      if (have) {
         const double spread = max_start - min_start;
         printf("\nstart spread (last job start - first job start): %.3f s\n", spread);

         if (spread >= sleep_d) {
            printf("\n*** WARNING: start spread %.3f s >= job runtime %.3f s.\n", spread, sleep_d);
            printf("***          The jobs did NOT run concurrently");
            if (min_end > 0.0 && min_end < max_start) {
               printf(" - the first job ended %.3f s\n"
                      "***          BEFORE the last one started",
                      max_start - min_end);
            }
            printf(".\n");
            printf("***          local_lat therefore measures job dispatch, not event\n");
            printf("***          latency, and will grow with the job count for that\n");
            printf("***          reason alone. Re-run with a longer sleep (>= %.0f s for\n",
                   spread * 2.0);
            printf("***          this job count on this cluster), or read wait_lat,\n");
            printf("***          which is measured against each job's own end_time.\n");
         }
      }
   }

   printf("\ntotal wallclock (submit .. last reap): %.3f s\n", t_all1 - t_all0);

   if (drmaa_exit(err, sizeof(err)) != DRMAA_ERRNO_SUCCESS) {
      fprintf(stderr, "drmaa_exit: %s\n", err);
      return 1;
   }

   return 0;
}
