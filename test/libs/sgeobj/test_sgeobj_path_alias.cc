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
 * @brief Unit tests for reading the path alias files in `libs/sgeobj`
 *
 * CS-3210: each line of `<cell>/common/sge_aliases` and `~/.sge_aliases` was read into a buffer of 10000
 * characters and split with `sscanf("%s %s %s %s")` into buffers of `SGE_PATH_MAX` characters without a
 * field width, so an over-long field overflowed the stack, and a line longer than the buffer was split into
 * several lines. The tests write alias files and read them with path_alias_list_initialize(): regular
 * entries, comments and empty lines, a field longer than `SGE_PATH_MAX` (rejected with an error) and lines
 * longer than the old buffer (read as one line).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_path_alias.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;
static std::string s_cell_root;
static std::string s_home_dir;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `ok`/`FAIL` with the test's id and label and counts the failure, so
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
 * @brief Write a file, or remove it
 * @param path the file to write
 * @param text the content of the file, nullptr removes the file
 */
static void
write_file(const std::string &path, const char *text) {
   if (text == nullptr) {
      unlink(path.c_str());
      return;
   }
   FILE *fp = fopen(path.c_str(), "w");
   if (fp == nullptr) {
      printf("cannot write %s\n", path.c_str());
      exit(1);
   }
   fputs(text, fp);
   fclose(fp);
}

/** @brief Result of reading the alias files */
struct ReadResult {
   int ret{0};                              ///< return value of path_alias_list_initialize()
   ocs::tom::ListPtr<ocs::tom::PA> list;    ///< the entries read
   std::string first_error{"<none>"};       ///< the text of the first error answer
};

/**
 * @brief Write the common and the user alias file and read them
 * @param common content of the cluster global file, nullptr for no file
 * @param home content of the user file, nullptr for no file
 * @return return value, entries and first error of path_alias_list_initialize()
 */
static ReadResult
read_aliases(const char *common, const char *home = nullptr) {
   write_file(s_cell_root + "/common/sge_aliases", common);
   write_file(s_home_dir + "/.sge_aliases", home);

   ReadResult result;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   result.ret = path_alias_list_initialize(result.list, &alp, s_cell_root.c_str(), "nobody", "submithost");
   if (alp) {
      for (const auto answer : *alp) {
         if (answer.get<AN_quality>() == ANSWER_QUALITY_ERROR) {
            result.first_error = answer.get<AN_text>() != nullptr ? answer.get<AN_text>() : "";
            break;
         }
      }
   }
   return result;
}

/**
 * @brief Count the entries of a PA list
 * @param list the list, may be an empty handle
 * @return the number of entries
 */
static size_t
entries(const ocs::tom::ListPtr<ocs::tom::PA> &list) {
   return list ? list->size() : 0;
}

/**
 * @brief Check whether an entry has the given values
 * @param list the list read
 * @param pos the position of the entry
 * @param origin the expected source path
 * @param exec_host the expected execution host
 * @param translation the expected replacement
 * @return true if the entry exists and all three values match
 */
static bool
entry_is(const ocs::tom::ListPtr<ocs::tom::PA> &list, size_t pos, const std::string &origin,
         const std::string &exec_host, const std::string &translation) {
   if (entries(list) <= pos) {
      return false;
   }
   size_t i = 0;
   for (const auto pa : *list) {
      if (i++ == pos) {
         const std::string got_origin = pa.get<PA_origin>();
         const std::string got_submit_host = pa.get<PA_submit_host>().c_str();
         const std::string got_exec_host = pa.get<PA_exec_host>().c_str();
         const std::string got_translation = pa.get<PA_translation>();
         const bool same = origin == got_origin && got_submit_host == "*" && exec_host == got_exec_host
                           && translation == got_translation;
         if (!same && got_origin.size() + got_translation.size() < 200) {
            printf("      entry %zu is \"%s\" \"%s\" \"%s\" \"%s\"\n", pos, got_origin.c_str(), got_submit_host.c_str(),
                   got_exec_host.c_str(), got_translation.c_str());
         }
         return same;
      }
   }
   return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/** @brief Regular files: comments, empty lines, blanks and tabs, extra fields, slashes, both files */
static void
test_regular_files() {
   {
      const auto r = read_aliases(nullptr);
      CHECK(1, "no alias file: no entry, no error", r.ret == 0 && entries(r.list) == 0);
   }
   {
      const auto r = read_aliases("# comment\n"
                                  "\n"
                                  "/tmp_mnt/  *  *  /\n"
                                  "#/x * * /y\n"
                                  "\t/home//user\t*\texechost\t/net///user  extra field ignored\n"
                                  "/a * * /b\r\n"
                                  "/last * * /no/newline");
      CHECK(2, "regular file: four entries", r.ret == 0 && entries(r.list) == 4);
      CHECK(3, "regular file: first entry", entry_is(r.list, 0, "/tmp_mnt/", "*", "/"));
      CHECK(4, "regular file: tabs, slashes compressed, extra fields ignored",
            entry_is(r.list, 1, "/home/user", "exechost", "/net/user"));
      CHECK(5, "regular file: carriage return is a blank", entry_is(r.list, 2, "/a", "*", "/b"));
      CHECK(6, "regular file: last line without newline", entry_is(r.list, 3, "/last", "*", "/no/newline"));
   }
   {
      const auto r = read_aliases("/common * * /c\n", "/home * * /h\n");
      CHECK(7, "user file is appended to the common file",
            r.ret == 0 && entries(r.list) == 2 && entry_is(r.list, 0, "/common", "*", "/c")
            && entry_is(r.list, 1, "/home", "*", "/h"));
   }
   {
      const auto r = read_aliases("/a * * /b\n/only three fields\n");
      CHECK(8, "fewer than four fields: syntax error",
            r.ret == -1 && r.first_error.find("invalid syntax of path alias file") != std::string::npos);
   }
   // T9, T19-T21 (CS-3213): sge_aliases(5) says blank lines are skipped, so a line of whitespace only
   // is one, whatever the whitespace is. Before the fix each of these was a syntax error.
   {
      const auto r = read_aliases("/a * * /b\n   \n/c * * /d\n");
      CHECK(9, "a line of blanks only is skipped",
            r.ret == 0 && entries(r.list) == 2 && entry_is(r.list, 1, "/c", "*", "/d"));
   }
   {
      const auto r = read_aliases("/a * * /b\n\t\t\n/c * * /d\n");
      CHECK(19, "a line of tabs only is skipped",
            r.ret == 0 && entries(r.list) == 2 && entry_is(r.list, 1, "/c", "*", "/d"));
   }
   {
      const auto r = read_aliases("/a * * /b\r\n\r\n/c * * /d\r\n");
      CHECK(20, "a line of a carriage return only (Windows editor) is skipped",
            r.ret == 0 && entries(r.list) == 2 && entry_is(r.list, 1, "/c", "*", "/d"));
   }
   {
      const auto r = read_aliases(" \t\r\n/a * * /b\n \t ");
      CHECK(21, "mixed whitespace lines, also first and last without newline, are skipped",
            r.ret == 0 && entries(r.list) == 1 && entry_is(r.list, 0, "/a", "*", "/b"));
   }
}

/** @brief A field longer than SGE_PATH_MAX is rejected with an error, one of SGE_PATH_MAX - 1 is accepted */
static void
test_long_fields() {
   const std::string longest(SGE_PATH_MAX - 1, 'p');
   const std::string too_long(SGE_PATH_MAX + 4000, 'p');
   {
      const std::string text = "/" + longest.substr(1) + " * * /b\n";
      const auto r = read_aliases(text.c_str());
      CHECK(10, "origin of SGE_PATH_MAX - 1 characters is accepted",
            r.ret == 0 && entry_is(r.list, 0, "/" + longest.substr(1), "*", "/b"));
   }
   {
      const std::string text = "/a * * /" + longest.substr(1) + "\n";
      const auto r = read_aliases(text.c_str());
      CHECK(11, "translation of SGE_PATH_MAX - 1 characters is accepted",
            r.ret == 0 && entry_is(r.list, 0, "/a", "*", "/" + longest.substr(1)));
   }
   const char *where[] = {"origin", "submit host", "exec host", "translation"};
   for (int field = 0; field < 4; ++field) {
      std::string fields[4] = {"/a", "*", "*", "/b"};
      fields[field] = too_long;
      const std::string text = fields[0] + " " + fields[1] + " " + fields[2] + " " + fields[3] + "\n";
      const auto r = read_aliases(text.c_str());
      const std::string label = std::string("too long ") + where[field] + " is rejected with an error";
      CHECK(12 + field, label.c_str(),
            r.ret == -1 && entries(r.list) == 0 && r.first_error.find("exceeds") != std::string::npos);
   }
   {
      // only the first four fields are used, a long fifth one is ignored like any other extra field
      const std::string text = "/a * * /b " + too_long + "\n";
      const auto r = read_aliases(text.c_str());
      CHECK(16, "too long extra field is ignored", r.ret == 0 && entry_is(r.list, 0, "/a", "*", "/b"));
   }
}

/** @brief Lines longer than the old buffer of 10000 characters are read as one line */
static void
test_long_lines() {
   {
      const std::string text = "#" + std::string(20000, 'c') + "\n/a * * /b\n";
      const auto r = read_aliases(text.c_str());
      CHECK(17, "long comment line is skipped as a whole",
            r.ret == 0 && entries(r.list) == 1 && entry_is(r.list, 0, "/a", "*", "/b"));
   }
   {
      const std::string text = "/a" + std::string(15000, ' ') + "*\t*" + std::string(15000, '\t') + "/b\n/c * * /d\n";
      const auto r = read_aliases(text.c_str());
      CHECK(18, "entry spread over a long line by blanks is one entry",
            r.ret == 0 && entries(r.list) == 2 && entry_is(r.list, 0, "/a", "*", "/b")
            && entry_is(r.list, 1, "/c", "*", "/d"));
   }
}

int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   const char *tmp = getenv("TMPDIR");
   std::string templ = std::string(tmp != nullptr && *tmp != '\0' ? tmp : "/tmp") + "/test_path_alias.XXXXXX";
   if (mkdtemp(templ.data()) == nullptr) {
      printf("cannot create a temporary directory\n");
      return 1;
   }
   s_cell_root = templ + "/cell";
   s_home_dir = templ + "/home";
   if (mkdir(s_cell_root.c_str(), 0700) != 0 || mkdir((s_cell_root + "/common").c_str(), 0700) != 0
       || mkdir(s_home_dir.c_str(), 0700) != 0) {
      printf("cannot create the test directories below %s\n", templ.c_str());
      return 1;
   }
   // the user file is looked up there instead of in the home directory of the user
   setenv("SGE_TEST_USER_HOME_DIR", s_home_dir.c_str(), 1);

   test_regular_files();
   test_long_fields();
   test_long_lines();

   write_file(s_cell_root + "/common/sge_aliases", nullptr);
   write_file(s_home_dir + "/.sge_aliases", nullptr);
   rmdir((s_cell_root + "/common").c_str());
   rmdir(s_cell_root.c_str());
   rmdir(s_home_dir.c_str());
   rmdir(templ.c_str());

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
