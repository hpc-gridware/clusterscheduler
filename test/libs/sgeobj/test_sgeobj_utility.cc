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

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"

#include "basis_types.h"

/**
 * @brief Security regression test for CS-2364 (CWE-22): verify_host_name().
 *
 * Host names become spool file keys (EXECHOST_DIR/<name>) and are not run
 * through verify_str_key(), so verify_host_name() must reject names that are
 * unsafe as a single path component: a '/' anywhere or a leading '.'.
 * Before the fix those were accepted.
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
   return ret;
}

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

   if (ret == 0) {
      printf("PASS: test solved!\n");
   } else {
      printf("FAILED: test NOT solved!\n");
   }

   lFreeList(&answer_list);

   return ret;

}

