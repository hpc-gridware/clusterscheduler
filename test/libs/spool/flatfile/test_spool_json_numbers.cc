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

/*
 * The JSON spooling reads integer fields of every width with a conversion of that width (CS-3200).
 *
 * A value written as a JSON number reads back as it was. A value given as a string or as a float - in a spool
 * file edited by hand - was read through a 32 bit conversion: 64 bit values were cut, negative strings lost
 * their sign, and values out of range wrapped without a message. Now it is read with the width of the field,
 * and a value that does not fit is reported and fails the read.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_TEST.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/ocs_spool_json.h"

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

/// distinct values of the element written first, so that each one can be found in the JSON text and replaced
static constexpr std::uint32_t U32 = 4242;
static constexpr std::uint64_t U64 = 123456789;
static constexpr std::int32_t I32 = 31313;
static constexpr std::int64_t I64 = 987654;

/**
 * @brief Replace the one occurrence of a number in a JSON text
 * @param json   the text
 * @param from   the number as written
 * @param to     the replacement
 * @return the text with the replacement, or an empty string if the number does not occur exactly once
 */
static std::string
replace_once(const std::string &json, const std::string &from, const std::string &to) {
   const std::size_t pos = json.find(from);
   if (pos == std::string::npos || json.find(from, pos + 1) != std::string::npos) {
      return {};
   }
   std::string out = json;
   out.replace(pos, from.size(), to);
   return out;
}

/**
 * @brief Read a TEST element from a JSON text
 * @param fields  the spooling fields of TEST
 * @param json    the text
 * @param error   set to true if the answer list holds an error
 * @return the element, empty if the read failed
 */
static ocs::tom::RawElementPtr
read_test(const spooling_field *fields, const std::string &json, bool *error) {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = spool_json_read_object(&alp, ocs::tom::TEST::descriptor.e, fields, nullptr, json.c_str());
   *error = answer_list_has_error(&alp);
   return ep;
}

/**
 * @brief Run the checks of this module test
 * @param argc unused
 * @param argv unused
 * @return 0 if every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_json_numbers");

   ocs::Bootstrap::get_ignore_fqdn();
   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = spool_get_fields_to_spool(&alp, ocs::tom::TEST::descriptor.e, &spool_config_instr);

   auto test = ocs::tom::ElementPtr<ocs::tom::TEST>::create();
   test->set<TEST_uint32>(U32);
   test->set<TEST_uint64>(U64);
   test->set<TEST_int>(I32);
   test->set<TEST_long>(I64);
   dstring out = DSTRING_INIT;
   const bool written = fields != nullptr && spool_json_write_object(&alp, test.get(), fields, &out);
   const std::string json = written ? sge_dstring_get_string(&out) : "";
   sge_dstring_free(&out);
   CHECK(1, "a TEST element is written as JSON", written && !json.empty());

   bool error = false;
   auto ep = read_test(fields, json, &error);
   CHECK(2, "the JSON numbers read back unchanged",
         ep && !error && ep->get<std::uint32_t>(TEST_uint32) == U32 && ep->get<std::uint64_t>(TEST_uint64) == U64 &&
         ep->get<std::int32_t>(TEST_int) == I32 && ep->get<std::int64_t>(TEST_long) == I64);

   ep = read_test(fields, replace_once(json, std::to_string(U64), "\"5000000000\""), &error);
   CHECK(3, "a 64 bit value given as a string keeps its 64 bits",
         ep && !error && ep->get<std::uint64_t>(TEST_uint64) == 5000000000ULL);

   ep = read_test(fields, replace_once(json, std::to_string(U64), "5000000000.0"), &error);
   CHECK(4, "a 64 bit value given as an integral float keeps its 64 bits",
         ep && !error && ep->get<std::uint64_t>(TEST_uint64) == 5000000000ULL);

   ep = read_test(fields, replace_once(json, std::to_string(I64), "\"-5\""), &error);
   CHECK(5, "a negative 64 bit value given as a string keeps its sign",
         ep && !error && ep->get<std::int64_t>(TEST_long) == -5);

   ep = read_test(fields, replace_once(json, std::to_string(I32), "\"-7\""), &error);
   CHECK(6, "a negative 32 bit value given as a string keeps its sign",
         ep && !error && ep->get<std::int32_t>(TEST_int) == -7);

   ep = read_test(fields, replace_once(json, std::to_string(U32), "\"5000000000\""), &error);
   CHECK(7, "an unsigned 32 bit value out of range is reported and fails the read", !ep && error);

   ep = read_test(fields, replace_once(json, std::to_string(U32), "3.7"), &error);
   CHECK(8, "a float that is not integral is reported for an integer field", !ep && error);

   ep = read_test(fields, replace_once(json, std::to_string(U64), "\"-1\""), &error);
   CHECK(9, "a negative value for an unsigned 64 bit field is reported", !ep && error);

   ep = read_test(fields, replace_once(json, std::to_string(I32), "\"-2147483649\""), &error);
   CHECK(10, "a signed 32 bit value out of range is reported", !ep && error);

   ep = read_test(fields, replace_once(json, std::to_string(I64), "\"12abc\""), &error);
   CHECK(11, "a string that is not a number is reported", !ep && error);

   ep.reset();
   if (fields != nullptr) {
      spool_free_spooling_fields(fields);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
