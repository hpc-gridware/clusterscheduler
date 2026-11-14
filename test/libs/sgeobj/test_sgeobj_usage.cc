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

#include <cstdio>

#include "sgeobj/sge_usage.h"

#include "basis_types.h"

/**
 * @brief Parse a custom usage value and compare the outcome against the expected one.
 *
 * @param[in] value the value to parse
 * @param[in] expected_valid expected return value of usage_parse_number()
 * @param[in] expected_number expected number, only checked if expected_valid is true
 * @return the number of errors found (0 or 1)
 */
static int
check_parse_number(const char *value, bool expected_valid, double expected_number) {
   double number = -12345.0;
   const bool valid = usage_parse_number(value, number);

   if (valid != expected_valid || (valid && number != expected_number)) {
      printf("FAILED: usage_parse_number(\"%s\")\n", value != nullptr ? value : "(nullptr)");
      printf("   got:      valid=%d number=%g\n", valid, number);
      printf("   expected: valid=%d number=%g\n", expected_valid, expected_number);
      return 1;
   }
   if (!valid && number != -12345.0) {
      printf("FAILED: usage_parse_number(\"%s\") changed the number although it is not valid\n", value);
      return 1;
   }
   return 0;
}

int
main(int argc, char *argv[]) {
   int ret = 0;

   // numbers
   ret += check_parse_number("5", true, 5.0);
   ret += check_parse_number("0.42", true, 0.42);
   ret += check_parse_number("-1.5", true, -1.5);
   ret += check_parse_number("1e-7", true, 1e-7);
   ret += check_parse_number("123456789012345", true, 123456789012345.0);
   ret += check_parse_number("0", true, 0.0);

   // true and false give 1 and 0, in any case
   ret += check_parse_number("true", true, 1.0);
   ret += check_parse_number("TRUE", true, 1.0);
   ret += check_parse_number("False", true, 0.0);

   // not a number
   ret += check_parse_number(nullptr, false, 0.0);
   ret += check_parse_number("", false, 0.0);
   ret += check_parse_number("gpt", false, 0.0);
   ret += check_parse_number("5abc", false, 0.0);
   ret += check_parse_number("5 ", false, 0.0);
   ret += check_parse_number("\"5\"", false, 0.0);
   ret += check_parse_number("yes", false, 0.0);
   ret += check_parse_number("truex", false, 0.0);
   ret += check_parse_number("nan", false, 0.0);
   ret += check_parse_number("inf", false, 0.0);
   ret += check_parse_number("-Infinity", false, 0.0);
   ret += check_parse_number("1e999", false, 0.0);

   if (ret == 0) {
      printf("PASS: test solved!\n");
   } else {
      printf("FAILED: test NOT solved!\n");
   }

   return ret;
}
