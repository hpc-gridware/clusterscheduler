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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Setting up and tearing down a GDI client connection
 */

#include "uti/sge_component.h"

#include "cull/cull.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

namespace ocs::gdi {
   /// Communication errors that are remembered until a caller asks for them
   enum sge_gdi_stored_com_error_t {
      SGE_COM_ACCESS_DENIED = 101,     ///< the peer refused the connection
      SGE_COM_ENDPOINT_NOT_UNIQUE,     ///< another component is already registered under this name
      SGE_COM_WAS_COMMUNICATION_ERROR  ///< some other communication error occurred
   };

   /// Standard GDI return values
   enum ErrorValue {
      AE_ERROR = -1,      ///< the call failed
      AE_OK = 0,          ///< the call succeeded
      AE_ALREADY_SETUP,   ///< the GDI was already set up for this thread
      AE_UNKNOWN_PARAM,   ///< an argument was not understood
      AE_QMASTER_DOWN     ///< qmaster could not be reached
   };

   /**
    * @brief Finding qmaster, connecting to it, and shutting the connection down
    *
    * Adds the client side to `ClientServerBase`: locating the active
    * qmaster through the `act_qmaster` file, registering with the commlib, and
    * remembering communication errors so a caller can ask about them later.
    */
   class ClientBase : public ClientServerBase {
      // required by commlib
      static void general_communication_error(const cl_application_error_list_elem_t *commlib_error);
      static int log_flush_func(cl_raw_list_t *list_p);
   public:
      /**
       * @brief Ask whether a particular communication error has occurred
       *
       * @param progid the component asking
       * @param error_type which error to ask about
       * @param reset_error_flag true to clear the flag while reading it
       * @return true when that error has occurred since it was last cleared
       */
      static bool sge_get_com_error_flag(uint32_t progid, sge_gdi_stored_com_error_t error_type, bool reset_error_flag);

      // find qmaster
      /**
       * @brief Read the active qmaster host out of the `act_qmaster` file
       *
       * @param[out] master_host receives the host name
       * @param master_file path of the `act_qmaster` file
       * @param[out] err_str receives the reason on failure
       * @param err_str_size size of @p err_str
       * @return 0 on success, -1 on error
       */
      static int get_qm_name(char *master_host, const char *master_file, char *err_str, size_t err_str_size);
      /**
       * @brief Write the active qmaster host into the `act_qmaster` file
       *
       * @param master_host the host to record
       * @param master_file path of the `act_qmaster` file
       * @param[out] err_str receives the reason on failure
       * @param err_str_size size of @p err_str
       * @return 0 on success, -1 on error
       */
      static int write_qm_name(const char *master_host, const char *master_file, char *err_str, size_t err_str_size);
      /**
       * @brief The host qmaster currently runs on
       *
       * @param reread true to re-read `act_qmaster` rather than use the cached
       *        value — needed after a failover
       * @return the host name; owned by the GDI, do not free
       */
      static const char *gdi_get_act_master_host(bool reread);
      /**
       * @brief Is qmaster reachable?
       *
       * @param[out] answer_list receives the reason when it is not
       * @return CL_RETVAL_OK when qmaster answered
       */
      static int gdi_is_alive(lList **answer_list);

      // connect to qmaster
      /**
       * @brief Register this component with the commlib
       *
       * @param[out] answer_list receives the reason on failure
       * @return CL_RETVAL_OK on success
       */
      static int prepare_enroll(lList **answer_list);
      /**
       * @brief Set the GDI up for the calling thread, without connecting
       *
       * @param component_id which program this is
       * @param thread_id which thread of it
       * @param[out] answer_list receives the reason on failure
       * @param is_qmaster_intern_client true for a thread inside qmaster, which
       *        takes the internal path instead of the commlib
       * @return #AE_OK, or #AE_ALREADY_SETUP when this thread was already set up
       */
      static ErrorValue setup(ProgName component_id, ThreadName thread_id, lList **answer_list, bool is_qmaster_intern_client);
      /**
       * @brief Set the GDI up and register with the commlib in one call
       *
       * @param component_id which program this is
       * @param thread_id which thread of it
       * @param[out] answer_list receives the reason on failure
       * @param display_name name to register under, or nullptr for the default
       * @return #AE_OK on success
       */
      static ErrorValue setup_and_enroll(ProgName component_id, ThreadName thread_id, lList **answer_list,
                                         const char *display_name = nullptr);
      /**
       * @brief Deregister from the commlib and release the GDI state
       * @return CL_RETVAL_OK on success
       */
      static int shutdown();
   };
}
