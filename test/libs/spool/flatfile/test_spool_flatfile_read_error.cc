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
 * A spool file that fails to parse must not affect the next file read.
 *
 * The qmaster reads a classic spool directory file by file with
 * spool_flatfile_read_object(), HGRP_fields and qconf_sfi for host groups.
 * With one broken file in hostgroups/ the intact file read right after it
 * was reported as unreadable too, and a reserved or queue host group that is
 * missing after the read is re-created empty at startup and spooled.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "cull/cull_test_api.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_HR.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

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

/**
 * @brief Write a string into a new temporary file.
 *
 * @param content the complete file content
 * @param path    buffer of at least 64 bytes receiving the file name
 * @return true if the file was written
 */
static bool
write_tmp_file(const char *content, char *path) {
   strcpy(path, "/tmp/test_spool_ff_read_error_XXXXXX");
   int fd = mkstemp(path);
   if (fd < 0) {
      return false;
   }
   size_t len = strlen(content);
   bool ret = write(fd, content, len) == static_cast<ssize_t>(len);
   close(fd);
   return ret;
}

/**
 * @brief Read one host group file the way the qmaster reads its classic spool.
 *
 * Prints the messages the read added to the answer list, so a failing read
 * shows its reason.
 *
 * @param alp  the answer list; spool_read_list() passes one list for all files
 *             of a directory, nullptr uses a fresh list for this read only
 * @param path the file to read
 * @param what label for the printed messages
 * @return the host group element, nullptr if the file could not be read
 */
static lListElem *
read_hgroup(ocs::tom::ListPtr<ocs::tom::AN> *alp, const char *path, const char *what) {
   ocs::tom::ListPtr<ocs::tom::AN> own_alp;
   ocs::tom::ListPtr<ocs::tom::AN> *use_alp = alp != nullptr ? alp : &own_alp;
   int before = static_cast<int>((*use_alp)->size());

   lListElem *ep = spool_flatfile_read_object(use_alp, HGRP_Type, nullptr, HGRP_fields, nullptr,
                                              true, &qconf_sfi, SP_FORM_ASCII, nullptr, path);
   int i = 0;
   for (const ocs::tom::ConstElement<ocs::tom::AN> aep : use_alp->get()) {
      if (i++ >= before) {
         printf("      %s: %s\n", what, ocs::tom::get<AN_text>(aep));
      }
   }
   own_alp.reset();
   return ep;
}

/**
 * @brief Check the host list of a host group read from file.
 *
 * @param ep       the host group, may be nullptr
 * @param expected the expected single host list entry
 * @return true if ep carries exactly that one entry
 */
static bool
has_single_member(const lListElem *ep, const char *expected) {
   if (ep == nullptr) {
      return false;
   }
   ocs::tom::ConstList<ocs::tom::HR> hosts = ocs::tom::get<HGRP_host_list>(ep);
   return lGetNumberOfElem(hosts) == 1 &&
          strcmp(ocs::tom::get<HR_name>(hosts.front()), expected) == 0;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_flatfile_read_error");

   // see test_spool_flatfile2: resolve the bootstrap while errors still reach stderr
   ocs::Bootstrap::get_ignore_fqdn();

   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   int id = 1;
   char broken[64];
   char intact[64];
   char intact_none[64];

   // the content qmaster_spooling_issue2442 writes, and two host groups exactly as the qmaster spools them
   bool files_ok = write_tmp_file("wrong_argument\n", broken) &&
                   write_tmp_file("# Version: 9.2.0prealpha (170926-0748)\n# \n# DO NOT MODIFY THIS FILE MANUALLY!\n# \n"
                                  "group_name @@all.q\nhostlist @exec_hosts\n", intact) &&
                   write_tmp_file("# Version: 9.2.0prealpha (170926-0748)\n# \n# DO NOT MODIFY THIS FILE MANUALLY!\n# \n"
                                  "group_name @exec_hosts\nhostlist NONE\n", intact_none);
   CHECK(id, "temporary spool files written", files_ok); id++;
   if (!files_ok) {
      DRETURN(1);
   }

   lListElem *ep = read_hgroup(nullptr, intact, "intact, first read");
   CHECK(id, "intact host group file reads", has_single_member(ep, "@exec_hosts")); id++;
   lFreeElem(&ep);

   ep = read_hgroup(nullptr, intact_none, "intact NONE, first read");
   CHECK(id, "intact host group file with hostlist NONE reads", ep != nullptr); id++;
   lFreeElem(&ep);

   ep = read_hgroup(nullptr, broken, "broken");
   CHECK(id, "broken host group file is rejected", ep == nullptr); id++;
   lFreeElem(&ep);

   ep = read_hgroup(nullptr, intact, "intact, right after the broken one");
   CHECK(id, "intact host group file reads right after a broken one", has_single_member(ep, "@exec_hosts")); id++;
   lFreeElem(&ep);

   ep = read_hgroup(nullptr, broken, "broken, again");
   CHECK(id, "broken host group file is rejected again", ep == nullptr); id++;
   lFreeElem(&ep);

   ep = read_hgroup(nullptr, intact_none, "intact NONE, right after the broken one");
   CHECK(id, "intact host group file with hostlist NONE reads right after a broken one", ep != nullptr); id++;
   lFreeElem(&ep);

   ep = read_hgroup(nullptr, intact, "intact, second read after the broken one");
   CHECK(id, "intact host group file reads again after that", has_single_member(ep, "@exec_hosts")); id++;
   lFreeElem(&ep);

   // one answer list for the whole directory, as spool_read_list() does it
   {
      ocs::tom::ListPtr<ocs::tom::AN> alp;

      ep = read_hgroup(&alp, intact, "shared list, intact before the broken one");
      CHECK(id, "shared answer list: intact file before a broken one reads", has_single_member(ep, "@exec_hosts")); id++;
      lFreeElem(&ep);

      ep = read_hgroup(&alp, broken, "shared list, broken");
      CHECK(id, "shared answer list: broken file is rejected", ep == nullptr); id++;
      lFreeElem(&ep);

      ep = read_hgroup(&alp, intact, "shared list, intact after the broken one");
      CHECK(id, "shared answer list: intact file after a broken one reads", has_single_member(ep, "@exec_hosts")); id++;
      lFreeElem(&ep);

      ep = read_hgroup(&alp, intact_none, "shared list, second intact after the broken one");
      CHECK(id, "shared answer list: second intact file after a broken one reads", ep != nullptr); id++;
      lFreeElem(&ep);

      alp.reset();
   }

   sge_unlink(nullptr, broken);
   sge_unlink(nullptr, intact);
   sge_unlink(nullptr, intact_none);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
