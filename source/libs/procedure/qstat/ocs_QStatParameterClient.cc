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

/** @file
 * @brief Client side parameters of `qstat`: parsed from argv and the environment
 */

#include "uti/sge_io.h"
#include "uti/sge_rmon_monitoring_level.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_SPA.h"

#include "msg_clients_common.h"
#include "msg_common.h"
#include "msg_qstat.h"
#include "../msg_procedure.h"
#include "ocs_QStatParameterClient.h"

#include "ocs_client_parse.h"
#include "uti/sge_bootstrap_files.h"

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
int ocs::QStatParameterClient::qstat_usage(FILE *fp, char *what) {
   dstring ds;
   char buffer[256];

   bool qselect_mode = output_mode_ == OutputMode::QSELECT;

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

   if(!what) {
      /* display full usage */
      fprintf(fp, "%s %s [options]\n", MSG_SRC_USAGE ,qselect_mode?"qselect":"qstat");
      fprintf(fp, "        [-ectx client|server]             %s\n", MSG_COMMON_exec_ctx_OPT_USAGE);
      if (!qselect_mode) {
         fprintf(fp, "        [-ext]                            %s\n",MSG_QSTAT_USAGE_VIEWALSOSCHEDULINGATTRIBUTES);
      }
      if (!qselect_mode) {
         fprintf(fp, "        [-explain a|c|A|E]                %s\n",MSG_QSTAT_USAGE_EXPLAINOPT);
      }
      if (!qselect_mode) {
         fprintf(fp, "        [-f]                              %s\n",MSG_QSTAT_USAGE_FULLOUTPUT);
      }
      fprintf(fp, "        [-fmt plain|json|xml]             %s\n", MSG_COMMON_format_OPT_USAGE);
      if (!qselect_mode) {
         fprintf(fp, "        [-F [resource_attributes]]        %s\n",MSG_QSTAT_USAGE_FULLOUTPUTANDSHOWRESOURCESOFQUEUES);
      }
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
         fprintf(fp, "        [-s {p|r|s|f|hu|ho|hs|hd|hj|ha|h|a}] %s\n",MSG_QSTAT_USAGE_SHOWPENDINGRUNNINGSUSPENDESZOMBIEJOBS);
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

/** @brief First stage of the option parsing: collect the switches of a command line
 * @param ppcmdline receives the switches, created with the first one
 * @param answer_list receives error messages
 * @param argv the arguments, without the program name
 * @return true when every argument was a known switch
 */
bool
ocs::QStatParameterClient::switch_list_qstat_parse_from_cmdline(ocs::tom::ListPtr<ocs::tom::SPA> &ppcmdline,
                                                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                                char **argv) {
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

      /* -ectx */
      if ((rp = parse_until_next_opt(sp, "-ectx", nullptr, ppcmdline, answer_list)) != sp)
         continue;

      /* -fmt */
      if ((rp = parse_until_next_opt(sp, "-fmt", nullptr, ppcmdline, answer_list)) != sp)
         continue;

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

      /* oops */
      snprintf(str, sizeof(str), MSG_ANSWER_INVALIDOPTIONARGX_S, *sp);
      qstat_usage(stderr, nullptr);
      answer_list_add(answer_list, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(ret);
   }
   DRETURN(ret);
}

/** @brief Second stage of the option parsing: turn the switches into the parameters
 *
 * Takes the options out of the switch list, sets the display flags and puts
 * the queue, resource, user and job arguments into the member lists.
 *
 * @param ppcmdline the switches; the ones handled are removed from it
 * @return the answer list with the parsing errors, or nullptr when there were none
 */
ocs::tom::ListPtr<ocs::tom::AN>
ocs::QStatParameterClient::sge_parse_qstat(const ocs::tom::List<ocs::tom::SPA> ppcmdline) {
   DENTER(TOP_LAYER);
   stringT str;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   uint32_t helpflag;
   int usageshowed = 0;
   char *argstr;
   uint32_t full = 0;


   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */

   while (!ppcmdline.empty()) {
      if (parse_flag(ppcmdline, "-help",  &alp, &helpflag)) {
         usageshowed = qstat_usage(stdout, nullptr);
         sge_exit(0);
         break;
      }

      while (parse_string(ppcmdline, "-j", &alp, &argstr)) {
         output_mode_ = OutputMode::JOB_INFO;
         if (argstr) {
            // a later -j replaces the job list of an earlier one
            jid_list_.reset();
            jid_list_ = str_list_parse_from_string(argstr, ",");
            sge_free(&argstr);
         }
      }

      if (parse_string(ppcmdline, "-ectx", &alp, &argstr)) {
         if (argstr == nullptr) {
            continue;
         }
         if (strcmp(argstr, "client") == 0) {
            exec_context_ = ExecContext::CLIENT;
         } else if (strcmp(argstr, "server") == 0) {
            exec_context_ = ExecContext::SERVER;
         } else {
            char buf[BUFSIZ];
            snprintf(buf, sizeof(buf), MSG_PARSE_INVALIDOPTIONARGUMENTX_S, argstr);
            answer_list_add(&alp, buf, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            sge_free(&argstr);
         }
         sge_free(&argstr);
         continue;
      }

      if (parse_string(ppcmdline, "-fmt", &alp, &argstr)) {
         if (argstr == nullptr) {
            continue;
         }
         if (strcmp(argstr, "plain") == 0) {
            output_format_ = OutputFormat::PLAIN;
         } else if (strcmp(argstr, "json") == 0) {
            output_format_ = OutputFormat::JSON;
         } else if (strcmp(argstr, "xml") == 0) {
            output_format_ = OutputFormat::XML;
         } else {
            char buf[BUFSIZ];
            snprintf(buf, sizeof(buf), MSG_PARSE_INVALIDOPTIONARGUMENTX_S, argstr);
            answer_list_add(&alp, buf, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            sge_free(&argstr);
         }
         sge_free(&argstr);
         continue;
      }

      uint32_t in_xml_mode = false;
      while (parse_flag(ppcmdline, "-xml", &alp, &in_xml_mode)){
         if (in_xml_mode) {
            output_format_ = OutputFormat::XML;
         }
      }

      while (parse_flag(ppcmdline, "-ne", &alp, &full)) {
         if (full) {
            show_ |= QSTAT_DISPLAY_NOEMPTYQ;
            full = 0;
         }
      }


      while (parse_flag(ppcmdline, "-f", &alp, &full)) {
         if (full) {
            show_ |= QSTAT_DISPLAY_FULL;
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
         uint32_t filter = QI_AMBIGUOUS | QI_ALARM | QI_SUSPEND_ALARM | QI_ERROR;
         explain_bits_ = qinstance_state_from_string(argstr, &alp, filter);
         show_ |= QSTAT_DISPLAY_FULL;
         need_queues_ = true;
         sge_free(&argstr);
      }

      while (parse_string(ppcmdline, "-F", &alp, &argstr)) {
         show_ |= QSTAT_DISPLAY_QRESOURCES|QSTAT_DISPLAY_FULL;
         need_queues_ = true;
         if (argstr) {
            // frees the list it is handed and gives nullptr when the text cannot be parsed
            q_resource_list_ = centry_list_parse_from_string(std::move(q_resource_list_), argstr, false);
            sge_free(&argstr);
         }
      }

      while (parse_flag(ppcmdline, "-ext", &alp, &full)) {
         if (full) {
            show_ |= QSTAT_DISPLAY_EXTENDED;
            full = 0;
         }
      }

      if (output_mode_ == OutputMode::QSTAT_DEFAULT || output_mode_ == OutputMode::QSTAT_GROUP) {
         while (parse_flag(ppcmdline, "-urg", &alp, &full)) {
            need_queues_ = true;
            if (full) {
               show_ |= QSTAT_DISPLAY_URGENCY;
               full = 0;
            }
         }

         while (parse_flag(ppcmdline, "-pri", &alp, &full)) {
            if (full) {
               show_ |= QSTAT_DISPLAY_PRIORITY;
               full = 0;
            }
         }
      }

      while (parse_flag(ppcmdline, "-r", &alp, &full)) {
         if (full) {
            show_ |= QSTAT_DISPLAY_RESOURCES;
            full = 0;
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-t", &alp, &full)) {
         if (full) {
            show_ |= QSTAT_DISPLAY_TASKS;
            group_opt_ |= GROUP_NO_PETASK_GROUPS;
            full = 0;
         }
      }

      while (parse_string(ppcmdline, "-qs", &alp, &argstr)) {
         uint32_t filter = 0xFFFFFFFF;
         queue_state_ = qinstance_state_from_string(argstr, &alp, filter);
         need_queues_ = true;
         sge_free(&argstr);
      }

      while (parse_string(ppcmdline, "-l", &alp, &argstr)) {
         // frees the list it is handed and gives nullptr when the text cannot be parsed
         resource_list_ = centry_list_parse_from_string(std::move(resource_list_), argstr, false);
         need_queues_ = true;
         sge_free(&argstr);
      }

      // the parser appends to the lists
      while (parse_multi_stringlist<ST_name>(ppcmdline, "-u", &alp, user_list_)) {
         ;
      }

      while (parse_multi_stringlist<ST_name>(ppcmdline, "-U", &alp, queue_user_list_)) {
         need_queues_ = true;
      }

      while (parse_multi_stringlist<ST_name>(ppcmdline, "-pe", &alp, pe_ref_list_)) {
         need_queues_ = true;
      }

      while (parse_multi_stringlist<QR_name>(ppcmdline, "-q", &alp, queue_ref_list_)) {
         need_queues_ = true;
      }

      ocs::tom::ListPtr<ocs::tom::ST> plstringopt;
      while (parse_multi_stringlist<ST_name>(ppcmdline, "-g", &alp, plstringopt)) {
         group_opt_ |= parse_group_options(*plstringopt, &alp);

         // -g c is here misused to switch to a different output mode.
         // @todo We should consider to introduce a switch like -mode ...
         if (group_opt_ & GROUP_CQ_SUMMARY) {
            output_mode_ = OutputMode::QSTAT_GROUP;
            group_opt_ &= ~GROUP_CQ_SUMMARY;
         }
         need_queues_ = true;
         plstringopt.reset();
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

   if (!ppcmdline.empty()) {
     snprintf(str, sizeof(str), "%s\n", MSG_PARSE_TOOMANYOPTIONS);
     if (!usageshowed) {
        qstat_usage(stderr, nullptr);
     }
     answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
     DRETURN(alp);
   }

   DRETURN(alp);
}

/** @brief First stage of the option parsing: collect the switches of a default file
 * @param switch_list receives the switches, created with the first one
 * @param answer_list receives error messages
 * @param file the file; a missing or unreadable one is no error
 * @return true when the file is missing or every word in it was a known switch
 */
bool
ocs::QStatParameterClient::switch_list_qstat_parse_from_file(ocs::tom::ListPtr<ocs::tom::SPA> &switch_list,
                                                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                             const char *file) {
   DENTER(TOP_LAYER);
   int file_as_string_length;
   char *file_as_string = sge_file2string(file, &file_as_string_length);
   if (file_as_string == nullptr) {
      // file does not exist or is not readable — both are acceptable
      DRETURN(true);
   }

   char **token = stra_from_str(file_as_string, " \n\t");
   const int ret = switch_list_qstat_parse_from_cmdline(switch_list, answer_list, token);
   sge_strafree(&token);
   sge_free(&file_as_string);
   DRETURN(ret);
}

/** @brief Parse the command line and the environment into this object
 * @param answer_list receives error messages
 * @param argv the arguments, without the program name
 * @param envp the environment
 * @return true when the parameters are usable
 */
bool ocs::QStatParameterClient::parse_parameters(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char **argv,
                                                 char **envp) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::SPA> file_switches;
   ocs::tom::ListPtr<ocs::tom::SPA> cmdline_switches;

   const char *username = component_get_username();
   const char *cell_root = bootstrap_get_cell_root();

   if (!strcmp(sge_basename(*argv++, '/'), "qselect")) {
      output_mode_ = OutputMode::QSELECT;
      need_queues_ = true;
   }

   bool more = true;
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
      switch_list_qstat_parse_from_file(file_switches, &alp, sge_dstring_get_string(&file));
   }
   if (get_user_home_file_path(&file, home_file, username, &alp)) {
      switch_list_qstat_parse_from_file(file_switches, &alp, sge_dstring_get_string(&file));
   }
   sge_dstring_free(&file);

   // get options from the command line
   switch_list_qstat_parse_from_cmdline(cmdline_switches, &alp, argv);

   // remove duplicate options
   for (const auto ep_1 : *cmdline_switches) {
      do {
         /*
          * Need that logic to handle multiple SPA
          * objects representing the same option.
          */
         more = false;
         for (const auto ep_2 : *file_switches) {
            if (strcmp(ep_1.get<SPA_switch_val>(), ep_2.get<SPA_switch_val>()) == 0) {
               // remove duplicate options
               file_switches->erase(ep_2);
               more = true;
               break;
            }
         }
      } while(more);
   }

   // merge the options from the files and the command line
   if (!cmdline_switches->empty()) {
      cmdline_switches->splice(std::move(file_switches));
   } else if (!file_switches->empty()) {
      file_switches->splice(std::move(cmdline_switches));
      cmdline_switches = std::move(file_switches);
   }

   // parsing error => show error and exit
   if (alp != nullptr) {
      for (const auto aep : *alp) {
         fprintf(stderr, "%s\n", aep.get<AN_text>());
      }
      alp.reset();
      cmdline_switches.reset();
      sge_exit(1);
   }

   // handle all switches; the parser takes the handled ones out of the list
   alp = sge_parse_qstat(*cmdline_switches);
   if (alp != nullptr) {
      /*
      ** low level parsing error! show answer list
      */
      for (const auto aep : *alp) {
         fprintf(stderr, "%s\n", aep.get<AN_text>());
      }
      alp.reset();
      cmdline_switches.reset();
      sge_exit(1);
   }

   str_list_transform_user_list(user_list_, answer_list, username);

   // CS-2387: resolve SGE_QSTAT_LOAD_AVG on the client side so that the value
   // reaches server-rendered qstat (ExecContext::SERVER) through the marshalled
   // QStatParameter bundle. Validated against the same whitelist as SGE_LOAD_AVG
   // in QStatModelBase::select_by_queue_state; unknown / unset / empty values
   // leave the default (LOAD_ATTR_NP_LOAD_AVG) in place.
   {
      const char *load_avg_variable = getenv("SGE_QSTAT_LOAD_AVG");
      static const char *valid_load_attrs[] = {
         LOAD_ATTR_LOAD_AVG, LOAD_ATTR_LOAD_SHORT, LOAD_ATTR_LOAD_MEDIUM, LOAD_ATTR_LOAD_LONG,
         LOAD_ATTR_NP_LOAD_AVG, LOAD_ATTR_NP_LOAD_SHORT, LOAD_ATTR_NP_LOAD_MEDIUM, LOAD_ATTR_NP_LOAD_LONG,
         nullptr
      };
      for (int i = 0; valid_load_attrs[i] != nullptr; i++) {
         if (load_avg_variable != nullptr && strcmp(load_avg_variable, valid_load_attrs[i]) == 0) {
            set_load_avg_variable(load_avg_variable);
            break;
         }
      }
   }

   DRETURN(true);
}
