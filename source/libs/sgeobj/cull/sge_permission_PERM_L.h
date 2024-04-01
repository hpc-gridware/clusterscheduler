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
* @brief Object used to request given permissions of user and host.
*
* Object used to request given permissions of user and host.
*
*    SGE_BOOL(PERM_is_manager) - true if manager
*    true if user has manager role
*
*    SGE_BOOL(PERM_is_operator) - true if operator
*    true if user has operator role
*
*    SGE_BOOL(PERM_is_admin_host) - true if admin host
*    true if the specified host is an admin host
*
*    SGE_BOOL(PERM_is_submit_host) - true if submit host
*    true if the specified host is a submit host
*
*    SGE_HOST(PERM_host) - hostname
*    name of the host with the specified permissions
*
*    SGE_STRING(PERM_username) - username
*    username with the specified permissions
*
*/

enum {
   PERM_is_manager = PERM_LOWERBOUND,
   PERM_is_operator,
   PERM_is_admin_host,
   PERM_is_submit_host,
   PERM_host,
   PERM_username
};

LISTDEF(PERM_Type)
   SGE_BOOL(PERM_is_manager, CULL_DEFAULT)
   SGE_BOOL(PERM_is_operator, CULL_DEFAULT)
   SGE_BOOL(PERM_is_admin_host, CULL_DEFAULT)
   SGE_BOOL(PERM_is_submit_host, CULL_DEFAULT)
   SGE_HOST(PERM_host, CULL_DEFAULT)
   SGE_STRING(PERM_username, CULL_DEFAULT)
LISTEND

NAMEDEF(PERMN)
   NAME("PERM_is_manager")
   NAME("PERM_is_operator")
   NAME("PERM_is_admin_host")
   NAME("PERM_is_submit_host")
   NAME("PERM_host")
   NAME("PERM_username")
NAMEEND

#define PERM_SIZE sizeof(PERMN)/sizeof(char *)


