/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
#include <cstdlib>
#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "sge_time.h"

#include <sge_log.h>

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

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_time");
   int id = 1;

   uint64_t t = sge_get_gmt64();
   time_t now = time(nullptr);

   printf("\n--- gmt64 conversions ---\n");
   // T01: seconds extracted from gmt64 should be within 1s of wall clock
   CHECK(id, "sge_gmt64_to_gmt32 matches time(nullptr) within 1s",
         labs((long)(sge_gmt64_to_gmt32(t) - (uint32_t)now)) <= 1); id++;
   // T02: gmt64_to_time_t should be within 1s of wall clock
   CHECK(id, "sge_gmt64_to_time_t matches time(nullptr) within 1s",
         labs((long)(sge_gmt64_to_time_t(t) - now)) <= 1); id++;
   // T03: double representation should be within 1.0 of wall clock seconds
   {
      double d = sge_gmt64_to_gmt32_double(t);
      double diff = d - (double)now;
      CHECK(id, "sge_gmt64_to_gmt32_double matches time(nullptr) within 1.0",
            d > 0.0 && diff > -1.0 && diff < 1.0);
   } id++;
   // T04: 1 second expressed as gmt32 becomes exactly 1,000,000 µs in gmt64
   CHECK(id, "sge_gmt32_to_gmt64(1) == 1000000",
         sge_gmt32_to_gmt64(1) == UINT64_C(1000000)); id++;
   // T05: round-trip gmt64 → gmt32 → gmt64 preserves the seconds portion
   {
      uint32_t s32 = sge_gmt64_to_gmt32(t);
      CHECK(id, "sge_gmt32_to_gmt64 round-trip preserves seconds",
            sge_gmt64_to_gmt32(sge_gmt32_to_gmt64(s32)) == s32);
   } id++;
   // T06: two consecutive calls must be non-decreasing (monotonicity)
   {
      uint64_t t1 = sge_get_gmt64();
      uint64_t t2 = sge_get_gmt64();
      CHECK(id, "sge_get_gmt64 is monotonically non-decreasing", t2 >= t1);
   } id++;
   // T07: zero is a valid input — gmt32_to_gmt64(0) must be 0
   CHECK(id, "sge_gmt32_to_gmt64(0) == 0",
         sge_gmt32_to_gmt64(0) == UINT64_C(0)); id++;
   // T08: zero is a valid input — gmt64_to_gmt32(0) must be 0
   CHECK(id, "sge_gmt64_to_gmt32(0) == 0",
         sge_gmt64_to_gmt32(UINT64_C(0)) == 0); id++;

   printf("\n--- time_t and timespec ---\n");
   // T09: gmt64_to_time_t is the integer division by 1,000,000
   CHECK(id, "sge_gmt64_to_time_t equals t / 1000000",
         sge_gmt64_to_time_t(t) == (time_t)(t / 1000000)); id++;
   // T10/T11: timespec tv_sec and tv_nsec are consistent with the gmt64 value
   {
      struct timespec ts{};
      sge_gmt64_to_timespec(t, ts);
      CHECK(id, "sge_gmt64_to_timespec: tv_sec matches sge_gmt64_to_time_t",
            ts.tv_sec == sge_gmt64_to_time_t(t)); id++;
      CHECK(id, "sge_gmt64_to_timespec: tv_nsec in [0, 999999000]",
            ts.tv_nsec >= 0 && ts.tv_nsec <= 999999000); id++;
   }
   // T12: sge_time_t_to_gmt64 round-trips to within 1s of the original time_t
   CHECK(id, "sge_time_t_to_gmt64 round-trips within 1s",
         labs((long)(sge_gmt64_to_gmt32(sge_time_t_to_gmt64(now)) - (uint32_t)now)) <= 1); id++;

   printf("\n--- format functions return non-null ---\n");
   {
      DSTRING_STATIC(dstr, 256);
      // T13: default sge_ctime64 (local time)
      CHECK(id, "sge_ctime64 (default) returns non-null",
            sge_ctime64(t, &dstr) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T14: sge_ctime64 plain (no xml, no micro)
      CHECK(id, "sge_ctime64 (plain) returns non-null",
            sge_ctime64(t, &dstr, false, false) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T15: sge_ctime64 xml with microseconds
      CHECK(id, "sge_ctime64 (xml+micro) returns non-null",
            sge_ctime64(t, &dstr, true, true) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T16: short format (HH:MM:SS)
      CHECK(id, "sge_ctime64_short returns non-null",
            sge_ctime64_short(t, &dstr) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T17: xml format
      CHECK(id, "sge_ctime64_xml returns non-null",
            sge_ctime64_xml(t, &dstr) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T18: date-time format (YYYY-MM-DD HH:MM:SS)
      CHECK(id, "sge_ctime64_date_time returns non-null",
            sge_ctime64_date_time(t, &dstr) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T19: append_time (uint64_t overload, non-xml)
      CHECK(id, "append_time (uint64_t, non-xml) returns non-null",
            append_time(t, &dstr, false) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T20: append_time (time_t overload, non-xml)
      CHECK(id, "append_time (time_t, non-xml) returns non-null",
            append_time(now, &dstr, false) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T21: append_time (uint64_t, xml)
      CHECK(id, "append_time (uint64_t, xml) returns non-null",
            append_time(t, &dstr, true) != nullptr); id++;
      sge_dstring_clear(&dstr);
      // T22: append_time (time_t, xml)
      CHECK(id, "append_time (time_t, xml) returns non-null",
            append_time(now, &dstr, true) != nullptr); id++;
   }

   printf("\n--- duration_add_offset ---\n");
   // T23: identity: 0 + 0 = 0
   CHECK(id, "duration_add_offset(0, 0) == 0",
         duration_add_offset(0, 0) == 0); id++;
   // T24: normal addition
   CHECK(id, "duration_add_offset(1000000, 2000000) == 3000000",
         duration_add_offset(UINT64_C(1000000), UINT64_C(2000000)) == UINT64_C(3000000)); id++;
   // T25: saturation — UINT64_MAX + 1 must not overflow
   CHECK(id, "duration_add_offset(UINT64_MAX, 1) saturates at UINT64_MAX",
         duration_add_offset(UINT64_MAX, 1) == UINT64_MAX); id++;
   // T26: near-overflow — (UINT64_MAX - 1) + 2 must also saturate
   CHECK(id, "duration_add_offset(UINT64_MAX - 1, 2) saturates at UINT64_MAX",
         duration_add_offset(UINT64_MAX - 1, 2) == UINT64_MAX); id++;

   printf("\n--- sge_usleep ---\n");
   // T27: sge_usleep with a small value must return without blocking
   sge_usleep(10000); // 10 ms
   CHECK(id, "sge_usleep(10000) returns", true); id++;

   printf("\n--- platform requirements ---\n");
   // T28: time_t must be at least 8 bytes to handle post-2038 timestamps
   CHECK(id, "sizeof(time_t) >= 8 (post-2038 safe)", sizeof(time_t) >= 8); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
