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
 * @brief Delivering signals to jobs, and resending the ones they ignore
 */

#include "gdi/ocs_gdi_ClientServerBase.h"

int do_signal_queue(ocs::gdi::ClientServerBase::struct_msg_t *aMsg, sge_pack_buffer *apb);

int signal_job(uint32_t jobid, uint32_t jataskid, uint32_t signal);

int sge_execd_deliver_signal(uint32_t sig, const lListElem *jep, lListElem *jatep);


int sge_kill(int pid, uint32_t sge_signal, uint32_t jobid, uint32_t jataskid,
             const char *petask);

void sge_send_suspend_mail(uint32_t signal,lListElem *master_q ,lListElem *jep, lListElem *jatep);
