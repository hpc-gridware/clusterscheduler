#pragma once
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

/**
 * @file ocs_test_model.h
 * @brief An object model for the object types a module test writes by hand
 *
 * The tests of the list library define their own types with LISTDEF and
 * NAMEDEF instead of the generated ones. The list library takes the field
 * names and the static descriptors of the types from the installed object
 * model (tom/ocs_Model.h), so such a test installs a model of its types at
 * start: install_test_model() builds the field tables from the descriptors and
 * the name tables and installs them. The model replaces the CULL name space
 * that these tests bound with lInit() before.
 */

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "tom/cull_types.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_Model.h"

namespace ocs::module_test {

   /** @brief An object type written by hand in a module test */
   struct TestType {
      const char *name;            ///< the name of the type, e.g. "TEST"
      const lDescr *descr;         ///< its descriptor, built with LISTDEF
      const char *const *names;    ///< its field names in field order, built with NAMEDEF
      std::size_t size;            ///< the number of fields
   };

   /**
    * @brief The flags of the typed object model for the attribute bits of a descriptor entry
    * @param mt the type and attribute bits of the entry
    * @return the flags
    */
   inline ocs::tom::Flag test_flags_of(const int mt) {
      const std::pair<int, ocs::tom::Flag> map[] = {
         {CULL_PRIMARY_KEY, ocs::tom::Flag::PrimaryKey}, {CULL_HASH, ocs::tom::Flag::Hash},
         {CULL_UNIQUE, ocs::tom::Flag::Unique}, {CULL_NO_TRANSFER, ocs::tom::Flag::NoTransfer},
         {CULL_CONFIGURE, ocs::tom::Flag::Configure}, {CULL_SPOOL, ocs::tom::Flag::Spool},
         {CULL_SUBLIST, ocs::tom::Flag::SpoolSublist}, {CULL_SPOOL_PROJECT, ocs::tom::Flag::SpoolProject},
      };
      ocs::tom::Flag flags = ocs::tom::Flag::None;
      for (const auto &[bit, flag] : map) {
         if ((mt & bit) != 0) {
            flags = flags | flag;
         }
      }
      return flags;
   }

   /**
    * @brief Install an object model of the types a module test writes by hand
    *
    * The field tables are built from the descriptors and the name tables and
    * live for the rest of the program, as install_model() requires. A model
    * the list library rejects is reported on stderr and ends the test with
    * abort(): a test that went on without its model would check nothing.
    *
    * @param types the types of the test
    * @return true; a rejected model does not return
    */
   inline bool install_test_model(const std::initializer_list<TestType> types) {
      // the model keeps views of the tables, so they are never freed
      auto *tables = new std::vector<ocs::tom::TypeTable>;
      tables->reserve(types.size());
      for (const TestType &type : types) {
         auto *fields = new ocs::tom::FieldInfo[type.size];
         for (std::size_t i = 0; i < type.size; i++) {
            fields[i] = {type.descr[i].nm, type.names[i], ocs::tom::field_type_of(type.descr[i].mt),
                         test_flags_of(type.descr[i].mt)};
         }
         tables->push_back({type.name, type.descr, fields, type.size});
      }
      std::string reason;
      const bool installed = ocs::tom::install_model({*tables, {}}, &reason);
      if (!installed) {
         std::fprintf(stderr, "the object model of the test is rejected: %s\n", reason.c_str());
         std::abort();
      }
      return installed;
   }

} // namespace ocs::module_test
