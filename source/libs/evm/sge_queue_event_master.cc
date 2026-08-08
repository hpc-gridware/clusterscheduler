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
 * @brief Queueing events about cluster queues and queue instances
 *
 * Thin wrappers that pick the right event type and fill in the object, so
 * callers do not have to know the `sgeE_*` values.
 *
 * @see sge_queue_event_master.h
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_qinstance.h"

#include "evm/sge_event_master.h"
#include "evm/sge_queue_event_master.h"

/**
 * @brief Queue an event about one queue instance
 *
 * @param this_elem the queue instance the event is about
 * @param type the event type, one of the `sgeE_QINSTANCE_*` values
 * @param gdi_session the session the change belongs to, for read-after-write
 */
void
qinstance_add_event(lListElem *this_elem, ev_event type, uint64_t gdi_session)
{
   DENTER(TOP_LAYER);
   sge_add_event(0, type, 0, 0, lGetString(this_elem, QU_qname), lGetHost(this_elem, QU_qhostname), nullptr, this_elem, gdi_session);
   DRETURN_VOID;
}

/**
 * @brief Queue an event about one cluster queue
 *
 * @param this_elem the cluster queue the event is about
 * @param type the event type, one of the `sgeE_CQUEUE_*` values
 * @param gdi_session the session the change belongs to, for read-after-write
 */
void
cqueue_add_event(lListElem *this_elem, ev_event type, uint64_t gdi_session)
{
   DENTER(TOP_LAYER);
   sge_add_event(0, type, 0, 0, lGetString(this_elem, CQ_name), nullptr, nullptr, this_elem, gdi_session);
   DRETURN_VOID;
}
