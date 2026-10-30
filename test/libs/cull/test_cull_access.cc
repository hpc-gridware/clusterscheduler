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
 * @brief Unit tests for field access by position in `libs/cull`
 *
 * Covers the descriptor queries (lGetPosType, lGetType, lGetPosName,
 * lGetPosViaElem), the positional getters and setters for every data type,
 * the arithmetic and bit-mask helpers, and the functions that move a string,
 * a list or an object out of an element (lXchgString, lXchgList, lSwapList,
 * lGetListRef, lDechainObject). Where a setter maintains a hash table, the
 * tests check that the element is still found by its new value.
 *
 * Accessing a field as the wrong type is a programming error that aborts the
 * process. The last group checks that it does, each case in a child process.
 */

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_list.h"
#include "cull/cull_multitype.h"

// ---------------------------------------------------------------------------
// Local CULL type used by all tests
// ---------------------------------------------------------------------------

/** @brief A synthetic CULL object type, defined only for these tests
 *
 * One attribute per CULL data type, plus hashed ulong, ulong64 and string
 * fields so the setters' hash maintenance can be observed.
 */
enum {
   TEST_int = 1,   ///< an int attribute
   TEST_host,   ///< a host name attribute
   TEST_string,   ///< a string attribute
   TEST_double,   ///< a double attribute
   TEST_long,   ///< a long attribute
   TEST_ulong,   ///< an unsigned long attribute
   TEST_bool,   ///< a bool attribute
   TEST_list,   ///< a sublist attribute
   TEST_object,   ///< a sub-object attribute
   TEST_ref,   ///< a reference attribute
   TEST_ulong64,   ///< a 64 bit unsigned attribute
   TEST_list2,   ///< a second sublist attribute, the partner for lSwapList
   TEST_ulong_ukey,   ///< an unsigned long attribute that is a unique hashed key
   TEST_ulong64_ukey,   ///< a 64 bit unsigned attribute that is a unique hashed key
   TEST_string_ukey   ///< a string attribute that is a unique hashed key
};

LISTDEF(TEST_Type)
   SGE_INT    (TEST_int,    CULL_DEFAULT)
   SGE_HOST   (TEST_host,   CULL_DEFAULT)
   SGE_STRING (TEST_string, CULL_DEFAULT)
   SGE_DOUBLE (TEST_double, CULL_DEFAULT)
   SGE_LONG   (TEST_long,   CULL_DEFAULT)
   SGE_ULONG  (TEST_ulong,  CULL_DEFAULT)
   SGE_BOOL   (TEST_bool,   CULL_DEFAULT)
   SGE_LIST   (TEST_list,   TEST_Type, CULL_DEFAULT)
   SGE_OBJECT (TEST_object, TEST_Type, CULL_DEFAULT)
   SGE_REF    (TEST_ref,    TEST_Type, CULL_DEFAULT)
   SGE_ULONG64(TEST_ulong64, CULL_DEFAULT)
   SGE_LIST   (TEST_list2,  TEST_Type, CULL_DEFAULT)
   SGE_ULONG  (TEST_ulong_ukey, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG64(TEST_ulong64_ukey, CULL_UNIQUE | CULL_HASH)
   SGE_STRING (TEST_string_ukey, CULL_UNIQUE | CULL_HASH)
LISTEND

NAMEDEF(TEST_Name)
   NAME("TEST_int")
   NAME("TEST_host")
   NAME("TEST_string")
   NAME("TEST_double")
   NAME("TEST_long")
   NAME("TEST_ulong")
   NAME("TEST_bool")
   NAME("TEST_list")
   NAME("TEST_object")
   NAME("TEST_ref")
   NAME("TEST_ulong64")
   NAME("TEST_list2")
   NAME("TEST_ulong_ukey")
   NAME("TEST_ulong64_ukey")
   NAME("TEST_string_ukey")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the synthetic type

/** @brief The name space registering the synthetic types with CULL */
lNameSpace nmv[] = {
   {1, TEST_Size, TEST_Name, TEST_Type},
   {0, 0, nullptr, nullptr}
};

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
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

/** @brief Run a callable in a child process and report whether it aborted
 *
 * The child closes stderr and disables core files, so the stack trace and the
 * core that a CULL type error produces do not end up in the test output or the
 * working directory.
 *
 * @param what the code that is expected to abort
 * @return true when the child was terminated by SIGABRT
 */
static bool aborts(const std::function<void()> &what) {
   fflush(stdout);
   fflush(stderr);
   pid_t pid = fork();
   if (pid < 0) {
      return false;
   }
   if (pid == 0) {
      struct rlimit no_core = {0, 0};
      setrlimit(RLIMIT_CORE, &no_core);
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) {
         dup2(devnull, STDERR_FILENO);
         dup2(devnull, STDOUT_FILENO);
         close(devnull);
      }
      what();
      _exit(0);
   }
   int status = 0;
   if (waitpid(pid, &status, 0) != pid) {
      return false;
   }
   return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

/** @brief Position of a field in the synthetic type's descriptor
 *
 * @param nm the field name id
 * @return the field's index in TEST_Type
 */
static int pos_of(int nm) {
   return lGetPosInDescr(TEST_Type, nm);
}

// ---------------------------------------------------------------------------
// Descriptor queries  [T01–T08]
//
// lGetPosType/lGetType map a position or a name to the CULL data type,
// lGetPosName maps a position back to the name, lGetPosViaElem looks a name up
// through an element. Unknown names yield lEndT, NoName or -1.
// ---------------------------------------------------------------------------

static void test_descriptor_queries() {
   printf("\n--- descriptor queries ---\n");

   lListElem *ep = lCreateElem(TEST_Type);

   CHECK(1, "lGetPosType of TEST_double is lDoubleT", lGetPosType(TEST_Type, pos_of(TEST_double)) == lDoubleT);
   CHECK(2, "lGetType of TEST_list is lListT", lGetType(TEST_Type, TEST_list) == lListT);
   CHECK(3, "lGetType of TEST_ulong64_ukey is lUlong64T", lGetType(TEST_Type, TEST_ulong64_ukey) == lUlong64T);
   CHECK(4, "lGetType of an unknown name is lEndT", lGetType(TEST_Type, 4711) == lEndT);
   CHECK(5, "lGetPosName maps a position back to its name", lGetPosName(TEST_Type, pos_of(TEST_ref)) == TEST_ref);
   CHECK(6, "lGetPosName of a negative position is NoName", lGetPosName(TEST_Type, -1) == NoName);
   CHECK(7, "lGetPosViaElem matches lGetPosInDescr",
         lGetPosViaElem(ep, TEST_host, SGE_NO_ABORT) == pos_of(TEST_host));
   CHECK(8, "lGetPosViaElem of an unknown name is negative", lGetPosViaElem(ep, 4711, SGE_NO_ABORT) < 0);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// Positional getters and setters  [T09–T22]
//
// Every lSetPosXxx must be readable through lGetPosXxx and through the
// name-based lGetXxx, since both address the same slot.
// ---------------------------------------------------------------------------

static void test_positional_access() {
   printf("\n--- positional getters and setters ---\n");

   lListElem *ep = lCreateElem(TEST_Type);

   lSetPosInt(ep, pos_of(TEST_int), -3);
   CHECK(9, "lSetPosInt/lGetPosInt", lGetPosInt(ep, pos_of(TEST_int)) == -3 && lGetInt(ep, TEST_int) == -3);

   lSetPosUlong(ep, pos_of(TEST_ulong), 17);
   CHECK(10, "lSetPosUlong/lGetPosUlong", lGetPosUlong(ep, pos_of(TEST_ulong)) == 17 && lGetUlong(ep, TEST_ulong) == 17);

   lSetPosUlong64(ep, pos_of(TEST_ulong64), 0x123456789ULL);
   CHECK(11, "lSetPosUlong64/lGetPosUlong64",
         lGetPosUlong64(ep, pos_of(TEST_ulong64)) == 0x123456789ULL && lGetUlong64(ep, TEST_ulong64) == 0x123456789ULL);

   lSetPosString(ep, pos_of(TEST_string), "pos");
   CHECK(12, "lSetPosString/lGetPosString",
         strcmp(lGetPosString(ep, pos_of(TEST_string)), "pos") == 0 && strcmp(lGetString(ep, TEST_string), "pos") == 0);

   lSetPosHost(ep, pos_of(TEST_host), "hpos");
   CHECK(13, "lSetPosHost/lGetPosHost",
         strcmp(lGetPosHost(ep, pos_of(TEST_host)), "hpos") == 0 && strcmp(lGetHost(ep, TEST_host), "hpos") == 0);

   lSetPosDouble(ep, pos_of(TEST_double), 2.5);
   CHECK(14, "lSetPosDouble/lGetPosDouble", lGetPosDouble(ep, pos_of(TEST_double)) == 2.5);

   lSetPosLong(ep, pos_of(TEST_long), -1234567L);
   CHECK(15, "lSetPosLong/lGetPosLong", lGetPosLong(ep, pos_of(TEST_long)) == -1234567L);

   lSetPosBool(ep, pos_of(TEST_bool), true);
   CHECK(16, "lSetPosBool/lGetPosBool", lGetPosBool(ep, pos_of(TEST_bool)) == true);

   int target = 0;
   lSetPosRef(ep, pos_of(TEST_ref), &target);
   CHECK(17, "lSetPosRef/lGetPosRef", lGetPosRef(ep, pos_of(TEST_ref)) == &target);

   lList *sub = lCreateList("sub", TEST_Type);
   lAppendElem(sub, lCreateElem(TEST_Type));
   lSetPosList(ep, pos_of(TEST_list), sub);
   CHECK(18, "lSetPosList/lGetPosList stores the list itself",
         lGetPosList(ep, pos_of(TEST_list)) == sub && lGetList(ep, TEST_list) == sub);

   lListElem *obj = lCreateElem(TEST_Type);
   lSetPosObject(ep, pos_of(TEST_object), obj);
   CHECK(19, "lSetPosObject/lGetPosObject stores the object itself",
         lGetPosObject(ep, pos_of(TEST_object)) == obj && lGetObject(ep, TEST_object) == obj);

   // T20-T21: the string reference functions address the stored pointer
   char **sref = lGetPosStringRef(ep, pos_of(TEST_string));
   char **href = lGetPosHostRef(ep, pos_of(TEST_host));
   CHECK(20, "lGetPosStringRef points at the stored string", sref != nullptr && *sref == lGetString(ep, TEST_string));
   CHECK(21, "lGetPosHostRef points at the stored host", href != nullptr && *href == lGetHost(ep, TEST_host));

   // T22: lSetPosString with nullptr clears the field
   lSetPosString(ep, pos_of(TEST_string), nullptr);
   CHECK(22, "lSetPosString(nullptr) clears the field", lGetString(ep, TEST_string) == nullptr);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// Hash maintenance of the setters  [T23–T28]
//
// When an element in a list changes a hashed key, the old key must no longer
// find it and the new key must. This is checked for the positional setters and
// for lAddUlong/lAddUlong64, which change the value by an offset.
// ---------------------------------------------------------------------------

static void test_setter_hash_upkeep() {
   printf("\n--- hash maintenance of the setters ---\n");

   lList *lp = lCreateList("keys", TEST_Type);
   lListElem *ep = lCreateElem(TEST_Type);
   lSetUlong(ep, TEST_ulong_ukey, 10);
   lSetUlong64(ep, TEST_ulong64_ukey, 100);
   lSetString(ep, TEST_string_ukey, "old");
   lAppendElem(lp, ep);

   lSetPosUlong(ep, pos_of(TEST_ulong_ukey), 11);
   CHECK(23, "lSetPosUlong on a hashed key: new key found, old key gone",
         lGetElemUlong(lp, TEST_ulong_ukey, 11) == ep && lGetElemUlong(lp, TEST_ulong_ukey, 10) == nullptr);

   lSetPosUlong64(ep, pos_of(TEST_ulong64_ukey), 101);
   CHECK(24, "lSetPosUlong64 on a hashed key: new key found, old key gone",
         lGetElemUlong64(lp, TEST_ulong64_ukey, 101) == ep && lGetElemUlong64(lp, TEST_ulong64_ukey, 100) == nullptr);

   lSetPosString(ep, pos_of(TEST_string_ukey), "new");
   CHECK(25, "lSetPosString on a hashed key: new key found, old key gone",
         lGetElemStr(lp, TEST_string_ukey, "new") == ep && lGetElemStr(lp, TEST_string_ukey, "old") == nullptr);

   lAddUlong(ep, TEST_ulong_ukey, 5);
   CHECK(26, "lAddUlong on a hashed key: value and hash follow",
         lGetUlong(ep, TEST_ulong_ukey) == 16 && lGetElemUlong(lp, TEST_ulong_ukey, 16) == ep &&
         lGetElemUlong(lp, TEST_ulong_ukey, 11) == nullptr);

   lAddUlong64(ep, TEST_ulong64_ukey, 0x100000000ULL);
   CHECK(27, "lAddUlong64 on a hashed key: value and hash follow",
         lGetUlong64(ep, TEST_ulong64_ukey) == 0x100000065ULL &&
         lGetElemUlong64(lp, TEST_ulong64_ukey, 0x100000065ULL) == ep &&
         lGetElemUlong64(lp, TEST_ulong64_ukey, 101) == nullptr);

   // T28: an offset of zero changes nothing, the key stays findable
   lAddUlong(ep, TEST_ulong_ukey, 0);
   CHECK(28, "lAddUlong with offset 0 keeps value and hash",
         lGetUlong(ep, TEST_ulong_ukey) == 16 && lGetElemUlong(lp, TEST_ulong_ukey, 16) == ep);

   lFreeList(&lp);
}

// ---------------------------------------------------------------------------
// Arithmetic, bit masks, string helpers  [T29–T37]
// ---------------------------------------------------------------------------

static void test_value_helpers() {
   printf("\n--- arithmetic, bit masks, string helpers ---\n");

   lListElem *ep = lCreateElem(TEST_Type);

   lSetDouble(ep, TEST_double, 1.25);
   lAddDouble(ep, TEST_double, 0.5);
   CHECK(29, "lAddDouble adds the offset", lGetDouble(ep, TEST_double) == 1.75);

   lSetUlong(ep, TEST_ulong, 7);
   lAddUlong(ep, TEST_ulong, 3);
   CHECK(30, "lAddUlong on an unhashed field adds the offset", lGetUlong(ep, TEST_ulong) == 10);

   // T31-T35: bit masks on 0b1010
   lSetUlong(ep, TEST_ulong, 0xA);
   CHECK(31, "lMatchUlongBitMask: any common bit matches", lMatchUlongBitMask(ep, TEST_ulong, 0x3));
   CHECK(32, "lMatchUlongBitMask: no common bit does not match", !lMatchUlongBitMask(ep, TEST_ulong, 0x5));
   lOrUlongBitMask(ep, TEST_ulong, 0x1);
   CHECK(33, "lOrUlongBitMask sets the bits", lGetUlong(ep, TEST_ulong) == 0xB);
   lAndUlongBitMask(ep, TEST_ulong, 0x9);
   CHECK(34, "lAndUlongBitMask keeps only the common bits", lGetUlong(ep, TEST_ulong) == 0x9);
   lClearUlongBitMask(ep, TEST_ulong, 0x8);
   CHECK(35, "lClearUlongBitMask clears the bits", lGetUlong(ep, TEST_ulong) == 0x1);

   // T36: lGetStringNotNull never returns nullptr
   CHECK(36, "lGetStringNotNull of an unset string is empty",
         lGetString(ep, TEST_string) == nullptr && strcmp(lGetStringNotNull(ep, TEST_string), "") == 0);

   // T37: lXchgString swaps the stored pointer with the caller's
   lSetString(ep, TEST_string, "inside");
   char *outside = strdup("outside");
   char *before = outside;
   lXchgString(ep, TEST_string, &outside);
   CHECK(37, "lXchgString swaps the pointers",
         lGetString(ep, TEST_string) == before && strcmp(outside, "inside") == 0);
   free(outside);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// Moving lists and objects out of an element  [T38–T45]
//
// lXchgList swaps a sublist with a caller's list, lSwapList swaps sublists of
// two elements, lGetListRef hands out the address of the slot and
// lDechainObject detaches a sub-object so the caller owns it.
// ---------------------------------------------------------------------------

static void test_move_out() {
   printf("\n--- moving lists and objects out of an element ---\n");

   lListElem *ep = lCreateElem(TEST_Type);
   lList *inner = lCreateList("inner", TEST_Type);
   lSetList(ep, TEST_list, inner);

   // T38: lXchgList hands back the stored list and stores the caller's
   lList *outer = lCreateList("outer", TEST_Type);
   lList *swap = outer;
   lXchgList(ep, TEST_list, &swap);
   CHECK(38, "lXchgList swaps the lists", swap == inner && lGetList(ep, TEST_list) == outer);

   // T39: lXchgList without a list pointer fails; a field of another type
   // aborts instead, see T46
   CHECK(39, "lXchgList without a list pointer fails", lXchgList(ep, TEST_list, nullptr) == -1);
   lFreeList(&swap);

   // T40: lSwapList exchanges the sublists of two elements
   lListElem *other = lCreateElem(TEST_Type);
   lList *other_list = lCreateList("other", TEST_Type);
   lSetList(other, TEST_list2, other_list);
   lSwapList(ep, TEST_list, other, TEST_list2);
   CHECK(40, "lSwapList exchanges the sublists",
         lGetList(ep, TEST_list) == other_list && lGetList(other, TEST_list2) == outer);
   lFreeElem(&other);

   // T41: lGetListRef addresses the slot, writing through it is visible
   lList **ref = lGetListRef(ep, TEST_list);
   CHECK(41, "lGetListRef points at the stored list", ref != nullptr && *ref == other_list);

   // T42-T45: lDechainObject detaches the sub-object
   lListElem *obj = lCreateElem(TEST_Type);
   lSetUlong(obj, TEST_ulong, 42);
   lSetObject(ep, TEST_object, obj);
   lListElem *out = lDechainObject(ep, TEST_object);
   CHECK(42, "lDechainObject returns the sub-object", out == obj && lGetUlong(out, TEST_ulong) == 42);
   CHECK(43, "lDechainObject leaves the field empty", lGetObject(ep, TEST_object) == nullptr);
   CHECK(44, "lDechainObject on an empty field returns nullptr", lDechainObject(ep, TEST_object) == nullptr);

   // the detached object is free again: it can be stored in another element
   lListElem *holder = lCreateElem(TEST_Type);
   CHECK(45, "a dechained object can be stored again", lSetObject(holder, TEST_object, out) == 0 &&
         lGetObject(holder, TEST_object) == out);
   lFreeElem(&holder);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------
// Type errors abort  [T46–T50]
//
// A field accessed as another type is a programming error. The accessors do
// not return an error code for it: they abort the process (incompatibleType2),
// so a wrong type can neither be read as a wrong value nor leave a half-done
// change behind (lSwapList exchanges in three steps). Each case runs in a
// child process.
// ---------------------------------------------------------------------------

static void test_type_errors_abort() {
   printf("\n--- type errors abort ---\n");

   lListElem *ep = lCreateElem(TEST_Type);
   lSetString(ep, TEST_string, "s");

   CHECK(46, "lXchgList on a non-list field aborts", aborts([ep] {
      lList *lp = nullptr;
      lXchgList(ep, TEST_ulong, &lp);
   }));
   CHECK(47, "lSwapList with a non-list field aborts", aborts([ep] {
      lListElem *other = lCreateElem(TEST_Type);
      lSwapList(ep, TEST_list, other, TEST_string);
   }));
   CHECK(48, "lGetUlong on a string field aborts", aborts([ep] {
      lGetUlong(ep, TEST_string);
   }));
   CHECK(49, "lSetString on an ulong field aborts", aborts([ep] {
      lSetString(ep, TEST_ulong, "x");
   }));
   CHECK(50, "lXchgString on a list field aborts", aborts([ep] {
      char *str = nullptr;
      lXchgString(ep, TEST_list, &str);
   }));

   // the parent's element is untouched by what the children did
   CHECK(51, "the element in the parent is unchanged", strcmp(lGetString(ep, TEST_string), "s") == 0);

   lFreeElem(&ep);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_descriptor_queries();
   test_positional_access();
   test_setter_hash_upkeep();
   test_value_helpers();
   test_move_out();
   test_type_errors_abort();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
