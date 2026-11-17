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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The global and per-host configurations, and how a host learns they changed
 */
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <pwd.h>

#include "uti/config_file.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "comm/commlib.h"

#include "spool/sge_spooling.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/ocs_AutoAddHosts.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_RequestLimits.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_List.h"
#include "tom/ocs_ListCore.h"

#include "evm/sge_event_master.h"

#include "configuration_qmaster.h"
#include "ocs_ReportingFileWriter.h"
#include "qmaster_to_execd.h"
#include "sge.h"
#include "ocs_FinishedJob.h"
#include "ocs_SharetreeUsage.h"
#include "sge_persistence_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "reschedule.h"
#include "msg_common.h"
#include "msg_qmaster.h"

/* Static configuration entries may be changed at runtime with a warning */
static const char *Static_Conf_Entries[] = {"execd_spool_dir", nullptr};


static int
check_config(lList **alpp, ocs::tom::ConstElement<ocs::tom::CONF> conf);

static bool
check_config_script_path(lList **alpp, const char *name, const char *path, const char **allowed);

static int
do_mod_config(char *aConfName, ocs::tom::ConstElement<ocs::tom::CONF> anOldConf,
              ocs::tom::Element<ocs::tom::CONF> aNewConf, lList **anAnswer, uint64_t gdi_session);

static int
check_static_conf_entries(ocs::tom::ConstList<ocs::tom::CF> theOldConfEntries,
                          ocs::tom::ConstList<ocs::tom::CF> theNewConfEntries, lList **anAnswer);

static int
exchange_conf_by_name(char *aConfName, ocs::tom::ConstElement<ocs::tom::CONF> anOldConf,
                      ocs::tom::Element<ocs::tom::CONF> aNewConf, lList **anAnswer, uint64_t gdi_session);

static bool
has_reschedule_unknown_change(ocs::tom::ConstList<ocs::tom::CF> theOldConfEntries,
                              ocs::tom::ConstList<ocs::tom::CF> theNewConfEntries);

static int
do_add_config(char *aConfName, ocs::tom::ConstElement<ocs::tom::CONF> aConf, lList **anAnswer,
              uint64_t gdi_session);

static int
remove_conf_by_name(char *aConfName);

static bool
conf_exists_by_name(const char *aConfName);

static ocs::tom::ElementPtr<ocs::tom::CF>
get_entry_from_conf(ocs::tom::ConstElement<ocs::tom::CONF> aConf, const char *anEntryName);

static uint32_t
sge_get_config_version_for_host(const char *aName);

static void
invalidate_load_report_intervals(const char *conf_name);

static void
notify_execds_about_conf_change(const char *conf_name);

/** @brief Read the cluster configuration from the spool at startup
 *
 * Read the cluster configuration from secondary storage using 'aSpoolContext'.
 * This is the bootstrap function for the configuration module. It does populate
 * the list with the cluster configuration.
 *
 * @param aSpoolContext the spooling context to read through
 * @param config_list receives the configurations; the process-lifetime master list, so it stays raw
 * @param answer_list receives messages for the caller
 * @return 0 on success, -1 if there is no global configuration or merging failed
 */
int
sge_read_configuration(const lListElem *aSpoolContext, lList **config_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   // read the spooled list
   spool_read_list(answer_list, aSpoolContext, config_list, SGE_TYPE_CONFIG);
   // the spooling layer reads CONF_Type elements for SGE_TYPE_CONFIG
   const auto configs = ocs::tom::List<ocs::tom::CONF>::from_typed(*config_list);

   // add potentially missing attributes, in this order
   const auto spooled_global = ocs::tom::find_first_rw<CONF_name>(configs, "global");
   if (spooled_global) {
      const ocs::tom::ConstList<ocs::tom::CF> entries = ocs::tom::get<CONF_entries>(spooled_global);
      const std::pair<const char *, const char *> defaults[] = {
         {"jsv_url", NONE_STR},
         {"jsv_allowed_mod", JSV_ALLOWED_MOD_DEFAULT},
         {"gdi_request_limits", NONE_STR},
         {"binding_params", BINDING_PARAMS_DEFAULT},
         {"jsv_params", JSV_PARAMS_DEFAULT},
         {"topology_file", NONE_STR},
      };
      for (const auto &[name, value] : defaults) {
         if (!ocs::tom::find_first<CF_name>(entries, name)) {
            const auto entry = ocs::tom::emplace_back_in<CONF_entries, CF_name>(spooled_global, name);
            ocs::tom::set<CF_value>(entry, value);
         }
      }
   }

   // configurations spooled by an older version, or by an installer which wrote them in a
   // different order, are brought into the defined order here - the spool file itself is
   // rewritten with the next modification of the configuration
   for (const auto conf : configs) {
      conf_sort_entries(conf);
   }
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   answer_list_output(answer_list);

   const char *qualified_hostname = component_get_qualified_hostname();
   DPRINTF("qualified_hostname: '%s'\n", qualified_hostname);
   const auto local = sge_get_configuration_for_host(qualified_hostname);
   if (!local) {
      /* write a warning into messages file, if no local config exists*/
      WARNING(MSG_CONFIG_NOLOCAL_S, qualified_hostname);
   }

   const auto global = sge_get_configuration_for_host(SGE_GLOBAL_NAME);
   if (!global) {
      ERROR(SFNMAX, MSG_CONFIG_NOGLOBAL);
      DRETURN(-1);
   }

   const uint32_t progid = component_get_component_id();
   const char *cell_root = bootstrap_get_cell_root();
   const int ret = merge_configuration(answer_list, progid, cell_root, *global, *local).first;
   answer_list_output(answer_list);

   if (0 != ret) {
      ERROR(MSG_CONFIG_ERRORXMERGINGCONFIGURATIONY_IS, ret, qualified_hostname);
      DRETURN(-1);
   }

   sge_show_conf();

   DRETURN(0);
}

/** @brief Delete a host's local configuration
 *
 * Delete configuration 'confp' from cluster configuration.
 * TODO: A fix for IZ issue #79 is needed. For this to be done it may be
 * necessary to introduce something like 'protected' configuration entries.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param aConf the configuration to delete
 * @param anAnswer receives messages for the caller
 * @param aUser the requesting user
 * @param aHost the requesting host
 * @return STATUS_OK on success, STATUS_EEXIST or STATUS_EUNKNOWN on error
 */
int
sge_del_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                      const ocs::tom::ConstElement<ocs::tom::CONF> aConf, lList **anAnswer, char *aUser,
                      char *aHost) {
   DENTER(TOP_LAYER);

   const char *tmp_name = nullptr;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;

   if (!aConf || !aUser || !aHost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((tmp_name = ocs::tom::get<CONF_name>(aConf)) == nullptr) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(CONF_name), __func__);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /*
    * Due to CR 6319231 IZ 1760:
    *    try to resolve the hostname
    *    if it is not resolvable then
    *       ignore this and use the hostname stored in the configuration obj
    *       or use the given name if no object can be found
    */
   ret = sge_resolve_hostname(tmp_name, unique_name, EH_name);
   if (ret != CL_RETVAL_OK) {
      DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), tmp_name);

      const auto conf_obj = sge_get_configuration_for_host(tmp_name);
      if (conf_obj) {
         DPRINTF("using hostname stored in configuration object\n");
         strcpy(unique_name, ocs::tom::get<CONF_name>(conf_obj));
      } else {
         ERROR(MSG_SGETEXT_CANT_DEL_CONFIG2_S, tmp_name);
         answer_list_add(anAnswer, SGE_EVENT,
                         STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   /* Do not allow to delete global configuration */
   if (!strcasecmp(SGE_GLOBAL_NAME, unique_name)) {
      ERROR(MSG_SGETEXT_CANT_DEL_CONFIG_S, unique_name);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /*
    * CS-2754: refuse a configuration that is not there, before touching the
    * spool. remove_conf_by_name() ignores whether it found anything, so this
    * used to report success for a host that never had a local configuration --
    * right after the spooling layer had complained about the missing object,
    * which made the pair read as "it went wrong, and it worked". The client hid
    * the contradiction as long as it printed only the first answer of a delete;
    * once qconf -dconf became one request for the whole name list it had to
    * print them all, and the second line was plainly wrong.
    */
   if (!conf_exists_by_name(unique_name)) {
      ERROR(MSG_SGETEXT_CANT_DEL_CONFIG2_S, unique_name);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   sge_event_spool(anAnswer, 0, sgeE_CONFIG_DEL, 0, 0, unique_name, nullptr, nullptr, nullptr, nullptr, nullptr, true,
                   true, packet->gdi_session);

   remove_conf_by_name(unique_name);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, aUser, aHost, unique_name, MSG_OBJ_CONF);
   answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   update_reschedule_unknown_timout_values(unique_name);

   /* invalidate cached configuration values */
   mconf_set_new_config(true);

   // the deleted configuration may have had the longest load_report_time
   sge_update_heard_from_timeout();

   // the sge_execd of the host uses the global configuration from now on
   invalidate_load_report_intervals(unique_name);
   notify_execds_about_conf_change(unique_name);

   DRETURN(STATUS_OK);
}

/**
 * @brief sge_del_configuration() for an untyped caller
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param aConf the configuration to delete (`CONF_Type`)
 * @param anAnswer receives messages for the caller
 * @param aUser the requesting user
 * @param aHost the requesting host
 * @return as the typed sge_del_configuration()
 */
int
sge_del_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *aConf, lList **anAnswer,
                      char *aUser, char *aHost) {
   return sge_del_configuration(packet, task, ocs::tom::ConstElement<ocs::tom::CONF>(aConf), anAnswer, aUser,
                                aHost);
}


/**
 * @brief Forget the cached load_report_time of the hosts which use a changed configuration.
 *
 * sge_qmaster caches the load_report_time of a host in EH_load_report_interval (see
 * load_report_interval()), it decides with it when a host is unheard. The cache was renewed only
 * while the new-configuration flag was set, which the load value cleanup resets every 15 seconds -
 * a host in unknown state at that moment, e.g. while its sge_execd restarts, kept the old value.
 * After the load_report_time had been raised, the host was taken as unheard too early.
 *
 * @param[in] conf_name name of the changed configuration, "global" or a host name
 */
static void
invalidate_load_report_intervals(const char *conf_name) {
   DENTER(TOP_LAYER);

   const bool is_global = strcmp(conf_name, SGE_GLOBAL_NAME) == 0;
   for (const auto hep : ocs::tom::List<ocs::tom::EH>(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST))) {
      const char *host = ocs::tom::get<EH_name>(hep);
      if (is_global || sge_hostcmp(conf_name, host) == 0) {
         ocs::tom::set<EH_load_report_interval>(hep, 0);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Notify the sge_execds which use a changed configuration.
 *
 * An sge_execd gets a changed configuration only with the answer to its next report, up to
 * one load_report_time later. Until then it works with the old one, e.g. it sends its reports
 * in the old interval while sge_qmaster already expects them in a shorter new one (CS-3095).
 * The notification makes the sge_execds fetch the configuration at once. A change of the
 * global configuration notifies all sge_execds, which all request their configuration at the
 * same time - in very large clusters qmaster_params NOTIFY_EXECDS_ON_CONF_CHANGE=false switches
 * the notification off.
 * sge_execds on hosts in unknown state and sge_execds without a connection to sge_qmaster, e.g.
 * stopped ones, are not notified, they get the configuration when they register.
 *
 * @param[in] conf_name name of the changed configuration, "global" or a host name
 */
static void
notify_execds_about_conf_change(const char *conf_name) {
   DENTER(TOP_LAYER);

   if (!mconf_get_notify_execds_on_conf_change()) {
      DRETURN_VOID;
   }

   const bool is_global = strcmp(conf_name, SGE_GLOBAL_NAME) == 0;
   for (const auto hep : ocs::tom::List<ocs::tom::EH>(*ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST))) {
      const char *host = ocs::tom::get<EH_name>(hep);
      if (strcmp(host, SGE_GLOBAL_NAME) == 0 || strcmp(host, SGE_TEMPLATE_NAME) == 0) {
         continue;
      }
      if (!is_global && sge_hostcmp(conf_name, host) != 0) {
         continue;
      }
      if (ocs::tom::get<EH_lt_heard_from>(hep) == 0) {
         continue;
      }
      // an sge_execd stopped by a signal is taken as known until it is unheard, a notification
      // would only fail to connect and make commlib log an error
      unsigned long connect_time = 0;
      cl_commlib_get_connect_time(cl_com_get_handle(to_cstr(QMASTER), 0), host, to_cstr(EXECD), 1,
                                  &connect_time);
      if (connect_time == 0) {
         DPRINTF("no connection to %s on host %s, not notified about the new configuration\n",
                 to_cstr(EXECD), host);
         continue;
      }
      // a failed notification is not an error, the sge_execd gets the configuration with the
      // answer to its next report
      host_notify_about_new_conf(hep);
   }

   DRETURN_VOID;
}

/**
 * @brief Modify cluster configuration
 *
 * Modify cluster configuration. 'confp' is a pointer to a 'CONF_Type' list
 * element and does contain the modified configuration entry. Adding a new
 * configuration entry is also viewed as a modification.
 *
 * @param aConf CONF_Type element containing the modified conf
 * @param anAnswer answer list
 * @param aUser target user
 * @param aHost target host
 * @param gdi_session the session the change belongs to
 *
 * @return STATUS_OK on success, a STATUS_* error code otherwise
 *
 * @note MT-NOTE: sge_mod_configuration() is MT safe
 */
int
sge_mod_configuration(const ocs::tom::Element<ocs::tom::CONF> aConf, lList **anAnswer, const char *aUser,
                      const char *aHost, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const char *tmp_name = nullptr;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;
   const char *cell_root = bootstrap_get_cell_root();
   const char *qualified_hostname = component_get_qualified_hostname();
   const uint32_t progid = component_get_component_id();

   if (!aConf || !aUser || !aHost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((tmp_name = ocs::tom::get<CONF_name>(aConf)) == nullptr) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(CONF_name), __func__);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((ret = sge_resolve_hostname(tmp_name, unique_name, EH_name)) != CL_RETVAL_OK) {
      DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), tmp_name);
      ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, tmp_name);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((ret = check_config(anAnswer, aConf))) {
      DRETURN(ret);
   }

   // the client sends the entries in the order they were written in the configuration file
   // (qconf -Aconf/-Mconf) or in the order the editor left them in (qconf -aconf/-mconf) -
   // bring them into the defined order before they are stored
   conf_sort_entries(aConf);

   if (auto old_conf = sge_get_configuration_for_host(unique_name); old_conf) {
      const int lret = do_mod_config(unique_name, *old_conf, aConf, anAnswer, gdi_session);
      old_conf.reset();

      if (lret == 0) {
         INFO(MSG_SGETEXT_MODIFIEDINLIST_SSSS, aUser, aHost, unique_name, MSG_OBJ_CONF);
         answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      } else {
         DRETURN(STATUS_EUNKNOWN);
      }

   } else {
      do_add_config(unique_name, aConf, anAnswer, gdi_session);

      INFO(MSG_SGETEXT_ADDEDTOLIST_SSSS, aUser, aHost, unique_name, MSG_OBJ_CONF);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   /*
   ** is the configuration change relevant for the qmaster itself?
   ** if so, initialise conf struct anew
   */
   if (strcmp(unique_name, SGE_GLOBAL_NAME) == 0 || sge_hostcmp(unique_name, qualified_hostname) == 0) {
      lList *answer_list = nullptr;
      char *qmaster_params = nullptr;

      auto local = sge_get_configuration_for_host(qualified_hostname);
      if (!local) {
         WARNING(MSG_CONFIG_NOLOCAL_S, qualified_hostname);
      }

      auto global = sge_get_configuration_for_host(SGE_GLOBAL_NAME);
      if (!global) {
         ERROR(SFNMAX, MSG_CONFIG_NOGLOBAL);
      }

      if (merge_configuration(&answer_list, progid, cell_root, *global, *local).first != 0) {
         ERROR(MSG_CONF_CANTMERGECONFIGURATIONFORHOST_S, qualified_hostname);
      }
      answer_list_output(&answer_list);

      local.reset();
      global.reset();

      sge_show_conf();

      /* updating the commlib parameter list and gdi_timeout with new or changed parameters */
      qmaster_params = mconf_get_qmaster_params();
      cl_com_update_parameter_list(qmaster_params);
      sge_free(&qmaster_params);

      // propagate possible changes in the reporting_params to reporting writers
      ocs::ReportingFileWriter::update_config_all();

      // The maximum number of events clients might have changed.
      sge_set_max_dynamic_event_clients(mconf_get_max_dynamic_event_clients());

      // CS-1239: STREE_TICK_INTERVAL may have changed. The pending one-time
      // tick event was scheduled with the *previous* interval, so without
      // an explicit re-schedule a tighter new interval would not take effect
      // until the already-queued event fires - which can be up to 300 s away.
      // Drop the queued event and re-queue at +5 s so the new value is in
      // effect within seconds.
      sge_reschedule_sharetree_tick();

      // CS-1239: same story for STREE_SPOOL_INTERVAL and the dirty-objects
      // flush event - the already-queued event keeps its original 240 s
      // schedule unless we explicitly re-arm it. issue_1385 pins
      // STREE_SPOOL_INTERVAL low then reads the spool file within seconds.
      sge_reschedule_sharetree_spool();

      // CS-1908: FINISHED_JOBS_SWEEP_INTERVAL may have changed, or the admin
      // may have just flipped retention on. The pending one-time sweep event
      // was scheduled with the *previous* interval; drop it and re-queue at
      // +5 s so the new cadence takes effect within seconds rather than up
      // to sweep_interval away.
      sge_reschedule_finished_jobs_sweep();
   }

   /* invalidate configuration cache */
   mconf_set_new_config(true);

   // max_unheard or the load_report_time of any host may have changed
   sge_update_heard_from_timeout();

   invalidate_load_report_intervals(unique_name);
   notify_execds_about_conf_change(unique_name);

   DRETURN(STATUS_OK);
}

/**
 * @brief sge_mod_configuration() for an untyped caller
 *
 * @param aConf CONF_Type element containing the modified conf
 * @param anAnswer answer list
 * @param aUser target user
 * @param aHost target host
 * @param gdi_session the session the change belongs to
 * @return as the typed sge_mod_configuration()
 */
int
sge_mod_configuration(lListElem *aConf, lList **anAnswer, const char *aUser, const char *aHost,
                      const uint64_t gdi_session) {
   return sge_mod_configuration(ocs::tom::Element<ocs::tom::CONF>(aConf), anAnswer, aUser, aHost, gdi_session);
}

/**
 * @brief Tell commlib how long it has to remember the last message of an execd
 *
 * sge_qmaster gets the time of the last message of an sge_execd from the endpoint list of
 * commlib. Commlib removes the entry of an sge_execd which did not send anything for the
 * HEARD_FROM_TIMEOUT, the time of its last message is then 0. An idle sge_execd sends a
 * message only once per load_report_time, and sge_qmaster takes its host as unheard after
 * 3 load report intervals, but not before max_unheard (see host_unheard_timeout()).
 * The HEARD_FROM_TIMEOUT is therefore the maximum of max_unheard and 3 times the longest
 * load_report_time of the global and the local configurations.
 */
void
sge_update_heard_from_timeout() {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list(SGE_TYPE_CONFIG));
   uint32_t max_load_report_time = 0;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   for (const auto conf : config_list) {
      const auto entry = ocs::tom::find_first_in<CONF_entries, CF_name>(conf, "load_report_time");
      uint32_t load_report_time = 0;
      if (entry && parse_ulong_val(nullptr, &load_report_time, ocs::CEntry::Type::TIME,
                                              ocs::tom::get<CF_value>(entry), nullptr, 0) != 0) {
         max_load_report_time = std::max(max_load_report_time, load_report_time);
      }
   }
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   const uint32_t timeout = std::max(mconf_get_max_unheard(), 3 * max_load_report_time);
   DPRINTF("%s: HEARD_FROM_TIMEOUT is " sge_u32 "\n", __func__, timeout);
   cl_commlib_set_connection_param(cl_com_get_handle(to_cstr(QMASTER), 1), HEARD_FROM_TIMEOUT,
                                   timeout);

   DRETURN_VOID;
}

/*!
 * @brief Verify the path of a script in the configuration, which may contain special variables.
 *
 * The path has to be absolute: it has to start with a "/" or with the variable $sge_root.
 * It may contain only the variables listed in allowed, which are expanded when the script
 * is started.
 *
 * @param alpp The answer list which receives the error message.
 * @param name The name of the configuration parameter.
 * @param path The path of the script, without a user@ prefix.
 * @param allowed The nullptr terminated list of the variables which may be used in path.
 * @return true if the path is valid, false otherwise.
 */
static bool
check_config_script_path(lList **alpp, const char *name, const char *path, const char **allowed) {
   DENTER(TOP_LAYER);

   if (!config_is_absolute_script_path(path)) {
      ERROR(MSG_CONF_PATHMUSTSTARTWITHSLASHORSGEROOT_S, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // ensure that the variables are valid
   config_errfunc = set_error;
   if (replace_params(path, nullptr, 0, allowed) != 0) {
      ERROR(MSG_CONF_PARAMETERXINCONFIGURATION_SS, name, err_msg);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   DRETURN(true);
}

/**
 * @brief Validate a configuration sent by a client before it is stored
 *
 * @param alpp receives the reason a configuration is rejected
 * @param conf the configuration (CONF_Type)
 *
 * @return STATUS_OK if the configuration is valid, an error status otherwise;
 *         STATUS_ESYNTAX when an attribute is given twice
 */
static int
check_config(lList **alpp, const ocs::tom::ConstElement<ocs::tom::CONF> conf) {
   DENTER(TOP_LAYER);

   const char *name, *value;
   const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));

   const char *conf_name = ocs::tom::get<CONF_name>(conf);

   // an attribute given twice has no defined value (CS-3053)
   const std::string object_name = std::string("configuration ") + (conf_name != nullptr ? conf_name : "");
   if (!ocs_object_verify_unique_keys(conf, alpp, object_name.c_str())) {
      DRETURN(STATUS_ESYNTAX);
   }

   for (const auto ep : ocs::tom::get<CONF_entries>(conf)) {
      name = ocs::tom::get<CF_name>(ep);
      value = ocs::tom::get<CF_value>(ep);

      if (name == nullptr) {
         ERROR(MSG_CONF_NAMEISNULLINCONFIGURATIONLISTOFX_S, conf_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
      if (value == nullptr) {
         ERROR(MSG_CONF_VALUEISNULLFORATTRXINCONFIGURATIONLISTOFY_SS, name, conf_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }

      if (!strcmp(name, "loglevel")) {
         uint32_t tmp_uval;
         if (sge_parse_loglevel_val(&tmp_uval, value) != 1) {
            ERROR(MSG_CONF_GOTINVALIDVALUEXFORLOGLEVEL_S, value);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      } else if (strcmp(name, "jsv_url") == 0) {
         if (strcasecmp("none", value) != 0) {
            dstring input = DSTRING_INIT;
            dstring type = DSTRING_INIT;
            dstring user = DSTRING_INIT;
            dstring path = DSTRING_INIT;
            bool lret = true;

            sge_dstring_append(&input, value);
            lret = jsv_url_parse(&input, alpp, &type, &user, &path, false);
            sge_dstring_free(&input);
            sge_dstring_free(&type);
            sge_dstring_free(&user);
            sge_dstring_free(&path);
            if (!lret) {
               /* answer is written by jsv_url_parse */
               DRETURN(STATUS_EEXIST);
            }
         }
      } else if (!strcmp(name, "shell_start_mode")) {
         if ((strcasecmp("unix_behavior", value) != 0) &&
             (strcasecmp("posix_compliant", value) != 0) &&
             (strcasecmp("script_from_stdin", value) != 0)) {
            ERROR(MSG_CONF_GOTINVALIDVALUEXFORSHELLSTARTMODE_S, value);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "shell")) {
         if (!path_verify(name, alpp, "shell", true)) {
            ERROR(MSG_CONF_GOTINVALIDVALUEXFORSHELL_S, value);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "load_report_time")) {
         /* do not allow infinity entry for load_report_time */
         if (strcasecmp(value, INFINITY_STR) == 0) {
            ERROR(MSG_CONF_INFNOTALLOWEDFORATTRXINCONFLISTOFY_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "max_unheard")) {
         /* do not allow infinity entry */
         if (strcasecmp(value, INFINITY_STR) == 0) {
            ERROR(MSG_CONF_INFNOTALLOWEDFORATTRXINCONFLISTOFY_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "reporting_params")) {
         // Validate parameters whose grammar we can fail-fast on. Today this
         // covers online_usage and usage_patterns; other reporting_params tokens are
         // sanity-checked at merge time in sge_conf.cc::merge_configuration().
         struct saved_vars_s *context = nullptr;
         bool rp_valid = true;
         for (const char *s = sge_strtok_r(value, PARAMS_DELIMITER, &context);
              rp_valid && s != nullptr;
              s = sge_strtok_r(nullptr, PARAMS_DELIMITER, &context)) {
            std::string online_usage_str;
            if (parse_string_param(s, "online_usage", online_usage_str)) {
               std::vector<std::string> discard;
               if (!parse_online_usage_value(alpp, online_usage_str.c_str(), discard)) {
                  rp_valid = false;
               }
               continue;
            }
            std::string usage_patterns_str;
            if (parse_string_param(s, "usage_patterns", usage_patterns_str)) {
               std::vector<std::pair<std::string, std::string>> discard;
               if (!parse_usage_patterns_value(alpp, usage_patterns_str.c_str(), discard)) {
                  rp_valid = false;
               }
            }
         }
         sge_free_saved_vars(context);
         if (!rp_valid) {
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "qmaster_params")) {
         // Validate parameters whose grammar we can fail-fast on. Today this covers only
         // AUTO_ADD_HOSTS; other qmaster_params tokens are sanity-checked at merge time
         // in sge_conf.cc::merge_configuration().
         struct saved_vars_s *context = nullptr;
         bool qp_valid = true;
         for (const char *s = sge_strtok_r(value, PARAMS_DELIMITER, &context);
              s != nullptr;
              s = sge_strtok_r(nullptr, PARAMS_DELIMITER, &context)) {
            std::string auto_add_hosts;
            if (parse_string_param(s, "AUTO_ADD_HOSTS", auto_add_hosts)) {
               std::vector<ocs::AutoAddHosts::Rule> discard;
               if (!ocs::AutoAddHosts::parse(auto_add_hosts.c_str(), discard, alpp)) {
                  qp_valid = false;
               }
               continue;
            }

            // parse_time_param() turns a value it cannot read into 0, which means "switched
            // off", so the time is parsed here once more to be able to report a bad one
            const char *time_value = strchr(s, '=');
            if (strncasecmp(s, "AUTO_REMOVE_HOSTS_AFTER", strlen("AUTO_REMOVE_HOSTS_AFTER")) == 0 &&
                time_value != nullptr) {
               char err_str[MAX_STRING_SIZE];
               uint32_t seconds = 0;

               time_value++;
               if (!extended_parse_ulong_val(nullptr, &seconds, ocs::CEntry::Type::TIME, time_value,
                                             err_str, sizeof(err_str), 0, false)) {
                  answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_AUTOREMOVEHOSTS_TIME_SS, time_value, err_str);
                  qp_valid = false;
               }
               continue;
            }
         }
         sge_free_saved_vars(context);
         if (!qp_valid) {
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "admin_user")) {
         struct passwd pw_struct;
         char *buffer;
         int size;

         size = get_pw_buffer_size();
         buffer = sge_malloc(size);
         if (strcasecmp(value, "none") && !sge_getpwnam_r(value, &pw_struct, buffer, size)) {
            ERROR(MSG_CONF_GOTINVALIDVALUEXASADMINUSER_S, value);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            sge_free(&buffer);
            DRETURN(STATUS_EEXIST);
         }
         sge_free(&buffer);
      } else if (!strcmp(name, "user_lists") || !strcmp(name, "xuser_lists")) {
         /* parse just for .. */
         ocs::tom::ListPtr<ocs::tom::US> tmp;
         try {
            tmp = ocs::tom::ListPtr<ocs::tom::US>::from_string<US_name>(value, " \t,", ocs::tom::NoneName::Empty);
         } catch (const ocs::tom::ListCoreError &) {
            // "none" beside other names, an error of lString2ListNone()
            ERROR(MSG_CONF_FORMATERRORFORXINYCONFIG_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /* .. checking userset names */
         if (userset_list_validate_acl_list(*tmp, alpp, master_userset_list) != STATUS_OK) {
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "projects") || !strcmp(name, "xprojects")) {
         /* parse just for .. */
         ocs::tom::ListPtr<ocs::tom::PR> tmp;
         try {
            tmp = ocs::tom::ListPtr<ocs::tom::PR>::from_string<PR_name>(value, " \t,", ocs::tom::NoneName::Empty);
         } catch (const ocs::tom::ListCoreError &) {
            // "none" beside other names, an error of lString2ListNone()
            ERROR(MSG_CONF_FORMATERRORFORXINYCONFIG_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /* .. checking project names */
         const ocs::tom::ConstList<ocs::tom::PR>
            master_project_list(*ocs::DataStore::get_master_list(SGE_TYPE_PROJECT));
         if (verify_project_list(alpp, *tmp, master_project_list, name, "configuration", conf_name) != STATUS_OK) {
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "prolog") || !strcmp(name, "epilog")) {
         if (strcasecmp(value, "none")) {
            const char *t, *script = value;

            /* skip user name */
            if ((t = strpbrk(script, "@ ")) && *t == '@')
               script = &t[1];

            // force use of absolute paths and ensure that the variables are valid
            if (!check_config_script_path(alpp, name, script, prolog_epilog_variables)) {
               DRETURN(STATUS_EEXIST);
            }
         }
      } else if (!strcmp(name, "auto_user_oticket") || !strcmp(name, "auto_user_fshare")) {
         uint32_t uval = 0;
         if (!extended_parse_ulong_val(nullptr, &uval, ocs::CEntry::Type::INT, value, nullptr, 0, 0, true)) {
            ERROR(MSG_CONF_FORMATERRORFORXINYCONFIG_SS, name, value ? value : "(nullptr)");
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }

         /*
          * check paths, see also CR 6506580.
          * The following must be none or a valid absolute path:
          * - load_sensor
          * - set_token_cmd
          * - pag_cmd
          * - shepherd_cmd
          *
          * The following must be a valid absolute path:
          * - mailer
          * - xterm
          * - *_daemon, may also be "builtin"
          */
      else if (strcmp(name, "set_token_cmd") == 0 ||
               strcmp(name, "pag_cmd") == 0 ||
               strcmp(name, "shepherd_cmd") == 0) {
         if (strcasecmp(value, "none") != 0) {
            if (!path_verify(value, alpp, name, true)) {
               answer_list_log(alpp, false, false);
               DRETURN(STATUS_EEXIST);
            }
         }
      } else if (strcmp(name, "mailer") == 0 ||
                 strcmp(name, "xterm") == 0) {
         if (!path_verify(value, alpp, name, true)) {
            answer_list_log(alpp, false, false);
            DRETURN(STATUS_EEXIST);
         }
      } else if (strcmp(name, "qlogin_daemon") == 0 ||
                 strcmp(name, "rlogin_daemon") == 0 ||
                 strcmp(name, "rsh_daemon") == 0) {
         if (strcasecmp(value, "builtin") != 0) {
            if (!path_verify(value, alpp, name, true)) {
               answer_list_log(alpp, false, false);
               DRETURN(STATUS_EEXIST);
            }
         }
      }

         /* load_sensor is a comma separated list of scripts */
      else if (strcmp(name, "load_sensor") == 0 && strcasecmp(value, "none") != 0) {
         struct saved_vars_s *context = nullptr;
         const char *path = sge_strtok_r(value, ",", &context);
         do {
            if (!path_verify(path, alpp, name, false)) {
               answer_list_log(alpp, false, false);
               sge_free_saved_vars(context);
               DRETURN(STATUS_EEXIST);
            }
            // force use of absolute paths and ensure that the variables are valid
            if (!check_config_script_path(alpp, name, path, load_sensor_variables)) {
               sge_free_saved_vars(context);
               DRETURN(STATUS_EEXIST);
            }
         } while ((path = sge_strtok_r(nullptr, ",", &context)) != nullptr);
         sge_free_saved_vars(context);
      }
   }

   DRETURN(0);
}


/** @brief Has a host's effective configuration changed since it last asked?
 *
 * Compare configuration 'aConf' for host 'aHost' with the cluster configuration.
 * Return '0' if 'aConf' is equal to the cluster configuration, '1' otherwise.
 *
 * @param aHost the execution host
 * @param aConf the configurations the host currently holds
 * @return 0 when the two are the same, 1 otherwise
 */
int
sge_compare_configuration(const ocs::tom::ConstElement<ocs::tom::EH> aHost,
                          const ocs::tom::ConstList<ocs::tom::CONF> aConf) {
   DENTER(TOP_LAYER);

   if (aConf.empty()) {
      const char *host_name = ocs::tom::get<EH_name>(aHost);
      DPRINTF("%s: configuration for %s is empty\n", __func__, host_name);
      DRETURN(1);
   }

   for (const auto conf_entry : aConf) {
      const char *host_name = ocs::tom::get<CONF_name>(conf_entry);
      const uint32_t master_version = sge_get_config_version_for_host(host_name);
      const uint32_t conf_version = ocs::tom::get<CONF_version>(conf_entry);

      if (master_version != conf_version) {
         DPRINTF("%s: configuration for %s changed from version %ld to %ld\n", __func__, host_name, master_version,
                 conf_version);
         DRETURN(1);
      }
   }

   DRETURN(0);
}

/**
 * @brief sge_compare_configuration() for an untyped caller
 *
 * The list comes from the configuration report of an sge_execd, which holds CONF_Type elements; it is
 * wrapped without a check, every access to an element checks its type as the raw accessors did.
 *
 * @param aHost the execution host (`EH_Type`)
 * @param aConf the configurations the host currently holds
 * @return as the typed sge_compare_configuration()
 */
int
sge_compare_configuration(const lListElem *aHost, const lList *aConf) {
   return sge_compare_configuration(ocs::tom::ConstElement<ocs::tom::EH>(aHost),
                                    ocs::tom::ConstList<ocs::tom::CONF>::from_typed(aConf));
}


/** @brief One entry of a host's effective configuration
 *
 * Return a *COPY* of configuration entry 'anEntryName'. First we do query the
 * local configuration 'aHost'. If that is fruitless, we try the global
 * configuration.
 *
 * @param aHost the execution host
 * @param anEntryName the entry to read
 * @return the entry, empty when it is not set
 */
ocs::tom::ElementPtr<ocs::tom::CF>
ocs_get_configuration_entry_by_name(const char *aHost, const char *anEntryName) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::CF> elem;

   SGE_ASSERT((nullptr != aHost) && (nullptr != anEntryName));

   /* try local configuration first */
   if (const auto conf = sge_get_configuration_for_host(aHost); conf) {
      elem = get_entry_from_conf(*conf, anEntryName);
   }

   /* local configuration did not work, try global one */
   if (!elem) {
      if (const auto conf = sge_get_configuration_for_host(SGE_GLOBAL_NAME); conf) {
         elem = get_entry_from_conf(*conf, anEntryName);
      }
   }

   DRETURN(elem);
}

/**
 * @brief sge_get_configuration_entry_by_name() for an untyped caller
 *
 * @param aHost the execution host
 * @param anEntryName the entry to read
 * @return a copy of the entry, which the caller owns, or nullptr when it is not set
 */
lListElem *
sge_get_configuration_entry_by_name(const char *aHost, const char *anEntryName) {
   return ocs_get_configuration_entry_by_name(aHost, anEntryName).release();
}

/**
 * @brief A copy of one entry of a configuration
 *
 * @param aConf the configuration
 * @param anEntryName the entry to read
 * @return the copy, empty if the configuration has no such entry
 */
static ocs::tom::ElementPtr<ocs::tom::CF>
get_entry_from_conf(const ocs::tom::ConstElement<ocs::tom::CONF> aConf, const char *anEntryName) {
   DENTER(TOP_LAYER);
   const auto elem = ocs::tom::find_first<CF_name>(ocs::tom::get<CONF_entries>(aConf), anEntryName);
   DRETURN(elem.clone());
}

/** @brief The configurations matching a condition
 *
 * Return a *COPY* of the master configuration.
 *
 * @param condition which configurations to return
 * @param enumeration which fields to include
 * @return the matching configurations, which the caller owns
 */
lList *
sge_get_configuration(const lCondition *condition, const lEnumeration *enumeration) {
   DENTER(TOP_LAYER);

   lList *conf = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   const lList *config_list = *ocs::DataStore::get_master_list(SGE_TYPE_CONFIG);

   conf = lSelectHashPack("", config_list, condition, enumeration, false, nullptr);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(conf);
}

/**
 * @brief The version of the master configuration of a host
 *
 * @param aName the configuration name, a host name or "global"
 * @return the version, 0 if there is no such configuration
 */
static uint32_t
sge_get_config_version_for_host(const char *aName) {
   DENTER(TOP_LAYER);

   uint32_t version = 0;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   const ocs::tom::ConstList<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list(SGE_TYPE_CONFIG));

   auto conf = ocs::tom::find_first<CONF_name>(config_list, aName);
   if (!conf) {
      /*
       * Due to CR 6319231 IZ 1760:
       *    Try to resolve the hostname
       *    if it is not resolvable then
       *       ignore this and use the given hostname
       */
      ret = sge_resolve_hostname(aName, unique_name, EH_name);
      if (CL_RETVAL_OK != ret) {
         DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), aName);
      }
      conf = ocs::tom::find_first<CONF_name>(config_list, unique_name);
   }

   if (!conf) {
      DPRINTF("%s: no master configuration for %s found\n", __func__, aName);
   } else {
      version = ocs::tom::get<CONF_version>(conf);
   }

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(version);
}

/** @brief A host's effective configuration: global, with its local entries applied
 *
 * Return a *COPY* of the configuration for host 'aName'. The host name 'aName'
 * will be resolved to eliminate any host name differences caused by the
 * various host name formats or the host name alias mechanism.
 *
 * @param aName the execution host
 * @return the configuration, empty if there is none
 */
ocs::tom::ElementPtr<ocs::tom::CONF>
sge_get_configuration_for_host(const char *aName) {
   DENTER(TOP_LAYER);

   char unique_name[CL_MAXHOSTNAMELEN];

   SGE_ASSERT((nullptr != aName));

   /*
    * Due to CR 6319231 IZ 1760:
    *    Try to resolve the hostname
    *    if it is not resolvable then
    *       ignore this and use the given hostname
    */
   const int ret = sge_resolve_hostname(aName, unique_name, EH_name);
   if (CL_RETVAL_OK != ret) {
      DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), aName);
      strcpy(unique_name, aName);
   }

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   const ocs::tom::ConstList<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list(SGE_TYPE_CONFIG));
   auto conf = ocs::tom::find_first<CONF_name>(config_list, unique_name).clone();

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(conf);
}

/**
 * @brief Replace the configuration with name 'aConfName'
 *
 * NOTE: Either 'anOldConf' or 'aNewConf' could be an empty configuration!
 * Empty configurations do not contain any 'CONF_entries'.
 *
 * @param aConfName the name of the configuration
 * @param anOldConf a *COPY* of the configuration
 * @param aNewConf the new configuration
 * @param anAnswer receives messages for the caller
 * @param gdi_session the session the change belongs to
 * @return 0 on success, STATUS_EUNKNOWN for invalid gdi_request_limits, -1 on other errors
 */
static int
do_mod_config(char *aConfName, const ocs::tom::ConstElement<ocs::tom::CONF> anOldConf,
              const ocs::tom::Element<ocs::tom::CONF> aNewConf, lList **anAnswer, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const auto old_entries = ocs::tom::get<CONF_entries>(anOldConf);
   const auto new_entries = ocs::tom::get<CONF_entries>(aNewConf);

   // VALIDATION AND ADJUSTMENTS

   // check if gdi_request_limits are correct
   const auto gdi_request_limits_cfg = ocs::tom::find_first<CF_name>(new_entries, "gdi_request_limits");
   if (gdi_request_limits_cfg) {
      const char *gdi_request_limit_str = ocs::tom::get<CF_value>(gdi_request_limits_cfg);
      ocs::RequestLimits& limits_instance = ocs::RequestLimits::get_instance();
      if (!limits_instance.parse(gdi_request_limit_str, anAnswer)) {
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   // log warning if static attributes are changed during runtime (like exec spool dir)
   if (check_static_conf_entries(old_entries, new_entries, anAnswer) != 0) {
      DRETURN(-1);
   }

   // MODIFY CONFIGURATION

   // replace the old config by the new one
   exchange_conf_by_name(aConfName, anOldConf, aNewConf, anAnswer, gdi_session);

   // UPDATE AFTER HANGES HAVE BEEN APPLIED

   if (has_reschedule_unknown_change(old_entries, new_entries)) {
      update_reschedule_unknown_timout_values(aConfName);
   }

   DRETURN(0);
}

/**
 * @brief Warn when a static configuration entry changes
 *
 * Static configuration entries may be changed at runtime with a warning.
 *
 * @param theOldConfEntries the entries of the old configuration
 * @param theNewConfEntries the entries of the new configuration
 * @param anAnswer receives the warnings
 * @return 0
 */
static int
check_static_conf_entries(const ocs::tom::ConstList<ocs::tom::CF> theOldConfEntries,
                          const ocs::tom::ConstList<ocs::tom::CF> theNewConfEntries, lList **anAnswer) {
   DENTER(TOP_LAYER);

   int entry_idx = 0;

   while (nullptr != Static_Conf_Entries[entry_idx]) {
      const char *entry_name = Static_Conf_Entries[entry_idx];

      const auto old_entry = ocs::tom::find_first<CF_name>(theOldConfEntries, entry_name);
      const auto new_entry = ocs::tom::find_first<CF_name>(theNewConfEntries, entry_name);

      if (old_entry && new_entry) {
         const char *old_value = ocs::tom::get<CF_value>(old_entry);
         const char *new_value = ocs::tom::get<CF_value>(new_entry);

         if (((nullptr != old_value) && (nullptr != new_value)) && (strcmp(old_value, new_value) != 0)) {
            /* log in qmaster messages file */
            WARNING(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, entry_name);
            /*   INFO(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, entry_name); */
            answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }
      } else if (static_cast<bool>(old_entry) != static_cast<bool>(new_entry)) {
         /* log error only if one value is set */
         /* log in qmaster messages file */
         WARNING(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, entry_name);
         /* INFO(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, entry_name); */
         answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
      entry_idx++;
   }

   DRETURN(0);
}

/**
 * @brief Remove configuration 'aConfName' from cluster configuration and append 'aNewConf' to it
 *
 * @param aConfName the name of the configuration
 * @param anOldConf a *COPY* of the old configuration entry
 * @param aNewConf the new configuration; gets the next version and the old name
 * @param anAnswer receives messages for the caller
 * @param gdi_session the session the change belongs to
 * @return 0
 */
static int
exchange_conf_by_name(char *aConfName, const ocs::tom::ConstElement<ocs::tom::CONF> anOldConf,
                      const ocs::tom::Element<ocs::tom::CONF> aNewConf, lList **anAnswer,
                      const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   const char *old_conf_name = ocs::tom::get<CONF_name>(anOldConf);
   const uint32_t old_version = ocs::tom::get<CONF_version>(anOldConf);
   const uint32_t new_version = (old_version + 1);

   ocs::tom::set<CONF_version>(aNewConf, new_version);

   /* Make sure, 'aNewConf' does have a unique name */
   ocs::tom::set<CONF_name>(aNewConf, old_conf_name);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   const ocs::tom::List<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG));

   if (const auto old_elem = ocs::tom::find_first_rw<CONF_name>(config_list, old_conf_name); old_elem) {
      config_list.erase(old_elem);
   }

   const auto elem = config_list.push_back(aNewConf.clone());

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   sge_event_spool(anAnswer, 0, sgeE_CONFIG_MOD, 0, 0, old_conf_name, nullptr, nullptr, elem, nullptr, nullptr, true,
                   true, gdi_session);

   DRETURN(0);
}

/**
 * @brief Did the reschedule_unknown entry change?
 *
 * @param theOldConfEntries the entries of the old configuration
 * @param theNewConfEntries the entries of the new configuration
 * @return true if the value changed or is missing in one of the configurations
 */
static bool
has_reschedule_unknown_change(const ocs::tom::ConstList<ocs::tom::CF> theOldConfEntries,
                              const ocs::tom::ConstList<ocs::tom::CF> theNewConfEntries) {
   DENTER(TOP_LAYER);

   bool res = false;

   const auto old_elem = ocs::tom::find_first<CF_name>(theOldConfEntries, "reschedule_unknown");
   const auto new_elem = ocs::tom::find_first<CF_name>(theNewConfEntries, "reschedule_unknown");

   const char *old_value = old_elem ? ocs::tom::get<CF_value>(old_elem) : nullptr;
   const char *new_value = new_elem ? ocs::tom::get<CF_value>(new_elem) : nullptr;

   if ((nullptr == old_value) || (nullptr == new_value)) {
      res = true;  /* change by omission in one configuration */
   } else if (strcmp(old_value, new_value) != 0) {
      res = true;  /* value did change */
   }

   if (res) {
      DPRINTF("%s: reschedule_unknown did change!\n", __func__);
   }

   DRETURN(res);
}

/**
 * @brief Add a copy of a configuration to the cluster configuration
 *
 * @param aConfName the name of the configuration
 * @param aConf the new configuration
 * @param anAnswer receives messages for the caller
 * @param gdi_session the session the change belongs to
 * @return 0
 */
static int
do_add_config(char *aConfName, const ocs::tom::ConstElement<ocs::tom::CONF> aConf, lList **anAnswer,
              const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   auto copy = aConf.clone();

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   const ocs::tom::List<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG));
   const auto elem = config_list.push_back(std::move(copy));

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   sge_event_spool(anAnswer, 0, sgeE_CONFIG_ADD, 0, 0, aConfName, nullptr, nullptr, elem, nullptr, nullptr, true, true,
                   gdi_session);

   DRETURN(0);
}

/**
 * @brief Remove a configuration from the cluster configuration
 *
 * @param aConfName the name of the configuration, already host-resolved
 * @return 0
 */
static int
remove_conf_by_name(char *aConfName) {
   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   const ocs::tom::List<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG));

   if (const auto elem = ocs::tom::find_first_rw<CONF_name>(config_list, aConfName); elem) {
      config_list.erase(elem);
   }

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   DRETURN(0);
}

/** @brief Is there a configuration under this name?
 *
 * CS-2754. The lookup is the one remove_conf_by_name() uses, so both agree on
 * what counts as a match -- lGetElemHost() normalises through sge_hostcpy(),
 * which is why a configuration spooled under the short name is found when the
 * fully qualified name is asked for, and the other way round.
 *
 * @param aConfName the configuration name, already host-resolved
 * @return true if a configuration of that name exists
 */
static bool
conf_exists_by_name(const char *aConfName) {
   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   const ocs::tom::ConstList<ocs::tom::CONF> config_list(*ocs::DataStore::get_master_list(SGE_TYPE_CONFIG));

   const bool exists = static_cast<bool>(ocs::tom::find_first<CONF_name>(config_list, aConfName));

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(exists);
}
