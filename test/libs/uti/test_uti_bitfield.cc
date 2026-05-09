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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#define XMALLINFO

#include <cstdio>
#include <unistd.h>
#include <ctime>
#include <sys/times.h>

#ifdef MALLINFO
#include <malloc.h>
#endif

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

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

const unsigned int test_bf_max_size = 100;
unsigned int test_bf_loops = 1000000;

static void test_null_safety(int *id) {
   printf("\n--- null-pointer safety ---\n");

   // T01: zero-size bitfield is a valid object
   bitfield *b0 = sge_bitfield_new(0);
   CHECK(*id, "sge_bitfield_new(0) returns non-null", b0 != nullptr); (*id)++;
   b0 = sge_bitfield_free(b0);

   // T02: copy to a differently-sized target is rejected
   bitfield *src = sge_bitfield_new(20);
   bitfield *dst = sge_bitfield_new(30);
   CHECK(*id, "sge_bitfield_copy to different-size target returns false",
         !sge_bitfield_copy(src, dst)); (*id)++;
   dst = sge_bitfield_free(dst);

   // null-pointer calls must not crash (implicit — program continues if they don't)
   sge_bitfield_free(nullptr);
   sge_bitfield_free_data(nullptr);
   sge_bitfield_copy(src, nullptr);
   sge_bitfield_copy(nullptr, src);
   sge_bitfield_copy(nullptr, nullptr);
   sge_bitfield_bitwise_copy(src, nullptr);
   sge_bitfield_bitwise_copy(nullptr, src);
   sge_bitfield_bitwise_copy(nullptr, nullptr);
   sge_bitfield_set(nullptr, 0);
   sge_bitfield_get(nullptr, 0);
   sge_bitfield_clear(nullptr, 0);
   sge_bitfield_reset(nullptr);
   sge_bitfield_changed(nullptr);
   sge_bitfield_print(nullptr, stdout);
   sge_bitfield_print(src, nullptr);

   src = sge_bitfield_free(src);
}

static void test_bit_operations(int *id) {
   printf("\n--- bit set/get/clear/reset/changed (20-bit fixed bitfield) ---\n");

   bitfield *bf = sge_bitfield_new(20);

   // T03-T04: fresh bitfield
   CHECK(*id, "fresh bitfield: bit 0 is clear",   !sge_bitfield_get(bf, 0));  (*id)++;
   CHECK(*id, "fresh bitfield: changed is false",  !sge_bitfield_changed(bf)); (*id)++;

   // T05-T07: set operations
   CHECK(*id, "sge_bitfield_set(0) returns true",  sge_bitfield_set(bf, 0));   (*id)++;
   CHECK(*id, "after set(0): get(0) returns true", sge_bitfield_get(bf, 0));   (*id)++;
   sge_bitfield_set(bf, 10);
   CHECK(*id, "after set(10): get(10) returns true", sge_bitfield_get(bf, 10)); (*id)++;
   CHECK(*id, "bit 1 (never set) returns false",   !sge_bitfield_get(bf, 1));  (*id)++;

   // T09: changed flag
   CHECK(*id, "changed is true after set",         sge_bitfield_changed(bf));  (*id)++;

   // T10: clear
   sge_bitfield_clear(bf, 0);
   CHECK(*id, "after clear(0): get(0) returns false", !sge_bitfield_get(bf, 0)); (*id)++;

   // T11-T12: reset
   sge_bitfield_reset(bf);
   CHECK(*id, "after reset: get(10) returns false", !sge_bitfield_get(bf, 10)); (*id)++;
   CHECK(*id, "after reset: changed is false",       !sge_bitfield_changed(bf)); (*id)++;

   bf = sge_bitfield_free(bf);
}

static void test_copy(int *id) {
   printf("\n--- copy ---\n");

   bitfield *src = sge_bitfield_new(20);
   sge_bitfield_set(src, 5);
   sge_bitfield_set(src, 15);

   // T13: copy to same-size target succeeds
   bitfield *dst = sge_bitfield_new(20);
   CHECK(*id, "sge_bitfield_copy to same-size target returns true",
         sge_bitfield_copy(src, dst)); (*id)++;

   // T14: copied bits are present in the destination
   CHECK(*id, "after copy: bit 5 is set in destination",
         sge_bitfield_get(dst, 5) && sge_bitfield_get(dst, 15)); (*id)++;
   dst = sge_bitfield_free(dst);

   src = sge_bitfield_free(src);
}

static void test_bitwise_copy(int *id) {
   printf("\n--- bitwise copy ---\n");

   // source: 40-bit bitfield with bit 5 set
   bitfield *src = sge_bitfield_new(40);
   sge_bitfield_set(src, 5);

   // T15: bitwise copy to smaller target succeeds
   bitfield *smaller = sge_bitfield_new(10);
   CHECK(*id, "sge_bitfield_bitwise_copy to smaller target returns true",
         sge_bitfield_bitwise_copy(src, smaller)); (*id)++;
   CHECK(*id, "after bitwise copy to smaller: bit 5 copied correctly",
         sge_bitfield_get(smaller, 5)); (*id)++;
   smaller = sge_bitfield_free(smaller);

   // T17: bitwise copy to same-size target succeeds
   bitfield *same = sge_bitfield_new(40);
   sge_bitfield_bitwise_copy(src, same);
   CHECK(*id, "after bitwise copy to same-size: bit 5 copied correctly",
         sge_bitfield_get(same, 5)); (*id)++;
   same = sge_bitfield_free(same);

   // T18: bitwise copy to larger target succeeds
   bitfield *larger = sge_bitfield_new(80);
   CHECK(*id, "sge_bitfield_bitwise_copy to larger target returns true",
         sge_bitfield_bitwise_copy(src, larger)); (*id)++;
   CHECK(*id, "after bitwise copy to larger: bit 5 copied correctly",
         sge_bitfield_get(larger, 5)); (*id)++;
   larger = sge_bitfield_free(larger);

   src = sge_bitfield_free(src);
}

static void test_word_boundary(int *id) {
   printf("\n--- word-boundary bits (70-bit dynamic bitfield) ---\n");

   // 70 bits: forces dynamic allocation; tests bits straddling the word boundary
   bitfield *bf = sge_bitfield_new(70);

   sge_bitfield_set(bf, 63);
   CHECK(*id, "bit 63 (last bit of first word): set+get works",
         sge_bitfield_get(bf, 63)); (*id)++;

   sge_bitfield_set(bf, 64);
   CHECK(*id, "bit 64 (first bit of second word): set+get works",
         sge_bitfield_get(bf, 64)); (*id)++;

   // bit 62 was never set — must still be clear
   CHECK(*id, "bit 62 (neighbor of boundary, never set) remains clear",
         !sge_bitfield_get(bf, 62)); (*id)++;

   bf = sge_bitfield_free(bf);
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_bitfield");

   int id = 1;

   test_null_safety(&id);
   test_bit_operations(&id);
   test_copy(&id);
   test_bitwise_copy(&id);
   test_word_boundary(&id);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);

   /* evaluate commandline args */
   if (argc > 1) {
      test_bf_loops = atoi(argv[1]);
   }

   /* performance benchmark — not a functional test */
   bitfield *b1 = (bitfield *) sge_malloc(test_bf_loops * sizeof(bitfield));
   SGE_ASSERT(b1 != nullptr);
   bitfield *b2 = (bitfield *) sge_malloc(test_bf_loops * sizeof(bitfield));
   SGE_ASSERT(b2 != nullptr);

   long clk_tck = sysconf(_SC_CLK_TCK);
   const char *header_format = "%5s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s\n";
   const char *data_format = "%5d %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f"
                             " %8.3f %8.3f %8d\n";

#ifdef MALLINFO
   struct mallinfo meminfo;
#endif

   printf("\nperformance benchmark (%d loops per action)\n", test_bf_loops);
   printf(header_format, "size", "create", "free",
          "copy", "bwcopy",
          "set", "get",
          "clear", "reset", "changed", "mem");

   for (unsigned int i = 0; i <= test_bf_max_size; i += 10) {
      struct tms tms_buffer;
      clock_t now, start;
      double prof_create = 0.0;
      double prof_free = 0.0;
      double prof_copy = 0.0;
      double prof_bwcopy = 0.0;
      double prof_set = 0.0;
      double prof_get = 0.0;
      double prof_clear = 0.0;
      double prof_reset = 0.0;
      double prof_changed = 0.0;

      srand(time(0));

      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_init(&b1[loops], i);
         sge_bitfield_init(&b2[loops], i);
      }
      now = times(&tms_buffer);
      prof_create = (now - start) * 1.0 / clk_tck;

      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_copy(&b1[loops], &b2[loops]);
      }
      now = times(&tms_buffer);
      prof_copy = (now - start) * 1.0 / clk_tck;

      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_bitwise_copy(&b2[loops], &b2[loops]);
      }
      now = times(&tms_buffer);
      prof_bwcopy = (now - start) * 1.0 / clk_tck;

      unsigned int index = rand() % (i + 1);
      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_set(&b1[loops], index);
      }
      now = times(&tms_buffer);
      prof_set = (now - start) * 1.0 / clk_tck;

      index = rand() % (i + 1);
      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_get(&b1[loops], index);
      }
      now = times(&tms_buffer);
      prof_get = (now - start) * 1.0 / clk_tck;

      index = rand() % (i + 1);
      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_clear(&b1[loops], index);
      }
      now = times(&tms_buffer);
      prof_clear = (now - start) * 1.0 / clk_tck;

      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_reset(&b1[loops]);
      }
      now = times(&tms_buffer);
      prof_reset = (now - start) * 1.0 / clk_tck;

      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_changed(&b1[loops]);
      }
      now = times(&tms_buffer);
      prof_changed = (now - start) * 1.0 / clk_tck;

#ifdef MALLINFO
      meminfo = mallinfo();
#endif

      start = times(&tms_buffer);
      for (unsigned int loops = 0; loops < test_bf_loops; loops++) {
         sge_bitfield_free_data(&b1[loops]);
         sge_bitfield_free_data(&b2[loops]);
      }
      now = times(&tms_buffer);
      prof_free = (now - start) * 1.0 / clk_tck;

      printf(data_format, i, prof_create, prof_free,
             prof_copy, prof_bwcopy,
             prof_set, prof_get, prof_clear,
             prof_reset, prof_changed,
#ifdef MALLINFO
             (meminfo.usmblks + meminfo.uordblks) / 1024
#else
             0
#endif
      );
   }

   sge_free(&b1);
   sge_free(&b2);
   DRETURN(s_fail == 0 ? 0 : 1);
}
