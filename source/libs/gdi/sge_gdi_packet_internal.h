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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "gdi/sge_gdi_packet_type.h"
#include "uti/sge_tq.h"

extern sge_tq_queue_t *GlobalRequestQueue;
extern sge_tq_queue_t *ReaderRequestQueue;
extern sge_tq_queue_t *ReaderWaitingRequestQueue;

void
sge_gdi_packet_wait_till_handled(sge_gdi_packet_class_t *packet);

void
sge_gdi_packet_broadcast_that_handled(sge_gdi_packet_class_t *packet);

bool
sge_gdi_packet_is_handled(sge_gdi_packet_class_t *packet);

bool
sge_gdi_packet_execute_external(lList **answer_list, sge_gdi_packet_class_t *packet);

bool
sge_gdi_packet_execute_internal(lList **answer_list, sge_gdi_packet_class_t *packet);

void
sge_gdi_packet_wait_for_result_external(sge_gdi_packet_class_t **packet_handle, lList **malpp);

void
sge_gdi_packet_wait_for_result_internal(sge_gdi_packet_class_t **packet_handle, lList **malpp);
