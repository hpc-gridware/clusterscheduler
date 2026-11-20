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
#include <format>
#include <optional>
#include <ostream>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_STN.h"

#include "msg_common.h"


/**
 * @brief Number a share tree node and its subtree
 *
 * @param ep the node to start at, not empty
 * @param id the id to give this node
 * @return the next unused id
 */
static int
ocs_id_sharetree_subtree(const ocs::tom::Element<ocs::tom::STN> ep, const int id) {
   DENTER(TOP_LAYER);

   int my_id = id;
   ocs::tom::set<STN_id>(ep, my_id++);

   /* handle the children */
   for (const auto cep : ocs::tom::get<STN_children>(ep)) {
      my_id = ocs_id_sharetree_subtree(cep, my_id);
   }

   DRETURN(my_id);
}

/**
 * @brief Number the share tree nodes
 *
 * Walks the tree from `ep` and gives every node an id, so nodes can be
 * referred to by number rather than by path.
 *
 * @param[out] alpp receives error messages
 * @param ep the node to start at, may be an empty handle
 * @param id the id to give this node
 * @return true when the whole subtree could be numbered, false for an empty @p ep
 */
bool id_sharetree(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<ocs::tom::STN> ep, const int id) {
   DENTER(TOP_LAYER);

   if (!ep) {
      answer_list_add(alpp, MSG_OBJ_NOSTREEELEM, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   ocs_id_sharetree_subtree(ep, id);

   DRETURN(true);
}


/**
 * @brief printf's rendering of a `%s` argument, nullptr included
 *
 * `printf("%s", nullptr)` prints `(null)` with glibc, while `os << nullptr` is
 * undefined behaviour. The two printers below went from `fprintf` to a stream
 * (CS-1841) and keep the old rendering rather than risking a crash on a node
 * without a name.
 *
 * @param s the string to write, may be nullptr
 * @return @p s, or `(null)` when @p s is nullptr
 */
static const char *
stn_cstr(const char *s) {
   return s != nullptr ? s : "(null)";
}

/**
 * @brief Print a share tree as an indented tree
 *
 * Writes `name=shares` per node, one level of indentation per depth.
 *
 * @param os     the stream to write to
 * @param ep     the node to start at (STN), may be an empty handle
 * @param indent the string one level of depth is indented by
 * @param level  the depth `ep` sits at, 0 for the root; the recursion passes
 *               its own depth down (CS-1841: it used to live in a function
 *               local `static`, which is not reentrant and would be wrong once
 *               this runs inside qmaster)
 * @return 0 on success, -1 when `ep` is nullptr
 */
int show_sharetree(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::STN> ep, const char *indent,
                   const int level) {
   DENTER(TOP_LAYER);

   if (!ep) {
      DRETURN(-1);
   }

   for (int i = 0; i < level; i++) {
      os << (indent != nullptr ? indent : "");
   }
   os << std::format("{}={}\n", stn_cstr(ocs::tom::get<STN_name>(ep)), ocs::tom::get<STN_shares>(ep));
   for (const auto cep : ocs::tom::get<STN_children>(ep)) {
      show_sharetree(os, cep, "   ", level + 1);
   }

   DRETURN(0);
}

/**
 * @brief Print a share tree as one path per node
 *
 * Writes the full path of every node, which is the form `qconf` accepts back.
 *
 * @param os   the stream to write to
 * @param root the node to start at (STN), may be an empty handle
 * @param path the path prefix the printed paths start with
 * @return 0 on success, 1 when `root` is nullptr or `path` names no node
 *
 * @note It writes **no** diagnostic of its own. Until CS-2820 it printed "Unable
 *       to locate <path> in sharetree" to stderr, which works in a client and
 *       not in qmaster - there the line lands in the daemon's stderr and the
 *       user is left with a silent failure, the same trap CS-2799 and CS-2819
 *       hit. The return value is the whole signal now, and the caller turns it
 *       into a message where it has an answer list to put one in.
 */
int show_sharetree_path(std::ostream &os, const ocs::tom::Element<ocs::tom::STN> root, const char *path) {
   DENTER(TOP_LAYER);
   ocs::tom::Element<ocs::tom::STN> node;
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
         os << "/" << stn_cstr(ocs::tom::get<STN_name>(ancestors.nodes[i]));
      if (!strcmp(path, "/") || !strcasecmp(path, "Root") )
         os << std::format("/={}\n", ocs::tom::get<STN_shares>(node));
      else
         os << std::format("={}\n", ocs::tom::get<STN_shares>(node));
      free_ancestors(&ancestors);
      for (const auto cep : ocs::tom::get<STN_children>(node)) {

         if (!strcmp(path, "/") || !strcasecmp(path, "Root") )
            sge_dstring_sprintf(&sb, "/%s", ocs::tom::get<STN_name>(cep));
         else
            sge_dstring_sprintf(&sb, "%s/%s", path,
                                 ocs::tom::get<STN_name>(cep));
         show_sharetree_path(os, root, sge_dstring_get_string(&sb));
      }
   }
   else {
      sge_dstring_free(&sb);
      DRETURN(1);
   }
 
   sge_dstring_free(&sb);
   DRETURN(0);
}                                                                               

/**
 * @brief Generate a template for a share tree node, valid as a share tree on its own
 *
 * The node is named `default` rather than `template` on purpose: `default` is
 * the catch-all that matches every user, so the offered template is a share
 * tree an administrator can accept unchanged. A leaf named `template` would be
 * rejected, since it names neither a user nor a project.
 *
 * @return the new node; the caller owns it
 */
ocs::tom::ElementPtr<ocs::tom::STN> getSNTemplate() {
   DENTER(TOP_LAYER);

   /* Use the catch-all "default" node so the offered template is a valid,
    * acceptable sharetree as-is: a single leaf named "template" would be rejected
    * (it is not a known user/project), whereas "default" matches all users. */
   auto ep = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep, "default");
   ocs::tom::set<STN_type>(ep, 0);
   ocs::tom::set<STN_id>(ep, 0);
   ocs::tom::set<STN_shares>(ep, 0);
   ocs::tom::set<STN_children>(ep, nullptr);

   DRETURN(ep);
}


/**
 * @brief Release the node array of an ancestors structure
 *
 * The structure itself is not freed; only the array #ancestors_t::nodes points
 * at. The nodes themselves belong to the share tree and are not touched.
 *
 * @param[in,out] ancestors the structure to clean up; nullptr is ignored
 */
void free_ancestors(ancestors_t *ancestors) {
   DENTER(TOP_LAYER);
   if (ancestors && ancestors->nodes) {
      sge_free(&(ancestors->nodes));
   }
   DRETURN_VOID;
}


/**
 * @brief Allocate the node array of an ancestors structure
 *
 * @param[out] ancestors the structure to fill
 * @param depth how many nodes the path has
 */
static void
ocs_ancestors_alloc(ancestors_t *ancestors, const int depth) {
   DENTER(TOP_LAYER);
   ancestors->depth = depth;
   // a handle is trivially copyable and destructible, so a malloc'ed array of them is fine
   ancestors->nodes = static_cast<ocs::tom::Element<ocs::tom::STN> *>(
      static_cast<void *>(sge_malloc(depth * sizeof(ocs::tom::Element<ocs::tom::STN>))));
   DRETURN_VOID;
}

/**
 * @brief Search a share tree node by the rest of a path
 *
 * @param ep root of the [sub]tree, not empty
 * @param name the path component this node has to match; nullptr at the start of the search
 * @param path the rest of the path
 * @param delim the delimiter that preceded @p name, '.' or '/'
 * @param[out] ancestors receives the nodes from the root down to the match; may be nullptr
 * @param depth the depth @p ep sits at, 0 for the root
 * @return the node, or an empty handle when the path does not resolve
 */
static ocs::tom::Element<ocs::tom::STN>
search_by_path(const ocs::tom::Element<ocs::tom::STN> ep, const char *name, const char *path, int delim,
               ancestors_t *ancestors, const int depth) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::STN> ret;
   char *buf = nullptr;
   char *bufp;

   if (name == nullptr)
      delim = '.';

   if (name == nullptr || !strcmp(name, "*") ||
       !strcmp(name, ocs::tom::get<STN_name>(ep))) {
      if (*path == 0) {
         if (name) {
            ret = ep;
            if (ancestors && depth > 0) {
               ocs_ancestors_alloc(ancestors, depth);
               ancestors->nodes[depth-1] = ep;
            }
         }
         DRETURN(ret);
      }

      /* get next component from path */

      bufp = buf = sge_malloc(strlen(path)+1);
      if (*path == '.' || *path == '/')
         delim = *path++;
      while (*path && *path != '.' && *path != '/')
         *bufp++ = *path++;
      *bufp = 0;
      name = buf;
   } else if (delim == '/') {
      DRETURN(nullptr);
   }

   for (auto child = ocs::tom::get<STN_children>(ep).front(); child && !ret; child = child.next()) {
      ret = search_by_path(child, name, path, delim, ancestors, depth+1);
   }

   if (ret && ancestors && ancestors->nodes && depth > 0)
      ancestors->nodes[depth-1] = ep;
   if (buf) {
      sge_free(&buf);
   }
   DRETURN(ret);
}

/**
 * @brief Find a share tree node by its path
 *
 * The path components are separated by `.` or `/`; `*` matches any node name.
 * This identifies exactly one node even when the same name occurs several
 * times in the tree.
 *
 * @param ep root of the tree (STN), not empty
 * @param path the path to resolve
 * @param[out] ancestors receives the nodes from the root down to the match;
 *             may be nullptr. The caller frees it with #free_ancestors.
 * @return the node, or an empty handle when the path does not resolve
 */
ocs::tom::Element<ocs::tom::STN>
search_named_node_path(const ocs::tom::Element<ocs::tom::STN> ep, const char *path, ancestors_t *ancestors) {
   DENTER(TOP_LAYER);
   const auto ret = search_by_path(ep, nullptr, path, 0, ancestors, 0);
   DRETURN(ret);
}



/**
 * @brief Find a node by name and record the path that led to it
 *
 * Searches depth first from `ep`; the first match wins. Fills `ancestors`
 * with the nodes from the root down to the match, in that order.
 *
 * @param ep root of the [sub]tree (STN), may be an empty handle
 * @param name the node name to look for
 * @param[out] ancestors receives the path; the caller frees it with
 *             #free_ancestors
 * @param depth the depth `ep` sits at, 1 for the root
 * @return the node, or an empty handle when no node has that name
 */
ocs::tom::Element<ocs::tom::STN>
search_ancestors(const ocs::tom::Element<ocs::tom::STN> ep, const char *name, ancestors_t *ancestors,
                 const int depth) {
   DENTER(TOP_LAYER);

   if (!ep || !name) {
      DRETURN(nullptr);
   }

   if (strcmp(ocs::tom::get<STN_name>(ep), name) == 0) {
      ocs_ancestors_alloc(ancestors, depth);
      ancestors->nodes[depth-1] = ep;
      DRETURN(ep);
   }

   for (const auto cep : ocs::tom::get<STN_children>(ep)) {
      if (const auto fep = search_ancestors(cep, name, ancestors, depth+1); fep) {
         ancestors->nodes[depth-1] = ep;
         DRETURN(fep);
      }
   }

   DRETURN(nullptr);
}
