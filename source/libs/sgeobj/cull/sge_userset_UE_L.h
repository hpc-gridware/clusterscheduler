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
* @brief UserSet Entry
*
* An object of this type represents one entry in an user set.
*
*    SGE_STRING(UE_name) - User or Group name
*    Name of an user or a UNIX user group. Groups have the prefix @, e.g. @tape
*
*/

enum {
   UE_name = UE_LOWERBOUND
};

LISTDEF(UE_Type)
   SGE_STRING(UE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
LISTEND

NAMEDEF(UEN)
   NAME("UE_name")
NAMEEND

#define UE_SIZE sizeof(UEN)/sizeof(char *)


