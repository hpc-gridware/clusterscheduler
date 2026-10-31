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
 * @brief The `qconf` delete action, see the header
 */

#include <string>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "procedure/qconf/msg_qconf.h"
#include "procedure/qconf/ocs_QConfObject.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "ocs_QConfDeleteQmaster.h"
#include "sge_c_gdi.h"

namespace {
   /** @brief The object types the delete action serves, in the order a target is looked up
    *
    * The reserved lists AH, SH, MGR and OP share the target of the host groups
    * and the access lists; they come after their owners here, so a target never
    * resolves to them - and they are not deleted by name anyway.
    */
   constexpr QConfObj deletable_types[] = {
      QConfObj::CAL, QConfObj::CK, QConfObj::CE, QConfObj::EH, QConfObj::HGRP, QConfObj::PE, QConfObj::PR,
      QConfObj::CQ, QConfObj::RL, QConfObj::RQS, QConfObj::US, QConfObj::UU, QConfObj::STN, QConfObj::CONF
   };

   /** @brief The object type a delete request is addressed to
    *
    * @param target the target of the request
    * @param type receives the object type
    * @return false when no type the action serves is stored in that list
    */
   bool
   delete_type_of(const ocs::gdi::Target target, QConfObj &type) {
      for (const QConfObj o : deletable_types) {
         if (qconf_desc(o).target == target) {
            type = o;
            return true;
         }
      }
      return false;
   }

   /** @brief Split an argument at QConfActionParameter::LINE_SEPARATOR
    *
    * @param text the argument
    * @return its lines; none for an empty argument
    */
   std::vector<std::string>
   split_lines(const std::string &text) {
      std::vector<std::string> lines;
      if (text.empty()) {
         return lines;
      }
      std::string::size_type begin = 0;
      while (true) {
         const std::string::size_type end = text.find(ocs::QConfActionParameter::LINE_SEPARATOR, begin);
         lines.emplace_back(text.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
         if (end == std::string::npos) {
            break;
         }
         begin = end + 1;
      }
      return lines;
   }

   /** @brief Whether an object of a type is stored under a name
    *
    * The share tree is one object without a name; it exists when it has a root.
    *
    * @param o the object type
    * @param name the name; ignored for the share tree
    * @return true when the object exists
    */
   bool
   object_exists(const QConfObj o, const char *name) {
      const qconf_object_desc &d = qconf_desc(o);
      const lList *list = *ocs::DataStore::get_master_list(qconf_master_list_type(o));
      if (d.name_nm == NoName) {
         return lGetNumberOfElem(list) > 0;
      }
      return qconf_name_is_host(d.descr, d.name_nm) ? lGetElemHost(list, d.name_nm, name) != nullptr
                                                    : lGetElemStr(list, d.name_nm, name) != nullptr;
   }

   /** @brief Build the element a GDI delete of one named object carries
    *
    * @param d the object type
    * @param name the name
    * @return the element, owned by the caller
    */
   lListElem *
   delete_element(const qconf_object_desc &d, const char *name) {
      lListElem *ep = lCreateElem(d.descr);
      if (qconf_name_is_host(d.descr, d.name_nm)) {
         lSetHost(ep, d.name_nm, name);
      } else {
         lSetString(ep, d.name_nm, name);
      }
      return ep;
   }

   /** @brief What a missing object is called in a dry run of a name list
    *
    * @param d the object type
    * @return the noun of "denied: <noun> "x" does not exist"
    */
   const char *
   missing_noun(const qconf_object_desc &d) {
      if (d.read.not_found_noun != nullptr) {
         return d.read.not_found_noun;
      }
      return d.read.list_name != nullptr ? d.read.list_name : MSG_UNKNOWN_OBJECT;
   }
}

/** @brief Delete the named objects of the list the request is addressed to, see the header
 *
 * Until CS-2983 the client sent a GDI delete, and for "-D<obj>" it first asked,
 * name by name, whether each object existed, so that a missing one could be
 * skipped as a warning. That was a request per name, a race between the check
 * and the delete, and under RBAC an object the caller may not see would have
 * looked exactly like one that does not exist. Here both happen under the lock,
 * after qmaster has checked the permission for the list.
 *
 * Each object is deleted on its own through sge_c_gdi_del_element(), the
 * function a GDI delete uses, so the checks and messages are the same: an
 * object qmaster refuses - one that is still referenced, say - stays, the
 * others of the request are deleted, and the action fails. A request is not a
 * transaction, as the GDI delete was not.
 *
 * A dry run checks what can be checked without deleting: whether each object
 * exists, after the permission check that let the request through. Whether a
 * delete would be refused for another reason is only found out by deleting.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered; its target is the object list
 * @param monitor for monitoring qmaster threads
 * @param parameter the names, their files, and whether a missing name is skipped and whether it is a dry run
 * @param answer_list receives what each delete has to say
 * @return true when no delete failed
 */
bool
qconf_delete_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                            const ocs::QConfActionParameter &parameter, lList **answer_list) {
   DENTER(TOP_LAYER);

   QConfObj type;
   if (!delete_type_of(task->target, type)) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }
   const qconf_object_desc &d = qconf_desc(type);
   const bool dry = parameter.get_arg(ocs::QConfActionParameter::ARG_DRY) == ocs::QConfActionParameter::ARG_TRUE;
   const bool skip_missing =
      parameter.get_arg(ocs::QConfActionParameter::ARG_SKIP_MISSING) == ocs::QConfActionParameter::ARG_TRUE;

   /* the share tree is deleted as a whole, by a request that names nothing */
   if (d.name_nm == NoName) {
      if (dry) {
         if (!object_exists(type, nullptr)) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_DOESNOTEXIST_S, MSG_OBJ_SHARETREE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_DRYRUNWOULDDELETE_S, MSG_OBJ_SHARETREE);
         answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         DRETURN(true);
      }
      lList *alp = nullptr;
      sge_c_gdi_del_element(packet, task, task->target, nullptr, &alp, ocs::gdi::Command::DEL,
                            ocs::gdi::SubCommand::NONE, monitor);
      const bool ok = !answer_list_has_error(&alp);
      answer_list_append_list(answer_list, &alp);
      DRETURN(ok);
   }

   const std::vector<std::string> names = split_lines(parameter.get_arg(ocs::QConfActionParameter::ARG_NAMES));
   const std::vector<std::string> sources = split_lines(parameter.get_arg(ocs::QConfActionParameter::ARG_SOURCES));

   bool ok = true;
   for (size_t i = 0; i < names.size(); i++) {
      const char *name = names[i].c_str();
      const bool exists = object_exists(type, name);

      /* "-D<obj>" is idempotent: an object that is gone already is reported and skipped */
      if (!exists && skip_missing) {
         const char *source = i < sources.size() ? sources[i].c_str() : "";
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_DELSKIPPEDNOTEXIST_SS, name, source);
         answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_WARNING);
         continue;
      }

      if (dry) {
         if (exists) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_DRYRUNWOULDDELETE_S, name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_DOESNOTEXIST_SS, missing_noun(d), name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            ok = false;
         }
         continue;
      }

      /* a missing object of a name list is left to the delete, which words the refusal per type */
      lListElem *ep = delete_element(d, name);
      lList *alp = nullptr;
      sge_c_gdi_del_element(packet, task, task->target, ep, &alp, ocs::gdi::Command::DEL,
                            ocs::gdi::SubCommand::NONE, monitor);
      if (answer_list_has_error(&alp)) {
         ok = false;
      }
      answer_list_append_list(answer_list, &alp);
      lFreeElem(&ep);
   }

   DRETURN(ok);
}
