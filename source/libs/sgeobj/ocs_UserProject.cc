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
 * @brief Users and projects, and the attributes they share
 */

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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_UU.h"

/**
 * @brief Decay the usage of a user or a project object
 *
 * Both objects have the same usage layout; the fields are given per type.
 *
 * @tparam USAGE_SEQNO the usage sequence number field
 * @tparam USAGE_TIME_STAMP the usage time stamp field
 * @tparam USAGE the usage list field
 * @tparam PROJECT the per-project usage list field
 * @tparam T the object type
 * @param userprj the user or project element, may be an empty handle
 * @param decay_list the decay constants (UA), may be an empty handle
 * @param seqno the sequence number indicating new scheduling cycles; (u_long)-1 leaves the stored one
 * @param curr_time the current time
 */
template <auto USAGE_SEQNO, auto USAGE_TIME_STAMP, auto USAGE, auto PROJECT, typename T>
static void
ocs_decay_userprj_usage(const ocs::tom::Element<T> userprj, const ocs::tom::ConstList<ocs::tom::UA> decay_list,
                        const u_long seqno, const uint64_t curr_time) {
   DENTER(BASIS_LAYER);

   if (userprj && seqno != ocs::tom::get<USAGE_SEQNO>(userprj)) {
      const uint64_t usage_time_stamp = ocs::tom::get<USAGE_TIME_STAMP>(userprj);

      if (usage_time_stamp > 0 && (curr_time > usage_time_stamp)) {
         const double interval = sge_gmt64_to_gmt32_double(curr_time - usage_time_stamp);

         ocs::Usage::decay_usage(ocs::tom::get<USAGE>(userprj), decay_list, interval);

         for (const auto upp : ocs::tom::get<PROJECT>(userprj)) {
            ocs::Usage::decay_usage(ocs::tom::get<UPP_usage>(upp), decay_list, interval);
         }
      }

      ocs::tom::set<USAGE_TIME_STAMP>(userprj, curr_time);
      if (seqno != (u_long) -1) {
         ocs::tom::set<USAGE_SEQNO>(userprj, seqno);
      }
   }

   DRETURN_VOID;
}

/** @brief Decay usage for the passed user object
 *
 * In order to decay usage once per decay interval, we keep a time stamp in the
 * user/project of when it was last decayed and then apply the appropriate decay based
 * on the time stamp. This allows the usage to be decayed on the scheduling interval,
 * even though the decay interval is different from the scheduling interval.
 *
 * @param user The user element to decay, may be an empty handle.
 * @param decay_list The list of decay elements to use for decay calculations (UA).
 * @param seqno The sequence number indicating new scheduling cycles.
 * @param curr_time The current time.
 */
void
ocs::UserProject::decay_userprj_usage(const ocs::tom::Element<ocs::tom::UU> user,
                                      const ocs::tom::ConstList<ocs::tom::UA> decay_list, const u_long seqno,
                                      const uint64_t curr_time) {
   DENTER(BASIS_LAYER);
   ocs_decay_userprj_usage<UU_usage_seqno, UU_usage_time_stamp, UU_usage, UU_project>(user, decay_list, seqno,
                                                                                     curr_time);
   DRETURN_VOID;
}

/** @brief Decay usage for the passed project object
 *
 * See the overload for a user object.
 *
 * @param project The project element to decay, may be an empty handle.
 * @param decay_list The list of decay elements to use for decay calculations (UA).
 * @param seqno The sequence number indicating new scheduling cycles.
 * @param curr_time The current time.
 */
void
ocs::UserProject::decay_userprj_usage(const ocs::tom::Element<ocs::tom::PR> project,
                                      const ocs::tom::ConstList<ocs::tom::UA> decay_list, const u_long seqno,
                                      const uint64_t curr_time) {
   DENTER(BASIS_LAYER);
   ocs_decay_userprj_usage<PR_usage_seqno, PR_usage_time_stamp, PR_usage, PR_project>(project, decay_list, seqno,
                                                                                     curr_time);
   DRETURN_VOID;
}

