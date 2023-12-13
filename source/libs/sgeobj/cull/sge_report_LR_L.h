#ifndef SGE_LR_L_H
#define SGE_LR_L_H
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
*    SGE_STRING(LR_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(LR_value) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(LR_global) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(LR_static) - @todo add summary
*    @todo add description
*
*    SGE_HOST(LR_host) - @todo add summary
*    @todo add description
*
*/

enum {
   LR_name = LR_LOWERBOUND,
   LR_value,
   LR_global,
   LR_static,
   LR_host
};

LISTDEF(LR_Type)
   SGE_STRING(LR_name, CULL_HASH)
   SGE_STRING(LR_value, CULL_DEFAULT)
   SGE_ULONG(LR_global, CULL_DEFAULT)
   SGE_ULONG(LR_static, CULL_DEFAULT)
   SGE_HOST(LR_host, CULL_HASH)
LISTEND

NAMEDEF(LRN)
   NAME("LR_name")
   NAME("LR_value")
   NAME("LR_global")
   NAME("LR_static")
   NAME("LR_host")
NAMEEND

#define LR_SIZE sizeof(LRN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
