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

#include <cstring>

#include "basis_types.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_profiling.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_str.h"

#include "gdi/sge_gdi2.h"
#include "gdi/oge_gdi_client.h"

#include "parse_qsub.h"
#include "usage.h"
#include "sig_handlers.h"
#include "msg_common.h"
#include "msg_clients_common.h"

static bool sge_parse_cmdline_qrdel(char **argv, char **envp, lList **ppcmdline, lList **alpp);
static bool sge_parse_qrdel(lList **ppcmdline, lList **ppid_list, lList **alpp);

extern char **environ;

/************************************************************************/
int main(int argc, char **argv) {
   lList *pcmdline = nullptr, *id_list = nullptr;
   lList *alp = nullptr;

   DENTER_MAIN(TOP_LAYER, "qrdel");

   /* Set up the program information name */
   sge_setup_sig_handlers(QRDEL);

   log_state_set_log_gui(1);

   if (gdi_client_setup_and_enroll(QRDEL, MAIN_THREAD, &alp) != AE_OK) {
      answer_list_output(&alp);
      goto error_exit;
   }

   /*
   ** stage 1 of commandline parsing
   */ 
   if (!sge_parse_cmdline_qrdel(++argv, environ, &pcmdline, &alp)) {
      /*
      ** high level parsing error! show answer list
      */
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      goto error_exit;
   }

   /*
   ** stage 2 of command line parsing
   */
   if (!sge_parse_qrdel(&pcmdline, &id_list, &alp)) {
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      goto error_exit;
   }

   if (!id_list) {
      sge_usage(QRDEL, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      goto error_exit;
   }

   alp = sge_gdi2(SGE_AR_LIST, SGE_GDI_DEL, &id_list, nullptr, nullptr);
   lFreeList(&id_list);
   if (answer_list_has_error(&alp)) {
      answer_list_on_error_print_or_exit(&alp, stdout);
      goto error_exit;
   }
   answer_list_on_error_print_or_exit(&alp, stdout);

   gdi_client_shutdown();
   sge_prof_cleanup();
   DRETURN(0);

error_exit:
   gdi_client_shutdown();
   sge_prof_cleanup();
   sge_exit(1);
   DRETURN(1);
}

static bool sge_parse_cmdline_qrdel(char **argv, char **envp, lList **ppcmdline, lList **alpp) {
   char **sp;
   char **rp;

   DENTER(TOP_LAYER);

   rp = argv;
   while (*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, alpp)) != sp)
         continue;
      
      /* -f option */
      if ((rp = parse_noopt(sp, "-f", "--force", ppcmdline, alpp)) != sp)
         continue;

      /* -u option */
      if (!strcmp("-u", *sp)) {
         lList *user_list = nullptr;
         lListElem *ep_opt;

         sp++;
         if (*sp) {
            str_list_parse_from_string(&user_list, *sp, ",");

            ep_opt = sge_add_arg(ppcmdline, 0, lListT, *(sp - 1), *sp);
            lSetList(ep_opt, SPA_argval_lListT, user_list);
            sp++;
         }
         rp = sp;
         continue;
      }
      /* job id's */
      if ((rp = parse_param(sp, "ars", ppcmdline, alpp)) != sp) {
         continue;
      }
      /* oops */
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      sge_usage(QRDEL, stderr);
      DRETURN(false);
   }

   DRETURN(true);
}


static bool sge_parse_qrdel(lList **ppcmdline, lList **ppid_list, lList **alpp)
{
   u_long32 pforce = 0;
   u_long32 helpflag;
   lList *plist = nullptr;
   lList *user_list = nullptr;
   bool ret = true;

   DENTER(TOP_LAYER);

   while (lGetNumberOfElem(*ppcmdline)) {
      const lListElem *ep = nullptr;
      
      if (parse_flag(ppcmdline, "-help",  alpp, &helpflag)) {
         sge_usage(QRDEL, stdout);
         sge_exit(0);
         break;
      }
      if (parse_flag(ppcmdline, "-f", alpp, &pforce)) 
         continue;

      if (parse_multi_stringlist(ppcmdline, "-u", alpp, &user_list, ST_Type, ST_name)) {
         continue;
      }
     
      if (parse_multi_stringlist(ppcmdline, "ars", alpp, &plist, ST_Type, ST_name)) {
         for_each_ep(ep, plist) {
            const char *id = lGetString(ep, ST_name);
            lAddElemStr(ppid_list, ID_str, id, ID_Type);
         }
         lFreeList(&plist);
         continue;
      }
    
      for_each_ep(ep, *ppcmdline) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                                 lGetString(ep,SPA_switch_val)); 

      }
      break;
   }

   if (answer_list_has_error(alpp)) {
      ret = false;
      lFreeList(ppid_list);
      lFreeList(&user_list);
   } else {
      /* fill up ID list */
      if (user_list != nullptr) {
         lListElem *id;

         if (lGetNumberOfElem(*ppid_list) == 0){
            id = lAddElemStr(ppid_list, ID_str, "0", ID_Type);
            lSetList(id, ID_user_list, lCopyList("", user_list));
         } else {
            for_each_rw (id, *ppid_list){
               lSetList(id, ID_user_list, lCopyList("", user_list));
            }
         }
         lFreeList(&user_list);
      }

      if (pforce != 0) {
         lListElem *id;
         for_each_rw (id, *ppid_list){
            lSetUlong(id, ID_force, pforce);
         }
      }
   }

   DRETURN(ret);
}
