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
 * @brief The purging half of the qconf purge procedure, see the header
 */

#include <string>
#include <vector>

#include "cull/cull_db.h"
#include "cull/cull_what.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_str.h"
#include "uti/sge.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "procedure/qconf/msg_qconf.h"
#include "msg_common.h"
#include "ocs_QConfPurgeQmaster.h"
#include "sge_c_gdi.h"

/** @brief Remove the values of one host or host group, see the header
 *
 * The purge works on a copy of the stored queue and writes only what lost an
 * entry, as a modify of a reduced element: the queue name plus each changed
 * attribute list with SET_ALL. That is the shape "-mattr" sends, and it is what
 * keeps the purge from touching anything else of the queue.
 *
 * The host list is the exception to "the queue holds it". Since CS-2677 it is
 * the member list of the queue's own host group `@@<queue>`, and a `CQ_hostlist`
 * arriving with a queue change is an instruction to rewrite that group. So the
 * copy is given the current members only when an attribute pattern matches the
 * host list, and the reduced element carries them only when the purge actually
 * removed the host from them. A purge of any other attribute leaves the group
 * alone - which is the defect this replaced: the client sent the members it had
 * read with every purge, and the group was rewritten with that snapshot.
 *
 * Finding nothing to remove is answered with a warning and is not a failure,
 * as it was in the client.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the arguments ARG_QUEUE, ARG_ATTRIBUTES and ARG_DOMAIN
 * @param answer_list receives the outcome
 * @return true when the purge did not fail
 */
bool
qconf_purge_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                           const ocs::QConfActionParameter &parameter, lList **answer_list) {
   DENTER(TOP_LAYER);

   const std::string queue_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_QUEUE);
   const std::string attributes_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_ATTRIBUTES);
   const std::string domain_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_DOMAIN);

   /* the client never sends an incomplete request; this guards the action
    * against a caller that is not qconf */
   if (queue_arg.empty() || attributes_arg.empty() || domain_arg.empty()) {
      answer_list_add(answer_list, MSG_QCONF_PURGEINCOMPLETE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }
   const char *queue_name = queue_arg.c_str();
   const char *attributes = attributes_arg.c_str();
   const char *domain = domain_arg.c_str();

   const lListElem *stored = cqueue_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE), queue_name);
   if (stored == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR, MSG_CQUEUE_DOESNOTEXIST_S,
                              queue_name);
      DRETURN(false);
   }

   gdi_object_t *object = get_gdi_object(ocs::gdi::Target::CQ_LIST);
   if (object == nullptr) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // the patterns, separated as the client always separated them
   lList *attr_list = nullptr;
   lString2List(attributes, &attr_list, US_Type, US_name, ",");

   lListElem *copy = lCopyElem(stored);

   /* The current members of @@<queue>, for a pattern that matches the host list -
    * see the function comment. */
   bool hostlist_wanted = false;
   for_each_ep_lv(attr, attr_list) {
      if (sge_eval_expression(ocs::CEntry::Type::HOST, lGetString(attr, US_name), SGE_ATTR_HOSTLIST, nullptr) == 0) {
         hostlist_wanted = true;
         break;
      }
   }
   uint32_t hostlist_before = 0;
   if (hostlist_wanted) {
      dstring group_name = DSTRING_INIT;
      const lListElem *group = hgroup_list_locate(*ocs::DataStore::get_master_list(SGE_TYPE_HGROUP),
                                                  hgroup_queue_group_name(queue_name, &group_name));
      sge_dstring_free(&group_name);
      if (group != nullptr) {
         lSetList(copy, CQ_hostlist, lCopyList("", lGetList(group, HGRP_host_list)));
      }
      hostlist_before = lGetNumberOfElem(lGetList(copy, CQ_hostlist));
   }

   if (!cqueue_purge_host(copy, answer_list, attr_list, domain)) {
      answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_WARNING, MSG_QCONF_ATTR_ARGS_NOT_FOUND,
                              attributes, domain);
      lFreeElem(&copy);
      lFreeList(&attr_list);
      DRETURN(true);
   }
   lFreeList(&attr_list);

   /* What lost an entry: the queue name to address it, every attribute list that
    * is shorter than the stored one, and the host list when it is. */
   std::vector<int> fields{CQ_name};
   for (int i = 0; cqueue_attribute_array[i].name != nullptr; i++) {
      const int nm = cqueue_attribute_array[i].cqueue_attr;
      if (lGetNumberOfElem(lGetList(copy, nm)) != lGetNumberOfElem(lGetList(stored, nm))) {
         fields.push_back(nm);
      }
   }
   if (hostlist_wanted && lGetNumberOfElem(lGetList(copy, CQ_hostlist)) != hostlist_before) {
      fields.push_back(CQ_hostlist);
   }
   fields.push_back(NoName);

   lEnumeration *what = lIntVector2What(CQ_Type, fields.data());
   lListElem *reduced = lSelectElemPack(copy, nullptr, what, false, nullptr);
   lFreeWhat(&what);
   lFreeElem(&copy);

   lList *post_list = nullptr;
   const int ret = sge_gdi_add_mod_generic(packet, task, answer_list, reduced, 0, object, packet->user,
                                           packet->host, ocs::gdi::Command::MOD_PROCEDURE,
                                           ocs::gdi::SubCommand::SET_ALL, &post_list, monitor);
   lFreeList(&post_list);
   lFreeElem(&reduced);

   DRETURN(ret == STATUS_OK);
}
