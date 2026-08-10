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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The share tree: the hierarchy the fair share policy distributes tickets over
 *
 * Each node is a user or a project and carries a share of its parent. A node
 * is addressed either by name - which need not be unique - or by its path from
 * the root, which is.
 *
 * @see sge_sharetree.h
 */

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_answer.h"

#include "msg_common.h"


/************************************************************************
   id_sharetree - set the sharetree node id
************************************************************************/
/**
 * @brief Number the share tree nodes
 *
 * Walks the tree from `ep` and gives every node an id, so nodes can be
 * referred to by number rather than by path.
 *
 * @param[out] alpp receives error messages
 * @param ep the node to start at
 * @param id the id to give this node
 * @param[out] ret_id receives the next unused id
 * @return true when the whole subtree could be numbered
 */
bool id_sharetree(lList **alpp, lListElem *ep, int id, int *ret_id)
{
   DENTER(TOP_LAYER);
   int my_id = id;

   if (ep == nullptr) {
      answer_list_add(alpp, MSG_OBJ_NOSTREEELEM, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }
   
   lSetUlong(ep, STN_id, my_id++);

   /* handle the children */
   for_each_rw_lv(cep, lGetList(ep, STN_children)) {
      if (false == id_sharetree(nullptr, cep, my_id, &my_id)) {
         DRETURN(false);
      }
   }

   if (ret_id) {
      *ret_id = my_id;
   }   

   DRETURN(true);
}  

/************************************************************************
  show_sharetree

  display a tree representation of sharetree 

 ************************************************************************/
/**
 * @brief Print a share tree as an indented tree
 *
 * Writes `name=shares` per node to stdout, one level of indentation per depth.
 *
 * @param ep the node to start at
 * @param indent the string one level of depth is indented by
 * @return 0 on success, -1 when `ep` is nullptr
 */
int show_sharetree(
const lListElem *ep,
const char *indent
) {
   DENTER(TOP_LAYER);
   FILE *fp = stdout;
   static int level = 0;
   int i;

   if (!ep) {
      DRETURN(-1);
   }

   for (i=0;i<level;i++)
      fprintf(fp, "%s", indent ? indent : "");
   fprintf(fp, "%s=" sge_u32 "\n", lGetString(ep, STN_name), lGetUlong(ep, STN_shares));
   for_each_ep_lv(cep, lGetList(ep, STN_children)) {
      level++;
      show_sharetree(cep, "   ");
      level--;
   }   

   DRETURN(0);
}

/************************************************************************
  show_sharetree_path

  display a path representation of sharetree 

 ************************************************************************/
/**
 * @brief Print a share tree as one path per node
 *
 * Writes the full path of every node to stdout, which is the form `qconf`
 * accepts back.
 *
 * @param root the node to start at
 * @param path the path prefix the printed paths start with
 * @return 0 on success, 1 when `root` is nullptr
 */
int show_sharetree_path(
lListElem *root,
const char *path 
) {
   DENTER(TOP_LAYER);
   lListElem *node;
   FILE *fp = stdout;
   ancestors_t ancestors;
   int i;
   dstring sb = DSTRING_INIT;

   if (!root) {
      DRETURN(1);
   }
 
   memset(&ancestors, 0, sizeof(ancestors));
   if ( !strcmp(path, "/") || !strcasecmp(path, "Root") ) {
      node = root;
   } else {
      node = search_named_node_path(root, path, &ancestors);
   }
 
   if (node) {
      for(i=0; i<ancestors.depth; i++)
         fprintf(fp, "/%s", lGetString(ancestors.nodes[i], STN_name));
      if (!strcmp(path, "/") || !strcasecmp(path, "Root") )
         fprintf(fp, "/=" sge_u32 "\n", lGetUlong(node, STN_shares));
      else
         fprintf(fp, "=" sge_u32 "\n", lGetUlong(node, STN_shares));
      free_ancestors(&ancestors);
      for_each_ep_lv(cep, lGetList(node, STN_children)) {

         if (!strcmp(path, "/") || !strcasecmp(path, "Root") )
            sge_dstring_sprintf(&sb, "/%s", lGetString(cep, STN_name));
         else
            sge_dstring_sprintf(&sb, "%s/%s", path,
                                 lGetString(cep, STN_name));
         show_sharetree_path(root, sge_dstring_get_string(&sb));
      }
   }
   else {
      fprintf(stderr, MSG_TREE_UNABLETOLACATEXINSHARETREE_S, path);
      fprintf(stderr, "\n");
      return 1;
   }
 
   sge_dstring_free(&sb);
   DRETURN(0);
}                                                                               

/***************************************************
 Generate a Template for a sharetreenode
 ***************************************************/
/**
 * @brief A share tree node template, valid as a share tree on its own
 *
 * The node is named `default` rather than `template` on purpose: `default` is
 * the catch-all that matches every user, so the offered template is a share
 * tree an administrator can accept unchanged. A leaf named `template` would be
 * rejected, since it names neither a user nor a project.
 *
 * @return the new node; the caller owns it
 */
lListElem *getSNTemplate()
{
   lListElem *ep;

   DENTER(TOP_LAYER);

   /* Use the catch-all "default" node so the offered template is a valid,
    * acceptable sharetree as-is: a single leaf named "template" would be rejected
    * (it is not a known user/project), whereas "default" matches all users. */
   ep = lCreateElem(STN_Type);
   lSetString(ep, STN_name, "default");
   lSetUlong(ep, STN_type, 0);
   lSetUlong(ep, STN_id, 0);
   lSetUlong(ep, STN_shares, 0);
   lSetList(ep, STN_children, nullptr);

   DRETURN(ep);
}

/********************************************************
 Search for a share tree node with a given name in a
 share tree
 ********************************************************/
/**
 * @brief Find the first share tree node with a given name
 *
 * Searches depth first from `ep`. A name may occur more than once in a share
 * tree, so the first match wins - use #search_named_node_path when the exact
 * node matters.
 *
 * @param ep root of the tree
 * @param name the node name to look for
 * @return the node, or nullptr when no node has that name
 */
lListElem *search_named_node(lListElem *ep,  /* root of the tree */
                             const char *name )
{
   DENTER(TOP_LAYER);
   lListElem *fep;
   static int sn_children_pos = -1;
   static int sn_name_pos = -1;

   if (!ep || !name) {
      DRETURN(nullptr);
   }

   if (sn_name_pos == -1) {
      sn_children_pos = lGetPosViaElem(ep, STN_children, SGE_NO_ABORT);
      sn_name_pos = lGetPosViaElem(ep, STN_name, SGE_NO_ABORT);
   }

   if (strcmp(lGetPosString(ep, sn_name_pos), name) == 0) {
      DRETURN(ep);
   }

   for_each_rw_lv(cep, lGetPosList(ep, sn_children_pos)) {
      if ((fep = search_named_node(cep, name))) {
         DRETURN(fep);
      }
   }
      
   DRETURN(nullptr);
}


/********************************************************
 Free internals of ancestors structure
 ********************************************************/
/**
 * @brief Release the node array of an ancestors structure
 *
 * The structure itself is not freed; only the array #ancestors_t::nodes points
 * at. The nodes themselves belong to the share tree and are not touched.
 *
 * @param[in,out] ancestors the structure to clean up; nullptr is ignored
 */
void free_ancestors( ancestors_t *ancestors )
{
   if (ancestors && ancestors->nodes) {
      sge_free(&(ancestors->nodes));
   }
}


/********************************************************
 Search for a share tree node with a given path in a
 share tree
 ********************************************************/

static lListElem *
search_by_path( lListElem *ep,  /* root of the [sub]tree */
                const char *name,
                const char *path,
                int delim,
                ancestors_t *ancestors,
                int depth )
{
   lList *children;
   lListElem *ret = nullptr;
   lListElem *child;
   char *buf=nullptr, *bufp;

   if (name == nullptr)
      delim = '.';

   if (name == nullptr || !strcmp(name, "*") ||
       !strcmp(name, lGetString(ep, STN_name))) {
      if (*path == 0) {
         if (name) {
            ret = ep;
            if (ancestors && depth > 0) {
               ancestors->depth = depth;
               ancestors->nodes =
                     (lListElem **)sge_malloc(depth * sizeof(lListElem *));
               ancestors->nodes[depth-1] = ep;
            }
         }
         return ret;
      }

      /* get next component from path */

      bufp = buf = sge_malloc(strlen(path)+1);
      if (*path == '.' || *path == '/')
         delim = *path++;
      while (*path && *path != '.' && *path != '/')
         *bufp++ = *path++;
      *bufp = 0;
      name = buf;
   } else if (delim == '/')
      return nullptr;

   if ((children = lGetListRW(ep, STN_children)))
      for (child=lFirstRW(children); child && !ret; child = child->next)
         ret = search_by_path(child, name, path, delim, ancestors, depth+1);

   if (ret && ancestors && ancestors->nodes && depth > 0)
      ancestors->nodes[depth-1] = ep;
   if (buf) {
      sge_free(&buf);
   }
   return ret;
}


/********************************************************
 Search for a share tree node with a given path in a
 share tree
 ********************************************************/
/**
 * @brief Find a share tree node by its path
 *
 * The path components are separated by `.` or `/`; `*` matches any node name.
 * Unlike #search_named_node this identifies exactly one node even when the
 * same name occurs several times in the tree.
 *
 * @param ep root of the tree
 * @param path the path to resolve
 * @param[out] ancestors receives the nodes from the root down to the match;
 *             may be nullptr. The caller frees it with #free_ancestors.
 * @return the node, or nullptr when the path does not resolve
 */
lListElem *
search_named_node_path( lListElem *ep,  /* root of the tree */
                        const char *path,
                        ancestors_t *ancestors )
{
   return search_by_path(ep, nullptr, path, 0, ancestors, 0);
}


/********************************************************
 Search for a share tree node with a given name in a
 share tree returning an array of ancestor nodes. The
 array is contained in the ancestors_t structure which
 consist of the depth and a dynamically allocated array
 of lListElem pointers for each node.  The nodes are
 ordered from the root node to the found node. The 
 caller is reponsible for freeing the nodes array.
 ********************************************************/

#ifdef notdef

lListElem *search_ancestor_list( lListElem *ep,  /* root of the tree */
                                 char *name,
                                 ancestors_t *ancestors )
{
   if (ancestors)
      return search_ancestors(ep, name, ancestors, 1);
   else
      return search_named_node(ep, name);
}

#endif

/**
 * @brief Find a node by name and record the path that led to it
 *
 * Like #search_named_node, but additionally fills `ancestors` with the nodes
 * from the root down to the match, in that order.
 *
 * @param ep root of the [sub]tree
 * @param name the node name to look for
 * @param[out] ancestors receives the path; the caller frees it with
 *             #free_ancestors
 * @param depth the depth `ep` sits at, 1 for the root
 * @return the node, or nullptr when no node has that name
 */
lListElem *
search_ancestors( lListElem *ep,
                  const char *name,
                  ancestors_t *ancestors,
                  int depth )
{
   DENTER(TOP_LAYER);
   lListElem *fep;
   static int sn_children_pos = -1;
   static int sn_name_pos = -1;

   if (!ep || !name) {
      DRETURN(nullptr);
   }

   if (sn_name_pos == -1) {
      sn_children_pos = lGetPosViaElem(ep, STN_children, SGE_NO_ABORT);
      sn_name_pos = lGetPosViaElem(ep, STN_name, SGE_NO_ABORT);
   }
   if (strcmp(lGetPosString(ep, sn_name_pos), name) == 0) {
      ancestors->depth = depth;
      ancestors->nodes = (lListElem **)sge_malloc(depth * sizeof(lListElem *));
      ancestors->nodes[depth-1] = ep;
      DRETURN(ep);
   }

   for_each_rw_lv(cep, lGetPosList(ep, sn_children_pos)) {
      if ((fep = search_ancestors(cep, name, ancestors, depth+1))) {
         ancestors->nodes[depth-1] = ep;
         DRETURN(fep);
      }
   }
      
   DRETURN(nullptr);
}

/**
 * @brief Search for a node which is not specified
 *
 * The function walks through the sharetree looking for the first node which
 * has no name.  A node with no name means that it was created as a result of
 * a dangling or circular child reference.
 *
 * @param ep root of the tree
 *
 * @return the first node which has no name or nullptr if all nodes have names
 */
lListElem *sge_search_unspecified_node(lListElem *ep)
{
   DENTER(TOP_LAYER);
   lListElem *ret = nullptr;

   if (ep == nullptr) {
      DRETURN(nullptr);
   }

   for_each_rw_lv(cep, lGetList(ep, STN_children)) {
      if ((ret = sge_search_unspecified_node(cep))) {
         DRETURN(ret);
      }   
   }

   if (lGetString(ep, STN_name) == nullptr) {
      DRETURN(ep);         /* no name filled in -> unspecified */
   }
   
   DRETURN(nullptr);
}
