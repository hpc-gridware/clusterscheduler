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
 * @brief qdel - deletes jobs
 */

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_Client.h"

#include "comm/commlib.h"

#include "usage.h"
#include "sge_options.h"
#include "msg_common.h"
#include "msg_clients_common.h"

static bool sge_parse_cmdline_qdel(char **argv, char **envp, ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                                   ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static bool sge_parse_qdel(ocs::tom::List<ocs::tom::SPA> cmdline, ocs::tom::ListPtr<ocs::tom::ID> &reflist,
                           uint32_t *pforce, ocs::tom::ListPtr<ocs::tom::ST> &userlist,
                           ocs::tom::ListPtr<ocs::tom::AN> *alpp);

/** @brief The process environment, as the C library defines it
 *
 * Declared here rather than included, and passed to the command line parser so
 * that options which read an environment variable resolve it against the
 * submitting user's environment rather than against `getenv()`.
 */
extern char **environ;

/**
 * @brief qdel: parse the command line and delete the jobs it names
 * @param argc the number of arguments
 * @param argv the arguments
 * @return 0 when every deletion succeeded, 1 otherwise; the process ends with sge_exit()
 */
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qdel");
   int ret = 0;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::SPA> pcmdline;
   ocs::tom::ListPtr<ocs::tom::ID> ref_list;
   ocs::tom::ListPtr<ocs::tom::ST> user_list;
   uint32_t force = 0;
   int wait;
   unsigned long status = 0;
   bool have_master_privileges;
   cl_com_handle_t* handle = nullptr;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QDEL, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      goto error_exit;
   }

   if (!sge_parse_cmdline_qdel(++argv, environ, pcmdline, &alp)) {
      /*
      ** high level parsing error! show answer list
      */
      answer_list_output(&alp);
      pcmdline.reset();
      goto error_exit;
   }

   if (!sge_parse_qdel(*pcmdline, ref_list, &force, user_list, &alp)) {
      /*
      ** low level parsing error! show answer list
      */
      answer_list_output(&alp);
      pcmdline.reset();
      goto error_exit;
   }

   DPRINTF("force     = " sge_u32"\n", force);
   
   if (user_list) {
      if (ref_list->empty()) {
         // created as the free emplace_back() created it
         if (!ref_list) {
            ref_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
         }
         const auto id = ref_list->emplace_back<ID_str>("0");
         id.set<ID_user_list>(std::move(user_list));
      } else {
         // every reference gets its own copy: the request list frees the sublist of each element, so
         // one list shared by several elements was freed once per element (CS-3118)
         for (const auto id : *ref_list) {
            id.set<ID_user_list>(user_list->clone(""));
         }
         user_list.reset();
      }
   }

   /* TODO: remove this code from client, should be hidden in gdi layer 
   **       timeout value should be set in gdi_setup
   */
   handle=cl_com_get_handle(to_cstr(QDEL), 0);
   cl_com_set_synchron_receive_timeout(handle, 10*60);

   /* Are there jobs which should be deleted? */
   if (!ref_list) {
      sge_usage(QDEL, stderr);
      printf("%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      goto error_exit;
   }

   /* Has the user the permission to use the the '-f' (forced) flag */
   have_master_privileges = false;
   if (force == 1) {
      ocs::gdi::Client::sge_gdi_get_permission(&alp, &have_master_privileges, nullptr, nullptr, nullptr);
      alp.reset();
   }
   /* delete the job */
   {
      int delete_mode;

      /* 
       * delete_mode:
       *    1 => admin user used '-f'     
       *         -> forced deletion
       *    7 => non admin user used '-f' 
       *         -> first try normal deletion
       *         -> wait a minute
       *         -> forced deletion (delete_mode==5)
       *    3 => normal qdel
       *         -> normal deletion
       */
      if (force == 1) {
         if (have_master_privileges) {
            delete_mode = 1;
         } else {
            delete_mode = 7;
         }
      } else {
         delete_mode = 3;
      }
      while (delete_mode) {
         int no_forced_deletion = delete_mode & 2;
         bool do_again;
         bool first_try = true;
         uint32_t MAX_DELETE_JOBS = 500;
         ocs::tom::ListPtr<ocs::tom::ID> part_ref_list;
         const auto cp_refs = ref_list->clone("");

         for (const auto idep : *cp_refs) {
            idep.set<ID_force>(!no_forced_deletion);
         } 

         /*
          * Send delete request to master. If the master is not able to
          * execute the whole request when the 'all' or '-uall' flag was
          * specified, then the master may discontinue the 
          * transaction (STATUS_OK_DOAGAIN). In this case the client has 
          * to redo the transaction.
          */ 
         do {
            do_again = false;

            if (!part_ref_list) {
               const auto max = std::min(cp_refs->size(), std::size_t{MAX_DELETE_JOBS});

               first_try = true;

               auto part_refs = ocs::tom::ListPtr<ocs::tom::ID>::create("part_del_jobs");
               for (std::size_t i = 0; i < max; i++) {
                  const auto temp_ref = cp_refs->front();
                  const char *job = temp_ref.get<ID_str>();
                  /* break if we got job ids first and hit a job name now */
                  if (!isdigit(job[0]) && (i > 0)) {
                     break;
                  }   
                  part_refs->push_back(cp_refs->extract(temp_ref));
                  /* break, if we got a job name by itself */
                  if (!isdigit(job[0])){
                     break;
                  }   
               }
               part_ref_list = std::move(part_refs);
            }
            // the request list is replaced by the response, if any
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::DEL,
                                            ocs::gdi::SubCommand::NONE, part_ref_list);

            for (const auto aep : *alp) {
               status = aep.get<AN_status>();

               if (aep.get<AN_quality>() == ANSWER_QUALITY_ERROR) {
                  ret = 1;
               }

               if (delete_mode != 5 && 
                   ((first_try  && status != STATUS_OK_DOAGAIN) ||
                    (!first_try && status == STATUS_OK))) {
                  
                  printf("%s\n", aep.get<AN_text>());
                  
               }
               /* but a job name might have extended to more than MAX_DELETE_JOBS */
               if (status == STATUS_OK_DOAGAIN) {
                  do_again = true;
               }
            }

            if (!do_again){
               part_ref_list.reset();
            }
            alp.reset();

            first_try = false;
         } while (do_again || !cp_refs->empty());

         if (delete_mode == 7) {
            /* 
             * loop for one minute
             * this should prevent non-admin-users from using the '-f'
             * option regularly
             */
            for(wait = 12; wait > 0; wait--) {
               printf(".");
               fflush(stdout);
               sleep(5);
            } 
            printf("\n");

            delete_mode = 5;
         } else {
            delete_mode = 0;
         } 
      }
   }

   alp.reset();
   ref_list.reset();
   ocs::gdi::ClientBase::shutdown();
   sge_exit(ret);
   return ret;

error_exit:
   alp.reset();
   ref_list.reset();
   ocs::gdi::ClientBase::shutdown();
   sge_exit(1);
   DRETURN(1);
}

/**
 * @brief 'Stage 1' parsing of the qdel options
 *
 * Parses the options with their arguments and stores them in @p cmdline.
 *
 * @param argv the arguments, without the program name
 * @param envp the process environment
 * @param cmdline receives the parsed options (`SPA`), created with the first option
 * @param alpp receives the parse errors
 * @return true on success, false on a parse error
 */
static bool sge_parse_cmdline_qdel(char **argv, [[maybe_unused]] char **envp,
                                   ocs::tom::ListPtr<ocs::tom::SPA> &cmdline, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   char **sp;
   char **rp;

   rp = argv;
   while (*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, cmdline, alpp)) != sp) {
         continue;
      }

      /* -f option */
      if ((rp = parse_noopt(sp, "-f", "--force", cmdline, alpp)) != sp) {
         continue;
      }

      /* -u */
      if (!strcmp("-u", *sp)) {
         sp++;
         if (*sp) {
            auto user_list = str_list_parse_from_string(*sp, ",");

            const auto ep_opt = sge_add_arg(cmdline, 0, lListT, *(sp - 1), *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(user_list));
            sp++;
         }
         rp = sp;
         continue;
      }

      if (!strcmp("-t", *sp)) {
         /* next field is path_name */
         sp++;
         if (*sp == nullptr) {
             answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_TOPTIONMUSTHAVEALISTOFTASKIDRANGES);
             goto error;
         }

         DPRINTF("\"-t %s\"\n", *sp);

         ocs::tom::ListPtr<ocs::tom::RN> task_id_ranges;
         range_list_parse_from_string(task_id_ranges, alpp, *sp, false, true, INF_NOT_ALLOWED);
         if (!task_id_ranges) {
            goto error; 
         }

         range_list_sort_uniq_compress(*task_id_ranges, alpp, true);
         if (task_id_ranges->size() > 1) {
            answer_list_add(alpp, MSG_QCONF_ONLYONERANGE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            goto error;
         }

         const auto ep_opt = sge_add_arg(cmdline, t_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(task_id_ranges));

         sp++;
         rp = sp;
         continue;
      }

      /* job id's */
      if (*sp) {
         ocs::tom::Element<ocs::tom::SPA> ep;
         const auto del_list = str_list_parse_from_string(*sp, ",");

         // the job names are collected here and put into the option, which created its list with the first name
         ocs::tom::ListPtr<ocs::tom::ST> job_names;
         for (const auto job : *del_list) {
            const char *job_name = job.get<ST_name>();
            if (!ep) {
               ep = sge_add_arg(cmdline, 0, lListT, "jobs", nullptr);
            }
            // a null name added nothing in the free emplace_back()
            if (job_name != nullptr) {
               if (!job_names) {
                  job_names = ocs::tom::ListPtr<ocs::tom::ST>::create("");
               }
               job_names->emplace_back<ST_name>(job_name);
            }
         }
         if (ep) {
            ocs::tom::set<SPA_argval_lListT>(ep.raw(), std::move(job_names));
         }
         sp ++;
         rp = sp;
         continue;        
      }

      /* oops */
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
error:      
      sge_usage(QDEL, stderr);
      DRETURN(false);
   }

   DRETURN(true);

}

/**
 * @brief 'Stage 2' parsing of the qdel options
 *
 * Gets the options from @p cmdline, sets the force flag and puts the
 * job numbers (strings) in @p reflist.
 *
 * @param cmdline the parsed options (`SPA`); the options used are removed
 * @param reflist receives the job references (`ID`)
 * @param pforce receives the -f flag
 * @param userlist receives the users of -u (`ST`)
 * @param alpp receives the parse errors
 * @return true on success, false on a parse error
 */
static bool sge_parse_qdel(const ocs::tom::List<ocs::tom::SPA> cmdline, ocs::tom::ListPtr<ocs::tom::ID> &reflist,
                           uint32_t *pforce, ocs::tom::ListPtr<ocs::tom::ST> &userlist,
                           ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   uint32_t helpflag;
   ocs::tom::Element<ocs::tom::SPA> ep;
   bool ret = true;

   /* Loop over all options. Only valid options can be in the
      cmdline list.
   */

   while (!cmdline.empty()) {
      if (parse_flag(cmdline, "-help",  alpp, &helpflag)) {
         sge_usage(QDEL, stdout);
         sge_exit(0);
         break;
      }
      if (parse_flag(cmdline, "-f", alpp, pforce)) {
         continue;
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-u"))) {
         // SPA_argval_lListT carries lists of several types and has no typed exchange; the parser fills the
         // argument of -u with an ST user list
         lList *users = userlist.release();
         ocs::tom::exchange<SPA_argval_lListT>(ep, &users);
         userlist = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(users);
         cmdline.erase(ep);
      }

      if (parse_multi_stringlist<ST_name>(cmdline, "-u", alpp, userlist)) {
         continue;
      }

      if (parse_multi_jobtaskslist(cmdline, "jobs", alpp, reflist, true, 0)) {
         continue;
      }

       /* we get to this point, than there are -t options without job names. We have to write an error message */
      if ((ep = cmdline.find_first<SPA_switch_val>("-t"))) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_JOB_LONELY_TOPTION_S,
                                 ep.get<SPA_switch_arg>());
         break;
      }
   }

   if (answer_list_has_error(alpp)) {
      ret = false;
   }

   DRETURN(ret);
}
