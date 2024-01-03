#ifndef SGE_CE_L_H
#define SGE_CE_L_H
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
*    SGE_STRING(CE_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CE_shortcut) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_valtype) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CE_stringval) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(CE_doubleval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_relop) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_consumable) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CE_defaultval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_dominant) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CE_pj_stringval) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(CE_pj_doubleval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_pj_dominant) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_requestable) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CE_tagged) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CE_urgency_weight) - @todo add summary
*    @todo add description
*
*/

enum {
   CE_name = CE_LOWERBOUND,
   CE_shortcut,
   CE_valtype,
   CE_stringval,
   CE_doubleval,
   CE_relop,
   CE_consumable,
   CE_defaultval,
   CE_dominant,
   CE_pj_stringval,
   CE_pj_doubleval,
   CE_pj_dominant,
   CE_requestable,
   CE_tagged,
   CE_urgency_weight
};

LISTDEF(CE_Type)
   SGE_STRING(CE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_STRING(CE_shortcut, CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(CE_valtype, CULL_SPOOL)
   SGE_STRING(CE_stringval, CULL_SPOOL | CULL_SUBLIST)
   SGE_DOUBLE(CE_doubleval, CULL_DEFAULT)
   SGE_ULONG(CE_relop, CULL_SPOOL)
   SGE_ULONG(CE_consumable, CULL_SPOOL)
   SGE_STRING(CE_defaultval, CULL_SPOOL)
   SGE_ULONG(CE_dominant, CULL_DEFAULT)
   SGE_STRING(CE_pj_stringval, CULL_DEFAULT)
   SGE_DOUBLE(CE_pj_doubleval, CULL_DEFAULT)
   SGE_ULONG(CE_pj_dominant, CULL_DEFAULT)
   SGE_ULONG(CE_requestable, CULL_SPOOL)
   SGE_ULONG(CE_tagged, CULL_DEFAULT)
   SGE_STRING(CE_urgency_weight, CULL_SPOOL)
LISTEND

NAMEDEF(CEN)
   NAME("CE_name")
   NAME("CE_shortcut")
   NAME("CE_valtype")
   NAME("CE_stringval")
   NAME("CE_doubleval")
   NAME("CE_relop")
   NAME("CE_consumable")
   NAME("CE_defaultval")
   NAME("CE_dominant")
   NAME("CE_pj_stringval")
   NAME("CE_pj_doubleval")
   NAME("CE_pj_dominant")
   NAME("CE_requestable")
   NAME("CE_tagged")
   NAME("CE_urgency_weight")
NAMEEND

#define CE_SIZE sizeof(CEN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
