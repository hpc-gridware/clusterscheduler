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
 * @brief The parameters of a qconf action procedure (CS-2962)
 *
 * An action is a qconf switch that changes objects in qmaster without handing
 * object texts over, as the write procedure does: `-purge` removes host specific
 * values from a queue (CS-2948), `-au`/`-du` and their reserved-list siblings
 * change the entries of access lists (CS-2959). Each used to be a client-side
 * read-modify-write; as an action the change runs where the objects are stored,
 * under the lock.
 *
 * What an action needs is what the user typed, so the parameters are the name
 * of the action - the sub-procedure name the dispatch table is keyed by - and a
 * set of named string arguments. Shared by both halves: the client fills them,
 * qmaster reads them back. The model and the providers live in
 * ocs_QConfActionServer.h.
 */

#include <string>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"
#include "uti/ocs_ProgName.h"

#include "../ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief The name of an action and its arguments
    * @ingroup libprocedure
    */
   class QConfActionParameter : public ProcedureParameter {
   public:
      // the actions
      static constexpr auto QUEUE_PURGE = "queue_purge";         ///< `-purge` (CS-2948)
      static constexpr auto USERSET_MEMBERS = "userset_members"; ///< `-au`/`-du`/`-am`/`-ao`/`-dm`/`-do` (CS-2959)
      static constexpr auto USAGE_CLEAR = "usage_clear";         ///< `-clearusage` (CS-2963), takes no arguments
      static constexpr auto SHARETREE_NODES = "sharetree_nodes"; ///< `-astnode`/`-mstnode`/`-dstnode` (CS-2966)

      // the arguments of QUEUE_PURGE
      static constexpr auto ARG_QUEUE = "queue";           ///< the cluster queue
      static constexpr auto ARG_ATTRIBUTES = "attributes"; ///< comma separated attribute names, wildcards allowed
      static constexpr auto ARG_DOMAIN = "domain";         ///< the host or host group whose values are removed

      // the arguments of USERSET_MEMBERS
      static constexpr auto ARG_USERS = "users";           ///< comma separated user names
      static constexpr auto ARG_LISTS = "lists";           ///< comma separated access list names
      static constexpr auto ARG_ADD = "add";               ///< ARG_TRUE to add the users, anything else removes them

      // the arguments of SHARETREE_NODES
      static constexpr auto ARG_NODES = "nodes";           ///< the node list as typed, "path=shares,..." or "path,..."
      static constexpr auto ARG_OPERATION = "operation";   ///< OP_ADD, OP_MODIFY or OP_DELETE
      static constexpr auto OP_ADD = "add";                ///< `-astnode`: set the shares, add a missing node
      static constexpr auto OP_MODIFY = "modify";          ///< `-mstnode`: set the shares of existing nodes
      static constexpr auto OP_DELETE = "delete";          ///< `-dstnode`: delete leaf nodes

      static constexpr auto ARG_TRUE = "1";                ///< the value of a set flag argument

   private:
      static constexpr auto ACTION_ARGS = "action_args";   ///< the bundle entry holding the arguments
      lList *args_ = nullptr;                               ///< VA_Type: VA_variable the name, VA_value the value

   public:
      /** @brief Build the parameters of one action on the client side
       *
       * The procedure name has to be the program's: it is the first half of the
       * key the dispatch table is looked up by. The action is the second half.
       *
       * @param action the action, one of the constants above
       */
      explicit QConfActionParameter(const char *action) : ProcedureParameter(to_cstr(QCONF), nullptr) {
         set_sub_procedure_name(action);
      }

      /** @brief Rebuild the parameters inside qmaster from the request bundle
       * @param bundle the request bundle received over GDI
       * @param packet the client request
       */
      explicit QConfActionParameter(lList *bundle, gdi::Packet *packet)
         : ProcedureParameter("", packet) {
         QConfActionParameter::set_bundle(bundle);
      }

      ~QConfActionParameter() override {
         lFreeList(&args_);
      }

      QConfActionParameter(const QConfActionParameter &) = delete;
      QConfActionParameter &operator=(const QConfActionParameter &) = delete;

      /** @brief Set one argument, replacing a value it already has
       * @param name the argument, one of the ARG_ constants
       * @param value its value, as the user typed it
       */
      void set_arg(const char *name, const std::string &value);

      /** @brief Read one argument
       * @param name the argument, one of the ARG_ constants
       * @return its value; empty when it was not set, or set to an empty string
       */
      [[nodiscard]] std::string get_arg(const char *name) const;

      lList *get_bundle() override;

      void set_bundle(const lList *bundle) override;
   };
}
