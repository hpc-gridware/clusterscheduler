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
* @todo we don't have a primary key and cannot search by index. Primary key would be name + host.
*
*    SGE_ULONG(GRU_type) - Type
*    Type of the resource:
*    GRU_HARD_REQUEST_TYPE
*    GRU_SOFT_REQUEST_TYPE
*    GRU_RESOURCE_MAP_TYPE
*
*    SGE_STRING(GRU_name) - Name
*    Name of the resource (complex variable name).
*
*    SGE_DOUBLE(GRU_amount) - Granted Amount
*    How much of the resource has been granted.
*
*    SGE_LIST(GRU_resource_map_list) - Resource Map List
*    In case of resource maps: Which Ids and how much per Id has been granted
*    For RSMAPs: Which ids have been granted.
*
*    SGE_HOST(GRU_host) - Host
*    Host on which the resource has been granted (required in case of parallel jobs).
*
*/

enum {
   GRU_type = GRU_LOWERBOUND,
   GRU_name,
   GRU_amount,
   GRU_resource_map_list,
   GRU_host
};

LISTDEF(GRU_Type)
   SGE_ULONG(GRU_type, CULL_SPOOL)
   SGE_STRING(GRU_name, CULL_SPOOL)
   SGE_DOUBLE(GRU_amount, CULL_SPOOL)
   SGE_LIST(GRU_resource_map_list, RESL_Type, CULL_SPOOL)
   SGE_HOST(GRU_host, CULL_SPOOL)
LISTEND

NAMEDEF(GRUN)
   NAME("GRU_type")
   NAME("GRU_name")
   NAME("GRU_amount")
   NAME("GRU_resource_map_list")
   NAME("GRU_host")
NAMEEND

#define GRU_SIZE sizeof(GRUN)/sizeof(char *)


