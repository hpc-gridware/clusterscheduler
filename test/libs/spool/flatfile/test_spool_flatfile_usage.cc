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
 * @brief Unit tests for flatfile usage in `libs/spool`
 */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_usage.h"

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

/**
 * Read a whole file into a string and undo the line continuations the flatfile
 * writer inserts, so that one logical attribute line can be matched as a single
 * contiguous piece of text.
 */
static std::string read_unwrapped(const char *filepath) {
   std::string content;

   FILE *fp = fopen(filepath, "r");
   if (fp == nullptr) {
      return content;
   }
   char buffer[4096];
   size_t n;
   while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
      content.append(buffer, n);
   }
   fclose(fp);

   std::string::size_type pos;
   while ((pos = content.find("\\\n")) != std::string::npos) {
      std::string::size_type end = pos + 2;
      while (end < content.size() && (content[end] == ' ' || content[end] == '\t')) {
         end++;
      }
      content.erase(pos, end - pos);
   }

   return content;
}

/**
 * Return the unwrapped line introduced by the given attribute name, or an empty
 * string if the attribute does not appear in the content.
 */
static std::string get_attribute_line(const std::string &content, const char *name) {
   std::string::size_type pos = content.find(name);
   if (pos == std::string::npos) {
      return "";
   }
   std::string::size_type end = content.find('\n', pos);
   if (end == std::string::npos) {
      end = content.size();
   }
   return content.substr(pos, end - pos);
}

/*
 * Regression test for the flatfile representation of UA_Type sublists.
 *
 * UA_svalue (string usage values, CS-849) must not be part of UA_sub_fields.
 * Every list spooled through that table - the scheduler config weight list and
 * the sharetree usage lists - is numeric by construction, so an svalue field
 * would only ever be written as the placeholder "NONE" and would then be read
 * back as a literal string value. This is the qconf -ssconf output path.
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_flatfile_usage");
   lInit(nmv);

   lList *alp = nullptr;
   int id = 1;

   lListElem *sconf = sconf_create_default();
   CHECK(id, "sconf_create_default returns a scheduler config", sconf != nullptr); id++;

   const char *file1 = spool_flatfile_write_object(&alp, sconf, false,
                                                   SC_fields, &qconf_comma_sfi,
                                                   SP_DEST_TMP, SP_FORM_ASCII, nullptr, false);
   CHECK(id, "scheduler config write returns non-null", file1 != nullptr); id++;
   lFreeList(&alp);

   if (file1 != nullptr) {
      std::string line = get_attribute_line(read_unwrapped(file1), "usage_weight_list");

      CHECK(id, "usage_weight_list is written as name=value pairs only",
            line.find("cpu=1.000000,mem=0.000000,io=0.000000") != std::string::npos); id++;
      CHECK(id, "usage_weight_list carries no NONE placeholder for UA_svalue",
            line.find("NONE") == std::string::npos); id++;

      lListElem *reread = spool_flatfile_read_object(&alp, SC_Type, nullptr,
                                                     SC_fields, nullptr, true, &qconf_comma_sfi,
                                                     SP_FORM_ASCII, nullptr, file1);
      CHECK(id, "scheduler config read returns non-null", reread != nullptr); id++;

      if (reread != nullptr) {
         const lListElem *cpu = lGetSubStr(reread, UA_name, USAGE_ATTR_CPU, SC_usage_weight_list);
         CHECK(id, "cpu weight survives the round-trip", cpu != nullptr); id++;
         CHECK(id, "cpu weight value is preserved",
               cpu != nullptr && lGetDouble(cpu, UA_value) == 1.0); id++;
         CHECK(id, "cpu weight has no string value after the round-trip",
               cpu != nullptr && lGetString(cpu, UA_svalue) == nullptr); id++;
         lFreeElem(&reread);
      }

      sge_unlink(nullptr, file1);
      sge_free(&file1);
      lFreeList(&alp);
   }

   lFreeElem(&sconf);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
