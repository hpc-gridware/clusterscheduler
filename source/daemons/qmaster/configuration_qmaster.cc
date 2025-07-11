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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>

#include "uti/config_file.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"

#include "cull/cull.h"

#include "comm/commlib.h"

#include "spool/sge_spooling.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_RequestLimits.h"

#include "configuration_qmaster.h"
#include "ocs_ReportingFileWriter.h"
#include "sge.h"
#include "sge_persistence_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "reschedule.h"
#include "msg_common.h"
#include "msg_qmaster.h"

/* Static configuration entries may be changed at runtime with a warning */
static const char *Static_Conf_Entries[] = {"execd_spool_dir", nullptr};


static int
check_config(lList **alpp, lListElem *conf);

static int
do_mod_config(char *aConfName, lListElem *anOldConf, lListElem *aNewConf, lList **anAnswer, u_long64 gdi_session);

static int
check_static_conf_entries(const lList *theOldConfEntries, const lList *theNewConfEntries, lList **anAnswer);

static int
exchange_conf_by_name(char *aConfName, lListElem *anOldConf, lListElem *aNewConf,
                      lList **anAnswer, u_long64 gdi_session);

static bool
has_reschedule_unknown_change(const lList *theOldConfEntries, const lList *theNewConfEntries);

static int
do_add_config(char *aConfName, lListElem *aConf, lList **anAnswer, u_long64 gdi_session);

static int
remove_conf_by_name(char *aConfName);

static lListElem *
get_entry_from_conf(lListElem *aConf, const char *anEntryName);

static u_long32
sge_get_config_version_for_host(const char *aName);

/* 
 * Read the cluster configuration from secondary storage using 'aSpoolContext'.
 * This is the bootstrap function for the configuration module. It does populate
 * the list with the cluster configuration.
 */
int
sge_read_configuration(const lListElem *aSpoolContext, lList **config_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   // read the spooled list
   spool_read_list(answer_list, aSpoolContext, config_list, SGE_TYPE_CONFIG);

   lListElem *global = lGetElemHostRW(*config_list, CONF_name, "global");
   if (global != nullptr) {
      const lList *entries = lGetList(global, CONF_entries);

      // add jsv_url if it is missing
      lListElem *jsv_url = lGetElemStrRW(entries, CF_name, "jsv_url");
      if (jsv_url == nullptr) {
         jsv_url = lAddSubStr(global, CF_name, "jsv_url", CONF_entries, CF_Type);
         lSetString(jsv_url, CF_value, "none");
      }

      // add jsv_allowed_mod if it is missing
      lListElem *jsv_allowed_mod = lGetElemStrRW(entries, CF_name, "jsv_allowed_mod");
      if (jsv_allowed_mod == nullptr) {
         jsv_allowed_mod = lAddSubStr(global, CF_name, "jsv_allowed_mod", CONF_entries, CF_Type);
         lSetString(jsv_allowed_mod, CF_value, "ac,h,i,e,o,j,M,N,p,w");
      }

      // add gdi_request_limits if the attribute is missing
      lListElem *gdi_request_limits = lGetElemStrRW(entries, CF_name, "gdi_request_limits");
      if (gdi_request_limits == nullptr) {
         gdi_request_limits = lAddSubStr(global, CF_name, "gdi_request_limits", CONF_entries, CF_Type);
         lSetString(gdi_request_limits, CF_value, "none");
      }
   }
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   answer_list_output(answer_list);

   const char *qualified_hostname = component_get_qualified_hostname();
   DPRINTF("qualified_hostname: '%s'\n", qualified_hostname);
   lListElem *local = nullptr;
   if ((local = sge_get_configuration_for_host(qualified_hostname)) == nullptr) {
      /* write a warning into messages file, if no local config exists*/
      WARNING(MSG_CONFIG_NOLOCAL_S, qualified_hostname);
   }

   if ((global = sge_get_configuration_for_host(SGE_GLOBAL_NAME)) == nullptr) {
      ERROR(SFNMAX, MSG_CONFIG_NOGLOBAL);
      DRETURN(-1);
   }

   u_long32 progid = component_get_component_id();
   const char *cell_root = bootstrap_get_cell_root();
   int ret = merge_configuration(answer_list, progid, cell_root, global, local, nullptr);
   answer_list_output(answer_list);

   lFreeElem(&local);
   lFreeElem(&global);

   if (0 != ret) {
      ERROR(MSG_CONFIG_ERRORXMERGINGCONFIGURATIONY_IS, ret, qualified_hostname);
      DRETURN(-1);
   }

   sge_show_conf();

   DRETURN(0);
}

/*
 * Delete configuration 'confp' from cluster configuration.
 *
 * TODO: A fix for IZ issue #79 is needed. For this to be done it may be 
 * necessary to introduce something like 'protected' configuration entries.
 */
int
sge_del_configuration(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *aConf, lList **anAnswer, char *aUser, char *aHost) {
   const char *tmp_name = nullptr;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;

   DENTER(TOP_LAYER);

   if (!aConf || !aUser || !aHost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((tmp_name = lGetHost(aConf, CONF_name)) == nullptr) {
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
      lListElem *conf_obj = nullptr;

      DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), tmp_name);

      conf_obj = sge_get_configuration_for_host(tmp_name);
      if (conf_obj != nullptr) {
         DPRINTF("using hostname stored in configuration object\n");
         strcpy(unique_name, lGetHost(conf_obj, CONF_name));
         lFreeElem(&conf_obj);
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

   sge_event_spool(anAnswer, 0, sgeE_CONFIG_DEL, 0, 0, unique_name, nullptr, nullptr, nullptr, nullptr, nullptr, true, true, packet->gdi_session);

   remove_conf_by_name(unique_name);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, aUser, aHost, unique_name, MSG_OBJ_CONF);
   answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   update_reschedule_unknown_timout_values(unique_name);

   /* invalidate cached configuration values */
   mconf_set_new_config(true);

   DRETURN(STATUS_OK);
}


/****** qmaster/sge_mod_configuration() ****************************************
*  NAME
*     sge_mod_configuration() -- modify cluster configuration
*
*  SYNOPSIS
*     int sge_mod_configuration(lListElem *aConf, lList **anAnswer, char *aUser,
*                               char *aHost)
*
*  FUNCTION
*     Modify cluster configuration. 'confp' is a pointer to a 'CONF_Type' list
*     element and does contain the modified configuration entry. Adding a new
*     configuration entry is also viewed as a modification.
*
*  INPUTS
*     lListElem *aConf  - CONF_Type element containing the modified conf
*     lList **anAnswer  - answer list
*     char *aUser       - target user
*     char *aHost       - target host
*
*  RESULT
*     int - 0 success
*          -1 error
*
*  NOTES
*     MT-NOTE: sge_mod_configuration() is MT safe 
*
*******************************************************************************/
int
sge_mod_configuration(lListElem *aConf, lList **anAnswer, const char *aUser, const char *aHost, u_long64 gdi_session) {
   DENTER(TOP_LAYER);
   const char *tmp_name = nullptr;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;
   const char *cell_root = bootstrap_get_cell_root();
   const char *qualified_hostname = component_get_qualified_hostname();
   u_long32 progid = component_get_component_id();

   if (!aConf || !aUser || !aHost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(anAnswer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((tmp_name = lGetHost(aConf, CONF_name)) == nullptr) {
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

   lListElem *old_conf = sge_get_configuration_for_host(unique_name);
   if (old_conf != nullptr) {
      int lret = do_mod_config(unique_name, old_conf, aConf, anAnswer, gdi_session);
      lFreeElem(&old_conf);

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
   ** is the configuration change relevant for the qmaster itsself?
   ** if so, initialise conf struct anew
   */
   if (strcmp(unique_name, SGE_GLOBAL_NAME) == 0 || sge_hostcmp(unique_name, qualified_hostname) == 0) {
      lListElem *local = nullptr;
      lListElem *global = nullptr;
      lList *answer_list = nullptr;
      char *qmaster_params = nullptr;

      if ((local = sge_get_configuration_for_host(qualified_hostname)) == nullptr) {
         WARNING(MSG_CONFIG_NOLOCAL_S, qualified_hostname);
      }

      if ((global = sge_get_configuration_for_host(SGE_GLOBAL_NAME)) == nullptr) {
         ERROR(SFNMAX, MSG_CONFIG_NOGLOBAL);
      }

      if (merge_configuration(&answer_list, progid, cell_root, global, local, nullptr) != 0) {
         ERROR(MSG_CONF_CANTMERGECONFIGURATIONFORHOST_S, qualified_hostname);
      }
      answer_list_output(&answer_list);

      lFreeElem(&local);
      lFreeElem(&global);

      sge_show_conf();

      /* 'max_unheard' may have changed */
      cl_commlib_set_connection_param(cl_com_get_handle(prognames[QMASTER], 1), HEARD_FROM_TIMEOUT, mconf_get_max_unheard());

      /* updating the commlib parameterlist and gdi_timeout with new or changed parameters */
      qmaster_params = mconf_get_qmaster_params();
      cl_com_update_parameter_list(qmaster_params);
      sge_free(&qmaster_params);


      // propagate possible changes in the reporting_params to reporting writers
      ocs::ReportingFileWriter::update_config_all();
   }

   /* invalidate configuration cache */
   mconf_set_new_config(true);

   DRETURN(STATUS_OK);
}

static int
check_config(lList **alpp, lListElem *conf) {
   const lListElem *ep;
   const char *name, *value;
   const char *conf_name;
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);

   DENTER(TOP_LAYER);

   conf_name = lGetHost(conf, CONF_name);

   for_each_ep(ep, lGetList(conf, CONF_entries)) {
      name = lGetString(ep, CF_name);
      value = lGetString(ep, CF_value);

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
         u_long32 tmp_uval;
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
         if (strcasecmp(value, "infinity") == 0) {
            ERROR(MSG_CONF_INFNOTALLOWEDFORATTRXINCONFLISTOFY_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "max_unheard")) {
         /* do not allow infinity entry */
         if (strcasecmp(value, "infinity") == 0) {
            ERROR(MSG_CONF_INFNOTALLOWEDFORATTRXINCONFLISTOFY_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
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
         lList *tmp = nullptr;
         int ok;

         /* parse just for .. */
         if (lString2ListNone(value, &tmp, US_Type, US_name, " \t,")) {
            ERROR(MSG_CONF_FORMATERRORFORXINYCONFIG_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /* .. checking userset names */
         ok = (userset_list_validate_acl_list(tmp, alpp, master_userset_list) == STATUS_OK);
         lFreeList(&tmp);
         if (!ok) {
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "projects") || !strcmp(name, "xprojects")) {
         lList *tmp = nullptr;
         int ok = 1;

         /* parse just for .. */
         if (lString2ListNone(value, &tmp, PR_Type, PR_name, " \t,")) {
            ERROR(MSG_CONF_FORMATERRORFORXINYCONFIG_SS, name, conf_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /* .. checking project names */
         ok = (verify_project_list(alpp, tmp, *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT), name, "configuration",
                                   conf_name) == STATUS_OK);
         lFreeList(&tmp);
         if (!ok) {
            DRETURN(STATUS_EEXIST);
         }
      } else if (!strcmp(name, "prolog") || !strcmp(name, "epilog")) {
         if (strcasecmp(value, "none")) {
            const char *t, *script = value;

            /* skip user name */
            if ((t = strpbrk(script, "@ ")) && *t == '@')
               script = &t[1];

            /* force use of absolute paths if string <> none */
            if (script[0] != '/') {
               ERROR(MSG_CONF_THEPATHGIVENFORXMUSTSTARTWITHANY_S, name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EEXIST);
            }

            /* ensure that variables are valid */
            if (replace_params(script, nullptr, 0, prolog_epilog_variables)) {
               ERROR(MSG_CONF_PARAMETERXINCONFIGURATION_SS, name, err_msg);
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EEXIST);
            }
         }
      } else if (!strcmp(name, "auto_user_oticket") || !strcmp(name, "auto_user_fshare")) {
         u_long32 uval = 0;
         if (!extended_parse_ulong_val(nullptr, &uval, TYPE_INT, value, nullptr, 0, 0, true)) {
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
            if (!path_verify(path, alpp, name, true)) {
               answer_list_log(alpp, false, false);
               sge_free_saved_vars(context);
               DRETURN(STATUS_EEXIST);
            }
         } while ((path = sge_strtok_r(nullptr, ",", &context)) != nullptr);
         sge_free_saved_vars(context);
      }
   }

   DRETURN(0);
}


/*
 * Compare configuration 'aConf' for host 'aHost' with the cluster configuration.
 * Return '0' if 'aConf' is equal to the cluster configuration, '1' otherwise.
 *
 * 'aHost' is of type 'EH_Type', 'aConf' is of type 'CONF_Type'.
 */
int
sge_compare_configuration(const lListElem *aHost, const lList *aConf) {
   const lListElem *conf_entry = nullptr;

   DENTER(TOP_LAYER);

   if (lGetNumberOfElem(aConf) == 0) {
      DPRINTF("%s: configuration for %s is empty\n", __func__, lGetHost(aHost, EH_name));
      DRETURN(1);
   }

   for_each_ep(conf_entry, aConf) {
      const char *host_name = nullptr;
      u_long32 conf_version;
      u_long32 master_version;

      host_name = lGetHost(conf_entry, CONF_name);
      master_version = sge_get_config_version_for_host(host_name);

      conf_version = lGetUlong(conf_entry, CONF_version);

      if (master_version != conf_version) {
         DPRINTF("%s: configuration for %s changed from version %ld to %ld\n", __func__, host_name, master_version, conf_version);
         DRETURN(1);
      }
   }

   DRETURN(0);
}


/*
 * Return a *COPY* of configuration entry 'anEntryName'. First we do query the
 * local configuration 'aHost'. If that is fruitless, we try the global
 * configuration. 
 */
lListElem *
sge_get_configuration_entry_by_name(const char *aHost, const char *anEntryName) {
   lListElem *conf = nullptr;
   lListElem *elem = nullptr;

   DENTER(TOP_LAYER);

   SGE_ASSERT((nullptr != aHost) && (nullptr != anEntryName));

   /* try local configuration first */
   if ((conf = sge_get_configuration_for_host(aHost)) != nullptr) {
      elem = get_entry_from_conf(conf, anEntryName);
   }
   lFreeElem(&conf);

   /* local configuration did not work, try global one */
   if ((elem == nullptr) && ((conf = sge_get_configuration_for_host(SGE_GLOBAL_NAME)) != nullptr)) {
      elem = get_entry_from_conf(conf, anEntryName);
   }

   lFreeElem(&conf);
   DRETURN(elem);
}

static lListElem *
get_entry_from_conf(lListElem *aConf, const char *anEntryName) {
   const lList *entries = lGetList(aConf, CONF_entries);
   const lListElem *elem = lGetElemStr(entries, CF_name, anEntryName);
   return lCopyElem(elem);
}

/*
 * Return a *COPY* of the master configuration.
 */
lList *
sge_get_configuration(const lCondition *condition, const lEnumeration *enumeration) {
   lList *conf = nullptr;
   const lList *config_list = *ocs::DataStore::get_master_list(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   conf = lSelectHashPack("", config_list, condition, enumeration, false, nullptr);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(conf);
}

static u_long32
sge_get_config_version_for_host(const char *aName) {
   const lListElem *conf = nullptr;
   u_long32 version = 0;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;
   const lList *config_list = *ocs::DataStore::get_master_list(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   conf = lGetElemHost(config_list, CONF_name, aName);
   if (conf == nullptr) {
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
      conf = lGetElemHost(config_list, CONF_name, unique_name);
   }

   if (conf == nullptr) {
      DPRINTF("%s: no master configuration for %s found\n", __func__, aName);
   } else {
      version = lGetUlong(conf, CONF_version);
   }

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(version);
}

/*
 * Return a *COPY* of the configuration for host 'aName'. The host name 'aName'
 * will be resolved to eliminate any host name differences caused by the
 * various host name formats or the host name alias mechanism.
 */
lListElem *
sge_get_configuration_for_host(const char *aName) {
   lListElem *conf = nullptr;
   char unique_name[CL_MAXHOSTNAMELEN];
   int ret = -1;
   const lList *config_list = *ocs::DataStore::get_master_list(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   SGE_ASSERT((nullptr != aName));

   /*
    * Due to CR 6319231 IZ 1760:
    *    Try to resolve the hostname
    *    if it is not resolvable then
    *       ignore this and use the given hostname
    */
   ret = sge_resolve_hostname(aName, unique_name, EH_name);
   if (CL_RETVAL_OK != ret) {
      DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), aName);
      strcpy(unique_name, aName);
   }

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   conf = lCopyElem(lGetElemHost(config_list, CONF_name, unique_name));

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(conf);
}

/*
 * Modify configuration with name 'aConfName'. 'anOldConf' is a *COPY* of this
 * configuration. 'aNewConf' is the new configuration.
 *
 * NOTE: Either 'anOldConf' or 'aNewConf' could be an empty configuration!
 * Empty configurations do not contain any 'CONF_entries'.
 */
static int
do_mod_config(char *aConfName, lListElem *anOldConf, lListElem *aNewConf, lList **anAnswer, u_long64 gdi_session) {
   DENTER(TOP_LAYER);
   const lList *old_entries = lGetList(anOldConf, CONF_entries);
   lList *new_entries = lGetListRW(aNewConf, CONF_entries);

   // VALIDATION AND ADJUSTMENTS

   // check if gdi_request_limits are correct
   const lListElem *gdi_request_limits_cfg = lGetElemStr(new_entries, CF_name, "gdi_request_limits");
   if (gdi_request_limits_cfg != nullptr) {
      const char *gdi_request_limit_str = lGetString(gdi_request_limits_cfg, CF_value);
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

/*
 * Static configuration entries may be changed at runtime with a warning.
 */
static int
check_static_conf_entries(const lList *theOldConfEntries, const lList *theNewConfEntries, lList **anAnswer) {
   int entry_idx = 0;

   DENTER(TOP_LAYER);

   while (nullptr != Static_Conf_Entries[entry_idx]) {
      const lListElem *old_entry, *new_entry = nullptr;
      const char *old_value, *new_value = nullptr;
      const char *entry_name = Static_Conf_Entries[entry_idx];

      old_entry = lGetElemStr(theOldConfEntries, CF_name, entry_name);
      new_entry = lGetElemStr(theNewConfEntries, CF_name, entry_name);

      if ((nullptr != old_entry) && (nullptr != new_entry)) {
         old_value = lGetString(old_entry, CF_value);
         new_value = lGetString(new_entry, CF_value);

         if (((nullptr != old_value) && (nullptr != new_value)) && (strcmp(old_value, new_value) != 0)) {
            /* log in qmaster messages file */
            WARNING(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, entry_name);
            /*   INFO(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, entry_name); */
            answer_list_add(anAnswer, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }
      } else if ((nullptr != old_entry) != (nullptr != new_entry)) {
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

/*
 * Remove configuration 'aConfName' from cluster configuration and append 'aNewConf' to it. 
 *
 * NOTE: 'anOldConf' is a *COPY* of the old configuration entry.
 */
static int
exchange_conf_by_name(char *aConfName, lListElem *anOldConf, lListElem *aNewConf, lList **anAnswer, u_long64 gdi_session) {
   lListElem *elem = nullptr;
   u_long32 old_version, new_version = 0;
   const char *old_conf_name = lGetHost(anOldConf, CONF_name);
   lList *config_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   old_version = lGetUlong(anOldConf, CONF_version);

   new_version = (old_version + 1);

   lSetUlong(aNewConf, CONF_version, new_version);

   /* Make sure, 'aNewConf' does have a unique name */
   lSetHost(aNewConf, CONF_name, old_conf_name);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   elem = lGetElemHostRW(config_list, CONF_name, old_conf_name);

   lRemoveElem(config_list, &elem);

   elem = lCopyElem(aNewConf);

   lAppendElem(config_list, elem);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   sge_event_spool(anAnswer, 0, sgeE_CONFIG_MOD, 0, 0, old_conf_name, nullptr, nullptr, elem, nullptr, nullptr, true, true, gdi_session);

   DRETURN(0);
}

static bool
has_reschedule_unknown_change(const lList *theOldConfEntries, const lList *theNewConfEntries) {
   const lListElem *old_elem = nullptr;
   const lListElem *new_elem = nullptr;
   const char *old_value = nullptr;
   const char *new_value = nullptr;
   bool res = false;

   DENTER(TOP_LAYER);

   old_elem = lGetElemStr(theOldConfEntries, CF_name, "reschedule_unknown");
   new_elem = lGetElemStr(theNewConfEntries, CF_name, "reschedule_unknown");

   old_value = (nullptr != old_elem) ? lGetString(old_elem, CF_value) : nullptr;
   new_value = (nullptr != new_elem) ? lGetString(new_elem, CF_value) : nullptr;

   if ((nullptr == old_value) || (nullptr == new_value)) {
      res = true;  /* change by omission in one configuration */
   } else if (((nullptr != old_value) && (nullptr != new_value)) && (strcmp(old_value, new_value) != 0)) {
      res = true;  /* value did change */
   }

   if (true == res) {
      DPRINTF("%s: reschedule_unknown did change!\n", __func__);
   }

   DRETURN(res);
}

static int
do_add_config(char *aConfName, lListElem *aConf, lList **anAnswer, u_long64 gdi_session) {
   lListElem *elem = nullptr;
   lList *config_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   elem = lCopyElem(aConf);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   lAppendElem(config_list, elem);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   sge_event_spool(anAnswer, 0, sgeE_CONFIG_ADD, 0, 0, aConfName, nullptr, nullptr, elem, nullptr, nullptr, true, true, gdi_session);

   DRETURN(0);
}

static int
remove_conf_by_name(char *aConfName) {
   lListElem *elem = nullptr;
   lList *config_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   elem = lGetElemHostRW(config_list, CONF_name, aConfName);

   lRemoveElem(config_list, &elem);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   DRETURN(0);
}
