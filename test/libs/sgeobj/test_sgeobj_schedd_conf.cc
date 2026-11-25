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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for schedd conf in `libs/sgeobj`
 */

#include <cstdio>
#include <string>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_UA.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/** @brief One value to try on the attribute under test, and what should happen */
typedef struct {
   const char *test_value;   ///< value to set on the attribute under test
   bool        result;       ///< true if the value should pass validation
   const char *desc;         ///< human-readable check label
} conf_settings_t;

/** @brief Which scheduler configuration attribute a block of settings applies to */
typedef struct {
   u_long      test_attribute;   ///< field to set (e.g. SC_halflife_decay_list)
   u_long      type;             ///< field type (MT_STRING, MT_DOUBLE, MT_UINT32)
} schedd_conf_t;

/**
 * @brief For each setting, create a default scheduler configuration, overwrite one attribute, validate the whole
 *        configuration and check that the result matches the expected outcome
 *
 * run_tests: for each setting, create a default schedd_conf, overwrite one attribute,
 * validate the whole config, and CHECK that the result matches the expected outcome
 * @param id the number of the next check; advanced by every check
 * @param conf the attribute and its type
 * @param settings the values and the expected outcomes, ended by a nullptr value
 */
static void run_tests(int *id, schedd_conf_t *conf, conf_settings_t *settings) {
   for (int i = 0; settings[i].test_value != nullptr; i++, (*id)++) {
      auto schedd_list = ocs::tom::ListPtr<ocs::tom::SC>::create("schedd_conf");
      const auto schedd_conf = schedd_list->push_back(sconf_create_default());
      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      bool passed = false;

      if (schedd_conf) {
         switch (conf->type) {
            case MT_STRING:
               schedd_conf.set(static_cast<int>(conf->test_attribute), settings[i].test_value);
               break;
            case MT_DOUBLE:
               break;
            case MT_UINT32:
               break;
            default:
               fprintf(stderr, "unsupported attribute type %d in test infrastructure\n",
                       (int)conf->type);
               break;
         }

         bool result = sconf_validate_config(&answer_list, *schedd_list);
         passed = (result == settings[i].result);
      }

      CHECK(*id, settings[i].desc, passed);
      schedd_list.reset();
      answer_list.reset();
   }
}

// --- halflife_decay_list ---
// format: "NONE" | "<key>=<val>[:<key>=<val>]*[:]"
// invalid if any colon-delimited token is not in "key=number" form

static schedd_conf_t halflife_conf = { SC_halflife_decay_list, MT_STRING };

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

static schedd_conf_t policy_hierarchy_conf = { SC_policy_hierarchy, MT_STRING };

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

static schedd_conf_t schedd_job_info_conf = { SC_schedd_job_info, MT_STRING };

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

static schedd_conf_t algorithm_conf = { SC_algorithm, MT_STRING };

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

static schedd_conf_t schedule_interval_conf = { SC_schedule_interval, MT_STRING };

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

static schedd_conf_t params_conf = { SC_params, MT_STRING };

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

// --- error messages (CS-3145) ---
// a refused configuration names the attribute and the value that failed

/**
 * @brief Whether one of the answers holds the given text
 * @param answer_list the answers of a validation
 * @param text the text to look for
 * @return true if an answer's text contains @p text
 */
static bool answers_contain(const ocs::tom::ListPtr<ocs::tom::AN> &answer_list, const std::string &text) {
   if (!answer_list) {
      return false;
   }
   for (const auto answer : *answer_list) {
      const char *answer_text = answer.get<AN_text>();
      if (answer_text != nullptr && std::string(answer_text).find(text) != std::string::npos) {
         return true;
      }
   }
   return false;
}

/**
 * @brief Print the answers of a validation, for the log of a failed check
 * @param answer_list the answers
 */
static void print_answers(const ocs::tom::ListPtr<ocs::tom::AN> &answer_list) {
   if (answer_list) {
      for (const auto answer : *answer_list) {
         const char *answer_text = answer.get<AN_text>();
         printf("      answer: %s\n", answer_text != nullptr ? answer_text : "(null)");
      }
   }
}

/**
 * @brief Validate a default configuration with one string attribute unset and check the message
 * @param id the number of the check; advanced by one
 * @param attribute the attribute to unset
 * @param expected the text the message must contain
 * @param desc the check's label
 */
static void check_unset_attribute(int *id, const int attribute, const char *expected, const char *desc) {
   auto schedd_list = ocs::tom::ListPtr<ocs::tom::SC>::create("schedd_conf");
   const auto schedd_conf = schedd_list->push_back(sconf_create_default());
   schedd_conf.set(attribute, static_cast<const char *>(nullptr));
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const bool result = sconf_validate_config(&answer_list, *schedd_list);
   const bool ok = !result && answers_contain(answer_list, expected);
   if (!ok) {
      print_answers(answer_list);
   }
   CHECK(*id, desc, ok);
   (*id)++;
}

/**
 * @brief Validate a configuration whose job load adjustments are too long and check that the message shows them
 * @param id the number of the check; advanced by one
 */
static void check_long_job_load_adjustments(int *id) {
   auto schedd_list = ocs::tom::ListPtr<ocs::tom::SC>::create("schedd_conf");
   const auto schedd_conf = schedd_list->push_back(sconf_create_default());
   // 100 entries of "load_adjustment_NN=0.50," are far beyond the 1024 characters the validation allows
   for (int i = 0; i < 100; i++) {
      const std::string name = "load_adjustment_" + std::to_string(i);
      const auto added = schedd_conf.emplace_back_in<SC_job_load_adjustments, CE_name>(name.c_str());
      added.set<CE_stringval>("0.50");
   }
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const bool result = sconf_validate_config(&answer_list, *schedd_list);
   const bool ok = !result && answers_contain(answer_list, "np_load_avg=0.50,load_adjustment_0=0.50");
   if (!ok) {
      print_answers(answer_list);
   }
   CHECK(*id, "job_load_adjustments: a list too long is refused and the message shows the list", ok);
   (*id)++;
}

/**
 * @brief Validate a configuration whose usage weight list is too long and check that the message shows the list
 *
 * The message printed an unrelated buffer instead of the list before CS-3145.
 *
 * @param id the number of the check; advanced by one
 */
static void check_long_usage_weight_list(int *id) {
   auto schedd_list = ocs::tom::ListPtr<ocs::tom::SC>::create("schedd_conf");
   const auto schedd_conf = schedd_list->push_back(sconf_create_default());
   // 100 entries of "usage_weight_NN=0.500000," are far beyond the 1024 characters the validation allows
   for (int i = 0; i < 100; i++) {
      const std::string name = "usage_weight_" + std::to_string(i);
      const auto added = schedd_conf.emplace_back_in<SC_usage_weight_list, UA_name>(name.c_str());
      added.set<UA_value>(0.5);
   }
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const bool result = sconf_validate_config(&answer_list, *schedd_list);
   const bool ok = !result && answers_contain(answer_list, "cannot parse usage weight list \"cpu=");
   if (!ok) {
      print_answers(answer_list);
   }
   CHECK(*id, "usage_weight_list: a list too long is refused and the message shows the list", ok);
   (*id)++;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_schedd_conf");
   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

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

   printf("\n--- error messages ---\n");
   check_long_job_load_adjustments(&id);
   check_unset_attribute(&id, SC_schedule_interval, "attribute \"schedule_interval\" is not defined",
                         "schedule_interval: an unset value is reported as not defined");
   check_unset_attribute(&id, SC_load_adjustment_decay_time, "attribute \"load_adjustment_decay_time\" is not defined",
                         "load_adjustment_decay_time: an unset value is reported as not defined");
   check_unset_attribute(&id, SC_reprioritize_interval, "attribute \"reprioritize_interval\" is not defined",
                         "reprioritize_interval: an unset value is reported as not defined");
   check_unset_attribute(&id, SC_policy_hierarchy, "attribute \"policy_hierarchy\" is not defined",
                         "policy_hierarchy: an unset value is reported as not defined");
   check_unset_attribute(&id, SC_default_duration, "attribute \"default_duration\" is not defined",
                         "default_duration: an unset value is reported as not defined");
   check_long_usage_weight_list(&id);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
