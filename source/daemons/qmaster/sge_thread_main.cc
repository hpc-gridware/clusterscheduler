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
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief qmaster's threads, and the state the main thread keeps about them
 */

#include <fcntl.h>

#include "uti/sge_rmon_macros.h"

#include "comm/cl_commlib.h"

#include "sge_thread_main.h"
#include "sge_thread_signaler.h"

/** @brief @copybrief main_control_t */
main_control_t Main_Control = {
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        {},
};

/** @brief Ask the signal thread to bring qmaster down
 *
 * Shutdown has to happen on the signal thread rather than wherever the
 * decision was made, so that the thread pools are torn down in order.
 *
 * @param i the exit state to end with
 * @return 0 on success
 */
int
sge_qmaster_shutdown_via_signal_thread(int i) {
   DENTER(TOP_LAYER);

   int ret = 0;

   sge_signaler_initiate_termination();
   sge_qmaster_set_exit_state(i);

   DRETURN(ret);
}

/** @brief How qmaster is going to exit
 * @return the exit state; 100 means another master took over
 */
int
sge_qmaster_get_exit_state() {
   return Main_Control.exit_state;
}

/** @brief Record how qmaster is going to exit
 * @param new_state the exit state
 */
void
sge_qmaster_set_exit_state(int new_state) {
   Main_Control.exit_state = new_state;
}

/** @brief Write everything still in memory to the spool before exiting
 *
 * The last chance for anything that was deferred - share tree usage, the job
 * number - to reach disk.
 *
 * @return true on success
 */
bool
sge_qmaster_do_final_spooling() {
   /*
    * If the exit_state is 100 than another qmaster has taken over!
    * and final spooling should not be done
    */
   return (Main_Control.exit_state == 100) ? true : false;
}

