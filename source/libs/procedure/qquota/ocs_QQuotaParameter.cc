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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_string.h"
#include "uti/sge_io.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_feature.h"

#include "ocs_QQuotaParameter.h"
#include "msg_qquota.h"

#include "ocs_client_parse.h"
#include "msg_common.h"
#include "msg_clients_common.h"

void
ocs::QQuotaParameter::free_data() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

bool
ocs::QQuotaParameter::qquota_usage(FILE *fp) {
   DENTER(TOP_LAYER);
   dstring ds;
   char buffer[256];

   if (fp == nullptr) {
      DRETURN(false);
   }

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));
   fprintf(fp,"%s qquota [options]\n", MSG_SRC_USAGE);
   fprintf(fp, "  [-help]                              %s\n", MSG_COMMON_help_OPT_USAGE);
   fprintf(fp, "  [-h wc_host_list|wc_hostgroup_list]  %s\n", MSG_QQUOTA_h_OPT_USAGE);
   fprintf(fp, "  [-l resource_attributes]             %s\n", MSG_QQUOTA_l_OPT_USAGE);
   fprintf(fp, "  [-u wc_user]                         %s\n", MSG_QQUOTA_u_OPT_USAGE);
   fprintf(fp, "  [-pe wc_pe_list]                     %s\n", MSG_QQUOTA_pe_OPT_USAGE);
   fprintf(fp, "  [-P wc_project_list]                 %s\n", MSG_QQUOTA_P_OPT_USAGE);
   fprintf(fp, "  [-q wc_cqueue_list]                  %s\n", MSG_QQUOTA_q_OPT_USAGE);
   fprintf(fp, "  [-xml]                               %s\n", MSG_COMMON_xml_OPT_USAGE);
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
ocs::QQuotaParameter::sge_parse_cmdline_qquota(char *argv[], lList **ppcmdline, lList **alpp) {
   DENTER(TOP_LAYER);
   char **sp;
   char **rp;

   if (argv == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_NULLPOINTER);
      DRETURN(false);
   }

   rp = ++argv;
   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -h option */
      if ((rp = parse_until_next_opt2(sp, "-h", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -l option */
      if ((rp = parse_until_next_opt2(sp, "-l", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -u option */
      if ((rp = parse_until_next_opt2(sp, "-u", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -pe option */
      if ((rp = parse_until_next_opt2(sp, "-pe", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -P option */
      if ((rp = parse_until_next_opt2(sp, "-P", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -q */
      if ((rp = parse_until_next_opt2(sp, "-q", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -xml */
      if ((rp = parse_noopt(sp, "-xml", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* oops */
      qquota_usage(stderr);
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      DRETURN(false);
   }
   DRETURN(true);
}

bool
ocs::QQuotaParameter::sge_parse_from_file_qquota(const char *file, lList **ppcmdline, lList **alpp) {
   DENTER(TOP_LAYER);

   if (ppcmdline == nullptr) {
      DRETURN(false);
   }
   if (!sge_is_file(file)) {
      // This is no error
      DRETURN(true);
   }

   int file_as_string_length;
   const char *file_as_string = sge_file2string(file, &file_as_string_length);
   if (file_as_string == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
      DRETURN(false);
   }

   char **token = stra_from_str(file_as_string, " \n\t");
   const bool ret = sge_parse_cmdline_qquota(token, ppcmdline, alpp);
   DRETURN(ret);
}

bool
ocs::QQuotaParameter::sge_parse_qquota(lList **ppcmdline, lList **alpp)
{
   u_long32 helpflag = 0;
   char *argstr = nullptr;
   bool ret = true;

   DENTER(TOP_LAYER);

   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */
   while (lGetNumberOfElem(*ppcmdline)) {
      if (parse_flag(ppcmdline, "-help",  alpp, &helpflag)) {
         qquota_usage(stdout);
         sge_exit(0);
         break;
      }

      if (parse_multi_stringlist(ppcmdline, "-h", alpp, &host_list, ST_Type, ST_name)) {
         /*
         ** resolve hostnames and replace them in list
         */
         lListElem *ep = nullptr;
         for_each_rw (ep, host_list) {
            sge_resolve_host(ep, ST_name);
         }
         continue;
      }

      if (parse_string(ppcmdline, "-l", alpp, &argstr)) {
         resource_match_list = centry_list_parse_from_string(resource_match_list, argstr, false);
         sge_free(&argstr);
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-u", alpp, &user_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-pe", alpp, &pe_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-P", alpp, &project_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-q", alpp, &cqueue_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_flag(ppcmdline, "-xml", alpp, &helpflag)) {
         is_xml = true;
         continue;
      }
   }

   if (lGetNumberOfElem(*ppcmdline)) {
      qquota_usage(stderr);
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_PARSE_TOOMANYOPTIONS);
      ret = false;
   }

   DRETURN(ret);
}

bool ocs::QQuotaParameter::parse_parameters(lList **answer_list, char **argv, char **envp) {
   DENTER(TOP_LAYER);
   lList *pcmdline = nullptr;

   dstring file = DSTRING_INIT;
   const char *user = component_get_username();
   const char *cell_root = bootstrap_get_cell_root();

   /* arguments from SGE_ROOT/common/sge_qquota file */
   get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QQUOTA_FILE);
   if (!sge_parse_from_file_qquota(sge_dstring_get_string(&file), &pcmdline, answer_list)) {
      sge_dstring_free(&file);
      DRETURN(false);
   }

   /* arguments from $HOME/.qquota file */
   get_user_home_file_path(&file, SGE_HOME_DEF_QQUOTA_FILE, user, answer_list);
   if (!sge_parse_from_file_qquota(sge_dstring_get_string(&file), &pcmdline, answer_list)) {
      sge_dstring_free(&file);
      DRETURN(false);
   }

   sge_dstring_free(&file);

   if (!sge_parse_cmdline_qquota(argv, &pcmdline, answer_list)) {
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   if (!sge_parse_qquota(&pcmdline, answer_list)) {
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   DRETURN(true);
}