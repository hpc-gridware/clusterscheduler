#ifndef SGE_APRJLIST_L_H
#define SGE_APRJLIST_L_H
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
*    SGE_HOST(APRJLIST_href) - @todo add summary
*    @todo add description
*
*    SGE_LIST(APRJLIST_value) - @todo add summary
*    @todo add description
*
*/

enum {
   APRJLIST_href = APRJLIST_LOWERBOUND,
   APRJLIST_value
};

LISTDEF(APRJLIST_Type)
   SGE_HOST(APRJLIST_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(APRJLIST_value, PR_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(APRJLISTN)
   NAME("APRJLIST_href")
   NAME("APRJLIST_value")
NAMEEND

#define APRJLIST_SIZE sizeof(APRJLISTN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
