#pragma once
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
 * @brief The value type of a host name field in the typed object model
 */

#include <cstddef>

namespace ocs::tom {

   /**
    * @brief A host name held by an element, compared like a host name
    *
    * A host field and a string field both hold a C string, but two host names
    * are equal when sge_hostcmp() says so, not when their bytes are. A Host
    * carries that rule: `==` compares like sge_hostcmp(), also against a plain
    * C string, so a host name cannot be compared byte-wise by accident.
    *
    * A Host is the size of a pointer and owns nothing. It converts implicitly
    * to `const char *`, so code that passes a host name on to a C function
    * compiles unchanged. It is made from a C string only explicitly, so a string
    * field is not set as a host field, or the other way round, by mistake.
    */
   class Host {
   public:
      /** @brief An empty host name, nullptr */
      constexpr Host() noexcept = default;

      /**
       * @brief Take a host name
       * @param name the host name, or nullptr; not copied
       */
      constexpr explicit Host(const char *name) noexcept : name_(name) {}

      /**
       * @brief The host name as C string
       * @return the host name, or nullptr if it is not set
       */
      constexpr operator const char *() const noexcept {
         return name_;
      }

      /**
       * @brief The host name as C string, for varargs and deduced templates
       * @return the host name, or nullptr if it is not set
       */
      [[nodiscard]] constexpr const char *c_str() const noexcept {
         return name_;
      }

      /**
       * @brief Whether two host names name the same host
       * @param a the first host name
       * @param b the second host name
       * @return true if both are unset, or both are set and sge_hostcmp() finds them equal
       */
      friend bool operator==(const Host a, const Host b) noexcept {
         return equal(a.name_, b.name_);
      }

      /**
       * @brief Whether a host name names the host of a C string
       * @param a the host name
       * @param b the C string, or nullptr
       * @return true if both are unset, or both are set and sge_hostcmp() finds them equal
       */
      friend bool operator==(const Host a, const char *b) noexcept {
         return equal(a.name_, b);
      }

      /**
       * @brief Whether a host name is unset
       * @param a the host name
       * @return true if it is nullptr
       */
      friend constexpr bool operator==(const Host a, std::nullptr_t) noexcept {
         return a.name_ == nullptr;
      }

   private:
      const char *name_ = nullptr;   ///< the host name, owned by the element it was read from

      /**
       * @brief Compare two host names as sge_hostcmp() does, with nullptr as a value of its own
       *
       * Out of line, so that the header of every object type does not pull in
       * the host name functions.
       *
       * @param a the first host name, or nullptr
       * @param b the second host name, or nullptr
       * @return true if both are nullptr, or neither is and sge_hostcmp() returns 0
       */
      static bool equal(const char *a, const char *b) noexcept;
   };

} // namespace ocs::tom
