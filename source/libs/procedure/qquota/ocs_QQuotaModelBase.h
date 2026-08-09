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
 * @brief Base model of `qquota`: the lists the report is built from
 */

#include "cull/cull.h"

#include "ocs_QQuotaParameter.h"

namespace ocs {
   /** @brief Base model for qquota that holds the CULL lists needed for resource quota rendering.
    *
    * Fetches resource quota sets (RQS), complex entries, user sets, host groups,
    * and execution hosts, then passes them to the view for rendering.
    *
    * ## Client vs. server execution
    *
    * - **QQuotaModelClient** (`ExecContext::CLIENT`): issues GDI requests to qmaster
    *   to fetch all required lists.
    * - **QQuotaModelServer** (`ExecContext::SERVER`): reads directly from the in-process
    *   qmaster master lists, avoiding any network round-trip.
    *
    * ## Virtual pipeline inside make_snapshot()
    *
    * 1. `fetch_data()` — populates the CULL list members (overridden per context).
    *
    * @ingroup libprocedure
    */
   class QQuotaModelBase {
#pragma region Data
   protected:
      lList *centry_list_ = nullptr;      ///< Complex entries, for the type of each limited resource
      lList *user_set_list_ = nullptr;    ///< User sets, to resolve the `@group` filters of a rule
      lList *hgroup_list_ = nullptr;      ///< Host groups, to resolve the `@group` filters of a rule
      lList *exec_host_list_ = nullptr;   ///< Execution hosts
      lList *rqs_list_ = nullptr;         ///< The resource quota sets being reported

   public:
      /** @brief The resource quota sets being reported
       * @return the RQS list
       */
      [[nodiscard]] const lList *get_rqs_list() const { return rqs_list_; }

      /** @brief The complex entries
       * @return the centry list
       */
      [[nodiscard]] const lList *get_centry_list() const { return centry_list_; }

      /** @brief The user sets
       * @return the user set list
       */
      [[nodiscard]] const lList *get_user_set_list() const { return user_set_list_; }

      /** @brief The host groups
       * @return the hgroup list
       */
      [[nodiscard]] const lList *get_hgroup_list() const { return hgroup_list_; }

      /** @brief The execution hosts
       * @return the exec host list
       */
      [[nodiscard]] const lList *get_exec_host_list() const { return exec_host_list_; }
#pragma endregion

#pragma region Data Retrieval
   protected:
      static lEnumeration *get_rqs_what();

      static lEnumeration *get_centry_what();

      static lEnumeration *get_user_set_what();

      static lEnumeration *get_hgroup_what();

      static lEnumeration *get_host_what();

      static lCondition *get_host_where(const lList *host_list);

      virtual bool fetch_data(lList **answer_list, const lList *host_list);
   public:
      virtual bool make_snapshot(lList **answer_list, QQuotaParameter &parameter);
#pragma endregion

#pragma region Constructors/Destructors
   public:
      QQuotaModelBase() = default;
      virtual ~QQuotaModelBase();
#pragma endregion

   };
}
