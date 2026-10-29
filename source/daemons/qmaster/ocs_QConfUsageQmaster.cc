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

#include "cull/cull_db.h"
#include "cull/cull_what.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
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
    * @param packet the client request
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param target the object list, `UU_LIST` or `PR_LIST`
    * @param type the master list type that goes with it
    * @param descr the CULL type of its elements
    * @param key_nm the field holding the object's name
    * @param usage_nm the usage list field
    * @param project_nm the per project usage field
    * @param answer_list receives the reason of every write that failed
    * @return true when every write succeeded
    */
   bool
   clear_usage_of(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                  const ocs::gdi::Target target, const sge_object_type type, const lDescr *descr,
                  const int key_nm, const int usage_nm, const int project_nm, lList **answer_list) {
      DENTER(TOP_LAYER);

      gdi_object_t *object = get_gdi_object(target);
      if (object == nullptr) {
         answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }

      std::vector<std::string> names;
      const lListElem *ep;
      for_each_ep(ep, *ocs::DataStore::get_master_list(type)) {
         if (lGetNumberOfElem(lGetList(ep, usage_nm)) > 0 || lGetNumberOfElem(lGetList(ep, project_nm)) > 0) {
            names.emplace_back(lGetString(ep, key_nm));
         }
      }

      bool all_ok = true;
      const int fields[] = {key_nm, usage_nm, project_nm, NoName};
      for (const std::string &name : names) {
         /* A fresh element's list fields are empty, and empty is the payload. */
         lEnumeration *what = lIntVector2What(descr, fields);
         lListElem *full = lCreateElem(descr);
         lSetString(full, key_nm, name.c_str());
         lListElem *reduced = lSelectElemPack(full, nullptr, what, false, nullptr);
         lFreeWhat(&what);
         lFreeElem(&full);

         lList *alp = nullptr;
         lList *post_list = nullptr;
         const int ret = sge_gdi_add_mod_generic(packet, task, &alp, reduced, 0, object, packet->user,
                                                 packet->host, ocs::gdi::Command::MOD_PROCEDURE,
                                                 ocs::gdi::SubCommand::NONE, &post_list, monitor);
         /* only what went wrong is passed on: a successful clear said nothing
          * when the client did it, and one line per user would bury a failure */
         if (ret != STATUS_OK) {
            all_ok = false;
            const lListElem *aep;
            for_each_ep(aep, alp) {
               if (lGetUlong(aep, AN_status) != STATUS_OK) {
                  answer_list_add(answer_list, lGetString(aep, AN_text), lGetUlong(aep, AN_status),
                                  ANSWER_QUALITY_ERROR);
               }
            }
         }
         lFreeList(&post_list);
         lFreeList(&alp);
         lFreeElem(&reduced);
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
                                 lList **answer_list) {
   DENTER(TOP_LAYER);

   const bool users_ok = clear_usage_of(packet, task, monitor, ocs::gdi::Target::UU_LIST, SGE_TYPE_USER, UU_Type,
                                        UU_name, UU_usage, UU_project, answer_list);
   const bool projects_ok = clear_usage_of(packet, task, monitor, ocs::gdi::Target::PR_LIST, SGE_TYPE_PROJECT,
                                           PR_Type, PR_name, PR_usage, PR_project, answer_list);

   DRETURN(users_ok && projects_ok);
}
