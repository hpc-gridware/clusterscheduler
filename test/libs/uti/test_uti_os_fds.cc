/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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
#include <filesystem>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

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

// Open /dev/null then close all fds except {stdin, stdout, stderr}; verify only those three remain.
static void
test_close_stdin(int *id) {
   printf("\n--- close_all_fds keeps {stdin, stdout, stderr} ---\n");

   // open something that sge_close_all_fds must close
   int extra_fd = open("/dev/null", O_WRONLY);

   int keep_open[3]{STDOUT_FILENO, STDERR_FILENO, STDIN_FILENO};
   uint64_t start = sge_get_gmt64();
   sge_close_all_fds(keep_open, 3);
   uint64_t end = sge_get_gmt64();
   printf("      sge_close_all_fds took %" PRIu64 " µs\n", end - start);
   (void)extra_fd; // was closed by sge_close_all_fds

#if defined(LINUX) || defined(SOLARIS)
   std::set open_fds = get_all_fds();
   CHECK(*id, "exactly 3 fds open after close_all_fds", open_fds.size() == 3); (*id)++;
   CHECK(*id, "stdin still open", open_fds.find(STDIN_FILENO) != open_fds.end()); (*id)++;
   CHECK(*id, "stdout still open", open_fds.find(STDOUT_FILENO) != open_fds.end()); (*id)++;
   CHECK(*id, "stderr still open", open_fds.find(STDERR_FILENO) != open_fds.end()); (*id)++;
#endif
}

// Open num_fds files in test_dir; keep every (num_fds/10)th one plus stdin/stdout/stderr.
// Verify that the kept fds are still open after sge_close_all_fds.
static void
test_close_many_fds(int *id, const std::filesystem::path &test_dir, int num_fds) {
   printf("\n--- close_all_fds with %d open fds ---\n", num_fds);

   std::vector<int> keep_list{STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
   int modulo = num_fds / 10;

   for (int i = 0; i < num_fds; ++i) {
      std::filesystem::path file_path = test_dir / std::to_string(i);
      int fd = open(file_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      if (fd >= 0 && i % modulo == 0) {
         keep_list.push_back(fd);
      }
   }

   // sge_close_all_fds takes a raw array
   int keep_open[20];
   int keep_count = 0;
   for (auto fd : keep_list) {
      keep_open[keep_count++] = fd;
   }

   uint64_t start = sge_get_gmt64();
   sge_close_all_fds(keep_open, keep_count);
   uint64_t end = sge_get_gmt64();
   printf("      sge_close_all_fds took %" PRIu64 " µs\n", end - start);

#if defined(LINUX) || defined(SOLARIS)
   std::set still_open = get_all_fds();
   // T05: all fds in keep_list are still open
   bool all_kept = true;
   for (auto fd : keep_list) {
      if (still_open.find(fd) == still_open.end()) {
         all_kept = false;
      }
   }
   CHECK(*id, "all kept fds still open after close_all_fds", all_kept); (*id)++;
   // T06: no extra fds snuck in (exactly keep_count fds are open)
   CHECK(*id, "no extra fds open after close_all_fds", still_open.size() == static_cast<size_t>(keep_count)); (*id)++;

   // close the kept non-standard fds
   for (auto fd : still_open) {
      if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
         close(fd);
      }
   }
#else
   // on other platforms we can only verify the call did not crash
   CHECK(*id, "sge_close_all_fds completes without crash", true); (*id)++;
   for (int i = 0; i < keep_count; ++i) {
      int fd = keep_open[i];
      if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
         close(fd);
      }
   }
#endif
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_os_fds");
   int id = 1;

   printf("\n--- sge_get_max_fd ---\n");
   // T01: max fd is at least large enough to cover the standard file descriptors
   CHECK(id, "sge_get_max_fd() > STDERR_FILENO", sge_get_max_fd() > STDERR_FILENO); id++;

   // T02–T05: close_all_fds keeps only the specified fds
   test_close_stdin(&id);

   // create a temp directory for the many-fds test
   std::filesystem::path test_dir;
   if (argc > 1) {
      test_dir = std::filesystem::path(argv[1]);
   } else {
      test_dir = std::filesystem::temp_directory_path() / "test_uti_os_fds";
      std::filesystem::create_directories(test_dir);
   }

   // T05/T06 (Linux/Solaris) or T05 (other): close_all_fds with many open fds
   test_close_many_fds(&id, test_dir, 1000);

   if (argc <= 1) {
      std::filesystem::remove_all(test_dir);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
