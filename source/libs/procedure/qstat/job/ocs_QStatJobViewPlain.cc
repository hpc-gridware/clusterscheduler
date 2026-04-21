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
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_grantedres.h"

#include "sched/sge_schedd_text.h"

#include "qstat/ocs_QStatParameter.h"
#include "qstat/job/ocs_QStatJobViewPlain.h"
#include "qstat/msg_qstat.h"

#include "parse_qsub.h"
#include "msg_clients_common.h"
#include "sgeobj/cull_parse_util.h"

void ocs::QStatJobViewPlain::show_ce_type_list(std::ostream &os, const lList *cel, const char *indent, const char *separator) {
   DENTER(TOP_LAYER);

   bool first = true;
   for_each_ep_lv(ce, cel) {
      if (first) {
         first = false;
      } else {
         os << separator << indent;
      }

      const char *name = lGetString(ce, CE_name);
      if (const char *s = lGetString(ce, CE_stringval)) {
         os << name << "=" << s;
      } else {
         os << name;
      }
   }

   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::cull_show_job(std::ostream &os, const lListElem *job, const int flags) {
   // create dummy params and view so that we can call virtual methods in this static function
   const ProcedureParameter parameter("");
   QStatJobViewPlain view(parameter);

   view.show_job(os, job, flags);
}

void ocs::QStatJobViewPlain::show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);

   /* print scheduler job information and global scheduler info */
   for_each_ep_lv(j_elem, model.jlp) {
      const uint32_t jid = lGetUlong(j_elem, JB_job_number);
      const char *owner = lGetString(j_elem, JB_owner);
      if (const bool show_job = job_is_visible(owner, model.is_manager()); !show_job) {
         DTRACE;
         continue;
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
         for_each_ep_lv(mes, lGetList(sme, SME_message_list)) {
            const lListElem *mes_jid;

            for_each_ep(mes_jid, lGetList(mes, MES_job_number_list)) {
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

   DRETURN_VOID;
}

void
ocs::QStatJobViewPlain::show_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   char text[256], ltext[256];

   if (lListElem *sme = lFirstRW(model.ilp); sme != nullptr) {
      lList *mlp = nullptr;
      uint32_t last_jid = 0;
      int initialized = 0;
      uint32_t last_mid = 0;
      int ids_per_line = 0;
      int first_run = 1;
      int first_row = 1;

      /* print global scheduling info */
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
      lListElem *flt_msg;

      lList *new_list = lCreateList("filtered message list", MES_Type);

      lListElem *flt_nxt_msg = lFirstRW(mlp);
      while ((flt_msg = flt_nxt_msg)) {
         lListElem *flt_jid;

         flt_nxt_msg = lNextRW(flt_msg);
         int found_msg = 0;
         for_each_ep_lv(ref_msg, new_list) {
            if (lGetUlong(ref_msg, MES_message_number) == lGetUlong(flt_msg, MES_message_number)) {
               lListElem *flt_nxt_jid = lFirstRW(lGetList(flt_msg, MES_job_number_list));
               while ((flt_jid = flt_nxt_jid)) {
                  flt_nxt_jid = lNextRW(flt_jid);

                  int found_jid = 0;
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
            const uint32_t mid = lGetUlong(mes, MES_message_number);
            const uint32_t jid = lGetUlong(jid_ulng, ULNG_value);

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


void ocs::QStatJobViewPlain::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_jobs_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_job_id(std::ostream &os, const lListElem *job, int flags) {
   DENTER(TOP_LAYER);
   if (!(flags & FLG_QALTER)) {
      uint32_t jid = lGetUlong(job, JB_job_number);
      if (jid > 0) {
         os << std::format("{:<{}} {}", "job_number:", left_width, jid) << "\n";
      } else {
         os << std::format("{:<{}} {}", "job_number:", left_width, MSG_JOB_UNASSIGNED) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_category_id(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_category_id, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} {}", "category_id:", left_width, lGetUlong(job, JB_category_id)) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_exec_file(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_exec_file, SGE_NO_ABORT) >= 0) {
      if (const char *exec_file = lGetString(job, JB_exec_file)) {
         os << std::format("{:<{}} {}", "exec_file:", left_width, exec_file) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_submission_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (lGetPosViaElem(job, JB_submission_time, SGE_NO_ABORT) >= 0) {
      if (const uint64_t time_value = lGetUlong64(job, JB_submission_time); time_value > 0) {
         os << std::format("{:<{}} {}", "submission_time:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_deadline_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (const uint64_t time_value = lGetUlong64(job, JB_deadline); time_value > 0) {
      os << std::format("{:<{}} {}", "deadline:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_submit_cmd_line(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const char *str = lGetString(job, JB_submission_command_line); str != nullptr) {
      os << std::format("{:<{}} {}", "submit_cmd_line:", left_width, str) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_effective_submit_cmd_line(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const char *str = lGetString(job, JB_submission_command_line); str != nullptr) {
      char *copied_str = strdup(str);
      if (const char *command = strtok(copied_str, " "); command != nullptr) {
         dstring dstr_cmd = DSTRING_INIT;
         os << std::format("{:<{}} {}", "effective_submit_cmd_line:", left_width, job_get_effective_command_line(job, &dstr_cmd, command)) << "\n";
         sge_dstring_free(&dstr_cmd);
      }
      sge_free(&copied_str);
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_ownership(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_owner, SGE_NO_ABORT) > NoName) {
      if (const char *owner = lGetString(job, JB_owner)) {
         os << std::format("{:<{}} {}", "owner:", left_width, owner) << "\n";
      } else {
         os << std::format("{:<{}} {}", "owner:", left_width, "") << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_uid, SGE_NO_ABORT) > NoName) {
      os << std::format("{:<{}} {}", "uid:", left_width, (int) lGetUlong(job, JB_uid)) << "\n";
   }

   if (lGetPosViaElem(job, JB_group, SGE_NO_ABORT) > NoName) {
      if (const char *group = lGetString(job, JB_group)) {
         os << std::format("{:<{}} {}", "group:", left_width, group) << "\n";
      } else {
         os << std::format("{:<{}} {}", "group:", left_width, "") << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_gid, SGE_NO_ABORT) > NoName) {
      os << std::format("{:<{}} {}", "gid:", left_width, lGetUlong(job, JB_gid)) << "\n";
   }

   if (lGetPosViaElem(job, JB_grp_list, SGE_NO_ABORT) > NoName) {
      std::stringstream ss_groups;
#if defined(WITH_EXTENSIONS)
      const lList *grp_list = lGetList(job, JB_grp_list);

      if (grp_list == nullptr) {
         ss_groups << "NONE";
      } else {
         bool first = true;

         for_each_ep_lv(grp_elem, grp_list) {
            if (first) {
               first = false;
            } else {
               ss_groups << ",";
            }
            ss_groups << std::format("{}({})", lGetUlong(grp_elem, ST_id), lGetString(grp_elem, ST_name));
         }
      }
#else
      ss_groups << "NOT-AVAILABLE-IN-OCS";
#endif
      os << std::format("{:<{}} {}", "groups:", left_width, ss_groups.str()) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_env_core(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   const char *name[] = {"O_HOME", "O_LOGNAME", "O_PATH", "O_SHELL", "O_TZ", "O_WORKDIR", "O_HOST", nullptr};
   const char *fmt_string[] = {"sge_o_home:", "sge_o_log_name:", "sge_o_path:", "sge_o_shell:", "sge_o_tz:", "sge_o_workdir:", "sge_o_host:", nullptr};
   int i = -1;

   while (name[++i] != nullptr) {
      char fullname[MAX_STRING_SIZE];
      snprintf(fullname, sizeof(fullname), "%s%s", VAR_PREFIX, name[i]);
      if (const char *value = job_get_env_string(job, fullname)) {
         os << std::format("{:<{}} {}", fmt_string[i], left_width, value) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_execution_time(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (const uint64_t time_value = lGetUlong64(job, JB_execution_time); time_value > 0) {
      os << std::format("{:<{}} {}", "execution_time:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_account(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const char *account = lGetString(job, JB_account)) {
      os << std::format("{:<{}} {}", "account:", left_width, account) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_checkpoint(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_checkpoint_name, SGE_NO_ABORT) >= 0) {
      if (const char *ckpt_name = lGetString(job, JB_checkpoint_name)) {
         os << std::format("{:<{}} {}", "checkpoint_object:", left_width, ckpt_name) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_checkpoint_attr, SGE_NO_ABORT) >= 0) {
      if (uint32_t ckpt_attr = lGetUlong(job, JB_checkpoint_attr)) {
         std::stringstream ss_ckpt_attr;
         job_get_ckpt_attr(ss_ckpt_attr, ckpt_attr);
         os << std::format("{:<{}} {}", "checkpoint_attr:", left_width, ss_ckpt_attr.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_checkpoint_interval, SGE_NO_ABORT) >= 0) {
      if (uint32_t ckpt_int = lGetUlong(job, JB_checkpoint_interval)) {
         os << std::format("{:<{}} {} seconds", "checkpoint_interval:", left_width, ckpt_int) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_cwd(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_cwd, SGE_NO_ABORT) >= 0) {
      if (const char *cwd = lGetString(job, JB_cwd)) {
         os << std::format("{:<{}} {}", "cwd:", left_width, cwd) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_path_aliases(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_cwd, SGE_NO_ABORT) >= 0) {
      if (const char *cwd = lGetString(job, JB_cwd)) {
         os << std::format("{:<{}} {}", "cwd:", left_width, cwd) << "\n";
      }
      if (lGetPosViaElem(job, JB_path_aliases, SGE_NO_ABORT) >= 0) {
         if (const lList *path_aliases = lGetList(job, JB_path_aliases)) {

            std::ostringstream ss_path_aliases;
            delis[0] = " ";
            int fields[] = {PA_origin, PA_submit_host, PA_exec_host, PA_translation, 0};
            uni_print_list(ss_path_aliases, path_aliases, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", "path_aliases:", left_width, ss_path_aliases.str());
         }
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_directive_prefix(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_directive_prefix, SGE_NO_ABORT) >= 0) {
      if (const char *prefix = lGetString(job, JB_directive_prefix)) {
         os << std::format("{:<{}} {}", "directive_prefix:", left_width, prefix) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_stdin_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const lList *list = lGetList(job, JB_stdin_path_list)) {
      std::ostringstream ss_list;
      delis[0] = ":";
      int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
      uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);

      os << std::format("{:<{}} {}", "stdin_path_list:", left_width, ss_list.str()) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_stdout_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_stdout_path_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_stdout_path_list)) {

         std::ostringstream ss_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", "stdout_path_list:", left_width, ss_list.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_stderr_path_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_stderr_path_list, SGE_NO_ABORT) >= 0) {
      if (const lList *path_list = lGetList(job, JB_stderr_path_list)) {

         std::ostringstream ss_path_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_path_list, path_list, fields, delis, FLG_NO_DELIS_STRINGS);

         os << std::format("{:<{}} {}", "stderr_path_list:", left_width, ss_path_list.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_reserve(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_reserve, SGE_NO_ABORT) >= 0) {
      if (const bool reserve = lGetBool(job, JB_reserve)) {
         os << std::format("{:<{}} {}", "reserve:", left_width, reserve ? "y" : "n") << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_merge_stderr(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_merge_stderr, SGE_NO_ABORT) >= 0) {
      if (const bool merge = lGetBool(job, JB_merge_stderr)) {
         os << std::format("{:<{}} {}", "merge:", left_width, merge ? "y" : "n") << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_request_set_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_request_set_list, SGE_NO_ABORT) >= 0) {
      const lList *jrs_list = lGetList(job, JB_request_set_list);
      for_each_ep_lv (jrs, jrs_list) {
         uint32_t scope = lGetUlong(jrs, JRS_scope);

         if (const lList *lp = lGetList(jrs, JRS_hard_resource_list)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_hard_resource_list);

            std::ostringstream ss_list;
            show_ce_type_list(ss_list, lp, "", ",");

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str()) << "\n";
         }

         if (const lList *lp = lGetList(jrs, JRS_soft_resource_list)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_soft_resource_list);

            std::ostringstream ss_list;
            show_ce_type_list(ss_list, lp, "", ",");

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str()) << "\n";
         }

         if (const lList *lp = lGetList(jrs, JRS_hard_queue_list)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_hard_queue_list);
            delis[0] = " ";

            std::ostringstream ss_list;
            int fields[] = {QR_name, 0};
            uni_print_list(ss_list, lp, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str());
         }

         if (const lList *lp = lGetList(jrs, JRS_soft_queue_list)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_soft_queue_list);
            delis[0] = " ";

            std::ostringstream ss_list;
            int fields[] = {QR_name, 0};
            uni_print_list(ss_list, lp, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str());
         }

         const char *allocation_rule = lGetString(jrs, JRS_allocation_rule);
         if (allocation_rule != nullptr) {
            std::string str_attrib = get_scope_list_name(scope, JRS_allocation_rule);

            os << std::format("{:<{}} {}", str_attrib, left_width, allocation_rule) << "\n";
         }
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_mail_options(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (const uint32_t mail_options =  lGetUlong(job, JB_mail_options)) {
      dstring mailopt = DSTRING_INIT;
      sge_dstring_append_mailopt(&mailopt, mail_options);
      os << std::format("{:<{}} {}", "mail_options:", left_width, sge_dstring_get_string(&mailopt)) << "\n";
      sge_dstring_free(&mailopt);
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_mail_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_mail_list, SGE_NO_ABORT) >= 0) {
      if (const lList *mail_list = lGetList(job, JB_mail_list)) {
         std::ostringstream ss_list;
         int fields[] = {MR_user, MR_host, 0};
         delis[0] = "@";
         uni_print_list(ss_list, mail_list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} ", "mail_list:", left_width) << ss_list.str();
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_notify(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_notify, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} ", "notify:", left_width) << (lGetBool(job, JB_notify) ? "TRUE" : "FALSE") << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_name(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_job_name, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} ", "job_name:", left_width) << (lGetString(job, JB_job_name) ? lGetString(job, JB_job_name) : "") << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_priority(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_priority, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} {}", "priority:", left_width, static_cast<int>(lGetUlong(job, JB_priority)) - BASE_PRIORITY) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_job_share(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_jobshare, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} {}", "jobshare:", left_width, lGetUlong(job, JB_jobshare)) << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_restart(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_restart, SGE_NO_ABORT) >= 0) {
      if (const uint32_t restart = lGetUlong(job, JB_restart)) {
         os << std::format("{:<{}} {}", "restart:", left_width, (restart == 2) ? "n" : "y") << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_shell_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_shell_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_shell_list)) {
         std::ostringstream ss_list;
         delis[0] = ":";
         constexpr int fields[] = {PN_host, PN_path, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", "shell_list:", left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_env_list(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_env_list, SGE_NO_ABORT) >= 0) {
      if (lList *list = lGetListRW(job, JB_env_list)) {
         lList *do_not_print = nullptr;
         var_list_split_prefix_vars(&list, &do_not_print, VAR_PREFIX);

         if (lGetNumberOfElem(list) == 0) {
            os << std::format("{:<{}} {}", "env_list:", left_width, "NONE") << "\n";
         } else {
            std::ostringstream ss_list;
            delis[0] = "=";
            constexpr int fields[] = {VA_variable, VA_value, 0};
            uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS | FLG_NO_VALUE_AS_EMPTY);
            os << std::format("{:<{}} {}", "env_list:", left_width, ss_list.str()) << "\n";
         }
         lAddList(list, &do_not_print);
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_verify(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_verify, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(job, JB_verify)) {
         os << std::format("{:<{}} {}", "verify:", left_width, "-verify") << "\n";
      }
   }
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_job_args(std::ostream &os, const lListElem *job) {
   DENTER(TOP_LAYER);
   if (lGetPosViaElem(job, JB_job_args, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_job_args); list != nullptr) {
         std::ostringstream ss_list;
         delis[0] = "";
         int fields[] = {ST_name, 0};
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "job_args:", left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}
