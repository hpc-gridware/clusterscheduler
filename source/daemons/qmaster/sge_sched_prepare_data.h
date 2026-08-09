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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The event subscription the scheduler runs with
 *
 * A scheduling run needs a lot of the cluster but not all of it, and not every
 * field of what it does need. The conditions and enumerations here are what
 * the scheduler subscribes with, so the mirror keeps exactly that much and the
 * event stream carries no more than it has to.
 *
 * The `sge_process_*_event_*` callbacks are where the scheduler reacts to a
 * change the mirror has just applied - or is about to.
 */

#include "evc/sge_event_client.h"
#include "sgeobj/sge_object.h"
#include "mir/sge_mirror.h"
#include "cull/cull.h"

/** @brief Which objects the scheduler subscribes to, and which of their fields
 *
 * Built once and reused for every subscription; ensure_valid_what_and_where()
 * rebuilds whatever is missing.
 */
typedef struct {
   lCondition *where_queue;      ///< Which queue instances are of interest
   lCondition *where_queue2;     ///< The same, for the second queue subscription
   lCondition *where_all_queue;  ///< Which queue instances go into the all-queue list
   lCondition *where_cqueue;     ///< Which cluster queues are of interest
   lCondition *where_job;        ///< Which jobs are of interest
   lCondition *where_host;       ///< Which hosts are of interest
   lCondition *where_dept;       ///< Which departments are of interest
   lCondition *where_acl;        ///< Which access control lists are of interest
   lCondition *where_jat;        ///< Which array tasks are of interest
   lCondition *where_config;     ///< Which configuration entries are of interest

   lEnumeration *what_queue;     ///< Which queue instance fields to receive
   lEnumeration *what_queue2;    ///< The same, for the second queue subscription
   lEnumeration *what_cqueue;    ///< Which cluster queue fields to receive
   lEnumeration *what_job;       ///< Which job fields to receive
   lEnumeration *what_host;      ///< Which host fields to receive
   lEnumeration *what_acldept;   ///< Which access control list and department fields to receive
   lEnumeration *what_jat;       ///< Which array task fields to receive
   lEnumeration *what_pet;       ///< Which PE task fields to receive
   lEnumeration *what_pe;        ///< Which parallel environment fields to receive
   lEnumeration *what_config;    ///< Which configuration fields to receive
} sge_where_what_t;

void 
ensure_valid_what_and_where(sge_where_what_t *where_what);

void
free_what_and_where(sge_where_what_t *where_what);

#if 0
sge_callback_result
sge_process_project_event_before(sge_evc_class_t *evc, sge_object_type type,
                                 sge_event_action action, lListElem *event, void *clientdata);
#endif

/** @brief React to a scheduler configuration change, before the mirror applies it
 *
 * @param evc the event client
 * @param type the object type the event is about
 * @param action what happened to it
 * @param event the event
 * @param clientdata the scheduler's snapshot
 * @return whether the mirror should go on to apply the event
 *
 * @warning Declared here but defined nowhere in the tree, and called from
 *          nowhere either. Kept because removing a declaration is a code
 *          change; see the dead-declaration list.
 */
sge_callback_result
sge_process_schedd_conf_event_before(sge_evc_class_t *evc, sge_object_type type,
                                     sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_schedd_conf_event_after(sge_evc_class_t *evc, sge_object_type type,
                                    sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_job_event_before(sge_evc_class_t *evc, sge_object_type type,
                             sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_job_event_after(sge_evc_class_t *evc, sge_object_type type,
                            sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_ja_task_event_after(sge_evc_class_t *evc, sge_object_type type,
                                sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_global_config_event(sge_evc_class_t *evc, sge_object_type type,
                                sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_schedd_monitor_event(sge_evc_class_t *evc, sge_object_type type,
                                 sge_event_action action, lListElem *event, void *clientdata);

sge_callback_result
sge_process_category_event_before(sge_evc_class_t *evc, sge_object_type type, sge_event_action action, lListElem *event, void *clientdata);

#if 0
sge_callback_result
sge_process_userset_event_before(sge_evc_class_t *evc, 
                                 sge_object_type type, sge_event_action action, 
                                 lListElem *event, void *clientdata);
#endif
