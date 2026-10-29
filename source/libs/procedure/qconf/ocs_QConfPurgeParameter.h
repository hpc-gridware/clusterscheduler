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
 * @brief The parameters of the `qconf -purge` procedure (CS-2948)
 *
 * Shared by both halves: the client fills them, qmaster reads them back. The
 * model and the provider belong to qmaster alone and live in
 * ocs_QConfPurgeServer.h.
 *
 * `-purge` removes the host or host group specific values of some attributes
 * from one cluster queue. It used to be a read-modify-write in the client: fetch
 * the queue together with the members of its host group `@@<queue>`, strip the
 * entries locally and send the whole queue back with SET_ALL. Since CS-2677 a
 * host list arriving with a queue change is an instruction to write `@@<queue>`,
 * so every purge rewrote the host group with the members it had read, and
 * anything changed on the queue in between was reverted. As a procedure the
 * queue is changed where it is stored, under the lock, and only the attributes
 * that lost an entry are written.
 *
 * What travels is what the user typed - the queue, the attribute pattern list
 * and the host or host group - and nothing the client derived from reading the
 * queue, because the client no longer reads it.
 */

#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "uti/ocs_ProgName.h"

#include "../ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief What to purge from which queue
    * @ingroup libprocedure
    */
   class QConfPurgeParameter : public ProcedureParameter {
   public:
      static constexpr auto QUEUE_PURGE = "queue_purge";     ///< The sub-procedure name
      static constexpr auto PURGE_QUEUE = "purge_queue";     ///< The cluster queue
      static constexpr auto PURGE_ATTRIBUTES = "purge_attr"; ///< The attribute patterns
      static constexpr auto PURGE_DOMAIN = "purge_domain";   ///< The host or host group

   private:
      std::string queue_;        ///< the cluster queue name
      std::string attributes_;   ///< comma separated attribute names, wildcards allowed
      std::string domain_;       ///< the host or host group whose values are removed

   public:
      /** @brief Build the parameters on the client side
       *
       * The procedure name has to be the program's: it is the first half of the
       * key the dispatch table is looked up by.
       */
      QConfPurgeParameter() : ProcedureParameter(to_cstr(QCONF), nullptr) {
         set_sub_procedure_name(QUEUE_PURGE);
      }

      /** @brief Rebuild the parameters inside qmaster from the request bundle
       * @param bundle the request bundle received over GDI
       * @param packet the client request
       */
      explicit QConfPurgeParameter(lList *bundle, gdi::Packet *packet)
         : ProcedureParameter("", packet) {
         QConfPurgeParameter::set_bundle(bundle);
      }

      /** @brief Say what to purge
       * @param queue the cluster queue
       * @param attributes comma separated attribute names, wildcards allowed
       * @param domain the host or host group whose values are removed
       */
      void set_purge(const std::string &queue, const std::string &attributes, const std::string &domain) {
         queue_ = queue;
         attributes_ = attributes;
         domain_ = domain;
      }

      /// @return the cluster queue name
      [[nodiscard]] const std::string &get_queue() const { return queue_; }

      /// @return the attribute patterns, comma separated
      [[nodiscard]] const std::string &get_attributes() const { return attributes_; }

      /// @return the host or host group whose values are removed
      [[nodiscard]] const std::string &get_domain() const { return domain_; }

      lList *get_bundle() override;

      void set_bundle(const lList *bundle) override;
   };
}
