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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
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

#include "uti/ocs_Munge.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"

#include "msg_common.h"

#include <sge_log.h>

#include "ocs_Bootstrap.h"

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

static bool compare_objects(const lListElem *a, const lListElem *b) {
   bool ret = true;

   if (a == nullptr && b == nullptr) {
      return true;
   }

   if (a == nullptr) {
      fprintf(stderr, "first object is nullptr\n");
      ret = false;
   }

   if (a == nullptr) {
      fprintf(stderr, "second object is nullptr\n");
      ret = false;
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
   const lListElem *ao = lGetObject(a, TEST_object);
   const lListElem *bo = lGetObject(b, TEST_object);
   if (!compare_objects(ao, bo)) {
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

/* Call unpackstr() with cur_ptr sitting 'remaining' bytes before an unmapped
 * guard page, so any read past the in-bounds bytes faults deterministically
 * (CS-2342). With the fix, unpackstr never reads the guard page and returns
 * PACK_FORMAT; without it, case A reads cur_ptr[0] and case B runs strlen()
 * into the guard page and SIGSEGVs.
 * remaining: in-bounds bytes left at cur_ptr (0 = case A, 1 = case B).
 * Returns the unpackstr() return value (PACK_FORMAT expected). */
static int unpackstr_at_guard(size_t remaining) {
   const long ps = sysconf(_SC_PAGESIZE);
   char *region = static_cast<char *>(mmap(nullptr, 2 * ps, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
   if (region == MAP_FAILED) {
      return PACK_ENOMEM;
   }
   /* second page is the guard: any access faults */
   mprotect(region + ps, ps, PROT_NONE);

   char *const boundary = region + ps;
   const size_t L = (remaining == 0) ? 8 : remaining;  /* in-bounds content size */
   char *const buf_start = boundary - L;
   memset(buf_start, 0x41, L);                          /* all non-NUL -> no in-bounds terminator */

   sge_pack_buffer pb{};
   pb.head_ptr = buf_start;
   pb.mem_size = L;
   pb.bytes_used = L - remaining;          /* remaining = mem_size - bytes_used */
   pb.cur_ptr = buf_start + pb.bytes_used; /* sits 'remaining' bytes before the guard page */

   char *out = nullptr;
   const int ret = unpackstr(&pb, &out);
   if (ret == PACK_SUCCESS) {
      sge_free(&out);
   }
   munmap(region, 2 * ps);
   return ret;
}

int main(int argc, char *argv[]) {
   const char *const filename = "test_cull_pack.txt";
   lListElem *ep, *obj, *copy;
   sge_pack_buffer pb, copy_pb;
   int pack_ret;
   FILE *fd;
   char *buffer;
   u_long32 counted_size;

   lInit(nmv);

   if (ocs::Bootstrap::has_security_mode(ocs::Bootstrap::BS_SEC_MODE_MUNGE)) {
#if defined (OCS_WITH_MUNGE)
      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
      if (!ocs::uti::Munge::initialize(&error_dstr)) {
         fprintf(stderr, "initializing Munge failed: %s\n", sge_dstring_get_string(&error_dstr));
         return EXIT_FAILURE;
      }
#else
      fprintf(stderr, SFNMAX, "built without Munge\n");
      return EXIT_FAILURE;
#endif
   }
   /* create an element */
   ep = lCreateElem(TEST_Type);
   obj = lCreateElem(TEST_Type);

   /* test field access functions */
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

   /* fill the sublist */
   {
      lList *lp;
      int i;
      lp = lCreateList("", TEST_Type);
      lSetList(ep, TEST_list, lp);
      for (i = 0; i < 10000; i++) {
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
#if 0
   printf("element after setting fields\n");
   lWriteElemTo(ep, stdout);
#endif

   /* test just count */
   if ((pack_ret = init_packbuffer(&pb, 100, true, false)) != PACK_SUCCESS) {
      printf("initializing packbuffer failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   if ((pack_ret = cull_pack_elem(&pb, ep)) != PACK_SUCCESS) {
      printf("packing element failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }
   counted_size = pb.bytes_used + (2 * INTSIZE);
   clear_packbuffer(&pb);

   /* test packing */
   if ((pack_ret = init_packbuffer(&pb, 100)) != PACK_SUCCESS) {
      printf("initializing packbuffer failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   if ((pack_ret = cull_pack_elem(&pb, ep)) != PACK_SUCCESS) {
      printf("packing element failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   counted_size += strlen(pb.auth_info) + 1;
   if (counted_size != pb.bytes_used) {
      printf("just_count does not work, reported " sge_u32 ", expected " sge_u32"\n",
             counted_size, static_cast<u_long32>(pb.bytes_used));
      return EXIT_FAILURE;
   }
   printf("element uses " sge_u32 " kb, mem_size is " sge_u32" kb\n",
          static_cast<u_long32>(pb.bytes_used / 1024), static_cast<u_long32>(pb.mem_size / 1024));

   buffer = sge_malloc(pb.bytes_used);
   SGE_ASSERT(buffer != nullptr);
   memcpy(buffer, pb.head_ptr, pb.bytes_used);
   if ((pack_ret = init_packbuffer_from_buffer(&copy_pb, buffer, pb.bytes_used)) != PACK_SUCCESS) {
      printf("initializing packbuffer from packed data failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   if ((pack_ret = cull_unpack_elem(&copy_pb, &copy, TEST_Type)) != PACK_SUCCESS) {
      printf("unpacking element failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }
   if (!compare_objects(ep, copy)) {
      return EXIT_FAILURE;
   }
   clear_packbuffer(&pb);
   clear_packbuffer(&copy_pb);

#if 0
   printf("element after packing and unpacking\n");
   lWriteElemTo(copy, stdout);
#endif
   lFreeElem(&copy);

   /* test partial packing */
   if ((pack_ret = init_packbuffer(&pb, 100)) != PACK_SUCCESS) {
      printf("initializing packbuffer failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   if ((pack_ret = cull_pack_elem_partial(&pb, ep, nullptr, CULL_SPOOL)) != PACK_SUCCESS) {
      printf("partially packing element failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   buffer = sge_malloc(pb.bytes_used);
   SGE_ASSERT(buffer != nullptr);
   memcpy(buffer, pb.head_ptr, pb.bytes_used);
   if ((pack_ret = init_packbuffer_from_buffer(&copy_pb, buffer, pb.bytes_used)) != PACK_SUCCESS) {
      printf("initializing packbuffer from partially packed data failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }

   if ((pack_ret = cull_unpack_elem_partial(&copy_pb, &copy, TEST_Type, CULL_SPOOL)) != PACK_SUCCESS) {
      printf("partially unpacking element failed: %s\n", cull_pack_strerror(pack_ret));
      return EXIT_FAILURE;
   }
   clear_packbuffer(&pb);
   clear_packbuffer(&copy_pb);

#if 0
   printf("element after partial packing and unpacking\n");
   lWriteElemTo(copy, stdout);
#endif
   lFreeElem(&copy);

   /* test lDump functions */
   if (lDumpElem(filename, ep, 1)) {
      printf("error dumping element\n");
      return EXIT_FAILURE;
   }

   if ((fd = fopen(filename, "r")) == nullptr) {
      printf("error opening dump file test_cull_pack.txt\n");
      return EXIT_FAILURE;
   }

   if ((copy = lUndumpElemFp(fd, TEST_Type)) == nullptr) {
      FCLOSE(fd);
      unlink(filename);
      printf("error undumping element\n");
      return EXIT_FAILURE;
   }
   FCLOSE(fd);
#if 0
   printf("element after dumping and undumping\n");
   lWriteElemTo(copy, stdout);
#endif
   lFreeElem(&copy);
   unlink(filename);

   /* getByteArray (CS-2341: NULL-deref on unset PACK_string attribute) */
   {
      /* valid round-trip: encode known bytes, decode them back */
      lListElem *be = lCreateElem(TEST_Type);
      const char bytes[] = {(char)0xDE, (char)0xAD, (char)0xBE, (char)0xEF};
      char *decoded = nullptr;
      int n;

      setByteArray(bytes, (int)sizeof(bytes), be, TEST_string);
      n = getByteArray(&decoded, be, TEST_string);
      if (n != (int)sizeof(bytes) || decoded == nullptr ||
          memcmp(decoded, bytes, sizeof(bytes)) != 0) {
         fprintf(stderr, "getByteArray round-trip failed: size %d, decoded %p\n",
                 n, (void *)decoded);
         return EXIT_FAILURE;
      }
      sge_free(&decoded);
      lFreeElem(&be);

      /* regression: unset string attribute must return 0, not strlen(NULL) */
      lListElem *empty_ep = lCreateElem(TEST_Type);
      char *out = nullptr;
      if (getByteArray(&out, empty_ep, TEST_string) != 0) {
         fprintf(stderr, "getByteArray on unset string did not return 0 (NULL-deref regression)\n");
         return EXIT_FAILURE;
      }
      sge_free(&out);
      lFreeElem(&empty_ep);
   }

   /* unpackstr boundary (CS-2342: OOB read at buffer end) */
   {
      /* valid in-bounds string still round-trips */
      char vbuf[] = "hello";
      sge_pack_buffer spb{};
      spb.head_ptr = spb.cur_ptr = vbuf;
      spb.mem_size = sizeof(vbuf);   /* includes the terminating NUL */
      spb.bytes_used = 0;
      char *sout = nullptr;
      if (unpackstr(&spb, &sout) != PACK_SUCCESS || sout == nullptr ||
          strcmp(sout, "hello") != 0) {
         fprintf(stderr, "unpackstr valid string did not round-trip\n");
         return EXIT_FAILURE;
      }
      sge_free(&sout);

      /* case A: 0 bytes remaining -> must reject without reading cur_ptr[0] */
      if (unpackstr_at_guard(0) != PACK_FORMAT) {
         fprintf(stderr, "unpackstr with 0 bytes left did not return PACK_FORMAT (OOB read)\n");
         return EXIT_FAILURE;
      }

      /* case B: 1 non-NUL byte, no in-bounds terminator -> must reject without strlen OOB */
      if (unpackstr_at_guard(1) != PACK_FORMAT) {
         fprintf(stderr, "unpackstr without in-bounds NUL did not return PACK_FORMAT (OOB strlen)\n");
         return EXIT_FAILURE;
      }
   }

   /* cleanup and exit */
#if defined(OCS_WITH_MUNGE)
   ocs::uti::Munge::shutdown();
#endif
   lFreeElem(&ep);
   return EXIT_SUCCESS;
   FCLOSE_ERROR:
   printf(MSG_FILE_ERRORCLOSEINGXY_SS, filename, strerror(errno));
   return EXIT_FAILURE;
}


