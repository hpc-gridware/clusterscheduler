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
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief GDI calls specific to the execution daemon
 */

#include "cull/cull.h"

#include "gdi/ocs_gdi_Client.h"

namespace ocs::gdi {
   /// The GDI calls only an execution daemon makes
   class ClientExecd : public Client {
   public:
      /**
       * @brief Fetch the configuration with the host local values merged in
       *
       * @param[out] conf_list receives the merged configuration
       * @return 0 on success, otherwise an error code
       */
      static int gdi_get_merged_configuration(lList **conf_list);

      /**
       * @brief Block until qmaster has a configuration to hand out
       *
       * Used at execd start-up, when qmaster may not be up yet.
       *
       * @param[out] conf_list receives the configuration
       * @return 0 once a configuration was received
       */
      static int gdi_wait_for_conf(lList **conf_list);

      static int report_list_send(const lList *rlp, const char *rhost, const char *commproc, int id, int synchron);
   };
}
