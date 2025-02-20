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

#include <iostream>
#include <cstdlib>
#include <cstring>

#include "basis_types.h"
#include "uti/ocs_Munge.h"
#include "uti/sge_dstring.h"
#include "uti/sge_stdlib.h"

bool
test_munge() {
   bool ret = true;
   const char *payload = "Hello, OCS!";
   char *decoded_payload = nullptr;

   // encode
   char *cred = nullptr;
   munge_err_t err = ocs::uti::Munge::munge_encode_func(&cred, nullptr, payload, strlen(payload) + 1);
   if (err != EMUNGE_SUCCESS) {
      std::cerr << "munge_encode_func failed: " << ocs::uti::Munge::munge_strerror_func(err) << std::endl;
      ret = false;
   } else {
      std::cout << "munge decode succeeded, output is " << cred << std::endl;
   }

   // decode
   if (ret) {
      int decoded_len{0};
      uid_t uid{0};
      gid_t gid{0};
      err = ocs::uti::Munge::munge_decode_func(cred, nullptr, (void **)(&decoded_payload), &decoded_len, &uid, &gid);
      if (err != EMUNGE_SUCCESS) {
         std::cerr << "munge_decode_func failed: " << ocs::uti::Munge::munge_strerror_func(err) << std::endl;
         ret = false;
      } else {
         std::cout << "munge decode succeeded, output is " << decoded_payload << std::endl;
         std::cout << "uid: " << uid << ", gid: " << gid << std::endl;
      }
   }

   // decode a second time, must be rejected
   if (ret) {
      int decoded_len{0};
      uid_t uid{0};
      gid_t gid{0};
      err = ocs::uti::Munge::munge_decode_func(cred, nullptr, (void **)(&decoded_payload), &decoded_len, &uid, &gid);
      if (err == EMUNGE_SUCCESS) {
         std::cerr << "repeated munge_decode_func should have failed but succeeded" << std::endl;
         ret = false;
      } else if (err == EMUNGE_CRED_REPLAYED) {
         std::cout << "repeated munge_decode_func failed as expected: " << ocs::uti::Munge::munge_strerror_func(err) << std::endl;
      } else {
         std::cerr << "repeated munge_decode_func failed but expected EMUNGE_CRED_REPLAYED but got: " << ocs::uti::Munge::munge_strerror_func(err) << std::endl;
         ret = false;
      }
   }

   sge_free(&cred);
   sge_free(&decoded_payload);

   return ret;
}

int main(int argc, char *argv[]) {
   int ret = EXIT_SUCCESS;

   DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
   if (ocs::uti::Munge::initialize(&error_dstr)) {
      std::cout << "initializing munge succeeded" << std::endl;
      // do something
      ocs::uti::Munge::print_munge_enums();
      test_munge();
      ocs::uti::Munge::shutdown();
   } else {
      std::cerr << "initializing munge failed: " << sge_dstring_get_string(&error_dstr) << std::endl;
      ret = EXIT_FAILURE;
   }
   return ret;
}
