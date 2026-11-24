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
 * @brief Unit tests for run-time typed sgeobj helpers on the checked raw API (CS-3103)
 *
 * Covers host_list_locate(), which takes the host list as a raw handle, and
 * the freeing of the master list slots of the data store, which are indexed
 * by the object type at run time.
 */

#include <cstdio>
#include <utility>

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "uti/sge.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
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

// ---------------------------------------------------------------------------
// host_list_locate()
// ---------------------------------------------------------------------------

/**
 * @brief host_list_locate() on raw and typed host lists, on empty and foreign lists
 */
static void
test_host_list_locate() {
   auto hosts = ocs::tom::ListPtr<ocs::tom::EH>::create("hosts");
   const auto global_host = hosts->emplace_back<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME));
   const auto template_host = hosts->emplace_back<EH_name>(ocs::tom::Host(SGE_TEMPLATE_NAME));

   // a typed handle converts to the raw handle of the parameter
   CHECK(1, "typed list: global is found", host_list_locate(*hosts, SGE_GLOBAL_NAME) == global_host.raw());
   // a raw pointer converts as well
   const ocs::tom::ListData *raw_hosts = hosts->raw();
   CHECK(2, "raw list: template is found", host_list_locate(raw_hosts, SGE_TEMPLATE_NAME) == template_host.raw());
   CHECK(3, "unknown host: nullptr", host_list_locate(*hosts, "no_such_host_x") == nullptr);
   CHECK(4, "nullptr host name: nullptr", host_list_locate(*hosts, nullptr) == nullptr);
   CHECK(5, "nullptr list: nullptr", host_list_locate(nullptr, SGE_GLOBAL_NAME) == nullptr);

   auto empty = ocs::tom::ListPtr<ocs::tom::EH>::create("empty");
   CHECK(6, "empty list: nullptr", host_list_locate(*empty, SGE_GLOBAL_NAME) == nullptr);

   // a list of another type finds nothing (the old lookup on NoName), and does not throw
   auto centries = ocs::tom::ListPtr<ocs::tom::CE>::create("centries");
   centries->emplace_back<CE_name>(SGE_GLOBAL_NAME);
   bool threw = false;
   ocs::tom::ElementData *found = nullptr;
   try {
      found = host_list_locate(*centries, SGE_GLOBAL_NAME);
   } catch (...) {
      threw = true;
   }
   CHECK(7, "foreign list: nullptr, no throw", !threw && found == nullptr);
}

// ---------------------------------------------------------------------------
// DataStore::free_master_list() / free_all_master_lists()
// ---------------------------------------------------------------------------

/**
 * @brief The master list slots are freed through raw owners and hold nullptr afterwards
 */
static void
test_free_master_lists() {
   static_cast<void>(ocs::DataStore::replace_master_list<SGE_TYPE_USERSET>(
      ocs::tom::ListPtr<ocs::tom::US>::create("usersets")));
   CHECK(8, "slot holds the list", ocs::DataStore::master_list_raw(SGE_TYPE_USERSET));
   ocs::DataStore::free_master_list(SGE_TYPE_USERSET);
   CHECK(9, "free_master_list(): slot is nullptr", !ocs::DataStore::master_list_raw(SGE_TYPE_USERSET));
   // freeing an empty slot again is harmless
   ocs::DataStore::free_master_list(SGE_TYPE_USERSET);
   CHECK(10, "free_master_list() twice: slot is nullptr",
         !ocs::DataStore::master_list_raw(SGE_TYPE_USERSET));

   static_cast<void>(ocs::DataStore::replace_master_list<SGE_TYPE_USERSET>(
      ocs::tom::ListPtr<ocs::tom::US>::create("usersets")));
   static_cast<void>(ocs::DataStore::replace_master_list<SGE_TYPE_EXECHOST>(
      ocs::tom::ListPtr<ocs::tom::EH>::create("hosts")));
   ocs::DataStore::free_all_master_lists();
   bool all_null = true;
   for (int type = SGE_TYPE_FIRST; type < SGE_TYPE_ALL; type++) {
      if (ocs::DataStore::master_list_raw(static_cast<sge_object_type>(type))) {
         all_null = false;
      }
   }
   CHECK(11, "free_all_master_lists(): every slot is nullptr", all_null);
}

// ---------------------------------------------------------------------------
// DataStore::master_list_raw() / master_list_raw_rw() / replace_master_list_raw()
// ---------------------------------------------------------------------------

/**
 * @brief The raw accessors of the master lists of a type given at run time
 *
 * An empty slot gives empty handles; replace_master_list_raw() hands the previous list back as an owner and
 * leaves the new one in the slot, which the typed and the raw accessors then see alike.
 */
static void
test_raw_master_list_access() {
   ocs::DataStore::free_master_list(SGE_TYPE_USERSET);
   CHECK(12, "master_list_raw(): empty slot gives an empty handle", !ocs::DataStore::master_list_raw(SGE_TYPE_USERSET));
   CHECK(13, "master_list_raw_rw(): empty slot gives an empty handle",
         !ocs::DataStore::master_list_raw_rw(SGE_TYPE_USERSET));

   auto users = ocs::tom::ListPtr<ocs::tom::US>::create("usersets");
   const ocs::tom::ListData *users_raw = users.get().raw();
   auto previous = ocs::DataStore::replace_master_list_raw(SGE_TYPE_USERSET, ocs::tom::RawListPtr(std::move(users)));
   CHECK(14, "replace_master_list_raw(): an empty slot gives an empty owner back", !previous);
   CHECK(15, "replace_master_list_raw(): the slot holds the new list",
         ocs::DataStore::master_list_raw(SGE_TYPE_USERSET).raw() == users_raw
         && ocs::DataStore::master_list<SGE_TYPE_USERSET>().raw() == users_raw);

   ocs::DataStore::master_list_raw_rw(SGE_TYPE_USERSET).push_back(ocs::tom::ElementPtr<ocs::tom::US>::create());
   CHECK(16, "master_list_raw_rw(): changes the list in the slot",
         ocs::DataStore::master_list<SGE_TYPE_USERSET>().size() == 1);

   previous = ocs::DataStore::replace_master_list_raw(SGE_TYPE_USERSET, ocs::tom::RawListPtr());
   CHECK(17, "replace_master_list_raw(): the previous list comes back as the owner", previous.get().raw() == users_raw);
   CHECK(18, "replace_master_list_raw(): an empty owner leaves the slot empty",
         !ocs::DataStore::master_list_raw(SGE_TYPE_USERSET));
}

// ---------------------------------------------------------------------------

/**
 * @brief Run the checks of this module test
 * @return 0 if every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_host_list_locate();
   test_free_master_lists();
   test_raw_master_list_access();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
