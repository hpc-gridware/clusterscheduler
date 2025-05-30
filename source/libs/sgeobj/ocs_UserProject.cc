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
#include "sge_userprj.h"

/** @brief Decay usage for the passed user/project object
 *
 * In order to decay usage once per decay interval, we keep a time stamp in the
 * user/project of when it was last decayed and then apply the appropriate decay based
 * on the time stamp. This allows the usage to be decayed on the scheduling interval,
 * even though the decay interval is different from the scheduling interval.
 *
 * @param userprj The user/project element to decay.
 * @param is_user A boolean indicating if the element is a user or project.
 * @param decay_list The list of decay elements to use for decay calculations.
 * @param seqno The sequence number indicating new scheduling cycles.
 * @param curr_time The current time.
 */
void
ocs::UserProject::decay_userprj_usage(lListElem *userprj, bool is_user, const lList *decay_list, u_long seqno, const u_long64 curr_time) {
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
      u_long64 usage_time_stamp = lGetPosUlong64(userprj, obj_usage_time_stamp_POS);

      if (usage_time_stamp > 0 && (curr_time > usage_time_stamp)) {
         const lListElem *upp;
         double interval = sge_gmt64_to_gmt32_double(curr_time - usage_time_stamp);

         Usage::decay_usage(lGetPosList(userprj, obj_usage_POS), decay_list, interval);

         for_each_ep(upp, lGetPosList(userprj, obj_project_POS)) {
            Usage::decay_usage(lGetPosList(upp, UPP_usage_POS), decay_list, interval);
         }
      }

      lSetPosUlong64(userprj, obj_usage_time_stamp_POS, curr_time);
      if (seqno != (u_long) -1) {
      	lSetPosUlong(userprj, obj_usage_seqno_POS, seqno);
      }
   }
}
