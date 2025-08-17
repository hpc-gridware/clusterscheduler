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
 *   Copyright: 2009 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"
#include "sge_sl.h"

struct sge_tq_queue_t {
   /*
    * List that stores tasks.
    * Mutex of the list will be used to secure also this structure
    */
   sge_sl_list_t *list;

   /* Condition to signal waiting threads */
   pthread_cond_t cond;

   /* Waiting threads */
   u_long32 waiting;
};

enum sge_tq_type_t {
   SGE_TQ_UNKNOWN = 0,

   SGE_TQ_GDI_PACKET,    /* GDI packets */

   SGE_TQ_TYPE1,  /* used for module tests */
   SGE_TQ_TYPE2   /* used for module tests */
};

struct sge_tq_task_t {
   sge_tq_type_t type;
   void *data;
};

bool
sge_tq_create(sge_tq_queue_t **queue);

void
sge_tq_destroy(sge_tq_queue_t **queue, sge_sl_destroy_f destroy_func = nullptr);

bool
sge_tq_task_destroy(sge_tq_task_t **task);

u_long32
sge_tq_get_task_count(sge_tq_queue_t *queue);

u_long32
sge_tq_get_waiting_count(sge_tq_queue_t *queue);

bool
sge_tq_store_notify(sge_tq_queue_t *queue, sge_tq_type_t type, void *data);

void
sge_tq_wakeup_waiting(sge_tq_queue_t *queue);

bool
sge_tq_wait_for_task(sge_tq_queue_t *queue, int seconds, sge_tq_type_t type, void **data);

int
sge_tq_move_from_to_if(sge_tq_queue_t *src, sge_tq_queue_t *dst, sge_sl_compare_f cmp_func);
