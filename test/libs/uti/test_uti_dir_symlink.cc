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
 * CS-3203: ocs_mkdir_for_owner() creates the job's TMPDIR at a predictable path and hands it to
 * the job owner. A symbolic link or file a user planted at that path is removed itself, a directory
 * of someone else is refused, and nothing outside the directory it creates is ever changed.
 *
 * CS-3221: ocs_moved_aside_paths() finds exactly the directories ocs_mkdir_for_owner() renamed aside
 * from a path, never a symbolic link or file of such a name, and sge_rmdir() removes them without
 * touching the target of a link inside. A directory of such a name that belongs to the job owner is
 * never listed, it may be the live TMPDIR of a queue whose name ends like that.
 *
 * Everything runs as the calling user inside a scratch directory under $TMPDIR or /tmp.
 */

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/resource.h>
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

/**
 * @brief Owner, group and mode of a path, without following a symbolic link at its end
 * @param path the path
 * @param[out] st receives the result
 * @return true when lstat() succeeds
 */
static bool get_stat(const std::string &path, struct stat &st) {
   return lstat(path.c_str(), &st) == 0;
}

/**
 * @brief Build a chain of nested directories `dd` relative to descriptors, so PATH_MAX is no limit
 * @param top the directory to build in, it must exist
 * @param depth number of nested levels below @p top
 * @return true on success
 */
static bool make_deep_tree(const std::string &top, const int depth) {
   int fd = open(top.c_str(), O_RDONLY | O_DIRECTORY);
   if (fd < 0) {
      printf("setup: open(%s) failed: %s\n", top.c_str(), strerror(errno));
      return false;
   }
   for (int i = 0; i < depth; i++) {
      if (mkdirat(fd, "dd", 0755) != 0) {
         printf("setup: mkdirat() at level %d failed: %s\n", i, strerror(errno));
         close(fd);
         return false;
      }
      const int sub = openat(fd, "dd", O_RDONLY | O_DIRECTORY);
      close(fd);
      if (sub < 0) {
         printf("setup: openat() at level %d failed: %s\n", i, strerror(errno));
         return false;
      }
      fd = sub;
   }
   // a file at the bottom
   const int file = openat(fd, "f", O_WRONLY | O_CREAT, 0644);
   if (file >= 0) {
      close(file);
   }
   close(fd);
   return file >= 0;
}

/**
 * @brief Test sge_rmdir() on deep trees: no recursion on the stack, clean failure at the limits
 * @param base scratch directory
 * @param id running test number
 */
static void test_rmdir_deep(const std::string &base, int &id) {
   printf("\n--- RMDIR: deep trees do not depend on the stack (CS-3202) ---\n");

   // as many descriptors as allowed, so the depth is the only limit
   struct rlimit saved{};
   getrlimit(RLIMIT_NOFILE, &saved);
   struct rlimit raised = saved;
   raised.rlim_cur = saved.rlim_max;
   setrlimit(RLIMIT_NOFILE, &raised);
   struct rlimit now{};
   getrlimit(RLIMIT_NOFILE, &now);
   printf("      descriptor limit %llu\n", (unsigned long long) now.rlim_cur);

   DSTRING_STATIC(err, 1024);

   // 2000 levels of "dd/", 6000 characters: longer than PATH_MAX; removed when the descriptors suffice,
   // else a clean error
   const std::string deep = base + "/deep";
   CHECK(id++, "setup of a tree 2000 levels deep", make_dir(deep) && make_deep_tree(deep, 2000));
   sge_dstring_clear(&err);
   const int ret = sge_rmdir(deep.c_str(), &err);
   if (now.rlim_cur >= 2100) {
      CHECK(id++, "a tree 2000 levels deep is removed", ret == 0 && !exists(deep));
   } else {
      CHECK(id++, "a tree 2000 levels deep fails cleanly with too few descriptors",
            ret != 0 && sge_dstring_strlen(&err) > 0 && exists(deep));
   }
   if (sge_dstring_strlen(&err) > 0) {
      printf("      error: %s\n", sge_dstring_get_string(&err));
   }
   cleanup(deep);

   // deeper than the limit of the walk: a clean error, the deepest part stays
   const std::string too_deep = base + "/too_deep";
   CHECK(id++, "setup of a tree 5000 levels deep", make_dir(too_deep) && make_deep_tree(too_deep, 5000));
   sge_dstring_clear(&err);
   CHECK(id++, "a tree 5000 levels deep fails cleanly",
         sge_rmdir(too_deep.c_str(), &err) != 0 && sge_dstring_strlen(&err) > 0 && exists(too_deep + "/dd"));
   const std::string too_deep_error = sge_dstring_get_string(&err);
   printf("      error: %.200s\n", too_deep_error.c_str());
   if (now.rlim_cur > 4200) {
      CHECK(id++, "... and it is the depth limit of the walk, not the descriptor limit",
            too_deep_error.find("deeper than 4096 levels") != std::string::npos);
   } else {
      printf("SKIP  [T%02d] ... the depth limit: the descriptor limit %llu is reached first\n", id++,
             (unsigned long long) now.rlim_cur);
   }
   cleanup(too_deep);

   // running out of descriptors: a clean error, a second call with enough descriptors finishes
   const std::string fds = base + "/fds";
   CHECK(id++, "setup of a tree 200 levels deep", make_dir(fds) && make_deep_tree(fds, 200));
   struct rlimit low = now;
   low.rlim_cur = 64;
   setrlimit(RLIMIT_NOFILE, &low);
   sge_dstring_clear(&err);
   const int low_ret = sge_rmdir(fds.c_str(), &err);
   setrlimit(RLIMIT_NOFILE, &now);
   CHECK(id++, "with 64 descriptors the removal fails cleanly and leaves the tree",
         low_ret != 0 && sge_dstring_strlen(&err) > 0 && exists(fds + "/dd"));
   printf("      error: %.200s\n", sge_dstring_get_string(&err));
   sge_dstring_clear(&err);
   CHECK(id++, "with enough descriptors a second removal finishes",
         sge_rmdir(fds.c_str(), &err) == 0 && !exists(fds));
   cleanup(fds);

   setrlimit(RLIMIT_NOFILE, &saved);
}

/**
 * @brief Test ocs_mkdir_for_owner() against whatever a user planted at the path (CS-3203)
 *
 * The test runs unprivileged, so the new owner is the calling user and the group is one of its
 * other groups when it has one: a chown through a planted link would then show as a changed group
 * of the link's target. Without a second group that part cannot be observed and is reported as
 * SKIP. The invariant is that nothing but the directory at the path is ever changed: a planted
 * non-directory is removed itself, never followed, and a directory of another user is left alone.
 *
 * @param base scratch directory
 * @param id running test number
 */
static void test_mkdir_for_owner(const std::string &base, int &id) {
   printf("\n--- MKDIR-FOR-OWNER: a planted path is removed or refused, never followed (CS-3203) ---\n");

   const uid_t uid = getuid();
   gid_t gid = getgid();
   gid_t groups[256];
   const int ngroups = getgroups(256, groups);
   for (int i = 0; i < ngroups; i++) {
      if (groups[i] != gid) {
         gid = groups[i];
         break;
      }
   }
   const bool group_observable = gid != getgid();
   printf("      new owner uid %d, gid %d (primary gid %d)\n", (int) uid, (int) gid, (int) getgid());

   const std::string parent = base + "/parent";
   const std::string victim_dir = base + "/victim_dir";
   const std::string victim_file = base + "/victim_file";
   CHECK(id++, "setup of the parent and the link targets",
         make_dir(parent) && make_dir(victim_dir) && make_file(victim_file));
   chmod(victim_dir.c_str(), 0700);
   chmod(victim_file.c_str(), 0600);
   struct stat before_dir{};
   struct stat before_file{};
   get_stat(victim_dir, before_dir);
   get_stat(victim_file, before_file);

   /**
    * @brief Check that a link target kept owner, group and mode
    * @param label what is checked
    * @param path the target
    * @param before its state before
    */
   auto check_untouched = [&id, group_observable](const char *label, const std::string &path,
                                                 const struct stat &before) {
      struct stat now{};
      CHECK(id++, label, get_stat(path, now) && now.st_uid == before.st_uid && now.st_mode == before.st_mode);
      if (group_observable) {
         CHECK(id++, "... and its group", now.st_gid == before.st_gid);
      } else {
         printf("SKIP  [T%02d] ... and its group: the test user has no second group, a chown is not observable\n",
                id++);
      }
   };

   DSTRING_STATIC(err, 1024);
   struct stat st{};

   // the normal case: a new directory, owned by uid and gid, with the mode exactly as asked for
   const std::string fresh = parent + "/1.1.all.q";
   const mode_t old_umask = umask(077);
   CHECK(id++, "a new directory is created and handed over with the mode asked for",
         ocs_mkdir_for_owner(fresh.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK && get_stat(fresh, st) &&
         S_ISDIR(st.st_mode) && st.st_uid == uid && st.st_gid == gid && (st.st_mode & 07777) == 0755);
   umask(old_umask);

   // a directory left behind by an earlier attempt is used again and gets the mode back
   chmod(fresh.c_str(), 0700);
   sge_dstring_clear(&err);
   CHECK(id++, "an existing directory of the caller or the new owner is used again with the mode asked for",
         ocs_mkdir_for_owner(fresh.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK && get_stat(fresh, st) &&
         S_ISDIR(st.st_mode) && (st.st_mode & 07777) == 0755);

   // a planted link to a directory: removed, the directory created, the target untouched
   const std::string link_dir = parent + "/2.1.all.q";
   CHECK(id++, "setup of a planted link to a directory", make_link(victim_dir, link_dir));
   sge_dstring_clear(&err);
   CHECK(id++, "a planted link to a directory is replaced by the directory",
         ocs_mkdir_for_owner(link_dir.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK &&
         !is_symlink(link_dir) && get_stat(link_dir, st) && S_ISDIR(st.st_mode) && st.st_uid == uid);
   check_untouched("the link's target keeps owner and mode", victim_dir, before_dir);

   // a planted link to a file
   const std::string link_file = parent + "/3.1.all.q";
   CHECK(id++, "setup of a planted link to a file", make_link(victim_file, link_file));
   sge_dstring_clear(&err);
   CHECK(id++, "a planted link to a file is replaced by the directory",
         ocs_mkdir_for_owner(link_file.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK &&
         get_stat(link_file, st) && S_ISDIR(st.st_mode));
   check_untouched("the file keeps owner and mode", victim_file, before_file);
   CHECK(id++, "the file still exists", exists(victim_file));

   // a dangling link: mkdir() must not create its target
   const std::string dangling_target = base + "/created_through_link";
   const std::string dangling = parent + "/4.1.all.q";
   CHECK(id++, "setup of a planted dangling link", make_link(dangling_target, dangling));
   sge_dstring_clear(&err);
   CHECK(id++, "a planted dangling link is replaced and nothing is created at its target",
         ocs_mkdir_for_owner(dangling.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK &&
         get_stat(dangling, st) && S_ISDIR(st.st_mode) && !exists(dangling_target));

   // a planted regular file
   const std::string file = parent + "/5.1.all.q";
   CHECK(id++, "setup of a planted regular file", make_file(file));
   sge_dstring_clear(&err);
   CHECK(id++, "a planted regular file is replaced by the directory",
         ocs_mkdir_for_owner(file.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK && get_stat(file, st) &&
         S_ISDIR(st.st_mode));

   // a planted fifo: open() must not block on it, it is replaced
   const std::string fifo = parent + "/6.1.all.q";
   CHECK(id++, "setup of a planted fifo", mkfifo(fifo.c_str(), 0644) == 0);
   sge_dstring_clear(&err);
   CHECK(id++, "a planted fifo is replaced by the directory",
         ocs_mkdir_for_owner(fifo.c_str(), 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_OK && get_stat(fifo, st) &&
         S_ISDIR(st.st_mode));

   // planted again before every attempt: the job fails, the host is fine, nothing is followed
   const std::string again = parent + "/7.1.all.q";
   int attempts = 0;
   sge_dstring_clear(&err);
   const int again_ret = ocs_mkdir_for_owner(again.c_str(), 0755, uid, gid, &err, [&](const int) {
      attempts++;
      make_link(victim_file, again);
   });
   CHECK(id++, "a path planted again before every attempt gives up as planted after a few attempts",
         again_ret == OCS_MKDIR_FOR_OWNER_PLANTED && attempts > 1 && attempts < 10);
   printf("      %d attempts, error: %s\n", attempts, sge_dstring_get_string(&err));
   check_untouched("the target of the links keeps owner and mode", victim_file, before_file);
   unlink(again.c_str());

   // a planted entry that cannot be removed: a job failure, the entry stays
   const std::string locked_parent = base + "/locked";
   const std::string locked = locked_parent + "/8.1.all.q";
   CHECK(id++, "setup of a planted file in a parent the caller cannot write",
         make_dir(locked_parent) && make_file(locked) && chmod(locked_parent.c_str(), 0555) == 0);
   sge_dstring_clear(&err);
   const int locked_ret = ocs_mkdir_for_owner(locked.c_str(), 0755, uid, gid, &err);
   if (uid != 0) {
      CHECK(id++, "a planted entry that cannot be removed is no success and stays",
            locked_ret != OCS_MKDIR_FOR_OWNER_OK && exists(locked) && !is_symlink(locked) && get_stat(locked, st) &&
            S_ISREG(st.st_mode));
      printf("      result %d, error: %s\n", locked_ret, sge_dstring_get_string(&err));
   } else {
      printf("SKIP  [T%02d] a planted entry that cannot be removed: the test runs as root\n", id++);
   }
   chmod(locked_parent.c_str(), 0755);

   // a directory of another user that cannot be moved aside (root's /) stands in for a planted foreign directory
   // that stays; moving one aside needs a second user and is covered by the testsuite check of CS-3203
   if (uid != 0) {
      struct stat root_before{};
      get_stat("/", root_before);
      sge_dstring_clear(&err);
      CHECK(id++, "a directory of another user that cannot be moved aside gives planted, not an error of the host",
            ocs_mkdir_for_owner("/", 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_PLANTED);
      printf("      error: %s\n", sge_dstring_get_string(&err));
      CHECK(id++, "that directory keeps owner, group and mode",
            get_stat("/", st) && st.st_uid == root_before.st_uid && st.st_gid == root_before.st_gid &&
            st.st_mode == root_before.st_mode);
   } else {
      printf("SKIP  [T%02d] a directory of another user: the test runs as root\n", id++);
   }

   // a failing fchown() is an error of the host, not a planted path: the directory was verified, only the
   // environment can make it fail (root_squash, a filesystem without ownership); unprivileged, handing a
   // directory to another user fails the same way
   if (uid != 0) {
      const std::string foreign = parent + "/9.1.all.q";
      sge_dstring_clear(&err);
      CHECK(id++, "a failing fchown() is an error of the host, the new directory is removed again",
            ocs_mkdir_for_owner(foreign.c_str(), 0755, uid + 1, gid, &err) == OCS_MKDIR_FOR_OWNER_ERROR &&
            !exists(foreign));
      printf("      error: %s\n", sge_dstring_get_string(&err));
   } else {
      printf("SKIP  [T%02d] a failing fchown(): the test runs as root\n", id++);
   }

   // a missing parent is an error of the host, and so is a nullptr
   sge_dstring_clear(&err);
   CHECK(id++, "a missing parent is an error, not a planted path",
         ocs_mkdir_for_owner((base + "/missing/1.1.all.q").c_str(), 0755, uid, gid, &err) ==
         OCS_MKDIR_FOR_OWNER_ERROR);
   CHECK(id++, "nullptr is an error", ocs_mkdir_for_owner(nullptr, 0755, uid, gid, &err) == OCS_MKDIR_FOR_OWNER_ERROR);
}

/**
 * @brief Test ocs_moved_aside_paths() and the removal of what it finds (CS-3221)
 *
 * Next to the path of a TMPDIR lie directories named like those ocs_mkdir_for_owner() moves aside,
 * one with a symbolic link to a file outside, and decoys: a symbolic link and a file that bear such
 * a name, and directories whose names only resemble it. Only the two directories are listed;
 * removing them with sge_rmdir() leaves the link's target and every decoy in place.
 *
 * A directory is listed only when it does not belong to the given owner. As a non-root user the
 * test can only create directories of its own uid, so every directory here belongs to the test:
 * passing a different uid as owner stands for the job owner whose TMPDIR the others were moved
 * aside from, passing the test's own uid stands for a match that belongs to the job owner, e.g.
 * the live TMPDIR of the same job in a queue named `all.q.stale.5.6`. That one is neither listed
 * nor removed. A parent holding directories of both kinds needs root and is not covered.
 *
 * @param base scratch directory
 * @param id running test number, incremented per check
 */
static void test_moved_aside(const std::string &base, int &id) {
   printf("\n--- ocs_moved_aside_paths() (CS-3221)\n");
   DSTRING_STATIC(err, 1024);

   const std::string parent = base + "/aside";
   const std::string path = parent + "/7.1.all.q";
   const std::string first = path + ".stale.123.1700000000";
   const std::string second = path + ".stale.4.5";
   const std::string target = base + "/aside_target";
   const std::string link_named = path + ".stale.6.7";
   const std::string file_named = path + ".stale.8.9";
   const std::vector<std::string> decoys = {path + ".stale.1", path + ".stale.1.2x", path + ".stale..2",
                                            path + ".stale.1.2.3", parent + "/17.1.all.q.stale.1.2",
                                            path + "x.stale.1.2"};
   bool ok = make_dir(parent) && make_dir(path) && make_dir(first) && make_file(first + "/keep") &&
             make_dir(second) && make_file(target) && symlink(target.c_str(), (second + "/link").c_str()) == 0 &&
             symlink(target.c_str(), link_named.c_str()) == 0 && make_file(file_named);
   for (const auto &decoy : decoys) {
      ok = ok && make_dir(decoy);
   }
   if (!ok) {
      CHECK(id++, "setup of the moved aside directories", false);
      cleanup(parent);
      return;
   }

   // the test's own uid stands for the job owner, any other uid for the owner of a foreign directory
   const uid_t self = getuid();
   const uid_t other = self + 1;

   std::vector<std::string> paths;
   CHECK(id++, "a directory that belongs to the owner is not listed",
         ocs_moved_aside_paths(path.c_str(), self, paths, &err) == 0 && paths.empty());
   CHECK(id++, "the directories of the owner are still there", exists(first) && exists(second));

   CHECK(id++, "the parent is read", ocs_moved_aside_paths(path.c_str(), other, paths, &err) == 0);
   std::sort(paths.begin(), paths.end());
   std::vector<std::string> expected = {first, second};
   std::sort(expected.begin(), expected.end());
   CHECK(id++, "exactly the two directories are listed, no link, no file, no look-alike", paths == expected);
   for (const auto &listed : paths) {
      printf("      listed: %s\n", listed.c_str());
   }

   bool removed = true;
   for (const auto &listed : paths) {
      sge_dstring_clear(&err);
      removed = removed && sge_rmdir(listed.c_str(), &err) == 0;
   }
   CHECK(id++, "the listed directories are removed", removed && !exists(first) && !exists(second));
   CHECK(id++, "the target of the link inside is untouched", exists(target) && !is_symlink(target));
   bool kept = exists(path) && is_symlink(link_named) && exists(file_named);
   for (const auto &decoy : decoys) {
      kept = kept && exists(decoy);
   }
   CHECK(id++, "the directory itself and every decoy are still there", kept);
   CHECK(id++, "a second search finds nothing",
         ocs_moved_aside_paths(path.c_str(), other, paths, &err) == 0 && paths.empty());

   CHECK(id++, "a missing parent is no error and lists nothing",
         ocs_moved_aside_paths((base + "/missing/7.1.all.q").c_str(), other, paths, &err) == 0 && paths.empty());
   CHECK(id++, "nullptr is an error", ocs_moved_aside_paths(nullptr, other, paths, &err) == -1);

   cleanup(parent);
   cleanup(target);
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
   test_rmdir_deep(base, id);
   test_mkdir_for_owner(base, id);
   test_moved_aside(base, id);

   cleanup(base);
   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
