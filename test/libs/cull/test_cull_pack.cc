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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>

/* MAP_ANONYMOUS is the Linux spelling; classic BSD/Darwin use MAP_ANON.
 * FreeBSD/Solaris/macOS define MAP_ANON, so fall back to it when needed. */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "msg_common.h"

#include <sge_log.h>

enum {
   TEST_host = 1,
   TEST_string,
   TEST_double,
   TEST_ulong,
   TEST_ulong64,
   TEST_bool,
   TEST_list,
   TEST_object,
   TEST_ref
};

LISTDEF(TEST_Type)
                SGE_HOST   (TEST_host, CULL_DEFAULT | CULL_SPOOL)
                SGE_STRING (TEST_string, CULL_DEFAULT)
                SGE_DOUBLE (TEST_double, CULL_DEFAULT)
                SGE_ULONG  (TEST_ulong, CULL_DEFAULT)
                SGE_ULONG64 (TEST_ulong64, CULL_DEFAULT)
                SGE_BOOL   (TEST_bool, CULL_DEFAULT)
                SGE_LIST   (TEST_list, TEST_Type, CULL_DEFAULT | CULL_SPOOL)
                SGE_OBJECT (TEST_object, TEST_Type, CULL_DEFAULT | CULL_SPOOL)
                SGE_REF    (TEST_ref, TEST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(TEST_Name)
                NAME("TEST_host")
                NAME("TEST_string")
                NAME("TEST_double")
                NAME("TEST_ulong")
                NAME("TEST_ulong64")
                NAME("TEST_bool")
                NAME("TEST_list")
                NAME("TEST_object")
                NAME("TEST_ref")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)

lNameSpace nmv[] = {
        {1, TEST_Size, TEST_Name, TEST_Type},
        {0, 0, nullptr, nullptr}
};

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

// Returns true when a and b contain identical field values; reports diffs to stderr.
static bool compare_objects(const lListElem *a, const lListElem *b) {
   bool ret = true;

   if (a == nullptr && b == nullptr) {
      return true;
   }
   if (a == nullptr) {
      fprintf(stderr, "first object is nullptr\n");
      return false;
   }
   if (b == nullptr) {
      fprintf(stderr, "second object is nullptr\n");
      return false;
   }

   if (sge_strnullcmp(lGetHost(a, TEST_host), lGetHost(b, TEST_host)) != 0) {
      fprintf(stderr, "TEST_host differs after unpacking: %s vs. %s\n",
              lGetHost(a, TEST_host), lGetHost(b, TEST_host));
      ret = false;
   }
   if (sge_strnullcmp(lGetString(a, TEST_string), lGetString(b, TEST_string)) != 0) {
      fprintf(stderr, "TEST_string differs after unpacking: %s vs. %s\n",
              lGetString(a, TEST_string), lGetString(b, TEST_string));
      ret = false;
   }
   if (lGetDouble(a, TEST_double) != lGetDouble(b, TEST_double)) {
      fprintf(stderr, "TEST_double differs after unpacking: %f vs. %f\n",
              lGetDouble(a, TEST_double), lGetDouble(b, TEST_double));
      ret = false;
   }
   if (lGetUlong(a, TEST_ulong) != lGetUlong(b, TEST_ulong)) {
      fprintf(stderr, "TEST_ulong differs after unpacking: " sge_u32 " vs. " sge_u32 " \n",
              lGetUlong(a, TEST_ulong), lGetUlong(b, TEST_ulong));
      ret = false;
   }
   if (lGetUlong64(a, TEST_ulong64) != lGetUlong64(b, TEST_ulong64)) {
      fprintf(stderr, "TEST_ulong64 differs after unpacking: " sge_u64 " vs. " sge_u64 " \n",
              lGetUlong64(a, TEST_ulong64), lGetUlong64(b, TEST_ulong64));
      ret = false;
   }
   if (lGetBool(a, TEST_bool) != lGetBool(b, TEST_bool)) {
      fprintf(stderr, "TEST_bool differs after unpacking: %d vs. %d\n",
              lGetBool(a, TEST_bool), lGetBool(b, TEST_bool));
      ret = false;
   }
   if (lGetRef(b, TEST_ref) != nullptr) {
      fprintf(stderr, "TEST_ref should be nullptr after unpacking but is: %p\n",
              lGetRef(b, TEST_ref));
      ret = false;
   }

   if (!compare_objects(lGetObject(a, TEST_object), lGetObject(b, TEST_object))) {
      ret = false;
   }

   const lList *al = lGetList(a, TEST_list);
   const lList *bl = lGetList(b, TEST_list);
   if (al == nullptr && bl == nullptr) {
      ;
   } else {
      if (lGetNumberOfElem(al) != lGetNumberOfElem(bl)) {
         fprintf(stderr, "TEST_list have different number of elements\n");
         ret = false;
      } else {
         const lListElem *ae = lFirst(al), *be = lFirst(bl);
         while (ae != nullptr && be != nullptr) {
            if (!compare_objects(ae, be)) {
               ret = false;
               break;
            }
            ae = lNext(ae);
            be = lNext(be);
         }
      }
   }

   return ret;
}

/* @brief Call unpackstr() with cur_ptr sitting @p remaining bytes before an
 * unmapped guard page, so any read past the in-bounds bytes faults
 * deterministically (CS-2342). With the fix, unpackstr never reads the guard
 * page and returns PACK_FORMAT; without it, case A reads cur_ptr[0] and case B
 * runs strlen() into the guard page and SIGSEGVs.
 *
 * @param remaining  in-bounds bytes left at cur_ptr (0 = case A, 1 = case B)
 * @return the unpackstr() return value (PACK_FORMAT expected)
 */
static int unpackstr_at_guard(size_t remaining) {
   const long ps = sysconf(_SC_PAGESIZE);
   char *region = static_cast<char *>(mmap(nullptr, 2 * ps, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
   if (region == MAP_FAILED) {
      return PACK_ENOMEM;
   }
   // second page is the guard: any access faults
   mprotect(region + ps, ps, PROT_NONE);

   char *const boundary = region + ps;
   const size_t L = (remaining == 0) ? 8 : remaining;  // in-bounds content size
   char *const buf_start = boundary - L;
   memset(buf_start, 0x41, L);                          // all non-NUL -> no in-bounds terminator

   sge_pack_buffer pb{};
   pb.head_ptr = buf_start;
   pb.mem_size = L;
   pb.bytes_used = L - remaining;          // remaining = mem_size - bytes_used
   pb.cur_ptr = buf_start + pb.bytes_used; // sits 'remaining' bytes before the guard page

   char *out = nullptr;
   const int ret = unpackstr(&pb, &out);
   if (ret == PACK_SUCCESS) {
      sge_free(&out);
   }
   munmap(region, 2 * ps);
   return ret;
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_cull_pack");
   const char *const filename = "test_cull_pack.txt";
   lListElem *ep = nullptr, *obj = nullptr, *empty_ep = nullptr;
   lListElem *copy = nullptr;
   sge_pack_buffer pb;
   int pack_ret;
   uint32_t predicted_size = 0;

   component_set_daemonized(true);
   lInit(nmv);


   // create fully-populated element
   ep       = lCreateElem(TEST_Type);
   obj      = lCreateElem(TEST_Type);
   empty_ep = lCreateElem(TEST_Type);

   lSetHost(ep, TEST_host, "test_host");
   lSetString(ep, TEST_string, "test_string");
   lSetDouble(ep, TEST_double, 3.1);
   lSetUlong(ep, TEST_ulong, 3);
   lSetUlong64(ep, TEST_ulong64, 3);
   lSetBool(ep, TEST_bool, true);

   lSetHost(obj, TEST_host, "test sub host");
   lSetString(obj, TEST_string, "test sub string");
   lSetObject(ep, TEST_object, obj);

   lSetRef(ep, TEST_ref, ep);

   {
      lList *lp = lCreateList("", TEST_Type);
      lSetList(ep, TEST_list, lp);
      for (int i = 0; i < 10000; i++) {
         char name[1024];
         lListElem *ep1 = lCreateElem(TEST_Type);
         snprintf(name, sizeof(name),
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx%d",
                  i);
         lSetHost(ep1, TEST_host, name);
         lSetString(ep1, TEST_string, name);
         lAppendElem(lp, ep1);
      }
   }

   // --- count-only pass ---
   printf("\n--- count-only pass ---\n");
   {
      sge_pack_buffer count_pb;
      pack_ret = init_packbuffer(&count_pb, 100, true, false);
      CHECK(1, "init_packbuffer (count mode) returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = cull_pack_elem(&count_pb, ep);
         CHECK(2, "cull_pack_elem (count mode) returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            // header = 2 × INTSIZE (pad + version) + 1 byte (null auth_info string)
            predicted_size = count_pb.bytes_used + (2 * INTSIZE) + 1;
         }
         clear_packbuffer(&count_pb);
      }
   }

   // --- full pack/unpack ---
   printf("\n--- full pack/unpack ---\n");
   {
      pack_ret = init_packbuffer(&pb, 100, false, false);
      CHECK(3, "init_packbuffer returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = cull_pack_elem(&pb, ep);
         CHECK(4, "cull_pack_elem returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            if (predicted_size > 0) {
               CHECK(5, "count-only predicted size matches actual packed size",
                     predicted_size == static_cast<uint32_t>(pb.bytes_used));
            }
            char *buf = sge_malloc(pb.bytes_used);
            SGE_ASSERT(buf != nullptr);
            memcpy(buf, pb.head_ptr, pb.bytes_used);
            sge_pack_buffer copy_pb;
            if (init_packbuffer_from_buffer(&copy_pb, buf, pb.bytes_used, false) == PACK_SUCCESS) {
               pack_ret = cull_unpack_elem(&copy_pb, &copy, TEST_Type);
               CHECK(6, "full round-trip: all fields identical",
                     pack_ret == PACK_SUCCESS && compare_objects(ep, copy));
               lFreeElem(&copy);
               clear_packbuffer(&copy_pb);
            }
         }
         clear_packbuffer(&pb);
      }
   }

   // --- empty element round-trip ---
   printf("\n--- empty element round-trip ---\n");
   {
      pack_ret = init_packbuffer(&pb, 100, false, false);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = cull_pack_elem(&pb, empty_ep);
         if (pack_ret == PACK_SUCCESS) {
            char *buf = sge_malloc(pb.bytes_used);
            SGE_ASSERT(buf != nullptr);
            memcpy(buf, pb.head_ptr, pb.bytes_used);
            sge_pack_buffer copy_pb;
            if (init_packbuffer_from_buffer(&copy_pb, buf, pb.bytes_used, false) == PACK_SUCCESS) {
               pack_ret = cull_unpack_elem(&copy_pb, &copy, TEST_Type);
               CHECK(7, "all-zero element survives full pack/unpack intact",
                     pack_ret == PACK_SUCCESS && compare_objects(empty_ep, copy));
               lFreeElem(&copy);
               clear_packbuffer(&copy_pb);
            }
         }
         clear_packbuffer(&pb);
      }
   }

   // --- partial pack/unpack (CULL_SPOOL fields only) ---
   printf("\n--- partial pack/unpack ---\n");
   {
      pack_ret = init_packbuffer(&pb, 100, false, false);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = cull_pack_elem_partial(&pb, ep, nullptr, CULL_SPOOL);
         CHECK(8, "cull_pack_elem_partial (CULL_SPOOL) returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            char *buf = sge_malloc(pb.bytes_used);
            SGE_ASSERT(buf != nullptr);
            memcpy(buf, pb.head_ptr, pb.bytes_used);
            sge_pack_buffer copy_pb;
            if (init_packbuffer_from_buffer(&copy_pb, buf, pb.bytes_used, false) == PACK_SUCCESS) {
               pack_ret = cull_unpack_elem_partial(&copy_pb, &copy, TEST_Type, CULL_SPOOL);
               CHECK(9, "cull_unpack_elem_partial (CULL_SPOOL) returns PACK_SUCCESS",
                     pack_ret == PACK_SUCCESS);
               if (pack_ret == PACK_SUCCESS && copy != nullptr) {
                  CHECK(10, "CULL_SPOOL host field preserved after partial round-trip",
                        sge_strnullcmp(lGetHost(copy, TEST_host), "test_host") == 0);
                  CHECK(11, "non-CULL_SPOOL string field absent (nullptr) after partial round-trip",
                        lGetString(copy, TEST_string) == nullptr);
                  CHECK(12, "non-CULL_SPOOL double field absent (0.0) after partial round-trip",
                        lGetDouble(copy, TEST_double) == 0.0);
                  CHECK(13, "CULL_SPOOL list field preserved (10000 elements) after partial round-trip",
                        (int)lGetNumberOfElem(lGetList(copy, TEST_list)) == 10000);
               }
               lFreeElem(&copy);
               clear_packbuffer(&copy_pb);
            }
         }
         clear_packbuffer(&pb);
      }
   }

   // --- list pack/unpack ---
   printf("\n--- list pack/unpack ---\n");
   {
      lList *test_list = lCreateList("test", TEST_Type);
      for (int i = 0; i < 3; i++) {
         lListElem *le = lCreateElem(TEST_Type);
         lSetHost(le, TEST_host, "list_host");
         lSetString(le, TEST_string, "list_string");
         lSetUlong(le, TEST_ulong, static_cast<lUlong>(i));
         lAppendElem(test_list, le);
      }

      pack_ret = init_packbuffer(&pb, 100, false, false);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = cull_pack_list(&pb, test_list);
         CHECK(14, "cull_pack_list returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            char *buf = sge_malloc(pb.bytes_used);
            SGE_ASSERT(buf != nullptr);
            memcpy(buf, pb.head_ptr, pb.bytes_used);
            sge_pack_buffer copy_pb;
            if (init_packbuffer_from_buffer(&copy_pb, buf, pb.bytes_used, false) == PACK_SUCCESS) {
               lList *unpacked_list = nullptr;
               pack_ret = cull_unpack_list(&copy_pb, &unpacked_list);
               CHECK(15, "cull_unpack_list returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
               if (pack_ret == PACK_SUCCESS && unpacked_list != nullptr) {
                  CHECK(16, "list round-trip: element count matches (3)",
                        (int)lGetNumberOfElem(unpacked_list) == 3);
                  CHECK(17, "list round-trip: first element fields identical",
                        compare_objects(lFirst(test_list), lFirst(unpacked_list)));
               }
               lFreeList(&unpacked_list);
               clear_packbuffer(&copy_pb);
            }
         }
         clear_packbuffer(&pb);
      }
      lFreeList(&test_list);
   }

   // --- dump/undump ---
   printf("\n--- dump/undump ---\n");
   {
      CHECK(18, "lDumpElem returns 0", lDumpElem(filename, ep, 1) == 0);

      FILE *fd = fopen(filename, "r");
      if (fd != nullptr) {
         copy = lUndumpElemFp(fd, TEST_Type);
         CHECK(19, "lUndumpElemFp returns non-null element", copy != nullptr);
         if (copy != nullptr) {
            CHECK(20, "dump/undump round-trip: all fields identical", compare_objects(ep, copy));
            lFreeElem(&copy);
         }
         if (fclose(fd) != 0) {
            fprintf(stderr, MSG_FILE_ERRORCLOSEINGXY_SS, filename, strerror(errno));
         }
      }
      unlink(filename);
   }

   // --- getByteArray (CS-2341: NULL-deref on unset string attribute) ---
   printf("\n--- getByteArray ---\n");
   {
      // valid round-trip: encode known bytes, decode them back
      lListElem *be = lCreateElem(TEST_Type);
      const char bytes[] = {(char)0xDE, (char)0xAD, (char)0xBE, (char)0xEF};
      setByteArray(bytes, (int)sizeof(bytes), be, TEST_string);

      char *decoded = nullptr;
      int n = getByteArray(&decoded, be, TEST_string);
      CHECK(21, "getByteArray round-trip returns original size",
            n == (int)sizeof(bytes));
      CHECK(22, "getByteArray round-trip yields original bytes",
            decoded != nullptr && memcmp(decoded, bytes, sizeof(bytes)) == 0);
      sge_free(&decoded);
      lFreeElem(&be);

      // regression: unset string attribute must not crash (was strlen(NULL))
      char *out = nullptr;
      int n_null = getByteArray(&out, empty_ep, TEST_string);
      CHECK(23, "getByteArray on unset string returns 0 (no NULL-deref)",
            n_null == 0);
      sge_free(&out);
   }

   // --- unpackstr boundary (CS-2342: OOB read at buffer end) ---
   printf("\n--- unpackstr boundary ---\n");
   {
      // valid in-bounds string still round-trips
      char vbuf[] = "hello";
      sge_pack_buffer pb{};
      pb.head_ptr = pb.cur_ptr = vbuf;
      pb.mem_size = sizeof(vbuf);   // includes the terminating NUL
      pb.bytes_used = 0;
      char *out = nullptr;
      int ret = unpackstr(&pb, &out);
      CHECK(24, "unpackstr valid string round-trips",
            ret == PACK_SUCCESS && out != nullptr && strcmp(out, "hello") == 0);
      sge_free(&out);

      // case A: 0 bytes remaining -> must reject without reading cur_ptr[0]
      CHECK(25, "unpackstr with 0 bytes left returns PACK_FORMAT (no OOB read)",
            unpackstr_at_guard(0) == PACK_FORMAT);

      // case B: 1 non-NUL byte, no in-bounds terminator -> must reject without strlen OOB
      CHECK(26, "unpackstr without in-bounds NUL returns PACK_FORMAT (no OOB strlen)",
            unpackstr_at_guard(1) == PACK_FORMAT);
   }

   lFreeElem(&ep);
   lFreeElem(&empty_ep);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
