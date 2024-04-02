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
* @brief Range Element
*
* 
* Object holding values which define an id range
* (e.g. 1-9:2 => 1, 3, 5, 7, 9).
* Lists of such objects are held within job objects
* (JB_Type) to hold job array task ids.
* Several functions may be used to access/modify/delete range
* elements and range lists. You may find them in the 'SEE ALSO'
* section below. It is highly advised to use these access functions
* because they assure and require a defined structure of
* elements and lists.
* 
* Range elements and lists stored in other objects fullfill
* following conditions:
* 
*    - min <= max
*    - step >= 1
*    - real range elements (e.g. 1-9:2 instead of 1-10:2)
*    - min-ids within range elements part of the same
*      list are in ascending order: min_id(n) < min_id(n+1)
*      (e.g. NOT 11-20:1; 1-9:2)
*    - ids within range elements part of the same
*      list are non-overlapping: max_id(n) < min_id(n+1)
*      (e.g. 1-9:2; 11-20:1; 25-28:3)
* 
* @see gdi/range/range_list_calculate_union_set()
* @see gdi/range/range_list_calculate_difference_set()
* @see gdi/range/range_list_calculate_intersection_set() 
* @see gdi/range/range_list_compress()
* @see gdi/range/range_list_get_first_id()
* @see gdi/range/range_list_get_last_id()
* @see gdi/range/range_list_get_number_of_ids()
* @see gdi/range/range_list_initialize()
* @see gdi/range/range_list_insert_id()
* @see gdi/range/range_list_is_id_within()
* @see gdi/range/range_list_move_first_n_ids()
* @see gdi/range/range_list_print_to_string()
* @see gdi/range/range_list_remove_id()
* @see gdi/range/range_correct_end()
* @see gdi/range/range_get_all_ids()
* @see gdi/range/range_get_number_of_ids()
* @see gdi/range/range_is_overlapping()
* @see gdi/range/range_is_id_within()
* @see gdi/range/range_set_all_ids()
* @see gdi/range/range_sort_uniq_compress()
*
*    SGE_ULONG(RN_min) - Lower Bound
*    minimum or start value of an id range (e.g. 1)
*
*    SGE_ULONG(RN_max) - Upper Bound
*    maximum or end value of an id range (e.g. 9)
*
*    SGE_ULONG(RN_step) - Step Size
*    stepsize (e.g. 2)
*
*/

enum {
   RN_min = RN_LOWERBOUND,
   RN_max,
   RN_step
};

LISTDEF(RN_Type)
   SGE_ULONG(RN_min, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(RN_max, CULL_SUBLIST)
   SGE_ULONG(RN_step, CULL_SUBLIST)
LISTEND

NAMEDEF(RNN)
   NAME("RN_min")
   NAME("RN_max")
   NAME("RN_step")
NAMEEND

#define RN_SIZE sizeof(RNN)/sizeof(char *)


