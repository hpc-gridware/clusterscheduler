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

/** @file
 * @brief The process table, as the execution daemon reads it
 *
 * @see sge_proc.h
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_proc.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

/// The process table; a process-lifetime list of PRO elements, read through typed handles
static lList *procList;

/**
 * @brief Look for a certain process entry in the proc table
 *
 * Looks for the element with the specified pid and return it.
 * Otherwise return an empty handle. Creates the table if there is none yet.
 *
 * @param pid The process ID of the process we're looking for.
 *
 * @return the PRO element, or an empty handle
 */
ocs::tom::Element<ocs::tom::PRO> get_pr(const int pid) {
   DENTER(TOP_LAYER);
   if (procList == nullptr) {
      gen_procList();
      DRETURN(nullptr);
   }

   DRETURN(ocs::tom::List<ocs::tom::PRO>(procList).find_first<PRO_pid>(pid));
}

/**
 * @brief Append a process element to the process list
 *
 * Creates the table if there is none yet.
 *
 * @param pr the element to append; the list takes it over, @p pr is empty afterwards
 * @return the handle on the element, now in the list
 */
ocs::tom::Element<ocs::tom::PRO> append_pr(ocs::tom::ElementPtr<ocs::tom::PRO> &&pr) {
   DENTER(TOP_LAYER);
   auto element = std::move(pr);
   if (procList == nullptr) {
      gen_procList();
   }
   DRETURN(ocs::tom::List<ocs::tom::PRO>(procList).push_back(std::move(element)));
}

/**
 * @brief Creates the proc table
 *
 * Creates the hashed list procList
 */
void gen_procList() {
   DENTER(TOP_LAYER);
   // the table is a process-lifetime static, so the owning handle hands it over
   procList = ocs::tom::ListPtr<ocs::tom::PRO>::create("procList").release();
   DRETURN_VOID;
}

/**
 * @brief Frees the formerly created procList
 */
void free_procList() {
   DENTER(TOP_LAYER);
   ocs::tom::free_raw_list(procList);
   DRETURN_VOID;
}

/**
 * @brief Cleans the procList from already finished jobs
 *
 * Remove all elements from procList which has not been marked as running.
 * Mark all remaining elements as not running.
 */
void clean_procList() {
   DENTER(TOP_LAYER);
   const auto list = ocs::tom::List<ocs::tom::PRO>(procList);

   // free all finished jobs
   list.remove_if(ocs::tom::field<PRO_run>() == false);

   // mark all jobs to finished
   for (auto pr : list) {
      ocs::tom::set<PRO_run>(pr, false);
   }
   DRETURN_VOID;
}
