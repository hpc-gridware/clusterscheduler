/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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

#include <cstdio>
#include <string>

#include "uti/ocs_Encoder.h"
#include "uti/sge_rmon_macros.h"

static int s_fail = 0;

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_Encoder");
   int id = 1;
   std::string enc, dec;

   printf("\n--- encode / decode round-trip ---\n");
   // T01: encode returns true
   CHECK(id, "encode returns true", ocs::Encoder::encode("HPC-Gridware", enc)); id++;
   // T02: round-trip decode restores original string
   ocs::Encoder::decode(enc, dec);
   CHECK(id, "decode round-trip matches original", dec == "HPC-Gridware"); id++;
   // T03: empty string encodes and decodes to empty
   ocs::Encoder::encode("", enc);
   ocs::Encoder::decode(enc, dec);
   CHECK(id, "empty string round-trip", dec.empty()); id++;
   // T04: encoded length is exactly double the input length
   ocs::Encoder::encode("HPC-Gridware", enc);
   CHECK(id, "encoded length == 2 * input length", enc.size() == 2 * std::string("HPC-Gridware").size()); id++;

   printf("\n--- known-value encoding ---\n");
   // T05: 'A' (0x41) -> hi nibble 4 -> 'e', lo nibble 1 -> 'b', so encoded = "eb"
   ocs::Encoder::encode("A", enc);
   CHECK(id, "encode(\"A\") == \"eb\"", enc == "eb"); id++;
   // T06: decode of a known encoded value directly (not via round-trip)
   ocs::Encoder::decode("eb", dec);
   CHECK(id, "decode(\"eb\") == \"A\"", dec == "A"); id++;
   // T07: 0x00 encodes to "**" (ALPHABET[0]='*'), 0xFF encodes to "pp" (ALPHABET[15]='p')
   ocs::Encoder::encode(std::string("\x00\xff", 2), enc);
   CHECK(id, "encode(\"\\x00\\xff\") == \"**pp\"", enc == "**pp"); id++;
   // T08: multi-byte binary string with embedded null and 0xFF round-trips correctly
   ocs::Encoder::decode(enc, dec);
   CHECK(id, "binary string with null and 0xFF round-trips", dec == std::string("\x00\xff", 2)); id++;
   // T09: encoded output contains only characters from the custom alphabet
   {
      static constexpr std::string_view ALPHABET = "*b~de,gh&jklrn=p";
      ocs::Encoder::encode("HPC-Gridware", enc);
      bool only_alphabet = true;
      for (char c : enc) {
         if (ALPHABET.find(c) == std::string_view::npos) {
            only_alphabet = false;
            break;
         }
      }
      CHECK(id, "encoded output contains only alphabet characters", only_alphabet); id++;
   }
   // T10: all 256 byte values encode and decode back to the original byte
   {
      bool all_pass = true;
      for (int i = 0; i < 256 && all_pass; ++i) {
         std::string s(1, static_cast<char>(i));
         std::string e, d;
         if (!ocs::Encoder::encode(s, e) || !ocs::Encoder::decode(e, d) || d != s) {
            all_pass = false;
         }
      }
      CHECK(id, "all 256 byte values round-trip", all_pass); id++;
   }

   printf("\n--- decode error handling ---\n");
   // T11: odd-length input has no valid pairing and must be rejected
   CHECK(id, "decode rejects odd-length input", !ocs::Encoder::decode("*", dec)); id++;
   // T12: first character of a pair is outside the custom alphabet ('a' is not in the alphabet)
   CHECK(id, "decode rejects invalid first char in pair", !ocs::Encoder::decode("ab", dec)); id++;
   // T13: second character of a pair is outside the custom alphabet ('?' is not in the alphabet)
   CHECK(id, "decode rejects invalid second char in pair", !ocs::Encoder::decode("b?", dec)); id++;
   // T14: decode of empty string is valid and produces an empty output
   CHECK(id, "decode(\"\") returns true", ocs::Encoder::decode("", dec)); id++;
   CHECK(id, "decode(\"\") produces empty output", dec.empty()); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
