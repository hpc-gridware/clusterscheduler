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

#include "procedure/ocs_ProcedureParameter.h"

namespace ocs {
   class QQuotaParameter : public ProcedureParameter {
#pragma region Constants

   private:
      static constexpr auto QUEUE_NAME_LIST = "queue_name_list";
      static constexpr auto HOSTNAME_LIST = "hostname_list";
      static constexpr auto PE_NAME_LIST = "pe_name_list";
      static constexpr auto PROJECT_NAME_LIST = "project_name_list";
      static constexpr auto RESOURCE_MATCH_LIST = "resource_match_list";
      static constexpr auto USER_NAME_LIST = "user_name_list";

#pragma endregion

#pragma region Data

   protected:
      lList *queue_name_list_ = nullptr; //< -q
      lList *host_name_list_ = nullptr; //< -h
      lList *pe_name_list_ = nullptr; //< -pe
      lList *project_name_list_ = nullptr; //< -P
      lList *resource_match_list_ = nullptr; //< -l
      lList *user_name_list = nullptr; //< -u

   public:
      [[nodiscard]] lList *get_host_list() const { return host_name_list_; }
      [[nodiscard]] lList *get_resource_match_list() const { return resource_match_list_; }
      [[nodiscard]] lList *get_user_list() const { return user_name_list; }
      [[nodiscard]] lList *get_pe_list() const { return pe_name_list_; }
      [[nodiscard]] lList *get_project_list() const { return project_name_list_; }
      [[nodiscard]] lList *get_cqueue_list() const { return queue_name_list_; }

#pragma endregion

#pragma region Marshaling

   protected:
      void set_bundle(const lList *bundle) override;

   public:
      [[nodiscard]] lList *get_bundle() override;

#pragma endregion

#pragma region Constructor/Destructor

   private:

   public:
      explicit QQuotaParameter(lList *bundle);

      explicit QQuotaParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name)) {
      };

      ~QQuotaParameter() override;

#pragma endregion
   };
}
