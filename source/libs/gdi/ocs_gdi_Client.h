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

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_SubCommand.h"
#include "gdi/ocs_gdi_Target.h"

/* from gdi_tsm.h */
#define MASTER_KILL       (1<<0)
#define SCHEDD_KILL       (1<<1)
#define EXECD_KILL        (1<<2)
#define JOB_KILL          (1<<3)
#define EVENTCLIENT_KILL  (1<<4)
#define THREAD_START      (1<<5)

namespace ocs::gdi {
   class Client : public ClientBase {
   public:
      static lList *sge_gdi(Target::TargetValue target, Command::Cmd cmd, SubCommand::SubCmd, lList **lpp, lCondition *cp, lEnumeration *enp);
      static lList *gdi_tsm();
      static lList *gdi_kill(lList *id_list, u_long32 action_flag);
      static bool sge_gdi_get_permission(lList **alpp, bool *is_manager, bool *is_operator, bool *is_admin_host, bool *is_submit_host);
      static int gdi_get_configuration(const char *config_name, lListElem **gepp, lListElem **lepp);
   };
}
