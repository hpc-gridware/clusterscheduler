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

#include "sgeobj/sge_proc.h"

static lList *procList;

/**
 * @brief Look for a certain process entry in the proc table
 *
 * Looks for the element with the specified pid and return it.
 * Otherwise return nullptr
 *
 * @param pid The process ID of the process we're looking for.
 *
 * @return PRO_Type object or nullptr
 */
lListElem *get_pr(int pid) {
   if (procList == nullptr) {
      gen_procList();
      return nullptr;
   }

   return lGetElemUlongRW(procList, PRO_pid, pid);
}

/**
 * @brief Append a process element to the process list
 *
 * @param pr the element to append; ownership passes to the list
 */
void append_pr(lListElem *pr) {
   if (procList == nullptr) {
      gen_procList();
   }
   lAppendElem(procList, pr);
}

/**
 * @brief Creates the proc table
 *
 * Creates the hashed list procList
 */
void gen_procList() {
   procList = lCreateListHash("procList", PRO_Type, true);
}

/**
 * @brief Frees the formerly created procList
 */
void free_procList() {
   lFreeList(&procList);
}

/**
 * @brief Cleans the procList from already finished jobs
 *
 * Remove all elements from procList which has not been marked as running.
 * Mark all remaining elements as not running.
 */
void clean_procList() {

   lListElem *next = nullptr;
   lListElem *ep = nullptr;
   lCondition *cp = lWhere("%T(%I == %b)", PRO_Type, PRO_run, false); 
   int pos = lGetPosInDescr(PRO_Type, PRO_run);

   next = lFindFirstRW(procList, cp);

   /* free all finished jobs */

   while (next != nullptr) {
      ep = lFindNextRW(next, cp);
      lRemoveElem(procList, &next);
      next = ep;
   }

   lFreeWhere(&cp);

   /* mark all jobs to finished */

   for_each_rw (next, procList) {
      lSetPosBool(next, pos, false);
   }
}
