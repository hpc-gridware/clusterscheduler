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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The share tree that decides fair-share entitlements
 */

#include <cstring>
#include <utility>

#include "sgeobj/sge_userset.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#ifndef NO_SGE_COMPILE_DEBUG   
#   define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_ShareTree.h"
#include "ocs_Usage.h"
#include "ocs_UserProject.h"
#include "sge_job.h"
#include "sge_schedd_conf.h"
#include "sge_sharetree.h"
#include "sge_usage.h"
#include "sge_userprj.h"

/** @brief Iterate over each node in the share tree and apply a function
 *
 * This function recursively traverses the share tree starting from the
 * given node and applies the specified function to each node. If the
 * function returns a positive value, the iteration stops and that value
 * is returned.
 *
 * @param node The starting node of the share tree, may be an empty handle.
 * @param func The function to apply to each node.
 * @param ptr Additional pointer argument for the function.
 * @return A positive value if the function returns a positive value, otherwise 0.
 */
int
ocs::ShareTree::foreach_call_func(const ocs::tom::Element<ocs::tom::STN> node, const sge_node_func_t func, void *ptr) {
   DENTER(BASIS_LAYER);

   if (!node) {
      DRETURN(0);
   }

   int ret = (*func)(node, ptr);
   if (ret > 0) {
      DRETURN(ret);
   }

   for (const auto child_node : node.get<STN_children>()) {
      ret = foreach_call_func(child_node, func, ptr);
      if (ret > 0) {
         DRETURN(ret);
      }
   }

   DRETURN(ret);
}

/** @brief Zero out the share tree node fields
 *
 * This function sets specific fields in the given node to zero. It is
 * used to initialize the share tree nodes before they are displayed.
 *
 * The signature is the one of #sge_node_func_t, so the node stays a raw
 * pointer.
 *
 * @param node_raw The node to be zeroed out (STN).
 * @param ptr Unused pointer argument.
 * @return Always returns 0.
 */
int
ocs::ShareTree::zero_node_fields(lListElem *node_raw, [[maybe_unused]] void *ptr) {
   DENTER(BASIS_LAYER);

   // the callback is only applied to share tree nodes; called for every node, so no check
   const auto node = ocs::tom::Element<ocs::tom::STN>::from_typed(node_raw);
   node.set<STN_m_share>(0);
   node.set<STN_adjusted_current_proportion>(0);
   node.set<STN_job_ref_count>(0);

   DRETURN(0);
}

/** @brief Add the usage of a list to the usage list of a share tree node
 *
 * The attributes the node does not have yet are added.
 *
 * @param node the node, not empty
 * @param usage_list the usage to add (UA), may be an empty handle
 * @param skip_default_attrs whether cpu, mem and io are left out
 */
static void
ocs_sharetree_node_add_usage(const ocs::tom::Element<ocs::tom::STN> node,
                             const ocs::tom::ConstList<ocs::tom::UA> usage_list, const bool skip_default_attrs) {
   DENTER(BASIS_LAYER);

   for (const auto usage_elem : usage_list) {
      const char *nm = usage_elem.get<UA_name>();
      if (skip_default_attrs &&
          (strcmp(nm, USAGE_ATTR_CPU) == 0 || strcmp(nm, USAGE_ATTR_MEM) == 0 || strcmp(nm, USAGE_ATTR_IO) == 0)) {
         continue;
      }
      auto u = node.get<STN_usage_list>().find_first<UA_name>(nm);
      if (!u) {
         u = node.emplace_back_in<STN_usage_list, UA_name>(nm);
      }
      if (u) {
         u.set<UA_value>(u.get<UA_value>() + usage_elem.get<UA_value>());
      }
   }

   DRETURN_VOID;
}

/** @brief Calculate the usage for a node and its descendants
 *
 * This function calculates the usage for the given node and its
 * descendants in the share tree. It retrieves usage information from
 * user and project lists, applies decay if necessary, and sums up the
 * usage values.
 *
 * @param node The node to calculate usage for, not empty.
 * @param user_list The list of users (UU), may be an empty handle.
 * @param project_list The list of projects (PR), may be an empty handle.
 * @param decay_list The list of decay factors (UA), may be an empty handle.
 * @param now The current time.
 * @param project_name The name of the project to filter by (optional).
 * @param seqno The sequence number for usage updates.
 * @return The calculated usage value for the node.
 */
double
ocs::ShareTree::calc_node_usage(const ocs::tom::Element<ocs::tom::STN> node,
                                const ocs::tom::ConstList<ocs::tom::UU> user_list,
                                const ocs::tom::ConstList<ocs::tom::PR> project_list,
                                const ocs::tom::ConstList<ocs::tom::UA> decay_list, const uint64_t now,
                                const char *project_name, const u_long seqno) {
   DENTER(TOP_LAYER);

   bool is_project = false;
   const auto children = node.get<STN_children>();
   const char *node_name = node.get<STN_name>();
   ocs::tom::ConstList<ocs::tom::UA> usage_list;
   // the node's object: a user or a project, at most one of both
   ocs::tom::Element<ocs::tom::UU> user;
   ocs::tom::Element<ocs::tom::PR> project;
   if (!children) {
      if (project_name) {
         // Get usage from project usage sub-list in user object
         if ((user = user_list_locate(user_list, node_name))) {
            if (const auto upp = user.get<UU_project>().find_first<UPP_name>(project_name); upp) {
               usage_list = upp.get<UPP_usage>();
            }
         }
      } else {
         // Get usage directly from corresponding user or project object
         if ((user = user_list_locate(user_list, node_name))) {
            usage_list = user.get<UU_usage>();
         } else if ((project = prj_list_locate(project_list, node_name))) {
            usage_list = project.get<PR_usage>();
         }
      }
   } else {
      // If this is a project node, then return the project usage rather than the children's usage
      if (!project_name) {
         if ((project = prj_list_locate(project_list, node_name))) {
            is_project = true;
            usage_list = project.get<PR_usage>();
            project_name = project.get<PR_name>();
         }
      }
   }

   double usage_value = 0;
   if (usage_list) {
      double sum_of_usage_weights = 0;
      ocs::tom::ListPtr<ocs::tom::UA> usage_weight_list;

      // Decay usage
      if (now) {
         if (user) {
            UserProject::decay_userprj_usage(user, decay_list, seqno, now);
         } else if (project) {
            UserProject::decay_userprj_usage(project, decay_list, seqno, now);
         }
      }

      // Sum usage weighting factors
      if (sconf_is()) {
         usage_weight_list = sconf_get_usage_weight_list();
         for (const auto usage_weight : usage_weight_list.get()) {
            sum_of_usage_weights += usage_weight.get<UA_value>();
         }
      }

      // Combine user/project usage based on usage weighting factors
      if (usage_weight_list) {
         for (const auto usage_elem : usage_list) {
            const char *usage_name = usage_elem.get<UA_name>();
            const auto usage_weight = usage_weight_list.get().find_first<UA_name>(usage_name);
            if (usage_weight && sum_of_usage_weights > 0) {
               usage_value += usage_elem.get<UA_value>() *
                              (usage_weight.get<UA_value>() / sum_of_usage_weights);
            }
         }
      }

      usage_weight_list.reset();

      // Store other usage values in node usage list
      ocs_sharetree_node_add_usage(node, usage_list, true);
   }

   if (children) {
      double child_usage = 0;

      // Sum child usage
      for (const auto child_node : children) {
         child_usage += calc_node_usage(child_node, user_list, project_list, decay_list, now, project_name, seqno);

         // Sum other usage values
         if (!is_project) {
            ocs_sharetree_node_add_usage(node, child_node.get<STN_usage_list>(), false);
         }
      }

      if (!is_project) {
         /* if this is not a project node, we include the child usage */
         usage_value += child_usage;
      } else {

         // If this is a project node, then we calculate the usage being used by all users which map
         // to the "default" user node by subtracting the sum of all the child usage from the project
         // usage. Then, we add this usage to all the nodes leading to the "default" user node.
         ancestors_t ancestors;
         if (search_ancestors(node, DEFAULT_NODE_NAME, &ancestors, 1)) {
            if (const double default_usage = usage_value - child_usage; default_usage > 1.0) {
               for(int i = 1; i < ancestors.depth; i++) {
                  const double u = ancestors.nodes[i].get<STN_combined_usage>();
                  ancestors.nodes[i].set<STN_combined_usage>(u + default_usage);
               }
            }
            free_ancestors(&ancestors);
         }
      }
   }

   // Set combined usage in the node
   node.set<STN_combined_usage>(usage_value);

   DRETURN(usage_value);
}

/** @brief Calculate the node proportions
 *
 * This function calculates the node proportions for the given node
 * and its descendants based on the total usage. It sets the STN_actual_proportion
 * field in each node.
 *
 * @param node The node to calculate proportions for, not empty.
 * @param total_usage The total usage value to use for calculations.
 */
void
ocs::ShareTree::calc_node_proportion(const ocs::tom::Element<ocs::tom::STN> node, const double total_usage) {
   DENTER(BASIS_LAYER);

   // Calculate node proportions for all children
   for (const auto child_node : node.get<STN_children>()) {
      calc_node_proportion(child_node, total_usage);
   }

   // Set proportion in the node
   node.set<STN_actual_proportion>(total_usage == 0 ? 0 :
                                              node.get<STN_combined_usage>() / total_usage);

   DRETURN_VOID;
}

/** @brief Calculate the share tree proportions
 *
 * This function calculates the share tree proportions for the given share tree
 * calculates the total usage (STN_combined_usage) and then sets the
 * proportions (STN_actual_proportion) for each node in the share tree.
 *
 * @param share_tree The share tree to calculate proportions for (STN), may be an empty handle.
 * @param user_list The list of users to consider (UU).
 * @param project_list The list of projects to consider (PR).
 * @param decay_list The list of decay values to apply (UA), may be an empty handle.
 * @param curr_time The current time for decay calculations.
 */
void
ocs::ShareTree::calc_proportions(const ocs::tom::List<ocs::tom::STN> share_tree,
                                 const ocs::tom::ConstList<ocs::tom::UU> user_list,
                                 const ocs::tom::ConstList<ocs::tom::PR> project_list,
                                 const ocs::tom::ConstList<ocs::tom::UA> decay_list, const uint64_t curr_time) {
   DENTER(TOP_LAYER);

   const auto root = share_tree.front();
   if (!root) {
      DRETURN_VOID;
   }

   Usage::calculate_default_decay_constant(sconf_get_halftime());

   const double total_usage = calc_node_usage(root, user_list, project_list, decay_list, curr_time, nullptr, -1);

   calc_node_proportion(root, total_usage);

   DRETURN_VOID;
}

/** @brief Set the project flag for the share tree nodes
 *
 * This function sets the project flag for the given node and its
 * descendants based on whether they are part of the specified project
 * list.
 *
 * @param node The node to set the project flag for, may be an empty handle.
 * @param project_list The list of projects to check against (PR), may be an empty handle.
 */
void
ocs::ShareTree::set_node_project_flag(const ocs::tom::Element<ocs::tom::STN> node,
                                      const ocs::tom::ConstList<ocs::tom::PR> project_list) {
   DENTER(BASIS_LAYER);

   if (!project_list || !node) {
      DRETURN_VOID;
   }

   if (prj_list_locate(project_list, node.get<STN_name>())) {
      node.set<STN_project>(1);
   } else {
      node.set<STN_project>(0);
   }
   for (const auto child : node.get<STN_children>()) {
      set_node_project_flag(child, project_list);
   }

   DRETURN_VOID;
}

/** @brief Adds a tmp-user node for a user if a default node exists
 *
 * @param node The root of the share tree to search.
 * @param user_name The name of the user to add.
 * @param proj_name The name of the project to check against.
 */
void
ocs::ShareTree::add_auto_user(const ocs::tom::Element<ocs::tom::STN> node, const char *user_name,
                              const char *proj_name) {
   DENTER(BASIS_LAYER);

   if (const auto default_node = search_user_project_node(node, user_name, proj_name, node).first;
       default_node && !strcmp(DEFAULT_NODE_NAME, default_node.get<STN_name>())) {

      // Create a new node for the user
      auto tmp_node = default_node.clone();
      tmp_node->set<STN_name>(user_name);
      tmp_node->set<STN_children>(nullptr);
      tmp_node->set<STN_temp>(1);

      // Add the new node as a child of the parent
      ocs::tom::get_or_create<STN_children>(default_node, "display").push_back(std::move(tmp_node));
   }

   DRETURN_VOID;
}

/** @brief Adds a tmp-user node for each user in the user list
 *
 * This function iterates over the user list and adds a temporary node
 * for each user if a default node exists in the share tree.
 *
 * @param node The root of the share tree to search.
 * @param user_list The list of users to add (UU).
 * @param project_list The list of projects to check against (PR).
 * @param acl_list The list of user sets to check against (US).
 */
void
ocs::ShareTree::add_all_auto_users(const ocs::tom::Element<ocs::tom::STN> node,
                                   const ocs::tom::ConstList<ocs::tom::UU> user_list,
                                   const ocs::tom::ConstList<ocs::tom::PR> project_list,
                                   const ocs::tom::ConstList<ocs::tom::US> acl_list) {
   DENTER(BASIS_LAYER);

   set_node_project_flag(node, project_list);

   for (const auto project : project_list) {
      // check projects acl/xacl if the users are allowed
      if (const char *proj_name = project.get<PR_name>();
          search_user_project_node(node, DEFAULT_NODE_NAME, proj_name, node).first) {
         const auto xacl = project.get<PR_xacl>();
         const auto acl = project.get<PR_acl>();

         for (const auto user : user_list) {
            if (const char *user_name = user.get<UU_name>();
                sge_has_access_(user_name, nullptr, nullptr, acl, xacl, acl_list)) {

               add_auto_user(node, user_name, proj_name);
            }
         }
      }
   }

   if (search_user_project_node(node, DEFAULT_NODE_NAME, nullptr, node).first) {
      for (const auto user : user_list) {
         const char *user_name = user.get<UU_name>();

         add_auto_user(node, user_name, nullptr);
      }
   }

   DRETURN_VOID;
}

/** @brief Search for a user/project node in the share tree
 *
 * This function searches the share tree for a node corresponding to
 * the specified user or project combination. If found, it returns the
 * node and its parent node.
 *
 * @param node The root of the share tree to search, may be an empty handle.
 * @param username The username to search for.
 * @param project_name The project name to search for.
 * @param root The root of the share tree.
 * @return the found node, an empty handle if not found; and the parent of the found node, an empty handle where
 *         the search did not record it (the node is the root, or was found through the project node)
 */
std::pair<ocs::tom::Element<ocs::tom::STN>, ocs::tom::Element<ocs::tom::STN>>
ocs::ShareTree::search_user_project_node(const ocs::tom::Element<ocs::tom::STN> node, const char *username,
                                         const char *project_name, const ocs::tom::ConstElement<ocs::tom::STN> root) {
   DENTER(BASIS_LAYER);

   using Found = std::pair<ocs::tom::Element<ocs::tom::STN>, ocs::tom::Element<ocs::tom::STN>>;

   if (!node || (username == nullptr && project_name == nullptr)) {
      DRETURN(Found());
   }

   // skip project nodes which don't match
   const char *node_name = node.get<STN_name>();
   if (node.get<STN_project>() && node != root && (!project_name || strcmp(node_name, project_name))) {
      DRETURN(Found());
   }

   // if project name is supplied, look for the project and a user node below that
   // otherwise look for the user node
   const auto children = node.get<STN_children>();
   if (project_name != nullptr) {
      if (strcmp(node_name, project_name) == 0) {
         if (!children) {
            DRETURN(Found(node, nullptr));
         }
         DRETURN(search_user_project_node(node, username, nullptr, node));
      }
   } else {
      if (strcmp(node_name, username) == 0) {
         DRETURN(Found(node, nullptr));
      }
   }

   // nothing found so far - do the same with all children
   for (const auto child : children) {
      if (auto found = search_user_project_node(child, username, project_name, root); found.first) {
         if (child == found.first) {
            found.second = node;
         }
         DRETURN(found);
      }
   }

   // still nothing found - repeat the same search with the default username
   if (project_name == nullptr) {
      if (node == root && strcmp(username, DEFAULT_NODE_NAME)) {
         DRETURN(search_user_project_node(node, DEFAULT_NODE_NAME, nullptr, node));
      }
   }

   DRETURN(Found());
}
