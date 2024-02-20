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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <fcntl.h>

#include "uti/sge_rmon_macros.h"

#include "comm/cl_commlib.h"

#include "sge_thread_main.h"
#include "sge_thread_signaler.h"

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
        nullptr
};

int
sge_qmaster_shutdown_via_signal_thread(int i) {
   int ret = 0;

   DENTER(TOP_LAYER);

   sge_signaler_initiate_termination();
   sge_qmaster_set_exit_state(i);

   DRETURN(ret);
}

int
sge_qmaster_get_exit_state() {
   return Main_Control.exit_state;
}

void
sge_qmaster_set_exit_state(int new_state) {
   Main_Control.exit_state = new_state;
}

bool
sge_qmaster_do_final_spooling() {
   /*
    * If the exit_state is 100 than another qmaster has taken over!
    * and final spooling should not be done
    */
   return (Main_Control.exit_state == 100) ? true : false;
}

