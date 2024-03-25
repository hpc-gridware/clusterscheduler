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
*    SGE_BOOL(PERM_is_manager) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(PERM_is_operator) - @todo add summary
*    @todo add description
*
*    SGE_HOST(PERM_req_host) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PERM_sge_username) - @todo add summary
*    @todo add description
*
*/

enum {
   PERM_is_manager = PERM_LOWERBOUND,
   PERM_is_operator,
   PERM_is_admin_host,
   PERM_is_submit_host,
   PERM_req_host,
   PERM_sge_username
};

LISTDEF(PERM_Type)
   SGE_BOOL(PERM_is_manager, CULL_DEFAULT)
   SGE_BOOL(PERM_is_operator, CULL_DEFAULT)
   SGE_BOOL(PERM_is_admin_host, CULL_DEFAULT)
   SGE_BOOL(PERM_is_submit_host, CULL_DEFAULT)
   SGE_HOST(PERM_req_host, CULL_DEFAULT)
   SGE_STRING(PERM_sge_username, CULL_DEFAULT)
LISTEND

NAMEDEF(PERMN)
   NAME("PERM_is_manager")
   NAME("PERM_is_operator")
   NAME("PERM_is_admin_host")
   NAME("PERM_is_submit_host")
   NAME("PERM_req_host")
   NAME("PERM_sge_username")
NAMEEND

#define PERM_SIZE sizeof(PERMN)/sizeof(char *)


