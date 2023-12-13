#ifndef SGE_ET_L_H
#define SGE_ET_L_H
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
*    SGE_ULONG(ET_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ET_timestamp) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ET_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ET_intkey) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ET_intkey2) - @todo add summary
*    @todo add description
*
*    SGE_STRING(ET_strkey) - @todo add summary
*    @todo add description
*
*    SGE_STRING(ET_strkey2) - @todo add summary
*    @todo add description
*
*    SGE_LIST(ET_new_version) - @todo add summary
*    @todo add description
*
*/

enum {
   ET_number = ET_LOWERBOUND,
   ET_timestamp,
   ET_type,
   ET_intkey,
   ET_intkey2,
   ET_strkey,
   ET_strkey2,
   ET_new_version
};

LISTDEF(ET_Type)
   SGE_ULONG(ET_number, CULL_DEFAULT)
   SGE_ULONG(ET_timestamp, CULL_DEFAULT)
   SGE_ULONG(ET_type, CULL_DEFAULT)
   SGE_ULONG(ET_intkey, CULL_DEFAULT)
   SGE_ULONG(ET_intkey2, CULL_DEFAULT)
   SGE_STRING(ET_strkey, CULL_DEFAULT)
   SGE_STRING(ET_strkey2, CULL_DEFAULT)
   SGE_LIST(ET_new_version, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(ETN)
   NAME("ET_number")
   NAME("ET_timestamp")
   NAME("ET_type")
   NAME("ET_intkey")
   NAME("ET_intkey2")
   NAME("ET_strkey")
   NAME("ET_strkey2")
   NAME("ET_new_version")
NAMEEND

#define ET_SIZE sizeof(ETN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
