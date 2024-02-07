#pragma once
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
*    SGE_STRING(RT_tid) - @todo add summary
*    @todo add description
*
*    SGE_HOST(RT_hostname) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RT_status) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RT_state) - @todo add summary
*    @todo add description
*
*/

enum {
   RT_tid = RT_LOWERBOUND,
   RT_hostname,
   RT_status,
   RT_state
};

LISTDEF(RT_Type)
   SGE_STRING(RT_tid, CULL_DEFAULT)
   SGE_HOST(RT_hostname, CULL_DEFAULT)
   SGE_ULONG(RT_status, CULL_DEFAULT)
   SGE_ULONG(RT_state, CULL_DEFAULT)
LISTEND

NAMEDEF(RTN)
   NAME("RT_tid")
   NAME("RT_hostname")
   NAME("RT_status")
   NAME("RT_state")
NAMEEND

#define RT_SIZE sizeof(RTN)/sizeof(char *)


