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

#include <string>

#include "uti/sge_bootstrap_files.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_io.h"
#include "uti/sge_string.h"
#include "uti/sge_profiling.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/parse.h"

#include "qhost/ocs_QHostParameter.h"
#include "ocs_client_parse.h"
#include "ocs_client_print.h"

#include "msg_common.h"
#include "msg_qhost.h"
#include "msg_clients_common.h"

void ocs::QHostParameter::free_data() {
   lFreeList(&hostname_list_);
   lFreeList(&user_name_list_);
   lFreeList(&resource_match_list_);
}

bool
ocs::QHostParameter::show_usage(FILE *fp) {
   DENTER(TOP_LAYER);

   if (fp == nullptr) {
      DRETURN(false);
   }

   DSTRING_STATIC(ds, 256);

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

   fprintf(fp,"%s qhost [options]\n", MSG_SRC_USAGE);

   fprintf(fp, "  [-F [resource_attribute]]  %s\n", MSG_QHOST_F_OPT_USAGE);
   fprintf(fp, "  [-h hostlist]              %s\n", MSG_QHOST_h_OPT_USAGE);
   fprintf(fp, "  [-help]                    %s\n", MSG_COMMON_help_OPT_USAGE);
   fprintf(fp, "  [-j]                       %s\n", MSG_QHOST_j_OPT_USAGE);
   fprintf(fp, "  [-l attr=val,...]          %s\n", MSG_QHOST_l_OPT_USAGE);
   fprintf(fp, "  [-q]                       %s\n", MSG_QHOST_q_OPT_USAGE);
   fprintf(fp, "  [-u user[,user,...]]       %s\n", MSG_QHOST_u_OPT_USAGE);
   fprintf(fp, "  [-xml]                     %s\n", MSG_COMMON_xml_OPT_USAGE);

   DRETURN(true);
}


bool
ocs::QHostParameter::parse_cmdline_from_file(lList **switch_list, lList **answer_list, const char *file) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (switch_list == nullptr) {
      DRETURN(false);
   }

   // check if file exists - this is no error if it does not exist, because the file is optional
   if (!sge_is_file(file)) {
      DRETURN(true);
   }

   // read file content into a string
   int file_as_string_length;
   char *file_as_string = sge_file2string(file, &file_as_string_length);
   if (file_as_string == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
      DRETURN(false);
   }

   // split string into tokens and parse them
   char **token = stra_from_str(file_as_string, " \n\t");
   const bool ret = parse_cmdline_and_env(token, environ, switch_list, answer_list);
   sge_strafree(&token);
   sge_free(&file_as_string);
   DRETURN(ret);
}

bool
ocs::QHostParameter::parse_cmdline_and_env(char **argv, char **envp, lList **ppcmdline, lList **alpp) {
   DENTER(TOP_LAYER);
   char **sp;
   char **rp;

   rp = argv;

   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -q */
      if ((rp = parse_noopt(sp, "-q", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -F */
      if ((rp = parse_until_next_opt2(sp, "-F", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -h */
      if ((rp = parse_until_next_opt(sp, "-h", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -j */
      if ((rp = parse_noopt(sp, "-j", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -l */
      if ((rp = parse_until_next_opt(sp, "-l", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -u */
      if ((rp = parse_until_next_opt(sp, "-u", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -xml */
      if ((rp = parse_noopt(sp, "-xml", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* oops */
      show_usage(stderr);
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      DRETURN(false);
   }
   DRETURN(true);
}

int
ocs::QHostParameter::parse_switch_list(lList **ppcmdline, lList **alpp) {
   DENTER(TOP_LAYER);
   u_long32 helpflag;
   bool usageshowed = false;
   u_long32 full = 0;
   char * argstr = nullptr;
   lListElem *ep;
   int ret = 1;

   while (lGetNumberOfElem(*ppcmdline)) {
      if (parse_flag(ppcmdline, "-help",  alpp, &helpflag)) {
         usageshowed = true;
         show_usage(stdout);
         ret = 2;
         goto exit;
      }

      if (parse_multi_stringlist(ppcmdline, "-h", alpp, &hostname_list_, ST_Type, ST_name)) {
         /*
         ** resolve hostnames and replace them in list
         */
         for_each_rw(ep, hostname_list_) {
            if (sge_resolve_host(ep, ST_name) != CL_RETVAL_OK) {
               char buf[BUFSIZ];
               snprintf(buf, sizeof(buf), MSG_SGETEXT_CANTRESOLVEHOST_S, lGetString(ep,ST_name) );
               answer_list_add(alpp, buf, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               goto error;
            }
         }
         continue;
      }

      if (parse_multi_stringlist(ppcmdline, "-F", alpp, &resource_visible_list_, ST_Type, ST_name)) {
         show_ |= QHOST_DISPLAY_RESOURCES;
         continue;
      }
      if (parse_flag(ppcmdline, "-q", alpp, &full)) {
         if(full) {
            show_ |= QHOST_DISPLAY_QUEUES;
            full = 0;
         }
         continue;
      }

      if (parse_flag(ppcmdline, "-j", alpp, &full)) {
         if(full) {
            show_ |= QHOST_DISPLAY_JOBS;
            full = 0;
         }
         continue;
      }
      while (parse_string(ppcmdline, "-l", alpp, &argstr)) {
         resource_match_list_ = centry_list_parse_from_string(resource_match_list_, argstr, false);
         sge_free(&argstr);
         continue;
      }

      if (parse_multi_stringlist(ppcmdline, "-u", alpp, &user_name_list_, ST_Type, ST_name)) {
         show_ |= QHOST_DISPLAY_JOBS;
         continue;
      }

      if (parse_flag(ppcmdline, "-xml", alpp, &full)) {
         output_format_ = OutputFormat::XML;
         continue;
      }

   }

   DRETURN(1);

   error:
      ret = 0;
   exit:
      if (!usageshowed) {
         show_usage(stderr);
      }

   DRETURN(ret);
}

bool ocs::QHostParameter::parse_parameters(lList **alpp, char **argv, char **envp) {
   DENTER(TOP_LAYER);

   lList *switches_default_files = nullptr;

   // find cluster wide qhost file and parse it
   dstring file = DSTRING_INIT;
   const char *cell_root = bootstrap_get_cell_root();
   get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QHOST_FILE);
   parse_cmdline_from_file(&switches_default_files, alpp, sge_dstring_get_string(&file));

   // find user specific qhost file and parse it
   const char *username = component_get_username();
   if (get_user_home_file_path(&file, SGE_HOME_DEF_QHOST_FILE, username, alpp)) {
      parse_cmdline_from_file(&switches_default_files, alpp, sge_dstring_get_string(&file));
   }
   sge_dstring_free(&file);

   lList *switches_cmd_line = nullptr;

   // parse the command line
   if (!parse_cmdline_and_env(++argv, environ, &switches_cmd_line, alpp)) {
      answer_list_output(alpp);
      lFreeList(&switches_cmd_line);
      sge_exit(1);
   }

   // find duplicates in file and commandline and remove them
   const lListElem* switch_cmd;
   for_each_ep(switch_cmd, switches_cmd_line) {
      lListElem *switch_default = nullptr;

      while ((switch_default = lGetElemStrRW(switches_default_files, SPA_switch_val, lGetString(switch_cmd, SPA_switch_val))) != nullptr) {
         lRemoveElem(switches_default_files, &switch_default);
      }
   }

   // merge the command line and the file options
   if (lGetNumberOfElem(switches_cmd_line) > 0) {
      lAddList(switches_cmd_line, &switches_default_files);
   } else if (lGetNumberOfElem(switches_default_files) > 0) {
      lAddList(switches_default_files, &switches_cmd_line);
      switches_cmd_line = switches_default_files;
   }

   // parse the commandline
   int is_ok = parse_switch_list(&switches_cmd_line, alpp);
   lFreeList(&switches_cmd_line);
   if (is_ok == 0) {
      answer_list_output(alpp);
      sge_exit(1);
   } else if (is_ok == 2) {
      /* -help output generated, exit normally */
      answer_list_output(alpp);
      sge_exit(0);
   }
   return true;
}