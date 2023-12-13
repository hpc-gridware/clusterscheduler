#ifndef SGE_REP_L_H
#define SGE_REP_L_H
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
*    SGE_ULONG(REP_type) - @todo add summary
*    @todo add description
*
*    SGE_HOST(REP_host) - @todo add summary
*    @todo add description
*
*    SGE_LIST(REP_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(REP_version) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(REP_seqno) - @todo add summary
*    @todo add description
*
*/

enum {
   REP_type = REP_LOWERBOUND,
   REP_host,
   REP_list,
   REP_version,
   REP_seqno
};

LISTDEF(REP_Type)
   SGE_ULONG(REP_type, CULL_DEFAULT)
   SGE_HOST(REP_host, CULL_DEFAULT)
   SGE_LIST(REP_list, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(REP_version, CULL_DEFAULT)
   SGE_ULONG(REP_seqno, CULL_DEFAULT)
LISTEND

NAMEDEF(REPN)
   NAME("REP_type")
   NAME("REP_host")
   NAME("REP_list")
   NAME("REP_version")
   NAME("REP_seqno")
NAMEEND

#define REP_SIZE sizeof(REPN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
