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
 * @brief Module test: the pack format matches its recorded reference (CS-2995 HTL-03, CS-2997)
 *
 * The test packs a fixed set of elements and lists, in every element state and
 * with a reduced field selection, and compares the buffers byte by byte with
 * buffers recorded by the base revision of the change. It then unpacks the
 * recorded buffers and packs the result again: equal bytes show that this build
 * reads the old buffers into the same states and values. Identical bytes also
 * cover the other direction, because the old build then reads the new buffers
 * as its own.
 *
 * The recorded buffers live in test_tom_core_pack_format_golden.h. They were
 * written by `test_tom_core_pack_format --dump`, run on a build of the base
 * revision, and are never regenerated for a change that keeps the format. Since
 * CS-3103 the test drives the core of the typed object model (pack_elem(),
 * pack_list(), unpack_raw_elem(), unpack_raw_list() and the non-throwing buffer
 * fronts of ocs_PackCore.h); the test types have no generated traits, so the
 * elements are built with the run-time typed handles of tom/ocs_Raw.h. A build
 * for --dump alone is compiled with OCS_PACK_FORMAT_DUMP_ONLY.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

/** @brief Make this translation unit the one that defines the descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Host.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_PackBuffer.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"
#include "ocs_test_model.h"

#ifndef OCS_PACK_FORMAT_DUMP_ONLY
#include "test_tom_core_pack_format_golden.h"
#endif

/** @brief The element type: every field type the pack format supports, hashed keys, a sublist and a sub-object
 *
 * MT_INT64 and MT_INT32 are not packed (PACK_FORMAT), MT_REF is never sent.
 */
enum {
   PF_name = 1, ///< unique hashed string
   PF_host,     ///< host name; not hashed, a host key needs a bootstrap file for the compare
   PF_group,    ///< non unique hashed ulong
   PF_u64,      ///< 64 bit ulong
   PF_dbl,      ///< double
   PF_bool,     ///< boolean
   PF_sub,      ///< a sublist of PFS_Type
   PF_obj       ///< a sub-object of PFS_Type
};

/** @brief The type of the sublist and of the sub-object */
enum {
   PFS_name = 51, ///< a string
   PFS_value      ///< a ulong
};

LISTDEF(PF_Type)
   SGE_STRING(PF_name, MT_DEFAULT | MT_HASH | MT_UNIQUE)
   SGE_HOST(PF_host, MT_DEFAULT)
   SGE_ULONG(PF_group, MT_DEFAULT | MT_HASH)
   SGE_ULONG64(PF_u64, MT_DEFAULT)
   SGE_DOUBLE(PF_dbl, MT_DEFAULT)
   SGE_BOOL(PF_bool, MT_DEFAULT)
   SGE_LIST(PF_sub, PFS_Type, MT_DEFAULT)
   SGE_OBJECT(PF_obj, PFS_Type, MT_DEFAULT)
LISTEND(PF_Type)

LISTDEF(PFS_Type)
   SGE_STRING(PFS_name, MT_DEFAULT)
   SGE_ULONG(PFS_value, MT_DEFAULT)
LISTEND(PFS_Type)

NAMEDEF(PF_Name)
   NAME("PF_name")
   NAME("PF_host")
   NAME("PF_group")
   NAME("PF_u64")
   NAME("PF_dbl")
   NAME("PF_bool")
   NAME("PF_sub")
   NAME("PF_obj")
NAMEEND

NAMEDEF(PFS_Name)
   NAME("PFS_name")
   NAME("PFS_value")
NAMEEND

#define PF_Size sizeof(PF_Name) / sizeof(char *)   ///< number of fields of PF_Type
#define PFS_Size sizeof(PFS_Name) / sizeof(char *) ///< number of fields of PFS_Type

/** @brief A packed buffer */
using bytes = std::vector<uint8_t>;

/**
 * @brief Create an element of PF_Type with fixed values
 *
 * @param name the key; nullptr leaves the string unset
 * @param n a number the other values are derived from
 * @param with_children whether to add a sub-object and two sublist elements
 * @return the free element
 */
static ocs::tom::RawElementPtr make_elem(const char *name, const uint32_t n, const bool with_children) {
   auto ep = ocs::tom::RawElementPtr::create(PF_Type);
   ep->set(PF_name, name);
   ep->set(PF_host, ocs::tom::Host("host-0001.example.com"));
   ep->set(PF_group, n % 2);
   ep->set(PF_u64, (uint64_t{1} << 40) + n);
   ep->set(PF_dbl, 0.25 * n);
   ep->set(PF_bool, n % 2 == 0);
   if (with_children) {
      auto obj = ocs::tom::RawElementPtr::create(PFS_Type);
      obj->set(PFS_name, "object");
      obj->set(PFS_value, n);
      ep->set(PF_obj, std::move(obj));
      // the sublist as lAddSubStr() made it: created by the keyed add of the core, named ""
      ocs::tom::ListData *sub = nullptr;
      static_cast<void>(ocs::tom::add_elem_str(sub, PFS_name, "s1", PFS_Type));
      static_cast<void>(ocs::tom::add_elem_str(sub, PFS_name, "s2", PFS_Type));
      ep->set(PF_sub, ocs::tom::RawListPtr(sub));
   }
   return ep;
}

/**
 * @brief Create the list that is packed: three elements, one with unset fields
 *
 * @return the list, hashed
 */
static ocs::tom::RawListPtr make_list() {
   auto lp = ocs::tom::RawListPtr::create(PF_Type, ocs::tom::Hashing::On, "pack format");
   lp->push_back(make_elem("a", 1, true));
   lp->push_back(make_elem("b", 2, true));
   lp->push_back(make_elem(nullptr, 3, false));
   return lp;
}

/**
 * @brief Run a pack function into a fresh buffer and return its bytes
 *
 * @param pack the pack function; a PackException is a failure
 * @return the packed bytes; empty if packing failed
 */
static bytes pack(const std::function<void(sge_pack_buffer *)> &pack) {
   bytes result;
   sge_pack_buffer pb;
   if (ocs::tom::try_init_buffer(&pb, 1024, false, false) != PACK_SUCCESS) {
      return result;
   }
   try {
      pack(&pb);
      const auto *p = reinterpret_cast<const uint8_t *>(pb.head_ptr);
      result.assign(p, p + ocs::tom::buffer_used(&pb));
   } catch (const ocs::tom::PackException &) {
      // an empty result reports the failure
   }
   ocs::tom::clear_buffer(&pb);
   return result;
}

/**
 * @brief Run an unpack function over a copy of a buffer
 *
 * @param data the buffer
 * @param size its length in bytes
 * @param unpack the unpack function; a PackException is a failure
 * @return true if the buffer was set up and unpack threw nothing
 */
static bool unpack(const uint8_t *data, const size_t size, const std::function<void(sge_pack_buffer *)> &unpack) {
   char *buf = sge_malloc(size);
   memcpy(buf, data, size);
   sge_pack_buffer pb;
   bool ok = ocs::tom::try_init_buffer_from(&pb, buf, static_cast<std::uint32_t>(size), false) == PACK_SUCCESS;
   if (ok) {
      try {
         unpack(&pb);
      } catch (const ocs::tom::PackException &) {
         ok = false;
      }
   }
   ocs::tom::clear_buffer(&pb);
   return ok;
}

/** @brief The buffers of the fixed set, in a fixed order */
struct packed_set {
   bytes free_elem;    ///< a free element
   bytes bound_elem;   ///< an element bound to the list
   bytes object_elem;  ///< the sub-object of the bound element
   bytes trans_elem;   ///< the bound element after a pack and unpack: transitionally bound
   bytes list;         ///< the whole list, descriptor included
   bytes partial_list; ///< the list reduced to three fields
};

/**
 * @brief Pack the fixed set
 *
 * @return the buffers; a buffer is empty where packing failed
 */
static packed_set pack_set() {
   packed_set s;
   const auto lp = make_list();
   const auto free_ep = make_elem("free", 4, true);
   const ocs::tom::ConstRawElement bound = lp->front();
   const ocs::tom::ConstRawElement object = bound.get<ocs::tom::ConstRawElement>(PF_obj);

   s.free_elem = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_elem(pb, free_ep.get(), nullptr, 0); });
   s.bound_elem = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_elem(pb, bound, nullptr, 0); });
   s.object_elem = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_elem(pb, object, nullptr, 0); });

   ocs::tom::RawElementPtr trans;
   if (unpack(s.bound_elem.data(), s.bound_elem.size(),
              [&](sge_pack_buffer *pb) { trans = ocs::tom::unpack_raw_elem(pb, PF_Type); })) {
      s.trans_elem = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_elem(pb, trans.get(), nullptr, 0); });
   }

   s.list = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_list(pb, lp.get(), nullptr, 0); });
   const int fields[] = {PF_name, PF_group, PF_sub};
   const ocs::tom::RawWhatPtr what = ocs::tom::what_of(PF_Type, fields);
   s.partial_list = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_list(pb, lp.get(), what.get(), 0); });

   return s;
}

/**
 * @brief Print one buffer as a C array
 *
 * @param name the name of the array, without the prefix
 * @param b the buffer
 */
static void dump_array(const char *name, const bytes &b) {
   printf("/** @brief Recorded buffer: %s */\n", name);
   printf("static const uint8_t golden_%s[] = {", name);
   for (size_t i = 0; i < b.size(); ++i) {
      printf("%s0x%02x,", i % 12 == 0 ? "\n   " : " ", b[i]);
   }
   printf("\n};\n\n");
}

/**
 * @brief Write test_tom_core_pack_format_golden.h to stdout
 *
 * @return 0 on success, 1 if a buffer could not be packed
 */
static int dump() {
   packed_set s = pack_set();
   bool failed = false;
   for (const auto &[name, b] : {std::pair<const char *, const bytes &>{"free_elem", s.free_elem},
                                 {"bound_elem", s.bound_elem}, {"object_elem", s.object_elem},
                                 {"trans_elem", s.trans_elem}, {"list", s.list}, {"partial_list", s.partial_list}}) {
      if (b.empty()) {
         fprintf(stderr, "test_tom_core_pack_format: packing %s failed\n", name);
         failed = true;
      }
   }
   if (failed) {
      return 1;
   }
   printf("/*___INFO__MARK_BEGIN_NEW__*/\n"
          "/***************************************************************************\n"
          " *\n"
          " *  Copyright 2026 HPC-Gridware GmbH\n"
          " *\n"
          " *  Licensed under the Apache License, Version 2.0 (the \"License\");\n"
          " *  you may not use this file except in compliance with the License.\n"
          " *  You may obtain a copy of the License at\n"
          " *\n"
          " *      http://www.apache.org/licenses/LICENSE-2.0\n"
          " *\n"
          " *  Unless required by applicable law or agreed to in writing, software\n"
          " *  distributed under the License is distributed on an \"AS IS\" BASIS,\n"
          " *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
          " *  See the License for the specific language governing permissions and\n"
          " *  limitations under the License.\n"
          " *\n"
          " ***************************************************************************/\n"
          "/*___INFO__MARK_END_NEW__*/\n\n"
          "#pragma once\n\n"
          "/** @file\n"
          " * @brief Pack buffers recorded by the base revision for test_tom_core_pack_format\n"
          " *\n"
          " * Written by `test_tom_core_pack_format --dump`. Do not edit. Regenerate only with\n"
          " * a deliberate change of the pack format (CS-2997: full descriptors as type\n"
          " * references), on the build that introduces it, and check the difference to\n"
          " * the previous buffers: the buffers are the reference every later build is\n"
          " * compared with.\n"
          " */\n\n"
          "#include <cstdint>\n\n");
   dump_array("free_elem", s.free_elem);
   dump_array("bound_elem", s.bound_elem);
   dump_array("object_elem", s.object_elem);
   dump_array("trans_elem", s.trans_elem);
   dump_array("list", s.list);
   dump_array("partial_list", s.partial_list);
   return 0;
}

#ifndef OCS_PACK_FORMAT_DUMP_ONLY

/**
 * @brief Whether a buffer equals a recorded one
 *
 * @param b the buffer
 * @param golden the recorded buffer
 * @param size its length
 * @return true if both have the same length and bytes
 */
static bool same(const bytes &b, const uint8_t *golden, size_t size) {
   return b.size() == size && memcmp(b.data(), golden, size) == 0;
}

/**
 * @brief Unpack a recorded element buffer and pack the result again
 *
 * @param golden the recorded buffer
 * @param size its length
 * @return the bytes packed from the unpacked element; empty on failure
 */
static bytes repack_elem(const uint8_t *golden, const size_t size) {
   bytes result;
   ocs::tom::RawElementPtr ep;
   if (unpack(golden, size, [&](sge_pack_buffer *pb) { ep = ocs::tom::unpack_raw_elem(pb, PF_Type); })) {
      result = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_elem(pb, ep.get(), nullptr, 0); });
   }
   return result;
}

/**
 * @brief Unpack a recorded list buffer and pack the result again
 *
 * @param golden the recorded buffer
 * @param size its length
 * @return the bytes packed from the unpacked list; empty on failure
 */
static bytes repack_list(const uint8_t *golden, const size_t size) {
   bytes result;
   ocs::tom::RawListPtr lp;
   if (unpack(golden, size, [&](sge_pack_buffer *pb) { lp = ocs::tom::unpack_raw_list(pb); })) {
      result = pack([&](sge_pack_buffer *pb) { ocs::tom::pack_list(pb, lp.get(), nullptr, 0); });
   }
   return result;
}

/** @brief The buffers of the current build, packed once for all scenarios */
static packed_set current;

/**
 * @brief PACKFMT-01: elements in every state are packed as by the base revision
 *
 * @return nothing; CHECK records the results
 */
static void test_elements() {
   CHECK(1, "a free element is packed as by the base revision",
         same(current.free_elem, golden_free_elem, sizeof(golden_free_elem)));
   CHECK(2, "a bound element is packed as by the base revision",
         same(current.bound_elem, golden_bound_elem, sizeof(golden_bound_elem)));
   CHECK(3, "a sub-object is packed as by the base revision",
         same(current.object_elem, golden_object_elem, sizeof(golden_object_elem)));
   CHECK(4, "a transitionally bound element is packed as by the base revision",
         same(current.trans_elem, golden_trans_elem, sizeof(golden_trans_elem)));
}

/**
 * @brief PACKFMT-02: whole and reduced lists are packed as by the base revision
 *
 * @return nothing; CHECK records the results
 */
static void test_lists() {
   CHECK(5, "a list with its descriptor is packed as by the base revision",
         same(current.list, golden_list, sizeof(golden_list)));
   CHECK(6, "a list reduced to three fields is packed as by the base revision",
         same(current.partial_list, golden_partial_list, sizeof(golden_partial_list)));
}

/**
 * @brief PACKFMT-03: buffers of the base revision are read into the same states and values
 *
 * @return nothing; CHECK records the results
 */
static void test_read_old() {
   CHECK(7, "a recorded free element is read as a free element with the same values",
         same(repack_elem(golden_free_elem, sizeof(golden_free_elem)), golden_free_elem,
              sizeof(golden_free_elem)));
   CHECK(8, "a recorded bound element is read as a transitionally bound element with the same values",
         same(repack_elem(golden_bound_elem, sizeof(golden_bound_elem)), golden_trans_elem,
              sizeof(golden_trans_elem)));
   CHECK(9, "a recorded list is read with the same descriptor, elements and values",
         same(repack_list(golden_list, sizeof(golden_list)), golden_list, sizeof(golden_list)));
}

#endif

/**
 * @brief Run the scenarios, or write the recorded buffers with --dump
 * @param argc the number of arguments
 * @param argv the arguments: --dump, or those of the module test frame
 * @return the exit code of --dump or of the module test frame
 */
int main(int argc, char *argv[]) {
   ocs::module_test::install_test_model({{"PF", PF_Type, PF_Name, PF_Size}, {"PFS", PFS_Type, PFS_Name, PFS_Size}});

   if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
      return dump();
   }

#ifdef OCS_PACK_FORMAT_DUMP_ONLY
   fprintf(stderr, "test_tom_core_pack_format: built for --dump only\n");
   return 2;
#else
   current = pack_set();
   return ocs::module_test::run(argc, argv, {
      {"PACKFMT-01", "element states on the wire",
       "elements in every state are packed exactly as by the revision before the hash table move",
       {"pack a free element, a bound element, its sub-object and a transitionally bound element",
        "compare each buffer with the recorded one"},
       "every buffer equals the recorded one byte by byte",
       {{"HTL-03", "covers"}}, {1, 2, 3, 4}, test_elements},
      {"PACKFMT-02", "lists on the wire",
       "a whole list with its descriptor and a list reduced by a field selection are packed exactly as "
       "by the revision before the hash table move",
       {"pack a hashed list of three elements, one with unset fields", "pack it reduced to three fields",
        "compare each buffer with the recorded one"},
       "both buffers equal the recorded ones byte by byte",
       {{"HTL-03", "covers"}}, {5, 6}, test_lists},
      {"PACKFMT-03", "reading buffers of the base revision",
       "buffers recorded by the revision before the hash table move are read into the same states and values",
       {"unpack the recorded free element, bound element and list", "pack each result again"},
       "the free element and the list give their recorded bytes again; the bound element gives the bytes "
       "of a transitionally bound one, as before",
       {{"HTL-03", "covers"}}, {7, 8, 9}, test_read_old},
   });
#endif
}
