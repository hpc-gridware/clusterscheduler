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
 * @brief Dumping as text and element files through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how objects are written as text, read back,
 * and kept in files with the typed object model. Every scenario does the same
 * thing twice: a block marked CULL uses only the old functions of the list
 * library, a block marked TOM only the typed handles. Each block builds its
 * own data and reduces what came out to plain values (the text written, the
 * job numbers and owners read back), and the scenario compares those. Each
 * step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old interface returns -1 or nullptr and leaves the reason in the error
 * number; the typed form throws. Both write the same text.
 *
 * In a Release build the last scenario times dumping and reading back, and
 * writing and reading an element file, with both forms and prints the ratios.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include "cull/cull_dump_scan.h"
#include "cull/cull_file.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::JB;
using ocs::tom::ListCoreError;
using ocs::tom::ListPtr;
using ocs::tom::US;

namespace {

   /** @brief The number of jobs the scenarios write */
   constexpr std::uint32_t Jobs = 100;

   /** @brief A job reduced to plain values */
   struct Plain {
      std::uint32_t number;   ///< JB_job_number
      std::string owner;      ///< JB_owner, empty if not set

      /**
       * @brief Equal if number and owner are equal
       * @param other the other job
       * @return true if equal
       */
      bool operator==(const Plain &other) const = default;
   };

   /**
    * @brief The owner of a job
    * @param number the job number
    * @return the owner
    */
   std::string owner_of(const std::uint32_t number) {
      return "user" + std::to_string(number % 7);
   }

   /**
    * @brief The jobs of an old list as plain values
    * @param lp the list
    * @return the values
    */
   std::vector<Plain> plain(const lList *lp) {
      std::vector<Plain> jobs;
      for_each_ep_lv(ep, lp) {
         const char *owner = lGetString(ep, JB_owner);
         jobs.push_back({lGetUlong(ep, JB_job_number), owner != nullptr ? owner : ""});
      }
      return jobs;
   }

   /**
    * @brief The jobs of a typed list as plain values
    * @param jobs the list
    * @return the values
    */
   std::vector<Plain> plain(const ListPtr<JB> &jobs) {
      std::vector<Plain> values;
      for (const ConstElement<JB> job : *jobs) {
         const char *owner = job.get<JB_owner>();
         values.push_back({job.get<JB_job_number>(), owner != nullptr ? owner : ""});
      }
      return values;
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
    * @brief Read from a text as from a file
    * @param text the text
    * @param read the reader
    */
   template <typename Read>
   void from_text(const std::string &text, Read read) {
      FILE *fp = fmemopen(const_cast<char *>(text.data()), text.size(), "r");
      read(fp);
      fclose(fp);
   }

   /**
    * @brief Build the jobs as an old list
    * @return the list
    */
   lList *old_jobs() {
      lList *lp = lCreateList("jobs", JB_Type);
      for (std::uint32_t number = 1; number <= Jobs; number++) {
         lListElem *ep = lAddElemUlong(&lp, JB_job_number, number, JB_Type);
         lSetString(ep, JB_owner, owner_of(number).c_str());
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
         jobs->emplace_back<JB_job_number>(number).set<JB_owner>(owner_of(number).c_str());
      }
      return jobs;
   }

   /**
    * @brief Whether a function throws an exception of a given type
    * @param call the call
    * @return true if it threw one
    */
   template <typename E, typename Call>
   bool throws(Call call) {
      try {
         call();
      } catch (const E &) {
         return true;
      }
      return false;
   }

} // namespace

/**
 * @brief Dump a job list as text and read it back
 */
void test_list() {
   // --- CULL
   lList *lp = old_jobs();
   // old:     lDumpList(fp, lp, 0), lUndumpList(fp, nullptr, nullptr)
   const std::string old_text = text_of([&](FILE *fp) { lDumpList(fp, lp, 0); });
   std::vector<Plain> old_values;
   from_text(old_text, [&](FILE *fp) {
      lList *back = lUndumpList(fp, nullptr, nullptr);
      old_values = plain(back);
      lFreeList(&back);
   });
   lFreeList(&lp);

   // --- TOM
   const ListPtr<JB> jobs = typed_jobs();
   // typed:   jobs->dump(fp), ListPtr<JB>::undump(fp)
   // differs: undump() throws instead of giving nullptr, and checks the type of the list in the text
   const std::string typed_text = text_of([&](FILE *fp) { jobs->dump(fp); });
   std::vector<Plain> typed_values;
   from_text(typed_text, [&](FILE *fp) { typed_values = plain(ListPtr<JB>::undump(fp)); });

   CHECK(1, "dump() writes the text lDumpList writes, and undump() reads what lUndumpList reads",
         old_text == typed_text && old_values == typed_values && old_values.size() == Jobs);
}

/**
 * @brief Dump one element as text and read it back
 *
 * A quote inside a string is escaped. Both forms lose a quote at the start or
 * the end of a string, and one of two quotes in a row: the dump splits the
 * string at its quotes with sge_strtok(), which drops empty pieces.
 */
void test_element() {
   // --- CULL
   lListElem *ep = lCreateElem(JB_Type);
   lSetUlong(ep, JB_job_number, 42);
   lSetString(ep, JB_owner, "say \"hi\" now");
   // old:     lDumpElemFp(fp, ep, 0), lUndumpElemFp(fp, JB_Type)
   const std::string old_text = text_of([&](FILE *fp) { lDumpElemFp(fp, ep, 0); });
   lFreeElem(&ep);

   // --- TOM
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(42);
   job->set<JB_owner>("say \"hi\" now");
   // typed:   job->dump(fp), ElementPtr<JB>::undump(fp)
   // differs: the type is that of the handle; the text of an element does not name one
   const std::string typed_text = text_of([&](FILE *fp) { job->dump(fp); });
   std::string owner;
   std::uint32_t number = 0;
   from_text(typed_text, [&](FILE *fp) {
      ElementPtr<JB> back = ElementPtr<JB>::undump(fp);
      number = back->get<JB_job_number>();
      owner = back->get<JB_owner>();
   });

   CHECK(2, "an element dumps to the text of lDumpElemFp, quotes escaped, and reads back with its values",
         old_text == typed_text && number == 42 && owner == "say \"hi\" now");
}

/**
 * @brief Write a job into a file and read it back
 */
void test_file() {
   char dir[] = "/tmp/test_tom_ops_dump_XXXXXX";
   const bool made = mkdtemp(dir) != nullptr;

   // --- CULL
   lListElem *ep = lCreateElem(JB_Type);
   lSetUlong(ep, JB_job_number, 7);
   lSetString(ep, JB_owner, "alice");
   // old:     lWriteElemToDisk(ep, dir, name, "job"), lReadElemFromDisk(dir, name, JB_Type, "job")
   const bool old_written = lWriteElemToDisk(ep, dir, "old", "job") == 0;
   lListElem *old_back = lReadElemFromDisk(dir, "old", JB_Type, "job");
   const bool old_read = old_back != nullptr && lGetUlong(old_back, JB_job_number) == 7 &&
                         strcmp(lGetString(old_back, JB_owner), "alice") == 0;
   lFreeElem(&old_back);
   lFreeElem(&ep);

   // --- TOM
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(7);
   job->set<JB_owner>("alice");
   // typed:   job->write_to_disk(dir, name, "job"), ElementPtr<JB>::read_from_disk(dir, name, "job")
   // differs: a failure throws with the message that is logged, and a file of another type throws WrongType
   job->write_to_disk(dir, "typed", "job");
   const ElementPtr<JB> back = ElementPtr<JB>::read_from_disk(dir, "typed", "job");
   const bool typed_read = back->get<JB_job_number>() == 7 && std::string(back->get<JB_owner>()) == "alice";
   ElementPtr<US> set = ElementPtr<US>::create();
   set->set<US_name>("staff");
   set->write_to_disk(dir, "set", "userset");
   const bool wrong_type = throws<FieldException>([&] {
      static_cast<void>(ElementPtr<JB>::read_from_disk(dir, "set", "job"));
   });
   const bool missing = throws<ocs::RuntimeException>([&] {
      static_cast<void>(ElementPtr<JB>::read_from_disk(dir, "missing", "job"));
   });

   for (const char *name : {"old", "typed", "set"}) {
      unlink((std::string(dir) + "/" + name).c_str());
   }
   rmdir(dir);
   CHECK(3, "write_to_disk() and read_from_disk() carry a job through a file as lWriteElemToDisk and "
            "lReadElemFromDisk do; a file of another type and a missing file throw",
         made && old_written && old_read && typed_read && wrong_type && missing);
}

/**
 * @brief Errors: a cut text, a list of another type
 */
void test_errors() {
   const std::string text = text_of([](FILE *fp) { typed_jobs()->dump(fp); });
   const std::string cut = text.substr(0, text.size() / 2);

   // --- CULL
   // old:     a cut text gives nullptr and leaves the reason in the error number
   bool old_cut = false;
   from_text(cut, [&](FILE *fp) {
      lList *lp = lUndumpList(fp, nullptr, nullptr);
      old_cut = lp == nullptr;
      lFreeList(&lp);
   });

   // --- TOM
   // typed:   the same text throws ListCoreError with that error number
   // differs: a list of another type throws WrongType, where the old form hands it out untyped
   bool typed_cut = false;
   from_text(cut, [&](FILE *fp) { typed_cut = throws<ListCoreError>([&] { static_cast<void>(ListPtr<JB>::undump(fp)); }); });
   bool wrong_type = false;
   from_text(text, [&](FILE *fp) { wrong_type = throws<FieldException>([&] { static_cast<void>(ListPtr<US>::undump(fp)); }); });

   CHECK(4, "a cut text throws ListCoreError where the old form gives nullptr; a list of another type throws "
            "WrongType",
         old_cut && typed_cut && wrong_type);
}

/**
 * @brief In a Release build, time dumping and reading back, and the element files, with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int rounds = 50;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   char dir[] = "/tmp/test_tom_ops_dump_XXXXXX";
   if (mkdtemp(dir) == nullptr) {
      return;
   }

   // --- CULL
   lList *lp = old_jobs();
   std::size_t old_read = 0;
   auto t0 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      const std::string text = text_of([&](FILE *fp) { lDumpList(fp, lp, 0); });
      from_text(text, [&](FILE *fp) {
         lList *back = lUndumpList(fp, nullptr, nullptr);
         old_read += lGetNumberOfElem(back);
         lFreeList(&back);
      });
   }
   auto t1 = std::chrono::steady_clock::now();
   const lListElem *first = lFirst(lp);
   for (int r = 0; r < rounds * 20; r++) {
      lWriteElemToDisk(first, dir, "old", "job");
      lListElem *back = lReadElemFromDisk(dir, "old", JB_Type, "job");
      lFreeElem(&back);
   }
   auto t2 = std::chrono::steady_clock::now();
   lFreeList(&lp);

   // --- TOM
   const ListPtr<JB> jobs = typed_jobs();
   std::size_t typed_read = 0;
   auto t3 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      const std::string text = text_of([&](FILE *fp) { jobs->dump(fp); });
      from_text(text, [&](FILE *fp) { typed_read += ListPtr<JB>::undump(fp)->size(); });
   }
   auto t4 = std::chrono::steady_clock::now();
   const ConstElement<JB> job = jobs->front();
   for (int r = 0; r < rounds * 20; r++) {
      job.write_to_disk(dir, "typed", "job");
      static_cast<void>(ElementPtr<JB>::read_from_disk(dir, "typed", "job"));
   }
   auto t5 = std::chrono::steady_clock::now();
   unlink((std::string(dir) + "/old").c_str());
   unlink((std::string(dir) + "/typed").c_str());
   rmdir(dir);

   printf("timing, %d dumps of %u jobs, %d element files:\n", rounds, Jobs, rounds * 20);
   printf("   dump and read: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t3, t4),
          ms(t3, t4) / ms(t0, t1));
   printf("   element file:  old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t1, t2), ms(t4, t5),
          ms(t4, t5) / ms(t1, t2));
   CHECK(5, "the timing loops read the same number of jobs", old_read == typed_read && old_read == Jobs * rounds);
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
      {"OPS-DUMP-01", "list", "dump() and undump() of a list write and read what lDumpList and lUndumpList do",
       {"dump 100 jobs, read them back"}, "the same text, the same jobs", {{"OPS-01", "covers"}}, {1}, test_list},
      {"OPS-DUMP-02", "element", "an element dumps and reads back as with lDumpElemFp and lUndumpElemFp",
       {"dump a job whose owner holds quotes, read it back"}, "the same text and values", {{"OPS-01", "covers"}}, {2},
       test_element},
      {"OPS-DUMP-03", "file", "write_to_disk() and read_from_disk() as lWriteElemToDisk and lReadElemFromDisk",
       {"write a job to a file and read it back; read a user set file as a job; read a missing file"},
       "the same job; WrongType and a runtime error thrown", {{"OPS-01", "covers"}}, {3}, test_file},
      {"OPS-DUMP-04", "errors", "the typed form throws where the old one gave nullptr",
       {"read a cut text, read a job list as user sets"}, "ListCoreError and WrongType thrown",
       {{"OPS-01", "covers"}}, {4}, test_errors},
      {"OPS-DUMP-05", "timing", "a Release build times both forms",
       {"dump and read back 100 jobs 50 times; write and read an element file 1 000 times"}, "ratios printed",
       {{"OPS-01", "covers"}}, {5}, test_timing},
   });
}
