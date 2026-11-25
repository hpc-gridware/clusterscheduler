/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <string>

#include "comm/commlib.h"

#include "uti/sge.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_cull_xml.h"

#include "basis_types.h"

/**
 * @brief Security regression test for CS-2364 (CWE-22): verify_host_name().
 *
 * Host names become spool file keys (EXECHOST_DIR/<name>) and are not run
 * through verify_str_key(), so verify_host_name() must reject names that are
 * unsafe as a single path component: a '/' anywhere or a leading '.'.
 * Before the fix those were accepted.
 *
 * Also covers the length limit (CS-3214): a host name is held in char[CL_MAXHOSTNAMELEN]
 * buffers, so it may have CL_MAXHOSTNAMELEN - 1 characters at most. A too long name once
 * added an error to the answer list but returned true, and a name of exactly
 * CL_MAXHOSTNAMELEN characters was accepted although it does not fit.
 *
 * @return number of failed checks
 */
static int test_verify_host_name() {
   int ret = 0;
   lList *al = nullptr;

   struct {
      const char *name;
      bool expect_ok;
   } cases[] = {
      {"myhost", true},
      {"host-01.example.com", true},
      {"../../tmp/x", false},
      {"/etc/passwd", false},
      {"..", false},
      {".", false},
      {".hidden", false},
      {"a/b", false},
      {nullptr, false},
      {"", false},
   };
   for (const auto &c : cases) {
      if (verify_host_name(&al, c.name) != c.expect_ok) {
         printf("verify_host_name(%s) should %s\n", c.name != nullptr ? c.name : "nullptr",
                c.expect_ok ? "be accepted" : "be rejected");
         ret++;
      }
   }
   lFreeList(&al);

   // CS-3214: the length limit
   const std::string at_limit(CL_MAXHOSTNAMELEN - 1, 'a');
   const std::string above_limit(CL_MAXHOSTNAMELEN, 'a');
   if (!verify_host_name(&al, at_limit.c_str()) || answer_list_has_error(&al)) {
      printf("verify_host_name() should accept a name of CL_MAXHOSTNAMELEN - 1 characters\n");
      ret++;
   }
   lFreeList(&al);
   if (verify_host_name(&al, above_limit.c_str())) {
      printf("verify_host_name() should reject a name of CL_MAXHOSTNAMELEN characters\n");
      ret++;
   }
   if (!answer_list_has_error(&al)) {
      printf("verify_host_name() should leave an error for a too long name\n");
      ret++;
   }
   lFreeList(&al);

   return ret;
}

/**
 * @brief Regression test for CS-3214: sge_resolve_hostname() refuses an over-long name instead of truncating it.
 *
 * The first CL_MAXHOSTNAMELEN - 1 characters of an over-long name could be the name of another host, so the
 * result must stay empty. None of the names checked here reaches the resolver, so no commlib setup is needed.
 *
 * @return number of failed checks
 */
static int test_resolve_hostname() {
   int ret = 0;
   char unique[CL_MAXHOSTNAMELEN];

   // a name of exactly CL_MAXHOSTNAMELEN characters does not fit with its terminator
   const std::string at_limit(CL_MAXHOSTNAMELEN, 'h');
   memset(unique, 'x', sizeof(unique));
   int rc = sge_resolve_hostname(at_limit.c_str(), unique, EH_name);
   if (rc != CL_RETVAL_HOSTNAME_LENGTH_ERROR || unique[0] != '\0') {
      printf("sge_resolve_hostname() of CL_MAXHOSTNAMELEN characters: rc %d, should be a length error and empty\n",
             rc);
      ret++;
   }

   // its first CL_MAXHOSTNAMELEN - 1 characters could be the name of another host
   const std::string over_long(300, 'h');
   memset(unique, 'x', sizeof(unique));
   rc = sge_resolve_hostname(over_long.c_str(), unique, NoName);
   if (rc != CL_RETVAL_HOSTNAME_LENGTH_ERROR || unique[0] != '\0') {
      printf("sge_resolve_hostname() of 300 characters: rc %d, should be a length error and empty\n", rc);
      ret++;
   }

   // the special names are kept as they are, without a resolver
   memset(unique, 'x', sizeof(unique));
   rc = sge_resolve_hostname(SGE_GLOBAL_NAME, unique, EH_name);
   if (rc != CL_RETVAL_OK || strcmp(unique, SGE_GLOBAL_NAME) != 0) {
      printf("sge_resolve_hostname(" SGE_GLOBAL_NAME ") should keep the name as it is\n");
      ret++;
   }

   // a missing name is a parameter error and leaves the result empty
   memset(unique, 'x', sizeof(unique));
   rc = sge_resolve_hostname(nullptr, unique, EH_name);
   if (rc != CL_RETVAL_PARAMS || unique[0] != '\0') {
      printf("sge_resolve_hostname(nullptr): rc %d, should be a parameter error and empty\n", rc);
      ret++;
   }

   return ret;
}

/**
 * @brief Regression test for CS-1980: escape_string() replaces the XML special characters.
 *
 * qrstat -xml escapes user-chosen strings with escape_string(); '<' used to be
 * written as "%lt;" instead of "&lt;".
 *
 * @return number of failed checks
 */
static int test_escape_string() {
   int ret = 0;

   struct {
      const char *input;
      const char *expected;
   } cases[] = {
      {"plain", "plain"},
      {"<a&\"'>", "&lt;a&amp;&quot;&apos;&gt;"},
      {"</account><x>", "&lt;/account&gt;&lt;x&gt;"},
   };
   for (const auto &c : cases) {
      dstring buf = DSTRING_INIT;
      const bool ok = escape_string(c.input, &buf);
      const char *result = sge_dstring_get_string(&buf);
      if (!ok || result == nullptr || strcmp(result, c.expected) != 0) {
         printf("escape_string(%s) = %s, expected %s\n", c.input, result != nullptr ? result : "(null)", c.expected);
         ret++;
      }
      sge_dstring_free(&buf);
   }

   dstring buf = DSTRING_INIT;
   if (escape_string(nullptr, &buf)) {
      printf("escape_string(nullptr) should return false\n");
      ret++;
   }
   sge_dstring_free(&buf);

   return ret;
}

/**
 * @brief Run the sgeobj utility module tests
 *
 * @param argc number of command line arguments (unused)
 * @param argv command line arguments (unused)
 * @return 0 if all checks passed, otherwise the number of failed checks
 */
int main(int argc, char *argv[])
{
   lList *answer_list = nullptr;

   int i=0, ret=0;

   static const char* denied[] = {
      "forbiddencharacterwithing@thestring",
      ".forbiddencharacteratthebeginning",
      "TEMPLATE",
      "ALL",
      "NONE",
      "thisisawordwithmorethanfivehundredandtwelvecharactersitishardtowritesomethinglongbecauseidontknwowhatishouldwritesoidecidedtowritedownashortstoryaboutanythingwhichisnotrealonceuponatimetherewasalittlesoftwareprogrammerhewasinsanebecausehehastofindwordswhicharelongerthanfivehunderdandtwelvecharactersandhefoundithardtowritesuchlongwordsbuthediditandhedecidedtowritedownashortstoryaboutalittleprogrammerasoftwareprogrammerwhohastowritetestsfortestingfunctionwhichteststehlengthofstringsandaftmanymanymanycharactershesolvedtheproblem",
      "bla%sfoo",
      nullptr
   };

   static const char* allowed[] = {
      "forbiddencharacterwithingthestring",
      "forbiddencharacteratthebeginning",
      "EMPLATE",
      "boutanythingwhichisnotrealonceuponatimetherewasalittlesoftwareprogrammerhewasinsanebecausehehastofindwordswhicharelongerthanfivehunderdandtwelvecharactersandhefoundithardtowritesuchlongwordsbuthediditandhedecidedtowritedownashortstoryaboutalittleprogrammerasoftwareprogrammerwhohastowritetestsfortestingfunctionwhichteststehlengthofstringsandaftmanymanymanycharactershesolvedtheproblem",
      nullptr
   };

   for (i=0; denied[i] != nullptr; i++) {
      if (verify_str_key(
            &answer_list, denied[i], MAX_VERIFY_STRING, "test", KEY_TABLE) == STATUS_OK) {
         printf("%s should be forbidden\n",  denied[i]);
         ret++;
      }
   }

   for (i=0; allowed[i] != nullptr; i++) {
      if (verify_str_key(
            &answer_list, allowed[i], MAX_VERIFY_STRING, "test", KEY_TABLE) != STATUS_OK) {
         printf("%s should be allowed\n",  allowed[i]);
         ret++;
      }
   }

   ret += test_verify_host_name();
   ret += test_resolve_hostname();
   ret += test_escape_string();

   if (ret == 0) {
      printf("PASS: test solved!\n");
   } else {
      printf("FAILED: test NOT solved!\n");
   }

   lFreeList(&answer_list);

   return ret;

}

