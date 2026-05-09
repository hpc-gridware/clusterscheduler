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

#include <cstdio>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"

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

typedef struct {
   const char *test_value;   ///< value to set on the attribute under test
   bool        result;       ///< true if the value should pass validation
   const char *desc;         ///< human-readable check label
} conf_settings_t;

typedef struct {
   u_long      test_attribute;   ///< CULL attribute to set (e.g. SC_halflife_decay_list)
   u_long      type;             ///< CULL attribute type (lStringT, lDoubleT, lUlongT)
} schedd_conf_t;

// run_tests: for each setting, create a default schedd_conf, overwrite one attribute,
// validate the whole config, and CHECK that the result matches the expected outcome
static void run_tests(int *id, schedd_conf_t *conf, conf_settings_t *settings) {
   for (int i = 0; settings[i].test_value != nullptr; i++, (*id)++) {
      lListElem *schedd_conf = sconf_create_default();
      lList *schedd_list = lCreateList("schedd_conf", SC_Type);
      lList *answer_list = nullptr;
      bool passed = false;

      if (schedd_conf != nullptr && schedd_list != nullptr) {
         lAppendElem(schedd_list, schedd_conf);

         switch (conf->type) {
            case lStringT:
               lSetString(schedd_conf, (int)conf->test_attribute, settings[i].test_value);
               break;
            case lDoubleT:
               break;
            case lUlongT:
               break;
            default:
               fprintf(stderr, "unsupported attribute type %d in test infrastructure\n",
                       (int)conf->type);
               break;
         }

         bool result = sconf_validate_config(&answer_list, schedd_list);
         passed = (result == settings[i].result);
      }

      CHECK(*id, settings[i].desc, passed);
      lFreeList(&schedd_list);
      lFreeList(&answer_list);
   }
}

// --- halflife_decay_list ---
// format: "NONE" | "<key>=<val>[:<key>=<val>]*[:]"
// invalid if any colon-delimited token is not in "key=number" form

static schedd_conf_t halflife_conf = { SC_halflife_decay_list, lStringT };

static conf_settings_t halflife_settings[] = {
   {"NONE",
    true,  "halflife_decay_list: \"NONE\" sentinel is accepted"},
   {"nonesense",
    false, "halflife_decay_list: plain word without key=value format is rejected"},
   {"what:ever=this_is",
    false, "halflife_decay_list: colon before equals (no numeric value) is rejected"},
   {"cpu=1",
    true,  "halflife_decay_list: single metric cpu=1 is valid"},
   {"cpu=1:",
    true,  "halflife_decay_list: trailing colon after single metric is accepted"},
   {"cpu=1:io=-1",
    true,  "halflife_decay_list: two metrics with negative value are valid"},
   {"cpu=1:io=0:",
    true,  "halflife_decay_list: two metrics with trailing colon are valid"},
   {"cpu=1:io=-1:mem=0",
    true,  "halflife_decay_list: three metrics are valid"},
   {"cpu=1:io=-1:mem=0:help=-1",
    true,  "halflife_decay_list: four metrics are valid"},
   {"cpu=",
    false, "halflife_decay_list: key without value (cpu=) is rejected"},
   {nullptr, false, nullptr}
};

// --- policy_hierarchy ---
// valid chars: O, F, S — each may appear at most once; "NONE" and "" skip validation

static schedd_conf_t policy_hierarchy_conf = { SC_policy_hierarchy, lStringT };

static conf_settings_t policy_hierarchy_settings[] = {
   {"OFS",
    true,  "policy_hierarchy: \"OFS\" (all valid chars, default value) is accepted"},
   {"OS",
    true,  "policy_hierarchy: \"OS\" (valid subset, no duplicates) is accepted"},
   {"NONE",
    true,  "policy_hierarchy: \"NONE\" sentinel skips validation and is accepted"},
   {"OO",
    false, "policy_hierarchy: \"OO\" (duplicate character) is rejected"},
   {"OFX",
    false, "policy_hierarchy: \"OFX\" (unknown character X) is rejected"},
   {nullptr, false, nullptr}
};

// --- schedd_job_info ---
// valid: "true", "false", "job_list <range>"

static schedd_conf_t schedd_job_info_conf = { SC_schedd_job_info, lStringT };

static conf_settings_t schedd_job_info_settings[] = {
   {"true",
    true,  "schedd_job_info: \"true\" enables job info reporting"},
   {"false",
    true,  "schedd_job_info: \"false\" disables job info reporting"},
   {"job_list 1-10",
    true,  "schedd_job_info: \"job_list 1-10\" with valid range is accepted"},
   {"invalid",
    false, "schedd_job_info: unknown keyword is rejected"},
   {nullptr, false, nullptr}
};

// --- algorithm ---
// valid: "default" | "simple_sched" | "ext_<anything>"

static schedd_conf_t algorithm_conf = { SC_algorithm, lStringT };

static conf_settings_t algorithm_settings[] = {
   {"default",
    true,  "algorithm: \"default\" is accepted"},
   {"simple_sched",
    true,  "algorithm: \"simple_sched\" is accepted"},
   {"ext_custom",
    true,  "algorithm: any \"ext_*\" name is accepted"},
   {"badname",
    false, "algorithm: unknown name is rejected"},
   {nullptr, false, nullptr}
};

// --- schedule_interval ---
// time string in HH:MM:SS format, parsed by extended_parse_ulong_val(TIME)

static schedd_conf_t schedule_interval_conf = { SC_schedule_interval, lStringT };

static conf_settings_t schedule_interval_settings[] = {
   {"0:0:15",
    true,  "schedule_interval: \"0:0:15\" (15 s, default) is accepted"},
   {"0:1:0",
    true,  "schedule_interval: \"0:1:0\" (1 min) is accepted"},
   {"notatime",
    false, "schedule_interval: non-time string is rejected"},
   {nullptr, false, nullptr}
};

// --- params ---
// space/comma-delimited keywords; valid set: NONE, PROFILE, MONITOR,
// DURATION_OFFSET, PE_RANGE_ALG — unknown keywords are rejected

static schedd_conf_t params_conf = { SC_params, lStringT };

static conf_settings_t params_settings[] = {
   {"none",
    true,  "params: \"none\" sentinel is accepted"},
   {"DURATION_OFFSET=30",
    true,  "params: known keyword DURATION_OFFSET is accepted"},
   {"PROFILE=1, DURATION_OFFSET=60",
    true,  "params: multiple known keywords are accepted"},
   {"UNKNOWN_PARAM=1",
    false, "params: unknown keyword is rejected"},
   {nullptr, false, nullptr}
};

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_schedd_conf");
   component_set_daemonized(true);
   lInit(nmv);

   int id = 1;

   printf("\n--- halflife_decay_list ---\n");
   run_tests(&id, &halflife_conf, halflife_settings);

   printf("\n--- policy_hierarchy ---\n");
   run_tests(&id, &policy_hierarchy_conf, policy_hierarchy_settings);

   printf("\n--- schedd_job_info ---\n");
   run_tests(&id, &schedd_job_info_conf, schedd_job_info_settings);

   printf("\n--- algorithm ---\n");
   run_tests(&id, &algorithm_conf, algorithm_settings);

   printf("\n--- schedule_interval ---\n");
   run_tests(&id, &schedule_interval_conf, schedule_interval_settings);

   printf("\n--- params ---\n");
   run_tests(&id, &params_conf, params_settings);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
