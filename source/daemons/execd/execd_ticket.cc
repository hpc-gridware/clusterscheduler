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
 * @brief Handling the ticket values the scheduler sends for the jobs on this host
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_FreeAccess.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"

#include "dispatcher.h"
#include "execd_ticket.h"
#include "msg_execd.h"
#include "tom/ocs_PackCore.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#endif

/**
 * @brief Handle the new ticket values message from qmaster
 *
 * Called by the dispatcher: gets a list of jobid/tickets tuples and passes
 * them to the PTF.
 *
 * @param aMsg the received message
 * @return 0 on success, also when the message cannot be unpacked
 */
int
do_ticket(ocs::gdi::ClientServerBase::struct_msg_t *aMsg) {
   DENTER(TOP_LAYER);
   uint32_t jobid, jataskid;
   double ticket;
   // freed on every way out, also on the early return for a message that cannot be unpacked
   ocs::tom::ListPtr<ocs::tom::JB> ticket_modifier;


   while (ocs::tom::buffer_has_unread(&(aMsg->buf))) {
      if (ocs::tom::try_get_uint32(&(aMsg->buf), &jobid) || ocs::tom::try_get_uint32(&(aMsg->buf), &jataskid) ||
          ocs::tom::try_get_double(&(aMsg->buf), &ticket)) {
         ERROR(SFNMAX, MSG_JOB_TICKETFORMAT);
         DRETURN(0);
      }

      DPRINTF("got %lf new tickets for job " sge_u32 "." sge_u32 "\n", ticket, jobid, jataskid);

      // @todo we use only very few attributes of JB_Type and JAT_type here - use reduced elements
      if (!ticket_modifier) {
         // created with the first element, under the name lAddElemUlong() gave it
         ticket_modifier = ocs::tom::ListPtr<ocs::tom::JB>::create("ulong_sublist");
      }
      const auto job_ticket = ticket_modifier->emplace_back<JB_job_number>(jobid);
      if (job_ticket) {
         const auto task_ticket = ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(job_ticket, jataskid);
         if (task_ticket) {
            ocs::tom::set<JAT_tix>(task_ticket, ticket);
         }
      }
   }
  
   DPRINTF("got new tickets for %d jobs\n", static_cast<int>(ticket_modifier->size()));

#ifdef COMPILE_DC
   { 
      int ptf_error;
      /* forward new tickets to ptf */
      if ((ptf_error=ptf_process_job_ticket_list(*ticket_modifier))) {
         ERROR(MSG_JOB_TICKETPASS2PTF_IS, static_cast<int>(ticket_modifier->size()), ptf_errstr(ptf_error));
      }

      sge_switch2start_user();
      DPRINTF("ADJUST PRIORITIES\n");
      ptf_adjust_job_priorities();
      sge_switch2admin_user();
   }
#endif

   DRETURN(0);
}


