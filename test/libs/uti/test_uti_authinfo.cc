/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * Tests for component_parse_auth_info() (the pre-auth GDI auth_info parser).
 *
 * The headline scenario is CS-2346: the supplementary-group count is read from
 * the client-controlled auth_info and sized an allocation with only a ">0"
 * guard - a huge count forced a multi-TB sge_malloc (DoS) and wrapped on 32-bit.
 * The fix caps the count at the OS maximum (sge_sysconf(SGE_SYSCONF_NGROUPS_MAX))
 * before allocating. The remaining cases cover the parser's other branches
 * (field parsing, malformed/short/over-long inputs) for coverage.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/ocs_Encoder.h"

#include <sge_log.h>

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

/** @brief Join auth_info fields with the 0xff separator the protocol uses. */
static std::string join_authinfo(const std::vector<std::string> &fields) {
   std::string s;
   for (size_t i = 0; i < fields.size(); ++i) {
      if (i != 0) {
         s.push_back(static_cast<char>(0xff));
      }
      s += fields[i];
   }
   return s;
}

/** @brief Result of a parse() call; @p grp must be sge_free()d by the caller. */
struct parse_result_t {
   bool ret;
   uid_t uid;
   gid_t gid;
   std::string user;
   std::string group;
   int amount;
   ocs_grp_elem_t *grp;
};

/** @brief Encode @p fields the way the client does and run component_parse_auth_info(). */
static parse_result_t parse(const std::vector<std::string> &fields) {
   std::string enc;
   ocs::Encoder::encode(join_authinfo(fields), enc);
   std::vector<char> buf(enc.begin(), enc.end());
   buf.push_back('\0');

   DSTRING_STATIC(err, MAX_STRING_SIZE);
   char user[MAX_STRING_SIZE] = {};
   char group[MAX_STRING_SIZE] = {};
   parse_result_t r{};
   r.amount = -1;
   r.grp = nullptr;
   r.ret = component_parse_auth_info(&err, buf.data(), &r.uid, user, sizeof(user),
                                     &r.gid, group, sizeof(group), &r.amount, &r.grp);
   r.user = user;
   r.group = group;
   return r;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_authinfo");
   int id = 1;

   // suppress ERROR-level sge_log output from the parse helper
   component_set_daemonized(true);

   const uint32_t cap = sge_sysconf(SGE_SYSCONF_NGROUPS_MAX);
   printf("NGROUPS_MAX cap = %u\n", cap);

   printf("\n--- AUTHINFO: valid input ---\n");
   {
      // uid | gid | user | group | amount | gid1 | name1 | gid2 | name2
      parse_result_t r = parse({"1001", "2002", "testuser", "testgroup", "2", "100", "grpA", "200", "grpB"});
      CHECK(id++, "valid auth_info accepted", r.ret);
      CHECK(id++, "uid/gid parsed", r.uid == 1001 && r.gid == 2002);
      CHECK(id++, "user/group parsed", r.user == "testuser" && r.group == "testgroup");
      CHECK(id++, "amount == 2", r.amount == 2);
      CHECK(id++, "supplementary groups parsed",
            r.grp != nullptr &&
            r.grp[0].id == 100 && strcmp(r.grp[0].name, "grpA") == 0 &&
            r.grp[1].id == 200 && strcmp(r.grp[1].name, "grpB") == 0);
      sge_free(&r.grp);
   }
   {
      // amount == 0: valid request with no supplementary groups, no allocation
      parse_result_t r = parse({"0", "0", "root", "root", "0"});
      CHECK(id++, "amount 0 accepted, no group array", r.ret && r.amount == 0 && r.grp == nullptr);
      sge_free(&r.grp);
   }

   printf("\n--- AUTHINFO: group count above NGROUPS_MAX cap (CS-2346) ---\n");
   {
      // amount = cap+1 WITH that many matching gid/name tokens, so the request is
      // well-formed: WITHOUT the cap it would be accepted (forcing the oversized
      // allocation); WITH the cap it is rejected before allocating. (red->green:
      // pre-fix ret == true, post-fix ret == false.)
      const uint64_t over = static_cast<uint64_t>(cap) + 1;
      std::vector<std::string> fields = {"0", "0", "testuser", "testgroup", std::to_string(over)};
      fields.reserve(5 + over * 2);
      for (uint64_t i = 0; i < over; ++i) {
         fields.emplace_back("1");   // supplementary gid
         fields.emplace_back("g");   // supplementary group name
      }
      parse_result_t r = parse(fields);
      CHECK(id++, "over-cap count rejected (would be accepted without the cap)", !r.ret);
      CHECK(id++, "no group array retained for rejected count", r.grp == nullptr);
      sge_free(&r.grp);
   }

   printf("\n--- AUTHINFO: malformed / inconsistent input rejected ---\n");
   {
      parse_result_t r = parse({"notauid", "0", "u", "g", "0"});
      CHECK(id++, "non-numeric uid rejected", !r.ret);
      sge_free(&r.grp);
   }
   {
      parse_result_t r = parse({"0", "notagid", "u", "g", "0"});
      CHECK(id++, "non-numeric gid rejected", !r.ret);
      sge_free(&r.grp);
   }
   {
      // too few tokens (no count field) -> "old client" rejection
      parse_result_t r = parse({"0", "0", "u"});
      CHECK(id++, "too few tokens rejected", !r.ret);
      sge_free(&r.grp);
   }
   {
      // declared count (3) does not match the supplied group tokens (2 pairs)
      parse_result_t r = parse({"0", "0", "u", "g", "3", "10", "a", "20", "b"});
      CHECK(id++, "count larger than supplied groups rejected", !r.ret);
      sge_free(&r.grp);
   }
   {
      // more group tokens than the declared count (1) -> "too many IDs"
      parse_result_t r = parse({"0", "0", "u", "g", "1", "10", "a", "20", "b"});
      CHECK(id++, "more groups than declared count rejected", !r.ret);
      sge_free(&r.grp);
   }
   {
      // non-numeric supplementary gid in the group list
      parse_result_t r = parse({"0", "0", "u", "g", "1", "notagid", "a"});
      CHECK(id++, "non-numeric supplementary gid rejected", !r.ret);
      sge_free(&r.grp);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
