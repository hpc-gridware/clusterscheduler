#ifndef SGE_BN_L_H
#define SGE_BN_L_H
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
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(BN_strategy) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_n) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_socket_offset) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_core_offset) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_striding_step_size) - @todo add summary
*    @todo add description
*
*    SGE_STRING(BN_parameter_explicit) - @todo add summary
*    @todo add description
*
*/

enum {
   BN_strategy = BN_LOWERBOUND,
   BN_type,
   BN_parameter_n,
   BN_parameter_socket_offset,
   BN_parameter_core_offset,
   BN_parameter_striding_step_size,
   BN_parameter_explicit
};

LISTDEF(BN_Type)
   SGE_STRING(BN_strategy, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(BN_type, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_n, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_socket_offset, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_core_offset, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_striding_step_size, CULL_SUBLIST)
   SGE_STRING(BN_parameter_explicit, CULL_SUBLIST)
LISTEND

NAMEDEF(BNN)
   NAME("BN_strategy")
   NAME("BN_type")
   NAME("BN_parameter_n")
   NAME("BN_parameter_socket_offset")
   NAME("BN_parameter_core_offset")
   NAME("BN_parameter_striding_step_size")
   NAME("BN_parameter_explicit")
NAMEEND

#define BN_SIZE sizeof(BNN)/sizeof(char *)


#endif
