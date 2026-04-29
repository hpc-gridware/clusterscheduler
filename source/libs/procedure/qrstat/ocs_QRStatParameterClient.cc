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

#include "uti/sge_bootstrap_files.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_io.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/msg_utilib.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/parse.h"

#include "qrstat/ocs_QRStatParameterClient.h"
#include "ocs_client_parse.h"
#include "parse_qsub.h"
#include "msg_common.h"
#include "usage.h"

extern char **environ;


bool
ocs::QRStatParameterClient::sge_parse_from_file_qrstat(const char *file, lList **ppcmdline, lList **alpp) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (ppcmdline == nullptr) {
      ret = false;
   } else {
      if (!sge_is_file(file)) {
         DPRINTF("file " SFQ " does not exist\n", file);
      } else {
         int file_as_string_length;
         char *file_as_string = sge_file2string(file, &file_as_string_length);
         if (file_as_string != nullptr) {
            auto token = const_cast<const char **>(stra_from_str(file_as_string, " \n\t"));
            *alpp = cull_parse_cmdline(QRSTAT, token, environ, ppcmdline, FLG_USE_PSEUDOS);
            sge_free(&file_as_string);
            sge_free(&token);
         } else {
            answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

bool
ocs::QRStatParameterClient::sge_parse_qrstat(lList **answer_list, lList **cmdline) {
   bool ret = true;

   DENTER(TOP_LAYER);
   is_summary_ = true;
   while (lGetNumberOfElem(*cmdline)) {
      uint32_t value;
      char *argstr = nullptr;

      /* -help */
      if (opt_list_has_X(*cmdline, "-help")) {
         sge_usage(QRSTAT, stdout);
         sge_exit(0);
      }

      /* -u */
      while (parse_multi_stringlist(cmdline, "-u", answer_list,
                                    &user_list_, ST_Type, ST_name)) {
      }

      /* -explain */
      while (parse_flag(cmdline, "-explain", answer_list, &value)) {
         is_explain_ = (value > 0) ? true : false;
      }

      if (parse_string_arg(cmdline, "-ectx", answer_list, &argstr)) {
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

      if (parse_string_arg(cmdline, "-fmt", answer_list, &argstr)) {
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

      /* -xml */
      while (parse_flag(cmdline, "-xml", answer_list, &value)) {
         output_format_ = OutputFormat::XML;
      }

      /* -ar */
      while (parse_u_longlist(cmdline, "-ar", answer_list, &ar_id_list_)) {
         is_summary_ = false;
      }

      if (lGetNumberOfElem(*cmdline)) {
         sge_usage(QRSTAT, stdout);
         answer_list_add(answer_list, MSG_PARSE_TOOMANYOPTIONS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = false;
         break;
      }
   }

   if (sge_uid2user(geteuid(), user_, sizeof(user_), MAX_NIS_RETRIES)) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SYSTEM_RESOLVEUSER);
      ret = false;
   }

   DRETURN(ret);
}

bool
ocs::QRStatParameterClient::parse_parameters(lList **answer_list, const char **argv, char **envp) {
   DENTER(TOP_LAYER);
   lList *pcmdline = nullptr;

   dstring file = DSTRING_INIT;
   const char *user = component_get_username();
   const char *cell_root = bootstrap_get_cell_root();

   // Read switches from global file
   get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QRSTAT_FILE);
   if (!sge_parse_from_file_qrstat(sge_dstring_get_string(&file), &pcmdline, answer_list)) {
      sge_dstring_free(&file);
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   // Read switches from user specific file
   if (!get_user_home_file_path(&file, SGE_HOME_DEF_QRSTAT_FILE, user, answer_list)) {
      sge_dstring_free(&file);
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   if (!sge_parse_from_file_qrstat(sge_dstring_get_string(&file), &pcmdline, answer_list)) {
      sge_dstring_free(&file);
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   sge_dstring_free(&file);

   *answer_list = cull_parse_cmdline(QRSTAT, argv+1, environ, &pcmdline, FLG_USE_PSEUDOS);
   if (*answer_list != nullptr) {
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   // initialize (must be done before `goto error_exit`)
   if (!sge_parse_qrstat(answer_list, &pcmdline)) {
      lFreeList(&pcmdline);
      DRETURN(false);
   }

   // no longer needed
   lFreeList(&pcmdline);
   DRETURN(true);
}
