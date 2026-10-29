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
 * @brief The share tree node action, see the header
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "cull/cull_db.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_sharetree.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "procedure/qconf/msg_qconf.h"
#include "msg_common.h"
#include "ocs_QConfShareTreeQmaster.h"
#include "sge_sharetree_qmaster.h"

namespace {
   /** @brief The lines the client prints, in order, until it is known whether they are errors
    *
    * A path that cannot be found used to be printed and to fail the command only
    * when nothing else was changed. Whether something was is known after the
    * last node, so the lines are collected and their status decided then.
    */
   struct NodeReport {
      std::vector<std::pair<std::string, bool>> lines; ///< the text, and whether it reports a problem

      /// @param text a line that reports a change
      void info(const char *text) { lines.emplace_back(text, false); }

      /// @param text a line that reports a node that could not be changed
      void problem(const char *text) { lines.emplace_back(text, true); }

      /** @brief Hand the lines over as answers
       * @param answer_list where they go
       * @param failed true when the command failed, which makes every problem an error
       */
      void flush(lList **answer_list, const bool failed) const {
         for (const auto &[text, is_problem] : lines) {
            if (!is_problem) {
               answer_list_add(answer_list, text.c_str(), STATUS_OK, ANSWER_QUALITY_INFO);
            } else if (failed) {
               answer_list_add(answer_list, text.c_str(), STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            } else {
               answer_list_add(answer_list, text.c_str(), STATUS_OK, ANSWER_QUALITY_WARNING);
            }
         }
      }
   };

   /** @brief The path of a node as the ancestors spell it, "/a/b"
    * @param ancestors the result of search_named_node_path()
    * @param buffer receives the path
    * @return the path, owned by @p buffer
    */
   const char *
   ancestors_path(const ancestors_t &ancestors, dstring *buffer) {
      sge_dstring_clear(buffer);
      for (int i = 0; i < ancestors.depth; i++) {
         sge_dstring_sprintf_append(buffer, "/%s", lGetString(ancestors.nodes[i], STN_name));
      }
      return sge_dstring_get_string(buffer);
   }

   /** @brief Read one "path=shares" argument
    * @param arg the argument
    * @param path receives the path
    * @param shares receives the shares
    * @return true when the argument has that form
    */
   bool
   parse_node_shares(const char *arg, std::string &path, int &shares) {
      const char *eq = (arg != nullptr) ? strchr(arg, '=') : nullptr;
      if (eq == nullptr || eq == arg) {
         return false;
      }
      path.assign(arg, eq - arg);
      return sscanf(eq + 1, "%d", &shares) == 1;
   }

   /** @brief Set the shares of the named nodes, adding missing ones for "-astnode"
    *
    * The loop of the old client path, on the copy of the stored tree.
    *
    * @param root the copy of the tree
    * @param nodes the arguments (`STN_Type`, the text in `STN_name`)
    * @param add true for "-astnode": a missing node is added below its parent
    * @param typed the node list as typed, for the "not a valid argument" message
    * @param report receives the lines to print
    * @param refused set when the request has to be refused as a whole
    * @return how many nodes were changed
    */
   int
   set_node_shares(lListElem *root, const lList *nodes, const bool add, const char *typed, NodeReport &report,
                   bool &refused) {
      DENTER(TOP_LAYER);

      int modified = 0;
      dstring buffer = DSTRING_INIT;
      const lListElem *arg;
      for_each_ep(arg, nodes) {
         std::string path;
         int shares = 0;
         if (!parse_node_shares(lGetString(arg, STN_name), path, shares)) {
            report.problem(sge_dstring_sprintf(&buffer, MSG_ANSWER_XISNOTVALIDSEENODESHARESLIST_S, typed));
            continue;
         }

         ancestors_t ancestors;
         memset(&ancestors, 0, sizeof(ancestors));
         lListElem *node = search_named_node_path(root, path.c_str(), &ancestors);
         if (node == nullptr && add) {
            // the basename of the path, after the last '/' or '.'
            const std::string::size_type last = path.find_last_of("/.");
            if (last != std::string::npos && last + 1 < path.size()) {
               lListElem *parent = nullptr;
               if (last == 0 && path[0] == '/') {
                  parent = root;
               } else {
                  ancestors_t parent_ancestors;
                  memset(&parent_ancestors, 0, sizeof(parent_ancestors));
                  parent = search_named_node_path(root, path.substr(0, last).c_str(), &parent_ancestors);
                  free_ancestors(&parent_ancestors);
               }
               if (parent != nullptr) {
                  lList *children = lGetListRW(parent, STN_children);
                  lListElem *added = lAddElemStr(&children, STN_name, path.substr(last + 1).c_str(), STN_Type);
                  if (added != nullptr && lGetList(parent, STN_children) == nullptr) {
                     lSetList(parent, STN_children, children);
                  }
                  free_ancestors(&ancestors);
                  memset(&ancestors, 0, sizeof(ancestors));
                  node = search_named_node_path(root, path.c_str(), &ancestors);
                  if (node != added) {
                     report.problem(sge_dstring_sprintf(&buffer, MSG_TREE_CANTADDNODEXISNONUNIQUE_S, path.c_str()));
                     free_ancestors(&ancestors);
                     refused = true;
                     break;
                  }
               }
            }
         }

         if (node != nullptr) {
            modified++;
            lSetUlong(node, STN_shares, shares);
            report.info(MSG_TREE_SETTING);
            std::string line = ancestors_path(ancestors, &buffer);
            line += "=" + std::to_string(shares);
            report.info(line.c_str());
         } else {
            report.problem(sge_dstring_sprintf(&buffer, MSG_TREE_UNABLETOLACATEXINSHARETREE_S, path.c_str()));
         }
         free_ancestors(&ancestors);
      }
      sge_dstring_free(&buffer);

      DRETURN(modified);
   }

   /** @brief Delete the named leaf nodes
    *
    * The loop of the old client path, on the copy of the stored tree: the root
    * and a node with children are not deleted.
    *
    * @param root the copy of the tree
    * @param nodes the paths (`STN_Type`, the text in `STN_name`)
    * @param report receives the lines to print
    * @return how many nodes were deleted
    */
   int
   delete_nodes(lListElem *root, const lList *nodes, NodeReport &report) {
      DENTER(TOP_LAYER);

      int modified = 0;
      dstring buffer = DSTRING_INIT;
      const lListElem *arg;
      for_each_ep(arg, nodes) {
         const char *path = lGetString(arg, STN_name);
         if (path == nullptr) {
            continue;
         }
         ancestors_t ancestors;
         memset(&ancestors, 0, sizeof(ancestors));
         lListElem *node = search_named_node_path(root, path, &ancestors);
         if (node == nullptr) {
            report.problem(sge_dstring_sprintf(&buffer, MSG_TREE_UNABLETOLACATEXINSHARETREE_S, path));
         } else if (lGetList(node, STN_children) != nullptr) {
            report.problem(MSG_TREE_CANTDELNODESWITHCHILD);
         } else if (ancestors.depth <= 0) {
            report.problem(MSG_TREE_CANTDELROOTNODE);
         } else {
            modified++;
            lListElem *parent = (ancestors.depth == 1) ? root : ancestors.nodes[ancestors.depth - 2];
            report.info(MSG_TREE_REMOVING);
            report.info(ancestors_path(ancestors, &buffer));
            lList *siblings = lGetListRW(parent, STN_children);
            lRemoveElem(siblings, &node);
            if (lGetNumberOfElem(siblings) == 0) {
               lSetList(parent, STN_children, nullptr);
            }
         }
         free_ancestors(&ancestors);
      }
      sge_dstring_free(&buffer);

      DRETURN(modified);
   }
}

/** @brief Add, modify or delete share tree nodes, see the header
 *
 * What the client did with the tree it had fetched - the same path search, the
 * same rules, the same lines - done to a copy of the stored tree under the lock
 * and written through sge_mod_sharetree(), as "-Mstree" writes. A change made
 * by someone else between a read and a write can no longer be reverted, and
 * there is no client read RBAC could reduce.
 *
 * Two things fail now that used to exit 0: a write that qmaster refused, and
 * "-mstnode" or "-dstnode" on a cluster without a tree. A negative share value
 * refuses the whole request before anything is changed.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the arguments ARG_NODES and ARG_OPERATION
 * @param answer_list receives one answer per line the client prints
 * @return true when at least one node was changed and the tree was written
 */
bool
qconf_sharetree_nodes_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                     [[maybe_unused]] monitoring_t *monitor,
                                     const ocs::QConfActionParameter &parameter, lList **answer_list) {
   DENTER(TOP_LAYER);

   const std::string nodes_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_NODES);
   const std::string operation = parameter.get_arg(ocs::QConfActionParameter::ARG_OPERATION);
   const bool add = (operation == ocs::QConfActionParameter::OP_ADD);
   const bool del = (operation == ocs::QConfActionParameter::OP_DELETE);

   /* the client never sends an incomplete request; this guards the action
    * against a caller that is not qconf */
   if (nodes_arg.empty() || (!add && !del && operation != ocs::QConfActionParameter::OP_MODIFY)) {
      answer_list_add(answer_list, MSG_QCONF_STNODEINCOMPLETE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lList *nodes = nullptr;
   lString2List(nodes_arg.c_str(), &nodes, STN_Type, STN_name, ", ");

   /* a negative share value refuses the request before anything is changed */
   if (!del) {
      const lListElem *arg;
      for_each_ep(arg, nodes) {
         std::string path;
         int shares = 0;
         if (parse_node_shares(lGetString(arg, STN_name), path, shares) && shares < 0) {
            answer_list_add(answer_list, MSG_QCONF_POSITIVE_SHARE_VALUE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            lFreeList(&nodes);
            DRETURN(false);
         }
      }
   }

   lList **tree_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE);
   const lListElem *stored = lFirst(*tree_list);
   lListElem *root = nullptr;
   if (stored != nullptr) {
      root = lCopyElem(stored);
   } else if (add) {
      /* "-astnode" on a cluster without a tree starts one, as it did */
      root = lCreateElem(STN_Type);
      lSetString(root, STN_name, "Root");
      lSetUlong(root, STN_shares, 1);
   } else {
      answer_list_add(answer_list, MSG_TREE_NOSHARETREE, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      lFreeList(&nodes);
      DRETURN(false);
   }

   NodeReport report;
   bool refused = false;
   const int modified = del ? delete_nodes(root, nodes, report)
                            : set_node_shares(root, nodes, add, nodes_arg.c_str(), report, refused);
   lFreeList(&nodes);

   bool ok = false;
   if (refused) {
      report.flush(answer_list, true);
   } else if (modified == 0) {
      report.flush(answer_list, true);
      answer_list_add(answer_list, MSG_TREE_NOMIDIFIEDSHARETREE, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
   } else {
      report.flush(answer_list, false);
      /* the reason of a refused write is what the client printed; the generic
       * confirmation is replaced by its own line, as before */
      lList *alp = nullptr;
      const int ret = sge_mod_sharetree(packet, task, root, tree_list, &alp, packet->user, packet->host);
      if (ret == STATUS_OK) {
         answer_list_add(answer_list, MSG_TREE_MODIFIEDSHARETREE, STATUS_OK, ANSWER_QUALITY_INFO);
         ok = true;
      } else {
         const lListElem *aep;
         bool reported = false;
         for_each_ep(aep, alp) {
            if (lGetUlong(aep, AN_status) != STATUS_OK) {
               answer_list_add(answer_list, lGetString(aep, AN_text), lGetUlong(aep, AN_status),
                               ANSWER_QUALITY_ERROR);
               reported = true;
               break;
            }
         }
         if (!reported) {
            answer_list_add(answer_list, MSG_TREE_NOMIDIFIEDSHARETREE, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         }
      }
      lFreeList(&alp);
   }
   lFreeElem(&root);

   DRETURN(ok);
}
