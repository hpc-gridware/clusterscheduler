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
 * @brief JSON rendering of `qstat -j`
 */

#include <sstream>
#include <string>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingType.h"
#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_qref.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_QIM.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "qstat/ocs_QStatParameter.h"
#include "qstat/job/ocs_QStatJobViewJSON.h"

void
ocs::QStatJobViewJSON::show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);
   // -j without jid's is deprecated and not implemented for JSON output
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n"
         << std::string(indent * 3, ' ') <<
         R"("$id": "https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qstat-job.schema.json")";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n}\n";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"jobs\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_jobs_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_job_separator(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_job_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_job_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   first_attribute = true;
   DRETURN_VOID;
}

/**
 * @brief Write a 32 bit number field of the job as a member of the job object
 *
 * Nothing is written if the (reduced) job does not hold the field.
 *
 * @param os stream to write to
 * @param job the job
 * @tparam F the field, a `uint32_t` field of `JB`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_uint32(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                            const char *name) {
   DENTER(TOP_LAYER);
   if (!job.has<F>()) {
      DRETURN_VOID;
   }
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": " << job.get<F>();
   DRETURN_VOID;
}

/**
 * @brief Report the job number, as JSON
 * @param os stream to write to
 * @param job the job
 * @param flags which parts of the listing were requested
 */
void ocs::QStatJobViewJSON::report_job_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job, int flags) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_job_number>(os, job, "job_number");
   DRETURN_VOID;
}

/**
 * @brief Report the scheduling category the job was sorted into, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_category_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_category_id>(os, job, "category_id");
   DRETURN_VOID;
}

/**
 * @brief Write a string field of an element as a member of the current JSON object
 *
 * Nothing is written for a null string.
 *
 * @tparam F the string field; its type is the element's
 * @param os stream to write to
 * @param elem the job or one of its request sets
 * @param name the name of the member
 * @return none
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_string(std::ostream &os,
                                            const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> elem,
                                            const char *name) {
   DENTER(TOP_LAYER);
   if (const char *value = elem.template get<F>(); value != nullptr) {
      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "\"" << name << "\": " << raw2quotedJSON(value);
   }
   DRETURN_VOID;
}

/**
 * @brief Report the job script as it was spooled, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_exec_file(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_exec_file>(os, job, "exec_file");
   DRETURN_VOID;
}

/**
 * @brief Write a time field of the job as an ISO 8601 timestamp member of the job object
 *
 * Nothing is written for a time of 0.
 *
 * @param os stream to write to
 * @param job the job
 * @tparam F the field, a `uint64_t` time field of `JB`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_ISO_8601_timestamp(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                        const char *name) {
   DENTER(TOP_LAYER);
   const uint64_t sec = job.get<F>();
   if (sec == 0) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": \"";
   show_ISO_8601_timestamp(os, sec);
   os << "\"";
   DRETURN_VOID;
}

/**
 * @brief Write a time field of an array task as an ISO 8601 timestamp member of the task object
 *
 * Nothing is written for a time of 0.
 *
 * @param os stream to write to
 * @param task the array task the row is about
 * @tparam F the field, a `uint64_t` time field of `JAT`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_task_ISO_8601_timestamp(std::ostream &os,
                                                             const ocs::tom::ConstElement<ocs::tom::JAT> task,
                                                             const char *name) {
   DENTER(TOP_LAYER);
   const uint64_t sec = task.get<F>();
   if (sec == 0) {
      DRETURN_VOID;
   }

   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": \"";
   show_ISO_8601_timestamp(os, sec);
   os << "\"";
   DRETURN_VOID;
}

/**
 * @brief Report when the job was submitted, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_submission_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp<JB_submission_time>(os, job, "submission_time");
   DRETURN_VOID;
}

/**
 * @brief Report the deadline of a deadline job, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_deadline_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp<JB_deadline>(os, job, "deadline_time");
   DRETURN_VOID;
}

/**
 * @brief Report the command line the job was submitted with, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_submit_cmd_line(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_submission_command_line>(os, job, "submit_cmd_line");
   DRETURN_VOID;
}

/**
 * @brief Report the command line after defaults and `sge_request` files were applied, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_effective_submit_cmd_line(std::ostream &os,
                                                             const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const char *str = job.get<JB_submission_command_line>();
   if (str == nullptr) {
      DRETURN_VOID;
   }

   // strtok_r() writes into the text, so it works on a copy
   std::string copied_str{str};
   char *saveptr = nullptr;
   if (const char *command = strtok_r(copied_str.data(), " ", &saveptr); command != nullptr) {
      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      dstring dstr_cmd = DSTRING_INIT;
      os << std::string(indent * 3, ' ') << "\"effective_submit_cmd_line\": " << raw2quotedJSON(
         job_get_effective_command_line(job, &dstr_cmd, command));
      sge_dstring_free(&dstr_cmd);
   }
   DRETURN_VOID;
}

/**
 * @brief Report the submitting user and group, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ownership(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"ownership\": {\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"primary\": {\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"owner\": " << raw2quotedJSON(
      job.get<JB_owner>() ? job.get<JB_owner>() : "") << ",\n";
   os << std::string(indent * 3, ' ') << "\"uid\": " << job.get<JB_uid>() << ",\n";
   os << std::string(indent * 3, ' ') << "\"group\": " << raw2quotedJSON(
      job.get<JB_group>() ? job.get<JB_group>() : "") << ",\n";
   os << std::string(indent * 3, ' ') << "\"gid\": " << job.get<JB_gid>() << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "},\n";
   os << std::string(indent * 3, ' ') << "\"supplementary\": [\n";
   indent++;
#if defined(WITH_EXTENSIONS)
   ocs::tom::ConstList<ocs::tom::ST> grp_list = job.get<JB_grp_list>();
   bool first = true;
   for (ocs::tom::ConstElement<ocs::tom::ST> grp_elem : grp_list) {
      if (first) {
         first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(
         grp_elem.get<ST_name>() ? grp_elem.get<ST_name>() : "") << ",\n";
      os << std::string(indent * 3, ' ') << "\"id\": " << grp_elem.get<ST_id>() << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }
#endif
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
}

/**
 * @brief Report the parts of the environment the system itself sets, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_env_core(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const char *name[] = {"O_HOME", "O_LOGNAME", "O_PATH", "O_SHELL", "O_TZ", "O_WORKDIR", "O_HOST", nullptr};
   const char *fmt_string[] = {
      "sge_o_home:", "sge_o_log_name:", "sge_o_path:", "sge_o_shell:", "sge_o_tz:", "sge_o_workdir:", "sge_o_host:",
      nullptr
   };
   int i = -1;

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"environment_core\": [\n";
   indent++;

   bool is_first = true;
   while (name[++i] != nullptr) {
      char fullname[MAX_STRING_SIZE];
      snprintf(fullname, sizeof(fullname), "%s%s", VAR_PREFIX, name[i]);
      if (const char *value = job_get_env_string(job, fullname)) {
         if (is_first) {
            os << "\n";
            is_first = false;
         } else {
            os << ",\n";
         }
         os << std::string(indent * 3, ' ') << "{\n";
         indent++;
         os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(fmt_string[i]) << ",\n";
         os << std::string(indent * 3, ' ') << "\"value\": " << raw2quotedJSON(value) << "\n";
         indent--;
         os << std::string(indent * 3, ' ') << "}";
      }
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";

   DRETURN_VOID;
}

/**
 * @brief Report the earliest time the job may start, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_execution_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp<JB_execution_time>(os, job, "execution_time");
   DRETURN_VOID;
}

/**
 * @brief Report the accounting string, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_account(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_account>(os, job, "account");
   DRETURN_VOID;
}

/**
 * @brief Report the checkpointing environment, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_checkpoint(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const char *ckpt_name = job.get<JB_checkpoint_name>();
   const uint32_t ckpt_attr = job.get<JB_checkpoint_attr>();
   const uint32_t ckpt_int = job.get<JB_checkpoint_interval>();

   if (ckpt_name == nullptr && ckpt_attr == 0 && ckpt_int == 0) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"checkpoint\": {\n";
   indent++;

   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(ckpt_name ? ckpt_name : "") << ",\n";

   std::stringstream ss_ckpt_attr;
   job_get_ckpt_attr(ss_ckpt_attr, ckpt_attr);
   os << std::string(indent * 3, ' ') << "\"attr\": " << raw2quotedJSON(ss_ckpt_attr.str()) << ",\n";

   os << std::string(indent * 3, ' ') << "\"interval\": " << ckpt_int << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";

   DRETURN_VOID;
}

/**
 * @brief Report the working directory, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_cwd(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_cwd>(os, job, "cwd");
   DRETURN_VOID;
}

/**
 * @brief Report the path aliases in effect, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_path_aliases(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::ConstList<ocs::tom::PA> path_aliases = job.get<JB_path_aliases>()) {
      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "\"path_aliases\": [";
      indent++;

      bool is_first = true;
      for (ocs::tom::ConstElement<ocs::tom::PA> pa : path_aliases) {
         if (is_first) {
            os << "\n";
            is_first = false;
         } else {
            os << ",\n";
         }
         os << std::string(indent * 3, ' ') << "{\n";
         indent++;
         const char *src_path = pa.get<PA_origin>();
         os << std::string(indent * 3, ' ') << "\"src_path\": " << raw2quotedJSON(src_path ? src_path : "") << ",\n";
         const char *submit_host = pa.get<PA_submit_host>();
         os << std::string(indent * 3, ' ') << "\"submit_host\": " << raw2quotedJSON(submit_host ? submit_host : "") <<
               ",\n";
         const char *exec_host = pa.get<PA_exec_host>();
         os << std::string(indent * 3, ' ') << "\"exec_host\": " << raw2quotedJSON(exec_host ? exec_host : "") << ",\n";
         const char *dst_path = pa.get<PA_translation>();
         os << std::string(indent * 3, ' ') << "\"dst_path\": " << raw2quotedJSON(dst_path ? dst_path : "") << "\n";
         indent--;
         os << std::string(indent * 3, ' ') << "}";
      }

      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "]";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the prefix that marks directives in the job script, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_directive_prefix(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_directive_prefix>(os, job, "directive_prefix");
   DRETURN_VOID;
}

/**
 * @brief Write a path list of the job as an array member of the job object
 *
 * Nothing is written if the job has no such list.
 *
 * @param os stream to write to
 * @param job the job
 * @tparam F the field, a path list field (`PN`) of `JB`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                               const char *name) {
   DENTER(TOP_LAYER);
   if (const ocs::tom::ConstList<ocs::tom::PN> list = job.get<F>(); list != nullptr) {
      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
      indent++;

      bool is_first = true;
      for (const ocs::tom::ConstElement<ocs::tom::PN> elem : list) {
         if (is_first) {
            os << "\n";
            is_first = false;
         } else {
            os << ",\n";
         }
         os << std::string(indent * 3, ' ') << "{\n";
         indent++;

         const char *host = elem.get<PN_host>();
         os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << ",\n";

         const char *file_host = elem.get<PN_file_host>();
         os << std::string(indent * 3, ' ') << "\"file_host\": " << raw2quotedJSON(file_host ? file_host : "") << ",\n";

         const char *path = elem.get<PN_path>();
         os << std::string(indent * 3, ' ') << "\"path\": " << raw2quotedJSON(path ? path : "") << ",\n";

         const bool file_staging = elem.get<PN_file_staging>();
         os << std::string(indent * 3, ' ') << "\"file_staging\": " << (file_staging ? "true" : "false") << "\n";

         indent--;
         os << std::string(indent * 3, ' ') << "}";
      }

      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "]";
   }
   DRETURN_VOID;
}

/**
 * @brief Report where standard input comes from, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_stdin_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_path_list<JB_stdin_path_list>(os, job, "stdin_path_list");
   DRETURN_VOID;
}

/**
 * @brief Report where standard output goes, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_stdout_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_path_list<JB_stdout_path_list>(os, job, "stdout_path_list");
   DRETURN_VOID;
}

/**
 * @brief Report where standard error goes, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_stderr_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_path_list<JB_stderr_path_list>(os, job, "stderr_path_list");
   DRETURN_VOID;
}

/**
 * @brief Write a boolean field of the job as a member of the job object
 * @param os stream to write to
 * @param job the job
 * @tparam F the field, a `bool` field of `JB`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_boolean(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                             const char *name) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   const bool value = job.get<F>();
   os << std::string(indent * 3, ' ') << "\"" << name << "\": " << (value ? "true" : "false");
   DRETURN_VOID;
}


/**
 * @brief Report whether the job reserves resources while it waits, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_reserve(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_boolean<JB_reserve>(os, job, "reserve");
   DRETURN_VOID;
}

/**
 * @brief Report whether standard error is merged into standard output, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_merge_stderr(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_boolean<JB_merge_stderr>(os, job, "merge_stderr");
   DRETURN_VOID;
}

/**
 * @brief Write a resource request list of a request set as an array member of the job object
 * @param os stream to write to
 * @param jrs the request set of the job
 * @tparam F the field, a resource list field (`CE`) of `JRS`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_resource_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JRS> jrs,
                                                   const char *name) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<ocs::tom::CE> elem : ocs::tom::ConstList<ocs::tom::CE>(jrs.get<F>())) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *resource_name = elem.get<CE_name>();
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(resource_name ? resource_name : "") << ",\n";
      os << std::string(indent * 3, ' ') << "\"value\": ",
            show_resource_as_JSON_type(os, elem);
      os << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Write a queue request list of a request set as an array member of the job object
 * @param os stream to write to
 * @param jrs the request set of the job
 * @tparam F the field, a queue reference list field (`QR`) of `JRS`
 * @param name the name of the member
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_queue_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JRS> jrs,
                                                const char *name) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<ocs::tom::QR> elem : ocs::tom::ConstList<ocs::tom::QR>(jrs.get<F>())) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const char *qname = elem.get<QR_name>();
      os << std::string(indent * 3, ' ') << raw2quotedJSON(qname ? qname : "");
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Report the hard and soft resource requests, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_request_set_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   for (ocs::tom::ConstElement<ocs::tom::JRS> jrs : job.get<JB_request_set_list>()) {
      const uint32_t scope = jrs.get<JRS_scope>();

      if (jrs.get<JRS_hard_resource_list>()) {
         std::string list_name = get_scope_list_name(scope, JRS_hard_resource_list);
         report_X_resource_list<JRS_hard_resource_list>(os, jrs, list_name.c_str());
      }

      if (jrs.get<JRS_soft_resource_list>()) {
         std::string list_name = get_scope_list_name(scope, JRS_soft_resource_list);
         report_X_resource_list<JRS_soft_resource_list>(os, jrs, list_name.c_str());
      }

      if (jrs.get<JRS_hard_queue_list>()) {
         std::string list_name = get_scope_list_name(scope, JRS_hard_queue_list);
         report_X_queue_list<JRS_hard_queue_list>(os, jrs, list_name.c_str());
      }

      if (jrs.get<JRS_soft_queue_list>()) {
         std::string list_name = get_scope_list_name(scope, JRS_soft_queue_list);
         report_X_queue_list<JRS_soft_queue_list>(os, jrs, list_name.c_str());
      }

      if (jrs.get<JRS_allocation_rule>() != nullptr) {
         std::string name = get_scope_list_name(scope, JRS_allocation_rule);
         report_X_string<JRS_allocation_rule>(os, jrs, name.c_str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report which job events trigger a mail, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_mail_options(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (const uint32_t mail_options = job.get<JB_mail_options>()) {
      dstring mailopt = DSTRING_INIT;
      sge_dstring_append_mailopt(&mailopt, mail_options);

      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "\"mail_options\": " << raw2quotedJSON(sge_dstring_get_string(&mailopt));
      sge_dstring_free(&mailopt);
   }
   DRETURN_VOID;
}

/**
 * @brief Report who is mailed about the job, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_mail_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   ocs::tom::ConstList<ocs::tom::MR> mail_list = job.get<JB_mail_list>();
   if (mail_list == nullptr) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"mail_list\": [";
   indent++;

   bool is_first = true;
   for (ocs::tom::ConstElement<ocs::tom::MR> elem : mail_list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *user = elem.get<MR_user>();
      os << std::string(indent * 3, ' ') << "\"user\": " << raw2quotedJSON(user ? user : "") << ",\n";
      const char *host = elem.get<MR_host>();
      os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Report whether the job is warned before it is signalled, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_notify(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_boolean<JB_notify>(os, job, "notify");
   DRETURN_VOID;
}

/**
 * @brief Report the job name, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_name(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_job_name>(os, job, "job_name");
   DRETURN_VOID;
}

/**
 * @brief Report the priority the job was submitted with, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_priority(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"priority\": " << (
      static_cast<int>(job.get<JB_priority>()) - BASE_PRIORITY);
   DRETURN_VOID;
}

/**
 * @brief Report the share the job was submitted with, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_job_share(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_jobshare>(os, job, "job_share");
   DRETURN_VOID;
}

/**
 * @brief Report whether the job may be restarted after a failure, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_restart(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"restart\": " << (job.get<JB_restart>() == 2 ? "false" : "true");
   DRETURN_VOID;
}

/**
 * @brief Report the shell the job runs under, per host, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_shell_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::PN> shell_list = job.get<JB_shell_list>();
   if (shell_list == nullptr) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"shell_list\": [";
   indent++;

   bool is_first = true;
   for (ocs::tom::ConstElement<ocs::tom::PN> elem : shell_list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *user = elem.get<PN_path>();
      os << std::string(indent * 3, ' ') << "\"path\": " << raw2quotedJSON(user ? user : "") << ",\n";
      const char *host = elem.get<PN_host>();
      os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Report the verification mode the job was submitted with, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_verify(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_verify>(os, job, "verify");
   DRETURN_VOID;
}

/**
 * @brief Report the arguments passed to the job script, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_job_args(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::ST> list = job.get<JB_job_args>();
   if (list == nullptr) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"job_args\": [";
   indent++;

   bool is_first = true;
   for (ocs::tom::ConstElement<ocs::tom::ST> elem : list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const char *arg = elem.get<ST_name>();
      os << std::string(indent * 3, ' ') << raw2quotedJSON(arg ? arg : "");
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Write one string field of the elements of a list of the job as an array member of the job object
 *
 * Nothing is written if the (reduced) job does not hold the list or has no
 * such list.
 *
 * @param os stream to write to
 * @param job the job
 * @tparam LF the list field of `JB`
 * @tparam VF the string field of the list's elements; its type is the type of the elements
 * @param name the name of the member
 */
template <auto LF, auto VF>
void ocs::QStatJobViewJSON::report_X_string_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                 const char *name) {
   DENTER(TOP_LAYER);
   using Elem = typename ocs::tom::FieldTraits<VF>::Type;
   if (!job.has<LF>()) {
      DRETURN_VOID;
   }
   // the list field gives a handle of its sub-type; the checked handle confirms it is the element type of VF
   const auto list = ocs::tom::ConstList<Elem>(job.get<LF>());
   if (list == nullptr) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<Elem> elem : list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const char *value = elem.template get<VF>();
      os << std::string(indent * 3, ' ') << raw2quotedJSON(value ? value : "");
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Write one 32 bit number field of the elements of a job reference list as an array member of the job object
 *
 * Nothing is written if the (reduced) job does not hold the list or has no
 * such list.
 *
 * @param os stream to write to
 * @param job the job
 * @tparam LF the list field of `JB`, a job reference list (`JRE`)
 * @tparam VF the `uint32_t` field of `JRE`
 * @param name the name of the member
 */
template <auto LF, auto VF>
void ocs::QStatJobViewJSON::report_X_uint32_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                 const char *name) {
   DENTER(TOP_LAYER);
   if (!job.has<LF>()) {
      DRETURN_VOID;
   }
   const ocs::tom::ConstList<ocs::tom::JRE> list = job.get<LF>();
   if (list == nullptr) {
      DRETURN_VOID;
   }

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<ocs::tom::JRE> elem : list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const uint32_t value = elem.get<VF>();
      os << std::string(indent * 3, ' ') << value;
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}


/**
 * @brief Report the tasks of an array job that were selected, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_job_identifier_list(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string_list<JB_job_identifier_list, ID_str>(os, job, "job_identifier_list");
   DRETURN_VOID;
}

/**
 * @brief Report the size of the spooled job script, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_script_size(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_script_size>(os, job, "script_size");
   DRETURN_VOID;
}

/**
 * @brief Report the name of the job script, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_script_file(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_script_file>(os, job, "script_file");
   DRETURN_VOID;
}

/**
 * @brief Report the job script itself, for a script read from standard input, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_script_ptr(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_script_ptr>(os, job, "script_ptr");
   DRETURN_VOID;
}

/**
 * @brief Report the parallel environment and the slot range requested, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_pe(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (!job.has<JB_pe>()) {
      DRETURN_VOID;
   }
   const char *pe_name = job.get<JB_pe>();
   if (pe_name == nullptr) {
      DRETURN_VOID;
   }
   dstring range_string = DSTRING_INIT;
   range_list_print_to_string(job.get<JB_pe_range>(), &range_string, true, false, false);

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"pe\": {\n";
   indent++;

   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(pe_name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"range\": " << raw2quotedJSON(sge_dstring_get_string(&range_string)) << "\n";

   indent--;
   os << std::string(indent * 3, ' ') << "}";

   sge_dstring_free(&range_string);
   DRETURN_VOID;
}

/**
 * @brief Report the jobs this one was asked to wait for, as they were named, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_jid_request_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string_list<JB_jid_request_list, JRE_job_name>(os, job, "jid_request_list");
   DRETURN_VOID;
}

/**
 * @brief Report the jobs this one is still waiting for, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_jid_predecessor_list(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list<JB_jid_predecessor_list, JRE_job_number>(os, job, "jid_predecessor_list");
   DRETURN_VOID;
}

/**
 * @brief Report the jobs waiting for this one, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_jid_successor_list(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list<JB_jid_successor_list, JRE_job_number>(os, job, "jid_successor_list");
   DRETURN_VOID;
}

/**
 * @brief Report the array dependencies as they were named, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ja_ad_request_list(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string_list<JB_ja_ad_request_list, JRE_job_name>(os, job, "ja_ad_request_list");
   DRETURN_VOID;
}

/**
 * @brief Report the array jobs whose tasks this one is waiting for, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ja_ad_predecessor_list(std::ostream &os,
                                                          const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list<JB_ja_ad_predecessor_list, JRE_job_number>(os, job, "ja_ad_predecessor_list");
   DRETURN_VOID;
}

/**
 * @brief Report the array jobs whose tasks wait for this one, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ja_ad_successor_list(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list<JB_ja_ad_successor_list, JRE_job_number>(os, job, "ja_ad_successor_list");
   DRETURN_VOID;
}

/**
 * @brief Report which queues the job could run in, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_verify_suitable_queues(std::ostream &os,
                                                          const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_verify_suitable_queues>(os, job, "verify_suitable_queues");
   DRETURN_VOID;
}

/**
 * @brief Report the soft wallclock limit, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_soft_wallclock_gmt(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp<JB_soft_wallclock_gmt>(os, job, "soft_wallclock_gmt");
   DRETURN_VOID;
}

/**
 * @brief Report the hard wallclock limit, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_hard_wallclock_gmt(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp<JB_hard_wallclock_gmt>(os, job, "hard_wallclock_gmt");
   DRETURN_VOID;
}

/**
 * @brief Report the job version, which counts how often the job was modified, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_version(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_version>(os, job, "version");
   DRETURN_VOID;
}

/**
 * @brief Report the tickets a manager granted the job, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_override_tickets(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_override_tickets>(os, job, "override_tickets");
   DRETURN_VOID;
}

/**
 * @brief Report the advance reservation the job runs in, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ar(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_ar>(os, job, "ar_id");
   DRETURN_VOID;
}

/**
 * @brief Report the project the job is accounted to, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_project(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_project>(os, job, "project");
   DRETURN_VOID;
}

/**
 * @brief Report the department the submitting user belongs to, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_department(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_string<JB_department>(os, job, "department");
   DRETURN_VOID;
}

/**
 * @brief Report whether the client waits for the job, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_sync_options(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (!job.has<JB_sync_options>()) {
      DRETURN_VOID;
   }
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   const std::string sync_flags = job_get_sync_options_string(job);
   os << std::string(indent * 3, ' ') << "\"sync_options\": " << raw2quotedJSON(sync_flags);
   DRETURN_VOID;
}

/**
 * @brief Report the task range of an array job, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ja_structure(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (!job.has<JB_ja_structure>()) {
      DRETURN_VOID;
   }
   const auto [start, end, step] = job_get_submit_task_ids(job);

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"job_array_tasks\": {\n";
   indent++;

   os << std::string(indent * 3, ' ') << "\"start\": " << start << ",\n";
   os << std::string(indent * 3, ' ') << "\"end\": " << end << ",\n";
   os << std::string(indent * 3, ' ') << "\"step\": " << step << "\n";

   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

/**
 * @brief Report how many tasks of an array job may run at once, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ja_task_concurrency(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_uint32<JB_ja_task_concurrency>(os, job, "task_concurrency");
   DRETURN_VOID;
}

/**
 * @brief Report how many tasks of an array job have not started yet, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_pending_tasks(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (!job.has<JB_ja_n_h_ids>()) {
      DRETURN_VOID;
   }
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"pending_tasks\": " << count_pending_tasks(job);
   DRETURN_VOID;
}

/**
 * @brief Emit a job name/value list (env_list or context) as a JSON array.
 *
 * Shared helper used by report_env_list (nm == JB_env_list) and report_ctx_list
 * (nm == JB_context).
 *
 * SECURITY (CS-2355, MEDIUM-QSTAT-001): when called for JB_env_list this emits
 * the full captured job environment unredacted, the JSON counterpart of the
 * plain-view disclosure (QStatJobViewPlain::report_env_list). Any redaction
 * policy decided in CS-2355 for the job environment must be applied here for the
 * JSON view; none is applied yet.
 *
 * @param[in,out] os   output stream
 * @param[in]     job  job element to report
 * @tparam        F    field to emit (JB_env_list or JB_context)
 * @param[in]     name JSON key name for the emitted array
 */
template <auto F>
void ocs::QStatJobViewJSON::report_X_env_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                              const char *name) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::VA> job_env_list = job.get<F>();
   if (!job_env_list) {
      DRETURN_VOID;
   }

   // the variables with VAR_PREFIX are not shown: they are split off a copy, since the job is the caller's
   const auto env_list = job_env_list.clone();
   ocs::tom::ListPtr<ocs::tom::VA> do_not_print;
   var_list_split_prefix_vars(*env_list, do_not_print, VAR_PREFIX);

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<ocs::tom::VA> elem : *env_list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *user = elem.get<VA_variable>();
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(user ? user : "");
      if (const char *host = elem.get<VA_value>(); host != nullptr) {
         os << ",\n" << std::string(indent * 3, ' ') << "\"value\": " << raw2quotedJSON(host ? host : "") << "\n";
      } else {
         os << "\n";
      }

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Emit the job's environment (JB_env_list) in JSON qstat -j output.
 *
 * SECURITY (CS-2355, MEDIUM-QSTAT-001): unredacted job-environment disclosure,
 * the JSON counterpart of QStatJobViewPlain::report_env_list. The actual
 * emission happens in report_X_env_list(); the pending redaction decision is
 * tracked in CS-2355.
 *
 * @param[in,out] os  output stream
 * @param[in]     job job element to report
 */
void ocs::QStatJobViewJSON::report_env_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_env_list<JB_env_list>(os, job, "env_list");
   DRETURN_VOID;
}

/**
 * @brief Report the job context, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_ctx_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   report_X_env_list<JB_context>(os, job, "context");
   DRETURN_VOID;
}

/**
 * @brief Report the requested core binding, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_binding(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (!job.has<JB_binding>()) {
      DRETURN_VOID;
   }
   ocs::tom::ConstElement<ocs::tom::BN> binding_elem = job.get<JB_binding>();
   if (binding_elem == nullptr) {
      DRETURN_VOID;
   }
   const uint32_t amount = binding_elem.get<BN_amount>();
   const auto instance = static_cast<BindingInstance::Instance>(binding_elem.get<BN_instance>());
   const char *sort = binding_elem.get<BN_sort>();
   const auto start = static_cast<BindingStart::Start>(binding_elem.get<BN_start>());
   const auto stop = static_cast<BindingStop::Stop>(binding_elem.get<BN_stop>());
   const auto strategy = static_cast<BindingStrategy::Strategy>(binding_elem.get<BN_strategy>());
   const auto type = static_cast<BindingType::Type>(binding_elem.get<BN_new_type>());
   const auto unit = static_cast<BindingUnit::Unit>(binding_elem.get<BN_unit>());
   const auto filter = binding_elem.get<BN_filter>();

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"binding\": {\n";
   indent++;

   os << std::string(indent * 3, ' ') << "\"bstrategy\": " << raw2quotedJSON(BindingStrategy::to_string(strategy)) <<
         ",\n";
   os << std::string(indent * 3, ' ') << "\"amount\": " << amount << ",\n";
   os << std::string(indent * 3, ' ') << "\"bunit\": " << raw2quotedJSON(BindingUnit::to_string(unit)) << ",\n";
   os << std::string(indent * 3, ' ') << "\"btype\": " << raw2quotedJSON(BindingType::to_string(type)) << ",\n";
   os << std::string(indent * 3, ' ') << "\"bfilter\": " << raw2quotedJSON(filter ? filter : "") << ",\n";
   os << std::string(indent * 3, ' ') << "\"bsort\": " << raw2quotedJSON(sort ? sort : "") << ",\n";
   os << std::string(indent * 3, ' ') << "\"bstart\": " << raw2quotedJSON(BindingStart::to_string(start)) << ",\n";
   os << std::string(indent * 3, ' ') << "\"bstop\": " << raw2quotedJSON(BindingStop::to_string(stop)) << ",\n";
   os << std::string(indent * 3, ' ') << "\"binstance\": " << raw2quotedJSON(BindingInstance::to_string(instance)) <<
         "\n";

   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

/**
 * @brief Begin the tasks of the current job, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_task_list_started(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"array_tasks\": [";
   indent++;
   first_task = true;
   first_task_attribute = true;
   DRETURN_VOID;
}

/**
 * @brief End the task list, as JSON
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_task_list_finished(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Begin one task, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_started(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   if (first_task) {
      os << "\n";
      first_task = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   first_task_attribute = true;
   DRETURN_VOID;
}

/**
 * @brief End the current task, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_finished(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                 const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   first_task_attribute = true;
   DRETURN_VOID;
}

/**
 * @brief Report the task number, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                           const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   const uint32_t task_id = task.get<JAT_task_number>();
   os << std::string(indent * 3, ' ') << "\"task_id\": " << task_id;
   DRETURN_VOID;
}

/**
 * @brief Report the state of the task, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_state(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                              const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);

   char state_string[8];
   const uint32_t state = jatask_combine_state_and_status_for_output(job, task);
   job_get_state_string(state_string, state);

   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"job_state\": " << raw2quotedJSON(state_string);
   DRETURN_VOID;
}

/**
 * @brief Report the resources the task has consumed, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_usage(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                              const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   /* jobs whose job start orders were not processed to the end due to a qmaster/schedd collision appear
    * in the JB_ja_tasks list but are not running - thus we may not print usage for those */
   if (task.get<JAT_status>() != JRUNNING && task.get<JAT_status>() != JTRANSFERING) {
      DRETURN_VOID;
   }

   // Collect data
   Usage usage = {};
   accumulate_usage(task, usage);

   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"usage\": {\n";
   indent++;

   os << std::string(indent * 3, ' ') << "\"wallclock\": " << usage.wallclock;
   os << ",\n" << std::string(indent * 3, ' ') << "\"cpu\": " << usage.cpu;
   os << ",\n" << std::string(indent * 3, ' ') << "\"mem\": " << usage.mem;
   os << ",\n" << std::string(indent * 3, ' ') << "\"io\": " << usage.io;
   os << ",\n" << std::string(indent * 3, ' ') << "\"ioops\": " << usage.ioops;
   os << ",\n" << std::string(indent * 3, ' ') << "\"iow\": " << usage.iow;
   os << ",\n" << std::string(indent * 3, ' ') << "\"vmem\": " << usage.vmem;
   os << ",\n" << std::string(indent * 3, ' ') << "\"maxvmem\": " << usage.maxvmem;
   os << ",\n" << std::string(indent * 3, ' ') << "\"maxrss\": " << usage.maxrss;
   if (usage.have_mem_details) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"pss\": " << usage.pss;
      os << ",\n" << std::string(indent * 3, ' ') << "\"maxpss\": " << usage.maxpss;
      os << ",\n" << std::string(indent * 3, ' ') << "\"pmem\": " << usage.pmem;
      os << ",\n" << std::string(indent * 3, ' ') << "\"smem\": " << usage.smem;
   }
   os << "\n";

   indent--;
   os << std::string(indent * 3, ' ') << "}";

   DRETURN_VOID;
}

/**
 * @brief Report the cores the task was bound to, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_exec_binding_list(std::ostream &os,
                                                          const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                          const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::GRU> list = task.get<JAT_granted_resources_list>();
   if (list == nullptr) {
      DRETURN_VOID;
   }

   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"exec_binding_list\": [";
   indent++;

   bool is_first = true;
   for (ocs::tom::ConstElement<ocs::tom::GRU> elem : list) {
      if (const char *name = elem.get<GRU_name>(); name == nullptr || strcmp(name, SGE_ATTR_SLOTS) != 0) {
         continue;
      }

      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *hostname = elem.get<GRU_host>();
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(hostname ? hostname : "") << ",\n";
      os << std::string(indent * 3, ' ') << "\"binding\": [\n";
      indent++;
      ocs::tom::ConstList<ocs::tom::ST> binding_in_use_list = elem.get<GRU_binding_inuse>();
      bool first_binding = true;
      for (ocs::tom::ConstElement<ocs::tom::ST> binding_in_use : binding_in_use_list) {
         if (first_binding) {
            first_binding = false;
         } else {
            os << ",\n";
         }
         TopologyString topo_in_use(binding_in_use.get<ST_name>());
         os << std::string(indent * 3, ' ') << "\"" << topo_in_use.to_product_topology_string() << "\"";
      }
      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "]";
      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Write a granted destination list of an array task as an array of objects with a string and a number
 * @param os stream to write to
 * @param task the array task the row is about
 * @tparam F the field, a granted destination list field (`JG`) of `JAT`
 * @tparam SF the string field of `JG`
 * @tparam UF the `uint32_t` field of `JG`
 * @param name the name of the member
 * @param str_name the name of the string member of each object
 * @param uint32_name the name of the number member of each object
 */
template <auto F, auto SF, auto UF>
void ocs::QStatJobViewJSON::report_X_task_str_uint32_list(std::ostream &os,
                                                          const ocs::tom::ConstElement<ocs::tom::JAT> task,
                                                          const char *name, const char *str_name,
                                                          const char *uint32_name) {
   DENTER(TOP_LAYER);
   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<ocs::tom::JG> elem : ocs::tom::ConstList<ocs::tom::JG>(task.get<F>())) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *string = elem.get<SF>();
      os << std::string(indent * 3, ' ') << "\"" << str_name << "\": " << raw2quotedJSON(string ? string : "") << ",\n";
      const uint32_t value = elem.get<UF>();
      os << std::string(indent * 3, ' ') << "\"" << uint32_name << "\": " << value << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Report the queue instances the task runs in, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_exec_queue_list(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                        const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   report_X_task_str_uint32_list<JAT_granted_destin_identifier_list, JG_qname, JG_slots>(
      os, task, "granted_queue_list", "queue_name", "slots");
   DRETURN_VOID;
}

/**
 * @brief Report the hosts the task runs on, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_exec_host_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                       const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);

   if (!task.has<JAT_granted_destin_identifier_list>()) {
      DRETURN_VOID;
   }

   ocs::tom::ConstList<ocs::tom::JG> gdil_org = task.get<JAT_granted_destin_identifier_list>();
   const auto gdil_unique = gdil_make_host_unique(gdil_org);

   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"exec_host_list\": [";
   indent++;

   bool is_first = true;
   for (const ocs::tom::ConstElement<ocs::tom::JG> elem : *gdil_unique) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *string = elem.get<JG_qhostname>();
      os << std::string(indent * 3, ' ') << "\"hostname\": " << raw2quotedJSON(string ? string : "") << ",\n";
      const uint32_t value = elem.get<JG_slots>();
      os << std::string(indent * 3, ' ') << "\"slots\": " << value << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";

   DRETURN_VOID;
}

/**
 * @brief Report when the task started, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_start_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                   const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   report_X_task_ISO_8601_timestamp<JAT_start_time>(os, task, "start_time");
   DRETURN_VOID;
}

/**
 * @brief Report when the task finished, as JSON
 *
 * CS-1908 retention: emit end_time only for retained finished ja_tasks.
 *
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_end_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                 const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   if (task.get<JAT_status>() != JFINISHED) {
      DRETURN_VOID;
   }
   report_X_task_ISO_8601_timestamp<JAT_end_time>(os, task, "end_time");
   DRETURN_VOID;
}

/**
 * @brief Report the resource map ids the task was granted, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_resource_map(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                     const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"resource_map\": [";
   indent++;

   /* go through all RSMAP resources for the particular task and create a string */
   bool is_first_remap = true;
   for (ocs::tom::ConstElement<ocs::tom::GRU> resu : task.get<JAT_granted_resources_list>()) {
      if (static_cast<GrantedResources::Type>(resu.get<GRU_type>()) ==
          GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
         const char *name = resu.get<GRU_name>();
         const char *host = resu.get<GRU_host>();
         ocs::tom::ConstList<ocs::tom::RESL> ids = resu.get<GRU_resource_map_list>();

         if (name != nullptr && ids != nullptr && host != nullptr) {
            if (is_first_remap) {
               is_first_remap = false;
               os << "\n";
            } else {
               os << ",\n";
            }
            os << std::string(indent * 3, ' ') << "{\n";
            indent++;

            os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
            os << std::string(indent * 3, ' ') << "\"hostname\": " << raw2quotedJSON(host) << ",\n";
            os << std::string(indent * 3, ' ') << "\"rsmap_ids\": [";
            indent++;
            u_int32_t total_amount = 0;
            bool first_id = true;
            for (ocs::tom::ConstElement<ocs::tom::RESL> id : ids) {
               const char *id_string = id.get<RESL_value>();
               const uint32_t amount = id.get<RESL_amount>();

               total_amount += amount;
               for (uint32_t i = 0; i < amount; i++) {
                  if (first_id) {
                     os << "\n";
                     first_id = false;
                  } else {
                     os << ",\n";
                  }
                  os << std::string(indent * 3, ' ') << raw2quotedJSON(id_string ? id_string : "");
               }
            }
            indent--;
            os << "\n" << std::string(indent * 3, ' ') << "],";
            os << "\n" << std::string(indent * 3, ' ') << "\"total_amount\": " << total_amount;
            indent--;
            os << "\n" << std::string(indent * 3, ' ') << "}";
         }
      }
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

/**
 * @brief Report why the task failed, as JSON
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewJSON::report_task_error_reason(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                     const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"error_reason\": [";
   indent++;

   bool is_first = true;
   for (ocs::tom::ConstElement<ocs::tom::QIM> elem : task.get<JAT_message_list>()) {
      if (const char *message = elem.get<QIM_message>()) {
         if (is_first) {
            os << "\n";
            is_first = false;
         } else {
            os << ",\n";
         }
         os << std::string(indent * 3, ' ') << raw2quotedJSON(message);
      }
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";

   DRETURN_VOID;
}

/**
 * @brief Report why the job has not been scheduled yet, as JSON
 * @param os stream to write to
 * @param ilp the scheduler job info list (`SME`), holding the reasons a job is not running
 * @param job the job
 */
void ocs::QStatJobViewJSON::report_schedd_job_info(std::ostream &os, const ocs::tom::ConstList<ocs::tom::SME> ilp,
                                                   const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (const ocs::tom::ConstElement<ocs::tom::SME> sme = ilp.front(); sme != nullptr) {
      // Local lambda to print a message with correct prefix/indent
      bool first_run = true;
      auto print_sched_message = [&](const char *msg) {
         os << ",\n";
         if (first_run) {
            os << std::string(indent * 3, ' ') << "\"scheduling_info\": [\n";
            indent++;
            first_run = false;
         }
         os << std::string(indent * 3, ' ') << raw2quotedJSON(msg ? msg : "");
      };

      /* global scheduling info */
      for (ocs::tom::ConstElement<ocs::tom::MES> mes : sme.get<SME_global_message_list>()) {
         print_sched_message(mes.get<MES_message>());
      }

      /* job scheduling info */
      const uint32_t jid = job.get<JB_job_number>();
      for (ocs::tom::ConstElement<ocs::tom::MES> mes : sme.get<SME_message_list>()) {
         for (ocs::tom::ConstElement<ocs::tom::ULNG> mes_jid : mes.get<MES_job_number_list>()) {
            if (mes_jid.get<ULNG_value>() == jid) {
               print_sched_message(mes.get<MES_message>());
            }
         }
      }
      if (!first_run) {
         indent--;
         os << "\n" << std::string(indent * 3, ' ') << "]";
      }
   }
   DRETURN_VOID;
}
