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

#include <sstream>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

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

void ocs::QStatJobViewJSON::report_X_uint32(std::ostream &os, const lListElem *job, const int nm, const char *name) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, nm, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": " << lGetUlong(job, nm);
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_job_id(std::ostream &os, const lListElem *job, int flags) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_job_number, "job_number");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_category_id(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_category_id, "category_id");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_string(std::ostream &os, const lListElem *job, const int nm, const char *name) {
   DENTER(TOP_LAYER);
   if (const char *value = lGetString(job, nm); value != nullptr) {
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

void ocs::QStatJobViewJSON::report_exec_file(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_exec_file, "exec_file");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_ISO_8601_timestamp(std::ostream &os, const lListElem *job, const int nm,
                                                        const char *name) {
   DENTER(TOP_LAYER);
   const uint64_t sec = lGetUlong64(job, nm);
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

void ocs::QStatJobViewJSON::report_X_task_ISO_8601_timestamp(std::ostream &os, const lListElem *job, const int nm,
                                                             const char *name) {
   DENTER(TOP_LAYER);
   const uint64_t sec = lGetUlong64(job, nm);
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

void ocs::QStatJobViewJSON::report_submission_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp(os, job, JB_submission_time, "submission_time");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_deadline_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp(os, job, JB_deadline, "deadline_time");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_submit_cmd_line(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_submission_command_line, "submit_cmd_line");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_effective_submit_cmd_line(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   const char *str = lGetString(job, JB_submission_command_line);
   if (str == nullptr) {
      DRETURN_VOID;
   }

   char *copied_str = strdup(str);
   char *saveptr = nullptr;
   if (const char *command = strtok_r(copied_str, " ", &saveptr); command != nullptr) {
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
   sge_free(&copied_str);
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_ownership(std::ostream &os, const lListElem *job) {
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
      lGetString(job, JB_owner) ? lGetString(job, JB_owner) : "") << ",\n";
   os << std::string(indent * 3, ' ') << "\"uid\": " << lGetUlong(job, JB_uid) << ",\n";
   os << std::string(indent * 3, ' ') << "\"group\": " << raw2quotedJSON(
      lGetString(job, JB_group) ? lGetString(job, JB_group) : "") << ",\n";
   os << std::string(indent * 3, ' ') << "\"gid\": " << lGetUlong(job, JB_gid) << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "},\n";
   os << std::string(indent * 3, ' ') << "\"supplementary\": [\n";
   indent++;
#if defined(WITH_EXTENSIONS)
   const lList *grp_list = lGetList(job, JB_grp_list);
   bool first = true;
   for_each_ep_lv(grp_elem, grp_list) {
      if (first) {
         first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(
         lGetString(grp_elem, ST_name) ? lGetString(grp_elem, ST_name) : "") << ",\n";
      os << std::string(indent * 3, ' ') << "\"id\": " << lGetUlong(grp_elem, ST_id) << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }
#endif
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
}

void ocs::QStatJobViewJSON::report_env_core(std::ostream &os, const lListElem *job) {
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

void ocs::QStatJobViewJSON::report_execution_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp(os, job, JB_execution_time, "execution_time");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_account(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_account, "account");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_checkpoint(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   const char *ckpt_name = lGetString(job, JB_checkpoint_name);
   const uint32_t ckpt_attr = lGetUlong(job, JB_checkpoint_attr);
   const uint32_t ckpt_int = lGetUlong(job, JB_checkpoint_interval);

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

void ocs::QStatJobViewJSON::report_cwd(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_cwd, "cwd");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_path_aliases(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const lList *path_aliases = lGetList(job, JB_path_aliases)) {
      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "\"path_aliases\": [";
      indent++;

      bool is_first = true;
      for_each_ep_lv(pa, path_aliases) {
         if (is_first) {
            os << "\n";
            is_first = false;
         } else {
            os << ",\n";
         }
         os << std::string(indent * 3, ' ') << "{\n";
         indent++;
         const char *src_path = lGetString(pa, PA_origin);
         os << std::string(indent * 3, ' ') << "\"src_path\": " << raw2quotedJSON(src_path ? src_path : "") << ",\n";
         const char *submit_host = lGetHost(pa, PA_submit_host);
         os << std::string(indent * 3, ' ') << "\"submit_host\": " << raw2quotedJSON(submit_host ? submit_host : "") <<
               ",\n";
         const char *exec_host = lGetHost(pa, PA_exec_host);
         os << std::string(indent * 3, ' ') << "\"exec_host\": " << raw2quotedJSON(exec_host ? exec_host : "") << ",\n";
         const char *dst_path = lGetString(pa, PA_translation);
         os << std::string(indent * 3, ' ') << "\"dst_path\": " << raw2quotedJSON(dst_path ? dst_path : "") << "\n";
         indent--;
         os << std::string(indent * 3, ' ') << "}";
      }

      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "]";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_directive_prefix(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_directive_prefix, "directive_prefix");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_path_list(std::ostream &os, const lListElem *job, const int nm, const char *name) {
   DENTER(TOP_LAYER);
   if (const lList *list = lGetList(job, nm); list != nullptr) {
      if (first_attribute) {
         os << "\n";
         first_attribute = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
      indent++;

      bool is_first = true;
      for_each_ep_lv(elem, list) {
         if (is_first) {
            os << "\n";
            is_first = false;
         } else {
            os << ",\n";
         }
         os << std::string(indent * 3, ' ') << "{\n";
         indent++;

         const char *host = lGetHost(elem, PN_host);
         os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << ",\n";

         const char *file_host = lGetHost(elem, PN_file_host);
         os << std::string(indent * 3, ' ') << "\"file_host\": " << raw2quotedJSON(file_host ? file_host : "") << ",\n";

         const char *path = lGetString(elem, PN_path);
         os << std::string(indent * 3, ' ') << "\"path\": " << raw2quotedJSON(path ? path : "") << ",\n";

         const bool file_staging = lGetBool(elem, PN_file_staging);
         os << std::string(indent * 3, ' ') << "\"file_staging\": " << (file_staging ? "true" : "false") << "\n";

         indent--;
         os << std::string(indent * 3, ' ') << "}";
      }

      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "]";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_stdin_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_path_list(os, job, JB_stdin_path_list, "stdin_path_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_stdout_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_path_list(os, job, JB_stdout_path_list, "stdout_path_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_stderr_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_path_list(os, job, JB_stderr_path_list, "stderr_path_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_boolean(std::ostream &os, const lListElem *job, const int nm, const char *name) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   const bool value = lGetBool(job, nm);
   os << std::string(indent * 3, ' ') << "\"" << name << "\": " << (value ? "true" : "false");
   DRETURN_VOID;
}


void ocs::QStatJobViewJSON::report_reserve(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_boolean(os, job, JB_reserve, "reserve");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_merge_stderr(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_boolean(os, job, JB_merge_stderr, "merge_stderr");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_resource_list(std::ostream &os, const lListElem *jrs, const int nm,
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
   for_each_ep_lv(elem, lGetList(jrs, nm)) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *resource_name = lGetString(elem, CE_name);
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

void ocs::QStatJobViewJSON::report_X_queue_list(std::ostream &os, const lListElem *jrs, const int nm,
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
   for_each_ep_lv(elem, lGetList(jrs, nm)) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const char *qname = lGetString(elem, QR_name);
      os << std::string(indent * 3, ' ') << raw2quotedJSON(qname ? qname : "");
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_request_set_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   for_each_ep_lv(jrs, lGetList(job, JB_request_set_list)) {
      const uint32_t scope = lGetUlong(jrs, JRS_scope);

      if (lGetList(jrs, JRS_hard_resource_list) != nullptr) {
         std::string list_name = get_scope_list_name(scope, JRS_hard_resource_list);
         report_X_resource_list(os, jrs, JRS_hard_resource_list, list_name.c_str());
      }

      if (lGetList(jrs, JRS_soft_resource_list) != nullptr) {
         std::string list_name = get_scope_list_name(scope, JRS_soft_resource_list);
         report_X_resource_list(os, jrs, JRS_soft_resource_list, list_name.c_str());
      }

      if (lGetList(jrs, JRS_hard_queue_list) != nullptr) {
         std::string list_name = get_scope_list_name(scope, JRS_hard_queue_list);
         report_X_queue_list(os, jrs, JRS_hard_queue_list, list_name.c_str());
      }

      if (lGetList(jrs, JRS_soft_queue_list) != nullptr) {
         std::string list_name = get_scope_list_name(scope, JRS_soft_queue_list);
         report_X_queue_list(os, jrs, JRS_soft_queue_list, list_name.c_str());
      }

      if (lGetString(jrs, JRS_allocation_rule) != nullptr) {
         std::string name = get_scope_list_name(scope, JRS_allocation_rule);
         report_X_string(os, jrs, JRS_allocation_rule, name.c_str());
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_mail_options(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const uint32_t mail_options = lGetUlong(job, JB_mail_options)) {
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

void ocs::QStatJobViewJSON::report_mail_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   const lList *mail_list = lGetList(job, JB_mail_list);
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
   for_each_ep_lv(elem, mail_list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *user = lGetString(elem, MR_user);
      os << std::string(indent * 3, ' ') << "\"user\": " << raw2quotedJSON(user ? user : "") << ",\n";
      const char *host = lGetHost(elem, MR_host);
      os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_notify(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_boolean(os, job, JB_notify, "notify");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_name(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_job_name, "job_name");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_priority(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"priority\": " << (
      static_cast<int>(lGetUlong(job, JB_priority)) - BASE_PRIORITY);
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_job_share(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_jobshare, "job_share");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_restart(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"restart\": " << (lGetUlong(job, JB_restart) == 2 ? "false" : "true");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_shell_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);

   const lList *shell_list = lGetList(job, JB_shell_list);
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
   for_each_ep_lv(elem, shell_list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *user = lGetString(elem, PN_path);
      os << std::string(indent * 3, ' ') << "\"path\": " << raw2quotedJSON(user ? user : "") << ",\n";
      const char *host = lGetHost(elem, PN_host);
      os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_verify(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_verify, "verify");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_job_args(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);

   const lList *list = lGetList(job, JB_job_args);
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
   for_each_ep_lv(elem, list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const char *arg = lGetString(elem, ST_name);
      os << std::string(indent * 3, ' ') << raw2quotedJSON(arg ? arg : "");
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_string_list(std::ostream &os, const lListElem *job, const int list_nm,
                                                 const int value_nm, const char *name) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, list_nm, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }
   const lList *list = lGetList(job, list_nm);
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
   for_each_ep_lv(elem, list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const char *value = lGetString(elem, value_nm);
      os << std::string(indent * 3, ' ') << raw2quotedJSON(value ? value : "");
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_uint32_list(std::ostream &os, const lListElem *job, const int list_nm,
                                                 const int value_nm, const char *name) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, list_nm, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }
   const lList *list = lGetList(job, list_nm);
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
   for_each_ep_lv(elem, list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      const uint32_t value = lGetUlong(elem, value_nm);
      os << std::string(indent * 3, ' ') << value;
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}


void ocs::QStatJobViewJSON::report_job_identifier_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string_list(os, job, JB_job_identifier_list, JRE_job_number, "job_identifier_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_script_size(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_script_size, "script_size");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_script_file(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_script_file, "script_file");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_script_ptr(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_script_ptr, "script_ptr");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_pe(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_pe, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }
   const char *pe_name = lGetString(job, JB_pe);
   if (pe_name == nullptr) {
      DRETURN_VOID;
   }
   dstring range_string = DSTRING_INIT;
   range_list_print_to_string(lGetList(job, JB_pe_range), &range_string, true, false, false);

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

void ocs::QStatJobViewJSON::report_jid_request_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string_list(os, job, JB_jid_request_list, JRE_job_name, "jid_request_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_jid_predecessor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list(os, job, JB_jid_predecessor_list, JRE_job_number, "jid_predecessor_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_jid_successor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list(os, job, JB_jid_successor_list, JRE_job_number, "jid_successor_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_ja_ad_request_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string_list(os, job, JB_ja_ad_request_list, JRE_job_name, "ja_ad_request_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_ja_ad_predecessor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list(os, job, JB_ja_ad_predecessor_list, JRE_job_number, "ja_ad_predecessor_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_ja_ad_successor_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32_list(os, job, JB_ja_ad_successor_list, JRE_job_number, "ja_ad_successor_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_verify_suitable_queues(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_verify_suitable_queues, "verify_suitable_queues");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_soft_wallclock_gmt(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp(os, job, JB_soft_wallclock_gmt, "soft_wallclock_gmt");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_hard_wallclock_gmt(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_ISO_8601_timestamp(os, job, JB_hard_wallclock_gmt, "hard_wallclock_gmt");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_version(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_version, "version");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_override_tickets(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_override_tickets, "override_tickets");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_ar(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_ar, "ar_id");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_project(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_project, "project");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_department(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_string(os, job, JB_department, "department");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_sync_options(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_sync_options, SGE_NO_ABORT) < 0) {
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

void ocs::QStatJobViewJSON::report_ja_structure(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_ja_structure, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }
   uint32_t start, end, step;
   job_get_submit_task_ids(job, &start, &end, &step);

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

void ocs::QStatJobViewJSON::report_ja_task_concurrency(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_uint32(os, job, JB_ja_task_concurrency, "task_concurrency");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_pending_tasks(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_ja_n_h_ids, SGE_NO_ABORT) < 0) {
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
 * @param[in]     nm   CULL field to emit (JB_env_list or JB_context)
 * @param[in]     name JSON key name for the emitted array
 */
void ocs::QStatJobViewJSON::report_X_env_list(std::ostream &os, const lListElem *job, int nm, const char *name) {
   DENTER(TOP_LAYER);

   lList *env_list = lGetListRW(job, nm);
   if (env_list == nullptr) {
      DRETURN_VOID;
   }

   lList *do_not_print = nullptr;
   var_list_split_prefix_vars(&env_list, &do_not_print, VAR_PREFIX);

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"" << name << "\": [";
   indent++;

   bool is_first = true;
   for_each_ep_lv(elem, env_list) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *user = lGetString(elem, VA_variable);
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(user ? user : "");
      if (const char *host = lGetString(elem, VA_value); host != nullptr) {
         os << ",\n" << std::string(indent * 3, ' ') << "\"value\": " << raw2quotedJSON(host ? host : "") << "\n";
      } else {
         os << "\n";
      }

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   lAddList(env_list, &do_not_print);

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
void ocs::QStatJobViewJSON::report_env_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_env_list(os, job, JB_env_list, "env_list");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_ctx_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   report_X_env_list(os, job, JB_context, "context");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_binding(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_binding, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }
   const lListElem *binding_elem = lGetObject(job, JB_binding);
   if (binding_elem == nullptr) {
      DRETURN_VOID;
   }
   const uint32_t amount = lGetUlong(binding_elem, BN_amount);
   const auto instance = static_cast<BindingInstance::Instance>(lGetUlong(binding_elem, BN_instance));
   const char *sort = lGetString(binding_elem, BN_sort);
   const auto start = static_cast<BindingStart::Start>(lGetUlong(binding_elem, BN_start));
   const auto stop = static_cast<BindingStop::Stop>(lGetUlong(binding_elem, BN_stop));
   const auto strategy = static_cast<BindingStrategy::Strategy>(lGetUlong(binding_elem, BN_strategy));
   const auto type = static_cast<BindingType::Type>(lGetUlong(binding_elem, BN_new_type));
   const auto unit = static_cast<BindingUnit::Unit>(lGetUlong(binding_elem, BN_unit));
   const auto filter = lGetString(binding_elem, BN_filter);

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

void ocs::QStatJobViewJSON::report_task_list_started(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"array_tasks\": [";
   indent++;
   first_task = true;
   first_task_attribute = true;
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_list_finished(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_started(std::ostream &os, const lListElem *job, const lListElem *task) {
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

void ocs::QStatJobViewJSON::report_task_finished(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   first_task_attribute = true;
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_id(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   const uint32_t task_id = lGetUlong(task, JAT_task_number);
   os << std::string(indent * 3, ' ') << "\"task_id\": " << task_id;
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_state(std::ostream &os, const lListElem *job, const lListElem *task) {
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

void ocs::QStatJobViewJSON::report_task_usage(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   /* jobs whose job start orders were not processed to the end due to a qmaster/schedd collision appear
    * in the JB_ja_tasks list but are not running - thus we may not print usage for those */
   if (lGetUlong(task, JAT_status) != JRUNNING && lGetUlong(task, JAT_status) != JTRANSFERING) {
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

void ocs::QStatJobViewJSON::report_task_exec_binding_list(std::ostream &os, const lListElem *job,
                                                          const lListElem *task) {
   DENTER(TOP_LAYER);

   const lList *list = lGetListRW(task, JAT_granted_resources_list);
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
   for_each_ep_lv(elem, list) {
      if (const char *name = lGetString(elem, GRU_name); name == nullptr || strcmp(name, SGE_ATTR_SLOTS) != 0) {
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

      const char *hostname = lGetHost(elem, GRU_host);
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(hostname ? hostname : "") << ",\n";
      os << std::string(indent * 3, ' ') << "\"binding\": [\n";
      indent++;
      const lList *binding_in_use_list = lGetList(elem, GRU_binding_inuse);
      bool first_binding = true;
      for_each_ep_lv(binding_in_use, binding_in_use_list) {
         if (first_binding) {
            first_binding = false;
         } else {
            os << ",\n";
         }
         TopologyString topo_in_use(lGetString(binding_in_use, ST_name));
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

void ocs::QStatJobViewJSON::report_X_task_str_uint32_list(std::ostream &os, const lListElem *task, const int nm,
                                                          const char *name,
                                                          const int str_nm, const char *str_name, const int uint32_nm,
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
   for_each_ep_lv(elem, lGetList(task, nm)) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *string = lGetString(elem, str_nm);
      os << std::string(indent * 3, ' ') << "\"" << str_name << "\": " << raw2quotedJSON(string ? string : "") << ",\n";
      const uint32_t value = lGetUlong(elem, uint32_nm);
      os << std::string(indent * 3, ' ') << "\"" << uint32_name << "\": " << value << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_exec_queue_list(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   report_X_task_str_uint32_list(os, task, JAT_granted_destin_identifier_list, "granted_queue_list", JG_qname,
                                 "queue_name", JG_slots, "slots");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_exec_host_list(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);

   if (lGetPosViaElem(task, JAT_granted_destin_identifier_list, SGE_NO_ABORT) < 0) {
      DRETURN_VOID;
   }

   lList *gdil_org = lGetListRW(task, JAT_granted_destin_identifier_list);
   lList *gdil_unique = gdil_make_host_unique(gdil_org);

   if (first_task_attribute) {
      os << "\n";
      first_task_attribute = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"exec_host_list\": [";
   indent++;

   bool is_first = true;
   for_each_ep_lv(elem, gdil_unique) {
      if (is_first) {
         os << "\n";
         is_first = false;
      } else {
         os << ",\n";
      }
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *string = lGetHost(elem, JG_qhostname);
      os << std::string(indent * 3, ' ') << "\"hostname\": " << raw2quotedJSON(string ? string : "") << ",\n";
      const uint32_t value = lGetUlong(elem, JG_slots);
      os << std::string(indent * 3, ' ') << "\"slots\": " << value << "\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";

   lFreeList(&gdil_unique);
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_start_time(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   report_X_task_ISO_8601_timestamp(os, task, JAT_start_time, "start_time");
   DRETURN_VOID;
}

/* CS-1908 retention: emit end_time only for retained finished ja_tasks. */
void ocs::QStatJobViewJSON::report_task_end_time(std::ostream &os, const lListElem *job, const lListElem *task) {
   DENTER(TOP_LAYER);
   if (lGetUlong(task, JAT_status) != JFINISHED) {
      DRETURN_VOID;
   }
   report_X_task_ISO_8601_timestamp(os, task, JAT_end_time, "end_time");
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_task_resource_map(std::ostream &os, const lListElem *job, const lListElem *task) {
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
   for_each_ep_lv(resu, lGetList(task, JAT_granted_resources_list)) {
      if (static_cast<GrantedResources::Type>(lGetUlong(resu, GRU_type)) ==
          GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
         const char *name = lGetString(resu, GRU_name);
         const char *host = lGetHost(resu, GRU_host);
         const lList *ids = lGetList(resu, GRU_resource_map_list);

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
            for_each_ep_lv(id, ids) {
               const char *id_string = lGetString(id, RESL_value);
               const uint32_t amount = lGetUlong(id, RESL_amount);

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

void ocs::QStatJobViewJSON::report_task_error_reason(std::ostream &os, const lListElem *job, const lListElem *task) {
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
   for_each_ep_lv(elem, lGetList(task, JAT_message_list)) {
      if (const char *message = lGetString(elem, QIM_message)) {
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

void ocs::QStatJobViewJSON::report_schedd_job_info(std::ostream &os, const lList *ilp, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const lListElem *sme = lFirst(ilp); sme != nullptr) {
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
      for_each_ep_lv(mes, lGetList(sme, SME_global_message_list)) {
         print_sched_message(lGetString(mes, MES_message));
      }

      /* job scheduling info */
      const uint32_t jid = lGetUlong(job, JB_job_number);
      for_each_ep_lv(mes, lGetList(sme, SME_message_list)) {
         for_each_ep_lv(mes_jid, lGetList(mes, MES_job_number_list)) {
            if (lGetUlong(mes_jid, ULNG_value) == jid) {
               print_sched_message(lGetString(mes, MES_message));
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
