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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The client side entry points of the GDI
 */

#include "cull/cull.h"

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_Target.h"

/* from gdi_tsm.h */
#define MASTER_KILL       (1<<0) ///< `gdi_kill()` target: shut the qmaster down
#define SCHEDD_KILL       (1<<1) ///< `gdi_kill()` target: shut the scheduler down
#define EXECD_KILL        (1<<2) ///< `gdi_kill()` target: shut execution daemons down
#define JOB_KILL          (1<<3) ///< `gdi_kill()` target: kill jobs
#define EVENTCLIENT_KILL  (1<<4) ///< `gdi_kill()` target: shut event clients down
#define THREAD_START      (1<<5) ///< `gdi_kill()` target: start a qmaster thread instead of killing

namespace ocs::gdi {
   /**
    * @brief The GDI calls an ordinary client makes
    *
    * Everything here goes to qmaster and waits for the answer.
    */
   class Client : public ClientBase {
   public:
      /**
       * @brief Send a single-task GDI request and wait for the answer
       *
       * @param target which object list to act on
       * @param cmd what to do
       * @param sub_cmd modifiers refining @p cmd
       * @param lpp the objects to send; receives the ones read back
       * @param cp which objects to act on, from `lWhere()`
       * @param enp which fields to transfer, from `lWhat()`
       * @return the answer list; the caller owns it
       */
      static lList *sge_gdi(Target target, Command cmd, SubCommand, lList **lpp, lCondition *cp, lEnumeration *enp);
      /**
       * @brief Trigger a scheduling run
       * @return the answer list; the caller owns it
       */
      static lList *gdi_tsm();
      /**
       * @brief Shut components down, or start a qmaster thread
       *
       * @param id_list which instances to act on, e.g. host names; nullptr for all
       * @param action_flag a combination of #MASTER_KILL and its neighbours
       * @return the answer list; the caller owns it
       */
      static lList *gdi_kill(lList *id_list, uint32_t action_flag);
      /**
       * @brief Ask qmaster which permissions the caller has
       *
       * @param[out] alpp receives the reason on failure
       * @param[out] is_manager true when the user is a cluster manager
       * @param[out] is_operator true when the user is a cluster operator
       * @param[out] is_admin_host true when the call comes from an admin host
       * @param[out] is_submit_host true when the call comes from a submit host
       * @return true when the permissions could be determined
       */
      static bool sge_gdi_get_permission(lList **alpp, bool *is_manager, bool *is_operator, bool *is_admin_host, bool *is_submit_host);
      /**
       * @brief Fetch the global and the host local configuration
       *
       * @param config_name host whose local configuration is wanted
       * @param[out] gepp receives the global configuration
       * @param[out] lepp receives the host local one, or nullptr when there is none
       * @return 0 on success, otherwise an error code
       */
      static int gdi_get_configuration(const char *config_name, lListElem **gepp, lListElem **lepp);
   };
}
