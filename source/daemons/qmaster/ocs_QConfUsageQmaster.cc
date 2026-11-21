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
 * @brief The `qconf -clearusage` action, see the header
 */

#include <string>
#include <vector>

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "tom/ocs_Element.h"
#include "uti/sge_rmon_macros.h"

#include "msg_common.h"
#include "ocs_QConfUsageQmaster.h"
#include "sge_c_gdi.h"

namespace {
   /** @brief Clear the usage of every object of one list that has some
    *
    * One write per object that carries usage, as a reduced element - the name
    * and the two emptied lists - so userprj_mod() changes nothing else of it.
    * An object without usage is not written: clearing it would change nothing
    * but its version, its spool file and the event stream.
    *
    * The names are collected before the first write, because a write replaces
    * the element in the very list this walks.
    *
    * @tparam V the master list, SGE_TYPE_USER or SGE_TYPE_PROJECT; its object type is UU or PR
    * @tparam KeyF the field holding the object's name
    * @tparam UsageF the usage list field
    * @tparam ProjectF the per project usage field
    * @param packet the client request
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param target the object list, `UU_LIST` or `PR_LIST`, the one that goes with @p V
    * @param answer_list receives the reason of every write that failed
    * @return true when every write succeeded
    */
   template <sge_object_type V, auto KeyF, auto UsageF, auto ProjectF>
   bool
   clear_usage_of(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                  const ocs::gdi::Target target, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
      DENTER(TOP_LAYER);
      using T = ocs::DataStore::master_type_t<V>;

      gdi_object_t *object = get_gdi_object(target);
      if (object == nullptr) {
         answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }

      std::vector<std::string> names;
      for (const auto ep : ocs::DataStore::master_list<V>()) {
         if (!ep.template get<UsageF>().empty() || !ep.template get<ProjectF>().empty()) {
            names.emplace_back(ep.template get<KeyF>());
         }
      }

      /* the name and the two lists, as lSelectElemPack() reduced the element before */
      const auto reduced_layout = ocs::tom::layout<KeyF, UsageF, ProjectF>();
      bool all_ok = true;
      for (const std::string &name : names) {
         /* A fresh element's list fields are empty, and empty is the payload. */
         auto reduced = ocs::tom::ElementPtr<T>::create(reduced_layout);
         reduced->template set<KeyF>(name.c_str());

         ocs::tom::ListPtr<ocs::tom::AN> alp;
         // the on_success hook of users and projects leaves nothing for after the request
         const int ret = ocs_gdi_add_mod<T>(packet, task, &alp, reduced.get(), 0, object, packet->user,
                                            packet->host, ocs::gdi::Command::MOD_PROCEDURE,
                                            ocs::gdi::SubCommand::NONE, nullptr, monitor);
         /* only what went wrong is passed on: a successful clear said nothing
          * when the client did it, and one line per user would bury a failure */
         if (ret != STATUS_OK) {
            all_ok = false;
            for (const auto aep : *alp) {
               if (aep.get<AN_status>() != STATUS_OK) {
                  answer_list_add(answer_list, aep.get<AN_text>(), aep.get<AN_status>(),
                                  ANSWER_QUALITY_ERROR);
               }
            }
         }
         alp.reset();
      }

      DRETURN(all_ok);
   }
}

/** @brief Clear the share tree usage of every user and every project, see the header
 *
 * Until CS-2963 the client read the names of all users and all projects and sent
 * one modify per list, with every name and the two emptied fields. That was no
 * longer a read-modify-write of whole objects - CS-2825 had cut the read to the
 * names - but it still raced: an object created between the read and the write
 * kept its usage, and one deleted in between failed the write. Under the lock
 * the set of objects is the set that is stored.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the action; it takes no arguments
 * @param answer_list receives the reason of every write that failed
 * @return true when the usage of every user and project was cleared
 */
bool
qconf_usage_clear_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                                 [[maybe_unused]] const ocs::QConfActionParameter &parameter,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   const bool users_ok = clear_usage_of<SGE_TYPE_USER, UU_name, UU_usage, UU_project>(
           packet, task, monitor, ocs::gdi::Target::UU_LIST, answer_list);
   const bool projects_ok = clear_usage_of<SGE_TYPE_PROJECT, PR_name, PR_usage, PR_project>(
           packet, task, monitor, ocs::gdi::Target::PR_LIST, answer_list);

   DRETURN(users_ok && projects_ok);
}
