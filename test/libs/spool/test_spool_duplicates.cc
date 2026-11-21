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
 * @brief Module test of the objects found twice while a list is read from the spool (N-H-18, SPL-01)
 */

#include <string>

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_Element.h"
#include "spool/ocs_spool_duplicates.h"

#include "ocs_module_test.h"

/**
 * @brief Read a list of objects through a filter as a spooling backend does
 * @tparam T the object type
 * @param filter the filter of the list
 * @param lp the list
 * @param ep the object read; freed if the filter refuses it
 * @param spool_key where the object was read from
 * @return true if the object was appended
 */
template <ocs::tom::ObjectType T>
static bool
read_into(ocs::spool::DuplicateFilter &filter, const ocs::tom::List<T> lp, ocs::tom::ElementPtr<T> &&ep,
          const char *spool_key) {
   if (filter.admit(*ep, spool_key)) {
      lp.push_back(std::move(ep));
      return true;
   }
   ep.reset();
   return false;
}

/**
 * @brief An object with one key field set
 * @tparam F the key field
 * @tparam V the type of the key value
 * @param key the value of the key field
 * @return the object
 */
template <auto F, typename V>
static ocs::tom::ElementPtr<typename ocs::tom::FieldTraits<F>::Type>
make_keyed(const V key) {
   auto ep = ocs::tom::ElementPtr<typename ocs::tom::FieldTraits<F>::Type>::create();
   ep->template set<F>(key);
   return ep;
}

/**
 * @brief A user set read twice under different spool keys
 * @return nothing; CHECK records the results
 */
static void
test_string_key() {
   ocs::spool::clear_duplicate_objects();
   auto usersets = ocs::tom::ListPtr<ocs::tom::US>::create("usersets");
   ocs::spool::DuplicateFilter filter(ocs::tom::US::descriptor.e, "USERSET");

   auto a = make_keyed<US_name>("arusers");
   auto b = make_keyed<US_name>("deadlineusers");
   auto copy = make_keyed<US_name>("arusers");

   const bool first = read_into(filter, *usersets, std::move(a), "usersets/arusers");
   const bool other = read_into(filter, *usersets, std::move(b), "usersets/deadlineusers");
   const bool second = read_into(filter, *usersets, std::move(copy), "usersets/arusers.copy");
   CHECK(1, "distinct names are admitted, the second arusers is not", first && other && !second);
   CHECK(2, "the list holds two user sets", usersets->size() == 2);

   const auto &duplicates = ocs::spool::duplicate_objects();
   CHECK(3, "one duplicate is recorded with type, attribute, key and both spool keys",
         duplicates.size() == 1 && duplicates[0].type == "USERSET" && duplicates[0].field == "US_name" &&
         duplicates[0].key == "arusers" && duplicates[0].first_spool_key == "usersets/arusers" &&
         duplicates[0].second_spool_key == "usersets/arusers.copy");
}

/**
 * @brief Host keys compare as the hash table of the list compares them
 * @return nothing; CHECK records the results
 */
static void
test_host_key() {
   ocs::spool::clear_duplicate_objects();
   auto hosts = ocs::tom::ListPtr<ocs::tom::EH>::create("exec hosts");
   ocs::spool::DuplicateFilter filter(ocs::tom::EH::descriptor.e, "EXECHOST");

   auto lower = make_keyed<EH_name>(ocs::tom::Host("host-1"));
   auto upper = make_keyed<EH_name>(ocs::tom::Host("HOST-1"));

   const bool first = read_into(filter, *hosts, std::move(lower), "EXECHOST:host-1");
   const bool second = read_into(filter, *hosts, std::move(upper), "EXECHOST:HOST-1");
   const auto &duplicates = ocs::spool::duplicate_objects();
   CHECK(4, "a host name differing only in case is a duplicate, recorded as the second copy holds it",
         first && !second && hosts->size() == 1 && duplicates.size() == 1 &&
         duplicates[0].key == "HOST-1" && duplicates[0].first_spool_key == "EXECHOST:host-1");
}

/**
 * @brief Number keys, objects without a key, separate filters and clearing the record
 * @return nothing; CHECK records the results
 */
static void
test_number_key_and_clear() {
   ocs::spool::clear_duplicate_objects();
   auto ars = ocs::tom::ListPtr<ocs::tom::AR>::create("advance reservations");
   ocs::spool::DuplicateFilter filter(ocs::tom::AR::descriptor.e, "AR");

   auto ar = make_keyed<AR_id>(17);
   auto again = make_keyed<AR_id>(17);
   const bool first = read_into(filter, *ars, std::move(ar), "AR:17");
   const bool second = read_into(filter, *ars, std::move(again), "AR:17.copy");
   CHECK(5, "an advance reservation id read twice is a duplicate", first && !second && ars->size() == 1);

   auto usersets = ocs::tom::ListPtr<ocs::tom::US>::create("usersets");
   ocs::spool::DuplicateFilter unnamed_filter(ocs::tom::US::descriptor.e, "USERSET");
   const bool unnamed_1 = read_into(unnamed_filter, *usersets, ocs::tom::ElementPtr<ocs::tom::US>::create(),
                                          "usersets/a");
   const bool unnamed_2 = read_into(unnamed_filter, *usersets, ocs::tom::ElementPtr<ocs::tom::US>::create(),
                                          "usersets/b");
   CHECK(6, "objects without a key are admitted", unnamed_1 && unnamed_2 && usersets->size() == 2);

   ocs::spool::DuplicateFilter other_filter(ocs::tom::AR::descriptor.e, "AR");
   auto same_id = make_keyed<AR_id>(17);
   auto other_ars = ocs::tom::ListPtr<ocs::tom::AR>::create("other advance reservations");
   CHECK(7, "a filter knows only the keys of its own list",
         read_into(other_filter, *other_ars, std::move(same_id), "AR:17") &&
            ocs::spool::duplicate_objects().size() == 1);

   ocs::spool::clear_duplicate_objects();
   CHECK(8, "clearing forgets the recorded duplicates", ocs::spool::duplicate_objects().empty());
   other_ars.reset();
   usersets.reset();
}

/**
 * @brief Run the scenarios of the duplicate objects read from a spool
 * @param argc number of arguments
 * @param argv the arguments; the module test frame takes its own options from them
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"SPD-01", "string key",
       "a spool holding two user sets of the same name",
       {"read three user sets through one filter, the third named like the first"},
       "the third is refused; one duplicate is recorded with type, attribute, key and both spool keys",
       {{"SPL-01", "covers"}}, {1, 2, 3}, test_string_key},
      {"SPD-02", "host key",
       "a spool holding the same exec host twice, its name differing in case",
       {"read host-1 and HOST-1 through one filter"},
       "HOST-1 is refused and recorded under the key it holds",
       {{"SPL-01", "covers"}}, {4}, test_host_key},
      {"SPD-03", "number key, no key, clear",
       "number keys, objects without a key, and the scope of a filter",
       {"read advance reservation 17 twice", "read two user sets without a name",
        "read advance reservation 17 into another list", "clear the record"},
       "the second advance reservation 17 is refused; objects without a key and another list are not affected; "
       "the record is empty after clearing",
       {{"SPL-01", "covers"}}, {5, 6, 7, 8}, test_number_key_and_clear},
   });
}
