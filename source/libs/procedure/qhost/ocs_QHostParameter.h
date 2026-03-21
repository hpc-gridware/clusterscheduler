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

#include "cull/cull.h"

namespace ocs {
   class QHostParameter {
   public:
      enum class OutputFormat{
         PLAIN,
         XML
      };
   private:
      lList *hostname_list_ = nullptr;
      lList *user_name_list_ = nullptr;
      lList *resource_match_list_ = nullptr;
      lList *resource_visible_list_ = nullptr;
      u_long32 show_ = 0;
      OutputFormat output_format_ = OutputFormat::PLAIN;

      void free_data();

      bool show_usage(FILE *fp);
      bool parse_cmdline_and_env(char **argv, char **envp, lList **ppcmdline, lList **alpp);
      bool parse_cmdline_from_file(lList **switch_list, lList **answer_list, const char *file);
      int parse_switch_list(lList **ppcmdline, lList **alpp);
   public:
      QHostParameter() = default;
      virtual ~QHostParameter() { free_data(); }

      [[nodiscard]] const lList *get_hostname_list() const { return hostname_list_; }
      [[nodiscard]] const lList *get_user_name_list() const { return user_name_list_; }
      [[nodiscard]] const lList *get_resource_match_list() const { return resource_match_list_; }
      [[nodiscard]] const lList *get_resource_visible_list() const { return resource_visible_list_; }
      [[nodiscard]] u_long32 get_show() const { return show_; }
      [[nodiscard]] OutputFormat get_output_format() const { return output_format_ ; }

      bool parse_parameters(lList **answer_list, char **argv, char **envp);
   };
}