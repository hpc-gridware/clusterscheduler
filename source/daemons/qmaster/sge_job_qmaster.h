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

#include "uti/sge_monitor.h"

#include "sgeobj/sge_event.h"
#include "sgeobj/sge_event.h"

#include "sgeobj/sge_daemonize.h"
#include "gdi/sge_gdi_packet.h"

#include "sge_qmaster_timed_event.h"

typedef enum {
   JL_UNKNOWN = 0,   /* job is in unknown state - should never be seen */
   JL_PENDING,       /* job is pending */
   JL_SENT,          /* job has been sent to execd */
   JL_RESENT,        /* job has been resent to execd - sent hasn't been ack */
   JL_DELIVERED,     /* job has been delivered - execd replied with ack */
   JL_RUNNING,       /* job is running (reported by execd) */
   JL_SUSPENDED,     /* job has been suspended */
   JL_UNSUSPENDED,   /* job has been unsuspended */
   JL_HELD,          /* a hold was applied */
   JL_RELEASED,      /* all holds were released */
   JL_RESTART,       /* a restart of the job was requested */
   JL_MIGRATE,       /* a migration was requested */
   JL_DELETED,       /* the job has been deleted */
   JL_FINISHED,      /* the job has finished */
   JL_ERROR,         /* job is in error state */

   JL_ALL
} job_log_t;

const char *
get_job_log_name(job_log_t type);

int
sge_gdi_add_job(lListElem **jep, lList **alpp, lList **lpp,
                sge_gdi_packet_class_t *packet, ocs::GdiTask *task, monitoring_t *monitor);

int
sge_gdi_copy_job(lListElem *jep, lList **alpp, lList **lpp,
                 sge_gdi_packet_class_t *packet, ocs::GdiTask *task, monitoring_t *monitor);

int
sge_gdi_mod_job(const sge_gdi_packet_class_t *packet, ocs::GdiTask *task, lListElem *jep, lList **alpp, int sub_command);

int
sge_gdi_del_job(const sge_gdi_packet_class_t *packet, ocs::GdiTask *task, lListElem *jep, lList **alpp, int sub_command, monitoring_t *monitor);

void
sge_add_job_event(ev_event type, lListElem *jep, lListElem *jatep, u_long64 gdi_request);

bool
is_pe_master_task_send(lListElem *jatep);

bool
all_slave_jobs_finished(lListElem *jatep);

void
tag_all_host_gdil(lListElem *jatep);

void
ack_all_slaves(u_long32 job_id, u_long32 ja_task_id, const lListElem *ja_task, u_long32 type);

void
sge_add_jatask_event(ev_event type, lListElem *jep, lListElem *jatask, u_long64 gdi_request);

void
job_suc_pre(lListElem *jep);

void
job_suc_pre_ad(lListElem *jep);

void
sge_init_job_number();

void
sge_store_job_number(te_event_t anEvent, monitoring_t *monitor);

void
job_ja_task_send_abort_mail(const lListElem *job, const lListElem *ja_task, const char *ruser, const char *rhost,
                            const char *err_str);

void
get_rid_of_job_due_to_qdel(lListElem *j, lListElem *t, lList **answer_list, const char *ruser,
                           int force, monitoring_t *monitor, u_long64 gdi_session);

void
job_mark_job_as_deleted(lListElem *j, lListElem *t);

bool
spool_write_script(lList **answer_list, u_long32 jobid, const lListElem *jep);

bool
spool_delete_script(lList **answer_list, u_long32 jobid, lListElem *jep);

bool
spool_read_script(lList **answer_list, u_long32 jobid, lListElem *jep);

u_long32
sge_get_job_number(monitoring_t *monitor);

int
deny_soft_consumables(lList **alpp, const lList *srl, const lList *master_centry_list);

int
job_verify_project(const lListElem *job, lList **alpp, const char *user, const char *group, const lList *grp_list);

int
job_verify_predecessors(lListElem *job, lList **alpp);

int
verify_suitable_queues(lList **alpp, lListElem *jep, int *trigger, bool is_modify);

int
job_verify_predecessors_ad(lListElem *job, lList **alpp, u_long64 gdi_session);
