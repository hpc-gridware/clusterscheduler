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

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "uti/ocs_Encoder.h"

int main() {
   std::string plain = "HPC-Gridware";
   std::string encoded;
   if (!ocs::Encoder::encode(plain, encoded)) {
      std::cerr << "encode failed\n"; return 1;
   }
   std::cout << "plain: " << plain << std::endl;
   std::cout << "encoded: " << encoded << std::endl;

   std::string decoded;
   if (!ocs::Encoder::decode(encoded, decoded)) {
      std::cerr << "decode failed\n"; return 1;
   }
   std::cout << "decoded: " << decoded << std::endl;

   if (decoded != plain) {
      std::cerr << "decoded string does not match original\n";
      return 1;
   }
   return 0;
}
