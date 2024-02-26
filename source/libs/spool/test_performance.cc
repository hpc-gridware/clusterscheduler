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
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_job.h"

#include "mir/sge_mirror.h"

#include "comm/commlib.h"

#include "spool/sge_spooling.h"
#include "spool/loader/sge_spooling_loader.h"

#include "msg_clients_common.h"

#ifndef TEST_READ_ONLY

static const int num_jobs = 10000;

static const char *random_string(int length)
{
   static char buf[1000];
   int i;

   srand(time(nullptr));

   for (i = 0; i < length; i++) {
      buf[i] = rand() % 26 + 64;
   }
   buf[i] = 0;

   return buf;
}

/*
 * generate num jobs
 * every 10th job has array tasks
 * the number of array tasks varies between 1 to 100
 */
static bool generate_jobs(int num)
{
   int i;
   int num_array = 0;
   int num_total = 0;

   for (i = 0; i < num; i++) {
      lListElem *job;

      job = lCreateElem(JB_Type); num_total++;
      lSetUlong(job, JB_job_number, i + 1);
      lSetString(job, JB_job_name, random_string(15));
      lSetString(job, JB_project, random_string(20));
      lSetString(job, JB_department, random_string(20));
      lSetString(job, JB_directive_prefix, random_string(100));
      lSetString(job, JB_exec_file, random_string(500));
      lSetString(job, JB_script_file, random_string(500));
      lSetString(job, JB_owner, random_string(10));
      lSetString(job, JB_group, random_string(10));
      lSetString(job, JB_account, random_string(20));
      lSetString(job, JB_cwd, random_string(100));
      lAppendElem(*object_type_get_master_list_rw(SGE_TYPE_JOB), job);

      if ((i % 10) == 0) {
         int j;
         num_array = (num_array + 1) % 100;
         for (j = 0; j < num_array; j++) {
            lAddSubUlong(job, JAT_task_number, j + 1, JB_ja_tasks, JAT_Type); num_total++;
         }
      }
   }

   printf("==> created %d objects in total\n", num_total);

   return true;
}

static bool update_jobs(void)
{
   lListElem *job;
   int num_total = 0;

   for_each_rw(job, *object_type_get_master_list_rw(SGE_TYPE_JOB)) {
      lSetString(job, JB_project, random_string(20));
      num_total++;
   }

   printf("==> updated %d objects in total\n", num_total);

   return true;
}

static bool spool_data(void)
{
   lList *answer_list = nullptr;
   const lListElem *context;
   lListElem *job;
   dstring key_ds = DSTRING_INIT;
   const char *key;
   int num_total = 0;

   context = spool_get_default_context();

   fprintf(stdout, "spooling " sge_uu32 " jobs\n", lGetNumberOfElem(*object_type_get_master_list(SGE_TYPE_JOB)));

   for_each_rw(job, *object_type_get_master_list(SGE_TYPE_JOB)) {
      const lList *ja_tasks = lGetList(job, JB_ja_tasks);
      if (ja_tasks == nullptr || lGetNumberOfElem(ja_tasks) == 0) {
         key = job_get_key(lGetUlong(job, JB_job_number), 0, nullptr, &key_ds);
         spool_write_object(&answer_list, context, job, key, SGE_TYPE_JOB, true);
         num_total++;
      } else {
         const lListElem *ja_task;
         for_each_rw(ja_task, ja_tasks) {
            key = job_get_key(lGetUlong(job, JB_job_number), lGetUlong(ja_task, JAT_task_number), nullptr, &key_ds);
            spool_write_object(&answer_list, context, ja_task, key, SGE_TYPE_JATASK, true);
            num_total++;
         }
      }
      answer_list_output(&answer_list);
   }

   sge_dstring_free(&key_ds);

   printf("==> spooled %d objects in total\n", num_total);

   return true;
}
#endif
static bool read_spooled_data()
{
   lList *answer_list = nullptr;
   const lListElem *context;

   context = spool_get_default_context();

   /* jobs */
   spool_read_list(&answer_list, context, object_type_get_master_list_rw(SGE_TYPE_JOB), SGE_TYPE_JOB);
   answer_list_output(&answer_list);
/*    DPRINTF(("read %d entries to Master_Job_List\n", lGetNumberOfElem(*object_type_get_master_list(SGE_TYPE_JOB)))); */

   return true;
}

static bool delete_spooled_data()
{
   lList *answer_list = nullptr;
   lListElem *job;
   const lListElem *context;
   char key[100];
   int num_total = 0;

   context = spool_get_default_context();

   /* jobs */
   for_each_rw(job, *object_type_get_master_list(SGE_TYPE_JOB)) {
      sprintf(key, sge_U32CFormat".0", sge_u32c(lGetUlong(job, JB_job_number)));
      spool_delete_object(&answer_list, context, SGE_TYPE_JOB, key, true);
      answer_list_output(&answer_list);
      num_total++;
   }

   printf("==> deleted %d objects in total\n", num_total);

   return true;
}

static void write_csv_header()
{
   static const char *header = "scenario,wallclock,utime,stime,utilization,jobs_per_second";
   FILE* csv;

   csv = fopen("spooling_performance.csv", "w");
   fprintf(csv, "%s\n", header);
   fclose(csv);
}

static void write_csv(const char *scenario, prof_level level)
{
   static const char *fmt = "%s,%.2f,%.2f,%.2f,%.0f,%.0f\n";

   FILE* csv;
   double busy;
   double utime;
   double stime;
   double utilization;
   double jobs_per_second;

   busy        = prof_get_total_busy(level, true, nullptr);
   utime       = prof_get_total_utime(level, true, nullptr);
   stime       = prof_get_total_stime(level, true, nullptr);
   utilization = busy > 0 ? (utime + stime) / busy * 100 : 0;
   jobs_per_second = busy > 0 ? num_jobs / busy : 0;

   csv = fopen("spooling_performance.csv", "a");
   fprintf(csv, fmt, scenario, busy, utime, stime, utilization, jobs_per_second);
   fclose(csv);
}

void clear_caches()
{
   printf("\n===> clear the filesystem caches\n");
   printf("on linux as user root: echo 3 >/proc/sys/vm/drop_caches\n");
   printf("press RETURN to continue ...\n");
   getc(stdin);
   printf("... continuing\n");
}

int main(int argc, char *argv[])
{
   lListElem *spooling_context;
   lList *answer_list = nullptr;

   DENTER_MAIN(TOP_LAYER, "test_performance");

#define NM10 "%I%I%I%I%I%I%I%I%I%I"
#define NM5  "%I%I%I%I%I"
#define NM2  "%I%I"
#define NM1  "%I"

   prof_start(SGE_PROF_CUSTOM1, nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM1, "performance", nullptr);

   prof_start(SGE_PROF_SPOOLINGIO, nullptr);
   prof_set_level_name(SGE_PROF_SPOOLINGIO, "io", nullptr);

   /* parse commandline parameters */
   if(argc != 4) {
      ERROR((SGE_EVENT, "usage: test_sge_spooling <method> <shared lib> <arguments>\n"));
      sge_exit(1);
   }

   *object_type_get_master_list_rw(SGE_TYPE_JOB) = lCreateList("job list", JB_Type);

#define defstring(str) #str

   /* initialize spooling */
   spooling_context = spool_create_dynamic_context(&answer_list, argv[1], argv[2], argv[3]);
   answer_list_output(&answer_list);
   if (spooling_context == nullptr) {
      sge_exit(EXIT_FAILURE);
   }

   spool_set_default_context(spooling_context);

   if (!spool_startup_context(&answer_list, spooling_context, true)) {
      answer_list_output(&answer_list);
      sge_exit(EXIT_FAILURE);
   }
   answer_list_output(&answer_list);

   /* initialize csv output file */
   write_csv_header();

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   generate_jobs(num_jobs);
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   write_csv("generating jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "generating jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, nullptr);
/*
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   copy = copy_jobs();
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   lFreeList(&copy);
   write_csv("copy jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "copy jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, nullptr);

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   copy = select_jobs(what_job);
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   lFreeList(&copy);
   write_csv("select jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "select jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, SGE_PROF_CUSTOM1, nullptr);
*/
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   spool_data();
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   write_csv("spool jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "spool jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, nullptr);
   write_csv("spooling io", SGE_PROF_SPOOLINGIO);
   prof_output_info(SGE_PROF_SPOOLINGIO, true, "IO:\n");
   prof_reset(SGE_PROF_SPOOLINGIO, nullptr);

   /* modify jobs */
   clear_caches();
   update_jobs();
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   spool_data();
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   write_csv("respool jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "respool jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, nullptr);
   write_csv("respooling io", SGE_PROF_SPOOLINGIO);
   prof_output_info(SGE_PROF_SPOOLINGIO, true, "IO:\n");
   prof_reset(SGE_PROF_SPOOLINGIO, nullptr);

   clear_caches();
   lFreeList(object_type_get_master_list_rw(SGE_TYPE_JOB));
   *object_type_get_master_list_rw(SGE_TYPE_JOB) = lCreateList("job list", JB_Type);
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   read_spooled_data();
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   write_csv("read jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "read jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, nullptr);

   clear_caches();
   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);
   delete_spooled_data();
   PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);
   write_csv("delete jobs", SGE_PROF_CUSTOM1);
   prof_output_info(SGE_PROF_CUSTOM1, true, "delete jobs:\n");
   prof_reset(SGE_PROF_CUSTOM1, nullptr);

   spool_shutdown_context(&answer_list, spooling_context);
   spool_startup_context(&answer_list, spooling_context, true);

   lFreeList(object_type_get_master_list_rw(SGE_TYPE_JOB));

   spool_shutdown_context(&answer_list, spooling_context);
   answer_list_output(&answer_list);

   DRETURN(EXIT_SUCCESS);
}
