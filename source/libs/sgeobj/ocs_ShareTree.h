#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"

typedef int (*sge_node_func_t) (lListElem *node, void *ptr);

namespace ocs {
   class ShareTree {
      static constexpr auto DEFAULT_NODE_NAME = "default";

      static void
      add_auto_user(lListElem *node, const char *user_name, const char *proj_name);

   public:

      static void
      add_all_auto_users(lListElem *node, const lList *user_list, const lList *project_list, const lList *acl_list);

      static int
      foreach_call_func(lListElem *node, sge_node_func_t func, void *ptr);

      static int
      zero_node_fields(lListElem *node, void *ptr);

      static int
      zero_fields(lListElem *node);

      static double
      calc_node_usage(lListElem *node, const lList *user_list, const lList *project_list,
                      const lList *decay_list, u_long64 now, const char *project_name, u_long seqno);

      static void
      calc_node_proportion(lListElem *node, double total_usage);

      static void
      calc_proportions(const lList *share_tree, const lList *user_list, const lList *project_list, const lList *decay_list, u_long64 now);

      static void
      set_node_project_flag(lListElem *node, const lList *project_list);

      static lListElem *
      search_user_project_node(lListElem *node, const char *username, const char *project_name, lListElem **parent, const lListElem *root);

      static lListElem *
      duplicate_modified_nodes(lListElem *node, int last_seqno);
   };
}
