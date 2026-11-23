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
 * @brief Unit tests for #MT_NO_TRANSFER (CS-2634)
 *
 * A field marked #MT_NO_TRANSFER carries process-local derived state. It
 * keeps its place in the descriptor, so the object keeps its shape, but the
 * value handed out is the one a freshly created element carries. Ordinary
 * copies made inside the process are not affected; only a copy that is
 * explicitly built for hand-out - an event payload - skips the field.
 */

#include <cstdio>
#include <cstring>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "msg_common.h"
#include "ocs_test_model.h"

enum {
   TNT_name = 1,        ///< an ordinary host name attribute
   TNT_text,            ///< an ordinary string attribute
   TNT_number,          ///< an ordinary unsigned long attribute
   TNT_sub,             ///< an ordinary sublist, carries nested marked fields
   TNT_child,           ///< an ordinary sub-object, carries nested marked fields
   TNT_cache_text,      ///< a string attribute that stays in the process
   TNT_cache_host,      ///< a host name attribute that stays in the process
   TNT_cache_list,      ///< a sublist that stays in the process
   TNT_cache_object,    ///< a sub-object that stays in the process
   TNT_cache_number,    ///< an unsigned long attribute that stays in the process
   TNT_cache_number64,  ///< a 64 bit unsigned attribute that stays in the process
   TNT_cache_flag,      ///< a boolean attribute that stays in the process
   TNT_cache_value,     ///< a double attribute that stays in the process
   TNT_cache_ref        ///< an opaque pointer that stays in the process
};

LISTDEF(TNT_Type)
                SGE_HOST    (TNT_name, MT_DEFAULT | MT_SPOOL)
                SGE_STRING  (TNT_text, MT_DEFAULT | MT_SPOOL)
                SGE_ULONG   (TNT_number, MT_DEFAULT | MT_SPOOL)
                SGE_LIST    (TNT_sub, TNT_Type, MT_DEFAULT | MT_SPOOL)
                SGE_OBJECT  (TNT_child, TNT_Type, MT_DEFAULT | MT_SPOOL)
                SGE_STRING  (TNT_cache_text, MT_NO_TRANSFER)
                SGE_HOST    (TNT_cache_host, MT_NO_TRANSFER)
                SGE_LIST    (TNT_cache_list, TNT_Type, MT_NO_TRANSFER)
                SGE_OBJECT  (TNT_cache_object, TNT_Type, MT_NO_TRANSFER)
                SGE_ULONG   (TNT_cache_number, MT_NO_TRANSFER)
                SGE_ULONG64 (TNT_cache_number64, MT_NO_TRANSFER)
                SGE_BOOL    (TNT_cache_flag, MT_NO_TRANSFER)
                SGE_DOUBLE  (TNT_cache_value, MT_NO_TRANSFER)
                SGE_REF     (TNT_cache_ref, TNT_Type, MT_NO_TRANSFER)
LISTEND(TNT_Type)

NAMEDEF(TNT_Name)
                NAME("TNT_name")
                NAME("TNT_text")
                NAME("TNT_number")
                NAME("TNT_sub")
                NAME("TNT_child")
                NAME("TNT_cache_text")
                NAME("TNT_cache_host")
                NAME("TNT_cache_list")
                NAME("TNT_cache_object")
                NAME("TNT_cache_number")
                NAME("TNT_cache_number64")
                NAME("TNT_cache_flag")
                NAME("TNT_cache_value")
                NAME("TNT_cache_ref")
NAMEEND

#define TNT_Size sizeof(TNT_Name) / sizeof(char *)   ///< number of attributes of the synthetic type

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `ok`/`FAIL` with the test's id and label and counts the failure, so
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


/** @brief Give every #MT_NO_TRANSFER field of an element a value
 *
 * @param ep the element to fill
 */
static void set_cache(const ocs::tom::RawElement ep) {
   auto cached_list = ocs::tom::RawListPtr::create(TNT_Type, ocs::tom::Hashing::On, "cache");
   auto cached_elem = ocs::tom::RawElementPtr::create(TNT_Type);

   cached_elem->set(TNT_name, ocs::tom::Host("cached_host"));
   cached_list->push_back(std::move(cached_elem));

   ep.set(TNT_cache_text, static_cast<const char *>("cached_text"));
   ep.set(TNT_cache_host, ocs::tom::Host("cached_hostname"));
   ep.set(TNT_cache_list, std::move(cached_list));
   ep.set(TNT_cache_object, ocs::tom::RawElementPtr::create(TNT_Type));
   ep.set(TNT_cache_number, std::uint32_t{42});
   ep.set(TNT_cache_number64, std::uint64_t{4242});
   ep.set(TNT_cache_flag, true);
   ep.set(TNT_cache_value, 4.2);
   ep.set(TNT_cache_ref, static_cast<void *>(ep.raw()));
}

/** @brief Does the element still carry every #MT_NO_TRANSFER value?
 *
 * @param ep the element to inspect, may be empty
 * @return true when all of them are set
 */
static bool cache_is_set(const ocs::tom::ConstRawElement ep) {
   return ep &&
          sge_strnullcmp(ep.get<const char *>(TNT_cache_text), "cached_text") == 0 &&
          sge_strnullcmp(ep.get<ocs::tom::Host>(TNT_cache_host), "cached_hostname") == 0 &&
          ep.get<ocs::tom::ConstRawList>(TNT_cache_list).size() == 1 &&
          ep.get<ocs::tom::ConstRawElement>(TNT_cache_object) &&
          ep.get<std::uint32_t>(TNT_cache_number) == 42 &&
          ep.get<std::uint64_t>(TNT_cache_number64) == 4242 &&
          ep.get<bool>(TNT_cache_flag) &&
          ep.get<double>(TNT_cache_value) == 4.2 &&
          ep.get<void *>(TNT_cache_ref) != nullptr;
}

/** @brief Is every #MT_NO_TRANSFER field of the element empty?
 *
 * Empty means what a freshly created element carries: nullptr for the pointer
 * valued types, 0 for the scalar ones.
 *
 * @param ep the element to inspect, may be empty
 * @return true when all of them are empty
 */
static bool cache_is_empty(const ocs::tom::ConstRawElement ep) {
   return ep &&
          ep.get<const char *>(TNT_cache_text) == nullptr &&
          ep.get<ocs::tom::Host>(TNT_cache_host) == nullptr &&
          ep.get<const lList *>(TNT_cache_list) == nullptr &&
          ep.get<const lListElem *>(TNT_cache_object) == nullptr &&
          ep.get<std::uint32_t>(TNT_cache_number) == 0 &&
          ep.get<std::uint64_t>(TNT_cache_number64) == 0 &&
          !ep.get<bool>(TNT_cache_flag) &&
          ep.get<double>(TNT_cache_value) == 0.0 &&
          ep.get<void *>(TNT_cache_ref) == nullptr;
}

/** @brief Does the element still know all of its fields?
 *
 * The property must not reduce the object: a consumer asking for the position
 * of a marked field has to receive one, otherwise it lands in an error path
 * instead of reading an empty value.
 *
 * @param ep the element to inspect, may be empty
 * @return true when every attribute of the type is addressable
 */
static bool shape_is_complete(const ocs::tom::ConstRawElement ep) {
   static const int all_fields[] = {
           TNT_name, TNT_text, TNT_number, TNT_sub, TNT_child,
           TNT_cache_text, TNT_cache_host, TNT_cache_list, TNT_cache_object,
           TNT_cache_number, TNT_cache_number64, TNT_cache_flag,
           TNT_cache_value, TNT_cache_ref
   };

   if (!ep) {
      return false;
   }
   if (ocs::tom::descr_length(ep.descr()) != static_cast<int>(TNT_Size)) {
      return false;
   }
   for (const int field : all_fields) {
      if (!ep.has(field)) {
         return false;
      }
   }
   return true;
}

/** @brief Send an element through a pack buffer and read it back
 *
 * @param ep the element to pack
 * @param what which fields to pack, or nullptr for all of them
 * @param[out] packed_size receives the number of bytes written, may be nullptr
 * @return the owner of the unpacked element, empty on failure
 */
static ocs::tom::RawElementPtr round_trip(const ocs::tom::ConstRawElement ep, const lEnumeration *what,
                                          size_t *packed_size) {
   ocs::tom::RawElementPtr copy;

   try {
      ocs::tom::PackBuffer pb(false, 100);
      ocs::tom::pack_elem(&pb.raw(), ep, what, 0);
      if (packed_size != nullptr) {
         *packed_size = pb.size();
      }

      ocs::tom::PackBuffer copy_pb = ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
      copy = ocs::tom::unpack_raw_elem(&copy_pb.raw(), nullptr, 0);
   } catch (const ocs::tom::PackException &) {
      copy.reset();
   }

   return copy;
}

/** @brief The first element of a sub-list field
 *
 * @param ep the element holding the sub-list, may be empty
 * @param nm the sub-list field
 * @return the first element, empty if there is no element or no list
 */
static ocs::tom::ConstRawElement first_of(const ocs::tom::ConstRawElement ep, const int nm) {
   return ep ? ep.get<ocs::tom::ConstRawList>(nm).front() : ocs::tom::ConstRawElement();
}

/** @brief The element of an object field
 *
 * @param ep the element holding the object, may be empty
 * @param nm the object field
 * @return the sub-object, empty if there is none
 */
static ocs::tom::ConstRawElement object_of(const ocs::tom::ConstRawElement ep, const int nm) {
   return ep ? ep.get<ocs::tom::ConstRawElement>(nm) : ocs::tom::ConstRawElement();
}

/**
 * @brief Run the scenarios of this test
 * @param argc the number of arguments
 * @param argv the arguments, handed to the module test driver
 * @return the exit status of the module test driver
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_tom_core_no_transfer");

   component_set_daemonized(true);
   ocs::module_test::install_test_model({{"TNT", TNT_Type, TNT_Name, TNT_Size}});

   // the object as the process that built it holds it: ordinary fields, a
   // process local cache, and a nested element carrying a cache of its own
   auto origin = ocs::tom::RawElementPtr::create(TNT_Type);
   origin->set(TNT_name, ocs::tom::Host("origin_host"));
   origin->set(TNT_text, static_cast<const char *>("origin_text"));
   origin->set(TNT_number, std::uint32_t{7});
   set_cache(*origin);
   {
      auto sub = ocs::tom::RawListPtr::create(TNT_Type, ocs::tom::Hashing::On, "sub");
      auto sub_elem = ocs::tom::RawElementPtr::create(TNT_Type);
      auto child = ocs::tom::RawElementPtr::create(TNT_Type);

      sub_elem->set(TNT_text, static_cast<const char *>("sub_text"));
      set_cache(*sub_elem);
      sub->push_back(std::move(sub_elem));
      origin->set(TNT_sub, std::move(sub));

      child->set(TNT_text, static_cast<const char *>("child_text"));
      set_cache(*child);
      origin->set(TNT_child, std::move(child));
   }

   CHECK(1, "the element under test starts out with its cache set", cache_is_set(*origin));

   // --- a reader asking for all fields ---
   printf("\n--- reader requesting all fields ---\n");
   {
      size_t size_with_cache = 0;
      ocs::tom::RawElementPtr copy = round_trip(*origin, nullptr, &size_with_cache);

      CHECK(2, "the object arrives", static_cast<bool>(copy));
      CHECK(3, "the object keeps its shape, every field is addressable", shape_is_complete(*copy));
      CHECK(4, "the marked fields arrive empty", cache_is_empty(*copy));
      CHECK(5, "the ordinary fields are untouched",
            copy &&
            sge_strnullcmp(copy->get<ocs::tom::Host>(TNT_name), "origin_host") == 0 &&
            sge_strnullcmp(copy->get<const char *>(TNT_text), "origin_text") == 0 &&
            copy->get<std::uint32_t>(TNT_number) == 7);
      CHECK(6, "a marked field of a nested element arrives empty too",
            cache_is_empty(first_of(*copy, TNT_sub)) &&
            cache_is_empty(object_of(*copy, TNT_child)) &&
            sge_strnullcmp(first_of(*copy, TNT_sub).get<const char *>(TNT_text), "sub_text") == 0 &&
            sge_strnullcmp(object_of(*copy, TNT_child).get<const char *>(TNT_text), "child_text") == 0);

      // the answer must not grow with the content of a marked field
      size_t size_without_cache = 0;
      ocs::tom::RawElementPtr without = origin.clone();

      without->set(TNT_cache_text, static_cast<const char *>(nullptr));
      without->set(TNT_cache_host, ocs::tom::Host());
      without->set(TNT_cache_list, ocs::tom::RawListPtr());
      without->set(TNT_cache_object, ocs::tom::RawElementPtr());
      without->set(TNT_cache_number, std::uint32_t{0});
      without->set(TNT_cache_number64, std::uint64_t{0});
      without->set(TNT_cache_flag, false);
      without->set(TNT_cache_value, 0.0);
      without->set(TNT_cache_ref, static_cast<void *>(nullptr));

      static_cast<void>(round_trip(*without, nullptr, &size_without_cache));
      CHECK(7, "the size of the answer does not depend on the content of the marked fields",
            size_with_cache > 0 && size_with_cache == size_without_cache);
   }

   // --- a reader asking for the marked fields by name ---
   printf("\n--- reader requesting the marked fields explicitly ---\n");
   {
      const int fields[] = {TNT_name, TNT_cache_text, TNT_cache_host, TNT_cache_list,
                            TNT_cache_object, TNT_cache_number, TNT_cache_number64,
                            TNT_cache_flag, TNT_cache_value, TNT_cache_ref};
      ocs::tom::RawWhatPtr what = ocs::tom::what_of(TNT_Type, fields);
      size_t size_with_cache = 0;
      ocs::tom::RawElementPtr copy = round_trip(*origin, what.get(), &size_with_cache);

      CHECK(8, "the reduced object arrives", static_cast<bool>(copy));
      CHECK(9, "the requested ordinary field is untouched",
            copy && sge_strnullcmp(copy->get<ocs::tom::Host>(TNT_name), "origin_host") == 0);
      CHECK(10, "the explicitly requested marked fields arrive empty", cache_is_empty(*copy));
      CHECK(11, "the explicitly requested marked fields are still addressable",
            copy && copy->has(TNT_cache_text) && copy->has(TNT_cache_list));

      auto without = ocs::tom::RawElementPtr::create(TNT_Type);
      size_t size_without_cache = 0;

      without->set(TNT_name, ocs::tom::Host("origin_host"));
      static_cast<void>(round_trip(*without, what.get(), &size_without_cache));
      CHECK(12, "the size of the reduced answer does not depend on the content either",
            size_with_cache > 0 && size_with_cache == size_without_cache);
   }

   // --- copies inside the process ---
   printf("\n--- copies inside the process ---\n");
   {
      ocs::tom::RawElementPtr copy = origin.clone();

      CHECK(13, "clone() keeps the marked fields", cache_is_set(*copy));
      CHECK(14, "clone() keeps them in nested elements too",
            cache_is_set(first_of(*copy, TNT_sub)) &&
            cache_is_set(object_of(*copy, TNT_child)));
      copy.reset();

      ocs::tom::RawWhatPtr what = ocs::tom::what_all();
      ocs::tom::RawElementPtr selected =
              ocs::tom::select_elem_as(origin.get(), nullptr, what.get(), ocs::tom::Hashing::Off);

      CHECK(15, "select_elem_as() keeps them unless asked to skip", cache_is_set(*selected));
   }

   // --- a copy that will be handed out, the way an event payload is built ---
   printf("\n--- copy for hand-out ---\n");
   {
      ocs::tom::RawElementPtr payload(ocs::tom::copy_elem(origin.get(), false, true));

      CHECK(16, "the marked fields are empty in the copy", cache_is_empty(*payload));
      CHECK(17, "the copy keeps its shape", shape_is_complete(*payload));
      CHECK(18, "the ordinary fields are copied as usual",
            sge_strnullcmp(payload->get<ocs::tom::Host>(TNT_name), "origin_host") == 0 &&
            sge_strnullcmp(payload->get<const char *>(TNT_text), "origin_text") == 0 &&
            payload->get<std::uint32_t>(TNT_number) == 7 &&
            payload->get<ocs::tom::ConstRawList>(TNT_sub).size() == 1 &&
            payload->get<ocs::tom::ConstRawElement>(TNT_child));
      CHECK(19, "nested elements are skipped as well",
            cache_is_empty(first_of(*payload, TNT_sub)) &&
            cache_is_empty(object_of(*payload, TNT_child)));
      CHECK(20, "the source is not affected", cache_is_set(*origin));
      payload.reset();

      auto lp = ocs::tom::RawListPtr::create(TNT_Type, ocs::tom::Hashing::On, "payload");
      lp->push_back(origin.clone());
      ocs::tom::RawListPtr payload_list(ocs::tom::copy_list("payload", lp.get(), false, true));
      CHECK(21, "copy_list() with skip_no_transfer skips them for every element of a list",
            cache_is_empty(payload_list->front()));
      payload_list.reset();
      lp.reset();

      // the reduced copy an event client with a what filter receives
      const int fields[] = {TNT_name, TNT_cache_text, TNT_cache_list, TNT_cache_number};
      ocs::tom::RawWhatPtr what = ocs::tom::what_of(TNT_Type, fields);
      ocs::tom::RawElementPtr reduced(ocs::tom::select_elem(origin.get(), what.get(), false, true));

      // only the selected fields exist in a reduced element, so this cannot go
      // through cache_is_empty()
      CHECK(22, "a reduced copy for hand-out skips them too",
            reduced &&
            reduced->get<const char *>(TNT_cache_text) == nullptr &&
            reduced->get<const lList *>(TNT_cache_list) == nullptr &&
            reduced->get<std::uint32_t>(TNT_cache_number) == 0 &&
            sge_strnullcmp(reduced->get<ocs::tom::Host>(TNT_name), "origin_host") == 0);
   }

   // --- a client reading the object and writing it back ---
   printf("\n--- read, modify, write back ---\n");
   {
      ocs::tom::RawElementPtr at_client = round_trip(*origin, nullptr, nullptr);

      CHECK(23, "the client never sees the value", cache_is_empty(*at_client));

      // the client changes an ordinary attribute and sends the object back
      ocs::tom::RawElementPtr instructions;
      if (at_client) {
         at_client->set(TNT_text, static_cast<const char *>("changed_by_client"));
         instructions = round_trip(*at_client, nullptr, nullptr);
      }

      CHECK(24, "the change arrives at the server",
            instructions &&
            sge_strnullcmp(instructions->get<const char *>(TNT_text), "changed_by_client") == 0);
      CHECK(25, "the write back carries no value for the marked fields",
            cache_is_empty(*instructions));
      CHECK(26, "what the server holds is unchanged", cache_is_set(*origin));
   }

   origin.reset();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
