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
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <pthread.h>

#include <pwd.h>

#include <unistd.h>

#include "comm/commlib.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/cull/sge_multi_MA_L.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/cull/sge_permission_PERM_L.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_data.h"
#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_Client.h"
#include "gdi/sge_security.h"
#include "gdi/msg_gdilib.h"

#include "basis_types.h"
#include "msg_common.h"
#include "uti/sge.h"

#include <ocs_gdi_ClientServerBase.h>

#ifdef KERBEROS
#  include "krb_lib.h"
#endif



int gdi_wait_for_conf(lList **conf_list) {
   lListElem *global = nullptr;
   lListElem *local = nullptr;
   int ret_val;
   int ret;
   static u_long64 last_qmaster_file_read = 0;
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();

   /* TODO: move this function to execd */
   DENTER(GDI_LAYER);
   /*
    * for better performance retrieve 2 configurations
    * in one gdi call
    */
   DPRINTF("qualified hostname: %s\n", qualified_hostname);

   while ((ret = ocs::gdi::Client::gdi_get_configuration(qualified_hostname, &global, &local))) {
      if (ret == -6 || ret == -7) {
         /* confict: endpoint not unique or no permission to get config */
         DRETURN(-1);
      }

      if (ret == -8) {
         /* access denied */
         ocs::gdi::ClientBase::sge_get_com_error_flag(progid, ocs::gdi::SGE_COM_ACCESS_DENIED, true);
         sleep(30);
      }

      DTRACE;
      cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
      ret_val = cl_commlib_trigger(handle, 1);
      switch (ret_val) {
         case CL_RETVAL_SELECT_TIMEOUT:
            sleep(1);  /* If we could not establish the connection */
            break;
         case CL_RETVAL_OK:
            break;
         default:
            sleep(1);  /* for other errors */
            break;
      }

      u_long64 now = sge_get_gmt64();
      if (now - last_qmaster_file_read >= sge_gmt32_to_gmt64(30)) {
         ocs::gdi::ClientBase::gdi_get_act_master_host(true);
         DPRINTF("re-read actual qmaster file\n");
         last_qmaster_file_read = now;
      }
   }

   ret = merge_configuration(nullptr, progid, cell_root, global, local, nullptr);
   if (ret) {
      DPRINTF("Error %d merging configuration \"%s\"\n", ret, qualified_hostname);
   }

   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   lSetList(global, CONF_entries, nullptr);
   lSetList(local, CONF_entries, nullptr);
   lFreeList(conf_list);
   *conf_list = lCreateList("config list", CONF_Type);
   lAppendElem(*conf_list, global);
   lAppendElem(*conf_list, local);
   DRETURN(0);
}

/*-------------------------------------------------------------------------*
 * NAME
 *   get_merged_conf - requests new configuration set from master
 * RETURN
 *   -1      - could not get configuration from qmaster
 *   -2      - could not merge global and local configuration
 * EXTERNAL
 *
 *-------------------------------------------------------------------------*/
int gdi_get_merged_configuration(lList **conf_list) {
   lListElem *global = nullptr;
   lListElem *local = nullptr;
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();
   int ret;

   DENTER(GDI_LAYER);

   DPRINTF("qualified hostname: %s\n", qualified_hostname);
   ret = ocs::gdi::Client::gdi_get_configuration(qualified_hostname, &global, &local);
   if (ret) {
      ERROR(MSG_CONF_NOREADCONF_IS, ret, qualified_hostname);
      lFreeElem(&global);
      lFreeElem(&local);
      DRETURN(-1);
   }

   ret = merge_configuration(nullptr, progid, cell_root, global, local, nullptr);
   if (ret) {
      ERROR(MSG_CONF_NOMERGECONF_IS, ret, qualified_hostname);
      lFreeElem(&global);
      lFreeElem(&local);
      DRETURN(-2);
   }
   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   lSetList(global, CONF_entries, nullptr);
   lSetList(local, CONF_entries, nullptr);

   lFreeList(conf_list);
   *conf_list = lCreateList("config list", CONF_Type);
   lAppendElem(*conf_list, global);
   lAppendElem(*conf_list, local);

   DRETURN(0);
}



/****** sgeobj/sge_report/report_list_send() ******************************************
*  NAME
*     report_list_send() -- Send a list of reports.
*
*  SYNOPSIS
*     int report_list_send(const lList *rlp, const char *rhost,
*                          const char *commproc, int id,
*                          int synchron, u_long32 *mid)
*
*  FUNCTION
*     Send a list of reports.
*
*  INPUTS
*     const lList *rlp     - REP_Type list
*     const char *rhost    - Hostname
*     const char *commproc - Component name
*     int id               - Component id
*     int synchron         - true or false
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Unexpected error
*        -2 - No memory
*        -3 - Format error
*        other - see sge_send_any_request()
*
*  NOTES
*     MT-NOTE: report_list_send() is not MT safe (assumptions)
*******************************************************************************/
int report_list_send(const lList *rlp, const char *rhost, const char *commproc, int id, int synchron) {
   sge_pack_buffer pb;
   int ret;
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

   /* prepare packing buffer */
   if ((ret = init_packbuffer(&pb, 1024, 0)) == PACK_SUCCESS) {
      ret = cull_pack_list(&pb, rlp);
   }

   switch (ret) {
      case PACK_SUCCESS:
         break;

      case PACK_ENOMEM:
         ERROR(MSG_GDI_REPORTNOMEMORY_I, 1024);
         clear_packbuffer(&pb);
         DRETURN(-2);

      case PACK_FORMAT:
         ERROR(SFNMAX, MSG_GDI_REPORTFORMATERROR);
         clear_packbuffer(&pb);
         DRETURN(-3);

      default:
         ERROR(SFNMAX, MSG_GDI_REPORTUNKNOWERROR);
         clear_packbuffer(&pb);
         DRETURN(-1);
   }

   ret = ocs::gdi::ClientServerBase::sge_gdi_send_any_request(synchron, nullptr, rhost, commproc, id, &pb,
                                                          ocs::gdi::ClientServerBase::TAG_REPORT_REQUEST, 0, &alp);

   clear_packbuffer(&pb);
   answer_list_output(&alp);

   DRETURN(ret);
}

