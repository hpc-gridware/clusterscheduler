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
*    SGE_STRING(PA_origin) - @todo add summary
*    @todo add description
*
*    SGE_HOST(PA_submit_host) - @todo add summary
*    @todo add description
*
*    SGE_HOST(PA_exec_host) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PA_translation) - @todo add summary
*    @todo add description
*
*/

enum {
   PA_origin = PA_LOWERBOUND,
   PA_submit_host,
   PA_exec_host,
   PA_translation
};

LISTDEF(PA_Type)
   SGE_STRING(PA_origin, CULL_PRIMARY_KEY | CULL_SPOOL)
   SGE_HOST(PA_submit_host, CULL_SPOOL)
   SGE_HOST(PA_exec_host, CULL_SPOOL)
   SGE_STRING(PA_translation, CULL_SPOOL)
LISTEND

NAMEDEF(PAN)
   NAME("PA_origin")
   NAME("PA_submit_host")
   NAME("PA_exec_host")
   NAME("PA_translation")
NAMEEND

#define PA_SIZE sizeof(PAN)/sizeof(char *)


