/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_cull_xml.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_ulong.h"

#include "sched/sge_schedd_text.h"

#include "qstat/job/ocs_QStatJobViewXML.h"
#include "qstat/ocs_QStatModelBase.h"

void ocs::QStatJobViewXML::show_job(std::ostream &os, const lList *ilp, const lListElem *job, int flags) {
}

void
ocs::QStatJobViewXML::show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);
   lList *XML_out = lCreateList("detailed_job_info", XMLE_Type);

   /* add job infos */
   lListElem *xmlElem = lCreateElem(XMLE_Type);
   lListElem *attrElem = lCreateElem(XMLA_Type);
   lSetString(attrElem, XMLA_Name, "djob_info");
   lSetObject(xmlElem, XMLE_Element, attrElem);
   lSetBool(xmlElem, XMLE_Print, true);
   lSetList(xmlElem, XMLE_List, model.jlp);
   model.jlp = nullptr;
   lAppendElem(XML_out, xmlElem);

   /* add messages */
   xmlElem = lCreateElem(XMLE_Type);
   attrElem = lCreateElem(XMLA_Type);
   lSetString(attrElem, XMLA_Name, "messages");
   lSetObject(xmlElem, XMLE_Element, attrElem);
   lSetBool(xmlElem, XMLE_Print, true);
   lSetList(xmlElem, XMLE_List, model.ilp);
   model.ilp = nullptr;
   lAppendElem(XML_out, xmlElem);

   lListElem *xml_elem = xml_getHead("detailed_job_info", XML_out, nullptr);
   lWriteElemXMLTo(xml_elem, os);
   lFreeElem(&xml_elem);

   DRETURN_VOID;
}

void ocs::QStatJobViewXML::show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);

   /* need to modify list to display correct message */
   if (const lListElem *sme = lFirst(model.ilp); sme != nullptr) {
      for_each_rw_lv(mes, lGetListRW(sme, SME_message_list)) {
         lPSortList(lGetListRW(mes, MES_job_number_list), "I+", ULNG_value);

         for_each_rw_lv(jid_ulng, lGetList(mes, MES_job_number_list)) {
            const uint32_t mid = lGetUlong(mes, MES_message_number);
            lSetString(mes, MES_message, sge_schedd_text(mid + SCHEDD_INFO_OFFSET));
         }
      }
   }

   /* print out xml info from list */
   lListElem *xml_elem = xml_getHead("message", model.ilp, nullptr);
   lWriteElemXMLTo(xml_elem, os);
   lFreeElem(&xml_elem);
   model.ilp = nullptr;

   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}


void ocs::QStatJobViewXML::report_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_jobs_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_separator(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_id(std::ostream &os, const lListElem *job, int flags) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_category_id(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_exec_file(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_submission_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_deadline_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_submit_cmd_line(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_effective_submit_cmd_line(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ownership(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}
void ocs::QStatJobViewXML::report_env_core(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_execution_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_account(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_checkpoint(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_cwd(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_path_aliases(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_directive_prefix(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_stdin_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_stdout_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_stderr_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_reserve(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_merge_stderr(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_request_set_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_mail_options(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_mail_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_notify(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_name(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_priority(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_share(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_restart(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_shell_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Job environment is intentionally not emitted in qstat -j XML output.
 *
 * Unlike the plain and JSON views, the XML view does not expose the job
 * environment (JB_env_list); this override is deliberately a no-op.
 *
 * SECURITY (CS-2355, MEDIUM-QSTAT-001): if environment output is ever added
 * here, do NOT emit JB_env_list verbatim — apply the redaction policy decided in
 * CS-2355 (the plain and JSON views disclose it unredacted today).
 *
 * @param[in,out] os  output stream (unused)
 * @param[in]     job job element (unused)
 */
void ocs::QStatJobViewXML::report_env_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_verify(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_args(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_job_identifier_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_script_size(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_script_file(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_script_ptr(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_pe(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_jid_request_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_jid_predecessor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_jid_successor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ja_ad_request_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ja_ad_predecessor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ja_ad_successor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_verify_suitable_queues(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_soft_wallclock_gmt(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_hard_wallclock_gmt(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_version(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_override_tickets(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ar(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_project(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_department(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_sync_options(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ja_structure(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ja_task_concurrency(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_pending_tasks(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_ctx_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_binding(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_list_started(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_list_finished(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_started(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_finished(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_state(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_id(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_usage(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_exec_binding_list(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_exec_queue_list(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_exec_host_list(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_start_time(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_end_time(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_resource_map(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_task_error_reason(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_schedd_job_info(std::ostream &os, const lList *ilp, const lListElem *job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}
