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
 * @brief Client side parameters of `qquota`: parsed from argv and the environment
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_string.h"
#include "uti/sge_io.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_feature.h"

#include "ocs_QQuotaParameterClient.h"
#include "msg_qquota.h"

#include "ocs_client_parse.h"
#include "msg_common.h"
#include "msg_clients_common.h"

bool
ocs::QQuotaParameterClient::show_usage(FILE *fp) {
   DENTER(TOP_LAYER);

   if (fp == nullptr) {
      DRETURN(false);
   }

   char buffer[256];
   dstring ds;
   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));
   fprintf(fp, "%s qquota [options]\n", MSG_SRC_USAGE);
   fprintf(fp, "  [-ectx client|server]                %s\n", MSG_COMMON_exec_ctx_OPT_USAGE);
   fprintf(fp, "  [-fmt plain|json|xml]                %s\n", MSG_COMMON_format_OPT_USAGE);
   fprintf(fp, "  [-help]                              %s\n", MSG_COMMON_help_OPT_USAGE);
   fprintf(fp, "  [-h wc_host_list|wc_hostgroup_list]  %s\n", MSG_QQUOTA_h_OPT_USAGE);
   fprintf(fp, "  [-l resource_attributes]             %s\n", MSG_QQUOTA_l_OPT_USAGE);
   fprintf(fp, "  [-pe wc_pe_list]                     %s\n", MSG_QQUOTA_pe_OPT_USAGE);
   fprintf(fp, "  [-P wc_project_list]                 %s\n", MSG_QQUOTA_P_OPT_USAGE);
   fprintf(fp, "  [-q wc_cqueue_list]                  %s\n", MSG_QQUOTA_q_OPT_USAGE);
   fprintf(fp, "  [-u wc_user]                         %s\n", MSG_QQUOTA_u_OPT_USAGE);
   fprintf(fp, "\n");
   fprintf(fp, "resource_attributes      resource_name,resource_name,...\n");
   fprintf(fp, "wc_cqueue                %s\n", MSG_QSTAT_HELP_WCCQ);
   fprintf(fp, "wc_cqueue_list           wc_cqueue[,wc_cqueue,...]\n");
   fprintf(fp, "wc_host                  %s\n", MSG_QSTAT_HELP_WCHOST);
   fprintf(fp, "wc_host_list             wc_host[,wc_host,...]\n");
   fprintf(fp, "wc_hostgroup             %s\n", MSG_QSTAT_HELP_WCHG);
   fprintf(fp, "wc_hostgroup_list        wc_hostgroup[,wc_hostgroup,...]\n");
   fprintf(fp, "wc_pe                    %s\n", MSG_QQUOTA_HELP_WCPE);
   fprintf(fp, "wc_pe_list               wc_pe[,wc_pe,...]\n");
   fprintf(fp, "wc_project               %s\n", MSG_QQUOTA_HELP_WCPROJECT);
   fprintf(fp, "wc_project_list          wc_project[,wc_project,...]\n");

   DRETURN(true);
}

bool
ocs::QQuotaParameterClient::parse_cmdline_and_env(char *argv[], lList **switch_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (argv == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_NULLPOINTER);
      DRETURN(false);
   }

   char **sp;
   char **rp = ++argv;
   while (*(sp = rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -h option */
      if ((rp = parse_until_next_opt2(sp, "-h", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -l option */
      if ((rp = parse_until_next_opt2(sp, "-l", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -u option */
      if ((rp = parse_until_next_opt2(sp, "-u", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -pe option */
      if ((rp = parse_until_next_opt2(sp, "-pe", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -P option */
      if ((rp = parse_until_next_opt2(sp, "-P", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -q */
      if ((rp = parse_until_next_opt2(sp, "-q", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -ectx */
      if ((rp = parse_until_next_opt(sp, "-ectx", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -fmt */
      if ((rp = parse_until_next_opt(sp, "-fmt", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* -xml */
      if ((rp = parse_noopt(sp, "-xml", nullptr, switch_list, answer_list)) != sp)
         continue;

      /* oops */
      show_usage(stderr);
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                              *sp);
      DRETURN(false);
   }
   DRETURN(true);
}

bool
ocs::QQuotaParameterClient::parse_cmdline_from_file(const char *file, lList **switch_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (switch_list == nullptr) {
      DRETURN(false);
   }
   if (!sge_is_file(file)) {
      // This is no error
      DRETURN(true);
   }

   int file_as_string_length;
   const char *file_as_string = sge_file2string(file, &file_as_string_length);
   if (file_as_string == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ANSWER_ERRORREADINGFROMFILEX_S,
                              file);
      DRETURN(false);
   }

   char **token = stra_from_str(file_as_string, " \n\t");
   const bool ret = parse_cmdline_and_env(token, switch_list, answer_list);
   DRETURN(ret);
}

bool
ocs::QQuotaParameterClient::parse_switch_list(lList **switch_list, lList **answer_list) {
   DENTER(TOP_LAYER);
   uint32_t helpflag = 0;
   char *argstr = nullptr;
   bool ret = true;


   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */
   while (lGetNumberOfElem(*switch_list)) {
      if (parse_flag(switch_list, "-help", answer_list, &helpflag)) {
         show_usage(stdout);
         sge_exit(0);
         break;
      }

      if (parse_multi_stringlist(switch_list, "-h", answer_list, &host_name_list_, ST_Type, ST_name)) {
         /*
         ** resolve hostnames and replace them in list
         */
         for_each_rw_lv(ep, host_name_list_) {
            sge_resolve_host(ep, ST_name);
         }
         continue;
      }

      if (parse_string(switch_list, "-l", answer_list, &argstr)) {
         resource_match_list_ = centry_list_parse_from_string(resource_match_list_, argstr, false);
         sge_free(&argstr);
         continue;
      }
      if (parse_multi_stringlist(switch_list, "-u", answer_list, &user_name_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(switch_list, "-pe", answer_list, &pe_name_list_, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(switch_list, "-P", answer_list, &project_name_list_, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(switch_list, "-q", answer_list, &queue_name_list_, ST_Type, ST_name)) {
         continue;
      }

      if (parse_string(switch_list, "-ectx", answer_list, &argstr)) {
         if (strcmp(argstr, "client") == 0) {
            exec_context_ = ExecContext::CLIENT;
         } else if (strcmp(argstr, "server") == 0) {
            exec_context_ = ExecContext::SERVER;
         } else {
            char buf[BUFSIZ];
            snprintf(buf, sizeof(buf), MSG_PARSE_INVALIDOPTIONARGUMENTX_S, argstr);
            answer_list_add(answer_list, buf, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            sge_free(&argstr);
            ret = false;
         }
         sge_free(&argstr);
         continue;
      }
      if (parse_string(switch_list, "-fmt", answer_list, &argstr)) {
         if (strcmp(argstr, "plain") == 0) {
            output_format_ = OutputFormat::PLAIN;
         } else if (strcmp(argstr, "json") == 0) {
            output_format_ = OutputFormat::JSON;
         } else if (strcmp(argstr, "xml") == 0) {
            output_format_ = OutputFormat::XML;
         } else {
            char buf[BUFSIZ];
            snprintf(buf, sizeof(buf), MSG_PARSE_INVALIDOPTIONARGUMENTX_S, argstr);
            answer_list_add(answer_list, buf, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            sge_free(&argstr);
            ret = false;
         }
         sge_free(&argstr);
         continue;
      }
      if (parse_flag(switch_list, "-xml", answer_list, &helpflag)) {
         output_format_ = OutputFormat::XML;
         continue;
      }
   }

   if (lGetNumberOfElem(*switch_list)) {
      show_usage(stderr);
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_PARSE_TOOMANYOPTIONS);
      ret = false;
   }

   DRETURN(ret);
}

/** @brief Parse the command line and the environment into this object
 * @param answer_list receives error messages
 * @param argv the arguments, without the program name
 * @param envp the environment
 * @return true when the parameters are usable
 */
bool ocs::QQuotaParameterClient::parse_parameters(lList **answer_list, char **argv, char **envp) {
   DENTER(TOP_LAYER);
   lList *pcmdline = nullptr;

   dstring file = DSTRING_INIT;
   const char *user = component_get_username();
   const char *cell_root = bootstrap_get_cell_root();

   /* arguments from SGE_ROOT/common/sge_qquota file */
   get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QQUOTA_FILE);
   if (!parse_cmdline_from_file(sge_dstring_get_string(&file), &pcmdline, answer_list)) {
      sge_dstring_free(&file);
      DRETURN(false);
   }

   /* arguments from $HOME/.qquota file */
   get_user_home_file_path(&file, SGE_HOME_DEF_QQUOTA_FILE, user, answer_list);
   if (!parse_cmdline_from_file(sge_dstring_get_string(&file), &pcmdline, answer_list)) {
      sge_dstring_free(&file);
      DRETURN(false);
   }

   sge_dstring_free(&file);

   if (!parse_cmdline_and_env(argv, &pcmdline, answer_list)) {
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   if (!parse_switch_list(&pcmdline, answer_list)) {
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   DRETURN(true);
}
