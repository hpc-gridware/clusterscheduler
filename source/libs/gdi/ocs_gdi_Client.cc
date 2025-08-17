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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_profiling.h"
#include "uti/sge.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/cull/sge_permission_PERM_L.h"

#include "cull/cull.h"

#include "ocs_gdi_Client.h"
#include "ocs_gdi_Mode.h"
#include "ocs_gdi_Request.h"

#include "msg_common.h"

lList *
ocs::gdi::Client::sge_gdi(Target::TargetValue target, Command::Cmd cmd, SubCommand::SubCmd sub_cmd,
                          lList **lpp, lCondition *cp, lEnumeration *enp) {
   DENTER(GDI_LAYER);
   lList *alp = nullptr;
   Request gdi_multi{};

   PROF_START_MEASUREMENT(SGE_PROF_GDI);
   int id = gdi_multi.request(&alp, ocs::Mode::SEND, target, cmd, sub_cmd, lpp, cp, enp, true);
   if (id != -1) {
      gdi_multi.wait();
      gdi_multi.get_response(&alp, cmd, sub_cmd, target, id, lpp);
   }
   PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
   DRETURN(alp);
}

/*
** NAME
**   gdi_tsm   - trigger scheduler monitoring
** PARAMETER
**   schedd_name   - scheduler name  - ignored!
**   cell          - ignored!
** RETURN
**   answer list
** EXTERNAL
**
** DESCRIPTION
**
** NOTES
**    MT-NOTE: gdi_tsm() is MT safe (assumptions)
*/
lList *ocs::gdi::Client::gdi_tsm() {
   DENTER(GDI_LAYER);
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_SC_LIST, ocs::gdi::Command::SGE_GDI_TRIGGER,
                        ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, nullptr);
   DRETURN(alp);
}

/*
** NAME
**   gdi_kill  - send shutdown/kill request to scheduler, master, execds
** PARAMETER
**   id_list     - id list, EH_Type or EV_Type
**   cell          - cell, ignored!!!
**   option_flags  - 0
**   action_flag   - combination of MASTER_KILL, SCHEDD_KILL, EXECD_KILL,
**                                       JOB_KILL
** RETURN
**   answer list
** EXTERNAL
**
** DESCRIPTION
**
** NOTES
**    MT-NOTE: gdi_kill() is MT safe (assumptions)
*/
lList *ocs::gdi::Client::gdi_kill(lList *id_list, u_long32 action_flag) {
   DENTER(GDI_LAYER);
   bool id_list_created = false;
   lList *alp = lCreateList("answer", AN_Type);

   if (action_flag & MASTER_KILL) {
      lList *tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_MASTER_EVENT, ocs::gdi::Command::SGE_GDI_TRIGGER,
                              ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if (action_flag & SCHEDD_KILL) {
      char buffer[10];

      snprintf(buffer, sizeof(buffer), "%d", EV_ID_SCHEDD);
      id_list = lCreateList("kill scheduler", ID_Type);
      id_list_created = true;
      lAddElemStr(&id_list, ID_str, buffer, ID_Type);
      lList *tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EV_LIST, ocs::gdi::Command::SGE_GDI_TRIGGER,
                              ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &id_list, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if (action_flag & THREAD_START) {
      lList *tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_DUMMY_LIST, ocs::gdi::Command::SGE_GDI_TRIGGER,
                              ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &id_list, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if (action_flag & EVENTCLIENT_KILL) {
      if (id_list == nullptr) {
         char buffer[10];
         snprintf(buffer, sizeof(buffer), "%d", EV_ID_ANY);
         id_list = lCreateList("kill all event clients", ID_Type);
         id_list_created = true;
         lAddElemStr(&id_list, ID_str, buffer, ID_Type);
      }
      lList *tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EV_LIST, ocs::gdi::Command::SGE_GDI_TRIGGER,
                              ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &id_list, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if ((action_flag & EXECD_KILL) || (action_flag & JOB_KILL)) {
      lListElem *hlep;
      const lListElem *hep;
      lList *hlp = nullptr;
      if (id_list != nullptr) {
         /*
         ** we have to convert the EH_Type to ID_Type
         ** It would be better to change the call to use ID_Type!
         */
         for_each_ep(hep, id_list) {
            hlep = lAddElemStr(&hlp, ID_str, lGetHost(hep, EH_name), ID_Type);
            lSetUlong(hlep, ID_force, (action_flag & JOB_KILL) ? 1 : 0);
         }
      } else {
         hlp = lCreateList("kill all hosts", ID_Type);
         hlep = lCreateElem(ID_Type);
         lSetString(hlep, ID_str, nullptr);
         lSetUlong(hlep, ID_force, (action_flag & JOB_KILL) ? 1 : 0);
         lAppendElem(hlp, hlep);
      }
      lList *tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_TRIGGER,
                              ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &hlp, nullptr, nullptr);
      lAddList(alp, &tmpalp);
      lFreeList(&hlp);
   }

   if (id_list_created) {
      lFreeList(&id_list);
   }

   DRETURN(alp);
}

bool
ocs::gdi::Client::sge_gdi_get_permission(lList **alpp, bool *is_manager, bool *is_operator,
                       bool *is_admin_host, bool *is_submit_host) {
   DENTER(TOP_LAYER);

   // fetch permissions for current user and host from qmaster
   lList *permission_list = nullptr;
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_DUMMY_LIST, ocs::gdi::Command::SGE_GDI_PERMCHECK,
                        ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &permission_list, nullptr, nullptr);
   if (permission_list == nullptr || lGetNumberOfElem(permission_list) != 1) {
      answer_list_append_list(alpp, &alp);
      lFreeList(&permission_list);
      DRETURN(false);
   }

   // prepare return values
   const lListElem *perm = lFirst(permission_list);
   if (is_manager != nullptr) {
      *is_manager = lGetBool(perm, PERM_is_manager);
      DPRINTF("is_manager: %s\n", *is_manager ? "true" : "false");
   }
   if (is_operator != nullptr) {
      *is_operator = lGetBool(perm, PERM_is_operator);
      DPRINTF("is_operator: %s\n", *is_manager ? "true" : "false");
   }
   if (is_admin_host != nullptr) {
      *is_admin_host = lGetBool(perm, PERM_is_admin_host);
      DPRINTF("is_admin_host: %s\n", *is_admin_host ? "true" : "false");
   }
   if (is_submit_host != nullptr) {
      *is_submit_host = lGetBool(perm, PERM_is_submit_host);
      DPRINTF("is_submit_host: %s\n", *is_submit_host ? "true" : "false");
   }

   lFreeList(&permission_list);
   lFreeList(&alp);
   DRETURN(true);
}



/*-------------------------------------------------------------------------*
 * NAME
 *   get_configuration - retrieves configuration from qmaster
 * PARAMETER
 *   config_name       - name of local configuration or "global",
 *                       name is being resolved before action
 *   gepp              - pointer to list element containing global
 *                       configuration, CONF_Type, should point to nullptr
 *                       or otherwise will be freed
 *   lepp              - pointer to list element containing local configuration
 *                       by name given by config_name, can be nullptr if global
 *                       configuration is requested, CONF_Type, should point
 *                       to nullptr or otherwise will be freed
 * RETURN
 *    0   on success
 *   -1   nullptr pointer received
 *   -2   error resolving host
 *   -3   invalid nullptr pointer received for local configuration
 *   -4   request to qmaster failed
 *   -5   there is no global configuration
 *   -6   endpoint not unique
 *   -7   no permission to get configuration
 *   -8   access denied error on commlib layer
 * EXTERNAL
 *
 * DESCRIPTION
 *   retrieves a configuration from the qmaster. If the configuration
 *   "global" is requested, then this function requests only this one.
 *   If not, both the global configuration and the requested local
 *   configuration are retrieved.
 *   This function was introduced to make execution hosts independent
 *   of being able to mount the local_conf directory.
 *-------------------------------------------------------------------------*/
int
ocs::gdi::Client::gdi_get_configuration(const char *config_name, lListElem **gepp, lListElem **lepp) {
   DENTER(GDI_LAYER);
   lCondition *where;
   lEnumeration *what;
   lList *alp = nullptr;
   lList *lp = nullptr;
   u_long32 is_global_requested = 0;
   int ret;
   lListElem *hep = nullptr;
   int success;
   static int already_logged = 0;
   u_long32 status;
   u_long32 me = component_get_component_id();

   if (config_name == nullptr || gepp == nullptr) {
      DRETURN(-1);
   }

   /* free elements referenced in gepp and lepp - we will overwrite them */
   if (*gepp != nullptr) {
      lFreeElem(gepp);
   }
   if (lepp != nullptr && *lepp) {
      lFreeElem(lepp);
   }

   /* resolve hostname, unless the global config is requested */
   if (!strcasecmp(config_name, "global")) {
      is_global_requested = 1;
   } else {
      hep = lCreateElem(EH_Type);
      lSetHost(hep, EH_name, config_name);

      ret = sge_resolve_host(hep, EH_name);

      if (ret != CL_RETVAL_OK) {
         DPRINTF("get_configuration: error %d resolving host %s: %s\n", ret, config_name, cl_get_error_text(ret));
         lFreeElem(&hep);
         ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, config_name);
         DRETURN(-2);
      }
      DPRINTF("get_configuration: unique for %s: %s\n", config_name, lGetHost(hep, EH_name));

      if (ocs::gdi::ClientBase::sge_get_com_error_flag(me, ocs::gdi::SGE_COM_ACCESS_DENIED, false)) {
         lFreeElem(&hep);
         DRETURN(-8);
      }
      if (ocs::gdi::ClientBase::sge_get_com_error_flag(me, ocs::gdi::SGE_COM_ENDPOINT_NOT_UNIQUE, false)) {
         lFreeElem(&hep);
         DRETURN(-6);
      }
   }

   if (is_global_requested == 0 && !lepp) {
      ERROR(SFNMAX, MSG_NULLPOINTER);
      lFreeElem(&hep);
      DRETURN(-3);
   }

   /* query configuration from sge_qmaster via gdi request */
   if (is_global_requested != 0) {
      where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
      DPRINTF("requesting global\n");
   } else {
      where = lWhere("%T(%I c= %s || %I h= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME, CONF_name, lGetHost(hep, EH_name));
      DPRINTF("requesting global and %s\n", lGetHost(hep, EH_name));
   }
   what = lWhat("%T(ALL)", CONF_Type);
   alp = sge_gdi(Target::SGE_CONF_LIST, Command::SGE_GDI_GET, SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);

   lFreeWhat(&what);
   lFreeWhere(&where);

   /* in case the gdi request failed: error handling & return */
   success = ((status = lGetUlong(lFirst(alp), AN_status)) == STATUS_OK);
   if (!success) {
      if (!already_logged) {
         ERROR(SFN, lGetString(lFirst(alp), AN_text));
         already_logged = 1;
      }

      lFreeList(&alp);
      lFreeList(&lp);
      lFreeElem(&hep);
      DRETURN((status != STATUS_EDENIED2HOST) ? -4 : -7);
   }
   lFreeList(&alp);

   /* we didn't get the correct number of configurations? */
   if (lGetNumberOfElem(lp) > (2 - is_global_requested)) {
      WARNING(MSG_CONF_REQCONF_II, (int) (2 - is_global_requested), lGetNumberOfElem(lp));
   }

   /* we did not get the global configuration? */
   if (!(*gepp = lGetElemHostRW(lp, CONF_name, SGE_GLOBAL_NAME))) {
      ERROR(SFNMAX, MSG_CONF_NOGLOBAL);
      lFreeList(&lp);
      lFreeElem(&hep);
      DRETURN(-5);
   }
   lDechainElem(lp, *gepp);

   /* if we requested the local configuration but there is none,
    * print a warning
    */
   if (is_global_requested == 0) {
      if (!(*lepp = lGetElemHostRW(lp, CONF_name, lGetHost(hep, EH_name)))) {
         if (*gepp) {
            INFO(MSG_CONF_NOLOCAL_S, lGetHost(hep, EH_name));
         }
         lFreeList(&lp);
         lFreeElem(&hep);
         already_logged = 0;
         DRETURN(0);
      }
      lDechainElem(lp, *lepp);
   }

   lFreeElem(&hep);
   lFreeList(&lp);
   already_logged = 0;
   DRETURN(0);
}

