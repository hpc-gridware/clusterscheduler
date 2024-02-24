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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"

bool
cqueue_mod_qinstances(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem,
                      bool refresh_all_values, bool is_startup, monitoring_t *monitor, const lList *master_hgroup_list,
                      lList *master_cqueue_list);

bool
cqueue_handle_qinstances(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem,
                         lList *add_hosts, lList *rem_hosts, bool refresh_all_values, monitoring_t *monitor,
                         const lList *master_hgroup_list, lList *master_cqueue_list);

void
cqueue_commit(lListElem *cqueue);

void
cqueue_rollback(lListElem *cqueue);

int
cqueue_success(lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList,
               monitoring_t *monitor);

int
cqueue_mod(lList **alpp, lListElem *modp, lListElem *ep, int add, const char *ruser,
           const char *rhost, gdi_object_t *object, int sub_command, monitoring_t *monitor);

int
cqueue_spool(lList **alpp, lListElem *this_elem, gdi_object_t *object);

int
cqueue_del(lListElem *this_elem, lList **alpp, char *ruser, char *rhost);

bool
cqueue_del_all_orphaned(lListElem *this_elem, lList **answer_list, const char *ehname);

bool
cqueue_list_del_all_orphaned(lList *this_list, lList **answer_list, const char *cqname,
                             const char *ehname);

void
cqueue_list_set_unknown_state(lList *this_list, const char *hostname,
                              bool send_events, bool is_unknown);

void cqueue_diff_projects(const lListElem *new_cqueue, const lListElem *old_cqueue, lList **new_prj, lList **old_prj);

void cqueue_diff_usersets(const lListElem *new_cqueue, const lListElem *old_cqueue, lList **new_acl, lList **old_acl);
