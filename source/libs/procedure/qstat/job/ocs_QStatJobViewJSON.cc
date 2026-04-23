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

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_path_alias.h"

#include "sched/sge_schedd_text.h"

#include "qstat/ocs_QStatParameter.h"
#include "qstat/job/ocs_QStatJobViewJSON.h"

#include <sstream>

#include "qstat/msg_qstat.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/cull/sge_centry_CE_L.h"
#include "sgeobj/cull/sge_mailrec_MR_L.h"
#include "sgeobj/cull/sge_qref_QR_L.h"
#include "uti/sge_stdlib.h"

void ocs::QStatJobViewJSON::show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   bool first_job = true;

   /* print scheduler job information and global scheduler info */
   for_each_ep_lv(j_elem, model.jlp) {
      // Do not show jobs that should not be visible
      if (!job_is_visible(lGetString(j_elem, JB_owner), model.is_manager())) {
         continue;
      }

      if (first_job) {
         report_jobs_started(os, parameter);
         first_job = false;
      }

      os << "==============================================================\n";
      /* print job information */
      show_job(os, j_elem, 0);

      /* print scheduling information */
      if (const lListElem *sme = lFirst(model.ilp); sme != nullptr) {
         int first_run = 1;

         /* global scheduling info */
         for_each_ep_lv(mes, lGetList(sme, SME_global_message_list)) {
            if (first_run) {
               os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
               first_run = 0;
            } else {
               os << "                                 ";
            }
            os << lGetString(mes, MES_message) << "\n";
         }

         /* job scheduling info */
         const uint32_t jid = lGetUlong(j_elem, JB_job_number);
         for_each_ep_lv(mes, lGetList(sme, SME_message_list)) {
            for_each_ep_lv(mes_jid, lGetList(mes, MES_job_number_list)) {
               if (lGetUlong(mes_jid, ULNG_value) == jid) {
                  if (first_run) {
                     os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
                     first_run = 0;
                  } else {
                     os << "                                 ";
                  }
                  os << lGetString(mes, MES_message) << "\n";
               }
            }
         }
      }
   }
   if (!first_job) {
      report_jobs_finished(os, parameter);
      first_job = false;
   }

   DRETURN_VOID;
}

void
ocs::QStatJobViewJSON::show_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   lList *mlp = nullptr;
   int initialized = 0;
   uint32_t last_jid = 0;
   uint32_t last_mid = 0;
   char text[256], ltext[256];
   int ids_per_line = 0;
   int first_run = 1;
   int first_row = 1;

   lListElem *sme = lFirstRW(model.ilp);
   if (sme) {
      /* print global schduling info */
      first_run = 1;
      for_each_ep_lv(mes, lGetList(sme, SME_global_message_list)) {
         if (first_run) {
            os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
            first_run = 0;
         } else {
            os << "                                 ";
         }
         os << lGetString(mes, MES_message) << "\n";
      }
      if (!first_run) {
         os << "\n";
      }

      first_run = 1;

      mlp = lGetListRW(sme, SME_message_list);
      lPSortList(mlp, "I+", MES_message_number);

      /*
       * Remove all jids which have more than one entry for a MES_message_number
       * After this step the MES_messages are not correct anymore
       * We do not need this messages for the summary output
       */
      lListElem *flt_msg, *flt_nxt_msg;

      lList *new_list = lCreateList("filtered message list", MES_Type);

      flt_nxt_msg = lFirstRW(mlp);
      while ((flt_msg = flt_nxt_msg)) {
         lListElem *flt_jid;
         lListElem *flt_nxt_jid;
         int found_msg, found_jid;

         flt_nxt_msg = lNextRW(flt_msg);
         found_msg = 0;
         for_each_ep_lv(ref_msg, new_list) {
            if (lGetUlong(ref_msg, MES_message_number) == lGetUlong(flt_msg, MES_message_number)) {
               flt_nxt_jid = lFirstRW(lGetList(flt_msg, MES_job_number_list));
               while ((flt_jid = flt_nxt_jid)) {
                  flt_nxt_jid = lNextRW(flt_jid);

                  found_jid = 0;
                  for_each_ep_lv(ref_jid, lGetList(ref_msg, MES_job_number_list)) {
                     if (lGetUlong(ref_jid, ULNG_value) ==
                         lGetUlong(flt_jid, ULNG_value)) {
                        lRemoveElem(lGetListRW(flt_msg, MES_job_number_list), &flt_jid);
                        found_jid = 1;
                        break;
                     }
                  }
                  if (!found_jid) {
                     lDechainElem(lGetListRW(flt_msg, MES_job_number_list), flt_jid);
                     lAppendElem(lGetListRW(ref_msg, MES_job_number_list), flt_jid);
                  }
               }
               found_msg = 1;
            }
         }
         if (!found_msg) {
            lDechainElem(mlp, flt_msg);
            lAppendElem(new_list, flt_msg);
         }
         lSetList(sme, SME_message_list, new_list);
         mlp = new_list;
      }

      text[0] = 0;
      for_each_ep_lv(mes, mlp) {
         lPSortList(lGetListRW(mes, MES_job_number_list), "I+", ULNG_value);

         for_each_ep_lv(jid_ulng, lGetList(mes, MES_job_number_list)) {
            int skip = 0;
            int header = 0;

            uint32_t mid = lGetUlong(mes, MES_message_number);
            uint32_t jid = lGetUlong(jid_ulng, ULNG_value);

            if (initialized) {
               if (last_mid == mid && last_jid == jid) {
                  skip = 1;
               } else if (last_mid != mid) {
                  header = 1;
               }
            } else {
               initialized = 1;
               header = 1;
            }

            if (strlen(text) >= MAX_LINE_LEN || ids_per_line >= MAX_IDS_PER_LINE || header) {
               os << text;
               text[0] = 0;
               ids_per_line = 0;
               first_row = 0;
            }

            if (header) {
               if (!first_run) {
                  os << "\n\n";
               } else {
                  first_run = 0;
               }
               os << sge_schedd_text(mid + SCHEDD_INFO_OFFSET) << "\n";
               first_row = 1;
            }

            if (!skip) {
               if (ids_per_line == 0) {
                  if (first_row) {
                     strcat(text, "\t");
                  } else {
                     strcat(text, ",\n\t");
                  }
               } else {
                  strcat(text, ",\t");
               }
               snprintf(ltext, sizeof(ltext), sge_u32, jid);
               strcat(text, ltext);
               ids_per_line++;
            }

            last_jid = jid;
            last_mid = mid;
         }
      }
      if (text[0] != 0) {
         os << text << "\n";
      }
   }

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
   os << ",\n" << std::string(indent * 3, ' ') << "\"jobs\": [\n";
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

void ocs::QStatJobViewJSON::report_X_uint32(std::ostream &os, const lListElem *job, const int nm, const char *name) {
   DENTER(TOP_LAYER);
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

   if (first_attribute) {
      os << "\n";
      first_attribute = false;
   } else {
      os << ",\n";
   }
   char *copied_str = strdup(str);
   if (const char *command = strtok(copied_str, " "); command != nullptr) {
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
   const lList *grp_list = lGetList(job, JB_grp_list);
#if defined(WITH_EXTENSIONS)
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

   while (name[++i] != nullptr) {
      char fullname[MAX_STRING_SIZE];
      snprintf(fullname, sizeof(fullname), "%s%s", VAR_PREFIX, name[i]);
      if (const char *value = job_get_env_string(job, fullname)) {
         if (i != 0) {
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
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;

      const char *qname = lGetString(elem, QR_name);
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(qname ? qname : "") << ",\n";
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
   if (const uint32_t mail_options =  lGetUlong(job, JB_mail_options)) {
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
      os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << ",\n";

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
   os << std::string(indent * 3, ' ') << "\"priority\": " << (static_cast<int>(lGetUlong(job, JB_priority)) - BASE_PRIORITY);
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
      os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host ? host : "") << ",\n";

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_env_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);

   lList *env_list = lGetListRW(job, JB_env_list);
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
   os << std::string(indent * 3, ' ') << "\"env_list\": [";
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
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(user ? user : "") << ",\n";
      if (const char *host = lGetString(elem, VA_value); host != nullptr) {
         os << std::string(indent * 3, ' ') << "\"value\": " << raw2quotedJSON(host ? host : "") << ",\n";
      }

      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }

   lAddList(env_list, &do_not_print);

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

void ocs::QStatJobViewJSON::report_X_string_list(std::ostream &os, const lListElem *job, const int list_nm, const int value_nm, const char *name) {
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
      os << std::string(indent * 3, ' ') << value;
   }

   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   DRETURN_VOID;
}

void ocs::QStatJobViewJSON::report_X_uint32_list(std::ostream &os, const lListElem *job, const int list_nm, const int value_nm, const char *name) {
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
