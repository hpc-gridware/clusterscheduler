/*___INFO__MARK_BEGIN_NEW__*/
/*************************************************************************
 *
 *  Copyright 2003 Sun Microsystems, Inc.
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
 ************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "qmaster_heartbeat.h"

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

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sge_qmaster_heartbeat");
   component_set_daemonized(true);

   // -only-write mode: preserved for external callers (e.g. testsuite)
   if (argc == 3 && strcmp(argv[1], "-only-write") == 0) {
      const char *only_write_file = argv[2];
      int beat_val = 0;
      printf("only writing heartbeat file once\n");
      inc_qmaster_heartbeat(only_write_file, 15, &beat_val);
      int i = get_qmaster_heartbeat(only_write_file, 15);
      printf("heartbeat value is %d\n", i);
      exit(0);
   }

   const char *filename = "/tmp/test_heartbeat_cs2089.txt";
   const time_t timeout = 15;
   int beat_val = 0;
   int ret = 0;

   unlink(filename);

   // --- basic inc/get ---
   printf("\n--- basic inc/get ---\n");
   // T01: inc on missing file creates it and returns 0
   ret = inc_qmaster_heartbeat(filename, timeout, &beat_val);
   CHECK(1, "inc on missing file returns 0", ret == 0);
   CHECK(2, "beat_val == 1 after first inc", beat_val == 1);
   CHECK(3, "get returns 1 after first inc", get_qmaster_heartbeat(filename, timeout) == 1);
   // second inc on existing file
   ret = inc_qmaster_heartbeat(filename, timeout, &beat_val);
   CHECK(4, "inc on existing file returns 0", ret == 0);
   CHECK(5, "beat_val == 2 after second inc", beat_val == 2);
   CHECK(6, "get returns 2 and matches beat_val", get_qmaster_heartbeat(filename, timeout) == beat_val);

   // --- sequential invariant ---
   printf("\n--- sequential invariant ---\n");
   {
      // 5 sequential increments: all succeed, consecutive values, get matches inc
      int prev_val = beat_val;
      bool all_inc_ok = true;
      bool all_consecutive = true;
      bool all_match = true;
      for (int iter = 0; iter < 5; iter++) {
         int bv = 0;
         int r = inc_qmaster_heartbeat(filename, timeout, &bv);
         int gv = get_qmaster_heartbeat(filename, timeout);
         if (r != 0) { all_inc_ok = false; }
         if (bv != prev_val + 1) { all_consecutive = false; }
         if (gv != bv) { all_match = false; }
         prev_val = bv;
      }
      CHECK(7, "5 sequential inc calls all return 0", all_inc_ok);
      CHECK(8, "each beat_val is one more than previous", all_consecutive);
      CHECK(9, "each get matches beat_val from same inc", all_match);
   }

   // --- wraparound ---
   printf("\n--- wraparound ---\n");
   {
      // write 99999 directly, then inc should wrap to 1
      FILE *fp = fopen(filename, "w");
      fprintf(fp, "%05d\n", 99999);
      fclose(fp);
      ret = inc_qmaster_heartbeat(filename, timeout, &beat_val);
      CHECK(10, "inc on value 99999 returns 0", ret == 0);
      CHECK(11, "beat_val wraps to 1 after 99999", beat_val == 1);
      CHECK(12, "get returns 1 after wraparound", get_qmaster_heartbeat(filename, timeout) == 1);
   }

   // --- corrupt file ---
   printf("\n--- corrupt file ---\n");
   {
      // inc on file with non-numeric content: silently resets hb to 1
      FILE *fp = fopen(filename, "w");
      fputs("not_a_number\n", fp);
      fclose(fp);
      ret = inc_qmaster_heartbeat(filename, timeout, &beat_val);
      CHECK(13, "inc on corrupt file returns 0", ret == 0);
      CHECK(14, "inc on corrupt file resets beat_val to 1", beat_val == 1);

      // get on file with non-numeric content returns -2
      fp = fopen(filename, "w");
      fputs("not_a_number\n", fp);
      fclose(fp);
      CHECK(15, "get on corrupt file returns -2", get_qmaster_heartbeat(filename, timeout) == -2);
   }

   // --- zero value ---
   printf("\n--- zero value ---\n");
   {
      // file containing 0: fscanf succeeds, hb increments to 1 (same outcome as corrupt)
      FILE *fp = fopen(filename, "w");
      fprintf(fp, "%05d\n", 0);
      fclose(fp);
      ret = inc_qmaster_heartbeat(filename, timeout, &beat_val);
      CHECK(16, "inc on value 0 returns 0", ret == 0);
      CHECK(17, "inc on value 0 produces beat_val 1", beat_val == 1);
   }

   // --- error cases ---
   printf("\n--- error cases ---\n");
   unlink(filename);
   CHECK(18, "get on nonexistent file returns -1", get_qmaster_heartbeat(filename, timeout) == -1);
   CHECK(19, "inc with nullptr filename returns -1", inc_qmaster_heartbeat(nullptr, timeout, &beat_val) == -1);
   CHECK(20, "get with nullptr filename returns -1", get_qmaster_heartbeat(nullptr, timeout) == -1);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   unlink(filename);
   DRETURN(s_fail == 0 ? 0 : 1);
}
