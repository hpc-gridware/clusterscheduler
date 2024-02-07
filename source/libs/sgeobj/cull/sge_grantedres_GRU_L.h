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
* @brief Granted Resource
*
* An object of this type represents a granted resource.
* A granted resource defines the amount of a resource which was requested by and granted to a job.
* Currently only used for RSMAPs.
* @todo Use it for all granted resources (e.g. prerequisit for soft consumables)
* @todo Don't we have to store it per petask?
*
*    SGE_ULONG(GRU_type) - Type
*    Type of the resource.
*
*    SGE_STRING(GRU_name) - Name
*    Name of the resource (complex variable name).
*
*    SGE_STRING(GRU_value) - Value
*    Value of the resource (how many/much has been granted)
*    @todo also which ones in case of RSMAPs?
*
*    SGE_STRING(GRU_host) - Host
*    Host on which the resource has been granted (required in case of parallel jobs).
*    @todo shouldn't it be of type lHostT?
*
*/

enum {
   GRU_type = GRU_LOWERBOUND,
   GRU_name,
   GRU_value,
   GRU_host
};

LISTDEF(GRU_Type)
   SGE_ULONG(GRU_type, CULL_SPOOL)
   SGE_STRING(GRU_name, CULL_SPOOL)
   SGE_STRING(GRU_value, CULL_SPOOL)
   SGE_STRING(GRU_host, CULL_SPOOL)
LISTEND

NAMEDEF(GRUN)
   NAME("GRU_type")
   NAME("GRU_name")
   NAME("GRU_value")
   NAME("GRU_host")
NAMEEND

#define GRU_SIZE sizeof(GRUN)/sizeof(char *)


