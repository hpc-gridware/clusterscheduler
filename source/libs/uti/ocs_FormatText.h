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
 * @brief A string argument for std::format that is padded by its byte count when it is ASCII
 *
 * For a string in a replacement field with a width, libstdc++ computes the display width of the string by
 * walking it as Unicode grapheme clusters. That walk dominated the rendering of the stored procedures, and
 * almost every padded string there is ASCII, whose display width is its byte count. Wrapping the argument in
 * ocs::ascii_width() pads it by size() when every byte is ASCII and leaves anything else to std::format, so
 * the output is the same byte for byte:
 *
 * @code
 * os << std::format("{:<{}} {}", ocs::ascii_width("job_number:"), left_width, job_id);
 * @endcode
 *
 * Only fill, alignment, width, precision and the type `s` are supported; width and precision as digits, as `{}`
 * or as `{n}`. Any other part of a format spec is a compile error for a format string known at compile time, and
 * a std::format_error otherwise.
 */

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>

namespace ocs {
   /** @brief A string to be formatted, see ocs::ascii_width() */
   struct FormatText {
      std::string_view text; ///< the string; not owned
   };

   /**
    * @brief Wrap a string for a padded replacement field
    *
    * @param text the string
    * @return the wrapper, referring to @p text
    */
   constexpr FormatText
   ascii_width(const std::string_view text) {
      return FormatText{text};
   }

   /**
    * @brief Wrap a C string for a padded replacement field
    *
    * The string is measured here, before the format string decides whether it uses the argument at all. Some
    * callers pass nullptr with a format string that never refers to it, which std::format accepts since it never
    * reads the pointer. nullptr is therefore taken as the empty string; a field which does use it prints nothing,
    * where std::format would crash.
    *
    * @param text the string, or nullptr
    * @return the wrapper, referring to @p text
    */
   constexpr FormatText
   ascii_width(const char *text) {
      return FormatText{text != nullptr ? std::string_view(text) : std::string_view()};
   }

   /**
    * @brief Wrap a string for a padded replacement field
    *
    * @param text the string; must outlive the formatting
    * @return the wrapper, referring to @p text
    */
   inline FormatText
   ascii_width(const std::string &text) {
      return FormatText{std::string_view(text)};
   }
}

/**
 * @brief std::format support for ocs::FormatText
 */
template<>
struct std::formatter<ocs::FormatText, char> {
private:
   /** @brief A width or a precision: given as digits, or taken from an argument (`{}`, `{n}`) */
   struct Count {
      std::size_t value = 0;  ///< the count given as digits
      std::size_t arg = 0;    ///< the argument id when taken from an argument
      bool dynamic = false;   ///< true when taken from an argument
      bool given = false;     ///< true when the spec has the count at all
   };

   char fill_ = ' ';  ///< fill character, ASCII only
   char align_ = '<'; ///< '<', '>' or '^'; strings are left aligned by default
   Count width_;      ///< the minimum field width
   Count precision_;  ///< the maximum number of characters taken from the string

   /**
    * @brief Whether a character is an alignment
    *
    * @param c the character
    * @return true for '<', '>' and '^'
    */
   static constexpr bool
   is_align(const char c) {
      return c == '<' || c == '>' || c == '^';
   }

   /**
    * @brief Whether a character is a decimal digit
    *
    * @param c the character
    * @return true for '0' to '9'
    */
   static constexpr bool
   is_digit(const char c) {
      return c >= '0' && c <= '9';
   }

   /**
    * @brief Whether a string is ASCII only
    *
    * @param s the string
    * @return true when every byte is below 0x80
    */
   static constexpr bool
   is_ascii(const std::string_view s) {
      return std::ranges::all_of(s, [](const char c) { return static_cast<unsigned char>(c) < 0x80; });
   }

   /**
    * @brief Parse a count given as digits, as `{}` or as `{n}`
    *
    * @param ctx the parse context, for the argument ids
    * @param it the position of the count; moved behind it
    * @param end the end of the spec
    * @param count receives the count
    * @param what "width" or "precision", for the error
    * @throw std::format_error for a malformed argument reference
    */
   static constexpr void
   parse_count(std::format_parse_context &ctx, std::format_parse_context::iterator &it,
               const std::format_parse_context::iterator end, Count &count, const char *what) {
      if (it != end && *it == '{') {
         ++it;
         if (it != end && *it == '}') {
            count.arg = ctx.next_arg_id();
         } else {
            const auto first = it;
            std::size_t id = 0;
            while (it != end && is_digit(*it)) {
               id = id * 10 + static_cast<std::size_t>(*it - '0');
               ++it;
            }
            if (it == first || it == end || *it != '}') {
               throw std::format_error(what);
            }
            ctx.check_arg_id(id);
            count.arg = id;
         }
         ++it;
         count.dynamic = true;
         count.given = true;
      } else if (it != end && is_digit(*it)) {
         while (it != end && is_digit(*it)) {
            count.value = count.value * 10 + static_cast<std::size_t>(*it - '0');
            ++it;
         }
         count.given = true;
      }
   }

   /**
    * @brief The value of a count, taken from its argument if it has one
    *
    * @param count the count
    * @param ctx the format context
    * @return the count
    * @throw std::format_error when the argument is not an integer or is negative, as with std::format
    */
   template<typename FormatContext>
   static std::size_t
   resolve(const Count &count, FormatContext &ctx) {
      if (!count.dynamic) {
         return count.value;
      }
      return std::visit_format_arg([](const auto value) -> std::size_t {
         using T = std::decay_t<decltype(value)>;
         if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>) {
            if constexpr (std::is_signed_v<T>) {
               if (value < 0) {
                  throw std::format_error("ocs::FormatText: negative width or precision");
               }
            }
            return static_cast<std::size_t>(value);
         } else {
            throw std::format_error("ocs::FormatText: width or precision is not an integer");
         }
      }, ctx.arg(count.arg));
   }

   /**
    * @brief Write a string as it is, as one block
    *
    * Copying character by character to the output iterator of a format context costs a call into the format
    * sink per character. The string_view formatter without a spec hands the string to the sink in one piece.
    *
    * @param s the string
    * @param ctx the format context; its output position is moved behind the string
    */
   template<typename FormatContext>
   static void
   write(const std::string_view s, FormatContext &ctx) {
      std::formatter<std::string_view, char> plain;
      std::format_parse_context no_spec{std::string_view{}};
      plain.parse(no_spec);
      ctx.advance_to(plain.format(s, ctx));
   }

   /**
    * @brief Write the fill character a number of times, in blocks
    *
    * @param count how many fill characters
    * @param ctx the format context; its output position is moved behind them
    */
   template<typename FormatContext>
   void
   write_fill(std::size_t count, FormatContext &ctx) const {
      char block[64];
      std::fill_n(block, sizeof(block), fill_);
      while (count > 0) {
         const std::size_t n = std::min(count, sizeof(block));
         write(std::string_view(block, n), ctx);
         count -= n;
      }
   }

public:
   /**
    * @brief Parse the format spec: `[[fill]align][width][.precision][s]`
    *
    * Width and precision are given as digits, as `{}` or as `{n}`.
    *
    * @param ctx the parse context, positioned after the colon of the replacement field
    * @return the position of the closing brace
    * @throw std::format_error for any other spec
    */
   constexpr std::format_parse_context::iterator
   parse(std::format_parse_context &ctx) {
      auto it = ctx.begin();
      const auto end = ctx.end();

      if (it != end && it + 1 != end && is_align(*(it + 1)) && *it != '{' && *it != '}') {
         if (static_cast<unsigned char>(*it) >= 0x80) {
            throw std::format_error("ocs::FormatText supports an ASCII fill character only");
         }
         fill_ = *it;
         align_ = *(it + 1);
         it += 2;
      } else if (it != end && is_align(*it)) {
         align_ = *it;
         ++it;
      }

      // a width starting with 0 would be zero padding, which strings do not have
      if (it != end && *it != '0') {
         parse_count(ctx, it, end, width_, "ocs::FormatText: invalid width");
      }
      if (it != end && *it == '.') {
         ++it;
         parse_count(ctx, it, end, precision_, "ocs::FormatText: invalid precision");
         if (!precision_.given) {
            throw std::format_error("ocs::FormatText: a precision needs a value");
         }
      }
      if (it != end && *it == 's') {
         ++it;
      }

      if (it != end && *it != '}') {
         throw std::format_error("ocs::FormatText supports fill, align, width, precision and type s only");
      }
      return it;
   }

   /**
    * @brief Write the string, truncated to the precision and padded to the width
    *
    * ASCII text is truncated and padded by its byte count, any other text is formatted by std::format with the
    * same spec, so that its display width is computed exactly as without the wrapper.
    *
    * @param t the string
    * @param ctx the format context
    * @return the output iterator after the written characters
    * @throw std::format_error when a width or precision argument is not an integer or is negative
    */
   template<typename FormatContext>
   typename FormatContext::iterator
   format(const ocs::FormatText &t, FormatContext &ctx) const {
      const std::size_t width = resolve(width_, ctx);
      const std::size_t precision = precision_.given ? resolve(precision_, ctx) : 0;

      std::string_view s = t.text;
      if (width == 0 && !precision_.given) {
         write(s, ctx);
         return ctx.out();
      }
      if (!is_ascii(s)) {
         // the same spec, with width and precision as arguments; a width of 0 pads nothing, as no width does
         std::string_view spec = precision_.given ? "{0:__{1}.{2}}" : "{0:__{1}}";
         std::string runtime_spec(spec);
         runtime_spec[3] = fill_;
         runtime_spec[4] = align_;
         return std::vformat_to(ctx.out(), runtime_spec, std::make_format_args(s, width, precision));
      }
      if (precision_.given && s.size() > precision) {
         s = s.substr(0, precision);
      }

      const std::size_t pad = width > s.size() ? width - s.size() : 0;
      const std::size_t before = align_ == '>' ? pad : align_ == '^' ? pad / 2 : 0;
      write_fill(before, ctx);
      write(s, ctx);
      write_fill(pad - before, ctx);
      return ctx.out();
   }
};
