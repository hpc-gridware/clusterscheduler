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
 * @brief Declarations of the cluster queue object
 *
 * @see sge_cqueue.cc
 */

#include <string>

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "sgeobj/sge_answer.h"

/**
 * @brief What has to happen to a queue instance after a cluster queue change
 *
 * Changing a cluster queue can create, delete or modify the queue instances
 * derived from it. Each instance is tagged with what it needs, and the tags
 * are then acted on in one pass.
 */
enum {
   SGE_QI_TAG_DEFAULT = 0,          ///< nothing to do
   SGE_QI_TAG_DEL = 1,              ///< send delete event and remove from spool area
   SGE_QI_TAG_ADD = 2,              ///< send add events and make persistent
   SGE_QI_TAG_MOD = 4,              ///< send mod event and make persistent
   SGE_QI_TAG_MOD_ONLY_CONFIG = 8   ///< send mod event but skip spooling (no state value changed!)
};

// the cluster queue attributes, their fields and verify functions: see sgeobj/ocs_cqueue_attributes.h

bool
cqueue_name_split(const char *name, dstring *cqueue_name, dstring *host_domain,
                  bool *has_hostname, bool *has_domain);

std::string cqueue_get_name_from_qinstance(const char *queue_instance);

[[nodiscard]] ocs::tom::ElementPtr<ocs::tom::CQ> cqueue_create(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                               const char *name);

/* CS-2677: the host list of a cluster queue, i.e. the member list of the host
 * group "@@<queue>" it owns. Replaces lGetList(cqueue, CQ_hostlist), which is
 * empty on every element the master list holds. */
ocs::tom::ConstList<ocs::tom::HR> cqueue_get_hostlist(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                                      ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

/* CS-2677: the client side of the above -- join the member list of "@@<queue>"
 * back onto a fetched queue, so that the configuration renders and round-trips
 * in the shape it always had. */
void cqueue_fill_hostlist(ocs::tom::Element<ocs::tom::CQ> cqueue,
                          ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

void cqueue_list_fill_hostlist(ocs::tom::List<ocs::tom::CQ> cqueue_list,
                               ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

bool cqueue_is_href_referenced(ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                               ocs::tom::ConstElement<ocs::tom::HR> href, bool only_hostlist);

bool cqueue_is_a_href_referenced(ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                 ocs::tom::ConstList<ocs::tom::HR> href_list, bool only_hostlist);

bool cqueue_set_template_attributes(ocs::tom::Element<ocs::tom::CQ> this_elem,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

ocs::tom::ConstElement<ocs::tom::QU> cqueue_locate_qinstance(ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                                             const char *hostname);

ocs::tom::Element<ocs::tom::QU> cqueue_locate_qinstance(ocs::tom::Element<ocs::tom::CQ> this_elem,
                                                        const char *hostname);

ocs::tom::ConstElement<ocs::tom::QU> cqueue_list_locate_qinstance_msg(ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                                                      const char *full_name, bool raise_error);

ocs::tom::Element<ocs::tom::QU> cqueue_list_locate_qinstance_msg(ocs::tom::List<ocs::tom::CQ> cqueue_list,
                                                                 const char *full_name, bool raise_error);

[[nodiscard]] ocs::tom::ListPtr<ocs::tom::QR>
cqueue_list_find_all_matching_references(ocs::tom::ConstList<ocs::tom::CQ> this_list,
                                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *cqueue_pattern);

[[nodiscard]] ocs::tom::ListPtr<ocs::tom::ST>
cqueue_list_find_hgroup_references(ocs::tom::ConstList<ocs::tom::CQ> this_list,
                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   ocs::tom::ConstElement<ocs::tom::HGRP> hgroup);

bool
cqueue_xattr_pre_gdi(ocs::tom::ListData *this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool cqueue_verify_attributes(ocs::tom::Element<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              ocs::tom::ConstElement<ocs::tom::CQ> reduced_elem, bool in_master,
                              ocs::tom::ConstList<ocs::tom::CAL> master_calendar_list,
                              ocs::tom::ConstList<ocs::tom::CK> master_ckpt_list,
                              ocs::tom::ConstList<ocs::tom::PE> master_pe_list,
                              ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                              ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                              ocs::tom::ConstList<ocs::tom::CE> master_centry_list,
                              ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                              ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

bool cqueue_is_used_in_subordinate(const char *cqueue_name, ocs::tom::ConstElement<ocs::tom::CQ> cqueue);

void cqueue_list_set_tag(ocs::tom::List<ocs::tom::CQ> this_list, uint32_t tag_value, bool tag_qinstances);

ocs::tom::ConstElement<ocs::tom::QU> cqueue_list_locate_qinstance(ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                                                  const char *full_name);

ocs::tom::Element<ocs::tom::QU> cqueue_list_locate_qinstance(ocs::tom::List<ocs::tom::CQ> cqueue_list,
                                                             const char *full_name);

bool cqueue_purge_host(ocs::tom::Element<ocs::tom::CQ> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       ocs::tom::ConstList<ocs::tom::US> attr_list, const char *hgroup_or_hostname);

bool cqueue_sick(ocs::tom::ConstElement<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                 ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *ds);
