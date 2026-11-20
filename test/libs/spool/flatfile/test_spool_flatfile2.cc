/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Further tests of the flatfile spooling format, beyond test_spool_flatfile
 */

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <sys/wait.h>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_test_api.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "tom/ocs_Element.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
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

static int diff(const char *file1, const char *file2) {
   int ret = 1;
   char **argv = (char **)sge_malloc(sizeof(char *) * 4);
   const char *path = "/usr/bin/diff";

   if (argv == nullptr || file1 == nullptr || file2 == nullptr) {
      sge_free(&argv);
      return 1;
   }

   if (!fork()) {
      argv[0] = (char *)path;
      argv[1] = (char *)file1;
      argv[2] = (char *)file2;
      argv[3] = nullptr;
      execv(path, argv);
   } else {
      int stat_loc = 0;
      wait(&stat_loc);
      if (WIFEXITED(stat_loc)) {
         ret = WEXITSTATUS(stat_loc);
      }
   }

   sge_free(&argv);
   return ret;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_flatfile2");

   // Resolve the bootstrap configuration while log output still reaches stderr.
   // The CQ fixture below sets a host field, and hashing a host key goes through
   // sge_hostcpy(), which needs the ignore_fqdn bootstrap setting. That lookup
   // aborts the process when SGE_ROOT is unset, and once we are daemonized
   // sge_log() no longer writes to stderr - the test would then die without any
   // indication why. Touching it here keeps the critical error visible while the
   // checks themselves stay free of log noise.
   ocs::Bootstrap::get_ignore_fqdn();

   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   int id = 1;

   // build a minimal CQ with a terminate_method sublist entry
   auto created = cqueue_create(nullptr, "template");
   cqueue_set_template_attributes(*created, &alp);
   alp.reset();

   ocs::tom::set<CQ_name>(created, "foobar");

   auto lp = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Terminate Methods");
   auto ep = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep, "global");
   ocs::tom::set<ASTR_value>(ep, "/tmp/myterminate_method.sh");
   lAppendElem(*lp, std::move(ep));
   ocs::tom::set<CQ_terminate_method>(created, std::move(lp));

   auto cqueue_list = ocs::tom::ListPtr<ocs::tom::CQ>::create("CQ List");
   const auto cqueue = cqueue_list->push_back(std::move(created));

   // --- field alignment ---
   CHECK(id, "spool_flatfile_align_object with CQ_fields returns true",
         spool_flatfile_align_object(&alp, CQ_fields)); id++;
   CHECK(id, "aligned field width is positive",
         CQ_fields[0].width > 0); id++;
   alp.reset();

   // --- single-object round-trip ---
   {
      const char *file1 = nullptr;
      const char *file2 = nullptr;
      file1 = spool_flatfile_write_object(&alp, cqueue, false,
                                          CQ_fields, &qconf_sfi,
                                          SP_DEST_TMP, SP_FORM_ASCII, nullptr, false);
      lListElem *reread = spool_flatfile_read_object(&alp, CQ_Type, nullptr,
                                                     CQ_fields, nullptr, true, &qconf_sfi,
                                                     SP_FORM_ASCII, nullptr, file1);
      if (reread != nullptr) {
         file2 = spool_flatfile_write_object(&alp, reread, false,
                                             CQ_fields, &qconf_sfi,
                                             SP_DEST_TMP, SP_FORM_ASCII, nullptr, false);
         lFreeElem(&reread);
      }
      CHECK(id, "CQ single-object write-read-write round-trip produces identical output",
            diff(file1, file2) == 0); id++;
      sge_unlink(nullptr, file1);
      sge_unlink(nullptr, file2);
      sge_free(&file1);
      sge_free(&file2);
      alp.reset();
   }

   // --- list round-trip (CE objects, the canonical qconf_ce_list_sfi use case) ---
   {
      const char *file1 = nullptr;
      const char *file2 = nullptr;
      lList *read_list = nullptr;

      lList *ce_list = lCreateList("CE List", CE_Type);
      auto ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
      ocs::tom::set<CE_name>(ce, "test_ce");
      ocs::tom::set<CE_shortcut>(ce, "tc");
      ocs::tom::set<CE_valtype>(ce, 1);
      ocs::tom::set<CE_relop>(ce, CMPLXEQ_OP);
      ocs::tom::set<CE_requestable>(ce, REQU_YES);
      ocs::tom::set<CE_defaultval>(ce, "0");
      ocs::tom::set<CE_urgency_weight>(ce, "1");
      lAppendElem(ce_list, std::move(ce));
      spool_flatfile_align_list(&alp, (const lList *)ce_list, CE_fields, 0);
      alp.reset();

      file1 = spool_flatfile_write_list(&alp, ce_list, CE_fields,
                                        &qconf_ce_list_sfi,
                                        SP_DEST_TMP, SP_FORM_ASCII, nullptr, false);
      CHECK(id, "CE list write: spool_flatfile_write_list returns non-null", file1 != nullptr); id++;

      if (file1 != nullptr) {
         read_list = spool_flatfile_read_list(&alp, CE_Type, CE_fields, nullptr, true,
                                             &qconf_ce_list_sfi, SP_FORM_ASCII, nullptr, file1);
      }
      CHECK(id, "CE list read: spool_flatfile_read_list returns non-null", read_list != nullptr); id++;

      if (read_list != nullptr) {
         spool_flatfile_align_list(&alp, (const lList *)read_list, CE_fields, 0);
         alp.reset();
         file2 = spool_flatfile_write_list(&alp, read_list, CE_fields,
                                           &qconf_ce_list_sfi,
                                           SP_DEST_TMP, SP_FORM_ASCII, nullptr, false);
         lFreeList(&read_list);
      }
      CHECK(id, "CE list write-read-write round-trip produces identical output",
            diff(file1, file2) == 0); id++;
      sge_unlink(nullptr, file1);
      sge_unlink(nullptr, file2);
      sge_free(&file1);
      sge_free(&file2);
      alp.reset();
      lFreeList(&ce_list);
   }

   // --- error handling ---
   CHECK(id, "spool_flatfile_align_object rejects null fields: returns false",
         !spool_flatfile_align_object(&alp, nullptr)); id++;
   alp.reset();
   CHECK(id, "spool_flatfile_align_list rejects null list: returns false",
         !spool_flatfile_align_list(&alp, nullptr, CQ_fields, 0)); id++;
   alp.reset();
   CHECK(id, "spool_flatfile_align_list rejects null fields: returns false",
         !spool_flatfile_align_list(&alp, *cqueue_list, nullptr, 0)); id++;
   alp.reset();

   cqueue_list.reset();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
