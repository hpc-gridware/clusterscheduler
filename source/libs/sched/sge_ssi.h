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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief SSI - the simple scheduler interface
 *
 * A very small interface for custom schedulers: start a job, cancel a job.
 * A custom scheduler is built on the event client or the event mirror
 * interface, watches the pending jobs, and uses these two calls to act on
 * them. It was created to make integrating the MAUI scheduler into Cluster
 * Scheduler easier.
 */

#include "evc/sge_event_client.h"



/**
 * @brief One host of a job's layout: how many tasks run there
 *
 * A job can be spread over any number of hosts with any number of tasks per
 * host, so the layout is passed as an **array** of these. The array is
 * terminated by an entry whose `procs` is 0.
 */
typedef struct {
   int procs;              ///< Number of tasks on this host, 0 marks the end of the array
   const char *host_name;  ///< Name of the host
} task_map;


bool sge_ssi_job_start(sge_evc_class_t *evc, const char *job_identifier, const char *pe, task_map tasks[]);
bool sge_ssi_job_cancel(sge_evc_class_t *evc, const char *job_identifier, bool reschedule); 
