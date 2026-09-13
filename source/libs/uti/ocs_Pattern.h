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
 * @brief Telling an expression apart from a plain string
 */

#include <array>
#include <strings.h>

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

   /// The prefix of a host name matcher; the payload is an fnmatch expression
   inline constexpr const char *MATCHER_PREFIX_HOST = "host:";
   /// The prefix of an address matcher; reserved, not supported yet
   inline constexpr const char *MATCHER_PREFIX_IP = "ip:";
   /// The prefix of an IPv6 address matcher; reserved, not supported yet
   inline constexpr const char *MATCHER_PREFIX_IP6 = "ip6:";

   /** @brief Which reserved prefix a host group member carries, if any
    *
    * The three prefixes are part of the configuration language from the first
    * release on and cannot be changed afterwards. A host must therefore not be
    * named such that its name begins with one of them.
    */
   enum class MatcherKind {
      NONE, ///< no reserved prefix; the member is not a matcher
      HOST, ///< `host:` followed by an fnmatch expression over host names
      IP,   ///< `ip:` followed by an IPv4 range; reserved, rejected on validation
      IP6   ///< `ip6:` followed by an IPv6 range; reserved, rejected on validation
   };

   /** @brief The three classes an entry of a host group member list belongs to
    *
    * The classification is total and needs no heuristic: it is a prefix
    * comparison, exactly as `@` already marks a host group.
    */
   enum class MemberClass {
      GROUP_REFERENCE, ///< `@name`, the membership of another host group
      MATCHER,         ///< a reserved prefix and a payload; describes a set of hosts
      LITERAL_HOST     ///< everything else; names exactly one host
   };

   /** @brief Which matcher prefix a member carries.
    *
    * The comparison is not case-sensitive, so `host:`, `HOST:` and every mixed
    * spelling are recognised alike.
    *
    * A member beginning with `@` never carries a prefix as far as this function
    * is concerned: no reserved prefix starts with that character, so a group
    * reference always answers #MatcherKind::NONE and the precedence of `@` over
    * every other rule holds without a test of its own.
    *
    * @param s the member to classify, may be nullptr
    * @return the prefix the member carries, #MatcherKind::NONE if it carries none
    */
   [[nodiscard]] inline MatcherKind
   matcher_kind(const char *s) noexcept {
      if (s == nullptr) {
         return MatcherKind::NONE;
      }
      if (strncasecmp(s, MATCHER_PREFIX_HOST, sizeof("host:") - 1) == 0) {
         return MatcherKind::HOST;
      }
      if (strncasecmp(s, MATCHER_PREFIX_IP6, sizeof("ip6:") - 1) == 0) {
         return MatcherKind::IP6;
      }
      if (strncasecmp(s, MATCHER_PREFIX_IP, sizeof("ip:") - 1) == 0) {
         return MatcherKind::IP;
      }
      return MatcherKind::NONE;
   }

   /** @brief Checks if a member is a matcher, that is, carries a reserved prefix.
    *
    * The reserved but unsupported prefixes answer true as well. They are matchers
    * syntactically and are refused during validation with a message saying so;
    * reading them as host names instead would report an unknown host.
    *
    * @param s the member to check, may be nullptr
    * @return true if the member carries one of the reserved prefixes
    */
   [[nodiscard]] inline bool
   is_matcher(const char *s) noexcept {
      return matcher_kind(s) != MatcherKind::NONE;
   }

   /** @brief The payload of a matcher, that is, the member without its prefix.
    *
    * The result points into @p s and is valid as long as that string is. It may
    * be the empty string; a matcher with an empty payload is refused during
    * validation, not here.
    *
    * @param s the member, may be nullptr
    * @return the payload, or nullptr if the member is not a matcher
    */
   [[nodiscard]] inline const char *
   matcher_payload(const char *s) noexcept {
      switch (matcher_kind(s)) {
         case MatcherKind::HOST:
            return s + sizeof("host:") - 1;
         case MatcherKind::IP6:
            return s + sizeof("ip6:") - 1;
         case MatcherKind::IP:
            return s + sizeof("ip:") - 1;
         case MatcherKind::NONE:
            break;
      }
      return nullptr;
   }

   /** @brief Classifies one entry of a host group member list.
    *
    * A member beginning with `@` is a group reference, and that rule takes
    * precedence over all others. A member carrying one of the reserved prefixes
    * is a matcher. Every other member is a literal host name and is subject to
    * the handling of host names unchanged, including the obligation to resolve.
    *
    * @param s the member to classify, may be nullptr
    * @return the class the member belongs to; nullptr counts as a literal host name
    */
   [[nodiscard]] inline MemberClass
   classify_member(const char *s) noexcept {
      if (is_hgroup_name(s)) {
         return MemberClass::GROUP_REFERENCE;
      }
      if (is_matcher(s)) {
         return MemberClass::MATCHER;
      }
      return MemberClass::LITERAL_HOST;
   }
}
