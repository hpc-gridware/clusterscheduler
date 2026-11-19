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

/** @file
 * @brief Parameters of `qrstat`: what the call was asked to report
 */

#include "cull/cull.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"

#include "procedure/ocs_ProcedureParameter.h"

namespace ocs {
   /** @brief Everything one `qrstat` call was asked to report
    *
    * Built from the command line on the client and from the marshalled bundle
    * on the server, so that both sides run the same model against the same
    * parameters.
    *
    * @ingroup libprocedure
    */
   class QRStatParameter : public ProcedureParameter {
#pragma region Constants

   private:
      /** @name Names of this command's entries in the marshalled bundle
       * @{
       */
      static constexpr auto USER_NAME_LIST = "user_name_list";   ///< The users whose reservations to report
      static constexpr auto AR_ID_LIST = "ar_id_list";           ///< The reservations to report
      static constexpr auto EXPLAIN = "explain";                 ///< Whether to explain an error state
      static constexpr auto SUMMARY = "summary";                 ///< Whether one line per reservation is enough
      static constexpr auto USERNAME = "username";               ///< The calling user, resolved on the client
      /** @} */

#pragma endregion

#pragma region Data

   protected:
      ocs::tom::ListPtr<ocs::tom::ST> user_list_;     ///< -u user_list
      ocs::tom::ListPtr<ocs::tom::ULNG> ar_id_list_;  ///< -ar ar_id
      bool is_explain_ = false; ///< -explain
      bool is_summary_ = false; ///< show summary of selected ar's or all details of one or multiple ar's
      char user_[128] = ""; ///< executing user on client side

   public:
      void transform_user_list();

      /** @brief The users whose reservations to report
       * @return the user names, empty when all users were requested
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::ST> get_user_list() const { return *user_list_; }

      /** @brief The reservations to report
       * @return the reservation ids, empty when all were requested
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::ULNG> get_ar_id_list() const { return *ar_id_list_; }

      /** @brief Whether the user asked why a reservation is in an error state
       * @return true when `-explain` was given
       */
      [[nodiscard]] bool is_explain() const { return is_explain_; }

      /** @brief Whether one line per reservation is wanted rather than full detail
       * @return true for the summary listing
       */
      [[nodiscard]] bool is_summary() const { return is_summary_; }

#pragma endregion

#pragma region Marshaling

   protected:
      void set_bundle(ocs::tom::ConstList<ocs::tom::SPP> bundle) override;

   public:
      [[nodiscard]] ocs::tom::ListPtr<ocs::tom::SPP> get_bundle() override;

#pragma endregion

#pragma region Constructor/Destructor

   public:

      explicit QRStatParameter(ocs::tom::ConstList<ocs::tom::SPP> bundle, gdi::Packet *packet);

      /** @brief Build empty parameters, to be filled in by the client
       * @param procedure_name the command being run
       */
      explicit QRStatParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name), nullptr) {
      };

      ~QRStatParameter() override = default;

#pragma endregion
   };
}
