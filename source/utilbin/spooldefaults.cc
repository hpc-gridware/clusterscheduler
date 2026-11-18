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
 * @brief spooldefaults - write the initial cluster configuration into the spool
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "spool/sge_spooling.h"
#include "spool/loader/sge_spooling_loader.h"
#include "spool/flatfile/sge_spooling_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/sge_dirent.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "msg_utilbin.h"
#include "ocs_Bootstrap.h"

static int spool_object_list(const char *directory,
                             const spooling_field *fields, 
                             const spool_flatfile_instr *instr,
                             const lDescr *descr,
                             sge_object_type obj_type);

static void usage(const char *argv0) {
   fprintf(stderr, "%s\n %s command\n\n", MSG_UTILBIN_USAGE, argv0);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_COMMANDINTRO1);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_COMMANDINTRO2);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_TEST);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_ADMINHOSTS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_CALENDARS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_CKPTS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_COMPLEXES);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_CONFIGURATION);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_CQUEUES);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_EXECHOSTS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_LOCAL_CONF);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_MANAGERS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_OPERATORS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_PES);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_PROJECTS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_SHARETREE);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_SUBMITHOSTS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_USERS);
   fprintf(stderr, "%s\n", MSG_SPOOLDEFAULTS_USERSETS);
}

static int init_framework() {
   DENTER(TOP_LAYER);
   int ret = EXIT_FAILURE;

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   lListElem *spooling_context = nullptr;
   const char *spooling_method = ocs::Bootstrap::get_spooling_method();
   const char *spooling_lib = ocs::Bootstrap::get_spooling_lib();
   const char *spooling_params = ocs::Bootstrap::get_spooling_params();


   /* create spooling context */
   spooling_context = spool_create_dynamic_context(&answer_list, 
                                                   spooling_method,
                                                   spooling_lib, 
                                                   spooling_params);
   answer_list_output(&answer_list);
   if (spooling_context == nullptr) {
      CRITICAL(SFNMAX, MSG_SPOOLDEFAULTS_CANNOTCREATECONTEXT);
   } else {
      spool_set_default_context(spooling_context);

      /* initialize spooling context */
      if (!spool_startup_context(&answer_list, spooling_context, true)) {
         CRITICAL(SFNMAX, MSG_SPOOLDEFAULTS_CANNOTSTARTUPCONTEXT);
      } else {
         ret = EXIT_SUCCESS;
      }
      answer_list_output(&answer_list);
   }

   DRETURN(ret);
}

/**
 * @brief Spool the global configuration read from a file
 *
 * @param argc number of command line arguments
 * @param argv command line arguments; argv[2] is the file holding the global configuration
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
static int spool_configuration(int argc, char *argv[]) {
   DENTER(TOP_LAYER);

   int ret = EXIT_SUCCESS;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   spooling_field *fields = sge_build_CONF_field_list(true);

   // read with the CONF_Type descriptor, so the element is of that type; freed when leaving the function
   const auto conf = ocs::tom::ElementPtr<ocs::tom::CONF>::from_typed(
      spool_flatfile_read_object(&answer_list, CONF_Type, nullptr, fields, nullptr, false, &qconf_sfi,
                                 SP_FORM_ASCII, nullptr, argv[2]));
   sge_free(&fields);
   if (!conf) {
      ERROR(MSG_SPOOLDEFAULTS_CANTREADGLOBALCONF_S, argv[2]);
      ret = EXIT_FAILURE;
   } else {
      /* put config into a list - we can't spool free objects */
      ocs::tom::set<CONF_name>(conf, SGE_GLOBAL_NAME);
      if (!spool_write_object(&answer_list, spool_get_default_context(), *conf, SGE_GLOBAL_NAME,
                              SGE_TYPE_CONFIG)) {
         /* error output has been done in spooling function */
         ret = EXIT_FAILURE;
      }
      answer_list_output(&answer_list);
   }

   DRETURN(ret);
}

/**
 * @brief Spool a local configuration read from a file, unless one of that name is spooled already
 *
 * @param argc number of command line arguments
 * @param argv command line arguments; argv[2] is the file, argv[3] the name of the configuration
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
static int spool_local_conf(int argc, char *argv[]) {
   DENTER(TOP_LAYER);

   int ret = EXIT_SUCCESS;

   /* we get an additional argument: the config name */
   if (argc < 4) {
      usage(argv[0]);
      ret = EXIT_FAILURE;
   } else {
      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      spooling_field *fields = sge_build_CONF_field_list(true);

      // read with the CONF_Type descriptor, so the element is of that type; freed when leaving the block
      const auto conf = ocs::tom::ElementPtr<ocs::tom::CONF>::from_typed(
         spool_flatfile_read_object(&answer_list, CONF_Type, nullptr, fields, nullptr, false, &qconf_sfi,
                                    SP_FORM_ASCII, nullptr, argv[2]));
      sge_free(&fields);

      if (!conf) {
         ERROR(MSG_SPOOLDEFAULTS_CANTREADLOCALCONF_S, argv[2]);
         ret = EXIT_FAILURE;
      } else {
         /* check if a config is already there */
         // a configuration object (CONF_Type) as the spooling stores it; it used to be leaked
         const auto le = ocs::tom::ElementPtr<ocs::tom::CONF>::from_typed(
            spool_read_object(nullptr, spool_get_default_context(), SGE_TYPE_CONFIG, argv[3]));
         if (!le) {
            /* put config into a list - we can't spool free objects */
            ocs::tom::set<CONF_name>(conf, argv[3]);
            if (!spool_write_object(&answer_list, spool_get_default_context(), *conf, argv[3],
                                    SGE_TYPE_CONFIG)) {
               /* error output has been done in spooling function */
               ret = EXIT_FAILURE;
            } 
         } else {
            fprintf(stderr, "config already there!\n");
         }
         answer_list_output(&answer_list);
      }
      answer_list.reset();
   }

   DRETURN(ret);
}

static int spool_sharetree(int argc, char *argv[]) {
   const spooling_field *fields = sge_build_STN_field_list(true, false);
   int ret; 

   ret = spool_object_list(argv[2], fields, &qconf_sfi, STN_Type, SGE_TYPE_SHARETREE);
   sge_free(&fields);
   return ret;
}

static int spool_complexes(int argc, char *argv[]) {
   return spool_object_list(argv[2], CE_fields, &qconf_sfi, CE_Type, SGE_TYPE_CENTRY);
}


static int spool_calendars(int argc, char *argv[]) {
   return spool_object_list(argv[2], CAL_fields, &qconf_sfi, CAL_Type, SGE_TYPE_CALENDAR);
}

static int spool_ckpts(int argc, char *argv[]) {
   return spool_object_list(argv[2], CK_fields, &qconf_sfi, CK_Type, SGE_TYPE_CKPT);
}

static int spool_cqueues(int argc, char *argv[]) {
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   spool_read_list(&answer_list, spool_get_default_context(), 
                   ocs::DataStore::get_master_list_rw(SGE_TYPE_CENTRY), SGE_TYPE_CENTRY);
   spool_read_list(&answer_list, spool_get_default_context(), 
                   ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST), SGE_TYPE_EXECHOST);
   answer_list_output(&answer_list);

   return spool_object_list(argv[2], CQ_fields, &qconf_sfi, CQ_Type, SGE_TYPE_CQUEUE);
}

static int spool_exechosts(int argc, char *argv[]) {
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const spooling_field *fields = sge_build_EH_field_list(true, false, false, false);
   int ret; 

   spool_read_list(&answer_list, spool_get_default_context(), 
                   ocs::DataStore::get_master_list_rw(SGE_TYPE_CENTRY),
                   SGE_TYPE_CENTRY);
   answer_list_output(&answer_list);

   ret = spool_object_list(argv[2], fields, &qconf_sfi, EH_Type, SGE_TYPE_EXECHOST);
   sge_free(&fields);
   return ret;
}

static int spool_projects(int argc, char *argv[]) {
   const spooling_field *fields = sge_build_PR_field_list(true);
   int ret; 

   ret = spool_object_list(argv[2], fields, &qconf_sfi, PR_Type, SGE_TYPE_PROJECT);
   sge_free(&fields);
   return ret;
}


static int spool_users(int argc, char *argv[]) {
   const spooling_field *fields = sge_build_UU_field_list(true);
   int ret; 

   ret = spool_object_list(argv[2], fields, &qconf_sfi, UU_Type, SGE_TYPE_USER);   // not PROJECT (CS-3126)
   sge_free(&fields);
   return ret;
}

static int spool_pes(int argc, char *argv[]) {
   return spool_object_list(argv[2], PE_fields, &qconf_sfi, PE_Type, SGE_TYPE_PE);
}

static int spool_usersets(int argc, char *argv[]) {
   return spool_object_list(argv[2], US_fields, &qconf_sfi, US_Type, SGE_TYPE_USERSET);
}

/**
 * @brief Spool every object stored as a file in a directory
 *
 * Files whose name starts with a dot are skipped.
 *
 * @param directory the directory holding one file per object
 * @param fields the spooling fields of the object type
 * @param instr the flatfile format of the files
 * @param descr the descriptor of the object type, known only at run time here
 * @param obj_type the object type to spool the objects as
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
static int spool_object_list(const char *directory,
                             const spooling_field *fields,
                             const spool_flatfile_instr *instr,
                             const lDescr *descr,
                             const sge_object_type obj_type) {
   DENTER(TOP_LAYER);

   int ret = EXIT_SUCCESS;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   lListElem *ep;
   const char *name;
   dstring file = DSTRING_INIT;

   // the directory entries (ST_Type), freed when leaving the function
   const auto direntries = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(sge_get_dirents(directory));
   for (const auto direntry : *direntries) {
      name = ocs::tom::get<ST_name>(direntry);
      if (name[0] != '.') {
         sge_dstring_sprintf(&file, "%s/%s", directory, name);
         ep = spool_flatfile_read_object(&answer_list, descr, nullptr,
                                         fields, nullptr, true, instr,
                                         SP_FORM_ASCII, nullptr, sge_dstring_get_string(&file));
         
         if (ep != nullptr && !spool_write_object(&answer_list, spool_get_default_context(), ep,
                                 name, obj_type)) {
            /* error output has been done in spooling function */
            ret = EXIT_FAILURE;
            answer_list_output(&answer_list);
            break;
         }
         lFreeElem(&ep);
      }
   }
   sge_dstring_free(&file);

   answer_list.reset();

   DRETURN(ret);
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "spooldefaults");
   int ret = EXIT_SUCCESS;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup(SPOOLDEFAULTS, MAIN_THREAD, &answer_list, false) != ocs::gdi::ErrorValue::AE_OK) {
      show_answer(answer_list);
      answer_list.reset();
      sge_exit(EXIT_FAILURE);
   }

   /* parse commandline */
   if (argc < 2) {
      usage(argv[0]);
      ret = EXIT_FAILURE;
   } else {
      ret = init_framework();

      if (ret == EXIT_SUCCESS) {
         if (strcmp(argv[1], "test") == 0) {
            /* nothing to do - init_framework succeeded */
         } else {
            /* all other commands have at least one parameter */
            if (argc < 3) {
               usage(argv[0]);
               ret = EXIT_FAILURE;
            } else if (strcmp(argv[1], "calendars") == 0) {
               ret = spool_calendars(argc, argv);
            } else if (strcmp(argv[1], "ckpts") == 0) {
               ret = spool_ckpts(argc, argv);
            } else if (strcmp(argv[1], "complexes") == 0) {
               ret = spool_complexes(argc, argv);
            } else if (strcmp(argv[1], "configuration") == 0) {
               ret = spool_configuration(argc, argv);
            } else if (strcmp(argv[1], "cqueues") == 0) {
               ret = spool_cqueues(argc, argv);
            } else if (strcmp(argv[1], "exechosts") == 0) {
               ret = spool_exechosts(argc, argv);
            } else if (strcmp(argv[1], "local_conf") == 0) {
               ret = spool_local_conf(argc, argv);
            } else if (strcmp(argv[1], "pes") == 0) {
               ret = spool_pes(argc, argv);
            } else if (strcmp(argv[1], "projects") == 0) {
               ret = spool_projects(argc, argv);
            } else if (strcmp(argv[1], "sharetree") == 0) {
               ret = spool_sharetree(argc, argv);
            } else if (strcmp(argv[1], "users") == 0) {
               ret = spool_users(argc, argv);
            } else if (strcmp(argv[1], "usersets") == 0) {
               ret = spool_usersets(argc, argv);
            } else {
               usage(argv[0]);
               ret = EXIT_FAILURE;
            }
         }
      }
   }

   if ((ret != EXIT_FAILURE) && (spool_get_default_context() != nullptr)) {
      uint64_t next_trigger = 0;

      if (!spool_trigger_context(&answer_list, spool_get_default_context(), 
                                 0, &next_trigger)) {
         ret = EXIT_FAILURE;
      }
      if (!spool_shutdown_context(&answer_list, spool_get_default_context())) {
         ret = EXIT_FAILURE;
      }
   }

   answer_list_output(&answer_list);

   sge_exit(ret);

   DRETURN(ret);
}
