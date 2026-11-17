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
 * @brief Declarations for host reference lists (`HR_Type`)
 *
 * @see sge_href.cc
 */

#include "uti/sge_dstring.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"

#include "sgeobj/cull/sge_href_HR_L.h"

bool href_list_add(lList **this_list, lList **answer_list, const char *host_or_group);

bool href_list_remove_existing(lList **this_list, lList **answer_list, ocs::tom::ConstList<ocs::tom::HR> list);
OCS_TRANSITIONAL bool href_list_remove_existing(lList **this_list, lList **answer_list, lList *list);

bool href_list_has_member(ocs::tom::ConstList<ocs::tom::HR> this_list, const char *host_or_group);
OCS_TRANSITIONAL bool href_list_has_member(const lList *this_list, const char *host_or_group);

bool href_list_append_to_dstring(ocs::tom::ConstList<ocs::tom::HR> this_list, dstring *string);
OCS_TRANSITIONAL bool href_list_append_to_dstring(const lList *this_list, dstring *string);

bool href_list_compare(ocs::tom::ConstList<ocs::tom::HR> this_list, lList **answer_list,
                       ocs::tom::ConstList<ocs::tom::HR> list, lList **add_hosts, lList **add_groups,
                       lList **equity_hosts, lList **equity_groups);
OCS_TRANSITIONAL bool href_list_compare(const lList *this_list, lList **answer_list, const lList *list,
                                        lList **add_hosts, lList **add_groups, lList **equity_hosts,
                                        lList **equity_groups);

bool href_list_find_diff(ocs::tom::ConstList<ocs::tom::HR> this_list, lList **answer_list,
                         ocs::tom::ConstList<ocs::tom::HR> list, lList **add_hosts, lList **rem_hosts,
                         lList **add_groups, lList **rem_groups);
OCS_TRANSITIONAL bool href_list_find_diff(const lList *this_list, lList **answer_list, const lList *list,
                                          lList **add_hosts, lList **rem_hosts, lList **add_groups,
                                          lList **rem_groups);

bool href_list_find_effective_diff(lList **answer_list, ocs::tom::ConstList<ocs::tom::HR> add_groups,
                                   ocs::tom::ConstList<ocs::tom::HR> rem_groups,
                                   ocs::tom::ConstList<ocs::tom::HGRP> master_list, lList **add_hosts,
                                   lList **rem_hosts);
OCS_TRANSITIONAL bool href_list_find_effective_diff(lList **answer_list, const lList *add_groups,
                                                    const lList *rem_groups, const lList *master_list,
                                                    lList **add_hosts, lList **rem_hosts);

bool href_list_find_references(ocs::tom::ConstList<ocs::tom::HR> this_list, lList **answer_list,
                               ocs::tom::ConstList<ocs::tom::HGRP> master_list, lList **used_hosts,
                               lList **used_groups, lList **used_matchers = nullptr, bool expand_matchers = true);

bool href_list_find_all_references(ocs::tom::ConstList<ocs::tom::HR> this_list, lList **answer_list,
                                   ocs::tom::ConstList<ocs::tom::HGRP> master_list, lList **used_hosts,
                                   lList **used_groups, lList **used_matchers = nullptr,
                                   bool expand_matchers = true);
OCS_TRANSITIONAL bool href_list_find_all_references(const lList *this_list, lList **answer_list,
                                                    const lList *master_list, lList **used_hosts,
                                                    lList **used_groups, lList **used_matchers = nullptr,
                                                    bool expand_matchers = true);

bool href_list_find_referencees(ocs::tom::ConstList<ocs::tom::HR> this_list, lList **answer_list,
                                ocs::tom::ConstList<ocs::tom::HGRP> master_list, lList **occupant_groups);

bool href_list_find_all_referencees(ocs::tom::ConstList<ocs::tom::HR> this_list, lList **answer_list,
                                    ocs::tom::ConstList<ocs::tom::HGRP> master_list, lList **occupant_groups);

ocs::tom::Element<ocs::tom::HR> href_list_locate(ocs::tom::ConstList<ocs::tom::HR> this_list, const char *name);
OCS_TRANSITIONAL lListElem *href_list_locate(const lList *this_list, const char *name);

bool href_list_resolve_hostnames(ocs::tom::List<ocs::tom::HR> this_list, lList **answer_list, bool ignore_error,
                                 bool introducing, ocs::tom::ConstElement<ocs::tom::HGRP> hgroup = nullptr);
OCS_TRANSITIONAL bool href_list_resolve_hostnames(lList *this_list, lList **answer_list, bool ignore_error,
                                                  bool introducing, const lListElem *hgroup = nullptr);

void href_list_debug_print(ocs::tom::ConstList<ocs::tom::HR> this_list, const char *prefix);
OCS_TRANSITIONAL void href_list_debug_print(const lList *this_list, const char *prefix);

void href_list_make_uniq(ocs::tom::List<ocs::tom::HR> this_list, lList **answer_list);
OCS_TRANSITIONAL void href_list_make_uniq(lList *this_list, lList **answer_list);
