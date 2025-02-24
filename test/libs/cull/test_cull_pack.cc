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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"

#include "uti/ocs_Munge.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"

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

int main(int argc, char *argv[]) {
   const char *const filename = "test_cull_pack.txt";
   lListElem *ep, *obj, *copy;
   sge_pack_buffer pb, copy_pb;
   int pack_ret;
   FILE *fd;
   char *buffer;
   u_long32 counted_size;

   lInit(nmv);

   if (bootstrap_get_use_munge()) {
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
      printf("just_count does not work, reported " sge_u32", expected " sge_u32"\n", counted_size,
             (u_long32) pb.bytes_used);
      return EXIT_FAILURE;
   }
   printf("element uses " sge_u32" kb, mem_size is " sge_u32" kb\n", (u_long32) pb.bytes_used / 1024,
          (u_long32) pb.mem_size / 1024);

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


