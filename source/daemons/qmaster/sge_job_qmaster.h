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
 * @brief Submitting, modifying and deleting jobs
 */

#include <cstddef>

#include "uti/sge_monitor.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"

#include "sgeobj/sge_event.h"

#include "sgeobj/sge_daemonize.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "gdi/ocs_gdi_Packet.h"

#include "sge_qmaster_timed_event.h"
#include "sgeobj/sge_answer.h"

/** @brief The points in a job's life that are worth recording
 *
 * Each one becomes a job log record in the reporting file, so this is the
 * vocabulary an accounting or auditing tool reads a job's history in.
 */
typedef enum {
   JL_UNKNOWN = 0,   ///< job is in unknown state - should never be seen
   JL_PENDING,       ///< job is pending
   JL_SENT,          ///< job has been sent to execd
   JL_RESENT,        ///< job has been resent to execd - sent hasn't been ack
   JL_DELIVERED,     ///< job has been delivered - execd replied with ack
   JL_RUNNING,       ///< job is running (reported by execd)
   JL_SUSPENDED,     ///< job has been suspended
   JL_UNSUSPENDED,   ///< job has been unsuspended
   JL_HELD,          ///< a hold was applied
   JL_RELEASED,      ///< all holds were released
   JL_RESTART,       ///< a restart of the job was requested
   JL_MIGRATE,       ///< a migration was requested
   JL_DELETED,       ///< the job has been deleted
   JL_FINISHED,      ///< the job has finished
   JL_ERROR,         ///< job is in error state

   JL_ALL            ///< Not a state; the number of them
} job_log_t;

const char *
get_job_log_name(job_log_t type);

int
sge_gdi_add_job(ocs::tom::ElementPtr<ocs::tom::JB> &jep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lList **lpp,
                ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

int
sge_gdi_copy_job(ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lList **lpp,
                 ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor);

int
sge_gdi_mod_job(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::JB> jep,
                ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::gdi::SubCommand sub_command);

int
sge_gdi_del_job(const ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::ID> idep,
                ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
                monitoring_t *monitor);

void
sge_add_job_event(ev_event type, ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                  uint64_t gdi_request);
void
sge_add_job_event(ev_event type, ocs::tom::Element<ocs::tom::JB> jep, std::nullptr_t, uint64_t gdi_request);

bool
is_pe_master_task_send(ocs::tom::ConstElement<ocs::tom::JAT> jatep);

bool
all_slave_jobs_finished(ocs::tom::ConstElement<ocs::tom::JAT> jatep);

void
tag_all_host_gdil(ocs::tom::Element<ocs::tom::JAT> jatep);

void
ack_all_slaves(uint32_t job_id, uint32_t ja_task_id, ocs::tom::ConstElement<ocs::tom::JAT> ja_task, uint32_t type);

void
sge_add_jatask_event(ev_event type, ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> jatask,
                     uint64_t gdi_request);

void
job_suc_pre(ocs::tom::Element<ocs::tom::JB> jep);

void
job_suc_pre_ad(ocs::tom::Element<ocs::tom::JB> jep);

void
sge_init_job_number();

void
sge_store_job_number(te_event_t anEvent, monitoring_t *monitor);

void
job_ja_task_send_abort_mail(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                            const char *ruser, const char *rhost, const char *err_str);

void
get_rid_of_job_due_to_qdel(ocs::tom::Element<ocs::tom::JB> j, ocs::tom::Element<ocs::tom::JAT> t,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const char *ruser, int force, monitoring_t *monitor, uint64_t gdi_session);

void
job_mark_job_as_deleted(ocs::tom::Element<ocs::tom::JB> j, ocs::tom::Element<ocs::tom::JAT> t);

void
job_set_deleted_by(ocs::tom::Element<ocs::tom::JAT> ja_task, const char *user, const char *host);

bool
spool_write_script(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t jobid,
                   ocs::tom::ConstElement<ocs::tom::JB> jep);

bool
spool_delete_script(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t jobid, ocs::tom::Element<ocs::tom::JB> jep);

bool
spool_read_script(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t jobid, ocs::tom::Element<ocs::tom::JB> jep);

uint32_t
sge_get_job_number(monitoring_t *monitor);

int
deny_soft_consumables(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ConstList<ocs::tom::CE> srl,
                      ocs::tom::ConstList<ocs::tom::CE> master_centry_list);

int
job_verify_project(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *user,
                   const char *group, ocs::tom::ConstList<ocs::tom::ST> grp_list);

int
job_verify_predecessors(ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp);

int
verify_suitable_queues(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::JB> jep, int *trigger,
                       bool is_modify);

int
job_verify_predecessors_ad(ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                           uint64_t gdi_session);
