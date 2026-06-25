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
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"
#include "uti/sge_tmpnam.h"
#include "basis_types.h"

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

#define STATIC_SIZE 20

static void test_null_safety(int *id) {
   printf("\n--- null-pointer safety ---\n");

   // T01-T03: null-pointer calls return 0/null and don't crash
   CHECK(*id, "sge_dstring_get_string(nullptr) returns null",
         sge_dstring_get_string(nullptr) == nullptr); (*id)++;
   CHECK(*id, "sge_dstring_strlen(nullptr) returns 0",
         sge_dstring_strlen(nullptr) == 0); (*id)++;
   CHECK(*id, "sge_dstring_remaining(nullptr) returns 0",
         sge_dstring_remaining(nullptr) == 0); (*id)++;

   // void-return null calls must not crash (implicit)
   sge_dstring_append(nullptr, nullptr);
   sge_dstring_append(nullptr, "text");
   sge_dstring_append_char(nullptr, 'x');
   sge_dstring_sprintf(nullptr, "fmt");
   sge_dstring_sprintf_append(nullptr, "fmt");
   sge_dstring_clear(nullptr);
   sge_dstring_free(nullptr);
   sge_dstring_copy_string(nullptr, nullptr);
   sge_dstring_copy_string(nullptr, "text");
   sge_dstring_copy_dstring(nullptr, nullptr);
}

static void test_append(int *id) {
   printf("\n--- sge_dstring_append ---\n");

   dstring ds = DSTRING_INIT;

   // T04: basic append
   sge_dstring_append(&ds, "hello");
   CHECK(*id, "append \"hello\": get_string returns \"hello\"",
         strcmp(sge_dstring_get_string(&ds), "hello") == 0); (*id)++;

   // T05: append null doesn't change content
   sge_dstring_append(&ds, nullptr);
   CHECK(*id, "append nullptr: content unchanged",
         strcmp(sge_dstring_get_string(&ds), "hello") == 0); (*id)++;

   // T06: strlen matches
   CHECK(*id, "strlen after append equals 5",
         sge_dstring_strlen(&ds) == 5); (*id)++;

   // T07: append_dstring concatenates correctly
   {
      dstring ds2 = DSTRING_INIT;
      sge_dstring_append(&ds2, " world");
      sge_dstring_append_dstring(&ds, &ds2);
      CHECK(*id, "append_dstring: result is \"hello world\"",
            strcmp(sge_dstring_get_string(&ds), "hello world") == 0); (*id)++;
      sge_dstring_free(&ds2);
   }

   // T08: append_char builds string correctly
   sge_dstring_clear(&ds);
   sge_dstring_append_char(&ds, 'x');
   sge_dstring_append_char(&ds, 'y');
   CHECK(*id, "append_char 'x'+'y': result is \"xy\"",
         strcmp(sge_dstring_get_string(&ds), "xy") == 0); (*id)++;

   sge_dstring_free(&ds);
}

static void test_sprintf(int *id) {
   printf("\n--- sge_dstring_sprintf / sprintf_append ---\n");

   dstring ds = DSTRING_INIT;

   // T09: sprintf formats correctly
   sge_dstring_sprintf(&ds, "val=%d", 42);
   CHECK(*id, "sprintf \"val=%d\" 42: result is \"val=42\"",
         strcmp(sge_dstring_get_string(&ds), "val=42") == 0); (*id)++;

   // T10: sprintf replaces previous content
   sge_dstring_sprintf(&ds, "new");
   CHECK(*id, "sprintf \"new\": replaces previous content",
         strcmp(sge_dstring_get_string(&ds), "new") == 0); (*id)++;

   // T11: sprintf_append adds to existing content
   sge_dstring_sprintf_append(&ds, "=%d", 7);
   CHECK(*id, "sprintf_append \"=%d\" 7: result is \"new=7\"",
         strcmp(sge_dstring_get_string(&ds), "new=7") == 0); (*id)++;

   sge_dstring_free(&ds);
}

static void test_copy_clear_free(int *id) {
   printf("\n--- copy / clear / free ---\n");

   dstring ds = DSTRING_INIT;
   sge_dstring_append(&ds, "original");

   // T12: copy_string replaces content
   sge_dstring_copy_string(&ds, "replaced");
   CHECK(*id, "copy_string \"replaced\": content is \"replaced\"",
         strcmp(sge_dstring_get_string(&ds), "replaced") == 0); (*id)++;
   CHECK(*id, "strlen after copy_string equals 8",
         sge_dstring_strlen(&ds) == 8); (*id)++;

   // T14: clear resets length to 0
   sge_dstring_clear(&ds);
   CHECK(*id, "after clear: strlen is 0",
         sge_dstring_strlen(&ds) == 0); (*id)++;

   // T15: after free, dstring is reset (get_string returns null, strlen is 0)
   sge_dstring_append(&ds, "before free");
   sge_dstring_free(&ds);
   CHECK(*id, "after free: get_string returns null",
         sge_dstring_get_string(&ds) == nullptr); (*id)++;
   CHECK(*id, "after free: strlen is 0",
         sge_dstring_strlen(&ds) == 0); (*id)++;
}

static void test_static_dstring(int *id) {
   printf("\n--- static dstring (buffer size %d) ---\n", STATIC_SIZE);

   char buf[STATIC_SIZE];
   dstring ds;
   sge_dstring_init(&ds, buf, STATIC_SIZE);

   // T17: short string fits in static buffer
   sge_dstring_append(&ds, "short");
   CHECK(*id, "static dstring: short string stored correctly",
         strcmp(sge_dstring_get_string(&ds), "short") == 0); (*id)++;

   // T18: long string is silently truncated to buffer capacity
   sge_dstring_clear(&ds);
   sge_dstring_append(&ds, "this string is definitely too long for the buffer");
   CHECK(*id, "static dstring: overlong string is truncated to STATIC_SIZE-1 chars",
         sge_dstring_strlen(&ds) == STATIC_SIZE - 1); (*id)++;

   // T19: remaining reports available space correctly
   sge_dstring_clear(&ds);
   sge_dstring_append(&ds, "abc");
   CHECK(*id, "static dstring: remaining = STATIC_SIZE - strlen - 1",
         sge_dstring_remaining(&ds) == STATIC_SIZE - 3 - 1); (*id)++;
}

static void test_split(int *id) {
   printf("\n--- sge_dstring_split ---\n");

   dstring src = DSTRING_INIT;
   dstring before = DSTRING_INIT;
   dstring after = DSTRING_INIT;

   sge_dstring_append(&src, "key:value");

   // T20: split on ':' produces correct before and after
   bool ok = sge_dstring_split(&src, ':', &before, &after);
   CHECK(*id, "split \"key:value\" on ':' returns true",
         ok); (*id)++;
   CHECK(*id, "split: before part is \"key\"",
         strcmp(sge_dstring_get_string(&before), "key") == 0); (*id)++;
   CHECK(*id, "split: after part is \"value\"",
         strcmp(sge_dstring_get_string(&after), "value") == 0); (*id)++;

   sge_dstring_free(&src);
   sge_dstring_free(&before);
   sge_dstring_free(&after);
}

static void test_nappend(int *id) {
   printf("\n--- sge_dstring_nappend ---\n");

   dstring ds = DSTRING_INIT;

   // T23: nappend takes only the first N chars
   sge_dstring_nappend(&ds, "hello world", 5);
   CHECK(*id, "nappend(\"hello world\", 5): result is \"hello\"",
         strcmp(sge_dstring_get_string(&ds), "hello") == 0); (*id)++;

   // T24: nappend with N >= strlen appends the whole string
   sge_dstring_clear(&ds);
   sge_dstring_nappend(&ds, "hi", 100);
   CHECK(*id, "nappend(\"hi\", 100): result is \"hi\"",
         strcmp(sge_dstring_get_string(&ds), "hi") == 0); (*id)++;

   sge_dstring_free(&ds);
}

static void test_copy_dstring(int *id) {
   printf("\n--- sge_dstring_copy_dstring ---\n");

   dstring src = DSTRING_INIT;
   dstring dst = DSTRING_INIT;
   sge_dstring_append(&src, "source content");
   sge_dstring_append(&dst, "old");

   // T25: copy_dstring replaces destination content with source
   sge_dstring_copy_dstring(&dst, &src);
   CHECK(*id, "copy_dstring: destination becomes \"source content\"",
         strcmp(sge_dstring_get_string(&dst), "source content") == 0); (*id)++;
   CHECK(*id, "copy_dstring: source is unchanged",
         strcmp(sge_dstring_get_string(&src), "source content") == 0); (*id)++;

   sge_dstring_free(&src);
   sge_dstring_free(&dst);
}

static void test_strip_whitespace(int *id) {
   printf("\n--- sge_dstring_strip_trailing_blanks ---\n");

   dstring ds = DSTRING_INIT;

   // T27: trailing spaces (and tabs) are removed from the raw buffer;
   // the function strips only ' ' and '\t', not '\n'
   sge_dstring_append(&ds, "hello   ");
   sge_dstring_strip_trailing_blanks(&ds);
   CHECK(*id, "strip_white_space_at_eol \"hello   \": get_string is \"hello\"",
         strcmp(sge_dstring_get_string(&ds), "hello") == 0); (*id)++;

   // T28: string with no trailing whitespace is unchanged
   sge_dstring_copy_string(&ds, "clean");
   sge_dstring_strip_trailing_blanks(&ds);
   CHECK(*id, "strip_white_space_at_eol \"clean\": unchanged",
         strcmp(sge_dstring_get_string(&ds), "clean") == 0); (*id)++;

   sge_dstring_free(&ds);
}

static void test_ulong_to_binstring(int *id) {
   printf("\n--- sge_dstring_ulong_to_binstring ---\n");

   dstring ds = DSTRING_INIT;

   // T29: binary representation of 5 is "101"
   sge_dstring_ulong_to_binstring(&ds, 5);
   CHECK(*id, "ulong_to_binstring(5) = \"101\"",
         strcmp(sge_dstring_get_string(&ds), "101") == 0); (*id)++;

   // T30: binary representation of 8 is "1000"
   sge_dstring_ulong_to_binstring(&ds, 8);
   CHECK(*id, "ulong_to_binstring(8) = \"1000\"",
         strcmp(sge_dstring_get_string(&ds), "1000") == 0); (*id)++;

   // T31: binary representation of 0 is empty string
   sge_dstring_ulong_to_binstring(&ds, 0);
   CHECK(*id, "ulong_to_binstring(0) = \"\" (empty)",
         sge_dstring_strlen(&ds) == 0); (*id)++;

   sge_dstring_free(&ds);
}

/**
 * @brief Regression test: sge_tmpnam() must not interpret $TMPDIR as a format string.
 *
 * SECURITY REGRESSION (CS-2354, MEDIUM-UTI-003, CWE-134): sge_tmpnam() used to
 * store its result with sge_dstring_sprintf(aBuffer, tmp_string), passing a path
 * derived from the attacker-influenceable $TMPDIR as the *format* string. A
 * TMPDIR containing printf conversion specifiers was therefore interpreted
 * (%n = out-of-bounds write, %s/%x = garbage read/crash). The fix copies the
 * path verbatim, so specifiers must survive literally in the returned path. The
 * test uses "%x%x" (not "%n") so a reintroduced bug fails the assertion cleanly
 * instead of writing to a wild pointer.
 *
 * @param[in,out] id running CHECK counter, advanced once per check
 */
static void test_tmpnam_format_string_safety(int *id) {
   printf("\n--- sge_tmpnam format-string safety (CS-2354) ---\n");

   char dir[256];
   snprintf(dir, sizeof(dir), "/tmp/sge_tmpnam_fmt_%%x%%x_%ld", (long)getpid());
   // dir now literally contains the four characters "%x%x"
   const bool made_dir = (mkdir(dir, 0700) == 0);

   char saved_tmpdir[SGE_PATH_MAX];
   const char *cur = getenv("TMPDIR");
   const bool had_tmpdir = (cur != nullptr);
   if (had_tmpdir) {
      snprintf(saved_tmpdir, sizeof(saved_tmpdir), "%s", cur);
   }
   setenv("TMPDIR", dir, 1);

   char buf[SGE_PATH_MAX];
   int fd = -1;
   dstring err = DSTRING_INIT;
   char *res = sge_tmpnam(buf, &fd, &err);

   CHECK(*id, "tmpnam: returns non-null for TMPDIR containing format specifiers",
         res != nullptr); (*id)++;
   CHECK(*id, "tmpnam: format specifiers in TMPDIR preserved literally (not interpreted)",
         res != nullptr && strstr(res, "%x%x") != nullptr); (*id)++;

   if (fd >= 0) {
      close(fd);
   }
   if (res != nullptr) {
      unlink(res);
   }
   if (made_dir) {
      rmdir(dir);
   }
   if (had_tmpdir) {
      setenv("TMPDIR", saved_tmpdir, 1);
   } else {
      unsetenv("TMPDIR");
   }
   sge_dstring_free(&err);
}

static void test_split_no_delimiter(int *id) {
   printf("\n--- sge_dstring_split without delimiter ---\n");

   dstring src = DSTRING_INIT;
   dstring before = DSTRING_INIT;
   dstring after = DSTRING_INIT;
   sge_dstring_append(&src, "nodot");

   // T32: when delimiter is absent, before is empty and after gets the whole string
   sge_dstring_split(&src, '.', &before, &after);
   CHECK(*id, "split \"nodot\" on '.': before is empty",
         sge_dstring_strlen(&before) == 0); (*id)++;
   CHECK(*id, "split \"nodot\" on '.': after gets the whole string",
         strcmp(sge_dstring_get_string(&after), "nodot") == 0); (*id)++;

   sge_dstring_free(&src);
   sge_dstring_free(&before);
   sge_dstring_free(&after);
}

static void test_dynamic_growth(int *id) {
   printf("\n--- dynamic dstring growth ---\n");

   dstring ds = DSTRING_INIT;
   const char *chunk = "0123456789";  // 10 chars

   // append 50 chunks = 500 chars, forcing multiple reallocations
   for (int i = 0; i < 50; i++) {
      sge_dstring_append(&ds, chunk);
   }
   CHECK(*id, "after 50x 10-char appends: strlen is 500",
         sge_dstring_strlen(&ds) == 500); (*id)++;

   // verify first and last chunk are correct
   const char *s = sge_dstring_get_string(&ds);
   CHECK(*id, "after growth: first 10 chars are correct",
         s != nullptr && strncmp(s, chunk, 10) == 0); (*id)++;
   CHECK(*id, "after growth: last 10 chars are correct",
         s != nullptr && strcmp(s + 490, chunk) == 0); (*id)++;

   sge_dstring_free(&ds);
}

static void test_static_full_capacity(int *id) {
   printf("\n--- static dstring at full capacity ---\n");

   char buf[STATIC_SIZE];
   dstring ds;
   sge_dstring_init(&ds, buf, STATIC_SIZE);

   // fill the buffer to capacity (STATIC_SIZE-1 chars)
   for (int i = 0; i < STATIC_SIZE - 1; i++) {
      sge_dstring_append_char(&ds, 'x');
   }
   CHECK(*id, "after filling static buffer: strlen is STATIC_SIZE-1",
         sge_dstring_strlen(&ds) == STATIC_SIZE - 1); (*id)++;

   // one more char must be silently dropped
   sge_dstring_append_char(&ds, 'Z');
   CHECK(*id, "append_char to full static buffer: strlen is still STATIC_SIZE-1",
         sge_dstring_strlen(&ds) == STATIC_SIZE - 1); (*id)++;
   CHECK(*id, "append_char to full static buffer: 'Z' was not written",
         sge_dstring_get_string(&ds)[STATIC_SIZE - 1] == '\0'); (*id)++;
}

static void test_from_argv(int *id) {
   printf("\n--- sge_dstring_from_argv ---\n");

   dstring ds = DSTRING_INIT;
   const char *argv[] = { "ls", "-l", "/tmp" };

   // T39: basic join without quoting
   sge_dstring_from_argv(&ds, 3, argv, false, false);
   CHECK(*id, "from_argv [\"ls\", \"-l\", \"/tmp\"]: result is \"ls -l /tmp\"",
         strcmp(sge_dstring_get_string(&ds), "ls -l /tmp") == 0); (*id)++;

   // T40: arg with whitespace is quoted when quote_whitespace=true
   sge_dstring_clear(&ds);
   const char *argv2[] = { "echo", "hello world" };
   sge_dstring_from_argv(&ds, 2, argv2, true, false);
   CHECK(*id, "from_argv with whitespace arg, quote_whitespace=true: arg is quoted",
         strcmp(sge_dstring_get_string(&ds), "echo 'hello world'") == 0); (*id)++;

   sge_dstring_free(&ds);
}

static void test_dstring_performance(dstring *ds, int max, const char *data) {
   struct timeval before;
   struct timeval after;

   gettimeofday(&before, nullptr);
   for (int i = 0; i < max; i++) {
      sge_dstring_sprintf(ds, "%s", data);
   }
   gettimeofday(&after, nullptr);

   double elapsed = after.tv_sec - before.tv_sec +
                    (after.tv_usec - before.tv_usec) / 1000000.0;
   printf("%d sge_dstring_sprintf took %.2fs\n", max, elapsed);
}

static void test_dstring_performance_static(int max, const char *data) {
   struct timeval before;
   struct timeval after;

   gettimeofday(&before, nullptr);
   for (int i = 0; i < max; i++) {
      dstring ds;
      char ds_buffer[MAX_STRING_SIZE];
      sge_dstring_init(&ds, ds_buffer, sizeof(ds_buffer));
      sge_dstring_sprintf(&ds, "%s/%s", data, data);
   }
   gettimeofday(&after, nullptr);

   double elapsed = after.tv_sec - before.tv_sec +
                    (after.tv_usec - before.tv_usec) / 1000000.0;
   printf("%d static dstring creations took %.2fs\n", max, elapsed);
}

static void test_dstring_performance_dynamic(int max, const char *data) {
   struct timeval before;
   struct timeval after;

   gettimeofday(&before, nullptr);
   for (int i = 0; i < max; i++) {
      dstring ds = DSTRING_INIT;
      sge_dstring_sprintf(&ds, "%s/%s", data, data);
      sge_dstring_free(&ds);
   }
   gettimeofday(&after, nullptr);

   double elapsed = after.tv_sec - before.tv_sec +
                    (after.tv_usec - before.tv_usec) / 1000000.0;
   printf("%d dynamic dstring creations took %.2fs\n", max, elapsed);
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_dstring");

   int id = 1;

   test_null_safety(&id);
   test_append(&id);
   test_sprintf(&id);
   test_copy_clear_free(&id);
   test_static_dstring(&id);
   test_split(&id);
   test_nappend(&id);
   test_copy_dstring(&id);
   test_strip_whitespace(&id);
   test_ulong_to_binstring(&id);
   test_tmpnam_format_string_safety(&id);
   test_split_no_delimiter(&id);
   test_dynamic_growth(&id);
   test_static_full_capacity(&id);
   test_from_argv(&id);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);

   /* performance benchmarks — not functional tests */
   dstring dynamic_ds = DSTRING_INIT;
   printf("\nperformance benchmarks\n");
   test_dstring_performance(&dynamic_ds, 100000, "test_data");
   test_dstring_performance_dynamic(100000, "test_data");
   test_dstring_performance_static(100000, "test_data");
   sge_dstring_free(&dynamic_ds);

   DRETURN(s_fail == 0 ? 0 : 1);
}
