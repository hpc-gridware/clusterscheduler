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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_c_gdi.h"
#include "sge_qmaster_timed_event.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"


int
calendar_mod(ocs::GdiPacket *packet, ocs::GdiTask *task, lList **alpp, lListElem *new_cal, lListElem *cep, int add, const char *ruser,
             const char *rhost, gdi_object_t *object, ocs::GdiCommand::Command cmd, ocs::GdiSubCommand::SubCommand sub_command, monitoring_t *monitor);

int
calendar_spool(ocs::GdiPacket *packet, ocs::GdiTask *task, lList **alpp, lListElem *cep, gdi_object_t *object);

int
calendar_update_queue_states(ocs::GdiPacket *packet, ocs::GdiTask *task, lListElem *cep, lListElem *old_cep, gdi_object_t *object,
                             lList **ppList, monitoring_t *monitor);

int
sge_del_calendar(ocs::GdiPacket *packet, ocs::GdiTask *task, lListElem *ep, lList **alpp, char *ruser, char *rhost);

void
sge_calendar_event_handler(te_event_t anEvent, monitoring_t *monitor);

void
calendar_initalize_timer(monitoring_t *monitor);
