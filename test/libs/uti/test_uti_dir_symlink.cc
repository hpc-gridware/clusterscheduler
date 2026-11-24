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
 * @brief Unit tests for the directory helpers of `libs/uti` that must not follow symbolic links
 *
 * CS-3202: sge_rmdir() removes a tree that may belong to another user (the job's TMPDIR,
 * removed by execd as root). A symbolic link inside the tree is removed itself, its target
 * stays intact, and a symbolic link at the top is refused. A swap of a directory for a link
 * during the removal cannot be provoked deterministically; the tests pin the invariant the
 * descriptor-based walk guarantees: nothing outside the tree is ever touched.
 *
 * Everything runs as the calling user inside a scratch directory under $TMPDIR or /tmp.
 */

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `ok`/`FAIL` with the test's id and label and counts the failure, so a run reports
 * every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/**
 * @brief Does a path exist, without following a symbolic link at its end?
 * @param path the path
 * @return true when lstat() finds it
 */
static bool exists(const std::string &path) {
   struct stat st{};
   return lstat(path.c_str(), &st) == 0;
}

/**
 * @brief Is the path a symbolic link?
 * @param path the path
 * @return true when lstat() reports a symbolic link
 */
static bool is_symlink(const std::string &path) {
   struct stat st{};
   return lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

/**
 * @brief Create a directory, report a failure
 * @param path the directory
 * @return true on success
 */
static bool make_dir(const std::string &path) {
   if (mkdir(path.c_str(), 0755) != 0) {
      printf("setup: mkdir(%s) failed: %s\n", path.c_str(), strerror(errno));
      return false;
   }
   return true;
}

/**
 * @brief Create a file with some content, report a failure
 * @param path the file
 * @return true on success
 */
static bool make_file(const std::string &path) {
   FILE *fp = fopen(path.c_str(), "w");
   if (fp == nullptr) {
      printf("setup: fopen(%s) failed: %s\n", path.c_str(), strerror(errno));
      return false;
   }
   fputs("keep me\n", fp);
   fclose(fp);
   return true;
}

/**
 * @brief Create a symbolic link, report a failure
 * @param target what the link points to
 * @param path the link
 * @return true on success
 */
static bool make_link(const std::string &target, const std::string &path) {
   if (symlink(target.c_str(), path.c_str()) != 0) {
      printf("setup: symlink(%s, %s) failed: %s\n", target.c_str(), path.c_str(), strerror(errno));
      return false;
   }
   return true;
}

/**
 * @brief Remove a scratch tree with the shell, independent of the code under test
 * @param path the tree
 */
static void cleanup(const std::string &path) {
   const std::string cmd = "rm -rf '" + path + "'";
   if (system(cmd.c_str()) != 0) {
      printf("cleanup: %s failed\n", cmd.c_str());
   }
}

/**
 * @brief Test sge_rmdir() on trees with symbolic links (CS-3202)
 * @param base scratch directory
 * @param id running test number
 */
static void test_rmdir(const std::string &base, int &id) {
   printf("\n--- RMDIR: symbolic links are removed, never followed (CS-3202) ---\n");

   // outside the tree: a directory with a file and a plain file, both must survive
   const std::string outside = base + "/outside";
   const std::string target_dir = outside + "/target_dir";
   const std::string target_dir_file = target_dir + "/precious";
   const std::string target_file = outside + "/target_file";
   bool setup = make_dir(outside) && make_dir(target_dir) && make_file(target_dir_file) && make_file(target_file);

   // the tree: nested directories, files and links pointing out of it, also from deep inside
   const std::string tree = base + "/tree";
   setup = setup && make_dir(tree) && make_dir(tree + "/a") && make_dir(tree + "/a/b") && make_dir(tree + "/a/b/c") &&
           make_file(tree + "/f") && make_file(tree + "/a/b/c/f") &&
           make_link(target_dir, tree + "/link_dir") && make_link(target_file, tree + "/link_file") &&
           make_link(target_dir, tree + "/a/b/link_dir_deep") && make_link("../../outside", tree + "/a/link_rel") &&
           make_link(base + "/does_not_exist", tree + "/a/dangling");
   CHECK(id++, "setup of the tree with links out of it", setup);

   DSTRING_STATIC(err, 1024);
   CHECK(id++, "recursive sge_rmdir() of the tree succeeds", sge_rmdir(tree.c_str(), &err) == 0);
   if (sge_dstring_strlen(&err) > 0) {
      printf("      error: %s\n", sge_dstring_get_string(&err));
   }
   CHECK(id++, "the tree is gone", !exists(tree));
   CHECK(id++, "the directory a link pointed to still exists", exists(target_dir));
   CHECK(id++, "the file inside that directory still exists", exists(target_dir_file));
   CHECK(id++, "the file a link pointed to still exists", exists(target_file));

   // a symbolic link at the top is refused and its target is not emptied
   const std::string top_link = base + "/top_link";
   CHECK(id++, "setup of a link to a directory as the top", make_link(target_dir, top_link));
   sge_dstring_clear(&err);
   CHECK(id++, "recursive sge_rmdir() of a link fails", sge_rmdir(top_link.c_str(), &err) != 0);
   CHECK(id++, "the failure has a message", sge_dstring_strlen(&err) > 0);
   CHECK(id++, "the top link is left alone", is_symlink(top_link));
   CHECK(id++, "the link's target is not emptied", exists(target_dir_file));
   unlink(top_link.c_str());

   // non-recursive removal and ignore_notempty keep their semantics
   const std::string full = base + "/full";
   CHECK(id++, "setup of a non-empty directory", make_dir(full) && make_file(full + "/f"));
   sge_dstring_clear(&err);
   CHECK(id++, "non-recursive sge_rmdir() of a non-empty directory fails",
         sge_rmdir(full.c_str(), &err, false, false) != 0 && exists(full + "/f"));
   sge_dstring_clear(&err);
   CHECK(id++, "non-recursive with ignore_notempty succeeds and removes nothing",
         sge_rmdir(full.c_str(), &err, false, true) == 0 && exists(full + "/f"));
   unlink((full + "/f").c_str());
   CHECK(id++, "non-recursive sge_rmdir() of an empty directory succeeds",
         sge_rmdir(full.c_str(), &err, false, false) == 0 && !exists(full));

   // a missing directory and a nullptr are errors
   sge_dstring_clear(&err);
   CHECK(id++, "a missing directory is an error", sge_rmdir((base + "/missing").c_str(), &err) != 0);
   CHECK(id++, "nullptr is an error", sge_rmdir(nullptr, &err) != 0);
   CHECK(id++, "nullptr error buffer is accepted", sge_rmdir((base + "/missing").c_str(), nullptr) != 0);

   // a directory that is a regular file at the top is an error, the file stays
   sge_dstring_clear(&err);
   CHECK(id++, "a regular file as the top is an error and stays",
         sge_rmdir(target_file.c_str(), &err) != 0 && exists(target_file));
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_dir_symlink");

   const char *tmp = getenv("TMPDIR");
   std::string templ = std::string(tmp != nullptr && *tmp != '\0' ? tmp : "/tmp") + "/test_uti_dir_symlink.XXXXXX";
   if (mkdtemp(templ.data()) == nullptr) {
      printf("FAIL  mkdtemp(%s) failed: %s\n", templ.c_str(), strerror(errno));
      DRETURN(1);
   }
   const std::string base = templ;
   int id = 1;

   test_rmdir(base, id);

   cleanup(base);
   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
