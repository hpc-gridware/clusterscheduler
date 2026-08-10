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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The job reports waiting to be acknowledged by qmaster
 */

#include "gdi/ocs_gdi_ClientServerBase.h"

void sge_set_flush_jr_flag(bool value);
bool sge_get_flush_jr_flag();
void flush_job_report(lListElem *jr);

lListElem *add_job_report(uint32_t jobid, uint32_t jataskid, const char *petaskid, const lListElem *jep);
lListElem *get_job_report(uint32_t jobid, uint32_t jataskid, const char *petaskid);

void del_job_report(lListElem *jr);
void cleanup_job_report(uint32_t jobid, uint32_t jataskid);
/** @brief Write the pending job reports to the trace log
 *
 * @note The definition in `job_report_execd.cc` is inside an `#if 0`, so this
 *       is declared but not built. The block sits here because there is no
 *       visible definition to attach it to.
 */
void trace_jr();

int add_usage(lListElem *jr, const char *name, const char *uval_as_str, double val);
void job_report_update_from_usage_list(lListElem *jr, const lList *usage_list);

#include "dispatcher.h"

int do_ack(ocs::gdi::ClientServerBase::struct_msg_t *aMsg);

void modify_queue_limits_flag_for_job(const char *qualified_hostname, lListElem *jep, bool increase);
bool check_for_queue_limits();
