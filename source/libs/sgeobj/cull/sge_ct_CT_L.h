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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
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
*    SGE_STRING(CT_str) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CT_refcount) - @todo add summary
*    @todo add description
*
*    SGE_INT(CT_count) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CT_rejected) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CT_cache) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(CT_messages_added) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(CT_resource_contribution) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(CT_rc_valid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CT_reservation_rejected) - @todo add summary
*    @todo add description
*
*/

enum {
   CT_str = CT_LOWERBOUND,
   CT_refcount,
   CT_count,
   CT_rejected,
   CT_cache,
   CT_messages_added,
   CT_resource_contribution,
   CT_rc_valid,
   CT_reservation_rejected
};

LISTDEF(CT_Type)
   SGE_STRING(CT_str, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(CT_refcount, CULL_DEFAULT)
   SGE_INT(CT_count, CULL_DEFAULT)
   SGE_ULONG(CT_rejected, CULL_DEFAULT)
   SGE_LIST(CT_cache, CCT_Type, CULL_DEFAULT)
   SGE_BOOL(CT_messages_added, CULL_DEFAULT)
   SGE_DOUBLE(CT_resource_contribution, CULL_DEFAULT)
   SGE_BOOL(CT_rc_valid, CULL_DEFAULT)
   SGE_ULONG(CT_reservation_rejected, CULL_DEFAULT)
LISTEND

NAMEDEF(CTN)
   NAME("CT_str")
   NAME("CT_refcount")
   NAME("CT_count")
   NAME("CT_rejected")
   NAME("CT_cache")
   NAME("CT_messages_added")
   NAME("CT_resource_contribution")
   NAME("CT_rc_valid")
   NAME("CT_reservation_rejected")
NAMEEND

#define CT_SIZE sizeof(CTN)/sizeof(char *)


