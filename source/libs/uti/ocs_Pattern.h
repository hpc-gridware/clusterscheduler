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

#include <array>

namespace ocs {
   /** @brief Checks if a string contains any of the special characters that are used in expressions.
    *
    * The special characters are: * ? [ ] & | ! ( )
    *
    * This function is used to determine if a string is an expression or a simple string.
    * If it is an expression, it needs to be parsed and evaluated, while a simple string
    * can be compared directly. This function is used in the expression evaluation code
    * to optimize the case where there are no expressions, which is the most common case.
    *
    * @param s the string to check
    * @return true if the string contains any of the special characters, false otherwise
   */
   [[nodiscard]] inline bool
   is_expression(const char* s) noexcept {
      // This is a bit more code than the std::strpbrk() version, but it is also faster, especially for long strings
      // without any of the special characters. The std::strpbrk() version has to check each character against all the
      // special characters, while this version just does a single array lookup per character.
#if 1
      if (!s) {
         return false;
      }

      static constexpr auto is_expr = []() constexpr {
         std::array<unsigned char, 256> t{}; // zero-initialized
         t[static_cast<unsigned char>('*')] = 1;
         t[static_cast<unsigned char>('?')] = 1;
         t[static_cast<unsigned char>('[')] = 1;
         t[static_cast<unsigned char>(']')] = 1;
         t[static_cast<unsigned char>('&')] = 1;
         t[static_cast<unsigned char>('|')] = 1;
         t[static_cast<unsigned char>('!')] = 1;
         t[static_cast<unsigned char>('(')] = 1;
         t[static_cast<unsigned char>(')')] = 1;
         return t;
      }();

      const auto* p = reinterpret_cast<const unsigned char*>(s);
      for (; *p != 0; ++p) {
         if (is_expr[*p]) {
            return true;
         }
      }
      return false;
#else
      // Slower than the code above but easier to understand
      return s && std::strpbrk(s, "*?[]&|!()") != nullptr;
#endif
   }

   /** @brief Checks if a string contains any of the special characters that are used in patterns.
    *
    * The special characters are: * ? [ ]
    *
    * This function is used to determine if a string is a pattern (fnmatch) or a simple string.
    * If it is a pattern, it needs to be matched against the target string, while a simple string
    * can be compared directly. This function is used in the pattern matching code
    * to optimize the case where there are no patterns, which is the most common case.
    *
    * @param s the string to check
    * @return true if the string contains any of the special characters, false otherwise
   */
   [[nodiscard]] inline bool
   is_pattern(const char* s) noexcept {
      if (!s) {
         return false;
      }

      static constexpr auto table = []() constexpr {
         std::array<unsigned char, 256> t{}; // all zeros
         t[static_cast<unsigned char>('*')] = 1;
         t[static_cast<unsigned char>('?')] = 1;
         t[static_cast<unsigned char>('[')] = 1;
         t[static_cast<unsigned char>(']')] = 1;
         return t;
      }();

      for (auto p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
         if (table[*p]) {
            return true;
         }
      }
      return false;
   }

   /** @brief Checks if a string is a host group name.
    *
    * A host group name is a string that starts with the character '@'. This function is used to determine
    * if a string is a host group name, which is treated differently in some contexts (e.g. in host lists).
    *
    * @param name the string to check
    * @return true if the string is a host group name, false otherwise
   */
   [[nodiscard]] inline bool
   is_hgroup_name(const char *name) noexcept {
      return name != nullptr && name[0] == '@';
   }
}
