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
*    SGE_HOST(HGRP_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(HGRP_host_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(HGRP_cqueue_list) - @todo add summary
*    @todo add description
*
*/

enum {
   HGRP_name = HGRP_LOWERBOUND,
   HGRP_host_list,
   HGRP_cqueue_list
};

LISTDEF(HGRP_Type)
   SGE_HOST(HGRP_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(HGRP_host_list, HR_Type, CULL_SPOOL)
   SGE_LIST(HGRP_cqueue_list, CQ_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(HGRPN)
   NAME("HGRP_name")
   NAME("HGRP_host_list")
   NAME("HGRP_cqueue_list")
NAMEEND

#define HGRP_SIZE sizeof(HGRPN)/sizeof(char *)


