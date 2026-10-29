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
 * @brief The parameters of the access list membership procedure (CS-2959)
 *
 * Shared by both halves: the client fills them, qmaster reads them back. The
 * model and the provider belong to qmaster alone and live in
 * ocs_QConfMemberServer.h.
 *
 * `-au`/`-du` add users to or remove them from access lists, and
 * `-am`/`-ao`/`-dm`/`-do` do the same to the reserved lists `manager` and
 * `operator` (CS-2394). They used to be a read-modify-write in the client: fetch
 * each list whole, change its entries locally and send it back, which reverted
 * a change made in between and had to refuse a reduced read. As a procedure the
 * list is changed where it is stored, under the lock.
 *
 * What travels is what the user typed: the users and the lists, as the two
 * comma separated arguments, and the direction.
 */

#include <string>

#include "gdi/ocs_gdi_Packet.h"
#include "uti/ocs_ProgName.h"

#include "../ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief Which users to add to or remove from which access lists
    * @ingroup libprocedure
    */
   class QConfMemberParameter : public ProcedureParameter {
   public:
      static constexpr auto USERSET_MEMBERS = "userset_members"; ///< The sub-procedure name
      static constexpr auto MEMBER_USERS = "member_users";       ///< The users
      static constexpr auto MEMBER_LISTS = "member_lists";       ///< The access lists
      static constexpr auto MEMBER_ADD = "member_add";           ///< 1 to add, 0 to remove

   private:
      std::string users_;   ///< comma separated user (or @group) names
      std::string lists_;   ///< comma separated access list names
      bool add_ = true;     ///< add the users, or remove them

   public:
      /** @brief Build the parameters on the client side
       *
       * The procedure name has to be the program's: it is the first half of the
       * key the dispatch table is looked up by.
       */
      QConfMemberParameter() : ProcedureParameter(to_cstr(QCONF), nullptr) {
         set_sub_procedure_name(USERSET_MEMBERS);
      }

      /** @brief Rebuild the parameters inside qmaster from the request bundle
       * @param bundle the request bundle received over GDI
       * @param packet the client request
       */
      explicit QConfMemberParameter(lList *bundle, gdi::Packet *packet)
         : ProcedureParameter("", packet) {
         QConfMemberParameter::set_bundle(bundle);
      }

      /** @brief Say what to change
       * @param users comma separated user names, as typed
       * @param lists comma separated access list names, as typed
       * @param add true to add the users, false to remove them
       */
      void set_members(const std::string &users, const std::string &lists, const bool add) {
         users_ = users;
         lists_ = lists;
         add_ = add;
      }

      /// @return the users, comma separated
      [[nodiscard]] const std::string &get_users() const { return users_; }

      /// @return the access lists, comma separated
      [[nodiscard]] const std::string &get_lists() const { return lists_; }

      /// @return true when the users are added, false when they are removed
      [[nodiscard]] bool is_add() const { return add_; }

      lList *get_bundle() override;

      void set_bundle(const lList *bundle) override;
   };
}
