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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cctype>
#include <cstring>

#include "comm/cl_communication.h"

#include "uti/sge_log.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "comm/cl_commlib.h"

#include "msg_qmaster.h"

/****** sge_utility/verify_str_key() *******************************************
*  NAME
*     verify_str_key() -- Generic function for verifying object names
*
*  SYNOPSIS
*     an_status_t verify_str_key(lList **alpp, const char *str, size_t 
*     str_length, const char *name, int table) 
*
*  FUNCTION
*     Verifies object names. The follwing verification tables are 
*     used
*
*        QSUB_TABLE    job account strings 
*           (see qsub(1) -A for characters not allowed)
*        QSUB_TABLE    job name
*           (see qsub(1) -N for characters not allowed)
*        KEY_TABLE     parallel environemnt names
*           (see sge_pe(5))
*        KEY_TABLE     calendar names 
*           (see calendar_conf(5))
*        KEY_TABLE     cluster queue names 
*           (see queue_conf(5))
*        KEY_TABLE     project names
*           (see project(5))
*        KEY_TABLE     userset names
*           (see access_list(5))
*        KEY_TABLE     department names
*           (see access_list(5))
*        KEY_TABLE     checkpoint interface name
*           (see checkpoint(5))
*        KEY_TABLE     user name
*           (see user(5))
*        KEY_TABLE     hostgroup names
*           (see hostgroup(5))
*
*        KEY_TABLE     event client name (internal purposes only)
*        KEY_TABLE     JAPI session key (internal purposes only)
*
*           Note, there is test_sge_utility
*
*  INPUTS
*     lList **alpp      - answer list
*     const char *str   - string to be verified
*     size_t str_length - length of the string to be verified
*     const char *name  - verbal description of the object
*     int table         - verification table to be used
*
*  RESULT
*     an_status_t - STATUS_OK upon success
*
*  NOTES
*     MT-NOTE: verify_str_key() is MT safe 
* 
*  SEE ALSO
*     There is a module test (test_sge_utility) for verify_str_key().
*******************************************************************************/
an_status_t verify_str_key(
   lList **alpp, const char *str, size_t str_length, const char *name, int table) 
{
   static const char *begin_strings[2][3];
   static const char *mid_strings[2][20];

   static char begin_chars[2][3] =
      { { '.', '#', 0 },
        { 0, 0, 0 } };

   static const char mid_characters[2][20] =
      { { '\n', '\t', '\r', ' ', '/', ':', '\'', '\"', '\\', '[', ']', '{', '}', '|', '(', ')', '@', '%', ',', 0},  /* KEY_TABLE  */
        { '\n', '\t', '\r', '/', ':', '@', '\\', '*', '?', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} };                      /* QSUB_TABLE */
   static const char* keyword[] = { "NONE", "ALL", "TEMPLATE", nullptr };
   static const char* keyword_strings[4];

   static int initialized = 0;
   char forbidden_char;
   const char* forbidden_string;
   int i;

   table = table -1;
   if (!initialized) {
      begin_strings[0][0] = MSG_GDI_KEYSTR_DOT;
      begin_strings[0][1] = MSG_GDI_KEYSTR_HASH;
      begin_strings[0][2] = nullptr;
      begin_strings[1][0] = nullptr;
      begin_strings[1][1] = nullptr;
      begin_strings[1][2] = nullptr;

      mid_strings[0][0] = MSG_GDI_KEYSTR_RETURN;
      mid_strings[0][1] = MSG_GDI_KEYSTR_TABULATOR;
      mid_strings[0][2] = MSG_GDI_KEYSTR_CARRIAGERET;
      mid_strings[0][3] = MSG_GDI_KEYSTR_SPACE;
      mid_strings[0][4] = MSG_GDI_KEYSTR_SLASH;
      mid_strings[0][5] = MSG_GDI_KEYSTR_COLON;
      mid_strings[0][6] = MSG_GDI_KEYSTR_QUOTE;
      mid_strings[0][7] = MSG_GDI_KEYSTR_DBLQUOTE;
      mid_strings[0][8] = MSG_GDI_KEYSTR_BACKSLASH;
      mid_strings[0][9] = MSG_GDI_KEYSTR_BRACKETS;
      mid_strings[0][10] = MSG_GDI_KEYSTR_BRACKETS;
      mid_strings[0][11] = MSG_GDI_KEYSTR_BRACES;
      mid_strings[0][12] = MSG_GDI_KEYSTR_BRACES;
      mid_strings[0][13] = MSG_GDI_KEYSTR_PIPE;
      mid_strings[0][14] = MSG_GDI_KEYSTR_PARENTHESIS;
      mid_strings[0][15] = MSG_GDI_KEYSTR_PARENTHESIS;
      mid_strings[0][16] = MSG_GDI_KEYSTR_AT;
      mid_strings[0][17] = MSG_GDI_KEYSTR_PERCENT;
      mid_strings[0][18] = MSG_GDI_KEYSTR_COMMA;
      mid_strings[0][19] = nullptr;
      mid_strings[1][0] = MSG_GDI_KEYSTR_RETURN;
      mid_strings[1][1] = MSG_GDI_KEYSTR_TABULATOR;
      mid_strings[1][2] = MSG_GDI_KEYSTR_CARRIAGERET;
      mid_strings[1][3] = MSG_GDI_KEYSTR_SLASH;
      mid_strings[1][4] = MSG_GDI_KEYSTR_COLON;
      mid_strings[1][5] = MSG_GDI_KEYSTR_AT;
      mid_strings[1][6] = MSG_GDI_KEYSTR_BACKSLASH;
      mid_strings[1][7] = MSG_GDI_KEYSTR_ASTERISK;
      mid_strings[1][8] = MSG_GDI_KEYSTR_QUESTIONMARK;
      mid_strings[1][9] = nullptr;

      keyword_strings[0] = MSG_GDI_KEYSTR_KEYWORD;
      keyword_strings[1] = MSG_GDI_KEYSTR_KEYWORD;
      keyword_strings[2] = MSG_GDI_KEYSTR_KEYWORD;
      keyword_strings[3] = nullptr;

      initialized = 1;
   }

   if (str == nullptr) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_NULL_S, name));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      return STATUS_EUNKNOWN;
   }

   /* check string length first, if too long -> error */
   if (strlen(str) > str_length) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_LENGTH_U, sge_u32c(str_length)));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      return STATUS_EUNKNOWN;
   }

   /* check first character */
   i = -1;
   while ((forbidden_char = begin_chars[table][++i])) {
      if (str[0] == forbidden_char) {
         if (isprint((int) forbidden_char)) {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_FIRSTCHAR_SC,
                           begin_strings[table][i], begin_chars[table][i]));
         } else {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_FIRSTCHAR_S, 
                           begin_strings[table][i]));
         }
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         return STATUS_EUNKNOWN;
      }
   }

   /* check all characters in str */
   i = -1;
   while ((forbidden_char = mid_characters[table][++i])) {
      if (strchr(str, forbidden_char)) {
         if (isprint((int) forbidden_char)) {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_MIDCHAR_SC,
                           mid_strings[table][i], mid_characters[table][i]));
         } else {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_MIDCHAR_S, 
                           mid_strings[table][i]));
         }
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         return STATUS_EUNKNOWN;
      }
   }

   /* reject invalid keywords */
   i = -1;
   while ((forbidden_string = keyword[++i])) {
      if (!strcasecmp(str, forbidden_string)) {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_KEYSTR_KEYWORD_SS, 
                        keyword_strings[i],
            forbidden_string));
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         return STATUS_EUNKNOWN;
      }
   }

   return STATUS_OK;
}

/****** sge_utility/verify_host_name() *****************************************
*  NAME
*     verify_host_name() -- verify a hostname
*
*  SYNOPSIS
*     bool 
*     verify_host_name(lList **answer_list, const char *host_name) 
*
*  FUNCTION
*     Verifies if a hostname is correct (regarding maximum length etc.).
*
*  INPUTS
*     lList **answer_list   - answer list to pass back error messages
*     const char *host_name - the hostname to verify
*
*  RESULT
*     bool - true on success,
*            false on error with error message in answer_list
*
*  NOTES
*     MT-NOTE: verify_host_name() is MT safe 
*******************************************************************************/
bool verify_host_name(lList **answer_list, const char *host_name)
{
   bool ret = true;

   if (host_name == nullptr || *host_name == '\0') {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_HOSTNAME_NOT_EMPTY);
      ret = false;
   }

   if (ret) {
      if (strlen(host_name) > CL_MAXHOSTNAMELEN_LENGTH) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_HOSTNAME_NOT_EMPTY);
      }
   }

   /* TODO: further verification (e.g. character set) */

   return ret;
}

int reresolve_qualified_hostname(void) {
   int ret = CL_RETVAL_OK;

   char unique_hostname[256];
   const char *qh = component_get_qualified_hostname();

#if 1
   // lazy initialize of the qualified hostname store in uti_state
   if (qh == nullptr) {
      stringT tmp_str;
      struct hostent *hent = nullptr;

      SGE_ASSERT((gethostname(tmp_str, sizeof(tmp_str)) == 0));
      SGE_ASSERT(((hent = sge_gethostbyname(tmp_str,nullptr)) != nullptr));
      component_set_qualified_hostname(hent->h_name);
      sge_free_hostent(&hent);
      qh = component_get_qualified_hostname();
   }
#endif

   // (re)resolve
   ret = getuniquehostname(qh, unique_hostname, 0);
   if (ret != CL_RETVAL_OK) {
      return ret;
   }

   // store the re(resolved) name
   component_set_qualified_hostname(unique_hostname);
   return ret;
}

