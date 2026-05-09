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

#include <cstdio>
#include <cstring>
#include <string>

#include "sge_string.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// sge_strlcpy  [T01–T04]
//
// sge_strlcpy() copies at most (size-1) characters from src into dst and
// always NUL-terminates when size > 0.  It returns the length of src.
// ---------------------------------------------------------------------------

static void test_strlcpy() {
   printf("\n--- sge_strlcpy ---\n");
   char buf[10];

   // T01: source shorter than buffer — full content copied, NUL-terminated
   sge_strlcpy(buf, "hello", sizeof(buf));
   CHECK(1, "short src copied verbatim", strcmp(buf, "hello") == 0);

   // T02: source longer than buffer — truncated to (size-1) chars, NUL-terminated
   sge_strlcpy(buf, "12345678901234567890", sizeof(buf));
   CHECK(2, "long src truncated to 9 chars", strlen(buf) == 9);
   CHECK(3, "truncated string is NUL-terminated", buf[9] == '\0');

   // T04: empty source — dst becomes empty string
   sge_strlcpy(buf, "", sizeof(buf));
   CHECK(4, "empty src yields empty dst", strcmp(buf, "") == 0);
}

// ---------------------------------------------------------------------------
// sge_strlcat  [T05–T08]
//
// sge_strlcat() appends src to dst up to (size-1) total characters and always
// NUL-terminates when size > 0.
// ---------------------------------------------------------------------------

static void test_strlcat() {
   printf("\n--- sge_strlcat ---\n");
   char buf[10];

   // T05: concatenation that fits within the buffer
   sge_strlcpy(buf, "1234", sizeof(buf));
   sge_strlcat(buf, "1234", sizeof(buf));
   CHECK(5, "concat within buffer yields '12341234'", strcmp(buf, "12341234") == 0);

   // T06: concatenation that overflows — result is clamped to (size-1) chars
   sge_strlcat(buf, "1234", sizeof(buf));
   CHECK(6, "concat overflow clamped to 9 chars", strcmp(buf, "123412341") == 0);

   // T07: subsequent append when buffer already full — dst unchanged
   sge_strlcat(buf, "1234", sizeof(buf));
   CHECK(7, "append to full buffer leaves dst unchanged", strcmp(buf, "123412341") == 0);

   // T08: append to empty dst
   sge_strlcpy(buf, "", sizeof(buf));
   sge_strlcat(buf, "abc", sizeof(buf));
   CHECK(8, "append to empty dst", strcmp(buf, "abc") == 0);
}

// ---------------------------------------------------------------------------
// sge_str_move_left  [T09–T14]
//
// sge_str_move_left(start, substr) moves the content at *substr leftward so
// that it begins at *start.  Returns start on success, nullptr on invalid args.
//
// Preconditions:
//   - Both pointers must be non-null.
//   - substr must be >= start (substr points into or at the start of the buffer).
// ---------------------------------------------------------------------------

static void test_str_move_left() {
   printf("\n--- sge_str_move_left ---\n");

   // T09: both arguments nullptr — must return nullptr without crashing
   CHECK(9,  "both nullptr → nullptr",   sge_str_move_left(nullptr, nullptr) == nullptr);

   // T10: start non-null, substr nullptr — must return nullptr
   {
      char buf[] = "123 456 789";
      CHECK(10, "substr nullptr → nullptr", sge_str_move_left(buf, nullptr) == nullptr);
   }

   // T11: start nullptr, substr non-null — must return nullptr
   {
      char buf[] = "123 456 789";
      CHECK(11, "start nullptr → nullptr", sge_str_move_left(nullptr, buf) == nullptr);
   }

   // T12: substr points before start — out-of-bounds, must return nullptr
   {
      char buf[] = "123 456 789";
      CHECK(12, "substr < start → nullptr", sge_str_move_left(buf, buf - 5) == nullptr);
   }

   // T13: substr == start — no-op, returns start with buffer unchanged
   {
      char buf[] = "123 456 789";
      const char *result = sge_str_move_left(buf, buf);
      CHECK(13, "substr == start → returns start", result == buf);
      CHECK(14, "substr == start → buffer unchanged", strcmp(buf, "123 456 789") == 0);
   }

   // T14 (second part already used id 14 above; T15 here for the shift case)
}

// helper for the shift scenarios — called with fresh buffers
static void test_str_move_left_shift() {
   printf("\n--- sge_str_move_left (shift) ---\n");

   // T15: shift suffix "456 789" to front — drops "123 " prefix
   {
      char buf[] = "123 456 789";
      const char *result = sge_str_move_left(buf, buf + 4);
      CHECK(15, "shift from offset 4 → returns start", result == buf);
      CHECK(16, "shift from offset 4 → content is '456 789'", strcmp(buf, "456 789") == 0);
   }

   // T16: shift with start not at buffer head — result pointer is start, not buf[0]
   {
      char buf[] = "456 789";   // after prior test; rebuild fresh
      const char *result = sge_str_move_left(buf + 2, buf + 4);
      CHECK(17, "shift mid-buffer → result == buf+2", result == buf + 2);
      CHECK(18, "shift mid-buffer → '789' now at buf+2", strcmp(buf + 2, "789") == 0);
   }
}

// ---------------------------------------------------------------------------
// sge_strlen  [T19–T20]
//
// sge_strlen() is a nullptr-safe wrapper: returns 0 for nullptr instead of
// crashing like plain strlen().
// ---------------------------------------------------------------------------

static void test_strlen() {
   printf("\n--- sge_strlen ---\n");

   // T19: normal string — same result as strlen()
   CHECK(19, "normal string length", sge_strlen("hello") == 5);

   // T20: nullptr — returns 0 safely
   CHECK(20, "nullptr → 0", sge_strlen(nullptr) == 0);
}

// ---------------------------------------------------------------------------
// sge_strnullcmp  [T21–T24]
//
// sge_strnullcmp() is a nullptr-safe strcmp: treats nullptr as less-than any
// non-null string.  Two nullptr arguments compare equal.
// ---------------------------------------------------------------------------

static void test_strnullcmp() {
   printf("\n--- sge_strnullcmp ---\n");

   // T21: both nullptr — equal (returns 0)
   CHECK(21, "nullptr == nullptr", sge_strnullcmp(nullptr, nullptr) == 0);

   // T22: nullptr < non-null string (returns negative)
   CHECK(22, "nullptr < string", sge_strnullcmp(nullptr, "a") < 0);

   // T23: non-null string > nullptr (returns positive)
   CHECK(23, "string > nullptr", sge_strnullcmp("a", nullptr) > 0);

   // T24: two equal non-null strings
   CHECK(24, "equal strings → 0", sge_strnullcmp("abc", "abc") == 0);
}

// ---------------------------------------------------------------------------
// sge_strnullcasecmp  [T25–T26]
//
// Like sge_strnullcmp() but case-insensitive.
// ---------------------------------------------------------------------------

static void test_strnullcasecmp() {
   printf("\n--- sge_strnullcasecmp ---\n");

   // T25: same-case equal strings
   CHECK(25, "same-case equal", sge_strnullcasecmp("abc", "abc") == 0);

   // T26: mixed-case strings that are equal when folded
   CHECK(26, "ABC == abc (case-insensitive)", sge_strnullcasecmp("ABC", "abc") == 0);
}

// ---------------------------------------------------------------------------
// sge_patternnullcmp  [T27–T29]
//
// sge_patternnullcmp() compares a string against a shell-style glob pattern
// (supports '*' and '?').  Returns 0 on match, non-zero on mismatch.
// ---------------------------------------------------------------------------

static void test_patternnullcmp() {
   printf("\n--- sge_patternnullcmp ---\n");

   // T27: exact match with no wildcards
   CHECK(27, "exact match", sge_patternnullcmp("hello", "hello") == 0);

   // T28: '*' wildcard matches any suffix
   CHECK(28, "'hel*' matches 'hello'", sge_patternnullcmp("hello", "hel*") == 0);

   // T29: pattern does not match the string
   CHECK(29, "'world' does not match 'hello'", sge_patternnullcmp("hello", "world") != 0);
}

// ---------------------------------------------------------------------------
// sge_basename  [T30–T33]
//
// sge_basename(str, delim) returns a pointer to the substring following the
// last occurrence of delim in str.  If delim does not appear in str the whole
// string is returned.  Returns nullptr for nullptr input.
// ---------------------------------------------------------------------------

static void test_basename() {
   printf("\n--- sge_basename ---\n");

   // T30: normal path with '/' delimiter
   CHECK(30, "last component of '/a/b/c'", strcmp(sge_basename("/a/b/c", '/'), "c") == 0);

   // T31: delimiter not present — full string returned
   CHECK(31, "no delimiter → whole string", strcmp(sge_basename("nodot", '.'), "nodot") == 0);

   // T32: trailing delimiter — implementation returns nullptr (not an empty string)
   CHECK(32, "trailing '/' → nullptr", sge_basename("path/", '/') == nullptr);

   // T33: nullptr input — returns nullptr
   CHECK(33, "nullptr → nullptr", sge_basename(nullptr, '/') == nullptr);
}

// ---------------------------------------------------------------------------
// sge_strip_blanks  [T34–T36]
//
// sge_strip_blanks() removes ALL space characters from a string in-place
// (including interior spaces — it is NOT a trim operation).
// The function modifies the string in-place and returns void.
// ---------------------------------------------------------------------------

static void test_strip_blanks() {
   printf("\n--- sge_strip_blanks ---\n");

   // T34: leading spaces are removed
   {
      char buf[] = "   hello";
      sge_strip_blanks(buf);
      CHECK(34, "leading spaces removed", strcmp(buf, "hello") == 0);
   }

   // T35: trailing spaces are removed
   {
      char buf[] = "hello   ";
      sge_strip_blanks(buf);
      CHECK(35, "trailing spaces removed", strcmp(buf, "hello") == 0);
   }

   // T36: interior spaces are also removed (not a trim — all spaces go)
   {
      char buf[] = "a b c";
      sge_strip_blanks(buf);
      CHECK(36, "interior spaces removed (all spaces, not trim)", strcmp(buf, "abc") == 0);
   }
}

// ---------------------------------------------------------------------------
// sge_strtoupper / sge_strtolower  [T37–T38]
//
// These functions convert a string in-place and return void.
// ---------------------------------------------------------------------------

static void test_case_convert() {
   printf("\n--- sge_strtoupper / sge_strtolower ---\n");

   // T37: lowercase → uppercase
   {
      char buf[] = "hello";
      sge_strtoupper(buf, strlen(buf));
      CHECK(37, "sge_strtoupper converts to uppercase", strcmp(buf, "HELLO") == 0);
   }

   // T38: uppercase → lowercase
   {
      char buf[] = "HELLO";
      sge_strtolower(buf, strlen(buf));
      CHECK(38, "sge_strtolower converts to lowercase", strcmp(buf, "hello") == 0);
   }
}

// ---------------------------------------------------------------------------
// sge_strisint  [T39–T42]
//
// sge_strisint() returns 1 if every character in str is a digit, 0 otherwise.
// Note: an empty string returns 1 (loop never executes — vacuously true).
// ---------------------------------------------------------------------------

static void test_strisint() {
   printf("\n--- sge_strisint ---\n");

   // T39: pure digit string
   CHECK(39, "digit string is int", sge_strisint("12345") == 1);

   // T40: string with letters — not an integer
   CHECK(40, "alpha string is not int", sge_strisint("abc") == 0);

   // T41: negative number with leading '-' — not an integer (only digits accepted)
   CHECK(41, "negative number not accepted (has '-')", sge_strisint("-1") == 0);

   // T42: empty string — returns 1 (vacuously true; documents current behavior)
   CHECK(42, "empty string returns 1 (vacuously true)", sge_strisint("") == 1);
}

// ---------------------------------------------------------------------------
// sge_has_whitespace  [T43–T45]
//
// sge_has_whitespace() returns true if the string contains any whitespace
// character (space, tab, newline, etc.).
// ---------------------------------------------------------------------------

static void test_has_whitespace() {
   printf("\n--- sge_has_whitespace ---\n");

   // T43: space character
   CHECK(43, "string with space has whitespace", sge_has_whitespace("a b") == true);

   // T44: tab character
   CHECK(44, "string with tab has whitespace", sge_has_whitespace("a\tb") == true);

   // T45: no whitespace
   CHECK(45, "string without whitespace", sge_has_whitespace("abc") == false);
}

// ---------------------------------------------------------------------------
// sge_str_is_number  [T46–T49]
//
// sge_str_is_number() returns true when the string can be parsed as an integer
// or floating-point number.
// ---------------------------------------------------------------------------

static void test_str_is_number() {
   printf("\n--- sge_str_is_number ---\n");

   // T46: plain integer
   CHECK(46, "integer string is a number", sge_str_is_number("42") == true);

   // T47: negative integer
   CHECK(47, "negative integer is a number", sge_str_is_number("-7") == true);

   // T48: decimal number
   CHECK(48, "decimal string is a number", sge_str_is_number("3.14") == true);

   // T49: non-numeric string
   CHECK(49, "alpha string is not a number", sge_str_is_number("abc") == false);
}

// ---------------------------------------------------------------------------
// sge_replace_substring  [T50–T53]
//
// sge_replace_substring(src, from, to) returns a newly heap-allocated string
// with all occurrences of 'from' replaced by 'to'.  Returns nullptr when 'from'
// does not appear in src (the caller must free() the returned pointer).
// ---------------------------------------------------------------------------

static void test_replace_substring() {
   printf("\n--- sge_replace_substring ---\n");

   // T50: 'from' appears — result is non-null (caller must free)
   {
      const char *r = sge_replace_substring("hello world", "world", "there");
      CHECK(50, "match → non-null result", r != nullptr);

      // T51: replaced content is correct
      CHECK(51, "replacement content correct", r != nullptr && strcmp(r, "hello there") == 0);
      free(const_cast<char *>(r));
   }

   // T52: 'from' does not appear — returns nullptr (not the original string)
   {
      const char *r = sge_replace_substring("hello", "xyz", "abc");
      CHECK(52, "no match → nullptr", r == nullptr);
   }

   // T53: nullptr src — returns nullptr without crashing
   {
      const char *r = sge_replace_substring(nullptr, "x", "y");
      CHECK(53, "nullptr src → nullptr", r == nullptr);
   }
}

// ---------------------------------------------------------------------------
// sge_compress_slashes  [T54–T56]
//
// sge_compress_slashes() collapses runs of consecutive '/' characters into a
// single '/' in-place.  A string with no consecutive slashes is unchanged.
// ---------------------------------------------------------------------------

static void test_compress_slashes() {
   printf("\n--- sge_compress_slashes ---\n");

   // T54: a run of two slashes is collapsed to one
   {
      char buf[] = "a//b";
      sge_compress_slashes(buf);
      CHECK(54, "double slash compressed to single", strcmp(buf, "a/b") == 0);
   }

   // T55: a string with no consecutive slashes is unchanged
   {
      char buf[] = "a/b/c";
      sge_compress_slashes(buf);
      CHECK(55, "no consecutive slashes — unchanged", strcmp(buf, "a/b/c") == 0);
   }

   // T56: a run of three slashes is also collapsed to one
   {
      char buf[] = "a///b";
      sge_compress_slashes(buf);
      CHECK(56, "triple slash compressed to single", strcmp(buf, "a/b") == 0);
   }
}

// ---------------------------------------------------------------------------
// sge_strip_trailing_blanks  [T57–T59]
//
// sge_strip_trailing_blanks() removes trailing space and tab characters
// in-place.  Only ' ' and '\t' are stripped — it does NOT remove interior or
// leading whitespace (contrast with sge_strip_blanks).
// ---------------------------------------------------------------------------

static void test_strip_white_space_at_eol() {
   printf("\n--- sge_strip_trailing_blanks ---\n");

   // T57: trailing spaces are removed
   {
      char buf[] = "hello   ";
      sge_strip_trailing_blanks(buf);
      CHECK(57, "trailing spaces removed", strcmp(buf, "hello") == 0);
   }

   // T58: trailing tab is removed
   {
      char buf[] = "hello\t\t";
      sge_strip_trailing_blanks(buf);
      CHECK(58, "trailing tabs removed", strcmp(buf, "hello") == 0);
   }

   // T59: leading spaces are NOT touched — only end-of-line whitespace is stripped
   {
      char buf[] = "  hello";
      sge_strip_trailing_blanks(buf);
      CHECK(59, "leading spaces not touched", strcmp(buf, "  hello") == 0);
   }
}

// ---------------------------------------------------------------------------
// sge_strip_slash_at_eol  [T60–T62]
//
// sge_strip_slash_at_eol() removes all trailing '/' characters in-place.
// Note: the implementation does not guard against an empty string, so callers
// must not pass one.
// ---------------------------------------------------------------------------

static void test_strip_slash_at_eol() {
   printf("\n--- sge_strip_slash_at_eol ---\n");

   // T60: single trailing slash removed
   {
      char buf[] = "path/";
      sge_strip_slash_at_eol(buf);
      CHECK(60, "single trailing slash removed", strcmp(buf, "path") == 0);
   }

   // T61: multiple trailing slashes all removed
   {
      char buf[] = "path///";
      sge_strip_slash_at_eol(buf);
      CHECK(61, "multiple trailing slashes removed", strcmp(buf, "path") == 0);
   }

   // T62: no trailing slash — string unchanged
   {
      char buf[] = "path";
      sge_strip_slash_at_eol(buf);
      CHECK(62, "no trailing slash — unchanged", strcmp(buf, "path") == 0);
   }
}

// ---------------------------------------------------------------------------
// sge_dirname  [T63–T66]
//
// sge_dirname(name, delim) returns a heap-allocated string containing
// everything before the first occurrence of delim:
//   - nullptr input → nullptr
//   - string starting with delim → nullptr
//   - no delim in string → heap copy of entire string
//   - delim present → heap copy of prefix (caller must free)
// ---------------------------------------------------------------------------

static void test_dirname() {
   printf("\n--- sge_dirname ---\n");

   // T63: normal path — prefix before first '/' is returned
   {
      char *d = sge_dirname("src/libs/uti", '/');
      CHECK(63, "prefix before first '/' is 'src'", d != nullptr && strcmp(d, "src") == 0);
      free(d);
   }

   // T64: no delimiter present — copy of entire string returned
   {
      char *d = sge_dirname("nodot", '.');
      CHECK(64, "no delimiter → copy of whole string", d != nullptr && strcmp(d, "nodot") == 0);
      free(d);
   }

   // T65: string starts with delimiter — returns nullptr
   {
      char *d = sge_dirname("/absolute/path", '/');
      CHECK(65, "starts with '/' → nullptr", d == nullptr);
   }

   // T66: nullptr input — returns nullptr without crashing
   {
      char *d = sge_dirname(nullptr, '/');
      CHECK(66, "nullptr → nullptr", d == nullptr);
   }
}

// ---------------------------------------------------------------------------
// sge_str_reverse  [T67–T70]
//
// sge_str_reverse() reverses a string in-place.  nullptr is a no-op.
// ---------------------------------------------------------------------------

static void test_str_reverse() {
   printf("\n--- sge_str_reverse ---\n");

   // T67: normal string is reversed
   {
      char buf[] = "hello";
      sge_str_reverse(buf);
      CHECK(67, "\"hello\" reversed to \"olleh\"", strcmp(buf, "olleh") == 0);
   }

   // T68: palindrome content is unchanged after reversal
   {
      char buf[] = "abcba";
      sge_str_reverse(buf);
      CHECK(68, "palindrome content unchanged after reverse", strcmp(buf, "abcba") == 0);
   }

   // T69: nullptr — no crash
   {
      sge_str_reverse(nullptr);
      CHECK(69, "nullptr — no crash", true);
   }

   // T70: single character — unchanged
   {
      char buf[] = "x";
      sge_str_reverse(buf);
      CHECK(70, "single char unchanged", strcmp(buf, "x") == 0);
   }
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   test_strlcpy();
   test_strlcat();
   test_str_move_left();
   test_str_move_left_shift();
   test_strlen();
   test_strnullcmp();
   test_strnullcasecmp();
   test_patternnullcmp();
   test_basename();
   test_strip_blanks();
   test_case_convert();
   test_strisint();
   test_has_whitespace();
   test_str_is_number();
   test_replace_substring();
   test_compress_slashes();
   test_strip_white_space_at_eol();
   test_strip_slash_at_eol();
   test_dirname();
   test_str_reverse();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
