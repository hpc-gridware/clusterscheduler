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
 * @brief Module test: the C++ language features and the exception handling the typed element classes need (CS-3039)
 *
 * The planned typed element and list classes move field type, field ownership and
 * field position into the compile run, and report errors the compiler cannot catch
 * as exceptions. This test answers per build host whether its compiler and runtime
 * can do that, before any of it is written:
 *
 * - The first part is compile time. It uses every language and library feature
 *   the classes will use, in the way they will use it, on a small model of a field
 *   enum, a tag type, a handle and a list. A compiler that lacks a feature fails to
 *   build this file - that is the answer for its host.
 * - The second part is run time: exceptions of our own classes inside the
 *   executable, and across a module loaded with dlopen(RTLD_LOCAL) and linked like
 *   a spooling plugin. The tool chain links libstdc++ statically, so the module and
 *   the executable each carry their own copy of the type information.
 *
 * Features that are deliberately not used yet are reported from the feature-test
 * macros, not required.
 */

#include <array>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <version>

#include "ocs_module_test.h"
#include "test_uti_cpp_features.h"

// ---------------------------------------------------------------------------
// compile-time part: a model of the typed element classes
// ---------------------------------------------------------------------------

namespace probe_model {

   /// the fields of the model type XX, a name space of their own like the generated field enums
   enum : int {
      XX_LOWERBOUND = 100,
      XX_id = XX_LOWERBOUND,   ///< a hashed number
      XX_name,                 ///< a string
      XX_slots,                ///< a number
      XX_UPPERBOUND
   };

   /// the fields of a second model type YY
   enum : int {
      YY_LOWERBOUND = 200,
      YY_host = YY_LOWERBOUND,   ///< a string
      YY_UPPERBOUND
   };

   /** @brief Tag type of XX: carries the base and the number of fields */
   struct XX {
      static constexpr int base = XX_LOWERBOUND;            ///< first field
      static constexpr int count = XX_UPPERBOUND - XX_LOWERBOUND;   ///< number of fields
   };

   /** @brief Tag type of YY */
   struct YY {
      static constexpr int base = YY_LOWERBOUND;            ///< first field
      static constexpr int count = YY_UPPERBOUND - YY_LOWERBOUND;   ///< number of fields
   };

   /// a tag type: a type with a base and a positive number of fields
   template <typename T>
   concept ObjectTag = requires {
      { T::base } -> std::convertible_to<int>;
      { T::count } -> std::convertible_to<int>;
   } && (T::count > 0);

   static_assert(ObjectTag<XX> && ObjectTag<YY>);
   static_assert(!ObjectTag<int>);

   /// the value kinds of the model
   enum class Kind { number, string };

   /** @brief One entry of a field table */
   struct FieldInfo {
      int nm;                  ///< the field
      std::string_view name;   ///< its name
      Kind kind;               ///< its value kind
      bool hashed;             ///< whether lookups by it are hashed
   };

   /**
    * @brief Build the field table of XX at compile time and validate it
    *
    * consteval: calling it at run time does not compile. A table whose fields are
    * not contiguous from the base is a compile error.
    *
    * @return the table, in field order
    */
   consteval std::array<FieldInfo, XX::count> make_xx_table() {
      std::array<FieldInfo, XX::count> t{{
         {.nm = XX_id, .name = "XX_id", .kind = Kind::number, .hashed = true},
         {.nm = XX_name, .name = "XX_name", .kind = Kind::string, .hashed = false},
         {.nm = XX_slots, .name = "XX_slots", .kind = Kind::number, .hashed = false},
      }};
      for (int i = 0; i < XX::count; ++i) {
         if (t[i].nm != XX::base + i) {
            throw "field table is not contiguous";   // not a constant expression: compile error
         }
      }
      return t;
   }

   /// the field table of XX, a constant
   constexpr auto xx_table = make_xx_table();

   /**
    * @brief Find a field by name in the table, at compile time
    * @param name the field name
    * @return the field, or -1
    */
   consteval int field_by_name(std::string_view name) {
      for (const auto &f : xx_table) {
         if (f.name == name) {
            return f.nm;
         }
      }
      return -1;
   }

   static_assert(xx_table[1].name == "XX_name");
   static_assert(field_by_name("XX_slots") == XX_slots);
   static_assert(field_by_name("XX_nothing") == -1);

   /// traits of a field: object type, value type, hashed; only the specialisations exist
   template <auto F>
   struct FieldTraits;

   /** @brief Traits of XX_id */
   template <>
   struct FieldTraits<XX_id> {
      using Object = XX;                   ///< the type it belongs to
      using Value = uint32_t;              ///< its value type
      static constexpr bool hashed = true; ///< lookups are hashed
   };

   /** @brief Traits of XX_name */
   template <>
   struct FieldTraits<XX_name> {
      using Object = XX;                    ///< the type it belongs to
      using Value = const char *;           ///< its value type
      static constexpr bool hashed = false; ///< lookups are linear
   };

   /** @brief Traits of XX_slots */
   template <>
   struct FieldTraits<XX_slots> {
      using Object = XX;                    ///< the type it belongs to
      using Value = uint32_t;               ///< its value type
      static constexpr bool hashed = false; ///< lookups are linear
   };

   /** @brief Traits of YY_host */
   template <>
   struct FieldTraits<YY_host> {
      using Object = YY;                    ///< the type it belongs to
      using Value = const char *;           ///< its value type
      static constexpr bool hashed = false; ///< lookups are linear
   };

   /** @brief Stand-in for the untyped element structure of the old interface */
   struct RawElem {
      int base;                     ///< the type, as the base of its fields
      uint32_t numbers[4];          ///< number values, by position
      const char *strings[4];       ///< string values, by position
      int hash_updates;             ///< how often a setter maintained a hash
   };

   /**
    * @brief A non-owning typed handle over a RawElem
    *
    * Converts implicitly to the raw pointer (back into the old interface); the way
    * in is an explicit constructor that checks the type.
    *
    * @tparam T the tag type
    */
   template <ObjectTag T>
   class Handle {
      RawElem *ep_{nullptr};   ///< the element, not owned

   public:
      /** @brief An empty handle */
      constexpr Handle() = default;

      /**
       * @brief The checked way in from a raw pointer
       * @param ep the element, or nullptr for an empty handle
       * @throws ocs::probe::LogicError if the element is of another type
       */
      constexpr explicit Handle(RawElem *ep) : ep_(ep) {
         if (ep != nullptr && ep->base != T::base) {
            throw ocs::probe::LogicError("element is not of the handle's type");
         }
      }

      /**
       * @brief Back into the old interface
       * @return the raw pointer
       */
      constexpr operator RawElem *() const { return ep_; }

      /**
       * @brief The raw pointer, where no implicit conversion happens (varargs, deduced templates)
       * @return the raw pointer
       */
      [[nodiscard]] constexpr RawElem *raw() const { return ep_; }

      /**
       * @brief Read a field: the field must belong to T, the value type comes from the traits
       * @tparam F the field
       * @return the value
       */
      template <auto F>
         requires std::same_as<typename FieldTraits<F>::Object, T>
      [[nodiscard]] typename FieldTraits<F>::Value get() const {
         constexpr int pos = F - T::base;   // a constant
         if constexpr (std::is_same_v<typename FieldTraits<F>::Value, uint32_t>) {
            return ep_->numbers[pos];
         } else {
            return ep_->strings[pos];
         }
      }

      /**
       * @brief Write a field; hash maintenance exists only for hashed fields
       * @tparam F the field
       * @param v the new value
       * @return nothing
       */
      template <auto F>
         requires std::same_as<typename FieldTraits<F>::Object, T>
      void set(typename FieldTraits<F>::Value v) {
         constexpr int pos = F - T::base;
         if constexpr (std::is_same_v<typename FieldTraits<F>::Value, uint32_t>) {
            ep_->numbers[pos] = v;
         } else {
            ep_->strings[pos] = v;
         }
         if constexpr (FieldTraits<F>::hashed) {
            ++ep_->hash_updates;
         }
      }
   };

   /// whether get<F>() compiles on handle type H
   template <typename H, auto F>
   concept CanGet = requires(H h) { h.template get<F>(); };

   static_assert(CanGet<Handle<XX>, XX_id>);
   static_assert(!CanGet<Handle<YY>, XX_id>, "a field of a foreign type must not compile");
   static_assert(std::same_as<decltype(std::declval<Handle<XX>>().get<XX_name>()), const char *>);
   static_assert(std::is_convertible_v<Handle<XX>, RawElem *>, "handle to raw pointer is implicit");
   static_assert(!std::is_convertible_v<RawElem *, Handle<XX>>, "raw pointer to handle is never implicit");
   static_assert(std::is_constructible_v<Handle<XX>, RawElem *>, "raw pointer to handle is explicit");

   /**
    * @brief Old signature of a function
    * @param ep an element
    * @return 1
    */
   constexpr int pick([[maybe_unused]] RawElem *ep) { return 1; }

   /**
    * @brief New signature of the same function
    * @param job an element of type XX
    * @return 2
    */
   constexpr int pick([[maybe_unused]] Handle<XX> job) { return 2; }

   static_assert(pick(Handle<XX>{}) == 2, "a handle picks the new signature");
   static_assert(pick(static_cast<RawElem *>(nullptr)) == 1, "a raw pointer picks the old signature");
   static_assert(pick(nullptr) == 1, "nullptr picks the old signature, the checked constructor is not implicit");

   /**
    * @brief A typed view over a range of elements, iterated with range-for
    * @tparam T the tag type
    */
   template <ObjectTag T>
   class ListView {
      std::span<RawElem> elems_;   ///< the elements, not owned

   public:
      /** @brief Iterator that yields handles */
      class iterator {
         RawElem *p_{nullptr};   ///< current element

      public:
         using value_type = Handle<T>;         ///< what it yields
         using difference_type = std::ptrdiff_t;   ///< distance type

         /** @brief A singular iterator */
         iterator() = default;

         /**
          * @brief An iterator at an element
          * @param p the element
          */
         explicit iterator(RawElem *p) : p_(p) {}

         /**
          * @brief The current element
          * @return a handle on it
          */
         Handle<T> operator*() const { return Handle<T>{p_}; }

         /**
          * @brief Advance
          * @return this iterator
          */
         iterator &operator++() {
            ++p_;
            return *this;
         }

         /**
          * @brief Advance, returning the old position
          * @return the old position
          */
         iterator operator++(int) {
            iterator old = *this;
            ++p_;
            return old;
         }

         /**
          * @brief Compare positions
          * @param other the other iterator
          * @return true at the same position
          */
         bool operator==(const iterator &other) const = default;
      };

      /**
       * @brief A view over elements
       * @param elems the elements
       */
      explicit ListView(std::span<RawElem> elems) : elems_(elems) {}

      /**
       * @brief First element
       * @return iterator at the first element
       */
      [[nodiscard]] iterator begin() const { return iterator{elems_.data()}; }

      /**
       * @brief Past the last element
       * @return iterator past the last element
       */
      [[nodiscard]] iterator end() const { return iterator{elems_.data() + elems_.size()}; }

      /**
       * @brief Find the first element whose number field has a value
       * @tparam F the field
       * @param value the value
       * @return the element, or an empty optional - absence is no error
       */
      template <auto F>
         requires std::same_as<typename FieldTraits<F>::Object, T>
      [[nodiscard]] std::optional<Handle<T>> find_first(typename FieldTraits<F>::Value value) const {
         for (Handle<T> h : *this) {
            if (h.template get<F>() == value) {
               return h;
            }
         }
         return std::nullopt;
      }
   };

   static_assert(std::input_iterator<ListView<XX>::iterator>);

} // namespace probe_model

using namespace probe_model;

// ---------------------------------------------------------------------------
// run-time part
// ---------------------------------------------------------------------------

/**
 * @brief Print the compiler and the feature-test macros of the features not used yet
 * @return nothing
 */
static void report_features() {
#if defined(__clang__)
   printf("compiler: clang %s\n", __clang_version__);
#elif defined(__GNUC__)
   printf("compiler: gcc %s\n", __VERSION__);
#else
   printf("compiler: unknown\n");
#endif
   printf("__cplusplus: %ld\n", static_cast<long>(__cplusplus));

   struct Feature {
      const char *name;   ///< the macro
      long have;          ///< its value, 0 if undefined
      long need;          ///< the value that means the full feature
   };
   const Feature features[] = {
#ifdef __cpp_nontype_template_args
      {"__cpp_nontype_template_args (class types as template parameters)", __cpp_nontype_template_args, 201911L},
#else
      {"__cpp_nontype_template_args (class types as template parameters)", 0, 201911L},
#endif
#ifdef __cpp_if_consteval
      {"__cpp_if_consteval", __cpp_if_consteval, 202106L},
#else
      {"__cpp_if_consteval", 0, 202106L},
#endif
#ifdef __cpp_lib_expected
      {"__cpp_lib_expected", __cpp_lib_expected, 202202L},
#else
      {"__cpp_lib_expected", 0, 202202L},
#endif
#ifdef __cpp_explicit_this_parameter
      {"__cpp_explicit_this_parameter (deducing this)", __cpp_explicit_this_parameter, 202110L},
#else
      {"__cpp_explicit_this_parameter (deducing this)", 0, 202110L},
#endif
   };
   for (const auto &f : features) {
      printf("info: %s = %ld, %s\n", f.name, f.have, f.have >= f.need ? "available" : "not available");
   }
}

/** @brief CPP-01: the compile-time model also works at run time */
static void test_model() {
   RawElem elems[3] = {
      {XX::base, {1, 0, 4, 0}, {nullptr, "first", nullptr, nullptr}, 0},
      {XX::base, {2, 0, 8, 0}, {nullptr, "second", nullptr, nullptr}, 0},
      {XX::base, {3, 0, 16, 0}, {nullptr, "third", nullptr, nullptr}, 0},
   };
   RawElem other = {YY::base, {0, 0, 0, 0}, {"host", nullptr, nullptr, nullptr}, 0};

   Handle<XX> h{&elems[1]};
   CHECK(1, "get<F>() reads the value at the constant position with the type from the traits",
         h.get<XX_id>() == 2 && strcmp(h.get<XX_name>(), "second") == 0 && h.get<XX_slots>() == 8);

   h.set<XX_slots>(9);
   h.set<XX_id>(7);
   CHECK(2, "set<F>() writes, and only the hashed field maintains a hash",
         elems[1].numbers[2] == 9 && elems[1].numbers[0] == 7 && elems[1].hash_updates == 1);

   bool thrown = false;
   try {
      [[maybe_unused]] const Handle<XX> wrong{&other};
   } catch (const ocs::probe::LogicError &) {
      thrown = true;
   }
   CHECK(3, "the checked constructor throws a LogicError for an element of another type", thrown);

   ListView<XX> list{std::span<RawElem>{elems}};
   uint32_t sum = 0;
   int n = 0;
   for (Handle<XX> e : list) {
      sum += e.get<XX_slots>();
      ++n;
   }
   CHECK(4, "range-for over the view yields one handle per element", n == 3 && sum == 4 + 9 + 16);

   const auto found = list.find_first<XX_id>(3);
   const auto missing = list.find_first<XX_id>(42);
   CHECK(5, "a lookup returns the element, or an empty optional when nothing matches",
         found.has_value() && strcmp(found->get<XX_name>(), "third") == 0 && !missing.has_value());

   const auto [first, last] = std::pair{*list.begin(), Handle<XX>{&elems[2]}};
   RawElem *raw = first;   // implicit, back into the old interface
   CHECK(6, "structured bindings and the implicit conversion to the raw pointer",
         raw == &elems[0] && last.raw() == &elems[2] && field_by_name("XX_id") == XX_id);
}

/** @brief CPP-02: exceptions of our own classes inside the executable */
static void test_local_exceptions() {
   bool by_derived = false;
   try {
      throw ocs::probe::NotFound("job 17 not found", 17);
   } catch (const ocs::probe::NotFound &e) {
      by_derived = e.key() == 17 && strcmp(e.what(), "job 17 not found") == 0;
   }
   CHECK(7, "a NotFound is caught as NotFound with message and detail", by_derived);

   bool by_base = false;
   try {
      throw ocs::probe::NotFound("queue missing", 1);
   } catch (const std::runtime_error &e) {
      by_base = strcmp(e.what(), "queue missing") == 0;
   }
   CHECK(8, "a NotFound is caught as std::runtime_error", by_base);

   bool logic = false;
   try {
      throw ocs::probe::LogicError("absent field read");
   } catch (const std::logic_error &e) {
      logic = strcmp(e.what(), "absent field read") == 0;
   }
   CHECK(9, "a LogicError is caught as std::logic_error", logic);
}

/**
 * @brief The path of the test module, next to this executable
 *
 * Portable: derived from argv[0] if it holds a directory, else from where the
 * dynamic linker found the executable.
 *
 * @return the path, or the file name alone
 */
static std::string module_path() {
   std::string exe = ocs::module_test::state().path;
   if (exe.find('/') == std::string::npos) {
      Dl_info info{};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      const auto self = reinterpret_cast<void *>(&module_path);
#pragma GCC diagnostic pop
      if (dladdr(self, &info) != 0 && info.dli_fname != nullptr) {
         exe = info.dli_fname;
      }
   }
   const auto slash = exe.rfind('/');
   if (slash == std::string::npos) {
      return TEST_UTI_CPP_FEATURES_MODULE;
   }
   return exe.substr(0, slash + 1) + TEST_UTI_CPP_FEATURES_MODULE;
}

/**
 * @brief Callback for the module: throws a NotFound
 * @param arg the message
 * @return never returns normally
 */
static void throw_not_found(void *arg) {
   throw ocs::probe::NotFound(static_cast<const char *>(arg), TEST_UTI_CPP_FEATURES_KEY);
}

/**
 * @brief Look a function up in the module
 * @tparam F the function pointer type
 * @param handle the module
 * @param name the symbol
 * @return the function, or nullptr
 */
template <typename F>
static F lookup(void *handle, const char *name) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   return reinterpret_cast<F>(dlsym(handle, name));
#pragma GCC diagnostic pop
}

/** @brief CPP-03: exceptions across a module loaded with RTLD_LOCAL */
static void test_module_exceptions() {
   const std::string path = module_path();
   void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
   if (handle == nullptr) {
      printf("dlopen %s: %s\n", path.c_str(), dlerror());
   }
   const auto do_throw = handle != nullptr ? lookup<test_uti_cpp_features_throw_func>(handle, "test_uti_cpp_features_throw") : nullptr;
   const auto do_call = handle != nullptr ? lookup<test_uti_cpp_features_call_func>(handle, "test_uti_cpp_features_call") : nullptr;
   const auto do_catch = handle != nullptr ? lookup<test_uti_cpp_features_catch_func>(handle, "test_uti_cpp_features_catch") : nullptr;
   const auto do_capture = handle != nullptr ? lookup<test_uti_cpp_features_capture_func>(handle, "test_uti_cpp_features_capture") : nullptr;
   CHECK(10, "the module is loaded with RTLD_LOCAL and has its four functions",
         do_throw != nullptr && do_call != nullptr && do_catch != nullptr && do_capture != nullptr);
   if (do_throw == nullptr || do_call == nullptr || do_catch == nullptr || do_capture == nullptr) {
      return;
   }

   bool from_module = false;
   try {
      do_throw(static_cast<int>(ocs::probe::ThrowKind::not_found), "thrown in the module");
   } catch (const ocs::probe::NotFound &e) {
      from_module = e.key() == TEST_UTI_CPP_FEATURES_KEY && strcmp(e.what(), "thrown in the module") == 0;
   } catch (...) {
   }
   CHECK(11, "a NotFound thrown in the module is caught in the executable as NotFound", from_module);

   bool logic_from_module = false;
   bool standard_from_module = false;
   try {
      do_throw(static_cast<int>(ocs::probe::ThrowKind::logic), "logic in the module");
   } catch (const std::logic_error &e) {
      logic_from_module = strcmp(e.what(), "logic in the module") == 0;
   } catch (...) {
   }
   try {
      do_throw(static_cast<int>(ocs::probe::ThrowKind::standard), "standard in the module");
   } catch (const std::exception &e) {
      standard_from_module = strcmp(e.what(), "standard in the module") == 0;
   } catch (...) {
   }
   CHECK(12, "a LogicError and a std::runtime_error from the module are caught by their standard base",
         logic_from_module && standard_from_module);

   bool through_module = false;
   char message[] = "thrown in the executable";
   try {
      do_call(throw_not_found, message);
   } catch (const ocs::probe::NotFound &e) {
      through_module = strcmp(e.what(), message) == 0;
   } catch (...) {
   }
   CHECK(13, "a NotFound of the executable passes through the frames of the module", through_module);

   char buf[128] = "";
   const int caught = do_catch(throw_not_found, message, buf, sizeof(buf));
   CHECK(14, "the module catches a NotFound of the executable as NotFound",
         caught == static_cast<int>(ocs::probe::Caught::not_found) && strcmp(buf, message) == 0);

   std::exception_ptr ep;
   do_capture(&ep, "captured in the module");
   bool rethrown = false;
   try {
      if (ep) {
         std::rethrow_exception(ep);
      }
   } catch (const ocs::probe::NotFound &e) {
      rethrown = e.key() == TEST_UTI_CPP_FEATURES_KEY && strcmp(e.what(), "captured in the module") == 0;
   } catch (...) {
   }
   ep = nullptr;
   CHECK(15, "a std::exception_ptr from the module is rethrown and caught as NotFound", rethrown);

   dlclose(handle);
}

int main(int argc, char *argv[]) {
   report_features();
   return ocs::module_test::run(argc, argv, {
      {"CPP-01", "the model of the typed classes",
       "the language features the typed element classes use compile on this host and behave at run time",
       {"build field traits, a consteval table, a handle and a list view of a model type",
        "read and write fields, iterate, look up, convert to the raw pointer",
        "enter the handle with an element of another type"},
       "values and hash maintenance are right, the wrong type throws a LogicError",
       {}, {1, 2, 3, 4, 5, 6}, test_model},
      {"CPP-02", "exceptions inside the executable",
       "our exception classes are caught by their own type and by the standard base, with message and detail",
       {"throw a NotFound and a LogicError", "catch them by derived and by base type"},
       "every exception is caught with its message",
       {}, {7, 8, 9}, test_local_exceptions},
      {"CPP-03", "exceptions across a loaded module",
       "exceptions cross the boundary of a module loaded with RTLD_LOCAL and linked like a spooling plugin",
       {"load the module", "throw in the module and catch in the executable",
        "throw in the executable through and into the module", "hand an exception over as std::exception_ptr"},
       "every exception is caught by its type on the other side",
       {}, {10, 11, 12, 13, 14, 15}, test_module_exceptions},
   });
}
