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
 * @brief Module test of ocs::ascii_width(), with std::format as the oracle (CS-3010)
 */

#include <cstdio>
#include <string>
#include <vector>

#include "uti/ocs_FormatText.h"

#include "ocs_module_test.h"

/// texts in UTF-8: ASCII, two byte, wide (CJK), emoji, a combining accent, and one longer than every width
static const std::vector<std::string> ascii_texts = {
   "", "a", "abc", "job_number:", "a text that is clearly longer than forty characters in total"
};
static const std::vector<std::string> other_texts = {
   "\xc3\xa9",                                // é
   "j\xc3\xb6" "b-\xe6\xbc\xa2\xe5\xad\x97",  // jöb-漢字
   "\xe6\xbc\xa2\xe5\xad\x97",                // 漢字, display width 4
   "\xf0\x9f\x91\x8d",                        // thumbs up
   "e\xcc\x81"                                // e + combining acute accent, display width 1
};
/// what may come between the colon and the width
static const std::vector<std::string> prefixes = {"", "<", ">", "^", "*<", "*>", "*^", "-^", " >"};
static const std::vector<int> widths = {0, 1, 2, 3, 5, 12, 40};
static const std::vector<std::string> precisions = {"", ".0", ".1", ".3", ".10", ".3s", "s"};

/**
 * @brief Compare ocs::ascii_width() with std::format for every spec, width, precision and text of a set
 *
 * Every spec is tried with the width as digits, as `{}` and as `{1}`, and with the precision as digits; a
 * precision taken from an argument is tried as `{}` and as `{2}`.
 *
 * @param texts the texts
 * @return the number of combinations whose output differs; each one is printed
 */
static int
count_differences(const std::vector<std::string> &texts) {
   int differences = 0;
   for (const auto &text : texts) {
      for (const auto &prefix : prefixes) {
         for (int width : widths) {
            for (const auto &precision : precisions) {
               const std::string specs[] = {
                  "{:" + prefix + (width > 0 ? std::to_string(width) : "") + precision + "}",
                  "{:" + prefix + "{}" + precision + "}",
                  "{0:" + prefix + "{1}" + precision + "}",
               };
               for (const auto &spec : specs) {
                  const std::string expected = std::vformat(spec, std::make_format_args(text, width));
                  const auto wrapped = ocs::ascii_width(text);
                  const std::string got = std::vformat(spec, std::make_format_args(wrapped, width));
                  if (got != expected) {
                     printf("spec %s, width %d, text \"%s\": \"%s\", expected \"%s\"\n",
                            spec.c_str(), width, text.c_str(), got.c_str(), expected.c_str());
                     differences++;
                  }
               }
            }
            for (int precision : {0, 2, 7}) {
               const std::string specs[] = {"{:" + prefix + "{}.{}}", "{0:" + prefix + "{1}.{2}}"};
               for (const auto &spec : specs) {
                  const std::string expected = std::vformat(spec, std::make_format_args(text, width, precision));
                  const auto wrapped = ocs::ascii_width(text);
                  const std::string got = std::vformat(spec, std::make_format_args(wrapped, width, precision));
                  if (got != expected) {
                     printf("spec %s, width %d, precision %d, text \"%s\": \"%s\", expected \"%s\"\n",
                            spec.c_str(), width, precision, text.c_str(), got.c_str(), expected.c_str());
                     differences++;
                  }
               }
            }
         }
      }
   }
   return differences;
}

/**
 * @brief Whether a spec is refused with std::format_error
 *
 * @param spec the format string, formatted with the wrapped text "x" and, if it needs one, @p width
 * @param width the width argument
 * @return true when std::vformat throws std::format_error
 */
template<typename W>
static bool
refused(const std::string &spec, W width) {
   try {
      const auto wrapped = ocs::ascii_width("x");
      (void) std::vformat(spec, std::make_format_args(wrapped, width));
   } catch (const std::format_error &) {
      return true;
   }
   return false;
}

/**
 * @brief ASCII text is padded by its byte count exactly as std::format pads it
 *
 * @return nothing; CHECK records the results
 */
static void
test_ascii() {
   CHECK(1, "ASCII text gives std::format's output for every fill, alignment, width, precision and type s",
         count_differences(ascii_texts) == 0);
   CHECK(2, "a format string checked at compile time gives the same output",
         std::format("{:<{}}|{:>5}|{:*^7}|{}|{:<6.3}|", ocs::ascii_width("ab"), 4, ocs::ascii_width("x"),
                     ocs::ascii_width(std::string("mid")), ocs::ascii_width("plain"), ocs::ascii_width("abcdef"))
         == "ab  |    x|**mid**|plain|abc   |");
}

/**
 * @brief Text which is not ASCII is left to std::format and keeps its display width
 *
 * @return nothing; CHECK records the results
 */
static void
test_other() {
   CHECK(3, "two byte, wide, emoji and combining text gives std::format's output",
         count_differences(other_texts) == 0);
}

/**
 * @brief What the wrapper does not support is refused, not formatted differently; nullptr is the empty string
 *
 * @return nothing; CHECK records the results
 */
static void
test_refused() {
   const char *null_pointer = nullptr;
   const auto null_text = ocs::ascii_width(null_pointer);
   CHECK(4, "a type other than s, debug format, zero padding, sign, alternate form, locale and an empty precision "
            "are refused",
         refused("{:x}", 0) && refused("{:?}", 0) && refused("{:05}", 0) && refused("{:+}", 0)
         && refused("{:#}", 0) && refused("{:L}", 0) && refused("{:.}", 0));
   CHECK(5, "a negative width or a width which is not an integer is refused",
         refused("{:<{}}", -1) && refused("{:<{}}", std::string("4")) && refused("{:<{}}", true));
   CHECK(6, "nullptr is the empty string: unused it is never read, used it prints nothing",
         std::vformat("        ", std::make_format_args(null_text)) == "        "
         && std::vformat("{:<4}|", std::make_format_args(null_text)) == "    |");
}

int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"FMTTEXT-01", "ASCII text",
       "a string wrapped in ocs::ascii_width() is padded by its byte count exactly as std::format pads it",
       {"format ASCII texts with every fill, alignment, width and precision as digits, {} and {n}, wrapped and not",
        "format a compile time checked string"},
       "the outputs are identical",
       {}, {1, 2}, test_ascii},
      {"FMTTEXT-02", "text which is not ASCII",
       "a wrapped string which is not ASCII keeps the display width std::format computes",
       {"format two byte, wide, emoji and combining texts with every spec, wrapped and not"},
       "the outputs are identical",
       {}, {3}, test_other},
      {"FMTTEXT-03", "unsupported specs",
       "the wrapper refuses what it does not implement instead of formatting it differently",
       {"format with another type, ?, 0, +, #, L or an empty precision", "format with a negative or non-integer width",
        "wrap nullptr, format it with and without a field for it"},
       "every attempt throws std::format_error; nullptr gives the empty string",
       {}, {4, 5, 6}, test_refused},
   });
}
