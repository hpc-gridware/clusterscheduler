#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Memory allocation wrappers and process exit handling
 */

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

/** @def SGE_STRTOU_LONG32
 * @brief parse a decimal string into a 32 bit unsigned long
 */
#if defined(TARGET_64BIT)
#  define SGE_STRTOU_LONG32(S) strtoul(S, nullptr, 10)
#else
#  define SGE_STRTOU_LONG32(S) strtoul(S, nullptr, 10)
#endif

/**
 * @brief Replacement for sge_free()
 *
 * Replacement for sge_free(). Accepts nullptr pointers.
 * After a call of this macro "cp" will be nullptr.
 *
 * @param ... pointer to a memory block
 *
 * @return nullptr
 *
 * @see #sge_free
 */
#define FREE(x) \
   if (x != nullptr) { \
      free((char *)x); \
      x = nullptr; \
   }

char *sge_malloc(size_t size);

void *sge_realloc(void *ptr, size_t size, int do_abort);

void sge_free(void *cp);

void *ocs_c_calloc(size_t n, size_t size);

/**
 * @brief Allocate and value-initialise one object with `new` - the replacement for `sge_malloc(sizeof(T))`
 *
 * Use it for every object that is owned by C++ code. The object is value-initialised: without arguments
 * every member that has no default member initialiser and no constructor is zero, as with `calloc()`.
 * Release it with ocs_delete(), or hand it to a `std::unique_ptr` right away.
 *
 * Aggregates (plain structs without constructors) are initialised with braces, `new T{args...}`, so a
 * struct can be filled member by member and a narrowing conversion is a compile error. Classes with
 * constructors are initialised with parentheses, `new T(args...)`, like `std::make_unique()`, so an
 * `std::initializer_list` constructor does not take over a call that meant another constructor
 * (`ocs_new<std::vector<int>>(3, 0)` makes three elements, not two). Without arguments both forms
 * value-initialise.
 *
 * Out of memory throws `std::bad_alloc`. Nobody catches it, so the process terminates, as sge_malloc()
 * aborts today; a `nullptr` check after the call is dead code.
 *
 * @tparam T type of the object, not an array type (see ocs_new_array())
 * @tparam Args types of the constructor or member arguments
 * @param args constructor arguments, or the members of an aggregate in declaration order
 * @return pointer to the new object, never `nullptr`
 */
template <typename T, typename... Args>
[[nodiscard]] T *
ocs_new(Args &&...args) {
   static_assert(!std::is_array_v<T>, "ocs_new() makes one object, use ocs_new_array() for an array");
   if constexpr (std::is_aggregate_v<T>) {
      return new T{std::forward<Args>(args)...};
   } else {
      return new T(std::forward<Args>(args)...);
   }
}

/**
 * @brief Allocate and value-initialise an array with `new[]` - the replacement for `calloc(n, sizeof(T))`
 *
 * Prefer `std::vector<T>` where the array has a local owner. This is for the places that keep a raw
 * array pointer in a struct until they are converted. Every element is value-initialised (zero for
 * trivial types). Release the array with ocs_delete_array(), never with ocs_delete() or `free()`.
 *
 * Out of memory, or a byte size that does not fit into `size_t`, throws (`std::bad_alloc` or
 * `std::bad_array_new_length`).
 *
 * @tparam T element type
 * @param n number of elements; 0 gives a valid pointer to an empty array
 * @return pointer to the first element, never `nullptr`
 */
template <typename T>
[[nodiscard]] T *
ocs_new_array(const size_t n) {
   static_assert(!std::is_array_v<T>, "give the element type, not an array type");
   return new T[n]();
}

/**
 * @brief Destroy an object made with ocs_new() (or `new`) and set the pointer to `nullptr`
 *
 * The replacement for sge_free() of a C++ owned object: like sge_free() it accepts `nullptr` and leaves
 * the pointer `nullptr`, so a second call or a later check of the pointer is safe. The destructor runs.
 * Never use it for memory from sge_malloc(), `malloc()`, `strdup()` or ocs_c_alloc() - that is released
 * with sge_free() or `free()`.
 *
 * Deleting through `void *` and deleting an incomplete type do not call the destructor; both are
 * rejected at compile time.
 *
 * @tparam T type of the object
 * @param p pointer to the object or `nullptr`; `nullptr` afterwards
 */
template <typename T>
void
ocs_delete(T *&p) noexcept {
   static_assert(!std::is_void_v<T>, "ocs_delete() of a void pointer does not run a destructor");
   static_assert(sizeof(T) > 0, "ocs_delete() of an incomplete type does not run its destructor");
   delete p;
   p = nullptr;
}

/**
 * @brief Destroy an array made with ocs_new_array() (or `new[]`) and set the pointer to `nullptr`
 *
 * Like ocs_delete(), for arrays: accepts `nullptr`, runs the destructor of every element and leaves the
 * pointer `nullptr`.
 *
 * @tparam T element type
 * @param p pointer to the first element or `nullptr`; `nullptr` afterwards
 */
template <typename T>
void
ocs_delete_array(T *&p) noexcept {
   static_assert(!std::is_void_v<T>, "ocs_delete_array() of a void pointer does not run a destructor");
   static_assert(sizeof(T) > 0, "ocs_delete_array() of an incomplete type does not run its destructor");
   delete[] p;
   p = nullptr;
}

/**
 * @brief The types ocs_c_alloc() accepts: no constructor and no destructor has to run
 *
 * Named, so that a module test can show which types ocs_c_alloc() rejects without a line that does not
 * compile.
 *
 * @tparam T type to check
 */
template <typename T>
concept ocs_c_allocatable = std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>;

/**
 * @brief Allocate zeroed C memory for `n` objects of a trivial type - the typed `sge_malloc(sizeof(T))`
 *
 * For structs that stay C-allocated for now: memory that is released with sge_free() or `free()`, or that
 * is handed to C code that frees it. The type must be trivially default constructible and trivially
 * destructible, because no constructor and no destructor ever runs; a struct with a `std::string`,
 * `ElementPtr` or other owning member fails to compile and has to use ocs_new() instead.
 *
 * The memory comes from `calloc()`, so it is zero like the memory of `calloc()`, and large blocks come
 * zeroed from the system without being written. The byte size `n * sizeof(T)` is checked for overflow.
 * Out of memory and overflow abort the process, like sge_malloc().
 *
 * @tparam T trivial element type
 * @param n number of elements, default 1
 * @return pointer to `n` zeroed elements, `nullptr` for `n == 0` (like sge_malloc(0))
 */
template <typename T>
[[nodiscard]] T *
ocs_c_alloc(const size_t n = 1) {
   static_assert(ocs_c_allocatable<T>,
                 "ocs_c_alloc() runs no constructor and no destructor, use ocs_new() for this type");
   static_assert(alignof(T) <= alignof(std::max_align_t), "calloc() does not guarantee this alignment");
   return static_cast<T *>(ocs_c_calloc(n, sizeof(T)));
}

/**
 * @brief Deleter that releases memory with `free()`, for ocs_c_ptr
 */
struct ocs_free_deleter {
   /**
    * @brief Release a block from `malloc()`, `calloc()`, `realloc()` or `strdup()`
    * @param p pointer to the block or `nullptr`
    */
   void
   operator()(void *p) const noexcept {
      free(p);
   }
};

/**
 * @brief Owning pointer for memory that a C library call returns and that must be released with `free()`
 *
 * For `realpath(path, nullptr)`, `getline()`, `asprintf()`, `scandir()`, `strdup()` results that cannot
 * become a `std::string` yet, and the like: the `free()` lives in the deleter, the owner releases it on
 * every path out of the scope. Also fits memory from ocs_c_alloc() and sge_malloc(). Never use it for
 * memory from `new`; use `std::unique_ptr<T>` there.
 *
 * Example: `ocs_c_ptr<char> real{realpath(path, nullptr)};`
 *
 * @tparam T type of the pointed-to memory, `T[]` for an array
 */
template <typename T>
using ocs_c_ptr = std::unique_ptr<T, ocs_free_deleter>;

const char *sge_getenv(const char *env_str);

int sge_putenv(const char *var);

int sge_setenv(const char *name, const char *value);

void sge_unsetenv(const char *name);
