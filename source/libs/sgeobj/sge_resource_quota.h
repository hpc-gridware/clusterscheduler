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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations for resource quota sets: limits that cut across the object model
 *
 * @see sge_resource_quota.cc
 */

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "sgeobj/cull/sge_resource_quota_RQS_L.h"
#include "sgeobj/cull/sge_resource_quota_RQR_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRF_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRL_L.h"
#include "sgeobj/cull/sge_resource_quota_RQL_L.h"

/// The five things a resource quota rule can filter on
enum {
   FILTER_USERS = 0, ///< the submitting user
   FILTER_PROJECTS,  ///< the job's project
   FILTER_PES,       ///< the job's parallel environment
   FILTER_QUEUES,    ///< the queue the job would run in
   FILTER_HOSTS      ///< the host the job would run on
};

/**
 * @brief Values found in `RQR_level`
 *
 * How far a rule's limit is spread: one shared limit, or one limit per host,
 * per cluster queue or per queue instance.
 */
enum {
   RQR_ALL = 0,  ///< one limit shared by everything the rule matches
   RQR_GLOBAL,   ///< one limit for the whole cluster
   RQR_CQUEUE,   ///< one limit per cluster queue
   RQR_HOST,     ///< one limit per host
   RQR_QUEUEI    ///< one limit per queue instance
};

ocs::tom::ElementPtr<ocs::tom::RQRF> rqs_parse_filter_from_string(const char *buffer, lList **alp);
OCS_TRANSITIONAL bool rqs_parse_filter_from_string(lListElem **filter, const char *buffer, lList **alp);

bool rqs_append_filter_to_dstring(ocs::tom::ConstElement<ocs::tom::RQRF> filter, dstring *buffer, lList **alp);
OCS_TRANSITIONAL bool rqs_append_filter_to_dstring(const lListElem *filter, dstring *buffer, lList **alp);

ocs::tom::Element<ocs::tom::RQS> rqs_set_defaults(ocs::tom::Element<ocs::tom::RQS> rqs);

bool rqs_verify_attributes(ocs::tom::Element<ocs::tom::RQS> rqs, lList **answer_list, bool in_master,
                           ocs::tom::ConstList<ocs::tom::CE> master_centry_list);
OCS_TRANSITIONAL bool rqs_verify_attributes(lListElem *rqs, lList **answer_list, bool in_master,
                                            const lList *master_centry_list);

ocs::tom::Element<ocs::tom::RQS> rqs_list_locate(ocs::tom::ConstList<ocs::tom::RQS> lp, const char *name);
OCS_TRANSITIONAL lListElem *rqs_list_locate(lList *lp, const char *name);

ocs::tom::Element<ocs::tom::RQR> rqs_rule_locate(ocs::tom::List<ocs::tom::RQR> lp, const char *name);

bool rqs_xattr_pre_gdi(lList *this_list, lList **answer_list);

bool rqs_get_rue_string(dstring *name, ocs::tom::ConstElement<ocs::tom::RQR> rule, const char *user,
                        const char *project, const char *host, const char *queue, const char *pe);

int rqs_debit_rule_usage(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::PE> pe,
                         ocs::tom::Element<ocs::tom::RQR> rule, dstring *rue_name, int slots,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list, const char *obj_name, bool is_master_task,
                         bool do_per_host_booking);

int rqs_debit_consumable(ocs::tom::Element<ocs::tom::RQS> rqs, ocs::tom::ConstElement<ocs::tom::JB> job,
                         ocs::tom::ConstElement<ocs::tom::JG> granted, ocs::tom::ConstElement<ocs::tom::PE> pe,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::ConstList<ocs::tom::US> acl_list,
                         ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list, int slots, bool is_master_task,
                         bool do_per_host_booking);
OCS_TRANSITIONAL int rqs_debit_consumable(lListElem *rqs, const lListElem *job, const lListElem *granted,
                                          const lListElem *pe, const lList *centry_list, const lList *acl_list,
                                          const lList *hgrp_list, int slots, bool is_master_task,
                                          bool do_per_host_booking);

ocs::tom::Element<ocs::tom::RQR> rqs_get_matching_rule(ocs::tom::ConstElement<ocs::tom::RQS> rqs, const char *user,
                                                       const char *group,
                                                       ocs::tom::ConstList<ocs::tom::ST> grp_list,
                                                       const char *project, const char *pe, const char *host,
                                                       const char *queue,
                                                       ocs::tom::ConstList<ocs::tom::US> userset_list,
                                                       ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list,
                                                       dstring *rule_name);

bool rqs_is_matching_rule(ocs::tom::ConstElement<ocs::tom::RQR> rule, const char *user, const char *group,
                          ocs::tom::ConstList<ocs::tom::ST> grp_list, const char *project, const char *pe,
                          const char *host, const char *queue, ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                          ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);
OCS_TRANSITIONAL bool rqs_is_matching_rule(lListElem *rule, const char *user, const char *group, const lList *grp_list,
                                           const char *project, const char *pe, const char *host, const char *queue,
                                           const lList *master_userset_list, const lList *master_hgroup_list);

bool sge_centry_referenced_in_rqs(ocs::tom::ConstElement<ocs::tom::RQS> rqs,
                                  ocs::tom::ConstElement<ocs::tom::CE> centry);

bool rqs_filter_match(ocs::tom::ConstElement<ocs::tom::RQRF> filter, int filter_type, const char *value,
                      ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                      ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, const char *group,
                      ocs::tom::ConstList<ocs::tom::ST> grp_list);
OCS_TRANSITIONAL bool rqs_filter_match(const lListElem *filter, int filter_type, const char *value,
                                       const lList *master_userset_list, const lList *master_hgroup_list,
                                       const char *group, const lList *grp_list);
