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
 * @brief qconf - one handler per configuration switch, and the table that dispatches to them
 */
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cerrno>
#include <cctype>
#include <format>
#include <iostream>
#include <memory>
#include <ostream>
#include <fnmatch.h>
#include <dirent.h>
#include <unistd.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_edit.h"
#include "uti/sge_tmpnam.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_hostname.h"
#include "uti/sge_parse_num_par.h"   /* CS-2313a: parse_ulong_val for typed load values */

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/config.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_var.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/ocs_spool_json.h"   /* CS-2313a: ocs_json_value_format_opt, name-list writer */
#include "spool/flatfile/sge_flatfile_obj.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "sge.h"
#include "sge_options.h"
#include "usage.h"
#include "procedure/ocs_ProcedureController.h"
#include "procedure/ocs_ProcedureModel.h"
#include "procedure/ocs_ProcedureView.h"
#include "procedure/qconf/action/ocs_QConfActionParameter.h"
#include "procedure/qconf/write/ocs_QConfWriteModelClient.h"

#include "qconf/ocs_QConfController.h"
#include "qconf/table/ocs_QConfTableController.h"
#include "qconf/table/ocs_QConfTableView.h"
#include "qconf/threadpool/ocs_QConfThreadPoolController.h"
#include "qconf/threadpool/ocs_QConfThreadPoolView.h"
#include "qconf/sharetree/ocs_QConfShareTreeNodesController.h"
#include "qconf/sharetree/ocs_QConfShareTreeTreeController.h"
#include "qconf/sharetree/ocs_QConfShareTreeView.h"
#include "qconf/processors/ocs_QConfProcessorController.h"
#include "qconf/processors/ocs_QConfProcessorView.h"
#include "qconf/eventclient/ocs_QConfEventClientController.h"
#include "qconf/eventclient/ocs_QConfEventClientView.h"
#include "qconf/objl/ocs_QConfObjectFilterController.h"
#include "qconf/objl/ocs_QConfObjectFilterView.h"
#include "qconf/sick/ocs_QConfSickController.h"
#include "qconf/sick/ocs_QConfSickView.h"
#include "qconf/cqueue/ocs_QConfCQueueShowController.h"
#include "qconf/cqueue/ocs_QConfCQueueShowView.h"
#include "qconf/hgroup/ocs_QConfHGroupResolvedController.h"
#include "qconf/hgroup/ocs_QConfHGroupShowController.h"
#include "qconf/hgroup/ocs_QConfHGroupShowViewJSON.h"
#include "qconf/hgroup/ocs_QConfHGroupStructureView.h"
#include "qconf/hgroup/ocs_QConfHGroupTreeController.h"
#include "qconf/hgroup/ocs_QConfHGroupWhyController.h"
#include "qconf/hgroup/ocs_QConfHGroupWhyView.h"
#include "qconf/ocs_QConfModelClient.h"
#include "qconf/ocs_QConfObject.h"
#include "qconf/ocs_QConfViewJSON.h"
#include "qconf/ocs_QConfViewPlain.h"
#include "qconf/ocs_QConfParameterClient.h"
#include "ocs_qconf_parse.h"
#include "ocs_qconf_common.h"
#include "ocs_qconf_read.h"
#include "ocs_qconf_write.h"
#include "ocs_qconf_delete.h"
#include "ocs_qconf_action.h"
#include "sgeobj/ocs_Role.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qconf.h"

static int sge_next_is_an_opt(char **ptr);

static bool del_host_of_type(lList *arglp, ocs::gdi::Target target);
static bool mod_reserved_hgroup(std::ostream &os, lList *arglp, const char *group,
                                ocs::gdi::SubCommand sub_command, const char *what);
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp, lListElem **epp, ocs::gdi::SubCommand sub_command, struct object_info_entry *info_entry);


static const char *write_attr_tmp_file(const char *name, const char *value, 
                                       const char *delimiter, dstring *error_message);

/** @brief Step to the argument of the current switch, exiting when there is none
 *
 * A switch at the end of the command line is reported with its name and the
 * usage, and the client exits with status 1.
 *
 * @param arg the switch
 * @return the argument following @p arg
 */
static char **sge_parser_get_next(char **arg) {
   DENTER(TOP_LAYER);
   if (!*(arg+1)) {
      ERROR(MSG_QCONF_NOOPTIONARGPROVIDEDTOX_S , *arg);
      sge_usage(QCONF, stderr);
      sge_exit(1);
   }

   DRETURN(++arg);
}

/** @brief Work through the `qconf` command line, one switch at a time
 *
 * `qconf` accepts several switches in one invocation and applies them in the
 * order given, so this is a loop over the arguments rather than a single
 * dispatch. Most switches are handled by the table in #object_info_entry; the
 * ones that are not have their own handlers in this file.
 *
 * @param argv the arguments, not including `argv[0]`
 * @return 0 when every switch succeeded
 */
int sge_parse_qconf(char *argv[]) {
   DENTER(TOP_LAYER);

   char *cp = nullptr;
   char **spp = nullptr;
   int sge_parse_return = 0;
   lList *lp=nullptr, *alp=nullptr;
   lListElem *ep=nullptr;
   const lListElem *aep=nullptr;
   const char* qualified_hostname = component_get_qualified_hostname();
   const char* username = component_get_username();
   uint32_t prog_number = component_get_component_id();
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   /* The one place the sink is chosen (CS-1841). Everything below renders into
    * this stream instead of writing to stdout, so that step 8 (CS-2799) can hand
    * in qmaster's response buffer here without touching a single print site. */
   std::ostream &os = std::cout;

   /* If no arguments were given, output the help message on stderr. */
   if (*argv == nullptr) {
      sge_usage(QCONF, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      DRETURN(1);
   }
   
   /* CS-2798: the command line is parsed as data before anything is executed.
    * What comes back are the invocation-wide switches and the ordered list of
    * operations the execution phase walks.
    *
    * Those switches have to take effect no matter where they appear, so that
    * "qconf -sq all.q -fmt json" renders as JSON although -fmt comes last.
    * parse_parameters() reads them in a pass of its own over argv, exactly as
    * the hand written pre-scan did.
    *
    * MISSING_ARGUMENT and UNKNOWN_OPTION are not acted on here but after the
    * execution phase, so that a partially valid command line still produces the
    * output of its valid operations before the complaint - the order the single
    * parse-and-execute pass produced. */
   ocs::QConfParameterClient param(to_cstr(QCONF));
   std::string bad_token;
   const QConfParseStatus parse_status = param.parse_parameters(argv, bad_token);
   switch (parse_status) {
      case QConfParseStatus::NO_FORMAT_VALUE:
         fprintf(stderr, "%s\n", MSG_GDI_USAGE_Fmt_OPT);
         DRETURN(1);
      case QConfParseStatus::BAD_FORMAT:
         fprintf(stderr, MSG_QCONF_UNKNOWNFMT_S, bad_token.c_str());
         fprintf(stderr, "\n");
         DRETURN(1);
      case QConfParseStatus::BAD_FORMAT_VALUE:
         fprintf(stderr, MSG_QCONF_UNKNOWNFMTVAL_S, bad_token.c_str());
         fprintf(stderr, "\n");
         DRETURN(1);
      case QConfParseStatus::BAD_EXEC_CONTEXT:
         fprintf(stderr, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, bad_token.c_str());
         fprintf(stderr, "\n");
         DRETURN(1);
      default:
         break;
   }

   /* CS-2823: a caller may ask qmaster to answer as if RBAC had reduced the
    * response, so that the refusal to offer a reduced object to the editor can
    * be reached before redaction exists. Shipped as a request variable the way
    * qstat ships SGE_LONG_QNAMES; see FORCE_REDUCTION_VARIABLE for why handing
    * it to the server grants nothing. */
   if (const char *forced = getenv(ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE);
       forced != nullptr) {
      param.add_variable(ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE, forced);
   }

   /* CS-2799: the parameter object is the single source of truth. The file
    * statics below are a client-side mirror, written once, for the code that
    * has not moved into libs/procedure yet - the write path and the bespoke
    * show options of group N. They are never read by anything that runs inside
    * qmaster, which is what the rule is about; a client is one process per
    * invocation. They go when that code moves. */
   qconf_opt_dry_run = param.get_dry_run();
   qconf_opt_force = param.get_force();
   qconf_opt_strict = param.get_strict();
   qconf_opt_format = (param.get_output_format() == ocs::ProcedureParameter::OutputFormat::JSON)
                      ? SP_FORM_JSON : SP_FORM_ASCII;
   ocs_json_value_format_opt =
      (param.get_value_format() == ocs::QConfParameter::ValueFormat::NUMERIC)
      ? OCS_JSON_VALUES_NUMERIC : OCS_JSON_VALUES_COMPACT;

   /* CS-2798: the execution phase. It walks the operations the parser produced,
    * in the order they were typed, and hands each option block its own argument
    * vector - { switch, arg, ..., nullptr }. That is the shape the blocks have
    * always read, so `*spp`, sge_parser_get_next() and sge_next_is_an_opt()
    * keep working unchanged; they simply no longer reach across the operation
    * they belong to. The trailing `spp++; continue;` of every block now means
    * "on to the next operation".
    *
    * CS-2950: the first operation that ends with a non-zero status stops the
    * command line; the ones after it do not run. What ran before it stays
    * applied - nothing is rolled back - so the state is "everything up to the
    * failed switch", and the last message is the one that explains the exit
    * status. Inside one operation a list of names or a directory still runs to
    * the end and reports each failure, unless -strict makes it all or nothing. */
   for (qconf_op &op : param.get_operations()) {
      if (sge_parse_return != 0) {
         break;
      }
      spp = op.as_argv();

      /* CS-2799: the read path of groups A and B is a stored procedure now. In
       * SERVER context the operation is handed to qmaster, which fetches and
       * renders it - that is where RBAC knows what the caller may see. In
       * CLIENT context the same model and view run here, against data fetched
       * over GDI, which is what makes the two comparable byte for byte.
       *
       * CS-2808 added the last four of groups A and B, so the whole read path
       * of both groups goes this way.
       *
       * Then the other table driven switches: the reads with a sub-procedure
       * of their own and the plain single-object reads (CS-2955), the deletes
       * (CS-2952) and the adds and modifies (CS-2954), each with the
       * client-side check it had. What none of the tables serves falls through
       * to the option blocks below and runs locally. */
      QConfOpResult result = qconf_run_read_procedure(os, param, op);
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_sub_procedure(os, param, op);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_delete(os, op, username, qualified_hostname);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_write(os, param, op, username, qualified_hostname, uid, gid);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_action(param, op, username, qualified_hostname);
      }
      if (result != QConfOpResult::NOT_HANDLED) {
         if (result == QConfOpResult::FAILED) {
            sge_parse_return = 1;
         }
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-ah server_name[,server_name,...]" */
      if (strcmp("-ah", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to add", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, ADMIN_HOSTGROUP,
                                  ocs::gdi::SubCommand::APPEND, "administrative host")) {
            sge_parse_return |= 1;
         }

         lFreeList(&lp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      /* "-as server_name[,server_name,...]" */
      if (strcmp("-as", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to add", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, SUBMIT_HOSTGROUP,
                                  ocs::gdi::SubCommand::APPEND, "submit host")) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-de server_name[,server_name,...]" */
      if (strcmp("-de", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, EH_Type, EH_name, *spp);
         if (!del_host_of_type(lp, ocs::gdi::Target::EH_LIST)) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-dh server_name[,server_name,...]" */
      if (strcmp("-dh", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, ADMIN_HOSTGROUP,
                                  ocs::gdi::SubCommand::REMOVE, "administrative host")) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-ds server_name[,server_name,...]" */
      if (strcmp("-ds", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, SUBMIT_HOSTGROUP,
                                  ocs::gdi::SubCommand::REMOVE, "submit host")) {
            sge_parse_return = 1;
         }
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
               sge_parse_return = 1;
               spp++;
               continue;
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

/*---------------------------------------------------------------------------*/

   if ((strcmp("-mattr", *spp) == 0) || (strcmp("-Mattr", *spp) == 0) ||
       (strcmp("-aattr", *spp) == 0) || (strcmp("-Aattr", *spp) == 0) ||
       (strcmp("-rattr", *spp) == 0) || (strcmp("-Rattr", *spp) == 0) ||
       (strcmp("-dattr", *spp) == 0) || (strcmp("-Dattr", *spp) == 0)) {

/* *INDENT-OFF* */
      static object_info_entry info_entry[] = {
         {ocs::gdi::Target::CQ_LIST,         SGE_OBJ_CQUEUE,    CQ_Type,   SGE_ATTR_QNAME,     CQ_name,   nullptr,     &qconf_sfi,        cqueue_xattr_pre_gdi},
         {ocs::gdi::Target::EH_LIST,         SGE_OBJ_EXECHOST,  EH_Type,   SGE_ATTR_HOSTNAME,  EH_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::PE_LIST,         SGE_OBJ_PE,        PE_Type,   SGE_ATTR_PE_NAME,   PE_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::CK_LIST,         SGE_OBJ_CKPT,      CK_Type,   SGE_ATTR_CKPT_NAME, CK_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::HGRP_LIST,       SGE_OBJ_HGROUP,    HGRP_Type, SGE_ATTR_HGRP_NAME, HGRP_name, nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::RQS_LIST,        SGE_OBJ_RQS,       RQS_Type,  SGE_ATTR_RQS_NAME,  RQS_name,  nullptr,     &qconf_rqs_sfi,    rqs_xattr_pre_gdi},
         {ocs::gdi::Target::NO_TARGET,           nullptr,           nullptr,   nullptr,            NoName,    nullptr,     nullptr,           nullptr}
      };
/* *INDENT-ON* */

      int from_file;
      int index;
      int ret = 0;
      ocs::gdi::SubCommand sub_command = ocs::gdi::SubCommand::NONE;

      /* This does not have to be freed later */
      info_entry[0].fields = CQ_fields;
      /* These have to be freed later */
      info_entry[1].fields = sge_build_EH_field_list(false, false, false, false);
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
         sub_command = ocs::gdi::SubCommand::CHANGE;
      } else if ((*spp)[1] == 'a' || (*spp)[1] == 'A') {
         sub_command = ocs::gdi::SubCommand::APPEND;
      } else if ((*spp)[1] == 'd' || (*spp)[1] == 'D') {
         sub_command = ocs::gdi::SubCommand::REMOVE;
      } else if ((*spp)[1] == 'r' || (*spp)[1] == 'R') {
         sub_command = ocs::gdi::SubCommand::SET;
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
         sge_parse_return = 1;
         continue;
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
            sge_parse_return = 1;
            continue;
         }
      }

      sge_free(&(info_entry[1].fields));

      continue;
   }

      /* "-cq destin_id[,destin_id,...]" */
      if (strcmp("-cq", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw (ep, lp) {
            lSetUlong(ep, ID_action, QI_DO_CLEAN);
         }
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::TRIGGER, ocs::gdi::SubCommand::NONE,
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
      /* "-huh?" - unreachable: qconf_parse_args() rejects a switch that is not
       * in the table, and every switch in the table is served by the read,
       * delete or write table or by a block above. Kept as the backstop it
       * always was, for a row added without its handler. */

      ERROR(MSG_ANSWER_INVALIDOPTIONARGX_S, *spp);
      fprintf(stderr, MSG_SRC_X_HELP_USAGE_S , "qconf");
      fprintf(stderr, "\n");
      DRETURN(1);
   }

   /* The command line was only understood up to here. The operations before the
    * bad token have run, so the complaint comes after their output - which is
    * where it came out when the loop parsed and executed in one pass.
    * MISSING_ARGUMENT needs nothing: the truncated operation was handed over
    * and its own block has already reported it. After a failed operation the
    * command line stopped before the bad token was reached (CS-2950), so the
    * complaint is not made - the failure is what the exit status reports. */
   if (sge_parse_return == 0 && parse_status == QConfParseStatus::UNKNOWN_OPTION) {
      ERROR(MSG_ANSWER_INVALIDOPTIONARGX_S, bad_token.c_str());
      fprintf(stderr, MSG_SRC_X_HELP_USAGE_S, "qconf");
      fprintf(stderr, "\n");
      lFreeList(&alp);
      DRETURN(1);
   }

   lFreeList(&alp);
   DRETURN(sge_parse_return);
}

/***********************************************************************/

void parse_name_list_to_cull(const char *name, lList **lpp, lDescr *dp, int nm, char *s) {
   DENTER(TOP_LAYER);

   char *cp2 = nullptr;
   lListElem *ep = nullptr;
   int pos;
   int dataType;


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

/** @brief Tell whether the next word of the command line is a switch or missing
 *
 * @param pptr the current word
 * @return 1 when there is no next word or it starts with '-', 0 otherwise
 */
static int sge_next_is_an_opt(char **pptr) {
   DENTER(TOP_LAYER);

   if (!*(pptr+1)) {
      DRETURN(1);
   }

   if (**(pptr+1) == '-') {
      DRETURN(1);
   }

   DRETURN(0);
}

int sge_error_and_exit(const char *ptr) {
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

/**
 * @brief Add/remove hosts in a reserved host group
 *
 * CS-2438. Admin and submit hosts live in the reserved host groups
 * "@admin_hosts" and "@submit_hosts", so qconf -ah/-dh/-as/-ds modify those
 * groups instead of the retired AH_LIST/SH_LIST targets.
 * The request is a GDI MOD on HGRP_LIST with the sublist sub-command
 * APPEND or REMOVE -- the mechanism behind "qconf -aattr/-dattr". It is
 * deliberately NOT a read-modify-write of HGRP_host_list in the client:
 * read-modify-write loses a concurrent "-as" issued between the read and the
 * write, which the old dedicated ADD/DEL target could not do. The qmaster
 * merges the sublist under its own lock (attr_mod_sub_list() via
 * hgroup_mod_hostlist()), so two concurrent additions cannot lose each other.
 * Privilege is unchanged: AH_LIST, SH_LIST and HGRP_LIST are labels of the
 * same manager-only case in sge_c_gdi.cc, so redirecting the target neither
 * grants nor removes any right.
 * CS-2754: ONE request for the whole name list, not one per host. The list
 * syntax always existed, the bulk behind it did not: "qconf -ah h1,...,h1000"
 * used to be a thousand round trips, each of which made the qmaster run
 * hgroup_mod() with its cache refresh and its event.
 *
 * The operation is atomic as a consequence. "qconf -ah h1,h2" with h1 already a
 * member used to add h2 and refuse h1; it now refuses both. That is the
 * behaviour these two groups are documented to have -- a script writing
 * "qconf -ds host || alert" has to fail when the removal did not happen -- and
 * it is the safer half of the trade for a security relevant list.
 *
 * The other half of the trade is the diagnosis. hgroup_reserved_delta_is_strict()
 * does report one error per offending entry, but only the LAST of them reaches
 * the client: sge_gdi_add_mod_generic() propagates lLast(tmp_alp) alone when the
 * modifier fails, because earlier elements may be success messages. So a list
 * with several bad names is refused naming one of them, where the per-host
 * version named every one. Every answer that does arrive is printed below, so
 * lifting that restriction in the qmaster is all it would take.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param arglp HR_Type list of names as parsed from the command line
 * @param group ADMIN_HOSTGROUP or SUBMIT_HOSTGROUP
 * @param sub_command APPEND to add, REMOVE to delete
 * @param what "administrative host" / "submit host", for messages
 *
 * @return true if every named host was resolved and the request succeeded
 */
static bool mod_reserved_hgroup(std::ostream &os, lList *arglp, const char *group,
                                ocs::gdi::SubCommand sub_command, const char *what) {
   DENTER(TOP_LAYER);

   bool ret = true;
   lList *hosts = nullptr;

   /* resolve first, locally, so an unusable name never reaches the request */
   for_each_rw_lv(argep, arglp) {
      const char *name = lGetHost(argep, HR_name);

      if (name == nullptr) {
         ret = false;
         continue;
      }

      /*
       * CS-2680, N-I-9. The class of the entry decides, and it is decided
       * *before* anything is resolved.
       *
       * A host group reference is passed through untouched: "-ah @group" is
       * legal (see the plan for CS-2438), and sge_resolve_host() would fail on
       * it. hgroup_mod() checks in the qmaster that the group exists and that
       * the reference introduces no cycle, so nothing has to be validated here.
       *
       * A matcher is passed through for a different reason: it describes a set
       * of hosts rather than naming one, so there is nothing to look up. The
       * qmaster normalises and validates it (ocs::Matcher::prepare()), which is
       * where the domain rules and the message catalogue are.
       *
       * Asking the classification rather than the resolver is the whole point.
       * sge_resolve_host() skips whatever ocs::is_expression() calls an
       * expression, so a matcher used to get through only when it happened to
       * carry a metacharacter: "host:gpu*" was let past by its star, while
       * "host:gpu001" - a perfectly good matcher describing exactly one name -
       * was sent to the name service and failed there.
       */
      switch (ocs::classify_member(name)) {
         case ocs::MemberClass::GROUP_REFERENCE:
         case ocs::MemberClass::MATCHER:
            break;

         case ocs::MemberClass::LITERAL_HOST:
            if (sge_resolve_host(argep, HR_name) != CL_RETVAL_OK) {
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, name);
               fprintf(stderr, "\n");
               ret = false;
               continue;
            }
            name = lGetHost(argep, HR_name);
            break;
      }

      lAddElemHost(&hosts, HR_name, name, HR_Type);
   }

   if (hosts == nullptr) {
      DRETURN(ret);
   }

   /* the names this request carries, kept for the report: the list itself
    * belongs to the request element once it is chained in below */
   lList *sent = lCopyList("hosts sent", hosts);

   /* the reduced element the qmaster merges: the group name and every entry to
    * add to or remove from its host list */
   lList *lp = lCreateList("host group to modify", HGRP_Type);
   lListElem *hgrp = lAddElemHost(&lp, HGRP_name, group, HGRP_Type);
   lSetList(hgrp, HGRP_host_list, hosts);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::MOD,
                                          sub_command, &lp, nullptr, nullptr);

   lListElem *aep = lFirstRW(alp);
   answer_exit_if_not_recoverable(aep);
   if (answer_get_status(aep) == STATUS_OK) {
      /*
       * CS-2645: every command reports either success or failure. This used
       * to confirm APPEND only, on the assumption that the retired targets
       * had said nothing on a successful -dh/-ds. They did say something:
       * their delete path printed the whole answer list of the qmaster,
       * INFO elements included, so a successful remove was confirmed with
       * "<user>@<host> removed \"<host>\" from submit host list". Staying
       * silent here was therefore not the preservation of the old behaviour
       * but a change of it, and it left a successful delete
       * indistinguishable from one that did nothing.
       *
       * Still one line per host, from the list this request carried: the
       * request became a batch, the report to the administrator did not.
       *
       * CS-2680, N-I-12. What the qmaster said on the way is printed first.
       * Notes and warnings live in that answer list - the transformation of a
       * matcher by the domain rules, the collapse of a duplicate, the catch-all,
       * the scope of a matcher in a reserved group - and dropping them made
       * "qconf -ah host:*" confer a standing grant on every host that can
       * reach the qmaster without saying a word. Everything but the last element
       * is printed; the last is the generic confirmation of the object layer,
       * which these commands deliberately replace with the per-host lines below.
       *
       * The return value stays what it is. A note or a warning must not turn a
       * change that was carried out into a reported failure.
       */
      const lListElem *aep_msg;
      const lListElem *aep_last = lLast(alp);

      for_each_ep(aep_msg, alp) {
         if (aep_msg == aep_last) {
            break;
         }
         fprintf(stderr, "%s\n", lGetString(aep_msg, AN_text));
      }

      const lListElem *host;

      for_each_ep(host, sent) {
         /* CS-2645 follow-up: this reports success, so it belongs on stdout.
          * qconf_info(os, ) writes there and appends the newline itself. */
         if (sub_command == ocs::gdi::SubCommand::APPEND) {
            qconf_info(os, MSG_QCONF_XADDEDTOYLIST_SS, lGetHost(host, HR_name), what);
         } else {
            qconf_info(os, MSG_QCONF_XREMOVEDFROMYLIST_SS, lGetHost(host, HR_name), what);
         }
      }
   } else {
      /* every answer, not just the first: the qmaster names each offending host */
      const lListElem *answer;

      for_each_ep(answer, alp) {
         fprintf(stderr, "%s\n", lGetString(answer, AN_text));
      }
      ret = false;
   }

   lFreeList(&sent);
   lFreeList(&lp);
   lFreeList(&alp);

   DRETURN(ret);
}

/* ------------------------------------------------------------ */

/* add_host_of_type() removed with CS-2438: -ah/-as now modify the reserved
 * host groups through mod_reserved_hgroup(). del_host_of_type() stays -- it is
 * still the implementation of -de for EH_LIST. */
/* ------------------------------------------------------------ */

/** @brief Delete every named host, the `qconf -de` switch
 *
 * CS-2753: ONE request for the whole name list. The list syntax always existed,
 * the bulk behind it did not -- this used to send one GDI DEL per name, and each
 * of those made the qmaster rebuild `@exec_hosts` from scratch.
 *
 * Nothing is given up for it, unlike in the host group case of CS-2754.
 * sge_c_gdi_del() walks the request element by element and appends one answer per
 * host to the task's answer list, so the report stays one line per host, and a
 * host that cannot be deleted holds up the others no more than it did before --
 * every element is still handled on its own in the qmaster.
 *
 * @param arglp the hosts to delete, as parsed from the command line
 * @param target the host list to delete from, EH_LIST
 * @return true if every host was deleted
 */
static bool del_host_of_type(lList *arglp, ocs::gdi::Target target) {
   DENTER(TOP_LAYER);

   lList *lp=nullptr, *alp=nullptr;
   lDescr *type = nullptr;
   bool ret = true;

   switch (target) {
   case ocs::gdi::Target::EH_LIST:
      type = EH_Type;
      break;
   default: ;
   }

   /* every named host in one request */
   lp = lCreateList("hosts_to_del", type);
   for_each_rw_lv (argep, arglp) {
      lAppendElem(lp, lCopyElem(argep));
   }

   /* delete them */
   alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::DEL, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);

   /* print results */
   if (answer_list_has_error(&alp)) {
      ret = false;
   }
   answer_list_on_error_print_or_exit(&alp, stderr);

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(ret);
}

/* ------------------------------------------------------------ */

int delete_configs(const lList *conf_list) {
   DENTER(TOP_LAYER);

   if (lGetNumberOfElem(conf_list) == 0) {
      DRETURN(0);
   }

   lList *lp = lCreateList("configs_to_del", CONF_Type);
   const lListElem *ep;
   for_each_ep(ep, conf_list) {
      lAddElemHost(&lp, CONF_name, lGetHost(ep, CONF_name), CONF_Type);
   }

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::DEL,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);

   /* one line per configuration, as before -- the qmaster reports each of them */
   int fail = 0;
   const lListElem *aep;
   for_each_ep(aep, alp) {
      fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      answer_exit_if_not_recoverable(aep);
      if (answer_get_status(aep) != STATUS_OK) {
         fail++;
      }
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(fail);
}

int
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

/**
 * @brief Sends a modify request to the master
 *
 * The function performs a SGE_GDI_MOD request to the qmaster.
 * It will get all necessary infomation from commandline or
 * file.
 * Depending on the parameters specified, the function will
 * modify only parts of an object. It is possible to address
 * only parts of a sublist of an object.
 *
 * @param alpp reference to an answer list where the master will store necessary messages for the user
 * @param from_file if set to 1 then the next commandline parameter (stored in spp) will contain a filename. This file contains the attributes which should be modified
 * @param spp pending list of commandline parameter
 * @param epp this reference will contain the reduced element which will be parsed from commandline or file after this function was called
 * @param sub_command what the request does with the sublist: CHANGE modifies
 *        sublist elements, APPEND adds elements, REMOVE removes them, SET
 *        replaces the complete sublist
 * @param info_entry pointer to a structure with function pointers, string pointers, and CULL names
 *
 * @return [alpp] Masters answer for the gdi request 1 for error 0 for success
 */
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp,
                                  lListElem **epp, ocs::gdi::SubCommand sub_command,
                                  struct object_info_entry *info_entry) {
   DENTER(TOP_LAYER);    

   int fields[150];
   lListElem *add_qp = nullptr;
   lList *qlp = nullptr;
   lEnumeration *what = nullptr;
   
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
                                        true, info_entry->instr, qconf_opt_format,
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
         for_each_rw_lv(aep, *alpp) {
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
         case lDoubleT:
            lSetDouble(add_qp, info_entry->nm_name, atof(**spp));
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
   if ((sub_command & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE && (lGetType((*epp)->descr, fields[0]) == lListT)) {
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
      *alpp = ocs::gdi::Client::sge_gdi(info_entry->target, ocs::gdi::Command::MOD, sub_command, &qlp,
                      nullptr, what);
   }
   lFreeElem(epp);
   lFreeWhat(&what);
   lFreeList(&qlp);
   (*spp)++;

   DRETURN(0);
}

/** @brief Write one attribute line "name<delimiter>value" into a new temporary file
 *
 * @param name the attribute name
 * @param value the attribute value
 * @param delimiter what separates name and value
 * @param error_message receives the reason when the file could not be written
 * @return the name of the file, allocated with sge_malloc() and owned by the
 *         caller, or nullptr on error
 */
static const char *write_attr_tmp_file(const char *name, const char *value,
                                       const char *delimiter, dstring *error_message) {
   DENTER(TOP_LAYER);

   char *filename = sge_malloc(sizeof(char) * SGE_PATH_MAX);
   int fd = -1;
   FILE *fp = nullptr;
   int my_errno;
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
