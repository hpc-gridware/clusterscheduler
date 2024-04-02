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
*    SGE_LIST(LDR_queue_ref_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(LDR_limit) - @todo add summary
*    @todo add description
*
*    SGE_REF(LDR_global) - @todo add summary
*    @todo add description
*
*    SGE_REF(LDR_host) - @todo add summary
*    @todo add description
*
*    SGE_REF(LDR_queue) - @todo add summary
*    @todo add description
*
*/

enum {
   LDR_queue_ref_list = LDR_LOWERBOUND,
   LDR_limit,
   LDR_global,
   LDR_host,
   LDR_queue
};

LISTDEF(LDR_Type)
   SGE_LIST(LDR_queue_ref_list, QR_Type, CULL_DEFAULT)
   SGE_STRING(LDR_limit, CULL_DEFAULT)
   SGE_REF(LDR_global, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LDR_host, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LDR_queue, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(LDRN)
   NAME("LDR_queue_ref_list")
   NAME("LDR_limit")
   NAME("LDR_global")
   NAME("LDR_host")
   NAME("LDR_queue")
NAMEEND

#define LDR_SIZE sizeof(LDRN)/sizeof(char *)


