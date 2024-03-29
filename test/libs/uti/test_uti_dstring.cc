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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/time.h>

#include "uti/sge_dstring.h"

#define STATIC_SIZE 20

static bool
check_dstring(dstring *sb) {
   bool ret = true;

   printf("%5d : %5d : %-50s\n", (int) sb->size, (int) sb->length,
          sb->s == nullptr ? "(null)" : sb->s);

   return ret;
}

static bool
check_all(dstring *sb) {
   bool ret = true;
   int i;

   /* sge_dstring_append */
   printf("\nchecking sge_dstring_append\n");
   sge_dstring_append(nullptr, nullptr);

   sge_dstring_append(sb, nullptr);
   check_dstring(sb);

   sge_dstring_append(sb, "blah");
   check_dstring(sb);

   sge_dstring_clear(sb);
   sge_dstring_append(sb, "too long string to fit into a static string buffer");
   check_dstring(sb);

   sge_dstring_clear(sb);
   sge_dstring_append(sb,
                      "long string that requires multiple chunks ....... ");
   check_dstring(sb);
   for (i = 0; i < 20; i++) {
      sge_dstring_append(sb,
                         "long string that requires multiple chunks ....... ");
   }
   check_dstring(sb);

   /* sge_dstring_append_dstring */
   printf("\nchecking sge_dstring_append_dstring\n");
   sge_dstring_clear(sb);
   sge_dstring_append_dstring(nullptr, nullptr);
   {
      dstring second = DSTRING_INIT;
      sge_dstring_append(&second, "dstring");
      sge_dstring_append_dstring(nullptr, &second);
      sge_dstring_append_dstring(sb, nullptr);
      sge_dstring_append_dstring(sb, &second);
      check_dstring(sb);

      sge_dstring_free(&second);
   }

   /* sge_dstring_append_char */
   printf("\nchecking sge_dstring_append_char\n");
   sge_dstring_clear(sb);
   sge_dstring_append_char(nullptr, 'a');
   sge_dstring_append_char(sb, '\0');
   check_dstring(sb);
   sge_dstring_append_char(sb, 'a');
   check_dstring(sb);
   sge_dstring_append_char(sb, 'b');
   check_dstring(sb);

   /* sge_dstring_sprintf */
   printf("\nchecking sge_dstring_sprintf\n");
   sge_dstring_sprintf(nullptr, "test %s", "string");
   sge_dstring_sprintf(sb, nullptr);
   sge_dstring_sprintf(sb, "test %s", "string");
   check_dstring(sb);

#if 0
   /* does not build on irix */
   /* sge_dstring_vsprintf */
   printf("\nchecking sge_dstring_vsprintf\n");
   {
      const char *args[] = { "string", nullptr };
      sge_dstring_clear(sb);
      sge_dstring_vsprintf(nullptr, "test %s", args);
      sge_dstring_vsprintf(sb, nullptr, args);
      sge_dstring_vsprintf(sb, "test %s", args);
      check_dstring(sb);
   }
#endif

   /* sge_dstring_sprintf_append */
   printf("\nchecking sge_dstring_sprintf_append\n");
   sge_dstring_clear(sb);
   sge_dstring_sprintf_append(nullptr, "test %s", "string");
   sge_dstring_sprintf_append(sb, nullptr);
   sge_dstring_sprintf_append(sb, "test %s", "string");
   sge_dstring_sprintf_append(sb, " appended test %s", "string");
   check_dstring(sb);

   /* sge_dstring_clear */
   printf("\nchecking sge_dstring_clear\n");
   sge_dstring_clear(nullptr);
   sge_dstring_clear(sb);
   check_dstring(sb);

   /* sge_dstring_free */
   printf("\nchecking sge_dstring_free\n");
   sge_dstring_free(nullptr);
   sge_dstring_free(sb);
   check_dstring(sb);

   /* sge_dstring_get_string */
   printf("\nchecking sge_dstring_get_string\n");
   sge_dstring_clear(sb);
   sge_dstring_append(sb, "test string");
   {
      const char *result;

      result = sge_dstring_get_string(nullptr);
      printf("sge_dstring_get_string(nullptr) = %s\n",
             result == nullptr ? "nullptr" : result);
      result = sge_dstring_get_string(sb);
      printf("sge_dstring_get_string(sb) = %s\n",
             result == nullptr ? "nullptr" : result);
   }

   /* sge_dstring_copy_string */
   printf("\nchecking sge_dstring_copy_string\n");
   sge_dstring_copy_string(nullptr, nullptr);
   sge_dstring_copy_string(sb, nullptr);
   sge_dstring_copy_string(nullptr, "new test string");
   sge_dstring_copy_string(sb, "new test string");
   check_dstring(sb);

   /* sge_dstring_copy_dstring 
    * check only nullptr pointer behaviour, it just calls sge_dstring_copy_string
    */
   printf("\nchecking sge_dstring_copy_dstring\n");
   sge_dstring_copy_dstring(nullptr, nullptr);
   sge_dstring_copy_dstring(sb, nullptr);
   check_dstring(sb);

   /* sge_dstring_strlen */
   printf("\nchecking sge_dstring_strlen\n");
   {
      int len;
      sge_dstring_copy_string(sb, "test string");
      len = sge_dstring_strlen(nullptr);
      printf("sge_dstring_strlen(nullptr) = %d\n", len);
      len = sge_dstring_strlen(sb);
      printf("sge_dstring_strlen(sb) = %d\n", len);
   }

   /* sge_dstring_remaining */
   printf("\nchecking sge_dstring_remaining\n");
   {
      int len;
      sge_dstring_copy_string(sb, "test string");
      len = sge_dstring_remaining(nullptr);
      printf("sge_dstring_remaining(nullptr) = %d\n", len);
      len = sge_dstring_remaining(sb);
      printf("sge_dstring_remaining(sb) = %d\n", len);
   }

   return ret;
}

static void test_dstring_performance(dstring *ds, int max, const char *data) {
   int i;
   struct timeval before;
   struct timeval after;
   double time;

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      sge_dstring_sprintf(ds, "%s", data, data);
   }
   gettimeofday(&after, nullptr);

   time = after.tv_usec - before.tv_usec;
   time = after.tv_sec - before.tv_sec + (time / 1000000);

   printf("%d sge_dstring_sprintf took %.2fs\n", max, time);
}

static void test_dstring_performance_static(int max, const char *data) {
   int i;
   struct timeval before;
   struct timeval after;
   double time;

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      dstring ds;
      char ds_buffer[MAX_STRING_SIZE];
      sge_dstring_init(&ds, ds_buffer, sizeof(ds_buffer));
      sge_dstring_sprintf(&ds, "%s/%s", data, data);
   }
   gettimeofday(&after, nullptr);

   time = after.tv_usec - before.tv_usec;
   time = after.tv_sec - before.tv_sec + (time / 1000000);

   printf("%d static dstring creations took %.2fs\n", max, time);
}

static void test_dstring_performance_dynamic(int max, const char *data) {
   int i;
   struct timeval before;
   struct timeval after;
   double time;

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      dstring ds = DSTRING_INIT;
      sge_dstring_sprintf(&ds, "%s/%s", data, data);
      sge_dstring_free(&ds);
   }
   gettimeofday(&after, nullptr);

   time = after.tv_usec - before.tv_usec;
   time = after.tv_sec - before.tv_sec + (time / 1000000);

   printf("%d dstring creations took %.2fs\n", max, time);
}

int main(int argc, char *argv[]) {
   bool ret = true;
   dstring dynamic_dstring = DSTRING_INIT;
   dstring static_dstring;
   char static_buffer[MAX_STRING_SIZE];

   sge_dstring_init(&static_dstring, static_buffer, STATIC_SIZE);

   printf("running all checks with a dynamic dstring\n");
   ret = check_all(&dynamic_dstring);
   test_dstring_performance(&dynamic_dstring, 100000, "test_data");
   test_dstring_performance_dynamic(100000, "test_data");
   printf("%s\n", sge_dstring_get_string(&dynamic_dstring));

   if (ret) {
      printf("\n\nrunning all checks with a static dstring of length %d\n",
             STATIC_SIZE);
      ret = check_all(&static_dstring);
      test_dstring_performance(&static_dstring, 100000, "test_data");
      test_dstring_performance_static(100000, "test_data");
      printf("%s\n", sge_dstring_get_string(&static_dstring));
   }


   sge_dstring_free(&dynamic_dstring);

   return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
