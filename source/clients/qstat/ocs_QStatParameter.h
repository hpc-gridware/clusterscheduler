#pragma once
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

#include "cull/cull.h"

#include "sgeobj/sge_qinstance_state.h"

#include "ocs_client_print.h"

namespace ocs {
   class QStatParameter {
   public:
      enum class OutputFormat{
         PLAIN,
         XML
      };
      enum class OutputMode {
         QSELECT,
         QSTAT_GROUP,
         QSTAT_DEFAULT
      };
      bool need_queues_ = false; //< need to fetch queues from master
      bool need_job_list_ = true; //< need to fetch job list from master

      u_long32 full_listing_ = QSTAT_DISPLAY_ALL; // similar to *show* in qhost
      u_long32 job_info_ = 0; // -j

      bool state_filter_ = false; // -s
      std::string state_filter_value_; // -s value

      u_long32 explain_bits_ = QI_DEFAULT;        /* -explain  */
      lList *qresource_list_ = nullptr;        /* -F qresource_request          */

      u_long32 queue_state_ = U_LONG32_MAX;         /* -qs       */

      lList *resource_list_ = nullptr;         /* -l resource_request           */
      lList* queueref_list_ = nullptr;         /* -q queue_list                 */
      lList* peref_list_ = nullptr;            /* -pe pe_list                   */
      lList* user_list_ = nullptr;             /* -u user_list - selects jobs   */
      lList* queue_user_list_ = nullptr;       /* -U user_list - selects queues */

      u_long32 group_opt_ = 0;           /* -g        */

      u_long32 isXML_ = 0;             /* -xml      */

      /* length of the longest queue name */
      int longest_queue_length = 30;

      OutputMode output_mode_ = OutputMode::QSTAT_DEFAULT;
      lList *jid_list_ = nullptr; // -j argument list

   private:
#if 0
      lList *hostname_list_ = nullptr;
      lList *user_name_list_ = nullptr;
      lList *resource_match_list_ = nullptr;
      lList *resource_visible_list_ = nullptr;
      u_long32 show_ = 0;
      OutputFormat output_format_ = OutputFormat::PLAIN;

      bool qhost_usage(FILE *fp);
      bool sge_parse_cmdline_qhost(char **argv, char **envp, lList **ppcmdline, lList **alpp);
      bool switch_list_qhost_parse_from_file(lList **switch_list, lList **answer_list, const char *file);
      int sge_parse_qhost(lList **ppcmdline, lList **alpp);
#endif
      lList * sge_parse_qstat(lList **ppcmdline, lList **ppljid);
      bool switch_list_qstat_parse_from_file(lList **switch_list, lList **answer_list, const char *file);
      int qstat_usage(FILE *fp, char *what);
      bool switch_list_qstat_parse_from_cmdline(lList **ppcmdline, lList **answer_list, char **argv);

      void free_data();
   public:
      QStatParameter() = default;
      virtual ~QStatParameter() { free_data(); }

#if 0
      [[nodiscard]] const lList *get_hostname_list() const { return hostname_list_; }
      [[nodiscard]] const lList *get_user_name_list() const { return user_name_list_; }
      [[nodiscard]] const lList *get_resource_match_list() const { return resource_match_list_; }
      [[nodiscard]] const lList *get_resource_visible_list() const { return resource_visible_list_; }
      [[nodiscard]] u_long32 get_show() const { return show_; }
      [[nodiscard]] OutputFormat get_output_format() const { return output_format_ ; }
#endif

      bool parse_parameters(lList **answer_list, char **argv, char **envp);
   };
}
