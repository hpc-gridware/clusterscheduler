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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Collecting the scheduler's reasons for not dispatching a job
 *
 * While the scheduler tries to dispatch a job it records why each candidate
 * did not work, into a **temporary** structure. Once the outcome is known the
 * collection is either kept - schedd_mes_commit() - or thrown away -
 * schedd_mes_rollback(); a job that did get dispatched has no reasons worth
 * keeping. What survives is what `qstat -j <job_id>` prints as scheduling
 * info, and the ids of those messages are defined in `sge_schedd_text.h`.
 */

#include "cull/cull.h"
#include "sge_select_queue.h"

/** Size of the buffer one formatted scheduler message is built in */
#define MAXMSGLEN 256


/* Initialize module variables */
/* prepare tmp_sme for collecting messages */
void schedd_mes_initialize();

/* Get message structure */
lListElem *schedd_mes_obtain_package(int *global_mes_count, int *job_mes_count);

void schedd_mes_add(lList **monitor_alpp, bool monitor_next_run, uint32_t job_id, uint32_t message_number, ...);

void schedd_mes_add_join(bool monitor_next_run, uint32_t job_number, uint32_t message_number, ...);

void schedd_mes_add_global(lList **monitor_alpp, bool monitor_next_run, uint32_t message_number, ...);

void schedd_mes_set_logging(int bval);

int schedd_mes_get_logging();

void schedd_mes_commit(lList *job_list, int ignore_category, lRef jid_category);

void schedd_mes_rollback();

lList *schedd_mes_get_tmp_list();

void schedd_mes_set_tmp_list(lListElem *category, int name, uint32_t job_number);
