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
 * @brief Module test for reading the output of a load sensor (CS-3209)
 *
 * The execution daemon read every line a load sensor wrote with `fscanf("%[^\n]\n")` into a stack buffer of
 * 10000 characters and split it with `sscanf("%[^:]:%[^:]:%[^\n]")` into three buffers of 1000 characters.
 * A longer line or a longer field overflowed the stack of sge_execd. The tests feed ocs_ls_read_line() the
 * output of a sensor through a temporary file that stands in for the pipe: normal output, lines longer than
 * 10000 characters, fields longer than 1000 characters, malformed lines and empty lines between the lines.
 */

#include <cstdio>
#include <string>
#include <vector>

#include "ocs_load_sensor_line.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

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

/** @brief One line as ocs_ls_read_line() classified it */
struct ParsedLine {
   ocs_ls_line_type type;  ///< what the line is
   std::string line;       ///< the line as it was read
   std::string host;       ///< the host field of a VALUE line
   std::string name;       ///< the name field of a VALUE line
   std::string value;      ///< the value field of a VALUE line
};

/**
 * @brief Read everything a load sensor wrote
 *
 * The text is written to a temporary file, which is read with ocs_ls_read_line() until no line is left.
 *
 * @param text what the load sensor wrote
 * @return the lines in the order they were read
 */
static std::vector<ParsedLine>
read_all(const std::string &text) {
   std::vector<ParsedLine> lines;
   FILE *fp = tmpfile();
   if (fp == nullptr) {
      perror("tmpfile");
      return lines;
   }
   fputs(text.c_str(), fp);
   rewind(fp);

   ParsedLine parsed;
   while ((parsed.type = ocs_ls_read_line(fp, parsed.line, parsed.host, parsed.name, parsed.value)) !=
          ocs_ls_line_type::NONE) {
      lines.push_back(parsed);
      parsed = ParsedLine{};
   }
   fclose(fp);
   return lines;
}

/**
 * @brief Whether a line is a VALUE line with the expected fields
 * @param parsed the line
 * @param host the expected host
 * @param name the expected name
 * @param value the expected value
 * @return true if type and all three fields match
 */
static bool
is_value(const ParsedLine &parsed, const std::string &host, const std::string &name, const std::string &value) {
   return parsed.type == ocs_ls_line_type::VALUE && parsed.host == host && parsed.name == name &&
          parsed.value == value;
}

/**
 * @brief A text of 'length' characters that shows where it was cut
 * @param length the number of characters, at least 3
 * @return "0123456789" repeated, ending with "END"
 */
static std::string
long_text(const size_t length) {
   std::string text;
   while (text.size() < length - 3) {
      text.push_back(static_cast<char>('0' + text.size() % 10));
   }
   return text + "END";
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/**
 * @brief A normal load report is read as before
 */
static void
test_normal_output() {
   printf("\n--- normal load sensor output ---\n");
   const auto lines = read_all("begin\nhost1:arch_load:0.5\nglobal:license:a:b\nend\nstart\nend\n");
   CHECK(1, "six lines are read", lines.size() == 6);
   if (lines.size() == 6) {
      CHECK(2, "begin", lines[0].type == ocs_ls_line_type::BEGIN);
      CHECK(3, "host:name:value", is_value(lines[1], "host1", "arch_load", "0.5"));
      CHECK(4, "the value can contain a colon", is_value(lines[2], "global", "license", "a:b"));
      CHECK(5, "end", lines[3].type == ocs_ls_line_type::END);
      CHECK(6, "start is a begin", lines[4].type == ocs_ls_line_type::BEGIN);
      CHECK(7, "the second end", lines[5].type == ocs_ls_line_type::END);
   }
}

/**
 * @brief Lines that are not `host:name:value` are format errors, as before
 */
static void
test_format_errors() {
   printf("\n--- malformed lines ---\n");
   const auto lines = read_all("no colon\n:name:value\nhost::value\nhost:name:\nhost:name\nbegin x\n");
   CHECK(8, "six lines are read", lines.size() == 6);
   bool all_errors = lines.size() == 6;
   for (const auto &parsed : lines) {
      all_errors = all_errors && parsed.type == ocs_ls_line_type::FORMAT_ERROR;
   }
   CHECK(9, "every line is a format error", all_errors);
   CHECK(10, "the line is returned as it was read", !lines.empty() && lines[0].line == "no colon");
}

/**
 * @brief Empty lines and the indentation between lines are skipped, as before
 */
static void
test_line_separation() {
   printf("\n--- empty lines and whitespace ---\n");
   const auto lines = read_all("\n\nbegin\n\n  host:n:v\n \t\n\tend\n\n");
   CHECK(11, "three lines are read", lines.size() == 3);
   if (lines.size() == 3) {
      CHECK(12, "begin after empty lines", lines[0].type == ocs_ls_line_type::BEGIN);
      CHECK(13, "indented value line", is_value(lines[1], "host", "n", "v"));
      CHECK(14, "indented end", lines[2].type == ocs_ls_line_type::END);
   }
}

/**
 * @brief Lines longer than the former 10000 character buffer arrive complete (CS-3209)
 */
static void
test_long_lines() {
   printf("\n--- lines longer than 10000 characters ---\n");
   const std::string value = long_text(20000);
   const std::string garbage = long_text(30000);
   const auto lines = read_all("begin\nhost:name:" + value + "\n" + garbage + "\nend\n");
   CHECK(15, "four lines are read", lines.size() == 4);
   if (lines.size() == 4) {
      printf("      value: %zu characters, garbage line: %zu characters\n", lines[1].value.size(),
             lines[2].line.size());
      CHECK(16, "the long value arrives complete", is_value(lines[1], "host", "name", value));
      CHECK(17, "the long line without colons is one format error",
            lines[2].type == ocs_ls_line_type::FORMAT_ERROR && lines[2].line == garbage);
      CHECK(18, "the line after the long ones is read", lines[3].type == ocs_ls_line_type::END);
   }
}

/**
 * @brief Each field can be longer than the former 1000 character buffers (CS-3209)
 */
static void
test_long_fields() {
   printf("\n--- fields longer than 1000 characters ---\n");
   const std::string host = "h" + long_text(2000);
   const std::string name = "n" + long_text(3000);
   const std::string value = "v" + long_text(4000);
   const auto lines = read_all(host + ":short:1\nhost:" + name + ":2\nhost:short:" + value + "\n" + host + ":" + name +
                               ":" + value + "\nend\n");
   CHECK(19, "five lines are read", lines.size() == 5);
   if (lines.size() == 5) {
      CHECK(20, "long host", is_value(lines[0], host, "short", "1"));
      CHECK(21, "long name", is_value(lines[1], "host", name, "2"));
      CHECK(22, "long value", is_value(lines[2], "host", "short", value));
      CHECK(23, "all three fields long", is_value(lines[3], host, name, value));
      CHECK(24, "the line after the long ones is read", lines[4].type == ocs_ls_line_type::END);
   }
}

/**
 * @brief Entry point
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   test_normal_output();
   test_format_errors();
   test_line_separation();
   test_long_lines();
   test_long_fields();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
