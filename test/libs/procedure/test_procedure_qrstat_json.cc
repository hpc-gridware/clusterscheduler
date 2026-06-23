/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * Regression test for CS-2339:
 *   The qrstat JSON view passes server-supplied AR string fields (CE_stringval,
 *   MR_user, ARA_name, ...) to raw2quotedJSON(). A NULL value used to construct
 *   std::string(nullptr) -> crash (CWE-476). The fix is a NULL-safe
 *   raw2quotedJSON(const char*) overload in ProcedureView; this test drives the
 *   unguarded view nodes with NULL and asserts no crash + empty-string output.
 */

#include <cstdio>
#include <sstream>
#include <string>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include <sge_log.h>

#include "qrstat/ocs_QRStatParameterClient.h"
#include "qrstat/ocs_QRStatViewJSON.h"

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
   DENTER_MAIN(TOP_LAYER, "test_procedure_qrstat_json");
   int id = 1;

   // suppress ERROR-level sge_log output
   component_set_daemonized(true);

   ocs::QRStatParameterClient parameter("qrstat");

   printf("\n--- QRSTAT-JSON: raw2quotedJSON NULL overload ---\n");
   {
      // the centralized fix: a NULL const char* must yield an empty JSON string
      CHECK(id, "QRSTAT-JSON: raw2quotedJSON(NULL) -> empty string",
            ocs::QRStatViewJSON::raw2quotedJSON(static_cast<const char *>(nullptr)) == "\"\""); id++;
   }

   printf("\n--- QRSTAT-JSON: resource_list NULL fields ---\n");
   {
      // CE_stringval may be NULL for an unset string-typed centry
      ocs::QRStatViewJSON view(parameter);
      std::ostringstream os;
      view.report_resource_list_node_str(os, "myres", nullptr);
      const std::string out = os.str();
      CHECK(id, "QRSTAT-JSON: NULL resource value -> empty string",
            contains(out, "\"value\": \"\"")); id++;
   }
   {
      ocs::QRStatViewJSON view(parameter);
      std::ostringstream os;
      view.report_resource_list_node_str(os, nullptr, nullptr);
      const std::string out = os.str();
      CHECK(id, "QRSTAT-JSON: NULL resource name -> empty string",
            contains(out, "\"name\": \"\"")); id++;
   }

   printf("\n--- QRSTAT-JSON: mail_list NULL fields ---\n");
   {
      // MR_user is unguarded (the host was already guarded upstream)
      ocs::QRStatViewJSON view(parameter);
      std::ostringstream os;
      view.report_mail_list_node(os, nullptr, nullptr);
      const std::string out = os.str();
      CHECK(id, "QRSTAT-JSON: NULL mail user -> empty string",
            contains(out, "\"user\": \"\"")); id++;
      CHECK(id, "QRSTAT-JSON: NULL mail host -> empty string",
            contains(out, "\"host\": \"\"")); id++;
   }

   printf("\n--- QRSTAT-JSON: acl_list NULL name ---\n");
   {
      ocs::QRStatViewJSON view(parameter);
      std::ostringstream os;
      view.report_acl_list_node(os, nullptr);
      const std::string out = os.str();
      CHECK(id, "QRSTAT-JSON: NULL acl name -> empty string",
            contains(out, "\"\"")); id++;
   }

   printf("\n--- QRSTAT-JSON: valid input still renders ---\n");
   {
      ocs::QRStatViewJSON view(parameter);
      std::ostringstream os;
      view.report_resource_list_node_str(os, "slots", "5");
      const std::string out = os.str();
      CHECK(id, "QRSTAT-JSON: non-NULL value -> rendered verbatim",
            contains(out, "\"value\": \"5\"")); id++;
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
