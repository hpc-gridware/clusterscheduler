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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <string>
#include <cstring>
#include <cstdio>

#include "uti/sge_bitfield.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/ocs_BindingIo.h"
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

#include "get_path.h"
#include "ocs_client_job.h"
#include "parse_qsub.h"

#include "msg_clients_common.h"

static void sge_show_checkpoint(int how, int op);

static void sge_show_y_n(int op, int how);

static void show_ce_type_list(const lList *cel, const char *indent, const char *separator,
                              bool display_resource_contribution, const lList *centry_list, int slots);

void cull_show_job(const lListElem *job, int flags, bool show_binding) {
   DENTER(TOP_LAYER);

   if (!job) {
      DRETURN_VOID;
   }

   const char *delis[] = {nullptr, ",", "\n"};
   u_long64 ultime;
   DSTRING_STATIC(dstr, 128);

   if (!(flags & FLG_QALTER)) {
      if (lGetUlong(job, JB_job_number))
         printf("job_number:                     " sge_u32 "\n", lGetUlong(job, JB_job_number));
      else
         printf("job_number:                     %s\n", MSG_JOB_UNASSIGNED);
   }

   if (lGetPosViaElem(job, JB_category_id, SGE_NO_ABORT) >= 0)
      printf("category_id:                    " sge_u32 "\n", lGetUlong(job, JB_category_id));

   if (lGetPosViaElem(job, JB_exec_file, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_exec_file))
         printf("exec_file:                      %s\n", lGetString(job, JB_exec_file));

   if (lGetPosViaElem(job, JB_submission_time, SGE_NO_ABORT) >= 0)
      if ((ultime = lGetUlong64(job, JB_submission_time))) {
         printf("submission_time:                %s\n", sge_ctime64(ultime, &dstr));
      }

   if (lGetPosViaElem(job, JB_submission_command_line, SGE_NO_ABORT) >= 0) {
      const char *str = lGetString(job, JB_submission_command_line);
      if (str != nullptr) {
         printf("submit_cmd_line:                %s\n", str);

         // generate and print the currently effective command line
         char *copied_str = strdup(str);
         const char *command = strtok(copied_str, " ");
         if (command != nullptr) {
            dstring dstr_cmd = DSTRING_INIT;
            printf("effective_submit_cmd_line:      %s\n", job_get_effective_command_line(job, &dstr_cmd, command));
            sge_dstring_free(&dstr_cmd);
         }
         sge_free(&copied_str);
      }
   }

#if 0
   // @todo print for all running ja_tasks
   if (lGetPosViaElem(ja_task, JAT_start_time, SGE_NO_ABORT) >= 0)
      if ((ultime = lGetUlong64(ja_task, JAT_start_time))) {
         printf("start_time:                 %s\n", sge_ctime64(ultime, &dstr));
      }
#endif

   if (lGetPosViaElem(job, JB_deadline, SGE_NO_ABORT) >= 0)
      if ((ultime = lGetUlong64(job, JB_deadline))) {
         printf("deadline:                       %s\n", sge_ctime64(ultime, &dstr));
      }

   if (lGetPosViaElem(job, JB_owner, SGE_NO_ABORT) > NoName) {
      if (lGetString(job, JB_owner))
         printf("owner:                          %s\n", lGetString(job, JB_owner));
      else
         printf("owner:                          %s\n", "");
   }

   if (lGetPosViaElem(job, JB_uid, SGE_NO_ABORT) > NoName) {
      printf("uid:                            %d\n", (int) lGetUlong(job, JB_uid));
   }

   if (lGetPosViaElem(job, JB_group, SGE_NO_ABORT) > NoName) {
      if (lGetString(job, JB_group))
         printf("group:                          %s\n", lGetString(job, JB_group));
      else
         printf("group:                          %s\n", "");
   }

   if (lGetPosViaElem(job, JB_gid, SGE_NO_ABORT) > NoName) {
      printf("gid:                            %d\n", (int) lGetUlong(job, JB_gid));
   }

   if (lGetPosViaElem(job, JB_grp_list, SGE_NO_ABORT) > NoName) {
      printf("groups:                         ");
#if defined(WITH_EXTENSIONS)
      const lListElem *grp_elem;
      const lList *grp_list = lGetList(job, JB_grp_list);

      if (grp_list == nullptr) {
         printf("NONE");
      } else {
         bool first = true;

         for_each_ep(grp_elem, grp_list) {
            if (first) {
               first = false;
            } else {
               printf(",");
            }
            printf(gid_t_fmt "(%s)", (gid_t) lGetUlong(grp_elem, ST_id), lGetString(grp_elem, ST_name));
         }
      }
#else
      printf("NOT-AVAILABLE-IN-OCS");
#endif
      printf("\n");
   }

   {
      const char *name[] = {"O_HOME", "O_LOGNAME", "O_PATH", "O_SHELL", "O_TZ", "O_WORKDIR", "O_HOST", nullptr};
      const char *fmt_string[] = {"sge_o_home:                     %s\n", "sge_o_log_name:                 %s\n",
                                  "sge_o_path:                     %s\n", "sge_o_shell:                    %s\n",
                                  "sge_o_tz:                       %s\n", "sge_o_workdir:                  %s\n",
                                  "sge_o_host:                     %s\n", nullptr};
      int i = -1;

      while (name[++i] != nullptr) {
         const char *value;
         char fullname[MAX_STRING_SIZE];

         snprintf(fullname, sizeof(fullname), "%s%s", VAR_PREFIX, name[i]);
         value = job_get_env_string(job, fullname);
         if (value != nullptr) {
            printf(fmt_string[i], value);
         }
      }
   }

   if (lGetPosViaElem(job, JB_execution_time, SGE_NO_ABORT) >= 0)
      if ((ultime = lGetUlong64(job, JB_execution_time)))
         printf("execution_time:                 %s\n", sge_ctime64(ultime, &dstr));

   if (lGetPosViaElem(job, JB_account, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_account))
         printf("account:                        %s\n", lGetString(job, JB_account));

   if (lGetPosViaElem(job, JB_checkpoint_name, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_checkpoint_name))
         printf("checkpoint_object:              %s\n", lGetString(job, JB_checkpoint_name));

   if (lGetPosViaElem(job, JB_checkpoint_attr, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_checkpoint_attr)) {
         printf("checkpoint_attr:                ");
         sge_show_checkpoint(SGE_STDOUT, lGetUlong(job, JB_checkpoint_attr));
         printf("\n");
      }

   if (lGetPosViaElem(job, JB_checkpoint_interval, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_checkpoint_interval)) {
         printf("checkpoint_interval:            ");
         printf("%d seconds\n", (int) lGetUlong(job, JB_checkpoint_interval));
   }

   if (lGetPosViaElem(job, JB_cwd, SGE_NO_ABORT) >= 0) {
      if (lGetString(job, JB_cwd))
         printf("cwd:                            %s\n", lGetString(job, JB_cwd));
      if (lGetPosViaElem(job, JB_path_aliases, SGE_NO_ABORT) >= 0)
         if (lGetList(job, JB_path_aliases)) {
            int fields[] = {PA_origin, PA_submit_host, PA_exec_host, PA_translation, 0};

            delis[0] = " ";
            printf("path_aliases:                   ");
            uni_print_list(stdout, nullptr, 0, lGetList(job, JB_path_aliases), fields, delis, FLG_NO_DELIS_STRINGS);
         }
   }

   if (lGetPosViaElem(job, JB_directive_prefix, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_directive_prefix))
         printf("directive_prefix:               %s\n", lGetString(job, JB_directive_prefix));

   if (lGetPosViaElem(job, JB_stderr_path_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_stderr_path_list)) {
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};

         delis[0] = ":";
         printf("stderr_path_list:               ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_stderr_path_list), fields, delis, FLG_NO_DELIS_STRINGS);
         printf("\n");
      }

   if (lGetPosViaElem(job, JB_reserve, SGE_NO_ABORT) >= 0)
      if (lGetBool(job, JB_reserve)) {
         printf("reserve:                        ");
         sge_show_y_n(lGetBool(job, JB_reserve), SGE_STDOUT);
         printf("\n");
      }

   if (lGetPosViaElem(job, JB_merge_stderr, SGE_NO_ABORT) >= 0)
      if (lGetBool(job, JB_merge_stderr)) {
         printf("merge:                          ");
         sge_show_y_n(lGetBool(job, JB_merge_stderr), SGE_STDOUT);
         printf("\n");
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
         const lList *lp = lGetList(jrs, JRS_hard_resource_list);
         if (lp != nullptr) {
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "hard_resource_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_hard_resource_list:", str_scope);
            }
            printf("%-32s", str_attrib);
            sge_show_ce_type_list(lp);
            printf("\n");
         }

         lp = lGetList(jrs, JRS_soft_resource_list);
         if (lp != nullptr) {
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "soft_resource_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_soft_resource_list:", str_scope);
            }
            printf("%-32s", str_attrib);
            sge_show_ce_type_list(lp);
            printf("\n");
         }

         lp = lGetList(jrs, JRS_hard_queue_list);
         if (lp != nullptr) {
            int fields[] = {QR_name, 0};
            delis[0] = " ";
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "hard_queue_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_hard_queue_list:", str_scope);
            }
            printf("%-32s", str_attrib);
            uni_print_list(stdout, nullptr, 0, lp, fields, delis, FLG_NO_DELIS_STRINGS);
         }

         lp = lGetList(jrs, JRS_soft_queue_list);
         if (lp != nullptr) {
            int fields[] = {QR_name, 0};
            delis[0] = " ";
            if (str_scope == nullptr) {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "soft_queue_list:");
            } else {
               str_attrib = sge_dstring_sprintf(&dstr_attrib, "%s_soft_queue_list:", str_scope);
            }
            printf("%-32s", str_attrib);
            uni_print_list(stdout, nullptr, 0, lp, fields, delis, FLG_NO_DELIS_STRINGS);
         }
      }
   }

   if (lGetPosViaElem(job, JB_mail_options, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_mail_options)) {
         dstring mailopt = DSTRING_INIT;

         printf("mail_options:                   %s\n",
                sge_dstring_append_mailopt(&mailopt, lGetUlong(job, JB_mail_options)));

         sge_dstring_free(&mailopt);
      }

   if (lGetPosViaElem(job, JB_mail_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_mail_list)) {
         int fields[] = {MR_user, MR_host, 0};

         delis[0] = "@";
         printf("mail_list:                      ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_mail_list), fields, delis, FLG_NO_DELIS_STRINGS);
      }

   if (lGetPosViaElem(job, JB_notify, SGE_NO_ABORT) >= 0)
      printf("notify:                         %s\n", (lGetBool(job, JB_notify) ? "TRUE" : "FALSE"));

   if (lGetPosViaElem(job, JB_job_name, SGE_NO_ABORT) >= 0) {
      if (lGetString(job, JB_job_name))
         printf("job_name:                       %s\n", lGetString(job, JB_job_name));
      else
         printf("job_name:                       %s\n", "");
   }

   if (lGetPosViaElem(job, JB_stdout_path_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_stdout_path_list)) {
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};

         delis[0] = ":";
         printf("stdout_path_list:               ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_stdout_path_list), fields, delis, FLG_NO_DELIS_STRINGS);
         printf("\n");
      }

   if (lGetPosViaElem(job, JB_stdin_path_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_stdin_path_list)) {
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};

         delis[0] = ":";
         printf("stdin_path_list:                ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_stdin_path_list), fields, delis, FLG_NO_DELIS_STRINGS);
         printf("\n");
      }

   if (lGetPosViaElem(job, JB_priority, SGE_NO_ABORT) >= 0) {
      printf("priority:                       ");
      printf("%d\n", (int) lGetUlong(job, JB_priority) - BASE_PRIORITY);
   }

   if (lGetPosViaElem(job, JB_jobshare, SGE_NO_ABORT) >= 0) {
      printf("jobshare:                       ");
      printf(sge_u32 "\n", lGetUlong(job, JB_jobshare));
   }

   if (lGetPosViaElem(job, JB_restart, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_restart)) {
         printf("restart:                        ");
         sge_show_y_n((lGetUlong(job, JB_restart) == 2) ? 0 : 1, SGE_STDOUT);
         printf("\n");
      }

   if (lGetPosViaElem(job, JB_shell_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_shell_list)) {
         int fields[] = {PN_host, PN_path, 0};

         delis[0] = ":";
         printf("shell_list:                     ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_shell_list), fields, delis, FLG_NO_DELIS_STRINGS);
      }

   if (lGetPosViaElem(job, JB_verify, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_verify))
         printf("verify:                         %s\n", "-verify");

   if (lGetPosViaElem(job, JB_env_list, SGE_NO_ABORT) >= 0) {
      if (lGetList(job, JB_env_list)) {
         lList *print = nullptr;
         lList *do_not_print = nullptr;
         int fields[] = {VA_variable, VA_value, 0};

         delis[0] = "=";
         printf("env_list:                       ");

         print = lGetListRW(job, JB_env_list);
         var_list_split_prefix_vars(&print, &do_not_print, VAR_PREFIX);
         uni_print_list(stdout, nullptr, 0, print, fields, delis, FLG_NO_DELIS_STRINGS | FLG_NO_VALUE_AS_EMPTY);
         if (lGetNumberOfElem(print) == 0) {
            printf("NONE\n");
         }
         lAddList(print, &do_not_print);
      }
   }

   if (lGetPosViaElem(job, JB_job_args, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_job_args) || (flags & FLG_QALTER)) {
         int fields[] = {ST_name, 0};

         delis[0] = "";
         printf("job_args:                       ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_job_args), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_qs_args, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_qs_args) || (flags & FLG_QALTER)) {
         int fields[] = {ST_name, 0};
         delis[0] = "";
         printf("qs_args:                        ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_qs_args), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_job_identifier_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_job_identifier_list)) {
         int fields[] = {JRE_job_number, 0};

         delis[0] = "";
         printf("job_identifier_list:            ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_job_identifier_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_script_size, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_script_size))
         printf("script_size:                    " sge_u32 "\n", lGetUlong(job, JB_script_size));

   if (lGetPosViaElem(job, JB_script_file, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_script_file))
         printf("script_file:                    %s\n", lGetString(job, JB_script_file));

   if (lGetPosViaElem(job, JB_script_ptr, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_script_ptr))
         printf("script_ptr:                \n%s\n", lGetString(job, JB_script_ptr));

   if (lGetPosViaElem(job, JB_pe, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_pe)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(lGetList(job, JB_pe_range), &range_string, true, false, false);
         printf("%-32s%s range: %s\n", "parallel_environment:", lGetString(job, JB_pe), sge_dstring_get_string(&range_string));
         sge_dstring_free(&range_string);
      }

   if (lGetPosViaElem(job, JB_jid_request_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_jid_request_list)) {
         int fields[] = {JRE_job_name, 0};

         delis[0] = "";
         printf("jid_predecessor_list (req):     ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_jid_request_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_jid_predecessor_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_jid_predecessor_list)) {
         int fields[] = {JRE_job_number, 0};

         delis[0] = "";
         printf("jid_predecessor_list:           ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_jid_predecessor_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_jid_successor_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_jid_successor_list)) {
         int fields[] = {JRE_job_number, 0};

         delis[0] = "";
         printf("jid_successor_list:             ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_jid_successor_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_ja_ad_request_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_ja_ad_request_list)) {
         int fields[] = {JRE_job_name, 0};

         delis[0] = "";
         printf("ja_ad_predecessor_list (req):      ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_ja_ad_request_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_ja_ad_predecessor_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_ja_ad_predecessor_list)) {
         int fields[] = {JRE_job_number, 0};

         delis[0] = "";
         printf("ja_ad_predecessor_list:           ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_ja_ad_predecessor_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_ja_ad_successor_list, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_ja_ad_successor_list)) {
         int fields[] = {JRE_job_number, 0};

         delis[0] = "";
         printf("ja_ad_successor_list:              ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_ja_ad_successor_list), fields, delis, 0);
      }

   if (lGetPosViaElem(job, JB_verify_suitable_queues, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_verify_suitable_queues))
         printf("verify_suitable_queues:         %d\n", (int) lGetUlong(job, JB_verify_suitable_queues));

   if (lGetPosViaElem(job, JB_soft_wallclock_gmt, SGE_NO_ABORT) >= 0)
      if ((ultime = lGetUlong64(job, JB_soft_wallclock_gmt))) {
         printf("soft_wallclock_gmt:             %s", sge_ctime64(ultime, &dstr));
      }

   if (lGetPosViaElem(job, JB_hard_wallclock_gmt, SGE_NO_ABORT) >= 0)
      if ((ultime = lGetUlong64(job, JB_hard_wallclock_gmt))) {
         printf("hard_wallclock_gmt:             %s", sge_ctime64(ultime, &dstr));
      }

   if (lGetPosViaElem(job, JB_version, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_version))
         printf("version:                        %d\n", (int) lGetUlong(job, JB_version));
   /*
    ** problem: found no format anywhere
    */

   if (lGetPosViaElem(job, JB_override_tickets, SGE_NO_ABORT) >= 0)
      if (lGetUlong(job, JB_override_tickets))
         printf("oticket:                        %d\n", (int) lGetUlong(job, JB_override_tickets));

   if (lGetPosViaElem(job, JB_project, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_project))
         printf("project:                        %s\n", lGetString(job, JB_project));

   if (lGetPosViaElem(job, JB_department, SGE_NO_ABORT) >= 0)
      if (lGetString(job, JB_department))
         printf("department:                     %s\n", lGetString(job, JB_department));

   if (lGetPosViaElem(job, JB_ar, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(job, JB_ar)) {
         printf("ar_id:                          %d\n", (int) lGetUlong(job, JB_ar));
      }
   }

   if (lGetPosViaElem(job, JB_sync_options, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(job, JB_sync_options)) {
         std::string sync_flags =  job_get_sync_options_string(job);
         printf("sync_options:                   %s\n", sync_flags.c_str());
      }
   }

   if (lGetPosViaElem(job, JB_ja_structure, SGE_NO_ABORT) >= 0) {
      u_long32 start, end, step;

      job_get_submit_task_ids(job, &start, &end, &step);
      if (job_is_array(job))
         printf("job-array tasks:                " sge_u32 "-" sge_u32 ":" sge_u32 "\n", start, end, step);
   }

   if (lGetPosViaElem(job, JB_context, SGE_NO_ABORT) >= 0)
      if (lGetList(job, JB_context)) {
         int fields[] = {VA_variable, VA_value, 0};

         delis[0] = "=";
         printf("context:                        ");
         uni_print_list(stdout, nullptr, 0, lGetList(job, JB_context), fields, delis, FLG_NO_DELIS_STRINGS);
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

   if (show_binding) {
      const lList *binding_list = lGetList(job, JB_binding);

      if (binding_list != nullptr) {
         const lListElem *binding_elem = lFirst(binding_list);
         dstring binding_param = DSTRING_INIT;

         ocs::BindingIo::binding_print_to_string(binding_elem, &binding_param);
         printf("binding:                        " SFN "\n", sge_dstring_get_string(&binding_param));
         sge_dstring_free(&binding_param);
      }
   }


   if (lGetPosViaElem(job, JB_ja_tasks, SGE_NO_ABORT) >= 0) {

      lList *ja_tasks = lGetListRW(job, JB_ja_tasks);
      if (ja_tasks != nullptr) {
         lPSortList(ja_tasks, "%I+", JAT_task_number);
      }

      const lListElem *uep, *jatep, *pe_task_ep;
      for_each_ep(jatep, lGetList(job, JB_ja_tasks)) {
         // show task state
         {
            // create state string and show it
            char state_string[8];
            u_long32 state = jatask_combine_state_and_status_for_output(job, jatep);
            job_get_state_string(state_string, state);
            printf("%-16s %11d:   %s\n", "job_state", static_cast<int>(lGetUlong(jatep, JAT_task_number)), state_string);
         }

         // show job usage information
         {
            /* jobs whose job start orders were not processed to the end
               due to a qmaster/schedd collision appear in the JB_ja_tasks
               list but are not running - thus we may not print usage for those */
            if (lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING) {
               printf("%-16s %11d:   ", "usage", static_cast<int>(lGetUlong(jatep, JAT_task_number)));

               double wallclock{}, cpu{}, mem{}, io{}, ioops{}, vmem{}, maxvmem{}, rss{}, maxrss{};

               /* master task */
               SUM_UP_JATASK_USAGE(jatep, wallclock, USAGE_ATTR_WALLCLOCK);
               SUM_UP_JATASK_USAGE(jatep, cpu, USAGE_ATTR_CPU);
               SUM_UP_JATASK_USAGE(jatep, mem, USAGE_ATTR_MEM);
               SUM_UP_JATASK_USAGE(jatep, io, USAGE_ATTR_IO);
               SUM_UP_JATASK_USAGE(jatep, ioops, USAGE_ATTR_IOOPS);
               SUM_UP_JATASK_USAGE(jatep, vmem, USAGE_ATTR_VMEM);
               SUM_UP_JATASK_USAGE(jatep, maxvmem, USAGE_ATTR_MAXVMEM);
               SUM_UP_JATASK_USAGE(jatep, rss, USAGE_ATTR_RSS);
               SUM_UP_JATASK_USAGE(jatep, maxrss, USAGE_ATTR_MAXRSS);

               /* slave tasks */
               for_each_ep(pe_task_ep, lGetList(jatep, JAT_task_list)) {
                  // we do not sum up wallclock usage per pe task
                  SUM_UP_PETASK_USAGE(pe_task_ep, cpu, USAGE_ATTR_CPU);
                  SUM_UP_PETASK_USAGE(pe_task_ep, mem, USAGE_ATTR_MEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, io, USAGE_ATTR_IO);
                  SUM_UP_PETASK_USAGE(pe_task_ep, ioops, USAGE_ATTR_IOOPS);
                  SUM_UP_PETASK_USAGE(pe_task_ep, vmem, USAGE_ATTR_VMEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, maxvmem, USAGE_ATTR_MAXVMEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, rss, USAGE_ATTR_RSS);
                  SUM_UP_PETASK_USAGE(pe_task_ep, maxrss, USAGE_ATTR_MAXRSS);
               }

               DSTRING_STATIC(wallclock_string, 32);
               DSTRING_STATIC(cpu_string, 32);
               DSTRING_STATIC(vmem_string, 32);
               DSTRING_STATIC(maxvmem_string, 32);
               DSTRING_STATIC(rss_string, 32);
               DSTRING_STATIC(maxrss_string, 32);

               double_print_time_to_dstring(wallclock, &wallclock_string, true);
               double_print_time_to_dstring(cpu, &cpu_string, true);
               double_print_memory_to_dstring(vmem, &vmem_string);
               double_print_memory_to_dstring(maxvmem, &maxvmem_string);
               double_print_memory_to_dstring(rss, &rss_string);
               double_print_memory_to_dstring(maxrss, &maxrss_string);
               printf("wallclock=%s,cpu=%s,mem=%-5.5f GBs,io=%-5.5f,ioops=%.0f,vmem=%s,maxvmem=%s,rss=%s,maxrss=%s\n",
                      sge_dstring_get_string(&wallclock_string), sge_dstring_get_string(&cpu_string),
                      mem, io, ioops,
                      (vmem == 0.0) ? "N/A" : sge_dstring_get_string(&vmem_string),
                      (maxvmem == 0.0) ? "N/A" : sge_dstring_get_string(&maxvmem_string),
                      (rss == 0.0) ? "N/A" : sge_dstring_get_string(&rss_string),
                      (maxrss == 0.0) ? "N/A" : sge_dstring_get_string(&maxrss_string));
            }
         }

         // show binding information
         if (show_binding) {
            const lListElem *usage_elem;
            const char *binding_inuse = nullptr;

            if (lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING) {
               for_each_ep(usage_elem, lGetList(jatep, JAT_scaled_usage_list)) {
                  const char *binding_name = "binding_inuse";
                  const char *usage_name = lGetString(usage_elem, UA_name);

                  if (strncmp(usage_name, binding_name, strlen(binding_name)) == 0) {
                     binding_inuse = strstr(usage_name, "!");
                     if (binding_inuse != nullptr) {
                        binding_inuse++;
                     }
                     break;
                  }
               }
               printf("%-16s %11d:   %s\n", "binding",
                      static_cast<int>(lGetUlong(jatep, JAT_task_number)), binding_inuse != nullptr ? binding_inuse : "NONE");
            }
         }

         // show granted host list
         // If we do not have the list then skip the output
         if (lGetPosViaElem(jatep, JAT_granted_destin_identifier_list, SGE_NO_ABORT) >= 0) {
            delis[0] = ":";
            const lList *gdil_org = lGetList(jatep, JAT_granted_destin_identifier_list);
            int queue_fields[] = {JG_qname, JG_slots, 0};
            printf("%-16s %11d:   ", "exec_queue_list", static_cast<int>(lGetUlong(jatep, JAT_task_number)));
            uni_print_list(stdout, nullptr, 0, gdil_org, queue_fields, delis, 0);

            // get the list of granted hosts and make it unique
            lList *gdil_unique = gdil_make_host_unique(gdil_org);

            // skip tasks that have not been scheduled so far
            if (gdil_unique != nullptr) {
               // print the task number and the list of granted hosts
               int host_fields[] = {JG_qhostname, JG_slots, 0};
               printf("%-16s %11d:   ", "exec_host_list", static_cast<int>(lGetUlong(jatep, JAT_task_number)));
               uni_print_list(stdout, nullptr, 0, gdil_unique, host_fields, delis, 0);

               // free the list of granted hosts
               lFreeList(&gdil_unique);
            }
         }

         // show start time of each task
         {
            DSTRING_STATIC(time_ds, 32);

            printf("%-16s %11d:   %s\n", "start_time",
                   static_cast<int>(lGetUlong(jatep, JAT_task_number)),
                   sge_ctime64(lGetUlong64(jatep, JAT_start_time), &time_ds));
         }

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

            printf("%-16s %11d:   %s\n", first_task ? "resource_map" : " ",
                   (int) lGetUlong(jatep, JAT_task_number), resource_ids != nullptr ? resource_ids : "NONE");
            if (first_task) {
               first_task = false;
            }

            sge_dstring_free(&task_resources);
         }

         // show messages (error reasons) for each task
         {
            const lListElem *mesobj;

            for_each_ep(mesobj, lGetList(jatep, JAT_message_list)) {
               const char *message = lGetString(mesobj, QIM_message);

               if (message != nullptr) {
                  printf(SFN " %15d:   %s\n", "error reason",
                         static_cast<int>(lGetUlong(jatep, JAT_task_number)), message);
               }
            }
         }
      }
   }

   DRETURN_VOID;
}

static void sge_show_checkpoint(int how, int op) {
   int i = 0;
   int count = 0;
   dstring string = DSTRING_INIT;

   DENTER(TOP_LAYER);
   job_get_ckpt_attr(op, &string);
   if (VALID(SGE_STDOUT, how)) {
      printf("%s", sge_dstring_get_string(&string));
      for (i = count; i < 4; i++) {
         printf(" ");
      }
   }
   if (VALID(SGE_STDERR, how)) {
      fprintf(stderr, "%s", sge_dstring_get_string(&string));
      for (i = count; i < 4; i++) {
         fprintf(stderr, " ");
      }
   }
   sge_dstring_free(&string);
   DRETURN_VOID;
}

static void sge_show_y_n(int op, int how) {
   stringT tmp_str;

   DENTER(TOP_LAYER);

   if (op)
      snprintf(tmp_str, sizeof(tmp_str), "y");
   else
      snprintf(tmp_str, sizeof(tmp_str), "n");

   if (VALID(how, SGE_STDOUT))
      printf("%s", tmp_str);

   if (VALID(how, SGE_STDERR))
      fprintf(stderr, "%s", tmp_str);

   DRETURN_VOID;
}

void sge_show_ce_type_list(const lList *rel) {
   DENTER(TOP_LAYER);

   show_ce_type_list(rel, "", ",", false, nullptr, 0);

   DRETURN_VOID;
}

/*************************************************************/
/* cel CE_Type List */

/* TODO: EB: this function should be replaced by centry_list_append_to_dstring() */
static void show_ce_type_list(const lList *cel, const char *indent, const char *separator,
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
         printf("%s", separator);
         printf("%s", indent);
      }

      name = lGetString(ce, CE_name);
      if (display_resource_contribution && (centry = centry_list_locate(centry_list, name))) {
         uc = centry_urgency_contribution(slots, name, lGetDouble(ce, CE_doubleval), centry);
      }
      s = lGetString(ce, CE_stringval);
      if (s) {
         if (!display_resource_contribution)
            printf("%s=%s", name, s);
         else
            printf("%s=%s (%f)", name, s, uc);
      } else {
         if (!display_resource_contribution)
            printf("%s", name);
         else
            printf("%s (%f)", name, uc);
      }
   }

   DRETURN_VOID;
}

/*************************************************************/
/* rel CE_Type List */
void sge_show_ce_type_list_line_by_line(const char *label, const char *indent, const lList *rel,
                                        bool display_resource_contribution, const lList *centry_list, int slots) {
   DENTER(TOP_LAYER);

   printf("%s", label);
   show_ce_type_list(rel, indent, "\n", display_resource_contribution, centry_list, slots);
   printf("\n");

   DRETURN_VOID;
}
