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
 * @brief The helper operations on whole lists, selections and the debug text through the typed handles and
 *        through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how names are parsed into lists, lists are
 * split, made unique and compared, selections merged, trees counted and
 * objects written for debugging with the typed object model. Every scenario
 * does the same thing twice: a block marked CULL uses only the old functions
 * of the list library, a block marked TOM only the typed handles. Each block
 * builds its own data and reduces what came out to plain values (names,
 * numbers, text), and the scenario compares those. Each step carries a
 * comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * In a Release build the last scenario times parsing, splitting and making
 * unique with both forms and prints the ratios.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cull/cull_db.h"
#include "cull/cull_tree.h"
#include "cull/cull_what_print.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::ConstList;
using ocs::tom::EH;
using ocs::tom::Element;
using ocs::tom::field;
using ocs::tom::Host;
using ocs::tom::JAT;
using ocs::tom::JB;
using ocs::tom::ListCoreError;
using ocs::tom::ListPtr;
using ocs::tom::NoneName;
using ocs::tom::ST;
using ocs::tom::STN;
using ocs::tom::TEST;
using ocs::tom::What;
using ocs::tom::what;

namespace {

   /** @brief The number of jobs the split and the timing work on */
   constexpr std::uint32_t Jobs = 1000;

   /**
    * @brief The names of an old list, separated by blanks
    * @param lp the list
    * @param nm a string or host field
    * @return the names
    */
   std::string names(const lList *lp, const int nm) {
      std::string text;
      for_each_ep_lv(ep, lp) {
         const char *name = lGetType(lGetListDescr(lp), nm) == lHostT ? lGetHost(ep, nm) : lGetString(ep, nm);
         text += (text.empty() ? "" : " ") + std::string(name != nullptr ? name : "(null)");
      }
      return text;
   }

   /**
    * @brief The names of a typed string list, separated by blanks
    * @param names the list
    * @return the names
    */
   std::string names(const ConstList<ST> list) {
      std::string text;
      for (const ConstElement<ST> st : list) {
         const char *name = st.get<ST_name>();
         text += (text.empty() ? "" : " ") + std::string(name != nullptr ? name : "(null)");
      }
      return text;
   }

   /**
    * @brief The names of a typed host list, separated by blanks
    * @param list the list
    * @return the names
    */
   std::string names(const ConstList<EH> list) {
      std::string text;
      for (const ConstElement<EH> eh : list) {
         text += (text.empty() ? "" : " ") + std::string(eh.get<EH_name>().c_str());
      }
      return text;
   }

   /**
    * @brief The host names of a typed list of test rows, separated by blanks
    * @param list the list
    * @return the names
    */
   std::string names(const ConstList<TEST> list) {
      std::string text;
      for (const ConstElement<TEST> row : list) {
         text += (text.empty() ? "" : " ") + std::string(row.get<TEST_host>().c_str());
      }
      return text;
   }

   /**
    * @brief The text a writer writes into a stream
    * @param write the writer
    * @return the text
    */
   template <typename Write>
   std::string text_of(Write write) {
      char *buf = nullptr;
      size_t size = 0;
      FILE *fp = open_memstream(&buf, &size);
      write(fp);
      fclose(fp);
      std::string text(buf, size);
      free(buf);
      return text;
   }

   /**
    * @brief Build the jobs as an old list
    * @return the list
    */
   lList *old_jobs() {
      lList *lp = lCreateList("jobs", JB_Type);
      for (std::uint32_t number = 1; number <= Jobs; number++) {
         lListElem *ep = lAddElemUlong(&lp, JB_job_number, number, JB_Type);
         lSetString(ep, JB_owner, ("user" + std::to_string(number % 7)).c_str());
      }
      return lp;
   }

   /**
    * @brief Build the jobs as a typed list
    * @return the list
    */
   ListPtr<JB> typed_jobs() {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      for (std::uint32_t number = 1; number <= Jobs; number++) {
         jobs->emplace_back<JB_job_number>(number).set<JB_owner>(("user" + std::to_string(number % 7)).c_str());
      }
      return jobs;
   }

   /**
    * @brief The job numbers of an old list, summed
    * @param lp the list
    * @return the sum
    */
   std::uint64_t sum(const lList *lp) {
      std::uint64_t n = 0;
      for_each_ep_lv(ep, lp) {
         n += lGetUlong(ep, JB_job_number);
      }
      return n;
   }

   /**
    * @brief The job numbers of a typed list, summed
    * @param jobs the list
    * @return the sum
    */
   std::uint64_t sum(const ConstList<JB> jobs) {
      std::uint64_t n = 0;
      for (const ConstElement<JB> job : jobs) {
         n += job.get<JB_job_number>();
      }
      return n;
   }

} // namespace

/**
 * @brief Parse names into a list, with and without "none"
 */
void test_from_string() {
   // --- CULL
   std::vector<std::string> old_seen;
   lList *lp = nullptr;
   // old:     lString2List(s, &lp, ST_Type, ST_name, delimiters)
   lString2List("a b,a c", &lp, ST_Type, ST_name, ", ");
   old_seen.push_back(names(lp, ST_name));
   lFreeList(&lp);
   lString2List("h2 h1 H2", &lp, EH_Type, EH_name, " ");
   old_seen.push_back(names(lp, EH_name));
   lFreeList(&lp);
   // old:     lString2ListNone(s, &lp, ST_Type, ST_name, delimiters) == 1 for "none" beside other names
   lString2ListNone("NONE", &lp, ST_Type, ST_name, " ");
   old_seen.push_back(lp == nullptr ? "empty" : names(lp, ST_name));
   const bool old_mixed = lString2ListNone("a none", &lp, ST_Type, ST_name, " ") == 1 && lp == nullptr;

   // --- TOM
   std::vector<std::string> typed_seen;
   // typed:   ListPtr<ST>::from_string<ST_name>(s, delimiters)
   typed_seen.push_back(names(*ListPtr<ST>::from_string<ST_name>("a b,a c", ", ")));
   typed_seen.push_back(names(*ListPtr<EH>::from_string<EH_name>("h2 h1 H2", " ")));
   // typed:   from_string<ST_name>(s, delimiters, NoneName::Empty)
   // differs: "none" beside other names throws rather than giving 1
   const ListPtr<ST> none = ListPtr<ST>::from_string<ST_name>("NONE", " ", NoneName::Empty);
   typed_seen.push_back(none ? names(*none) : "empty");
   bool typed_mixed = false;
   try {
      static_cast<void>(ListPtr<ST>::from_string<ST_name>("a none", " ", NoneName::Empty));
   } catch (const ListCoreError &) {
      typed_mixed = true;
   }

   for (std::size_t i = 0; i < old_seen.size(); i++) {
      printf("   %-12s old \"%s\"   typed \"%s\"\n", i < 2 ? "names" : "none", old_seen[i].c_str(),
             typed_seen[i].c_str());
   }
   CHECK(1, "from_string<F>() gives the lists of lString2List and lString2ListNone",
         old_seen == typed_seen && old_seen[0] == "a b c" && old_seen[1] == "h2 h1" && old_seen[2] == "empty" &&
         old_mixed && typed_mixed);
}

/**
 * @brief Split a list by a condition, make a host list unique, take out the names two lists share
 */
void test_split_unique_common() {
   // --- CULL
   lList *jobs = old_jobs();
   lList *rest = nullptr;
   // old:     lSplit(&jobs, &rest, "rest", lWhere("%T(%I <= %u)", JB_Type, JB_job_number, 300))
   lCondition *cp = lWhere("%T(%I <= %u)", JB_Type, JB_job_number, 300U);
   lSplit(&jobs, &rest, "rest", cp);
   lFreeWhere(&cp);
   const std::vector<std::uint64_t> old_split = {lGetNumberOfElem(jobs), sum(jobs), lGetNumberOfElem(rest), sum(rest)};
   lFreeList(&jobs);
   lFreeList(&rest);
   lList *hosts = nullptr;
   for (const char *h : {"h3", "h1", "h3", "h2", "h1"}) {
      lListElem *ep = lCreateElem(TEST_Type);
      lSetHost(ep, TEST_host, h);
      if (hosts == nullptr) {
         hosts = lCreateList("hosts", TEST_Type);
      }
      lAppendElem(hosts, ep);
   }
   // old:     lUniqHost(hosts, TEST_host)
   lUniqHost(hosts, TEST_host);
   const std::string old_unique = names(hosts, TEST_host);
   lFreeList(&hosts);
   lList *a = nullptr;
   lList *b = nullptr;
   lString2List("a b c", &a, ST_Type, ST_name, " ");
   lString2List("b c d", &b, ST_Type, ST_name, " ");
   // old:     lDiffListStr(ST_name, &a, &b)
   lDiffListStr(ST_name, &a, &b);
   const std::string old_common = names(a, ST_name) + " | " + names(b, ST_name);
   lFreeList(&a);
   lFreeList(&b);

   // --- TOM
   const ListPtr<JB> typed = typed_jobs();
   // typed:   jobs->split_off(field<JB_job_number>() <= 300, "rest")
   // differs: the elements taken out come back as an owning handle; an empty list stays
   const ListPtr<JB> typed_rest = typed->split_off(field<JB_job_number>() <= 300U, "rest");
   const std::vector<std::uint64_t> typed_split = {typed->size(), sum(*typed), typed_rest->size(), sum(*typed_rest)};
   const ListPtr<TEST> typed_hosts = ListPtr<TEST>::create("hosts");
   for (const char *h : {"h3", "h1", "h3", "h2", "h1"}) {
      typed_hosts->push_back(ocs::tom::ElementPtr<TEST>::create()).set<TEST_host>(Host{h});
   }
   // typed:   hosts->unique<TEST_host>()
   typed_hosts->unique<TEST_host>();
   const std::string typed_unique = names(*typed_hosts);
   const ListPtr<ST> ta = ListPtr<ST>::from_string<ST_name>("a b c", " ");
   const ListPtr<ST> tb = ListPtr<ST>::from_string<ST_name>("b c d", " ");
   // typed:   a->remove_common<ST_name>(*b)
   ta->remove_common<ST_name>(*tb);
   const std::string typed_common = names(*ta) + " | " + names(*tb);

   printf("   split   old %llu/%llu %llu/%llu   typed %llu/%llu %llu/%llu\n",
          (unsigned long long) old_split[0], (unsigned long long) old_split[1], (unsigned long long) old_split[2],
          (unsigned long long) old_split[3], (unsigned long long) typed_split[0], (unsigned long long) typed_split[1],
          (unsigned long long) typed_split[2], (unsigned long long) typed_split[3]);
   printf("   unique  old \"%s\"   typed \"%s\"\n", old_unique.c_str(), typed_unique.c_str());
   printf("   common  old \"%s\"   typed \"%s\"\n", old_common.c_str(), typed_common.c_str());
   CHECK(2, "split_off(), unique<F>() and remove_common<F>() give the results of lSplit, lUniqHost and lDiffListStr",
         old_split == typed_split && old_split[0] == 300 && old_unique == typed_unique && old_unique == "h1 h2 h3" &&
         old_common == typed_common && old_common == "a | d");
}

/**
 * @brief Merge two selections and write them, a list and a condition as debug text
 */
void test_merge_and_print() {
   // --- CULL
   // old:     lMergeWhat(&what1, &what2)
   lEnumeration *merged = lWhat("%T(%I%I->%T(%I))", JB_Type, JB_job_number, JB_ja_tasks, JAT_Type, JAT_task_number);
   lEnumeration *other = lWhat("%T(%I%I->%T(%I))", JB_Type, JB_owner, JB_ja_tasks, JAT_Type, JAT_status);
   lMergeWhat(&merged, &other);
   // old:     lWriteWhatTo(what, fp), lWriteListTo(lp, fp), lWriteWhereTo(cp, fp)
   const std::string old_what = text_of([&](FILE *fp) { lWriteWhatTo(merged, fp); });
   lFreeWhat(&merged);
   lList *jobs = old_jobs();
   lCondition *cp = lWhere("%T(%I == %u || %I == %s)", JB_Type, JB_job_number, 7U, JB_owner, "user1");
   lEnumeration *fields = lWhat("%T(%I%I)", JB_Type, JB_job_number, JB_owner);
   lList *few = lSelect("few", jobs, cp, fields);
   lFreeWhat(&fields);
   const std::string old_list = text_of([&](FILE *fp) { lWriteListTo(few, fp); });
   const std::string old_where = text_of([&](FILE *fp) { lWriteWhereTo(cp, fp); });
   lFreeList(&few);
   lFreeWhere(&cp);
   lFreeList(&jobs);

   // --- TOM
   // typed:   std::move(what1).merge(std::move(what2))
   // differs: a field both select keeps the nested selections of both, as lMergeWhat since CS-3077
   What<JB> typed_merged = what<JB_job_number>().with<JB_ja_tasks>(what<JAT_task_number>());
   typed_merged = std::move(typed_merged).merge(what<JB_owner>().with<JB_ja_tasks>(what<JAT_status>()));
   // typed:   what.print(fp), list.print(fp), where.print(fp)
   const std::string typed_what = text_of([&](FILE *fp) { typed_merged.print(fp); });
   const ListPtr<JB> typed = typed_jobs();
   const auto where = field<JB_job_number>() == 7U || field<JB_owner>() == "user1";
   const ListPtr<JB> typed_few = typed->select(where, what<JB_job_number, JB_owner>(), "few");
   const std::string typed_list = text_of([&](FILE *fp) { typed_few->print(fp); });
   const std::string typed_where = text_of([&](FILE *fp) { where.print(fp); });

   printf("%s", typed_what.c_str());
   printf("%s", typed_where.c_str());
   CHECK(3, "merge() and print() give the selection and the text of lMergeWhat, lWriteWhatTo, lWriteListTo and "
            "lWriteWhereTo",
         old_what == typed_what && old_what.find("JAT_status") != std::string::npos && old_list == typed_list &&
         old_list.find("#Elements: 144") != std::string::npos && old_where == typed_where && !old_where.empty());
}

/**
 * @brief Count the nodes and leaves of a tree
 */
void test_tree() {
   // --- CULL
   lList *tree = lCreateList("tree", STN_Type);
   lListElem *root = lAddElemStr(&tree, STN_name, "root", STN_Type);
   lListElem *left = lAddSubStr(root, STN_name, "left", STN_children, STN_Type);
   lAddSubStr(root, STN_name, "right", STN_children, STN_Type);
   lAddSubStr(left, STN_name, "leaf", STN_children, STN_Type);
   // old:     lGetNumberOfNodes(root, nullptr, STN_children), lGetNumberOfLeafs(nullptr, tree, STN_children)
   const std::vector<int> old_counts = {lGetNumberOfNodes(root, nullptr, STN_children),
                                        lGetNumberOfLeafs(root, nullptr, STN_children),
                                        lGetNumberOfNodes(nullptr, tree, STN_children),
                                        lGetNumberOfLeafs(nullptr, tree, STN_children)};
   lFreeList(&tree);

   // --- TOM
   const ListPtr<STN> typed = ListPtr<STN>::create("tree");
   const Element<STN> typed_root = typed->emplace_back<STN_name>("root");
   const Element<STN> typed_left = typed_root.get_or_create<STN_children>().emplace_back<STN_name>("left");
   typed_root.get_or_create<STN_children>().emplace_back<STN_name>("right");
   typed_left.get_or_create<STN_children>().emplace_back<STN_name>("leaf");
   // typed:   root.count_nodes<STN_children>(), tree->count_leaves<STN_children>()
   const std::vector<int> typed_counts = {typed_root.count_nodes<STN_children>(),
                                          typed_root.count_leaves<STN_children>(),
                                          typed->count_nodes<STN_children>(), typed->count_leaves<STN_children>()};

   printf("   nodes and leaves  old %d %d %d %d   typed %d %d %d %d\n", old_counts[0], old_counts[1], old_counts[2],
          old_counts[3], typed_counts[0], typed_counts[1], typed_counts[2], typed_counts[3]);
   CHECK(4, "count_nodes<F>() and count_leaves<F>() count as lGetNumberOfNodes and lGetNumberOfLeafs",
         old_counts == typed_counts && old_counts == std::vector<int>({4, 2, 4, 2}));
}

/**
 * @brief In a Release build, time parsing, splitting and making unique with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int rounds = 200;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   std::string text;
   for (std::uint32_t i = 0; i < Jobs; i++) {
      text += "host" + std::to_string(i % 500) + " ";
   }

   // --- CULL
   std::size_t old_n = 0;
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      lList *lp = nullptr;
      lString2List(text.c_str(), &lp, EH_Type, EH_name, " ");
      old_n += lGetNumberOfElem(lp);
      lFreeList(&lp);
   }
   auto t1 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      lList *jobs = old_jobs();
      lList *rest = nullptr;
      lCondition *cp = lWhere("%T(%I <= %u)", JB_Type, JB_job_number, Jobs / 2);
      lSplit(&jobs, &rest, "rest", cp);
      lUniqStr(rest, JB_owner);
      old_n += lGetNumberOfElem(rest);
      lFreeWhere(&cp);
      lFreeList(&jobs);
      lFreeList(&rest);
   }
   auto t2 = std::chrono::steady_clock::now();

   // --- TOM
   std::size_t typed_n = 0;
   auto t3 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      typed_n += ListPtr<EH>::from_string<EH_name>(text.c_str(), " ")->size();
   }
   auto t4 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      const ListPtr<JB> jobs = typed_jobs();
      const ListPtr<JB> rest = jobs->split_off(field<JB_job_number>() <= Jobs / 2, "rest");
      rest->unique<JB_owner>();
      typed_n += rest->size();
   }
   auto t5 = std::chrono::steady_clock::now();

   printf("timing, %d rounds:\n", rounds);
   printf("   from_string:        old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t3, t4),
          ms(t3, t4) / ms(t0, t1));
   printf("   split and unique:   old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t1, t2), ms(t4, t5),
          ms(t4, t5) / ms(t1, t2));
   CHECK(5, "the timing loops give the same numbers of elements", old_n == typed_n);
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
   return ocs::module_test::run(argc, argv, {
      {"OPS-REST-01", "from_string", "from_string<F>() parses names as lString2List and lString2ListNone do",
       {"parse names with a repeated one, host names, \"none\" alone and beside a name"},
       "the same lists; ListCoreError thrown where 1 was given", {{"OPS-01", "covers"}}, {1}, test_from_string},
      {"OPS-REST-02", "split", "split_off(), unique<F>() and remove_common<F>() as lSplit, lUniqHost, lDiffListStr",
       {"split 1 000 jobs by number, make five host names unique, take out the names two lists share"},
       "the same lists", {{"OPS-01", "covers"}}, {2}, test_split_unique_common},
      {"OPS-REST-03", "merge and print", "merge() and print() as lMergeWhat and the lWrite*To functions",
       {"merge two job selections with nested task selections, print it, a selected list and the condition"},
       "the same selection and text", {{"OPS-01", "covers"}}, {3}, test_merge_and_print},
      {"OPS-REST-04", "tree", "count_nodes<F>() and count_leaves<F>() as lGetNumberOfNodes and lGetNumberOfLeafs",
       {"count a share tree of four nodes from the root and from the list"}, "4 nodes, 2 leaves",
       {{"OPS-01", "covers"}}, {4}, test_tree},
      {"OPS-REST-05", "timing", "a Release build times both forms",
       {"parse 1 000 host names 200 times; split 1 000 jobs and make the rest unique by owner 200 times"},
       "ratios printed", {{"OPS-01", "covers"}}, {5}, test_timing},
   });
}
