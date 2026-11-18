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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Checkpointing environments
 */

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/sge_answer.h"

int
ckpt_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
         ocs::tom::Element<ocs::tom::CK> new_ckpt, ocs::tom::ConstElement<ocs::tom::CK> ckpt, int add,
         const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
         ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

int
ckpt_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
           ocs::tom::ConstElement<ocs::tom::CK> ep, gdi_object_t *object);

int
ckpt_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::CK> ep,
             ocs::tom::ConstElement<ocs::tom::CK> old_ep, gdi_object_t *object, lList **ppList,
             monitoring_t *monitor);

int
sge_del_ckpt(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ConstElement<ocs::tom::CK> ep,
             ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost);

const char *
get_checkpoint_when(int bitmask);

int
is_checkpoint_when_valid(int bitmask);
