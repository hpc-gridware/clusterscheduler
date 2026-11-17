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

/**
 * @file
 * @brief Module test for sge_get_pids() and sge_contains_pid() (CS-2531).
 *
 * sge_get_pids() used to fill a caller supplied array bounded by an element
 * count, and execd passed sizeof() of the array -- a byte size four times the
 * element count -- so a host with many shepherds could overflow the stack.
 * It now fills a std::vector without a bound. The test feeds it a ps output
 * with more entries than the old fixed array had.
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "uti/sge_component.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"

#include <sge_log.h>

/** number of matching processes the fake ps prints, more than the old 10000 */
static constexpr int NUM_PIDS = 12000;

/**
 * @brief Check one condition and report it.
 *
 * @param ok     result of the check
 * @param label  what was checked
 * @return @p ok
 */
static bool
check(bool ok, const char *label) {
   printf("%s  %s\n", ok ? "ok   " : "FAIL ", label);
   return ok;
}

/**
 * @brief Run sge_get_pids() on a fake ps output with more entries than the old fixed array.
 *
 * @param argc  unused
 * @param argv  unused
 * @return EXIT_SUCCESS if all checks passed, EXIT_FAILURE otherwise
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_os_pids");
   bool ret = true;

   component_set_daemonized(true);

   // a ps-like output: a header line, NUM_PIDS matching processes (some with a
   // path, which sge_get_pids() strips to the basename) and one other process
   char pscommand[512];
   snprintf(pscommand, sizeof(pscommand),
            "echo '  PID CMD'; "
            "awk 'BEGIN { for (i = 1; i <= %d; i++) { if (i == 5) print i, \"/opt/bin/sge_shepherd\"; else print i, \"sge_shepherd\" } }'; "
            "echo '%d sge_execd'",
            NUM_PIDS, NUM_PIDS + 1);

   std::vector<pid_t> pids;
   int n = sge_get_pids(pids, "sge_shepherd", pscommand);
   char label[128];
   snprintf(label, sizeof(label), "sge_get_pids() found %d processes, expected %d", n, NUM_PIDS);
   ret = check(n == NUM_PIDS && pids.size() == NUM_PIDS, label) && ret;
   ret = check(sge_contains_pid(5, pids), "a process started via a path is found by its basename") && ret;
   ret = check(sge_contains_pid(NUM_PIDS, pids), "the last process is found") && ret;
   ret = check(!sge_contains_pid(NUM_PIDS + 1, pids), "a process with another name is not found") && ret;

   // a second call starts from an empty list instead of appending
   n = sge_get_pids(pids, "sge_execd", pscommand);
   ret = check(n == 1 && pids.size() == 1 && pids[0] == NUM_PIDS + 1,
               "a second call replaces the result of the first one") && ret;

   n = sge_get_pids(pids, "nosuchprocess", pscommand);
   ret = check(n == 0 && pids.empty(), "no match yields 0 and an empty list") && ret;

   printf("\n%s\n", ret ? "PASS" : "FAIL");
   DRETURN(ret ? EXIT_SUCCESS : EXIT_FAILURE);
}
