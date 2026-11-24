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
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/ocs_parse_util.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qref.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_ARA.h"
#include "sgeobj/tom/ocs_tom_RN.h"

#include "symbols.h"
#include <cinttypes>
#include "parse_qsub.h"
#include "usage.h"
#include "ocs_qrsub_parse.h"

#include "msg_common.h"
#include "ocs_AdvanceReservation.h"


/** @brief Build an advance reservation from the parsed `qrsub` command line
 *
 * @param pcmdline the parsed options (`SPA`); the options used are removed
 * @param alpp used to return error messages
 * @param ar the advance reservation the options are written into
 * @return true on success; false with `alpp` filled otherwise
 */
bool sge_parse_qrsub(const ocs::tom::List<ocs::tom::SPA> pcmdline, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                     const ocs::tom::Element<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SPA> ep;
   /* -help print this help */
   if ((ep = pcmdline.find_first<SPA_switch_val>("-help"))) {
      pcmdline.erase(ep);
      sge_usage(QRSUB, stdout);
      sge_exit(0);
   }

   /* -a date_time start time in [[CC]YY]MMDDhhmm[.SS] SGE_ULONG */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-a"))) {
      ar.set<AR_start_time>(ep.get<SPA_argval_lUlong64T>());
      pcmdline.erase(ep);
   }

   // -btype host | slot
   ep = pcmdline.find_first<SPA_switch_val>("-btype");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      // nullptr means failure, as in the other -b* branches (the check was inverted, CS-3119)
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_new_type>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }

   // -bamount number
   ep = pcmdline.find_first<SPA_switch_val>("-bamount");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_amount>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }

   // -binstance set | env | pe
   ep = pcmdline.find_first<SPA_switch_val>("-binstance");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_instance>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }

   // -bunit binding_unit
   ep = pcmdline.find_first<SPA_switch_val>("-bunit");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_unit>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }

   // -bfilter topo_string
   ep = pcmdline.find_first<SPA_switch_val>("-bfilter");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_filter>(ep.get<SPA_argval_lStringT>());
      pcmdline.erase(ep);
   }

   // -bsort sort_string
   ep = pcmdline.find_first<SPA_switch_val>("-bsort");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_sort>(ep.get<SPA_argval_lStringT>());
      pcmdline.erase(ep);
   }

   // -bstart
   ep = pcmdline.find_first<SPA_switch_val>("-bstart");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_start>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }

   // -bstop
   ep = pcmdline.find_first<SPA_switch_val>("-bstop");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_stop>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }

   // -bstrategy
   ep = pcmdline.find_first<SPA_switch_val>("-bstrategy");
   if (ep) {
      const auto binding_elem = ocs::AdvanceReservation::binding_get_or_create_elem(ar, alpp);
      if (!binding_elem) {
         DRETURN(false);
      }
      binding_elem.set<BN_strategy>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }


   /*  -e date_time 	 end time in [[CC]YY]MMDDhhmm[.SS] SGE_ULONG*/
   while ((ep = pcmdline.find_first<SPA_switch_val>("-e"))) {
      ar.set<AR_end_time>(ep.get<SPA_argval_lUlong64T>());
      pcmdline.erase(ep);
   }

   /*  -d time duration in TIME format SGE_ULONG */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-d"))) {
      ar.set<AR_duration>(ep.get<SPA_argval_lUlong64T>());
      pcmdline.erase(ep);
   }
   
   /*  -w e/v 	 validate availability of AR request, default e SGE_ULONG */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-w"))) {
      ar.set<AR_verify>(ep.get<SPA_argval_lIntT>());
      pcmdline.erase(ep);
   }
  
   /*  -N name 	 AR name SGE_STRING */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-N"))) {
      ar.set<AR_name>(ep.get<SPA_argval_lStringT>());
      pcmdline.erase(ep);
   }
      
   /*  -A account_string AR name in accounting record SGE_STRING */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-A"))) {
      ar.set<AR_account>(ep.get<SPA_argval_lStringT>());
      pcmdline.erase(ep);
   }
     
   /*  -P project_name project of the AR SGE_STRING */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-P"))) {
      ar.set<AR_project>(ep.get<SPA_argval_lStringT>());
      pcmdline.erase(ep);
   }

   /*  -l resource_list request the given resources  SGE_LIST */
   /*  a resource given twice keeps its last value, merged by parse_list_append (CS-3053) */
   parse_list_append<AR_resource_list>(pcmdline, "-l", ar);

   /*  -u wc_user access list SGE_LIST */
   /*  -u ! wc_user TBD: Think about eval_expression support in compare allowed and excluded lists */
   /*  an access list is a set: a name given twice is kept once, merged by parse_list_append (CS-3053) */
   parse_list_append<AR_acl_list>(pcmdline, "-u", ar);
   /*  -u ! list separation */
   ocs::tom::List<ocs::tom::ARA> lp = ar.get<AR_acl_list>();
   for (auto acl = lp.front(); acl;) {
      bool is_xacl = false;
      const char *name = acl.get<ARA_name>();

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
         acl.set<ARA_group>(group);
         sge_free(&buffer);
      }

      if (is_xacl) {
         ocs::tom::Element<ocs::tom::ARA> new_ep = ar.emplace_back_in<AR_xacl_list, ARA_name>(name);
         new_ep.set<ARA_group>(acl.get<ARA_group>());
         lp.erase(acl);
      }

      acl = next_acl;
   }
   ocs_object_list_remove_repeated_keys(ar.get<AR_xacl_list>(), ARA_name);

   /*  -q wc_queue_list 	 reserve in queue(s) SGE_LIST */
   /*  a queue given twice is kept once, merged by parse_list_append (CS-3053) */
   parse_list_append<AR_queue_list>(pcmdline, "-q", ar);

  /*    -pe pe_name slot_range reserve slot range for parallel jobs */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-pe"))) {
      ar.set<AR_pe>(ep.get<SPA_argval_lStringT>()); /* SGE_STRING, */
      // SPA_argval_lListT carries lists of several types; at -pe the parser fills it with a slot range, so the
      // list is taken out and adopted checked as RN here; the range held before is freed by the set
      ar.set<AR_pe_range>(ocs::tom::adopt<ocs::tom::RN>(
         ep.exchange<SPA_argval_lListT>(ocs::tom::ListPtr<ocs::tom::RN>{})));
      pcmdline.erase(ep);
   }
   /*  -par allocation_rule  reserve the slots with this rule instead of the PE's allocation_rule */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-par"))) {
      /* The scope the tokenizer stored in SPA_argval_lCharT is always the global one: qrsub has no
       * -scope switch, and a -scope given anyway stays in pcmdline and is rejected further down. */
      ar.set<AR_allocation_rule>(ep.get<SPA_switch_arg>());
      pcmdline.erase(ep);
   }

   /*   AR_master_queue_list  -masterq wc_queue_list, SGE_LIST bind master task to queue(s) */
   parse_list_append<AR_master_queue_list>(pcmdline, "-masterq", ar);

   /*  -ckpt ckpt-name 	 reserve in queue with ckpt method SGE_STRING */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-ckpt"))) {
      ar.set<AR_checkpoint_name>(ep.get<SPA_argval_lStringT>());
      pcmdline.erase(ep);
   }
   
   /*  -m b/e/a/n 	 define mail notification events SGE_ULONG */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-m"))) {
      uint32_t ul;
      uint32_t old_mail_opts;

      ul = ep.get<SPA_argval_lIntT>();
      if  ((ul & NO_MAIL)) {
         ar.set<AR_mail_options>(0);
      } else {
         old_mail_opts = ar.get<AR_mail_options>();
         ar.set<AR_mail_options>(ul | old_mail_opts);
      }
      pcmdline.erase(ep);
   }

   /*   -M user[@host],... 	 notify these e-mail addresses SGE_LIST*/
   parse_list_merge<AR_mail_list, MR_host, MR_user>(pcmdline, "-M", ar);

   /*  -he yes/no 	 hard error handling SGE_ULONG */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-he"))) {
      ar.set<AR_error_handling>(ep.get<SPA_argval_lUlongT>());
      pcmdline.erase(ep);
   }

   /*   -now 	 reserve in queues with qtype interactive  SGE_ULONG */
   while ((ep = pcmdline.find_first<SPA_switch_val>("-now"))) {
      uint32_t ar_now = ar.get<AR_type>();
      if(ep.get<SPA_argval_lIntT>()) {
         JOB_TYPE_SET_IMMEDIATE(ar_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(ar_now);
      }

      ar.set<AR_type>(ar_now);

      pcmdline.erase(ep);
   }

  /* Remove the script elements. They are not stored in the ar structure */
  if ((ep = pcmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPT))) {
      pcmdline.erase(ep);
   }

   if ((ep = pcmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPTLEN))) {
      pcmdline.erase(ep);
   }

   if ((ep = pcmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPTPTR))) {
      pcmdline.erase(ep);
   }

   ep = pcmdline.front();
   if (ep) {
      const char *option = ep.get<SPA_switch_val>();
      /* as jobarg are stored no switch values, need to be filtered */ 
      if(sge_strnullcmp(option, "jobarg") != 0) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                              ep.get<SPA_switch_val>()); 
      } else {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 SFNMAX, MSG_PARSE_INVALIDOPTIONARGUMENT);
       }
      DRETURN(false);
   }

   if (ar.get<AR_start_time>() == 0 && ar.get<AR_end_time>() != 0 &&
       ar.get<AR_duration>() != 0) {
      ar.set<AR_start_time>(ar.get<AR_end_time>() - ar.get<AR_duration>());
   } else if (ar.get<AR_start_time>() != 0 && ar.get<AR_end_time>() == 0 &&
              ar.get<AR_duration>() != 0) {
      ar.set<AR_end_time>(duration_add_offset(ar.get<AR_start_time>(),
                                                         ar.get<AR_duration>()));
      ar.set<AR_duration>(ar.get<AR_end_time>() - ar.get<AR_start_time>());
   } else if (ar.get<AR_start_time>() != 0 && ar.get<AR_end_time>() != 0 &&
              ar.get<AR_duration>() == 0) {
      ar.set<AR_duration>(ar.get<AR_end_time>() - ar.get<AR_start_time>());
   }

   DRETURN(true);
}
