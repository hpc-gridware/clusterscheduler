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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_lerrno.h"
#include "cull/cull_db.h"
#include "cull/cull_dump_scan.h"
#include "cull/cull_tree.h"
#include "cull/cull_what_print.h"
#include "cull/cull_file.h"
#include "cull/cull_pack.h"
#include "cull/cull_sort.h"
#include "cull/cull_state.h"
#include "cull/cull_whatP.h"
#include "cull/ocs_cull_descr.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_pack.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

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
      [[maybe_unused]] FILE *quiet_stderr = std::freopen("/dev/null", "w", stderr);
      [[maybe_unused]] FILE *quiet_stdout = std::freopen("/dev/null", "w", stdout);
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
   // since CS-3077 a missing sub-list field aborts, as for every lookup in a sub-list; before it gave nullptr
   CHECK(26, "lAddSubUlong returns nullptr on a null element and aborts on a missing sub-list field",
         finds_nothing([] { return lAddSubUlong(nullptr, JAT_task_number, 1, JB_ja_tasks, JAT_Type); }) &&
         aborts([&] { static_cast<void>(lAddSubUlong(f.full, JAT_task_number, 1, JB_ja_tasks, JAT_Type)); }));
   CHECK(27, "lGetSubUlong aborts on a missing sub-list field",
         aborts([&] { static_cast<void>(lGetSubUlong(f.full, JAT_task_number, 1, JB_ja_tasks)); }));
}

/**
 * @brief Whether a call returns nullptr and sets an error number
 * @param call the call
 * @param lerrno the expected error number
 * @return true if it returned nullptr with that error number
 */
static bool
null_with(const std::function<const void *()> &call, const int lerrno) {
   cull_state_set_lerrno(0);
   const void *result = call();
   return result == nullptr && cull_state_get_lerrno() == lerrno;
}

/**
 * @brief The list structure and ownership functions report their errors as before
 *
 * Pins every soft error and every abort of the functions whose implementation
 * moves into the core (CS-3077), so that the move is proven not to change them.
 */
static void
test_list_structure() {
   // creating
   lList *unnamed = lCreateList(nullptr, TEST_Type);
   const bool create = null_with([] { return lCreateElem(nullptr); }, LECOUNTDESCR) &&
                       null_with([] { return lCreateListHash("x", nullptr, true); }, LEDESCRNULL) &&
                       unnamed != nullptr && std::string(lGetListName(unnamed)) == "No list name specified";
   lFreeList(&unnamed);
   CHECK(31, "lCreateElem and lCreateListHash return nullptr with LECOUNTDESCR and LEDESCRNULL for no descriptor",
         create);

   // freeing nothing is a no-op
   lListElem *no_elem = nullptr;
   lList *no_list = nullptr;
   const bool free_nothing = !aborts([&] {
      lFreeElem(nullptr);
      lFreeElem(&no_elem);
      lFreeList(nullptr);
      lFreeList(&no_list);
   });
   CHECK(32, "lFreeElem and lFreeList accept nullptr", free_nothing);

   // copying
   lList *src = lCreateList("source", TEST_Type);
   lAppendElem(src, lCreateElem(TEST_Type));
   lList *copy = lCopyList(nullptr, src);
   const bool copy_errors = null_with([] { return lCopyElem(nullptr); }, LEELEMNULL) &&
                            null_with([] { return lCopyList("x", nullptr); }, LELISTNULL) &&
                            copy != nullptr && std::string(lGetListName(copy)) == "source";
   lFreeList(&copy);
   CHECK(33, "lCopyElem and lCopyList return nullptr with LEELEMNULL and LELISTNULL, a copy keeps the name",
         copy_errors);

   // appending and inserting
   lListElem *ep = lCreateElem(TEST_Type);
   const bool append_errors = fails_with([&] { return lAppendElem(nullptr, ep); }, LELISTNULL) &&
                              fails_with([&] { return lAppendElem(src, nullptr); }, LEELEMNULL) &&
                              fails_with([&] { return lInsertElem(nullptr, nullptr, ep); }, LELISTNULL) &&
                              fails_with([&] { return lInsertElem(src, nullptr, nullptr); }, LEELEMNULL) &&
                              aborts([&] { lAppendElem(src, lFirstRW(src)); }) &&
                              aborts([&] { lInsertElem(src, nullptr, lFirstRW(src)); });
   lFreeElem(&ep);
   CHECK(34, "lAppendElem and lInsertElem return -1 with LELISTNULL and LEELEMNULL, and abort on a bound element",
         append_errors);

   // removing and dechaining
   lList *other = lCreateList("other", TEST_Type);
   lAppendElem(other, lCreateElem(TEST_Type));
   lListElem *null_ep = nullptr;
   lListElem *some = lFirstRW(src);
   cull_state_set_lerrno(0);
   const bool remove_soft = lRemoveElem(nullptr, &some) == -1 && lRemoveElem(src, nullptr) == -1 &&
                            lRemoveElem(src, &null_ep) == -1 && cull_state_get_lerrno() == 0 && some != nullptr;
   const bool remove_errors = remove_soft &&
                              null_with([&] { return lDechainElem(nullptr, lFirstRW(src)); }, LELISTNULL) &&
                              null_with([&] { return lDechainElem(src, nullptr); }, LEELEMNULL) &&
                              aborts([&] {
                                 lListElem *foreign = lFirstRW(other);
                                 lRemoveElem(src, &foreign);
                              }) &&
                              aborts([&] { lDechainElem(src, lFirstRW(other)); }) && lGetNumberOfElem(src) == 1;
   CHECK(35, "lRemoveElem returns -1 without an error number, lDechainElem nullptr with LELISTNULL and "
             "LEELEMNULL, and both abort on an element of another list", remove_errors);

   // appending lists
   lList *jobs = lCreateList("jobs", JB_Type);
   lList *moved = lCreateList("moved", TEST_Type);
   const bool list_errors = fails_with([&] { return lAppendList(nullptr, src); }, LELISTNULL) &&
                            fails_with([&] { return lAppendList(src, nullptr); }, LELISTNULL) &&
                            fails_with([&] { return lAppendList(jobs, src); }, LEDIFFDESCR) &&
                            lGetNumberOfElem(src) == 1 && lGetNumberOfElem(jobs) == 0 &&
                            fails_with([&] { return lAddList(nullptr, &moved); }, LELISTNULL) && moved == nullptr &&
                            lSwapList(nullptr, TEST_obj_any, nullptr, TEST_obj_any) == -1;
   lFreeList(&jobs);
   lFreeList(&other);
   lFreeList(&src);
   CHECK(36, "lAppendList returns -1 with LELISTNULL and LEDIFFDESCR, lAddList frees the second list even then",
         list_errors);
}

/**
 * @brief The case-insensitive, prefix and sub-list lookups report their errors as before
 *
 * Pins the soft errors and the aborts of the lookups whose implementation
 * moves into the core (CS-3077).
 */
static void
test_more_lookups() {
   Fixture f;
   lSetString(f.full, TEST_string, "Name");
   // a null string, a null list, a foreign field, a field of another type and an absent field give nullptr
   const auto soft = [&](const auto &lookup) {
      return lookup(f.list, TEST_string, nullptr) == nullptr && lookup(nullptr, TEST_string, "x") == nullptr &&
             !aborts([&] { static_cast<void>(lookup(f.list, QU_qname, "x")); }) &&
             lookup(f.list, QU_qname, "x") == nullptr &&
             !aborts([&] { static_cast<void>(lookup(f.list, TEST_uint32, "x")); }) &&
             lookup(f.list, TEST_uint32, "x") == nullptr && lookup(f.reduced_list, TEST_string, "x") == nullptr;
   };
   const bool case_like =
      soft([](const lList *lp, int nm, const char *str) { return lGetElemCaseStr(lp, nm, str); }) &&
      soft([](const lList *lp, int nm, const char *str) { return lGetElemStrLike(lp, nm, str); }) &&
      lGetElemCaseStr(f.list, TEST_string, "NAME") == f.full && lGetElemStrLike(f.list, TEST_string, "Na*") == f.full &&
      lGetElemStrLike(f.list, TEST_string, "Name") == f.full && lGetElemStrLike(f.list, TEST_string, "name") == nullptr;
   CHECK(37, "lGetElemCaseStr and lGetElemStrLike return nullptr on every error without aborting", case_like);

   // a sub-list lookup on a null element gives nullptr, on an element without the sub-list field it aborts
   lList *jobs = lCreateList("jobs", JB_Type);
   lAppendElem(jobs, lCreateElem(JB_Type));
   lEnumeration *what = lWhat("%T(%I)", JB_Type, JB_job_number);
   lList *reduced = lSelect("reduced", jobs, nullptr, what);
   // every sub-list lookup gives nullptr for a null element; before CS-3077 only lGetSubStr did, the others aborted
   const bool sub = lGetSubStr(nullptr, ST_name, "x", JB_ja_tasks) == nullptr &&
                    lGetSubUlong(nullptr, JAT_task_number, 1, JB_ja_tasks) == nullptr &&
                    lGetSubUlong64(nullptr, JAT_task_number, 1, JB_ja_tasks) == nullptr &&
                    lGetSubHost(nullptr, EH_name, "h", JB_ja_tasks) == nullptr &&
                    lGetSubUlong(lFirst(jobs), JAT_task_number, 1, JB_ja_tasks) == nullptr &&
                    aborts([&] { static_cast<void>(lGetSubUlong(lFirst(reduced), JAT_task_number, 1, JB_ja_tasks)); });
   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&jobs);
   CHECK(38, "every sub-list lookup gives nullptr for a null element and an empty sub-list, and aborts without "
             "the sub-list field",
         sub);
}

/**
 * @brief Adding and deleting by key report their errors as before
 *
 * Pins the results, the soft errors and the aborts of the functions whose
 * implementation moves into the core (CS-3077), including where the variants
 * for the different key types differ from each other.
 */
static void
test_add_and_delete_by_key() {
   // adding: a null list is created, null arguments and wrong fields give nullptr
   lList *lp = nullptr;
   lListElem *added = lAddElemStr(&lp, TEST_string, "a", TEST_Type);
   const bool add_str = added != nullptr && lp != nullptr && std::string(lGetListName(lp)) == "" &&
                        lGetNumberOfElem(lp) == 1 && std::string(lGetString(added, TEST_string)) == "a" &&
                        lAddElemStr(nullptr, TEST_string, "a", TEST_Type) == nullptr &&
                        lAddElemStr(&lp, TEST_string, nullptr, TEST_Type) == nullptr &&
                        lAddElemStr(&lp, TEST_string, "a", nullptr) == nullptr &&
                        !aborts([&] { static_cast<void>(lAddElemStr(&lp, QU_qname, "a", TEST_Type)); }) &&
                        lAddElemStr(&lp, QU_qname, "a", TEST_Type) == nullptr &&
                        lAddElemStr(&lp, TEST_uint32, "a", TEST_Type) == nullptr &&
                        lAddElemHost(&lp, TEST_string, "h", TEST_Type) == nullptr && lGetNumberOfElem(lp) == 1;
   lFreeList(&lp);
   // the number variants name a list they create differently; since CS-3077 they give nullptr for a field of
   // another type like the string variants, before they aborted
   lList *numbers = nullptr;
   const bool add_ulong = lAddElemUlong(&numbers, TEST_uint32, 7, TEST_Type) != nullptr &&
                          std::string(lGetListName(numbers)) == "ulong_sublist" &&
                          lAddElemUlong(&numbers, QU_qname, 7, TEST_Type) == nullptr &&
                          !aborts([&] { static_cast<void>(lAddElemUlong(&numbers, TEST_string, 7, TEST_Type)); }) &&
                          lAddElemUlong(&numbers, TEST_string, 7, TEST_Type) == nullptr && lGetNumberOfElem(numbers) == 1;
   lFreeList(&numbers);
   CHECK(39, "lAddElemStr/Host/Ulong create the list, give nullptr on null arguments and on a field that is not "
             "of the key's type", add_str && add_ulong);

   // deleting: found, not found, null list, null arguments; an emptied list is freed
   lList *strings = nullptr;
   lAddElemStr(&strings, TEST_string, "a", TEST_Type);
   lAddElemStr(&strings, TEST_string, "b", TEST_Type);
   lList *no_list = nullptr;
   const bool del_str = lDelElemStr(&strings, TEST_string, "x") == 0 && lDelElemStr(&strings, TEST_string, "a") == 1 &&
                        strings != nullptr && lDelElemStr(&strings, TEST_string, "b") == 1 && strings == nullptr &&
                        lDelElemStr(&no_list, TEST_string, "a") == 1 && lDelElemStr(nullptr, TEST_string, "a") == 0 &&
                        lDelElemStr(&no_list, TEST_string, nullptr) == 0;
   lList *ulongs = nullptr;
   lAddElemUlong(&ulongs, TEST_uint32, 1, TEST_Type);
   // since CS-3077 lDelElemUlong returns 0 when nothing is found, like lDelElemStr; before it returned 1
   const bool del_ulong = lDelElemUlong(&ulongs, TEST_uint32, 9) == 0 && ulongs != nullptr &&
                          lDelElemUlong(&ulongs, TEST_uint32, 1) == 1 && ulongs == nullptr &&
                          lDelElemUlong(nullptr, TEST_uint32, 1) == 0;
   CHECK(40, "lDelElemStr and lDelElemUlong return 0 when nothing is found and free an emptied list",
         del_str && del_ulong);

   // in a list field: a null element and a missing field
   lList *jobs = lCreateList("jobs", JB_Type);
   lAppendElem(jobs, lCreateElem(JB_Type));
   lListElem *job = lFirstRW(jobs);
   lEnumeration *what = lWhat("%T(%I)", JB_Type, JB_job_number);
   lList *reduced = lSelect("reduced", jobs, nullptr, what);
   lListElem *rjob = lFirstRW(reduced);
   const bool sub = lAddSubStr(job, VA_variable, "X", JB_env_list, VA_Type) != nullptr &&
                    lGetNumberOfElem(lGetList(job, JB_env_list)) == 1 &&
                    lAddSubStr(nullptr, VA_variable, "X", JB_env_list, VA_Type) == nullptr &&
                    aborts([&] { static_cast<void>(lAddSubStr(rjob, VA_variable, "X", JB_env_list, VA_Type)); }) &&
                    lDelSubStr(job, VA_variable, "X", JB_env_list) == 1 && lGetList(job, JB_env_list) == nullptr &&
                    lDelSubStr(nullptr, VA_variable, "X", JB_env_list) == 0 &&
                    aborts([&] { static_cast<void>(lDelSubStr(rjob, VA_variable, "X", JB_env_list)); });
   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&jobs);
   CHECK(41, "lAddSubStr and lDelSubStr do nothing for a null element and abort without the list field",
         sub);
}

/**
 * @brief Increments keep their results, the hash table and the aborts of before
 *
 * Pins what the old increments do beyond the error paths of LEGACY-03: the sum,
 * the 32 bit wrap-around, an offset of 0, the hash table of a hashed key field
 * and the abort on a duplicate in a unique field (CS-3077).
 *
 * @return nothing; CHECK records the results
 */
static void
test_increments() {
   // a hashed unique key: the lookup follows the new value, a duplicate aborts in a debug build
   lList *tasks = lCreateList("tasks", JAT_Type);
   for (lUlong i = 1; i <= 40; i++) {
      lAppendElem(tasks, lCreateElem(JAT_Type));
      lSetUlong(lLastRW(tasks), JAT_task_number, i);
   }
   lListElem *task = lGetElemUlongRW(tasks, JAT_task_number, 7);
   const bool hashed = lAddUlong(task, JAT_task_number, 100) == 0 && lGetUlong(task, JAT_task_number) == 107 &&
                       lGetElemUlong(tasks, JAT_task_number, 7) == nullptr &&
                       lGetElemUlong(tasks, JAT_task_number, 107) == task;
#ifdef ENABLE_DEBUG_CHECKS
   // task 9 plus 1 is the number task 10 holds
   lListElem *nine = lGetElemUlongRW(tasks, JAT_task_number, 9);
   const bool duplicate = aborts([&] { static_cast<void>(lAddUlong(nine, JAT_task_number, 1)); });
#else
   const bool duplicate = true;
#endif
   lFreeList(&tasks);
   CHECK(42, "lAddUlong on a hashed key keeps the lookup right and aborts on a duplicate in a debug build",
         hashed && duplicate);

   // the sum of each field type, the 32 bit wrap-around and an offset of 0
   lListElem *ep = lCreateElem(TEST_Type);
   lSetUlong(ep, TEST_uint32, 0xFFFFFFFFU);
   lSetUlong64(ep, TEST_uint64, 5);
   lSetDouble(ep, TEST_double, 1.5);
   const bool sums = lAddUlong(ep, TEST_uint32, 2) == 0 && lGetUlong(ep, TEST_uint32) == 1 &&
                     lAddUlong(ep, TEST_uint32, 0) == 0 && lGetUlong(ep, TEST_uint32) == 1 &&
                     lAddUlong64(ep, TEST_uint64, 0x100000000ULL) == 0 && lGetUlong64(ep, TEST_uint64) == 0x100000005ULL &&
                     lAddDouble(ep, TEST_double, 2.25) == 0 && lGetDouble(ep, TEST_double) == 3.75;
   lFreeElem(&ep);
   CHECK(43, "lAddUlong, lAddUlong64 and lAddDouble add, a 32 bit sum wraps around, an offset of 0 changes nothing",
         sums);
}

/**
 * @brief The keys (TEST_uint32) of the elements of a list, in list order
 * @param lp the list, or nullptr
 * @return the keys joined by commas, "null" for no list
 */
static std::string
test_keys(const lList *lp) {
   if (lp == nullptr) {
      return "null";
   }
   std::string keys;
   for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
      keys += (keys.empty() ? "" : ",") + std::to_string(lGetUlong(ep, TEST_uint32));
   }
   return keys;
}

/**
 * @brief The keys of the elements a condition selects from a list; frees the condition
 * @param lp the list
 * @param cp the condition, or nullptr; freed here
 * @return the keys joined by commas, "null" if lSelect() returned no list
 */
static std::string
selected_keys(const lList *lp, lCondition *cp) {
   lEnumeration *all = lWhat("%T(ALL)", TEST_Type);
   lList *selected = lSelect("selected", lp, cp, all);
   std::string keys = test_keys(selected);
   lFreeList(&selected);
   lFreeWhat(&all);
   lFreeWhere(&cp);
   return keys;
}

/**
 * @brief Build the list the selection pins work on
 *
 * Four TEST elements, keyed 1 to 4 in TEST_uint32; the fourth holds no string
 * and no host.
 *
 * @return the list, owned by the caller
 */
static lList *
selection_list() {
   struct Row {
      lUlong key;
      lUlong64 u64;
      int i;
      lLong l;
      double d;
      bool b;
      const char *s;
      const char *h;
   };
   const Row rows[] = {{1, 10, -1, -10, 0.5, true, "alpha", "node1"},
                       {2, 20, 0, 0, 1.5, false, "beta", "node2"},
                       {3, 30, 1, 10, 2.5, true, "Gamma", "NODE3"},
                       {4, 40, 2, 20, 3.5, false, nullptr, nullptr}};
   lList *lp = lCreateList("selection", TEST_Type);
   for (const Row &r : rows) {
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_uint32, r.key);
      lSetUlong64(ep, TEST_uint64, r.u64);
      lSetInt(ep, TEST_int, r.i);
      lSetLong(ep, TEST_long, r.l);
      lSetDouble(ep, TEST_double, r.d);
      lSetBool(ep, TEST_bool, r.b);
      lSetString(ep, TEST_string, r.s);
      lSetHost(ep, TEST_host, r.h);
      lAppendElem(lp, ep);
   }
   return lp;
}

/**
 * @brief Selections keep the results of before
 *
 * Pins what lWhat(), lWhere(), lSelect(), lSelectDestroy(), the lFind*RW()
 * functions, lAndWhere(), lOrWhere(), lCompare() and the transfer of a
 * condition do, before their bodies move into the core (CS-3077).
 *
 * @return nothing; CHECK records the results
 */
static void
test_selections() {
   // lWhat: nested, sorted by position, a field given twice kept once, ALL and NONE, the flat form
   lEnumeration *nested = lWhat("%T(%I->%T(%I)%I)", US_Type, US_entries, UE_Type, UE_name, US_name);
   const bool nested_ok = nested != nullptr && lCountWhat(nested, US_Type) == 2 && nested[0].nm == US_name &&
                          nested[0].ep == nullptr && nested[1].nm == US_entries && nested[1].ep != nullptr &&
                          nested[1].ep[0].nm == UE_name && nested[1].ep[1].nm == NoName;
   lFreeWhat(&nested);
   // a sub-selection on a field that is not a list is taken, one without its %T fails
   lEnumeration *on_string = lWhat("%T(%I->%T(%I))", US_Type, US_name, UE_Type, UE_name);
   const bool nesting_quirks = on_string != nullptr && on_string[0].ep != nullptr &&
                               lWhat("%T(%I%I->(%I))", US_Type, US_name, US_entries, UE_name) == nullptr;
   lFreeWhat(&on_string);
   lEnumeration *twice = lWhat("%T(%I%I)", US_Type, US_name, US_name);
   const bool twice_ok = twice != nullptr && lCountWhat(twice, US_Type) == 1;
   lFreeWhat(&twice);
   lEnumeration *all = lWhat("%T(ALL)", US_Type);
   lEnumeration *none = lWhat("%T(NONE)", US_Type);
   lEnumeration *all2 = lWhatAll();
   const bool special = all != nullptr && all[0].pos == WHAT_ALL && none != nullptr && none[0].pos == WHAT_NONE &&
                        all2 != nullptr && all2[0].pos == WHAT_ALL && lCountWhat(none, US_Type) == 0;
   lFreeWhat(&all);
   lFreeWhat(&none);
   lFreeWhat(&all2);
   const int fields[] = {US_type, US_name, NoName};
   const int foreign[] = {US_name, JB_job_number, NoName};
   const int empty[] = {NoName};
   lEnumeration *flat = lIntVector2What(US_Type, fields);
   const bool flat_ok = flat != nullptr && lCountWhat(flat, US_Type) == 2 && flat[0].nm == US_name;
   lFreeWhat(&flat);
   cull_state_set_lerrno(0);
   const bool flat_foreign = lIntVector2What(US_Type, foreign) == nullptr && cull_state_get_lerrno() == LENAMENOT;
   cull_state_set_lerrno(0);
   const bool flat_empty = lIntVector2What(US_Type, empty) == nullptr && cull_state_get_lerrno() == LENAMENOT;
   CHECK(44, "lWhat nests, sorts by position and keeps a repeated field once, takes a sub-selection on any field and fails "
             "on one without %T; ALL, NONE and lIntVector2What as before",
         nested_ok && nesting_quirks && twice_ok && special && flat_ok && flat_foreign && flat_empty);

   // lWhere: the comparisons of every value type
   lList *lp = selection_list();
   const bool numbers =
           selected_keys(lp, lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 2U)) == "2" &&
           selected_keys(lp, lWhere("%T(%I != %u)", TEST_Type, TEST_uint32, 2U)) == "1,3,4" &&
           selected_keys(lp, lWhere("%T(%I < %u)", TEST_Type, TEST_uint32, 2U)) == "1" &&
           selected_keys(lp, lWhere("%T(%I <= %u)", TEST_Type, TEST_uint32, 2U)) == "1,2" &&
           selected_keys(lp, lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 2U)) == "3,4" &&
           selected_keys(lp, lWhere("%T(%I >= %u)", TEST_Type, TEST_uint32, 2U)) == "2,3,4" &&
           selected_keys(lp, lWhere("%T(%I > %lu)", TEST_Type, TEST_uint64, static_cast<lUlong64>(15))) == "2,3,4" &&
           selected_keys(lp, lWhere("%T(%I < %d)", TEST_Type, TEST_int, 0)) == "1" &&
           selected_keys(lp, lWhere("%T(%I >= %o)", TEST_Type, TEST_long, static_cast<lLong>(10))) == "3,4" &&
           selected_keys(lp, lWhere("%T(%I > %g)", TEST_Type, TEST_double, 1.0)) == "2,3,4" &&
           selected_keys(lp, lWhere("%T(%I == %b)", TEST_Type, TEST_bool, true)) == "1,3";
   // a null string is neither equal nor unequal; strcmp orders upper case first. "Gamma" < "beta" held only in a
   // build whose strcmp() returns -1 and 1 (ASAN): the old lCompare() took no other result, fixed in CS-3077
   const bool strings =
           selected_keys(lp, lWhere("%T(%I == %s)", TEST_Type, TEST_string, "beta")) == "2" &&
           selected_keys(lp, lWhere("%T(%I != %s)", TEST_Type, TEST_string, "beta")) == "1,3" &&
           selected_keys(lp, lWhere("%T(%I < %s)", TEST_Type, TEST_string, "beta")) == "1,3" &&
           selected_keys(lp, lWhere("%T(%I == %s)", TEST_Type, TEST_host, "node2")) == "2";
   CHECK(45, "lWhere compares 32 and 64 bit, int, long, double, bool, string and host fields as before; "
             "a null string matches neither == nor !=", numbers && strings);

   // the named comparisons; %f is no double token, m= on a 64 bit field selects nothing
   const bool named =
           selected_keys(lp, lWhere("%T(%I c= %s)", TEST_Type, TEST_string, "GAMMA")) == "3" &&
           selected_keys(lp, lWhere("%T(%I p= %s)", TEST_Type, TEST_string, "*a")) == "1,2,3" &&
           selected_keys(lp, lWhere("%T(%I p= %s)", TEST_Type, TEST_string, "*")) == "1,2,3,4" &&
           selected_keys(lp, lWhere("%T(%I h= %s)", TEST_Type, TEST_host, "node3")) == "3" &&
           selected_keys(lp, lWhere("%T(%I m= %u)", TEST_Type, TEST_uint32, 3U)) == "3" &&
           selected_keys(lp, lWhere("%T(%I m= %lu)", TEST_Type, TEST_uint64, static_cast<lUlong64>(8))) == "null";
   cull_state_set_lerrno(0);
   const bool float_token = lWhere("%T(%I > %f)", TEST_Type, TEST_double, 1.0) == nullptr &&
                            cull_state_get_lerrno() == LEPARSECOND;
   CHECK(46, "c=, p=, h= and m= as before: a pattern matches a null string as empty, m= on a 64 bit field "
             "selects nothing, %f is no token of lWhere", named && float_token);

   // combinations, a sub-list, and the combinations built by hand
   const bool logic =
           selected_keys(lp, lWhere("%T(%I < %u && %I == %b)", TEST_Type, TEST_uint32, 3U, TEST_bool, true)) == "1" &&
           selected_keys(lp, lWhere("%T(%I == %u || %I == %u)", TEST_Type, TEST_uint32, 1U, TEST_uint32, 4U)) == "1,4" &&
           selected_keys(lp, lWhere("%T(!(%I == %u || %I == %u))", TEST_Type, TEST_uint32, 1U, TEST_uint32, 4U)) == "2,3";
   const bool by_hand =
           selected_keys(lp, lOrWhere(lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 1U),
                                      lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 3U))) == "1,3" &&
           selected_keys(lp, lAndWhere(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 1U),
                                       lWhere("%T(%I < %u)", TEST_Type, TEST_uint32, 3U))) == "2";
   cull_state_set_lerrno(0);
   const bool null_operand = lOrWhere(nullptr, nullptr) == nullptr && cull_state_get_lerrno() == LECONDNULL;
   lCondition *original = lWhere("%T(%I p= %s && %I != %u)", TEST_Type, TEST_string, "*a", TEST_uint32, 2U);
   const bool copied = selected_keys(lp, lCopyWhere(original)) == "1,3";
   lFreeWhere(&original);

   lList *sets = lCreateList("sets", US_Type);
   const char *const set_entries[][2] = {{"a", "x"}, {"b", "y"}, {"c", nullptr}};
   for (const auto &entry : set_entries) {
      lListElem *set = lAddElemStr(&sets, US_name, entry[0], US_Type);
      if (entry[1] != nullptr) {
         lAddSubStr(set, UE_name, entry[1], US_entries, UE_Type);
      }
   }
   lCondition *sub = lWhere("%T(%I -> %T(%I == %s))", US_Type, US_entries, UE_Type, UE_name, "y");
   lEnumeration *set_all = lWhat("%T(ALL)", US_Type);
   lList *with_y = lSelect("y", sets, sub, set_all);
   const bool sub_list = lGetNumberOfElem(with_y) == 1 && strcmp(lGetString(lFirst(with_y), US_name), "b") == 0;
   lFreeList(&with_y);
   lFreeWhat(&set_all);
   lFreeWhere(&sub);
   CHECK(47, "&&, || and ! as before, lAndWhere and lOrWhere take their operands, lCopyWhere copies, "
             "-> matches an element of a sub-list", logic && by_hand && null_operand && copied && sub_list);

   // lWhere errors: unknown field and nesting on a field that is not a list give nullptr, a wrong value aborts
   cull_state_set_lerrno(0);
   const bool foreign_field = lWhere("%T(%I == %u)", TEST_Type, JB_job_number, 1U) == nullptr &&
                              cull_state_get_lerrno() == LEPARSECOND;
   const bool not_a_list = lWhere("%T(%I -> %T(%I == %s))", US_Type, US_name, UE_Type, UE_name, "y") == nullptr;
   const bool wrong_value = aborts([] {
      lCondition *cp = lWhere("%T(%I == %s)", TEST_Type, TEST_uint32, "1");
      lFreeWhere(&cp);
   });
   CHECK(48, "lWhere gives nullptr for a foreign field and for -> on a field that is not a list, "
             "and aborts on a value of another type", foreign_field && not_a_list && wrong_value);

   // lSelect: reduction, no condition, no enumeration (the condition is then ignored), an empty result
   lEnumeration *keys_only = lWhat("%T(%I)", TEST_Type, TEST_uint32);
   lList *reduced = lSelect("reduced", lp, nullptr, keys_only);
   const bool reduction = test_keys(reduced) == "1,2,3,4" && strcmp(lGetListName(reduced), "reduced") == 0 &&
                          lGetPosViaElem(lFirst(reduced), TEST_string, SGE_NO_ABORT) < 0;
   lFreeList(&reduced);
   lCondition *one = lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 1U);
   lList *ignored = lSelect("ignored", lp, one, nullptr);
   const bool without_what = test_keys(ignored) == "1,2,3,4" && strcmp(lGetListName(ignored), "selection") == 0;
   lFreeList(&ignored);
   lCondition *nothing = lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 10U);
   cull_state_set_lerrno(0);
   const bool empty_result = lSelect("empty", lp, nothing, keys_only) == nullptr &&
                             cull_state_get_lerrno() == LEGETNROFELEM && lSelect("none", nullptr, one, keys_only) == nullptr;
   lFreeWhere(&one);
   lFreeWhere(&nothing);
   lFreeWhat(&keys_only);
   CHECK(49, "lSelect reduces and names the result; without a selection it copies the whole list under its own "
             "name and ignores the condition; an empty result is nullptr", reduction && without_what && empty_result);

   // the lFind*RW functions and lCompare
   lCondition *odd = lWhere("%T(%I m= %u)", TEST_Type, TEST_uint32, 1U);
   const lListElem *first = lFindFirstRW(lp, odd);
   const lListElem *next = lFindNextRW(first, odd);
   const lListElem *last = lFindLastRW(lp, odd);
   const lListElem *prev = lFindPrevRW(last, odd);
   const bool finds = first != nullptr && lGetUlong(first, TEST_uint32) == 1 && next != nullptr &&
                      lGetUlong(next, TEST_uint32) == 3 && lFindNextRW(next, odd) == nullptr && last == next &&
                      prev == first && lFindFirstRW(nullptr, odd) == nullptr && lFindFirstRW(lp, nullptr) == lFirst(lp);
   cull_state_set_lerrno(0);
   const bool compare = lCompare(first, odd) == 1 && lCompare(lNext(first), odd) == 0 && lCompare(first, nullptr) == 1 &&
                        lCompare(nullptr, odd) == 0 && cull_state_get_lerrno() == LEELEMNULL;
   CHECK(50, "lFindFirstRW, lFindNextRW, lFindLastRW and lFindPrevRW find as before; lCompare takes no condition "
             "as true and a null element as false", finds && compare);

   // lSelectDestroy keeps what matches and frees an emptied list
   lList *kept = lSelectDestroy(lCopyList("copy", lp), odd);
   const bool keeps = test_keys(kept) == "1,3";
   lFreeList(&kept);
   lCondition *none_match = lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 10U);
   const bool emptied = lSelectDestroy(lCopyList("copy", lp), none_match) == nullptr;
   lFreeWhere(&none_match);

   // a condition survives the transfer
   lListElem *packed = lWhereToElem(odd);
   lCondition *unpacked = lWhereFromElem(packed);
   const bool transfer = unpacked != nullptr && selected_keys(lp, unpacked) == "1,3";
   lFreeElem(&packed);
   lFreeWhere(&odd);
   CHECK(51, "lSelectDestroy keeps the matching elements and frees an emptied list; "
             "lWhereToElem and lWhereFromElem keep a condition", keeps && emptied && transfer);

   lFreeList(&sets);
   lFreeList(&lp);
}

/**
 * @brief The functions the core used from the old interface keep their results
 *
 * Pins what lCopyElem() and lSelect() do with sub-lists and sub-objects through
 * lCopySwitchPack(), what the iterating lookups lGetElem*First()/Next() find,
 * and what lCompListDescr(), lCountDescr(), lGetListName() and lGetPosInDescr()
 * report, before the core stops calling them (CS-3077).
 *
 * @return nothing; CHECK records the results
 */
static void
test_core_helpers() {
   // lCopyElem: a sub-list, an empty sub-list, no sub-list, a sub-object
   lListElem *set = lCreateElem(US_Type);
   lSetString(set, US_name, "set");
   lAddSubStr(set, UE_name, "alice", US_entries, UE_Type);
   lListElem *copy = lCopyElem(set);
   const bool sub_list = lGetNumberOfElem(lGetList(copy, US_entries)) == 1 &&
                         lGetList(copy, US_entries) != lGetList(set, US_entries) &&
                         strcmp(lGetString(lFirst(lGetList(copy, US_entries)), UE_name), "alice") == 0;
   lFreeElem(&copy);
   lSetList(set, US_entries, lCreateList("empty", UE_Type));
   copy = lCopyElem(set);
   const bool empty_kept = lGetList(copy, US_entries) != nullptr && lGetNumberOfElem(lGetList(copy, US_entries)) == 0;
   lFreeElem(&copy);
   lSetList(set, US_entries, nullptr);
   copy = lCopyElem(set);
   const bool null_kept = lGetList(copy, US_entries) == nullptr;
   lFreeElem(&copy);

   lListElem *holder = lCreateElem(TEST_Type);
   lListElem *job = lCreateElem(JB_Type);
   lSetUlong(job, JB_job_number, 7);
   lSetObject(holder, TEST_obj_jb, job);
   copy = lCopyElem(holder);
   const lListElem *copied_job = lGetObject(copy, TEST_obj_jb);
   const bool sub_object = copied_job != nullptr && copied_job != job && lGetUlong(copied_job, JB_job_number) == 7;
   lFreeElem(&copy);
   lFreeElem(&holder);
   CHECK(52, "lCopyElem copies a sub-list, keeps an empty one empty and a missing one missing, and copies a sub-object",
         sub_list && empty_kept && null_kept && sub_object);

   // a field marked CULL_NO_TRANSFER is left at its default in a copy that is handed out
   lListElem *cct = lCreateElem(CCT_Type);
   int marker = 0;
   lSetRef(cct, CCT_pe_job_slots, &marker);
   lListElem *kept = lCopyElemHash(cct, true, false);
   lListElem *skipped = lCopyElemHash(cct, true, true);
   const bool no_transfer = lGetRef(kept, CCT_pe_job_slots) == &marker && lGetRef(skipped, CCT_pe_job_slots) == nullptr;
   lFreeElem(&kept);
   lFreeElem(&skipped);
   lFreeElem(&cct);

   // lSelect with a nested selection: a sub-list that comes out empty is no list
   lList *sets = lCreateList("sets", US_Type);
   lListElem *with = lAddElemStr(&sets, US_name, "with", US_Type);
   lAddSubStr(with, UE_name, "bob", US_entries, UE_Type);
   lListElem *empty = lAddElemStr(&sets, US_name, "empty", US_Type);
   lSetList(empty, US_entries, lCreateList("empty", UE_Type));
   lEnumeration *nested = lWhat("%T(%I%I->%T(%I))", US_Type, US_name, US_entries, UE_Type, UE_name);
   lList *selected = lSelect("selected", sets, nullptr, nested);
   const lListElem *sel_with = lGetElemStr(selected, US_name, "with");
   const lListElem *sel_empty = lGetElemStr(selected, US_name, "empty");
   const bool nested_empty = sel_with != nullptr && lGetNumberOfElem(lGetList(sel_with, US_entries)) == 1 &&
                             sel_empty != nullptr && lGetList(sel_empty, US_entries) == nullptr;
   lFreeList(&selected);
   lFreeWhat(&nested);
   lFreeList(&sets);
   lFreeElem(&set);
   CHECK(53, "a CULL_NO_TRANSFER field is left out of a copy handed out; lSelect makes an emptied sub-list no list",
         no_transfer && nested_empty);

   // the iterating lookups, below and above the hash threshold
   bool walks = true;
   for (const int n : {5, 40}) {
      lList *lp = lCreateList("rows", TEST_Type);
      int expected = 0;
      for (int i = 1; i <= n; i++) {
         lListElem *ep = lCreateElem(TEST_Type);
         lSetUlong(ep, TEST_uint32, i);
         lSetUlong64(ep, TEST_uint64, i % 3);
         const std::string text = "s" + std::to_string(i % 3);
         lSetString(ep, TEST_string, text.c_str());
         // the host names differ in case only, which the lookup ignores
         lSetHost(ep, TEST_host, i % 2 == 0 ? (i % 3 == 1 ? "H1" : "h0") : (i % 3 == 1 ? "h1" : "h2"));
         lAppendElem(lp, ep);
         expected += i % 3 == 1 ? 1 : 0;
      }
      const auto count = [](auto first, auto next) {
         const void *iterator = nullptr;
         int found = 0;
         for (const lListElem *ep = first(&iterator); ep != nullptr; ep = next(&iterator)) {
            found++;
         }
         return found == 0 || iterator == nullptr ? found : -1;
      };
      const int by_u64 = count([&](const void **it) { return lGetElemUlong64First(lp, TEST_uint64, 1, it); },
                               [&](const void **it) { return lGetElemUlong64Next(lp, TEST_uint64, 1, it); });
      const int by_str = count([&](const void **it) { return lGetElemStrFirst(lp, TEST_string, "s1", it); },
                               [&](const void **it) { return lGetElemStrNext(lp, TEST_string, "s1", it); });
      const int by_host = count([&](const void **it) { return lGetElemHostFirst(lp, TEST_host, "h1", it); },
                                [&](const void **it) { return lGetElemHostNext(lp, TEST_host, "h1", it); });
      const int by_key = count([&](const void **it) { return lGetElemUlongFirst(lp, TEST_uint32, 4, it); },
                               [&](const void **it) { return lGetElemUlongNext(lp, TEST_uint32, 4, it); });
      walks = walks && by_u64 == expected && by_str == expected && by_host == expected && by_key == 1;
      lFreeList(&lp);
   }
   CHECK(54, "lGetElemUlong64/Str/Host/UlongFirst and Next walk every match below and above the hash threshold "
             "and end with a null iterator", walks);

   // the descriptor helpers
   lEnumeration *name_only = lWhat("%T(%I)", US_Type, US_name);
   const lDescr *reduced = nullptr;
   lReduceDescr(&reduced, US_Type, name_only);
   cull_state_set_lerrno(0);
   const bool comp = lCompListDescr(US_Type, US_Type) == 0 && lCompListDescr(US_Type, reduced) == -1 &&
                     lCompListDescr(US_Type, JB_Type) == -1 && lCompListDescr(nullptr, US_Type) == -1 &&
                     cull_state_get_lerrno() == LELISTNULL;
   cull_state_set_lerrno(0);
   const bool count_descr = lCountDescr(nullptr) == -1 && cull_state_get_lerrno() == LEDESCRNULL &&
                            lCountDescr(US_Type) == lCountDescr(reduced) && lCountDescr(US_Type) > 0;
   cull_state_set_lerrno(0);
   const bool pos = lGetPosInDescr(reduced, US_type) == -1 && lGetPosInDescr(reduced, US_name) >= 0 &&
                    lGetPosInDescr(US_Type, JB_job_number) == -1 && cull_state_get_lerrno() == 0;
   ocs_cull_descr_release(&reduced);
   lFreeWhat(&name_only);
   lList *unnamed = lCreateList(nullptr, US_Type);
   cull_state_set_lerrno(0);
   const bool names = strcmp(lGetListName(unnamed), "No list name specified") == 0 &&
                      strcmp(lGetListName(nullptr), "No List specified") == 0 && cull_state_get_lerrno() == LELISTNULL;
   lFreeList(&unnamed);
   CHECK(55, "lCompListDescr, lCountDescr, lGetPosInDescr and lGetListName report as before",
         comp && count_descr && pos && names);
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

/**
 * @brief Build a list of TEST elements with every value type, some values repeated, a string and a host missing
 * @param n the number of elements
 * @param hashed true for a list with hash tables
 * @return the list, in insertion order
 */
static lList *
sort_rows(const int n, const bool hashed) {
   lList *lp = lCreateListHash("rows", TEST_Type, hashed);
   for (int i = 0; i < n; i++) {
      const int k = (i * 7) % n;  // a permutation of 0 .. n-1 for an n that 7 does not divide
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_uint32, k + 1);
      lSetUlong64(ep, TEST_uint64, (k % 4) * 10000000000ULL);
      lSetInt(ep, TEST_int, (k % 5) - 2);
      lSetLong(ep, TEST_long, (k % 3) - 1);
      lSetDouble(ep, TEST_double, (k % 6) * 0.5 - 1.0);
      lSetBool(ep, TEST_bool, k % 2 == 0);
      if (k % 7 != 0) {
         lSetString(ep, TEST_string, ("s" + std::to_string(k % 9)).c_str());
         lSetHost(ep, TEST_host, ("h" + std::to_string(k % 4)).c_str());
      }
      lAppendElem(lp, ep);
   }
   return lp;
}

/**
 * @brief Compare one field of two elements as the old sort did, independently of it
 * @param a the first element
 * @param b the second element
 * @param nm the field
 * @return negative, 0 or positive
 */
static int
sort_field_cmp(const lListElem *a, const lListElem *b, const int nm) {
   auto sign = [](auto x, auto y) { return x < y ? -1 : (x > y ? 1 : 0); };
   switch (nm) {
      case TEST_uint32: return sign(lGetUlong(a, nm), lGetUlong(b, nm));
      case TEST_uint64: return sign(lGetUlong64(a, nm), lGetUlong64(b, nm));
      case TEST_int: return sign(lGetInt(a, nm), lGetInt(b, nm));
      case TEST_long: return sign(lGetLong(a, nm), lGetLong(b, nm));
      case TEST_double: return sign(lGetDouble(a, nm), lGetDouble(b, nm));
      case TEST_bool: return sign(lGetBool(a, nm), lGetBool(b, nm));
      case TEST_string: return sge_strnullcmp(lGetString(a, nm), lGetString(b, nm));
      case TEST_host: return sge_strnullcmp(lGetHost(a, nm), lGetHost(b, nm));
      default: return 0;
   }
}

/**
 * @brief Whether a list is sorted by one or two keys
 * @param lp the list
 * @param nm1 the first key
 * @param dir1 +1 ascending, -1 descending
 * @param nm2 the second key, or NoName
 * @param dir2 +1 ascending, -1 descending
 * @return true if no two neighbours are out of order
 */
static bool
is_sorted_by(const lList *lp, const int nm1, const int dir1, const int nm2 = NoName, const int dir2 = 1) {
   const lListElem *prev = nullptr;
   for_each_ep_lv(ep, lp) {
      if (prev != nullptr) {
         int c = sort_field_cmp(prev, ep, nm1) * dir1;
         if (c == 0 && nm2 != NoName) {
            c = sort_field_cmp(prev, ep, nm2) * dir2;
         }
         if (c > 0) {
            return false;
         }
      }
      prev = ep;
   }
   return true;
}

/**
 * @brief Pin the sort functions of the old interface (CS-3077, sorting group)
 *
 * Only the order of the keys is pinned, not the order of elements with equal
 * keys: qsort() makes no promise about it.
 */
static void
test_sorting() {
   // one key, every value type, both directions
   bool one_key = true;
   for (const int nm : {TEST_uint32, TEST_uint64, TEST_int, TEST_long, TEST_double, TEST_bool, TEST_string, TEST_host}) {
      for (const int dir : {1, -1}) {
         lList *lp = sort_rows(40, false);
         const int rc = lPSortList(lp, dir > 0 ? "%I+" : "%I-", nm);
         one_key = one_key && rc == 0 && lGetNumberOfElem(lp) == 40 && is_sorted_by(lp, nm, dir);
         lFreeList(&lp);
      }
   }
   // two keys, an empty and a one-element list, no list
   lList *lp = sort_rows(40, false);
   const bool two_keys = lPSortList(lp, "%I-%I+", TEST_uint64, TEST_uint32) == 0 &&
                         is_sorted_by(lp, TEST_uint64, -1, TEST_uint32, 1) && lFirst(lp)->prev == nullptr &&
                         lLast(lp)->next == nullptr;
   lFreeList(&lp);
   lList *empty = lCreateList("empty", TEST_Type);
   lList *one = sort_rows(1, false);
   const bool small = lPSortList(empty, "%I+", TEST_uint32) == 0 && lGetNumberOfElem(empty) == 0 &&
                      lPSortList(one, "%I+", TEST_uint32) == 0 && lGetNumberOfElem(one) == 1;
   lFreeList(&empty);
   lFreeList(&one);
   cull_state_set_lerrno(0);
   const bool no_list = lPSortList(nullptr, "%I+", TEST_uint32) == -1 && cull_state_get_lerrno() == LELISTNULL &&
                        lSortList(nullptr, nullptr) == 0;
   CHECK(56, "lPSortList orders by one key of every value type in both directions and by two keys, and keeps "
             "empty, one-element and null lists as before",
         one_key && two_keys && small && no_list);

   // a hashed list: every lookup still finds its element after sorting
   lp = sort_rows(40, true);
   bool found = lPSortList(lp, "%I-", TEST_uint32) == 0 && is_sorted_by(lp, TEST_uint32, -1);
   for (lUlong k = 1; k <= 40; k++) {
      const lListElem *ep = lGetElemUlong(lp, TEST_uint32, k);
      found = found && ep != nullptr && lGetUlong(ep, TEST_uint32) == k;
   }
   const void *iterator = nullptr;
   int zeros = 0;
   for (const lListElem *ep = lGetElemUlong64First(lp, TEST_uint64, 0, &iterator); ep != nullptr;
        ep = lGetElemUlong64Next(lp, TEST_uint64, 0, &iterator)) {
      zeros++;
   }
   found = found && zeros == 10;
   lFreeList(&lp);
   CHECK(57, "sorting a hashed list keeps its unique and non-unique lookups working", found);

   // inserting sorted, resorting
   lSortOrder *so = lParseSortOrderVarArg(TEST_Type, "%I+", TEST_uint32);
   lp = lCreateList("sorted", TEST_Type);
   bool inserted = so != nullptr;
   for (const lUlong k : {20u, 10u, 30u, 5u, 25u, 40u}) {
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_uint32, k);
      inserted = inserted && lInsertSorted(so, ep, lp) == 0;
   }
   inserted = inserted && lGetNumberOfElem(lp) == 6 && is_sorted_by(lp, TEST_uint32, 1) &&
              lGetUlong(lFirst(lp), TEST_uint32) == 5 && lGetUlong(lLast(lp), TEST_uint32) == 40;
   lListElem *dummy = lCreateElem(TEST_Type);
   const bool null_args = lInsertSorted(nullptr, dummy, lp) == -1 && lInsertSorted(so, nullptr, lp) == -1 &&
                          lInsertSorted(so, dummy, nullptr) == -1;
   lFreeElem(&dummy);
   lListElem *moved = lGetElemUlongRW(lp, TEST_uint32, 10);
   lSetUlong(moved, TEST_uint32, 35);
   const bool resorted = lResortElem(so, moved, lp) == 0 && lGetNumberOfElem(lp) == 6 &&
                         is_sorted_by(lp, TEST_uint32, 1) && lNext(moved) != nullptr &&
                         lGetUlong(lNext(moved), TEST_uint32) == 40;
   // lSortCompare and lSortList with an order of their own
   const lListElem *first = lFirst(lp);
   const lListElem *second = lNext(first);
   lSortOrder *desc = lParseSortOrderVarArg(TEST_Type, "%I-", TEST_uint32);
   const bool compare = lSortCompare(first, second, so) < 0 && lSortCompare(second, first, so) > 0 &&
                        lSortCompare(first, first, so) == 0 && lSortCompare(first, second, desc) > 0;
   const bool sort_list = lSortList(lp, desc) == 0 && is_sorted_by(lp, TEST_uint32, -1);
   lFreeSortOrder(&desc);
   lFreeSortOrder(&so);
   lFreeList(&lp);
   CHECK(58, "lInsertSorted keeps a list sorted and returns -1 for a null argument, lResortElem moves a changed "
             "element, lSortCompare and lSortList follow the order",
         inserted && null_args && resorted && compare && sort_list && so == nullptr);

   // building orders: unknown fields, a missing direction, by hand
   cull_state_set_lerrno(0);
   const bool unknown = lParseSortOrderVarArg(TEST_Type, "%I+", JB_job_number) == nullptr &&
                        cull_state_get_lerrno() == LENAMENOT;
   cull_state_set_lerrno(0);
   const bool syntax = lParseSortOrderVarArg(TEST_Type, "%I", TEST_uint32) == nullptr &&
                       cull_state_get_lerrno() == LESYNTAX;
   lp = sort_rows(3, false);
   cull_state_set_lerrno(0);
   const bool psort_unknown = lPSortList(lp, "%I+", JB_job_number) == -1 &&
                              cull_state_get_lerrno() == LEPARSESORTORD;
   lSortOrder *hand = lCreateSortOrder(2);
   const bool by_hand = hand != nullptr && lAddSortCriteria(TEST_Type, hand, TEST_bool, -1) == 0 &&
                        lAddSortCriteria(TEST_Type, hand, TEST_uint32, 1) == 0 &&
                        lAddSortCriteria(TEST_Type, hand, JB_job_number, 1) == -1 && lSortList(lp, hand) == 0 &&
                        is_sorted_by(lp, TEST_bool, -1, TEST_uint32, 1);
   lFreeSortOrder(&hand);
   lFreeList(&lp);
   CHECK(59, "building a sort order reports an unknown field and a missing direction as before",
         unknown && syntax && psort_unknown && by_hand);
}

/**
 * @brief The bytes a filled pack buffer holds, header included
 * @param pb the buffer
 * @return the bytes
 */
static std::vector<char>
packed_bytes(const sge_pack_buffer &pb) {
   return {pb.head_ptr, pb.head_ptr + pb.bytes_used};
}

/**
 * @brief A buffer to read from, made of the first bytes of a packed buffer
 * @param bytes the packed bytes
 * @param n how many of them
 * @param[out] pb the buffer, which owns a copy of the bytes afterwards
 * @return the result of init_packbuffer_from_buffer()
 */
static int
read_buffer(const std::vector<char> &bytes, const std::size_t n, sge_pack_buffer *pb) {
   char *copy = static_cast<char *>(malloc(n > 0 ? n : 1));
   memcpy(copy, bytes.data(), n);
   return init_packbuffer_from_buffer(pb, copy, static_cast<uint32_t>(n), false);
}

/**
 * @brief The size of the header a pack buffer starts with: padding, version and the empty sender information
 * @return the size in bytes
 */
static std::size_t
header_size() {
   sge_pack_buffer pb;
   init_packbuffer(&pb, 0, false, false);
   const std::size_t n = pb.bytes_used;
   clear_packbuffer(&pb);
   return n;
}

/**
 * @brief Whether unpacking from every cut of a packed object, after the header, fails with PACK_FORMAT
 *
 * @param bytes the packed object with its header
 * @param unpack unpacks one object from a buffer, returning the code and whether its result is null
 * @return true if every cut gives PACK_FORMAT and a null result, and the whole buffer PACK_SUCCESS
 */
static bool
every_cut_fails(const std::vector<char> &bytes, const std::function<int(sge_pack_buffer *, bool *)> &unpack) {
   const std::size_t header = header_size();
   for (std::size_t n = header; n < bytes.size(); n++) {
      sge_pack_buffer pb;
      if (read_buffer(bytes, n, &pb) != PACK_SUCCESS) {
         clear_packbuffer(&pb);
         return false;
      }
      bool null_result = false;
      const int ret = unpack(&pb, &null_result);
      clear_packbuffer(&pb);
      if (ret != PACK_FORMAT || !null_result) {
         printf("cut at %zu of %zu: %d, %s result\n", n, bytes.size(), ret, null_result ? "null" : "a");
         return false;
      }
   }
   sge_pack_buffer pb;
   read_buffer(bytes, bytes.size(), &pb);
   bool null_result = true;
   const int ret = unpack(&pb, &null_result);
   clear_packbuffer(&pb);
   return ret == PACK_SUCCESS && !null_result;
}

/**
 * @brief Pin the pack buffer and the packing of objects of the old interface (CS-3077, pack group)
 *
 * The bytes themselves are pinned by test_cull_pack_format; this pins the
 * error results and that a failed unpack leaves nothing behind (ASAN).
 */
static void
test_pack() {
   // the primitives there and back, then past the end
   sge_pack_buffer pb;
   bool primitives = init_packbuffer(&pb, 0, false, false) == PACK_SUCCESS && packint(&pb, 7) == PACK_SUCCESS &&
                     packint64(&pb, 1ULL << 40) == PACK_SUCCESS && packdouble(&pb, 2.5) == PACK_SUCCESS &&
                     packstr(&pb, "abc") == PACK_SUCCESS && packstr(&pb, nullptr) == PACK_SUCCESS &&
                     packbuf(&pb, "xyz", 3) == PACK_SUCCESS;
   const std::vector<char> bytes = packed_bytes(pb);
   clear_packbuffer(&pb);
   sge_pack_buffer rb;
   primitives = primitives && read_buffer(bytes, bytes.size(), &rb) == PACK_SUCCESS;
   uint32_t i = 0;
   uint64_t i64 = 0;
   double d = 0;
   char *str = nullptr;
   char *null_str = reinterpret_cast<char *>(1);
   char *buf = nullptr;
   primitives = primitives && unpackint(&rb, &i) == PACK_SUCCESS && i == 7 && unpackint64(&rb, &i64) == PACK_SUCCESS &&
                i64 == (1ULL << 40) && unpackdouble(&rb, &d) == PACK_SUCCESS && d == 2.5 &&
                unpackstr(&rb, &str) == PACK_SUCCESS && str != nullptr && strcmp(str, "abc") == 0 &&
                unpackstr(&rb, &null_str) == PACK_SUCCESS && null_str == nullptr &&
                unpackbuf(&rb, &buf, 3) == PACK_SUCCESS && buf != nullptr && memcmp(buf, "xyz", 3) == 0;
   sge_free(&str);
   sge_free(&buf);
   char *past_str = nullptr;
   char *past_buf = nullptr;
   const bool past_end = unpackint(&rb, &i) == PACK_FORMAT && unpackint64(&rb, &i64) == PACK_FORMAT &&
                         unpackdouble(&rb, &d) == PACK_FORMAT && unpackstr(&rb, &past_str) == PACK_FORMAT &&
                         past_str == nullptr && unpackbuf(&rb, &past_buf, 3) == PACK_FORMAT && past_buf == nullptr;
   clear_packbuffer(&rb);
   CHECK(60, "the primitives pack and unpack their values and give PACK_FORMAT past the end", primitives && past_end);

   // a wrong padding or version, no buffer at all
   std::vector<char> header(bytes.begin(), bytes.begin() + header_size());
   std::vector<char> wrong_version = header;
   wrong_version[INTSIZE + 3] ^= 0x01;
   std::vector<char> wrong_pad = header;
   wrong_pad[3] = 0x01;
   sge_pack_buffer vb;
   const bool version = read_buffer(wrong_version, wrong_version.size(), &vb) == PACK_VERSION;
   clear_packbuffer(&vb);
   const bool pad = read_buffer(wrong_pad, wrong_pad.size(), &vb) == PACK_VERSION;
   clear_packbuffer(&vb);
   const bool no_buffer = init_packbuffer(nullptr, 0, false, false) == PACK_FORMAT;
   bool strings = std::string(cull_pack_strerror(12345)).empty();
   for (const int code : {PACK_SUCCESS, PACK_ENOMEM, PACK_FORMAT, PACK_BADARG, PACK_VERSION, PACK_AUTHINFO}) {
      strings = strings && !std::string(cull_pack_strerror(code)).empty();
   }
   CHECK(61, "a wrong padding or version gives PACK_VERSION, no buffer PACK_FORMAT, every code has a message",
         version && pad && no_buffer && strings);

   // objects: every cut after the header fails with PACK_FORMAT and leaves no result
   lList *sets = lCreateList("sets", US_Type);
   for (const char *name : {"one", "two", "three"}) {
      lListElem *set = lAddElemStr(&sets, US_name, name, US_Type);
      lAddSubStr(set, UE_name, "alice", US_entries, UE_Type);
      lAddSubStr(set, UE_name, "bob", US_entries, UE_Type);
   }
   init_packbuffer(&pb, 0, false, false);
   cull_pack_list(&pb, sets);
   const std::vector<char> list_bytes = packed_bytes(pb);
   clear_packbuffer(&pb);
   init_packbuffer(&pb, 0, false, false);
   cull_pack_elem(&pb, lFirst(sets));
   const std::vector<char> elem_bytes = packed_bytes(pb);
   clear_packbuffer(&pb);
   lEnumeration *what = lWhat("%T(%I%I->%T(%I))", US_Type, US_name, US_entries, UE_Type, UE_name);
   init_packbuffer(&pb, 0, false, false);
   cull_pack_enum(&pb, what);
   const std::vector<char> what_bytes = packed_bytes(pb);
   clear_packbuffer(&pb);
   lCondition *where = lWhere("%T(%I p= %s || %I -> %T(%I == %s))", US_Type, US_name, "t*", US_entries, UE_Type,
                              UE_name, "bob");
   init_packbuffer(&pb, 0, false, false);
   cull_pack_cond(&pb, where);
   const std::vector<char> where_bytes = packed_bytes(pb);
   clear_packbuffer(&pb);
   lFreeWhere(&where);
   lFreeWhat(&what);

   const bool list_cuts = every_cut_fails(list_bytes, [](sge_pack_buffer *b, bool *null_result) {
      lList *lp = reinterpret_cast<lList *>(1);
      const int ret = cull_unpack_list(b, &lp);
      *null_result = lp == nullptr;
      lFreeList(&lp);
      return ret;
   });
   const bool elem_cuts = every_cut_fails(elem_bytes, [](sge_pack_buffer *b, bool *null_result) {
      lListElem *ep = reinterpret_cast<lListElem *>(1);
      const int ret = cull_unpack_elem(b, &ep, US_Type);
      *null_result = ep == nullptr;
      lFreeElem(&ep);
      return ret;
   });
   const bool what_cuts = every_cut_fails(what_bytes, [](sge_pack_buffer *b, bool *null_result) {
      lEnumeration *ep = reinterpret_cast<lEnumeration *>(1);
      const int ret = cull_unpack_enum(b, &ep);
      *null_result = ep == nullptr;
      lFreeWhat(&ep);
      return ret;
   });
   const bool where_cuts = every_cut_fails(where_bytes, [](sge_pack_buffer *b, bool *null_result) {
      lCondition *cp = reinterpret_cast<lCondition *>(1);
      const int ret = cull_unpack_cond(b, &cp);
      *null_result = cp == nullptr;
      lFreeWhere(&cp);
      return ret;
   });
   CHECK(62, "unpacking a list, an element, a selection and a condition cut at any byte gives PACK_FORMAT and "
             "no result",
         list_cuts && elem_cuts && what_cuts && where_cuts);

   // the counting mode measures what packing writes
   sge_pack_buffer cb;
   const bool counted = init_packbuffer(&cb, 0, true, false) == PACK_SUCCESS && cull_pack_list(&cb, sets) == PACK_SUCCESS &&
                        cb.bytes_used == list_bytes.size() - header_size() && cb.head_ptr == nullptr;
   clear_packbuffer(&cb);
   lFreeList(&sets);
   CHECK(63, "the counting mode measures exactly the bytes packing writes, without writing", counted);

   // byte arrays in a string field
   lListElem *holder = lCreateElem(TEST_Type);
   const char raw[] = {0x00, 0x1f, static_cast<char>(0xa5), 0x7f, static_cast<char>(0xff)};
   setByteArray(raw, sizeof(raw), holder, TEST_string);
   char *back = nullptr;
   const int size = getByteArray(&back, holder, TEST_string);
   const bool there_and_back = size == static_cast<int>(sizeof(raw)) && back != nullptr &&
                               memcmp(back, raw, sizeof(raw)) == 0 &&
                               strcmp(lGetString(holder, TEST_string), "00F15AF7FF") == 0;
   sge_free(&back);
   lSetString(holder, TEST_string, nullptr);
   const bool unset = getByteArray(&back, holder, TEST_string) == 0 && back == nullptr;
   lSetString(holder, TEST_string, "0G12");
   const int bad = getByteArray(&back, holder, TEST_string);
   sge_free(&back);
   setByteArray(nullptr, 3, holder, TEST_string);
   const bool null_kept = strcmp(lGetString(holder, TEST_string), "0G12") == 0;
   lFreeElem(&holder);
   CHECK(64, "setByteArray and getByteArray carry bytes through a string field and report a bad digit by a "
             "negative size",
         there_and_back && unset && bad < 0 && null_kept);
}

/// the hash of the dump text, recorded on the code before the move
static constexpr std::uint64_t DumpTextHash = 0x77f15bc08feb7b97ULL;
/// the hash of the error numbers of every cut of the dump text, recorded on the code before the move
static constexpr std::uint64_t DumpCutHash = 0xc5f21f6804def9fbULL;

/**
 * @brief FNV-1a over bytes
 * @param bytes the bytes
 * @param n how many
 * @param h the hash so far
 * @return the hash
 */
static std::uint64_t
fnv1a(const char *bytes, const std::size_t n, std::uint64_t h = 0xcbf29ce484222325ULL) {
   for (std::size_t i = 0; i < n; i++) {
      h ^= static_cast<unsigned char>(bytes[i]);
      h *= 0x100000001b3ULL;
   }
   return h;
}

/**
 * @brief What a dump function writes, as text
 * @param dump writes to the stream
 * @return the text
 */
static std::string
dump_text(const std::function<void(FILE *)> &dump) {
   char *buf = nullptr;
   size_t size = 0;
   FILE *fp = open_memstream(&buf, &size);
   dump(fp);
   fclose(fp);
   std::string text(buf, size);
   free(buf);
   return text;
}

/**
 * @brief Read from a text as from a file
 * @param text the text
 * @param read reads from the stream
 */
static void
read_text(const std::string &text, const std::function<void(FILE *)> &read) {
   FILE *fp = fmemopen(const_cast<char *>(text.data()), text.size(), "r");
   read(fp);
   fclose(fp);
}

/**
 * @brief The elements the dump pins use: every value type, a quote in a string, a sub-object, a sub-list
 * @param[out] list a list of user sets with entries
 * @return a TEST element
 */
static lListElem *
dump_fixture(lList **list) {
   lListElem *ep = lCreateElem(TEST_Type);
   lSetBool(ep, TEST_bool, true);
   lSetUlong(ep, TEST_uint32, 42);
   lSetUlong64(ep, TEST_uint64, 1ULL << 40);
   lSetInt(ep, TEST_int, -7);
   lSetLong(ep, TEST_long, -123456789L);
   lSetDouble(ep, TEST_double, 2.5);
   lSetHost(ep, TEST_host, "host1");
   lSetString(ep, TEST_string, "say \"hi\" now");
   lListElem *job = lCreateElem(JB_Type);
   lSetUlong(job, JB_job_number, 7);
   lSetString(job, JB_owner, "alice");
   lSetObject(ep, TEST_obj_jb, job);
   *list = lCreateList("sets", US_Type);
   for (const char *name : {"one", "two"}) {
      lListElem *set = lAddElemStr(list, US_name, name, US_Type);
      lAddSubStr(set, UE_name, "bob", US_entries, UE_Type);
   }
   return ep;
}

/**
 * @brief Pin dump, scan and the files of the old interface (CS-3077, dump group)
 *
 * The text is pinned by its hash, recorded on the code before the move; so are
 * the error numbers of reading every cut of it.
 */
static void
test_dump() {
   lList *sets = nullptr;
   lListElem *ep = dump_fixture(&sets);

   // the text of an element, a list and a descriptor
   const std::string elem_text = dump_text([&](FILE *fp) { lDumpElemFp(fp, ep, 1); });
   const std::string list_text = dump_text([&](FILE *fp) { lDumpList(fp, sets, 0); });
   const std::string object_text = dump_text([&](FILE *fp) { lDumpObject(fp, lGetObject(ep, TEST_obj_jb), 0); });
   const std::string descr_text = dump_text([&](FILE *fp) { lDumpDescr(fp, US_Type, 2); });
   std::uint64_t hash = fnv1a(elem_text.data(), elem_text.size());
   hash = fnv1a(list_text.data(), list_text.size(), hash);
   hash = fnv1a(object_text.data(), object_text.size(), hash);
   hash = fnv1a(descr_text.data(), descr_text.size(), hash);
   if (DumpTextHash == 0) {
      printf("dump text hash: 0x%016llxULL\n", static_cast<unsigned long long>(hash));
   }

   // there and back
   std::string elem_again;
   std::string list_again;
   read_text(elem_text, [&](FILE *fp) {
      lListElem *back = lUndumpElemFp(fp, TEST_Type);
      elem_again = dump_text([&](FILE *out) { lDumpElemFp(out, back, 1); });
      lFreeElem(&back);
   });
   read_text(list_text, [&](FILE *fp) {
      lList *back = lUndumpList(fp, nullptr, nullptr);
      list_again = dump_text([&](FILE *out) { lDumpList(out, back, 0); });
      lFreeList(&back);
   });
   std::string object_again;
   read_text(object_text, [&](FILE *fp) {
      lListElem *back = lUndumpObject(fp);
      object_again = dump_text([&](FILE *out) { lDumpObject(out, back, 0); });
      lFreeElem(&back);
   });
   CHECK(65, "dumping writes the text of before, and reading it back gives the same text",
         (DumpTextHash == 0 || hash == DumpTextHash) && elem_again == elem_text && list_again == list_text &&
         object_again == object_text);

   // every cut of the text: nullptr and an error number, the same as before
   std::uint64_t cuts = 0xcbf29ce484222325ULL;
   bool all_null = true;
   for (const std::string *text : {&elem_text, &list_text, &object_text}) {
      for (std::size_t n = 0; n < text->size(); n++) {
         if (n > 0 && (*text)[n - 1] != '\n') {
            continue;   // cut after whole lines only
         }
         const std::string cut = text->substr(0, n);
         int lerrno = 0;
         bool null_result = false;
         read_text(cut.empty() ? std::string(" ") : cut, [&](FILE *fp) {
            cull_state_set_lerrno(0);
            if (text == &elem_text) {
               lListElem *back = lUndumpElemFp(fp, TEST_Type);
               null_result = back == nullptr;
               lFreeElem(&back);
            } else if (text == &list_text) {
               lList *back = lUndumpList(fp, nullptr, nullptr);
               null_result = back == nullptr;
               lFreeList(&back);
            } else {
               lListElem *back = lUndumpObject(fp);
               null_result = back == nullptr;
               lFreeElem(&back);
            }
            lerrno = cull_state_get_lerrno();
         });
         all_null = all_null && null_result;
         cuts = fnv1a(reinterpret_cast<const char *>(&lerrno), sizeof(lerrno), cuts);
      }
   }
   if (DumpCutHash == 0) {
      printf("dump cut hash: 0x%016llxULL\n", static_cast<unsigned long long>(cuts));
   }
   cull_state_set_lerrno(0);
   const bool null_file = lDumpElemFp(nullptr, ep, 0) == -1 && cull_state_get_lerrno() == LEFILENULL &&
                          lUndumpElemFp(nullptr, TEST_Type) == nullptr && lUndumpList(nullptr, nullptr, nullptr) == nullptr;
   cull_state_set_lerrno(0);
   const bool no_file = lUndumpElem("/nonexistent/test_tom_legacy", TEST_Type) == nullptr &&
                        cull_state_get_lerrno() == LEOPEN;
   CHECK(66, "reading a cut text gives nullptr with the error numbers of before; a null file and a missing file "
             "fail as before",
         all_null && (DumpCutHash == 0 || cuts == DumpCutHash) && null_file && no_file);

   // files of packed elements; a TEST element holds int and long fields, which are not packed, so a job is used
   const lListElem *job = lGetObject(ep, TEST_obj_jb);
   char dir[] = "/tmp/test_tom_legacy_XXXXXX";
   bool files = mkdtemp(dir) != nullptr;
   files = files && lWriteElemToDisk(job, dir, "elem", "test") == 0;
   lListElem *read = files ? lReadElemFromDisk(dir, "elem", JB_Type, "test") : nullptr;
   files = files && read != nullptr && lGetUlong(read, JB_job_number) == 7 &&
           strcmp(lGetString(read, JB_owner), "alice") == 0;
   lFreeElem(&read);
   const bool not_packed = lWriteElemToDisk(ep, dir, "test", "test") == 1;
   const bool no_name = lWriteElemToDisk(job, nullptr, nullptr, "test") == 1;
   const bool missing = lReadElemFromDisk(dir, "missing", JB_Type, "test") == nullptr;
   const std::string path = std::string(dir) + "/elem";
   bool cut_file = truncate(path.c_str(), 20) == 0;
   cut_file = cut_file && lReadElemFromDisk(dir, "elem", JB_Type, "test") == nullptr;
   unlink(path.c_str());
   unlink((std::string(dir) + "/test").c_str());
   rmdir(dir);
   CHECK(67, "lWriteElemToDisk and lReadElemFromDisk carry an element through a file and fail on fields that are "
             "not packed, no name, a missing file and a cut file",
         files && not_packed && no_name && missing && cut_file);

   lFreeList(&sets);
   lFreeElem(&ep);
}

/// the hash of the text the print functions write, recorded on the code before the move; 0 while recording
static constexpr std::uint64_t RestTextHash = 0x6ba2bcf5a05b2a91ULL;

/**
 * @brief The names of the elements of a string list, in list order
 * @param lp the list, or nullptr
 * @param nm the string field
 * @return the names, joined by blanks
 */
static std::string
names_of(const lList *lp, const int nm) {
   std::string names;
   for_each_ep_lv(ep, lp) {
      if (!names.empty()) {
         names += ' ';
      }
      const char *name = mt_get_type(ep->descr[lGetPosViaElem(ep, nm, SGE_NO_ABORT)].mt) == lHostT ? lGetHost(ep, nm)
                                                                                                   : lGetString(ep, nm);
      names += name != nullptr ? name : "(null)";
   }
   return names;
}

/**
 * @brief Pin the rest of the old interface: small queries, helpers and debug output (CS-3077, rest group)
 */
static void
test_rest() {
   // small queries
   lList *sets = lCreateList("sets", US_Type);
   for (const char *name : {"one", "two", "three"}) {
      lAddElemStr(&sets, US_name, name, US_Type);
   }
   lListElem *free_elem = lCreateElem(US_Type);
   cull_state_set_lerrno(0);
   bool queries = lGetNumberOfElem(nullptr) == 0 && cull_state_get_lerrno() == LELISTNULL &&
                  lGetNumberOfElem(sets) == 3;
   cull_state_set_lerrno(0);
   queries = queries && lGetElemDescr(nullptr) == nullptr && cull_state_get_lerrno() == LEELEMNULL &&
             lGetElemDescr(lFirst(sets)) == lFirst(sets)->descr;
   cull_state_set_lerrno(0);
   queries = queries && !lIsFreeElem(nullptr) && cull_state_get_lerrno() == LEELEMNULL && lIsFreeElem(free_elem) &&
             !lIsFreeElem(lFirst(sets));
   queries = queries && lGetNumberOfRemainingElem(lFirst(sets)) == 2 && lGetNumberOfRemainingElem(nullptr) == 0 &&
             lGetNumberOfRemainingElem(lLast(sets)) == 0;
   queries = queries && lGetType(TEST_Type, TEST_string) == lStringT && lGetType(TEST_Type, JB_job_number) == lEndT;
   cull_state_set_lerrno(0);
   queries = queries && lGetPosType(nullptr, 0) == NoName && cull_state_get_lerrno() == LEDESCRNULL &&
             lGetPosType(TEST_Type, -1) == NoName && lGetPosType(TEST_Type, lGetPosInDescr(TEST_Type, TEST_host)) == lHostT;
   lListElem *with_list = lFirstRW(sets);
   lAddSubStr(with_list, UE_name, "alice", US_entries, UE_Type);
   lList **ref = lGetListRef(with_list, US_entries);
   queries = queries && ref != nullptr && *ref == lGetList(with_list, US_entries) &&
             strcmp(lMt2Str(-1), "unknown multitype") == 0 && strcmp(lMt2Str(lUlong64T), "lUlong64T") == 0;
   const bool list_ref_aborts = aborts([] {
      lListElem *ep = lCreateElem(US_Type);
      static_cast<void>(lGetListRef(ep, US_name));
   });
   lFreeElem(&free_elem);
   CHECK(68, "the small queries give their results and error numbers as before", queries && list_ref_aborts);

   // strings to lists
   lList *lp = nullptr;
   bool to_list = lString2List("a b,a c", &lp, ST_Type, ST_name, ", ") == 0 && names_of(lp, ST_name) == "a b c";
   lFreeList(&lp);
   to_list = to_list && lString2List(nullptr, &lp, ST_Type, ST_name, " ") == 1 && lp == nullptr;
   to_list = to_list && lString2List("h1 h2 h1", &lp, TEST_Type, TEST_host, " ") == 0 &&
             names_of(lp, TEST_host) == "h1 h2";
   lFreeList(&lp);
   to_list = to_list && lString2ListNone("NONE", &lp, ST_Type, ST_name, " ") == 0 && lp == nullptr;
   to_list = to_list && lString2ListNone("a none", &lp, ST_Type, ST_name, " ") == 1 && lp == nullptr;
   to_list = to_list && lString2ListNone("a b", &lp, ST_Type, ST_name, " ") == 0 && names_of(lp, ST_name) == "a b";
   lFreeList(&lp);

   // differences, splits, unique hosts
   lList *a = nullptr;
   lList *b = nullptr;
   lString2List("a b c", &a, ST_Type, ST_name, " ");
   lString2List("b c d", &b, ST_Type, ST_name, " ");
   bool diff = lDiffListStr(ST_name, &a, &b) == 0 && names_of(a, ST_name) == "a" && names_of(b, ST_name) == "d" &&
               lDiffListStr(ST_name, nullptr, &b) == -1;
   lList *none = nullptr;
   diff = diff && lDiffListStr(ST_name, &a, &none) == 0 && names_of(a, ST_name) == "a";
   lFreeList(&a);
   lFreeList(&b);
   lList *numbers = lCreateList("numbers", TEST_Type);
   for (lUlong k = 1; k <= 10; k++) {
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_uint32, k);
      lAppendElem(numbers, ep);
   }
   lCondition *big = lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 5U);
   lList *small = nullptr;
   bool split = lSplit(&numbers, &small, "small", big) == 0 && lGetNumberOfElem(numbers) == 5 &&
                lGetNumberOfElem(small) == 5 && strcmp(lGetListName(small), "small") == 0 &&
                lGetUlong(lFirst(small), TEST_uint32) == 1;
   lCondition *none_big = lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 100U);
   split = split && lSplit(&numbers, nullptr, nullptr, none_big) == 0 && numbers == nullptr &&
           lSplit(nullptr, nullptr, nullptr, big) == -1;
   lFreeWhere(&none_big);
   lFreeWhere(&big);
   lFreeList(&small);
   lList *hosts = nullptr;
   for (const char *h : {"h2", "h1", "h2", "h1", "h3"}) {
      lListElem *ep = lCreateElem(TEST_Type);
      lSetHost(ep, TEST_host, h);
      if (hosts == nullptr) {
         hosts = lCreateList("hosts", TEST_Type);
      }
      lAppendElem(hosts, ep);
   }
   const bool uniq = lUniqHost(hosts, TEST_host) == 0 && names_of(hosts, TEST_host) == "h1 h2 h3";
   lFreeList(&hosts);
   CHECK(69, "lString2List, lString2ListNone, lDiffListStr, lSplit and lUniqHost give their results as before",
         to_list && diff && split && uniq);

   // trees, reduced descriptors, selections
   lList *tree = lCreateList("tree", STN_Type);
   lListElem *root = lAddElemStr(&tree, STN_name, "root", STN_Type);
   lListElem *left = lAddSubStr(root, STN_name, "left", STN_children, STN_Type);
   lAddSubStr(root, STN_name, "right", STN_children, STN_Type);
   lAddSubStr(left, STN_name, "leaf", STN_children, STN_Type);
   const bool counts = lGetNumberOfNodes(nullptr, tree, STN_children) == 4 &&
                       lGetNumberOfLeafs(nullptr, tree, STN_children) == 2 &&
                       lGetNumberOfNodes(root, nullptr, STN_children) == 4 && lGetNumberOfLeafs(root, nullptr, STN_children) == 2;
   lFreeList(&tree);
   lEnumeration *what_name = lWhat("%T(%I)", US_Type, US_name);
   const lDescr *reduced = lGetReducedDescr(US_Type, what_name);
   const bool reduction = reduced != nullptr && lGetPosInDescr(reduced, US_name) >= 0 &&
                          lGetPosInDescr(reduced, US_type) < 0;
   ocs_cull_descr_release(&reduced);
   lEnumeration *none_what = lWhat("%T(NONE)", US_Type);
   const bool no_reduction = lGetReducedDescr(US_Type, none_what) == nullptr;
   lFreeWhat(&none_what);
   lEnumeration *merged = lWhat("%T(%I%I->%T(%I))", JB_Type, JB_job_number, JB_ja_tasks, JAT_Type, JAT_task_number);
   lEnumeration *other = lWhat("%T(%I%I->%T(%I))", JB_Type, JB_owner, JB_ja_tasks, JAT_Type, JAT_status);
   bool merge = lMergeWhat(&merged, &other) == 0 && other == nullptr && merged != nullptr;
   // the fields of both, in type order; for a field in both, the sub-selections of both (before CS-3077 the
   // sub-selection of the second was lost, JAT_status here)
   int merged_fields = 0;
   const lEnumeration *tasks_sub = nullptr;
   for (int i = 0; merged != nullptr && merged[i].nm != NoName; i++) {
      merged_fields++;
      if (merged[i].nm == JB_ja_tasks) {
         tasks_sub = merged[i].ep;
      }
   }
   bool sub_task_number = false;
   bool sub_status = false;
   for (int i = 0; tasks_sub != nullptr && tasks_sub[i].nm != NoName; i++) {
      sub_task_number = sub_task_number || tasks_sub[i].nm == JAT_task_number;
      sub_status = sub_status || tasks_sub[i].nm == JAT_status;
   }
   printf("merged: %d fields, sub-selection of JB_ja_tasks with JAT_task_number %d, JAT_status %d\n", merged_fields,
          sub_task_number, sub_status);
   merge = merge && merged_fields == 3 && sub_task_number && sub_status;
   lEnumeration *sub = lWhat("%T(%I)", UE_Type, UE_name);
   const bool set_sub = lWhatSetSubWhat(what_name, US_name, &sub) == 0 && sub == nullptr;
   lEnumeration *sub2 = lWhat("%T(%I)", UE_Type, UE_name);
   const bool set_missing = lWhatSetSubWhat(what_name, US_type, &sub2) == -1 && sub2 == nullptr;

   // the text of the print functions
   std::string text = dump_text([&](FILE *fp) { lWriteListTo(sets, fp); });
   text += dump_text([&](FILE *fp) { lWriteElemTo(lFirst(sets), fp); });
   dstring buffer = DSTRING_INIT;
   lWriteElemToStr(lFirst(sets), &buffer);
   text += sge_dstring_get_string(&buffer) != nullptr ? sge_dstring_get_string(&buffer) : "";
   sge_dstring_free(&buffer);
   text += dump_text([&](FILE *fp) { lWriteWhatTo(what_name, fp); });
   lCondition *where = lWhere("%T(%I == %s && (%I > %u || !(%I p= %s)))", US_Type, US_name, "one", US_type, 3U,
                              US_name, "t*");
   text += dump_text([&](FILE *fp) { lWriteWhereTo(where, fp); });
   for (int mt = -1; mt <= lUlong64T + 1; mt++) {
      text += lMt2Str(mt);
   }
   const std::uint64_t hash = fnv1a(text.data(), text.size());
   if (RestTextHash == 0) {
      printf("rest text hash: 0x%016llxULL\n", static_cast<unsigned long long>(hash));
   }
   lFreeWhere(&where);
   lFreeWhat(&merged);
   lFreeWhat(&what_name);
   lFreeList(&sets);
   CHECK(70, "the tree counts, lGetReducedDescr, lMergeWhat and lWhatSetSubWhat, and the text of the print "
             "functions are those of before",
         counts && reduction && no_reduction && merge && set_sub && set_missing &&
         (RestTextHash == 0 || hash == RestTextHash));
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
       "field type aborts reading the first element; reading or adding to a missing sub-list aborts",
       {{"LEG-01", "covers"}}, {21, 22, 23, 24, 25, 26, 27}, test_lookups},
      {"LEGACY-06", "positions",
       "the position lookups of the descriptor report misses as before",
       {"ask lGetPosViaElem with and without abort, and lGetPosInDescr with a null descriptor and a miss"},
       "-1 without an error number, an abort where asked for, LEDESCRNULL for a null descriptor",
       {{"LEG-01", "covers"}}, {28, 29, 30}, test_positions},
      {"LEGACY-07", "list structure and ownership",
       "creating, freeing, copying, appending, inserting, removing and dechaining report their errors as before",
       {"call each with null lists, null elements, a missing descriptor, a bound element, an element of another "
        "list and lists of different types"},
       "nullptr or -1 with the error numbers of before, aborts where they aborted", {{"LEG-01", "covers"}},
       {31, 32, 33, 34, 35, 36}, test_list_structure},
      {"LEGACY-08", "case-insensitive, prefix and sub-list lookups",
       "the lookups whose implementation moves into the core report their errors as before",
       {"look up with a null string, a null list, a foreign field, a field of another type and an absent field",
        "look up in the sub-list of a null element, of an element with an empty sub-list and without the field"},
       "nullptr without an abort, except the abort of every sub-list lookup on a missing sub-list field", {{"LEG-01", "covers"}},
       {37, 38}, test_more_lookups},
      {"LEGACY-09", "add and delete by key",
       "adding and deleting by key report their errors as before",
       {"add with a null list, null arguments, a foreign field and fields of another type",
        "delete found and missing keys, from a null list and with null arguments",
        "add and delete in a list field of a null element and of an element without the field"},
       "the results of before, made alike for every key type and for adding and deleting (CS-3077)",
       {{"LEG-01", "covers"}}, {39, 40, 41},
       test_add_and_delete_by_key},
      {"LEGACY-10", "increments",
       "the increments keep their sums, the hash table and the aborts of before",
       {"add to a hashed unique key and look it up by the old and the new value, add a duplicate",
        "add to a 32 bit field past its maximum, add 0, add to a 64 bit and a double field"},
       "the lookup follows the new value, a duplicate aborts in a debug build, the 32 bit sum wraps around",
       {{"LEG-01", "covers"}}, {42, 43}, test_increments},
      {"LEGACY-11", "selections",
       "lWhat, lWhere, lSelect, lSelectDestroy, the lFind functions and lCompare keep their results",
       {"build selections nested, repeated, ALL, NONE and flat, with a foreign field and empty",
        "select from four elements with every comparison of every value type, the named comparisons, "
        "&&, || and !, a sub-list, conditions combined by hand and copied",
        "build conditions with a foreign field, -> on a field that is not a list and a value of another type",
        "select with a reduction, without a condition, without a selection and with an empty result",
        "find forward and backward, compare, keep the matching elements, transfer a condition"},
       "the results of before, including the quirks: a null string matches neither == nor !=, m= on a 64 bit "
       "field selects nothing, %f is no token, lSelect without a selection ignores the condition",
       {{"LEG-01", "covers"}}, {44, 45, 46, 47, 48, 49, 50, 51}, test_selections},
      {"LEGACY-12", "copies, iterating lookups and descriptor helpers",
       "the functions the core used from the old interface keep their results",
       {"copy elements with a sub-list, an empty and a missing one, a sub-object and a field not to be transferred",
        "select with a nested selection over an empty sub-list",
        "walk the matches of a 64 bit, a string, a host and a unique key, below and above the hash threshold",
        "compare, count and search descriptors, name a list without a name"},
       "the results of before, including an emptied sub-list of a selection as no list",
       {{"LEG-01", "covers"}}, {52, 53, 54, 55}, test_core_helpers},
      {"LEGACY-13", "sorting",
       "the sort functions keep the order of the keys and their errors",
       {"sort by one key of every value type in both directions, by two keys, an empty, a one-element and no list",
        "sort a hashed list and look its elements up", "insert sorted, resort a changed element, compare",
        "build orders with an unknown field, without a direction and by hand"},
       "the keys in order, the lookups working, the error results of before; equal keys in any order",
       {{"LEG-01", "covers"}}, {56, 57, 58, 59}, test_sorting},
      {"LEGACY-14", "pack and unpack",
       "the pack buffer and the packing of objects keep their codes and leave nothing behind on an error",
       {"pack and unpack every primitive and read past the end", "read a wrong padding and version",
        "unpack a list, an element, a selection and a condition from every cut of their bytes",
        "count a pack", "carry bytes through a string field"},
       "the codes of before, null results and no leak on every cut; the bytes are pinned by test_cull_pack_format",
       {{"LEG-01", "covers"}}, {60, 61, 62, 63, 64}, test_pack},
      {"LEGACY-15", "dump, scan and files",
       "dumping writes the text of before, scanning reads it back, the files carry an element",
       {"dump an element with every value type, a sub-object and a sub-list, a list, an object and a descriptor",
        "read each back and dump it again", "read every cut of the text", "write an element to a file, read it"},
       "the text and the error numbers of before, by hash; the same text after a round trip",
       {{"LEG-01", "covers"}}, {65, 66, 67}, test_dump},
      {"LEGACY-16", "the rest",
       "the small queries, the helpers and the debug output keep their results",
       {"ask the small queries with and without arguments", "turn strings into lists, diff, split, keep hosts once",
        "count a tree, reduce a descriptor, merge and nest selections", "print a list, an element, a selection and a condition"},
       "the results and error numbers of before, the text by hash", {{"LEG-01", "covers"}}, {68, 69, 70}, test_rest},
   });
}
