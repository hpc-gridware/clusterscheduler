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
 * @brief Unit tests for #CULL_NO_TRANSFER (CS-2634)
 *
 * A field marked #CULL_NO_TRANSFER carries process-local derived state. It
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

#include "cull/cull.h"

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "msg_common.h"

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
                SGE_HOST    (TNT_name, CULL_DEFAULT | CULL_SPOOL)
                SGE_STRING  (TNT_text, CULL_DEFAULT | CULL_SPOOL)
                SGE_ULONG   (TNT_number, CULL_DEFAULT | CULL_SPOOL)
                SGE_LIST    (TNT_sub, TNT_Type, CULL_DEFAULT | CULL_SPOOL)
                SGE_OBJECT  (TNT_child, TNT_Type, CULL_DEFAULT | CULL_SPOOL)
                SGE_STRING  (TNT_cache_text, CULL_NO_TRANSFER)
                SGE_HOST    (TNT_cache_host, CULL_NO_TRANSFER)
                SGE_LIST    (TNT_cache_list, TNT_Type, CULL_NO_TRANSFER)
                SGE_OBJECT  (TNT_cache_object, TNT_Type, CULL_NO_TRANSFER)
                SGE_ULONG   (TNT_cache_number, CULL_NO_TRANSFER)
                SGE_ULONG64 (TNT_cache_number64, CULL_NO_TRANSFER)
                SGE_BOOL    (TNT_cache_flag, CULL_NO_TRANSFER)
                SGE_DOUBLE  (TNT_cache_value, CULL_NO_TRANSFER)
                SGE_REF     (TNT_cache_ref, TNT_Type, CULL_NO_TRANSFER)
LISTEND

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

/** @brief The name space registering the synthetic type with CULL */
lNameSpace nmv[] = {
        {1, TNT_Size, TNT_Name, TNT_Type},
        {0, 0, nullptr, nullptr}
};

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

/** @brief Give every #CULL_NO_TRANSFER field of an element a value
 *
 * @param ep the element to fill
 */
static void set_cache(lListElem *ep) {
   lListElem *cached_elem = lCreateElem(TNT_Type);
   lList *cached_list = lCreateList("cache", TNT_Type);

   lSetHost(cached_elem, TNT_name, "cached_host");
   lAppendElem(cached_list, cached_elem);

   lSetString(ep, TNT_cache_text, "cached_text");
   lSetHost(ep, TNT_cache_host, "cached_hostname");
   lSetList(ep, TNT_cache_list, cached_list);
   lSetObject(ep, TNT_cache_object, lCreateElem(TNT_Type));
   lSetUlong(ep, TNT_cache_number, 42);
   lSetUlong64(ep, TNT_cache_number64, 4242);
   lSetBool(ep, TNT_cache_flag, true);
   lSetDouble(ep, TNT_cache_value, 4.2);
   lSetRef(ep, TNT_cache_ref, ep);
}

/** @brief Does the element still carry every #CULL_NO_TRANSFER value?
 *
 * @param ep the element to inspect
 * @return true when all of them are set
 */
static bool cache_is_set(const lListElem *ep) {
   return ep != nullptr &&
          sge_strnullcmp(lGetString(ep, TNT_cache_text), "cached_text") == 0 &&
          sge_strnullcmp(lGetHost(ep, TNT_cache_host), "cached_hostname") == 0 &&
          lGetNumberOfElem(lGetList(ep, TNT_cache_list)) == 1 &&
          lGetObject(ep, TNT_cache_object) != nullptr &&
          lGetUlong(ep, TNT_cache_number) == 42 &&
          lGetUlong64(ep, TNT_cache_number64) == 4242 &&
          lGetBool(ep, TNT_cache_flag) &&
          lGetDouble(ep, TNT_cache_value) == 4.2 &&
          lGetRef(ep, TNT_cache_ref) != nullptr;
}

/** @brief Is every #CULL_NO_TRANSFER field of the element empty?
 *
 * Empty means what a freshly created element carries: nullptr for the pointer
 * valued types, 0 for the scalar ones.
 *
 * @param ep the element to inspect
 * @return true when all of them are empty
 */
static bool cache_is_empty(const lListElem *ep) {
   return ep != nullptr &&
          lGetString(ep, TNT_cache_text) == nullptr &&
          lGetHost(ep, TNT_cache_host) == nullptr &&
          lGetList(ep, TNT_cache_list) == nullptr &&
          lGetObject(ep, TNT_cache_object) == nullptr &&
          lGetUlong(ep, TNT_cache_number) == 0 &&
          lGetUlong64(ep, TNT_cache_number64) == 0 &&
          !lGetBool(ep, TNT_cache_flag) &&
          lGetDouble(ep, TNT_cache_value) == 0.0 &&
          lGetRef(ep, TNT_cache_ref) == nullptr;
}

/** @brief Does the element still know all of its fields?
 *
 * The property must not reduce the object: a consumer asking for the position
 * of a marked field has to receive one, otherwise it lands in an error path
 * instead of reading an empty value.
 *
 * @param ep the element to inspect
 * @return true when every attribute of the type is addressable
 */
static bool shape_is_complete(const lListElem *ep) {
   static const int all_fields[] = {
           TNT_name, TNT_text, TNT_number, TNT_sub, TNT_child,
           TNT_cache_text, TNT_cache_host, TNT_cache_list, TNT_cache_object,
           TNT_cache_number, TNT_cache_number64, TNT_cache_flag,
           TNT_cache_value, TNT_cache_ref
   };

   if (ep == nullptr) {
      return false;
   }
   if (lCountDescr(lGetElemDescr(ep)) != static_cast<int>(TNT_Size)) {
      return false;
   }
   for (int field : all_fields) {
      if (lGetPosViaElem(ep, field, SGE_NO_ABORT) < 0) {
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
 * @return the unpacked element, owned by the caller, or nullptr on failure
 */
static lListElem *round_trip(const lListElem *ep, const lEnumeration *what, size_t *packed_size) {
   sge_pack_buffer pb;
   lListElem *copy = nullptr;

   if (init_packbuffer(&pb, 100, false, false) != PACK_SUCCESS) {
      return nullptr;
   }
   if (cull_pack_elem_partial(&pb, ep, what, 0) == PACK_SUCCESS) {
      if (packed_size != nullptr) {
         *packed_size = pb.bytes_used;
      }

      char *buf = sge_malloc(pb.bytes_used);
      sge_pack_buffer copy_pb;

      if (buf != nullptr) {
         memcpy(buf, pb.head_ptr, pb.bytes_used);
         if (init_packbuffer_from_buffer(&copy_pb, buf, pb.bytes_used, false) == PACK_SUCCESS) {
            if (cull_unpack_elem_partial(&copy_pb, &copy, nullptr, 0) != PACK_SUCCESS) {
               lFreeElem(&copy);
            }
            clear_packbuffer(&copy_pb);
         } else {
            sge_free(&buf);
         }
      }
   }
   clear_packbuffer(&pb);

   return copy;
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_cull_no_transfer");

   component_set_daemonized(true);
   lInit(nmv);

   // the object as the process that built it holds it: ordinary fields, a
   // process local cache, and a nested element carrying a cache of its own
   lListElem *origin = lCreateElem(TNT_Type);
   lSetHost(origin, TNT_name, "origin_host");
   lSetString(origin, TNT_text, "origin_text");
   lSetUlong(origin, TNT_number, 7);
   set_cache(origin);
   {
      lList *sub = lCreateList("sub", TNT_Type);
      lListElem *sub_elem = lCreateElem(TNT_Type);
      lListElem *child = lCreateElem(TNT_Type);

      lSetString(sub_elem, TNT_text, "sub_text");
      set_cache(sub_elem);
      lAppendElem(sub, sub_elem);
      lSetList(origin, TNT_sub, sub);

      lSetString(child, TNT_text, "child_text");
      set_cache(child);
      lSetObject(origin, TNT_child, child);
   }

   CHECK(1, "the element under test starts out with its cache set", cache_is_set(origin));

   // --- a reader asking for all fields ---
   printf("\n--- reader requesting all fields ---\n");
   {
      size_t size_with_cache = 0;
      lListElem *copy = round_trip(origin, nullptr, &size_with_cache);

      CHECK(2, "the object arrives", copy != nullptr);
      CHECK(3, "the object keeps its shape, every field is addressable", shape_is_complete(copy));
      CHECK(4, "the marked fields arrive empty", cache_is_empty(copy));
      CHECK(5, "the ordinary fields are untouched",
            copy != nullptr &&
            sge_strnullcmp(lGetHost(copy, TNT_name), "origin_host") == 0 &&
            sge_strnullcmp(lGetString(copy, TNT_text), "origin_text") == 0 &&
            lGetUlong(copy, TNT_number) == 7);
      CHECK(6, "a marked field of a nested element arrives empty too",
            cache_is_empty(lFirst(lGetList(copy, TNT_sub))) &&
            cache_is_empty(lGetObject(copy, TNT_child)) &&
            sge_strnullcmp(lGetString(lFirst(lGetList(copy, TNT_sub)), TNT_text), "sub_text") == 0 &&
            sge_strnullcmp(lGetString(lGetObject(copy, TNT_child), TNT_text), "child_text") == 0);

      // the answer must not grow with the content of a marked field
      size_t size_without_cache = 0;
      lListElem *without = lCopyElem(origin);

      lSetString(without, TNT_cache_text, nullptr);
      lSetHost(without, TNT_cache_host, nullptr);
      lSetList(without, TNT_cache_list, nullptr);
      lSetObject(without, TNT_cache_object, nullptr);
      lSetUlong(without, TNT_cache_number, 0);
      lSetUlong64(without, TNT_cache_number64, 0);
      lSetBool(without, TNT_cache_flag, false);
      lSetDouble(without, TNT_cache_value, 0.0);
      lSetRef(without, TNT_cache_ref, nullptr);

      lListElem *without_copy = round_trip(without, nullptr, &size_without_cache);
      lFreeElem(&without_copy);
      CHECK(7, "the size of the answer does not depend on the content of the marked fields",
            size_with_cache > 0 && size_with_cache == size_without_cache);

      lFreeElem(&without);
      lFreeElem(&copy);
   }

   // --- a reader asking for the marked fields by name ---
   printf("\n--- reader requesting the marked fields explicitly ---\n");
   {
      lEnumeration *what = lWhat("%T(%I %I %I %I %I %I %I %I %I %I)", TNT_Type,
                                 TNT_name, TNT_cache_text, TNT_cache_host, TNT_cache_list,
                                 TNT_cache_object, TNT_cache_number, TNT_cache_number64,
                                 TNT_cache_flag, TNT_cache_value, TNT_cache_ref);
      size_t size_with_cache = 0;
      lListElem *copy = round_trip(origin, what, &size_with_cache);

      CHECK(8, "the reduced object arrives", copy != nullptr);
      CHECK(9, "the requested ordinary field is untouched",
            copy != nullptr && sge_strnullcmp(lGetHost(copy, TNT_name), "origin_host") == 0);
      CHECK(10, "the explicitly requested marked fields arrive empty", cache_is_empty(copy));
      CHECK(11, "the explicitly requested marked fields are still addressable",
            copy != nullptr &&
            lGetPosViaElem(copy, TNT_cache_text, SGE_NO_ABORT) >= 0 &&
            lGetPosViaElem(copy, TNT_cache_list, SGE_NO_ABORT) >= 0);

      lListElem *without = lCreateElem(TNT_Type);
      size_t size_without_cache = 0;

      lSetHost(without, TNT_name, "origin_host");
      lListElem *without_copy = round_trip(without, what, &size_without_cache);
      lFreeElem(&without_copy);
      CHECK(12, "the size of the reduced answer does not depend on the content either",
            size_with_cache > 0 && size_with_cache == size_without_cache);

      lFreeElem(&without);
      lFreeElem(&copy);
      lFreeWhat(&what);
   }

   // --- copies inside the process ---
   printf("\n--- copies inside the process ---\n");
   {
      lListElem *copy = lCopyElem(origin);

      CHECK(13, "lCopyElem() keeps the marked fields", cache_is_set(copy));
      CHECK(14, "lCopyElem() keeps them in nested elements too",
            cache_is_set(lFirst(lGetList(copy, TNT_sub))) &&
            cache_is_set(lGetObject(copy, TNT_child)));
      lFreeElem(&copy);

      lEnumeration *what = lWhat("%T(ALL)", TNT_Type);
      lListElem *selected = lSelectElemPack(origin, nullptr, what, false, nullptr);

      CHECK(15, "lSelectElemPack() keeps them unless asked to skip", cache_is_set(selected));
      lFreeElem(&selected);
      lFreeWhat(&what);
   }

   // --- a copy that will be handed out, the way an event payload is built ---
   printf("\n--- copy for hand-out ---\n");
   {
      lListElem *payload = lCopyElemHash(origin, false, true);

      CHECK(16, "the marked fields are empty in the copy", cache_is_empty(payload));
      CHECK(17, "the copy keeps its shape", shape_is_complete(payload));
      CHECK(18, "the ordinary fields are copied as usual",
            sge_strnullcmp(lGetHost(payload, TNT_name), "origin_host") == 0 &&
            sge_strnullcmp(lGetString(payload, TNT_text), "origin_text") == 0 &&
            lGetUlong(payload, TNT_number) == 7 &&
            lGetNumberOfElem(lGetList(payload, TNT_sub)) == 1 &&
            lGetObject(payload, TNT_child) != nullptr);
      CHECK(19, "nested elements are skipped as well",
            cache_is_empty(lFirst(lGetList(payload, TNT_sub))) &&
            cache_is_empty(lGetObject(payload, TNT_child)));
      CHECK(20, "the source is not affected", cache_is_set(origin));
      lFreeElem(&payload);

      lList *lp = lCreateList("payload", TNT_Type);
      lAppendElem(lp, lCopyElem(origin));
      lList *payload_list = lCopyListHash("payload", lp, false, true);
      CHECK(21, "lCopyListHash() skips them for every element of a list",
            cache_is_empty(lFirst(payload_list)));
      lFreeList(&payload_list);
      lFreeList(&lp);

      // the reduced copy an event client with a what filter receives
      lEnumeration *what = lWhat("%T(%I %I %I %I)", TNT_Type,
                                 TNT_name, TNT_cache_text, TNT_cache_list, TNT_cache_number);
      lListElem *reduced = lSelectElemPack(origin, nullptr, what, false, nullptr, true);

      // only the selected fields exist in a reduced element, so this cannot go
      // through cache_is_empty()
      CHECK(22, "a reduced copy for hand-out skips them too",
            reduced != nullptr &&
            lGetString(reduced, TNT_cache_text) == nullptr &&
            lGetList(reduced, TNT_cache_list) == nullptr &&
            lGetUlong(reduced, TNT_cache_number) == 0 &&
            sge_strnullcmp(lGetHost(reduced, TNT_name), "origin_host") == 0);
      lFreeElem(&reduced);
      lFreeWhat(&what);
   }

   // --- a client reading the object and writing it back ---
   printf("\n--- read, modify, write back ---\n");
   {
      lListElem *at_client = round_trip(origin, nullptr, nullptr);

      CHECK(23, "the client never sees the value", cache_is_empty(at_client));

      // the client changes an ordinary attribute and sends the object back
      lSetString(at_client, TNT_text, "changed_by_client");
      lListElem *instructions = round_trip(at_client, nullptr, nullptr);

      CHECK(24, "the change arrives at the server",
            instructions != nullptr &&
            sge_strnullcmp(lGetString(instructions, TNT_text), "changed_by_client") == 0);
      CHECK(25, "the write back carries no value for the marked fields",
            cache_is_empty(instructions));
      CHECK(26, "what the server holds is unchanged", cache_is_set(origin));

      lFreeElem(&instructions);
      lFreeElem(&at_client);
   }

   lFreeElem(&origin);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
