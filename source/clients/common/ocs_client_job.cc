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

#include <string>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <ostream>
#include <format>

#include "uti/sge_bitfield.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_mesobj.h"

#include "ocs_client_job.h"

#include "parse_qsub.h"

#include "msg_clients_common.h"



void cull_show_job(std::ostream &os, const lListElem *job, int flags) {
   DENTER(TOP_LAYER);

   if (!job) {
      DRETURN_VOID;
   }

   const char *delis[] = {nullptr, ",", "\n"};
   DSTRING_STATIC(dstr, 128);

   const int left_width_short = 19;
   const int mid_width = 11;
   const int left_width = left_width_short + mid_width + 2;

   if (!(flags & FLG_QALTER)) {
      u_long32 jid = lGetUlong(job, JB_job_number);
      if (jid > 0) {
         os << std::format("{:<{}} {}", "job_number:", left_width, jid) << "\n";
      } else {
         os << std::format("{:<{}} {}", "job_number:", left_width, MSG_JOB_UNASSIGNED) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_category_id, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} {}", "category_id:", left_width, lGetUlong(job, JB_category_id)) << "\n";
   }

   if (lGetPosViaElem(job, JB_exec_file, SGE_NO_ABORT) >= 0) {
      if (const char *exec_file = lGetString(job, JB_exec_file)) {
         os << std::format("{:<{}} {}", "exec_file:", left_width, exec_file) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_submission_time, SGE_NO_ABORT) >= 0) {
      if (u_long64 time_value = lGetUlong64(job, JB_submission_time)) {
         os << std::format("{:<{}} {}", "submission_time:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_submission_command_line, SGE_NO_ABORT) >= 0) {
      if (const char *str = lGetString(job, JB_submission_command_line); str != nullptr) {
         os << std::format("{:<{}} {}", "submit_cmd_line:", left_width, str) << "\n";

         // generate and print the currently effective command line
         char *copied_str = strdup(str);
         const char *command = strtok(copied_str, " ");
         if (command != nullptr) {
            dstring dstr_cmd = DSTRING_INIT;
            os << std::format("{:<{}} {}", "effective_submit_cmd_line:", left_width, job_get_effective_command_line(job, &dstr_cmd, command)) << "\n";
            sge_dstring_free(&dstr_cmd);
         }
         sge_free(&copied_str);
      }
   }

   if (lGetPosViaElem(job, JB_deadline, SGE_NO_ABORT) >= 0) {
      if (u_long64 time_value = lGetUlong64(job, JB_deadline)) {
         os << std::format("{:<{}} {}", "deadline:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

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
      const lListElem *grp_elem;
      const lList *grp_list = lGetList(job, JB_grp_list);

      if (grp_list == nullptr) {
         ss_groups << "NONE";
      } else {
         bool first = true;

         for_each_ep(grp_elem, grp_list) {
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

   {
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
   }

   if (lGetPosViaElem(job, JB_execution_time, SGE_NO_ABORT) >= 0) {
      if (u_long64 time_value = lGetUlong64(job, JB_execution_time)) {
         os << std::format("{:<{}} {}", "execution_time:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_account, SGE_NO_ABORT) >= 0) {
      if (const char *account = lGetString(job, JB_account)) {
         os << std::format("{:<{}} {}", "account:", left_width, account) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_checkpoint_name, SGE_NO_ABORT) >= 0) {
      if (const char *ckpt_name = lGetString(job, JB_checkpoint_name)) {
         os << std::format("{:<{}} {}", "checkpoint_object:", left_width, ckpt_name) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_checkpoint_attr, SGE_NO_ABORT) >= 0) {
      if (u_long32 ckpt_attr = lGetUlong(job, JB_checkpoint_attr)) {
         std::stringstream ss_ckpt_attr;
         job_get_ckpt_attr(ss_ckpt_attr, ckpt_attr);
         os << std::format("{:<{}} {}", "checkpoint_attr:", left_width, ss_ckpt_attr.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_checkpoint_interval, SGE_NO_ABORT) >= 0) {
      if (u_long32 ckpt_int = lGetUlong(job, JB_checkpoint_interval)) {
         os << std::format("{:<{}} {} seconds", "checkpoint_interval:", left_width, ckpt_int) << "\n";
      }
   }

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

            os << std::format("{:<{}} {}", "path_aliases:", left_width, ss_path_aliases.str()) << "\n";
         }
      }
   }

   if (lGetPosViaElem(job, JB_directive_prefix, SGE_NO_ABORT) >= 0) {
      if (const char *prefix = lGetString(job, JB_directive_prefix)) {
         os << std::format("{:<{}} {}", "directive_prefix:", left_width, prefix) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_stderr_path_list, SGE_NO_ABORT) >= 0) {
      if (const lList *path_list = lGetList(job, JB_stderr_path_list)) {

         std::ostringstream ss_path_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_path_list, path_list, fields, delis, FLG_NO_DELIS_STRINGS);

         os << std::format("{:<{}} {}", "stderr_path_list:", left_width, ss_path_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_reserve, SGE_NO_ABORT) >= 0) {
      if (const bool reserve = lGetBool(job, JB_reserve)) {
         os << std::format("{:<{}} {}", "reserve:", left_width, reserve ? "y" : "n") << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_merge_stderr, SGE_NO_ABORT) >= 0) {
      if (const bool merge = lGetBool(job, JB_merge_stderr)) {
         os << std::format("{:<{}} {}", "merge:", left_width, merge ? "y" : "n") << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_request_set_list, SGE_NO_ABORT) >= 0) {
      const lList *jrs_list = lGetList(job, JB_request_set_list);
      const lListElem *jrs;
      for_each_ep (jrs, jrs_list) {
         u_long32 scope = lGetUlong(jrs, JRS_scope);
         const char *str_scope = nullptr;
         DSTRING_STATIC(dstr_attrib, 32);
         const char *str_attrib;
         if (scope > JRS_SCOPE_GLOBAL) {
            str_scope = job_scope_name(scope);
         }

         if (const lList *lp = lGetList(jrs, JRS_hard_resource_list)) {
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "hard_resource_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_hard_resource_list:", str_scope);
            }

            std::ostringstream ss_list;
            show_ce_type_list(ss_list, lp, "", ",", false, nullptr, 0);

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str()) << "\n";
         }

         if (const lList *lp = lGetList(jrs, JRS_soft_resource_list)) {
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "soft_resource_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_soft_resource_list:", str_scope);
            }

            std::ostringstream ss_list;
            show_ce_type_list(ss_list, lp, "", ",", false, nullptr, 0);

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str()) << "\n";
         }

         if (const lList *lp = lGetList(jrs, JRS_hard_queue_list)) {
            delis[0] = " ";
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "hard_queue_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_hard_queue_list:", str_scope);
            }

            std::ostringstream ss_list;
            int fields[] = {QR_name, 0};
            uni_print_list(ss_list, lp, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str());
         }

         if (const lList *lp = lGetList(jrs, JRS_soft_queue_list)) {
            delis[0] = " ";
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "soft_queue_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_soft_queue_list:", str_scope);
            }

            std::ostringstream ss_list;
            int fields[] = {QR_name, 0};
            uni_print_list(ss_list, lp, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", str_attrib, left_width, ss_list.str());
         }
      }
   }

   if (lGetPosViaElem(job, JB_mail_options, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_mail_options)) {
         dstring mailopt = DSTRING_INIT;
         sge_dstring_append_mailopt(&mailopt, lGetUlong(job, JB_mail_options));
         os << std::format("{:<{}} {}", "mail_options:", left_width, sge_dstring_get_string(&mailopt)) << "\n";
         sge_dstring_free(&mailopt);
      }

   if (lGetPosViaElem(job, JB_mail_list, SGE_NO_ABORT) >= 0) {
      if (const lList *mail_list = lGetList(job, JB_mail_list)) {
         std::ostringstream ss_list;
         int fields[] = {MR_user, MR_host, 0};
         delis[0] = "@";
         uni_print_list(ss_list, mail_list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} ", "mail_list:", left_width) << ss_list.str();
      }
   }

   if (lGetPosViaElem(job, JB_notify, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} ", "notify:", left_width) << (lGetBool(job, JB_notify) ? "TRUE" : "FALSE") << "\n";
   }

   if (lGetPosViaElem(job, JB_job_name, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} ", "job_name:", left_width) << (lGetString(job, JB_job_name) ? lGetString(job, JB_job_name) : "") << "\n";
   }

   if (lGetPosViaElem(job, JB_stdout_path_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_stdout_path_list)) {

         std::ostringstream ss_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", "stdout_path_list:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_stdin_path_list, SGE_NO_ABORT) >= 0)
      if (const lList *list = lGetList(job, JB_stdin_path_list)) {

         std::ostringstream ss_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);

         os << std::format("{:<{}} {}", "stdin_path_list:", left_width, ss_list.str());
      }

   if (lGetPosViaElem(job, JB_priority, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} {}", "priority:", left_width, (int) lGetUlong(job, JB_priority) - BASE_PRIORITY) << "\n";
   }

   if (lGetPosViaElem(job, JB_jobshare, SGE_NO_ABORT) >= 0) {
      os << std::format("{:<{}} {}", "jobshare:", left_width, lGetUlong(job, JB_jobshare)) << "\n";
   }

   if (lGetPosViaElem(job, JB_restart, SGE_NO_ABORT) >= 0) {
      if (const u_long32 restart = lGetUlong(job, JB_restart)) {
         os << std::format("{:<{}} {}", "restart:", left_width, (restart == 2) ? "n" : "y") << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_shell_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_shell_list)) {
         std::ostringstream ss_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_path, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", "shell_list:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_verify, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(job, JB_verify)) {
         os << std::format("{:<{}} {}", "verify:", left_width, "-verify") << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_env_list, SGE_NO_ABORT) >= 0) {
      if (lList *list = lGetListRW(job, JB_env_list)) {
         lList *do_not_print = nullptr;
         var_list_split_prefix_vars(&list, &do_not_print, VAR_PREFIX);

         if (lGetNumberOfElem(list) == 0) {
            os << std::format("{:<{}} {}", "env_list:", left_width, "NONE") << "\n";
         } else {
            std::ostringstream ss_list;
            delis[0] = "=";
            int fields[] = {VA_variable, VA_value, 0};
            uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS | FLG_NO_VALUE_AS_EMPTY);
            os << std::format("{:<{}} {}", "env_list:", left_width, ss_list.str()) << "\n";
         }
         lAddList(list, &do_not_print);
      }
   }

   if (lGetPosViaElem(job, JB_job_args, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_job_args); list != nullptr || (flags & FLG_QALTER)) {
         std::ostringstream ss_list;
         delis[0] = "";
         int fields[] = {ST_name, 0};
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "job_args:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_qs_args, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_qs_args); list != nullptr || (flags & FLG_QALTER)) {
         std::ostringstream ss_list;
         int fields[] = {ST_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "qs_args:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_job_identifier_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_job_identifier_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "job_identifier_list:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_script_size, SGE_NO_ABORT) >= 0) {
      if (const u_long32 size = lGetUlong(job, JB_script_size)) {
         os << std::format("{:<{}} {}", "script_size:", left_width, size) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_script_file, SGE_NO_ABORT) >= 0) {
      if (const char *file = lGetString(job, JB_script_file)) {
         os << std::format("{:<{}} {}", "script_file:", left_width, file) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_script_ptr, SGE_NO_ABORT) >= 0) {
      if (const char *script = lGetString(job, JB_script_ptr)) {
         os << std::format("{:<{}} {}", "script_ptr:\n", left_width, script) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_pe, SGE_NO_ABORT) >= 0) {
      if (const char *pe = lGetString(job, JB_pe)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(lGetList(job, JB_pe_range), &range_string, true, false, false);
         std::stringstream ss_pe_details;
         ss_pe_details << pe << " range: " << sge_dstring_get_string(&range_string);
         sge_dstring_free(&range_string);

         os << std::format("{:<{}} {}", "parallel_environment:", left_width, ss_pe_details.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_jid_request_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_jid_request_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "jid_request_list (req):", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_jid_predecessor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_jid_predecessor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "jid_predecessor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_jid_successor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_jid_successor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "jid_successor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_ad_request_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_ja_ad_request_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "ja_ad_redecessor_list (req):", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_ad_predecessor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_ja_ad_predecessor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "ja_ad_predecessor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_ad_successor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_ja_ad_successor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "ja_ad_successor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_verify_suitable_queues, SGE_NO_ABORT) >= 0) {
      if (const u_long32 vsq = lGetUlong(job, JB_verify_suitable_queues)) {
         os << std::format("{:<{}} {}", "verify_suitable_queues:", left_width, vsq) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_soft_wallclock_gmt, SGE_NO_ABORT) >= 0) {
      if (u_long64 time_value = lGetUlong64(job, JB_soft_wallclock_gmt)) {
         os << std::format("{:<{}} {}", "soft_wallclock_gmt:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_hard_wallclock_gmt, SGE_NO_ABORT) >= 0) {
      if (u_long64 time_value = lGetUlong64(job, JB_hard_wallclock_gmt)) {
         os << std::format("{:<{}} {}", "hard_wallclock_gmt:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_version, SGE_NO_ABORT) >= 0) {
      if (u_long32 version = lGetUlong(job, JB_version)) {
         os << std::format("{:<{}} {}", "version:", left_width, version) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_override_tickets, SGE_NO_ABORT) >= 0) {
      if (u_long32 tickets = lGetUlong(job, JB_override_tickets)) {
         os << std::format("{:<{}} {}", "override_tickets:", left_width, tickets) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_project, SGE_NO_ABORT) >= 0) {
      if (const char *project = lGetString(job, JB_project)) {
         os << std::format("{:<{}} {}", "project:", left_width, project) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_department, SGE_NO_ABORT) >= 0) {
      if (const char *dept = lGetString(job, JB_department)) {
         os << std::format("{:<{}} {}", "department:", left_width, dept) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ar, SGE_NO_ABORT) >= 0) {
      if (u_long32 ar_id = lGetUlong(job, JB_ar)) {
         os << std::format("{:<{}} {}", "ar_id:", left_width, ar_id) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_sync_options, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(job, JB_sync_options)) {
         std::string sync_flags =  job_get_sync_options_string(job);
         os << std::format("{:<{}} {}", "sync_options:", left_width, sync_flags) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_structure, SGE_NO_ABORT) >= 0) {
      if (job_is_array(job)) {
         u_long32 start, end, step;
         job_get_submit_task_ids(job, &start, &end, &step);
         std::ostringstream ss_range;
         ss_range << std::format("{}-{}:{}", start, end, step);
         os << std::format("{:<{}} {}", "job-array tasks:", left_width, ss_range.str()) << "\n";
      }
      if (u_long32 tc = lGetUlong(job, JB_ja_task_concurrency)) {
         os << std::format("{:<{}} {}", "task_concurrency:", left_width, tc) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_context, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_context)) {
         std::ostringstream ss_list;
         int fields[] = {VA_variable, VA_value, 0};
         delis[0] = "=";
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", "context:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_binding, SGE_NO_ABORT) >= 0) {
      const lListElem *binding_elem = lGetObject(job, JB_binding);
      std::string binding_param;;
      ocs::BindingIo::binding_print_to_string(binding_elem, binding_param);
      os << std::format("{:<{}} {}", "binding:", left_width, binding_param) << "\n";
   }

   /* display online job usage separately for each array job but summarized over all pe_tasks */
#define SUM_UP_JATASK_USAGE(ja_task, dst, attr)                                                                        \
   if ((uep = lGetSubStr(ja_task, UA_name, attr, JAT_scaled_usage_list))) {                                            \
      dst += lGetDouble(uep, UA_value);                                                                                \
   }

#define SUM_UP_PETASK_USAGE(pe_task, dst, attr)                                                                        \
   if ((uep = lGetSubStr(pe_task, UA_name, attr, PET_scaled_usage))) {                                                 \
      dst += lGetDouble(uep, UA_value);                                                                                \
   }

   if (lGetPosViaElem(job, JB_ja_tasks, SGE_NO_ABORT) >= 0) {

      lList *ja_tasks = lGetListRW(job, JB_ja_tasks);
      if (ja_tasks != nullptr) {
         lPSortList(ja_tasks, "%I+", JAT_task_number);
      }

      const lListElem *uep, *jatep, *pe_task_ep;
      for_each_ep(jatep, lGetList(job, JB_ja_tasks)) {
         u_long32 task_number = lGetUlong(jatep, JAT_task_number);
         // create state string and show it
         char state_string[8];
         u_long32 state = jatask_combine_state_and_status_for_output(job, jatep);
         job_get_state_string(state_string, state);
         os << std::format("{:<{}} {:>{}}: {}", "job_state", left_width_short, task_number, mid_width, state_string) << "\n";

         // show job usage information
         {
            /* jobs whose job start orders were not processed to the end
               due to a qmaster/schedd collision appear in the JB_ja_tasks
               list but are not running - thus we may not print usage for those */
            if (lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING) {
               double wallclock{}, cpu{}, mem{}, io{}, ioops{}, iow{}, vmem{}, maxvmem{}, rss{}, maxrss{};

               // In case we have execd_params ENABLE_MEM_DETAILS set, output these values as well.
               double pss{}, maxpss{}, pmem{}, smem{};
               bool have_mem_details = lGetSubStr(jatep, UA_name, USAGE_ATTR_PSS, JAT_scaled_usage_list) != nullptr;

               /* master task */
               SUM_UP_JATASK_USAGE(jatep, wallclock, USAGE_ATTR_WALLCLOCK);
               SUM_UP_JATASK_USAGE(jatep, cpu, USAGE_ATTR_CPU);
               SUM_UP_JATASK_USAGE(jatep, mem, USAGE_ATTR_MEM);
               SUM_UP_JATASK_USAGE(jatep, io, USAGE_ATTR_IO);
               SUM_UP_JATASK_USAGE(jatep, ioops, USAGE_ATTR_IOOPS);
               SUM_UP_JATASK_USAGE(jatep, iow, USAGE_ATTR_IOW);
               SUM_UP_JATASK_USAGE(jatep, vmem, USAGE_ATTR_VMEM);
               SUM_UP_JATASK_USAGE(jatep, maxvmem, USAGE_ATTR_MAXVMEM);
               SUM_UP_JATASK_USAGE(jatep, rss, USAGE_ATTR_RSS);
               SUM_UP_JATASK_USAGE(jatep, maxrss, USAGE_ATTR_MAXRSS);
               if (have_mem_details) {
                  SUM_UP_JATASK_USAGE(jatep, pss, USAGE_ATTR_PSS);
                  SUM_UP_JATASK_USAGE(jatep, maxpss, USAGE_ATTR_MAXPSS);
                  SUM_UP_JATASK_USAGE(jatep, pmem, USAGE_ATTR_PMEM);
                  SUM_UP_JATASK_USAGE(jatep, smem, USAGE_ATTR_SMEM);
               }

               /* slave tasks */
               for_each_ep(pe_task_ep, lGetList(jatep, JAT_task_list)) {
                  // we do not sum up wallclock usage per pe task
                  SUM_UP_PETASK_USAGE(pe_task_ep, cpu, USAGE_ATTR_CPU);
                  SUM_UP_PETASK_USAGE(pe_task_ep, mem, USAGE_ATTR_MEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, io, USAGE_ATTR_IO);
                  SUM_UP_PETASK_USAGE(pe_task_ep, ioops, USAGE_ATTR_IOOPS);
                  SUM_UP_PETASK_USAGE(pe_task_ep, iow, USAGE_ATTR_IOW);
                  SUM_UP_PETASK_USAGE(pe_task_ep, vmem, USAGE_ATTR_VMEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, maxvmem, USAGE_ATTR_MAXVMEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, rss, USAGE_ATTR_RSS);
                  SUM_UP_PETASK_USAGE(pe_task_ep, maxrss, USAGE_ATTR_MAXRSS);
                  if (have_mem_details) {
                     SUM_UP_PETASK_USAGE(pe_task_ep, pss, USAGE_ATTR_PSS);
                     SUM_UP_PETASK_USAGE(pe_task_ep, maxpss, USAGE_ATTR_MAXPSS);
                     SUM_UP_PETASK_USAGE(pe_task_ep, pmem, USAGE_ATTR_PMEM);
                     SUM_UP_PETASK_USAGE(pe_task_ep, smem, USAGE_ATTR_SMEM);
                  }
               }

               DSTRING_STATIC(wallclock_string, 32);
               DSTRING_STATIC(cpu_string, 32);
               DSTRING_STATIC(vmem_string, 32);
               DSTRING_STATIC(maxvmem_string, 32);
               DSTRING_STATIC(rss_string, 32);
               DSTRING_STATIC(maxrss_string, 32);
               DSTRING_STATIC(iow_string, 32);

               double_print_time_to_dstring(wallclock, &wallclock_string, true);
               double_print_time_to_dstring(cpu, &cpu_string, true);
               double_print_memory_to_dstring(vmem, &vmem_string);
               double_print_memory_to_dstring(maxvmem, &maxvmem_string);
               double_print_memory_to_dstring(rss, &rss_string);
               double_print_memory_to_dstring(maxrss, &maxrss_string);
               double_print_time_to_dstring(iow, &iow_string, true);
               std::stringstream ss_usage;
               ss_usage << std::format("wallclock={},cpu={},mem={:<5.5f} GBs,io={:<5.5f},ioops={:.0f},iow={},vmem={},maxvmem={},rss={},maxrss={}",
                      sge_dstring_get_string(&wallclock_string), sge_dstring_get_string(&cpu_string),
                      mem, io, ioops, sge_dstring_get_string(&iow_string),
                      (vmem == 0.0) ? "N/A" : sge_dstring_get_string(&vmem_string),
                      (maxvmem == 0.0) ? "N/A" : sge_dstring_get_string(&maxvmem_string),
                      (rss == 0.0) ? "N/A" : sge_dstring_get_string(&rss_string),
                      (maxrss == 0.0) ? "N/A" : sge_dstring_get_string(&maxrss_string));
               if (have_mem_details) {
                  DSTRING_STATIC(pss_string, 32);
                  DSTRING_STATIC(maxpss_string, 32);
                  DSTRING_STATIC(pmem_string, 32);
                  DSTRING_STATIC(smem_string, 32);
                  double_print_memory_to_dstring(pss, &pss_string);
                  double_print_memory_to_dstring(maxpss, &maxpss_string);
                  double_print_memory_to_dstring(pmem, &pmem_string);
                  double_print_memory_to_dstring(smem, &smem_string);
                  ss_usage << std::format(",pss={},maxpss={},pmem={},smem={}",
                     sge_dstring_get_string(&pss_string), sge_dstring_get_string(&maxpss_string),
                     sge_dstring_get_string(&pmem_string), sge_dstring_get_string(&smem_string));
               }
               os << std::format("{:<{}} {:>{}}: {}", "usage", left_width_short, task_number, mid_width, ss_usage.str()) << "\n";
            }
         }

         // show binding information
         if (lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING) {
            const lList *granted_resources = lGetList(jatep, JAT_granted_resources_list);
            os << std::format("{:<{}} {:>{}}: {}", "exec_binding_list", left_width_short, task_number, mid_width, ocs::GrantedResources::to_string(granted_resources)) << "\n";
         }

         // show granted host list
         // If we do not have the list then skip the output
         if (lGetPosViaElem(jatep, JAT_granted_destin_identifier_list, SGE_NO_ABORT) >= 0) {
            delis[0] = "=";
            const lList *gdil_org = lGetList(jatep, JAT_granted_destin_identifier_list);
            std::stringstream ss_list;
            int queue_fields[] = {JG_qname, JG_slots, 0};
            uni_print_list(ss_list, gdil_org, queue_fields, delis, 0);
            os << std::format("{:<{}} {:>{}}: {}", "exec_queue_list", left_width_short, task_number, mid_width, ss_list.str());

            // get the list of granted hosts and make it unique
            // skip tasks that have not been scheduled so far
            if (lList *gdil_unique = gdil_make_host_unique(gdil_org)) {
               // print the task number and the list of granted hosts
               std::stringstream ss_list2;
               int host_fields[] = {JG_qhostname, JG_slots, 0};
               uni_print_list(ss_list2, gdil_unique, host_fields, delis, 0);
               os << std::format("{:<{}} {:>{}}: {}", "exec_host_list", left_width_short, task_number, mid_width, ss_list2.str());

               // free the list of granted hosts
               lFreeList(&gdil_unique);
            }
         }

         // show start time of each task
         DSTRING_STATIC(time_ds, 32);
         os << std::format("{:<{}} {:>{}}: {}", "start_time", left_width_short, task_number, mid_width, sge_ctime64(lGetUlong64(jatep, JAT_start_time), &time_ds)) << "\n";

         // show granted resources
         {
            const lListElem *resu = nullptr;
            bool first_task = true;
            bool is_first_remap = true;
            dstring task_resources = DSTRING_INIT;
            const char *resource_ids = nullptr;

            /* go through all RSMAP resources for the particular task and create a string */
            for_each_ep (resu, lGetList(jatep, JAT_granted_resources_list)) {
               if (lGetUlong(resu, GRU_type) == GRU_RESOURCE_MAP_TYPE) {
                  const char *name = lGetString(resu, GRU_name);
                  const char *host = lGetHost(resu, GRU_host);
                  const lList *ids = lGetList(resu, GRU_resource_map_list);

                  if (name != nullptr && ids != nullptr && host != nullptr) {
                     if (is_first_remap) {
                        is_first_remap = false;
                     } else {
                        sge_dstring_append(&task_resources, ",");
                     }
                     sge_dstring_append(&task_resources, name);
                     sge_dstring_append(&task_resources, "=");
                     sge_dstring_append(&task_resources, host);
                     sge_dstring_append(&task_resources, "=");
                     sge_dstring_append(&task_resources, "(");
                     std::string id_buffer;
                     sge_dstring_append(&task_resources, granted_res_get_id_string(id_buffer, ids));
                     sge_dstring_append(&task_resources, ")");
                  }
               }
            }
            /* print out granted resources */
            resource_ids = sge_dstring_get_string(&task_resources);
            os << std::format("{:<{}} {:>{}}: {}", "resource_map", left_width_short, task_number, mid_width, resource_ids != nullptr ? resource_ids : "NONE") << "\n";
            if (first_task) {
               first_task = false;
            }

            sge_dstring_free(&task_resources);
         }

         // show messages (error reasons) for each task
         {
            const lListElem *mesobj;

            for_each_ep(mesobj, lGetList(jatep, JAT_message_list)) {
               if (const char *message = lGetString(mesobj, QIM_message)) {
                  os << std::format("{:<{}} {:>{}}: {}", "error_reason", left_width_short, task_number, mid_width, message) << "\n";
               }
            }
         }
      }
   }

   DRETURN_VOID;
}

/*************************************************************/
/* cel CE_Type List */

void show_ce_type_list(std::ostream &os, const lList *cel, const char *indent, const char *separator,
                       bool display_resource_contribution, const lList *centry_list, int slots) {
   bool first = true;
   const lListElem *ce, *centry;
   const char *s, *name;
   double uc = -1;

   DENTER(TOP_LAYER);

   /* walk through complex entries */
   for_each_ep(ce, cel) {
      if (first) {
         first = false;
      } else {
         os << separator << indent;
      }

      name = lGetString(ce, CE_name);
      if (display_resource_contribution && (centry = centry_list_locate(centry_list, name))) {
         uc = centry_urgency_contribution(slots, name, lGetDouble(ce, CE_doubleval), centry);
      }
      s = lGetString(ce, CE_stringval);
      if (s) {
         if (!display_resource_contribution) {
            os << name << "=" << s;
         } else {
            os << name << "=" << s << " (" << uc << ")";
         }
      } else {
         if (!display_resource_contribution) {
            os << name;
         } else {
            os << name << " (" << uc << ")";
         }
      }
   }

   DRETURN_VOID;
}

