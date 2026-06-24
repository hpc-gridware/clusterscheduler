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
 * Tests for parse_ulong_val() / extended_parse_ulong_val() - the type-aware
 * complex-attribute value parser.
 *
 * Motivated by CS-2348: qquota used std::stoul() on a typed RQS limit value,
 * which crashed on bad input and silently mis-parsed typed values (e.g. "4G" ->
 * 4). This pins the memory/time/numeric/bool/loglevel parsing, the infinity and
 * only-positive handling, and the (non-throwing) rejection of invalid input that
 * the qquota fix now relies on.
 */

#include <cstdio>
#include <cfloat>

#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_CEntry.h"

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

/** @brief Run parse_ulong_val(); returns success and fills @p dval / @p uval. */
static bool parse(CEntry::Type type, const char *s, double &dval, uint32_t &uval) {
   dval = 0.0;
   uval = 0;
   return parse_ulong_val(&dval, &uval, type, s, nullptr, 0) != 0;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_parse_num");
   int id = 1;
   double d = 0.0;
   uint32_t u = 0;

   printf("\n--- PARSE-NUM: memory values ---\n");
   {
      CHECK(id++, "MEM \"4G\" -> 4*1024^3", parse(CEntry::Type::MEM, "4G", d, u) && d == 4.0 * 1024 * 1024 * 1024);
      CHECK(id++, "MEM \"512M\" -> 512*1024^2", parse(CEntry::Type::MEM, "512M", d, u) && d == 512.0 * 1024 * 1024);
      CHECK(id++, "MEM \"1K\" -> 1024 (binary)", parse(CEntry::Type::MEM, "1K", d, u) && d == 1024.0);
      CHECK(id++, "MEM \"1k\" -> 1000 (decimal)", parse(CEntry::Type::MEM, "1k", d, u) && d == 1000.0);
      CHECK(id++, "MEM \"2g\" -> 2*10^9 (decimal)", parse(CEntry::Type::MEM, "2g", d, u) && d == 2.0 * 1000 * 1000 * 1000);
   }

   printf("\n--- PARSE-NUM: time values (h:m:s) ---\n");
   {
      CHECK(id++, "TIME \"1:0:0\" -> 3600s", parse(CEntry::Type::TIME, "1:0:0", d, u) && d == 3600.0);
      CHECK(id++, "TIME \"0:50:0\" -> 3000s", parse(CEntry::Type::TIME, "0:50:0", d, u) && d == 3000.0);
   }

   printf("\n--- PARSE-NUM: numeric / double ---\n");
   {
      CHECK(id++, "INT \"5\" -> 5", parse(CEntry::Type::INT, "5", d, u) && d == 5.0);
      CHECK(id++, "DOUBLE \"1.5\" -> 1.5", parse(CEntry::Type::DOUBLE, "1.5", d, u) && d == 1.5);
      CHECK(id++, "DOUBLE \"-2.5\" -> -2.5 (negatives allowed)", parse(CEntry::Type::DOUBLE, "-2.5", d, u) && d == -2.5);
   }

   printf("\n--- PARSE-NUM: boolean ---\n");
   {
      CHECK(id++, "BOOL \"true\" -> 1", parse(CEntry::Type::BOOL, "true", d, u) && u == 1);
      CHECK(id++, "BOOL \"false\" -> 0", parse(CEntry::Type::BOOL, "false", d, u) && u == 0);
   }

   printf("\n--- PARSE-NUM: infinity ---\n");
   {
      CHECK(id++, "MEM \"infinity\" -> DBL_MAX (enabled by default)",
            parse(CEntry::Type::MEM, "infinity", d, u) && d == DBL_MAX);
      // extended_parse_ulong_val with enable_infinity = 0 must reject it
      CHECK(id++, "MEM \"infinity\" rejected when infinity disabled",
            extended_parse_ulong_val(&d, &u, CEntry::Type::MEM, "infinity", nullptr, 0, 0, false) == 0);
   }

   printf("\n--- PARSE-NUM: loglevel (TYPE_LOG) ---\n");
   {
      CHECK(id++, "TYPE_LOG \"log_info\" -> accepted", parse(CEntry::Type::TYPE_LOG, "log_info", d, u));
      CHECK(id++, "TYPE_LOG \"bogus\" -> rejected", !parse(CEntry::Type::TYPE_LOG, "bogus", d, u));
   }

   printf("\n--- PARSE-NUM: only-positive constraint ---\n");
   {
      CHECK(id++, "only_positive rejects \"-5\"",
            extended_parse_ulong_val(&d, &u, CEntry::Type::INT, "-5", nullptr, 0, 1, true) == 0);
      CHECK(id++, "only_positive accepts \"5\"",
            extended_parse_ulong_val(&d, &u, CEntry::Type::INT, "5", nullptr, 0, 1, true) != 0 && d == 5.0);
   }

   printf("\n--- PARSE-NUM: invalid input rejected, not crashing (CS-2348) ---\n");
   {
      CHECK(id++, "INT \"abc\" -> rejected", !parse(CEntry::Type::INT, "abc", d, u));
      CHECK(id++, "MEM empty string -> rejected", !parse(CEntry::Type::MEM, "", d, u));
      CHECK(id++, "MEM NULL -> rejected, no crash", !parse(CEntry::Type::MEM, nullptr, d, u));
      // unsupported type falls through to the default branch -> rejected
      CHECK(id++, "STR type -> rejected (unsupported)", !parse(CEntry::Type::STR, "anything", d, u));
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
