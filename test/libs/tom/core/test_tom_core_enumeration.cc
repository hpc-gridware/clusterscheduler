/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for field selections in the core of the typed object model (libs/tom)
 *
 * The selections are built with the core builders ocs::tom::make_what() and
 * make_what_special(), which lWhat() parsed its format string into, and with
 * the run-time builder ocs::tom::what_of(); the test types have no generated
 * traits, so the typed what<F...>() does not apply.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <span>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_WhatP.h"
#include "tom/ocs_Descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_PrintCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"
#include "ocs_test_model.h"

/** @brief A synthetic CULL object type, defined only for these tests
 *
 * One attribute per CULL data type, so the tests can exercise every type
 * without depending on a real object whose layout may change.
 */
enum {
   TEST_int = 1,   ///< an int attribute
   TEST_host,   ///< a host name attribute
   TEST_string,   ///< a string attribute
   TEST_double,   ///< a double attribute
   TEST_long,   ///< a long attribute
   TEST_ulong,   ///< an unsigned long attribute
   TEST_ulong64,   ///< a 64-bit unsigned attribute
   TEST_bool,   ///< a bool attribute
   TEST_list,   ///< a sublist attribute
   TEST_object,   ///< a sub-object attribute
   TEST_ref   ///< a reference attribute
};

/** @brief A second synthetic type, for the tests that need two */
enum {
   TEST1_int = 51,   ///< an int attribute
   TEST1_host,   ///< a host name attribute
   TEST1_string,   ///< a string attribute
   TEST1_double,   ///< a double attribute
   TEST1_long,   ///< a long attribute
   TEST1_ulong,   ///< an unsigned long attribute
   TEST1_ulong64,   ///< a 64-bit unsigned attribute
   TEST1_bool,   ///< a bool attribute
   TEST1_list,   ///< a sublist attribute
   TEST1_object,   ///< a sub-object attribute
   TEST1_ref   ///< a reference attribute
};

LISTDEF(TEST_Type)
                SGE_INT    (TEST_int, CULL_DEFAULT)
                SGE_HOST   (TEST_host, CULL_DEFAULT)
                SGE_STRING (TEST_string, CULL_DEFAULT)
                SGE_DOUBLE (TEST_double, CULL_DEFAULT)
                SGE_LONG   (TEST_long, CULL_DEFAULT)
                SGE_ULONG  (TEST_ulong, CULL_DEFAULT)
                SGE_ULONG64 (TEST_ulong64, CULL_DEFAULT)
                SGE_BOOL   (TEST_bool, CULL_DEFAULT)
                SGE_LIST   (TEST_list, TEST1_Type, CULL_DEFAULT)
                SGE_OBJECT (TEST_object, TEST1_Type, CULL_DEFAULT)
                SGE_REF    (TEST_ref, TEST1_Type, CULL_DEFAULT)
LISTEND(TEST_Type)

LISTDEF(TEST1_Type)
                SGE_INT    (TEST1_int, CULL_DEFAULT)
                SGE_HOST   (TEST1_host, CULL_DEFAULT)
                SGE_STRING (TEST1_string, CULL_DEFAULT)
                SGE_DOUBLE (TEST1_double, CULL_DEFAULT)
                SGE_LONG   (TEST1_long, CULL_DEFAULT)
                SGE_ULONG  (TEST1_ulong, CULL_DEFAULT)
                SGE_ULONG64 (TEST1_ulong64, CULL_DEFAULT)
                SGE_BOOL   (TEST1_bool, CULL_DEFAULT)
                SGE_LIST   (TEST1_list, TEST_Type, CULL_DEFAULT)
                SGE_OBJECT (TEST1_object, TEST_Type, CULL_DEFAULT)
                SGE_REF    (TEST1_ref, TEST_Type, CULL_DEFAULT)
LISTEND(TEST1_Type)

NAMEDEF(TEST_Name)
                NAME("TEST_int")
                NAME("TEST_host")
                NAME("TEST_string")
                NAME("TEST_double")
                NAME("TEST_long")
                NAME("TEST_ulong")
                NAME("TEST_ulong64")
                NAME("TEST_bool")
                NAME("TEST_list")
                NAME("TEST_object")
                NAME("TEST_ref")
NAMEEND

NAMEDEF(TEST1_Name)
                NAME("TEST1_int")
                NAME("TEST1_host")
                NAME("TEST1_string")
                NAME("TEST1_double")
                NAME("TEST1_long")
                NAME("TEST1_ulong")
                NAME("TEST1_ulong64")
                NAME("TEST1_bool")
                NAME("TEST1_list")
                NAME("TEST1_object")
                NAME("TEST1_ref")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the synthetic type
#define TEST1_Size sizeof(TEST1_Name) / sizeof(char *)   ///< number of attributes of the second synthetic type

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

/**
 * @brief Build a selection of fields of one type with the core builder
 * @param descr the descriptor of the type
 * @param entries the fields with their nested selections, which the selection takes over
 * @return the selection
 */
static lEnumeration *make(const lDescr *descr, const std::initializer_list<ocs::tom::WhatEntry> entries) {
   return ocs::tom::make_what(descr, std::span<const ocs::tom::WhatEntry>(entries.begin(), entries.size()));
}

/**
 * @brief The selection of every field, as lWhat("%T(ALL)") gave it
 * @return the selection
 */
static lEnumeration *all() {
   return ocs::tom::make_what_special(WHAT_ALL);
}

/**
 * @brief The selection of no field, as lWhat("%T(NONE)") gave it
 * @return the selection
 */
static lEnumeration *none() {
   return ocs::tom::make_what_special(WHAT_NONE);
}

/**
 * @brief The nested selection the tests use on TEST_object, as
 *        lWhat("%T(%I %I %I -> %T(%I %I -> %T(%I %I)) %I -> %T(NONE) %I -> %T(ALL))") built it
 *
 * TEST_object is given three times; the selection keeps it once, with the first nested selection.
 *
 * @return the selection
 */
static ocs::tom::RawWhatPtr make_nested_object_what() {
   return ocs::tom::RawWhatPtr(make(TEST_Type, {
      {TEST_int, nullptr},
      {TEST_host, nullptr},
      {TEST_object, make(TEST1_Type, {{TEST1_int, nullptr},
                                      {TEST1_object, make(TEST1_Type, {{TEST1_int, nullptr},
                                                                       {TEST1_host, nullptr}})}})},
      {TEST_object, none()},
      {TEST_object, all()},
   }));
}

/**
 * @brief The nested selection the tests use on TEST_list, TEST_object and TEST_ref, as
 *        lWhat("%T(%I %I %I -> %T(%I %I -> %T(%I %I)) %I -> %T(NONE) %I -> %T(ALL))") built it
 * @return the selection of five fields
 */
static ocs::tom::RawWhatPtr make_complex_what() {
   return ocs::tom::RawWhatPtr(make(TEST_Type, {
      {TEST_int, nullptr},
      {TEST_host, nullptr},
      {TEST_list, make(TEST1_Type, {{TEST1_int, nullptr},
                                    {TEST1_object, make(TEST1_Type, {{TEST1_int, nullptr},
                                                                     {TEST1_host, nullptr}})}})},
      {TEST_object, none()},
      {TEST_ref, all()},
   }));
}

/**
 * @brief Compare the entries of a selection with an expected table, recursing into nested selections
 * @param what the selection
 * @param result the expected table: per entry nm, mt, pos and the number of entries of its nested selection
 * @param pos_what the entry of @p what to start at; advanced
 * @param pos_result the entry of @p result to start at; advanced
 * @param max the number of entries to compare
 * @return true if every entry matches
 */
static bool test_what_enumeration(const lEnumeration *what, const int *result,
                                  int *pos_what, int *pos_result, const int max) {
   bool ret = true;

   for (int i = 0; i < max; i++) {
      int tmp_pos_what = 0;

      ret &= (what[*pos_what].nm  == result[*pos_result] &&
              what[*pos_what].mt  == result[*pos_result + 1] &&
              what[*pos_what].pos == result[*pos_result + 2]);

      const int tmp_max = result[*pos_result + 3];
      *pos_result = *pos_result + 4;

      if (what[*pos_what].ep != nullptr) {
         ret &= test_what_enumeration(what[*pos_what].ep, result, &tmp_pos_what, pos_result, tmp_max);
      }

      *pos_what = *pos_what + 1;
   }

   return ret;
}

/**
 * @brief The entries of a selection of three plain and object fields, as lWhat("%T(%I%I%I)") laid them out
 * @return true if the entries are the expected ones
 */
static bool test_what_simple() {
   // MT_INT32=5, MT_HOST=10, MT_OBJECT=8 in current CULL enum
   const ocs::tom::RawWhatPtr what(make(TEST_Type, {{TEST_int, nullptr}, {TEST_host, nullptr},
                                                    {TEST_object, nullptr}}));
   const int result[] = {1,       5,  0, 0,   // TEST_int:    nm=1, mt=MT_INT32=5,    pos=0
                         2,      10,  1, 0,   // TEST_host:   nm=2, mt=MT_HOST=10,  pos=1
                         10,      8,  9, 0,   // TEST_object: nm=10, mt=MT_OBJECT=8, pos=9
                         NoName, MT_END, 0, 0};
   int pos_what = 0;
   int pos_result = 0;
   return test_what_enumeration(what.get(), result, &pos_what, &pos_result, 4);
}

/**
 * @brief Compare two selections by their printed form
 * @param what1 the first selection
 * @param what2 the second selection
 * @return 0 if both print the same, as strcmp() otherwise
 */
static int enumeration_compare(const lEnumeration *what1, const lEnumeration *what2) {
   dstring str1 = DSTRING_INIT;
   dstring str2 = DSTRING_INIT;
   ocs::tom::print_what(&str1, what1, 0);
   ocs::tom::print_what(&str2, what2, 0);
   const int ret = strcmp(sge_dstring_get_string(&str1), sge_dstring_get_string(&str2));
   sge_dstring_free(&str1);
   sge_dstring_free(&str2);
   return ret;
}

/**
 * @brief Run all selection tests
 * @return 0 if every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_tom_core_enumeration");
   component_set_daemonized(true);
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size},
                                         {"TEST1", TEST1_Type, TEST1_Name, TEST1_Size}});

   // --- building a selection ---
   printf("\n--- make_what ---\n");
   {
      // ALL sentinel: nm=-99, mt=-99, pos=-1
      const ocs::tom::RawWhatPtr what = ocs::tom::what_all();
      CHECK(1, "what_all sentinel: nm=-99 mt=-99 pos=-1",
            what.get()[0].nm == -99 && what.get()[0].mt == -99 && what.get()[0].pos == -1);
   }
   {
      // NONE sentinel: nm=-99, mt=-99, pos=-2
      const ocs::tom::RawWhatPtr what(none());
      CHECK(2, "make_what_special(WHAT_NONE) sentinel: nm=-99 mt=-99 pos=-2",
            what.get()[0].nm == -99 && what.get()[0].mt == -99 && what.get()[0].pos == -2);
   }
   CHECK(3, "make_what simple 3-field selection internal structure correct", test_what_simple());
   {
      // complex nested what: verify top-level count and copy consistency
      const ocs::tom::RawWhatPtr what = make_nested_object_what();
      const ocs::tom::RawWhatPtr copy = ocs::tom::clone(what.get());
      CHECK(4, "make_what complex nested what: copy is structurally identical",
            enumeration_compare(what.get(), copy.get()) == 0);
   }

   // --- count_what ---
   printf("\n--- count_what ---\n");
   {
      const ocs::tom::RawWhatPtr what1 = make_complex_what();
      const ocs::tom::RawWhatPtr what2(make(TEST_Type, {{TEST_int, nullptr}, {TEST_list, nullptr},
                                                        {TEST_object, nullptr}, {TEST_ref, nullptr},
                                                        {TEST_host, nullptr}}));
      const int count = ocs::tom::count_what(what1.get(), TEST_Type);
      CHECK(5, "count_what complex expression returns 5", count == 5);
      CHECK(6, "two equivalent whats yield same count", count == ocs::tom::count_what(what2.get(), TEST_Type));
   }

   // --- reduced_descr ---
   printf("\n--- reduced_descr ---\n");
   {
      const ocs::tom::RawWhatPtr what1(all());
      const ocs::tom::RawWhatPtr what2(none());
      const ocs::tom::RawWhatPtr what3 = make_complex_what();
      const lDescr *dst1 = ocs::tom::reduced_descr(TEST_Type, what1.get());
      const lDescr *dst2 = ocs::tom::reduced_descr(TEST_Type, what2.get());
      const lDescr *dst3 = ocs::tom::reduced_descr(TEST_Type, what3.get());
      // TEST_Type has 11 fields; ALL selects all → descr_length == count_what == 11
      CHECK(7,  "ALL: descr_length == 11", ocs::tom::descr_length(dst1) == 11);
      CHECK(8,  "ALL: count_what == 11",  ocs::tom::count_what(what1.get(), TEST_Type) == 11);
      CHECK(9,  "NONE: descr_length == -1", ocs::tom::descr_length(dst2) == -1);
      CHECK(10, "NONE: count_what == 0",  ocs::tom::count_what(what2.get(), TEST_Type) == 0);
      // a reduction has the length of its type and holds the selected fields (CS-439)
      CHECK(11, "partial: descr_length == 11, lCountPresent == 5",
            ocs::tom::descr_length(dst3) == 11 && lCountPresent(dst3) == 5);
      CHECK(12, "partial: count_what == 5",  ocs::tom::count_what(what3.get(), TEST_Type) == 5);
      ocs::tom::release_descr(dst1);
      ocs::tom::release_descr(dst2);
      ocs::tom::release_descr(dst3);
   }

   // --- clone ---
   printf("\n--- clone ---\n");
   {
      const ocs::tom::RawWhatPtr what1(all());
      const ocs::tom::RawWhatPtr what2(none());
      const ocs::tom::RawWhatPtr what3 = make_complex_what();
      const ocs::tom::RawWhatPtr copy1 = ocs::tom::clone(what1.get());
      const ocs::tom::RawWhatPtr copy2 = ocs::tom::clone(what2.get());
      const ocs::tom::RawWhatPtr copy3 = ocs::tom::clone(what3.get());
      CHECK(13, "copy of ALL is structurally identical", enumeration_compare(what1.get(), copy1.get()) == 0);
      CHECK(14, "copy of NONE is structurally identical", enumeration_compare(what2.get(), copy2.get()) == 0);
      CHECK(15, "copy of complex what is structurally identical", enumeration_compare(what3.get(), copy3.get()) == 0);
   }

   // --- what_of ---
   printf("\n--- what_of ---\n");
   {
      const ocs::tom::RawWhatPtr what1(make(TEST_Type, {{TEST_int, nullptr}, {TEST_list, nullptr},
                                                        {TEST_object, nullptr}, {TEST_ref, nullptr}}));
      const int vector1[] = {TEST_int, TEST_list, TEST_object, TEST_ref};
      const ocs::tom::RawWhatPtr what2 = ocs::tom::what_of(TEST_Type, vector1);
      CHECK(16, "what_of matches manually built what", enumeration_compare(what1.get(), what2.get()) == 0);
   }

   // --- select ---
   printf("\n--- select ---\n");
   {
      // lWhat("%T(%I %I -> %T( %I %I -> %T (%I %I %I %I) %I %I) %I -> %T(%I) %I)")
      const ocs::tom::RawWhatPtr what(make(TEST_Type, {
         {TEST_int, nullptr},
         {TEST_list, make(TEST1_Type, {{TEST1_int, nullptr},
                                       {TEST1_list, make(TEST_Type, {{TEST_int, nullptr}, {TEST_list, nullptr},
                                                                     {TEST_object, nullptr},
                                                                     {TEST_string, nullptr}})},
                                       {TEST1_object, nullptr},
                                       {TEST1_string, nullptr}})},
         {TEST_object, make(TEST1_Type, {{TEST1_string, nullptr}})},
         {TEST_string, nullptr},
      }));
      auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "");
      auto elem = ocs::tom::RawElementPtr::create(TEST_Type);
      elem->set(TEST_int, std::int32_t{0});
      elem->set(TEST_host, ocs::tom::Host("zero"));
      elem->set(TEST_string, "zero");
      elem->set(TEST_double, 0.0);
      elem->set(TEST_long, std::int64_t{0});
      elem->set(TEST_ulong, std::uint32_t{0});
      elem->set(TEST_ulong64, std::uint64_t{0});
      elem->set(TEST_bool, false);
      // the levels alternate between the types as the sublist fields do: TEST_list holds TEST1 elements,
      // TEST1_list holds TEST elements; an element of the other type would not have the list's layout
      auto elem1 = ocs::tom::RawElementPtr::create(TEST1_Type);
      elem1->set(TEST1_int, std::int32_t{0});
      elem1->set(TEST1_string, "zero");
      for (int i = 0; i < 5; i++) {
         auto tmp_list = ocs::tom::RawListPtr::create(TEST1_Type, ocs::tom::Hashing::On, "");
         auto tmp_elem = elem.clone();
         for (int j = 0; j < 5; j++) {
            auto tmp_list1 = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "");
            auto tmp_elem1 = elem1.clone();
            for (int k = 0; k < 5; k++) {
               auto tmp_elem2 = elem.clone();
               tmp_elem2->set(TEST_list, ocs::tom::RawListPtr::create(TEST1_Type, ocs::tom::Hashing::On, ""));
               tmp_list1->push_back(std::move(tmp_elem2));
            }
            tmp_elem1->set(TEST1_list, std::move(tmp_list1));
            tmp_list->push_back(std::move(tmp_elem1));
         }
         tmp_elem->set(TEST_list, std::move(tmp_list));
         list->push_back(std::move(tmp_elem));
      }
      const ocs::tom::RawListPtr list1 = ocs::tom::select(list.get(), nullptr, what.get(), ocs::tom::Hashing::On);
      // all 5 elements are selected, reduced to the 4 fields of the outer level of the what
      CHECK(17, "select with complex nested what selects every element, reduced to the what",
            list1 && list1->size() == 5 && lCountPresent(list1->descr()) == 4);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
