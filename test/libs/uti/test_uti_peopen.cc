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
 * Regression test for CS-2333:
 *   sge_peopen()/sge_peopen_r() drop the UID of a target user but never call
 *   setgid(), so the spawned command keeps the parent's primary GID (gid 0
 *   when the parent is root). This is reachable in qmaster via the server JSV
 *   fork (sge_jsv.cc passes a non-NULL user to sge_peopen_r()).
 *
 * The privileged user-switch path only runs when the test process is root.
 * When not root, or when no suitable non-root target user exists, the test
 * reports a skip and passes (so it is harmless under CTest in an unprivileged
 * build). Run as root (e.g. from the installed testsuite) it spawns "id" as
 * the target user and verifies both the uid and the *primary* gid: before the
 * fix the gid check fails with gid 0, after the fix it passes.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#include "uti/sge_component.h"
#include "uti/sge_stdio.h"
#include "uti/sge_rmon_macros.h"

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

/** @brief Select a usable non-root target user to switch to.
 *
 * An explicit user (argv[1] or TEST_PEOPEN_USER) takes precedence; otherwise
 * the passwd database is scanned for the first entry with a non-root uid and
 * a non-root primary gid.
 *
 * @param explicit_user  requested user name, or nullptr/empty to auto-scan
 * @param name_buf       buffer receiving the resolved user name
 * @param name_len       size of name_buf in bytes
 * @param uid            out: resolved user's uid
 * @param gid            out: resolved user's primary gid
 * @return true if a suitable user was found, false otherwise
 */
static bool
find_target_user(const char *explicit_user, char *name_buf, size_t name_len,
                 uid_t *uid, gid_t *gid) {
   if (explicit_user != nullptr && explicit_user[0] != '\0') {
      struct passwd *pw = getpwnam(explicit_user);
      if (pw == nullptr) {
         printf("requested user '%s' not found\n", explicit_user);
         return false;
      }
      if (pw->pw_uid == 0 || pw->pw_gid == 0) {
         printf("requested user '%s' must have non-root uid and primary gid\n", explicit_user);
         return false;
      }
      snprintf(name_buf, name_len, "%s", pw->pw_name);
      *uid = pw->pw_uid;
      *gid = pw->pw_gid;
      return true;
   }

   setpwent();
   struct passwd *pw;
   bool found = false;
   while ((pw = getpwent()) != nullptr) {
      if (pw->pw_uid != 0 && pw->pw_gid != 0) {
         snprintf(name_buf, name_len, "%s", pw->pw_name);
         *uid = pw->pw_uid;
         *gid = pw->pw_gid;
         found = true;
         break;
      }
   }
   endpwent();
   return found;
}

/** @brief Run "id <id_arg>" as @p user through the given peopen variant.
 *
 * @param use_r   true to use sge_peopen_r(), false to use sge_peopen()
 * @param user    target user the command should run as
 * @param id_arg  argument passed to id(1), e.g. "-u" or "-g"
 * @param result  out: numeric value printed by the command
 * @return true on successful spawn and read, false otherwise
 */
static bool
run_id(bool use_r, const char *user, const char *id_arg, long *result) {
   char command[32];
   snprintf(command, sizeof(command), "id %s", id_arg);

   FILE *fp_in = nullptr;
   FILE *fp_out = nullptr;
   FILE *fp_err = nullptr;

   pid_t pid;
   if (use_r) {
      pid = sge_peopen_r("/bin/sh", 0, command, user, nullptr,
                         &fp_in, &fp_out, &fp_err, false);
   } else {
      pid = sge_peopen("/bin/sh", 0, command, user, nullptr,
                       &fp_in, &fp_out, &fp_err, false);
   }
   if (pid <= 0) {
      return false;
   }

   bool ok = false;
   char line[64];
   if (fp_out != nullptr && fgets(line, sizeof(line), fp_out) != nullptr) {
      *result = strtol(line, nullptr, 10);
      ok = true;
   }
   sge_peclose(pid, fp_in, fp_out, fp_err, nullptr);
   return ok;
}

/** @brief Verify one peopen variant drops to the target user's uid and gid.
 *
 * Spawns id(1) as @p user and checks the child runs with both the target uid
 * and the target primary gid. The gid check is the CS-2333 regression:
 * without setgid() the child keeps the parent's gid (0 when run as root).
 *
 * @param id        in/out: running check counter, incremented per assertion
 * @param use_r     true to exercise sge_peopen_r(), false for sge_peopen()
 * @param user      target user to switch to
 * @param want_uid  expected uid of the spawned child
 * @param want_gid  expected primary gid of the spawned child
 */
static void
check_variant(int *id, bool use_r, const char *user, uid_t want_uid, gid_t want_gid) {
   const char *tag = use_r ? "PEOPEN_R" : "PEOPEN";
   char label[128];

   long got_uid = -1;
   bool ran_uid = run_id(use_r, user, "-u", &got_uid);
   snprintf(label, sizeof(label), "%s: spawn 'id -u' -> ok", tag);
   CHECK(*id, label, ran_uid); (*id)++;
   snprintf(label, sizeof(label), "%s: child uid -> got %ld, expected %ld",
            tag, got_uid, (long) want_uid);
   CHECK(*id, label, ran_uid && got_uid == (long) want_uid); (*id)++;

   long got_gid = -1;
   bool ran_gid = run_id(use_r, user, "-g", &got_gid);
   snprintf(label, sizeof(label), "%s: spawn 'id -g' -> ok", tag);
   CHECK(*id, label, ran_gid); (*id)++;
   snprintf(label, sizeof(label),
            "%s: child primary gid -> got %ld, expected %ld (gid 0 == CS-2333)",
            tag, got_gid, (long) want_gid);
   CHECK(*id, label, ran_gid && got_gid == (long) want_gid); (*id)++;
}

/** @brief Module test entry point for CS-2333 (sge_peopen* GID drop).
 *
 * Requires root to exercise the user-switch path. Resolves the target user
 * from argv[1], TEST_PEOPEN_USER, or an auto-scan, then checks both peopen
 * variants. Skips (and passes) when not root or no suitable user exists.
 *
 * @param argc  argument count; argv[1] optionally names the target user
 * @param argv  argument vector
 * @return 0 on success or skip, 1 if any check failed
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_peopen");
   int id = 1;

   // suppress ERROR-level sge_log output from the spawn helpers
   component_set_daemonized(true);

   if (geteuid() != 0) {
      printf("SKIP: must run as root to exercise the user-switch path (CS-2333)\n");
      DRETURN(0);
   }

   // target user to switch to: argv[1], else TEST_PEOPEN_USER, else auto-scan
   const char *requested_user = (argc > 1) ? argv[1] : getenv("TEST_PEOPEN_USER");

   char user[256];
   uid_t want_uid = 0;
   gid_t want_gid = 0;
   if (!find_target_user(requested_user, user, sizeof(user), &want_uid, &want_gid)) {
      printf("SKIP: no non-root user with non-root primary group found "
             "(set TEST_PEOPEN_USER)\n");
      DRETURN(0);
   }

   printf("target user=%s uid=%ld gid=%ld\n", user, (long) want_uid, (long) want_gid);

   printf("\n--- PEOPEN_R: qmaster server-JSV fork path ---\n");
   check_variant(&id, true, user, want_uid, want_gid);

   printf("\n--- PEOPEN: generic user-switch path ---\n");
   check_variant(&id, false, user, want_uid, want_gid);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
