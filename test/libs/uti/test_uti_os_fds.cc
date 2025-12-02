/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <filesystem>
#include <iostream>
#include <vector>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "uti/sge_os.h"
#include "uti/sge_time.h"

#if defined(LINUX) || defined(SOLARIS)
static bool
close_stdin() {
   bool ret = true;

   // open some other file, to have something to close
   int some_fd = open("/dev/null", O_WRONLY);
   std::cout << "opened /dev/null: " << some_fd << std::endl;

   // Keep stdout and stderr open.
   // And we cannot close stdin (will fail with "Bad file descriptor") - so keep it open as well
   // but at least fill keep_open in non-lexical order.
   int keep_open[3]{STDOUT_FILENO, STDERR_FILENO, STDIN_FILENO};
   u_long64 start = sge_get_gmt64();
   sge_close_all_fds(keep_open, 3);
   u_long64 end = sge_get_gmt64();
   std::cout << "close_stdin() took " << (end - start) << " µs" << std::endl;
   std::set open_fds = get_all_fds();
   if (open_fds.size() != 3) {
      std::cerr << "close_stdin(): expected 3 fds to be open, got " << open_fds.size() << ": ";
      for (auto fd : open_fds) {
         std::cerr << fd << " ";
      }
      std::cerr << std::endl;
      ret = false;
   } else {
      if (open_fds.find(STDIN_FILENO) == open_fds.end()) {
         std::cerr << "close_stdin(): expected stdin to be still open" << std::endl;
         ret = false;
      }
      if (open_fds.find(STDOUT_FILENO) == open_fds.end()) {
         std::cerr << "close_stdin(): expected stdout to be still open" << std::endl;
         ret = false;
      }
      if (open_fds.find(STDERR_FILENO) == open_fds.end()) {
         std::cerr << "close_stdin(): expected stderr to be still open" << std::endl;
         ret = false;
      }
   }
   return ret;
}

static bool
close_many_fds(std::filesystem::path &test_dir, int num_fds) {
   bool ret = true;

   // Open a bunch of files in the test_dir.
   // We keep open some 10 file descriptors + the standard ones.
   // Store them in a vector.
   std::vector<int> open_files;
   open_files.push_back(STDIN_FILENO);
   open_files.push_back(STDOUT_FILENO);
   open_files.push_back(STDERR_FILENO);

   int modulo = num_fds / 10;
   for (int i = 0; i < num_fds; ++i) {
      std::filesystem::path file_path = test_dir / std::to_string(i);
      int fd = open(file_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      if (fd < 0) {
         std::cerr << "close_many_fds(): failed to open " << file_path << ": " << strerror(errno) << std::endl;
      } else {
         std::cout << "opened " << file_path << ": " << fd << std::endl;
         if (i % modulo == 0) {
            std::cout << "keeping open " << fd << std::endl;
            open_files.push_back(fd);
         }

      }
   }

   // keep_open array for close_all_fds() - leave enough space.
   int keep_open[20];
   int idx = 0;
   for (auto fd : open_files) {
      std::cout << "keeping open " << idx << ": " << fd << std::endl;
      keep_open[idx++] = fd;
   }

   u_long64 start = sge_get_gmt64();
   sge_close_all_fds(keep_open, idx);
   u_long64 end = sge_get_gmt64();
   std::cout << "close_many_fds() took " << (end - start) << " µs" << std::endl;

   // Check what is still open via get_all_fds().
   std::set still_open_fds = get_all_fds();

   // Check if the fds to keep open are actually still open.
   for (auto fd : open_files) {
      if (still_open_fds.find(fd) == still_open_fds.end()) {
         std::cerr << "close_many_fds(): expected fd " << fd << " to be still open" << std::endl;
         ret = false;
      }
   }

   // Close the kept open fds.
   for (auto fd : still_open_fds) {
      if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
         close(fd);
      }
   }

   return ret;
}
#endif

int main(int argc, char *argv[]) {
   int ret = EXIT_SUCCESS;

   std::cout << "maximum number of fds: " << sge_get_max_fd() << std::endl;

#if defined(LINUX) || defined(SOLARIS)
   if (argc > 1) {
      // If a path to a directory is given, then start a test opening many files
      // and closing them with close_all_fds().
      std::filesystem::path test_dir(argv[1]);
      if (ret == EXIT_SUCCESS && !close_many_fds(test_dir, 1000)) {
         ret = EXIT_FAILURE;
      }

      // @todo Should we ever implement just setting the FD_CLOEXEC flag on the fd instead of actually closing it,
      //       then extend the test to do a fork()/exec() and check the open file descriptors of the child process.
      //if (ret == EXIT_SUCCESS && !close_many_fds(test_dir, 100, true)) {
      //   ret = EXIT_FAILURE;
      //}
   } else {
      // Run a short test.
      if (ret == EXIT_SUCCESS && !close_stdin()) {
         ret = EXIT_FAILURE;
      }
   }
#else
   std::cout << "this test is not implemented on platforms other than LINUX and SOLARIS" << std::endl;
#endif

   return ret;
}
