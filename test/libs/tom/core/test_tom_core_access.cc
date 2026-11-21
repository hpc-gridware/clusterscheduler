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
 * @brief Unit tests for field access through the run-time typed interface of `libs/tom`
 *
 * Covers the descriptor queries (field_type(), field_pos(), has()), the
 * checked getters and setters of the raw interface for every data type, the
 * hash maintenance of the setters and of the core's add_number(), and the
 * functions that move a list or an object out of an element (exchange(),
 * take()). Where a setter maintains a hash table, the tests check that the
 * element is still found by its new value.
 *
 * Accessing a field as the wrong type is a programming error. The old
 * interface aborted the process; the raw interface throws a FieldException
 * with reason WrongType and changes nothing. The last group checks that.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_Raw.h"
#include "ocs_test_model.h"

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
   TEST_list2,   ///< a second sublist attribute, the partner for swapping sublists
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
LISTEND(TEST_Type)

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

/** @brief Run a callable and report whether it throws a FieldException with a given reason
 *
 * @param reason the reason the exception must carry
 * @param what the code that is expected to throw
 * @return true when a FieldException with @p reason was thrown
 */
static bool throws(const ocs::tom::FieldException::Reason reason, const std::function<void()> &what) {
   try {
      what();
   } catch (const ocs::tom::FieldException &e) {
      return e.reason() == reason;
   }
   return false;
}

/** @brief Position of a field in the synthetic type's descriptor
 *
 * @param nm the field name id
 * @return the field's index in TEST_Type
 */
static int pos_of(const int nm) {
   return ocs::tom::field_pos(TEST_Type, nm);
}

/** @brief Add an offset to a number field through the core, as the typed add<F>() does
 *
 * @tparam V the value type of the field: std::uint32_t, std::uint64_t or double
 * @param ep the element, which holds the field
 * @param nm the field
 * @param offset the amount to add
 * @return the new value
 */
template <typename V>
static V add_to(const ocs::tom::RawElement ep, const int nm, const V offset) {
   const int pos = ocs::tom::field_pos(ep.descr(), nm);
   return ocs::tom::add_number<V>(ep, pos, ocs::tom::detail::value(ep, pos), offset);
}

// ---------------------------------------------------------------------------
// Descriptor queries  [T01–T08]
//
// field_type_of() and field_type() map a position or a name to the data
// type, field_pos() maps a name to its position, has() looks a name up through
// an element. Unknown names yield lEndT, -1 or false.
// ---------------------------------------------------------------------------

/** @brief The descriptor queries of the core */
static void test_descriptor_queries() {
   printf("\n--- descriptor queries ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);

   CHECK(1, "field_type_of() of the entry of TEST_double is Double",
         ocs::tom::field_type_of(TEST_Type[pos_of(TEST_double)].mt) == ocs::tom::FieldType::Double);
   CHECK(2, "field_type() of TEST_list is lListT", ocs::tom::field_type(TEST_Type, TEST_list) == lListT);
   CHECK(3, "field_type() of TEST_ulong64_ukey is lUlong64T",
         ocs::tom::field_type(TEST_Type, TEST_ulong64_ukey) == lUlong64T);
   CHECK(4, "field_type() of an unknown name is lEndT", ocs::tom::field_type(TEST_Type, 4711) == lEndT);
   CHECK(5, "the entry at the position field_pos() gives holds the name",
         TEST_Type[pos_of(TEST_ref)].nm == TEST_ref);
   // CHECK 6 removed: lGetPosName() has no TOM counterpart (CS-3103)
   CHECK(7, "field_pos() through an element gives the position of the descriptor, has() is true",
         ocs::tom::field_pos(ep->descr(), TEST_host) == 1 && ep->has(TEST_host));
   CHECK(8, "field_pos() of an unknown name is negative, has() is false",
         ocs::tom::field_pos(ep->descr(), 4711) < 0 && !ep->has(4711));
}

// ---------------------------------------------------------------------------
// Getters and setters  [T09–T22]
//
// Every value stored with set(nm, v) must be readable through get<V>(nm),
// since both address the same slot.
// ---------------------------------------------------------------------------

/** @brief The raw getters and setters for every data type */
static void test_positional_access() {
   printf("\n--- getters and setters ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);

   ep->set(TEST_int, std::int32_t{-3});
   CHECK(9, "set/get of an int field", ep->get<std::int32_t>(TEST_int) == -3);

   ep->set(TEST_ulong, std::uint32_t{17});
   CHECK(10, "set/get of an ulong field", ep->get<std::uint32_t>(TEST_ulong) == 17);

   ep->set(TEST_ulong64, std::uint64_t{0x123456789ULL});
   CHECK(11, "set/get of an ulong64 field", ep->get<std::uint64_t>(TEST_ulong64) == 0x123456789ULL);

   ep->set(TEST_string, static_cast<const char *>("pos"));
   CHECK(12, "set/get of a string field", strcmp(ep->get<const char *>(TEST_string), "pos") == 0);

   ep->set(TEST_host, ocs::tom::Host("hpos"));
   CHECK(13, "set/get of a host field", strcmp(ep->get<ocs::tom::Host>(TEST_host), "hpos") == 0);

   ep->set(TEST_double, 2.5);
   CHECK(14, "set/get of a double field", ep->get<double>(TEST_double) == 2.5);

   ep->set(TEST_long, std::int64_t{-1234567L});
   CHECK(15, "set/get of a long field", ep->get<std::int64_t>(TEST_long) == -1234567L);

   ep->set(TEST_bool, true);
   CHECK(16, "set/get of a bool field", ep->get<bool>(TEST_bool) == true);

   int target = 0;
   ep->set(TEST_ref, static_cast<void *>(&target));
   CHECK(17, "set/get of a reference field", ep->get<void *>(TEST_ref) == &target);

   auto sub = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "sub");
   sub->push_back(ocs::tom::RawElementPtr::create(TEST_Type));
   const lList *sub_raw = sub.get();
   ep->set(TEST_list, std::move(sub));
   CHECK(18, "set/get of a list field stores the list itself", ep->get<const lList *>(TEST_list) == sub_raw);

   auto obj = ocs::tom::RawElementPtr::create(TEST_Type);
   const lListElem *obj_raw = obj.get();
   ep->set(TEST_object, std::move(obj));
   CHECK(19, "set/get of an object field stores the object itself",
         ep->get<const lListElem *>(TEST_object) == obj_raw);

   // CHECK 20 removed: lGetPosStringRef() has no TOM counterpart (CS-3103)
   // CHECK 21 removed: lGetPosHostRef() has no TOM counterpart (CS-3103)

   // T22: setting a string to nullptr clears the field
   ep->set(TEST_string, static_cast<const char *>(nullptr));
   CHECK(22, "set of a string field to nullptr clears the field", ep->get<const char *>(TEST_string) == nullptr);
}

// ---------------------------------------------------------------------------
// Hash maintenance of the setters  [T23–T28]
//
// When an element in a list changes a hashed key, the old key must no longer
// find it and the new key must. This is checked for the raw setters and for
// the core's add_number(), which changes the value by an offset.
// ---------------------------------------------------------------------------

/** @brief The setters and add_number() keep the hash tables of the list up to date */
static void test_setter_hash_upkeep() {
   printf("\n--- hash maintenance of the setters ---\n");

   auto lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "keys");
   auto elem = ocs::tom::RawElementPtr::create(TEST_Type);
   elem->set(TEST_ulong_ukey, std::uint32_t{10});
   elem->set(TEST_ulong64_ukey, std::uint64_t{100});
   elem->set(TEST_string_ukey, static_cast<const char *>("old"));
   const ocs::tom::RawElement ep = lp->push_back(std::move(elem));
   const lListElem *ep_raw = ep.raw();

   ep.set(TEST_ulong_ukey, std::uint32_t{11});
   CHECK(23, "set of a hashed ulong key: new key found, old key gone",
         lp->find_first(TEST_ulong_ukey, std::uint32_t{11}).raw() == ep_raw &&
         !lp->find_first(TEST_ulong_ukey, std::uint32_t{10}));

   ep.set(TEST_ulong64_ukey, std::uint64_t{101});
   CHECK(24, "set of a hashed ulong64 key: new key found, old key gone",
         lp->find_first(TEST_ulong64_ukey, std::uint64_t{101}).raw() == ep_raw &&
         !lp->find_first(TEST_ulong64_ukey, std::uint64_t{100}));

   ep.set(TEST_string_ukey, static_cast<const char *>("new"));
   CHECK(25, "set of a hashed string key: new key found, old key gone",
         lp->find_first(TEST_string_ukey, static_cast<const char *>("new")).raw() == ep_raw &&
         !lp->find_first(TEST_string_ukey, static_cast<const char *>("old")));

   add_to(ep, TEST_ulong_ukey, std::uint32_t{5});
   CHECK(26, "add_number() on a hashed ulong key: value and hash follow",
         ep.get<std::uint32_t>(TEST_ulong_ukey) == 16 &&
         lp->find_first(TEST_ulong_ukey, std::uint32_t{16}).raw() == ep_raw &&
         !lp->find_first(TEST_ulong_ukey, std::uint32_t{11}));

   add_to(ep, TEST_ulong64_ukey, std::uint64_t{0x100000000ULL});
   CHECK(27, "add_number() on a hashed ulong64 key: value and hash follow",
         ep.get<std::uint64_t>(TEST_ulong64_ukey) == 0x100000065ULL &&
         lp->find_first(TEST_ulong64_ukey, std::uint64_t{0x100000065ULL}).raw() == ep_raw &&
         !lp->find_first(TEST_ulong64_ukey, std::uint64_t{101}));

   // T28: an offset of zero changes nothing, the key stays findable
   add_to(ep, TEST_ulong_ukey, std::uint32_t{0});
   CHECK(28, "add_number() with offset 0 keeps value and hash",
         ep.get<std::uint32_t>(TEST_ulong_ukey) == 16 &&
         lp->find_first(TEST_ulong_ukey, std::uint32_t{16}).raw() == ep_raw);
}

// ---------------------------------------------------------------------------
// Arithmetic  [T29–T37]
// ---------------------------------------------------------------------------

/** @brief add_number() on fields that are not hashed */
static void test_value_helpers() {
   printf("\n--- arithmetic ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);

   ep->set(TEST_double, 1.25);
   add_to(*ep, TEST_double, 0.5);
   CHECK(29, "add_number() on a double adds the offset", ep->get<double>(TEST_double) == 1.75);

   ep->set(TEST_ulong, std::uint32_t{7});
   add_to(*ep, TEST_ulong, std::uint32_t{3});
   CHECK(30, "add_number() on an unhashed ulong adds the offset", ep->get<std::uint32_t>(TEST_ulong) == 10);

   // The bit mask helpers, lGetStringNotNull() and lXchgString() exist in TOM only in the typed form
   // (has_any_bits<F>, or_bits<F>, and_bits<F>, clear_bits<F>, get_or_empty<F>, exchange<F>(ep, &str)), which
   // needs the traits of a generated type; test/libs/tom/test_tom_free.cc checks them.
   // CHECK 31 removed: lMatchUlongBitMask() has no TOM counterpart for a test type (CS-3103)
   // CHECK 32 removed: lMatchUlongBitMask() has no TOM counterpart for a test type (CS-3103)
   // CHECK 33 removed: lOrUlongBitMask() has no TOM counterpart for a test type (CS-3103)
   // CHECK 34 removed: lAndUlongBitMask() has no TOM counterpart for a test type (CS-3103)
   // CHECK 35 removed: lClearUlongBitMask() has no TOM counterpart for a test type (CS-3103)
   // CHECK 36 removed: lGetStringNotNull() has no TOM counterpart for a test type (CS-3103)
   // CHECK 37 removed: lXchgString() has no TOM counterpart for a test type (CS-3103)
}

// ---------------------------------------------------------------------------
// Moving lists and objects out of an element  [T38–T45]
//
// exchange() swaps a sublist with a caller's list, two exchanges swap the
// sublists of two elements, and take() detaches a sub-object so the caller
// owns it.
// ---------------------------------------------------------------------------

/** @brief exchange() and take() move lists and objects out of an element */
static void test_move_out() {
   printf("\n--- moving lists and objects out of an element ---\n");

   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   auto inner_owner = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "inner");
   const lList *inner = inner_owner.get();
   ep->set(TEST_list, std::move(inner_owner));

   // T38: exchange() hands back the stored list and stores the caller's
   auto outer_owner = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "outer");
   const lList *outer = outer_owner.get();
   ocs::tom::RawListPtr swap = ep->exchange(TEST_list, std::move(outer_owner));
   CHECK(38, "exchange() swaps the lists",
         static_cast<const lList *>(swap.get()) == inner && ep->get<const lList *>(TEST_list) == outer);

   // CHECK 39 removed: lXchgList() without a list pointer has no TOM counterpart (CS-3103)
   swap.reset();

   // T40: two exchanges swap the sublists of two elements
   auto other = ocs::tom::RawElementPtr::create(TEST_Type);
   auto other_owner = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "other");
   const lList *other_list = other_owner.get();
   other->set(TEST_list2, std::move(other_owner));
   ocs::tom::RawListPtr remaining =
           ep->exchange(TEST_list, other->exchange(TEST_list2, ep->exchange(TEST_list, {})));
   CHECK(40, "two exchanges swap the sublists",
         ep->get<const lList *>(TEST_list) == other_list && other->get<const lList *>(TEST_list2) == outer &&
         !remaining);
   other.reset();

   // CHECK 41 removed: lGetListRef() has no TOM counterpart for a test type (CS-3103)

   // T42-T45: take() detaches the sub-object
   auto obj = ocs::tom::RawElementPtr::create(TEST_Type);
   obj->set(TEST_ulong, std::uint32_t{42});
   const lListElem *obj_raw = obj.get();
   ep->set(TEST_object, std::move(obj));
   ocs::tom::RawElementPtr out = ep->take(TEST_object);
   CHECK(42, "take() returns the sub-object",
         static_cast<const lListElem *>(out.get()) == obj_raw && out->get<std::uint32_t>(TEST_ulong) == 42);
   CHECK(43, "take() leaves the field empty", ep->get<const lListElem *>(TEST_object) == nullptr);
   CHECK(44, "take() on an empty field returns an empty owner", !ep->take(TEST_object));

   // the detached object is free again: it can be stored in another element
   auto holder = ocs::tom::RawElementPtr::create(TEST_Type);
   bool stored = true;
   try {
      holder->set(TEST_object, std::move(out));
   } catch (const ocs::tom::FieldException &) {
      stored = false;
   }
   CHECK(45, "a taken object can be stored again",
         stored && holder->get<const lListElem *>(TEST_object) == obj_raw);
}

// ---------------------------------------------------------------------------
// Type errors throw  [T46–T51]
//
// A field accessed as another type is a programming error. The old accessors
// aborted the process for it (incompatibleType2); the raw interface throws a
// FieldException with reason WrongType before it changes anything, so a wrong
// type can neither be read as a wrong value nor leave a half-done change
// behind.
// ---------------------------------------------------------------------------

/** @brief Accessing a field as another type throws WrongType and changes nothing */
static void test_type_errors_abort() {
   printf("\n--- type errors throw ---\n");

   auto owner = ocs::tom::RawElementPtr::create(TEST_Type);
   const ocs::tom::RawElement ep = owner.get();
   ep.set(TEST_string, static_cast<const char *>("s"));

   CHECK(46, "exchange() on a non-list field throws WrongType",
         throws(ocs::tom::FieldException::Reason::WrongType, [ep] {
            static_cast<void>(ep.exchange(TEST_ulong, ocs::tom::RawListPtr()));
         }));
   // CHECK 47 removed: lSwapList() has no TOM counterpart; the type check of exchange() is CHECK 46 (CS-3103)
   CHECK(48, "get<std::uint32_t>() of a string field throws WrongType",
         throws(ocs::tom::FieldException::Reason::WrongType, [ep] {
            static_cast<void>(ep.get<std::uint32_t>(TEST_string));
         }));
   CHECK(49, "set() of a string into an ulong field throws WrongType",
         throws(ocs::tom::FieldException::Reason::WrongType, [ep] {
            ep.set(TEST_ulong, static_cast<const char *>("x"));
         }));
   // CHECK 50 removed: lXchgString() has no TOM counterpart for a test type (CS-3103)

   // the element is untouched by the calls that threw
   CHECK(51, "the element is unchanged", strcmp(ep.get<const char *>(TEST_string), "s") == 0 &&
         ep.get<std::uint32_t>(TEST_ulong) == 0 && ep.get<const lList *>(TEST_list) == nullptr);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size}});

   test_descriptor_queries();
   test_positional_access();
   test_setter_hash_upkeep();
   test_value_helpers();
   test_move_out();
   test_type_errors_abort();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
