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
#include "uti/msg_utilib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/parse.h"

#include "qrstat/ocs_QRStatParameter.h"
#include "ocs_client_parse.h"
#include "parse_qsub.h"
#include "msg_common.h"
#include "usage.h"

void
ocs::QRStatParameter::free_data() {
   DENTER(TOP_LAYER);
   lFreeList(&user_list);
   lFreeList(&ar_id_list);
   DRETURN_VOID;
}

bool
ocs::QRStatParameter::sge_parse_from_file_qrstat(const char *file, lList **ppcmdline, lList **alpp) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (ppcmdline == nullptr) {
      ret = false;
   } else {
      if (!sge_is_file(file)) {
         /*
          * This is no error
          */
         DPRINTF("file " SFQ " does not exist\n", file);
      } else {
         char *file_as_string = nullptr;
         int file_as_string_length;

         file_as_string = sge_file2string(file, &file_as_string_length);
         if (file_as_string == nullptr) {
            answer_list_add_sprintf(alpp, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
            ret = false;
         } else {
            const char **token = nullptr;

            token = (const char **)stra_from_str(file_as_string, " \n\t");
            *alpp = cull_parse_cmdline(QRSTAT, token, environ, ppcmdline, FLG_USE_PSEUDOS);
         }
      }
   }
   DRETURN(ret);
}

bool
ocs::QRStatParameter::sge_parse_qrstat(lList **answer_list, lList **cmdline) {
   bool ret = true;

   DENTER(TOP_LAYER);
   is_summary = true;
   while (lGetNumberOfElem(*cmdline)) {
      u_long32 value;

      /* -help */
      if (opt_list_has_X(*cmdline, "-help")) {
         sge_usage(QRSTAT, stdout);
         sge_exit(0);
      }

      /* -u */
      while (parse_multi_stringlist(cmdline, "-u", answer_list,
                                    &user_list, ST_Type, ST_name)) {
      }

      /* -explain */
      while (parse_flag(cmdline, "-explain", answer_list, &value)) {
         is_explain = (value > 0) ? true : false;
      }

      /* -xml */
      while (parse_flag(cmdline, "-xml", answer_list, &value)) {
         is_xml = (value > 0) ? true : false;
      }

      /* -ar */
      while (parse_u_longlist(cmdline, "-ar", answer_list, &ar_id_list)) {
         is_summary = false;
      }

      if (lGetNumberOfElem(*cmdline)) {
         sge_usage(QRSTAT, stdout);
         answer_list_add(answer_list, MSG_PARSE_TOOMANYOPTIONS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = false;
         break;
      }
   }

   if (sge_uid2user(geteuid(), user, sizeof(user), MAX_NIS_RETRIES)) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL, MSG_SYSTEM_RESOLVEUSER);
      ret = false;
   }

   DRETURN(ret);
}

bool
ocs::QRStatParameter::parse_parameters(lList **answer_list, const char **argv, char **envp) {
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
