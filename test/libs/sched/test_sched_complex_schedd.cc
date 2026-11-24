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
 * @brief Unit tests for the complex values the scheduler builds in `libs/sched/sge_complex_schedd.cc`
 *
 * get_attribute() reads a fixed queue attribute (h_rt, s_vmem, ...) into a complex entry. For an
 * attribute without a value it used to store an uninitialised local buffer as CE_stringval, i.e.
 * whatever the stack held (CS-3185). The tests fill the stack with a pattern before the call, so
 * the old code stores that pattern and the check sees it.
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "sge_complex_schedd.h"

static int failures = 0;

/**
 * @brief Record the outcome of one check
 * @param id the id of the check, printed as `[Tnn]`
 * @param what what the check is about
 * @param ok whether the check held
 */
static void
check(const char *id, const char *what, const bool ok) {
   if (ok) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s\n", id, what);
      failures++;
   }
}

/**
 * @brief Fill the stack below the caller with a pattern
 *
 * A local buffer the next call leaves uninitialised then holds the pattern instead of whatever
 * an earlier call left there.
 */
[[gnu::noinline]] static void
poison_stack() {
   volatile char buf[16384];
   for (auto &c : buf) {
      c = 'X';
   }
}

/**
 * @brief Read a fixed queue attribute without a value through get_attribute()
 * @param attrname the name of the queue attribute, a complex of the given type
 * @param type the type of the complex
 * @return the complex entry get_attribute() returns
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
queue_attribute_without_value(const char *attrname, const ocs::CEntry::Type type) {
   auto centry_list = ocs::tom::ListPtr<ocs::tom::CE>::create("");
   auto centry = centry_list->emplace_back<CE_name>(attrname);
   centry.set<CE_shortcut>(attrname);
   centry.set<CE_valtype>(static_cast<uint32_t>(type));
   centry.set<CE_relop>(CMPLXLE_OP);
   centry.set<CE_requestable>(REQU_YES);
   centry.set<CE_consumable>(CONSUMABLE_NO);
   centry.set<CE_defaultval>("0");

   // a queue instance whose attribute was never set
   auto queue = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   queue->set<QU_qname>("all.q");
   queue->set<QU_qhostname>(ocs::tom::Host("host1"));

   DSTRING_STATIC(reason, 256);
   poison_stack();
   return get_attribute(attrname, ocs::tom::ConstList<ocs::tom::CE>(), ocs::tom::ConstList<ocs::tom::RUE>(),
                        ocs::tom::ConstList<ocs::tom::HL>(), *centry_list, ocs::tom::ConstList<ocs::tom::CE>(),
                        *queue, DOMINANT_LAYER_QUEUE, 1.0, &reason, false, 0, 0);
}

/**
 * @brief Whether a complex entry carries no string value
 * @param ep the complex entry
 * @return true if CE_stringval is unset or empty
 */
static bool
has_no_string_value(const ocs::tom::ElementPtr<ocs::tom::CE> &ep) {
   const char *value = ep->get<CE_stringval>();
   return value == nullptr || value[0] == '\0';
}

/**
 * @brief A fixed queue attribute without a value gives an empty string value (CS-3185)
 */
static void
test_queue_attribute_without_value() {
   const auto h_rt = queue_attribute_without_value("h_rt", ocs::CEntry::Type::TIME);
   check("T01", "h_rt without a value is returned as a queue value", h_rt && h_rt->get<CE_doubleval>() == 0.0);
   check("T02", "h_rt without a value has no string value, not what the stack held",
         h_rt && has_no_string_value(h_rt));

   const auto s_vmem = queue_attribute_without_value("s_vmem", ocs::CEntry::Type::MEM);
   check("T03", "s_vmem without a value has no string value, not what the stack held",
         s_vmem && has_no_string_value(s_vmem));

   const auto calendar = queue_attribute_without_value("calendar", ocs::CEntry::Type::STR);
   check("T04", "calendar without a value has no string value, not what the stack held",
         calendar && has_no_string_value(calendar));

   const auto qname = queue_attribute_without_value("qname", ocs::CEntry::Type::STR);
   check("T05", "qname keeps the name of the queue",
         qname && qname->get<CE_stringval>() != nullptr && strcmp(qname->get<CE_stringval>(), "all.q") == 0);
}

/**
 * @brief Run the tests
 * @return 0 if all checks held, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sched_complex_schedd");

   ocs::tom::install_model(ocs::tom::gcs_model());

   test_queue_attribute_without_value();

   printf("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
   DRETURN(failures == 0 ? 0 : 1);
}
