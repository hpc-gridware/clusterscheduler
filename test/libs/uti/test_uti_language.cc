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

/**
 * @file
 * @brief Module test for sge_gettext_() with message id output (CS-2361).
 *
 * sge_gettext_() prefixes a message with "[id] " when message id output is
 * enabled. The buffer was sized for the longest positive id, "[99999] ", but
 * the guard that adds the prefix (msg_id <= 99999) also admits negative ids,
 * whose prefix is up to 14 bytes long -- a heap overflow. Under ASAN the old
 * code is reported; without ASAN the overrun stays inside the malloc slack and
 * the test only checks the complete result.
 *
 * Message ids only exist where gettext support is compiled in
 * (__SGE_COMPILE_WITH_GETTEXT__, Linux and Solaris); elsewhere the test passes
 * as skipped, because the testsuite runs it on every suited host.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <clocale>
#include <string>

#include "uti/sge_language.h"
#include "uti/sge_rmon_macros.h"

#if defined(__SGE_COMPILE_WITH_GETTEXT__)
/**
 * @brief Check the prefixed message for one message id.
 *
 * @param msg_id   message id handed to sge_gettext_()
 * @param msg_str  message, must contain a space to get a prefix
 * @return true if the result is "[msg_id] msg_str"
 */
static bool
check_prefix(int msg_id, const char *msg_str) {
   const std::string expected = "[" + std::to_string(msg_id) + "] " + msg_str;
   const char *result = sge_gettext_(msg_id, msg_str);
   const bool ok = result != nullptr && expected == result;
   printf("%s  sge_gettext_(%d): got \"%s\", expected \"%s\"\n", ok ? "ok   " : "FAIL ",
          msg_id, result != nullptr ? result : "(null)", expected.c_str());
   return ok;
}

#endif

/**
 * @brief Enable message id output and check sge_gettext_() for several ids.
 *
 * @param argc  unused
 * @param argv  unused
 * @return EXIT_SUCCESS if all checks passed or the test does not apply,
 *         EXIT_FAILURE otherwise
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_language");
#if !defined(__SGE_COMPILE_WITH_GETTEXT__)
   printf("skipped: built without gettext support, there are no message ids\n");
   DRETURN(EXIT_SUCCESS);
#else
   bool ret = true;

   // sge_init_languagefunc() needs at least setlocale(); without a gettext
   // function sge_gettext__() returns the message untranslated. The catalog
   // directory does not exist, which only means no translation is found.
   // SGE_ENABLE_MSG_ID=2 adds the message id to every message.
   sge_init_language_func(nullptr, setlocale, nullptr, nullptr);
   setenv("SGE_ENABLE_MSG_ID", "2", 1);
   char package[] = "gridengine";
   char locale_dir[] = "/nonexistent";
   sge_init_languagefunc(package, locale_dir);
   if (sge_get_message_id_output() == 0) {
      printf("FAIL  message id output could not be enabled\n");
      DRETURN(EXIT_FAILURE);
   }

   // the short message makes the buffer small, so a too long prefix overruns it
   ret = check_prefix(4711, "a b") && ret;
   ret = check_prefix(99999, "a b") && ret;
   ret = check_prefix(-1, "a b") && ret;
   ret = check_prefix(-99999, "a b") && ret;
   ret = check_prefix(INT_MIN, "a b") && ret;

   // ids above 99999 get no prefix
   const char *result = sge_gettext_(100000, "a b");
   const bool ok = result != nullptr && strcmp(result, "a b") == 0;
   printf("%s  sge_gettext_(100000): no prefix\n", ok ? "ok   " : "FAIL ");
   ret = ok && ret;

   printf("\n%s\n", ret ? "PASS" : "FAIL");
   DRETURN(ret ? EXIT_SUCCESS : EXIT_FAILURE);
#endif
}
