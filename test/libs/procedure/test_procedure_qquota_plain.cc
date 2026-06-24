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
 * Tests for QQuotaViewPlain::report_resource_value (CS-2348 display fix):
 * a static RQS limit must be rendered human-readably by attribute type
 * (memory -> "4.000G", time -> "01:00:00", integer -> plain "10") and must
 * not be truncated. Previously the plain view printed the raw byte count
 * truncated to 20 chars ("4294967296" -> "42949672").
 */

#include <cstdio>
#include <sstream>
#include <string>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_CEntry.h"

#include "qquota/ocs_QQuotaParameterClient.h"
#include "qquota/ocs_QQuotaViewPlain.h"

#include <sge_log.h>

using ocs::CEntry;

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

static bool contains(const std::string &haystack, const std::string &needle) {
   return haystack.find(needle) != std::string::npos;
}

/** @brief Render one resource value through the plain view and return the output. */
static std::string render(ocs::QQuotaViewPlain &view, const char *resource,
                          CEntry::Type type, uint64_t max, uint64_t used) {
   std::ostringstream os;
   view.report_resource_value(os, resource, type, max, used);
   return os.str();
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_procedure_qquota_plain");
   int id = 1;
   component_set_daemonized(true);

   ocs::QQuotaParameterClient parameter("qquota");
   ocs::QQuotaViewPlain view(parameter);

   const uint64_t four_gib = 4ULL * 1024 * 1024 * 1024;   // 4294967296
   const uint64_t two_gib = 2ULL * 1024 * 1024 * 1024;

   printf("\n--- QQUOTA-PLAIN: memory rendered human-readably (CS-2348) ---\n");
   {
      std::string out = render(view, "test_memory", CEntry::Type::MEM, four_gib, 0);
      CHECK(id++, "QQUOTA-PLAIN: 4 GiB memory limit -> 4.000G", contains(out, "test_memory=4.000G"));
      // the value must NOT be truncated to a misleading raw byte count
      CHECK(id++, "QQUOTA-PLAIN: not the truncated raw byte count", !contains(out, "test_memory=42949672"));
   }
   {
      std::string out = render(view, "test_memory", CEntry::Type::MEM, four_gib, two_gib);
      CHECK(id++, "QQUOTA-PLAIN: used/max memory -> 2.000G/4.000G", contains(out, "test_memory=2.000G/4.000G"));
   }

   printf("\n--- QQUOTA-PLAIN: time rendered as h:m:s ---\n");
   {
      std::string out = render(view, "rt", CEntry::Type::TIME, 3600, 0);
      CHECK(id++, "QQUOTA-PLAIN: 3600s -> 01:00:00", contains(out, "rt=01:00:00"));
   }

   printf("\n--- QQUOTA-PLAIN: integer rendered without .000000 ---\n");
   {
      std::string out = render(view, "slots", CEntry::Type::INT, 10, 0);
      CHECK(id++, "QQUOTA-PLAIN: INT limit -> plain integer", contains(out, "slots=10"));
      CHECK(id++, "QQUOTA-PLAIN: INT limit not rendered as 10.000000", !contains(out, "10.000000"));
   }
   {
      // a large integer value must be shown in full (no 20-char truncation)
      std::string out = render(view, "test_memory", CEntry::Type::INT, four_gib, 0);
      CHECK(id++, "QQUOTA-PLAIN: large integer not truncated", contains(out, "test_memory=4294967296"));
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
