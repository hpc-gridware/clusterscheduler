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

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"

/* funtions called from within gdi framework in qmaster */
int
pe_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_pe, lListElem *pe, int add, const char *ruser,
       const char *rhost, gdi_object_t *object,
       ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_command,
       monitoring_t *monitor);

int
pe_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *pep, gdi_object_t *object);

int
pe_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList,
           monitoring_t *monitor);

/* funtions called via gdi and inside the qmaster */
int
sge_del_pe(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *, lList **, char *, char *);

/* to do at qmasters startup */
void
debit_all_jobs_from_pes(lList *pe_list);

void
debit_job_from_pe(lListElem *pep, int slots, u_long32 job_id);

void
reverse_job_from_pe(lListElem *pep, int slots, u_long32 job_id);

void
pe_diff_usersets(const lListElem *new_pe, const lListElem *old, lList **new_acl, lList **old_acl);
