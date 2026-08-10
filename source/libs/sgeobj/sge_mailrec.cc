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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Mail recipients and the occasions mail is sent on
 *
 * @see sge_mailrec.h
 */

#include "uti/sge_bitfield.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_mailrec.h"

#include "symbols.h"
#include "get_path.h"

#include "msg_common.h"

/**
 * @brief Parse a list of mail recipients
 *
 * Parse a list of mail recipients.
 * user[`@host`][,user[`@host`],...]
 *
 * @param lpp MR_Type list
 * @param mail_str stringlist of mail recipients
 *
 * @return error state 0 - success >0 - error
 *
 * @note MT-NOTE: mailrec_parse() is MT safe
 *
 * @see #mailrec_unparse
 */
int mailrec_parse(lList **lpp, const char *mail_str) {
   DENTER(TOP_LAYER);

   const char *user;
   const char *host;
   char **str_str;
   char **pstr;
   lListElem *ep;
   const lListElem *tmp;
   char *mail;
   struct saved_vars_s *context;

   if (!lpp) {
      DRETURN(1);
   }

   mail = sge_strdup(nullptr, mail_str);
   if (!mail) {
      *lpp = nullptr;
      DRETURN(2);
   }
   str_str = string_list(mail, ",", nullptr);
   if (!str_str || !*str_str) {
      *lpp = nullptr;
      sge_free(&mail);
      DRETURN(3);
   }

   if (!*lpp) {
      *lpp = lCreateList("mail_list", MR_Type);
      if (!*lpp) {
         sge_free(&mail);
         sge_free(&str_str);
         DRETURN(4);
      }
   }

   for (pstr = str_str; *pstr; pstr++) {
      context = nullptr;
      user = sge_strtok_r(*pstr, "@", &context);
      host = sge_strtok_r(nullptr, "@", &context);
      if ((tmp=lGetElemStr(*lpp, MR_user, user))) {
         if (!sge_strnullcmp(host, lGetHost(tmp, MR_host))) {
            /* got this mail adress twice */
            sge_free_saved_vars(context);
            continue;
         }
      }

      /* got a new adress - add it */
      ep = lCreateElem(MR_Type);
      lSetString(ep, MR_user, user);
      if (host) 
         lSetHost(ep, MR_host, host);
      lAppendElem(*lpp, ep);

      sge_free_saved_vars(context);
   }

   sge_free(&mail);
   sge_free(&str_str);
   DRETURN(0);
}

/**
 * @brief Build a string of mail reipients
 *
 * Build a string of mail reipients ("`user@host`,user,...")
 *
 * @param head MR_Type list
 * @param mail_str buffer to be filled
 * @param mail_str_len size of buffer
 *
 * @return error state 0 - success >0 - error
 *
 * @see #mailrec_parse
 */
int mailrec_unparse(const lList *head, char *mail_str, unsigned int mail_str_len) {
   int len=0;
   int comma_needed = 0; /* whether we need to insert a comma */
   char tmpstr[1000];    /* need 1000 for brain damaged mail addresse(e)s */
   const char *h;
   const char *u;

   if (!head) {
      strcpy(mail_str, MSG_NONE);
      return 0;
   }

   *mail_str = '\0';

   for_each_ep_lv(elem,head) {
      if (!(u = lGetString(elem, MR_user)))
         u = MSG_SMALLNULL;

      if (!(h = lGetHost(elem, MR_host)))
         snprintf(tmpstr, sizeof(tmpstr), "%s", u);
      else
         snprintf(tmpstr, sizeof(tmpstr), "%s@%s", u, h);

      if (strlen(tmpstr)+len+1+comma_needed > mail_str_len)
         return 1;              /* forgot the rest */

      if (comma_needed)
         strcat(mail_str, ",");
      else
         comma_needed = 1;      /* need comma after first mailaddress */

      strcat(mail_str, tmpstr);
   }
   return 0;
}

/**
 * @brief Render the mail options as the letters a user writes
 *
 * @param opt the mail option bit field
 * @param[out] string receives the letters, appended
 * @return true when something was written
 */
bool sge_mailopt_to_dstring(uint32_t opt, dstring *string) {
   DENTER(TOP_LAYER);

   bool success = true;

   if (VALID(MAIL_AT_ABORT, opt)) {
      sge_dstring_append_char(string, 'a');
   } 
   if (VALID(MAIL_AT_BEGINNING, opt)) {
      sge_dstring_append_char(string, 'b');
   } 
   if (VALID(MAIL_AT_EXIT, opt)) {
      sge_dstring_append_char(string, 'e');
   } 
   if (VALID(NO_MAIL, opt)) {
      sge_dstring_append_char(string, 'n');
   } 
   if (VALID(MAIL_AT_SUSPENSION, opt)) {
      sge_dstring_append_char(string, 's');
   } 
   DRETURN(success);
}

/***********************************************************************/
/* MT-NOTE: sge_parse_mail_options() is MT safe */
/**
 * @brief Parse the `-m` letters into the mail option bit field
 *
 * @param[out] alpp receives the message naming an unknown letter
 * @param mail_str the letters the user wrote
 * @param prog_number the calling program, since not every client accepts every letter
 * @return the mail option bit field
 */
int sge_parse_mail_options(lList **alpp, const char *mail_str, uint32_t prog_number) {
   DENTER(TOP_LAYER);

   int i, j;
   int mail_opt = 0;

   i = strlen(mail_str);

   for (j = 0; j < i; j++) {
      if ((char) mail_str[j] == 'a') {
         mail_opt = mail_opt | MAIL_AT_ABORT;
      } else if ((char) mail_str[j] == 'b') {
         mail_opt = mail_opt | MAIL_AT_BEGINNING;
      } else if ((char) mail_str[j] == 'e') {
         mail_opt = mail_opt | MAIL_AT_EXIT;
      } else if ((char) mail_str[j] == 'n') {
         mail_opt = mail_opt | NO_MAIL;
      } else if ((char) mail_str[j] == 's') {
         if (prog_number == QRSUB) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                   MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-m");
            DRETURN(0);
         }
         mail_opt = mail_opt | MAIL_AT_SUSPENSION;
      } else {
         DRETURN(0);
      }
   }

   DRETURN(mail_opt);
}
