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
*    SGE_STRING(CCT_pe_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CCT_ignore_queues) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CCT_ignore_hosts) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CCT_job_messages) - @todo add summary
*    @todo add description
*
*    SGE_REF(CCT_pe_job_slots) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CCT_pe_job_slot_count) - @todo add summary
*    @todo add description
*
*/

enum {
   CCT_pe_name = CCT_LOWERBOUND,
   CCT_ignore_queues,
   CCT_ignore_hosts,
   CCT_job_messages,
   CCT_pe_job_slots,
   CCT_pe_job_slot_count
};

LISTDEF(CCT_Type)
   SGE_STRING(CCT_pe_name, CULL_DEFAULT)
   SGE_LIST(CCT_ignore_queues, CTI_Type, CULL_DEFAULT)
   SGE_LIST(CCT_ignore_hosts, CTI_Type, CULL_DEFAULT)
   SGE_LIST(CCT_job_messages, MES_Type, CULL_DEFAULT)
   SGE_REF(CCT_pe_job_slots, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(CCT_pe_job_slot_count, CULL_DEFAULT)
LISTEND

NAMEDEF(CCTN)
   NAME("CCT_pe_name")
   NAME("CCT_ignore_queues")
   NAME("CCT_ignore_hosts")
   NAME("CCT_job_messages")
   NAME("CCT_pe_job_slots")
   NAME("CCT_pe_job_slot_count")
NAMEEND

#define CCT_SIZE sizeof(CCTN)/sizeof(char *)


