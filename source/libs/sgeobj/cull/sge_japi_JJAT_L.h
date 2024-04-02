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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(JJAT_task_id) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JJAT_stat) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JJAT_rusage) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JJAT_failed_text) - @todo add summary
*    @todo add description
*
*/

enum {
   JJAT_task_id = JJAT_LOWERBOUND,
   JJAT_stat,
   JJAT_rusage,
   JJAT_failed_text
};

LISTDEF(JJAT_Type)
   SGE_ULONG(JJAT_task_id, CULL_DEFAULT)
   SGE_ULONG(JJAT_stat, CULL_DEFAULT)
   SGE_LIST(JJAT_rusage, UA_Type, CULL_DEFAULT)
   SGE_STRING(JJAT_failed_text, CULL_DEFAULT)
LISTEND

NAMEDEF(JJATN)
   NAME("JJAT_task_id")
   NAME("JJAT_stat")
   NAME("JJAT_rusage")
   NAME("JJAT_failed_text")
NAMEEND

#define JJAT_SIZE sizeof(JJATN)/sizeof(char *)


