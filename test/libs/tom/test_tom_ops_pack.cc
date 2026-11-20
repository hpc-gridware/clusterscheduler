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
 * @brief Packing and unpacking through the typed handles and through the old interface (CS-3077, OPS-01)
 *
 * This test is also an example of how objects are packed for the wire and
 * unpacked again with the typed object model. Every scenario does the same
 * thing twice: a block marked CULL uses only the old functions of the list
 * library, a block marked TOM only the typed handles. Each block builds its
 * own data and reduces what came out to plain values (the packed bytes, the
 * job numbers and owners unpacked), and the scenario compares those. Each step
 * carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old interface returns a PACK_* code from every call and leaves the
 * caller to free the buffer with clear_packbuffer() on every path. The typed
 * form throws a PackException with that code, PackBuffer frees the buffer when
 * it goes out of scope, and unpacking checks that the buffer holds the type
 * the handle expects. Both write the same bytes.
 *
 * In a Release build the last scenario times packing and unpacking with both
 * forms and prints the ratios.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "cull/cull_pack.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::JB;
using ocs::tom::ListPtr;
using ocs::tom::PackBuffer;
using ocs::tom::PackException;
using ocs::tom::US;
using ocs::tom::What;
using ocs::tom::Where;
using ocs::tom::field;
using ocs::tom::what;

namespace {

   /** @brief The number of jobs the scenarios pack */
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
    * @brief The bytes of a pack buffer
    * @param pb the buffer
    * @return the bytes, header included
    */
   std::vector<char> bytes_of(const sge_pack_buffer &pb) {
      return {pb.head_ptr, pb.head_ptr + pb.bytes_used};
   }

   /**
    * @brief Whether a function throws a PackException with a given code
    * @param call the call
    * @param code the expected code
    * @return true if it threw with that code
    */
   template <typename Call>
   bool throws_pack(Call call, const int code) {
      try {
         call();
      } catch (const PackException &e) {
         return e.code() == code;
      }
      return false;
   }

   /**
    * @brief Whether a function throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   template <typename Call>
   bool throws_field(Call call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
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

} // namespace

/**
 * @brief Pack a job list, whole and reduced to number and owner, and unpack it
 */
void test_list() {
   // --- CULL
   lList *lp = old_jobs();
   // old:     init_packbuffer(), cull_pack_list(), cull_unpack_list(), clear_packbuffer() on every path
   sge_pack_buffer pb;
   init_packbuffer(&pb, 0, false, false);
   cull_pack_list(&pb, lp);
   const std::vector<char> old_bytes = bytes_of(pb);
   lEnumeration *old_what = lWhat("%T(%I%I)", JB_Type, JB_job_number, JB_owner);
   sge_pack_buffer reduced_pb;
   init_packbuffer(&reduced_pb, 0, false, false);
   cull_pack_list_partial(&reduced_pb, lp, old_what, 0);
   const std::vector<char> old_reduced = bytes_of(reduced_pb);
   lFreeWhat(&old_what);
   sge_pack_buffer in;
   char *copy = static_cast<char *>(malloc(reduced_pb.bytes_used));
   memcpy(copy, reduced_pb.head_ptr, reduced_pb.bytes_used);
   init_packbuffer_from_buffer(&in, copy, reduced_pb.bytes_used, false);
   lList *unpacked = nullptr;
   const int ret = cull_unpack_list(&in, &unpacked);
   const std::vector<Plain> old_values = plain(unpacked);
   lFreeList(&unpacked);
   clear_packbuffer(&in);
   clear_packbuffer(&reduced_pb);
   clear_packbuffer(&pb);
   lFreeList(&lp);

   // --- TOM
   const ListPtr<JB> jobs = typed_jobs();
   // typed:   PackBuffer out; jobs->pack(out.raw()); ListPtr<JB>::unpack(in.raw())
   // differs: the buffer frees itself; unpack() throws instead of returning a code and checks the type
   PackBuffer out;
   jobs->pack(out.raw());
   const std::vector<char> typed_bytes = bytes_of(out.raw());
   PackBuffer reduced;
   jobs->pack(reduced.raw(), what<JB_job_number, JB_owner>());
   const std::vector<char> typed_reduced = bytes_of(reduced.raw());
   PackBuffer typed_in = PackBuffer::reading(reduced.data(), reduced.size());
   const ListPtr<JB> typed_unpacked = ListPtr<JB>::unpack(typed_in.raw());

   CHECK(1, "pack() writes the bytes cull_pack_list writes, whole and reduced, and unpack() reads what "
            "cull_unpack_list reads",
         ret == PACK_SUCCESS && old_bytes == typed_bytes && old_reduced == typed_reduced &&
         old_values == plain(typed_unpacked) && old_values.size() == Jobs);
}

/**
 * @brief Pack one element and unpack it
 */
void test_element() {
   // --- CULL
   lListElem *ep = lCreateElem(JB_Type);
   lSetUlong(ep, JB_job_number, 42);
   lSetString(ep, JB_owner, "alice");
   sge_pack_buffer pb;
   init_packbuffer(&pb, 0, false, false);
   // old:     cull_pack_elem(), cull_unpack_elem() with a null descriptor for a free element
   cull_pack_elem(&pb, ep);
   const std::vector<char> old_bytes = bytes_of(pb);
   lFreeElem(&ep);
   clear_packbuffer(&pb);

   // --- TOM
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(42);
   job->set<JB_owner>("alice");
   PackBuffer out;
   // typed:   job->pack(out.raw()), ElementPtr<JB>::unpack(in.raw())
   job->pack(out.raw());
   PackBuffer in = PackBuffer::reading(out.data(), out.size());
   ElementPtr<JB> back = ElementPtr<JB>::unpack(in.raw());

   CHECK(2, "an element packs to the bytes cull_pack_elem writes and unpacks with its values",
         old_bytes == bytes_of(out.raw()) && back->get<JB_job_number>() == 42 &&
         std::string(back->get<JB_owner>()) == "alice");
}

/**
 * @brief Pack a selection and a condition, unpack them and select with them
 */
void test_what_and_where() {
   // --- CULL
   lList *sets = lCreateList("sets", US_Type);
   for (const char *name : {"alpha", "beta", "gamma"}) {
      lAddElemStr(&sets, US_name, name, US_Type);
   }
   lEnumeration *old_what = lWhat("%T(%I)", US_Type, US_name);
   lCondition *old_where = lWhere("%T(%I p= %s)", US_Type, US_name, "*a");
   sge_pack_buffer pb;
   init_packbuffer(&pb, 0, false, false);
   // old:     cull_pack_enum(), cull_pack_cond(), cull_unpack_enum(), cull_unpack_cond()
   cull_pack_enum(&pb, old_what);
   cull_pack_cond(&pb, old_where);
   const std::vector<char> old_bytes = bytes_of(pb);
   lFreeWhere(&old_where);
   lFreeWhat(&old_what);
   sge_pack_buffer in;
   char *copy = static_cast<char *>(malloc(pb.bytes_used));
   memcpy(copy, pb.head_ptr, pb.bytes_used);
   init_packbuffer_from_buffer(&in, copy, pb.bytes_used, false);
   lEnumeration *back_what = nullptr;
   lCondition *back_where = nullptr;
   cull_unpack_enum(&in, &back_what);
   cull_unpack_cond(&in, &back_where);
   lList *selected = lSelect("selected", sets, back_where, back_what);
   const std::uint32_t old_count = lGetNumberOfElem(selected);
   lFreeList(&selected);
   lFreeWhere(&back_where);
   lFreeWhat(&back_what);
   clear_packbuffer(&in);
   clear_packbuffer(&pb);
   lFreeList(&sets);

   // --- TOM
   ListPtr<US> typed_sets = ListPtr<US>::create("sets");
   for (const char *name : {"alpha", "beta", "gamma"}) {
      typed_sets->emplace_back<US_name>(name);
   }
   // typed:   what.pack(out.raw()), What<US>::unpack(in.raw()), and the same for Where
   // differs: unpack() refuses a selection or condition on fields of another type
   const What<US> typed_what = what<US_name>();
   const Where<US> typed_where = field<US_name>().like("*a");
   PackBuffer out;
   typed_what.pack(out.raw());
   typed_where.pack(out.raw());
   PackBuffer typed_in = PackBuffer::reading(out.data(), out.size());
   const What<US> what_back = What<US>::unpack(typed_in.raw());
   const Where<US> where_back = Where<US>::unpack(typed_in.raw());
   const std::size_t typed_count = typed_sets->select(where_back, what_back)->size();

   CHECK(3, "a selection and a condition pack to the bytes of cull_pack_enum and cull_pack_cond and select the "
            "same after unpacking",
         old_bytes == bytes_of(out.raw()) && old_count == 3 && typed_count == old_count);
}

/**
 * @brief Errors: a truncated buffer, a list of another type, a buffer of another version
 */
void test_errors() {
   PackBuffer out;
   typed_jobs()->pack(out.raw());

   // --- CULL
   // old:     a truncated buffer gives PACK_FORMAT and a null list
   sge_pack_buffer in;
   const std::size_t cut = out.size() - 5;
   char *copy = static_cast<char *>(malloc(cut));
   memcpy(copy, out.data(), cut);
   init_packbuffer_from_buffer(&in, copy, static_cast<std::uint32_t>(cut), false);
   lList *lp = reinterpret_cast<lList *>(1);
   const bool old_format = cull_unpack_list(&in, &lp) == PACK_FORMAT && lp == nullptr;
   clear_packbuffer(&in);

   // --- TOM
   // typed:   the same buffer throws PackException with PACK_FORMAT
   // differs: a list of another type throws WrongType, where the old form hands it out untyped
   const bool typed_format = throws_pack([&] {
      PackBuffer cut_in = PackBuffer::reading(out.data(), cut);
      static_cast<void>(ListPtr<JB>::unpack(cut_in.raw()));
   }, PACK_FORMAT);
   const bool wrong_type = throws_field([&] {
      PackBuffer jobs_in = PackBuffer::reading(out.data(), out.size());
      static_cast<void>(ListPtr<US>::unpack(jobs_in.raw()));
   }, FieldException::Reason::WrongType);
   std::vector<char> wrong_version(out.data(), out.data() + out.size());
   wrong_version[7] ^= 0x01;
   const bool version = throws_pack([&] {
      static_cast<void>(PackBuffer::reading(wrong_version.data(), wrong_version.size()));
   }, PACK_VERSION);

   CHECK(4, "a truncated buffer throws PACK_FORMAT, a list of another type WrongType, another version PACK_VERSION",
         old_format && typed_format && wrong_type && version);
}

/**
 * @brief The counting buffer measures what packing writes
 */
void test_counting() {
   const ListPtr<JB> jobs = typed_jobs();
   PackBuffer counting = PackBuffer::counting();
   jobs->pack(counting.raw());
   PackBuffer out;
   const std::size_t header = out.size();
   jobs->pack(out.raw());

   CHECK(5, "PackBuffer::counting() measures the bytes a pack writes, without writing",
         counting.size() == out.size() - header && counting.data() == nullptr);
}

/**
 * @brief A receiver that does not know what comes: look at the type, then unpack
 *
 * The old interface unpacks whatever comes and leaves the caller to look at the
 * descriptor of the result; the typed form looks first and unpacks into the
 * handle of the type it found.
 */
void test_peek() {
   // a buffer holding a job list, a reduced user set list (its descriptor goes through the table), a null list,
   // and a free job element
   PackBuffer out;
   typed_jobs()->pack(out.raw());
   ListPtr<US> sets = ListPtr<US>::create("sets");
   sets->emplace_back<US_name>("alpha");
   sets->pack(out.raw(), what<US_name>());
   ListPtr<JB>().get().pack(out.raw());
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(7);
   job->pack(out.raw());

   // --- CULL
   // old:     cull_unpack_list() takes any type; the caller reads the descriptor of the result
   sge_pack_buffer in;
   char *copy = static_cast<char *>(malloc(out.size()));
   memcpy(copy, out.data(), out.size());
   init_packbuffer_from_buffer(&in, copy, static_cast<std::uint32_t>(out.size()), false);
   std::vector<int> old_types;
   for (int i = 0; i < 3; i++) {
      lList *lp = nullptr;
      cull_unpack_list(&in, &lp);
      old_types.push_back(lp != nullptr ? lp->descr[0].nm : NoName);
      lFreeList(&lp);
   }
   lListElem *ep = nullptr;
   cull_unpack_elem(&in, &ep, nullptr);
   old_types.push_back(ep != nullptr ? ep->descr[0].nm : NoName);
   lFreeElem(&ep);
   clear_packbuffer(&in);

   // --- TOM
   // typed:   next_list_is<T>(pb), next_elem_is<T>(pb), then the unpack() of the type found
   // differs: the type is known before the bytes are read, and the buffer does not move while looking
   PackBuffer typed_in = PackBuffer::reading(out.data(), out.size());
   std::vector<int> typed_types;
   std::size_t jobs_found = 0;
   std::size_t sets_found = 0;
   bool stays = true;
   for (int i = 0; i < 3; i++) {
      const std::size_t before = typed_in.size();
      if (ocs::tom::next_list_is<JB>(typed_in.raw())) {
         stays = stays && typed_in.size() == before;
         jobs_found += ListPtr<JB>::unpack(typed_in.raw())->size();
         typed_types.push_back(JB::fields[0].nm);
      } else if (ocs::tom::next_list_is<US>(typed_in.raw())) {
         stays = stays && typed_in.size() == before;
         sets_found += ListPtr<US>::unpack(typed_in.raw())->size();
         typed_types.push_back(US::fields[0].nm);
      } else {
         stays = stays && ocs::tom::peek_list_type(&typed_in.raw()) == NoName && typed_in.size() == before;
         static_cast<void>(ListPtr<JB>::unpack(typed_in.raw()));
         typed_types.push_back(NoName);
      }
   }
   const bool elem_is_job = ocs::tom::next_elem_is<JB>(typed_in.raw());
   typed_types.push_back(elem_is_job ? JB::fields[0].nm : NoName);
   const ElementPtr<JB> job_back = ElementPtr<JB>::unpack(typed_in.raw());

   // cut off inside the descriptor: looking throws as unpacking does
   const bool cut = throws_pack([&] {
      PackBuffer cut_in = PackBuffer::reading(out.data(), 2 * INTSIZE + 1 + 2 * INTSIZE + 6);
      static_cast<void>(ocs::tom::peek_list_type(&cut_in.raw()));
   }, PACK_FORMAT);

   CHECK(7, "next_list_is and next_elem_is name the type of what follows without moving the buffer, as the "
            "descriptor of what cull_unpack_list gives",
         old_types == typed_types && jobs_found == Jobs && sets_found == 1 && stays && elem_is_job &&
         job_back->get<JB_job_number>() == 7 && cut);
}

/**
 * @brief In a Release build, time packing and unpacking with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int rounds = 200;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };

   // --- CULL
   lList *lp = old_jobs();
   auto t0 = std::chrono::steady_clock::now();
   std::size_t old_bytes = 0;
   std::size_t old_unpacked = 0;
   for (int r = 0; r < rounds; r++) {
      sge_pack_buffer pb;
      init_packbuffer(&pb, 0, false, false);
      cull_pack_list(&pb, lp);
      old_bytes += pb.bytes_used;
      clear_packbuffer(&pb);
   }
   auto t1 = std::chrono::steady_clock::now();
   sge_pack_buffer packed;
   init_packbuffer(&packed, 0, false, false);
   cull_pack_list(&packed, lp);
   auto t2 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      sge_pack_buffer in;
      char *copy = static_cast<char *>(malloc(packed.bytes_used));
      memcpy(copy, packed.head_ptr, packed.bytes_used);
      init_packbuffer_from_buffer(&in, copy, packed.bytes_used, false);
      lList *back = nullptr;
      cull_unpack_list(&in, &back);
      old_unpacked += lGetNumberOfElem(back);
      lFreeList(&back);
      clear_packbuffer(&in);
   }
   auto t3 = std::chrono::steady_clock::now();
   clear_packbuffer(&packed);
   lFreeList(&lp);

   // --- TOM
   const ListPtr<JB> jobs = typed_jobs();
   auto t4 = std::chrono::steady_clock::now();
   std::size_t typed_bytes = 0;
   std::size_t typed_unpacked = 0;
   for (int r = 0; r < rounds; r++) {
      PackBuffer out;
      jobs->pack(out.raw());
      typed_bytes += out.size();
   }
   auto t5 = std::chrono::steady_clock::now();
   PackBuffer typed_packed;
   jobs->pack(typed_packed.raw());
   auto t6 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      PackBuffer in = PackBuffer::reading(typed_packed.data(), typed_packed.size());
      typed_unpacked += ListPtr<JB>::unpack(in.raw())->size();
   }
   auto t7 = std::chrono::steady_clock::now();

   printf("timing, %d rounds of %u jobs:\n", rounds, Jobs);
   printf("   pack:   old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t4, t5),
          ms(t4, t5) / ms(t0, t1));
   printf("   unpack: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t2, t3), ms(t6, t7),
          ms(t6, t7) / ms(t2, t3));
   CHECK(6, "the timing loops packed and unpacked the same", old_bytes == typed_bytes && old_unpacked == typed_unpacked);
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
      {"OPS-PACK-01", "list", "pack() and unpack() of a list write and read what cull_pack_list does",
       {"pack 100 jobs whole and reduced to number and owner, unpack the reduced ones"},
       "the same bytes, the same jobs", {{"OPS-01", "covers"}}, {1}, test_list},
      {"OPS-PACK-02", "element", "an element packs and unpacks as with cull_pack_elem",
       {"pack one job, unpack it"}, "the same bytes and values", {{"OPS-01", "covers"}}, {2}, test_element},
      {"OPS-PACK-03", "selection and condition", "What and Where pack and unpack as cull_pack_enum and cull_pack_cond",
       {"pack a selection and a condition, unpack them, select with them"}, "the same bytes and selection",
       {{"OPS-01", "covers"}}, {3}, test_what_and_where},
      {"OPS-PACK-04", "errors", "the typed form throws where the old one returned a code",
       {"unpack a truncated buffer, a list of another type, a buffer of another version"},
       "PACK_FORMAT, WrongType, PACK_VERSION", {{"OPS-01", "covers"}}, {4}, test_errors},
      {"OPS-PACK-05", "counting", "the counting buffer measures what packing writes",
       {"count a pack of 100 jobs, pack them"}, "the same number of bytes", {{"OPS-01", "covers"}}, {5},
       test_counting},
      {"OPS-PACK-07", "peek", "a receiver looks at the type of what follows, then unpacks it",
       {"pack a job list, a reduced user set list, a null list and a job; look at each type, then unpack it"},
       "the types the old form finds after unpacking; the buffer unmoved by looking", {{"OPS-01", "covers"}}, {7},
       test_peek},
      {"OPS-PACK-06", "timing", "a Release build times both forms",
       {"pack 100 jobs 200 times, unpack them 200 times"}, "ratios printed", {{"OPS-01", "covers"}}, {6},
       test_timing},
   });
}
