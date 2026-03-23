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

#include <cstring>

#include "uti/sge_bootstrap_files.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_io.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/parse.h"

#include "ocs_QStatParameter.h"
#include "ocs_client_parse.h"
#include "ocs_client_print.h"
#include "msg_clients_common.h"
#include "msg_common.h"
#include "msg_qstat.h"

void ocs::QStatParameter::free_data() {
   lFreeList(&resource_list_);
   lFreeList(&qresource_list_);
   lFreeList(&queueref_list_);
   lFreeList(&peref_list_);
   lFreeList(&user_list_);
   lFreeList(&queue_user_list_);
}

/****
 **** qstat_usage (static)
 ****
 **** displays usage of qstat on file fp.
 **** Is what nullptr, full usage will be displayed.
 ****
 **** Returns always 1.
 ****
 **** If what is a pointer to an option-string,
 **** only usage for that option will be displayed.
 ****   ** not implemented yet! **
 ****/
int
ocs::QStatParameter::qstat_usage(FILE *fp, char *what)
{
   dstring ds;
   char buffer[256];

   bool qselect_mode = output_mode_ == OutputMode::QSELECT;

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

   if(!what) {
      /* display full usage */
      fprintf(fp, "%s %s [options]\n", MSG_SRC_USAGE ,qselect_mode?"qselect":"qstat");
      if (!qselect_mode) {
         fprintf(fp, "        [-ext]                            %s\n",MSG_QSTAT_USAGE_VIEWALSOSCHEDULINGATTRIBUTES);
      }
      if (!qselect_mode) {
         fprintf(fp, "        [-explain a|c|A|E]                %s\n",MSG_QSTAT_USAGE_EXPLAINOPT);
      }
      if (!qselect_mode)
         fprintf(fp, "        [-f]                              %s\n",MSG_QSTAT_USAGE_FULLOUTPUT);
      if (!qselect_mode)
         fprintf(fp, "        [-F [resource_attributes]]        %s\n",MSG_QSTAT_USAGE_FULLOUTPUTANDSHOWRESOURCESOFQUEUES);
      if (!qselect_mode) {
         fprintf(fp, "        [-g {c}]                          %s\n",MSG_QSTAT_USAGE_DISPLAYCQUEUESUMMARY);
         fprintf(fp, "        [-g {d}]                          %s\n",MSG_QSTAT_USAGE_DISPLAYALLJOBARRAYTASKS);
         fprintf(fp, "        [-g {t}]                          %s\n",MSG_QSTAT_USAGE_DISPLAYALLPARALLELJOBTASKS);
      }
      fprintf(fp, "        [-help]                           %s\n",MSG_COMMON_help_OPT_USAGE);
      if (!qselect_mode)
         fprintf(fp, "        [-j [job_identifier_list]]         %s\n",MSG_QSTAT_USAGE_SHOWSCHEDULERJOBINFO);
      fprintf(fp, "        [-l resource_list]                %s\n",MSG_QSTAT_USAGE_REQUESTTHEGIVENRESOURCES);
      if (!qselect_mode)
         fprintf(fp, "        [-ne]                             %s\n",MSG_QSTAT_USAGE_HIDEEMPTYQUEUES);
      fprintf(fp, "        [-pe pe_list]                     %s\n",MSG_QSTAT_USAGE_SELECTONLYQUEESWITHONOFTHESEPE);
      fprintf(fp, "        [-q wc_queue_list]                %s\n",MSG_QSTAT_USAGE_PRINTINFOONGIVENQUEUE);
      fprintf(fp, "        [-qs {a|c|d|o|s|u|A|C|D|E|S}]     %s\n",MSG_QSTAT_USAGE_PRINTINFOCQUEUESTATESEL);
      if (!qselect_mode)
         fprintf(fp, "        [-r]                              %s\n",MSG_QSTAT_USAGE_SHOWREQUESTEDRESOURCESOFJOB);
      if (!qselect_mode) {
         fprintf(fp, "        [-s {p|r|s|hu|ho|hs|hd|hj|ha|h|a}] %s\n",MSG_QSTAT_USAGE_SHOWPENDINGRUNNINGSUSPENDESZOMBIEJOBS);
         fprintf(fp, "                                          %s\n",MSG_QSTAT_USAGE_JOBSWITHAUSEROPERATORSYSTEMHOLD);
         fprintf(fp, "                                          %s\n",MSG_QSTAT_USAGE_JOBSWITHSTARTTIMEINFUTORE);
         fprintf(fp, "                                          %s\n",MSG_QSTAT_USAGE_HISABBREVIATIONFORHUHOHSHJHA);
         fprintf(fp, "                                          %s\n",MSG_QSTAT_USAGE_AISABBREVIATIONFOR);
      }
      if (!qselect_mode)
         fprintf(fp, "        [-t]                              %s\n",MSG_QSTAT_USAGE_SHOWTASKINFO);
      if (!qselect_mode){
         fprintf(fp, "        [-u user_list]                    %s\n",MSG_QSTAT_USAGE_VIEWONLYJOBSOFTHISUSER);
      }
      fprintf(fp, "        [-U user_list]                    %s\n",MSG_QSTAT_USAGE_SELECTQUEUESWHEREUSERXHAVEACCESS);

      if (!qselect_mode) {
         fprintf(fp, "        [-urg]                            %s\n",MSG_QSTAT_URGENCYINFO );
         fprintf(fp, "        [-pri]                            %s\n",MSG_QSTAT_PRIORITYINFO );
      }
      fprintf(fp, "        [-xml]                            %s\n", MSG_COMMON_xml_OPT_USAGE);

      if (getenv("MORE_INFO")) {
         fprintf(fp, SFNMAX"\n", MSG_QSTAT_USAGE_ADDITIONALDEBUGGINGOPTIONS);
         fprintf(fp, "        [-dj]                             %s\n",MSG_QSTAT_USAGE_DUMPCOMPLETEJOBLISTTOSTDOUT);
         fprintf(fp, "        [-dq]                             %s\n",MSG_QSTAT_USAGE_DUMPCOMPLETEQUEUELISTTOSTDOUT);
      }
      fprintf(fp, "\n");
      fprintf(fp, "pe_list                  pe[,pe,...]\n");
      fprintf(fp, "job_identifier_list      [job_id|job_name|pattern]{, [job_id|job_name|pattern]}\n");
      fprintf(fp, "resource_list            resource[=value][,resource[=value],...]\n");
      fprintf(fp, "user_list                user|@group[,user|@group],...]\n");
      fprintf(fp, "resource_attributes      resource,resource,...\n");
      fprintf(fp, "wc_cqueue                %s\n", MSG_QSTAT_HELP_WCCQ);
      fprintf(fp, "wc_host                  %s\n", MSG_QSTAT_HELP_WCHOST);
      fprintf(fp, "wc_hostgroup             %s\n", MSG_QSTAT_HELP_WCHG);
      fprintf(fp, "wc_qinstance             wc_cqueue@wc_host\n");
      fprintf(fp, "wc_qdomain               wc_cqueue@wc_hostgroup\n");
      fprintf(fp, "wc_queue                 wc_cqueue|wc_qdomain|wc_qinstance\n");
      fprintf(fp, "wc_queue_list            wc_queue[,wc_queue,...]\n");
   } else {
      /* display option usage */
      fprintf(fp, MSG_QDEL_not_available_OPT_USAGE_S,what);
      fprintf(fp, "\n");
   }
   return 1;
}

bool
ocs::QStatParameter::switch_list_qstat_parse_from_cmdline(lList **ppcmdline, lList **answer_list, char **argv) {
   DENTER(TOP_LAYER);
   bool ret = true;
   char **sp;
   stringT str;
   bool qselect_mode = output_mode_ == OutputMode::QSELECT;


   char **rp = argv;
   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -f option */
      if (!qselect_mode && (rp = parse_noopt(sp, "-f", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -F */
      if (!qselect_mode && (rp = parse_until_next_opt2(sp, "-F", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      if (!qselect_mode) {
         /* -ext option */
         if ((rp = parse_noopt(sp, "-ext", nullptr, ppcmdline, answer_list)) != sp)
            continue;

         /* -urg option */
         if ((rp = parse_noopt(sp, "-urg", nullptr, ppcmdline, answer_list)) != sp)
            continue;

         /* -urg option */
         if ((rp = parse_noopt(sp, "-pri", nullptr, ppcmdline, answer_list)) != sp)
            continue;
      }

      /* -xml option */
      if ((rp = parse_noopt(sp, "-xml", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -g */
      if (!qselect_mode && (rp = parse_until_next_opt(sp, "-g", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -j [jid {,jid}]*/
      if (!qselect_mode && (rp = parse_until_next_opt2(sp, "-j", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -l */
      if ((rp = parse_until_next_opt(sp, "-l", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -ne option */
      if (!qselect_mode && (rp = parse_noopt(sp, "-ne", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -s [p|r|s|h|d|...] option */
      if (!qselect_mode && (rp = parse_until_next_opt(sp, "-s", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -qs [.{a|c|d|o|..] option */
      if ((rp = parse_until_next_opt(sp, "-qs", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -explain [c|a|A...] option */
      if (!qselect_mode && (rp = parse_until_next_opt(sp, "-explain", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -q */
      if ((rp = parse_until_next_opt(sp, "-q", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -r */
      if (!qselect_mode && (rp = parse_noopt(sp, "-r", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -t */
      if (!qselect_mode && (rp = parse_noopt(sp, "-t", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -u */
      if (!qselect_mode && (rp = parse_until_next_opt(sp, "-u", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -U */
      if ((rp = parse_until_next_opt(sp, "-U", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -pe */
      if ((rp = parse_until_next_opt(sp, "-pe", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /*
      ** Two additional flags only if MORE_INFO is set:
      ** -dj   dump jobs:  displays full global_job_list
      ** -dq   dump queue: displays full global_queue_list
      */
      if (getenv("MORE_INFO")) {
         /* -dj */
         if ((rp = parse_noopt(sp, "-dj", nullptr, ppcmdline, answer_list)) != sp)
            continue;

         /* -dq */
         if ((rp = parse_noopt(sp, "-dq", nullptr, ppcmdline, answer_list)) != sp)
            continue;
      }

      /* oops */
      snprintf(str, sizeof(str), MSG_ANSWER_INVALIDOPTIONARGX_S, *sp);
      qstat_usage(stderr, nullptr);
      answer_list_add(answer_list, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(ret);
   }
   DRETURN(ret);
}

/****
 **** sge_parse_qstat (static)
 ****
 **** 'stage 2' parsing of qstat-options. Gets the options from
 **** ppcmdline, sets the full and empry_qs flags and puts the
 **** queue/res/user-arguments into the lists.
 ****/
lList *
ocs::QStatParameter::sge_parse_qstat(lList **ppcmdline, lList **ppljid)
{
   stringT str;
   lList *alp = nullptr;
   u_long32 helpflag;
   int usageshowed = 0;
   char *argstr;
   u_long32 full = 0;
   lList *plstringopt = nullptr;

   DENTER(TOP_LAYER);

   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */

   while (lGetNumberOfElem(*ppcmdline)) {
      if (parse_flag(ppcmdline, "-help",  &alp, &helpflag)) {
         usageshowed = qstat_usage(stdout, nullptr);
         sge_exit(0);
         break;
      }

      while (parse_string(ppcmdline, "-j", &alp, &argstr)) {
         output_mode_ = OutputMode::JOB_INFO;
         if (argstr) {
            if (*ppljid) {
               lFreeList(ppljid);
            }
            str_list_parse_from_string(ppljid, argstr, ",");
            sge_free(&argstr);
         }
      }

      u_long32 in_xml_mode = false;
      while (parse_flag(ppcmdline, "-xml", &alp, &in_xml_mode)){
         if (in_xml_mode) {
            output_format_ = OutputFormat::XML;
         }
      }

      while (parse_flag(ppcmdline, "-ne", &alp, &full)) {
         if (full) {
            full_listing_ |= QSTAT_DISPLAY_NOEMPTYQ;
            full = 0;
         }
      }


      while (parse_flag(ppcmdline, "-f", &alp, &full)) {
         if (full) {
            full_listing_ |= QSTAT_DISPLAY_FULL;
            full = 0;
         }
         need_queues_ = true;
      }

      while (parse_string(ppcmdline, "-s", &alp, &argstr)) {
         if (argstr != nullptr) {
            state_filter_ = true;
            state_filter_value_ = argstr;
            sge_free(&argstr);
         }
      }

      while (parse_string(ppcmdline, "-explain", &alp, &argstr)) {
         u_long32 filter = QI_AMBIGUOUS | QI_ALARM | QI_SUSPEND_ALARM | QI_ERROR;
         explain_bits_ = qinstance_state_from_string(argstr, &alp, filter);
         full_listing_ |= QSTAT_DISPLAY_FULL;
         need_queues_ = true;
         sge_free(&argstr);
      }

      while (parse_string(ppcmdline, "-F", &alp, &argstr)) {
         full_listing_ |= QSTAT_DISPLAY_QRESOURCES|QSTAT_DISPLAY_FULL;
         need_queues_ = true;
         if (argstr) {
            qresource_list_ = centry_list_parse_from_string(qresource_list_, argstr, false);
            sge_free(&argstr);
         }
      }

      while (parse_flag(ppcmdline, "-ext", &alp, &full)) {
         if (full) {
            full_listing_ |= QSTAT_DISPLAY_EXTENDED;
            full = 0;
         }
      }

      if (output_mode_ == OutputMode::QSTAT_DEFAULT || output_mode_ == OutputMode::QSTAT_GROUP) {
         while (parse_flag(ppcmdline, "-urg", &alp, &full)) {
            need_queues_ = true;
            if (full) {
               full_listing_ |= QSTAT_DISPLAY_URGENCY;
               full = 0;
            }
         }

         while (parse_flag(ppcmdline, "-pri", &alp, &full)) {
            if (full) {
               full_listing_ |= QSTAT_DISPLAY_PRIORITY;
               full = 0;
            }
         }
      }

      while (parse_flag(ppcmdline, "-r", &alp, &full)) {
         if (full) {
            full_listing_ |= QSTAT_DISPLAY_RESOURCES;
            full = 0;
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-t", &alp, &full)) {
         if (full) {
            full_listing_ |= QSTAT_DISPLAY_TASKS;
            group_opt_ |= GROUP_NO_PETASK_GROUPS;
            full = 0;
         }
      }

      while (parse_string(ppcmdline, "-qs", &alp, &argstr)) {
         u_long32 filter = 0xFFFFFFFF;
         queue_state_ = qinstance_state_from_string(argstr, &alp, filter);
         need_queues_ = true;
         sge_free(&argstr);
      }

      while (parse_string(ppcmdline, "-l", &alp, &argstr)) {
         resource_list_ = centry_list_parse_from_string(resource_list_, argstr, false);
         need_queues_ = true;
         sge_free(&argstr);
      }

      while (parse_multi_stringlist(ppcmdline, "-u", &alp, &user_list_, ST_Type, ST_name)) {
         ;
      }

      while (parse_multi_stringlist(ppcmdline, "-U", &alp, &queue_user_list_, ST_Type, ST_name)) {
         need_queues_ = true;
      }

      while (parse_multi_stringlist(ppcmdline, "-pe", &alp, &peref_list_, ST_Type, ST_name)) {
         need_queues_ = true;
      }

      while (parse_multi_stringlist(ppcmdline, "-q", &alp, &queueref_list_, QR_Type, QR_name)) {
         need_queues_ = true;
      }

      while (parse_multi_stringlist(ppcmdline, "-g", &alp, &plstringopt, ST_Type, ST_name)) {
         group_opt_ |= parse_group_options(plstringopt, &alp);

         // -g c is here misused to switch to a different output mode.
         // @todo We should consider to introduce a siwtch like -mode ...
         if (group_opt_ & GROUP_CQ_SUMMARY) {
            output_mode_ = OutputMode::QSTAT_GROUP;
            group_opt_ &= ~GROUP_CQ_SUMMARY;
         }
         need_queues_ = true;
         lFreeList(&plstringopt);
      }
   }

   switch (output_mode_) {
      case OutputMode::QSELECT:
         need_job_list_ = false;
         break;
      case OutputMode::QSTAT_GROUP:
      case OutputMode::QSTAT_DEFAULT:
      case OutputMode::JOB_INFO:
         need_job_list_ = true;
         break;
   }

   if (lGetNumberOfElem(*ppcmdline)) {
     snprintf(str, sizeof(str), "%s\n", MSG_PARSE_TOOMANYOPTIONS);
     if (!usageshowed) {
        qstat_usage(stderr, nullptr);
     }
     answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
     DRETURN(alp);
   }

   DRETURN(alp);
}

bool
ocs::QStatParameter::switch_list_qstat_parse_from_file(lList **switch_list, lList **answer_list, const char *file)
{
   DENTER(TOP_LAYER);
   bool ret = true;

   if (switch_list == nullptr) {
      ret = false;
   } else {
      if (!sge_is_file(file)) {
         // it is ok if the file does not exist
         ret = true;
      } else {
         char *file_as_string = nullptr;
         int file_as_string_length;

         file_as_string = sge_file2string(file, &file_as_string_length);
         if (file_as_string == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
            ret = false;
         } else {
            char **token = stra_from_str(file_as_string, " \n\t");
            ret = switch_list_qstat_parse_from_cmdline(switch_list, answer_list, token);
            sge_strafree(&token);
         }
         sge_free(&file_as_string);
      }
   }
   DRETURN(ret);
}

bool ocs::QStatParameter::parse_parameters(lList **answer_list, char **argv, char **envp) {
   lList *alp = nullptr;
   lList *pfile = nullptr;
   lList *pcmdline = nullptr;
   lList *ref_list = nullptr;
   const lListElem *ep_1 = nullptr;
   lListElem *ep_2 = nullptr;
   bool more = true;

   const char *username = component_get_username();
   const char *cell_root = bootstrap_get_cell_root();

   if (!strcmp(sge_basename(*argv++, '/'), "qselect")) {
      output_mode_ = OutputMode::QSELECT;
      need_queues_ = true;
   }

   {
      // get name of files that contain default options
      dstring file = DSTRING_INIT;
      const char *common_file = SGE_COMMON_DEF_QSTAT_FILE;
      const char *home_file = SGE_HOME_DEF_QSTAT_FILE;

      if (output_mode_ == OutputMode::QSELECT) {
         common_file = SGE_COMMON_DEF_QSELECT_FILE;
         home_file = SGE_HOME_DEF_QSELECT_FILE;
      }

      // get options from the global and user specific files
      if (get_root_file_path(&file, cell_root, common_file)) {
         switch_list_qstat_parse_from_file(&pfile, &alp, sge_dstring_get_string(&file));
      }
      if (get_user_home_file_path(&file, home_file, username, &alp)) {
         switch_list_qstat_parse_from_file(&pfile, &alp, sge_dstring_get_string(&file));
      }
      sge_dstring_free(&file);

      // get options from the command line
      switch_list_qstat_parse_from_cmdline(&pcmdline, &alp, argv);

      // remove duplicate options
      for_each_ep(ep_1, pcmdline) {
         do {
            /*
             * Need that logic to handle multiple SPA
             * objects representing the same option.
             */
            more = false;
            for_each_rw(ep_2, pfile) {
               if (strcmp(lGetString(ep_1, SPA_switch_val), lGetString(ep_2, SPA_switch_val)) == 0) {
                  // remove duplicate options
                  lRemoveElem(pfile, &ep_2);
                  more = true;
                  break;
               }
            }
         } while(more);
      }

      // merge the options from the files and the command line
      if (lGetNumberOfElem(pcmdline) > 0) {
         lAppendList(pcmdline, pfile);
         lFreeList(&pfile);
      } else if (lGetNumberOfElem(pfile) > 0) {
         lAppendList(pfile, pcmdline);
         lFreeList(&pcmdline);
         pcmdline = pfile;
      }
   }

   const lListElem *aep = nullptr;

   // parsing error => show error and exit
   if (alp != nullptr) {
      for_each_ep(aep, alp) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      //qstat_env_destroy(&qstat_env);
      sge_exit(1);
   }

   // handle all switches
   alp = sge_parse_qstat(&pcmdline, &jid_list_);
   if (alp != nullptr) {
      /*
      ** low level parsing error! show answer list
      */
      for_each_ep(aep, alp) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      lFreeList(&ref_list);
      //qstat_env_destroy(&qstat_env);
      sge_exit(1);
   }

   str_list_transform_user_list(&user_list_, answer_list, username);
   return true;
}

