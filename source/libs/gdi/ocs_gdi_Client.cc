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
 * @brief The client side entry points of the GDI
 */

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
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_PERM.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "ocs_gdi_Client.h"
#include "ocs_gdi_Mode.h"
#include "ocs_gdi_Request.h"

#include "msg_common.h"

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
ocs::tom::ListPtr<ocs::tom::AN>
ocs::gdi::Client::sge_gdi(Target target, Command cmd, SubCommand sub_cmd,
                          lList **lpp, lCondition *cp, lEnumeration *enp) {
   DENTER(GDI_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   Request gdi_multi{};

   PROF_START_MEASUREMENT(SGE_PROF_GDI);
   int id = gdi_multi.request(&alp, Mode::SEND, target, cmd, sub_cmd, lpp, cp, enp, true);
   if (id != -1) {
      gdi_multi.wait();
      gdi_multi.get_response(&alp, cmd, sub_cmd, target, id, lpp);
   }
   PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
   DRETURN(alp);
}

/**
 * @brief Trigger a scheduling run (scheduler monitoring)
 * @return the answer list; the caller owns it
 *
 * @note MT-NOTE: gdi_tsm() is MT safe (assumptions)
 */
ocs::tom::ListPtr<ocs::tom::AN> ocs::gdi::Client::gdi_tsm() {
   DENTER(GDI_LAYER);
   ocs::tom::ListPtr<ocs::tom::SC> no_data;
   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SC_LIST, ocs::gdi::Command::TRIGGER,
                                        ocs::gdi::SubCommand::NONE, no_data);
   DRETURN(alp);
}

/**
 * @brief Shut components down, or start a qmaster thread
 *
 * Sends the shutdown/kill requests to scheduler, master and execds.
 *
 * The id list stays untyped: its type depends on the action (EH_Type for
 * #EXECD_KILL and #JOB_KILL, ID_Type for the event clients and the thread start).
 *
 * @param id_list which instances to act on, e.g. host names; nullptr for all
 * @param action_flag a combination of #MASTER_KILL, #SCHEDD_KILL, #EXECD_KILL, #JOB_KILL and their neighbours
 * @return the answer list; the caller owns it
 *
 * @note MT-NOTE: gdi_kill() is MT safe (assumptions)
 */
ocs::tom::ListPtr<ocs::tom::AN> ocs::gdi::Client::gdi_kill(lList *id_list, uint32_t action_flag) {
   DENTER(GDI_LAYER);
   bool id_list_created = false;
   auto alp = ocs::tom::ListPtr<ocs::tom::AN>::create("answer");

   if (action_flag & MASTER_KILL) {
      auto tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::MASTER_EVENT, ocs::gdi::Command::TRIGGER,
                              ocs::gdi::SubCommand::NONE, nullptr, nullptr, nullptr);
      alp->splice(std::move(tmpalp));
   }

   if (action_flag & SCHEDD_KILL) {
      char buffer[10];

      snprintf(buffer, sizeof(buffer), "%d", EV_ID_SCHEDD);
      id_list = lCreateList("kill scheduler", ID_Type);
      id_list_created = true;
      ocs::tom::emplace_back<ID_str>(&id_list, buffer);
      auto tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EV_LIST, ocs::gdi::Command::TRIGGER,
                              ocs::gdi::SubCommand::NONE, &id_list, nullptr, nullptr);
      alp->splice(std::move(tmpalp));
   }

   if (action_flag & THREAD_START) {
      auto tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::DUMMY_LIST, ocs::gdi::Command::TRIGGER,
                              ocs::gdi::SubCommand::NONE, &id_list, nullptr, nullptr);
      alp->splice(std::move(tmpalp));
   }

   if (action_flag & EVENTCLIENT_KILL) {
      if (id_list == nullptr) {
         char buffer[10];
         snprintf(buffer, sizeof(buffer), "%d", EV_ID_ANY);
         id_list = lCreateList("kill all event clients", ID_Type);
         id_list_created = true;
         ocs::tom::emplace_back<ID_str>(&id_list, buffer);
      }
      auto tmpalp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EV_LIST, ocs::gdi::Command::TRIGGER,
                              ocs::gdi::SubCommand::NONE, &id_list, nullptr, nullptr);
      alp->splice(std::move(tmpalp));
   }

   if ((action_flag & EXECD_KILL) || (action_flag & JOB_KILL)) {
      ocs::tom::ListPtr<ocs::tom::ID> hosts;
      if (id_list != nullptr) {
         /*
         ** we have to convert the EH_Type to ID_Type
         ** It would be better to change the call to use ID_Type!
         ** For these actions the caller hands in host names (EH_Type); the checked handle refuses another
         ** type, as reading EH_name from its elements did
         */
         for (const auto hep : ocs::tom::ConstList<ocs::tom::EH>(id_list)) {
            if (!hosts) {
               // created as the free emplace_back() creates a missing list
               hosts = ocs::tom::ListPtr<ocs::tom::ID>::create("");
            }
            const auto hlep = hosts->emplace_back<ID_str>(static_cast<const char *>(ocs::tom::get<EH_name>(hep)));
            ocs::tom::set<ID_force>(hlep, (action_flag & JOB_KILL) ? 1 : 0);
         }
      } else {
         hosts = ocs::tom::ListPtr<ocs::tom::ID>::create("kill all hosts");
         auto hlep = ocs::tom::ElementPtr<ocs::tom::ID>::create();
         ocs::tom::set<ID_str>(hlep, nullptr);
         ocs::tom::set<ID_force>(hlep, (action_flag & JOB_KILL) ? 1 : 0);
         hosts->push_back(std::move(hlep));
      }
      auto tmpalp = sge_gdi(Target::EH_LIST, Command::TRIGGER, SubCommand::NONE, hosts);
      alp->splice(std::move(tmpalp));
   }

   if (id_list_created) {
      lFreeList(&id_list);
   }

   DRETURN(alp);
}

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
bool
ocs::gdi::Client::sge_gdi_get_permission(ocs::tom::ListPtr<ocs::tom::AN> *alpp, bool *is_manager, bool *is_operator,
                       bool *is_admin_host, bool *is_submit_host) {
   DENTER(TOP_LAYER);

   // fetch permissions for current user and host from qmaster
   // the response holds the permissions (PERM_Type)
   ocs::tom::ListPtr<ocs::tom::PERM> permission_list;
   auto alp = sge_gdi(Target::DUMMY_LIST, Command::PERMCHECK, SubCommand::NONE, permission_list);
   if (!permission_list || permission_list->size() != 1) {
      answer_list_append_list(alpp, &alp);
      DRETURN(false);
   }

   // prepare return values
   const auto perm = permission_list->front();
   if (is_manager != nullptr) {
      *is_manager = ocs::tom::get<PERM_is_manager>(perm);
      DPRINTF("is_manager: %s\n", *is_manager ? "true" : "false");
   }
   if (is_operator != nullptr) {
      *is_operator = ocs::tom::get<PERM_is_operator>(perm);
      DPRINTF("is_operator: %s\n", *is_manager ? "true" : "false");
   }
   if (is_admin_host != nullptr) {
      *is_admin_host = ocs::tom::get<PERM_is_admin_host>(perm);
      DPRINTF("is_admin_host: %s\n", *is_admin_host ? "true" : "false");
   }
   if (is_submit_host != nullptr) {
      *is_submit_host = ocs::tom::get<PERM_is_submit_host>(perm);
      DPRINTF("is_submit_host: %s\n", *is_submit_host ? "true" : "false");
   }

   alp.reset();
   DRETURN(true);
}



/**
 * @brief Fetch the global and the host local configuration
 *
 * Retrieves a configuration from the qmaster. If the configuration
 * "global" is requested, then this function requests only this one.
 * If not, both the global configuration and the requested local
 * configuration are retrieved.
 * This function was introduced to make execution hosts independent
 * of being able to mount the local_conf directory.
 *
 * @param config_name name of the local configuration or "global"; the name is resolved before the action
 * @param with_local whether the caller takes a host local configuration; without it only "global" may be
 *        requested
 * @return a tuple of
 *         - 0 on success, -1 nullptr pointer received, -2 error resolving host, -3 a local configuration was
 *           requested without @p with_local, -4 request to qmaster failed, -5 there is no global
 *           configuration, -6 endpoint not unique, -7 no permission to get the configuration, -8 access
 *           denied error on commlib layer
 *         - the global configuration, an empty handle on an error
 *         - the host local one, an empty handle when there is none or only "global" was requested
 */
std::tuple<int, ocs::tom::ElementPtr<ocs::tom::CONF>, ocs::tom::ElementPtr<ocs::tom::CONF>>
ocs::gdi::Client::gdi_get_configuration(const char *config_name, const bool with_local) {
   DENTER(GDI_LAYER);
   static int already_logged = 0;
   ocs::tom::ElementPtr<ocs::tom::CONF> global_conf;
   ocs::tom::ElementPtr<ocs::tom::CONF> local_conf;

   if (config_name == nullptr) {
      DRETURN(std::make_tuple(-1, std::move(global_conf), std::move(local_conf)));
   }

   /* resolve hostname, unless the global config is requested */
   uint32_t is_global_requested = 0;
   ocs::tom::ElementPtr<ocs::tom::EH> hep;
   if (!strcasecmp(config_name, "global")) {
      is_global_requested = 1;
   } else {
      hep = ocs::tom::ElementPtr<ocs::tom::EH>::create();
      ocs::tom::set<EH_name>(hep, config_name);

      if (const int ret = sge_resolve_host(*hep, EH_name); ret != CL_RETVAL_OK) {
         DPRINTF("get_configuration: error %d resolving host %s: %s\n", ret, config_name, cl_get_error_text(ret));
         hep.reset();
         ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, config_name);
         DRETURN(std::make_tuple(-2, std::move(global_conf), std::move(local_conf)));
      }
      DPRINTF("get_configuration: unique for %s: %s\n", config_name,
              static_cast<const char *>(ocs::tom::get<EH_name>(hep)));

      const uint32_t me = component_get_component_id();
      if (sge_get_com_error_flag(me, ocs::gdi::SGE_COM_ACCESS_DENIED, false)) {
         DRETURN(std::make_tuple(-8, std::move(global_conf), std::move(local_conf)));
      }
      if (sge_get_com_error_flag(me, ocs::gdi::SGE_COM_ENDPOINT_NOT_UNIQUE, false)) {
         DRETURN(std::make_tuple(-6, std::move(global_conf), std::move(local_conf)));
      }
   }

   if (is_global_requested == 0 && !with_local) {
      ERROR(SFNMAX, MSG_NULLPOINTER);
      DRETURN(std::make_tuple(-3, std::move(global_conf), std::move(local_conf)));
   }

   /* query configuration from sge_qmaster via gdi request */
   ocs::tom::Where<ocs::tom::CONF> where;
   if (is_global_requested != 0) {
      where = ocs::tom::field<CONF_name>().equals_nocase(SGE_GLOBAL_NAME);
      DPRINTF("requesting global\n");
   } else {
      where = ocs::tom::field<CONF_name>().equals_nocase(SGE_GLOBAL_NAME) ||
              ocs::tom::field<CONF_name>().same_host(static_cast<const char *>(ocs::tom::get<EH_name>(hep)));
      DPRINTF("requesting global and %s\n", static_cast<const char *>(ocs::tom::get<EH_name>(hep)));
   }
   const auto what = ocs::tom::what_all<ocs::tom::CONF>();
   // the response holds the configurations (CONF_Type)
   ocs::tom::ListPtr<ocs::tom::CONF> lp;
   auto alp = sge_gdi(Target::CONF_LIST, Command::GET, SubCommand::NONE, lp, where, what);

   /* in case the gdi request failed: error handling & return */
   if (const uint32_t status = ocs::tom::get<AN_status>(lFirst(alp)); status != STATUS_OK) {
      if (!already_logged) {
         ERROR(SFN, ocs::tom::get<AN_text>(lFirst(alp)));
         already_logged = 1;
      }

      alp.reset();
      DRETURN(std::make_tuple((status != STATUS_EDENIED2HOST) ? -4 : -7, std::move(global_conf),
                              std::move(local_conf)));
   }
   alp.reset();

   /* we didn't get the correct number of configurations? */
   if (lp->size() > (2 - is_global_requested)) {
      WARNING(MSG_CONF_REQCONF_II, (int) (2 - is_global_requested), static_cast<int>(lp->size()));
   }

   /* we did not get the global configuration? */
   const auto global = lp->find_first<CONF_name>(ocs::tom::Host(SGE_GLOBAL_NAME));
   if (!global) {
      ERROR(SFNMAX, MSG_CONF_NOGLOBAL);
      DRETURN(std::make_tuple(-5, std::move(global_conf), std::move(local_conf)));
   }
   global_conf = lp->extract(global);

   /* if we requested the local configuration but there is none,
    * print a warning
    */
   if (is_global_requested == 0) {
      const char *hostname = ocs::tom::get<EH_name>(hep);
      const auto local = lp->find_first<CONF_name>(ocs::tom::Host(hostname));
      if (!local) {
         if (global_conf) {
            INFO(MSG_CONF_NOLOCAL_S, hostname);
         }
         already_logged = 0;
         DRETURN(std::make_tuple(0, std::move(global_conf), std::move(local_conf)));
      }
      local_conf = lp->extract(local);
   }

   already_logged = 0;
   DRETURN(std::make_tuple(0, std::move(global_conf), std::move(local_conf)));
}

/**
 * @brief gdi_get_configuration() for an untyped caller
 *
 * @param config_name name of the local configuration or "global"
 * @param[out] gepp receives the global configuration (CONF_Type); an element it points to is freed first
 * @param[out] lepp receives the host local one (CONF_Type), or nullptr when there is none; an element it
 *             points to is freed first; may be nullptr if the global configuration is requested
 * @return as the typed form, -3 for a local configuration requested with @p lepp nullptr
 */
int
ocs::gdi::Client::gdi_get_configuration(const char *config_name, lListElem **gepp, lListElem **lepp) {
   if (config_name == nullptr || gepp == nullptr) {
      return -1;
   }

   /* free elements referenced in gepp and lepp - we will overwrite them */
   if (*gepp != nullptr) {
      lFreeElem(gepp);
   }
   if (lepp != nullptr && *lepp) {
      lFreeElem(lepp);
   }

   auto [ret, global_conf, local_conf] = gdi_get_configuration(config_name, lepp != nullptr);
   *gepp = global_conf.release();
   if (lepp != nullptr) {
      *lepp = local_conf.release();
   }
   return ret;
}

