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

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_RN.h"

#include "sgeobj/cull/sge_range_RN_L.h"
#include "sgeobj/sge_answer.h"

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

/// The most slots a queue or a parallel environment may have; a slot range ending above it would mean infinity
constexpr uint32_t OCS_MAX_SLOTS = RANGE_INFINITY;


/*
 * range element
 */

void range_get_all_ids(ocs::tom::ConstElement<ocs::tom::RN> this_elem, uint32_t *min, uint32_t *max,
                       uint32_t *step);

void range_set_all_ids(ocs::tom::Element<ocs::tom::RN> this_elem, uint32_t min, uint32_t max, uint32_t step);

bool range_containes_id_less_than(ocs::tom::ConstElement<ocs::tom::RN> this_elem, uint32_t id);

bool range_is_id_within(ocs::tom::ConstElement<ocs::tom::RN> this_range, uint32_t id);

uint32_t range_get_number_of_ids(ocs::tom::ConstElement<ocs::tom::RN> this_elem);

void range_correct_end(ocs::tom::Element<ocs::tom::RN> this_elem);

[[nodiscard]] ocs::tom::ElementPtr<ocs::tom::RN>
range_parse_from_string(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string, int step_allowed,
                        int inf_allowed);

bool
range_parse_get_ids(const char *value, int step_allowed, uint32_t &start, uint32_t &end, uint32_t &step);

/*
 * range list
 */

void range_list_print_to_string(ocs::tom::ConstList<ocs::tom::RN> this_list, dstring *string, bool ignore_step,
                                bool comma_as_separator, bool print_always_as_range);

void range_to_dstring(uint32_t start, uint32_t end, int step,
                      dstring *dyn_taskrange_str, int ignore_step,
                      bool use_comma_as_separator, bool print_always_as_range);

void range_list_insert_id(ocs::tom::ListPtr<ocs::tom::RN> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          uint32_t id);

void range_list_remove_id(ocs::tom::ListPtr<ocs::tom::RN> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          uint32_t id);

void range_list_move_first_n_ids(ocs::tom::ListPtr<ocs::tom::RN> &this_list,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 ocs::tom::ListPtr<ocs::tom::RN> &list, uint32_t n);

bool range_list_is_id_within(ocs::tom::ConstList<ocs::tom::RN> this_list, uint32_t id);

bool range_list_is_empty(ocs::tom::ConstList<ocs::tom::RN> this_list);

void range_list_compress(ocs::tom::List<ocs::tom::RN> this_list);

void range_list_sort_uniq_compress(ocs::tom::List<ocs::tom::RN> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   bool correct_end);

uint32_t range_list_get_first_id(ocs::tom::ConstList<ocs::tom::RN> this_list,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

uint32_t range_list_get_last_id(ocs::tom::ConstList<ocs::tom::RN> this_list,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

double range_list_get_average(ocs::tom::ConstList<ocs::tom::RN> this_list, uint32_t upperbound);

void range_list_initialize(ocs::tom::ListPtr<ocs::tom::RN> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

uint32_t range_list_get_number_of_ids(ocs::tom::ConstList<ocs::tom::RN> this_list);

bool range_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::RN> &this_list,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  const char *string, bool just_parse, bool step_allowed, bool inf_allowed);

bool range_list_containes_id_less_than(ocs::tom::ConstList<ocs::tom::RN> range_list, uint32_t id);

void range_list_calculate_union_set(ocs::tom::ListPtr<ocs::tom::RN> &this_list,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    ocs::tom::ConstList<ocs::tom::RN> list1,
                                    ocs::tom::ConstList<ocs::tom::RN> list2);

void range_list_calculate_difference_set(ocs::tom::ListPtr<ocs::tom::RN> &this_list,
                                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                         ocs::tom::ConstList<ocs::tom::RN> list1,
                                         ocs::tom::ConstList<ocs::tom::RN> list2);

void range_list_calculate_intersection_set(ocs::tom::ListPtr<ocs::tom::RN> &this_list,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                           ocs::tom::ConstList<ocs::tom::RN> list1,
                                           ocs::tom::ConstList<ocs::tom::RN> list2);
