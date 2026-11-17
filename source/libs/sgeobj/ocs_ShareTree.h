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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The share tree that decides fair-share entitlements
 */

#include <utility>

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

/// Callback #ocs::ShareTree::foreach_call_func applies to every node; a non-zero return stops the walk
typedef int (*sge_node_func_t) (lListElem *node, void *ptr);

namespace ocs {
   /// The share tree: the hierarchy of users and projects the fair share policy distributes tickets over
   class ShareTree {
      static constexpr auto DEFAULT_NODE_NAME = "default";

      static void
      add_auto_user(ocs::tom::Element<ocs::tom::STN> node, const char *user_name, const char *proj_name);

   public:

      static void
      add_all_auto_users(ocs::tom::Element<ocs::tom::STN> node, ocs::tom::ConstList<ocs::tom::UU> user_list,
                         ocs::tom::ConstList<ocs::tom::PR> project_list, ocs::tom::ConstList<ocs::tom::US> acl_list);

      static int
      foreach_call_func(ocs::tom::Element<ocs::tom::STN> node, sge_node_func_t func, void *ptr);

      static int
      zero_node_fields(lListElem *node, void *ptr);

      static double
      calc_node_usage(ocs::tom::Element<ocs::tom::STN> node, ocs::tom::ConstList<ocs::tom::UU> user_list,
                      ocs::tom::ConstList<ocs::tom::PR> project_list, ocs::tom::ConstList<ocs::tom::UA> decay_list,
                      uint64_t now, const char *project_name, u_long seqno);

      static void
      calc_node_proportion(ocs::tom::Element<ocs::tom::STN> node, double total_usage);

      static void
      calc_proportions(ocs::tom::List<ocs::tom::STN> share_tree, ocs::tom::ConstList<ocs::tom::UU> user_list,
                       ocs::tom::ConstList<ocs::tom::PR> project_list, ocs::tom::ConstList<ocs::tom::UA> decay_list,
                       uint64_t now);

      static void
      set_node_project_flag(ocs::tom::Element<ocs::tom::STN> node, ocs::tom::ConstList<ocs::tom::PR> project_list);

      static std::pair<ocs::tom::Element<ocs::tom::STN>, ocs::tom::Element<ocs::tom::STN>>
      search_user_project_node(ocs::tom::Element<ocs::tom::STN> node, const char *username, const char *project_name,
                               ocs::tom::ConstElement<ocs::tom::STN> root);
   };
}
