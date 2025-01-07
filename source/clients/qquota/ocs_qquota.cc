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
 *  Portions of this code are Copyright 2011 Univa Corporation.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <math.h>

#include "basis_types.h"

#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_cull_xml.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "sig_handlers.h"
#include "ocs_qquota_print.h"
#include "ocs_client_parse.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qquota.h"

static report_handler_t* create_xml_report_handler(lList **alpp);

static int xml_report_finished(report_handler_t* handler, lList **alpp);
static int xml_report_started(report_handler_t* handler, lList **alpp);

static int xml_report_limit_rule_begin(report_handler_t* handler, const char* limit_name, lList **alpp);
static int xml_report_limit_rule_finished(report_handler_t* handler, const char *limit_name, lList **alpp);
static int xml_report_limit_string_value(report_handler_t* handler, const char* name, const char *value, bool exclude, lList **alpp);
static int xml_report_resource_value(report_handler_t* handler, const char* resource, const char* limit, const char*value, lList **alpp);
static int destroy_xml_report_handler(report_handler_t** handler, lList **alpp);

static int xml_report_started(report_handler_t* handler, lList **alpp) {
   printf("<?xml version='1.0'?>\n");
   printf("<qquota_result xmlns=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qquota/qquota.xsd\">\n");
   return 0;
}

static int xml_report_finished(report_handler_t* handler, lList **alpp) {
   printf("</qquota_result>\n");
   return 0;
}

static report_handler_t* create_xml_report_handler(lList **alpp) {
   
   report_handler_t* ret = (report_handler_t*)sge_malloc(sizeof(report_handler_t));
   if (ret == nullptr ) {
      answer_list_add(alpp, "malloc of report_handler_t failed",
                            STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
      return nullptr;
   }
   ret->ctx = sge_malloc(sizeof(dstring));
   if (ret->ctx == nullptr ) {
      answer_list_add(alpp, "malloc of dstring buffer failed",
                            STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
      return nullptr;
   }
   memset(ret->ctx, 0, sizeof(dstring));
   
   ret->report_started = xml_report_started;
   ret->report_finished = xml_report_finished;
   ret->report_limit_rule_begin = xml_report_limit_rule_begin;
   ret->report_limit_string_value = xml_report_limit_string_value;
   ret->report_limit_rule_finished = xml_report_limit_rule_finished;
   ret->report_resource_value = xml_report_resource_value;

   ret->destroy = destroy_xml_report_handler;
   return ret;
}

static int destroy_xml_report_handler(report_handler_t** handler, lList **alpp) {
   if (*handler != nullptr ) {
      sge_dstring_free((dstring*)(*handler)->ctx);
      sge_free(&((*handler)->ctx));
      sge_free(handler);
      *handler = nullptr;
   }
   return 0;
}

static int xml_report_limit_rule_begin(report_handler_t* handler, const char* limit_name, lList **alpp) {
   
  escape_string(limit_name, (dstring*)handler->ctx);
  printf(" <qquota_rule name='%s'>\n", sge_dstring_get_string((dstring*)handler->ctx));
  sge_dstring_clear((dstring*)handler->ctx);
  return 0;
}

static int xml_report_limit_string_value(report_handler_t* handler, const char *name, const char *value, bool exclude, lList **alpp) {
   escape_string(name, (dstring*)handler->ctx);
   if (exclude) {
      printf("   <x%s>", sge_dstring_get_string((dstring*)handler->ctx) );
   } else {
      printf("   <%s>", sge_dstring_get_string((dstring*)handler->ctx) );
   }

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(value, (dstring*)handler->ctx);
   printf("%s", sge_dstring_get_string((dstring*)handler->ctx));

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);
   if (exclude) {
      printf("</x%s>\n", sge_dstring_get_string((dstring*)handler->ctx));
   } else {
      printf("</%s>\n", sge_dstring_get_string((dstring*)handler->ctx));
   }

   sge_dstring_clear((dstring*)handler->ctx);

   return 0;
}

static int xml_report_limit_rule_finished(report_handler_t* handler, const char *limit_name, lList **alpp) {
  printf(" </qquota_rule>\n");   
   return 0;
}

static int xml_report_resource_value(report_handler_t* handler, const char* resource, const char* limit, const char *value, lList **alpp) {
   escape_string(resource, (dstring*)handler->ctx);
   printf("   <limit resource='%s' ", sge_dstring_get_string((dstring*)handler->ctx));
 
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(limit, (dstring*)handler->ctx);   
   printf("limit='%s'", sge_dstring_get_string((dstring*)handler->ctx));

   if (value != nullptr) {
      sge_dstring_clear((dstring*)handler->ctx);
      escape_string(value, (dstring*)handler->ctx);   
      printf(" value='%s'", sge_dstring_get_string((dstring*)handler->ctx));
   }
   printf("/>\n");
   
   sge_dstring_clear((dstring*)handler->ctx);
   return 0;
}

static bool sge_parse_from_file_qquota(const char *file, lList **ppcmdline, lList **alpp);
static bool sge_parse_cmdline_qquota(char **argv, lList **ppcmdline, lList **alpp);
static bool sge_parse_qquota(lList **ppcmdline, lList **host_list, lList **resource_match_list,
                             lList **user_list, lList **pe_list, lList **project_list, lList **cqueue_list,
                             report_handler_t **report_handler, lList **alpp);
static bool qquota_usage(FILE *fp);

extern char **environ;
                                      
/************************************************************************/
int main(int argc, char **argv)
{
   lList *pcmdline = nullptr;
   lList *host_list = nullptr;
   lList *resource_match_list = nullptr;
   lList *user_list = nullptr;
   lList *pe_list = nullptr;
   lList *project_list = nullptr;
   lList *cqueue_list = nullptr;
   lList *alp = nullptr;
   report_handler_t *report_handler = nullptr;
   bool qquota_result = true;

   DENTER_MAIN(TOP_LAYER, "qquota");

   log_state_set_log_gui(true);

   if (ocs::gdi::ClientBase::setup_and_enroll(QQUOTA, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   sge_setup_sig_handlers(QQUOTA);
   
   /*
   ** stage 1 of commandline parsing
   */
   {
      dstring file = DSTRING_INIT;
      const char *user = component_get_username();
      const char *cell_root = bootstrap_get_cell_root();

      /* arguments from SGE_ROOT/common/sge_qquota file */
      get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QQUOTA_FILE);
      if (sge_parse_from_file_qquota(sge_dstring_get_string(&file), &pcmdline, &alp)) {
         /* arguments from $HOME/.qquota file */
         if (get_user_home_file_path(&file, SGE_HOME_DEF_QQUOTA_FILE, user, &alp)) {
            sge_parse_from_file_qquota(sge_dstring_get_string(&file), &pcmdline, &alp);
         }
      }
      sge_dstring_free(&file); 

      if (alp) {
         answer_list_output(&alp);
         lFreeList(&pcmdline);
         sge_prof_cleanup();
         sge_exit(1);
      }
   }
   if (!sge_parse_cmdline_qquota(argv, &pcmdline, &alp)) {
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      sge_prof_cleanup();
      sge_exit(1);
   }

   /*
   ** stage 2 of commandline parsing 
   */
   if (!sge_parse_qquota(&pcmdline, 
            &host_list,             /* -h host_list                  */
            &resource_match_list,   /* -l resource_request           */
            &user_list,             /* -u user_list                  */
            &pe_list,               /* -pe pe_list                   */
            &project_list,          /* -P project_list               */
            &cqueue_list,           /* -q wc_queue_list              */
            &report_handler,
            &alp)) {
      /*
      ** low level parsing error! show answer list
      */
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      sge_prof_cleanup();
      sge_exit(1);
   }

   qquota_result = qquota_output(host_list, resource_match_list, user_list, pe_list, project_list, cqueue_list, &alp, report_handler);

   if (report_handler != nullptr ) {
      report_handler->destroy(&report_handler, &alp);
   }
   
   if (!qquota_result) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   sge_prof_cleanup();
   sge_exit(0); /* 0 means ok - others are errors */
   DRETURN(0);
}

/****** qquota/qquota_usage() **************************************************
*  NAME
*     qquota_usage() -- displays qquota help output
*
*  SYNOPSIS
*     static bool qquota_usage(FILE *fp) 
*
*  FUNCTION
*     displays qquota_usage for qlist client
*     note that the other clients use a common function
*     for this. output was adapted to a similar look.
*
*  INPUTS
*     FILE *fp - output file pointer
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: qquota_usage() is MT safe 
*
*******************************************************************************/
static bool 
qquota_usage(FILE *fp)
{
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER);

   if (fp == nullptr) {
      DRETURN(false);
   }

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));
   fprintf(fp,"%s qquota [options]\n", MSG_SRC_USAGE);
   fprintf(fp, "  [-help]                              %s\n", MSG_COMMON_help_OPT_USAGE);
   fprintf(fp, "  [-h wc_host_list|wc_hostgroup_list]  %s\n", MSG_QQUOTA_h_OPT_USAGE);
   fprintf(fp, "  [-l resource_attributes]             %s\n", MSG_QQUOTA_l_OPT_USAGE);
   fprintf(fp, "  [-u wc_user]                         %s\n", MSG_QQUOTA_u_OPT_USAGE);
   fprintf(fp, "  [-pe wc_pe_list]                     %s\n", MSG_QQUOTA_pe_OPT_USAGE);
   fprintf(fp, "  [-P wc_project_list]                 %s\n", MSG_QQUOTA_P_OPT_USAGE); 
   fprintf(fp, "  [-q wc_cqueue_list]                  %s\n", MSG_QQUOTA_q_OPT_USAGE); 
   fprintf(fp, "  [-xml]                               %s\n", MSG_COMMON_xml_OPT_USAGE);
   fprintf(fp, "\n");
   fprintf(fp, "resource_attributes      resource_name,resource_name,...\n");
   fprintf(fp, "wc_cqueue                %s\n", MSG_QSTAT_HELP_WCCQ);
   fprintf(fp, "wc_cqueue_list           wc_cqueue[,wc_cqueue,...]\n");
   fprintf(fp, "wc_host                  %s\n", MSG_QSTAT_HELP_WCHOST);
   fprintf(fp, "wc_host_list             wc_host[,wc_host,...]\n");
   fprintf(fp, "wc_hostgroup             %s\n", MSG_QSTAT_HELP_WCHG);
   fprintf(fp, "wc_hostgroup_list        wc_hostgroup[,wc_hostgroup,...]\n");
   fprintf(fp, "wc_pe                    %s\n", MSG_QQUOTA_HELP_WCPE);
   fprintf(fp, "wc_pe_list               wc_pe[,wc_pe,...]\n");
   fprintf(fp, "wc_project               %s\n", MSG_QQUOTA_HELP_WCPROJECT);
   fprintf(fp, "wc_project_list          wc_project[,wc_project,...]\n");

   DRETURN(true);
}

/****** qquota/sge_parse_from_file_qquota() ************************************
*  NAME
*     sge_parse_from_file_qquota() -- parse qquota command line options from
*                                     file
*
*  SYNOPSIS
*     static bool sge_parse_from_file_qquota(const char *file, lList 
*     **ppcmdline, lList **alpp) 
*
*  FUNCTION
*     parses the qquota command line options from file
*
*  INPUTS
*     const char *file  - file name
*     lList **ppcmdline - found command line options
*     lList **alpp      - answer list pointer
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: sge_parse_from_file_qquota() is MT safe 
*
*******************************************************************************/
static bool
sge_parse_from_file_qquota(const char *file, lList **ppcmdline, lList **alpp)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   if (ppcmdline == nullptr) {
      ret = false;
   } else {
      if (!sge_is_file(file)) {
         /*
          * This is no error
          */
         DPRINTF("file " SFQ " does not exist\n", file);
         ret = true;
      } else {
         char *file_as_string = nullptr;
         int file_as_string_length;

         file_as_string = sge_file2string(file, &file_as_string_length);
         if (file_as_string == nullptr) {
            answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
            ret = false;
         } else {
            char **token = nullptr;

            token = stra_from_str(file_as_string, " \n\t");
            ret = sge_parse_cmdline_qquota(token, ppcmdline, alpp);
         }
      }
   }  
   DRETURN(ret); 
}

/****** qquota/sge_parse_cmdline_qquota() **************************************
*  NAME
*     sge_parse_cmdline_qquota() -- ??? 
*
*  SYNOPSIS
*     static bool sge_parse_cmdline_qquota(char **argv, lList **ppcmdline, 
*     lList **alpp) 
*
*  FUNCTION
*     'stage 1' parsing of qquota-options. Parses options
*     with their arguments and stores them in ppcmdline.
*
*  INPUTS
*     char **argv       - argument list
*     lList **ppcmdline - found arguments
*     lList **alpp      - answer list pointer
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: sge_parse_cmdline_qquota() is MT safe 
*
*******************************************************************************/
static bool sge_parse_cmdline_qquota(char **argv, lList **ppcmdline, lList **alpp)
{
   char **sp;
   char **rp;
   DENTER(TOP_LAYER);

   if (argv == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_NULLPOINTER);
      DRETURN(false);
   }

   rp = ++argv;
   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, alpp)) != sp)
         continue;
 
      /* -h option */
      if ((rp = parse_until_next_opt2(sp, "-h", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -l option */
      if ((rp = parse_until_next_opt2(sp, "-l", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -u option */
      if ((rp = parse_until_next_opt2(sp, "-u", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -pe option */
      if ((rp = parse_until_next_opt2(sp, "-pe", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -P option */
      if ((rp = parse_until_next_opt2(sp, "-P", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -q */
      if ((rp = parse_until_next_opt2(sp, "-q", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -xml */
      if ((rp = parse_noopt(sp, "-xml", nullptr, ppcmdline, alpp)) != sp)
         continue;
      
      /* oops */
      qquota_usage(stderr);
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      DRETURN(false);
   }
   DRETURN(true);
}

/****** qquota/sge_parse_qquota() **********************************************
*  NAME
*     sge_parse_qquota() -- parse qquota options
*
*  SYNOPSIS
*     static bool sge_parse_qquota(lList **ppcmdline, lList **host_list, lList 
*     **resource_list, lList **user_list, lList **pe_list, lList 
*     **project_list, lList **cqueue_list, report_handler_t **report_handler, 
*     lList **alpp) 
*
*  FUNCTION
*     'stage 2' parsing of qquota-options. Gets the options from pcmdline
*
*  INPUTS
*     lList **ppcmdline                 - found command line options (from stage 1)
*     lList **host_list                 - parsed host list (-h option)
*     lList **resource_list             - parsed resource list (-l option)
*     lList **user_list                 - parsed user list (-u option)
*     lList **pe_list                   - parsed pe list (-pe option)
*     lList **project_list              - parsed project list (-P option)
*     lList **cqueue_list               - parsed queue list (-q option)
*     report_handler_t **report_handler - report handler for xml output
*     lList **alpp                      - answer list
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: sge_parse_qquota() is MT safe 
*
*******************************************************************************/
static bool
sge_parse_qquota(lList **ppcmdline, lList **host_list, lList **resource_list,
                 lList **user_list, lList **pe_list, lList **project_list,
                 lList **cqueue_list, report_handler_t **report_handler, lList **alpp)
{
   u_long32 helpflag = 0;
   char *argstr = nullptr;
   bool ret = true;
 
   DENTER(TOP_LAYER);
 
   /* Loop over all options. Only valid options can be in the
      ppcmdline list. 
   */
   while (lGetNumberOfElem(*ppcmdline)) {
      if (parse_flag(ppcmdline, "-help",  alpp, &helpflag)) {
         qquota_usage(stdout);
         sge_exit(0);
         break;
      }

      if (parse_multi_stringlist(ppcmdline, "-h", alpp, host_list, ST_Type, ST_name)) {
         /* 
         ** resolve hostnames and replace them in list
         */
         lListElem *ep = nullptr;
         for_each_rw (ep, *host_list) {
            sge_resolve_host(ep, ST_name);
         }
         continue;
      }

      if (parse_string(ppcmdline, "-l", alpp, &argstr)) {
         *resource_list = centry_list_parse_from_string(*resource_list, argstr, false);
         sge_free(&argstr);
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-u", alpp, user_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-pe", alpp, pe_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-P", alpp, project_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_multi_stringlist(ppcmdline, "-q", alpp, cqueue_list, ST_Type, ST_name)) {
         continue;
      }
      if (parse_flag(ppcmdline, "-xml", alpp, &helpflag)) {
         *report_handler = create_xml_report_handler(alpp);
         continue;
      }
   }

   if (lGetNumberOfElem(*ppcmdline)) {
     qquota_usage(stderr);
     answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_PARSE_TOOMANYOPTIONS);
     ret = false;
   }

   DRETURN(ret);
}
