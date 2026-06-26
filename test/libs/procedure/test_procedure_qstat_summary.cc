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

#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include <sge_log.h>

#include "qstat/default/ocs_QStatDefaultViewBase.h"

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

// SECURITY REGRESSION (CS-2366, LOW-QSTAT-003, use-after-scope):
// QStatDefaultViewBase::queue_summary_t's queue_type/arch/state used to be
// const char* aliasing stack locals of the controller — a latent dangling-
// pointer if a summary ever outlived that frame. They are now std::string and
// own their data. These checks pin that ownership: assigning from a buffer that
// is then mutated/destroyed must leave the stored value intact. Against the
// unpatched (const char*) struct, s.arch would alias the buffer and reflect the
// mutation / dangle after scope, so the assertions fail.
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_procedure_qstat_summary");
   int id = 1;

   component_set_daemonized(true);

   using QS = ocs::QStatDefaultViewBase::queue_summary_t;

   // T01: the fields are owning std::string (compile-time guarantee, surfaced at runtime)
   CHECK(id, "queue_type/arch/state are std::string (own their storage)",
         (std::is_same_v<decltype(QS{}.queue_type), std::string> &&
          std::is_same_v<decltype(QS{}.arch), std::string> &&
          std::is_same_v<decltype(QS{}.state), std::string>)); id++;

   QS s{};

   // T02/T03: arch keeps its own copy across source mutation and source scope end
   {
      char buf[80];
      std::snprintf(buf, sizeof(buf), "lx-amd64");
      s.arch = buf;
      std::snprintf(buf, sizeof(buf), "MUTATED!");   // mutate the source buffer
      CHECK(id, "arch keeps its own copy after source mutation", s.arch == "lx-amd64"); id++;
   }
   CHECK(id, "arch survives the source buffer going out of scope", s.arch == "lx-amd64"); id++;

   // T04/T05: same for state
   {
      char buf[80];
      std::snprintf(buf, sizeof(buf), "running");
      s.state = buf;
      std::snprintf(buf, sizeof(buf), "XXXXXXX");
      CHECK(id, "state keeps its own copy after source mutation", s.state == "running"); id++;
   }
   CHECK(id, "state survives the source buffer going out of scope", s.state == "running"); id++;

   // T06/T07: same for queue_type
   {
      char buf[80];
      std::snprintf(buf, sizeof(buf), "BIP");
      s.queue_type = buf;
      std::snprintf(buf, sizeof(buf), "ZZZ");
      CHECK(id, "queue_type keeps its own copy after source mutation", s.queue_type == "BIP"); id++;
   }
   CHECK(id, "queue_type survives the source buffer going out of scope", s.queue_type == "BIP"); id++;

   // T08: an absent arch is represented as the empty string (the former nullptr case)
   s.arch.clear();
   CHECK(id, "cleared arch is empty (absent)", s.arch.empty()); id++;

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
