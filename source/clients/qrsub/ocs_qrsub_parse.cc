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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <pwd.h>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_job.h"

#include "symbols.h"
#include "basis_types.h"
#include "parse_qsub.h"
#include "usage.h"
#include "ocs_qrsub_parse.h"

#include "msg_common.h"


bool sge_parse_qrsub(lList *pcmdline, lList **alpp, lListElem **ar)
{
   lListElem *ep = nullptr, *next_ep = nullptr;
   lList *lp = nullptr;
   DENTER(TOP_LAYER);

   /*  -help 	 print this help */
   if ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-help"))) {
      lRemoveElem(pcmdline, &ep);
      sge_usage(QRSUB, stdout);
      sge_exit(0);
   }

   /*  -a date_time 	 start time in [[CC]YY]MMDDhhmm[.SS] SGE_ULONG */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-a"))) {
      lSetUlong64(*ar, AR_start_time, lGetUlong64(ep, SPA_argval_lUlong64T));
      lRemoveElem(pcmdline, &ep);
   }

   /*  -e date_time 	 end time in [[CC]YY]MMDDhhmm[.SS] SGE_ULONG*/
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-e"))) {
      lSetUlong64(*ar, AR_end_time, lGetUlong64(ep, SPA_argval_lUlong64T));
      lRemoveElem(pcmdline, &ep);
   }

   /*  -d time 	 duration in TIME format SGE_ULONG */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-d"))) {
      lSetUlong64(*ar, AR_duration, lGetUlong64(ep, SPA_argval_lUlong64T));
      lRemoveElem(pcmdline, &ep);
   }
   
   /*  -w e/v 	 validate availability of AR request, default e SGE_ULONG */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-w"))) {
      lSetUlong(*ar, AR_verify, lGetInt(ep, SPA_argval_lIntT));
      lRemoveElem(pcmdline, &ep);
   }
  
   /*  -N name 	 AR name SGE_STRING */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-N"))) {
      lSetString(*ar, AR_name, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(pcmdline, &ep);
   }
      
   /*  -A account_string 	 AR name in accounting record SGE_STRING */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-A"))) {
      lSetString(*ar, AR_account, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(pcmdline, &ep);
   }
     
   /*  -l resource_list 	 request the given resources  SGE_LIST */
   parse_list_simple(pcmdline, "-l", *ar, AR_resource_list, 0, 0, FLG_LIST_APPEND);
   centry_list_remove_duplicates(lGetListRW(*ar, AR_resource_list));

   /*  -u wc_user 	       access list SGE_LIST */
   /*  -u ! wc_user TBD: Think about eval_expression support in compare allowed and excluded lists */
   parse_list_simple(pcmdline, "-u", *ar, AR_acl_list, ARA_name, 0, FLG_LIST_MERGE);
   /*  -u ! list separation */
   lp = lGetListRW(*ar,  AR_acl_list);
   next_ep = lFirstRW(lp);
   while ((ep = next_ep)) {
      bool is_xacl = false;
      const char *name = lGetString(ep, ARA_name);

      next_ep = lNextRW(ep);
      if (name[0] == '!') { /* move this element to xacl_list */
         is_xacl = true;
         name++;
      }

      if (!is_hgroup_name(name)) {
         struct passwd *pw;
         struct passwd pw_struct;
         char *buffer;
         int size;
         stringT group;

         size = get_pw_buffer_size();
         buffer = sge_malloc(size);
         pw = sge_getpwnam_r(name, &pw_struct, buffer, size);
         
         if (pw == nullptr) {
           answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_USER_XISNOKNOWNUSER_S, name);
           sge_free(&buffer);
           DRETURN(false);
         }
         sge_gid2group(pw->pw_gid, group, MAX_STRING_SIZE, MAX_NIS_RETRIES);
         lSetString(ep, ARA_group, group);
         sge_free(&buffer);
      }

      if (is_xacl) {
         lListElem *new_ep = lAddSubStr(*ar, ARA_name, name, AR_xacl_list, ARA_Type);
         lSetString(new_ep, ARA_group, lGetString(ep, ARA_group));
         lRemoveElem(lp, &ep);
      }

   }

   /*  -q wc_queue_list 	 reserve in queue(s) SGE_LIST */
   parse_list_simple(pcmdline, "-q", *ar, AR_queue_list, 0, 0, FLG_LIST_APPEND);

  /*    -pe pe_name slot_range reserve slot range for parallel jobs */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-pe"))) {
      lSetString(*ar, AR_pe, lGetString(ep, SPA_argval_lStringT)); /* SGE_STRING, */
      lSwapList(*ar, AR_pe_range, ep, SPA_argval_lListT);       /* SGE_LIST */
      lRemoveElem(pcmdline, &ep);
   }
   /*   AR_master_queue_list  -masterq wc_queue_list, SGE_LIST bind master task to queue(s) */
   parse_list_simple(pcmdline, "-masterq", *ar, AR_master_queue_list, 0, 0, FLG_LIST_APPEND);

   /*  -ckpt ckpt-name 	 reserve in queue with ckpt method SGE_STRING */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-ckpt"))) {
      lSetString(*ar, AR_checkpoint_name, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(pcmdline, &ep);
   }
   
   /*  -m b/e/a/n 	 define mail notification events SGE_ULONG */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-m"))) {
      u_long32 ul;
      u_long32 old_mail_opts;

      ul = lGetInt(ep, SPA_argval_lIntT);
      if  ((ul & NO_MAIL)) {
         lSetUlong(*ar, AR_mail_options, 0);
      } else {
         old_mail_opts = lGetUlong(*ar, AR_mail_options);
         lSetUlong(*ar, AR_mail_options, ul | old_mail_opts);
      }
      lRemoveElem(pcmdline, &ep);
   }

   /*   -M user[@host],... 	 notify these e-mail addresses SGE_LIST*/
   parse_list_simple(pcmdline, "-M", *ar, AR_mail_list, MR_host, MR_user, FLG_LIST_MERGE);

   /*  -he yes/no 	 hard error handling SGE_ULONG */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-he"))) {
      lSetUlong(*ar, AR_error_handling, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(pcmdline, &ep);
   }

   /*   -now 	 reserve in queues with qtype interactive  SGE_ULONG */
   while ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, "-now"))) {
      u_long32 ar_now = lGetUlong(*ar, AR_type);
      if(lGetInt(ep, SPA_argval_lIntT)) {
         JOB_TYPE_SET_IMMEDIATE(ar_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(ar_now);
      }

      lSetUlong(*ar, AR_type, ar_now);

      lRemoveElem(pcmdline, &ep);
   }

  /* Remove the script elements. They are not stored in the ar structure */
  if ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, STR_PSEUDO_SCRIPT))) {
      lRemoveElem(pcmdline, &ep);
   }

   if ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, STR_PSEUDO_SCRIPTLEN))) {
      lRemoveElem(pcmdline, &ep);
   }

   if ((ep = lGetElemStrRW(pcmdline, SPA_switch_val, STR_PSEUDO_SCRIPTPTR))) {
      lRemoveElem(pcmdline, &ep);
   }

   ep = lFirstRW(pcmdline);   
   if(ep) {
      const char *option = lGetString(ep,SPA_switch_val);
      /* as jobarg are stored no switch values, need to be filtered */ 
      if(sge_strnullcmp(option, "jobarg") != 0) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                              lGetString(ep,SPA_switch_val)); 
      } else {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 SFNMAX, MSG_PARSE_INVALIDOPTIONARGUMENT);
       }
      DRETURN(false);
   }

   if (lGetUlong64(*ar, AR_start_time) == 0 && lGetUlong64(*ar, AR_end_time) != 0 && lGetUlong64(*ar, AR_duration) != 0) {
      lSetUlong64(*ar, AR_start_time, lGetUlong64(*ar, AR_end_time) - lGetUlong64(*ar, AR_duration));
   } else if (lGetUlong64(*ar, AR_start_time) != 0 && lGetUlong64(*ar, AR_end_time) == 0 && lGetUlong64(*ar, AR_duration) != 0) {
      lSetUlong64(*ar, AR_end_time, duration_add_offset(lGetUlong64(*ar, AR_start_time), lGetUlong64(*ar, AR_duration)));
      lSetUlong64(*ar, AR_duration, lGetUlong64(*ar, AR_end_time) - lGetUlong64(*ar, AR_start_time));
   } else if (lGetUlong64(*ar, AR_start_time) != 0 && lGetUlong64(*ar, AR_end_time) != 0 && lGetUlong64(*ar, AR_duration) == 0) {
      lSetUlong64(*ar, AR_duration, lGetUlong64(*ar, AR_end_time) - lGetUlong64(*ar, AR_start_time));
   }

   DRETURN(true);
}
