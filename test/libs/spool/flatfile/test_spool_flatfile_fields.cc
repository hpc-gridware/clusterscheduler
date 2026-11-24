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
 * @brief Unit tests for single fields of the flatfile reader and writer in `libs/spool/flatfile`
 *
 * - A field the writer cannot render is left out instead of crashing the writer (CS-3167).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <unistd.h>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "tom/ocs_Element.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

static int failures = 0;

/**
 * @brief Record the outcome of one check
 * @param id the id of the check, printed as `[Tnn]`
 * @param what what the check is about
 * @param ok whether the check held
 */
static void
check(const char *id, const char *what, const bool ok) {
   if (ok) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s\n", id, what);
      failures++;
   }
   fflush(stdout);
}

/**
 * @brief Write a string into a new temporary file
 * @param content the complete file content
 * @param[out] path receives the file name, at least 64 bytes
 * @return true if the file was written
 */
static bool
write_tmp_file(const char *content, char *path) {
   strcpy(path, "/tmp/test_spool_ff_fields_XXXXXX");
   const int fd = mkstemp(path);
   if (fd < 0) {
      return false;
   }
   const size_t len = strlen(content);
   const bool ret = write(fd, content, len) == static_cast<ssize_t>(len);
   close(fd);
   return ret;
}

/**
 * @brief Print the messages of an answer list, so a failing check shows its reason
 * @param alp the answer list
 */
static void
print_answers(const ocs::tom::ListPtr<ocs::tom::AN> &alp) {
   if (alp) {
      for (const auto aep : *alp) {
         const char *text = aep.get<AN_text>();
         if (text != nullptr) {
            const size_t len = strlen(text);
            printf("      %s%s", text, len > 0 && text[len - 1] == '\n' ? "" : "\n");
         }
      }
   }
}

/**
 * @brief A field the writer cannot render is left out (CS-3167)
 *
 * A field list with the object field JG_queue: there is no text for an object field, the writer
 * has to skip it and write the other fields.
 */
static void
test_write_unrenderable_field() {
   const auto *type_ops = ocs_spool_type_ops_of(ocs::tom::JG::descriptor.e);
   check("T11", "the spooling records of JG exist", type_ops != nullptr);
   if (type_ops == nullptr) {
      return;
   }

   spooling_field fields[3] = {};
   const int nms[2] = {JG_qname, JG_queue};
   for (int i = 0; i < 2; i++) {
      fields[i].nm = nms[i];
      fields[i].name = ocs::tom::field_name(nms[i]);
      fields[i].ops = &type_ops->ops[nms[i] - type_ops->base];
   }
   fields[2].nm = NoName;

   auto gdil = ocs::tom::ElementPtr<ocs::tom::JG>::create();
   gdil->set<JG_qname>("all.q@host1");

   std::ostringstream os;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *name = spool_flatfile_write_object(&alp, gdil.get(), true, fields, &qconf_sfi, SP_DEST_STREAM,
                                                  SP_FORM_ASCII, nullptr, false, nullptr, &os, SP_LINE_BREAKS_OFF);
   print_answers(alp);
   const std::string text = os.str();
   check("T12", "an object field is left out, the writer does not crash", name != nullptr);
   check("T13", "the other fields are written", text.find("all.q@host1") != std::string::npos);
   check("T14", "nothing is written for the object field", text.find("JG_queue") == std::string::npos);
   check("T15", "leaving out a field without a text form is no error", !answer_list_has_error(&alp));
   sge_free(&name);
}

/**
 * @brief Run the tests
 * @return 0 if all checks held, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_flatfile_fields");

   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_write_unrenderable_field();

   printf("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
   DRETURN(failures == 0 ? 0 : 1);
}
