#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

#include "procedure/ocs_ProcedureParameter.h"

namespace ocs {
   class QRStatParameter : public ProcedureParameter {
#pragma region Constants

   private:
      static constexpr auto USER_NAME_LIST = "user_name_list";
      static constexpr auto AR_ID_LIST = "ar_id_list";
      static constexpr auto EXPLAIN = "explain";
      static constexpr auto SUMMARY = "summary";
      static constexpr auto USERNAME = "username";

#pragma endregion

#pragma region Data

   protected:
      lList *user_list_ = nullptr; ///< -u user_list
      lList *ar_id_list_ = nullptr; ///< -ar ar_id
      bool is_explain_ = false; ///< -explain
      bool is_summary_ = false; ///< show summary of selected ar's or all details of one or multiple ar's
      char user_[128] = ""; ///< executing user on client side

   public:
      void transform_user_list();

      [[nodiscard]] lList *get_user_list() const { return user_list_; }
      [[nodiscard]] lList *get_ar_id_list() const { return ar_id_list_; }
      [[nodiscard]] bool is_explain() const { return is_explain_; }
      [[nodiscard]] bool is_summary() const { return is_summary_; }

#pragma endregion

#pragma region Marshaling

   protected:
      void set_bundle(const lList *bundle) override;

   public:
      [[nodiscard]] lList *get_bundle() override;

#pragma endregion

#pragma region Constructor/Destructor

   public:
      explicit QRStatParameter(lList *bundle);

      explicit QRStatParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name)) {
      };

      ~QRStatParameter() override;

#pragma endregion
   };
}
