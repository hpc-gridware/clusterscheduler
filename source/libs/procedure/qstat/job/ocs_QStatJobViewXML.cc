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

/** @file
 * @brief XML rendering of `qstat -j`
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_cull_xml.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_ulong.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLE.h"

#include "sched/sge_schedd_text.h"

#include "qstat/job/ocs_QStatJobViewXML.h"
#include "qstat/ocs_QStatModelBase.h"

/**
 * @brief Report one job: nothing, the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param ilp the scheduler job info list (`SME`), holding the reasons a job is not running
 * @param job the job
 * @param flags which parts of the listing were requested
 */
void ocs::QStatJobViewXML::show_job(std::ostream &os, const ocs::tom::ConstList<ocs::tom::SME> ilp,
                                    const ocs::tom::ConstElement<ocs::tom::JB> job, int flags) {
}

/**
 * @brief Report the selected jobs and the messages of the scheduler as one XML document
 *
 * The job list and the message list are handed to the XML document and freed
 * with it, so the model holds neither afterwards.
 *
 * @param os stream to write to
 * @param parameter the call's parameters
 * @param model the fetched lists; its job and message lists are consumed
 * @return none
 */
void
ocs::QStatJobViewXML::show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);
   auto XML_out = ocs::tom::ListPtr<ocs::tom::XMLE>::create("detailed_job_info");

   /* add job infos */
   auto xmlElem = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
   auto attrElem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   ocs::tom::set<XMLA_Name>(attrElem, "djob_info");
   ocs::tom::set<XMLE_Element>(xmlElem, std::move(attrElem));
   ocs::tom::set<XMLE_Print>(xmlElem, true);
   ocs::tom::set<XMLE_List>(xmlElem, std::move(model.jlp));
   XML_out->push_back(std::move(xmlElem));

   /* add messages */
   auto xmlElem_xmle = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
   auto attrElem_xmla = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   ocs::tom::set<XMLA_Name>(attrElem_xmla, "messages");
   ocs::tom::set<XMLE_Element>(xmlElem_xmle, std::move(attrElem_xmla));
   ocs::tom::set<XMLE_Print>(xmlElem_xmle, true);
   ocs::tom::set<XMLE_List>(xmlElem_xmle, std::move(model.ilp));
   XML_out->push_back(std::move(xmlElem_xmle));

   // the head takes the list over
   const auto xml_head = xml_getHead("detailed_job_info", std::move(XML_out), nullptr);
   lWriteElemXMLTo(*xml_head, os);

   DRETURN_VOID;
}

/**
 * @brief Report why the pending jobs are not running, as XML
 *
 * Each message of the scheduler gets the text that summarizes its jobs, once per message. Its job ids are meant
 * to be sorted, but the sort order lacks its '%' and sorts by nothing. The list of messages is handed to the XML
 * document and freed with it.
 *
 * @param os stream to write to
 * @param parameter the call's parameters
 * @param model the fetched lists; its message list is consumed
 * @return none
 */
void ocs::QStatJobViewXML::show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);

   // each message gets the text that summarizes its jobs, as the plain view prints it above their ids
   if (const auto sme = model.ilp->front(); sme != nullptr) {
      for (const auto mes : ocs::tom::get<SME_message_list>(sme)) {
         // the job ids keep their stored order: the lPSortList(..., "I+", ULNG_value) that stood here lacked the
         // '%' and sorted by no key; a real sort would change the output
         const uint32_t mid = ocs::tom::get<MES_message_number>(mes);
         ocs::tom::set<MES_message>(mes, sge_schedd_text(mid + SCHEDD_INFO_OFFSET));
      }
   }

   /* print out xml info from list; the head takes the list over */
   const auto xml_head = xml_getHead("message", std::move(model.ilp), nullptr);
   lWriteElemXMLTo(*xml_head, os);

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

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param flags which parts of the listing were requested
 */
void ocs::QStatJobViewXML::report_job_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job, int flags) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_category_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_exec_file(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_submission_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_deadline_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_submit_cmd_line(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_effective_submit_cmd_line(std::ostream &os,
                                                            const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ownership(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}
/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_env_core(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_execution_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_account(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_checkpoint(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_cwd(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_path_aliases(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_directive_prefix(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_stdin_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_stdout_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_stderr_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_reserve(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_merge_stderr(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_request_set_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_mail_options(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_mail_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_notify(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_name(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_priority(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_job_share(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_restart(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_shell_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
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
void ocs::QStatJobViewXML::report_env_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_verify(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_job_args(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_job_identifier_list(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_script_size(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_script_file(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_script_ptr(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_pe(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_jid_request_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_jid_predecessor_list(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_jid_successor_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ja_ad_request_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ja_ad_predecessor_list(std::ostream &os,
                                                         const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ja_ad_successor_list(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_verify_suitable_queues(std::ostream &os,
                                                         const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_soft_wallclock_gmt(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_hard_wallclock_gmt(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_version(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_override_tickets(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ar(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_project(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_department(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_sync_options(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ja_structure(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ja_task_concurrency(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_pending_tasks(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_ctx_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_binding(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_task_list_started(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewXML::report_task_list_finished(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_started(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                               const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_finished(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_state(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                             const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                          const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_usage(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                             const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_exec_binding_list(std::ostream &os,
                                                         const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                         const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_exec_queue_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                       const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_exec_host_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                      const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_start_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                  const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_end_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_resource_map(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                    const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewXML::report_task_error_reason(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                    const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Unused: the XML view writes the whole job list in show_jobs_and_reasons()
 * @param os stream to write to
 * @param ilp the scheduler job info list (`SME`), holding the reasons a job is not running
 * @param job the job
 */
void ocs::QStatJobViewXML::report_schedd_job_info(std::ostream &os, const ocs::tom::ConstList<ocs::tom::SME> ilp,
                                                  const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}
