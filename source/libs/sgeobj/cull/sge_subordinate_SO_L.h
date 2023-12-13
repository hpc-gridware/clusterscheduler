#ifndef SGE_SO_L_H
#define SGE_SO_L_H
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
*    SGE_STRING(SO_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SO_threshold) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SO_slots_sum) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SO_seq_no) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SO_action) - @todo add summary
*    @todo add description
*
*/

enum {
   SO_name = SO_LOWERBOUND,
   SO_threshold,
   SO_slots_sum,
   SO_seq_no,
   SO_action
};

LISTDEF(SO_Type)
   SGE_STRING(SO_name, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(SO_threshold, CULL_SUBLIST)
   SGE_ULONG(SO_slots_sum, CULL_SUBLIST)
   SGE_ULONG(SO_seq_no, CULL_SUBLIST)
   SGE_ULONG(SO_action, CULL_SUBLIST)
LISTEND

NAMEDEF(SON)
   NAME("SO_name")
   NAME("SO_threshold")
   NAME("SO_slots_sum")
   NAME("SO_seq_no")
   NAME("SO_action")
NAMEEND

#define SO_SIZE sizeof(SON)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
