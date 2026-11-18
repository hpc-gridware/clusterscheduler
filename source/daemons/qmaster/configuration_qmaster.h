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
 * @brief The global and per-host configurations, and how a host learns they changed
 */

#include <cinttypes>
#include "cull/cull.h"
#include "sgeobj/sge_daemonize.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/sge_answer.h"

int
sge_read_configuration(const lListElem *aSpoolContext, lList **config_list,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

lList *
sge_get_configuration(const lCondition *condition, const lEnumeration *enumeration);

ocs::tom::ElementPtr<ocs::tom::CONF>
sge_get_configuration_for_host(const char *aName);

ocs::tom::ElementPtr<ocs::tom::CF>
ocs_get_configuration_entry_by_name(const char *aHost, const char *anEntryName);
OCS_TRANSITIONAL lListElem *sge_get_configuration_entry_by_name(const char *aHost, const char *anEntryName);

int
sge_del_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ConstElement<ocs::tom::CONF> aConf,
                      ocs::tom::ListPtr<ocs::tom::AN> *anAnswer, char *aUser, char *aHost);
OCS_TRANSITIONAL int sge_del_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *cxp,
                                           ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost);

int
sge_mod_configuration(ocs::tom::Element<ocs::tom::CONF> aConf, ocs::tom::ListPtr<ocs::tom::AN> *anAnswer,
                      const char *aUser, const char *aHost, uint64_t gdi_session);
OCS_TRANSITIONAL int sge_mod_configuration(lListElem *aConf, ocs::tom::ListPtr<ocs::tom::AN> *anAnswer,
                                           const char *aUser, const char *aHost, uint64_t gdi_session);

int
sge_compare_configuration(ocs::tom::ConstElement<ocs::tom::EH> aHost, ocs::tom::ConstList<ocs::tom::CONF> aConf);
OCS_TRANSITIONAL int sge_compare_configuration(const lListElem *aHost, const lList *aConf);

void
sge_update_heard_from_timeout();
