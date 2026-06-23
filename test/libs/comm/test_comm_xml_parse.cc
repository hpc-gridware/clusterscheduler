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
 * Regression test for CS-2343 (HIGH-COMM-001):
 *   cl_xml_parse_version() wrote charptr[buffer_length - 1] with no bounds
 *   check; the CM/MIH/... parsers could pass version_begin >= buffer_length,
 *   underflowing the length and handing the helper an out-of-bounds pointer
 *   (pre-auth remote crash). The fix clamps version_begin < buffer_length at
 *   the call sites and hardens the helper.
 *
 * The crafted buffer is placed flush against an unmapped guard page so the
 * out-of-bounds access faults deterministically: pre-fix the parser hands an
 * OOB pointer to cl_xml_parse_version() and strchr() reads the guard page
 * (SIGSEGV); post-fix the clamp skips the call and the parser returns cleanly.
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#include "comm/cl_commlib.h"
#include "comm/cl_xml_parsing.h"

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

/** @brief Run cl_xml_parse_CM() with @p payload placed flush against a guard
 * page, so any read past the buffer end faults deterministically (CS-2343).
 *
 * @param payload  message body bytes (not NUL-terminated on the wire)
 * @param len      number of bytes in @p payload
 * @param msg_out  out: parsed CM message (caller frees), or nullptr
 * @return the cl_xml_parse_CM() return value
 */
static int parse_cm_at_guard(const char *payload, size_t len, cl_com_CM_t **msg_out) {
   const long ps = sysconf(_SC_PAGESIZE);
   char *region = static_cast<char *>(mmap(nullptr, 2 * ps, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
   *msg_out = nullptr;
   if (region == MAP_FAILED) {
      return CL_RETVAL_MALLOC;
   }
   mprotect(region + ps, ps, PROT_NONE);            // trailing guard page

   char *const buf = region + ps - len;             // last payload byte ends at the boundary
   memcpy(buf, payload, len);

   cl_com_CM_t *msg = nullptr;
   const int ret = cl_xml_parse_CM(reinterpret_cast<unsigned char *>(buf), len, &msg);
   *msg_out = msg;
   munmap(region, 2 * ps);
   return ret;
}

int main() {
   cl_com_setup_commlib(CL_NO_THREAD, CL_LOG_OFF, nullptr);

   printf("\n--- COMM-XML: valid version attribute ---\n");
   {
      // a well-formed version="..." is still extracted unchanged. A valid
      // <port> is required to reach the version-extraction code; the CM is
      // otherwise incomplete, so the parser returns an error - we only assert
      // the version value here, proving the fix preserves valid parsing.
      char ok_buf[] = "<port>1</port><z version=\"9.0\">";
      cl_com_CM_t *msg = nullptr;
      cl_xml_parse_CM(reinterpret_cast<unsigned char *>(ok_buf), strlen(ok_buf), &msg);
      CHECK(1, "COMM-XML: valid version -> parsed as \"9.0\"",
            msg != nullptr && msg->version != nullptr && strcmp(msg->version, "9.0") == 0);
      cl_com_free_cm_message(&msg);
   }

   printf("\n--- COMM-XML: version= at buffer end (CS-2343) ---\n");
   {
      // valid <port> (to pass the earlier required-field check) followed by a
      // tag whose body ends exactly at the '=' of a version attribute, so
      // version_begin reaches buffer_length + 1. Pre-fix this hands an OOB
      // pointer to cl_xml_parse_version() and faults; post-fix the call is
      // skipped and the parser rejects the message cleanly (reaching these
      // CHECKs == no crash).
      const char attack[] = "<port>1</port><z version=";
      cl_com_CM_t *msg = nullptr;
      int ret = parse_cm_at_guard(attack, sizeof(attack) - 1, &msg);
      CHECK(2, "COMM-XML: version= at end does not crash, rejected cleanly",
            ret == CL_RETVAL_UNKNOWN);
      CHECK(3, "COMM-XML: degenerate version attribute yields no version",
            msg == nullptr || msg->version == nullptr);
      cl_com_free_cm_message(&msg);
   }

   cl_com_cleanup_commlib();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
