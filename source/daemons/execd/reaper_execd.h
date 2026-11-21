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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Reaping shepherds and turning what they left behind into a job report
 *
 * When a shepherd exits, the daemon reads the files it left in the job's
 * `active_jobs` directory - usage, exit status, error, and the
 * `shepherd_about_to_exit` marker - and builds the report qmaster gets. A
 * missing marker means the shepherd died rather than finished, and the job is
 * reported as failed even if the job itself ran.
 *
 * This is also where jobs left behind by a daemon restart are cleaned up.
 */

#include "tom/ocs_Types.h"

#include "sgeobj/sge_daemonize.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

int sge_reap_children_execd(int max_count, bool is_qmaster_down);
ocs::tom::Element<ocs::tom::JR>
execd_job_start_failure(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                        ocs::tom::ConstElement<ocs::tom::PET> petep, const char *error_string, int general);
ocs::tom::Element<ocs::tom::JR>
execd_job_run_failure(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                      ocs::tom::ConstElement<ocs::tom::PET> petep, const char *error_string, int general);
void job_unknown(uint32_t jobid, uint32_t jataskid, char *qname);
bool clean_up_old_jobs(bool startup);
void remove_acked_job_exit(uint32_t job_id, uint32_t ja_task_id, const char *pe_task_id,
                           ocs::tom::Element<ocs::tom::JR> jr);
void reaper_sendmail(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::JR> jr);

void execd_slave_job_exit(uint32_t job_id, uint32_t ja_task_id);
int count_master_tasks(ocs::tom::ConstList<ocs::tom::JB> lp, uint32_t job_id);
void set_enforce_cleanup_old_jobs();

void simulated_job_exit(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> jatep,
                        uint32_t sig = 0);
