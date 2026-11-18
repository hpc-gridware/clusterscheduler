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
 * @brief Plain text rendering of `qstat -j`
 */

#include <sstream>
#include <string>
#include <format>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"
#include "uti/ocs_FormatText.h"

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
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingType.h"
#include "sgeobj/cull/sge_binding_BN_L.h"
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

/** @brief Write a list of complex entries as `name=value` pairs
 * @param os stream to write to
 * @param cel the complex entries
 * @param indent leading whitespace for a continuation line
 * @param separator what goes between two entries
 */
void ocs::QStatJobViewPlain::show_ce_type_list(std::ostream &os, const ocs::tom::ConstList<ocs::tom::CE> cel,
                                               const char *indent, const char *separator) {
   DENTER(TOP_LAYER);

   bool first = true;
   for (const ocs::tom::ConstElement<ocs::tom::CE> ce : cel) {
      if (first) {
         first = false;
      } else {
         os << separator << indent;
      }

      const char *name = ocs::tom::get<CE_name>(ce);
      if (const char *s = ocs::tom::get<CE_stringval>(ce)) {
         os << name << "=" << s;
      } else {
         os << name;
      }
   }

   DRETURN_VOID;
}

/** @brief Report one job in plain text without going through a view object
 *
 * Kept for the callers that have a job and a stream but no view - `qalter -w v`
 * and the job verification path.
 *
 * @param os stream to write to
 * @param ilp the scheduler job info list, holding the reasons a job is not running
 * @param job the job (`JB_Type`)
 * @param flags which parts of the listing were requested
 */
void ocs::QStatJobViewPlain::cull_show_job(std::ostream &os, const ocs::tom::ConstList<ocs::tom::SME> ilp,
                                           const ocs::tom::ConstElement<ocs::tom::JB> job, const int flags) {
   DENTER(TOP_LAYER);
   // create dummy params and view so that we can call virtual methods in this static function
   const ProcedureParameter parameter("", nullptr);
   QStatJobViewPlain view(parameter);

   view.show_job(os, ilp, job, flags);
   DRETURN_VOID;
}


/**
 * @brief Report why the pending jobs are not running, as plain text
 *
 * The global messages of the scheduler come first. The job related messages
 * are then merged by message number - a job id named twice for the same
 * number is dropped - and printed as the message text followed by its job ids.
 * The merged list replaces the message list of the model. The message list and
 * the job ids are meant to be sorted, but both sort orders lack their '%' and
 * sort by nothing.
 *
 * @param os stream to write to
 * @param parameter the call's parameters
 * @param model the fetched lists; its message list is replaced by the merged one
 * @return none
 */
void
ocs::QStatJobViewPlain::show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);

   if (const auto sme = model.ilp->front(); sme != nullptr) {
      int first_row = 1;

      /* print global schduling info */
      bool first_run = true;
      for (const auto mes : ocs::tom::get<SME_global_message_list>(sme)) {
         if (first_run) {
            os << std::format("{:<{}} {}\n", ocs::ascii_width(MSG_SCHEDD_SCHEDULINGINFO), left_width,
                              ocs::tom::get<MES_message>(mes));
            first_run = false;
         } else {
            os << std::format("{:<{}} {}\n", ocs::ascii_width(""), left_width, ocs::tom::get<MES_message>(mes));
         }
      }

      auto mlp = ocs::tom::get<SME_message_list>(sme);
      // "I+" lacks the '%', so this sorts by no key and leaves the order as it is; kept raw rather than turned
      // into a real sort, which would change the output
      lPSortList(mlp, "I+", MES_message_number);

      /*
       * Remove all jids which have more than one entry for a MES_message_number
       * After this step the MES_messages are not correct anymore
       * We do not need this messages for the summary output
       */
      {
         auto new_list = ocs::tom::ListPtr<ocs::tom::MES>::create("filtered message list");
         auto flt_nxt_msg = mlp.front();

         while (flt_nxt_msg != nullptr) {
            const auto flt_msg = flt_nxt_msg;

            bool found_msg = false;
            flt_nxt_msg = flt_msg.next();
            for (const auto ref_msg : *new_list) {
               if (ocs::tom::get<MES_message_number>(ref_msg) == ocs::tom::get<MES_message_number>(flt_msg)) {
                  const auto flt_jids = ocs::tom::get<MES_job_number_list>(flt_msg);
                  const auto ref_jids = ocs::tom::get<MES_job_number_list>(ref_msg);
                  auto flt_nxt_jid = flt_jids.front();
                  while (flt_nxt_jid != nullptr) {
                     const auto flt_jid = flt_nxt_jid;
                     flt_nxt_jid = flt_jid.next();

                     bool found_jid = false;
                     for (const auto ref_jid : ref_jids) {
                        if (ocs::tom::get<ULNG_value>(ref_jid) == ocs::tom::get<ULNG_value>(flt_jid)) {
                           flt_jids.erase(flt_jid);
                           found_jid = true;
                           break;
                        }
                     }
                     if (!found_jid) {
                        auto moved_jid = flt_jids.extract(flt_jid);
                        // a message without a job list cannot take it; the id is dropped, as before
                        if (ref_jids != nullptr) {
                           ref_jids.push_back(std::move(moved_jid));
                        }
                     }
                  }
                  found_msg = true;
               }
            }
            if (!found_msg) {
               new_list->push_back(mlp.extract(flt_msg));
            }
         }
         const auto filtered = *new_list;
         ocs::tom::set<SME_message_list>(sme, std::move(new_list));
         mlp = filtered;
      }

      std::string text;
      uint32_t last_jid = 0;
      uint32_t last_mid = 0;
      int ids_per_line = 0;
      int initialized = 0;
      first_run = true;
      for (const auto mes : mlp) {
         // a no-op as well, see above
         lPSortList(ocs::tom::get<MES_job_number_list>(mes), "I+", ULNG_value);

         for (const auto jid_ulng : ocs::tom::get<MES_job_number_list>(mes)) {
            bool skip = false;
            bool header = false;
            const uint32_t mid = ocs::tom::get<MES_message_number>(mes);
            const uint32_t jid = ocs::tom::get<ULNG_value>(jid_ulng);

            if (initialized) {
               if (last_mid == mid && last_jid == jid) {
                  skip = true;
               } else if (last_mid != mid) {
                  header = true;
               }
            } else {
               initialized = 1;
               header = true;
            }
            if (text.size() >= MAX_LINE_LEN || ids_per_line >= MAX_IDS_PER_LINE || header) {
               os << text;
               text.clear();
               ids_per_line = 0;
               first_row = 0;
            }

            if (header) {
               if (!first_run) {
                  os << "\n\n";
               } else {
                  first_run = false;
               }
               os << sge_schedd_text(mid + SCHEDD_INFO_OFFSET) << "\n";
               first_row = 1;
            }

            if (!skip) {
               if (ids_per_line == 0) {
                  text += first_row ? "\t" : ",\n\t";
               } else {
                  text += ",\t";
               }
               text += std::to_string(jid);
               ids_per_line++;
            }

            last_jid = jid;
            last_mid = mid;
         }
      }
      if (!text.empty()) {
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

void ocs::QStatJobViewPlain::report_job_separator(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << "==============================================================\n";
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_job_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatJobViewPlain::report_job_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Report the job number, as plain text
 * @param os stream to write to
 * @param job the job
 * @param flags which parts of the listing were requested
 */
void ocs::QStatJobViewPlain::report_job_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                           int flags) {
   DENTER(TOP_LAYER);
   if (!(flags & FLG_QALTER)) {
      uint32_t jid = ocs::tom::get<JB_job_number>(job);
      if (jid > 0) {
         os << std::format("{:<{}} {}", ocs::ascii_width("job_number:"), left_width, jid) << "\n";
      } else {
         os << std::format("{:<{}} {}", ocs::ascii_width("job_number:"), left_width, MSG_JOB_UNASSIGNED) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the scheduling category the job was sorted into, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_category_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_category_id>(job)) {
      os << std::format("{:<{}} {}", ocs::ascii_width("category_id:"), left_width,
                        ocs::tom::get<JB_category_id>(job)) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the job script as it was spooled, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_exec_file(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_exec_file>(job)) {
      if (const char *exec_file = ocs::tom::get<JB_exec_file>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("exec_file:"), left_width, exec_file) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report when the job was submitted, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_submission_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (ocs::tom::has<JB_submission_time>(job)) {
      if (const uint64_t time_value = ocs::tom::get<JB_submission_time>(job); time_value > 0) {
         os << std::format("{:<{}} {}", ocs::ascii_width("submission_time:"), left_width,
                           sge_ctime64(time_value, &dstr)) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the deadline of a deadline job, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_deadline_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (ocs::tom::has<JB_deadline>(job)) {
      if (const uint64_t time_value = ocs::tom::get<JB_deadline>(job); time_value > 0) {
         os << std::format("{:<{}} {}", ocs::ascii_width("deadline:"), left_width,
                           sge_ctime64(time_value, &dstr)) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the command line the job was submitted with, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_submit_cmd_line(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_submission_command_line>(job)) {
      if (const char *str = ocs::tom::get<JB_submission_command_line>(job); str != nullptr) {
         os << std::format("{:<{}} {}", ocs::ascii_width("submit_cmd_line:"), left_width, str) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the command line after defaults and `sge_request` files were applied, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_effective_submit_cmd_line(std::ostream &os,
                                                              const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_submission_command_line>(job)) {
      if (const char *str = ocs::tom::get<JB_submission_command_line>(job); str != nullptr) {
         char *copied_str = strdup(str);
         char *saveptr = nullptr;
         if (const char *command = strtok_r(copied_str, " ", &saveptr); command != nullptr) {
            dstring dstr_cmd = DSTRING_INIT;
            os << std::format("{:<{}} {}", ocs::ascii_width("effective_submit_cmd_line:"), left_width,
                              job_get_effective_command_line(job, &dstr_cmd, command)) << "\n";
            sge_dstring_free(&dstr_cmd);
         }
         sge_free(&copied_str);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the submitting user and group, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ownership(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_owner>(job)) {
      if (const char *owner = ocs::tom::get<JB_owner>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("owner:"), left_width, owner) << "\n";
      } else {
         os << std::format("{:<{}} {}", ocs::ascii_width("owner:"), left_width, "") << "\n";
      }
   }

   if (ocs::tom::has<JB_uid>(job)) {
      os << std::format("{:<{}} {}", ocs::ascii_width("uid:"), left_width, (int) ocs::tom::get<JB_uid>(job)) << "\n";
   }

   if (ocs::tom::has<JB_group>(job)) {
      if (const char *group = ocs::tom::get<JB_group>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("group:"), left_width, group) << "\n";
      } else {
         os << std::format("{:<{}} {}", ocs::ascii_width("group:"), left_width, "") << "\n";
      }
   }

   if (ocs::tom::has<JB_gid>(job)) {
      os << std::format("{:<{}} {}", ocs::ascii_width("gid:"), left_width, ocs::tom::get<JB_gid>(job)) << "\n";
   }

   if (ocs::tom::has<JB_grp_list>(job)) {
      std::stringstream ss_groups;
#if defined(WITH_EXTENSIONS)
      ocs::tom::ConstList<ocs::tom::ST> grp_list = ocs::tom::get<JB_grp_list>(job);

      if (grp_list == nullptr) {
         ss_groups << "NONE";
      } else {
         bool first = true;

         for (ocs::tom::ConstElement<ocs::tom::ST> grp_elem : grp_list) {
            if (first) {
               first = false;
            } else {
               ss_groups << ",";
            }
            ss_groups << std::format("{}({})", ocs::tom::get<ST_id>(grp_elem), ocs::tom::get<ST_name>(grp_elem));
         }
      }
#else
      ss_groups << "NOT-AVAILABLE-IN-OCS";
#endif
      os << std::format("{:<{}} {}", ocs::ascii_width("groups:"), left_width, ss_groups.str()) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the parts of the environment the system itself sets, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_env_core(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const char *name[] = {"O_HOME", "O_LOGNAME", "O_PATH", "O_SHELL", "O_TZ", "O_WORKDIR", "O_HOST", nullptr};
   const char *fmt_string[] = {
      "sge_o_home:", "sge_o_log_name:", "sge_o_path:", "sge_o_shell:", "sge_o_tz:", "sge_o_workdir:", "sge_o_host:",
      nullptr
   };
   int i = -1;

   while (name[++i] != nullptr) {
      char fullname[MAX_STRING_SIZE];
      snprintf(fullname, sizeof(fullname), "%s%s", VAR_PREFIX, name[i]);
      if (const char *value = job_get_env_string(job, fullname)) {
         os << std::format("{:<{}} {}", ocs::ascii_width(fmt_string[i]), left_width, value) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the earliest time the job may start, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_execution_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_execution_time>(job)) {
      DSTRING_STATIC(dstr, 128);
      if (const uint64_t time_value = ocs::tom::get<JB_execution_time>(job); time_value > 0) {
         os << std::format("{:<{}} {}", ocs::ascii_width("execution_time:"), left_width,
                           sge_ctime64(time_value, &dstr)) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the accounting string, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_account(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_account>(job)) {
      if (const char *account = ocs::tom::get<JB_account>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("account:"), left_width, account) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the checkpointing environment, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_checkpoint(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_checkpoint_name>(job)) {
      if (const char *ckpt_name = ocs::tom::get<JB_checkpoint_name>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("checkpoint_object:"), left_width, ckpt_name) << "\n";
      }
   }

   if (ocs::tom::has<JB_checkpoint_attr>(job)) {
      if (uint32_t ckpt_attr = ocs::tom::get<JB_checkpoint_attr>(job)) {
         std::stringstream ss_ckpt_attr;
         job_get_ckpt_attr(ss_ckpt_attr, ckpt_attr);
         os << std::format("{:<{}} {}", ocs::ascii_width("checkpoint_attr:"), left_width, ss_ckpt_attr.str()) << "\n";
      }
   }

   if (ocs::tom::has<JB_checkpoint_interval>(job)) {
      if (uint32_t ckpt_int = ocs::tom::get<JB_checkpoint_interval>(job)) {
         os << std::format("{:<{}} {} seconds", ocs::ascii_width("checkpoint_interval:"), left_width, ckpt_int) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the working directory, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_cwd(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_cwd>(job)) {
      if (const char *cwd = ocs::tom::get<JB_cwd>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("cwd:"), left_width, cwd) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the path aliases in effect, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_path_aliases(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_cwd>(job)) {
      if (const char *cwd = ocs::tom::get<JB_cwd>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("cwd:"), left_width, cwd) << "\n";
      }
      if (ocs::tom::has<JB_path_aliases>(job)) {
         if (ocs::tom::ConstList<ocs::tom::PA> path_aliases = ocs::tom::get<JB_path_aliases>(job)) {
            std::ostringstream ss_path_aliases;
            delis[0] = " ";
            int fields[] = {PA_origin, PA_submit_host, PA_exec_host, PA_translation, 0};
            uni_print_list(ss_path_aliases, path_aliases, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", ocs::ascii_width("path_aliases:"), left_width, ss_path_aliases.str());
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the prefix that marks directives in the job script, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_directive_prefix(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_directive_prefix>(job)) {
      if (const char *prefix = ocs::tom::get<JB_directive_prefix>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("directive_prefix:"), left_width, prefix) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report where standard input comes from, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_stdin_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_stdin_path_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::PN> list = ocs::tom::get<JB_stdin_path_list>(job)) {
         std::ostringstream ss_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);

         os << std::format("{:<{}} {}", ocs::ascii_width("stdin_path_list:"), left_width, ss_list.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report where standard output goes, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_stdout_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_stdout_path_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::PN> list = ocs::tom::get<JB_stdout_path_list>(job)) {
         std::ostringstream ss_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", ocs::ascii_width("stdout_path_list:"), left_width, ss_list.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report where standard error goes, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_stderr_path_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_stderr_path_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::PN> path_list = ocs::tom::get<JB_stderr_path_list>(job)) {
         std::ostringstream ss_path_list;
         delis[0] = ":";
         int fields[] = {PN_host, PN_file_host, PN_path, PN_file_staging, 0};
         uni_print_list(ss_path_list, path_list, fields, delis, FLG_NO_DELIS_STRINGS);

         os << std::format("{:<{}} {}", ocs::ascii_width("stderr_path_list:"), left_width, ss_path_list.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report whether the job reserves resources while it waits, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_reserve(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_reserve>(job)) {
      if (const bool reserve = ocs::tom::get<JB_reserve>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("reserve:"), left_width, reserve ? "y" : "n") << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report whether standard error is merged into standard output, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_merge_stderr(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_merge_stderr>(job)) {
      if (const bool merge = ocs::tom::get<JB_merge_stderr>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("merge:"), left_width, merge ? "y" : "n") << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the hard and soft resource requests, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_request_set_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_request_set_list>(job)) {
      ocs::tom::ConstList<ocs::tom::JRS> jrs_list = ocs::tom::get<JB_request_set_list>(job);
      for (ocs::tom::ConstElement<ocs::tom::JRS> jrs : jrs_list) {
         uint32_t scope = ocs::tom::get<JRS_scope>(jrs);

         if (ocs::tom::ConstList<ocs::tom::CE> lp = ocs::tom::get<JRS_hard_resource_list>(jrs)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_hard_resource_list, true);

            std::ostringstream ss_list;
            show_ce_type_list(ss_list, lp, "", ",");

            os << std::format("{:<{}} {}", ocs::ascii_width(str_attrib), left_width, ss_list.str()) << "\n";
         }

         if (ocs::tom::ConstList<ocs::tom::CE> lp = ocs::tom::get<JRS_soft_resource_list>(jrs)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_soft_resource_list, true);

            std::ostringstream ss_list;
            show_ce_type_list(ss_list, lp, "", ",");

            os << std::format("{:<{}} {}", ocs::ascii_width(str_attrib), left_width, ss_list.str()) << "\n";
         }

         if (ocs::tom::ConstList<ocs::tom::QR> lp = ocs::tom::get<JRS_hard_queue_list>(jrs)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_hard_queue_list, true);
            delis[0] = " ";

            std::ostringstream ss_list;
            int fields[] = {QR_name, 0};
            uni_print_list(ss_list, lp, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", ocs::ascii_width(str_attrib), left_width, ss_list.str());
         }

         if (ocs::tom::ConstList<ocs::tom::QR> lp = ocs::tom::get<JRS_soft_queue_list>(jrs)) {
            std::string str_attrib = get_scope_list_name(scope, JRS_soft_queue_list, true);
            delis[0] = " ";

            std::ostringstream ss_list;
            int fields[] = {QR_name, 0};
            uni_print_list(ss_list, lp, fields, delis, FLG_NO_DELIS_STRINGS);

            os << std::format("{:<{}} {}", ocs::ascii_width(str_attrib), left_width, ss_list.str());
         }

         const char *allocation_rule = ocs::tom::get<JRS_allocation_rule>(jrs);
         if (allocation_rule != nullptr) {
            std::string str_attrib = get_scope_list_name(scope, JRS_allocation_rule, true);

            os << std::format("{:<{}} {}", ocs::ascii_width(str_attrib), left_width, allocation_rule) << "\n";
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report which job events trigger a mail, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_mail_options(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_mail_options>(job)) {
      if (const uint32_t mail_options = ocs::tom::get<JB_mail_options>(job)) {
         dstring mailopt = DSTRING_INIT;
         sge_dstring_append_mailopt(&mailopt, mail_options);
         os << std::format("{:<{}} {}", ocs::ascii_width("mail_options:"), left_width,
                           sge_dstring_get_string(&mailopt)) << "\n";
         sge_dstring_free(&mailopt);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report who is mailed about the job, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_mail_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_mail_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::MR> mail_list = ocs::tom::get<JB_mail_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {MR_user, MR_host, 0};
         delis[0] = "@";
         uni_print_list(ss_list, mail_list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} ", ocs::ascii_width("mail_list:"), left_width) << ss_list.str();
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report whether the job is warned before it is signalled, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_notify(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_notify>(job)) {
      os << std::format("{:<{}} ", ocs::ascii_width("notify:"), left_width)
         << (ocs::tom::get<JB_notify>(job) ? "TRUE" : "FALSE") << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the job name, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_name(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_job_name>(job)) {
      os << std::format("{:<{}} ", ocs::ascii_width("job_name:"), left_width) << (ocs::tom::get<JB_job_name>(job)
                                                                   ? ocs::tom::get<JB_job_name>(job)
                                                                   : "") << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the priority the job was submitted with, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_priority(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_priority>(job)) {
      os << std::format("{:<{}} {}", ocs::ascii_width("priority:"), left_width,
                        static_cast<int>(ocs::tom::get<JB_priority>(job)) - BASE_PRIORITY) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the share the job was submitted with, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_job_share(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_jobshare>(job)) {
      os << std::format("{:<{}} {}", ocs::ascii_width("jobshare:"), left_width,
                        ocs::tom::get<JB_jobshare>(job)) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report whether the job may be restarted after a failure, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_restart(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_restart>(job)) {
      if (const uint32_t restart = ocs::tom::get<JB_restart>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("restart:"), left_width, (restart == 2) ? "n" : "y") << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the shell the job runs under, per host, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_shell_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_shell_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::PN> list = ocs::tom::get<JB_shell_list>(job)) {
         std::ostringstream ss_list;
         delis[0] = ":";
         constexpr int fields[] = {PN_host, PN_path, 0};
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", ocs::ascii_width("shell_list:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Emit the job's environment (JB_env_list) in plain qstat -j output.
 *
 * SECURITY (CS-2355, MEDIUM-QSTAT-001): the environment is printed verbatim
 * (after splitting off the internal VAR_PREFIX variables) with no redaction, so
 * secrets captured at submit time — e.g. via qsub -V (AWS_SECRET_ACCESS_KEY,
 * *_TOKEN, *_PASSWORD, …) — are shown to any user permitted to qstat -j this
 * job. Whether and how to redact (denylist and/or owner/manager gating) is a
 * pending site-policy decision tracked in CS-2355; no redaction is applied here
 * yet. If this is changed, change the JSON view too
 * (QStatJobViewJSON::report_env_list); the XML view currently emits nothing.
 *
 * @param[in,out] os  output stream
 * @param[in]     job job element to report
 */
void ocs::QStatJobViewPlain::report_env_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_env_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::VA> env_list = ocs::tom::get<JB_env_list>(job)) {
         // the variables with VAR_PREFIX are not shown: they are split off a copy, since the job is the caller's
         const auto list = env_list.clone();
         ocs::tom::ListPtr<ocs::tom::VA> do_not_print;
         var_list_split_prefix_vars(*list, do_not_print, VAR_PREFIX);

         if (list->empty()) {
            os << std::format("{:<{}} {}", ocs::ascii_width("env_list:"), left_width, "NONE") << "\n";
         } else {
            std::ostringstream ss_list;
            delis[0] = "=";
            constexpr int fields[] = {VA_variable, VA_value, 0};
            uni_print_list(ss_list, *list, fields, delis, FLG_NO_DELIS_STRINGS | FLG_NO_VALUE_AS_EMPTY);
            os << std::format("{:<{}} {}", ocs::ascii_width("env_list:"), left_width, ss_list.str()) << "\n";
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the verification mode the job was submitted with, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_verify(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_verify>(job)) {
      if (ocs::tom::get<JB_verify>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("verify:"), left_width, "-verify") << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the arguments passed to the job script, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_job_args(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_job_args>(job)) {
      if (ocs::tom::ConstList<ocs::tom::ST> list = ocs::tom::get<JB_job_args>(job); list != nullptr) {
         std::ostringstream ss_list;
         delis[0] = "";
         int fields[] = {ST_name, 0};
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("job_args:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the tasks of an array job that were selected, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_job_identifier_list(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_job_identifier_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::ID> list = ocs::tom::get<JB_job_identifier_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("job_identifier_list:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the size of the spooled job script, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_script_size(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_script_size>(job)) {
      if (const uint32_t size = ocs::tom::get<JB_script_size>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("script_size:"), left_width, size) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the name of the job script, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_script_file(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_script_file>(job)) {
      if (const char *file = ocs::tom::get<JB_script_file>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("script_file:"), left_width, file) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the job script itself, for a script read from standard input, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_script_ptr(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_script_ptr>(job)) {
      if (const char *file = ocs::tom::get<JB_script_ptr>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("script_ptr:"), left_width, file) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the parallel environment and the slot range requested, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_pe(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_pe>(job)) {
      if (const char *pe = ocs::tom::get<JB_pe>(job)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(ocs::tom::get<JB_pe_range>(job), &range_string, true, false, false);
         std::stringstream ss_pe_details;
         ss_pe_details << pe << " range: " << sge_dstring_get_string(&range_string);
         sge_dstring_free(&range_string);

         os << std::format("{:<{}} {}", ocs::ascii_width("parallel_environment:"), left_width,
                           ss_pe_details.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the jobs this one was asked to wait for, as they were named, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_jid_request_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_jid_request_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::JRE> list = ocs::tom::get<JB_jid_request_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("jid_request_list (req):"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the jobs this one is still waiting for, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_jid_predecessor_list(std::ostream &os,
                                                         const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_jid_predecessor_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::JRE> list = ocs::tom::get<JB_jid_predecessor_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("jid_predecessor_list:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the jobs waiting for this one, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_jid_successor_list(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_jid_successor_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::JRE> list = ocs::tom::get<JB_jid_successor_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("jid_successor_list:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the array dependencies as they were named, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ja_ad_request_list(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ja_ad_request_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::JRE> list = ocs::tom::get<JB_ja_ad_request_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("ja_ad_request_list (req):"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the array jobs whose tasks this one is waiting for, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ja_ad_predecessor_list(std::ostream &os,
                                                           const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ja_ad_predecessor_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::JRE> list = ocs::tom::get<JB_ja_ad_predecessor_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("ja_ad_predecessor_list:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the array jobs whose tasks wait for this one, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ja_ad_successor_list(std::ostream &os,
                                                         const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ja_ad_successor_list>(job)) {
      if (ocs::tom::ConstList<ocs::tom::JRE> list = ocs::tom::get<JB_ja_ad_successor_list>(job)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", ocs::ascii_width("ja_ad_successor_list:"), left_width, ss_list.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report which queues the job could run in, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_verify_suitable_queues(std::ostream &os,
                                                           const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_verify_suitable_queues>(job)) {
      if (const uint32_t vsq = ocs::tom::get<JB_verify_suitable_queues>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("verify_suitable_queues:"), left_width, vsq) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the soft wallclock limit, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_soft_wallclock_gmt(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (ocs::tom::has<JB_soft_wallclock_gmt>(job)) {
      if (const uint64_t time_value = ocs::tom::get<JB_soft_wallclock_gmt>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("soft_wallclock_gmt:"), left_width,
                           sge_ctime64(time_value, &dstr)) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the hard wallclock limit, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_hard_wallclock_gmt(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(dstr, 128);
   if (ocs::tom::has<JB_hard_wallclock_gmt>(job)) {
      if (const uint64_t time_value = ocs::tom::get<JB_hard_wallclock_gmt>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("hard_wallclock_gmt:"), left_width,
                           sge_ctime64(time_value, &dstr)) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the job version, which counts how often the job was modified, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_version(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_version>(job)) {
      if (uint32_t version = ocs::tom::get<JB_version>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("version:"), left_width, version) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the tickets a manager granted the job, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_override_tickets(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_override_tickets>(job)) {
      if (uint32_t tickets = ocs::tom::get<JB_override_tickets>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("override_tickets:"), left_width, tickets) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the advance reservation the job runs in, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ar(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ar>(job)) {
      if (uint32_t ar_id = ocs::tom::get<JB_ar>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("ar_id:"), left_width, ar_id) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the project the job is accounted to, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_project(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_project>(job)) {
      if (const char *project = ocs::tom::get<JB_project>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("project:"), left_width, project) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the department the submitting user belongs to, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_department(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_department>(job)) {
      if (const char *dept = ocs::tom::get<JB_department>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("department:"), left_width, dept) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report whether the client waits for the job, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_sync_options(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_sync_options>(job)) {
      if (ocs::tom::get<JB_sync_options>(job)) {
         std::string sync_flags = job_get_sync_options_string(job);
         os << std::format("{:<{}} {}", ocs::ascii_width("sync_options:"), left_width, sync_flags) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the task range of an array job, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ja_structure(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ja_structure>(job)) {
      if (job_is_array(job)) {
         const auto [start, end, step] = job_get_submit_task_ids(job);
         std::ostringstream ss_range;
         ss_range << std::format("{}-{}:{}", start, end, step);
         os << std::format("{:<{}} {}", ocs::ascii_width("job-array tasks:"), left_width, ss_range.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report how many tasks of an array job have not started yet, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_pending_tasks(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ja_n_h_ids>(job)) {
      os << std::format("{:<{}} {}", ocs::ascii_width("pending_tasks:"), left_width, count_pending_tasks(job)) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report how many tasks of an array job may run at once, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ja_task_concurrency(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_ja_task_concurrency>(job)) {
      if (uint32_t tc = ocs::tom::get<JB_ja_task_concurrency>(job)) {
         os << std::format("{:<{}} {}", ocs::ascii_width("task_concurrency:"), left_width, tc) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the job context, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_ctx_list(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_context>(job)) {
      if (ocs::tom::ConstList<ocs::tom::VA> list = ocs::tom::get<JB_context>(job)) {
         std::ostringstream ss_list;
         constexpr int fields[] = {VA_variable, VA_value, 0};
         delis[0] = "=";
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", ocs::ascii_width("context:"), left_width, ss_list.str()) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report the requested core binding, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_binding(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (ocs::tom::has<JB_binding>(job)) {
      const auto binding_elem = ocs::tom::get<JB_binding>(job);
      std::string binding_param;;
      BindingIo::binding_print_to_string(binding_elem, binding_param);
      os << std::format("{:<{}} {}", ocs::ascii_width("binding:"), left_width, binding_param) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Begin the tasks of the current job, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_task_list_started(std::ostream &os,
                                                      const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief End the task list, as plain text
 * @param os stream to write to
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_task_list_finished(std::ostream &os,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Begin one task, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_started(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                 const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief End the current task, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_finished(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                  const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Report the task number, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_id(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                            const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Report the state of the task, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_state(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                               const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);

   char state_string[8];
   const uint32_t state = jatask_combine_state_and_status_for_output(job, task);
   job_get_state_string(state_string, state);

   os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("job_state"), left_width_short, task_number, mid_width,
                     state_string) << "\n";
   DRETURN_VOID;
}

/**
 * @brief Report the resources the task has consumed, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_usage(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                               const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);

   /* jobs whose job start orders were not processed to the end due to a qmaster/schedd collision appear
    * in the JB_ja_tasks list but are not running - thus we may not print usage for those */
   if (ocs::tom::get<JAT_status>(task) != JRUNNING && ocs::tom::get<JAT_status>(task) != JTRANSFERING) {
      DRETURN_VOID;
   }

   // Collect data
   Usage usage = {};
   accumulate_usage(task, usage);

   // Show data
   DSTRING_STATIC(wallclock_string, 32);
   DSTRING_STATIC(cpu_string, 32);
   DSTRING_STATIC(vmem_string, 32);
   DSTRING_STATIC(maxvmem_string, 32);
   DSTRING_STATIC(rss_string, 32);
   DSTRING_STATIC(maxrss_string, 32);
   DSTRING_STATIC(iow_string, 32);

   double_print_time_to_dstring(usage.wallclock, &wallclock_string, true);
   double_print_time_to_dstring(usage.cpu, &cpu_string, true);
   double_print_memory_to_dstring(usage.vmem, &vmem_string);
   double_print_memory_to_dstring(usage.maxvmem, &maxvmem_string);
   double_print_memory_to_dstring(usage.rss, &rss_string);
   double_print_memory_to_dstring(usage.maxrss, &maxrss_string);
   double_print_time_to_dstring(usage.iow, &iow_string, true);

   std::stringstream ss_usage;
   ss_usage << std::format(
      "wallclock={},cpu={},mem={:<5.5f} GBs,io={:<5.5f},ioops={:.0f},iow={},vmem={},maxvmem={},rss={},maxrss={}",
      sge_dstring_get_string(&wallclock_string), sge_dstring_get_string(&cpu_string),
      usage.mem, usage.io, usage.ioops, sge_dstring_get_string(&iow_string),
      (usage.vmem == 0.0) ? "N/A" : sge_dstring_get_string(&vmem_string),
      (usage.maxvmem == 0.0) ? "N/A" : sge_dstring_get_string(&maxvmem_string),
      (usage.rss == 0.0) ? "N/A" : sge_dstring_get_string(&rss_string),
      (usage.maxrss == 0.0) ? "N/A" : sge_dstring_get_string(&maxrss_string));

   if (usage.have_mem_details) {
      DSTRING_STATIC(pss_string, 32);
      DSTRING_STATIC(maxpss_string, 32);
      DSTRING_STATIC(pmem_string, 32);
      DSTRING_STATIC(smem_string, 32);

      double_print_memory_to_dstring(usage.pss, &pss_string);
      double_print_memory_to_dstring(usage.maxpss, &maxpss_string);
      double_print_memory_to_dstring(usage.pmem, &pmem_string);
      double_print_memory_to_dstring(usage.smem, &smem_string);
      ss_usage << std::format(",pss={},maxpss={},pmem={},smem={}",
                              sge_dstring_get_string(&pss_string), sge_dstring_get_string(&maxpss_string),
                              sge_dstring_get_string(&pmem_string), sge_dstring_get_string(&smem_string));
   }

   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);
   os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("usage"), left_width_short, task_number, mid_width,
                     ss_usage.str()) << "\n";
   DRETURN_VOID;
}

/**
 * @brief Report the cores the task was bound to, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_exec_binding_list(std::ostream &os,
                                                           const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                           const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);

   // show binding information
   if (ocs::tom::get<JAT_status>(task) == JRUNNING || ocs::tom::get<JAT_status>(task) == JTRANSFERING) {
      ocs::tom::ConstList<ocs::tom::GRU> granted_resources = ocs::tom::get<JAT_granted_resources_list>(task);
      os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("exec_binding_list"), left_width_short, task_number,
                        mid_width, GrantedResources::to_string(granted_resources)) << "\n";
   }
   DRETURN_VOID;
}

/**
 * @brief Report the queue instances the task runs in, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_exec_queue_list(std::ostream &os,
                                                         const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                         const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);

   // show granted host list
   // If we do not have the list then skip the output
   if (ocs::tom::has<JAT_granted_destin_identifier_list>(task)) {
      delis[0] = "=";
      ocs::tom::ConstList<ocs::tom::JG> gdil_org = ocs::tom::get<JAT_granted_destin_identifier_list>(task);
      std::stringstream ss_list;
      int queue_fields[] = {JG_qname, JG_slots, 0};
      uni_print_list(ss_list, gdil_org, queue_fields, delis, 0);
      uint32_t task_number = ocs::tom::get<JAT_task_number>(task);
      os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("exec_queue_list"), left_width_short, task_number,
                        mid_width, ss_list.str());
   }
   DRETURN_VOID;
}

/**
 * @brief Report the hosts the task runs on, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_exec_host_list(std::ostream &os,
                                                        const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                        const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);

   // show granted host list
   // If we do not have the list then skip the output
   if (ocs::tom::has<JAT_granted_destin_identifier_list>(task)) {
      delis[0] = "=";
      ocs::tom::ConstList<ocs::tom::JG> gdil_org = ocs::tom::get<JAT_granted_destin_identifier_list>(task);

      // get the list of granted hosts and make it unique
      // skip tasks that have not been scheduled so far
      if (const auto gdil_unique = gdil_make_host_unique(gdil_org)) {
         // print the task number and the list of granted hosts
         std::stringstream ss_list2;
         const int host_fields[] = {JG_qhostname, JG_slots, 0};
         uni_print_list(ss_list2, gdil_unique.get(), host_fields, delis, 0);
         os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("exec_host_list"), left_width_short, task_number,
                           mid_width, ss_list2.str());
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report when the task started, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_start_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                    const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);
   DSTRING_STATIC(time_ds, 32);
   os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("start_time"), left_width_short, task_number, mid_width,
                     sge_ctime64(ocs::tom::get<JAT_start_time>(task), &time_ds)) << "\n";
   DRETURN_VOID;
}

/**
 * @brief Report when the task finished, as plain text
 *
 * CS-1908 retention: emit end_time only for retained finished ja_tasks
 * (JAT_status == JFINISHED). For live/pending rows the field is either 0 or
 * an execd-side h_rt projection that would be misleading here.
 *
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_end_time(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                  const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   if (ocs::tom::get<JAT_status>(task) != JFINISHED) {
      DRETURN_VOID;
   }
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);
   DSTRING_STATIC(time_ds, 32);
   os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("end_time"), left_width_short, task_number, mid_width,
                     sge_ctime64(ocs::tom::get<JAT_end_time>(task), &time_ds)) << "\n";
   DRETURN_VOID;
}

/**
 * @brief Report the resource map ids the task was granted, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_resource_map(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                      const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   dstring task_resources = DSTRING_INIT;

   /* go through all RSMAP resources for the particular task and create a string */
   bool is_first_remap = true;
   for (ocs::tom::ConstElement<ocs::tom::GRU> resu : ocs::tom::get<JAT_granted_resources_list>(task)) {
      if (static_cast<GrantedResources::Type>(ocs::tom::get<GRU_type>(resu)) ==
          GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
         const char *name = ocs::tom::get<GRU_name>(resu);
         const char *host = ocs::tom::get<GRU_host>(resu);
         ocs::tom::ConstList<ocs::tom::RESL> ids = ocs::tom::get<GRU_resource_map_list>(resu);

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
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);
   const char *resource_ids = sge_dstring_get_string(&task_resources);
   os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("resource_map"), left_width_short, task_number, mid_width,
                     resource_ids != nullptr ? resource_ids : "NONE") << "\n";

   sge_dstring_free(&task_resources);
   DRETURN_VOID;
}

/**
 * @brief Report why the task failed, as plain text
 * @param os stream to write to
 * @param job the job
 * @param task the array task the row is about
 */
void ocs::QStatJobViewPlain::report_task_error_reason(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                      const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   DENTER(TOP_LAYER);
   uint32_t task_number = ocs::tom::get<JAT_task_number>(task);
   for (ocs::tom::ConstElement<ocs::tom::QIM> mesobj : ocs::tom::get<JAT_message_list>(task)) {
      if (const char *message = ocs::tom::get<QIM_message>(mesobj)) {
         os << std::format("{:<{}} {:>{}}: {}", ocs::ascii_width("error_reason"), left_width_short, task_number,
                           mid_width, message) << "\n";
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report why the job has not been scheduled yet, as plain text
 * @param os stream to write to
 * @param ilp the scheduler job info list (`SME_Type`), holding the reasons a job is not running
 * @param job the job
 */
void ocs::QStatJobViewPlain::report_schedd_job_info(std::ostream &os, const ocs::tom::ConstList<ocs::tom::SME> ilp,
                                                    const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   if (const ocs::tom::ConstElement<ocs::tom::SME> sme = ilp.front(); sme != nullptr) {
      // Local lambda to print a message with correct prefix/indent
      bool first_run = true;
      auto print_sched_message = [&](const char *msg) {
         if (first_run) {
            os << std::format("{:<{}} {}", ocs::ascii_width(MSG_SCHEDD_SCHEDULINGINFO), left_width, msg) << "\n";
            first_run = false;
         } else {
            os << std::format("{:<{}} {}", ocs::ascii_width(""), left_width, msg) << "\n";
         }
      };

      /* global scheduling info */
      for (ocs::tom::ConstElement<ocs::tom::MES> mes : ocs::tom::get<SME_global_message_list>(sme)) {
         print_sched_message(ocs::tom::get<MES_message>(mes));
      }

      /* job scheduling info */
      const uint32_t jid = ocs::tom::get<JB_job_number>(job);
      for (ocs::tom::ConstElement<ocs::tom::MES> mes : ocs::tom::get<SME_message_list>(sme)) {
         for (ocs::tom::ConstElement<ocs::tom::ULNG> mes_jid : ocs::tom::get<MES_job_number_list>(mes)) {
            if (ocs::tom::get<ULNG_value>(mes_jid) == jid) {
               print_sched_message(ocs::tom::get<MES_message>(mes));
            }
         }
      }
   }
   DRETURN_VOID;
}
