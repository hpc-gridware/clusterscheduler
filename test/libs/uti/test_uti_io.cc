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

/*
 * Regression test for CS-3135, sge_string2file():
 *   - the result of open() was tested with !fd, so a failed open() was not detected
 *   - the file was not truncated, a shorter string left the tail of the old content behind
 *   - the mode was 0666 minus the umask
 *   - a symbolic link at the path was followed
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "uti/sge_component.h"
#include "uti/sge_io.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_module_test.h"

/** @brief The directory the test files are written to, created by main(). */
static std::string test_dir;

/** @brief Read a whole file.
 * @param path the file
 * @param content out: the content of the file
 * @return true if the file could be read
 */
static bool
read_file(const std::string &path, std::string &content) {
   FILE *fp = fopen(path.c_str(), "r");
   if (fp == nullptr) {
      return false;
   }
   content.clear();
   char buf[256];
   size_t n;
   while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
      content.append(buf, n);
   }
   fclose(fp);
   return true;
}

/** @brief The permission bits of a file.
 * @param path the file
 * @return the permission bits, or -1 if the file does not exist
 */
static int
file_mode(const std::string &path) {
   struct stat st{};
   if (lstat(path.c_str(), &st) != 0) {
      return -1;
   }
   return static_cast<int>(st.st_mode & 07777);
}

/** @brief A shorter string replaces a longer one completely.
 * @param id the id of the first check
 * @return nothing; CHECK records the results
 */
static void
test_truncate(int id) {
   const std::string path = test_dir + "/truncate";
   const char *long_str = "first line of the old content\nsecond line of the old content\n";
   const char *short_str = "new\n";
   std::string content;

   bool ok = sge_string2file(long_str, 0, path.c_str(), 0600) == 0 &&
             sge_string2file(short_str, 0, path.c_str(), 0600) == 0;
   CHECK(id, "write a long and then a short string -> both return 0", ok);
   ok = read_file(path, content);
   std::string label = "file holds only the short string -> got " +
                       std::to_string(content.size()) + " bytes, expected " +
                       std::to_string(strlen(short_str));
   CHECK(id + 1, label.c_str(), ok && content == short_str);
}

/** @brief The mode is set as requested, independent of the umask and of an existing file.
 * @param id the id of the first check
 * @return nothing; CHECK records the results
 */
static void
test_mode(int id) {
   const std::string path_new = test_dir + "/mode_new";
   const std::string path_old = test_dir + "/mode_old";
   char label[256];

   mode_t old_umask = umask(0);
   int ret = sge_string2file("data\n", 0, path_new.c_str(), 0600);
   snprintf(label, sizeof(label), "new file with umask 0 -> mode %o, expected 600",
            file_mode(path_new));
   CHECK(id, label, ret == 0 && file_mode(path_new) == 0600);

   ret = sge_string2file("data\n", 0, path_old.c_str(), 0644);
   ret |= sge_string2file("data\n", 0, path_old.c_str(), 0600);
   umask(old_umask);
   snprintf(label, sizeof(label), "existing file with mode 644 -> mode %o, expected 600",
            file_mode(path_old));
   CHECK(id + 1, label, ret == 0 && file_mode(path_old) == 0600);
}

/** @brief A file that cannot be created is reported as an error.
 * @param id the id of the check
 * @return nothing; CHECK records the results
 */
static void
test_open_error(int id) {
   const std::string path = test_dir + "/no_such_dir/file";
   int ret = sge_string2file("data\n", 0, path.c_str(), 0600);
   std::string label = "write into a directory that does not exist -> returned " +
                       std::to_string(ret) + ", expected -1";
   CHECK(id, label.c_str(), ret == -1);
}

/** @brief A symbolic link at the path is not followed.
 * @param id the id of the first check
 * @return nothing; CHECK records the results
 */
static void
test_symlink(int id) {
   const std::string target = test_dir + "/link_target";
   const std::string link = test_dir + "/link";
   const char *target_str = "content of the link target\n";
   std::string content;

   bool ok = sge_string2file(target_str, 0, target.c_str(), 0600) == 0 &&
             symlink(target.c_str(), link.c_str()) == 0;
   int ret = sge_string2file("data\n", 0, link.c_str(), 0600);
   std::string label = "write to a symbolic link -> returned " + std::to_string(ret) +
                       ", expected -1";
   CHECK(id, label.c_str(), ok && ret == -1);
   ok = ok && read_file(target, content);
   CHECK(id + 1, "the target of the link is unchanged", ok && content == target_str);
}

/** @brief Remove the test directory and the files in it. */
static void
cleanup() {
   for (const char *name : {"truncate", "mode_new", "mode_old", "link_target", "link"}) {
      unlink((test_dir + "/" + name).c_str());
   }
   rmdir(test_dir.c_str());
}

/** @brief Module test entry point for CS-3135 (sge_string2file()).
 *
 * @param argc  argument count, see ocs_module_test.h for the frame's options
 * @param argv  argument vector
 * @return 0 on success, 1 if any check failed, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_io");

   // suppress the ERROR-level sge_log output of the error cases
   component_set_daemonized(true);

   char dir_template[] = "/tmp/test_uti_io.XXXXXX";
   if (mkdtemp(dir_template) == nullptr) {
      fprintf(stderr, "cannot create a temporary directory: %s\n", strerror(errno));
      DRETURN(1);
   }
   test_dir = dir_template;

   int ret = ocs::module_test::run(argc, argv, {
      {"IO-01", "sge_string2file() truncates an existing file (CS-3135)",
       "a shorter string written over a longer one leaves no tail of the old content behind",
       {"write a long string to a file", "write a short string to the same file",
        "read the file"},
       "the file holds only the short string",
       {}, {1, 2}, [] { test_truncate(1); }},
      {"IO-02", "sge_string2file() sets the requested mode (CS-3135)",
       "the mode of the file is the requested one, independent of the umask, also when the file "
       "already exists with another mode",
       {"write a new file with umask 0 and mode 0600", "write a file with mode 0644, then again "
        "with mode 0600"},
       "both files have mode 0600",
       {}, {3, 4}, [] { test_mode(3); }},
      {"IO-03", "sge_string2file() reports a failed open() (CS-3135)",
       "a file that cannot be created makes sge_string2file() return -1",
       {"write into a directory that does not exist"},
       "sge_string2file() returns -1",
       {}, {5}, [] { test_open_error(5); }},
      {"IO-04", "sge_string2file() does not follow a symbolic link (CS-3135)",
       "a symbolic link at the path is not followed, its target stays unchanged",
       {"create a file and a symbolic link to it", "write to the link"},
       "sge_string2file() returns -1 and the target is unchanged",
       {}, {6, 7}, [] { test_symlink(6); }},
   });

   cleanup();
   DRETURN(ret);
}
