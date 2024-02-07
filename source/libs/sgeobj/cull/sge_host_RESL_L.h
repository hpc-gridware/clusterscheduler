#ifndef SGE_RESL_L_H
#define SGE_RESL_L_H
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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource List
*
* Consumable Resource Map Resource List.
* Holds all consumable resource map identifiers
* of a particular job / array task.
*
*    SGE_STRING(RESL_value) - Value
*    The ID value of the RSMAP consumable complex.
*    @todo Is it the primary key? Would it be worth to have an index on it?
*
*    SGE_ULONG(RESL_jobid) - Job Number
*    The job, which currently uses this value (in case of 0 it is unused).
*
*    SGE_ULONG(RESL_taskid) - Task Number
*    The array task, which currently uses this value (in case of 0 it is unused).
*
*/

enum {
   RESL_value = RESL_LOWERBOUND,
   RESL_jobid,
   RESL_taskid
};

LISTDEF(RESL_Type)
   SGE_STRING(RESL_value, CULL_SPOOL)
   SGE_ULONG(RESL_jobid, CULL_SPOOL)
   SGE_ULONG(RESL_taskid, CULL_DEFAULT)
LISTEND

NAMEDEF(RESLN)
   NAME("RESL_value")
   NAME("RESL_jobid")
   NAME("RESL_taskid")
NAMEEND

#define RESL_SIZE sizeof(RESLN)/sizeof(char *)


#endif
