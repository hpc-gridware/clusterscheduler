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
 * @brief Unit tests for cluster queues in `libs/sgeobj`
 *
 * CS-3117: cqueue_purge_host() removes a host or host group entry from the
 * attribute lists of a cluster queue. When the removed entry was the last one
 * of a list, the list is freed - and the attribute field of the cluster queue
 * must then be nullptr, not a pointer to the freed list. The tests purge the
 * default entry "@/" of a template queue, where every attribute list holds
 * exactly that one entry, and check every attribute field afterwards.
 */

#include <cstdio>

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_cqueue_verify.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

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

/**
 * @brief Create the template cluster queue qconf -aq starts from
 * @param name the name of the queue
 * @return the queue, an empty handle if it could not be built
 */
static ocs::tom::ElementPtr<ocs::tom::CQ>
make_template_queue(const char *name) {
   lList *alp = nullptr;
   auto cqueue = cqueue_create(&alp, name);
   if (cqueue && !cqueue_set_template_attributes(*cqueue, &alp)) {
      cqueue.reset();
   }
   lFreeList(&alp);
   return cqueue;
}

/**
 * @brief Count the attribute lists of a cluster queue that are still set
 * @param cqueue the queue
 * @return the number of entries of cqueue_attribute_array whose list is not nullptr
 */
static int
count_attribute_lists(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue) {
   int set = 0;
   for (int index = 0; cqueue_attribute_array[index].name != nullptr; index++) {
      if (lGetList(cqueue, cqueue_attribute_array[index].cqueue_attr) != nullptr) {
         set++;
      }
   }
   return set;
}

// ---------------------------------------------------------------------------
// cqueue_purge_host()
// ---------------------------------------------------------------------------

/**
 * @brief cqueue_purge_host() leaves no attribute field pointing at a freed list (CS-3117)
 */
static void
test_purge_default_entry() {
   printf("\n--- cqueue_purge_host: default entry ---\n");

   auto cqueue = make_template_queue("all.q");
   CHECK(1, "template queue created", static_cast<bool>(cqueue));
   if (!cqueue) {
      return;
   }
   const int lists_before = count_attribute_lists(*cqueue);
   CHECK(2, "the template sets attribute lists", lists_before > 0);

   // a host that is in no list changes nothing
   auto patterns = ocs::tom::ListPtr<ocs::tom::US>::from_string<US_name>("*", ",");
   lList *alp = nullptr;
   const bool purged_unknown = cqueue_purge_host(*cqueue, &alp, *patterns, "no_such_host");
   CHECK(3, "purging an unknown host reports nothing deleted", !purged_unknown);
   CHECK(4, "purging an unknown host keeps every list", count_attribute_lists(*cqueue) == lists_before);

   // the default entry is the only entry of every list: each list becomes empty and is freed
   const bool purged_default = cqueue_purge_host(*cqueue, &alp, *patterns, "@/");
   CHECK(5, "purging @/ reports a deletion", purged_default);
   CHECK(6, "every emptied attribute field is nullptr", count_attribute_lists(*cqueue) == 0);

   // a second purge walks the (now unset) fields again; with dangling pointers this read freed memory
   const bool purged_again = cqueue_purge_host(*cqueue, &alp, *patterns, "@/");
   CHECK(7, "a second purge of @/ finds nothing to delete", !purged_again);
   CHECK(8, "a second purge leaves every field nullptr", count_attribute_lists(*cqueue) == 0);

   lFreeList(&alp);
}

/**
 * @brief The slots limit of queues and parallel environments (CS-3146)
 *
 * CS-173 raised the job and AR id limit that the slots checks shared, which lifted the slots limit too.
 * A new or modified queue or parallel environment above #OCS_MAX_SLOTS is refused again; a parallel
 * environment read from the spool at startup is only warned about, so qmaster still starts.
 */
static void
test_slots_limit() {
   printf("\n--- slots limit ---\n");

   auto cqueue = ocs::tom::ElementPtr<ocs::tom::CQ>::create();
   ocs::tom::set<CQ_name>(cqueue, "all.q");
   lListElem *attr = lCreateElem(AULNG_Type);

   lList *alp = nullptr;
   lSetUlong(attr, AULNG_value, OCS_MAX_SLOTS);
   CHECK(9, "a queue with OCS_MAX_SLOTS slots is accepted", cqueue_verify_job_slots(*cqueue, &alp, attr, nullptr));
   lSetUlong(attr, AULNG_value, OCS_MAX_SLOTS + 1);
   CHECK(10, "a queue with more slots is refused", !cqueue_verify_job_slots(*cqueue, &alp, attr, nullptr));
   CHECK(11, "the refusal is an error", answer_list_has_error(&alp));
   lFreeList(&alp);
   lFreeElem(&attr);

   CHECK(12, "a PE with OCS_MAX_SLOTS slots is accepted", pe_validate_slots(&alp, OCS_MAX_SLOTS, false) == STATUS_OK);
   CHECK(13, "a PE with more slots is refused", pe_validate_slots(&alp, OCS_MAX_SLOTS + 1, false) != STATUS_OK);
   lFreeList(&alp);
   CHECK(14, "at startup a PE with more slots is accepted",
         pe_validate_slots(&alp, OCS_MAX_SLOTS + 1, true) == STATUS_OK);
   CHECK(15, "and only warned about", alp != nullptr && !answer_list_has_error(&alp) &&
                                         answer_list_has_quality(&alp, ANSWER_QUALITY_WARNING));
   lFreeList(&alp);
}

/**
 * @brief Entry point
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_purge_default_entry();
   test_slots_limit();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
