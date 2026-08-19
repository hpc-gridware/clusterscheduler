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

#include <cmath>
#include <cstdio>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "cull/cull.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_rusage.h"

#include "basis_types.h"

// usage of the master task and of each of the two pe tasks. The values are kept
// apart so a wrong sum is recognisable in the failure output.
#define WALLCLOCK_MASTER 100.0
#define WALLCLOCK_SLAVE   50.0
#define CPU_MASTER        10.0
#define CPU_SLAVE         20.0
#define NUM_PE_TASKS      2

/**
 * Builds a tightly integrated job with a master task and NUM_PE_TASKS pe tasks,
 * lets sge_write_rusage() write one JSONL acct record for it and returns the
 * record's wallclock and cpu.
 *
 * The pe object is put into the data store because sge_write_rusage() looks up
 * the granted pe there to decide whether it has to aggregate over the pe tasks.
 */
static bool
write_acct_record(bool accounting_summary, bool intermediate, double *wallclock, double *cpu) {
   // the parallel environment - accounting_summary is only honoured for a
   // tightly integrated one, so control_slaves has to be set as well
   lList **master_pe_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_PE);
   lFreeList(master_pe_list);
   lListElem *pe = lAddElemStr(master_pe_list, PE_name, "testpe", PE_Type);
   lSetBool(pe, PE_control_slaves, true);
   lSetBool(pe, PE_accounting_summary, accounting_summary);

   lListElem *job = lCreateElem(JB_Type);
   lSetUlong(job, JB_job_number, 4711);
   lSetString(job, JB_owner, "testuser");
   lSetString(job, JB_group, "testgroup");

   lListElem *ja_task = lCreateElem(JAT_Type);
   lSetUlong(ja_task, JAT_task_number, 1);
   lSetString(ja_task, JAT_granted_pe, "testpe");
   lList *usage = lGetOrCreateList(ja_task, JAT_scaled_usage_list, "usage", UA_Type);
   usage_list_set_double_usage(usage, USAGE_ATTR_WALLCLOCK, WALLCLOCK_MASTER);
   usage_list_set_double_usage(usage, USAGE_ATTR_CPU, CPU_MASTER);
   usage_list_set_double_usage(usage, USAGE_ATTR_CPU_ACCT, CPU_MASTER);

   for (int i = 0; i < NUM_PE_TASKS; i++) {
      char pe_task_id[64];
      snprintf(pe_task_id, sizeof(pe_task_id), "%d.testhost", i + 1);
      lListElem *pe_task = lAddSubStr(ja_task, PET_id, pe_task_id, JAT_task_list, PET_Type);
      lList *pe_usage = lGetOrCreateList(pe_task, PET_scaled_usage, "usage", UA_Type);
      usage_list_set_double_usage(pe_usage, USAGE_ATTR_WALLCLOCK, WALLCLOCK_SLAVE);
      usage_list_set_double_usage(pe_usage, USAGE_ATTR_CPU, CPU_SLAVE);
      usage_list_set_double_usage(pe_usage, USAGE_ATTR_CPU_ACCT, CPU_SLAVE);
   }

   // a job report of the master task - a pe task's report would carry JR_pe_task_id_str
   lListElem *jr = lCreateElem(JR_Type);
   lSetUlong(jr, JR_job_number, 4711);

   rapidjson::StringBuffer json_buffer;
   rapidjson::Writer<rapidjson::StringBuffer> writer(json_buffer);
   bool ret = sge_write_rusage(nullptr, &writer, jr, job, ja_task, "category", nullptr, 0,
                               intermediate, true);

   if (ret) {
      rapidjson::Document doc;
      if (doc.Parse(json_buffer.GetString()).HasParseError() ||
          !doc.HasMember("usage") || !doc["usage"].HasMember("eusage")) {
         printf("   the record is not a JSON object with usage/eusage:\n   %s\n",
                json_buffer.GetString());
         ret = false;
      } else {
         const rapidjson::Value &eusage = doc["usage"]["eusage"];
         if (!eusage.HasMember(USAGE_ATTR_WALLCLOCK) || !eusage.HasMember(USAGE_ATTR_CPU)) {
            printf("   the record has no wallclock or no cpu:\n   %s\n", json_buffer.GetString());
            ret = false;
         } else {
            *wallclock = eusage[USAGE_ATTR_WALLCLOCK].GetDouble();
            *cpu = eusage[USAGE_ATTR_CPU].GetDouble();
         }
      }
   } else {
      printf("   sge_write_rusage() did not write a record\n");
   }

   lFreeElem(&jr);
   lFreeElem(&ja_task);
   lFreeElem(&job);
   lFreeList(master_pe_list);

   return ret;
}

/**
 * Compares one value of the acct record against what it has to be.
 */
static int
check_value(const char *test, const char *attribute, double is, double should) {
   if (fabs(is - should) > 0.001) {
      printf("FAILED: %s: %s is %.3f, expected %.3f\n", test, attribute, is, should);
      return 1;
   }
   printf("   %s is %.3f\n", attribute, is);
   return 0;
}

int
main(int argc, char *argv[]) {
   int failed = 0;

   lInit(nmv);

   // The usage of the pe tasks of a job with accounting_summary has to be
   // aggregated into the master task's record - but wallclock is elapsed time
   // and the pe tasks run concurrently, so it is the only attribute that must
   // not be added up (CS-2616). cpu is checked next to it: it is a consumable
   // and has to keep being summed, so the two together pin the behaviour from
   // both sides.
   printf("accounting_summary TRUE, intermediate record\n");
   {
      double wallclock = 0.0, cpu = 0.0;
      if (write_acct_record(true, true, &wallclock, &cpu)) {
         failed += check_value("accounting_summary TRUE", USAGE_ATTR_WALLCLOCK,
                               wallclock, WALLCLOCK_MASTER);
         failed += check_value("accounting_summary TRUE", USAGE_ATTR_CPU,
                               cpu, CPU_MASTER + NUM_PE_TASKS * CPU_SLAVE);
      } else {
         failed++;
      }
   }

   // Without accounting_summary the pe tasks get their own records, so nothing
   // is aggregated at all and both attributes stay at the master task's value.
   printf("accounting_summary FALSE, intermediate record\n");
   {
      double wallclock = 0.0, cpu = 0.0;
      if (write_acct_record(false, true, &wallclock, &cpu)) {
         failed += check_value("accounting_summary FALSE", USAGE_ATTR_WALLCLOCK,
                               wallclock, WALLCLOCK_MASTER);
         failed += check_value("accounting_summary FALSE", USAGE_ATTR_CPU,
                               cpu, CPU_MASTER);
      } else {
         failed++;
      }
   }

   if (failed == 0) {
      printf("PASS: test solved!\n");
   } else {
      printf("FAILED: %d test(s) NOT solved!\n", failed);
   }

   return failed;
}
