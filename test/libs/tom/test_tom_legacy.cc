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
 * @brief Module test of the error behaviour of the old field accessors (CS-3043, LEG-01)
 *
 * Pins down, family by family, what the old accessors do on an error: abort,
 * return -1 or nullptr, and which error number they leave behind. The
 * expectations were taken from the code before the old interface moved onto
 * the core of the typed object model, and the inventory in CS-3043 lists them;
 * the move must not change any of them. An abort is observed in a child
 * process.
 */

#include <csignal>
#include <cstdio>
#include <functional>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"
#include "sgeobj/cull/sge_all_listsL.h"

#include "ocs_module_test.h"

/** @brief What an old accessor did on an error */
enum class Outcome {
   Aborted,    ///< the process aborted
   Returned    ///< the call returned
};

/**
 * @brief Run a call in a child process and tell whether it aborted
 * @param call the call
 * @return Aborted if the child ended with SIGABRT, Returned otherwise
 */
static Outcome
run_in_child(const std::function<void()> &call) {
   std::fflush(nullptr);
   const pid_t pid = fork();
   if (pid == 0) {
      // keep the expected messages and aborts out of the test output
      std::freopen("/dev/null", "w", stderr);
      std::freopen("/dev/null", "w", stdout);
      call();
      _exit(0);
   }
   int status = 0;
   waitpid(pid, &status, 0);
   return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT ? Outcome::Aborted : Outcome::Returned;
}

/**
 * @brief Whether a call aborts
 * @param call the call
 * @return true if it aborted
 */
static bool
aborts(const std::function<void()> &call) {
   return run_in_child(call) == Outcome::Aborted;
}

/**
 * @brief Whether a call returns -1 and leaves an error number behind
 * @param call the call, returning the setter's result
 * @param lerrno the expected error number, 0 for none
 * @return true if the call returned -1 and the error number is the expected one
 */
static bool
fails_with(const std::function<int()> &call, const int lerrno) {
   cull_state_set_lerrno(0);
   const int result = call();
   return result == -1 && cull_state_get_lerrno() == lerrno;
}

/**
 * @brief Whether a lookup returns nullptr without aborting and without an error number
 * @param call the lookup
 * @return true if it returned nullptr and left the error number at 0
 */
static bool
finds_nothing(const std::function<const void *()> &call) {
   if (run_in_child([&] { static_cast<void>(call()); }) == Outcome::Aborted) {
      return false;
   }
   cull_state_set_lerrno(0);
   return call() == nullptr && cull_state_get_lerrno() == 0;
}

/** @brief The elements the cases work on */
struct Fixture {
   lList *list = nullptr;        ///< a list holding the full element
   lListElem *full = nullptr;    ///< a full TEST element, in the list
   lList *reduced_list = nullptr; ///< a list holding the reduced element
   lListElem *reduced = nullptr; ///< a TEST element reduced to TEST_obj_any, so that every other field is absent
   lEnumeration *what = nullptr; ///< the selection of the reduced element

   /** @brief Build the elements */
   Fixture() {
      list = lCreateList("full", TEST_Type);
      full = lCreateElem(TEST_Type);
      lAppendElem(list, full);
      what = lWhat("%T(%I)", TEST_Type, TEST_obj_any);
      reduced_list = lSelect("reduced", list, nullptr, what);
      reduced = lFirstRW(reduced_list);
   }

   /** @brief Free the elements */
   ~Fixture() {
      lFreeList(&reduced_list);
      lFreeWhat(&what);
      lFreeList(&list);
   }
};

/**
 * @brief Family A: getters by name abort on every error
 * @return nothing; CHECK records the results
 */
static void
test_getters_by_name() {
   Fixture f;
   const lListElem *null_ep = nullptr;
   // one getter per field type; each on a null element, a foreign field, an absent field and a field of another type
   const std::function<void(const lListElem *, int)> getters[] = {
      [](const lListElem *ep, int nm) { static_cast<void>(lGetUlong(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetUlong64(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetInt(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetLong(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetDouble(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetBool(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetString(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetHost(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetRef(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetObject(ep, nm)); },
      [](const lListElem *ep, int nm) { static_cast<void>(lGetList(ep, nm)); },
   };
   const int own_field[] = {TEST_uint32, TEST_uint64, TEST_int, TEST_long, TEST_double, TEST_bool,
                            TEST_string, TEST_host, TEST_ref, TEST_obj_jb, JB_ja_tasks};
   const int other_type_field[] = {TEST_string, TEST_string, TEST_string, TEST_string, TEST_string, TEST_string,
                                   TEST_uint32, TEST_uint32, TEST_uint32, TEST_uint32, JB_job_number};
   int null_abort = 0, foreign_abort = 0, absent_abort = 0, type_abort = 0;
   lListElem *job = lCreateElem(JB_Type);
   for (size_t i = 0; i < std::size(getters); i++) {
      // lGetList is tested on a job, the TEST type has no list field
      const lListElem *ep = own_field[i] == JB_ja_tasks ? job : f.full;
      null_abort += aborts([&] { getters[i](null_ep, own_field[i]); });
      foreign_abort += aborts([&] { getters[i](ep, QU_qname); });
      if (own_field[i] != JB_ja_tasks) {
         absent_abort += aborts([&] { getters[i](f.reduced, own_field[i]); });
      }
      type_abort += aborts([&] { getters[i](ep, other_type_field[i]); });
   }
   const int n = static_cast<int>(std::size(getters));
   CHECK(1, "every getter by name aborts on a null element", null_abort == n);
   CHECK(2, "every getter by name aborts on a field of another type", foreign_abort == n);
   CHECK(3, "every getter by name aborts on a field a reduced element does not hold", absent_abort == n - 1);
   CHECK(4, "every getter by name aborts on a field of another field type", type_abort == n);
   lFreeElem(&job);
}

/**
 * @brief Family B: getters by position check only the field type, and some of them the position
 * @return nothing; CHECK records the results
 */
static void
test_getters_by_position() {
   Fixture f;
   const int pos_string = lGetPosViaElem(f.full, TEST_string, SGE_DO_ABORT);
   const int pos_uint32 = lGetPosViaElem(f.full, TEST_uint32, SGE_DO_ABORT);
   const bool type_aborts =
      aborts([&] { static_cast<void>(lGetPosUlong(f.full, pos_string)); }) &&
      aborts([&] { static_cast<void>(lGetPosInt(f.full, pos_string)); }) &&
      aborts([&] { static_cast<void>(lGetPosDouble(f.full, pos_string)); }) &&
      aborts([&] { static_cast<void>(lGetPosBool(f.full, pos_string)); }) &&
      aborts([&] { static_cast<void>(lGetPosString(f.full, pos_uint32)); }) &&
      aborts([&] { static_cast<void>(lGetPosHost(f.full, pos_uint32)); });
   CHECK(5, "the getters by position abort on a field of another field type", type_aborts);
   CHECK(6, "lGetPosUlong, lGetPosUlong64, lGetPosObject and lGetPosList abort on a negative position",
         aborts([&] { static_cast<void>(lGetPosUlong(f.full, -1)); }) &&
         aborts([&] { static_cast<void>(lGetPosUlong64(f.full, -1)); }) &&
         aborts([&] { static_cast<void>(lGetPosObject(f.full, -1)); }) &&
         aborts([&] { static_cast<void>(lGetPosList(f.full, -1)); }));
   CHECK(7, "lGetPosString and lGetPosHost return nullptr on a negative position",
         lGetPosString(f.full, -1) == nullptr && lGetPosHost(f.full, -1) == nullptr);
}

/**
 * @brief Family C: setters by name return -1 on a null element, and handle a missing field in three ways
 * @return nothing; CHECK records the results
 */
static void
test_setters_by_name() {
   Fixture f;
   CHECK(8, "every setter by name returns -1 with LEELEMNULL on a null element",
         fails_with([] { return lSetUlong(nullptr, TEST_uint32, 1); }, LEELEMNULL) &&
         fails_with([] { return lSetUlong64(nullptr, TEST_uint64, 1); }, LEELEMNULL) &&
         fails_with([] { return lSetInt(nullptr, TEST_int, 1); }, LEELEMNULL) &&
         fails_with([] { return lSetLong(nullptr, TEST_long, 1); }, LEELEMNULL) &&
         fails_with([] { return lSetDouble(nullptr, TEST_double, 1); }, LEELEMNULL) &&
         fails_with([] { return lSetBool(nullptr, TEST_bool, true); }, LEELEMNULL) &&
         fails_with([] { return lSetString(nullptr, TEST_string, "x"); }, LEELEMNULL) &&
         fails_with([] { return lSetHost(nullptr, TEST_host, "x"); }, LEELEMNULL) &&
         fails_with([] { return lSetRef(nullptr, TEST_ref, nullptr); }, LEELEMNULL) &&
         fails_with([] { return lSetObject(nullptr, TEST_obj_jb, nullptr); }, LEELEMNULL) &&
         fails_with([] { return lSetList(nullptr, JB_ja_tasks, nullptr); }, LEELEMNULL) &&
         fails_with([] { return lAddUlong(nullptr, TEST_uint32, 1); }, LEELEMNULL) &&
         fails_with([] { return lAddDouble(nullptr, TEST_double, 1); }, LEELEMNULL));

   // a field of another type and a field the reduced element does not hold behave alike
   for (lListElem *ep : {f.full, f.reduced}) {
      const bool foreign = ep == f.full;
      const int nm_u32 = foreign ? static_cast<int>(QU_seq_no) : static_cast<int>(TEST_uint32);
      const int id = foreign ? 9 : 12;
      if (!foreign) {
         // the reduced element holds TEST_obj_any only, so every other field is absent
         CHECK(id, "the silent setters return -1 without an error number on an absent field",
               fails_with([&] { return lSetUlong(ep, TEST_uint32, 1); }, 0) &&
               fails_with([&] { return lSetUlong64(ep, TEST_uint64, 1); }, 0) &&
               fails_with([&] { return lSetObject(ep, TEST_obj_jb, nullptr); }, 0) &&
               fails_with([&] { return lAddUlong64(ep, TEST_uint64, 1); }, 0));
         CHECK(id + 1, "the other setters return -1 with LENEGPOS on an absent field",
               fails_with([&] { return lSetInt(ep, TEST_int, 1); }, LENEGPOS) &&
               fails_with([&] { return lSetLong(ep, TEST_long, 1); }, LENEGPOS) &&
               fails_with([&] { return lSetDouble(ep, TEST_double, 1); }, LENEGPOS) &&
               fails_with([&] { return lSetBool(ep, TEST_bool, true); }, LENEGPOS) &&
               fails_with([&] { return lSetRef(ep, TEST_ref, nullptr); }, LENEGPOS) &&
               fails_with([&] { return lAddDouble(ep, TEST_double, 1); }, LENEGPOS));
         CHECK(id + 2, "lSetString and lSetHost abort on an absent field",
               aborts([&] { lSetString(ep, TEST_string, "x"); }) && aborts([&] { lSetHost(ep, TEST_host, "x"); }));
      } else {
         CHECK(id, "the silent setters return -1 without an error number on a field of another type",
               fails_with([&] { return lSetUlong(ep, nm_u32, 1); }, 0) &&
               fails_with([&] { return lSetUlong64(ep, JB_submission_time, 1); }, 0) &&
               fails_with([&] { return lSetObject(ep, JB_checkpoint_object, nullptr); }, 0) &&
               fails_with([&] { return lSetList(ep, JB_ja_tasks, nullptr); }, 0) &&
               fails_with([&] { return lAddUlong(ep, nm_u32, 1); }, 0));
         CHECK(id + 1, "the other setters return -1 with LENEGPOS on a field of another type",
               fails_with([&] { return lSetInt(ep, RQL_result, 1); }, LENEGPOS) &&
               fails_with([&] { return lSetLong(ep, JL_pri, 1); }, LENEGPOS) &&
               fails_with([&] { return lSetDouble(ep, JB_urg, 1); }, LENEGPOS) &&
               fails_with([&] { return lSetBool(ep, JB_notify, true); }, LENEGPOS) &&
               fails_with([&] { return lSetRef(ep, JB_category, nullptr); }, LENEGPOS));
         CHECK(id + 2, "lSetString and lSetHost abort on a field of another type",
               aborts([&] { lSetString(ep, JB_job_name, "x"); }) && aborts([&] { lSetHost(ep, JB_host, "x"); }));
      }
   }

   CHECK(15, "every setter by name aborts on a field of another field type",
         aborts([&] { lSetUlong(f.full, TEST_string, 1); }) && aborts([&] { lSetUlong64(f.full, TEST_string, 1); }) &&
         aborts([&] { lSetInt(f.full, TEST_string, 1); }) && aborts([&] { lSetDouble(f.full, TEST_string, 1); }) &&
         aborts([&] { lSetBool(f.full, TEST_string, true); }) && aborts([&] { lSetString(f.full, TEST_uint32, "x"); }) &&
         aborts([&] { lSetHost(f.full, TEST_uint32, "x"); }) && aborts([&] { lSetRef(f.full, TEST_uint32, nullptr); }) &&
         aborts([&] { lSetObject(f.full, TEST_uint32, nullptr); }));

   lListElem *bound = lCreateElem(JB_Type);
   lList *holder = lCreateList("holder", JB_Type);
   lAppendElem(holder, bound);
   CHECK(16, "lSetObject returns -1 with LEBOUNDELEM for an element bound to a list",
         fails_with([&] { return lSetObject(f.full, TEST_obj_jb, bound); }, LEBOUNDELEM));
   lFreeList(&holder);

   char *str = nullptr;
   CHECK(17, "lXchgString returns -1 with LEELEMNULL for a null element or pointer, LENEGPOS for a missing field",
         fails_with([&] { return lXchgString(nullptr, TEST_string, &str); }, LEELEMNULL) &&
         fails_with([&] { return lXchgString(f.full, TEST_string, nullptr); }, LEELEMNULL) &&
         fails_with([&] { return lXchgString(f.full, JB_job_name, &str); }, LENEGPOS));
}

/**
 * @brief Family D: setters by position return -1 on a null element and a negative position, and abort on a wrong type
 * @return nothing; CHECK records the results
 */
static void
test_setters_by_position() {
   Fixture f;
   const int pos_uint32 = lGetPosViaElem(f.full, TEST_uint32, SGE_DO_ABORT);
   const int pos_string = lGetPosViaElem(f.full, TEST_string, SGE_DO_ABORT);
   CHECK(18, "the setters by position return -1 with LEELEMNULL on a null element",
         fails_with([&] { return lSetPosUlong(nullptr, pos_uint32, 1); }, LEELEMNULL) &&
         fails_with([&] { return lSetPosString(nullptr, pos_string, "x"); }, LEELEMNULL));
   CHECK(19, "the setters by position return -1 with LENEGPOS on a negative position",
         fails_with([&] { return lSetPosUlong(f.full, -1, 1); }, LENEGPOS) &&
         fails_with([&] { return lSetPosUlong64(f.full, -1, 1); }, LENEGPOS) &&
         fails_with([&] { return lSetPosInt(f.full, -1, 1); }, LENEGPOS) &&
         fails_with([&] { return lSetPosDouble(f.full, -1, 1); }, LENEGPOS) &&
         fails_with([&] { return lSetPosBool(f.full, -1, true); }, LENEGPOS) &&
         fails_with([&] { return lSetPosString(f.full, -1, "x"); }, LENEGPOS) &&
         fails_with([&] { return lSetPosHost(f.full, -1, "x"); }, LENEGPOS) &&
         fails_with([&] { return lSetPosRef(f.full, -1, nullptr); }, LENEGPOS) &&
         fails_with([&] { return lSetPosObject(f.full, -1, nullptr); }, LENEGPOS) &&
         fails_with([&] { return lSetPosList(f.full, -1, nullptr); }, LENEGPOS));
   CHECK(20, "the setters by position abort on a field of another field type",
         aborts([&] { lSetPosUlong(f.full, pos_string, 1); }) && aborts([&] { lSetPosString(f.full, pos_uint32, "x"); }));
}

/**
 * @brief Families E to I: lookups and the functions built on them return nullptr where they cannot search
 * @return nothing; CHECK records the results
 */
static void
test_lookups() {
   Fixture f;
   lSetUlong(f.full, TEST_uint32, 3);
   CHECK(21, "a lookup in a null list finds nothing",
         finds_nothing([] { return lGetElemUlong(nullptr, TEST_uint32, 3); }) &&
         finds_nothing([] { return lGetElemStr(nullptr, TEST_string, "x"); }));
   CHECK(22, "a lookup by a field of another type finds nothing and does not abort",
         finds_nothing([&] { return lGetElemUlong(f.list, JB_job_number, 3); }) &&
         finds_nothing([&] { return lGetElemStr(f.list, JB_job_name, "x"); }) &&
         finds_nothing([&] { return lGetElemHost(f.list, JB_host, "x"); }) &&
         finds_nothing([&] { return lGetElemUlong64(f.list, JB_submission_time, 3); }));
   CHECK(23, "a string or host lookup by a field of another field type finds nothing and does not abort, "
             "a number lookup does not check the type and aborts reading the first element",
         finds_nothing([&] { return lGetElemStr(f.list, TEST_uint32, "x"); }) &&
         finds_nothing([&] { return lGetElemHost(f.list, TEST_uint32, "x"); }) &&
         aborts([&] { static_cast<void>(lGetElemUlong(f.list, TEST_string, 3)); }) &&
         aborts([&] { static_cast<void>(lGetElemUlong64(f.list, TEST_string, 3)); }));
   CHECK(24, "a lookup by a key that no element holds finds nothing",
         finds_nothing([&] { return lGetElemUlong(f.list, TEST_uint32, 4); }) &&
         lGetElemUlong(f.list, TEST_uint32, 3) == f.full);

   lList *added = nullptr;
   CHECK(25, "lAddElemUlong with a field of another type returns nullptr and does not abort",
         finds_nothing([&] { return lAddElemUlong(&added, JB_job_number, 1, TEST_Type); }) && added == nullptr);
   CHECK(26, "lAddSubUlong on a null element or a missing sub-list field returns nullptr",
         finds_nothing([] { return lAddSubUlong(nullptr, JAT_task_number, 1, JB_ja_tasks, JAT_Type); }) &&
         finds_nothing([&] { return lAddSubUlong(f.full, JAT_task_number, 1, JB_ja_tasks, JAT_Type); }));
   CHECK(27, "lGetSubUlong aborts on a missing sub-list field",
         aborts([&] { static_cast<void>(lGetSubUlong(f.full, JAT_task_number, 1, JB_ja_tasks)); }));
}

/**
 * @brief Family J: the position lookups of the descriptor
 * @return nothing; CHECK records the results
 */
static void
test_positions() {
   Fixture f;
   cull_state_set_lerrno(0);
   const bool via_elem = lGetPosViaElem(nullptr, TEST_uint32, SGE_NO_ABORT) == -1 &&
                         lGetPosViaElem(f.full, QU_qname, SGE_NO_ABORT) == -1 &&
                         lGetPosViaElem(f.reduced, TEST_string, SGE_NO_ABORT) == -1 && cull_state_get_lerrno() == 0;
   CHECK(28, "lGetPosViaElem without abort returns -1 for a null element, a foreign and an absent field", via_elem);
   CHECK(29, "lGetPosViaElem with abort aborts on a null element and a foreign field",
         aborts([] { static_cast<void>(lGetPosViaElem(nullptr, TEST_uint32, SGE_DO_ABORT)); }) &&
         aborts([&] { static_cast<void>(lGetPosViaElem(f.full, QU_qname, SGE_DO_ABORT)); }));
   cull_state_set_lerrno(0);
   const bool null_descr = lGetPosInDescr(nullptr, TEST_uint32) == -1 && cull_state_get_lerrno() == LEDESCRNULL;
   cull_state_set_lerrno(0);
   const bool miss = lGetPosInDescr(TEST_Type, QU_qname) == -1 && cull_state_get_lerrno() == 0 &&
                     lGetPosInDescr(f.reduced->descr, TEST_string) == -1 && lGetPosInDescr(TEST_Type, TEST_uint32) >= 0;
   CHECK(30, "lGetPosInDescr returns -1 with LEDESCRNULL for a null descriptor, without an error number for a miss",
         null_descr && miss);
}

int main(int argc, char *argv[]) {
   lInit(nmv);
   return ocs::module_test::run(argc, argv, {
      {"LEGACY-01", "getters by name",
       "the getters by name abort on every error",
       {"call one getter per field type with a null element, a foreign field, an absent field and a field of "
        "another field type"},
       "each call aborts", {{"LEG-01", "covers"}}, {1, 2, 3, 4}, test_getters_by_name},
      {"LEGACY-02", "getters by position",
       "the getters by position check the field type, and some of them the position",
       {"call them with a field of another field type and with a negative position"},
       "wrong types abort; a negative position aborts for numbers, lists and objects and gives nullptr for strings "
       "and hosts", {{"LEG-01", "covers"}}, {5, 6, 7}, test_getters_by_position},
      {"LEGACY-03", "setters by name",
       "the setters by name return -1 on a null element, handle a missing field in three ways, and abort on a "
       "wrong field type",
       {"call every setter with a null element, a foreign field, an absent field and a field of another field type",
        "set a bound element as object", "exchange a string with null arguments and a missing field"},
       "as the inventory of CS-3043 lists it", {{"LEG-01", "covers"}},
       {8, 9, 10, 11, 12, 13, 14, 15, 16, 17}, test_setters_by_name},
      {"LEGACY-04", "setters by position",
       "the setters by position return -1 on a null element and a negative position, and abort on a wrong type",
       {"call them with a null element, a negative position and a field of another field type"},
       "-1 with LEELEMNULL and LENEGPOS; an abort for the wrong type", {{"LEG-01", "covers"}}, {18, 19, 20},
       test_setters_by_position},
      {"LEGACY-05", "lookups",
       "lookups and the functions built on them return nullptr where they cannot search",
       {"look up in a null list, by a foreign field, by a field of another field type and by a missing key",
        "add an element by a foreign field, add to a missing sub-list, read a missing sub-list"},
       "nullptr without an abort and without an error number, except that a number lookup by a field of another "
       "field type aborts reading the first element; reading a missing sub-list aborts",
       {{"LEG-01", "covers"}}, {21, 22, 23, 24, 25, 26, 27}, test_lookups},
      {"LEGACY-06", "positions",
       "the position lookups of the descriptor report misses as before",
       {"ask lGetPosViaElem with and without abort, and lGetPosInDescr with a null descriptor and a miss"},
       "-1 without an error number, an abort where asked for, LEDESCRNULL for a null descriptor",
       {{"LEG-01", "covers"}}, {28, 29, 30}, test_positions},
   });
}
