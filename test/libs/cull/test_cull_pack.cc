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

/** @file
 * @brief Unit tests for packing and dumping in the core of the typed object model (libs/tom)
 *
 * The buffers are prepared with the non-throwing fronts of ocs_PackCore.h
 * (try_init_buffer(), try_init_buffer_from(), try_get_str()); elements and
 * lists are written with pack_elem()/pack_list() and read with
 * unpack_raw_elem()/unpack_raw_list(), the dump goes through ocs_DumpCore.h.
 * The test type has no generated traits, so the elements are built with the
 * run-time typed handles of tom/ocs_Raw.h.
 */

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

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/cull_types.h"
#include "tom/ocs_DumpCore.h"
#include "tom/ocs_Host.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_PackBuffer.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "msg_common.h"

#include <sge_log.h>
#include "ocs_test_model.h"

enum {
   TEST_host = 1,   ///< a host name attribute
   TEST_string,   ///< a string attribute
   TEST_double,   ///< a double attribute
   TEST_ulong,   ///< an unsigned long attribute
   TEST_ulong64,   ///< a 64-bit unsigned attribute
   TEST_bool,   ///< a bool attribute
   TEST_list,   ///< a sublist attribute
   TEST_object,   ///< a sub-object attribute
   TEST_ref   ///< a reference attribute
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
LISTEND(TEST_Type)

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

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)   ///< number of attributes of the synthetic type

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
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
 * @brief Whether two elements hold identical field values; reports the differences on stderr
 * @param a the first element, or nullptr
 * @param b the second element, or nullptr; its reference must be nullptr, as unpacking leaves it
 * @return true if both are nullptr or hold the same values, recursively in the object and the list field
 */
static bool compare_objects(const ocs::tom::ConstRawElement a, const ocs::tom::ConstRawElement b) {
   bool ret = true;

   if (!a && !b) {
      return true;
   }
   if (!a) {
      fprintf(stderr, "first object is nullptr\n");
      return false;
   }
   if (!b) {
      fprintf(stderr, "second object is nullptr\n");
      return false;
   }

   const char *ah = a.get<ocs::tom::Host>(TEST_host).c_str();
   const char *bh = b.get<ocs::tom::Host>(TEST_host).c_str();
   if (sge_strnullcmp(ah, bh) != 0) {
      fprintf(stderr, "TEST_host differs after unpacking: %s vs. %s\n", ah, bh);
      ret = false;
   }
   if (sge_strnullcmp(a.get<const char *>(TEST_string), b.get<const char *>(TEST_string)) != 0) {
      fprintf(stderr, "TEST_string differs after unpacking: %s vs. %s\n",
              a.get<const char *>(TEST_string), b.get<const char *>(TEST_string));
      ret = false;
   }
   if (a.get<double>(TEST_double) != b.get<double>(TEST_double)) {
      fprintf(stderr, "TEST_double differs after unpacking: %f vs. %f\n",
              a.get<double>(TEST_double), b.get<double>(TEST_double));
      ret = false;
   }
   if (a.get<std::uint32_t>(TEST_ulong) != b.get<std::uint32_t>(TEST_ulong)) {
      fprintf(stderr, "TEST_ulong differs after unpacking: " sge_u32 " vs. " sge_u32 " \n",
              a.get<std::uint32_t>(TEST_ulong), b.get<std::uint32_t>(TEST_ulong));
      ret = false;
   }
   if (a.get<std::uint64_t>(TEST_ulong64) != b.get<std::uint64_t>(TEST_ulong64)) {
      fprintf(stderr, "TEST_ulong64 differs after unpacking: " sge_u64 " vs. " sge_u64 " \n",
              a.get<std::uint64_t>(TEST_ulong64), b.get<std::uint64_t>(TEST_ulong64));
      ret = false;
   }
   if (a.get<bool>(TEST_bool) != b.get<bool>(TEST_bool)) {
      fprintf(stderr, "TEST_bool differs after unpacking: %d vs. %d\n",
              a.get<bool>(TEST_bool), b.get<bool>(TEST_bool));
      ret = false;
   }
   if (b.get<void *>(TEST_ref) != nullptr) {
      fprintf(stderr, "TEST_ref should be nullptr after unpacking but is: %p\n", b.get<void *>(TEST_ref));
      ret = false;
   }

   if (!compare_objects(a.get<ocs::tom::ConstRawElement>(TEST_object),
                        b.get<ocs::tom::ConstRawElement>(TEST_object))) {
      ret = false;
   }

   const auto al = a.get<ocs::tom::ConstRawList>(TEST_list);
   const auto bl = b.get<ocs::tom::ConstRawList>(TEST_list);
   if (!al && !bl) {
      ;
   } else {
      if (al.size() != bl.size()) {
         fprintf(stderr, "TEST_list have different number of elements\n");
         ret = false;
      } else {
         ocs::tom::ConstRawElement ae = al.front();
         ocs::tom::ConstRawElement be = bl.front();
         while (ae && be) {
            if (!compare_objects(ae, be)) {
               ret = false;
               break;
            }
            ae = ae.next();
            be = be.next();
         }
      }
   }

   return ret;
}

/**
 * @brief Write an element into a buffer, returning the code instead of throwing
 * @param pb the buffer
 * @param ep the element
 * @param flags write only the fields whose flags match, 0 for all
 * @return PACK_SUCCESS or the code of the PackException, as cull_pack_elem_partial() returned it
 */
static int pack_elem_code(sge_pack_buffer *pb, const lListElem *ep, const int flags) {
   try {
      ocs::tom::pack_elem(pb, ep, nullptr, flags);
      return PACK_SUCCESS;
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/**
 * @brief Write a list into a buffer, returning the code instead of throwing
 * @param pb the buffer
 * @param lp the list
 * @return PACK_SUCCESS or the code of the PackException, as cull_pack_list() returned it
 */
static int pack_list_code(sge_pack_buffer *pb, const lList *lp) {
   try {
      ocs::tom::pack_list(pb, lp, nullptr, 0);
      return PACK_SUCCESS;
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/**
 * @brief Read an element from a buffer, returning the code instead of throwing
 * @param pb the buffer
 * @param[out] ep receives the element
 * @param descr the descriptor of the element
 * @param flags read only the fields whose flags match, 0 for all
 * @return PACK_SUCCESS or the code of the PackException, as cull_unpack_elem_partial() returned it
 */
static int unpack_elem_code(sge_pack_buffer *pb, ocs::tom::RawElementPtr &ep, const lDescr *descr, const int flags) {
   try {
      ep = ocs::tom::unpack_raw_elem(pb, descr, flags);
      return PACK_SUCCESS;
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/**
 * @brief Read a list from a buffer, returning the code instead of throwing
 * @param pb the buffer
 * @param[out] lp receives the list
 * @return PACK_SUCCESS or the code of the PackException, as cull_unpack_list() returned it
 */
static int unpack_list_code(sge_pack_buffer *pb, ocs::tom::RawListPtr &lp) {
   try {
      lp = ocs::tom::unpack_raw_list(pb);
      return PACK_SUCCESS;
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/**
 * @brief Prepare a reading buffer on a copy of the bytes written to another one
 * @param from the buffer written to
 * @param[out] to the buffer to read from, owning the copy
 * @return PACK_SUCCESS or the code of try_init_buffer_from()
 */
static int copy_for_reading(const sge_pack_buffer *from, sge_pack_buffer *to) {
   const std::size_t used = ocs::tom::buffer_used(from);
   char *buf = sge_malloc(used);
   SGE_ASSERT(buf != nullptr);
   memcpy(buf, from->head_ptr, used);
   return ocs::tom::try_init_buffer_from(to, buf, static_cast<std::uint32_t>(used), false);
}

/**
 * @brief Whether a call of the dump core succeeds, as the old dump functions returned 0
 * @param call the call
 * @return true if the call threw no ListCoreError
 */
template <typename F>
static bool dump_ok(F &&call) {
   try {
      call();
      return true;
   } catch (const ocs::tom::ListCoreError &) {
      return false;
   }
}

/**
 * @brief Read an element written by the dump, empty on an error, as lUndumpElemFp() gave nullptr
 * @param call the call of the undump core returning the element
 * @return the owner of the element, empty on an error
 */
template <typename F>
static ocs::tom::RawElementPtr undump_or_empty(F &&call) {
   try {
      return ocs::tom::RawElementPtr(call());
   } catch (const ocs::tom::ListCoreError &) {
      return ocs::tom::RawElementPtr();
   }
}

/**
 * @brief Call try_get_str() with cur_ptr sitting @p remaining bytes before an unmapped guard page
 *
 * Any read past the in-bounds bytes faults deterministically (CS-2342). With
 * the fix, the string reader never reads the guard page and returns
 * PACK_FORMAT; without it, case A reads cur_ptr[0] and case B runs strlen()
 * into the guard page and SIGSEGVs.
 *
 * @param remaining in-bounds bytes left at cur_ptr (0 = case A, 1 = case B)
 * @return the try_get_str() return value (PACK_FORMAT expected)
 */
static int unpackstr_at_guard(const size_t remaining) {
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
   const int ret = ocs::tom::try_get_str(&pb, &out);
   if (ret == PACK_SUCCESS) {
      sge_free(&out);
   }
   munmap(region, 2 * ps);
   return ret;
}

/**
 * @brief Run all pack and dump tests
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_cull_pack");
   const char *const filename = "test_cull_pack.txt";
   ocs::tom::RawElementPtr copy;
   sge_pack_buffer pb;
   int pack_ret;
   uint32_t predicted_size = 0;

   component_set_daemonized(true);
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size}});


   // create fully-populated element
   const auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   const auto empty_ep = ocs::tom::RawElementPtr::create(TEST_Type);

   ep->set(TEST_host, ocs::tom::Host("test_host"));
   ep->set(TEST_string, "test_string");
   ep->set(TEST_double, 3.1);
   ep->set(TEST_ulong, std::uint32_t{3});
   ep->set(TEST_ulong64, std::uint64_t{3});
   ep->set(TEST_bool, true);

   {
      auto obj = ocs::tom::RawElementPtr::create(TEST_Type);
      obj->set(TEST_host, ocs::tom::Host("test sub host"));
      obj->set(TEST_string, "test sub string");
      ep->set(TEST_object, std::move(obj));
   }
   const ocs::tom::ConstRawElement obj = ep->get<ocs::tom::ConstRawElement>(TEST_object);

   ep->set(TEST_ref, static_cast<void *>(ep.get().raw()));

   {
      ep->set(TEST_list, ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, ""));
      const ocs::tom::RawList lp = ep->get<ocs::tom::RawList>(TEST_list);
      for (int i = 0; i < 10000; i++) {
         char name[1024];
         auto ep1 = ocs::tom::RawElementPtr::create(TEST_Type);
         snprintf(name, sizeof(name),
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx%d",
                  i);
         ep1->set(TEST_host, ocs::tom::Host(name));
         ep1->set(TEST_string, static_cast<const char *>(name));
         lp.push_back(std::move(ep1));
      }
   }

   // --- count-only pass ---
   printf("\n--- count-only pass ---\n");
   {
      sge_pack_buffer count_pb;
      pack_ret = ocs::tom::try_init_buffer(&count_pb, 100, true, false);
      CHECK(1, "try_init_buffer (count mode) returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = pack_elem_code(&count_pb, ep.get(), 0);
         CHECK(2, "pack_elem (count mode) succeeds", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            // header = 2 × INTSIZE (pad + version) + 1 byte (null auth_info string)
            predicted_size = ocs::tom::buffer_used(&count_pb) + (2 * INTSIZE) + 1;
         }
         ocs::tom::clear_buffer(&count_pb);
      }
   }

   // --- full pack/unpack ---
   printf("\n--- full pack/unpack ---\n");
   {
      pack_ret = ocs::tom::try_init_buffer(&pb, 100, false, false);
      CHECK(3, "try_init_buffer returns PACK_SUCCESS", pack_ret == PACK_SUCCESS);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = pack_elem_code(&pb, ep.get(), 0);
         CHECK(4, "pack_elem succeeds", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            if (predicted_size > 0) {
               CHECK(5, "count-only predicted size matches actual packed size",
                     predicted_size == static_cast<uint32_t>(ocs::tom::buffer_used(&pb)));
            }
            sge_pack_buffer copy_pb;
            if (copy_for_reading(&pb, &copy_pb) == PACK_SUCCESS) {
               pack_ret = unpack_elem_code(&copy_pb, copy, TEST_Type, 0);
               CHECK(6, "full round-trip: all fields identical",
                     pack_ret == PACK_SUCCESS && compare_objects(ep.get(), copy.get()));
               copy.reset();
            }
            ocs::tom::clear_buffer(&copy_pb);
         }
         ocs::tom::clear_buffer(&pb);
      }
   }

   // --- empty element round-trip ---
   printf("\n--- empty element round-trip ---\n");
   {
      pack_ret = ocs::tom::try_init_buffer(&pb, 100, false, false);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = pack_elem_code(&pb, empty_ep.get(), 0);
         if (pack_ret == PACK_SUCCESS) {
            sge_pack_buffer copy_pb;
            if (copy_for_reading(&pb, &copy_pb) == PACK_SUCCESS) {
               pack_ret = unpack_elem_code(&copy_pb, copy, TEST_Type, 0);
               CHECK(7, "all-zero element survives full pack/unpack intact",
                     pack_ret == PACK_SUCCESS && compare_objects(empty_ep.get(), copy.get()));
               copy.reset();
            }
            ocs::tom::clear_buffer(&copy_pb);
         }
         ocs::tom::clear_buffer(&pb);
      }
   }

   // --- partial pack/unpack (CULL_SPOOL fields only) ---
   printf("\n--- partial pack/unpack ---\n");
   {
      pack_ret = ocs::tom::try_init_buffer(&pb, 100, false, false);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = pack_elem_code(&pb, ep.get(), CULL_SPOOL);
         CHECK(8, "pack_elem (CULL_SPOOL) succeeds", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            sge_pack_buffer copy_pb;
            if (copy_for_reading(&pb, &copy_pb) == PACK_SUCCESS) {
               pack_ret = unpack_elem_code(&copy_pb, copy, TEST_Type, CULL_SPOOL);
               CHECK(9, "unpack_raw_elem (CULL_SPOOL) succeeds", pack_ret == PACK_SUCCESS);
               if (pack_ret == PACK_SUCCESS && copy) {
                  CHECK(10, "CULL_SPOOL host field preserved after partial round-trip",
                        sge_strnullcmp(copy->get<ocs::tom::Host>(TEST_host).c_str(), "test_host") == 0);
                  CHECK(11, "non-CULL_SPOOL string field absent (nullptr) after partial round-trip",
                        copy->get<const char *>(TEST_string) == nullptr);
                  CHECK(12, "non-CULL_SPOOL double field absent (0.0) after partial round-trip",
                        copy->get<double>(TEST_double) == 0.0);
                  CHECK(13, "CULL_SPOOL list field preserved (10000 elements) after partial round-trip",
                        static_cast<int>(copy->get<ocs::tom::ConstRawList>(TEST_list).size()) == 10000);
               }
               copy.reset();
            }
            ocs::tom::clear_buffer(&copy_pb);
         }
         ocs::tom::clear_buffer(&pb);
      }
   }

   // --- list pack/unpack ---
   printf("\n--- list pack/unpack ---\n");
   {
      const auto test_list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "test");
      for (int i = 0; i < 3; i++) {
         auto le = ocs::tom::RawElementPtr::create(TEST_Type);
         le->set(TEST_host, ocs::tom::Host("list_host"));
         le->set(TEST_string, "list_string");
         le->set(TEST_ulong, static_cast<std::uint32_t>(i));
         test_list->push_back(std::move(le));
      }

      pack_ret = ocs::tom::try_init_buffer(&pb, 100, false, false);
      if (pack_ret == PACK_SUCCESS) {
         pack_ret = pack_list_code(&pb, test_list.get());
         CHECK(14, "pack_list succeeds", pack_ret == PACK_SUCCESS);
         if (pack_ret == PACK_SUCCESS) {
            sge_pack_buffer copy_pb;
            if (copy_for_reading(&pb, &copy_pb) == PACK_SUCCESS) {
               ocs::tom::RawListPtr unpacked_list;
               pack_ret = unpack_list_code(&copy_pb, unpacked_list);
               CHECK(15, "unpack_raw_list succeeds", pack_ret == PACK_SUCCESS);
               if (pack_ret == PACK_SUCCESS && unpacked_list) {
                  CHECK(16, "list round-trip: element count matches (3)",
                        static_cast<int>(unpacked_list->size()) == 3);
                  CHECK(17, "list round-trip: first element fields identical",
                        compare_objects(test_list->front(), unpacked_list->front()));
               }
            }
            ocs::tom::clear_buffer(&copy_pb);
         }
         ocs::tom::clear_buffer(&pb);
      }
   }

   // --- dump/undump ---
   printf("\n--- dump/undump ---\n");
   {
      CHECK(18, "dump_elem_to succeeds", dump_ok([&] { ocs::tom::dump_elem_to(filename, ep.get(), 1); }));

      FILE *fd = fopen(filename, "r");
      if (fd != nullptr) {
         copy = undump_or_empty([fd] { return ocs::tom::undump_elem(fd, TEST_Type); });
         CHECK(19, "undump_elem returns non-null element", static_cast<bool>(copy));
         if (copy) {
            CHECK(20, "dump/undump round-trip: all fields identical", compare_objects(ep.get(), copy.get()));
            copy.reset();
         }
         if (fclose(fd) != 0) {
            fprintf(stderr, MSG_FILE_ERRORCLOSEINGXY_SS, filename, strerror(errno));
         }
      }
      unlink(filename);
   }

   // --- byte arrays as hex text (CS-2341) ---
   printf("\n--- bytes_to_hex / hex_to_bytes ---\n");
   {
      // valid round-trip: encode known bytes into a string field, decode them back
      const auto be = ocs::tom::RawElementPtr::create(TEST_Type);
      const char bytes[] = {(char)0xDE, (char)0xAD, (char)0xBE, (char)0xEF};
      be->set(TEST_string, ocs::tom::bytes_to_hex(bytes, (int)sizeof(bytes)).c_str());

      char *decoded = nullptr;
      const int n = ocs::tom::hex_to_bytes(be->get<const char *>(TEST_string), &decoded);
      CHECK(21, "hex_to_bytes round-trip returns original size",
            n == (int)sizeof(bytes));
      CHECK(22, "hex_to_bytes round-trip yields original bytes",
            decoded != nullptr && memcmp(decoded, bytes, sizeof(bytes)) == 0);
      sge_free(&decoded);

      // CHECK 23 removed: hex_to_bytes(), the counterpart of getByteArray, takes no null text (CS-3103)
   }

   // --- string reader boundary (CS-2342: OOB read at buffer end) ---
   printf("\n--- try_get_str boundary ---\n");
   {
      // valid in-bounds string still round-trips
      char vbuf[] = "hello";
      sge_pack_buffer pb{};
      pb.head_ptr = pb.cur_ptr = vbuf;
      pb.mem_size = sizeof(vbuf);   // includes the terminating NUL
      pb.bytes_used = 0;
      char *out = nullptr;
      const int ret = ocs::tom::try_get_str(&pb, &out);
      CHECK(24, "try_get_str valid string round-trips",
            ret == PACK_SUCCESS && out != nullptr && strcmp(out, "hello") == 0);
      sge_free(&out);

      // case A: 0 bytes remaining -> must reject without reading cur_ptr[0]
      CHECK(25, "try_get_str with 0 bytes left returns PACK_FORMAT (no OOB read)",
            unpackstr_at_guard(0) == PACK_FORMAT);

      // case B: 1 non-NUL byte, no in-bounds terminator -> must reject without strlen OOB
      CHECK(26, "try_get_str without in-bounds NUL returns PACK_FORMAT (no OOB strlen)",
            unpackstr_at_guard(1) == PACK_FORMAT);
   }

   // --- dump/undump by file name and of whole lists ---
   printf("\n--- dump/undump of lists ---\n");
   {
      CHECK(27, "undump_elem_from reads back by file name what dump_elem_to wrote",
            dump_ok([&] { ocs::tom::dump_elem_to(filename, obj, 0); }) &&
            (copy = undump_or_empty([&] { return ocs::tom::undump_elem_from(filename, TEST_Type); })) &&
            compare_objects(obj, copy.get()));
      copy.reset();
      unlink(filename);

      const auto dump_list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "dump");
      for (int i = 0; i < 3; i++) {
         auto le = obj.clone();
         le->set(TEST_ulong, static_cast<std::uint32_t>(i));
         dump_list->push_back(std::move(le));
      }
      ocs::tom::RawListPtr undumped;
      FILE *fd = fopen(filename, "w");
      if (fd != nullptr) {
         CHECK(28, "dump_list succeeds", dump_ok([&] { ocs::tom::dump_list(fd, dump_list.get(), 0); }));
         fclose(fd);
         fd = fopen(filename, "r");
         if (fd != nullptr) {
            try {
               undumped = ocs::tom::RawListPtr(ocs::tom::undump_list(fd, nullptr, TEST_Type));
            } catch (const ocs::tom::ListCoreError &) {
               // checked below: no list
            }
            fclose(fd);
         }
      }
      CHECK(29, "undump_list returns a list with every element",
            undumped && undumped->size() == 3);
      bool same = static_cast<bool>(undumped);
      ocs::tom::ConstRawElement b = same ? undumped->front() : ocs::tom::ConstRawElement();
      for (ocs::tom::ConstRawElement a = dump_list->front(); same && a; a = a.next(), b = b.next()) {
         same = compare_objects(a, b);
      }
      CHECK(30, "dump/undump of a list: all fields identical, in order", same);
      unlink(filename);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
