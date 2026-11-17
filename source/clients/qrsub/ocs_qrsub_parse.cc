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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief qrsub - turns the command line into an advance reservation object
 */

#include <pwd.h>
#include <cstdarg>
#include <cstdlib>

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_hostname.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qref.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_ARA.h"

#include "symbols.h"
#include <cinttypes>
#include "parse_qsub.h"
#include "usage.h"
#include "ocs_qrsub_parse.h"

#include "msg_common.h"
#include "ocs_AdvanceReservation.h"


/** @brief Build an advance reservation from the parsed `qrsub` command line
 *
 * @param pcmdline the parsed options (`SPA_Type`)
 * @param alpp used to return error messages
 * @param ar the advance reservation the options are written into
 * @return true on success; false with `alpp` filled otherwise
 */
bool sge_parse_qrsub(lList *pcmdline, lList **alpp, const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);

   lListElem *ep = nullptr;
   /* -help print this help */
   if ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-help"))) {
      lRemoveElem(pcmdline, &ep);
      sge_usage(QRSUB, stdout);
      sge_exit(0);
   }

   /* -a date_time start time in [[CC]YY]MMDDhhmm[.SS] SGE_ULONG */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-a"))) {
      ocs::tom::set<AR_start_time>(ar, ocs::tom::get<SPA_argval_lUlong64T>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -btype host | slot
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-btype");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      // nullptr means failure, as in the other -b* branches (the check was inverted, CS-3119)
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_new_type>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bamount number
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bamount");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_amount>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -binstance set | env | pe
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-binstance");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_instance>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bunit binding_unit
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bunit");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_unit>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bfilter topo_string
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bfilter");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_filter>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bsort sort_string
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bsort");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_sort>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bstart
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bstart");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_start>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bstop
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bstop");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_stop>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   // -bstrategy
   ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-bstrategy");
   if (ep != nullptr) {
      lListElem *binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (binding_elem == nullptr) {
         DRETURN(false);
      }
      ocs::tom::set<BN_strategy>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }


   /*  -e date_time 	 end time in [[CC]YY]MMDDhhmm[.SS] SGE_ULONG*/
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-e"))) {
      ocs::tom::set<AR_end_time>(ar, ocs::tom::get<SPA_argval_lUlong64T>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   /*  -d time duration in TIME format SGE_ULONG */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-d"))) {
      ocs::tom::set<AR_duration>(ar, ocs::tom::get<SPA_argval_lUlong64T>(ep));
      lRemoveElem(pcmdline, &ep);
   }
   
   /*  -w e/v 	 validate availability of AR request, default e SGE_ULONG */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-w"))) {
      ocs::tom::set<AR_verify>(ar, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(pcmdline, &ep);
   }
  
   /*  -N name 	 AR name SGE_STRING */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-N"))) {
      ocs::tom::set<AR_name>(ar, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(pcmdline, &ep);
   }
      
   /*  -A account_string AR name in accounting record SGE_STRING */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-A"))) {
      ocs::tom::set<AR_account>(ar, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(pcmdline, &ep);
   }
     
   /*  -P project_name project of the AR SGE_STRING */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-P"))) {
      ocs::tom::set<AR_project>(ar, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   /*  -l resource_list request the given resources  SGE_LIST */
   /*  a resource given twice keeps its last value, merged by parse_list_simple (CS-3053) */
   parse_list_simple(pcmdline, "-l", ar, AR_resource_list, 0, 0, FLG_LIST_APPEND);

   /*  -u wc_user access list SGE_LIST */
   /*  -u ! wc_user TBD: Think about eval_expression support in compare allowed and excluded lists */
   /*  an access list is a set: a name given twice is kept once, merged by parse_list_simple (CS-3053) */
   parse_list_simple(pcmdline, "-u", ar, AR_acl_list, ARA_name, 0, FLG_LIST_APPEND);
   /*  -u ! list separation */
   ocs::tom::List<ocs::tom::ARA> lp = ocs::tom::get<AR_acl_list>(ar);
   for (auto acl = lp.front(); acl;) {
      bool is_xacl = false;
      const char *name = ocs::tom::get<ARA_name>(acl);

      const auto next_acl = acl.next();
      if (name[0] == '!') { /* move this element to xacl_list */
         is_xacl = true;
         name++;
      }

      if (!ocs::is_hgroup_name(name)) {
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
         ocs::tom::set<ARA_group>(acl, group);
         sge_free(&buffer);
      }

      if (is_xacl) {
         ocs::tom::Element<ocs::tom::ARA> new_ep = ocs::tom::emplace_back_in<AR_xacl_list, ARA_name>(ar, name);
         ocs::tom::set<ARA_group>(new_ep, ocs::tom::get<ARA_group>(acl));
         lp.erase(acl);
      }

      acl = next_acl;
   }
   ocs_object_list_remove_repeated_keys(ocs::tom::get<AR_xacl_list>(ar), ARA_name);

   /*  -q wc_queue_list 	 reserve in queue(s) SGE_LIST */
   /*  a queue given twice is kept once, merged by parse_list_simple (CS-3053) */
   parse_list_simple(pcmdline, "-q", ar, AR_queue_list, 0, 0, FLG_LIST_APPEND);

  /*    -pe pe_name slot_range reserve slot range for parallel jobs */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-pe"))) {
      ocs::tom::set<AR_pe>(ar, ocs::tom::get<SPA_argval_lStringT>(ep)); /* SGE_STRING, */
      lSwapList(ar, AR_pe_range, ep, SPA_argval_lListT);       /* SGE_LIST */
      lRemoveElem(pcmdline, &ep);
   }
   /*  -par allocation_rule  reserve the slots with this rule instead of the PE's allocation_rule */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-par"))) {
      /* The scope the tokenizer stored in SPA_argval_lCharT is always the global one: qrsub has no
       * -scope switch, and a -scope given anyway stays in pcmdline and is rejected further down. */
      ocs::tom::set<AR_allocation_rule>(ar, ocs::tom::get<SPA_switch_arg>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   /*   AR_master_queue_list  -masterq wc_queue_list, SGE_LIST bind master task to queue(s) */
   parse_list_simple(pcmdline, "-masterq", ar, AR_master_queue_list, 0, 0, FLG_LIST_APPEND);

   /*  -ckpt ckpt-name 	 reserve in queue with ckpt method SGE_STRING */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-ckpt"))) {
      ocs::tom::set<AR_checkpoint_name>(ar, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(pcmdline, &ep);
   }
   
   /*  -m b/e/a/n 	 define mail notification events SGE_ULONG */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-m"))) {
      uint32_t ul;
      uint32_t old_mail_opts;

      ul = ocs::tom::get<SPA_argval_lIntT>(ep);
      if  ((ul & NO_MAIL)) {
         ocs::tom::set<AR_mail_options>(ar, 0);
      } else {
         old_mail_opts = ocs::tom::get<AR_mail_options>(ar);
         ocs::tom::set<AR_mail_options>(ar, ul | old_mail_opts);
      }
      lRemoveElem(pcmdline, &ep);
   }

   /*   -M user[@host],... 	 notify these e-mail addresses SGE_LIST*/
   parse_list_simple(pcmdline, "-M", ar, AR_mail_list, MR_host, MR_user, FLG_LIST_MERGE);

   /*  -he yes/no 	 hard error handling SGE_ULONG */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-he"))) {
      ocs::tom::set<AR_error_handling>(ar, ocs::tom::get<SPA_argval_lUlongT>(ep));
      lRemoveElem(pcmdline, &ep);
   }

   /*   -now 	 reserve in queues with qtype interactive  SGE_ULONG */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, "-now"))) {
      uint32_t ar_now = ocs::tom::get<AR_type>(ar);
      if(ocs::tom::get<SPA_argval_lIntT>(ep)) {
         JOB_TYPE_SET_IMMEDIATE(ar_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(ar_now);
      }

      ocs::tom::set<AR_type>(ar, ar_now);

      lRemoveElem(pcmdline, &ep);
   }

  /* Remove the script elements. They are not stored in the ar structure */
  if ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, STR_PSEUDO_SCRIPT))) {
      lRemoveElem(pcmdline, &ep);
   }

   if ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, STR_PSEUDO_SCRIPTLEN))) {
      lRemoveElem(pcmdline, &ep);
   }

   if ((ep = ocs::tom::find_first_rw<SPA_switch_val>(pcmdline, STR_PSEUDO_SCRIPTPTR))) {
      lRemoveElem(pcmdline, &ep);
   }

   ep = lFirstRW(pcmdline);   
   if(ep) {
      const char *option = ocs::tom::get<SPA_switch_val>(ep);
      /* as jobarg are stored no switch values, need to be filtered */ 
      if(sge_strnullcmp(option, "jobarg") != 0) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                              ocs::tom::get<SPA_switch_val>(ep)); 
      } else {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 SFNMAX, MSG_PARSE_INVALIDOPTIONARGUMENT);
       }
      DRETURN(false);
   }

   if (ocs::tom::get<AR_start_time>(ar) == 0 && ocs::tom::get<AR_end_time>(ar) != 0 &&
       ocs::tom::get<AR_duration>(ar) != 0) {
      ocs::tom::set<AR_start_time>(ar, ocs::tom::get<AR_end_time>(ar) - ocs::tom::get<AR_duration>(ar));
   } else if (ocs::tom::get<AR_start_time>(ar) != 0 && ocs::tom::get<AR_end_time>(ar) == 0 &&
              ocs::tom::get<AR_duration>(ar) != 0) {
      ocs::tom::set<AR_end_time>(ar, duration_add_offset(ocs::tom::get<AR_start_time>(ar),
                                                         ocs::tom::get<AR_duration>(ar)));
      ocs::tom::set<AR_duration>(ar, ocs::tom::get<AR_end_time>(ar) - ocs::tom::get<AR_start_time>(ar));
   } else if (ocs::tom::get<AR_start_time>(ar) != 0 && ocs::tom::get<AR_end_time>(ar) != 0 &&
              ocs::tom::get<AR_duration>(ar) == 0) {
      ocs::tom::set<AR_duration>(ar, ocs::tom::get<AR_end_time>(ar) - ocs::tom::get<AR_start_time>(ar));
   }

   DRETURN(true);
}
