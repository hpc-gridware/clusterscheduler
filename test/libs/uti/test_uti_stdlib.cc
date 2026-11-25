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
 * @brief Module test of the allocation helpers ocs_new(), ocs_delete(), ocs_c_alloc() and ocs_c_ptr (CS-3105)
 */

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"

namespace {

   /** @brief A plain C struct: trivial, an aggregate, what sge_malloc() allocates today */
   struct plain_t {
      int number;
      double value;
      char *name;
      char buffer[16];
   };

   /** @brief A struct with an owning member: not trivial, must not be C-allocated */
   struct owning_t {
      std::string name;
   };

   /** @brief Number of counted_t objects destroyed so far */
   int destroyed = 0;

   /** @brief A class with a constructor and a destructor that counts */
   class counted_t {
   public:
      int id;

      /**
       * @brief Make a counted object
       * @param value id to keep
       */
      explicit counted_t(const int value = 0) : id{value} {}

      /** @brief Count the destruction */
      ~counted_t() { ++destroyed; }

      counted_t(const counted_t &) = delete;
      counted_t &operator=(const counted_t &) = delete;
   };

   /**
    * @brief Tell whether every byte of a block is zero
    * @param p start of the block
    * @param size size in bytes
    * @return true if all bytes are 0
    */
   bool
   all_zero(const void *p, const size_t size) {
      const auto *bytes = static_cast<const unsigned char *>(p);
      for (size_t i = 0; i < size; ++i) {
         if (bytes[i] != 0) {
            return false;
         }
      }
      return true;
   }

   // ocs_c_alloc() keys its static_assert on ocs_c_allocatable, so these assertions state at compile time
   // which types it accepts: `ocs_c_alloc<std::string>()` or `ocs_c_alloc<owning_t>()` does not compile.
   static_assert(ocs_c_allocatable<plain_t>);
   static_assert(ocs_c_allocatable<int>);
   static_assert(!ocs_c_allocatable<std::string>);
   static_assert(!ocs_c_allocatable<owning_t>);
   static_assert(!ocs_c_allocatable<counted_t>);

   // ocs_delete() and ocs_delete_array() take the pointer by reference, so they cannot be called on a
   // temporary whose nulling would get lost; `void *` and incomplete types are rejected by static_assert.
   template <typename P>
   concept deletable = requires { ocs_delete(std::declval<P>()); };
   static_assert(deletable<plain_t *&>);
   static_assert(!deletable<plain_t *>);

} // namespace

/**
 * @brief ocs_new() value-initialises like calloc() and forwards its arguments
 */
static void
test_new() {
   // dirty the heap first, so that a block that is not initialised would likely show garbage
   for (int i = 0; i < 16; ++i) {
      auto *dirty = static_cast<unsigned char *>(malloc(sizeof(plain_t)));
      memset(dirty, 0xa5, sizeof(plain_t));
      free(dirty);
   }

   plain_t *plain = ocs_new<plain_t>();
   CHECK(1, "ocs_new<plain_t>() is not nullptr", plain != nullptr);
   CHECK(2, "ocs_new<plain_t>() zeroes every member",
         plain->number == 0 && plain->value == 0.0 && plain->name == nullptr && plain->buffer[0] == '\0' &&
         all_zero(plain->buffer, sizeof(plain->buffer)));
   ocs_delete(plain);

   plain = ocs_new<plain_t>(7, 2.5);
   CHECK(3, "ocs_new<plain_t>(7, 2.5) fills an aggregate member by member, the rest is zero",
         plain->number == 7 && plain->value == 2.5 && plain->name == nullptr);
   ocs_delete(plain);

   auto *vec = ocs_new<std::vector<int>>(3, 7);
   CHECK(4, "ocs_new<std::vector<int>>(3, 7) calls the size constructor, not the initializer_list one",
         vec->size() == 3 && (*vec)[2] == 7);
   ocs_delete(vec);

   auto *owning = ocs_new<owning_t>("name");
   CHECK(5, "ocs_new<owning_t>(\"name\") constructs the std::string member", owning->name == "name");
   ocs_delete(owning);

   int *array = ocs_new_array<int>(8);
   CHECK(6, "ocs_new_array<int>(8) zeroes every element", all_zero(array, 8 * sizeof(int)));
   ocs_delete_array(array);
}

/**
 * @brief ocs_delete() and ocs_delete_array() run the destructors, null the pointer and accept nullptr
 */
static void
test_delete() {
   destroyed = 0;
   auto *one = ocs_new<counted_t>(42);
   CHECK(10, "ocs_new<counted_t>(42) calls the constructor", one->id == 42);
   ocs_delete(one);
   CHECK(11, "ocs_delete() runs the destructor once", destroyed == 1);
   CHECK(12, "ocs_delete() sets the pointer to nullptr", one == nullptr);
   ocs_delete(one);
   CHECK(13, "ocs_delete() of nullptr does nothing", one == nullptr && destroyed == 1);

   destroyed = 0;
   auto *many = ocs_new_array<counted_t>(5);
   ocs_delete_array(many);
   CHECK(14, "ocs_delete_array() runs the destructor of all 5 elements", destroyed == 5);
   CHECK(15, "ocs_delete_array() sets the pointer to nullptr", many == nullptr);
   ocs_delete_array(many);
   CHECK(16, "ocs_delete_array() of nullptr does nothing", many == nullptr && destroyed == 5);

   const plain_t *constant = ocs_new<plain_t>();
   ocs_delete(constant);
   CHECK(17, "ocs_delete() accepts a pointer to const", constant == nullptr);
}

/**
 * @brief ocs_c_alloc() returns zeroed C memory for n elements that sge_free() releases
 */
static void
test_c_alloc() {
   constexpr size_t n = 100;
   for (int i = 0; i < 16; ++i) {
      auto *dirty = static_cast<unsigned char *>(malloc(n * sizeof(plain_t)));
      memset(dirty, 0xa5, n * sizeof(plain_t));
      free(dirty);
   }

   plain_t *one = ocs_c_alloc<plain_t>();
   CHECK(20, "ocs_c_alloc<plain_t>() returns zeroed memory", one != nullptr && all_zero(one, sizeof(*one)));
   sge_free(&one);
   CHECK(21, "sge_free() releases it and nulls the pointer", one == nullptr);

   plain_t *many = ocs_c_alloc<plain_t>(n);
   CHECK(22, "ocs_c_alloc<plain_t>(100) zeroes all 100 elements",
         many != nullptr && all_zero(many, n * sizeof(plain_t)));
   many[n - 1].number = 1; // the last element is writable
   free(many);

   CHECK(23, "ocs_c_alloc<int>(0) returns nullptr like sge_malloc(0)", ocs_c_alloc<int>(0) == nullptr);
}

/**
 * @brief ocs_c_ptr owns memory from C calls and releases it with free()
 */
static void
test_c_ptr() {
   ocs_c_ptr<char> copy{strdup("text")};
   CHECK(30, "ocs_c_ptr<char> holds a strdup() result", copy != nullptr && strcmp(copy.get(), "text") == 0);
   copy.reset(strdup("other"));
   CHECK(31, "reset() frees the old block and takes the new one", strcmp(copy.get(), "other") == 0);
   copy.reset();
   CHECK(32, "reset() without argument frees the block", copy == nullptr);

   ocs_c_ptr<char> real{realpath("/", nullptr)};
   CHECK(33, "ocs_c_ptr<char> holds a realpath(path, nullptr) result",
         real != nullptr && strcmp(real.get(), "/") == 0);

   ocs_c_ptr<plain_t[]> array{ocs_c_alloc<plain_t>(4)};
   array[3].number = 3;
   CHECK(34, "ocs_c_ptr<plain_t[]> owns an ocs_c_alloc() array", array[3].number == 3 && array[0].number == 0);

   CHECK(35, "ocs_c_ptr adds no size to the pointer", sizeof(ocs_c_ptr<char>) == sizeof(char *));
}

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"STDLIB-01", "ocs_new() and ocs_new_array() (CS-3105)",
       "objects from ocs_new() are value-initialised like calloc() and get their arguments forwarded",
       {"allocate a plain struct without arguments on a dirtied heap",
        "allocate it with member arguments, a std::vector with constructor arguments and a struct with a "
        "std::string member",
        "allocate an int array"},
       "all members and elements that get no argument are zero, the arguments arrive",
       {}, {1, 2, 3, 4, 5, 6}, test_new},
      {"STDLIB-02", "ocs_delete() and ocs_delete_array() (CS-3105)",
       "deleting runs the destructors and leaves the pointer nullptr, like sge_free()",
       {"delete one counted object twice", "delete an array of 5 counted objects twice",
        "delete through a pointer to const"},
       "the destructors run once each, the pointer is nullptr, the second call does nothing",
       {}, {10, 11, 12, 13, 14, 15, 16, 17}, test_delete},
      {"STDLIB-03", "ocs_c_alloc() (CS-3105)",
       "ocs_c_alloc() gives zeroed C memory for n elements of a trivial type",
       {"allocate one and 100 plain structs on a dirtied heap", "release them with sge_free() and free()",
        "allocate 0 elements"},
       "the memory is zero, sge_free() nulls the pointer, 0 elements give nullptr",
       {}, {20, 21, 22, 23}, test_c_alloc},
      {"STDLIB-04", "ocs_c_ptr (CS-3105)",
       "ocs_c_ptr owns memory from C library calls and releases it with free()",
       {"hold and replace strdup() results", "hold a realpath() result", "hold an ocs_c_alloc() array"},
       "the blocks are reachable while owned and released on reset() and at scope end",
       {}, {30, 31, 32, 33, 34, 35}, test_c_ptr},
   });
}
