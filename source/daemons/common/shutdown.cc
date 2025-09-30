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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <string>
#include <filesystem>
#include <fstream>

#include "uti/ocs_Systemd.h"
#include "uti/sge_arch.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_feature.h"

#include "shutdown.h"

#include <uti/sge_string.h>

#include "msg_daemons_common.h"

void starting_up()
{
   DENTER(TOP_LAYER);

   // switch to the INFO logging level, we want to see the startup messages regardless of the configured log level
   u_long32 old_ll = log_state_get_log_level();
   log_state_set_log_level(LOG_INFO);
   
   DSTRING_STATIC(ds1, 256);
   feature_get_product_name(FS_VERSION, &ds1);
   const char *security_mode = bootstrap_get_security_mode();
   if (sge_strnullcasecmp(NONE_STR, security_mode) != 0) {
      sge_dstring_sprintf_append(&ds1, " (%s)", security_mode);
   }
   DSTRING_STATIC(ds2, 256);
   INFO(MSG_STARTUP_STARTINGUP_SSS, feature_get_product_name(FS_SHORT, &ds2), sge_dstring_get_string(&ds1), sge_get_arch());


   // log if we are using Munge
   if (bootstrap_has_security_mode(BS_SEC_MODE_MUNGE)) {
      INFO(SFNMAX, MSG_STARTUP_USING_MUNGE);
   }

   // if we are running within a cgroup then output the cgroup path
#if defined (LINUX)
   std::string proc_cgroup = "/proc/self/cgroup";
   if (std::filesystem::exists(proc_cgroup)) {
      std::ifstream cgroup_file(proc_cgroup);
      if (cgroup_file.is_open()) {
         std::string cgroup;
         if (std::getline(cgroup_file, cgroup)) {
            INFO(MSG_STARTUP_IN_CGROUP_S, cgroup.c_str());
         }
      }
   }
#endif

   // reset the log level to the previous value
   log_state_set_log_level(old_ll);

   DRETURN_VOID;
}

/******************************************************************************/
void sge_shutdown(int i)
{
   DENTER(TOP_LAYER);

   u_long32 old_ll = log_state_get_log_level();
   DSTRING_STATIC(ds, 256);
   feature_get_product_name(FS_VERSION, &ds);
   const char *security_mode = bootstrap_get_security_mode();
   if (sge_strnullcasecmp(NONE_STR, security_mode) != 0) {
      sge_dstring_sprintf_append(&ds, " (%s)", security_mode);
   }

   log_state_set_log_level(LOG_INFO);

   if (i != 0) {
      INFO(MSG_SHADOWD_CONTROLLEDSHUTDOWN_SI, sge_dstring_get_string(&ds), i);
   } else {
      INFO(MSG_SHADOWD_CONTROLLEDSHUTDOWN_S, sge_dstring_get_string(&ds));
   }

   log_state_set_log_level(old_ll);

   sge_exit(i);
}
