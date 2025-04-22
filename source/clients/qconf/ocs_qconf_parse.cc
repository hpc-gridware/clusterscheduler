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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <fnmatch.h>

#include "uti/sge_dstring.h"
#include "uti/sge_edit.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/config.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_utility.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "gdi/ocs_gdi_Client.h"

#include "sge.h"
#include "sge_options.h"
#include "usage.h"
#include "ocs_qconf_Category.h"
#include "ocs_qconf_acl.h"
#include "ocs_qconf_parse.h"
#include "sge_qconf_hgroup.h"
#include "ocs_qconf_centry.h"
#include "ocs_qconf_cqueue.h"
#include "ocs_qconf_rqs.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qconf.h"

static int sge_next_is_an_opt(char **ptr);
static int sge_error_and_exit(const char *ptr);

/* ------------------------------------------------------------- */
static bool show_object_list(ocs::gdi::Target::TargetValue, lDescr *, int, const char *);
static int show_thread_list();
static int show_processors(bool has_binding_param);
static int show_eventclients();

/* ------------------------------------------------------------- */
static void parse_name_list_to_cull(const char *name, lList **lpp, lDescr *dp, int nm, char *s);
static bool add_host_of_type(lList *arglp, ocs::gdi::Target::TargetValue target);
static bool del_host_of_type(lList *arglp, ocs::gdi::Target::TargetValue target);
static int print_acl(lList *arglp);
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp, lListElem **epp, ocs::gdi::SubCommand::SubCmd sub_command, struct object_info_entry *info_entry);
static lListElem *edit_exechost(lListElem *ep, uid_t uid, gid_t gid);
static int edit_usersets(lList *arglp);

/************************************************************************/
static int print_config(const char *config_name);
static int delete_config(const char *config_name);
static int add_modify_config(const char *cfn, const char *filename, u_long32 flags);
static lList* edit_sched_conf(lList *confl, uid_t uid, gid_t gid);
static lListElem* edit_project(lListElem *ep, uid_t uid, gid_t gid);
static lListElem* edit_user(lListElem *ep, uid_t uid, gid_t gid);
static lListElem *edit_sharetree(lListElem *ep, uid_t uid, gid_t gid);

/************************************************************************/
static int qconf_is_manager(const char *user);
static int qconf_is_adminhost(const char *host);
static int qconf_is_manager_on_admin_host(const char *user, const char *host);
/************************************************************************/

static const char *write_attr_tmp_file(const char *name, const char *value, 
                                       const char *delimiter, dstring *error_message);

/***************************************************************************/
static char **sge_parser_get_next(char **arg)
{
   DENTER(TOP_LAYER);
   if (!*(arg+1)) {
      ERROR(MSG_QCONF_NOOPTIONARGPROVIDEDTOX_S , *arg);
      sge_usage(QCONF, stderr);
      sge_exit(1);
   }

   DRETURN(++arg);
}

/*------------------------------------------------------------*/
int sge_parse_qconf(char *argv[])
{
   int status;
   char *cp = nullptr;
   char **spp = nullptr;
   int opt;
   int sge_parse_return = 0;
   lEnumeration *what = nullptr;
   lCondition *where = nullptr;
   lList *lp=nullptr, *arglp=nullptr, *alp=nullptr, *newlp=nullptr;
   lListElem *hep=nullptr, *ep=nullptr;
   lListElem *argep=nullptr;
   const lListElem *aep=nullptr;
   lListElem *newep = nullptr;
   const char *host = nullptr;
   char *filename;
   const char *filename_stdout;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   const char* qualified_hostname = component_get_qualified_hostname();
   const char* username = component_get_username();
   u_long32 prog_number = component_get_component_id();
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   bool has_binding_param = false;

   DENTER(TOP_LAYER);

   /* If no arguments were given, output the help message on stderr. */
   if (*argv == nullptr) {
      sge_usage(QCONF, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      DRETURN(1);
   }
   
   /* 
    * is there a -cb switch. we have to find that switch now because
    * the loop handles all switches in specified sequence and
    * -sep -cb would not be handled correctly.
    */
   spp = argv;
   while (*spp) {
      if (strcmp("-cb", *spp) == 0) {
         has_binding_param = true;
      }
      spp++;
   }

   /* now start from beginning */
   spp = argv;
   while (*spp) {

/*----------------------------------------------------------------------------*/
      /* "-cb" */

      if (strcmp("-cb", *spp) == 0) {
         /* just skip it we have parsed it above */
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-acal cal-name" */

      if ((strcmp("-acal", *spp) == 0) ||
          (strcmp("-Acal", *spp) == 0)) {
         if (!strcmp("-acal", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);
           
            /* get a generic calendar */
            ep = sge_generic_cal(*spp); 
            filename = (char *)spool_flatfile_write_object(&alp, ep, false,
                                                 CAL_fields, &qconf_sfi,
                                                 SP_DEST_TMP, SP_FORM_ASCII,
                                                 nullptr, false);
            lFreeElem(&ep);
            if (answer_list_output(&alp)) {
               if (filename != nullptr) {
                  unlink(filename);
                  sge_free(&filename);
               }
               sge_error_and_exit(nullptr);
            }

            /* edit this file */
            status = sge_edit(filename, uid, gid);
            if (status < 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
                  continue;
               }
            }

            if (status > 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
                  continue;
               }
            }
         
            /* read it in again */
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CAL_Type, nullptr,
                                            CAL_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CAL_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }      
            
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         } else { /* -Acal */
            spp = sge_parser_get_next(spp);
           
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CAL_Type, nullptr,
                                            CAL_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CAL_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
            
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         }

         /* send it to qmaster */
         lp = lCreateList("cal to add", CAL_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_CAL_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-ackpt ckpt_name" or "-Ackpt fname" */

      if ((strcmp("-ackpt", *spp) == 0) ||
          (strcmp("-Ackpt", *spp) == 0)) {

         if (!strcmp("-ackpt", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            /* get a generic ckpt configuration */
            ep = sge_generic_ckpt(*spp); 
            filename = (char *)spool_flatfile_write_object(&alp, ep, false,
                                                 CK_fields, &qconf_sfi,
                                                 SP_DEST_TMP, SP_FORM_ASCII,
                                                 nullptr, false);
            if (answer_list_output(&alp)) {
               if (filename != nullptr) {
                  unlink(filename);
                  sge_free(&filename);
               }
               sge_error_and_exit(nullptr);
            }

            lFreeElem(&ep);
            
            /* edit this file */
            status = sge_edit(filename, uid, gid);
            if (status < 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_PARSE_EDITFAILED))
                  continue;
            }

            if (status > 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED))
                  continue;
            }
         
            /* read it in again */
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CK_Type, nullptr,
                                            CK_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CK_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
            
            if ((ep != nullptr) && (ckpt_validate(ep, &alp) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
            
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         } else { /* -Ackpt */
            spp = sge_parser_get_next(spp);

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CK_Type, nullptr,
                                            CK_fields, fields_out,  true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CK_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (ckpt_validate(ep, &alp) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         }

         /* send it to qmaster */
         lp = lCreateList("CKPT list to add", CK_Type); 
         lAppendElem(lp, ep);

         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_CK_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ae [server_name]" */
      if (strcmp("-ae", *spp) == 0) {
         char *hostname = nullptr;

         cp = nullptr;
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            lListElem *host_ep = nullptr;

            spp = sge_parser_get_next(spp);

            /* try to resolve hostname */
            host_ep = lCreateElem(EH_Type);
            lSetHost(host_ep, EH_name, *spp);

            switch (sge_resolve_host(host_ep, EH_name)) {
               case CL_RETVAL_OK:
                  break;
               default:
                  fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(host_ep, EH_name));
                  fprintf(stderr, "\n");
                  lFreeElem(&host_ep);
                  DRETURN(1);
            }

            hostname = sge_strdup(hostname, lGetHost(host_ep, EH_name));
            lFreeElem(&host_ep);
         } else {
            /* no template name given - then use "template" as name */
            hostname = sge_strdup(hostname, SGE_TEMPLATE_NAME);
         }
         /* get a template host entry .. */
         where = lWhere("%T( %Ih=%s )", EH_Type, EH_name, hostname);
         what = lWhat("%T(ALL)", EH_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &arglp, where, what);
         lFreeWhat(&what);
         lFreeWhere(&where);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_free(&hostname);
            lFreeList(&alp);
            spp++;
            sge_parse_return = 1;
            continue;
         }
         
         if (arglp == nullptr || lGetNumberOfElem(arglp) == 0) {
            fprintf(stderr, MSG_EXEC_XISNOEXECHOST_S, hostname);
            fprintf(stderr, "\n");
            sge_free(&hostname);
            lFreeList(&alp);
            lFreeList(&arglp);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         sge_free(&hostname);
         lFreeList(&alp);
         
         /* edit the template */
         argep = lFirstRW(arglp);
         ep = edit_exechost(argep, uid, gid);
         if (ep == nullptr) {
            lFreeList(&arglp);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         switch (sge_resolve_host(ep, EH_name)) {
         case CL_RETVAL_OK:
            break;
         default:
            fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(ep, EH_name));
            fprintf(stderr, "\n");
            lFreeList(&arglp);
            DRETURN(1);
         }

         hostname = sge_strdup(hostname, lGetHost(ep, EH_name));
         lFreeList(&arglp);

         lp = lCreateList("hosts to add", EH_Type);
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         lFreeList(&lp);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_free(&hostname);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK) {
            fprintf(stderr, MSG_EXEC_ADDEDHOSTXTOEXECHOSTLIST_S, hostname);
         } else {
            fprintf(stderr, "%s", lGetString(ep, AN_text));
         }
         fprintf(stderr, "\n");
      
         sge_free(&hostname);
         lFreeList(&alp);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-Ae fname" */
      if (strcmp("-Ae", *spp) == 0) {
         spooling_field *fields = sge_build_EH_field_list(false, false, false);
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         /* read file */
         lp = lCreateList("exechosts to add", EH_Type); 
         fields_out[0] = NoName;
         ep = spool_flatfile_read_object(&alp, EH_Type, nullptr,
                                         fields, fields_out, true, &qconf_sfi,
                                         SP_FORM_ASCII, nullptr, *spp);
         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }

         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
         }

         sge_free(&fields);
            
         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if (!ep) {
            fprintf(stderr, "%s\n", MSG_ANSWER_INVALIDFORMAT); 
            DRETURN(1);
         }
         lAppendElem(lp, ep);

         /* test host name */
         switch (sge_resolve_host(ep, EH_name)) {
         case CL_RETVAL_OK:
            break;
         default:
            fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(ep, EH_name));
            fprintf(stderr, "\n");
            lFreeElem(&ep);
            DRETURN(1);
         }

         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-ah server_name[,server_name,...]" */
      if (strcmp("-ah", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to add", &lp, AH_Type, AH_name, *spp);
         if (!add_host_of_type(lp, ocs::gdi::Target::SGE_AH_LIST)) {
            sge_parse_return |= 1;
         }
         
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-arqs rqs_name" */
      if (strcmp("-arqs", *spp) == 0) {
         const char *name = "template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         rqs_add(&alp, name);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));          
            lFreeList(&alp);
            lFreeList(&lp);
            DRETURN(1);
         } else {
            fprintf(stdout, "%s\n", lGetString(aep, AN_text));
         }
         lFreeList(&alp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-Arqs fname" */
      if (strcmp("-Arqs", *spp) == 0) {
         const char *file = nullptr;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         rqs_add_from_file(&alp, file);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));          
            lFreeList(&alp);
            lFreeList(&lp);
            DRETURN(1);
         } else {
            fprintf(stdout, "%s\n", lGetString(aep, AN_text));
         }
         lFreeList(&alp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-am user_list" */

      if (strcmp("-am", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, UM_Type, UM_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UM_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-ao user_list" */

      if (strcmp("-ao", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, UO_Type, UO_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UO_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-ap pe_name" */

      if ((strcmp("-ap", *spp) == 0) ||
          (strcmp("-Ap", *spp) == 0)) {

         if (!strcmp("-ap", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
           
            spp = sge_parser_get_next(spp);

            /* get a generic parallel environment */
            ep = pe_create_template(*spp); 
            filename = (char *)spool_flatfile_write_object(&alp, ep, false,
                                                 PE_fields, &qconf_sfi,
                                                 SP_DEST_TMP, SP_FORM_ASCII,
                                                 nullptr, false);
            lFreeElem(&ep);
            
            if (answer_list_output(&alp)) {
               if (filename != nullptr) {
                  unlink(filename);
                  sge_free(&filename);
               }
               sge_error_and_exit(nullptr);
            }

            /* edit this file */
            status = sge_edit(filename, uid, gid);
            if (status < 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
                  continue;
               }
            }

            if (status > 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
                  continue;
               }
            }
         
            /* read it in again */
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                            PE_fields, fields_out,  true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(PE_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (pe_validate(ep, &alp, 0, nullptr) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
            
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         } else { /* -Ap */
            spp = sge_parser_get_next(spp);

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                            PE_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(PE_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (pe_validate(ep, &alp, 0, nullptr) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         }

         /* send it to qmaster */
         lp = lCreateList("PE list to add", PE_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PE_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-auser" */

      if (strcmp("-auser", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* get a template for editing */
         ep = getUserTemplate(); 

         newep = edit_user(ep, uid, gid);
         lFreeElem(&ep);

         /* send it to qmaster */
         lp = lCreateList("User list to add", UU_Type); 
         lAppendElem(lp, newep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));          
            lFreeList(&alp);
            lFreeList(&lp);
            DRETURN(1);
         } else {
            fprintf(stdout, "%s\n", lGetString(aep, AN_text));
         }
                  
         spp++;
         lFreeList(&alp);
         lFreeList(&lp);

         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-aprj" */

      if (strcmp("-aprj", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* get a template for editing */
         ep = getPrjTemplate(); 
         
         newep = edit_project(ep, uid, gid);
         lFreeElem(&ep);

         /* send it to qmaster */
         lp = lCreateList("Project list to add", PR_Type); 
         lAppendElem(lp, newep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
                  
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Auser" */

      if (strcmp("-Auser", *spp) == 0) {
         char* file = nullptr;
         spooling_field *fields = nullptr;
         
         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }        


         /* get project  */
         ep = nullptr;
         fields_out[0] = NoName;
         fields = sge_build_UU_field_list(false);
         ep = spool_flatfile_read_object(&alp, UU_Type, nullptr, fields, fields_out,
                                          true, &qconf_sfi, SP_FORM_ASCII, nullptr,
                                          file);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }

         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
         }

         sge_free(&fields);
         
         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if (ep == nullptr) {
            sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE); 
         }
         
         /* send it to qmaster */
         lp = lCreateList("User to add", UU_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));          
            lFreeList(&alp);
            lFreeList(&lp);
            DRETURN(1);
         } else {
            fprintf(stdout, "%s\n", lGetString(aep, AN_text));
         }         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }


/*----------------------------------------------------------------------------*/

      /* "-Aprj" */

      if (strcmp("-Aprj", *spp) == 0) {
         char* file = nullptr;
         spooling_field *fields = nullptr;
         
         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }        


         /* get project  */
         ep = nullptr;
         fields_out[0] = NoName;
         fields = sge_build_PR_field_list(false);
         ep = spool_flatfile_read_object(&alp, PR_Type, nullptr, fields, fields_out,
                                          true, &qconf_sfi, SP_FORM_ASCII, nullptr,
                                          file);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }

         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
         }

         sge_free(&fields);
         
         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
         }
            
         if (ep == nullptr) {
            sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE); 
         }
        
         /* send it to qmaster */
         lp = lCreateList("Project list to add", PR_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         fprintf(stderr, "%s\n", lGetString(aep, AN_text)); 
         if (answer_get_status(aep) != STATUS_OK) {         
            lFreeList(&alp);
            lFreeList(&lp);
            DRETURN(1);
         }         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      /* "-as server_name[,server_name,...]" */
      if (strcmp("-as", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to add", &lp, SH_Type, SH_name, *spp);
         if (!add_host_of_type(lp, ocs::gdi::Target::SGE_SH_LIST)) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-astree", "-Astree file":  add sharetree */

      if ((strcmp("-astree", *spp) == 0) || (strcmp("-Astree", *spp) == 0)) {
         lListElem *unspecified = nullptr;
         
         if (strcmp("-astree", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            /* get the sharetree .. */
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
            lFreeWhat(&what);

            sge_parse_return |= show_answer_list(alp);
            if (sge_parse_return) {
               lFreeList(&alp);
               spp++;
               continue;
            }
            lFreeList(&alp);
    
            ep = lFirstRW(lp);
            if (!(ep = edit_sharetree(ep, uid, gid)))
               continue;

            lFreeList(&lp);
         } else { /* -Astree */
            spooling_field *fields = sge_build_STN_field_list(false, true);
            
            spp = sge_parser_get_next(spp);
           
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                            fields, fields_out, true,
                                            &qconf_name_value_list_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
            }

            sge_free(&fields);
            
            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }      
         }
         
         /* Make sure that no nodes are left unspecified.  An unspecified node
          * happens when a node appears in another node's child list, but does
          * not appear itself. */
         unspecified = sge_search_unspecified_node(ep);
         
         if (unspecified != nullptr) {
            fprintf(stderr, MSG_STREE_NOVALIDNODEREF_U, lGetUlong(unspecified, STN_id));
            fprintf(stderr, "\n");

            lFreeElem(&ep);
            spp++;
            continue;
         }
         
         newlp = lCreateList("sharetree add", STN_Type);
         lAppendElem(newlp, ep);

         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &newlp, nullptr, what);
         lFreeWhat(&what);

         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_TREE_CHANGEDSHARETREE);
         else
            fprintf(stderr, "%s\n", lGetString(ep, AN_text));
         lFreeList(&newlp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-astnode node_path=shares[,...]"  create/modify sharetree node */
      /* "-mstnode node_path=shares[,...]"  modify sharetree node */

      if (((strcmp("-astnode", *spp) == 0) && ((opt=astnode_OPT) != 0)) ||
          ((strcmp("-mstnode", *spp) == 0) && ((opt=mstnode_OPT) != 0))) {
         int modified = 0;
         int print_usage = 0;

         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         ep = lFirstRW(lp);
         if (!ep && opt == astnode_OPT) {
            ep = lAddElemStr(&lp, STN_name, "Root", STN_Type);
            if (ep) lSetUlong(ep, STN_shares, 1);
         }
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw (argep, arglp) {
            lListElem *node = nullptr;
            char *buf=nullptr, *nodepath=nullptr, *sharestr=nullptr;
            int shares;
            ancestors_t ancestors;

            buf = sge_strdup(buf, lGetString(argep, STN_name));
            nodepath = sge_strtok(buf, "=");
            sharestr = sge_strtok(nullptr, "");
            if (nodepath && sharestr && sscanf(sharestr, "%d", &shares) == 1) {
               if (shares < 0) {
                  fprintf(stderr, "%s\n", MSG_QCONF_POSITIVE_SHARE_VALUE);                  
                  DRETURN(1);
               }
               memset(&ancestors, 0, sizeof(ancestors));
               node = search_named_node_path(ep, nodepath, &ancestors);
               if (!node && opt==astnode_OPT) {
                  char *c, *lc = nullptr;
                  lListElem *pnode, *nnode;

                  /* scan for basename of nodepath */
                  for (c=nodepath; *c; c++)
                     if (*c == '/' || *c == '.')
                        lc = c;

                  /* search for parent node of new node */
                  if (lc && *nodepath && *(lc+1)) {
                     char savelc = *lc;
                     *lc = '\000';
                     if (lc == nodepath && savelc == '/') /* root? */
                        pnode = ep;
                     else
                        pnode = search_named_node_path(ep, nodepath,
                                                       &ancestors);
                     if (pnode) {
                        lList *children = lGetListRW(pnode, STN_children);

                        nnode = lAddElemStr(&children, STN_name, lc+1, STN_Type);
                        if (nnode && !lGetList(pnode, STN_children))
                           lSetList(pnode, STN_children, children);
                        free_ancestors(&ancestors);
                        memset(&ancestors, 0, sizeof(ancestors));
                        *lc = savelc;
                        node = search_named_node_path(ep, nodepath, &ancestors);
                        if (node != nnode) {
                           fprintf(stderr, MSG_TREE_CANTADDNODEXISNONUNIQUE_S, nodepath);
                           fprintf(stderr, "\n");
                           DRETURN(1);
                        }
                     }
                  }
               }
               if (node) {
                  int i;
                  modified++;
                  lSetUlong(node, STN_shares, shares);
                  fprintf(stderr, "%s\n", MSG_TREE_SETTING);
                  for (i=0; i<ancestors.depth; i++)
                     fprintf(stderr, "/%s",
                             lGetString(ancestors.nodes[i], STN_name));
                  fprintf(stderr, "=%d\n", shares);
               } else {
                  fprintf(stderr, MSG_TREE_UNABLETOLACATEXINSHARETREE_S,
                          nodepath);
                  fprintf(stderr, "\n");
               }
               free_ancestors(&ancestors);

            } else {
               fprintf(stderr, MSG_ANSWER_XISNOTVALIDSEENODESHARESLIST_S, *spp);
               fprintf(stderr, "\n");
               print_usage = 1;
            }

            sge_free(&buf);
         }
         lFreeList(&arglp);

         if (modified) {
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
            lFreeWhat(&what);
            ep = lFirstRW(alp);
            answer_exit_if_not_recoverable(ep);
            if (answer_get_status(ep) == STATUS_OK)
               fprintf(stderr, "%s\n", MSG_TREE_MODIFIEDSHARETREE);
            else
               fprintf(stderr, "%s\n", lGetString(ep, AN_text));
            lFreeList(&alp);
         } else {
            fprintf(stderr, "%s\n", MSG_TREE_NOMIDIFIEDSHARETREE);
            if (print_usage)
               sge_usage(QCONF, stderr);
            DRETURN(1);
         }

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-au user_list list_name[,list_name,...]" */

      if (strcmp("-au", *spp) == 0) {

         /* no adminhost/manager check needed here */

         /* get user list */
         spp = sge_parser_get_next(spp);
         if (!*(spp+1)) {
            ERROR(MSG_ANSWER_NOLISTNAMEPROVIDEDTOAUX_S, *spp);
            sge_usage(QCONF, stderr);
            DRETURN(1);
         }

         lString2List(*spp, &lp, UE_Type, UE_name, ",  ");
         
         /* get list_name list */
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &arglp, US_Type, US_name, ", ");

         /* add all users/groups from lp to the acls in alp */
         sge_client_add_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&arglp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-clearusage"  clear sharetree usage */

      if (strcmp("-clearusage", *spp) == 0) {
         lList *lp2=nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* get user list */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         /* get project list */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp2, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         /* clear user usage */
         for_each_rw (ep, lp) {
            lSetList(ep, UU_usage, nullptr);
            lSetList(ep, UU_project, nullptr);
         }

         /* clear project usage */
         for_each_rw (ep, lp2) {
            lSetList(ep, PR_usage, nullptr);
            lSetList(ep, PR_project, nullptr);
         }

         /* update user usage */
         if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
            answer_list_on_error_print_or_exit(&alp, stderr);
            lFreeList(&alp);
         }

         /* update project usage */
         if (lp2 && lGetNumberOfElem(lp2) > 0) {
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp2, nullptr, nullptr);
            answer_list_on_error_print_or_exit(&alp, stderr);
            lFreeList(&alp);
         }
         
         lFreeList(&lp);
         lFreeList(&lp2);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-dcal calendar_name" */

      if (strcmp("-dcal", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         ep = lCreateElem(CAL_Type);
         lSetString(ep, CAL_name, *spp);
         lp = lCreateList("cal's to del", CAL_Type);
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CAL_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-dckpt ckpt_name" */

      if (strcmp("-dckpt", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);

         ep = lCreateElem(CK_Type);
         lSetString(ep, CK_name, *spp);
         lp = lCreateList("ckpt interfaces to del", CK_Type);
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CK_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-de server_name[,server_name,...]" */
      if (strcmp("-de", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, EH_Type, EH_name, *spp);
         if (!del_host_of_type(lp, ocs::gdi::Target::SGE_EH_LIST)) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-dh server_name[,server_name,...]" */
      if (strcmp("-dh", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, AH_Type, AH_name, *spp);
         if (!del_host_of_type(lp, ocs::gdi::Target::SGE_AH_LIST)) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-drqs rqs_name[,rqs_name,...]" */
      if (strcmp("-drqs", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, RQS_Type, RQS_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_RQS_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-dm user_list" */

      if (strcmp("-dm", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, UM_Type, UM_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UM_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-do user_list" */

      if (strcmp("-do", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, UO_Type, UO_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UO_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-dp pe-name" */

      if (strcmp("-dp", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         ep = lCreateElem(PE_Type);
         lSetString(ep, PE_name, *spp);
         lp = lCreateList("pe's to del", PE_Type);
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PE_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-ds server_name[,server_name,...]" */
      if (strcmp("-ds", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, SH_Type, SH_name, *spp);
         if (!del_host_of_type(lp, ocs::gdi::Target::SGE_SH_LIST)) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-du user_list list_name[,list_name,...]" */

      if (strcmp("-du", *spp) == 0) {
         
         /* no adminhost/manager check needed here */

         /* get user list */
         spp = sge_parser_get_next(spp);
         if (!*(spp+1)) {
            ERROR(MSG_ANSWER_NOLISTNAMEPROVIDEDTODUX_S, *spp);
            sge_usage(QCONF, stderr);
            DRETURN(1);
         }
         lString2List(*spp, &lp, UE_Type, UE_name, ",  ");
         
         /* get list_name list */
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &arglp, US_Type, US_name, ", ");

         /* remove users/groups from lp from the acls in alp */
         sge_client_del_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&lp);
         lFreeList(&alp);
         lFreeList(&arglp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-dul list_name_list" */

      if (strcmp("-dul", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, US_Type, US_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_US_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-duser user,..." */

      if (strcmp("-duser", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, UU_Type, UU_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dprj project,..." */

      if (strcmp("-dprj", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, PR_Type, PR_name, ", ");
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dstnode node_path[,...]"  delete sharetree node(s) */

      if (strcmp("-dstnode", *spp) == 0) {
         int modified = 0;
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         ep = lFirstRW(lp);
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            sge_parse_return = 1;
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw (argep, arglp) {

            lListElem *node = nullptr;
            const char *nodepath = nullptr;
            ancestors_t ancestors;

            nodepath = lGetString(argep, STN_name);
            if (nodepath) {
               memset(&ancestors, 0, sizeof(ancestors_t));
               node = search_named_node_path(ep, nodepath, &ancestors);
               if (node) {
                  if (lGetList(node, STN_children) == nullptr) {
                     if (ancestors.depth > 0) {
                        int i;
                        lList *siblings = nullptr;
                        lListElem *pnode = nullptr;
                        modified++;
                        if (ancestors.depth == 1)
                           pnode = ep;
                        else
                           pnode = ancestors.nodes[ancestors.depth-2];
                        fprintf(stderr, "%s\n", MSG_TREE_REMOVING);
                        for (i=0; i<ancestors.depth; i++)
                           fprintf(stderr, "/%s",
                                   lGetString(ancestors.nodes[i], STN_name));
                        fprintf(stderr, "\n");
                        siblings = lGetListRW(pnode, STN_children);
                        lRemoveElem(siblings, &node);
                        if (lGetNumberOfElem(siblings) == 0)
                           lSetList(pnode, STN_children, nullptr);
                     } else {
                        fprintf(stderr, "%s\n", MSG_TREE_CANTDELROOTNODE);
                     }
                  } else {
                     fprintf(stderr, "%s\n", MSG_TREE_CANTDELNODESWITHCHILD);
                  }
               } else {
                  fprintf(stderr, MSG_TREE_UNABLETOLACATEXINSHARETREE_S,
                          nodepath);
                  fprintf(stderr, "\n");
               }
               free_ancestors(&ancestors);

            }
         }
         lFreeList(&arglp);

         if (modified) {
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
            lFreeWhat(&what);
            ep = lFirstRW(alp);
            answer_exit_if_not_recoverable(ep);
            if (answer_get_status(ep) == STATUS_OK)
               fprintf(stderr, "%s\n", MSG_TREE_MODIFIEDSHARETREE);
            else {
               fprintf(stderr, "%s\n", lGetString(ep, AN_text));
               sge_parse_return = 1;
            }
            lFreeList(&alp);
         } else {
            fprintf(stderr, "%s\n", MSG_TREE_NOMIDIFIEDSHARETREE);
            DRETURN(1);
         }

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dstree" */

      if (strcmp("-dstree", *spp) == 0) {
         /* no adminhost/manager check needed here */
         
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-help" */

      if (strcmp("-help", *spp) == 0) {
         sge_usage(QCONF, stdout);
         DRETURN(0);
      }

/*----------------------------------------------------------------------------*/
      /* "-ks" */

      if (strcmp("-ks", *spp) == 0) {
         /* no adminhost/manager check needed here */

         alp = ocs::gdi::Client::gdi_kill(nullptr, SCHEDD_KILL);
         for_each_ep(aep, alp) {
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK)
               sge_parse_return = 1;
            answer_print_text(aep, stderr, nullptr, nullptr);
         }

         lFreeList(&alp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-km" */

      if (strcmp("-km", *spp) == 0) {
         /* no adminhost/manager check needed here */
         alp = ocs::gdi::Client::gdi_kill(nullptr, MASTER_KILL);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* -kec <id> ... */
      /* <id> may be "all" */
      /* parse before -ke[j] */

      if (strncmp("-kec", *spp, 4) == 0) {
         int action_flag = EVENTCLIENT_KILL;
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         /* found namelist -> process */
         if (strcmp(*spp, "all") == 0) { /* kill all dynamic event clients (EV_ID_ANY) */
            alp = ocs::gdi::Client::gdi_kill(nullptr, action_flag);
         } else {
            lString2List(*spp, &lp, ID_Type, ID_str, ", ");
            alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         }      
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/

      /* -at <name> ... */
      /* <name> may be "scheduler" */

      if (strncmp("-at", *spp, 4) == 0) {
         int action_flag = THREAD_START;
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw (ep, lp) {
            lSetUlong(ep, ID_action, SGE_THREAD_TRIGGER_START);
         }
         alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         lFreeList(&lp);
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* -kt <name> ... */
      /* <name> may be "scheduler" */

      if (strncmp("-kt", *spp, 4) == 0) {
         int action_flag = THREAD_START;
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw(ep, lp) {
            lSetUlong(ep, ID_action, SGE_THREAD_TRIGGER_STOP);
         }
         alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         lFreeList(&lp);
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/

      /* -ke[j] <host> ... */
      /* <host> may be "all" */
      if (strncmp("-k", *spp, 2) == 0) {
         int action_flag = EXECD_KILL;
         /* no adminhost/manager check needed here */

         cp = (*spp) + 2;
         switch (*cp++) {
            case 'e':
               break;
            default:
               ERROR(MSG_ANSWER_XISNOTAVALIDOPTIONY_SU, *spp, prog_number);
               sge_usage(QCONF, stderr);
               DRETURN(1);
         }

         if (*cp == 'j') {
            cp++;
            action_flag |= JOB_KILL;
         }

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
         } else {
            if (sge_error_and_exit(MSG_QCONF_NEEDAHOSTNAMEORALL))
               continue;
         }

         if (strcmp(*spp, "all") == 0) { /* kill all dynamic event clients (EV_ID_ANY) */
            alp = ocs::gdi::Client::gdi_kill(nullptr, action_flag);
         } else {   
            /* found namelist -> process */
            lString2List(*spp, &lp, EH_Type, EH_name, ", ");
            alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         }
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mc" */

      if (strcmp("-mc", *spp) == 0) {
         lList *answer_list = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         if (!centry_list_modify(&answer_list)) {
            sge_parse_return = 1;
         }
         sge_parse_return |= show_answer_list(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mcal cal-name" */

      if ((strcmp("-mcal", *spp) == 0) || 
          (strcmp("-Mcal", *spp) == 0)) {
         if (!strcmp("-mcal", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);
           
            where = lWhere("%T( %I==%s )", CAL_Type, CAL_name, *spp);
            what = lWhat("%T(ALL)", CAL_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CAL_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
               fprintf(stderr, MSG_CALENDAR_XISNOTACALENDAR_S, *spp);
               fprintf(stderr, "\n");
               lFreeList(&lp);
               DRETURN(1);
            }

            ep = lFirstRW(lp);
            filename = (char *)spool_flatfile_write_object(&alp, ep, false,
                                                 CAL_fields, &qconf_sfi,
                                                 SP_DEST_TMP, SP_FORM_ASCII,
                                                 nullptr, false);
            
            if (answer_list_output(&alp)) {
               if (filename != nullptr) {
                  unlink(filename);
                  sge_free(&filename);
               }
               sge_error_and_exit(nullptr);
            }

            lFreeList(&lp);
            
            /* edit this file */
            status = sge_edit(filename, uid, gid);
            if (status < 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_PARSE_EDITFAILED))
                  continue;
            }

            if (status > 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED))
                  continue;
            }

            /* read it in again */
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CAL_Type, nullptr,
                                            CAL_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CAL_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         } else {
            spp = sge_parser_get_next(spp);
           
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CAL_Type, nullptr,
                                            CAL_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CAL_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr)
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
         }

         /* send it to qmaster */
         lp = lCreateList("calendar to add", CAL_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CAL_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-Mc complex file" */

      if (strcmp("-Mc", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         if (!centry_list_modify_from_file(&answer_list, file)) {
            sge_parse_return = 1;
         }
         sge_parse_return |= show_answer_list(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-mckpt ckpt_name" or "-Mckpt fname" */

      if ((strcmp("-mckpt", *spp) == 0) || 
          (strcmp("-Mckpt", *spp) == 0)) {
         if (strcmp("-mckpt", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            /* get last version of this pe from qmaster */
            where = lWhere("%T( %I==%s )", CK_Type, CK_name, *spp);
            what = lWhat("%T(ALL)", CK_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CK_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp == nullptr || lGetNumberOfElem(lp) == 0 ) {
               fprintf(stderr, MSG_CKPT_XISNOTCHKPINTERFACEDEF_S, *spp);
               fprintf(stderr, "\n");
               lFreeList(&lp);
               DRETURN(1);
            }

            ep = lFirstRW(lp);
            filename = (char *)spool_flatfile_write_object(&alp, ep, false,
                                                 CK_fields, &qconf_sfi,
                                                 SP_DEST_TMP, SP_FORM_ASCII,
                                                 nullptr, false);
            
            if (answer_list_output(&alp)) {
               if (filename != nullptr) {
                  unlink(filename);
                  sge_free(&filename);
               }
               sge_error_and_exit(nullptr);
            }

            lFreeList(&lp);
            
            /* edit this file */
            status = sge_edit(filename, uid, gid);
            if (status < 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_PARSE_EDITFAILED))
                  continue;
            }

            if (status > 0) {
               unlink(filename);
               sge_free(&filename);
               if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED))
                  continue;
            }

            /* read it in again */
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CK_Type, nullptr,
                                            CK_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CK_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (ckpt_validate(ep, &alp) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         } else {
            spp = sge_parser_get_next(spp);

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, CK_Type, nullptr,
                                            CK_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(CK_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (ckpt_validate(ep, &alp) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         }

         /* send it to qmaster */
         lp = lCreateList("CKPT list to add", CK_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CK_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-Me fname" */
      if (strcmp("-Me", *spp) == 0) {
         spooling_field *fields = sge_build_EH_field_list(false, false, false);
         
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp); 

         /* read file */
         fields_out[0] = NoName;
         ep = spool_flatfile_read_object(&alp, EH_Type, nullptr,
                                         fields, fields_out, true, &qconf_sfi,
                                         SP_FORM_ASCII, nullptr, *spp);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }

         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
         }

         sge_free(&fields);
         
         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if (ep == nullptr) {
            fprintf(stderr, "%s\n", MSG_ANSWER_INVALIDFORMAT); 
            DRETURN(1);
         }

         /* test host name */
         if (sge_resolve_host(ep, EH_name) != CL_RETVAL_OK) {
            fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(ep, EH_name));
            fprintf(stderr, "\n");
            lFreeElem(&ep);
            sge_parse_return = 1;
            DRETURN(1);
         }

         lp = lCreateList("exechosts to change", EH_Type);
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

         sge_parse_return |= show_answer(alp);
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-me [server_name,...]" */

      if (strcmp("-me", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("hosts to change", &arglp, EH_Type, EH_name, 
            *spp);

         for_each_rw (argep, arglp) {
            /* resolve hostname */
            if (sge_resolve_host(argep, EH_name) != CL_RETVAL_OK) {
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(argep, EH_name));
               fprintf(stderr, "\n");
               sge_parse_return = 1;
               continue;
            }
            host = lGetHost(argep, EH_name);

            /* get the existing host entry .. */
            where = lWhere("%T( %Ih=%s )", EH_Type, EH_name, host);
            what = lWhat("%T(ALL)", EH_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            if (show_answer(alp) == 1) {
               lFreeList(&alp);
               sge_parse_return = 1;
               continue;
            }

            if (lGetNumberOfElem(lp) == 0) {
               fprintf(stderr, MSG_EXEC_XISNOTANEXECUTIONHOST_S, host);
               fprintf(stderr, "\n");
               sge_parse_return = 1;
               continue;
            }
            lFreeList(&alp);

            ep = edit_exechost(lFirstRW(lp), uid, gid);
            if (ep == nullptr) {
               continue;
            }
            lFreeList(&lp);
            lp = lCreateList("host to mod", EH_Type);
            lAppendElem(lp, ep);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
            lFreeList(&lp);

            if (show_answer(alp) == 1) {
               lFreeList(&alp);
               sge_parse_return = 1;
               continue;
            }
            lFreeList(&alp);
         }
         lFreeList(&arglp);
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-mrqs rqs_name" */
      if (strcmp("-mrqs", *spp) == 0) { 
         const char *name = nullptr;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         rqs_modify(&alp, name);
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-Mrqs fname [rqs_name,...]" */
      if (strcmp("-Mrqs", *spp) == 0) {
         const char *file = nullptr;
         const char *name = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp); 
            name = *spp;
         }

         rqs_modify_from_file(&alp, file, name);
         sge_parse_return |= show_answer_list(alp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-mp pe_name" */

      if ((strcmp("-mp", *spp) == 0) || 
          (strcmp("-Mp", *spp) == 0)) {

         if (!strcmp("-mp", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            /* get last version of this pe from qmaster */
            where = lWhere("%T( %I==%s )", PE_Type, PE_name, *spp);
            what = lWhat("%T(ALL)", PE_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PE_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
               fprintf(stderr, MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S, *spp);
               fprintf(stderr, "\n");
               lFreeList(&lp);
               DRETURN(1);
            }

            ep = lFirstRW(lp);

            /* write pe to temp file */
            filename = (char *)spool_flatfile_write_object(&alp, ep, false,
                                                 PE_fields, &qconf_sfi,
                                                 SP_DEST_TMP, SP_FORM_ASCII,
                                                 nullptr, false);
            lFreeList(&lp);

            if (answer_list_output(&alp)) {
               if (filename != nullptr) {
                  unlink(filename);
                  sge_free(&filename);
               }
               sge_error_and_exit(nullptr);
            }

            /* edit this file */
            status = sge_edit(filename, uid, gid);
            if (status < 0) {
               unlink(filename);
               if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
                  sge_free(&filename);
                  continue;
               }
            }

            if (status > 0) {
               unlink(filename);
               if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
                  sge_free(&filename);
                  continue;
               }
            }

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                            PE_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, filename);
            
            unlink(filename);
            sge_free(&filename);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(PE_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (pe_validate(ep, &alp, 0, nullptr) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         } else {
            spp = sge_parser_get_next(spp);

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                            PE_fields, fields_out, true, &qconf_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(PE_fields, fields_out, &alp);
            }

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if ((ep != nullptr) && (pe_validate(ep, &alp, 0, nullptr) != STATUS_OK)) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         }

         /* send it to qmaster */
         lp = lCreateList("PE list to add", PE_Type); 
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PE_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         
         sge_parse_return |= show_answer_list(alp);
         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
   if (strcmp("-sobjl", *spp) == 0) {
      dstring object_name = DSTRING_INIT;
      dstring attribute_pattern = DSTRING_INIT;
      dstring value_pattern = DSTRING_INIT;
      bool handle_cqueue;
      bool handle_domain;
      bool handle_qinstance;
      bool handle_exechost;

      spp = sge_parser_get_next(spp);
      sge_dstring_copy_string(&object_name, *spp);
      spp = sge_parser_get_next(spp);
      sge_dstring_copy_string(&attribute_pattern, *spp);
      spp = sge_parser_get_next(spp);
      sge_dstring_copy_string(&value_pattern, *spp);

      handle_cqueue = (strcmp(sge_dstring_get_string(&object_name), "queue") == 0) ? true : false;
      handle_domain = (strcmp(sge_dstring_get_string(&object_name), "queue_domain") == 0) ? true : false;
      handle_qinstance = (strcmp(sge_dstring_get_string(&object_name), "queue_instance") == 0) ? true : false;
      handle_exechost = (strcmp(sge_dstring_get_string(&object_name), "exechost") == 0) ? true : false;

      if (handle_exechost) {
         lEnumeration *what_all = nullptr;
         lList *list = nullptr;
         lListElem *elem = nullptr;
         lList *answer_list = nullptr;
         const lListElem *answer_ep;

         what_all = lWhat("%T(ALL)", EH_Type);
         answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &list, nullptr, what_all);
         lFreeWhat(&what_all);

         answer_ep = lFirst(answer_list);
         answer_exit_if_not_recoverable(answer_ep);
         if (answer_get_status(answer_ep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(answer_ep, AN_text));
            lFreeList(&answer_list);
            sge_dstring_free(&object_name);
            sge_dstring_free(&attribute_pattern);
            sge_dstring_free(&value_pattern);
            DRETURN(0);
         }
         lFreeList(&answer_list);

         for_each_rw (elem, list) {
            const char *hostname = nullptr;
            bool already_handled = false;

            /*
             * hostname
             */ 
            hostname = lGetHost(elem, EH_name);
            if (!fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_HOSTNAME, 0) &&
                !fnmatch(sge_dstring_get_string(&value_pattern), hostname, 0)) {
               printf("%s\n", hostname);
               already_handled = true;
            }

            /*
             * load scaling list
             */
            if (!already_handled && 
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_LOAD_SCALING, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_scaling_list);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_sprintf_append(&value, "%s=%.10g", lGetString(value_elem, HS_name), lGetDouble(value_elem, HS_value));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, ",");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  printf("%s\n", hostname);
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * complex_values list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_COMPLEX_VALUES, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_consumable_config_list);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_sprintf_append(&value, "%s=", lGetString(value_elem, CE_name));
                  if (lGetString(value_elem, CE_stringval) != nullptr) {
                     sge_dstring_append(&value, lGetString(value_elem, CE_stringval));
                  } else {
                     sge_dstring_sprintf_append(&value, "%f", lGetString(value_elem, CE_doubleval));
                  }
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, ",");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  printf("%s\n", hostname);
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * load_values list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_LOAD_VALUES, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_load_list);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_sprintf_append(&value, "%s=", lGetString(value_elem, HL_name));
                  sge_dstring_sprintf_append(&value, "%s", lGetString(value_elem, HL_value));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, ",");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  printf("%s\n", hostname);
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * processors 
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_PROCESSORS, 0)) {
               dstring value = DSTRING_INIT;
               const char *value_string = nullptr;

               sge_dstring_sprintf(&value, "%d", (int) lGetUlong(elem, EH_processors));
               value_string = sge_dstring_get_string(&value);
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  printf("%s\n", hostname);
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }


            /*
             * user_lists list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_USER_LISTS, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_acl);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_append(&value, lGetString(value_elem, US_name));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, " ");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  printf("%s\n", hostname);
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * user_lists list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_XUSER_LISTS, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_xacl);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_append(&value, lGetString(value_elem, US_name));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, " ");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  printf("%s\n", hostname);
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }
         }
      } else if (handle_cqueue || handle_domain || handle_qinstance) {
         lEnumeration *what_all = nullptr;
         lList *list = nullptr;
         lListElem *elem = nullptr;
         lList *answer_list = nullptr;
         const lListElem *answer_ep;

         what_all = lWhat("%T(ALL)", CQ_Type);
         answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CQ_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &list, nullptr, what_all);
         lFreeWhat(&what_all);

         answer_ep = lFirst(answer_list);
         answer_exit_if_not_recoverable(answer_ep);
         if (answer_get_status(answer_ep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(answer_ep, AN_text));
            lFreeList(&answer_list);
            sge_dstring_free(&object_name);
            sge_dstring_free(&attribute_pattern);
            sge_dstring_free(&value_pattern);
            DRETURN(0);
         }
         lFreeList(&answer_list);

         for_each_rw (elem, list) {
            int index = 0;
            bool already_handled = false;

            /*
             * Handle special case: qname
             */
            if (!fnmatch(sge_dstring_get_string(&attribute_pattern),
                         SGE_ATTR_QNAME, 0)) {
               if (handle_cqueue &&
                   !fnmatch(sge_dstring_get_string(&value_pattern), lGetString(elem, CQ_name), 0)) {
                  printf("%s\n", lGetString(elem, CQ_name));
                  already_handled = true;
               }
            }

            /*
             * Handle special case: hostlist
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern),
                         SGE_ATTR_HOST_LIST, 0)) {
               dstring value = DSTRING_INIT;
               const lList *hostref_list = lGetList(elem, CQ_hostlist);

               if (hostref_list != nullptr) {
                  href_list_append_to_dstring(hostref_list, &value);
               } else {
                  sge_dstring_copy_string(&value, "NONE");
               }
               if (handle_cqueue &&
                   !fnmatch(sge_dstring_get_string(&value_pattern), sge_dstring_get_string(&value), 0)) {
                  printf("%s\n", lGetString(elem, CQ_name));
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * Handle all other CQ attributes
             */
            while (!already_handled &&
                   cqueue_attribute_array[index].cqueue_attr != NoName) {
               if (!fnmatch(sge_dstring_get_string(&attribute_pattern),
                            cqueue_attribute_array[index].name, 0)) {
                  dstring value = DSTRING_INIT;
                  const lList *attribute_list = lGetList(elem, cqueue_attribute_array[index].cqueue_attr);
                  const lListElem *attribute;

                  already_handled = false;
                  for_each_ep(attribute, attribute_list) {
                     const lDescr *descr = lGetListDescr(attribute_list);
                     lList *tmp_attribute_list = lCreateList("", descr);
                     lListElem *tmp_attribute = lCopyElem(attribute);
                     const char *host_hgroup = lGetHost(attribute, cqueue_attribute_array[index].href_attr);
                     bool is_cqueue;
                     bool is_domain;
                     bool is_qinstance;

                     is_cqueue = (strcmp(host_hgroup, HOSTREF_DEFAULT) == 0) ? true : false;
                     is_domain = false;
                     if (!is_cqueue) {
                        is_domain = is_hgroup_name(host_hgroup);
                     }
                     is_qinstance = (!is_domain && !is_cqueue) ? true : false;

                     lAppendElem(tmp_attribute_list, tmp_attribute);
                     lSetHost(tmp_attribute,
                              cqueue_attribute_array[index].href_attr,
                              HOSTREF_DEFAULT);

                     attr_list_append_to_dstring(tmp_attribute_list,
                                                 &value, descr, cqueue_attribute_array[index].href_attr,
                                                 cqueue_attribute_array[index].value_attr);

                     if (!fnmatch(sge_dstring_get_string(&value_pattern), sge_dstring_get_string(&value), 0)) {
                        if (handle_cqueue && is_cqueue) {
                           printf("%s\n", lGetString(elem, CQ_name));
                        } else if ((handle_domain && is_domain) || (handle_qinstance && is_qinstance)) {
                           printf("%s@%s\n", lGetString(elem, CQ_name), host_hgroup);
                        }
                        already_handled = true;
                     }
                     lFreeList(&tmp_attribute_list);
                     if (already_handled) {
                        break;
                     }
                  }
                  sge_dstring_free(&value);
                  if (already_handled) {
                     break;
                  }
               }
               index++;
            }
         }
         lFreeList(&list);
      }
      sge_dstring_free(&object_name);
      sge_dstring_free(&attribute_pattern);
      sge_dstring_free(&value_pattern);

      spp++;
      continue;
   }

/*---------------------------------------------------------------------------*/

   if ((strcmp("-mattr", *spp) == 0) || (strcmp("-Mattr", *spp) == 0) ||
       (strcmp("-aattr", *spp) == 0) || (strcmp("-Aattr", *spp) == 0) ||   
       (strcmp("-rattr", *spp) == 0) || (strcmp("-Rattr", *spp) == 0) ||   
       (strcmp("-dattr", *spp) == 0) || (strcmp("-Dattr", *spp) == 0)) {   
     
/* *INDENT-OFF* */ 
      static object_info_entry info_entry[] = {
         {ocs::gdi::Target::SGE_CQ_LIST,         SGE_OBJ_CQUEUE,    CQ_Type,   SGE_ATTR_QNAME,     CQ_name,   nullptr,     &qconf_sfi,        cqueue_xattr_pre_gdi},
         {ocs::gdi::Target::SGE_EH_LIST,         SGE_OBJ_EXECHOST,  EH_Type,   SGE_ATTR_HOSTNAME,  EH_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::SGE_PE_LIST,         SGE_OBJ_PE,        PE_Type,   SGE_ATTR_PE_NAME,   PE_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::SGE_CK_LIST,         SGE_OBJ_CKPT,      CK_Type,   SGE_ATTR_CKPT_NAME, CK_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::SGE_HGRP_LIST,       SGE_OBJ_HGROUP,    HGRP_Type, SGE_ATTR_HGRP_NAME, HGRP_name, nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::SGE_RQS_LIST,        SGE_OBJ_RQS,       RQS_Type,  SGE_ATTR_RQS_NAME,  RQS_name,  nullptr,     &qconf_rqs_sfi,    rqs_xattr_pre_gdi},
         {ocs::gdi::Target::NO_TARGET,           nullptr,           nullptr,   nullptr,            NoName,    nullptr,     nullptr,           nullptr}
      }; 
/* *INDENT-ON* */
      
      int from_file;
      int index;
      int ret = 0;
      ocs::gdi::SubCommand::SubCmd sub_command = ocs::gdi::SubCommand::SGE_GDI_SUB_NONE;
   
      /* This does not have to be freed later */
      info_entry[0].fields = CQ_fields;
      /* These have to be freed later */
      info_entry[1].fields = sge_build_EH_field_list(false, false, false);
      /* These do not */
      info_entry[2].fields = PE_fields;
      info_entry[3].fields = CK_fields;
      info_entry[4].fields = HGRP_fields;
      info_entry[5].fields = RQS_fields;
      
      /* no admin host/manager check needed here */
        
      /* Capital letter => we will read from file */
      if (isupper((*spp)[1])) {
         from_file = 1;
      } else {
         from_file = 0;
      }

      /* Set sub command for co_gdi call */
      if ((*spp)[1] == 'm' || (*spp)[1] == 'M') {
         sub_command = ocs::gdi::SubCommand::SGE_GDI_CHANGE;
      } else if ((*spp)[1] == 'a' || (*spp)[1] == 'A') {
         sub_command = ocs::gdi::SubCommand::SGE_GDI_APPEND;
      } else if ((*spp)[1] == 'd' || (*spp)[1] == 'D') {
         sub_command = ocs::gdi::SubCommand::SGE_GDI_REMOVE;
      } else if ((*spp)[1] == 'r' || (*spp)[1] == 'R') {
         sub_command = ocs::gdi::SubCommand::SGE_GDI_SET;
      }
      spp = sge_parser_get_next(spp);

      /* is the objectname given in commandline
         supported by this function */
      index = 0;
      while(info_entry[index].object_name) {
         if (!strcmp(info_entry[index].object_name, *spp)) {
            spp = sge_parser_get_next(spp);
            break; 
         }
         index++;
      }

      if (!info_entry[index].object_name) {
         fprintf(stderr, "Modification of object " SFQ " not supported\n", *spp);
         sge_free(&(info_entry[1].fields));
         DRETURN(1);
      } 

      /* */
      DTRACE;
      ret = qconf_modify_attribute(&alp, from_file, &spp, &ep, sub_command, &(info_entry[index]));
      lFreeElem(&ep);

      /* Error handling */
      if (ret || lGetNumberOfElem(alp)) {
         int exit = 0;

         for_each_ep(aep, alp) {
            FILE *std_x = nullptr;
            
            if (lGetUlong(aep, AN_status) != STATUS_OK) {
               std_x = stderr;
               exit = 1;
            } else {
               std_x = stdout;
            } 
            fprintf(std_x, "%s\n", lGetString(aep, AN_text)); 
         }
         lFreeList(&alp);   
         if (exit) {
            sge_free(&(info_entry[1].fields));
            DRETURN(1);
         }
      }
      
      sge_free(&(info_entry[1].fields));
      
      continue;
   }


/*----------------------------------------------------------------------------*/
   /* "-purge" */
   if (strcmp("-purge", *spp) == 0) {

      static object_info_entry info_entry[] = {
         {ocs::gdi::Target::SGE_CQ_LIST, SGE_OBJ_CQUEUE, QR_Type, SGE_ATTR_QNAME, QR_name, nullptr, &qconf_sfi, cqueue_xattr_pre_gdi},
         {ocs::gdi::Target::NO_TARGET,           nullptr,        nullptr, nullptr,        NoName,  nullptr, nullptr,    nullptr}
      };

      int index = 0;
      char *object_instance = nullptr;
      char *object = nullptr;
      char *hgroup_or_hostname = nullptr;
      char *attr = nullptr;
      lListElem *cqueue = nullptr;

      /* This does not have to be freed later */
      info_entry[0].fields = CQ_fields;

      spp = sge_parser_get_next(spp);

      /* is the object given in commandline
         supported by this function */
      index = 0;
      while(info_entry[index].object_name) {
         if (!strcmp(info_entry[index].object_name, *spp)) {
            spp = sge_parser_get_next(spp);
         break; 
      }
      index++;
   }

   if (!info_entry[index].object_name) {
      ERROR(MSG_QCONF_MODIFICATIONOFOBJECTNOTSUPPORTED_S, *spp);
      DRETURN(1);
   } 

   /* parse command line arguments */
   attr = sge_strdup(nullptr, *spp);
   if (attr == nullptr) {
      ERROR(SFNMAX, MSG_QCONF_NOATTRIBUTEGIVEN);
      DRETURN(1);
   }
   spp = sge_parser_get_next(spp);

   object_instance = sge_strdup(nullptr, *spp);

   /* object_instance look like queue@host */
   if ((object = sge_strdup(nullptr, sge_strtok(object_instance, "@"))) != nullptr) {
       hgroup_or_hostname = sge_strdup(nullptr, sge_strtok(nullptr, nullptr));
   }
   
   if (object == nullptr || hgroup_or_hostname == nullptr) {
      ERROR(MSG_QCONF_GIVENOBJECTINSTANCEINCOMPLETE_S, object_instance);
      sge_free(&attr);
      sge_free(&object_instance);
      sge_free(&object);
      DRETURN(1);
   }
     
   /* queue_instance no longer neede */
   sge_free(&object_instance);

   if (strcmp("@/", hgroup_or_hostname) == 0) {
      ERROR(MSG_QCONF_MODIFICATIONOFHOSTNOTSUPPORTED_S, hgroup_or_hostname);
      sge_free(&attr);
      sge_free(&object);
      sge_free(&hgroup_or_hostname);
      DRETURN(1);
   }

      /* now get the queue, delete the objects and send the queue back to the master */
      cqueue = cqueue_get_via_gdi(&alp, object);

      if (cqueue == nullptr) {
         ERROR(MSG_CQUEUE_DOESNOTEXIST_S, object);
         sge_free(&attr); 
         sge_free(&object);
         sge_free(&hgroup_or_hostname);
         DRETURN(1);
      }

      parse_name_list_to_cull("attribute list", &lp, US_Type, US_name, attr);
      if (cqueue_purge_host(cqueue, &alp, lp, hgroup_or_hostname)) {
         cqueue_add_del_mod_via_gdi(cqueue, &alp, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SET_ALL);
      } else {
         WARNING(MSG_QCONF_ATTR_ARGS_NOT_FOUND, attr, hgroup_or_hostname);
      }
      lFreeList(&lp);

      /* Error handling */
      for_each_ep(aep, alp) {
            FILE *std_x = nullptr;
            
            if (lGetUlong(aep, AN_status) != STATUS_OK) {
               std_x = stderr;
            } else {
               std_x = stdout;
            } 
            fprintf(std_x, "%s\n", lGetString(aep, AN_text)); 
         }

      if (cqueue != nullptr) {
         lFreeElem(&cqueue);
      }

      sge_free(&attr); 
      sge_free(&object);
      sge_free(&hgroup_or_hostname);
      spp++;
      continue;
   }

/*----------------------------------------------------------------------------*/
      /* "-Msconf" */
      if (strcmp("-Msconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         fields_out[0] = NoName;
         ep = spool_flatfile_read_object(&alp, SC_Type, nullptr,
                                         SC_fields, fields_out, true, &qconf_comma_sfi,
                                         SP_FORM_ASCII, nullptr, *spp);

         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }
         
         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(SC_fields, fields_out, &alp);
         }

         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }
         
         if (ep != nullptr) {
            lp = lCreateList("scheduler config", SC_Type);
            lAppendElem (lp, ep);
            
            if (!sconf_validate_config (&alp, lp)) {
               lFreeList(&lp);
               answer_list_output(&alp);
            }
         }
         
         /* else we let the check for lp != nullptr catch the error below */

         if ((lp == nullptr) && (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE))) {
            continue;
         }

         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_SC_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_SCHEDD_CHANGEDSCHEDULERCONFIGURATION);
         else { /* error */
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
         }

         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
         
      }

/*----------------------------------------------------------------------------*/
      /* "-msconf"  modify scheduler configuration */

      if (strcmp("-msconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* get the scheduler configuration .. */
         what = lWhat("%T(ALL)", SC_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_SC_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (!(newlp=edit_sched_conf(lp, uid, gid)))
            continue;

         lFreeList(&lp);
         what = lWhat("%T(ALL)", SC_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_SC_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &newlp, nullptr, what);
         lFreeWhat(&what);
         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_SCHEDD_CHANGEDSCHEDULERCONFIGURATION);
         else {
            fprintf(stderr, "%s\n", lGetString(ep, AN_text));
            sge_parse_return = 1;
         }
         lFreeList(&newlp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mstree", "-Mstree file": modify sharetree */

      if ((strcmp("-mstree", *spp) == 0) || (strcmp("-Mstree", *spp) == 0)) {
         lListElem *unspecified = nullptr;
         
         if (strcmp("-mstree", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            /* get the sharetree .. */
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               spp++;
               continue;
            }
            lFreeList(&alp);
    
            ep = lFirstRW(lp);
            if (!(ep = edit_sharetree(ep, uid, gid)))
               continue;

            lFreeList(&lp);
         } else {
            spooling_field *fields = sge_build_STN_field_list(false, true);

            spp = sge_parser_get_next(spp);
           
            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                            fields, fields_out, true,
                                            &qconf_name_value_list_sfi,
                                            SP_FORM_ASCII, nullptr, *spp);
            
            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }
         
            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
            }

            sge_free(&fields);
            
            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }
               
            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE))
                  continue;
            }
         }

         /* Make sure that no nodes are left unspecified.  An unspecified node
          * happens when a node appears in another node's child list, but does
          * not appear itself. */
         unspecified = sge_search_unspecified_node(ep);
         
         if (unspecified != nullptr) {
            fprintf(stderr, MSG_STREE_NOVALIDNODEREF_U, lGetUlong(unspecified, STN_id));
            fprintf(stderr, "\n");
            sge_parse_return = 1;

            lFreeElem(&ep);
            spp++;
            continue;
         }
         
         newlp = lCreateList("sharetree modify", STN_Type);
         lAppendElem(newlp, ep);

         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &newlp, nullptr, what);
         lFreeWhat(&what);
         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_TREE_CHANGEDSHARETREE);
         else {
            fprintf(stderr, "%s\n", lGetString(ep, AN_text));
            sge_parse_return = 1;
         }
         lFreeList(&alp);
         lFreeList(&newlp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mu userset,..." */

      if (strcmp("-mu", *spp) == 0) {
         /* check for adminhost and manager privileges */
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         /* get userset */
         parse_name_list_to_cull("usersets", &lp, US_Type, US_name, *spp);

         if (edit_usersets(lp) != 0) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Mu fname" */

      if (strcmp("-Mu", *spp) == 0) {
         char* file = nullptr;
         const char* usersetname = nullptr;
         lList *acl = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }     

          
         /* get userset from file */
         ep = nullptr;
         fields_out[0] = NoName;
         ep = spool_flatfile_read_object(&alp, US_Type, nullptr,
                                         US_fields, fields_out, true, &qconf_param_sfi,
                                         SP_FORM_ASCII, nullptr, file);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }
         
         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(US_fields, fields_out, &alp);
         }

         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if ((ep != nullptr) &&
            (userset_validate_entries(ep, &alp) != STATUS_OK)) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }
            
         if (ep == nullptr) {
            sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE); 
         }
         usersetname = lGetString(ep, US_name);
 
         /* get userset from qmaster */
         where = lWhere("%T( %I==%s )", US_Type, US_name, usersetname);
         what = lWhat("%T(ALL)", US_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_US_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeElem(&ep);
            lFreeList(&lp);
            DRETURN(1); 
         }

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_PROJECT_XISNOKNWOWNPROJECT_S, usersetname);
            fprintf(stderr, "\n");
            fflush(stdout);
            fflush(stderr);
            lFreeList(&alp);
            lFreeElem(&ep);
            lFreeList(&lp);
            DRETURN(1); 
         }
         lFreeList(&alp);
         lFreeList(&lp);

         acl = lCreateList("modified usersetlist", US_Type); 
         lAppendElem(acl, ep);

         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_US_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &acl, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&acl);
            DRETURN(1);
         } 
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
         lFreeList(&alp);
         lFreeList(&acl);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Au fname" */

      if (strcmp("-Au", *spp) == 0) {
         lList *acl = nullptr;
         char* file = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }     

          
         /* get userset  */
         ep = nullptr;
         fields_out[0] = NoName;
         ep = spool_flatfile_read_object(&alp, US_Type, nullptr,
                                         US_fields, fields_out,  true,
                                         &qconf_param_sfi,
                                         SP_FORM_ASCII, nullptr, file);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }

         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(US_fields, fields_out, &alp);
         }
         
         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if ((ep != nullptr) && (userset_validate_entries(ep, &alp) != STATUS_OK)) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }
            
         if (ep == nullptr) {
            sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE); 
         }

         acl = lCreateList("usersetlist list to add", US_Type); 
         lAppendElem(acl,ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_US_LIST, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &acl, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&acl);
            DRETURN(1);
         } 
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
         lFreeList(&alp);
         lFreeList(&acl);
         spp++;
         continue;
      }


/*----------------------------------------------------------------------------*/

      /* "-muser username" */

      if (strcmp("-muser", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
        
         /* get user */
         where = lWhere("%T( %I==%s )", UU_Type, UU_name, *spp);
         what = lWhat("%T(ALL)", UU_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&lp);
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_USER_XISNOKNOWNUSER_S, *spp);
            fprintf(stderr, "\n");
            spp++;
            lFreeList(&lp);
            continue;
         }
         ep = lFirstRW(lp);
         
         /* edit user */
         newep = edit_user(ep, uid, gid);

         /* if the user name has changed, we need to print an error message */   
         if (newep == nullptr || strcmp(lGetString(ep, UU_name), lGetString(newep, UU_name)) != 0) {
            fprintf(stderr, MSG_QCONF_CANTCHANGEOBJECTNAME_SS, lGetString(ep, UU_name), lGetString(newep, UU_name));
            fprintf(stderr, "\n");
            lFreeElem(&newep);
            lFreeList(&lp);
            DRETURN(1);
         } else {
            lFreeList(&lp);
            /* send it to qmaster */
            lp = lCreateList("User list to modify", UU_Type); 
            lAppendElem(lp, newep);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               lFreeList(&lp);
               DRETURN(1);
            } else {
               fprintf(stdout, "%s\n", lGetString(aep, AN_text));
            }
         }
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mprj projectname" */

      if (strcmp("-mprj", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
        
         /* get project */
         where = lWhere("%T( %I==%s )", PR_Type, PR_name, *spp);
         what = lWhat("%T(ALL)", PR_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);
                  
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_PROJECT_XISNOKNWOWNPROJECT_S, *spp);
            fprintf(stderr, "\n");
            lFreeList(&lp);
            continue;
         }
         lFreeList(&alp);
         ep = lFirstRW(lp);
         
         /* edit project */
         newep = edit_project(ep, uid, gid);

         /* send it to qmaster */
         lFreeList(&lp);
         lp = lCreateList("Project list to modify", PR_Type); 
         lAppendElem(lp, newep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }




/*----------------------------------------------------------------------------*/

      /* "-Muser file" */

      if (strcmp("-Muser", *spp) == 0) {
         char* file = nullptr;
         const char* uname = nullptr;
         spooling_field *fields = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }

         /* get user from file */
         newep = nullptr;
         fields_out[0] = NoName;
         fields = sge_build_UU_field_list(false);
         newep = spool_flatfile_read_object(&alp, UU_Type, nullptr,
                                         fields, fields_out, true, &qconf_sfi,
                                         SP_FORM_ASCII, nullptr, file);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&newep);
         }
         
         if (newep != nullptr) {
            missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
         }

         sge_free(&fields);
         
         if (missing_field != NoName) {
            lFreeElem(&newep);
            answer_list_output(&alp);
         }

         if (newep == nullptr) {
            sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE); 
         }

         uname = lGetString(newep, UU_name);
                 
         /* get user */
         where = lWhere("%T( %I==%s )", UU_Type, UU_name, uname);
         what = lWhat("%T(ALL)", UU_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);
                  
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeElem(&newep);
            lFreeList(&lp);
            DRETURN(1); 
         }

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_USER_XISNOKNOWNUSER_S, uname);
            fprintf(stderr, "\n");
            fflush(stdout);
            fflush(stderr);
            lFreeList(&alp);
            lFreeElem(&newep);
            lFreeList(&lp);
            DRETURN(1); 
         }
         lFreeList(&alp);

         /* send it to qmaster */
         lFreeList(&lp);
         lp = lCreateList("User list to modify", UU_Type); 
         lAppendElem(lp, newep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
           fprintf(stderr, "%s\n", lGetString(aep, AN_text));
           lFreeList(&alp);
           lFreeList(&lp);
           DRETURN(1);
         } else {
           fprintf(stdout, "%s\n", lGetString(aep, AN_text));
         }
          
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Mprj file" */

      if (strcmp("-Mprj", *spp) == 0) {
         char* file = nullptr;
         const char* projectname = nullptr;
         spooling_field *fields = nullptr;
   
         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
            if (!sge_is_file(*spp)) {
               sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
            }
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }

         /* get project from file */
         newep = nullptr;
         fields_out[0] = NoName;
         fields = sge_build_PR_field_list(false);
         newep = spool_flatfile_read_object(&alp, PR_Type, nullptr,
                                         fields, fields_out, true, &qconf_sfi,
                                         SP_FORM_ASCII, nullptr, file);
         
         if (answer_list_output(&alp)) {
            lFreeElem(&newep);
         }
         
         if (newep != nullptr) {
            missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
         }

         sge_free(&fields);
         
         if (missing_field != NoName) {
            lFreeElem(&newep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if (newep == nullptr) {
            sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE); 
         } 
         
         projectname = lGetString(newep, PR_name); 
                 
         /* get project */
         where = lWhere("%T( %I==%s )", PR_Type, PR_name, projectname);
         what = lWhat("%T(ALL)", PR_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);
                  
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeElem(&newep);
            lFreeList(&lp);
            DRETURN(1); 
         }

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_PROJECT_XISNOKNWOWNPROJECT_S, projectname);
            fprintf(stderr, "\n");
            fflush(stdout);
            fflush(stderr);
            lFreeList(&lp);
            lFreeList(&alp);
            lFreeElem(&newep);
            DRETURN(1); 
         }
         lFreeList(&alp);
         lFreeList(&lp);
         
         /* send it to qmaster */
         lp = lCreateList("Project list to modify", PR_Type); 
         lAppendElem(lp, newep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
         sge_parse_return |= show_answer_list(alp);
         
         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sc" */

      if (strcmp("-sc", *spp) == 0) {
         lList *answer_list = nullptr;

         if (!centry_list_show(&answer_list)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }   
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scatl complex_name_list" */

      if (strcmp("-scatl", *spp) == 0) {
         lList *answer_list = nullptr;

         if (!ocs::CategoryQconf::show_list(&answer_list)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scal calendar_name" */
      if (strcmp("-scal", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         /* get the existing pe entry .. */
         where = lWhere("%T( %I==%s )", CAL_Type, CAL_name, *spp);
         what = lWhat("%T(ALL)", CAL_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CAL_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (!lp || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_CALENDAR_XISNOTACALENDAR_S, *spp);
            fprintf(stderr, "\n");
            lFreeList(&lp);
            DRETURN(1);
         }

         ep = lFirstRW(lp);
         filename_stdout = spool_flatfile_write_object(&alp, ep, false,
                                              CAL_fields, &qconf_sfi,
                                              SP_DEST_STDOUT, SP_FORM_ASCII,
                                              nullptr, false);
         sge_free(&filename_stdout);
         lFreeList(&lp);
         if (answer_list_output(&alp)) {
            sge_error_and_exit(nullptr);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-scall" */

      if (strcmp("-scall", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_CAL_LIST, CAL_Type, CAL_name, "calendar")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sconf host_list" || "-mconf host_list" || "-aconf host_list" || "-Mconf file_list" || "-Aconf file_list" */
      /* file list is also host list */
      if ((strcmp("-sconf", *spp) == 0) || 
         (strcmp("-aconf", *spp) == 0) || 
         (strcmp("-mconf", *spp) == 0) ||
         (strcmp("-Mconf", *spp) == 0) ||
         (strcmp("-Aconf", *spp) == 0)) {
         typedef enum {
            ACTION_sconf = 0,
            ACTION_aconf,
            ACTION_mconf,
            ACTION_Aconf,
            ACTION_Mconf
         } action_enum;   
         action_enum action = ACTION_sconf;
         char *host_list = nullptr;
         int ret, first = 1;
         lListElem *host_ep;
         const char *hostname;

         if (!strcmp("-aconf", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            action = ACTION_aconf;
         } else if (!strcmp("-mconf", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            action = ACTION_mconf;
         } else if (!strcmp("-Aconf", *spp)) {
            action = ACTION_Aconf;
         } else if (!strcmp("-Mconf", *spp)) {
            action = ACTION_Mconf;
         }

         if (!sge_next_is_an_opt(spp))  {
            spp = sge_parser_get_next(spp);
            host_list = sge_strdup(nullptr, *spp);
         } else {
            host_list = sge_strdup(nullptr, SGE_GLOBAL_NAME);
         }
            
         /* host_list might look like host1,host2,... */
         host_ep = lCreateElem(EH_Type);

         for ((cp = sge_strtok(host_list, ",")); cp && *cp;
             (cp = sge_strtok(nullptr, ","))) {

            if (!first) {
               fprintf(stdout, "\n");
            }

            /*
            ** it would be uncomfortable if you could only give files in .
            */
            if ((action == ACTION_Aconf || action == ACTION_Mconf) && cp && strrchr(cp, '/')) {
               lSetHost(host_ep, EH_name, strrchr(cp, '/') + 1);
            } else {
               lSetHost(host_ep, EH_name, cp);
            }
            
            switch ((ret=sge_resolve_host(host_ep, EH_name))) {
            case CL_RETVAL_OK:
               break;
            default:
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_SS, cp, cl_get_error_text(ret));
               fprintf(stderr, "\n");
               break;
            }
            hostname = lGetHost(host_ep, EH_name);

            first = 0;

            if (ret != CL_RETVAL_OK && (action == ACTION_sconf || action == ACTION_aconf) ) {
               sge_parse_return = 1;
               continue;
            }   
               
            if (action == ACTION_sconf) {
               if (print_config(hostname) != 0) {
                  sge_parse_return = 1;
               }
            } else if (action == ACTION_aconf) {
               if (add_modify_config(hostname, nullptr, 1) != 0) {
                  sge_parse_return = 1;
               }
            } else if (action == ACTION_mconf) {
               if (add_modify_config(hostname, nullptr, 0) != 0) {
                  sge_parse_return = 1;
               }
            } else if (action == ACTION_Aconf) {
               if (add_modify_config(hostname, cp, 1) != 0) {
                  sge_parse_return = 1;
               }
            } else if (action == ACTION_Mconf) {
               if (add_modify_config(hostname, cp, 2) != 0) {
                  sge_parse_return = 1;
               }
            }

         } /* end for */
         
         sge_free(&host_list);
         lFreeElem(&host_ep);

         spp++;
         continue;
      }
      
/*-----------------------------------------------------------------------------*/
      /* "-sckpt ckpt_name" */
      if (strcmp("-sckpt", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         /* get the existing ckpt entry .. */
         where = lWhere("%T( %I==%s )", CK_Type, CK_name, *spp);
         what = lWhat("%T(ALL)", CK_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CK_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
            spp++;
            lFreeList(&alp);
            lFreeList(&lp);
            continue;
         }
         lFreeList(&alp);

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_CKPT_XISNOTCHKPINTERFACEDEF_S, *spp);
            fprintf(stderr, "\n");
            lFreeList(&lp);
            DRETURN(1);
         }

         ep = lFirstRW(lp);
         filename_stdout = spool_flatfile_write_object(&alp, ep, false,
                                             CK_fields, &qconf_sfi,
                                             SP_DEST_STDOUT, SP_FORM_ASCII,
                                             nullptr, false);
         sge_free(&filename_stdout);
         lFreeList(&lp);
         if (answer_list_output(&alp)) {
            sge_error_and_exit(nullptr);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sckptl" */
      if (strcmp("-sckptl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_CK_LIST, CK_Type, CK_name,
               "ckpt interface definition")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sconfl" */

      if (strcmp("-sconfl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_CONF_LIST, CONF_Type, CONF_name, "config")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-dconf config_list" */
      if (strcmp("-dconf", *spp) == 0) {
         char *host_list = nullptr;
         lListElem *host_ep = nullptr;
         const char *hostname = nullptr;
         int ret;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp))  {
            spp = sge_parser_get_next(spp);

            host_list = sge_strdup(nullptr, *spp);
            host_ep = lCreateElem(EH_Type);

            for ((cp = sge_strtok(host_list, ",")); cp && *cp;
                (cp = sge_strtok(nullptr, ","))) {
               
               lSetHost(host_ep, EH_name, cp);
               
               switch (sge_resolve_host(host_ep, EH_name)) {
               case CL_RETVAL_OK:
                  break;
               default:
                  fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, cp);
                  fprintf(stderr, "\n");
                  sge_parse_return = 1;
                  break;
               }
               hostname = lGetHost(host_ep, EH_name);
               ret = delete_config(hostname);
               /*
               ** try the unresolved name if this was different
               */
               if (ret && strcmp(cp, hostname) != 0) {
                  delete_config(cp);
               }
            } /* end for */

            sge_free(&host_list);
            lFreeElem(&host_ep);
         }
         else {
            fprintf(stderr, "%s\n", MSG_ANSWER_NEEDHOSTNAMETODELLOCALCONFIG);
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-se exec_server" */
      if (strcmp("-se", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         /* resolve host */
         hep = lCreateElem(EH_Type);
         lSetHost(hep, EH_name, *spp);
         
         switch (sge_resolve_host(hep, EH_name)) {
         case CL_RETVAL_OK:
            break;
         default:
            fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(hep, EH_name));
            fprintf(stderr, "\n");
            lFreeElem(&hep);
            DRETURN(1);
         }

         host = lGetHost(hep, EH_name);
        
         /* get the existing host entry .. */
         where = lWhere("%T( %Ih=%s )", EH_Type, EH_name, host);
         what = lWhat("%T(ALL)", EH_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&lp);
            lFreeList(&alp);
            lFreeElem(&hep);
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_EXEC_XISNOTANEXECUTIONHOST_S, host);
            fprintf(stderr, "\n");
            lFreeList(&lp);
            lFreeElem(&hep);
            sge_parse_return = 1; 
            spp++;
            continue;
         }

         lFreeElem(&hep);
         ep = lFirstRW(lp);
         
         {
            spooling_field *fields = sge_build_EH_field_list(false, true, false);
            filename_stdout = spool_flatfile_write_object(&alp, ep, false, fields, &qconf_sfi,
                                        SP_DEST_STDOUT, SP_FORM_ASCII, nullptr,
                                        false);
            lFreeList(&lp);
            sge_free(&fields);
            sge_free(&filename_stdout);
            
            if (answer_list_output(&alp)) {
               sge_parse_return = 1; 
            }
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-secl" */
      if (strcmp("-secl", *spp) == 0) {
         show_eventclients();
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sel" */
      if (strcmp("-sel", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_EH_LIST, EH_Type, EH_name,
               "execution host")) { 
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sep" */
      if (strcmp("-sep", *spp) == 0) {
         show_processors(has_binding_param);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sh" */
      if (strcmp("-sh", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_AH_LIST, AH_Type, AH_name,
               "administrative host")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-srqsl " */
      if (strcmp("-srqsl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_RQS_LIST, RQS_Type, RQS_name, "resource quota set list")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-srqs [rqs_name,...]" */
      if (strcmp("-srqs", *spp) == 0) {
         const char *name = nullptr;
         bool ret = true;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         ret = rqs_show(&alp, name);
         if (!ret) {
            show_answer(alp);
            sge_parse_return = 1;
         }
         lFreeList(&alp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-stl " */
      if (strcmp("-stl", *spp) == 0) {
         if (!show_thread_list()) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sm" */

      if (strcmp("-sm", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_UM_LIST, UM_Type, UM_name, "manager")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sp pe_name" */
      if (strcmp("-sp", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         /* get the existing pe entry .. */
         where = lWhere("%T( %I==%s )", PE_Type, PE_name, *spp);
         what = lWhat("%T(ALL)", PE_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PE_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&lp);
            lFreeList(&alp);
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr,  MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S , *spp);
            fprintf(stderr, "\n");
            lFreeList(&lp);
            DRETURN(1);
         }

         ep = lFirstRW(lp);
         
         {
            filename_stdout = spool_flatfile_write_object(&alp, ep, false,
                                                 PE_fields, &qconf_sfi,
                                                 SP_DEST_STDOUT, SP_FORM_ASCII,
                                                 nullptr, false);
            lFreeList(&lp);
            sge_free(&filename_stdout);
            
            if (answer_list_output(&alp)) {
               sge_error_and_exit(nullptr);
            }
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-spl" */
      if (strcmp("-spl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_PE_LIST, PE_Type, PE_name,
               "parallel environment")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scel" */
      if (strcmp("-scel", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_CE_LIST, CE_Type, CE_name, "complex entry")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-so" */

      if (strcmp("-so", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_UO_LIST, UO_Type, UO_name, "operator")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ssconf" */

      if (strcmp("-ssconf", *spp) == 0) {
         /* get the scheduler configuration .. */
         what = lWhat("%T(ALL)", SC_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_SC_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1; 
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         filename_stdout = spool_flatfile_write_object(&alp, lFirst(lp), false, SC_fields,
                                     &qconf_comma_sfi, SP_DEST_STDOUT,
                                     SP_FORM_ASCII, nullptr, false);
        
         sge_free(&filename_stdout);
         if (answer_list_output(&alp)) {
            fprintf(stderr, "%s\n", MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION);
            sge_parse_return = 1; 
            spp++;
            continue;
         }

         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-sstnode node_path[,...]"  show sharetree node */

      if (strcmp("-sstnode", *spp) == 0) {
         int found = 0;

         spp = sge_parser_get_next(spp);

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         ep = lFirstRW(lp);
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw (argep, arglp) {
            const char *nodepath = lGetString(argep, STN_name);

            if (nodepath) {
               found = show_sharetree_path(ep, nodepath);
            }
         }

         if ( found != 0 ) {
            DRETURN(1);
         }

         lFreeList(&arglp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-rsstnode node_path[,...]"  show sharetree node */

      if (strcmp("-rsstnode", *spp) == 0) {
         int found = 0;

         spp = sge_parser_get_next(spp);

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         ep = lFirstRW(lp);
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw(argep, arglp) {
            const char *nodepath = nullptr;
            nodepath = lGetString(argep, STN_name);
            if (nodepath) {
               show_sharetree_path(ep, nodepath);
            }
         }                                                                      

         if (!found && *(spp+1) == nullptr) {
            DRETURN(1);
         }

         lFreeList(&arglp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-sstree" */

      if (strcmp("-sstree", *spp) == 0) {
         spooling_field *fields = nullptr;

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         sge_parse_return |= show_answer_list(alp);
         if (sge_parse_return) {
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            continue;
         }
 
         ep = lFirstRW(lp);

         if (ep == nullptr) {
            fprintf(stderr, "%s\n", MSG_OBJ_NOSTREEELEM);
            sge_parse_return = 1;
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            continue;
         }

         fields = sge_build_STN_field_list(false, true);
         filename_stdout = spool_flatfile_write_object(&alp, ep, true, fields,
                                     &qconf_name_value_list_sfi,
                                     SP_DEST_STDOUT, SP_FORM_ASCII, 
                                     nullptr, false);
         sge_free(&fields);
         sge_free(&filename_stdout);
         sge_parse_return |= show_answer_list(alp);
         if (sge_parse_return) {
            sge_error_and_exit(nullptr);
         }

         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-bonsai" This is the undocumented switch of showing the share tree
         we keep this switch  for compatibility reasons */

      if (strcmp("-bonsai", *spp) == 0) {
         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         ep = lFirstRW(lp);

         show_sharetree(ep, nullptr);

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-sst" This is the documented switch of showing the share tree */

      if (strcmp("-sst", *spp) == 0) {
         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_STN_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);
 
         ep = lFirstRW(lp);

         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         } else {
          show_sharetree(ep, nullptr);
         }

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ss" */
      if (strcmp("-ss", *spp) == 0) {

         if (!show_object_list(ocs::gdi::Target::SGE_SH_LIST, SH_Type, SH_name,
               "submit host")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sss" - show scheduler state */

      if (strcmp("-sss", *spp) == 0) {
         /* ... */
         if (!show_object_list(ocs::gdi::Target::SGE_EV_LIST, EV_Type, EV_host, "scheduling host")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-su [list_name[,list_name,...]]" */

      if (strcmp("-su", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("acl`s to show", &lp, 
               US_Type, US_name, *spp);
         if (print_acl(lp) != 0) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

      // -scat id
      if (strcmp("-scat", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         u_long64 id = strtoull(*spp, nullptr, 10);
         if (!ocs::CategoryQconf::show(&answer_list, id)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

      /* "-sce attribute"  */
      if (strcmp("-sce", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         if (!centry_show(&answer_list, *spp)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mce centry"  */
      if (strcmp("-mce", *spp) == 0) {
         lList *answer_list = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);
         centry_modify(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Mce filename"  */
      if (strcmp("-Mce", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         centry_modify_from_file(&answer_list, file);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dce attribute "  */
      if (strcmp("-dce", *spp) == 0) {
         lList *answer_list = nullptr;
   
         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);
         centry_delete(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list); 
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-ace attribute"  */
      if (strcmp("-ace", *spp) == 0) {
         lList *answer_list = nullptr;

         qconf_is_adminhost(qualified_hostname);
         qconf_is_manager(username);

         spp = sge_parser_get_next(spp);
         centry_add(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

      /* "-Ace filename"  */
      if (strcmp("-Ace", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }

         if (!centry_add_from_file(&answer_list, file)) {
            sge_parse_return |= 1;
         }
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /*
       * Hostgroup parameters
       */

      /* "-shgrpl" */
      if (strcmp("-shgrpl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_HGRP_LIST, HGRP_Type, HGRP_name,
                          "host group list")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

      /* "-mhgrp user"  */
      if (strcmp("-mhgrp", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         hgroup_modify(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-Mhgrp user filename" */
      if (strcmp("-Mhgrp", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         hgroup_modify_from_file(&answer_list, file);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-ahgrp group"  */
      if (strcmp("-ahgrp", *spp) == 0) {
         lList *answer_list = nullptr;
         const char* group = "@template";
         bool is_validate_name = false; /* This boolean is needed to create a
                                           hgrp templete. One could have done
                                           it with a string compare deep in
                                           the code. I prefered this way. */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            group = *spp;
            is_validate_name = true;
         }

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         hgroup_add(&answer_list, group, is_validate_name);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-Ahgrp file"  */
      if (strcmp("-Ahgrp", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN); 
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         hgroup_add_from_file(&answer_list, file);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-dhgrp group "  */
      if (strcmp("-dhgrp", *spp) == 0) {
         lList *answer_list = nullptr;
   
         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);
         hgroup_delete(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-shgrp group"  */
      if (strcmp("-shgrp", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         hgroup_show(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* Are these two options still supported?  qconf doesn't recognise them */
      /* "-shgrp_tree" */
      if (strcmp("-shgrp_tree", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         hgroup_show_structure(&answer_list, *spp, true);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-shgrp_resolved" */
      if (strcmp("-shgrp_resolved", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         hgroup_show_structure(&answer_list, *spp, false);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /*
       * Cluster Queue parameter
       */

      /* "-sql" */
      if (strcmp("-sql", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_CQ_LIST, CQ_Type, CQ_name, "cqueue list")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

      /* "-mq cqueue"  */
      if (strcmp("-mq", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         cqueue_modify(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }
  
      /* "-Mq filename"  */ 
      if (strcmp("-Mq", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         cqueue_modify_from_file(&answer_list, file);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-aq cqueue"  */
      if (strcmp("-aq", *spp) == 0) {
         lList *answer_list = nullptr;
         const char *name = "template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         cqueue_add(&answer_list, name);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-Aq filename"  */ 
      if (strcmp("-Aq", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         
         if (!cqueue_add_from_file(&answer_list, file)) {
            sge_parse_return |= 1;
         }
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-dq cqueue"  */
      if (strcmp("-dq", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         cqueue_delete(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);
         
         spp++;
         continue;
      }

      /* "-sq [pattern[,pattern,...]]" */
      if (strcmp("-sq", *spp) == 0) {

         while (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            lString2List(*spp, &arglp, QR_Type, QR_name, ", ");
         }
         
         cqueue_show(&alp, arglp);
         lFreeList(&arglp);
         sge_parse_return |= show_answer(alp);
         lFreeList(&alp);
         spp++;
         continue;
      }

      /* "-cq destin_id[,destin_id,...]" */
      if (strcmp("-cq", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw (ep, lp) {
            lSetUlong(ep, ID_action, QI_DO_CLEAN);
         }
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CQ_LIST, ocs::gdi::Command::SGE_GDI_TRIGGER, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE,
                       &lp, nullptr, nullptr);
         if (answer_list_has_error(&alp)) {
            sge_parse_return = 1;
         }
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }

      if (strcmp("-sds", *spp) == 0) {
         lList *answer_list = nullptr;

         /* Use cqueue_list_sick()'s return value to set the exit code */
         sge_parse_return |= (cqueue_list_sick(&answer_list)?0:1);
         show_answer(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-suser username" */

      if (strcmp("-suser", *spp) == 0) {
         const char*  user = nullptr;
         lList* uList = nullptr;
         const lListElem* uep = nullptr;
         bool first = true;
         spooling_field *fields = nullptr;

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &uList, ST_Type, ST_name , ", ");
         for_each_ep(uep, uList) {
            user = lGetString(uep, ST_name);
            /* get user */
            where = lWhere("%T( %I==%s )", UU_Type, UU_name, user);
            what = lWhat("%T(ALL)", UU_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_UU_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);
                        
            if (first) {
               first = false;
            } else {
               printf("\n");
            }
            
            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               lFreeList(&lp);
               continue;
            }
            lFreeList(&alp);

            if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
               fprintf(stderr, MSG_USER_XISNOKNOWNUSER_S, user);
               fprintf(stderr, "\n");
               lFreeList(&lp);
               continue;
            }
            ep = lFirstRW(lp);
            
            /* print to stdout */
            fields = sge_build_UU_field_list(false);
            filename_stdout = spool_flatfile_write_object(&alp, ep, false, fields, &qconf_param_sfi,
                                                 SP_DEST_STDOUT, SP_FORM_ASCII, 
                                                 nullptr, false);
            lFreeList(&lp);
            lFreeList(&alp);
            sge_free(&filename_stdout);
            sge_free(&fields);
         }

         lFreeList(&uList);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-sprj projectname" */

      if (strcmp("-sprj", *spp) == 0) {
         spooling_field *fields = nullptr;

         spp = sge_parser_get_next(spp);

         /* get project */
         where = lWhere("%T( %I==%s )", PR_Type, PR_name, *spp);
         what = lWhat("%T(ALL)", PR_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_PR_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);
                  
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            sge_parse_return = 1; 
            continue;
         }

         lFreeList(&alp);
         if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
            fprintf(stderr, MSG_PROJECT_XISNOKNWOWNPROJECT_S, *spp);
            fprintf(stderr, "\n");
            lFreeList(&lp);
            spp++;
            sge_parse_return = 1; 
            continue;
         }
         ep = lFirstRW(lp);
         
         /* print to stdout */
         fields = sge_build_PR_field_list(false);
         filename_stdout = spool_flatfile_write_object(&alp, ep, false, fields, &qconf_sfi,
                                              SP_DEST_STDOUT, SP_FORM_ASCII, 
                                              nullptr, false);
         lFreeList(&alp);
         lFreeList(&lp);
         sge_free(&filename_stdout);
         sge_free(&fields);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-ss" */
      if (strcmp("-ss", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_SH_LIST, SH_Type, SH_name,
               "submit")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sul" */

      if (strcmp("-sul", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_US_LIST, US_Type, US_name,
               "userset list")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

       /* "-suserl" */

      if (strcmp("-suserl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_UU_LIST, UU_Type, UU_name,
               "user list")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

       /* "-sprjl" */

      if (strcmp("-sprjl", *spp) == 0) {
         if (!show_object_list(ocs::gdi::Target::SGE_PR_LIST, PR_Type, PR_name,
               "project list")) {
            sge_parse_return = 1; 
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-tsm" trigger scheduler monitoring */

      if (strcmp("-tsm", *spp) == 0) {
         /* no adminhost/manager check needed here */
         alp = ocs::gdi::Client::gdi_tsm();
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         
         spp++;
         continue;
      } 

/*----------------------------------------------------------------------------*/
      /* "-huh?" */

      ERROR(MSG_ANSWER_INVALIDOPTIONARGX_S, *spp);
      fprintf(stderr, MSG_SRC_X_HELP_USAGE_S , "qconf");
      fprintf(stderr, "\n");
      DRETURN(1);
   }

   lFreeList(&alp);
   DRETURN(sge_parse_return);
}

/***********************************************************************/

static void parse_name_list_to_cull(const char *name, lList **lpp, lDescr *dp, int nm, char *s)
{
   char *cp2 = nullptr;
   lListElem *ep = nullptr;
   int pos;
   int dataType;

   DENTER(TOP_LAYER);

   
   *lpp = lCreateList(name, dp);
   cp2 = sge_strtok(s, ",");
   ep = lCreateElem(dp);
 
   pos = lGetPosInDescr(dp, nm);
   dataType = lGetPosType(dp,pos);
   switch (dataType) {
      case lStringT:
         DPRINTF("parse_name_list_to_cull: Adding lStringT type element\n");
         lSetString(ep, nm, cp2);
         break;
      case lHostT:
         DPRINTF("parse_name_list_to_cull: Adding lHostT type element\n");
         lSetHost(ep, nm, cp2);
         break;
      default:
         DPRINTF("parse_name_list_to_cull: unexpected data type\n");
         break;
   }
   lAppendElem(*lpp, ep);

   while ((cp2 = sge_strtok(nullptr, ",")) != nullptr) {
      ep = lCreateElem(dp);
      switch (dataType) {
         case lStringT:
            DPRINTF("parse_name_list_to_cull: Adding lStringT type element\n");
            lSetString(ep, nm, cp2);
            break;
         case lHostT:
            DPRINTF("parse_name_list_to_cull: Adding lHostT type element\n");
            lSetHost(ep, nm, cp2);
            sge_resolve_host(ep, EH_name);
            break;
         default:
            DPRINTF("parse_name_list_to_cull: unexpected data type\n");
            break;
      }
      lAppendElem(*lpp, ep);
   }

   DRETURN_VOID;
}

/****************************************************************************/
static int sge_next_is_an_opt(char **pptr) 
{
   DENTER(TOP_LAYER);

   if (!*(pptr+1)) {
      DRETURN(1);
   }

   if (**(pptr+1) == '-') {
      DRETURN(1);
   }

   DRETURN(0);
}

/****************************************************************************/
static int sge_error_and_exit(const char *ptr) {
   DENTER(TOP_LAYER);

   fflush(stderr);
   fflush(stdout);

   if (ptr) {
      fprintf(stderr, "%s\n", ptr);
      fflush(stderr);
   }

   fflush(stderr);
   sge_exit(1);
   DRETURN(1); /* to prevent warning */
}

static bool add_host_of_type(lList *arglp, ocs::gdi::Target::TargetValue target)
{
   lListElem *argep=nullptr, *ep=nullptr;
   lList *lp=nullptr, *alp=nullptr;
   const char *host = nullptr;
   int nm = NoName;
   lDescr *type = nullptr;
   const char *name = nullptr;
   bool ret = true;

   DENTER(TOP_LAYER);

   switch (target) {
      case ocs::gdi::Target::SGE_SH_LIST:
         nm = SH_name;
         type = SH_Type;
         name = "submit host";
         break;
      case ocs::gdi::Target::SGE_AH_LIST:
         nm = AH_name;
         type = AH_Type;
         name = "administrative host";
         break;
      default:
         DPRINTF("add_host_of_type: unexpected type\n");
         ret = false;
         DRETURN(ret);
   }
   
   for_each_rw(argep, arglp) {
      /* resolve hostname */
      if (sge_resolve_host(argep, nm) != CL_RETVAL_OK) {
         const char* hostname = lGetHost(argep, nm);
         ret = false;
         if (hostname == nullptr) {
            hostname = "";
         }
         fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, hostname);
         fprintf(stderr, "\n");
         continue;
      }
      host = lGetHost(argep, nm);

      /* make a new host element */
      lp = lCreateList("host to add", type);
      ep = lCopyElem(argep);
      lAppendElem(lp, ep);


      /* add the new host to the host list */
      alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

      /* report results */
      ep = lFirstRW(alp);
      answer_exit_if_not_recoverable(ep);
      if (answer_get_status(ep) == STATUS_OK) {
         fprintf(stderr, MSG_QCONF_XADDEDTOYLIST_SS, host, name);
         fprintf(stderr, "\n");
      } else {
         fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      }

      lFreeList(&lp);
      lFreeList(&alp);
   }

   DRETURN(ret);
}

/* ------------------------------------------------------------ */

static bool del_host_of_type(lList *arglp, ocs::gdi::Target::TargetValue target )
{
   lListElem *argep=nullptr, *ep=nullptr;
   lList *lp=nullptr, *alp=nullptr;
   lDescr *type = nullptr;
   bool ret = true;

   DENTER(TOP_LAYER);

   switch (target) {
   case ocs::gdi::Target::SGE_SH_LIST:
      type = SH_Type;
      break;
   case ocs::gdi::Target::SGE_AH_LIST:
      type = AH_Type;
      break;
   case ocs::gdi::Target::SGE_EH_LIST:
      type = EH_Type;
      break;
   default: ;
   }

   for_each_rw (argep, arglp) {

      /* make a new host element */
      lp = lCreateList("host_to_del", type);
      ep = lCopyElem(argep);
      lAppendElem(lp, ep);

      /* delete element */
      alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

      /* print results */
      if (answer_list_has_error(&alp)) {
         ret = false;
      }
      answer_list_on_error_print_or_exit(&alp, stderr);

      lFreeList(&alp);
      lFreeList(&lp);
   }

   DRETURN(ret);
}

/* ------------------------------------------------------------ */

static lListElem *edit_exechost(lListElem *ep, uid_t uid, gid_t gid)
{
   int status;
   lListElem *hep = nullptr;
   spooling_field *fields = sge_build_EH_field_list(false, false, false);
   lList *alp = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   /* used for generating filenames */
   char *filename = nullptr;

   DENTER(TOP_LAYER);

   filename = (char *)spool_flatfile_write_object(&alp, ep, false, fields,
                                                  &qconf_sfi, SP_DEST_TMP,
                                                  SP_FORM_ASCII, filename,
                                                  false);
   if (answer_list_output(&alp)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      sge_free(&fields);
      sge_error_and_exit(nullptr);
   }

   lFreeList(&alp);
   status = sge_edit(filename, uid, gid);

   if (status < 0) {
      unlink(filename);
      sge_free(&filename);
      sge_free(&fields);
      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      unlink(filename);
      sge_free(&filename);
      sge_free(&fields);
      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }
   
   fields_out[0] = NoName;
   hep = spool_flatfile_read_object(&alp, EH_Type, nullptr,
                                   fields, fields_out, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, filename);
            
   if (answer_list_output(&alp)) {
      lFreeElem(&hep);
   }

   if (hep != nullptr) {
      missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
   }

   if (missing_field != NoName) {
      lFreeElem(&hep);
      answer_list_output(&alp);
   }
   
   unlink(filename);
   sge_free(&filename);
   sge_free(&fields);
   DRETURN(hep);
}

/* ------------------------------------------------------------ */

static lList* edit_sched_conf(lList *confl, uid_t uid, gid_t gid)
{
   int status;
   char *fname = nullptr;
   lList *alp=nullptr, *newconfl=nullptr;
   lListElem *ep = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER);

   fname = (char *)spool_flatfile_write_object(&alp, lFirst(confl), false,
                                       SC_fields, &qconf_comma_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, 
                                       fname, false);
   if (answer_list_output(&alp)) {
      fprintf(stderr, "%s\n", MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION);
      sge_free(&fname);
      sge_exit(1);
   }

   status = sge_edit(fname, uid, gid);

   if (status < 0) {
      unlink(fname);
      sge_free(&fname);
      
      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      unlink(fname);
      sge_free(&fname);
      
      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }
   
   fields_out[0] = NoName;
   ep = spool_flatfile_read_object(&alp, SC_Type, nullptr,
                                   SC_fields, fields_out, true, &qconf_comma_sfi,
                                   SP_FORM_ASCII, nullptr, fname);
            
   if (answer_list_output(&alp)) {
      lFreeElem(&ep);
   }

   if (ep != nullptr) {
      missing_field = spool_get_unprocessed_field(SC_fields, fields_out, &alp);
   }

   if (missing_field != NoName) {
      lFreeElem(&ep);
      answer_list_output(&alp);
   }

   if (ep != nullptr) {
      newconfl = lCreateList("scheduler config", SC_Type);
      lAppendElem(newconfl, ep);
   }
   
   if ((newconfl != nullptr) && !sconf_validate_config(&alp, newconfl)) {
      lFreeList(&newconfl);
      answer_list_output(&alp);
   }
   
   if (newconfl == nullptr) {
      fprintf(stderr, MSG_QCONF_CANTREADCONFIG_S, "can't parse config");
      fprintf(stderr, "\n");
      unlink(fname);
      sge_free(&fname);
      sge_exit(1);
   }
   lFreeList(&alp);
   
   unlink(fname);
   sge_free(&fname);

   DRETURN(newconfl);
}

/* ------------------------------------------------------------ */

static lListElem *edit_user(lListElem *ep, uid_t uid, gid_t gid)
{
   int status;
   lListElem *newep = nullptr;
   lList *alp = nullptr;
   char *filename = nullptr;
   spooling_field *fields = sge_build_UU_field_list(false);
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER);

   filename = (char *)spool_flatfile_write_object(&alp, ep, false, fields,
                                                  &qconf_sfi, SP_DEST_TMP,
                                                  SP_FORM_ASCII, nullptr, false);
   if (answer_list_output(&alp)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      sge_free(&fields);
      sge_error_and_exit(nullptr);
   }

   lFreeList(&alp);

   status = sge_edit(filename, uid, gid);

   if (status < 0) {
      sge_free(&fields);
      unlink(filename);
      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      sge_free(&fields);
      unlink(filename);
      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }

   fields_out[0] = NoName;
   newep = spool_flatfile_read_object(&alp, UU_Type, nullptr, fields, fields_out,
                                    true, &qconf_sfi, SP_FORM_ASCII, nullptr,
                                    filename);
   
   if (answer_list_output(&alp)) {
      lFreeElem(&newep);
   }

   if (newep != nullptr) {
      missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
   }

   sge_free(&fields);
   
   if (missing_field != NoName) {
      lFreeElem(&newep);
      answer_list_output(&alp);
   }
   
   unlink(filename);
   sge_free(&filename);
   
   if (!newep) {
      fprintf(stderr, MSG_QCONF_CANTREADX_S, MSG_OBJ_USER);
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   
   DRETURN(newep);
}

/* ------------------------------------------------------------ */
static lListElem *edit_project(lListElem *ep, uid_t uid, gid_t gid)
{
   int status;
   lListElem *newep = nullptr;
   lList *alp = nullptr;
   char *filename = nullptr;
   spooling_field *fields = sge_build_PR_field_list(false);
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER);

   filename = (char *)spool_flatfile_write_object(&alp, ep, false, fields,
                                                  &qconf_sfi, SP_DEST_TMP,
                                                  SP_FORM_ASCII, nullptr, false);
   if (answer_list_output(&alp)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      sge_free(&fields);
      sge_error_and_exit(nullptr);
   }

   lFreeList(&alp);

   status = sge_edit(filename, uid, gid);

   if (status < 0) {
      sge_free(&fields);
      unlink(filename);
      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      sge_free(&fields);
      unlink(filename);
      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }

   fields_out[0] = NoName;
   newep = spool_flatfile_read_object(&alp, PR_Type, nullptr, fields, fields_out,
                                    true, &qconf_sfi, SP_FORM_ASCII, nullptr,
                                    filename);
   
   if (answer_list_output(&alp)) {
      lFreeElem(&newep);
   }

   if (newep != nullptr) {
      missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
   }

   sge_free(&fields);
   
   if (missing_field != NoName) {
      lFreeElem(&newep);
      answer_list_output(&alp);
   }
   
   unlink(filename);
   sge_free(&filename);
   
   if (!newep) {
      fprintf(stderr, MSG_QCONF_CANTREADX_S, MSG_JOB_PROJECT);
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   
   DRETURN(newep);
}

/****************************************************************/
static lListElem *edit_sharetree(lListElem *ep, uid_t uid, gid_t gid)
{
   int status;
   lListElem *newep = nullptr;
   const char *filename = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   bool is_missing = false;

   DENTER(TOP_LAYER);

   if (ep == nullptr) {
      is_missing = true;
   }
   if (is_missing) {
      ep = getSNTemplate();
   }

   fields = sge_build_STN_field_list(false, true);
   
   filename = spool_flatfile_write_object(&alp, ep, false,
                                          fields, &qconf_name_value_list_sfi,
                                          SP_DEST_TMP, SP_FORM_ASCII,
                                          nullptr, false);
   if (is_missing) {
      lFreeElem(&ep);
   }

   if (answer_list_output(&alp)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      sge_free(&fields);
      sge_error_and_exit(nullptr);
   }

   lFreeList(&alp);

   status = sge_edit(filename, uid, gid);

   if (status < 0) {
      sge_free(&fields);
      unlink(filename);
      sge_free(&filename);
      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      sge_free(&fields);
      unlink(filename);
      sge_free(&filename);
      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }
   
   fields_out[0] = NoName;
   newep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                      fields, fields_out,  true,
                                      &qconf_name_value_list_sfi,
                                      SP_FORM_ASCII, nullptr, filename);
   
   if (answer_list_output(&alp)) {
      lFreeElem(&newep);
   }

   if (newep != nullptr) {
      missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
   }

   sge_free(&fields);
   
   if (missing_field != NoName) {
      lFreeElem(&newep);
      answer_list_output(&alp);
   }

   unlink(filename);
   sge_free(&filename);

   if (newep == nullptr) {
      /* JG: TODO: do we need the following output?
       * Isn't an error message already written by spool_flatfile_read_object?
       */
      fprintf(stderr, MSG_QCONF_CANTREADSHARETREEX_S, "");
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   
   DRETURN(newep);
}

/* ------------------------------------------------------------ */

static bool show_object_list(ocs::gdi::Target::TargetValue target, lDescr *type, int keynm, const char *name)
{
   lEnumeration *what = nullptr;
   lCondition *where = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   lListElem *ep = nullptr;
   int pos;
   int dataType;
   bool ret = true;
   
   DENTER(TOP_LAYER);

   what = lWhat("%T(%I)", type, keynm);

   switch (keynm) {
   case EH_name:
      where = lWhere("%T(!(%Ic=%s || %Ic=%s))", type, keynm, SGE_TEMPLATE_NAME, keynm, SGE_GLOBAL_NAME );
      break;
   case CONF_name:
      where = lWhere("%T(!(%I c= %s))", type, keynm, SGE_GLOBAL_NAME );
      break;   
   case EV_host:
      where = lWhere("%T(%I==%u))", type, EV_id, EV_ID_SCHEDD);
      break;
   default:
      where = nullptr; /* all elements */
      break;
   } 

   alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   lPSortList(lp, "%I+", keynm);

   ep = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_list_output(&alp)) {
      lFreeList(&lp);
      DRETURN(false);
   }

   if (lGetNumberOfElem(lp) > 0) {
      for_each_rw (ep, lp) {
         const char *line = nullptr;
         pos = lGetPosInDescr(type, keynm);
         dataType = lGetPosType(type , pos);
         switch(dataType) {
            case lStringT: 
               line = lGetString(ep, keynm);
               if (line && line[0] != COMMENT_CHAR) { 
                  printf("%s\n", lGetString(ep, keynm));
               }
               break;
            case lHostT:
                line = lGetHost(ep, keynm);
               if (line && line[0] != COMMENT_CHAR) { 
                  printf("%s\n", lGetHost(ep, keynm));
               }
               break;
            default:
               DPRINTF("show_object_list: unexpected data type\n");
         }
      }
   } else {
      fprintf(stderr, MSG_QCONF_NOXDEFINED_S, name);
      fprintf(stderr, "\n");
      ret = false;
   }
   
   lFreeList(&alp);
   lFreeList(&lp);
   
   DRETURN(ret);
}

static int
show_thread_list() {
   DENTER(TOP_LAYER);

   // send the request
   lList *lp = nullptr;
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_DUMMY_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

   // check the answer
   const lListElem *ep = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      fprintf(stderr, "\n");
      DRETURN(-1);
   }

   if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
      printf("%-15s %s\n", MSG_TABLE_EV_POOL, MSG_TABLE_SIZE);
      printf("--------------------\n");
      for_each_ep(ep, lp) {
         printf("%-15s " sge_uu32 "\n", lGetString(ep, ST_name), lGetUlong(ep, ST_id));
      }
   }
   lFreeList(&alp);
   lFreeList(&lp);


   DRETURN(0);
}

static int show_eventclients()
{
   lEnumeration *what = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   const lListElem *ep = nullptr;

   DENTER(TOP_LAYER);

   what = lWhat("%T(%I %I %I)", EV_Type, EV_id, EV_name, EV_host);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EV_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, what);
   lFreeWhat(&what);

   ep = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      fprintf(stderr, "\n");
      DRETURN(-1);
   }

   if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
      lPSortList(lp, "%I+", EV_id);
   
      printf("%8s %-15s %-25s\n",MSG_TABLE_EV_ID, MSG_TABLE_EV_NAME, MSG_TABLE_HOST);
      printf("--------------------------------------------------\n");
      for_each_ep(ep, lp) {
         printf("%8d ", (int)lGetUlong(ep, EV_id));
         printf("%-15s ", lGetString(ep, EV_name));
         printf("%-25s\n", (lGetHost(ep, EV_host) != nullptr) ? lGetHost(ep, EV_host) : "-");
      }
   }
   else {
      fprintf(stderr, "%s\n", MSG_QCONF_NOEVENTCLIENTSREGISTERED);
   }
   lFreeList(&alp);
   lFreeList(&lp);
   
   DRETURN(0);
}



static int show_processors(bool has_binding_param)
{
   lEnumeration *what = nullptr;
   lCondition *where = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   const lListElem *ep = nullptr;
   const char *cp = nullptr;
   u_long32 sum = 0;
   u_long32 socket_sum = 0;
   u_long32 core_sum = 0;

   DENTER(TOP_LAYER);

   what = lWhat("%T(%I %I %I)", EH_Type, EH_name, EH_processors, EH_load_list);
   where = lWhere("%T(!(%Ic=%s || %Ic=%s))", EH_Type, EH_name, 
                  SGE_TEMPLATE_NAME, EH_name, SGE_GLOBAL_NAME);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   ep = lFirst(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      DRETURN(-1);
   }

   if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
      lPSortList(lp,"%I+", EH_name);

      if (has_binding_param) {
         printf("%-25.24s%10.9s%6.5s%6.5s%12.11s\n", MSG_TABLE_HOST, MSG_TABLE_PROCESSORS, 
            MSG_TABLE_SOCKETS, MSG_TABLE_CORES, MSG_TABLE_ARCH);
      } else {
         printf("%-25.24s%10.9s%12.11s\n", MSG_TABLE_HOST, MSG_TABLE_PROCESSORS, MSG_TABLE_ARCH);
      }
      printf("===============================================");
      if (has_binding_param) {
         printf("============");
      }
      printf("\n");
      for_each_ep(ep, lp) {
         const lListElem *arch_elem = lGetSubStr(ep, HL_name, "arch", EH_load_list);
         u_long32 sockets = 0;
         u_long32 cores = 0;

         printf("%-25.24s", ((cp = lGetHost(ep, EH_name)) ? cp : ""));
         printf("%10" sge_fu32, lGetUlong(ep, EH_processors));

         if (has_binding_param) {
            const lListElem *socket_elem = lGetSubStr(ep, HL_name, "m_socket", EH_load_list);
            const lListElem *core_elem = lGetSubStr(ep, HL_name, "m_core", EH_load_list);

            if (socket_elem != nullptr) {
               printf("%6.5s", lGetString(socket_elem, HL_value));
               sockets = atol(lGetString(socket_elem, HL_value));
            } else {
               printf("%6.5s", "-");
            }
            if (core_elem != nullptr) {
               printf("%6.5s", lGetString(core_elem, HL_value));
               cores = atol(lGetString(core_elem, HL_value));
            } else {
               printf("%6.5s", "-");
            }
         }
         if (arch_elem) {
            printf("%12.11s", lGetString(arch_elem, HL_value));
         }
         printf("\n");
         sum += lGetUlong(ep, EH_processors);
         socket_sum += sockets;
         core_sum += cores;
      }
      printf("===============================================");
      if (has_binding_param) {
         printf("============");
      }
      printf("\n");
        
      printf("%-25.24s%10" sge_fu32, MSG_TABLE_SUM_F, sum);
      if (has_binding_param) { 
         if (socket_sum > 0) {
            printf("%6" sge_fu32, socket_sum);
         } else {
            printf("%6.5s", "-");
         }
         if (core_sum > 0) {
            printf("%6" sge_fu32, core_sum);
         } else {
            printf("%6.5s", "-");
         }
      }
      printf("\n");
   } else {
      fprintf(stderr,  "%s\n", MSG_QCONF_NOEXECUTIONHOSTSDEFINED);
   }
   
   lFreeList(&alp);
   lFreeList(&lp);
   
   DRETURN(0);
}

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   get all acls listed in acl_arg 
   from qmaster and print them

   returns
      0 on success
      1 if arglp contains acl names not present at qmaster
      -1 failed to get any acl
*/
static int print_acl(lList *arglp) {
   lList *acls = nullptr;
   const lListElem *argep = nullptr, *ep = nullptr;
   int fail = 0;
   const char *acl_name = nullptr;
   int first_time = 1;
   const char *filename_stdout;

   DENTER(TOP_LAYER);

   /* get all acls named in arglp, put them into acls */
   if (sge_client_get_acls(nullptr, arglp, &acls)) {
      DRETURN(-1);
   }

   for_each_ep(argep, arglp) {
      acl_name = lGetString(argep, US_name);

      ep = lGetElemStrRW(acls, US_name, acl_name);
      if (ep == nullptr) {
         fprintf(stderr, MSG_SGETEXT_DOESNOTEXIST_SS, "access list", acl_name);
         fprintf(stderr, "\n");
         fail = 1;
      } else {
         lList *alp = nullptr;
         
         if (first_time)
            first_time = 0;
         else {
            printf("\n");
         }
         
         filename_stdout = spool_flatfile_write_object(&alp, ep, false, US_fields, &qconf_param_sfi,
                                     SP_DEST_STDOUT, SP_FORM_ASCII, nullptr,
                                     false);
         lFreeList(&alp);
         sge_free(&filename_stdout);
      }
   }
   lFreeList(&acls);
   DRETURN(fail);
}

/**************************************************************
   get all usersets listed in arglp 
   from qmaster and edit them

   This is pure client code, so output to stderr/out is allowed

   returns
      0 on success
      1 if arglp contains userset names not present at qmaster
      -1 failed to get any userset
      -2 if failed due to disk error
 **************************************************************/
static int edit_usersets(lList *arglp) {
   lList *usersets = nullptr;
   const lListElem *argep=nullptr;
   lListElem *ep=nullptr;
   const lListElem *aep=nullptr;
   lListElem *changed_ep=nullptr;
   int status;
   const char *userset_name = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   char *fname = nullptr;
   ocs::gdi::Command::Cmd cmd;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   DENTER(TOP_LAYER);

   /* get all usersets named in arglp, put them into usersets */
   if (sge_client_get_acls(nullptr, arglp, &usersets)) {
      DRETURN(-1);
   }

   for_each_ep(argep, arglp) {
      alp = nullptr;
      userset_name = lGetString(argep, US_name);

      ep = lGetElemStrRW(usersets, US_name, userset_name);
      if (ep == nullptr) {
         ep = lAddElemStr(&usersets, US_name, userset_name, US_Type);
         /* initialize type field in case of sge */
         lSetUlong(ep, US_type, US_ACL|US_DEPT);
         cmd = ocs::gdi::Command::SGE_GDI_ADD;
      } else {
         cmd = ocs::gdi::Command::SGE_GDI_MOD;
      }

      fname = (char *)spool_flatfile_write_object(&alp, ep, false, US_fields,
                                           &qconf_param_sfi, SP_DEST_TMP,
                                           SP_FORM_ASCII, fname, false);
      if (answer_list_output(&alp)) {
         fprintf(stderr, "%s\n", MSG_FILE_ERRORWRITINGUSERSETTOFILE);
         DRETURN(-2);
      }
      
      status = sge_edit(fname, uid, gid);
      
      if (status < 0) {
         unlink(fname);
         fprintf(stderr, "%s\n", MSG_PARSE_EDITFAILED);
         lFreeList(&usersets);
         DRETURN(-2);  /* why should the next edit have more luck */
      }

      if (status > 0) {
         unlink(fname);
         fprintf(stdout, "%s\n", MSG_FILE_FILEUNCHANGED);
         continue;
      }

      fields_out[0] = NoName;
      changed_ep = spool_flatfile_read_object(&alp, US_Type, nullptr,
                                      US_fields, fields_out,  true, &qconf_param_sfi,
                                      SP_FORM_ASCII, nullptr, fname);
      
      if (answer_list_output(&alp)) {
         lFreeElem(&changed_ep);
      }

      if (changed_ep != nullptr) {
         missing_field = spool_get_unprocessed_field(US_fields, fields_out, &alp);
      }

      if (missing_field != NoName) {
         lFreeElem(&changed_ep);
         answer_list_output(&alp);
      }

      if (changed_ep == nullptr) {
         fprintf(stderr, MSG_FILE_ERRORREADINGUSERSETFROMFILE_S, fname);
         fprintf(stderr, "\n");
         continue;   /* May be the user made a mistake. Just proceed with 
                        the next */
      }

      /* Create List; append Element; and do a modification gdi call */
      lp = lCreateList("userset list", US_Type);
      lAppendElem(lp, changed_ep);
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_US_LIST, cmd, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
      lFreeList(&lp);

      for_each_ep(aep, alp) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
   }

   lFreeList(&usersets);
   sge_free(&fname);
   DRETURN(0);
}

/***************************************************************************
  -sconf option 
 ***************************************************************************/
static int print_config(const char *config_name) {
   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   const lListElem *ep = nullptr;
   int fail=0;
   const char *cfn = nullptr;
   spooling_field *fields = nullptr;
   
   DENTER(TOP_LAYER);

   /* get config */
   if (!strcasecmp(config_name, "global")) {
      cfn = SGE_GLOBAL_NAME;
   } else {
      cfn = config_name;   
   }

   where = lWhere("%T(%Ih=%s)", CONF_Type, CONF_name, cfn);
   what = lWhat("%T(ALL)", CONF_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CONF_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   ep = lFirst(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      fail = 1;
   } else {
      const char *filename_stdout;

      if (!(ep = lFirst(lp))) {
         fprintf(stderr, MSG_ANSWER_CONFIGXNOTDEFINED_S, cfn);
         fprintf(stderr, "\n");
         lFreeList(&alp);
         lFreeList(&lp);
         DRETURN(1);
      }
      printf("#%s:\n", cfn);
      
      fields = sge_build_CONF_field_list(false);
      filename_stdout = spool_flatfile_write_object(&alp, ep, false, fields, &qconf_sfi,
                                  SP_DEST_STDOUT, SP_FORM_ASCII, nullptr, false);
      sge_free(&fields);
      sge_free(&filename_stdout);
      
      if (answer_list_output(&alp)) {
         sge_error_and_exit(nullptr);
      }
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(fail);
}

/*------------------------------------------------------------------------*
 * delete_config
 *------------------------------------------------------------------------*/
static int delete_config(const char *config_name) {
   lList *alp = nullptr, *lp = nullptr;
   const lListElem *ep = nullptr;
   int fail = 0;
   
   DENTER(TOP_LAYER);

   lAddElemHost(&lp, CONF_name, config_name, CONF_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CONF_LIST, ocs::gdi::Command::SGE_GDI_DEL, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);

   ep = lFirst(alp);
   fprintf(stderr, "%s\n", lGetString(ep, AN_text));

   answer_exit_if_not_recoverable(ep);
   fail = !(answer_get_status(ep) == STATUS_OK);

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(fail);
}

/*------------------------------------------------------------------------*
 * add_modify_config
 ** flags = 1 = add, 2 = modify, 3 = modify if exists, add if not
 *------------------------------------------------------------------------*/
static int add_modify_config(const char *cfn, const char *filename, u_long32 flags) {
   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   lListElem *ep = nullptr;
   int failed=0;
   char *tmpname = nullptr;
   int status;
   spooling_field *fields = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   
   DENTER(TOP_LAYER);

   where = lWhere("%T(%Ih=%s)", CONF_Type, CONF_name, cfn);
   what = lWhat("%T(ALL)", CONF_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CONF_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   failed = false;
   ep = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      lFreeList(&alp);
      lFreeList(&lp);
      DRETURN(1);
   }

   lFreeList(&alp);

   ep = lCopyElem(lFirst(lp));
   lFreeList(&lp);

   if (ep && (flags == 1)) {
      fprintf(stderr, MSG_ANSWER_CONFIGXALREADYEXISTS_S, cfn);
      fprintf(stderr, "\n");
      lFreeElem(&ep);
      DRETURN(2);
   }
   if (!ep && (flags == 2)) {
      fprintf(stderr, MSG_ANSWER_CONFIGXDOESNOTEXIST_S, cfn);
      fprintf(stderr, "\n");
      lFreeElem(&ep);
      DRETURN(3);
   }

   if (filename == nullptr) {
      bool local_failed = false;

      /* get config or make an empty config entry if none exists */
      if (ep == nullptr) {
         ep = lCreateElem(CONF_Type);
         lSetHost(ep, CONF_name, cfn);
      }

      fields = sge_build_CONF_field_list(false);
      tmpname = (char *)spool_flatfile_write_object(&alp, ep, false, fields,
                                            &qconf_sfi, SP_DEST_TMP, SP_FORM_ASCII, 
                                            tmpname, false);

      lFreeElem(&ep);
      status = sge_edit(tmpname, uid, gid);

      if (status != 0) {
         unlink(tmpname);
         local_failed = true;
         sge_free(&fields);
         sge_free(&tmpname);
      }
      if (status < 0) {
         fprintf(stderr, "%s\n", MSG_PARSE_EDITFAILED);
         sge_free(&fields);
         DRETURN(local_failed);
      }
      else if (status > 0) {
         fprintf(stderr, "%s\n", MSG_ANSWER_CONFIGUNCHANGED);
         sge_free(&fields);
         DRETURN(local_failed);
      }

      fields_out[0] = NoName;
      ep = spool_flatfile_read_object(&alp, CONF_Type, nullptr,
                                      fields, fields_out, false, &qconf_sfi,
                                      SP_FORM_ASCII, nullptr, tmpname);

      if (answer_list_output(&alp)) {
         lFreeElem(&ep);
         local_failed = true;
      }

      if (ep != nullptr) {
         missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
      }

      sge_free(&fields);

      if (missing_field != NoName) {
         lFreeElem(&ep);
         answer_list_output(&alp);
         local_failed = true;
      }

      /* If the configuration is legitematly nullptr, create an empty object. */
      if (!local_failed && (ep == nullptr)) {
         ep = lCreateElem(CONF_Type);
      }

      if (ep != nullptr) {
         lSetHost(ep, CONF_name, cfn);
      } else {
         fprintf(stderr, "%s\n", MSG_ANSWER_ERRORREADINGTEMPFILE);
         unlink(tmpname);
         sge_free(&tmpname);
         local_failed = true;
         DRETURN(local_failed);
      }
      unlink(tmpname);
      sge_free(&tmpname);
   } else {
      lFreeElem(&ep);

      fields_out[0] = NoName;
      fields = sge_build_CONF_field_list(false);
      ep = spool_flatfile_read_object(&alp, CONF_Type, nullptr,
                                      fields, fields_out, false, &qconf_sfi,
                                      SP_FORM_ASCII, nullptr, filename);

      if (answer_list_output(&alp)) {
         lFreeElem(&ep);
      }

      if (ep != nullptr) {
         missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
      }

      sge_free(&fields);

      if (missing_field != NoName) {
         lFreeElem(&ep);
         answer_list_output(&alp);
      }

      if (ep != nullptr) {
         lSetHost(ep, CONF_name, cfn);
      }

      if (!ep) {
         fprintf(stderr, MSG_ANSWER_ERRORREADINGCONFIGFROMFILEX_S, filename);
         fprintf(stderr, "\n");
         failed = true;
         DRETURN(failed);
      }
   }

   lp = lCreateList("modified configuration", CONF_Type); 
   lAppendElem(lp, ep);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_CONF_LIST, ocs::gdi::Command::SGE_GDI_MOD, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &lp, nullptr, nullptr);
   lFreeList(&lp);

   /* report results */
   failed = show_answer_list(alp);

   lFreeList(&alp);

   DRETURN(failed);
}

static int
qconf_is_manager(const char *user) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (user == nullptr) {
      DRETURN(-1);
   }

   // ask master if current user is manager
   lList *alp = nullptr;
   bool is_manager = false;
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(&alp, &is_manager, nullptr, nullptr, nullptr);
   if (!perm_return) {
      DTRACE;
      answer_list_output(&alp);
      sge_exit(1);
   }
   lFreeList(&alp);

   // if user is no manager then exit
   if (!is_manager) {
      fprintf(stderr, MSG_SGETEXT_MUSTBEMANAGER_S , user);
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(0);
}

static int
qconf_is_adminhost(const char *host) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (host == nullptr) {
      DRETURN(-1);
   }

   // ask master if current host is admin host
   lList *alp = nullptr;
   bool is_admin_host = false;
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(&alp, nullptr, nullptr, &is_admin_host, nullptr);
   if (!perm_return) {
      DTRACE;
      answer_list_output(&alp);
      sge_exit(1);
   }
   lFreeList(&alp);

   // if host has no permission then exit
   if (!is_admin_host) {
      fprintf(stderr,MSG_SGETEXT_NOADMINHOST_S, host);
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(0);
}

static int
qconf_is_manager_on_admin_host(const char *user, const char *host) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (host == nullptr) {
      DRETURN(-1);
   }

   // ask master if current host is admin host
   lList *alp = nullptr;
   bool is_admin_host = false;
   bool is_manager = false;
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(&alp, &is_manager, nullptr, &is_admin_host, nullptr);
   if (!perm_return) {
      DTRACE;
      answer_list_output(&alp);
      sge_exit(1);
   }
   lFreeList(&alp);

   // if user is no manager then exit
   if (!is_manager) {
      fprintf(stderr, MSG_SGETEXT_MUSTBEMANAGER_S , user);
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   // if host has no permission then exit
   if (!is_admin_host) {
      fprintf(stderr, MSG_SGETEXT_NOADMINHOST_S, host);
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(0);
}

/****** src/qconf_modify_attribute() ******************************************
*  NAME
*     qconf_modify_attribute() -- sends a modify request to the master 
*
*  SYNOPSIS
*
*     static int qconf_modify_attribute(lList **alpp, int from_file,
*                                        char ***spp, int sub_command,
*                                        struct object_info_entry *info_entry); 
*
*
*  FUNCTION
*     The function performs a SGE_GDI_MOD request to the qmaster.
*     It will get all necessary infomation from commandline or
*     file. 
*     Depending on the parameters specified, the function will
*     modify only parts of an object. It is possible to address
*     only parts of a sublist of an object.
*     
*
*  INPUTS
*     alpp        - reference to an answer list where the master will
*                 store necessary messages for the user 
*
*     from_file   - if set to 1 then the next commandline parameter 
*                   (stored in spp) will contain a filename. 
*                   This file contains the 
*                   attributes which should be modified 
*
*     spp         - pending list of commandline parameter
*
*     epp         - this reference will contain the reduced
*                   element which will be parsed from commandline or file
*                   after this function was called
*
*     sub_command - bitmask which will be added to the "command"
*                   parameter of the ocs::gdi::Client::sge_gdi-request:
*        SGE_GDI_CHANGE - modify sublist elements
*        SGE_GDI_APPEND - add elements to a sublist
*        SGE_GDI_REMOVE - remove sublist elements
*        SGE_GDI_SET - replace the complete sublist
*
*     info_entry -  pointer to a structure with function 
*                   pointers, string pointers, and CULL names
*
*  RESULT
*     [alpp] Masters answer for the gdi request
*     1 for error
*     0 for success
******************************************************************************/
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp,
                                  lListElem **epp, ocs::gdi::SubCommand::SubCmd sub_command,
                                  struct object_info_entry *info_entry) 
{
   int fields[150];
   lListElem *add_qp = nullptr;
   lList *qlp = nullptr;
   lEnumeration *what = nullptr;
   
   DENTER(TOP_LAYER);    

   DTRACE;

   fields[0] = NoName;

   if (from_file) {
      if (sge_next_is_an_opt(*spp))  {
          snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ANSWER_MISSINGFILENAMEASOPTIONARG_S, "qconf");
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(1);
      }                
      DTRACE;
      *epp = spool_flatfile_read_object(alpp, info_entry->cull_descriptor,
                                        nullptr, info_entry->fields, fields,
                                        true, info_entry->instr, SP_FORM_ASCII,
                                        nullptr, **spp);
            
      if (answer_list_output(alpp)) {
         DRETURN(1);
      }
      if (*epp == nullptr){
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_FILE_ERRORREADINGINFILE);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(1);
      }
      DTRACE;
   } else {
      const char *name = nullptr;
      const char *value = nullptr;
      const char *filename = nullptr;
      DSTRING_STATIC(delim, 100);

      /* attribute name to be [admr] for info_entry obj */
      name = (const char *)strdup(**spp);
      *spp = sge_parser_get_next(*spp);
      /* attribute value to be [admr] for info_entry obj */
      value = (const char *)strdup(**spp);

      if (!strcmp(info_entry->object_name, SGE_OBJ_RQS) && !strcmp(name, "limit")) {
         sge_dstring_append(&delim, " to ");
      } else {
         sge_dstring_append_char(&delim, info_entry->instr->name_value_delimiter);
      }
      {
         dstring write_attr_tmp_file_error = DSTRING_INIT;
         filename = write_attr_tmp_file(name, value, sge_dstring_get_string(&delim), &write_attr_tmp_file_error);
         if (filename == nullptr && sge_dstring_get_string(&write_attr_tmp_file_error) != nullptr) {
            answer_list_add_sprintf(alpp, STATUS_EDISK, ANSWER_QUALITY_ERROR, sge_dstring_get_string(&write_attr_tmp_file_error));
         } else {
            *epp = spool_flatfile_read_object(alpp, info_entry->cull_descriptor, nullptr,
                                      info_entry->fields, fields, true, info_entry->instr,
                                      SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
         }
         sge_dstring_free(&write_attr_tmp_file_error);
      }

      /* Bugfix: Issuezilla #1005
       * Since we're writing the information from the command line to a file so
       * we can read it, the error messages that come back from the flatfile
       * spooling will sound a little odd.  To avoid this, we hijack the answer
       * list and replace the error messages with ones that will make better
       * sense. */
      if (answer_list_has_error(alpp)) {
         lListElem *aep = nullptr;
         
         for_each_rw(aep, *alpp) {
            if (answer_has_quality(aep, ANSWER_QUALITY_ERROR) &&
                (answer_get_status(aep) == STATUS_ESYNTAX)) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_BAD_ATTR_ARGS_SS, name, value);
               lSetString(aep, AN_text, SGE_EVENT);
            }
         }
         
         sge_free(&name);
         sge_free(&value);
         sge_dstring_free(&delim);
         
         DRETURN(1);
      }
      
      sge_free(&name);
      sge_free(&value);
      sge_dstring_free(&delim);
   }

   /* add object name to int vector and transform
      it into an lEnumeration */
   if (add_nm_to_set(fields, info_entry->nm_name) < 0) {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_CANTCHANGEOBJECTNAME_SS, "qconf", info_entry->attribute_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      lFreeElem(epp);
      DRETURN(1);
   }

   if (!(what = lIntVector2What(info_entry->cull_descriptor, fields))) {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_INTERNALFAILURE_S, "qconf");
      lFreeElem(epp);
      DRETURN(1);
   }     

   while (!sge_next_is_an_opt(*spp)) { 
      *spp = sge_parser_get_next(*spp);
      if (!qlp)
         qlp = lCreateList("list", info_entry->cull_descriptor);
      add_qp = lCopyElem(*epp);
      switch(lGetType(add_qp->descr, info_entry->nm_name)) {
         case lUlongT:
            lSetUlong(add_qp, info_entry->nm_name, atol(**spp));
            break;
         case lLongT:
            lSetLong(add_qp, info_entry->nm_name, atol(**spp));
            break;
         case lIntT:
            lSetInt(add_qp, info_entry->nm_name, atoi(**spp));
            break;
         case lFloatT:
            lSetDouble(add_qp, info_entry->nm_name, atof(**spp));
            break;
         case lDoubleT:
            lSetDouble(add_qp, info_entry->nm_name, atof(**spp));
            break;
         case lCharT:
            lSetChar(add_qp, info_entry->nm_name, **spp[0]);
            break;
         case lStringT:
            lSetString(add_qp, info_entry->nm_name, **spp);
            break;
         case lHostT:   
            lSetHost(add_qp, info_entry->nm_name, **spp);
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_INTERNALFAILURE_S, "qconf");
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(1);
      }
      lAppendElem(qlp, add_qp);
   }

   if (!qlp) {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_MQATTR_MISSINGOBJECTLIST_S, "qconf");
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      lFreeElem(epp);
      lFreeWhat(&what);
      DRETURN(1);
   }

   /* Bugfix: Issuezilla #1025
    * If we get a list value of NONE with -mattr, complain.  This comes after
    * the complaint about not including the object list on purpose.  That order
    * seems to make the most sense for error reporting, even though it means we
    * do some unnecessary work. */
   if ((sub_command & ocs::gdi::SubCommand::SGE_GDI_CHANGE) && (lGetType((*epp)->descr, fields[0]) == lListT)) {
      lList *lp = lGetListRW(*epp, fields[0]);
      
      if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_QCONF_CANT_MODIFY_NONE);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         lFreeElem(epp);
         lFreeWhat(&what);
         lFreeList(&qlp);
         lFreeList(&lp);
         DRETURN(1);
      } else if (lGetNumberOfElem(lp) == 1) {
         const lListElem *ep = lFirst(lp);
         int count = 1;
         int nm = 0;

         /* Look for the list and see if it's nullptr.  These should all be very
          * simple CULL structures which should only contain an lHostT and an
          * lListT. */
         for (nm = ep->descr[0].nm; nm != NoName; nm = ep->descr[count++].nm) {            
            if (lGetType(ep->descr, nm) == lListT) {
               // for CE_Type objects which are no RSMAPs CE_resource_map_list will be nullptr - this is OK
               if (lGetList(ep, nm) == nullptr && nm != CE_resource_map_list) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_QCONF_CANT_MODIFY_NONE);
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  lFreeElem(epp);
                  lFreeWhat(&what);
                  lFreeList(&qlp);
                  DRETURN(1);
               }
            }
         }
      }
   }
   
   if (info_entry->pre_gdi_function == nullptr ||
      info_entry->pre_gdi_function(qlp, alpp)) {
      *alpp = ocs::gdi::Client::sge_gdi(info_entry->target, ocs::gdi::Command::SGE_GDI_MOD, sub_command, &qlp,
                      nullptr, what);
   }
   lFreeElem(epp);
   lFreeWhat(&what);
   lFreeList(&qlp);
   (*spp)++;

   DRETURN(0);
}

static const char *write_attr_tmp_file(const char *name, const char *value, 
                                       const char *delimiter, dstring *error_message)
{
   char *filename = sge_malloc(sizeof(char) * SGE_PATH_MAX);
   int fd = -1;
   FILE *fp = nullptr;
   int my_errno;
   DENTER(TOP_LAYER);

   if (sge_tmpnam(filename, &fd, error_message) == nullptr) {
      DRETURN(nullptr);
   }

   errno = 0;
   fp = fdopen(fd, "w");
   my_errno = errno;

   if (fp == nullptr) {
      sge_dstring_sprintf(error_message, MSG_ERROROPENINGFILEFORWRITING_SS, filename, strerror(my_errno));
      DRETURN(nullptr);
   }
   
   fprintf(fp, "%s", name);
   fprintf(fp, "%s", delimiter);
   fprintf(fp, "%s\n", value);
   
   FCLOSE(fp);
   
   DRETURN((const char *)filename);
FCLOSE_ERROR:
   DRETURN(nullptr);
}
