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
 * @brief Declarations for queue references: `cqueue`, `@hostgroup` or `cqueue@host`
 *
 * @see sge_qref.cc
 */

#include <string>

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_QR.h"

#include "sgeobj/cull/sge_qref_QR_L.h"

bool qref_list_add(lList **this_list, lList **answer_list, const char *qref_string);

void ocs_qref_list_remove_duplicates(ocs::tom::List<ocs::tom::QR> this_list);

bool qref_list_resolve(ocs::tom::ConstList<ocs::tom::QR> src_qref_list, lList **answer_list, lList **qref_list,
                       bool *found_something, ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                       ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list, bool resolve_cqueue, bool resolve_qdomain);
OCS_TRANSITIONAL bool qref_list_resolve(const lList *src_qref_list, lList **answer_list, lList **qref_list,
                                        bool *found_something, const lList *cqueue_list, const lList *hgroup_list,
                                        bool resolve_cqueue, bool resolve_qdomain);

bool qref_list_trash_some_elemts(lList **this_list, const char *full_name);

bool qref_list_is_valid(ocs::tom::ConstList<ocs::tom::QR> this_list, lList **answer_list,
                        ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                        ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                        ocs::tom::ConstList<ocs::tom::CE> master_centry_list);
OCS_TRANSITIONAL bool qref_list_is_valid(const lList *this_list, lList **answer_list, const lList *master_cqueue_list,
                                         const lList *master_hgroup_list, const lList *master_centry_list);

void qref_resolve_hostname(ocs::tom::Element<ocs::tom::QR> this_elem);

std::string ocs_qref_resolved_name(const char *name);

void qref_list_resolve_hostname(ocs::tom::List<ocs::tom::QR> this_list);
OCS_TRANSITIONAL void qref_list_resolve_hostname(lList *this_list);

bool qref_cq_rejected(const char *qref_pattern, const char *cqname, const char *hostname,
                      ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list);

bool qref_list_cq_rejected(ocs::tom::ConstList<ocs::tom::QR> qref_list, const char *cqname, const char *hostname,
                           ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list);
OCS_TRANSITIONAL bool qref_list_cq_rejected(const lList *qref_list, const char *cqname, const char *hostname,
                                            const lList *hgroup_list);

bool qref_list_eh_rejected(ocs::tom::ConstList<ocs::tom::QR> qref_list, const char *hostname,
                           ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list);

bool qref_list_host_rejected(const char *href, const char *hostname, ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list);
OCS_TRANSITIONAL bool qref_list_host_rejected(const char *href, const char *hostname, const lList *hgroup_list);

int
cull_parse_destination_identifier_list(lList **lpp, const char *dest_str);
