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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief User sets, access lists and departments
 */

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/sge_answer.h"

int
sge_del_userset(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ConstElement<ocs::tom::US> ep,
                ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                ocs::tom::List<ocs::tom::US> userset_list, const char *ruser, const char *rhost);
OCS_TRANSITIONAL int sge_del_userset(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep,
                                     ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                     lList **userset_list, char *ruser, char *rhost);

int
sge_verify_department_entries(ocs::tom::ConstList<ocs::tom::US> userset_list,
                              ocs::tom::ConstElement<ocs::tom::US> new_userset, ocs::tom::ListPtr<ocs::tom::AN> *alpp);

bool
job_is_valid_department(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                        const char *dept_name, ocs::tom::ConstList<ocs::tom::US> userset_list);

bool
job_set_department(ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                   ocs::tom::ConstList<ocs::tom::US> userset_list);

void
userset_update_categories(ocs::tom::ConstList<ocs::tom::US> added, ocs::tom::ConstList<ocs::tom::US> removed,
                          uint64_t gdi_session);

int
userset_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
            lListElem *new_userset, lListElem *userset,
            int add, const char *ruser, const char *rhost, gdi_object_t *object,
            ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
            monitoring_t *monitor);

int
userset_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *userset,
              gdi_object_t *object);

int
userset_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
                lList **ppList, monitoring_t *monitor);
