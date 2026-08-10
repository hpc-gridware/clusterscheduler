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

#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/msg_sgeobjlib.h"

/**
 * @brief Find project in list
 *
 * Find project in list.
 *
 * @param lp PR_Type list
 * @param name project name
 *
 * @return nullptr or element pointer
 */
lListElem *prj_list_locate(const lList *lp, const char *name) 
{
   lListElem *ep = nullptr;

   DENTER(BASIS_LAYER);

   ep = lGetElemStrRW(lp, PR_name, name);

   DRETURN(ep);
}

/**
 * @brief Find user in list
 *
 * Find user in list.
 *
 * @param lp UU_Type list
 * @param name user name
 *
 * @return nullptr or element pointer
 */
lListElem *user_list_locate(const lList *lp, const char *name) 
{
   lListElem *ep = nullptr;

   DENTER(BASIS_LAYER);

   ep = lGetElemStrRW(lp, UU_name, name);

   DRETURN(ep);
}


/**
 * @brief Append prj from list to dstring
 *
 * Append all projects in list lp to dstring string.
 *
 * @param this_list PR_Type list
 * @param string dstring to append to
 *
 * @return nullptr or resulting string of dstring
 */
const char *prj_list_append_to_dstring(const lList *this_list, dstring *string)
{
   const char *ret = nullptr;

   DENTER(BASIS_LAYER);
   if (string != nullptr) {
      bool printed = false;

      for_each_ep_lv(elem, this_list) {
         sge_dstring_append(string, lGetString(elem, PR_name));
         if (lNext(elem)) {
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
 * @param this_list the defined projects to check against
 * @param[out] answer_list receives the name of the first project that is missing
 * @param prj_list the projects to look for
 * @return true when all of them exist
 */
bool
prj_list_do_all_exist(const lList *this_list, lList **answer_list,
                      const lList *prj_list)
{
   DENTER(TOP_LAYER);
   bool ret = true;
   for_each_ep_lv(prj, prj_list) {
      const char *name = lGetString(prj, PR_name);

      if (prj_list_locate(this_list, name) == nullptr) {
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

/***************************************************
 Generate a Template for a user
 ***************************************************/
/**
 * @brief A user object template, as `qconf -suser template` prints it
 *
 * @return the new element; the caller owns it
 */
lListElem *getUserTemplate()
{
   lListElem *ep;

   DENTER(TOP_LAYER);

   ep = lCreateElem(UU_Type);
   lSetString(ep, UU_name, "template");
   lSetString(ep, UU_default_project, nullptr);
   lSetUlong(ep, UU_oticket, 0);
   lSetUlong(ep, UU_fshare, 0);
   lSetUlong(ep, UU_job_cnt, 0);
   lSetList(ep, UU_project, nullptr);
   lSetList(ep, UU_usage, nullptr);
   lSetList(ep, UU_long_term_usage, nullptr);

   DRETURN(ep);
}

/***************************************************
 Generate a Template for a user or project
 ***************************************************/
/**
 * @brief A project object template, as `qconf -sprj template` prints it
 *
 * @return the new element; the caller owns it
 */
lListElem *getPrjTemplate()
{
   lListElem *ep;

   DENTER(TOP_LAYER);

   ep = lCreateElem(PR_Type);
   lSetString(ep, PR_name, "template");
   lSetUlong(ep, PR_oticket, 0);
   lSetUlong(ep, PR_fshare, 0);
   lSetUlong(ep, PR_job_cnt, 0);
   lSetList(ep, PR_project, nullptr);
   lSetList(ep, PR_usage, nullptr);
   lSetList(ep, PR_long_term_usage, nullptr);
   lSetList(ep, PR_acl, nullptr);
   lSetList(ep, PR_xacl, nullptr);

   DRETURN(ep);
}

