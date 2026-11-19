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
 * @brief GDI calls specific to the execution daemon
 */

#include <tuple>
#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_profiling.h"
#include "uti/sge_time.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "ocs_gdi_ClientExecd.h"
#include "msg_common.h"
#include "msg_gdilib.h"
#include "uti/ocs_Topo.h"

/* TODO: move this function to execd */
/**
 * @brief Block until qmaster has a configuration to hand out
 *
 * Used at execd start-up, when qmaster may not be up yet.
 *
 * @param[out] conf_list receives the configuration; a list it holds is freed first. Untouched on failure.
 * @return 0 once a configuration was received, -1 if the endpoint is not unique or the permission is missing
 */
int ocs::gdi::ClientExecd::gdi_wait_for_conf(ocs::tom::ListPtr<ocs::tom::CONF> *conf_list) {
   DENTER(GDI_LAYER);
   static uint64_t last_qmaster_file_read = 0;

   /*
    * for better performance retrieve 2 configurations
    * in one gdi call
    */
   const char *qualified_hostname = component_get_qualified_hostname();
   DPRINTF("qualified hostname: %s\n", qualified_hostname);

   uint32_t progid = component_get_component_id();
   // the configurations received, replaced on every try
   ocs::tom::ElementPtr<ocs::tom::CONF> global;
   ocs::tom::ElementPtr<ocs::tom::CONF> local;
   int ret;
   while ((std::tie(ret, global, local) = gdi_get_configuration(qualified_hostname, true), ret != 0)) {
      if (ret == -6 || ret == -7) {
         /* confict: endpoint not unique or no permission to get config */
         DRETURN(-1);
      }

      if (ret == -8) {
         /* access denied */
         sge_get_com_error_flag(progid, SGE_COM_ACCESS_DENIED, true);
         sleep(30);
      }

      DTRACE;
      cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
      int ret_val = cl_commlib_trigger(handle, 1);
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

      uint64_t now = sge_get_gmt64();
      if (now - last_qmaster_file_read >= sge_gmt32_to_gmt64(30)) {
         gdi_get_act_master_host(true);
         DPRINTF("re-read actual qmaster file\n");
         last_qmaster_file_read = now;
      }
   }

   const char *cell_root = bootstrap_get_cell_root();
   ret = merge_configuration(nullptr, progid, cell_root, *global, *local).first;
   if (ret) {
      DPRINTF("Error %d merging configuration \"%s\"\n", ret, qualified_hostname);
   }

   sge_show_conf();

   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   ocs::tom::set<CONF_entries>(global, nullptr);
   conf_list->reset();
   auto confs = ocs::tom::ListPtr<ocs::tom::CONF>::create("config list");
   confs->push_back(std::move(global));
   // a host without a local configuration gets none
   if (local) {
      ocs::tom::set<CONF_entries>(local, nullptr);
      confs->push_back(std::move(local));
   }
   *conf_list = std::move(confs);
   DRETURN(0);
}

/**
 * @brief Fetch the configuration with the host local values merged in
 *
 * Requests a new configuration set from the qmaster.
 *
 * @param[out] conf_list receives the merged configuration; a list it holds is freed first. Untouched on failure.
 * @return 0 on success, -1 if the configuration could not be fetched from qmaster, -2 if the global and the
 *         local configuration could not be merged
 */
int ocs::gdi::ClientExecd::gdi_get_merged_configuration(ocs::tom::ListPtr<ocs::tom::CONF> *conf_list) {
   DENTER(GDI_LAYER);

   const char *qualified_hostname = component_get_qualified_hostname();
   const char *cell_root = bootstrap_get_cell_root();
   uint32_t progid = component_get_component_id();

   DPRINTF("qualified hostname: %s\n", qualified_hostname);
   auto [ret, global, local] = ocs::gdi::Client::gdi_get_configuration(qualified_hostname, true);
   if (ret) {
      ERROR(MSG_CONF_NOREADCONF_IS, ret, qualified_hostname);
      DRETURN(-1);
   }

   ret = merge_configuration(nullptr, progid, cell_root, *global, *local).first;
   if (ret) {
      ERROR(MSG_CONF_NOMERGECONF_IS, ret, qualified_hostname);
      DRETURN(-2);
   }

   sge_show_conf();

   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   ocs::tom::set<CONF_entries>(global, nullptr);

   conf_list->reset();
   auto confs = ocs::tom::ListPtr<ocs::tom::CONF>::create("config list");
   confs->push_back(std::move(global));
   // a host without a local configuration gets none, as lAppendElem() refused the null element
   if (local) {
      ocs::tom::set<CONF_entries>(local, nullptr);
      confs->push_back(std::move(local));
   }
   *conf_list = std::move(confs);

   DRETURN(0);
}



/**
 * @brief Send a list of reports
 *
 * Send a list of reports.
 *
 * @param rlp the reports
 * @param rhost Hostname
 * @param commproc Component name
 * @param id Component id
 * @param synchron true or false
 *
 * @return error state 0 - OK -1 - Unexpected error -2 - No memory -3 - Format error other - see
 *         sge_send_any_request()
 *
 * @note MT-NOTE: report_list_send() is not MT safe (assumptions)
 */
int ocs::gdi::ClientExecd::report_list_send(const ocs::tom::ConstList<ocs::tom::REP> rlp, const char *rhost,
                                            const char *commproc, const int id, const int synchron) {
   DENTER(TOP_LAYER);

   sge_pack_buffer pb;
   int ret;
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   /* prepare packing buffer */
   if ((ret = init_packbuffer(&pb, 1024)) == PACK_SUCCESS) {
      try {
         rlp.pack(pb);
      } catch (const ocs::tom::PackException &e) {
         // the code cull_pack_list() returned
         ret = e.code();
      }
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

