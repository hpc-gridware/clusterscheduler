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

/** @file
 * @brief Declarations for host reference lists (`HR`)
 *
 * @see sge_href.cc
 */

#include "uti/sge_dstring.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"

#include "sgeobj/sge_answer.h"


bool href_list_add(ocs::tom::ListPtr<ocs::tom::HR> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                   const char *host_or_group);
void ocs_href_list_emplace(ocs::tom::ListPtr<ocs::tom::HR> &this_list, const char *name);

bool href_list_remove_existing(ocs::tom::ListPtr<ocs::tom::HR> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                               ocs::tom::ConstList<ocs::tom::HR> list);

bool href_list_has_member(ocs::tom::ConstList<ocs::tom::HR> this_list, const char *host_or_group);

bool href_list_append_to_dstring(ocs::tom::ConstList<ocs::tom::HR> this_list, dstring *string);

bool href_list_compare(ocs::tom::ConstList<ocs::tom::HR> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       ocs::tom::ConstList<ocs::tom::HR> list, ocs::tom::ListPtr<ocs::tom::HR> *add_hosts,
                       ocs::tom::ListPtr<ocs::tom::HR> *add_groups, ocs::tom::ListPtr<ocs::tom::HR> *equity_hosts,
                       ocs::tom::ListPtr<ocs::tom::HR> *equity_groups);

bool href_list_find_diff(ocs::tom::ConstList<ocs::tom::HR> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                         ocs::tom::ConstList<ocs::tom::HR> list, ocs::tom::ListPtr<ocs::tom::HR> *add_hosts,
                         ocs::tom::ListPtr<ocs::tom::HR> *rem_hosts, ocs::tom::ListPtr<ocs::tom::HR> *add_groups,
                         ocs::tom::ListPtr<ocs::tom::HR> *rem_groups);

bool href_list_find_effective_diff(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   ocs::tom::ConstList<ocs::tom::HR> add_groups,
                                   ocs::tom::ConstList<ocs::tom::HR> rem_groups,
                                   ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                   ocs::tom::ListPtr<ocs::tom::HR> *add_hosts,
                                   ocs::tom::ListPtr<ocs::tom::HR> *rem_hosts);

bool href_list_find_references(ocs::tom::ConstList<ocs::tom::HR> this_list,
                               ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                               ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                               ocs::tom::ListPtr<ocs::tom::HR> *used_hosts,
                               ocs::tom::ListPtr<ocs::tom::HR> *used_groups,
                               ocs::tom::ListPtr<ocs::tom::HR> *used_matchers = nullptr,
                               bool expand_matchers = true);

bool href_list_find_all_references(ocs::tom::ConstList<ocs::tom::HR> this_list,
                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                   ocs::tom::ListPtr<ocs::tom::HR> *used_hosts,
                                   ocs::tom::ListPtr<ocs::tom::HR> *used_groups,
                                   ocs::tom::ListPtr<ocs::tom::HR> *used_matchers = nullptr,
                                   bool expand_matchers = true);

bool href_list_find_referencees(ocs::tom::ConstList<ocs::tom::HR> this_list,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                ocs::tom::ListPtr<ocs::tom::HR> *occupant_groups);

bool href_list_find_all_referencees(ocs::tom::ConstList<ocs::tom::HR> this_list,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                    ocs::tom::ListPtr<ocs::tom::HR> *occupant_groups);

ocs::tom::Element<ocs::tom::HR> href_list_locate(ocs::tom::ConstList<ocs::tom::HR> this_list, const char *name);

bool href_list_resolve_hostnames(ocs::tom::List<ocs::tom::HR> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 bool ignore_error,
                                 bool introducing, ocs::tom::ConstElement<ocs::tom::HGRP> hgroup = nullptr);

void href_list_debug_print(ocs::tom::ConstList<ocs::tom::HR> this_list, const char *prefix);

void href_list_make_uniq(ocs::tom::List<ocs::tom::HR> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
