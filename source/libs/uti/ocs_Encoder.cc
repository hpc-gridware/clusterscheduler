/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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
#include <cstdint>
#include <string>
#include <vector>

#include "uti/sge_rmon_macros.h"

#include "ocs_Encoder.h"

// Custom alphabet for encoding/decoding
static constexpr std::array<char, 16> ALPHABET = {'*','b','~','d','e',',','g','h','&','j','k','l','r','n','=','p'};

/** @brief Get the lookup table for decoding characters.
 *
 * @return A reference to the static lookup table array.
 */
const std::array<int8_t, 256>&
ocs::Encoder::get_lut() {
   static constexpr std::array<int8_t, 256> lut = []() -> std::array<int8_t, 256> {
      std::array<int8_t, 256> a;
      a.fill(static_cast<int8_t>(-1));
      for (int i = 0; i < static_cast<int>(ALPHABET.size()); ++i) {
         a[static_cast<unsigned char>(ALPHABET[i])] = static_cast<int8_t>(i);
      }
      return a;
   }();
   return lut;
}

/** @brief Encode a string into a hexadecimal-like representation using a custom alphabet.
 *
 * @param src The input binary string to be encoded.
 * @param dst The output string that will contain the encoded representation.
 * @return true if encoding was successful, false otherwise.
 */
bool
ocs::Encoder::encode(const std::string& src, std::string& dst) {
   DENTER(TOP_LAYER);
   dst.clear();
   dst.reserve(src.size() * 2);
   for (const char ch : src) {
      const uint8_t b = static_cast<uint8_t>(ch);
      const uint8_t hi = (b >> 4) & 0x0F;
      const uint8_t lo = b & 0x0F;
      dst.push_back(ALPHABET[hi]);
      dst.push_back(ALPHABET[lo]);
   }
   DRETURN(true);
}

/** @brief Decode a string from a hexadecimal-like representation using a custom alphabet.
 *
 * @param src The input encoded string to be decoded.
 * @param dst The output string that will contain the decoded binary data.
 * @return true if decoding was successful, false otherwise.
 */
bool
ocs::Encoder::decode(const std::string& src, std::string& dst) {
   DENTER(TOP_LAYER);

   if ((src.size() & 1) != 0) {
      DRETURN(false);
   }

   // Resize output buffer
   const size_t out_len = src.size() / 2;
   dst.clear();
   dst.resize(out_len);

   // Decode each pair of characters
   const auto lut = get_lut();
   for (size_t i = 0, j = 0; i < src.size(); i += 2, ++j) {
      const int8_t hi = lut[static_cast<unsigned char>(src[i])];
      const int8_t lo = lut[static_cast<unsigned char>(src[i+1])];

      // Validate characters
      if (hi < 0 || lo < 0) {
         DRETURN(false);
      }

      const unsigned char v = static_cast<unsigned char>((hi << 4) | lo);
      dst[j] = static_cast<char>(v);
   }

   DRETURN(true);
}
