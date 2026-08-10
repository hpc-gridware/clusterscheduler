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
 * @brief Parameters of `qquota`: what the call was asked to report
 */

#include "procedure/ocs_ProcedureParameter.h"

namespace ocs {
   /** @brief Everything one `qquota` call was asked to report
    *
    * Each list narrows the report along one dimension; a rule is printed only
    * where it matches all of them. Built from the command line on the client
    * and from the marshalled bundle on the server.
    *
    * @ingroup libprocedure
    */
   class QQuotaParameter : public ProcedureParameter {
#pragma region Constants

   private:
      /** @name Names of this command's entries in the marshalled bundle
       * @{
       */
      static constexpr auto QUEUE_NAME_LIST = "queue_name_list";           ///< The cluster queues to report on
      static constexpr auto HOSTNAME_LIST = "hostname_list";               ///< The hosts to report on
      static constexpr auto PE_NAME_LIST = "pe_name_list";                 ///< The parallel environments to report on
      static constexpr auto PROJECT_NAME_LIST = "project_name_list";       ///< The projects to report on
      static constexpr auto RESOURCE_MATCH_LIST = "resource_match_list";   ///< The resources to report on
      static constexpr auto USER_NAME_LIST = "user_name_list";             ///< The users to report on
      /** @} */

#pragma endregion

#pragma region Data

   protected:
      lList *queue_name_list_ = nullptr;       ///< The cluster queues to report on, from `-q`; empty means all
      lList *host_name_list_ = nullptr;        ///< The hosts to report on, from `-h`; empty means all
      lList *pe_name_list_ = nullptr;          ///< The parallel environments to report on, from `-pe`; empty means all
      lList *project_name_list_ = nullptr;     ///< The projects to report on, from `-P`; empty means all
      lList *resource_match_list_ = nullptr;   ///< The resources to report on, from `-l`; empty means all
      lList *user_name_list = nullptr;         ///< The users to report on, from `-u`; empty means the calling user

   public:
      /** @brief The hosts to report on
       * @return the host names, empty when the report is not restricted by host
       */
      [[nodiscard]] lList *get_host_list() const { return host_name_list_; }

      /** @brief The resources to report on
       * @return the resource names, empty when the report is not restricted by resource
       */
      [[nodiscard]] lList *get_resource_match_list() const { return resource_match_list_; }

      /** @brief The users to report on
       * @return the user names
       */
      [[nodiscard]] lList *get_user_list() const { return user_name_list; }

      /** @brief The parallel environments to report on
       * @return the PE names, empty when the report is not restricted by PE
       */
      [[nodiscard]] lList *get_pe_list() const { return pe_name_list_; }

      /** @brief The projects to report on
       * @return the project names, empty when the report is not restricted by project
       */
      [[nodiscard]] lList *get_project_list() const { return project_name_list_; }

      /** @brief The cluster queues to report on
       * @return the queue names, empty when the report is not restricted by queue
       */
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
      explicit QQuotaParameter(lList *bundle, gdi::Packet *packet);

      /** @brief Build empty parameters, to be filled in by the client
       * @param procedure_name the command being run
       */
      explicit QQuotaParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name), nullptr) {
      };

      ~QQuotaParameter() override;

#pragma endregion
   };
}
