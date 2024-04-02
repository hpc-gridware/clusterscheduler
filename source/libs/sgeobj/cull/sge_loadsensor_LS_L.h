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
*    SGE_STRING(LS_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(LS_command) - @todo add summary
*    @todo add description
*
*    SGE_STRING(LS_pid) - @todo add summary
*    @todo add description
*
*    SGE_REF(LS_in) - @todo add summary
*    @todo add description
*
*    SGE_REF(LS_out) - @todo add summary
*    @todo add description
*
*    SGE_REF(LS_err) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(LS_has_to_restart) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(LS_tag) - @todo add summary
*    @todo add description
*
*    SGE_LIST(LS_incomplete) - @todo add summary
*    @todo add description
*
*    SGE_LIST(LS_complete) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(LS_last_mod) - @todo add summary
*    @todo add description
*
*/

enum {
   LS_name = LS_LOWERBOUND,
   LS_command,
   LS_pid,
   LS_in,
   LS_out,
   LS_err,
   LS_has_to_restart,
   LS_tag,
   LS_incomplete,
   LS_complete,
   LS_last_mod
};

LISTDEF(LS_Type)
   SGE_STRING(LS_name, CULL_DEFAULT)
   SGE_STRING(LS_command, CULL_DEFAULT)
   SGE_STRING(LS_pid, CULL_DEFAULT)
   SGE_REF(LS_in, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LS_out, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LS_err, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_BOOL(LS_has_to_restart, CULL_DEFAULT)
   SGE_ULONG(LS_tag, CULL_DEFAULT)
   SGE_LIST(LS_incomplete, LR_Type, CULL_DEFAULT)
   SGE_LIST(LS_complete, LR_Type, CULL_DEFAULT)
   SGE_ULONG(LS_last_mod, CULL_DEFAULT)
LISTEND

NAMEDEF(LSN)
   NAME("LS_name")
   NAME("LS_command")
   NAME("LS_pid")
   NAME("LS_in")
   NAME("LS_out")
   NAME("LS_err")
   NAME("LS_has_to_restart")
   NAME("LS_tag")
   NAME("LS_incomplete")
   NAME("LS_complete")
   NAME("LS_last_mod")
NAMEEND

#define LS_SIZE sizeof(LSN)/sizeof(char *)


