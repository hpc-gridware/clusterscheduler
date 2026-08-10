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
 * @brief QETI - the queue end time iterator
 *
 * To find out when a job could start, the scheduler has to know at which
 * points in time the utilization of the resources it needs drops. Those
 * points are the end times of the jobs currently holding them, and only
 * those - between two of them nothing changes, so nothing has to be checked.
 *
 * A QETI iterates exactly over those instants, in **descending** order, for
 * one job: sge_qeti_first() gives the latest one and sge_qeti_next() walks
 * backwards towards the present, which is how the scheduler finds the
 * earliest start time rather than just any.
 */

#include "sched/sge_select_queue.h"

#include "sgeobj/cull/sge_qeti_QETI_L.h"

/**
 * @brief Opaque queue end time iterator
 *
 * The struct itself is private to `sge_qeti.cc`; it holds only references
 * into the resource utilization lists that matter for one job.
 */
typedef struct sge_qeti_s sge_qeti_t;

sge_qeti_t *sge_qeti_allocate(sge_assignment_t *a);
uint64_t sge_qeti_first(sge_qeti_t *qeti);
void sge_qeti_next_before(sge_qeti_t *qeti, uint64_t start);
uint64_t sge_qeti_next(sge_qeti_t *qeti);
void sge_qeti_release(sge_qeti_t **qeti);

sge_qeti_t *sge_qeti_allocate2(lList *cr_list);
