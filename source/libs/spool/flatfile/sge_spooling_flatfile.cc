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

/** @file
 * @brief The classic spooling backend: master lists as flat files
 */

/* system */
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>

#include "tom/ocs_Types.h"

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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_STU.h"

#include "uti/sge.h"

/* includes for old job spooling */
#include "spool/ocs_spool_duplicates.h"
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

/** @brief Report `"classic"` as this library's spooling method
 *
 * @return the string `"classic"`
 */
#ifdef SPOOLING_classic
const char *get_spooling_method()
#else
const char *get_classic_spooling_method()
#endif
{
   return spooling_method;
}

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
 * The return type is fixed by #spooling_create_context_func (the dynamic
 * loader), so the context is handed over raw.
 *
 * @param[out] answer_list  to return error messages
 * @param[in]  args         absolute spool directory path (the legacy
 *                          "<common_dir>;<spool_dir>" form is rejected)
 * @return on success the new spooling context (SPC), else nullptr
 */
lListElem *
spool_classic_create_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *args) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::SPC> context;

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

         /* create info which fields to spool once */
         field_info = (flatfile_info *)sge_malloc(sizeof(flatfile_info) * SGE_TYPE_ALL);
         for (i = (int)SGE_TYPE_FIRST; i < (int)SGE_TYPE_ALL; i++) {
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
                  /* without the host list: it lives in the queue host group
                   * @@<queue> and is spooled with that group (CS-2677) */
                  field_info[i].fields = sge_build_CQ_field_list(false);
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
               case SGE_TYPE_EXECHOST:
                  field_info[i].fields = sge_build_EH_field_list(true, false, false, false);
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
         context = ocs_spool_create_context(answer_list, "flatfile spooling");

         /* create rule and type for all objects spooled in the spool dir */
         const auto rule = spool_context_create_rule(answer_list, context.get(),
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
         ocs::tom::set<SPR_clientdata>(rule, field_info);
         const auto type = spool_context_create_type(answer_list, context.get(), SGE_TYPE_ALL);
         spool_type_add_rule(answer_list, type, rule, true);

         /* SGE_TYPE_CONFIG and SGE_TYPE_SCHEDD_CONF are deliberately not given
          * their own rule any more: with no type-specific rule they fall back to
          * the SGE_TYPE_ALL rule above (see the rule lookup in the read/write/
          * list/delete functions) and are therefore spooled into the spool
          * directory - the global and per-host configurations in the configs
          * subdirectory (keyed by name), the scheduler configuration as a single
          * file at the spool dir root. They no longer use the common
          * directory. */
      }
      sge_free_saved_vars(strtok_context);
   }

   DRETURN(context.release());
}

/**
 * @brief Remove the group and other permissions of an existing spool entry.
 *
 * A directory becomes 0700 and a file 0600. Nothing happens when the entry
 * does not exist, is a symbolic link or already is owner-only. A failure is
 * logged and otherwise ignored, it must not stop qmaster (CS-2384).
 *
 * @param[in] base directory the entry lives in
 * @param[in] name name of the directory or file below @p base
 */
static void
spool_classic_restrict_mode(const char *base, const char *name) {
   DENTER(TOP_LAYER);

   dstring path = DSTRING_INIT;
   sge_dstring_sprintf(&path, "%s/%s", base, name);

   struct stat st;
   if (lstat(sge_dstring_get_string(&path), &st) == 0 && (st.st_mode & (S_IRWXG | S_IRWXO)) != 0 &&
       (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode))) {
      const mode_t mode = S_ISDIR(st.st_mode) ? S_IRWXU : (S_IRUSR | S_IWUSR);
      if (chmod(sge_dstring_get_string(&path), mode) != 0) {
         WARNING(MSG_SPOOL_CANNOT_RESTRICT_MODE_SS, sge_dstring_get_string(&path), strerror(errno));
      }
   }

   sge_dstring_free(&path);
   DRETURN_VOID;
}

/**
 * @brief Set up the classic (flatfile) qmaster spool directory.
 *
 * Checks that the spool directory exists, changes into it, and creates the
 * per-object-type subdirectories if they do not yet exist. This includes the
 * configs subdirectory holding the global and per-host configurations; the
 * scheduler configuration is a single file at the spool directory root and
 * needs no subdirectory.
 *
 * The subdirectories are created owner-only (0700): they hold authoritative
 * job and configuration spool, and their entry names alone (job IDs, queue
 * instance and other object names) are information that must not be readable
 * by other local users. This complements CS-2352, which hardened the spool
 * *file* mode to 0600 but left the containing directories world-traversable.
 * Directories and root level spool files that an older version left with group
 * or other permissions are restricted to the owner as well (CS-2384).
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
bool spool_classic_default_startup_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                        const lListElem *rule, bool check) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *url;

   /* check spool directory */
   url = ocs::tom::get<SPR_url>(rule);
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
         /* Configuration objects (the global configuration and the per-host
          * local configurations) are spooled here, keyed by name ("global" and
          * the host name); the scheduler configuration remains a single file at
          * the spool dir root. Owner-only (0700): the entry names reveal which
          * hosts carry a local config (CS-2352). */
         sge_mkdir2(url, CONFIG_DIR, 0700, true);

         /* sge_mkdir2() keeps the mode of a directory that already exists. A
          * patch update only replaces the binaries and does not run
          * inst_sge -upd, so restrict what an older version left behind here
          * (CS-2384). An owner-only directory also protects the older files
          * inside it. The scheduler configuration and the share tree are plain
          * files at the spool directory root. */
         static const char *const spool_entries[] = {
            JOB_DIR, CQUEUE_DIR, QINSTANCES_DIR, EXECHOST_DIR, CENTRY_DIR, EXEC_DIR, PE_DIR, CKPTOBJ_DIR,
            USERSET_DIR, CAL_DIR, HGROUP_DIR, USER_DIR, PROJECT_DIR, RESOURCEQUOTAS_DIR, AR_DIR, ROLE_DIR,
            CONFIG_DIR, SCHED_CONF_FILE, SHARETREE_FILE
         };
         for (const char *name : spool_entries) {
            spool_classic_restrict_mode(url, name);
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Release what the classic backend allocated at startup
 *
 * Frees the field lists built by #spool_classic_create_context. Most object
 * types use one of the static arrays and must not be freed; only the ones
 * whose lists were built on the heap are released here.
 *
 * @param answer_list to return error messages
 * @param rule the spooling rule being shut down
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 */
bool spool_classic_default_shutdown_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                         const lListElem *rule) {
   DENTER(TOP_LAYER);

   bool ret = true;

   auto field_info = static_cast<flatfile_info *>(ocs::tom::get<SPR_clientdata>(rule));
   for (auto i = static_cast<int>(SGE_TYPE_FIRST); i < static_cast<int>(SGE_TYPE_ALL); i++) {
      // for most type we use static fields but for some we need to free the dynamic fields
      // @see spool_classic_create_context
      switch (i) {
         case SGE_TYPE_CQUEUE:
            field_info[i].fields = spool_free_spooling_fields(field_info[i].fields);
            break;
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
 * @brief Read one object from its file, validate it and append it to a list
 *
 * An object whose key in a unique attribute an object read before into the same list holds is not appended;
 * the filter records it with both files (N-H-18).
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to be used
 * @param key the file name, relative to the object type's directory
 * @param key_nm the attribute set from the file name, or NoName
 * @param object_type object type
 * @param master_list the list to append to
 * @param filter the filter of the list read
 * @param spool_key the file as the duplicate message names it, relative to the spool directory
 * @return true if the object was read and validated, else false; a filtered duplicate counts as read
 */
static bool read_validate_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 const lListElem *type, const lListElem *rule,
                                 const char *key, int key_nm,
                                 sge_object_type object_type, lList **master_list,
                                 ocs::spool::DuplicateFilter &filter, const char *spool_key) {
   DENTER(TOP_LAYER);

   bool ret = true;

   DPRINTF("reading " SFN " " SFQ "\n", object_type_get_name(object_type), key);

   ocs::tom::RawElementPtr ep(spool_classic_default_read_func(answer_list, type, rule, key, object_type));
   if (!ep) {
      ret = false;
   } else {
      spooling_validate_func validate_func;

      /* set key from filename, converted through the spooling record of the key field */
      if (key_nm != NoName) {
         if (const ocs_spool_field_ops *key_ops = ocs_spool_field_ops_of(key_nm); key_ops != nullptr) {
            ocs_spool_parse_value(*key_ops, ep.get(), nullptr, key);
         }
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      /* validate object */
      validate_func = (spooling_validate_func)ocs::tom::get<SPR_validate_func>(rule);
#pragma GCC diagnostic pop
      if (validate_func != nullptr) {
         if (!validate_func(answer_list, type, rule, ep.get(), object_type)) {
            ep.reset();
            ret = false;
         }
      }

      /* object read correctly and validate succeeded; a second object with a taken key is recorded and dropped */
      if (ep && filter.admit(ep.get(), spool_key)) {
         // the master list exists: the caller created it from the type's descriptor
         ocs::tom::append(*master_list, std::move(ep));
      }   // else ep frees the dropped duplicate
   }

   DRETURN(ret);
}

/**
 * @brief Read lists through flatfile spooling
 *
 * Depending on the object type given, calls the appropriate functions
 * reading the correspondent list of objects using the old spooling
 * functions.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to be used
 * @param list target list
 * @param object_type object type
 *
 * @return true, on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_read_list()`
 */
bool spool_classic_default_list_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                     const lListElem *type,
                                     const lListElem *rule,
                                     lList **list,
                                     const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   const lDescr *descr;

   const char *filename  = nullptr;
   const char *directory = nullptr;
   const char *url = nullptr;
   int key_nm = NoName;

   bool ret = true;

   if (!list) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                     ANSWER_QUALITY_WARNING,
                     "Cannot read in list because target list is missing\n");
      ret = false;
   } else {
      url = ocs::tom::get<SPR_url>(rule);
      descr = object_type_get_descr(object_type);

      if (*list == nullptr && descr != nullptr) {
         // handed out through the function table's raw out parameter
         *list = ocs::tom::create_raw_list(descr, ocs::tom::Hashing::On, "master list").release();
      }

      switch(object_type) {
         case SGE_TYPE_CALENDAR:
            directory = CAL_DIR;
            break;
         case SGE_TYPE_CKPT:
            directory = CKPTOBJ_DIR;
            break;
         case SGE_TYPE_CONFIG:
            /* The global ("global") and per-host configurations share one
             * directory, keyed by name; scanning it reads them all. */
            key_nm    = CONF_name;
            directory = CONFIG_DIR;
            break;
         case SGE_TYPE_EXECHOST:
            directory = EXECHOST_DIR;
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
         ocs::spool::DuplicateFilter filter(descr, object_type_get_name(object_type));

         /* single file to parse (SHARETREE, global config, schedd config */
         if (filename != nullptr) {
            ret = read_validate_object(answer_list, type, rule, filename, key_nm,
                                     object_type, list, filter, filename);
         }

         /* if we have a directory (= multiple files) to parse */
         if (ret && directory != nullptr) {
            char abs_dir_buf[SGE_PATH_MAX];
            dstring abs_dir_dstring;
            const char *abs_dir;

            sge_dstring_init(&abs_dir_dstring, abs_dir_buf, SGE_PATH_MAX);
            abs_dir = sge_dstring_sprintf(&abs_dir_dstring, "%s/%s", url,
                                          directory);

            const auto direntries = ocs_get_dirents(abs_dir);

            dstring spool_key = DSTRING_INIT;
            for (const ocs::tom::ConstElement<ocs::tom::ST> direntry : *direntries) {
               const char *key = direntry.get<ST_name>();

               if (key[0] != '.') {
                  sge_dstring_sprintf(&spool_key, "%s/%s", directory, key);
                  ret &= read_validate_object(answer_list, type, rule, key, key_nm,
                                           object_type, list, filter, sge_dstring_get_string(&spool_key));
               }
            }
            sge_dstring_free(&spool_key);
         }
      }

      switch(object_type) {
         case SGE_TYPE_CQUEUE:
            {
               const auto context = spool_get_default_context();
               const auto type = spool_context_search_type(context, SGE_TYPE_QINSTANCE);
               const auto rule = spool_type_search_default_rule(type);
               const char *url = ocs::tom::get<SPR_url>(rule);

               dstring key = DSTRING_INIT;
               dstring dir = DSTRING_INIT;

               dstring spool_key = DSTRING_INIT;
               // the list just read holds cluster queues (CQ), so it is taken without a check
               for (const ocs::tom::Element<ocs::tom::CQ> queue : ocs::tom::List<ocs::tom::CQ>::from_typed(*list)) {
                  // read_validate_object() appends through a raw list pointer; the list goes to the queue below
                  lList *qinstance_list = ocs::tom::ListPtr<ocs::tom::QU>::create("").release();
                  ocs::spool::DuplicateFilter qinstance_filter(ocs::tom::QU::descriptor.e,
                                                               object_type_get_name(SGE_TYPE_QINSTANCE));

                  sge_dstring_sprintf(&dir, "%s/%s/%s", url, QINSTANCES_DIR, queue.get<CQ_name>());
                  const auto direntries = ocs_get_dirents(sge_dstring_get_string(&dir));
                  for (const ocs::tom::ConstElement<ocs::tom::ST> direntry : *direntries) {
                     const char *directory = direntry.get<ST_name>();
                     if (directory[0] != '.') {
                        sge_dstring_sprintf(&key, "%s/%s", queue.get<CQ_name>(), directory);
                        sge_dstring_sprintf(&spool_key, "%s/%s", QINSTANCES_DIR, sge_dstring_get_string(&key));
                        read_validate_object(answer_list, type, rule, sge_dstring_get_string(&key), NoName,
                                           SGE_TYPE_QINSTANCE, &qinstance_list, qinstance_filter,
                                           sge_dstring_get_string(&spool_key));
                     }
                  }

                  queue.set<CQ_qinstances>(ocs::tom::ListPtr<ocs::tom::QU>::from_typed(qinstance_list));
               }
               sge_dstring_free(&spool_key);
               sge_dstring_free(&dir);
               sge_dstring_free(&key);
            }
            break;
         default:
            break;
      }


#ifdef DEBUG_FLATFILE
      if (list != nullptr && *list != nullptr) {
         ocs::tom::write_to(stderr, ocs::tom::ConstRawList(*list));
      }
#endif

      /* validate the complete list */
      if (ret) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
         spooling_validate_list_func validate_list =
            (spooling_validate_list_func)ocs::tom::get<SPR_validate_list_func>(rule);
#pragma GCC diagnostic pop

         ret = validate_list(answer_list, type, rule, object_type);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Read objects using flatfile spooling
 *
 * Reads an individual object by calling the appropriate flatfile spooling
 * function.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param key unique key specifying the object
 * @param object_type object type
 *
 * @return the object, if it could be read, else nullptr
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_read_object()`
 */
lListElem *
spool_classic_default_read_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const lListElem *type,
                                const lListElem *rule,
                                const char *key,
                                const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   const char *url = nullptr;
   const char *directory = nullptr;
   const char *filename = nullptr;
   const lDescr *descr = nullptr;
   flatfile_info *rule_clientdata;
   flatfile_info *field_info;
   lListElem *ep = nullptr;
   bool parse_values = true;

   rule_clientdata = (flatfile_info *)ocs::tom::get<SPR_clientdata>(rule);
   field_info = &(rule_clientdata[object_type]);
   url = ocs::tom::get<SPR_url>(rule);
   descr = object_type_get_descr(object_type);

   /* prepare filenames */
   switch(object_type) {
      case SGE_TYPE_CALENDAR:
         directory = CAL_DIR;
         filename = key;
         break;
      case SGE_TYPE_CKPT:
         directory = CKPTOBJ_DIR;
         filename = key;
         break;
      case SGE_TYPE_CONFIG:
         /* Global ("global") and per-host configurations are spooled in one
          * directory, keyed by name. */
         parse_values = false;
         directory = CONFIG_DIR;
         filename  = key;
         break;
      case SGE_TYPE_EXECHOST:
         directory = EXECHOST_DIR;
         filename  = key;
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
         ep = ocs::spool::classic::ar_read_spool_file(SGE_STRTOU_LONG32(key)).release();
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
                  auto script = ocs::tom::ElementPtr<ocs::tom::STU>::create();
                  ocs::tom::exchange<STU_name>(script.get(), &str);
                  ep = script.release();
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

/**
 * @brief Check that a spool object key is a safe relative spool path.
 *
 * A key is used verbatim to build a spool-file path (`EXECHOST_DIR/<key>` etc.).
 * Most keys are validated upstream by verify_str_key(), but host names are not,
 * so this is a defence-in-depth guard at the spool sink (CS-2364, CWE-22).
 *
 * A key may be a multi-component relative path: a queue instance is spooled
 * under `"<cqueue>/<host>"`, so '/' is a legitimate separator and must not be
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

/**
 * @brief Write an object with flatfile spooling
 *
 * Writes an object through the appropriate flatfile spooling functions.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param object object to spool
 * @param key unique key, also the relative path the object is written to
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see #spool_write_object
 */
bool spool_classic_default_write_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                      const lListElem *type,
                                      const lListElem *rule,
                                      const lListElem *object,
                                      const char *key,
                                      const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   const char *url = nullptr;
   const char *directory = nullptr;
   const char *filename = nullptr;
   const char *filepath = nullptr;
   flatfile_info *rule_clientdata;
   flatfile_info *field_info;
   dstring tmp = DSTRING_INIT;
   bool ret = true;

   rule_clientdata = (flatfile_info *)ocs::tom::get<SPR_clientdata>(rule);
   field_info = &(rule_clientdata[object_type]);
   url = ocs::tom::get<SPR_url>(rule);

   /* prepare filenames */
   switch(object_type) {
      case SGE_TYPE_CALENDAR:
         directory = CAL_DIR;
         filename = key;
         break;
      case SGE_TYPE_CKPT:
         directory = CKPTOBJ_DIR;
         filename = key;
         break;
      case SGE_TYPE_CONFIG:
         /* Global ("global") and per-host configurations are spooled in one
          * directory, keyed by name. */
         directory = CONFIG_DIR;
         filename  = key;
         break;
      case SGE_TYPE_EXECHOST:
         directory = EXECHOST_DIR;
         filename  = key;
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
         directory = sge_dstring_sprintf(&tmp, "%s/%s", QINSTANCES_DIR, ocs::tom::get<QU_qname>(object));
         filename = ocs::tom::get<QU_qhostname>(object);
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
            // the job writer takes the task list out of the job while it writes, so it needs a mutable job
            ocs::tom::Element<ocs::tom::JB> job;

            job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

            DPRINTF("spooling job %d.%d %s\n", job_id, ja_task_id,
                    pe_task_id != nullptr ? pe_task_id : "<null>");

            if (object_type == SGE_TYPE_JOB) {
               job = ocs::tom::Element<ocs::tom::JB>(const_cast<lListElem *>(object));

               /* we only want to spool the job object */
               if (only_job) {
                  flags |= SPOOL_IGNORE_TASK_INSTANCES;
               }
            } else {
               /* job_write_spool_file takes a job, even if we only want
                * to spool a ja_task or pe_task
                */
               job = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true)
                        .find_first<JB_job_number>(job_id);

               /* additional flags for job_write_spool_file
                * to avoid spooling too many files
                */
               if (object_type == SGE_TYPE_PETASK) {
                  flags |= SPOOL_ONLY_PETASK;
               } else if (object_type == SGE_TYPE_JATASK) {
                  flags |= SPOOL_ONLY_JATASK;
               }
            }

            if (job_write_spool_file(job, ja_task_id, pe_task_id, (sge_spool_flags_t)flags) != 0) {
               ret = false;
            }

            sge_free(&dup);
         }
         break;
      case SGE_TYPE_JOBSCRIPT:
         ret = sge_string2file(ocs::tom::get<JB_script_ptr>(object),
                               ocs::tom::get<JB_script_size>(object),
                               ocs::tom::get<JB_exec_file>(object)) ? false : true;
         break;
      case SGE_TYPE_RQS:
         directory = RESOURCEQUOTAS_DIR;
         filename  = key;
         break;
      case SGE_TYPE_AR:
         ocs::spool::classic::ar_write_spool_file(ocs::tom::ConstElement<ocs::tom::AR>(object));
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

   // CS-1597: for exec hosts only static load values shall be spooled: a copy without the dynamic ones is written
   ocs::tom::ElementPtr<ocs::tom::EH> exechost_copy;
   if (object_type == SGE_TYPE_EXECHOST) {
      exechost_copy = ocs_spool_exechost_static_copy(ocs::tom::ConstElement<ocs::tom::EH>(object));
      object = exechost_copy.get();
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

   sge_dstring_free(&tmp);

   DRETURN(ret);
}

/**
 * @brief Delete object in flatfile spooling
 *
 * Deletes an object in the flatfile spooling.
 * In most cases, the correspondent spool file is deleted, in some cases
 * (e.g. jobs), a special remove function is called. An object key that is not
 * a safe relative spool path is refused (CS-2364).
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param key unique key
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_delete_object()`
 */
bool spool_classic_default_delete_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                       const lListElem *type,
                                       const lListElem *rule,
                                       const char *key,
                                       const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /* Defence-in-depth: key is used to build the unlink path; reject an unsafe
    * key (e.g. an unvalidated host name like "../../tmp/x") so a delete cannot
    * escape the spool directory (CS-2364, CWE-22). Keys may contain '/' as a
    * separator ("<cqueue>/<host>"), but no component may be empty or start
    * with '.'. */
   if (!spool_flatfile_key_is_safe(key)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_SPOOL_UNSAFE_KEY_S, key != nullptr ? key : "");
      DRETURN(false);
   }

   switch(object_type) {
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
                                           ocs::tom::get<SPR_url>(rule),
                                           CONFIG_DIR);
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
