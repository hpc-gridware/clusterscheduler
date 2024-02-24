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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_feature.h"

#include "gdi/sge_gdi.h"
#include "gdi/oge_gdi_client.h"

#include "comm/commlib.h"

#include "oge_qconf_parse.h"
#include "sig_handlers.h"

extern char **environ;

/************************************************************************/
int main(int argc, char **argv)
{
   lList *alp = nullptr;

   DENTER_MAIN(TOP_LAYER, "qconf");

   log_state_set_log_gui(1);
   sge_setup_sig_handlers(QCONF);
   
   if (gdi_client_setup_and_enroll(QCONF, MAIN_THREAD, &alp) != AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   if (sge_parse_qconf(++argv)) {
      sge_exit(1);
   } else {
      sge_exit(0);
   }
   DRETURN(0);
}
