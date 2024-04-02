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
* @brief Resource Utilization
*
* Utilization of a certain resource.
* Both the current utilization can be stored, as well as
* future utilization (due to currently running jobs,
* advance reservations and resource reservations).
*
*    SGE_STRING(RUE_name) - Resource Name
*    The name of the resource (= the name of the complex variable, e.g. slots)
*
*    SGE_DOUBLE(RUE_utilized_now) - Utilized Now
*    Currently used amount.
*
*    SGE_LIST(RUE_utilized_now_resource_map_list) - Utilized Now Resource Map List
*    Currently used amount of Resource Maps
*
*    SGE_LIST(RUE_utilized) - Utilized
*    A resource diagram indicating future utilization.
*
*    SGE_DOUBLE(RUE_utilized_now_nonexclusive) - Utilized Now Non-Exclusive
*    Currently used amount of implicitly used exclusive resources.
*
*    SGE_LIST(RUE_utilized_nonexclusive) - Utilized Non-Exclusive
*    A resource diagram indicating future utilization of implicitly used exclusive resources.
*
*/

enum {
   RUE_name = RUE_LOWERBOUND,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive
};

LISTDEF(RUE_Type)
   SGE_STRING(RUE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_DOUBLE(RUE_utilized_now, CULL_DEFAULT)
   SGE_LIST(RUE_utilized_now_resource_map_list, RESL_Type, CULL_DEFAULT)
   SGE_LIST(RUE_utilized, RDE_Type, CULL_DEFAULT)
   SGE_DOUBLE(RUE_utilized_now_nonexclusive, CULL_DEFAULT)
   SGE_LIST(RUE_utilized_nonexclusive, RDE_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(RUEN)
   NAME("RUE_name")
   NAME("RUE_utilized_now")
   NAME("RUE_utilized_now_resource_map_list")
   NAME("RUE_utilized")
   NAME("RUE_utilized_now_nonexclusive")
   NAME("RUE_utilized_nonexclusive")
NAMEEND

#define RUE_SIZE sizeof(RUEN)/sizeof(char *)


