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
 * @brief Declarations for acknowledgements between the daemons
 *
 * @see sge_ack.cc
 */

#include "sgeobj/cull/sge_ack_ACK_L.h"
#include "sge_daemonize.h"

/**
 * @brief What an acknowledgement refers to
 *
 * Most messages between the daemons are acknowledged, so the sender knows it
 * may stop resending. The type says which message is being acknowledged.
 */
enum {
   ACK_JOB_EXIT,         ///< sent back by qmaster, when execd sends a job_exit
   ACK_SIGNAL_JOB,       ///< sent back by qmaster, when execd reports a job as running that was not supposed to be there
   ACK_EVENT_DELIVERY,   ///< sent back by schedd, when master sends events
   ACK_SIGJOB,           ///< sent back by execd, when qmaster signals a job
   ACK_SIGQUEUE,         ///< sent back by execd, when qmaster signals a queue
   ACK_LOAD_REPORT,      ///< sent back by qmaster, when execd sends a load report
   ACK_SIGNAL_SLAVE,     ///< sent to slave execds when the master task finished
   ACK_JOB_REPORT_RESEND ///< sent to the master execd to resend the master task finish as a trigger
};

int pack_ack(sge_pack_buffer *pb, uint32_t type, uint32_t id, uint32_t id2, const char *str);

int sge_send_ack_to_qmaster(uint32_t type, uint32_t ulong_val,
                            uint32_t ulong_val_2, const char *str, lList **alpp);
