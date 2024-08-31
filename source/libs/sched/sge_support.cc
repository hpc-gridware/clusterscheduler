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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <math.h>

#ifndef NO_SGE_COMPILE_DEBUG   
#   define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"

#include "sgeee.h"
#include "sge_support.h"
#include "valid_queue_user.h"
#include "sgeobj/cull/sge_eejob_SGEJ_L.h"

/* a boolean for the sort order */
enum {
  SGEJ_sort_decending = 0,
  SGEJ_sort_ascending
};

static const long sge_usage_interval = SGE_USAGE_INTERVAL;

/*--------------------------------------------------------------------
 * decay_usage - decay usage for the passed usage list
 *--------------------------------------------------------------------*/

// interval in seconds
static void decay_usage(lList *usage_list, const lList *decay_list, double interval)
{
   lListElem *usage = nullptr;

   if (usage_list) {
      double decay = 0;
      double default_decay = pow(sconf_get_decay_constant(), interval / (double)sge_usage_interval);

      for_each_rw (usage, usage_list) {
         const lListElem *decay_elem;
         if (decay_list && ((decay_elem = lGetElemStr(decay_list, UA_name, lGetPosString(usage, UA_name_POS))))) {

            decay = pow(lGetPosDouble(decay_elem, UA_value_POS), interval / (double)sge_usage_interval);
         } else {
            decay = default_decay;
         }
         lSetPosDouble(usage, UA_value_POS, lGetPosDouble(usage, UA_value_POS) * decay);
      }
   }
   return;
}

/*--------------------------------------------------------------------
 * decay_userprj_usage - decay usage for the passed user/project object
 *--------------------------------------------------------------------*/

void
decay_userprj_usage( lListElem *userprj,
                     bool is_user,
                     const lList *decay_list,
                     u_long seqno,
                     u_long64 curr_time)
{
   u_long64 usage_time_stamp;
   int obj_usage_seqno_POS;
   int obj_usage_time_stamp_POS;
   int obj_usage_POS;
   int obj_project_POS;

   if (is_user) {
      obj_usage_seqno_POS = UU_usage_seqno_POS;
      obj_usage_time_stamp_POS = UU_usage_time_stamp_POS;
      obj_usage_POS = UU_usage_POS;
      obj_project_POS = UU_project_POS;
   } else {
      obj_usage_seqno_POS = PR_usage_seqno_POS;
      obj_usage_time_stamp_POS = PR_usage_time_stamp_POS;
      obj_usage_POS = PR_usage_POS;
      obj_project_POS = PR_project_POS;
   }

   if (userprj && seqno != lGetPosUlong(userprj, obj_usage_seqno_POS)) {

   /*-------------------------------------------------------------
    * Note: In order to decay usage once per decay interval, we
    * keep a time stamp in the user/project of when it was last
    * decayed and then apply the approriate decay based on the time
    * stamp. This allows the usage to be decayed on the scheduling
    * interval, even though the decay interval is different than
    * the scheduling interval.
    *-------------------------------------------------------------*/

      usage_time_stamp = lGetPosUlong64(userprj, obj_usage_time_stamp_POS);

      if (usage_time_stamp > 0 && (curr_time > usage_time_stamp)) {
         const lListElem *upp;
         double interval = sge_gmt64_to_gmt32_double(curr_time - usage_time_stamp);

         decay_usage(lGetPosList(userprj, obj_usage_POS), decay_list, interval);

         for_each_ep(upp, lGetPosList(userprj, obj_project_POS)) {
            decay_usage(lGetPosList(upp, UPP_usage_POS), decay_list, interval);
         }
      }

      lSetPosUlong64(userprj, obj_usage_time_stamp_POS, curr_time);
      if (seqno != (u_long) -1) {
      	lSetPosUlong(userprj, obj_usage_seqno_POS, seqno);
      }

   }

   return;
}


/*--------------------------------------------------------------------
 * calculate_decay_constant - calculates decay rate and constant based
 * on the decay half life and usage interval. The halftime argument
 * is in minutes.
 *--------------------------------------------------------------------*/

void
calculate_decay_constant( double halftime,
                          double *decay_rate,
                          double *decay_constant )
{
   if (halftime < 0) {
      *decay_rate = 1.0;
      *decay_constant = 0;
   } else if (halftime == 0) {
      *decay_rate = 0;
      *decay_constant = 1.0;
   } else {
      *decay_rate = - log(0.5) / (halftime * 60);
      *decay_constant = 1 - (*decay_rate * sge_usage_interval);
   }
   return;
}


/*--------------------------------------------------------------------
 * calculate_default_decay_constant - calculates the default decay
 * rate and constant based on the decay half life and usage interval.
 * The halftime argument is in hours.
 *--------------------------------------------------------------------*/

void
calculate_default_decay_constant( int halftime )
{
   double sge_decay_rate = 0.0;
   double sge_decay_constant = 0.0;
   
   calculate_decay_constant(halftime*60.0, &sge_decay_rate, &sge_decay_constant); 
   
   sconf_set_decay_constant(sge_decay_constant);
}


/*--------------------------------------------------------------------
 * sge_for_each_node - visit each node and call the supplied function
 * until a non-zero return code is returned.
 *--------------------------------------------------------------------*/

int
sge_for_each_share_tree_node( lListElem *node,
                              sge_node_func_t func,
                              void *ptr )
{
   int retcode=0;
   lList *children = nullptr;
   lListElem *child_node = nullptr;

   if (node == nullptr) {
      return 0;
   }

   if ((retcode = (*func)(node, ptr))) {
      return retcode;
   }

   if ((children = lGetPosList(node, STN_children_POS))) {
      for_each_rw(child_node, children) {
         if ((retcode = sge_for_each_share_tree_node(child_node, func, ptr))) {
            break;
         }
      }
   }

   return retcode;
}


/*--------------------------------------------------------------------
 * zero_node_fields - zero out the share tree node fields that are 
 * passed to the qmaster from schedd and are displayed at qmon
 *--------------------------------------------------------------------*/

int
sge_zero_node_fields( lListElem *node,
                      void *ptr )
{
   lSetPosDouble(node, STN_m_share_POS, 0);
   lSetPosDouble(node, STN_adjusted_current_proportion_POS, 0);
   lSetPosUlong(node, STN_job_ref_count_POS, 0);

   return 0;
}


/*--------------------------------------------------------------------
 * sge_init_node_fields - zero out the share tree node fields that are 
 * passed to the qmaster from schedd and are displayed at qmon
 *
 * Always returns 0.
 *--------------------------------------------------------------------*/

int
sge_init_node_fields( lListElem *root )
{
   return sge_for_each_share_tree_node(root, sge_zero_node_fields, nullptr);
}


/*--------------------------------------------------------------------
 * sge_calc_node_usage - calculate usage for this share tree node
 * and all descendant nodes.
 *--------------------------------------------------------------------*/

double
sge_calc_node_usage( lListElem *node,
                     const lList *user_list,
                     const lList *project_list,
                     const lList *decay_list,
                     u_long64 curr_time,
                     const char *projname,
                     u_long seqno )
{
   double usage_value = 0;
   int project_node = 0;
   lListElem *child_node;
   lList *children;
   lListElem *userprj = nullptr;
   const lList *usage_list=nullptr;
   const lListElem *usage_weight, *usage_elem;
   double sum_of_usage_weights = 0;
   const char *usage_name;
   bool is_user = false;

   DENTER(TOP_LAYER);

   children = lGetPosList(node, STN_children_POS);
   if (!children) {
      if (projname) {

         /*-------------------------------------------------------------
          * Get usage from project usage sub-list in user object
          *-------------------------------------------------------------*/


         if ((userprj = user_list_locate(user_list, lGetPosString(node, STN_name_POS)))) {
            const lList *projects = lGetList(userprj, UU_project);
            const lListElem *upp;

            is_user = true;
            if (projects) {
               if ((upp=lGetElemStr(projects, UPP_name, projname))) {
                  usage_list = lGetList(upp, UPP_usage);
               }
            }   
         }

      } else {

         /*-------------------------------------------------------------
          * Get usage directly from corresponding user or project object
          *-------------------------------------------------------------*/

         if ((userprj = user_list_locate(user_list, lGetPosString(node, STN_name_POS)))) {
            is_user = true;
            usage_list = lGetList(userprj, UU_usage);
         } else if ((userprj = prj_list_locate(project_list, lGetPosString(node, STN_name_POS)))) {
            is_user = false;
            usage_list = lGetList(userprj, PR_usage);
         }
      }
   } else {

      /*-------------------------------------------------------------
       * If this is a project node, then return the project usage
       * rather than the children's usage
       *-------------------------------------------------------------*/
      if (!projname) {
         if ((userprj = prj_list_locate(project_list, 
                                            lGetPosString(node, STN_name_POS)))) {
            project_node = 1;
            is_user = false;
            usage_list = lGetList(userprj, PR_usage);
            projname = lGetString(userprj, PR_name);
         }
      }

   }

   if (usage_list) {
      lList *usage_weight_list = nullptr;

      /*-------------------------------------------------------------
       * Decay usage
       *-------------------------------------------------------------*/

      if (curr_time && userprj) {
         decay_userprj_usage(userprj, is_user, decay_list, seqno, curr_time);
      }  

      /*-------------------------------------------------------------
       * Sum usage weighting factors
       *-------------------------------------------------------------*/

      if (sconf_is()) {
         usage_weight_list = sconf_get_usage_weight_list();
         if (usage_weight_list) {
            for_each_ep(usage_weight, usage_weight_list)
               sum_of_usage_weights +=
                     lGetPosDouble(usage_weight, UA_value_POS);
         }
      }

      /*-------------------------------------------------------------
       * Combine user/project usage based on usage weighting factors
       *-------------------------------------------------------------*/

      if (usage_weight_list) {
         for_each_ep(usage_elem, usage_list) {
            usage_name = lGetPosString(usage_elem, UA_name_POS);
            usage_weight = lGetElemStr(usage_weight_list, UA_name, usage_name);
            if (usage_weight && sum_of_usage_weights>0) {
               usage_value += lGetPosDouble(usage_elem, UA_value_POS) * (lGetPosDouble(usage_weight, UA_value_POS) / sum_of_usage_weights);
            }
         }
      }

      lFreeList(&usage_weight_list);

      /*-------------------------------------------------------------
       * Store other usage values in node usage list
       *-------------------------------------------------------------*/

      for_each_ep(usage_elem, usage_list) {
         const char *nm = lGetPosString(usage_elem, UA_name_POS);
         lListElem *u;
         if (strcmp(nm, USAGE_ATTR_CPU) != 0 &&
             strcmp(nm, USAGE_ATTR_MEM) != 0 &&
             strcmp(nm, USAGE_ATTR_IO) != 0) {
            if (((u=lGetElemStrRW(lGetPosList(node, STN_usage_list_POS), UA_name, nm))) ||
                ((u = lAddSubStr(node, UA_name, nm, STN_usage_list, UA_Type))))
               lSetPosDouble(u, UA_value_POS, lGetPosDouble(u, UA_value_POS) + lGetPosDouble(usage_elem, UA_value_POS));
         }
      }
   }

   if (children) {
      double child_usage = 0;

      /*-------------------------------------------------------------
       * Sum child usage
       *-------------------------------------------------------------*/

      for_each_rw(child_node, children) {
         lListElem *nu;
         child_usage += sge_calc_node_usage(child_node, user_list,
                                            project_list, decay_list, curr_time,
                                            projname, seqno);

         /*-------------------------------------------------------------
          * Sum other usage values
          *-------------------------------------------------------------*/

         if (!project_node)
            for_each_rw(nu, lGetPosList(child_node, STN_usage_list_POS)) {
               const char *nm = lGetPosString(nu, UA_name_POS);
               lListElem *u;
               if (((u=lGetElemStrRW(lGetPosList(node, STN_usage_list_POS),
                                   UA_name, nm))) ||
                   ((u=lAddSubStr(node, UA_name, nm, STN_usage_list, UA_Type))))
                  lSetPosDouble(u, UA_value_POS,
                                lGetPosDouble(u, UA_value_POS) +
                                lGetPosDouble(nu, UA_value_POS));
            }
      }

      if (!project_node)

         /* if this is not a project node, we include the child usage */

         usage_value += child_usage;

      else {

         /* If this is a project node, then we calculate the usage
            being used by all users which map to the "default" user node
            by subtracting the sum of all the child usage from the
            project usage. Then, we add this usage to all of the nodes
            leading to the "default" user node. */

         ancestors_t ancestors;
         int i;
         if (search_ancestors(node, "default", &ancestors, 1)) {
            double default_usage = usage_value - child_usage;
            if (default_usage > 1.0) {
               for(i=1; i<ancestors.depth; i++) {
                  double u = lGetPosDouble(ancestors.nodes[i], STN_combined_usage_POS);
                  lSetPosDouble(ancestors.nodes[i], STN_combined_usage_POS, u + default_usage);
               }
            }
            free_ancestors(&ancestors);
         }
      }

#ifdef notdef
      else {
         lListElem *default_node;
         if ((default_node=search_named_node(node, "default")))
            lSetPosDouble(default_node, STN_combined_usage_POS,
               MAX(usage_value - child_usage, 0));
      }
#endif

   }

   /*-------------------------------------------------------------
    * Set combined usage in the node
    *-------------------------------------------------------------*/

   lSetPosDouble(node, STN_combined_usage_POS, usage_value);

   DRETURN(usage_value);
}


/*--------------------------------------------------------------------
 * sge_calc_node_proportions - calculate share tree node proportions
 * for this node and all descendant nodes.
 *--------------------------------------------------------------------*/

void
sge_calc_node_proportion( lListElem *node,
                          double total_usage )
{
   lList *children = nullptr;
   lListElem *child_node = nullptr;

   /*-------------------------------------------------------------
    * Calculate node proportions for all children
    *-------------------------------------------------------------*/

   if ((children = lGetPosList(node, STN_children_POS))) {
      for_each_rw(child_node, children) {
         sge_calc_node_proportion(child_node, total_usage);
      }
   }  

   /*-------------------------------------------------------------
    * Set proportion in the node
    *-------------------------------------------------------------*/

   if (total_usage == 0) {
      lSetPosDouble(node, STN_actual_proportion_POS, 0);
   }   
   else {
      lSetPosDouble(node, STN_actual_proportion_POS,
                    lGetPosDouble(node, STN_combined_usage_POS) / total_usage);
   }      

   return;
}


/*--------------------------------------------------------------------
 * sge_calc_share_tree_proportions - calculate share tree node
 * usage and proportions.
 *
 * Sets STN_combined_usage and STN_actual_proportion in each share
 * tree node contained in the passed-in share_tree argument.
 *--------------------------------------------------------------------*/

void
_sge_calc_share_tree_proportions(const lList *share_tree,
                                 const lList *user_list,
                                 const lList *project_list,
                                 const lList *decay_list,
                                 u_long64 curr_time )
{
   lListElem *root;
   double total_usage;

   DENTER(TOP_LAYER);

   if (!share_tree || !((root=lFirstRW(share_tree)))) {
      DRETURN_VOID;
   }

   calculate_default_decay_constant( sconf_get_halftime());

   total_usage = sge_calc_node_usage(root,
                                     user_list,
                                     project_list,
                                     decay_list,
                                     curr_time,
                    				       nullptr,
                                     -1);

   sge_calc_node_proportion(root, total_usage);

   DRETURN_VOID;
}


void
sge_calc_share_tree_proportions( lList *share_tree,
                                 const lList *user_list,
                                 const lList *project_list,
                                 const lList *decay_list )
{
   _sge_calc_share_tree_proportions(share_tree, user_list, project_list,
                                    decay_list, sge_get_gmt64());
   return;
}


/*--------------------------------------------------------------------
 * set_share_tree_project_flags - set the share tree project flag for
 *       node and descendants
 *--------------------------------------------------------------------*/

void
set_share_tree_project_flags( const lList *project_list,
                              lListElem *node )
{
   const lList *children;
   lListElem *child;

   if (!project_list || !node)
      return;

   if (prj_list_locate(project_list, lGetString(node, STN_name)))
      lSetUlong(node, STN_project, 1);
   else
      lSetUlong(node, STN_project, 0);

   children = lGetList(node, STN_children);
   if (children) {
      for_each_rw(child, children) {
         set_share_tree_project_flags(project_list, child);
      }
   }
   return;
}


void
sge_add_default_user_nodes( lListElem *root_node,
                            const lList *user_list,
                            const lList *project_list,
                            const lList *userset_list)
{
   const lListElem *user, *project;
   lListElem *pnode, *dnode;
   const char *proj_name, *user_name;

   /*
    * do for each project and for no project
    *    if default node exists
    *       do for each user
    *          if user maps to default node
    *             add temp node as sibling to default node
    *          endif
    *       end do
    *    endif
    * end do
    */

   set_share_tree_project_flags(project_list, root_node);

   for_each_ep(project, project_list) {
      /*
      ** check acl and xacl of project for the temp users
      ** only users that are allowed for the project are shown
      */
      const lList *xacl = lGetList(project, PR_xacl);
      const lList *acl = lGetList(project, PR_acl);

      proj_name = lGetString(project, PR_name);

      if (search_userprj_node(root_node, "default", proj_name, nullptr)) {
         for_each_ep(user, user_list) {
            int has_access = 1;
            
            user_name = lGetString(user, UU_name);

            /*
            ** check if user would be allowed
            */
            has_access = sge_has_access_(user_name, nullptr, nullptr, acl, xacl, userset_list);

            if (has_access && 
                ((dnode=search_userprj_node(root_node, user_name, 
                                            proj_name, &pnode))) &&
                !strcmp("default", lGetString(dnode, STN_name))) {

               lListElem *node = lCopyElem(dnode);
               lSetString(node, STN_name, user_name);
               lSetList(node, STN_children, nullptr);
               lSetUlong(node, STN_temp, 1);
               if (lGetList(dnode,STN_children) == nullptr) {
                  lList *children = lCreateList("display", STN_Type);
                  lSetList(dnode, STN_children, children);
               }   
               lAppendElem(lGetListRW(dnode,STN_children), node);
            }
         }
      }
   }

   proj_name = nullptr;
   if (search_userprj_node(root_node, "default", proj_name, nullptr)) {
      for_each_ep(user, user_list) {
         user_name = lGetString(user, UU_name);
         if (((dnode=search_userprj_node(root_node, user_name, proj_name, &pnode))) &&
             strcmp("default", lGetString(dnode, STN_name)) == 0) {
            lListElem *node = lCopyElem(dnode);
            lSetString(node, STN_name, user_name);
            lSetList(node, STN_children, nullptr);
            lSetUlong(node, STN_temp, 1);
            if (lGetList(dnode,STN_children) == nullptr) {
               lList *children = lCreateList("display", STN_Type);
               lSetList(dnode, STN_children, children);
            }   
            lAppendElem(lGetListRW(dnode,STN_children), node);
         }
      }
   }

}


/********************************************************
 Search the share tree for the node corresponding to the
 user / project combination
 ********************************************************/
static lListElem *
search_userprj_node_work( lListElem *ep,      /* branch to search */
                          const char *username,
                          const char *projname,
                          lListElem **pep,    /* parent of found node */
                          lListElem *root )

{
   lListElem *cep, *fep;
   const char *nodename;
   lList *children;

   if (ep == nullptr || (username == nullptr && projname == nullptr)) {
      return nullptr;
   }

   nodename = lGetPosString(ep, STN_name_POS);

   /*
    * skip project nodes which don't match
    */

   if (lGetPosUlong(ep, STN_project_POS) &&
        ep != root &&
       (!projname || strcmp(nodename, projname))) {
      return nullptr;
   }

   children = lGetPosList(ep, STN_children_POS);

   /*
    * if project name is supplied, look for the project
    */

   if (projname != nullptr) {

      if (strcmp(nodename, projname) == 0) {

         /*
          * We have found the project node, now find the user node
          * within the project sub-tree. If there are no children,
          * return the project node.
          */

         if (children == nullptr) {
            return ep;
         }

         return search_userprj_node_work(ep, username, nullptr, pep, ep);
      } 
      else {
          /* search the child nodes for the project */
         for_each_rw(cep, children) {
            if ((fep = search_userprj_node_work(cep, username, projname, pep, root))) {
               if (pep && (cep == fep)) {
                  *pep = ep;
               }   
               return fep;
            }
         }
         /* project was not found, fall thru and return nullptr */
      }
   } 
   else {

      if (strcmp(nodename, username) == 0) {
         return ep;
      }

      /*
       * no project name supplied, so search for child node
       */

      for_each_rw(cep, children) {
         if ((fep = search_userprj_node_work(cep, username, projname, pep, root))) {
            if (pep && (cep == fep)) {
               *pep = ep;
            }   
            return fep;
         }
      }

      /*
       * if we've searched the entire tree, search for default user
       */

      if (ep == root && strcmp(username, "default")) {
         return search_userprj_node(ep, "default", nullptr, pep);
       }   

      /*
       * user was not found, fall thru and return nullptr
       */

   }

   return nullptr;
}


/********************************************************
 Search the share tree for the node corresponding to the
 user / project combination
 ********************************************************/
lListElem *
search_userprj_node( lListElem *ep,      /* root of the tree */
                     const char *username,
                     const char *projname,
                     lListElem **pep )   /* parent of found node */
{
   return search_userprj_node_work(ep, username, projname, pep, ep);
}


/*--------------------------------------------------------------------
 * sgeee_sort_jobs - sort jobs according the task-priority and job number 
 *--------------------------------------------------------------------*/

void sgeee_sort_jobs(lList **job_list)              /* JB_Type */
{
   sgeee_sort_jobs_by(job_list, SGEJ_priority, SGEJ_sort_decending , SGEJ_sort_ascending); /* decreasing priority then increasing job number */
}

void sgeee_sort_jobs_by(lList **job_list , int by_SGEJ_field,
                        int field_sort_direction, int jobnum_sort_direction) /* JB_Type */
// @todo by_SGEJ_field, field_sort_direction, jobnum_sort_direction are always the same
{

   lListElem *job = nullptr, *nxt_job = nullptr;
   lList *tmp_list = nullptr;    /* SGEJ_Type */
   const char *sortorder = nullptr;

   DENTER(TOP_LAYER);

   if (!job_list || !*job_list) {
      DRETURN_VOID;
   }

#if 0
   DPRINTF("+ + + + + + + + + + + + + + + + \n");
   DPRINTF("     SORTING SGEEE JOB LIST     \n");
   DPRINTF("+ + + + + + + + + + + + + + + + \n");
#endif

   /*-----------------------------------------------------------------
    * Create tmp list 
    *-----------------------------------------------------------------*/
   tmp_list = lCreateList("tmp list", SGEJ_Type);

   nxt_job = lFirstRW(*job_list); 
   while((job=nxt_job)) {
      lListElem *tmp_sge_job = nullptr;   /* SGEJ_Type */
      
      nxt_job = lNextRW(nxt_job);
      tmp_sge_job = lCreateElem(SGEJ_Type);

      {
         /* 
          * First try to find an enrolled task 
          * It will have the highest priority
          */
         const lListElem *tmp_task = lFirst(lGetList(job, JB_ja_tasks));

         /* 
          * If there is no enrolled task then take the template element
          */
         if (tmp_task == nullptr) {
            tmp_task = lFirst(lGetList(job, JB_ja_template));
         }

         lSetDouble(tmp_sge_job, SGEJ_priority, lGetDouble(tmp_task, JAT_prio));
         if (by_SGEJ_field != SGEJ_priority) { 
            lSetUlong(tmp_sge_job, SGEJ_state, lGetUlong(tmp_task, JAT_state));
            lSetString(tmp_sge_job, SGEJ_master_queue, lGetString(tmp_task, JAT_master_queue));
         }           
      }

      /*
      ** JB_job_number    (Ulong)
      ** JAT_prio         (Double)
      ** JB_job_name      (String)
      ** JB_owner         (String)
      ** JAT_status       (Ulong)
      ** JAT_master_queue (String)
      */

      lSetUlong(tmp_sge_job, SGEJ_job_number, lGetUlong(job, JB_job_number));
      lSetUlong64(tmp_sge_job, SGEJ_submission_time, lGetUlong64(job, JB_submission_time));

      if (by_SGEJ_field != SGEJ_priority) { 
         lSetString(tmp_sge_job, SGEJ_job_name, lGetString(job, JB_job_name));
         lSetString(tmp_sge_job, SGEJ_owner, lGetString(job, JB_owner));
      }
      lSetRef(tmp_sge_job, SGEJ_job_reference, job);
#if 0
      DPRINTF("JOB: " sge_u32" SUBMISSION_TIME: " sge_u64" PRIORITY: %f NAME: %s OWNER: %s QUEUE: %s STATUS: " sge_u32"\n",
         lGetUlong(tmp_sge_job, SGEJ_job_number), 
         lGetUlong64(tmp_sge_job, SGEJ_submission_time),
         lGetDouble(tmp_sge_job, SGEJ_priority),
         lGetString(tmp_sge_job, SGEJ_job_name) ? lGetString(tmp_sge_job, SGEJ_job_name) : "",
         lGetString(tmp_sge_job, SGEJ_owner) ? lGetString(tmp_sge_job, SGEJ_owner) : "",
         lGetString(tmp_sge_job, SGEJ_master_queue) ? lGetString(tmp_sge_job, SGEJ_master_queue) :"", 
         lGetUlong(tmp_sge_job, SGEJ_state));
#endif
      lAppendElem(tmp_list, tmp_sge_job);
      
      lDechainElem(*job_list, job);
   }

   /*-----------------------------------------------------------------
    * Sort tmp list
    *-----------------------------------------------------------------*/
   if ((field_sort_direction) && (jobnum_sort_direction)) {
      sortorder = "%I+ %I+ %I+";
   } else if (!field_sort_direction) {
      sortorder = "%I- %I+ %I+";
   } else if (!jobnum_sort_direction) {
      sortorder = "%I+ %I- %I-";
   } else {
      sortorder = "%I- %I- %I-";
   }

   lPSortList(tmp_list, sortorder,
	      by_SGEJ_field,
	      SGEJ_submission_time,
	      SGEJ_job_number);

   /*-----------------------------------------------------------------
    * rebuild job_list according sort order
    *-----------------------------------------------------------------*/
   for_each_rw(job, tmp_list) {
      lAppendElem(*job_list, (lListElem *)lGetRef(job, SGEJ_job_reference)); 
   } 

   /*-----------------------------------------------------------------
    * Release tmp list
    *-----------------------------------------------------------------*/
   lFreeList(&tmp_list);

   DRETURN_VOID;
}


