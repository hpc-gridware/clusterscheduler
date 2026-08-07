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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations and limits for range lists (`RN_Type`)
 *
 * @see sge_range.cc
 */

#include "uti/sge_dstring.h"

#include "sgeobj/cull/sge_range_RN_L.h"

/// Passed where a function may either only parse its input or also apply it
#define JUST_PARSE        true
/// The range may end in infinity, written as an open upper bound
#define INF_ALLOWED       true
/// The range must have a finite upper bound
#define INF_NOT_ALLOWED   false

/// How many ids are printed on one line before it is wrapped
#define MAX_IDS_PER_LINE  8
/// Width a printed range list is wrapped at
#define MAX_LINE_LEN      70

/// The value standing for an unbounded upper end of a range
#define RANGE_INFINITY (9999999)

/// Callback that either removes an id from a range list or inserts it into one
typedef void (*range_remove_insert_t)(lList **, lList **, uint32_t);

/*
 * range element
 */

void
range_get_all_ids(const lListElem *this_elem, uint32_t *min,
                  uint32_t *max, uint32_t *step);

void
range_set_all_ids(lListElem *this_elem, uint32_t min,
                  uint32_t max, uint32_t step);

bool range_containes_id_less_than(const lListElem *this_elem, uint32_t id);

bool range_is_id_within(const lListElem *this_range, uint32_t id);

uint32_t range_get_number_of_ids(const lListElem *this_elem);

void range_correct_end(lListElem *this_elem);

void
range_parse_from_string(lListElem **this_elem, lList **answer_list,
                        const char *string, int step_allowed, int inf_allowed);

bool
range_parse_get_ids(const char *value, int step_allowed, uint32_t &start, uint32_t &end, uint32_t &step);

/*
 * range list
 */

void
range_list_print_to_string(const lList *this_list,
                           dstring *string, bool ignore_step,
                           bool comma_as_separator, bool print_always_as_range);

void range_to_dstring(uint32_t start, uint32_t end, int step,
                      dstring *dyn_taskrange_str, int ignore_step,
                      bool use_comma_as_separator, bool print_always_as_range);

void range_list_insert_id(lList **this_list, lList **answer_list, uint32_t id);

void range_list_remove_id(lList **this_list, lList **answer_list, uint32_t id);

void
range_list_move_first_n_ids(lList **this_list, lList **answer_list,
                            lList **list, uint32_t n);

bool range_list_is_id_within(const lList *this_list, uint32_t id);

bool range_list_is_empty(const lList *this_list);

void range_list_compress(lList *this_list);

void range_list_sort_uniq_compress(lList *this_list, lList **answer_list, bool correct_end);

uint32_t range_list_get_first_id(const lList *this_list, lList **answer_list);

uint32_t range_list_get_last_id(const lList *this_list, lList **answer_list);

double range_list_get_average(const lList *this_list, uint32_t upperbound);

void range_list_initialize(lList **this_list, lList **answer_list);

uint32_t range_list_get_number_of_ids(const lList *this_list);

bool
range_list_parse_from_string(lList **this_list, lList **answer_list,
                             const char *string, bool just_parse,
                             bool step_allowed, bool inf_allowed);

bool range_list_containes_id_less_than(const lList *range_list, uint32_t id);

void
range_list_calculate_union_set(lList **this_list, lList **answer_list,
                               const lList *list1, const lList *list2);

void
range_list_calculate_difference_set(lList **this_list, lList **answer_list,
                                    const lList *list1, const lList *list2);

void
range_list_calculate_intersection_set(lList **this_list, lList **answer_list,
                                      const lList *list1, const lList *list2);
