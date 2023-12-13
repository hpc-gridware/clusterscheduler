#ifndef SGE_TE_L_H
#define SGE_TE_L_H
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
*    SGE_ULONG(TE_when) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_mode) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_interval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_uval0) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_uval1) - @todo add summary
*    @todo add description
*
*    SGE_STRING(TE_sval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_seqno) - @todo add summary
*    @todo add description
*
*/

enum {
   TE_when = TE_LOWERBOUND,
   TE_type,
   TE_mode,
   TE_interval,
   TE_uval0,
   TE_uval1,
   TE_sval,
   TE_seqno
};

LISTDEF(TE_Type)
   SGE_ULONG(TE_when, CULL_DEFAULT)
   SGE_ULONG(TE_type, CULL_DEFAULT)
   SGE_ULONG(TE_mode, CULL_DEFAULT)
   SGE_ULONG(TE_interval, CULL_DEFAULT)
   SGE_ULONG(TE_uval0, CULL_DEFAULT)
   SGE_ULONG(TE_uval1, CULL_DEFAULT)
   SGE_STRING(TE_sval, CULL_DEFAULT)
   SGE_ULONG(TE_seqno, CULL_DEFAULT)
LISTEND

NAMEDEF(TEN)
   NAME("TE_when")
   NAME("TE_type")
   NAME("TE_mode")
   NAME("TE_interval")
   NAME("TE_uval0")
   NAME("TE_uval1")
   NAME("TE_sval")
   NAME("TE_seqno")
NAMEEND

#define TE_SIZE sizeof(TEN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
