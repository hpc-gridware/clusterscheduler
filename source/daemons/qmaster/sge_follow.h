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
 * @brief Following the scheduler's orders and booking the usage they imply
 */

#include "uti/sge_monitor.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_daemonize.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_RTIC.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sge_qmaster_timed_event.h"

int
sge_follow_order(ocs::tom::Element<ocs::tom::OR> ep, char *ruser, char *rhost, ocs::tom::ListPtr<ocs::tom::RTIC> *topp,
                 monitoring_t *monitor, uint64_t gdi_session);

int
distribute_ticket_orders(ocs::tom::ConstList<ocs::tom::RTIC> ticket_orders, monitoring_t *monitor);

void
set_next_stree_spooling_time();
