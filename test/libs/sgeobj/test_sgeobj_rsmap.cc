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

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

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

/**
 * @brief Return the text of the first answer-list element.
 * @param[in] answer_list answer list to inspect (may be nullptr/empty)
 * @return the first answer's text, or "" if the list is empty or has no text
 */
static const char *
first_answer_text(lList *answer_list) {
   const lListElem *aep = lFirst(answer_list);
   const char *text = (aep != nullptr) ? lGetString(aep, AN_text) : nullptr;
   return (text != nullptr) ? text : "";
}

/**
 * @brief Build a complex_values entry as it looks after centry_list_fill_request().
 * @param[in] type    the complex type, e.g. TYPE_RSMAP
 * @param[in] amount  the configured amount, i.e. what CE_doubleval holds
 * @return a CE_Type element the caller has to free
 */
static lListElem *
make_centry(u_long32 type, double amount) {
   lListElem *centry = lCreateElem(CE_Type);

   lSetString(centry, CE_name, "gpu");
   lSetUlong(centry, CE_valtype, type);
   lSetUlong(centry, CE_consumable, CONSUMABLE_YES);
   lSetDouble(centry, CE_doubleval, amount);

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

   const lListElem *resl;
   for_each_ep(resl, lGetList(centry, CE_resource_map_list)) {
      if (buffer[0] != '\0') {
         strncat(buffer, " ", size - strlen(buffer) - 1);
      }
      strncat(buffer, lGetString(resl, RESL_value), size - strlen(buffer) - 1);
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
      lListElem *centry = make_centry(TYPE_RSMAP, 4);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      ids_to_string(centry, ids, sizeof(ids));
      CHECK(1, "bare amount 4 expands to ids 0 1 2 3",
            ok && lGetNumberOfElem(lGetList(centry, CE_resource_map_list)) == 4 &&
            strcmp(ids, "0 1 2 3") == 0);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T02: every implicit id stands for exactly one instance
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 2);
      centry_rsmap_expand_implicit_ids(&al, centry, 512);
      const lListElem *first = lFirst(lGetList(centry, CE_resource_map_list));
      CHECK(2, "implicit id has amount 1", lGetUlong(first, RESL_amount) == 1);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T03: an explicit id list is never touched
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 2);
      lAddSubStr(centry, RESL_value, "gpu0", CE_resource_map_list, RESL_Type);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      ids_to_string(centry, ids, sizeof(ids));
      CHECK(3, "configured ids are left alone", ok && strcmp(ids, "gpu0") == 0);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T04: amount 0 is a host without instances, not an error
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 0);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      CHECK(4, "amount 0 stays without ids",
            ok && lGetList(centry, CE_resource_map_list) == nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T05: a complex which is no RSMAP is not given ids
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_INT, 4);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      CHECK(5, "non RSMAP is not expanded",
            ok && lGetList(centry, CE_resource_map_list) == nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T06: the limit itself is still accepted
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 512);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      CHECK(6, "amount equal to the limit is expanded",
            ok && lGetNumberOfElem(lGetList(centry, CE_resource_map_list)) == 512);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T07: one above the limit is rejected, and nothing is left half expanded
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 513);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 512);
      const char *msg = first_answer_text(al);
      CHECK(7, "amount above the limit is rejected",
            !ok && lGetList(centry, CE_resource_map_list) == nullptr &&
            lGetNumberOfElem(al) == 1 && strstr(msg, "exceeds the limit") != nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T08: a limit of 0 switches the implicit ids off - a bare amount is an error then,
   //      and it is reported as "needs to define IDs", not as a limit that was exceeded
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 1);
      bool ok = centry_rsmap_expand_implicit_ids(&al, centry, 0);
      const char *msg = first_answer_text(al);
      CHECK(8, "limit 0 rejects a bare amount",
            !ok && lGetList(centry, CE_resource_map_list) == nullptr &&
            lGetNumberOfElem(al) == 1 && strstr(msg, "exceeds the limit") == nullptr);
      lFreeElem(&centry);
      lFreeList(&al);
   }

   // T09: an explicit id list is accepted even when the limit is 0 - the limit only
   //      bounds what the system creates on its own
   {
      lList *al = nullptr;
      lListElem *centry = make_centry(TYPE_RSMAP, 1);
      lAddSubStr(centry, RESL_value, "gpu0", CE_resource_map_list, RESL_Type);
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

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_expand_implicit_ids();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
