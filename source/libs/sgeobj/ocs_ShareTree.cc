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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "sgeobj/sge_userset.h"

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
 * @param node The starting node of the share tree.
 * @param func The function to apply to each node.
 * @param ptr Additional pointer argument for the function.
 * @return A positive value if the function returns a positive value, otherwise 0.
 */
int
ocs::ShareTree::foreach_call_func(lListElem *node, sge_node_func_t func, void *ptr) {
   if (node == nullptr) {
      return 0;
   }

   int ret = (*func)(node, ptr);
   if (ret > 0) {
      return ret;
   }

   if (const lList *children = lGetPosList(node, STN_children_POS); children != nullptr) {
      lListElem *child_node = nullptr;
      for_each_rw(child_node, children) {
         ret = foreach_call_func(child_node, func, ptr);
         if (ret > 0) {
            return ret;
            break;
         }
      }
   }

   return ret;
}

/** @brief Zero out the share tree node fields
 *
 * This function sets specific fields in the given node to zero. It is
 * used to initialize the share tree nodes before they are displayed.
 *
 * @param node The node to be zeroed out.
 * @param ptr Unused pointer argument.
 * @return Always returns 0.
 */
int
ocs::ShareTree::zero_node_fields(lListElem *node, [[maybe_unused]] void *ptr) {
   lSetPosDouble(node, STN_m_share_POS, 0);
   lSetPosDouble(node, STN_adjusted_current_proportion_POS, 0);
   lSetPosUlong(node, STN_job_ref_count_POS, 0);
   return 0;
}

/** @brief Initialize the share tree node fields
 *
 * This function initializes specific fields in the given root node and
 * its descendants to zero. It is used to prepare the share tree for
 * display in qmon.
 *
 * @param node The root node of the share tree.
 * @return Always returns 0.
 */
int
ocs::ShareTree::zero_fields(lListElem *node) {
   return foreach_call_func(node, zero_node_fields, nullptr);
}

/** @brief Calculate the usage for a node and its descendants
 *
 * This function calculates the usage for the given node and its
 * descendants in the share tree. It retrieves usage information from
 * user and project lists, applies decay if necessary, and sums up the
 * usage values.
 *
 * @param node The node to calculate usage for.
 * @param user_list The list of users.
 * @param project_list The list of projects.
 * @param decay_list The list of decay factors.
 * @param now The current time.
 * @param project_name The name of the project to filter by (optional).
 * @param seqno The sequence number for usage updates.
 * @return The calculated usage value for the node.
 */
double
ocs::ShareTree::calc_node_usage(lListElem *node, const lList *user_list, const lList *project_list,
                                const lList *decay_list, const u_long64 now, const char *project_name, u_long seqno) {
   DENTER(TOP_LAYER);

   bool is_user = false;
   bool is_project = false;
   const lList *children = lGetPosList(node, STN_children_POS);
   const lList *usage_list=nullptr;
   lListElem *userprj = nullptr;
   if (!children) {
      if (project_name) {
         // Get usage from project usage sub-list in user object
         if ((userprj = user_list_locate(user_list, lGetPosString(node, STN_name_POS)))) {
            const lList *projects = lGetList(userprj, UU_project);
            const lListElem *upp;

            is_user = true;
            if (projects) {
               if ((upp=lGetElemStr(projects, UPP_name, project_name))) {
                  usage_list = lGetList(upp, UPP_usage);
               }
            }   
         }
      } else {
         // Get usage directly from corresponding user or project object
         if ((userprj = user_list_locate(user_list, lGetPosString(node, STN_name_POS)))) {
            is_user = true;
            usage_list = lGetList(userprj, UU_usage);
         } else if ((userprj = prj_list_locate(project_list, lGetPosString(node, STN_name_POS)))) {
            is_user = false;
            usage_list = lGetList(userprj, PR_usage);
         }
      }
   } else {
      // If this is a project node, then return the project usage rather than the children's usage
      if (!project_name) {
         if ((userprj = prj_list_locate(project_list, lGetPosString(node, STN_name_POS)))) {
            is_project = true;
            is_user = false;
            usage_list = lGetList(userprj, PR_usage);
            project_name = lGetString(userprj, PR_name);
         }
      }
   }

   double usage_value = 0;
   if (usage_list) {
      const lListElem *usage_weight, *usage_elem;
      double sum_of_usage_weights = 0;
      lList *usage_weight_list = nullptr;

      // Decay usage
      if (now && userprj) {
         UserProject::decay_userprj_usage(userprj, is_user, decay_list, seqno, now);
      }  

      // Sum usage weighting factors
      if (sconf_is()) {
         usage_weight_list = sconf_get_usage_weight_list();
         if (usage_weight_list) {
            for_each_ep(usage_weight, usage_weight_list) {
               sum_of_usage_weights += lGetPosDouble(usage_weight, UA_value_POS);
            }
         }
      }

      // Combine user/project usage based on usage weighting factors
      if (usage_weight_list) {
         for_each_ep(usage_elem, usage_list) {
            const char *usage_name = lGetPosString(usage_elem, UA_name_POS);
            usage_weight = lGetElemStr(usage_weight_list, UA_name, usage_name);
            if (usage_weight && sum_of_usage_weights>0) {
               usage_value += lGetPosDouble(usage_elem, UA_value_POS) * (lGetPosDouble(usage_weight, UA_value_POS) / sum_of_usage_weights);
            }
         }
      }

      lFreeList(&usage_weight_list);

      // Store other usage values in node usage list
      for_each_ep(usage_elem, usage_list) {
         const char *nm = lGetPosString(usage_elem, UA_name_POS);
         if (strcmp(nm, USAGE_ATTR_CPU) != 0 && strcmp(nm, USAGE_ATTR_MEM) != 0 && strcmp(nm, USAGE_ATTR_IO) != 0) {
            if (lListElem *u = lGetElemStrRW(lGetPosList(node, STN_usage_list_POS), UA_name, nm);
                u != nullptr || (u = lAddSubStr(node, UA_name, nm, STN_usage_list, UA_Type)) != nullptr) {
               lSetPosDouble(u, UA_value_POS, lGetPosDouble(u, UA_value_POS) + lGetPosDouble(usage_elem, UA_value_POS));
            }
         }
      }
   }

   if (children) {
      double child_usage = 0;
      lListElem *child_node;

      // Sum child usage
      for_each_rw(child_node, children) {
         child_usage += calc_node_usage(child_node, user_list, project_list, decay_list, now, project_name, seqno);

         // Sum other usage values
         if (!is_project) {
            lListElem *nu;

            for_each_rw(nu, lGetPosList(child_node, STN_usage_list_POS)) {
               const char *nm = lGetPosString(nu, UA_name_POS);
               if (lListElem *u = lGetElemStrRW(lGetPosList(node, STN_usage_list_POS), UA_name, nm);
                   u != nullptr || (u = lAddSubStr(node, UA_name, nm, STN_usage_list, UA_Type)) !=  nullptr) {
                  lSetPosDouble(u, UA_value_POS, lGetPosDouble(u, UA_value_POS) + lGetPosDouble(nu, UA_value_POS));
               }
            }
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
                  const double u = lGetPosDouble(ancestors.nodes[i], STN_combined_usage_POS);
                  lSetPosDouble(ancestors.nodes[i], STN_combined_usage_POS, u + default_usage);
               }
            }
            free_ancestors(&ancestors);
         }
      }
   }

   // Set combined usage in the node
   lSetPosDouble(node, STN_combined_usage_POS, usage_value);

   DRETURN(usage_value);
}

/** @brief Calculate the node proportions
 *
 * This function calculates the node proportions for the given node
 * and its descendants based on the total usage. It sets the STN_actual_proportion
 * field in each node.
 *
 * @param node The node to calculate proportions for.
 * @param total_usage The total usage value to use for calculations.
 */
void
ocs::ShareTree::calc_node_proportion(lListElem *node, const double total_usage) {

   // Calculate node proportions for all children
   if (const lList *children = nullptr; (children = lGetPosList(node, STN_children_POS))) {
      lListElem *child_node = nullptr;
      for_each_rw(child_node, children) {
         calc_node_proportion(child_node, total_usage);
      }
   }  

   // Set proportion in the node
   lSetPosDouble(node, STN_actual_proportion_POS, total_usage == 0 ? 0 : lGetPosDouble(node, STN_combined_usage_POS) / total_usage);
}

/** @brief Calculate the share tree proportions
 *
 * This function calculates the share tree proportions for the given share tree
 * calculates the total usage (STN_combined_usage) and then sets the
 * proportions (STN_actual_proportion) for each node in the share tree.
 *
 * @param share_tree The share tree to calculate proportions for.
 * @param user_list The list of users to consider.
 * @param project_list The list of projects to consider.
 * @param decay_list The list of decay values to apply.
 * @param curr_time The current time for decay calculations.
 */
void
ocs::ShareTree::calc_proportions(const lList *share_tree, const lList *user_list, const lList *project_list, const lList *decay_list, u_long64 curr_time) {
   DENTER(TOP_LAYER);

   lListElem *root;
   if (!share_tree || (root = lFirstRW(share_tree)) == nullptr) {
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
 * @param project_list The list of projects to check against.
 * @param node The node to set the project flag for.
 */
void
ocs::ShareTree::set_node_project_flag(lListElem *node, const lList *project_list) {
   if (!project_list || !node) {
      return;
   }

   if (prj_list_locate(project_list, lGetString(node, STN_name))) {
      lSetUlong(node, STN_project, 1);
   } else {
      lSetUlong(node, STN_project, 0);
   }
   if (const lList *children = lGetList(node, STN_children)) {
      lListElem *child;
      for_each_rw(child, children) {
         set_node_project_flag(child, project_list);
      }
   }
}

/** @brief Adds a tmp-user node for a user if a default node exists
 *
 * @param node The root of the share tree to search.
 * @param user_name The name of the user to add.
 * @param proj_name The name of the project to check against.
 */
void
ocs::ShareTree::add_auto_user(lListElem *node, const char *user_name, const char *proj_name) {
   if (lListElem *default_node = search_user_project_node(node, user_name, proj_name, nullptr, node);
       default_node != nullptr && !strcmp(DEFAULT_NODE_NAME, lGetString(default_node, STN_name))) {

      // Create a new node for the user
      lListElem *tmp_node = lCopyElem(default_node);
      lSetString(tmp_node, STN_name, user_name);
      lSetList(tmp_node, STN_children, nullptr);
      lSetUlong(tmp_node, STN_temp, 1);

      // Add the new node as a child of the parent
      lList *children = lGetListRW(default_node,STN_children);
      if (children == nullptr) {
         children = lCreateList("display", STN_Type);
         lSetList(default_node, STN_children, children);
      }
      lAppendElem(children, tmp_node);
   }
}

/** @brief Adds a tmp-user node for each user in the user list
 *
 * This function iterates over the user list and adds a temporary node
 * for each user if a default node exists in the share tree.
 *
 * @param node The root of the share tree to search.
 * @param user_list The list of users to add.
 * @param project_list The list of projects to check against.
 * @param acl_list The list of user sets to check against.
 */
void
ocs::ShareTree::add_all_auto_users(lListElem *node, const lList *user_list, const lList *project_list, const lList *acl_list) {
   set_node_project_flag(node, project_list);

   const lListElem *user, *project;
   for_each_ep(project, project_list) {
      // check projects acl/xacl if the users are allowed
      if (const char *proj_name = lGetString(project, PR_name);
          search_user_project_node(node, DEFAULT_NODE_NAME, proj_name, nullptr, node)) {
         const lList *xacl = lGetList(project, PR_xacl);
         const lList *acl = lGetList(project, PR_acl);

         for_each_ep(user, user_list) {
            if (const char *user_name = lGetString(user, UU_name);
                sge_has_access_(user_name, nullptr, nullptr, acl, xacl, acl_list)) {

               add_auto_user(node, user_name, proj_name);
            }
         }
      }
   }

   if (search_user_project_node(node, DEFAULT_NODE_NAME, nullptr, nullptr, node)) {
      for_each_ep(user, user_list) {
         const char *user_name = lGetString(user, UU_name);

         add_auto_user(node, user_name, nullptr);
      }
   }

}

/** @brief Search for a user/project node in the share tree
 *
 * This function searches the share tree for a node corresponding to
 * the specified user or project combination. If found, it returns the
 * node and optionally sets the parent node.
 *
 * @param node The root of the share tree to search.
 * @param username The username to search for.
 * @param project_name The project name to search for.
 * @param parent Pointer to store the parent of the found node.
 * @param root The root of the share tree.
 * @return Pointer to the found node or nullptr if not found.
 */
lListElem *
ocs::ShareTree::search_user_project_node(lListElem *node, const char *username, const char *project_name,
                                         lListElem **parent, const lListElem *root) {
   if (node == nullptr || (username == nullptr && project_name == nullptr)) {
      return nullptr;
   }

   // skip project nodes which don't match
   const char *node_name = lGetPosString(node, STN_name_POS);
   if (lGetPosUlong(node, STN_project_POS) && node != root && (!project_name || strcmp(node_name, project_name))) {
      return nullptr;
   }

   // if project name is supplied, look for the project and a user node below that
   // otherwise look for the user node
   const lList *children = lGetPosList(node, STN_children_POS);
   if (project_name != nullptr) {
      if (strcmp(node_name, project_name) == 0) {
         if (children == nullptr) {
            return node;
         }
         return search_user_project_node(node, username, nullptr, parent, node);
      }
   } else {
      if (strcmp(node_name, username) == 0) {
         return node;
      }
   }

   // nothing found so far - do the same with all children
   lListElem *child;
   for_each_rw(child, children) {
      if (lListElem *fep = search_user_project_node(child, username, project_name, parent, root); fep != nullptr) {
         if (parent && child == fep) {
            *parent = node;
         }
         return fep;
      }
   }

   // still nothing found - repeat the same search with the default username
   if (project_name == nullptr) {
      if (node == root && strcmp(username, DEFAULT_NODE_NAME)) {
         return search_user_project_node(node, DEFAULT_NODE_NAME, nullptr, parent, node);
      }
   }

   return nullptr;
}
