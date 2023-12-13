#ifndef SGE_JP_L_H
#define SGE_JP_L_H
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
*    SGE_ULONG(JP_pid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JP_background) - @todo add summary
*    @todo add description
*
*/

enum {
   JP_pid = JP_LOWERBOUND,
   JP_background
};

LISTDEF(JP_Type)
   SGE_ULONG(JP_pid, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(JP_background, CULL_DEFAULT)
LISTEND

NAMEDEF(JPN)
   NAME("JP_pid")
   NAME("JP_background")
NAMEEND

#define JP_SIZE sizeof(JPN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
