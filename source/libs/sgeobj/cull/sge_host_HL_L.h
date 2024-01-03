#ifndef SGE_HL_L_H
#define SGE_HL_L_H
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
*    SGE_STRING(HL_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(HL_value) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(HL_last_update) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(HL_is_static) - @todo add summary
*    @todo add description
*
*/

enum {
   HL_name = HL_LOWERBOUND,
   HL_value,
   HL_last_update,
   HL_is_static
};

LISTDEF(HL_Type)
   SGE_STRING(HL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(HL_value, CULL_SUBLIST)
   SGE_ULONG(HL_last_update, CULL_DEFAULT)
   SGE_BOOL(HL_is_static, CULL_DEFAULT)
LISTEND

NAMEDEF(HLN)
   NAME("HL_name")
   NAME("HL_value")
   NAME("HL_last_update")
   NAME("HL_is_static")
NAMEEND

#define HL_SIZE sizeof(HLN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
