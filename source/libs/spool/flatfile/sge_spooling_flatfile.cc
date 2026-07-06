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

/* system */
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>

#include "cull/cull.h"

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_dstring.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_hostname.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_DataStore.h"

#include "uti/sge.h"

/* includes for old job spooling */
#include "spool/sge_dirent.h"
#include "spool/sge_spooling_utilities.h"
#include "spool/msg_spoollib.h"
#include "spool/classic/read_write_job.h"
#include "spool/classic/read_write_ar.h"
#include "spool/flatfile/msg_spoollib_flatfile.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_spooling_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "basis_types.h"
#include "msg_common.h"

static const char *spooling_method = "classic";

#ifdef SPOOLING_classic
const char *get_spooling_method()
#else
const char *get_classic_spooling_method()
#endif
{
   return spooling_method;
}

static bool write_manop(int spool, ocs::gdi::Target target);
static bool read_manop(ocs::gdi::Target target);

/**
 * @brief Create a classic (flatfile) spooling context.
 *
 * Creates a single spooling rule bound to SGE_TYPE_ALL, so every object type is
 * spooled into the qmaster spool directory. This now includes the configuration
 * objects - the global configuration, the per-host local configurations and the
 * scheduler configuration - which used to be spooled into a separate common
 * directory. They no longer have a type-specific rule and therefore fall back to
 * the SGE_TYPE_ALL rule.
 *
 * The argument is the absolute path of the spool directory. The legacy
 * two-argument form "<common_dir>;<spool_dir>" is no longer supported and is
 * rejected with an error; a bootstrap file still carrying it must be migrated to
 * a single spool directory path.
 *
 * @param[out] answer_list  to return error messages
 * @param[in]  args         absolute spool directory path (the legacy
 *                          "<common_dir>;<spool_dir>" form is rejected)
 * @return on success the new spooling context, else nullptr
 */
lListElem *
spool_classic_create_context(lList **answer_list, const char *args)
{
   lListElem *context = nullptr;

   DENTER(TOP_LAYER);

   /* check parameter - must be set and be an absolute spool directory path */
   if (args == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_SPOOL_INCORRECTSPOOLDIRPATH);
   } else {
      char *first_token, *second_token, *spool_dir;
      struct saved_vars_s *strtok_context = nullptr;

      /* The classic spooling parameter is the absolute path of the spool
       * directory. The legacy form "<common_dir>;<spool_dir>", where the
       * configuration was spooled into a separate common directory, is no longer
       * supported: configuration now lives in the spool directory, so a
       * bootstrap file still carrying the two-argument form must be migrated and
       * is rejected here rather than silently ignoring the common directory. */
      first_token  = sge_strtok_r(args, ";", &strtok_context);
      second_token = sge_strtok_r(nullptr, ";", &strtok_context);
      spool_dir = first_token;

      if (second_token != nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_SPOOL_LEGACYCLASSICPARAMS);
      } else if (spool_dir == nullptr || *spool_dir != '/') {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_SPOOL_INCORRECTSPOOLDIRPATH);
      } else {
         int i;
         flatfile_info *field_info;
         lListElem *rule, *type;

         /* create info which fields to spool once */
         field_info = (flatfile_info *)sge_malloc(sizeof(flatfile_info) * SGE_TYPE_ALL);
         for (i = (int)SGE_TYPE_ADMINHOST; i < (int)SGE_TYPE_ALL; i++) {
            switch (i) {
               /* pseudo types without spooling action */
               // @see spool_classic_default_shutdown_func
               case SGE_TYPE_JOB_SCHEDD_INFO:
               case SGE_TYPE_SCHEDD_MONITOR:
               case SGE_TYPE_SHUTDOWN:
               case SGE_TYPE_MARK_4_REGISTRATION:
                  field_info[i].fields = nullptr;
                  field_info[i].instr  = nullptr;
                  break;
               case SGE_TYPE_CQUEUE:
                  field_info[i].fields = CQ_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_ADMINHOST:
                  field_info[i].fields = AH_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_SUBMITHOST:
                  field_info[i].fields = SH_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_HGROUP:
                  field_info[i].fields = HGRP_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_CALENDAR:
                  field_info[i].fields = CAL_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_QINSTANCE:
                  field_info[i].fields = sge_build_QU_field_list(false, true);
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_USERSET:
                  field_info[i].fields = US_fields;
                  field_info[i].instr  = &qconf_param_sfi;
                  break;
               /* standard case of spooling */
               case SGE_TYPE_SCHEDD_CONF:
                  field_info[i].fields = spool_get_fields_to_spool(answer_list,
                                              object_type_get_descr((sge_object_type)i),
                                              &spool_config_instr);
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_MANAGER:
               case SGE_TYPE_OPERATOR:
                  field_info[i].fields = nullptr;
                  field_info[i].instr  = nullptr;
                  break;
               case SGE_TYPE_EXECHOST:
                  field_info[i].fields = sge_build_EH_field_list(true, false, false);
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_PE:
                  field_info[i].fields = PE_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_RL:
                  field_info[i].fields = RL_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_CKPT:
                  field_info[i].fields = CK_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_CONFIG:
               /* special case config spooling */
                  field_info[i].fields = sge_build_CONF_field_list(true);
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_PROJECT:
                  field_info[i].fields = sge_build_PR_field_list(true);
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_USER:
                  field_info[i].fields = sge_build_UU_field_list(true);
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_SHARETREE:
                  field_info[i].fields = sge_build_STN_field_list(true, true);
                  field_info[i].instr  = &qconf_name_value_list_sfi;
                  break;
               case SGE_TYPE_CENTRY:
                  field_info[i].fields = CE_fields;
                  field_info[i].instr  = &qconf_sfi;
                  break;
               case SGE_TYPE_RQS:
                  field_info[i].fields = RQS_fields;
                  field_info[i].instr  = &qconf_rqs_sfi;
                  break;
               case SGE_TYPE_AR:
               case SGE_TYPE_JOB:
               case SGE_TYPE_JATASK:
               case SGE_TYPE_PETASK:
               default:
                  break;
            }
         }

         /* create spooling context */
         context = spool_create_context(answer_list, "flatfile spooling");

         /* create rule and type for all objects spooled in the spool dir */
         rule = spool_context_create_rule(answer_list, context,
                                          "default rule (spool dir)",
                                          spool_dir,
                                          nullptr,
                                          spool_classic_default_startup_func,
                                          spool_classic_default_shutdown_func,
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          spool_classic_default_list_func,
                                          spool_classic_default_read_func,
                                          nullptr, // @todo read keys from flatfile spooling db
                                          spool_classic_default_write_func,
                                          spool_classic_default_delete_func,
                                          spool_default_validate_func,
                                          spool_default_validate_list_func);
         lSetRef(rule, SPR_clientdata, field_info);
         type = spool_context_create_type(answer_list, context, SGE_TYPE_ALL);
         spool_type_add_rule(answer_list, type, rule, true);

         /* SGE_TYPE_CONFIG and SGE_TYPE_SCHEDD_CONF are deliberately not given
          * their own rule any more: with no type-specific rule they fall back to
          * the SGE_TYPE_ALL rule above (see the rule lookup in the read/write/
          * list/delete functions) and are therefore spooled into the spool
          * directory - the global configuration and scheduler configuration as
          * single files at the spool dir root, the per-host configurations in
          * the local_conf subdirectory. They no longer use the common
          * directory. */
      }
      sge_free_saved_vars(strtok_context);
   }

   DRETURN(context);
}

/**
 * @brief Set up the classic (flatfile) qmaster spool directory.
 *
 * Checks that the spool directory exists, changes into it, and creates the
 * per-object-type subdirectories if they do not yet exist. This includes the
 * local_conf subdirectory holding the per-host configurations; the global
 * configuration and scheduler configuration are single files at the spool
 * directory root and need no subdirectory.
 *
 * The subdirectories are created owner-only (0700): they hold authoritative
 * job and configuration spool, and their entry names alone (job IDs, queue
 * instance and other object names) are information that must not be readable
 * by other local users. This complements CS-2352, which hardened the spool
 * *file* mode to 0600 but left the containing directories world-traversable.
 *
 * This function should not be called directly; it is called by the spooling
 * framework.
 *
 * @param[out] answer_list  to return error messages
 * @param[in]  rule         rule containing data necessary for the startup
 *                          (e.g. the path to the spool directory)
 * @param[in]  check        check the spooling database
 * @return true if the startup succeeded, else false
 */
bool
spool_classic_default_startup_func(lList **answer_list,
                                    const lListElem *rule, bool check)
{
   bool ret = true;
   const char *url;

   DENTER(TOP_LAYER);

   /* check spool directory */
   url = lGetString(rule, SPR_url);
   if (!sge_is_directory(url)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_SPOOL_SPOOLDIRDOESNOTEXIST_S, url);
      ret = false;
   } else {
      if (sge_chdir(url) != 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_ERRORCHANGINGCWD_SS, url,
                                 strerror(errno));
        ret = false;
      } else {
         /* Create spool sub directories owner-only (0700). Their entry names
          * alone leak job IDs and object names, so they must not be readable
          * or traversable by group/other (CS-2352 directory hardening). */
         sge_mkdir2(url, JOB_DIR,  0700, true);
         sge_mkdir2(url, CQUEUE_DIR,  0700, true);
         sge_mkdir2(url, QINSTANCES_DIR,  0700, true);
         sge_mkdir2(url, EXECHOST_DIR, 0700, true);
         sge_mkdir2(url, SUBMITHOST_DIR, 0700, true);
         sge_mkdir2(url, ADMINHOST_DIR, 0700, true);
         sge_mkdir2(url, CENTRY_DIR, 0700, true);
         sge_mkdir2(url, EXEC_DIR, 0700, true);
         sge_mkdir2(url, PE_DIR, 0700, true);
         sge_mkdir2(url, CKPTOBJ_DIR, 0700, true);
         sge_mkdir2(url, USERSET_DIR, 0700, true);
         sge_mkdir2(url, CAL_DIR, 0700, true);
         sge_mkdir2(url, HGROUP_DIR, 0700, true);
         sge_mkdir2(url, USER_DIR, 0700, true);
         sge_mkdir2(url, PROJECT_DIR, 0700, true);
         sge_mkdir2(url, RESOURCEQUOTAS_DIR, 0700, true);
         sge_mkdir2(url, AR_DIR, 0700, true);
         sge_mkdir2(url, ROLE_DIR, 0700, true);
         sge_mkdir2(url, LOCAL_CONF_DIR, 0700, true);
      }
   }

   DRETURN(ret);
}

bool
spool_classic_default_shutdown_func(lList **answer_list,
                                    const lListElem *rule)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   auto field_info = static_cast<flatfile_info *>(lGetRef(rule, SPR_clientdata));
   for (auto i = static_cast<int>(SGE_TYPE_ADMINHOST); i < static_cast<int>(SGE_TYPE_ALL); i++) {
      // for most type we use static fields but for some we need to free the dynamic fields
      // @see spool_classic_create_context
      switch (i) {
         case SGE_TYPE_QINSTANCE:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
         case SGE_TYPE_SCHEDD_CONF:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
         case SGE_TYPE_EXECHOST:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
         case SGE_TYPE_CONFIG:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
         case SGE_TYPE_PROJECT:
            field_info[i].fields= spool_free_spooling_fields(field_info[i].fields);
            break;
         case SGE_TYPE_USER:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
         case SGE_TYPE_SHARETREE:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
         default:
            break;
      }
   }
   sge_free(&field_info);

   DRETURN(ret);
}

/**
 * @brief Set up the classic (flatfile) common directory.
 *
 * Checks that the common directory exists and creates the subdirectory for the
 * local (per-host) and global configuration spool if it does not yet exist.
 *
 * The local_conf subdirectory is created owner-only (0700). It is written and
 * read exclusively by qmaster (clients obtain their configuration via GDI, not
 * from disk), and its entry names reveal which hosts carry a local config, so
 * it must not be readable by group/other. The parent common directory itself
 * stays world-readable so clients can still read act_qmaster/bootstrap/settings.
 *
 * This function should not be called directly; it is called by the spooling
 * framework.
 *
 * @param[out] answer_list  to return error messages
 * @param[in]  rule         rule containing data like the path to the common
 *                          directory
 * @param[in]  check        check the spooling database
 * @return true on success, else false
 */
bool
spool_classic_common_startup_func(lList **answer_list,
                                   const lListElem *rule, bool check)
{
   bool ret = true;
   const char *url;

   DENTER(TOP_LAYER);

   /* check common directory */
   url = lGetString(rule, SPR_url);
   if (!sge_is_directory(url)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_SPOOL_COMMONDIRDOESNOTEXIST_S, url);
      ret = false;
   } else {
      /* Create directory for local/global configuration owner-only (0700);
       * qmaster-private, entry names leak per-host config (CS-2352). */
      sge_mkdir2(url, LOCAL_CONF_DIR, 0700, true);
   }

   DRETURN(ret);
}

static bool read_validate_object(lList **answer_list,
                   const lListElem *type, const lListElem *rule,
                   const char *key, int key_nm,
                   sge_object_type object_type, lList **master_list)
{
   bool ret = true;
   lListElem *ep;

   DENTER(TOP_LAYER);

   DPRINTF("reading " SFN " " SFQ "\n", object_type_get_name(object_type), key);

   ep = spool_classic_default_read_func(answer_list, type, rule, key,
                                         object_type);
   if (ep == nullptr) {
      ret = false;
   } else {
      spooling_validate_func validate_func;

      /* set key from filename */
      if (key_nm != NoName) {
         object_parse_field_from_string(ep, nullptr, key_nm, key);
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      /* validate object */
      validate_func = (spooling_validate_func)lGetRef(rule, SPR_validate_func);
#pragma GCC diagnostic pop
      if (validate_func != nullptr) {
         if (!validate_func(answer_list, type, rule, ep, object_type)) {
            lFreeElem(&ep);
            ret = false;
         }
      }

      /* object read correctly and validate succeeded */
      if (ep != nullptr) {
         lAppendElem(*master_list, ep);
      }
   }

   DRETURN(ret);
}

/****** spool/flatfile/spool_flatfile_default_list_func() *****************
*  NAME
*     spool_flatfile_default_list_func() -- read lists through flatfile spooling
*
*  SYNOPSIS
*     bool
*     spool_flatfile_default_list_func(lList **answer_list,
*                                      const lListElem *type,
*                                      const lListElem *rule,
*                                      lList **list,
*                                      const sge_object_type object_type)
*
*  FUNCTION
*     Depending on the object type given, calls the appropriate functions
*     reading the correspondent list of objects using the old spooling
*     functions.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to be used
*     lList **list                    - target list
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true, on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/flatfile/--Flatfile-Spooling
*     spool/spool_read_list()
*******************************************************************************/
bool
spool_classic_default_list_func(lList **answer_list,
                                 const lListElem *type,
                                 const lListElem *rule,
                                 lList **list,
                                 const sge_object_type object_type)
{
   const lDescr *descr;

   const char *filename  = nullptr;
   const char *directory = nullptr;
   const char *url = nullptr;
   int key_nm = NoName;

   bool ret = true;

   DENTER(TOP_LAYER);

   if (!list) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                     ANSWER_QUALITY_WARNING,
                     "Cannot read in list because target list is missing\n");
      ret = false;
   } else {
      url = lGetString(rule, SPR_url);
      descr = object_type_get_descr(object_type);

      if (*list == nullptr && descr != nullptr) {
         *list = lCreateList("master list", descr);
      }

      switch(object_type) {
         case SGE_TYPE_ADMINHOST:
            directory = ADMINHOST_DIR;
            break;
         case SGE_TYPE_CALENDAR:
            directory = CAL_DIR;
            break;
         case SGE_TYPE_CKPT:
            directory = CKPTOBJ_DIR;
            break;
         case SGE_TYPE_CONFIG:
            key_nm    = CONF_name;
            filename  = "global";
            directory = LOCAL_CONF_DIR;
            break;
         case SGE_TYPE_EXECHOST:
            directory = EXECHOST_DIR;
            break;
         case SGE_TYPE_MANAGER:
            ret = read_manop(ocs::gdi::Target::UM_LIST);
            break;
         case SGE_TYPE_OPERATOR:
            ret = read_manop(ocs::gdi::Target::UO_LIST);
            break;
         case SGE_TYPE_PE:
            directory = PE_DIR;
            break;
         case SGE_TYPE_RL:
            directory = ROLE_DIR;
            break;
         case SGE_TYPE_QINSTANCE:
            directory = QINSTANCES_DIR;
            /* JG: TODO: we'll have to quicksort the queue list, see
             * function queue_list_add_queue
             */
            break;
         case SGE_TYPE_CQUEUE:
            directory = CQUEUE_DIR;
            /* JG: TODO: we'll have to quicksort the queue list, see
             * function cqueue_list_add_cqueue
             */
            break;
         case SGE_TYPE_SUBMITHOST:
            directory = SUBMITHOST_DIR;
            break;
         case SGE_TYPE_USERSET:
            directory = USERSET_DIR;
            break;
         case SGE_TYPE_HGROUP:
            directory = HGROUP_DIR;
            break;
         case SGE_TYPE_PROJECT:
            directory = PROJECT_DIR;
            break;
         case SGE_TYPE_USER:
            directory = USER_DIR;
            break;
         case SGE_TYPE_SHARETREE:
            filename = SHARETREE_FILE;
            break;
         case SGE_TYPE_SCHEDD_CONF:
            filename = SCHED_CONF_FILE;
            break;
         case SGE_TYPE_RQS:
             directory = RESOURCEQUOTAS_DIR;
             break;
         case SGE_TYPE_AR:
            ocs::spool::classic::ar_list_read_from_disk(list, "master ar list");
            break;
         case SGE_TYPE_CENTRY:
             directory = CENTRY_DIR;
             break;
         case SGE_TYPE_JOB:
            job_list_read_from_disk(list, "master job list", 0, SPOOL_DEFAULT, nullptr);
            break;
         default:
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_WARNING,
                                    MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S,
                                    object_type_get_name(object_type));
            ret = false;
            break;
      }

      /* if all necessary data has been initialized */
      if (url != nullptr && list != nullptr && descr != nullptr) {
         /* single file to parse (SHARETREE, global config, schedd config */
         if (filename != nullptr) {
            ret = read_validate_object(answer_list, type, rule, filename, key_nm,
                                     object_type, list);
         }

         /* if we have a directory (= multiple files) to parse */
         if (ret && directory != nullptr) {
            lList *direntries;
            char abs_dir_buf[SGE_PATH_MAX];
            dstring abs_dir_dstring;
            const char *abs_dir;

            sge_dstring_init(&abs_dir_dstring, abs_dir_buf, SGE_PATH_MAX);
            abs_dir = sge_dstring_sprintf(&abs_dir_dstring, "%s/%s", url,
                                          directory);

            direntries = sge_get_dirents(abs_dir);

            for_each_ep_lv(direntry, direntries) {
               const char *key = lGetString(direntry, ST_name);

               if (key[0] != '.') {
                  ret &= read_validate_object(answer_list, type, rule, key, key_nm,
                                           object_type, list);
               }
            }
            lFreeList(&direntries);
         }
      }

      switch(object_type) {
         case SGE_TYPE_CQUEUE:
            {
               const lListElem *context = spool_get_default_context();
               lListElem *type = spool_context_search_type(context, SGE_TYPE_QINSTANCE);
               lListElem *rule = spool_type_search_default_rule(type);
               const char *url = lGetString(rule, SPR_url);

               dstring key = DSTRING_INIT;
               dstring dir = DSTRING_INIT;

               for_each_rw_lv(queue, *list) {
                  lList *direntries;
                  lList *qinstance_list = lCreateList("", QU_Type);

                  sge_dstring_sprintf(&dir, "%s/%s/%s", url, QINSTANCES_DIR, lGetString(queue, CQ_name));
                  direntries = sge_get_dirents(sge_dstring_get_string(&dir));
                  for_each_rw_lv(direntry, direntries) {
                     const char *directory = lGetString(direntry, ST_name);
                     if (directory[0] != '.') {
                        sge_dstring_sprintf(&key, "%s/%s", lGetString(queue, CQ_name), directory);
                        read_validate_object(answer_list, type, rule, sge_dstring_get_string(&key), NoName,
                                           SGE_TYPE_QINSTANCE, &qinstance_list);
                     }
                  }
                  lFreeList(&direntries);

                  lSetList(queue, CQ_qinstances, qinstance_list);
               }
               sge_dstring_free(&dir);
               sge_dstring_free(&key);
            }
            break;
         default:
            break;
      }


#ifdef DEBUG_FLATFILE
      if (list != nullptr && *list != nullptr) {
         lWriteListTo(*list, stderr);
      }
#endif

      /* validate the complete list */
      if (ret) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
         spooling_validate_list_func validate_list =
            (spooling_validate_list_func)lGetRef(rule, SPR_validate_list_func);
#pragma GCC diagnostic pop

         ret = validate_list(answer_list, type, rule, object_type);
      }
   }
   DRETURN(ret);
}

/****** spool/flatfile/spool_flatfile_default_read_func() *****************
*  NAME
*     spool_flatfile_default_read_func() -- read objects using flatfile spooling
*
*  SYNOPSIS
*     lListElem*
*     spool_flatfile_default_read_func(lList **answer_list,
*                                      const lListElem *type,
*                                      const lListElem *rule,
*                                      const char *key,
*                                      const sge_object_type object_type)
*
*  FUNCTION
*     Reads an individual object by calling the appropriate flatfile spooling
*     function.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const char *key                 - unique key specifying the object
*     const sge_object_type object_type - object type
*
*  RESULT
*     lListElem* - the object, if it could be read, else nullptr
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/flatfile/--Flatfile-Spooling
*     spool/spool_read_object()
*******************************************************************************/
lListElem *
spool_classic_default_read_func(lList **answer_list,
                                 const lListElem *type,
                                 const lListElem *rule,
                                 const char *key,
                                 const sge_object_type object_type)
{
   const char *url = nullptr;
   const char *directory = nullptr;
   const char *filename = nullptr;
   const lDescr *descr = nullptr;
   flatfile_info *rule_clientdata;
   flatfile_info *field_info;
   lListElem *ep = nullptr;
   bool parse_values = true;

   DENTER(TOP_LAYER);

   rule_clientdata = (flatfile_info *)lGetRef(rule, SPR_clientdata);
   field_info = &(rule_clientdata[object_type]);
   url = lGetString(rule, SPR_url);
   descr = object_type_get_descr(object_type);

   /* prepare filenames */
   switch(object_type) {
      case SGE_TYPE_ADMINHOST:
         directory = ADMINHOST_DIR;
         filename  = key;
         break;
      case SGE_TYPE_CALENDAR:
         directory = CAL_DIR;
         filename = key;
         break;
      case SGE_TYPE_CKPT:
         directory = CKPTOBJ_DIR;
         filename = key;
         break;
      case SGE_TYPE_CONFIG:
         parse_values = false;
         if (sge_hostcmp(key, "global") == 0) {
            directory = ".";
            filename  = CONF_FILE;
         } else {
            directory = LOCAL_CONF_DIR;
            filename  = key;
         }
         break;
      case SGE_TYPE_EXECHOST:
         directory = EXECHOST_DIR;
         filename  = key;
         break;
      case SGE_TYPE_MANAGER:
      case SGE_TYPE_OPERATOR:
         directory = nullptr;
         filename  = nullptr;
         break;
      case SGE_TYPE_PE:
         directory = PE_DIR;
         filename = key;
         break;
      case SGE_TYPE_RL:
         directory = ROLE_DIR;
         filename = key;
         break;
      case SGE_TYPE_CQUEUE:
         directory = CQUEUE_DIR;
         filename  = key;
         break;
      case SGE_TYPE_QINSTANCE:
         directory = QINSTANCES_DIR;
         filename  = key;
         break;
      case SGE_TYPE_SUBMITHOST:
         directory = SUBMITHOST_DIR;
         filename  = key;
         break;
      case SGE_TYPE_USERSET:
         directory = USERSET_DIR;
         filename  = key;
         break;
      case SGE_TYPE_HGROUP:
         directory = HGROUP_DIR;
         filename  = key;
         break;
      case SGE_TYPE_PROJECT:
         directory = PROJECT_DIR;
         filename  = key;
         break;
      case SGE_TYPE_USER:
         directory = USER_DIR;
         filename  = key;
         break;
      case SGE_TYPE_SHARETREE:
         directory = ".";
         filename  = SHARETREE_FILE;
         break;
      case SGE_TYPE_SCHEDD_CONF:
         directory = ".";
         filename  = SCHED_CONF_FILE;
         break;
      case SGE_TYPE_RQS:
         directory = RESOURCEQUOTAS_DIR;
         filename  = key;
         break;
      case SGE_TYPE_AR:
         ep = ocs::spool::classic::ar_read_spool_file(SGE_STRTOU_LONG32(key));
         break;
      case SGE_TYPE_CENTRY:
         directory = CENTRY_DIR;
         filename  = key;
         break;
      case SGE_TYPE_JOBSCRIPT:
         {
            const char *exec_file = nullptr;
            char *dup=strdup(key);
            jobscript_parse_key(dup, &exec_file);
            if (exec_file != nullptr ) {
               int len;
               char *str = sge_file2string(exec_file, &len);
               if (str != nullptr) {
                  ep = lCreateElem(STU_Type);
                  lXchgString(ep, STU_name, &str);
               }
            }
            sge_free(&dup);
         }
         break;
      case SGE_TYPE_JOB:
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_WARNING,
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S,
                                 object_type_get_name(object_type));
         break;
   }

   /* spool, if possible */
   if (url != nullptr && directory != nullptr && filename != nullptr && descr != nullptr) {
      dstring filepath_dstring = DSTRING_INIT;
      const char *filepath;

      filepath = sge_dstring_sprintf(&filepath_dstring, "%s/%s/%s",
                                     url, directory, filename);

      /* spool */
      ep = spool_flatfile_read_object(answer_list, descr, nullptr,
                                      field_info->fields, nullptr,
                                      parse_values, field_info->instr, SP_FORM_ASCII,
                                      nullptr, filepath);

      sge_dstring_free(&filepath_dstring);
   } else {
      DPRINTF("error: one of the required parameters is nullptr\n");
   }

   DRETURN(ep);
}

/****** spool/flatfile/spool_flatfile_default_write_func() ****************
*  NAME
*     spool_flatfile_default_write_func() -- write object with flatfile spooling
*
*  SYNOPSIS
*     bool
*     spool_flatfile_default_write_func(lList **answer_list,
*                                       const lListElem *type,
*                                       const lListElem *rule,
*                                       const lListElem *object,
*                                       const char *key,
*                                       const sge_object_type object_type)
*
*  FUNCTION
*     Writes an object through the appropriate flatfile spooling functions.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const lListElem *object         - object to spool
*     const char *key                 - unique key
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/flatfile/--Flatfile-Spooling
*     spool/spool_delete_object()
*******************************************************************************/

/**
 * @brief Check that a spool object key is a safe relative spool path.
 *
 * A key is used verbatim to build a spool-file path (EXECHOST_DIR/<key> etc.).
 * Most keys are validated upstream by verify_str_key(), but host names are not,
 * so this is a defence-in-depth guard at the spool sink (CS-2364, CWE-22).
 *
 * A key may be a multi-component relative path: a queue instance is spooled
 * under "<cqueue>/<host>", so '/' is a legitimate separator and must not be
 * rejected outright. Instead every component is validated independently — the
 * key must be relative (no leading '/') and no component may be empty, "." or
 * ".." or start with '.' — so the resulting path can never escape the spool
 * directory.
 *
 * @param[in] key the object key (a relative path of one or more components)
 * @return true if @p key is safe to use as a relative spool path, false otherwise
 */
bool
spool_flatfile_key_is_safe(const char *key) {
   // Reject nullptr, empty, or absolute keys outright.
   if (key == nullptr || key[0] == '\0' || key[0] == '/') {
      return false;
   }
   // Validate each '/'-separated component: none may be empty (handles "//" and
   // a trailing '/'), nor "." / ".." / a leading-dot name (handles "comp[0] == '.'").
   for (const char *comp = key; comp != nullptr; ) {
      if (comp[0] == '\0' || comp[0] == '.' || comp[0] == '/') {
         return false;
      }
      const char *slash = strchr(comp, '/');
      comp = (slash != nullptr) ? slash + 1 : nullptr;
   }
   return true;
}

bool
spool_classic_default_write_func(lList **answer_list,
                                  const lListElem *type,
                                  const lListElem *rule,
                                  const lListElem *object,
                                  const char *key,
                                  const sge_object_type object_type)
{
   const char *url = nullptr;
   const char *directory = nullptr;
   const char *filename = nullptr;
   const char *filepath = nullptr;
   flatfile_info *rule_clientdata;
   flatfile_info *field_info;
   dstring tmp = DSTRING_INIT;
   bool ret = true;

   DENTER(TOP_LAYER);

   rule_clientdata = (flatfile_info *)lGetRef(rule, SPR_clientdata);
   field_info = &(rule_clientdata[object_type]);
   url = lGetString(rule, SPR_url);

   /* prepare filenames */
   switch(object_type) {
      case SGE_TYPE_ADMINHOST:
         directory = ADMINHOST_DIR;
         filename  = key;
         break;
      case SGE_TYPE_CALENDAR:
         directory = CAL_DIR;
         filename = key;
         break;
      case SGE_TYPE_CKPT:
         directory = CKPTOBJ_DIR;
         filename = key;
         break;
      case SGE_TYPE_CONFIG:
         if (sge_hostcmp(key, "global") == 0) {
            directory = ".";
            filename  = CONF_FILE;
         } else {
            directory = LOCAL_CONF_DIR;
            filename  = key;
         }
         break;
      case SGE_TYPE_EXECHOST:
         directory = EXECHOST_DIR;
         filename  = key;
         break;
      case SGE_TYPE_MANAGER:
         ret = write_manop(1, ocs::gdi::Target::UM_LIST);
         break;
      case SGE_TYPE_OPERATOR:
         ret = write_manop(1, ocs::gdi::Target::UO_LIST);
         break;
      case SGE_TYPE_PE:
         directory = PE_DIR;
         filename = key;
         break;
      case SGE_TYPE_RL:
         directory = ROLE_DIR;
         filename = key;
         break;
      case SGE_TYPE_CQUEUE:
         {
            dstring qi_dir = DSTRING_INIT;
            sge_dstring_sprintf(&qi_dir, "%s/%s", QINSTANCES_DIR, key);
            // owner-only: the qinstance dir name leaks the queue/host (CS-2352)
            sge_mkdir(sge_dstring_get_string(&qi_dir), 0700, false, false);
            sge_dstring_free(&qi_dir);

         }
         directory = CQUEUE_DIR;
         filename  = key;
         break;
      case SGE_TYPE_QINSTANCE:
         sge_free(&directory);
         directory = sge_dstring_sprintf(&tmp, "%s/%s", QINSTANCES_DIR, lGetString(object, QU_qname));
         filename = lGetHost(object, QU_qhostname);
         break;
      case SGE_TYPE_SUBMITHOST:
         directory = SUBMITHOST_DIR;
         filename  = key;
         break;
      case SGE_TYPE_USERSET:
         directory = USERSET_DIR;
         filename  = key;
         break;
      case SGE_TYPE_HGROUP:
         directory = HGROUP_DIR;
         filename  = key;
         break;
      case SGE_TYPE_PROJECT:
         directory = PROJECT_DIR;
         filename  = key;
         break;
      case SGE_TYPE_USER:
         directory = USER_DIR;
         filename  = key;
         break;
      case SGE_TYPE_SHARETREE:
         directory = ".";
         filename  = SHARETREE_FILE;
         break;
      case SGE_TYPE_SCHEDD_CONF:
         directory = ".";
         filename  = SCHED_CONF_FILE;
         break;
      case SGE_TYPE_JOB:
      case SGE_TYPE_JATASK:
      case SGE_TYPE_PETASK:
         {
            uint32_t job_id, ja_task_id;
            char *pe_task_id;
            char *dup = strdup(key);
            bool only_job;
            int flags = SPOOL_DEFAULT;
            const lListElem *job;

            job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

            DPRINTF("spooling job %d.%d %s\n", job_id, ja_task_id,
                    pe_task_id != nullptr ? pe_task_id : "<null>");

            if (object_type == SGE_TYPE_JOB) {
               job = (lListElem *)object;

               /* we only want to spool the job object */
               if (only_job) {
                  flags |= SPOOL_IGNORE_TASK_INSTANCES;
               }
            } else {
               /* job_write_spool_file takes a job, even if we only want
                * to spool a ja_task or pe_task
                */
               job = lGetElemUlong(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), JB_job_number, job_id);

               /* additional flags for job_write_spool_file
                * to avoid spooling too many files
                */
               if (object_type == SGE_TYPE_PETASK) {
                  flags |= SPOOL_ONLY_PETASK;
               } else if (object_type == SGE_TYPE_JATASK) {
                  flags |= SPOOL_ONLY_JATASK;
               }
            }

            if (job_write_spool_file((lListElem *)job, ja_task_id, pe_task_id, (sge_spool_flags_t)flags) != 0) {
               ret = false;
            }

            sge_free(&dup);
         }
         break;
      case SGE_TYPE_JOBSCRIPT:
         ret = sge_string2file(lGetString(object, JB_script_ptr),
                               lGetUlong(object, JB_script_size),
                               lGetString(object, JB_exec_file)) ? false : true;
         break;
      case SGE_TYPE_RQS:
         directory = RESOURCEQUOTAS_DIR;
         filename  = key;
         break;
      case SGE_TYPE_AR:
         ocs::spool::classic::ar_write_spool_file(object);
         break;
      case SGE_TYPE_CENTRY:
         directory = CENTRY_DIR;
         filename  = key;
         break;
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_WARNING,
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S,
                                 object_type_get_name(object_type));
         ret = false;
         break;
   }

   /* CS-1597: for exec hosts only static load values shall be spooled;
    * strip the dynamic load values before writing and restore them afterwards
    */
   lList *backup_load_list = nullptr;
   if (object_type == SGE_TYPE_EXECHOST) {
      backup_load_list = spool_exechost_strip_dynamic_load(object);
   }

   /* Defence-in-depth: the key becomes a single filesystem path component below;
    * reject an unsafe key (e.g. an unvalidated host name like "../../tmp/x")
    * before it reaches the filesystem (CS-2364, CWE-22). */
   if (filename != nullptr && !spool_flatfile_key_is_safe(filename)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_SPOOL_UNSAFE_KEY_S, filename);
      ret = false;
      filename = nullptr;   /* skip the default spool path below */
   }

   /* spool, if possible, using default spooling behavior.
    * job are spooled in the corresponding case branch
    */
   if (url != nullptr && directory != nullptr && filename != nullptr) {
      const char *tmpfilepath = nullptr;
      dstring filepath_buffer = DSTRING_INIT;

      /* first write to a temporary file; for jobs it is already initialized */
      tmpfilepath = sge_dstring_sprintf(&filepath_buffer, "%s/%s/.%s",
                                        url, directory, filename);

      /* spool */
      tmpfilepath = spool_flatfile_write_object(answer_list, object, false,
                                                field_info->fields,
                                                field_info->instr,
                                                SP_DEST_SPOOL, SP_FORM_ASCII,
                                                tmpfilepath, true);

      if (tmpfilepath == nullptr) {
         ret = false;
      } else {
         /* spooling was ok: rename temporary to target file */
         filepath = sge_dstring_sprintf(&filepath_buffer, "%s/%s/%s",
                                        url, directory, filename);

         if (rename(tmpfilepath, filepath) == -1) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_ERROR,
                                    MSG_ERRORRENAMING_SSS,
                                    tmpfilepath, filepath, strerror(errno));
            ret = false;
         }
      }

      sge_free(&tmpfilepath);
      sge_dstring_free(&filepath_buffer);
   }

   /* CS-1597: restore the dynamic load values that were stripped above */
   if (object_type == SGE_TYPE_EXECHOST) {
      spool_exechost_restore_load_list(object, &backup_load_list);
   }

   sge_dstring_free(&tmp);

   DRETURN(ret);
}

/****** spool/flatfile/spool_flatfile_default_delete_func() ***************
*  NAME
*     spool_flatfile_default_delete_func() -- delete object in flatfile spooling
*
*  SYNOPSIS
*     bool
*     spool_flatfile_default_delete_func(lList **answer_list,
*                                        const lListElem *type,
*                                        const lListElem *rule,
*                                        const char *key,
*                                        const sge_object_type object_type)
*
*  FUNCTION
*     Deletes an object in the flatfile spooling.
*     In most cases, the correspondent spool file is deleted, in some cases
*     (e.g. jobs), a special remove function is called.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const char *key                 - unique key
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*  SEE ALSO
*     spool/flatfile/--Flatfile-Spooling
*     spool/spool_delete_object()
*******************************************************************************/
bool
spool_classic_default_delete_func(lList **answer_list,
                                   const lListElem *type,
                                   const lListElem *rule,
                                   const char *key,
                                   const sge_object_type object_type)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   /* Defence-in-depth: key is used to build the unlink path; reject an unsafe
    * key (e.g. an unvalidated host name like "../../tmp/x") so a delete cannot
    * escape the spool directory (CS-2364, CWE-22). No legitimate object key
    * contains '/' or starts with '.'. */
   if (!spool_flatfile_key_is_safe(key)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_SPOOL_UNSAFE_KEY_S, key != nullptr ? key : "");
      DRETURN(false);
   }

   switch(object_type) {
      case SGE_TYPE_ADMINHOST:
         ret = sge_unlink(ADMINHOST_DIR, key);
         break;
      case SGE_TYPE_CALENDAR:
         ret = sge_unlink(CAL_DIR, key);
         break;
      case SGE_TYPE_CKPT:
         ret = sge_unlink(CKPTOBJ_DIR, key);
         break;
      case SGE_TYPE_CENTRY:
         ret = sge_unlink(CENTRY_DIR, key);
         break;
      case SGE_TYPE_CONFIG:
         if(sge_hostcmp(key, "global") == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_ERROR,
                                    MSG_SPOOL_GLOBALCONFIGNOTDELETED);
            ret = false;
         } else {
            dstring dir_name_dstring = DSTRING_INIT;
            const char *dir_name;

            dir_name = sge_dstring_sprintf(&dir_name_dstring, "%s/%s",
                                           lGetString(rule, SPR_url),
                                           LOCAL_CONF_DIR);
            ret = sge_unlink(dir_name, key);
            sge_dstring_free(&dir_name_dstring);
         }
         break;
      case SGE_TYPE_EXECHOST:
         ret = sge_unlink(EXECHOST_DIR, key);
         break;
      case SGE_TYPE_JOB:
      case SGE_TYPE_JATASK:
      case SGE_TYPE_PETASK:
         {
            uint32_t job_id, ja_task_id;
            char *pe_task_id;
            char *dup = strdup(key);
            bool only_job;

            job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

            DPRINTF("spooling job %d.%d %s\n", job_id, ja_task_id,
                    pe_task_id != nullptr ? pe_task_id : "<null>");
            if (job_remove_spool_file(job_id, ja_task_id, pe_task_id,
                                      SPOOL_DEFAULT) != 0) {
               ret = false;
            }
            sge_free(&dup);
         }
         break;
      case SGE_TYPE_JOBSCRIPT:
        {
            const char *exec_file;
            char *dup = strdup(key);
            jobscript_parse_key(dup, &exec_file);
            ret = (unlink(exec_file) != 0) ? false: true;
            sge_free(&dup);
         }
         break;
      case SGE_TYPE_MANAGER:
         write_manop(1, ocs::gdi::Target::UM_LIST);
         break;
      case SGE_TYPE_OPERATOR:
         write_manop(1, ocs::gdi::Target::UO_LIST);
         break;
      case SGE_TYPE_SHARETREE:
         ret = sge_unlink(nullptr, SHARETREE_FILE);
         break;
      case SGE_TYPE_PE:
         ret = sge_unlink(PE_DIR, key);
         break;
      case SGE_TYPE_RL:
         ret = sge_unlink(ROLE_DIR, key);
         break;
      case SGE_TYPE_PROJECT:
         ret = sge_unlink(PROJECT_DIR, key);
         break;
      case SGE_TYPE_CQUEUE:
         ret = sge_unlink(CQUEUE_DIR, key);
         break;
      case SGE_TYPE_QINSTANCE:
         ret = sge_unlink(QINSTANCES_DIR, key);
         break;
      case SGE_TYPE_SCHEDD_CONF:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_SPOOL_SCHEDDCONFIGNOTDELETED);
         ret = false;
         break;
      case SGE_TYPE_SUBMITHOST:
         ret = sge_unlink(SUBMITHOST_DIR, key);
         break;
      case SGE_TYPE_USER:
         ret = sge_unlink(USER_DIR, key);
         break;
      case SGE_TYPE_USERSET:
         ret = sge_unlink(USERSET_DIR, key);
         break;
      case SGE_TYPE_HGROUP:
         ret = sge_unlink(HGROUP_DIR, key);
         break;
      case SGE_TYPE_RQS:
         ret = sge_unlink(RESOURCEQUOTAS_DIR, key);
         break;
      case SGE_TYPE_AR:
         ret = sge_unlink(AR_DIR, key);
         break;
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_WARNING,
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S,
                                 object_type_get_name(object_type));
         ret = false;
         break;
   }

   DRETURN(ret);
}

/**
 * @brief Spool the managers or operators name list to its flat file.
 *
 * Writes the manager (MAN_FILE) or operator (OP_FILE) list to a dot-prefixed
 * temporary file in the current (qmaster spool) directory and renames it into
 * place. The file is created owner-only (0600): it enumerates the cluster's
 * administrative accounts, so group/other must have neither read access
 * (disclosure of who the admins are) nor write access (tampering with the
 * manager/operator list is a privilege-escalation vector). See CS-2352.
 *
 * @param[in] spool   if non-zero, prepend the standard version comment header
 * @param[in] target  UM_LIST for the manager list, UO_LIST for the operator list
 * @return true on success, false on error
 */
static bool write_manop(int spool, ocs::gdi::Target target) {
   DENTER(TOP_LAYER);
   FILE *fp;
   const lList *lp;
   char filename[255], real_filename[255];
   dstring ds = DSTRING_INIT;
   int key = NoName;

   switch (target) {
   case ocs::gdi::Target::UM_LIST:
      lp = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
      strcpy(filename, ".");
      strcat(filename, MAN_FILE);
      strcpy(real_filename, MAN_FILE);
      key = UM_name;
      break;

   case ocs::gdi::Target::UO_LIST:
      lp = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
      strcpy(filename, ".");
      strcat(filename, OP_FILE);
      strcpy(real_filename, OP_FILE);
      key = UO_name;
      break;

   default:
      DRETURN(false);
   }

   /* Create the spool file owner-only (0600): it lists administrative accounts,
    * so group/other must have neither read (disclosure) nor write (tampering ->
    * privilege escalation) access. O_NOFOLLOW defeats a symlink pre-placement on
    * the dot-temp. See CS-2352. */
   {
      int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW,
                    S_IRUSR | S_IWUSR);
      if (fd == -1) {
         ERROR(MSG_ERRORWRITINGFILE_SS, filename, strerror(errno));
         DRETURN(false);
      }
      fp = fdopen(fd, "w");
      if (!fp) {
         ERROR(MSG_ERRORWRITINGFILE_SS, filename, strerror(errno));
         close(fd);
         DRETURN(false);
      }
   }

   if (spool && sge_spoolmsg_write(fp, COMMENT_CHAR,
             feature_get_product_name(FS_VERSION, &ds)) < 0) {
      sge_dstring_free(&ds);
      goto FPRINTF_ERROR;
   }
   sge_dstring_free(&ds);

   for_each_ep_lv(ep, lp) {
      FPRINTF((fp, "%s\n", lGetString(ep, key)));
   }

   FCLOSE(fp);

   if (rename(filename, real_filename) == -1) {
      DRETURN(false);
   } else {
      strcpy(filename, real_filename);
   }

   DRETURN(true);

FPRINTF_ERROR:
FCLOSE_ERROR:
   DRETURN(false);
}

static bool read_manop(ocs::gdi::Target target) {
   lList **lpp;
   stringT filename;
   char str[256];
   FILE *fp;
   SGE_STRUCT_STAT st;
   int key = NoName;
   lDescr *descr = nullptr;

   DENTER(TOP_LAYER);

   switch (target) {
   case ocs::gdi::Target::UM_LIST:
      lpp = ocs::DataStore::get_master_list_rw(SGE_TYPE_MANAGER);
      strcpy(filename, MAN_FILE);
      key = UM_name;
      descr = UM_Type;
      break;

   case ocs::gdi::Target::UO_LIST:
      lpp = ocs::DataStore::get_master_list_rw(SGE_TYPE_OPERATOR);
      strcpy(filename, OP_FILE);
      key = UO_name;
      descr = UO_Type;
      break;

   default:
      DRETURN(false);
   }

   /* if no such file exists. ok return without error */
   if (SGE_STAT(filename, &st) && errno==ENOENT) {
      DRETURN(true);
   }

   fp = fopen(filename, "r");
   if (!fp) {
      ERROR(MSG_FILE_ERROROPENINGX_S, filename);
      DRETURN(false);
   }

   lFreeList(lpp);
   *lpp = lCreateList("man/op list", descr);

   while (fscanf(fp, "%[^\n]\n", str) == 1) {
      if (str[0] != COMMENT_CHAR) {
         lAddElemStr(lpp, key, str, descr);
      }
   }

   FCLOSE(fp);

   DRETURN(true);
FCLOSE_ERROR:
   ERROR(MSG_FILE_ERRORCLOSEINGX_S, filename);
   DRETURN(false);
}
