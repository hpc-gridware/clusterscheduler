#ifndef SGE_RN_L_H
#define SGE_RN_L_H
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

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(RN_min) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RN_max) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RN_step) - @todo add summary
*    @todo add description
*
*/

enum {
   RN_min = RN_LOWERBOUND,
   RN_max,
   RN_step
};

LISTDEF(RN_Type)
   SGE_ULONG(RN_min, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(RN_max, CULL_SUBLIST)
   SGE_ULONG(RN_step, CULL_SUBLIST)
LISTEND

NAMEDEF(RNN)
   NAME("RN_min")
   NAME("RN_max")
   NAME("RN_step")
NAMEEND

#define RN_SIZE sizeof(RNN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
