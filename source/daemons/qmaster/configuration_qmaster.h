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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"
#include "cull/cull.h"
#include "sgeobj/sge_daemonize.h"

int
sge_read_configuration(const lListElem *aSpoolContext, lList **config_list, lList **answer_list);

lList *
sge_get_configuration(const lCondition *condition, const lEnumeration *enumeration);

lListElem *
sge_get_configuration_for_host(const char *aName);

lListElem *
sge_get_configuration_entry_by_name(const char *aHost, const char *anEntryName);

int
sge_del_configuration(ocs::GdiPacket *packet, ocs::GdiTask *task, lListElem *cxp, lList **alpp, char *ruser, char *rhost);

int
sge_mod_configuration(lListElem *aConf, lList **anAnswer, const char *aUser, const char *aHost, u_long64 gdi_session);

int
sge_compare_configuration(const lListElem *aHost, const lList *aConf);

void
sge_set_conf_reprioritize(lListElem *aConf, bool aFlag);
