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

/** @file
 * @brief Module test of the debug tracing (rmon) with every layer switched on
 */

#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "ocs_module_test.h"

/**
 * @brief The traced part of the child: enter a traced main and call traced helpers of the BASIS layer
 *
 * Writing a trace message reads the thread name filter from the environment. That went through
 * sge_getenv(), which is traced itself, so the first trace message recursed until the stack overflowed.
 * The helpers called here are traced as well and write trace messages of their own.
 *
 * @return 0 when it got through
 */
static int
traced_child_main() {
   DENTER_MAIN(TOP_LAYER, "test_uti_rmon");

   const char *path = sge_getenv("PATH");
   dstring buffer = DSTRING_INIT;
   sge_dstring_sprintf(&buffer, "%s", path != nullptr ? path : "");
   sge_dstring_append(&buffer, ":");
   DSTRING_STATIC(time_buffer, 64);
   [[maybe_unused]] const char *now = sge_ctime64(sge_get_gmt64(), &time_buffer);
   DPRINTF("%s %s\n", now, sge_dstring_get_string(&buffer));
   sge_dstring_free(&buffer);

   DRETURN(0);
}

/**
 * @brief Run traced_child_main() in a child process with the given debug environment
 *
 * The trace output goes to /dev/null. A crash shows as a signal in the exit status.
 *
 * @param thread_name_pattern value for `SGE_DEBUG_THREAD_NAME_PATTERN`, or nullptr to leave it unset
 * @return true if the child exited with 0
 */
static bool
run_traced_child(const char *thread_name_pattern) {
   const pid_t pid = fork();
   if (pid < 0) {
      perror("fork");
      return false;
   }
   if (pid == 0) {
      const int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) {
         dup2(devnull, STDOUT_FILENO);
         dup2(devnull, STDERR_FILENO);
         close(devnull);
      }
      setenv("SGE_DEBUG_LEVEL", "3 3 3 3 3 3 3 3", 1);
      setenv("SGE_ND", "1", 1);
      if (thread_name_pattern != nullptr) {
         setenv("SGE_DEBUG_THREAD_NAME_PATTERN", thread_name_pattern, 1);
      } else {
         unsetenv("SGE_DEBUG_THREAD_NAME_PATTERN");
      }
      _exit(traced_child_main());
   }

   int status = 0;
   if (waitpid(pid, &status, 0) != pid) {
      perror("waitpid");
      return false;
   }
   if (WIFSIGNALED(status)) {
      printf("   child was killed by signal %d\n", WTERMSIG(status));
   }
   return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/**
 * @brief Trace every layer, without and with a thread name filter (CS-3159)
 */
static void
test_trace_all_layers() {
   CHECK(1, "a traced program with every layer switched on runs to its end", run_traced_child(nullptr));
   CHECK(2, "the same with a thread name filter set", run_traced_child("*"));
}

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"RMON-01", "debug tracing of every layer (CS-3159)",
       "a program traced with SGE_DEBUG_LEVEL \"3 3 3 3 3 3 3 3\" does not recurse in the trace output",
       {"run a traced main that calls traced BASIS layer helpers, in a child process",
        "do the same with SGE_DEBUG_THREAD_NAME_PATTERN set"},
       "both children exit with 0",
       {}, {1, 2}, test_trace_all_layers},
   });
}
