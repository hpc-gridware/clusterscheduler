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
* @brief host load value
*
* an object of this type represents a single load value
*
*    SGE_STRING(HL_name) - name of the load variable
*    this is the name of the complex variable representing the load value
*
*    SGE_STRING(HL_value) - value of the load variable
*    value of the load variable as string
*
*    SGE_ULONG(HL_last_update) - date/time of last update
*    timestamp (seconds since epoch) when the load value was last updated
*
*    SGE_BOOL(HL_is_static) - is it a static load value?
*    true if it is a static load value, else false
*    a static load value is a value which is unlikely to change, e.g.
*     - arch
*     - num_proc
*     - mem_total
*    static load values are spooled and therefore are available even if an execution host is down
*
*/

enum {
   HL_name = HL_LOWERBOUND,
   HL_value,
   HL_last_update,
   HL_is_static
};

LISTDEF(HL_Type)
   SGE_STRING(HL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(HL_value, CULL_SUBLIST)
   SGE_ULONG(HL_last_update, CULL_DEFAULT)
   SGE_BOOL(HL_is_static, CULL_DEFAULT)
LISTEND

NAMEDEF(HLN)
   NAME("HL_name")
   NAME("HL_value")
   NAME("HL_last_update")
   NAME("HL_is_static")
NAMEEND

#define HL_SIZE sizeof(HLN)/sizeof(char *)


