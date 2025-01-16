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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

namespace ocs::gdi {
   enum sge_gdi_stored_com_error_t {
      SGE_COM_ACCESS_DENIED = 101,
      SGE_COM_ENDPOINT_NOT_UNIQUE,
      SGE_COM_WAS_COMMUNICATION_ERROR
   };

   /* these values are standard gdi return values */
   enum ErrorValue {
      AE_ERROR = -1,
      AE_OK = 0,
      AE_ALREADY_SETUP,
      AE_UNKNOWN_PARAM,
      AE_QMASTER_DOWN
   };

   class ClientBase : public ClientServerBase {
      // required by commlib
      static void general_communication_error(const cl_application_error_list_elem_t *commlib_error);
      static int log_flush_func(cl_raw_list_t *list_p);
   public:
      static bool sge_get_com_error_flag(u_long32 progid, sge_gdi_stored_com_error_t error_type, bool reset_error_flag);

      // find qmaster
      static int get_qm_name(char *master_host, const char *master_file, char *err_str, size_t err_str_size);
      static int write_qm_name(const char *master_host, const char *master_file, char *err_str, size_t err_str_size);
      static const char *gdi_get_act_master_host(bool reread);
      static int gdi_is_alive(lList **answer_list);

      // connect to qmaster
      static int prepare_enroll(lList **answer_list);
      static ErrorValue setup(int component_id, u_long32 thread_id, lList **answer_list, bool is_qmaster_intern_client);
      static ErrorValue setup_and_enroll(int component_id, u_long32 thread_id, lList **answer_list);
      static int shutdown();
   };
}
