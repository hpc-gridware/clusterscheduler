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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Users and projects, which share their usage bookkeeping
 *
 * The two objects are separate but nearly parallel: both carry shares,
 * override tickets and the accumulated usage the share tree policy decays.
 *
 * @see sge_userprj.h
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/**
 * @brief Append prj from list to dstring
 *
 * Append all projects in list lp to dstring string.
 *
 * @param this_list PR list, may be an empty handle
 * @param string dstring to append to
 *
 * @return nullptr or resulting string of dstring
 */
const char *prj_list_append_to_dstring(const ocs::tom::ConstList<ocs::tom::PR> this_list, dstring *string) {
   DENTER(BASIS_LAYER);

   const char *ret = nullptr;

   if (string != nullptr) {
      bool printed = false;

      for (const auto elem : this_list) {
         sge_dstring_append(string, elem.get<PR_name>());
         if (elem.next()) {
            sge_dstring_append(string, " ");
         }
         printed = true;
      }
      if (!printed) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }
   DRETURN(ret);
}


/**
 * @brief Is every project of a list defined?
 *
 * @param this_list the defined projects to check against (PR), may be an empty handle
 * @param[out] answer_list receives the name of the first project that is missing
 * @param prj_list the projects to look for (PR), may be an empty handle
 * @return true when all of them exist
 */
bool prj_list_do_all_exist(const ocs::tom::ConstList<ocs::tom::PR> this_list,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const ocs::tom::ConstList<ocs::tom::PR> prj_list) {
   DENTER(TOP_LAYER);
   bool ret = true;
   for (const auto prj : prj_list) {
      const char *name = prj.get<PR_name>();

      if (!this_list.find_first<PR_name>(name)) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_CQUEUE_UNKNOWNPROJECT_S, name);
         DTRACE;
         ret = false;
         break;
      }
   }
   DRETURN(ret);
}

/**
 * @brief A user object template, as `qconf -suser template` prints it
 *
 * @return the new element; the caller owns it
 */
ocs::tom::ElementPtr<ocs::tom::UU> getUserTemplate() {
   DENTER(TOP_LAYER);

   auto ep = ocs::tom::ElementPtr<ocs::tom::UU>::create();
   ep->set<UU_name>("template");
   ep->set<UU_default_project>(nullptr);
   ep->set<UU_oticket>(0);
   ep->set<UU_fshare>(0);
   ep->set<UU_job_cnt>(0);
   ep->set<UU_project>(nullptr);
   ep->set<UU_usage>(nullptr);
   ep->set<UU_long_term_usage>(nullptr);

   DRETURN(ep);
}

/**
 * @brief A project object template, as `qconf -sprj template` prints it
 *
 * @return the new element; the caller owns it
 */
ocs::tom::ElementPtr<ocs::tom::PR> getPrjTemplate() {
   DENTER(TOP_LAYER);

   auto ep = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep->set<PR_name>("template");
   ep->set<PR_oticket>(0);
   ep->set<PR_fshare>(0);
   ep->set<PR_job_cnt>(0);
   ep->set<PR_project>(nullptr);
   ep->set<PR_usage>(nullptr);
   ep->set<PR_long_term_usage>(nullptr);
   ep->set<PR_acl>(nullptr);
   ep->set<PR_xacl>(nullptr);

   DRETURN(ep);
}
