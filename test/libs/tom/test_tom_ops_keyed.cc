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
 * @brief Adding and deleting by key through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how elements are added and removed by their
 * key with the typed object model. Every scenario does the same thing twice:
 * a block marked CULL uses only the old functions of the list library, a block
 * marked TOM only the typed handles. Each block builds its own data and reduces
 * what came out to plain values (names, counts, flags), and the scenario
 * compares those. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old functions exist once per key type and once more for a list field
 * of an element (lAddElemStr, lAddElemUlong, lAddSubStr, lDelElemStr,
 * lDelSubStr, ...) and take the descriptor of the new element as an argument.
 * The typed forms are two templates whose key type and element type follow
 * from the field: emplace_back<F>(key), as std::list::emplace_back, and
 * erase<F>(key), as std::map::erase by key. For a list field, get_or_create<F>()
 * gives the list, created if the field holds none, and the two compose with it.
 *
 * One difference is deliberate: the old delete functions free a list that
 * becomes empty and set the reference to null, the typed erase leaves the empty
 * list in place, since a handle does not own the list it points to.
 *
 * In a Release build the last scenario times adding and deleting by name with
 * both forms and prints the ratio.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::CE;
using ocs::tom::EH;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::Host;
using ocs::tom::JB;
using ocs::tom::JRE;
using ocs::tom::ListPtr;

namespace {

   /** @brief The list lengths the scenarios run on: below and above the hash threshold of 16 */
   constexpr int Lengths[] = {5, 40};

   /**
    * @brief The name of complex entry or host i
    * @param prefix "c" or "host-"
    * @param i the number
    * @return the name
    */
   std::string name(const char *prefix, const int i) {
      return prefix + std::to_string(i);
   }

   /**
    * @brief Whether a function throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   template <typename Call>
   bool throws(Call call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

   /**
    * @brief What a block of the add scenario leaves behind, as plain values
    */
   struct AddResult {
      std::vector<std::string> entries;   ///< the names of the complex entries, in list order
      std::vector<std::uint32_t> refs;    ///< the job numbers of the job references, in list order
      std::vector<std::string> hosts;     ///< the host names, in list order
      bool all_found = false;             ///< whether a lookup by every key found its element

      /**
       * @brief Compare two results field by field
       * @param other the other result
       * @return true if both are equal
       */
      bool operator==(const AddResult &other) const = default;
   };

   /**
    * @brief What a block of a delete scenario leaves behind, as plain values
    */
   struct DeleteResult {
      int found = -1;                      ///< what deleting a name the list holds returned
      int missing = -1;                    ///< what deleting a name the list does not hold returned
      std::size_t size_after = 0;          ///< the number of elements after these two deletes

      /**
       * @brief Compare two results field by field
       * @param other the other result
       * @return true if both are equal
       */
      bool operator==(const DeleteResult &other) const = default;
   };

   /**
    * @brief The names in a list of complex entries, old interface
    * @param lp the list, may be nullptr
    * @return the names in list order
    */
   std::vector<std::string> old_names(const lList *lp) {
      std::vector<std::string> names;
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         names.emplace_back(lGetString(ep, CE_name));
      }
      return names;
   }

   /**
    * @brief The names in a list of complex entries, typed interface
    * @param entries the list
    * @return the names in list order
    */
   std::vector<std::string> typed_names(const ocs::tom::List<CE> entries) {
      std::vector<std::string> names;
      for (const auto entry : entries) {
         names.emplace_back(entry.get<CE_name>());
      }
      return names;
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Add by a string, a number and a host key
 */
void test_add() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lList *lp = nullptr; lAddElemStr(&lp, CE_name, "c1", CE_Type);
      //          the first add creates the list; the descriptor is passed every time
      // typed:   entries.emplace_back<CE_name>("c1") on a list created once
      // differs: the element type follows from the list, the key type from the field

      // --- CULL
      AddResult old_result;
      {
         lList *entries = nullptr;
         lList *refs = nullptr;
         lList *hosts = nullptr;
         for (int i = 1; i <= n; i++) {
            lAddElemStr(&entries, CE_name, name("c", i).c_str(), CE_Type);
            // old:   lAddElemUlong(&lp, JRE_job_number, i, JRE_Type)
            lAddElemUlong(&refs, JRE_job_number, i, JRE_Type);
            // old:   lAddElemHost(&lp, EH_name, "host-1", EH_Type)
            lAddElemHost(&hosts, EH_name, name("host-", i).c_str(), EH_Type);
         }
         old_result.entries = old_names(entries);
         for (const lListElem *ep = lFirst(refs); ep != nullptr; ep = lNext(ep)) {
            old_result.refs.push_back(lGetUlong(ep, JRE_job_number));
         }
         for (const lListElem *ep = lFirst(hosts); ep != nullptr; ep = lNext(ep)) {
            old_result.hosts.emplace_back(lGetHost(ep, EH_name));
         }
         old_result.all_found = true;
         for (int i = 1; i <= n; i++) {
            old_result.all_found = old_result.all_found && lGetElemStr(entries, CE_name, name("c", i).c_str()) != nullptr &&
                                   lGetElemUlong(refs, JRE_job_number, i) != nullptr &&
                                   lGetElemHost(hosts, EH_name, name("host-", i).c_str()) != nullptr;
         }
         lFreeList(&entries);
         lFreeList(&refs);
         lFreeList(&hosts);
      }

      // --- TOM
      AddResult typed_result;
      {
         ListPtr<CE> entries = ListPtr<CE>::create("entries");
         ListPtr<JRE> refs = ListPtr<JRE>::create("refs");
         ListPtr<EH> hosts = ListPtr<EH>::create("hosts");
         for (int i = 1; i <= n; i++) {
            entries->emplace_back<CE_name>(name("c", i).c_str());
            // typed: refs.emplace_back<JRE_job_number>(i)
            refs->emplace_back<JRE_job_number>(i);
            // typed: hosts.emplace_back<EH_name>(Host{"host-1"})
            hosts->emplace_back<EH_name>(Host{name("host-", i).c_str()});
         }
         typed_result.entries = typed_names(*entries);
         for (const auto ref : *refs) {
            typed_result.refs.push_back(ref.get<JRE_job_number>());
         }
         for (const auto host : *hosts) {
            typed_result.hosts.emplace_back(host.get<EH_name>().c_str());
         }
         typed_result.all_found = true;
         for (int i = 1; i <= n; i++) {
            typed_result.all_found = typed_result.all_found && entries->find_first<CE_name>(name("c", i).c_str()) &&
                                     refs->find_first<JRE_job_number>(i) &&
                                     hosts->find_first<EH_name>(Host{name("host-", i).c_str()});
         }
      }

      ok = ok && old_result == typed_result && old_result.all_found && old_result.entries.size() == static_cast<std::size_t>(n);
   }
   CHECK(1, "emplace_back adds by string, number and host key as lAddElemStr/Ulong/Host do, every key found", ok);
}

/**
 * @brief Add to a list field of an element
 */
void test_add_to_field() {
   // old:     lAddSubStr(job, VA_variable, "DISPLAY", JB_env_list, VA_Type)
   //          creates the environment list if the job has none
   // typed:   job.get_or_create<JB_env_list>().emplace_back<VA_variable>("DISPLAY")
   // differs: two steps instead of one function per key type, and no descriptor to pass

   // --- CULL
   bool old_empty_before;
   std::vector<std::string> old_variables;
   bool old_found;
   {
      lListElem *job = lCreateElem(JB_Type);
      old_empty_before = lGetList(job, JB_env_list) == nullptr;
      lAddSubStr(job, VA_variable, "DISPLAY", JB_env_list, VA_Type);
      lAddSubStr(job, VA_variable, "HOME", JB_env_list, VA_Type);
      for (const lListElem *ep = lFirst(lGetList(job, JB_env_list)); ep != nullptr; ep = lNext(ep)) {
         old_variables.emplace_back(lGetString(ep, VA_variable));
      }
      old_found = lGetSubStr(job, VA_variable, "HOME", JB_env_list) != nullptr;
      lFreeElem(&job);
   }

   // --- TOM
   bool typed_empty_before;
   std::vector<std::string> typed_variables;
   bool typed_found;
   {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      const Element<JB> job = jobs->push_back(ElementPtr<JB>::create());
      typed_empty_before = !job.get<JB_env_list>();
      job.get_or_create<JB_env_list>().emplace_back<VA_variable>("DISPLAY");
      job.get_or_create<JB_env_list>().emplace_back<VA_variable>("HOME");
      for (const auto variable : job.get<JB_env_list>()) {
         typed_variables.emplace_back(variable.get<VA_variable>());
      }
      typed_found = static_cast<bool>(job.get<JB_env_list>().find_first<VA_variable>("HOME"));
   }

   const bool same = old_empty_before && typed_empty_before && old_variables == typed_variables &&
                     old_variables.size() == 2 && old_found && typed_found;
   CHECK(2, "get_or_create and emplace_back add to a list field as lAddSubStr does, creating the list", same);
}

/**
 * @brief Delete by key, and the emptied list
 */
void test_delete() {
   bool ok = true;
   for (const int n : Lengths) {
      // old:     lDelElemStr(&lp, CE_name, "c1") returns 1 if it removed an element, 0 if none holds the key
      // typed:   entries.erase<CE_name>("c1") returns the number erased, 0 or 1, as std::map::erase

      // --- CULL
      DeleteResult old_result;
      bool old_list_freed;
      {
         lList *entries = nullptr;
         for (int i = 1; i <= n; i++) {
            lAddElemStr(&entries, CE_name, name("c", i).c_str(), CE_Type);
         }
         old_result.found = lDelElemStr(&entries, CE_name, "c1");
         old_result.missing = lDelElemStr(&entries, CE_name, "none");
         old_result.size_after = lGetNumberOfElem(entries);
         for (int i = 2; i <= n; i++) {
            lDelElemStr(&entries, CE_name, name("c", i).c_str());
         }
         // differs: the old function freed the emptied list and set the reference to nullptr
         old_list_freed = entries == nullptr;
         lFreeList(&entries);
      }

      // --- TOM
      DeleteResult typed_result;
      bool typed_list_kept_empty;
      {
         ListPtr<CE> entries = ListPtr<CE>::create("entries");
         for (int i = 1; i <= n; i++) {
            entries->emplace_back<CE_name>(name("c", i).c_str());
         }
         typed_result.found = static_cast<int>(entries->erase<CE_name>("c1"));
         typed_result.missing = static_cast<int>(entries->erase<CE_name>("none"));
         typed_result.size_after = entries->size();
         for (int i = 2; i <= n; i++) {
            entries->erase<CE_name>(name("c", i).c_str());
         }
         // differs: the typed list stays, empty, since the handle that erased does not own it
         typed_list_kept_empty = entries && entries->empty();
      }

      ok = ok && old_result == typed_result && old_result.found == 1 && old_result.missing == 0 &&
           old_result.size_after == static_cast<std::size_t>(n - 1) && old_list_freed && typed_list_kept_empty;
   }
   CHECK(3, "erase by key removes as lDelElemStr does; the old function frees an emptied list, the typed one keeps it",
         ok);
}

/**
 * @brief Delete in a list field of an element
 */
void test_delete_in_field() {
   // old:     lDelSubStr(job, VA_variable, "DISPLAY", JB_env_list)
   // typed:   job.get<JB_env_list>().erase<VA_variable>("DISPLAY")
   // differs: as for every delete, the old function frees the emptied list in the field

   // --- CULL
   int old_deleted;
   bool old_field_null;
   {
      lListElem *job = lCreateElem(JB_Type);
      lAddSubStr(job, VA_variable, "DISPLAY", JB_env_list, VA_Type);
      old_deleted = lDelSubStr(job, VA_variable, "DISPLAY", JB_env_list);
      old_field_null = lGetList(job, JB_env_list) == nullptr;
      lFreeElem(&job);
   }

   // --- TOM
   int typed_deleted;
   bool typed_list_empty;
   {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      const Element<JB> job = jobs->push_back(ElementPtr<JB>::create());
      job.get_or_create<JB_env_list>().emplace_back<VA_variable>("DISPLAY");
      typed_deleted = static_cast<int>(job.get<JB_env_list>().erase<VA_variable>("DISPLAY"));
      typed_list_empty = job.get<JB_env_list>() && job.get<JB_env_list>().empty();
   }

   CHECK(4, "erase in a list field removes as lDelSubStr does",
         old_deleted == 1 && typed_deleted == old_deleted && old_field_null && typed_list_empty);
}

/**
 * @brief Errors: a null key and a duplicate key in a unique field
 */
void test_errors() {
   // old:     lAddElemStr(&lp, CE_name, nullptr, CE_Type) returns nullptr
   // typed:   emplace_back<CE_name>(nullptr) throws a logic error with reason NullKey

   // --- CULL
   bool old_refused;
   {
      lList *entries = nullptr;
      old_refused = lAddElemStr(&entries, CE_name, nullptr, CE_Type) == nullptr && entries == nullptr;
      lFreeList(&entries);
   }

   // --- TOM
   ListPtr<CE> entries = ListPtr<CE>::create("entries");
   entries->emplace_back<CE_name>("c1");
   const bool typed_refused =
      throws([&] { entries->emplace_back<CE_name>(nullptr); }, FieldException::Reason::NullKey) &&
      entries->size() == 1;
   CHECK(5, "a null key throws NullKey where the old add returns nullptr", old_refused && typed_refused);

#ifdef ENABLE_DEBUG_CHECKS
   // old:     lAddElemStr() with a name the list holds aborts in a debug build (N-H-18); not run here
   // typed:   emplace_back<CE_name>() throws DuplicateKey, and the list is unchanged

   // --- TOM
   const bool duplicate = throws([&] { entries->emplace_back<CE_name>("c1"); }, FieldException::Reason::DuplicateKey) &&
                          entries->size() == 1;
   CHECK(6, "a duplicate key in a unique field throws DuplicateKey in a debug build, the list unchanged", duplicate);
#else
   CHECK(6, "a duplicate key is checked in a debug build only", true);
#endif
}

/**
 * @brief In a Release build, time adding and deleting by name with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int n = 10000;
   constexpr int rounds = 20;
   std::vector<std::string> names;
   for (int i = 1; i <= n; i++) {
      names.push_back(name("c", i));
   }

   // --- CULL
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      lList *lp = nullptr;
      for (int i = 0; i < n; i++) {
         lAddElemStr(&lp, CE_name, names[i].c_str(), CE_Type);
      }
      for (int i = 0; i < n; i++) {
         lDelElemStr(&lp, CE_name, names[i].c_str());
      }
   }
   // --- TOM
   auto t1 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      ListPtr<CE> entries = ListPtr<CE>::create("entries");
      for (int i = 0; i < n; i++) {
         entries->emplace_back<CE_name>(names[i].c_str());
      }
      for (int i = 0; i < n; i++) {
         entries->erase<CE_name>(names[i].c_str());
      }
   }
   auto t2 = std::chrono::steady_clock::now();

   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   printf("timing, %d rounds of adding and deleting %d complex entries by name:\n", rounds, n);
   printf("   add/delete: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t1, t2), ms(t1, t2) / ms(t0, t1));
   CHECK(7, "the timing loops ran", ms(t0, t2) > 0);
#endif
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   lInit(nmv);
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"OPS-KEYED-01", "add", "emplace_back adds by key as the old add functions do",
       {"add complex entries, job references and hosts by key to lists of 5 and 40"}, "same lists, every key found",
       {{"OPS-01", "covers"}}, {1}, test_add},
      {"OPS-KEYED-02", "add to a field", "get_or_create and emplace_back add to a list field as lAddSubStr does",
       {"add two variables to the environment of a job without one"}, "list created, both found",
       {{"OPS-01", "covers"}}, {2}, test_add_to_field},
      {"OPS-KEYED-03", "delete", "erase by key removes as lDelElemStr does",
       {"delete found and missing names, then every name"}, "same results; the emptied old list is freed",
       {{"OPS-01", "covers"}}, {3}, test_delete},
      {"OPS-KEYED-04", "delete in a field", "erase in a list field removes as lDelSubStr does",
       {"delete the only variable of a job's environment"}, "removed; the old field is null, the typed list empty",
       {{"OPS-01", "covers"}}, {4}, test_delete_in_field},
      {"OPS-KEYED-05", "errors", "a null key and a duplicate key throw where the old functions return or abort",
       {"add by a null name; add a name the list holds"}, "NullKey and, in a debug build, DuplicateKey thrown",
       {{"OPS-01", "covers"}}, {5, 6}, test_errors},
      {"OPS-KEYED-06", "timing", "a Release build times both forms",
       {"add and delete 10 000 complex entries by name, 20 times, with both forms"}, "ratio printed",
       {{"OPS-01", "covers"}}, {7}, test_timing},
   });
}
