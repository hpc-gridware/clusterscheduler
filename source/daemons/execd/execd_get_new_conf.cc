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
 * @brief Handling qmaster's notification that the configuration changed
 */

#include <cstring>
#include <optional>
#include <string>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"

#include "exec_job.h"
#include "execd_get_new_conf.h"
#include "ocs_execd_systemd.h"
#include "sge_load_sensor.h"
#include "admin_mail.h"
#include "msg_common.h"

#include <ocs_gdi_ClientExecd.h>
#include <reaper_execd.h>

#include "ocs_Topo.h"
#include "tom/ocs_PackCore.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#endif

/*
** DESCRIPTION
**   retrieves new configuration from qmaster, very similar to what is
**   executed on startup. This function is triggered by the execd
**   dispatcher table when the tag TAG_GET_NEW_CONF is received.
*/
/** @brief Handle the new configuration message from qmaster
 * @param aMsg the received message
 * @return 0 on success
 */
int do_get_new_conf(ocs::gdi::ClientServerBase::struct_msg_t *aMsg) {
   DENTER(TOP_LAYER);
   int ret;
   bool use_qidle = mconf_get_use_qidle();
   uint32_t dummy; /* always 0 */

   ocs::tom::try_get_uint32(&(aMsg->buf), &dummy);

   const std::optional<std::string> old_spool = mconf_get_execd_spool_dir();
   keep_active_t old_keep_active = mconf_get_keep_active();

   ocs::tom::ListPtr<ocs::tom::CONF> conf_list;
   ret = ocs::gdi::ClientExecd::gdi_get_merged_configuration(&conf_list);
   if (ret == 0) {
      // the configuration list goes to a process-lifetime global of the execd, which stays untyped;
      // the list it held before is freed
      ocs::tom::ListPtr<ocs::tom::CONF> old_conf_list(Execd_Config_List);
      Execd_Config_List = conf_list.release();
   }

   // Set fake topology and trigger reinit of HWLOC
#if defined(OCS_HWLOC)
   std::string topo_file = mconf_get_topology_file();
   ocs::Topo::set_fake_topo_file(topo_file);
#endif

#if defined(OCS_WITH_SYSTEMD)
   // e.g. ENABLE_SYSTEMD might have changed
   ocs::execd::execd_check_systemd_features();
#endif

   const std::optional<std::string> spool_dir = mconf_get_execd_spool_dir();
   if (old_spool != spool_dir) {
      WARNING(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, "execd_spool_dir");
   }

   // if the keep_active flag has changed,
   // and when keep_active has been switched off,
   // we need to enforce cleanup of old jobs to get rid of old active jobs directories
   keep_active_t keep_active = mconf_get_keep_active();
   if (old_keep_active != keep_active && keep_active == KEEP_ACTIVE_FALSE) {
      set_enforce_cleanup_old_jobs();
   }

   // @todo CS-1232 reset the priorities of the running jobs when reprioritization is switched off

   /*
   ** admin mail block is released on new conf
   */
   adm_mail_reset(BIT_ADM_NEW_CONF);

   sge_ls_qidle(use_qidle);
   DPRINTF("use_qidle: %d\n", use_qidle);

   DRETURN(ret);
}


