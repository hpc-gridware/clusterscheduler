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

#include <cstdio>
#include <cstring>

#include "basis_types.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "tom/ocs_ElementCore.h"

#include "ocs_module_test.h"

/**
 * @brief Return the text of the first answer-list element.
 * @param[in] answer_list answer list to inspect (may be nullptr/empty)
 * @return the first answer's text, or "" if the list is empty or has no text
 */
static const char *
first_answer_text(lList *answer_list) {
   const lListElem *aep = lFirst(answer_list);
   const char *text = (aep != nullptr) ? ocs::tom::get<AN_text>(aep) : nullptr;
   return (text != nullptr) ? text : "";
}

/**
 * @brief Build a complex_values entry as it looks after centry_list_fill_request().
 * @param[in] type    the complex type, e.g. TYPE_RSMAP
 * @param[in] amount  the configured amount, i.e. what CE_doubleval holds
 * @return a CE_Type element the caller has to free
 */
static lListElem *
make_centry(uint32_t type, double amount) {
   lListElem *centry = lCreateElem(CE_Type);

   ocs::tom::set<CE_name>(centry, "gpu");
   ocs::tom::set<CE_valtype>(centry, type);
   ocs::tom::set<CE_consumable>(centry, CONSUMABLE_YES);
   ocs::tom::set<CE_doubleval>(centry, amount);

   return centry;
}

/**
 * @brief Render the ids of a centry as a blank separated string, in list order.
 * @param[in]  centry the CE_Type element to read
 * @param[out] buffer receives the ids
 * @param[in]  size   size of buffer
 */
static void
ids_to_string(const lListElem *centry, char *buffer, size_t size) {
   buffer[0] = '\0';

   ocs::tom::ConstElement<ocs::tom::RESL> resl;
   for_each_ep(resl, ocs::tom::get<CE_resource_map_list>(centry)) {
      if (buffer[0] != '\0') {
         strncat(buffer, " ", size - strlen(buffer) - 1);
      }
      strncat(buffer, ocs::tom::get<RESL_value>(resl), size - strlen(buffer) - 1);
   }
}

// ---------------------------------------------------------------------------

/**
 * @brief CS-2669: an RSMAP configured with a bare amount gets the ids 0 to amount-1,
 *        bounded by the limit the caller passes in (MAX_RSMAP_IDS in qmaster_params).
 */
static void
test_expand_implicit_ids() {
   char ids[1024];

   // T01: a bare amount creates one id per instance, numbered from 0
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 4);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      ids_to_string(centry, ids, sizeof(ids));
      CHECK(1, "bare amount 4 expands to ids 0 1 2 3",
            ok && lGetNumberOfElem(ocs::tom::get<CE_resource_map_list>(centry)) == 4 &&
            strcmp(ids, "0 1 2 3") == 0);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T02: every implicit id stands for exactly one instance
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 2);
      centry_rsmap_expand_implicit_ids(&al, centry, 512);
      ocs::tom::ConstElement<ocs::tom::RESL> first = lFirst(ocs::tom::get<CE_resource_map_list>(centry));
      CHECK(2, "implicit id has amount 1", ocs::tom::get<RESL_amount>(first) == 1);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T03: an explicit id list is never touched
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 2);
      ocs::tom::emplace_back_in<CE_resource_map_list, RESL_value>(centry, "gpu0");
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      ids_to_string(centry, ids, sizeof(ids));
      CHECK(3, "configured ids are left alone", ok && strcmp(ids, "gpu0") == 0);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T04: amount 0 is a host without instances, not an error
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 0);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      CHECK(4, "amount 0 stays without ids",
            ok && static_cast<const lList *>(ocs::tom::get<CE_resource_map_list>(centry)) == nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T05: a complex which is no RSMAP is not given ids
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::INT), 4);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      CHECK(5, "non RSMAP is not expanded",
            ok && static_cast<const lList *>(ocs::tom::get<CE_resource_map_list>(centry)) == nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T06: the limit itself is still accepted
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 512);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      CHECK(6, "amount equal to the limit is expanded",
            ok && lGetNumberOfElem(ocs::tom::get<CE_resource_map_list>(centry)) == 512);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T07: one above the limit is rejected, and nothing is left half expanded
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 513);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      const char *msg = first_answer_text(al);
      CHECK(7, "amount above the limit is rejected",
            !ok && static_cast<const lList *>(ocs::tom::get<CE_resource_map_list>(centry)) == nullptr &&
            lGetNumberOfElem(al) == 1 && strstr(msg, "exceeds the limit") != nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T08: a limit of 0 switches the implicit ids off - a bare amount is an error then,
   //      and it is reported as "needs to define IDs", not as a limit that was exceeded
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 1);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 0);
      const char *msg = first_answer_text(al);
      CHECK(8, "limit 0 rejects a bare amount",
            !ok && static_cast<const lList *>(ocs::tom::get<CE_resource_map_list>(centry)) == nullptr &&
            lGetNumberOfElem(al) == 1 && strstr(msg, "exceeds the limit") == nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T09: an explicit id list is accepted even when the limit is 0 - the limit only
   //      bounds what the system creates on its own
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 1);
      ocs::tom::emplace_back_in<CE_resource_map_list, RESL_value>(centry, "gpu0");
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 0);
      CHECK(9, "limit 0 does not affect configured ids", ok);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T10: a nullptr element is tolerated
   {
      lList *al = nullptr;
      CHECK(10, "nullptr centry is tolerated",
            centry_rsmap_expand_implicit_ids(&al, nullptr, 512));
      lFreeList(&al);
   }
}

/**
 * @brief Add a complex to a master complex list.
 * @param[in,out] master     the list to add to
 * @param[in]     name       name of the complex
 * @param[in]     type       its type, e.g. ocs::CEntry::Type::STR
 * @param[in]     consumable its consumable setting, e.g. CONSUMABLE_NO
 */
static void
add_master_centry(lList *master, const char *name, ocs::CEntry::Type type, uint32_t consumable) {
   ocs::tom::Element<ocs::tom::CE> ep = ocs::tom::emplace_back<CE_name>(&master, name);
   ocs::tom::set<CE_valtype>(ep, static_cast<uint32_t>(type));
   ocs::tom::set<CE_relop>(ep, type == ocs::CEntry::Type::STR ? CMPLXEQ_OP : CMPLXLE_OP);
   ocs::tom::set<CE_consumable>(ep, consumable);
   ocs::tom::set<CE_requestable>(ep, REQU_YES);
}

/**
 * @brief Build the complex_values entry gpu=1(gpu0[<name>=<value>,...]) as the flatfile reader
 *        leaves it: the characteristics carry only their name and their value as a string.
 * @param[in] names  names of the characteristics
 * @param[in] values their values, one per name
 * @param[in] count  how many characteristics
 * @return a CE_Type element the caller has to free
 */
static lListElem *
make_map_with_characteristics(const char *const names[], const char *const values[], int count) {
   lListElem *centry = make_centry(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP), 1);
   ocs::tom::Element<ocs::tom::RESL> resl = ocs::tom::emplace_back_in<CE_resource_map_list, RESL_value>(centry, "gpu0");
   ocs::tom::set<RESL_amount>(resl, 1);
   for (int i = 0; i < count; i++) {
      lListElem *prop = lCreateElem(CE_Type);
      ocs::tom::set<CE_name>(prop, names[i]);
      ocs::tom::set<CE_stringval>(prop, values[i]);
      lAppendElem(ocs::tom::get_or_create<RESL_properties>(resl, "properties"), prop);
   }
   return centry;
}

/**
 * @brief The characteristics of a resource map instance are checked against the complexes they
 *        name; CS-2714: a complex of type RSMAP is refused.
 */
static void
test_check_characteristics() {
   lList *master = lCreateList("complexes", CE_Type);
   add_master_centry(master, "gpu",       ocs::CEntry::Type::RSMAP, CONSUMABLE_YES);
   add_master_centry(master, "nested",    ocs::CEntry::Type::RSMAP, CONSUMABLE_YES);
   add_master_centry(master, "gpu_model", ocs::CEntry::Type::STR,   CONSUMABLE_NO);
   add_master_centry(master, "gpu_mem",   ocs::CEntry::Type::MEM,   CONSUMABLE_YES);

   // T11: a string characteristic is accepted and gets the type of its complex
   {
      const char *names[] = {"gpu_model"};
      const char *values[] = {"A100"};
      lList *al = nullptr;
      lListElem *centry = make_map_with_characteristics(names, values, 1);
      bool ok = centry_check_rsmap_characteristics(&al, centry, master);
      ocs::tom::ConstElement<ocs::tom::RESL> resl = lFirst(ocs::tom::get<CE_resource_map_list>(centry));
      ocs::tom::ConstElement<ocs::tom::CE> prop = lFirst(ocs::tom::get<RESL_properties>(resl));
      CHECK(11, "a string characteristic is accepted and typed",
            ok && static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(prop)) ==
                  ocs::CEntry::Type::STR);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T12: a consumable complex is accepted - it is never booked, but it is no error either
   {
      const char *names[] = {"gpu_mem"};
      const char *values[] = {"80G"};
      lList *al = nullptr;
      lListElem *centry = make_map_with_characteristics(names, values, 1);
      bool ok = centry_check_rsmap_characteristics(&al, centry, master);
      CHECK(12, "a consumable characteristic is accepted", ok);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T13: a resource map is refused, whatever the value, and the message says why
   {
      const char *names[] = {"nested"};
      const char *values[] = {"4"};
      lList *al = nullptr;
      lListElem *centry = make_map_with_characteristics(names, values, 1);
      bool ok = centry_check_rsmap_characteristics(&al, centry, master);
      CHECK(13, "a resource map as characteristic is refused",
            !ok && strstr(first_answer_text(al), "is a resource map") != nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T14: the map itself is no characteristic of its own instances either
   {
      const char *names[] = {"gpu"};
      const char *values[] = {"1"};
      lList *al = nullptr;
      lListElem *centry = make_map_with_characteristics(names, values, 1);
      bool ok = centry_check_rsmap_characteristics(&al, centry, master);
      CHECK(14, "the resource map itself as characteristic is refused", !ok);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T15: a name which is no complex is refused
   {
      const char *names[] = {"no_such_complex"};
      const char *values[] = {"x"};
      lList *al = nullptr;
      lListElem *centry = make_map_with_characteristics(names, values, 1);
      bool ok = centry_check_rsmap_characteristics(&al, centry, master);
      CHECK(15, "an unknown characteristic is refused",
            !ok && strstr(first_answer_text(al), "is not a defined complex") != nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T16: a characteristic given twice on one id is refused; a debug build refuses the duplicate already at the
   // append (N-H-18), so the checks are suspended while the input is built
   {
      const ocs::tom::UniqueCheckSuspension suspension;
      const char *names[] = {"gpu_model", "gpu_model"};
      const char *values[] = {"A100", "H100"};
      lList *al = nullptr;
      lListElem *centry = make_map_with_characteristics(names, values, 2);
      bool ok = centry_check_rsmap_characteristics(&al, centry, master);
      CHECK(16, "a duplicate characteristic is refused",
            !ok && strstr(first_answer_text(al), "is set more than once") != nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   lFreeList(&master);
}

// ---------------------------------------------------------------------------

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame.
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   lInit(nmv);

   return ocs::module_test::run(argc, argv, {
      {"RSMAP-01", "implicit ids of a resource map with a bare amount (CS-2669)",
       "an RSMAP configured with a bare amount gets the ids 0 to amount-1, bounded by the limit "
       "the caller passes in; configured ids, amount 0 and other types are left alone",
       {"expand a bare amount 4",
        "expand a map with configured ids, one with amount 0 and a non-RSMAP complex",
        "expand an amount equal to the limit and one above it",
        "expand with the limit 0, with and without configured ids",
        "expand nullptr"},
       "the bare amount becomes ids 0 to 3 of amount 1 each, everything else is unchanged, an "
       "amount above the limit and any bare amount under limit 0 are rejected, nullptr is tolerated",
       {}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, test_expand_implicit_ids},
      {"RSMAP-02", "characteristics of a resource map instance (CS-2714)",
       "the characteristics of a resource map instance are checked against the complexes they "
       "name, and a complex of type RSMAP is refused as characteristic",
       {"check a string and a consumable characteristic",
        "check a resource map as characteristic, and the map itself",
        "check an unknown and a duplicate characteristic"},
       "the string and the consumable characteristic are accepted, the string one typed; every "
       "other case is refused with its own message",
       {}, {11, 12, 13, 14, 15, 16}, test_check_characteristics},
   });
}
