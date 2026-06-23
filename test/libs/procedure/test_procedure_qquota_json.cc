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
 * Regression test for CS-2338:
 *   QQuotaViewJSON::report_limit_string_value() must route the server-supplied
 *   scope name (RQS ST_name) through raw2quotedJSON() on BOTH branches. The
 *   same-filter (append) branch previously emitted the value raw inside a
 *   hand-written quoted string, so a name containing '"', '\\' or a newline
 *   could break out of the JSON string and inject arbitrary keys. The
 *   new-filter branch escaped but had no NULL guard.
 *
 * This test drives the view directly (no qmaster needed): it feeds crafted and
 * NULL scope values through both branches and asserts the output is escaped and
 * does not contain a raw breakout sequence.
 */

#include <cstdio>
#include <sstream>
#include <string>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include <sge_log.h>

#include "qquota/ocs_QQuotaParameterClient.h"
#include "qquota/ocs_QQuotaViewJSON.h"

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

/** @brief True if @p haystack contains @p needle as a substring. */
static bool
contains(const std::string &haystack, const std::string &needle) {
   return haystack.find(needle) != std::string::npos;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_procedure_qquota_json");
   int id = 1;

   // suppress ERROR-level sge_log output
   component_set_daemonized(true);

   ocs::QQuotaParameterClient parameter("qquota");

   // a scope name crafted to break out of the JSON string and inject keys
   const char *crafted = "alice\",\"injected\":1,\"x\":\"";
   const std::string escaped = ocs::QQuotaViewJSON::raw2quotedJSON(crafted);

   printf("\n--- QQUOTA-JSON: same-filter scope value escaping ---\n");
   {
      ocs::QQuotaViewJSON view(parameter);
      std::ostringstream os;
      // two scopes under the SAME filter name: the 2nd drives the append branch
      // (the formerly-raw line) which is the actual CS-2338 path
      view.report_limit_string_value(os, "users", "bob", false);
      view.report_limit_string_value(os, "users", crafted, false);
      const std::string out = os.str();

      // the crafted value must appear in its fully escaped form ...
      CHECK(id, "QQUOTA-JSON: same-filter crafted value -> escaped",
            contains(out, escaped)); id++;
      // ... and the raw breakout sequence ("alice","injected":1) must NOT appear
      CHECK(id, "QQUOTA-JSON: same-filter crafted value -> no raw breakout",
            !contains(out, "alice\",\"injected\":1")); id++;
   }

   printf("\n--- QQUOTA-JSON: new-filter scope value escaping ---\n");
   {
      ocs::QQuotaViewJSON view(parameter);
      std::ostringstream os;
      view.report_limit_string_value(os, "users", crafted, false);
      const std::string out = os.str();
      CHECK(id, "QQUOTA-JSON: new-filter crafted value -> escaped",
            contains(out, escaped)); id++;
      CHECK(id, "QQUOTA-JSON: new-filter crafted value -> no raw breakout",
            !contains(out, "alice\",\"injected\":1")); id++;
   }

   printf("\n--- QQUOTA-JSON: NULL scope value guard ---\n");
   {
      // new-filter branch with NULL must not crash and renders an empty string
      ocs::QQuotaViewJSON view(parameter);
      std::ostringstream os;
      view.report_limit_string_value(os, "queues", nullptr, false);
      const std::string out = os.str();
      CHECK(id, "QQUOTA-JSON: new-filter NULL value -> empty string",
            contains(out, "\"name\": \"\"")); id++;
   }
   {
      // same-filter (append) branch with NULL must not crash either
      ocs::QQuotaViewJSON view(parameter);
      std::ostringstream os;
      view.report_limit_string_value(os, "queues", "q1", false);
      view.report_limit_string_value(os, "queues", nullptr, false);
      const std::string out = os.str();
      CHECK(id, "QQUOTA-JSON: same-filter NULL value -> empty string",
            contains(out, "\"name\": \"\"")); id++;
   }

   printf("\n--- QQUOTA-JSON: rule/resource NULL guards ---\n");
   {
      // NULL rqs_name/rule_name must not crash and render empty strings
      ocs::QQuotaViewJSON view(parameter);
      std::ostringstream os;
      view.report_limit_rule_begin(os, nullptr, nullptr);
      const std::string out = os.str();
      CHECK(id, "QQUOTA-JSON: NULL rqs_name -> empty string",
            contains(out, "\"rqs_name\": \"\"")); id++;
      CHECK(id, "QQUOTA-JSON: NULL rule_name -> empty string",
            contains(out, "\"rule_name\": \"\"")); id++;
   }
   {
      // NULL resource name must not crash and renders an empty string
      ocs::QQuotaViewJSON view(parameter);
      std::ostringstream os;
      view.report_resource_value(os, nullptr, 5, 0);
      const std::string out = os.str();
      CHECK(id, "QQUOTA-JSON: NULL resource -> empty string",
            contains(out, "\"resource\": \"\"")); id++;
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
