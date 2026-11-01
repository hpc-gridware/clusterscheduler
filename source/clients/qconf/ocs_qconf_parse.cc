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
#include "ocs_qconf_attr.h"
#include "ocs_qconf_control.h"
#include "sgeobj/ocs_Role.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qconf.h"


/** @brief Print what an operation has to report, and the usage when it asks for it (CS-2971)
 * @param alpp  the answer list of the operation; printed, freed and set to nullptr
 * @param usage whether the usage follows the messages; reset
 */
static void
qconf_report_operation(lList **alpp, bool &usage) {
   qconf_answer_print(alpp);
   if (usage) {
      sge_usage(QCONF, stderr);
      usage = false;
   }
}

/** @brief Work through the `qconf` command line, one switch at a time
 *
 * `qconf` accepts several switches in one invocation and applies them in the
 * order given, so this is a loop over the operations rather than a single
 * dispatch. Every switch but `-help` is served by the table of its module:
 * reads, deletes, writes, actions, controls and attributes (CS-2986).
 *
 * @param argv the arguments, not including `argv[0]`
 * @return 0 when every switch succeeded
 */
int sge_parse_qconf(char *argv[]) {
   DENTER(TOP_LAYER);

   char **spp = nullptr;
   int sge_parse_return = 0;
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

   /* CS-2984: "-dry" promises that nothing changes. Only the file and directory
    * adds, modifies and deletes have a dry mode; every other switch would carry
    * out its change regardless. So a command line that holds such a switch is
    * refused as a whole, before any of it runs - refusing only that switch
    * would leave the ones before it applied, which is just what the caller
    * asked not to happen. A switch that only reads has nothing to hold back. */
   if (param.get_dry_run()) {
      lList *dry_alp = nullptr;
      for (const qconf_op &op : param.get_operations()) {
         /* an unknown token has no row and is reported as such by the loop below */
         if (op.spec != nullptr && op.token != "-help" && !qconf_read_changes_nothing(op) &&
             !qconf_write_honours_dry(op) && !qconf_delete_honours_dry(op)) {
            qconf_answer(&dry_alp, ANSWER_QUALITY_ERROR, MSG_QCONF_DRYNOTSUPPORTED_S, op.token.c_str());
         }
      }
      if (dry_alp != nullptr) {
         qconf_answer_print(&dry_alp);
         lFreeList(&dry_alp);
         DRETURN(1);
      }
   }

   /* CS-2798: the execution phase. It walks the operations the parser produced,
    * in the order they were typed, and hands each one to the tables of the
    * modules. Since CS-2986 no option block is left but "-help".
    *
    * CS-2950: the first operation that ends with a non-zero status stops the
    * command line; the ones after it do not run. What ran before it stays
    * applied - nothing is rolled back - so the state is "everything up to the
    * failed switch", and the last message is the one that explains the exit
    * status. Inside one operation a list of names or a directory still runs to
    * the end and reports each failure, unless -strict makes it all or nothing. */
   /* CS-2971: every operation reports through an answer list of its own - what
    * qmaster answered and what the client has to say alike - which is printed
    * when the operation is done: before the next one runs, and before the
    * command line stops after a failure. */
   lList *op_alp = nullptr;
   bool op_usage = false;
   for (qconf_op &op : param.get_operations()) {
      qconf_report_operation(&op_alp, op_usage);
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
       * (CS-2952) and the adds and modifies (CS-2954); the permission is
       * qmaster's (CS-2973). What none of the tables serves falls through
       * to the option blocks below and runs locally. */
      QConfOpResult result = qconf_run_read_procedure(os, param, op, &op_alp);
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_sub_procedure(os, param, op, &op_alp);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_delete(os, op, &op_alp);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_write(os, param, op, uid, gid, &op_alp);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_action(os, param, op, &op_alp);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_control(op, &op_alp);
      }
      if (result == QConfOpResult::NOT_HANDLED) {
         result = qconf_run_attr(op, &op_alp);
      }
      if (result != QConfOpResult::NOT_HANDLED) {
         if (result == QConfOpResult::FAILED || result == QConfOpResult::FAILED_USAGE) {
            sge_parse_return = 1;
         }
         op_usage = (result == QConfOpResult::FAILED_USAGE);
         continue;
      }

      /* what no table serves reports a short command line the way the tables do */
      if (qconf_missing_argument(op, &op_alp)) {
         sge_parse_return = 1;
         op_usage = true;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-help" */

      if (strcmp("-help", *spp) == 0) {
         sge_usage(QCONF, stdout);
         DRETURN(0);
      }

/*----------------------------------------------------------------------------*/
      /* "-huh?" - unreachable: qconf_parse_args() rejects a switch that is not
       * in the table, and every switch in the table is served by a module's
       * table or by "-help" above. Kept as the backstop it always was, for a
       * row added without its handler. */

      qconf_answer(&op_alp, ANSWER_QUALITY_ERROR, MSG_ANSWER_INVALIDOPTIONARGX_S, *spp);
      qconf_answer(&op_alp, ANSWER_QUALITY_INFO, MSG_SRC_X_HELP_USAGE_S, "qconf");
      qconf_report_operation(&op_alp, op_usage);
      DRETURN(1);
   }
   qconf_report_operation(&op_alp, op_usage);

   /* The command line was only understood up to here. The operations before the
    * bad token have run, so the complaint comes after their output - which is
    * where it came out when the loop parsed and executed in one pass.
    * MISSING_ARGUMENT needs nothing: the truncated operation was handed over
    * and its own block has already reported it. After a failed operation the
    * command line stopped before the bad token was reached (CS-2950), so the
    * complaint is not made - the failure is what the exit status reports. */
   if (sge_parse_return == 0 && parse_status == QConfParseStatus::UNKNOWN_OPTION) {
      qconf_answer(&op_alp, ANSWER_QUALITY_ERROR, MSG_ANSWER_INVALIDOPTIONARGX_S, bad_token.c_str());
      qconf_answer(&op_alp, ANSWER_QUALITY_INFO, MSG_SRC_X_HELP_USAGE_S, "qconf");
      qconf_report_operation(&op_alp, op_usage);
      DRETURN(1);
   }

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
