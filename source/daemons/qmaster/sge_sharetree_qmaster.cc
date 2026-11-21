/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The share tree, and carrying usage across a replacement
 */
/*
   This is the module for handling the SGE sharetree
   We save the sharetree to <spool>/qmaster/sharetree
 */

#include <cstdio>
#include <cstring>
#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "sge_persistence_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "sge_userprj_qmaster.h"

#include "msg_common.h"
#include "msg_qmaster.h"

/** @brief Install a share tree
 *
 * Add the sharetree
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the share tree; it goes into the master share tree list of the data store
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_add_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::STN> ep,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);

   const int ret = sge_mod_sharetree(packet, task, ep, alpp, ruser, rhost);
   DRETURN(ret);
}


/** @brief Replace the share tree as a whole
 *
 * We simply replace the old root element with a copy of the new one. If
 * there is no old one we simply add the new. The master share tree list of
 * the data store is changed; it is created if it does not exist yet.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the new share tree; its version and node ids are set
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_mod_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::STN> ep,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);

   int prev_version;
   int adding;
   const auto master_user_list = ocs::DataStore::master_list<SGE_TYPE_USER>();
   const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();

   /* do some checks */
   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   int ret;
   {
      ocs::tom::ListPtr<ocs::tom::STN> found;
      ret = check_sharetree(alpp, ep, master_user_list, master_project_list, nullptr, found);
   }
   if (ret) {
      /* alpp gets filled by check_sharetree() */
      DRETURN(STATUS_EUNKNOWN);
   }

   /* check for presence of sharetree list and create if neccesary */
   auto tree = ocs::DataStore::master_list_rw<SGE_TYPE_SHARETREE>();
   if (!tree) {
      prev_version = 0;
      tree = ocs::DataStore::master_list_create<SGE_TYPE_SHARETREE>("sharetree list");
      adding = 1;
   } else if (tree.empty()) {
      prev_version = 0;
      adding = 1;
   } else {
      const auto first = tree.front();
      /* real action: change user or project
         We simply replace the old element with the new one. If there is no
         old we simle add the new. */

      prev_version = first.get<STN_version>();
      tree.erase(first);
      adding = 0;
   }

   ep.set<STN_version>(prev_version + 1);

   id_sharetree(alpp, ep, 0);

   /* now insert new element */
   tree.push_back(ep.clone());

   /* write sharetree to file */
   if (!sge_event_spool(alpp, 0, sgeE_NEW_SHARETREE, 0, 0, nullptr, nullptr, nullptr, ep, nullptr, nullptr, true, true,
                        packet->gdi_session)) {

      /* answer list gets filled in sge_event_spool() */
      DRETURN(ret);
   }

   if (adding) {
      INFO(MSG_STREE_ADDSTREE_SSII, ruser, rhost, ep.count_nodes<STN_children>(), ep.count_leaves<STN_children>());
   } else {
      INFO(MSG_STREE_MODSTREE_SSII, ruser, rhost, ep.count_nodes<STN_children>(), ep.count_leaves<STN_children>());
   }

   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN(STATUS_OK);
}


/** @brief Remove the share tree
 *
 * Delete the sharetree: the master share tree list of the data store is freed,
 * its slot holds nullptr afterwards.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_del_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                  char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   if (ocs::DataStore::master_list_rw<SGE_TYPE_SHARETREE>().empty()) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_S, MSG_OBJ_SHARETREE);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   sge_event_spool(alpp, 0, sgeE_NEW_SHARETREE, 0, 0, nullptr, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);

   // take the list out of the data store and free it; the slot holds nullptr afterwards
   ocs::DataStore::replace_master_list<SGE_TYPE_SHARETREE>(ocs::tom::ListPtr<ocs::tom::STN>()).reset();

   INFO(MSG_SGETEXT_REMOVEDLIST_SSS, ruser, rhost, MSG_OBJ_SHARETREE);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN(STATUS_OK);
}

/** @brief Verify a share tree node and everything below it
 *
 * ensure
 * - all nodes have a unique path in share tree
 * - a project is not referenced more than once in share tree
 * - a user appears only once in a project sub-tree
 * - a user appears only once outside of a project sub-tree
 * - a user does not appear as a non-leaf node
 * - all leaf nodes in a project sub-tree reference a known
 * user object or the reserved name "default"
 * - there are no sub-projects within a project sub-tree
 * - all leaf nodes not in a project sub-tree reference a known
 * user or project object
 * - all user leaf nodes in a project sub-tree have access
 * to the project
 *
 * @param alpp receives messages for the caller
 * @param node the node to check; a leaf naming a project gets STN_type STT_PROJECT
 * @param user_list the known users
 * @param project_list the known projects
 * @param project the project this subtree belongs to, or an empty handle at the root
 * @param found tmp list that contains one entry for each found u/p, created when needed
 * @return 0 when the subtree is valid, -1 otherwise
 */
int
check_sharetree(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<ocs::tom::STN> node,
                const ocs::tom::ConstList<ocs::tom::UU> user_list, const ocs::tom::ConstList<ocs::tom::PR> project_list,
                ocs::tom::ConstElement<ocs::tom::PR> project, ocs::tom::ListPtr<ocs::tom::STN> &found) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::STN> save_found;
   const char *name = node.get<STN_name>();

   /* Check for dangling or circular references. */
   if (name == nullptr) {
      ERROR(MSG_STREE_NOVALIDNODEREF_U, node.get<STN_id>());
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   if (const auto children = node.get<STN_children>(); children) {

      /* not a leaf node */

      /* check if this is a project node */
      if (const auto pep = project_list.find_first<PR_name>(name); pep) {

         /* check for sub-projects (not allowed) */
         if (project) {
            ERROR(MSG_STREE_PRJINPTJSUBTREE_SS, name, project.get<PR_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }

         /* check for projects appearing more than once */
         if (found.get().find_first<STN_name>(name)) {
            ERROR(MSG_STREE_PRJTWICE_S, name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }

         /* register that this project was found */
         if (!found) {
            found = ocs::tom::ListPtr<ocs::tom::STN>::create("");
         }
         found->emplace_back<STN_name>(name);

         /* set up for project sub-tree recursion */
         project = pep;
         save_found = std::move(found);

         /* check for user appearing as non-leaf node */
      } else if (user_list.find_first<UU_name>(name)) {
         ERROR(MSG_STREE_USERNONLEAF_S, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }

      for (const auto child : children) {
         /* ensure pathes are identically inside share tree */
         for (auto remaining = child.next(); remaining; remaining = remaining.next()) {
            const char *cn = child.get<STN_name>();
            const char *rn = remaining.get<STN_name>();
            if (cn == nullptr || rn == nullptr) {
               ERROR(MSG_GDI_KEYSTR_NULL_S, "STN_name");
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               /* restore old found list */
               if (save_found) {
                  found = std::move(save_found);
               }
               DRETURN(-1);
            }
            if (!strcmp(cn, rn)) {
               ERROR(MSG_SGETEXT_FOUND_UP_TWICE_SS, cn, node.get<STN_name>());
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               /* restore old found list */
               if (save_found) {
                  found = std::move(save_found);
               }
               DRETURN(-1);
            }
         }

         if (check_sharetree(alpp, child, user_list, project_list, project, found)) {
            /* restore old found list */
            if (save_found) {
               found = std::move(save_found);
            }
            DRETURN(-1);
         }
      }

      /* restore old found list */
      if (save_found) {
         found = std::move(save_found);
      }
   } else {
      /* a leaf node */

      /* check if this is a project node */
      if (project_list.find_first<PR_name>(name)) {
         node.set<STN_type>(STT_PROJECT);
      }

      if (project) {

         /* project set means this is a project sub-tree */

         /* check for sub-projects */
         if (project_list.find_first<PR_name>(name)) {
            ERROR(MSG_STREE_PRJINPTJSUBTREE_SS, name, project.get<PR_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }

         /* leaf nodes of project sub-trees must be users */
         if (!user_list.find_first<UU_name>(name) &&
             strcmp(name, "default")) {
            ERROR(MSG_SGETEXT_UNKNOWN_SHARE_TREE_REF_TO_SS, MSG_OBJ_USER, name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }

         /* make sure this user is in the project sub-tree once */
         if (found.get().find_first<STN_name>(name)) {
            ERROR(MSG_STREE_USERTWICEINPRJSUBTREE_SS, name, project.get<PR_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }
      } else {
         const char *objname = MSG_OBJ_USER;

         /* non project sub-tree leaf nodes must be a user or a project */
         if (!user_list.find_first<UU_name>(name) &&
             strcmp(name, "default") != 0) {

            objname = MSG_JOB_PROJECT;

            if (!project_list.find_first<PR_name>(name)) {
               ERROR(MSG_SGETEXT_UNKNOWN_SHARE_TREE_REF_TO_SS, MSG_OBJ_USERPRJ, name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(-1);
            }
         }

         /* make sure this user or project is in the non-project sub-tree 
            portion of the share tree only once */
         if (found.get().find_first<STN_name>(name)) {
            ERROR(MSG_STREE_USERPRJTWICE_SS, objname, name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }

      }

      /* register that this was found */
      if (!found) {
         found = ocs::tom::ListPtr<ocs::tom::STN>::create("");
      }
      found->emplace_back<STN_name>(name);
   }

   DRETURN(0);
}

/** @brief Find the user or project node with a given name in a share tree
 *
 * @param share_tree the share tree (`STN`) to search
 * @param name the node name to look for
 * @param node_type `STT_USER` or `STT_PROJECT`; meant to restrict the match, but not evaluated
 * @param recurse meant to restrict the descent, but not evaluated: the search always descends
 * @return the node, or an empty handle if the tree is empty or holds no such node
 */
ocs::tom::ConstElement<ocs::tom::STN>
getNode(const ocs::tom::ConstList<ocs::tom::STN> share_tree, const char *name, const int node_type,
        const int recurse) {
   DENTER(TOP_LAYER);

   if (share_tree.empty()) {
      DRETURN(nullptr);
   }

   for (const auto node : share_tree) {
      if (!strcmp(name, node.get<STN_name>())) {
         DRETURN(node);
      }
      if (const auto tmp = getNode(node.get<STN_children>(), name, node_type, 1); tmp) {
         DRETURN(tmp);
      }
   }

   DRETURN(nullptr);
}
