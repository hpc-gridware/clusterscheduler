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
#include "ocs_client_print.h"

#include <cstdio>
#include <ostream>
#include <iomanip>
#include <format>

#include "uti/sge_bitfield.h"
#include "uti/sge_rmon_macros.h"

#include "sched/sge_urgency.h"

#include "msg_clients_common.h"

static char hashes[] =
    "##############################################################################################################";


void sge_printf_header(std::ostream &os, u_long32 full_listing, u_long32 sge_ext) {
   static int first_pending = 1;
   static int first_zombie = 1;

   if ((full_listing & QSTAT_DISPLAY_PENDING) && (full_listing & QSTAT_DISPLAY_FULL)) {
      if (first_pending) {
         first_pending = 0;
         os << std::endl;
         os << "############################################################################" << (sge_ext ? hashes : "") << std::endl;
         os << MSG_QSTAT_PRT_PEDINGJOBS << std::endl;
         os << "############################################################################" << (sge_ext ? hashes : "") << std::endl;
      }
   }
   if ((full_listing & QSTAT_DISPLAY_ZOMBIES) && (full_listing & QSTAT_DISPLAY_FULL)) {
      if (first_zombie) {
         first_zombie = 0;
         os << std::endl;
         os << "############################################################################" << (sge_ext ? hashes : "") << std::endl;
         os << MSG_QSTAT_PRT_FINISHEDJOBS << std::endl;
         os << "############################################################################" << (sge_ext ? hashes : "") << std::endl;
      }
   }
}

// @todo replace with the overloaded ostream version
void sge_printf_header(u_long32 full_listing, u_long32 sge_ext) {
   std::ostringstream oss;
   sge_printf_header(oss, full_listing, sge_ext);
   printf("%s", oss.str().c_str());
}



