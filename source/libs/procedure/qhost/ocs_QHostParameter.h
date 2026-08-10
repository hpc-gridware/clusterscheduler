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

/** @file
 * @brief Parameters of `qhost`: what the call was asked to report
 */

#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"

#include "procedure/ocs_ProcedureParameter.h"

namespace ocs {
   /** @brief Everything one `qhost` call was asked to report
    *
    * Built from the command line on the client and from the marshalled bundle
    * on the server, so that both sides run the same model against the same
    * parameters.
    *
    * @ingroup libprocedure
    */
   class QHostParameter : public ProcedureParameter {


#pragma region Constants

   private:
      /** @name Names of this command's entries in the marshalled bundle
       * @{
       */
      static constexpr auto HOSTNAME_LIST = "hostname_list";                     ///< The hosts to report
      static constexpr auto USER_NAME_LIST = "user_name_list";                   ///< The users whose jobs to report
      static constexpr auto RESOURCE_LIST = "resource_list";                     ///< The resource filter
      static constexpr auto RESOURCE_VISIBLE_LIST = "resource_visible_list";     ///< The resources to show
      static constexpr auto SHOW = "show";                                       ///< What to report
      static constexpr auto OUTPUT_FORMAT = "output_format";                     ///< Plain, XML or JSON
      /** @} */

#pragma endregion

#pragma region Data

   protected:
      lList *hostname_list_ = nullptr;          ///< The hosts to report; empty means all
      lList *user_name_list_ = nullptr;         ///< The users whose jobs to report; empty means all
      lList *resource_match_list_ = nullptr;    ///< The `-l` filter: a host is reported only if it matches
      lList *resource_visible_list_ = nullptr;  ///< The `-F` selection: which resources appear in the output
      uint32_t show_ = 0;                       ///< `QHOST_DISPLAY_*` bitmask: queues, jobs, resources

   public:
      /** @brief The hosts to report
       * @return the host names, empty when all hosts were requested
       */
      [[nodiscard]] const lList *get_hostname_list() const { return hostname_list_; }

      /** @brief The users whose jobs to report
       * @return the user names, empty when all users were requested
       */
      [[nodiscard]] const lList *get_user_name_list() const { return user_name_list_; }

      /** @brief The resource filter deciding which hosts are reported
       * @return the requested resources
       */
      [[nodiscard]] const lList *get_resource_match_list() const { return resource_match_list_; }

      /** @brief The resources the output is to show
       * @return the selected resources
       */
      [[nodiscard]] const lList *get_resource_visible_list() const { return resource_visible_list_; }

      /** @brief What the user asked to see
       * @return a `QHOST_DISPLAY_*` bitmask
       */
      [[nodiscard]] uint32_t get_show() const { return show_; }

#pragma endregion

#pragma region Marshaling

   protected:
      void set_bundle(const lList *bundle) override;

   public:
      [[nodiscard]] lList *get_bundle() override;

#pragma endregion

#pragma region Constructor/Destructor
   public:
      explicit QHostParameter(lList *bundle, gdi::Packet *packet);

      /** @brief Build empty parameters, to be filled in by the client
       * @param procedure_name the command being run
       */
      explicit QHostParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name), nullptr) {};
      ~QHostParameter() override;
#pragma endregion


   };
}