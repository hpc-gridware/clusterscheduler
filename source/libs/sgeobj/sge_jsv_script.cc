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
 *   Copyright: 2008 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Talking to a job submission verifier script
 *
 * The protocol is line based and synchronous: qmaster or the client sends a
 * command, the script answers, and a script that stays silent past its
 * timeout is killed.
 *
 * @see sge_jsv_script.h
 */

#include <ctime>
#include <cctype>
#include <vector>
#include <utility>
#include <tuple>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"
#include "uti/sge.h"

#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "sgeobj/ocs_BindingObj.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingType.h"
#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/ocs_parse_util.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_jsv_script.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_JSV.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "symbols.h"
#include "msg_common.h"

#include "sched/sgeee.h"

/*
 * defines the timeout how long a client/qmaster would wait maximally for
 * a response from a JSV script after a command string has been send
 */
/// Seconds to wait for a JSV script to answer one command
#define JSV_CMD_TIMEOUT (10) 

/**
 * @def JOB_NAME_DEL
 * @brief Separates the job name from what follows it in a JSV parameter
 *
 * Defined inside `jsv_field2switch_name` rather than at file scope, so it
 * is documented here explicitly - doxygen does not attach a preceding comment
 * to a macro defined in a function body.
 */

/**
 * @brief Handles one command a JSV script sent back
 *
 * @param jsv the JSV instance the command came from
 * @param answer_list receives error messages
 * @param c the command
 * @param s its subcommand, if any
 * @param a the remaining arguments
 * @return true when the command was handled
 */
typedef bool (*jsv_command_f)(ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              const dstring *c, const dstring *s, const dstring *a);

/// One entry of the table mapping a JSV command name onto its handler
typedef struct jsv_command_t_ jsv_command_t;

/// One entry of the table mapping a JSV command name onto its handler
struct jsv_command_t_ {
   const char *command; ///< the command name a JSV script sends
   jsv_command_f func;  ///< the handler for it
};

static bool
jsv_split_commandline(const char *input, dstring *command, dstring *subcommand, dstring *args) {
   DENTER(TOP_LAYER);
   if (input != nullptr) {
      struct saved_vars_s *ctx = nullptr;

      if (const char *token1 = sge_strtok_r(input, " ", &ctx); token1 != nullptr) {
         sge_dstring_append(command, token1);
         if (const char *token2 = sge_strtok_r(nullptr, " ", &ctx); token2 != nullptr) {
            bool first = true;

            sge_dstring_append(subcommand, token2);
            const char *arg = sge_strtok_r(nullptr, " ", &ctx);
            while (arg != nullptr) {
               if (first) {
                  first = false;
               } else {
                  sge_dstring_append(args, " ");
               }
               sge_dstring_append(args, arg);
               arg = sge_strtok_r(nullptr, " ", &ctx);
            }
         }
      }
      sge_free_saved_vars(ctx);
   }
   DRETURN(true);
}

static bool
jsv_split_token(const dstring *input, dstring *token, dstring *args) {
   DENTER(TOP_LAYER);

   if (const char *i = sge_dstring_get_string(input); i != nullptr) {
      struct saved_vars_s *ctx = nullptr;

      if (const char *token1 = sge_strtok_r(i, " ", &ctx); token1 != nullptr) {
         bool first = true;

         sge_dstring_append(token, token1);
         const char *arg = sge_strtok_r(nullptr, " ", &ctx);
         while (arg != nullptr) {
            if (first) {
               first = false;
            } else {
               sge_dstring_append(args, " ");
            }
            sge_dstring_append(args, arg);
            arg = sge_strtok_r(nullptr, " ", &ctx);
         }
      }
      sge_free_saved_vars(ctx);
   }
   DRETURN(true);
}

/**
 * @brief The job a JSV works on, from a reference field of the JSV
 *
 * The JSV holds the jobs as references (JSV_old_job, JSV_new_job), which
 * jsv_do_verify() sets to jobs (JB) only, so the handle is taken over unchecked.
 *
 * @tparam F the reference field, JSV_old_job or JSV_new_job
 * @param jsv the JSV
 * @return the job, or an empty handle if none is set
 */
template <auto F>
static ocs::tom::Element<ocs::tom::JB>
ocs_jsv_get_job(const ocs::tom::ConstElement<ocs::tom::JSV> jsv) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::tom::Element<ocs::tom::JB>::from_typed(static_cast<ocs::tom::ElementData *>(jsv.get<F>())));
}

/**
 * @brief Apply a path list parameter (-o, -i, -e, -S) the JSV script sent back to the new job
 *
 * @tparam F the path list field of the job
 * @param new_job the job to modify
 * @param answer_list receives an error message
 * @param param the parameter name, for the error message
 * @param value the path list, or nullptr to remove it
 * @return true if the path list was stored, false if 'value' could not be parsed; the job is unchanged then
 */
template <auto F>
static bool
ocs_jsv_set_path_list_param(const ocs::tom::Element<ocs::tom::JB> new_job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            const char *param, const char *value) {
   DENTER(TOP_LAYER);

   bool ret = true;
   ocs::tom::ListPtr<ocs::tom::PN> path_list;

   if (value != nullptr) {
      auto [local_ret, parsed] = ocs_parse_path_list(value);
      path_list = std::move(parsed);
      if (local_ret != 0) {
         answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS, param, value);
         ret = false;
      }
   }
   if (ret) {
      new_job.set<F>(std::move(path_list));
   }
   DRETURN(ret);
}

/**
 * @brief Convert a job id list the JSV script sent back (-hold_jid, -hold_jid_ad) into job references
 *
 * @param answer_list receives an error message
 * @param param the parameter name, for the error message
 * @param value the job id list, or nullptr
 * @return whether 'value' could be parsed, and the job references (JRE), nullptr if there are none;
 *         as before, the references of what was parsed are returned also on a parse error
 */
static std::pair<bool, ocs::tom::ListPtr<ocs::tom::JRE>>
ocs_jsv_parse_jid_request_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *param, const char *value) {
   DENTER(TOP_LAYER);

   bool ret = true;
   ocs::tom::ListPtr<ocs::tom::ST> hold_list;
   ocs::tom::ListPtr<ocs::tom::JRE> jref_list;

   if (value != nullptr) {
      auto [local_ret, parsed] = ocs_parse_jid_hold_list(value);
      hold_list = std::move(parsed);
      if (local_ret != 0) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS, param, value);
         ret = false;
      }
   }
   for (const auto jid_str : *hold_list) {
      if (!jref_list) {
         jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("");
      }
      jref_list->emplace_back<JRE_job_name>(jid_str.get<ST_name>());
   }
   DRETURN(std::make_pair(ret, std::move(jref_list)));
}

/**
 * @brief Apply a PARAM line the JSV script sent back to the new job
 *
 * Each parameter maps to a job attribute; the context parameter ac replaces the
 * job's context and drops its context change request (CS-3058).
 *
 * @param jsv the JSV (JSV), holding the new job
 * @param answer_list receives errors
 * @param c the command, PARAM
 * @param s the parameter name
 * @param a the parameter value, or an empty string
 * @return true if the parameter was applied, false on an error
 */
static bool
jsv_handle_param_command(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                         const dstring *c, const dstring *s, const dstring *a) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *param = sge_dstring_get_string(s);
   const char *value = sge_dstring_get_string(a);

   if (param != nullptr) {
      bool skip_check = false;
      ocs::tom::ListPtr<ocs::tom::AN> local_answer_list;
      const auto new_job = ocs_jsv_get_job<JSV_new_job>(jsv);

      /*
       * If we get a "__JSV_TEST_RESULT" then this code is triggered as part of a
       * testsuite test. We store that we are in test mode and we store the
       * expected result which will be tested after we did the parameter
       * modification.
       */
      if (strcmp(param, "__JSV_TEST_RESULT") == 0) {
         jsv.set<JSV_test>(true);
         jsv.set<JSV_test_pos>(0);
         jsv.set<JSV_result>(value);
         skip_check = true;
      }

      /*
       * Reject read-only parameter
       */
      {
         int i = 0;
         const char *read_only_param[] = {
            "CLIENT", "CONTEXT", "GROUP", "JOB_ID", "USER", "VERSION",
            nullptr
         };

         while (read_only_param[i] != nullptr) {
            if (strcmp(param, read_only_param[i]) == 0) {
               answer_list_add_sprintf(&local_answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_READ_S,
                                       param);
               ret = false;
               break;
            }
            i++;
         }
      }

      /*
       * Handle boolean parameter.
       * -b -j -notify -shell -R -r
       */
      if (ret) {
         int i = 0;
         const char *read_only_param[] = {
            "b", "j", "notify", "shell", "R", "r",
            nullptr
         };
         bool is_readonly = false;

         while (read_only_param[i] != nullptr) {
            if (strcmp(param, read_only_param[i]) == 0) {
               is_readonly = true;
               if (value == nullptr) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_BOOL_S,
                                          param);
                  ret = false;
               } else if (strcmp(value, "y") != 0 && strcmp(value, "n") != 0) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS,
                                          param, value);
                  ret = false;
               }
               break;
            }
            i++;
         }
         if (ret && is_readonly) {
            if (strcmp(param, "b") == 0) {
               job_set_binary(new_job, strcmp(value, "y") == 0 ? true : false);
            } else if (strcmp(param, "j") == 0) {
               new_job.set<JB_merge_stderr>(strcmp(value, "y") == 0 ? true : false);
            } else if (strcmp(param, "notify") == 0) {
               new_job.set<JB_notify>(strcmp(value, "y") == 0 ? true : false);
            } else if (strcmp(param, "R") == 0) {
               new_job.set<JB_reserve>(strcmp(value, "y") == 0 ? true : false);
            } else if (strcmp(param, "r") == 0) {
               new_job.set<JB_restart>(strcmp(value, "y") == 0 ? 1 : 0);
            } else if (strcmp(param, "shell") == 0) {
               job_set_no_shell(new_job, strcmp(value, "y") == 0 ? true : false);
            } else {
               answer_list_add_sprintf(&local_answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS,
                                       param);
               ret = false;
            }
         }
      }

      /* 
       * Handle string parameter 
       *    -A -ckpt -cwd -N -pe <pe_name> -P 
       */
      if (ret) {
         if (strcmp(param, "A") == 0) {
            new_job.set<JB_account>(value);
         } else if (strcmp(param, "ckpt") == 0) {
            // resetting ckpt name also resets ckpt attribute
            if (value == nullptr) {
               new_job.set<JB_checkpoint_attr>(NO_CHECKPOINT);
            }
            new_job.set<JB_checkpoint_name>(value);
         } else if (strcmp(param, "cwd") == 0) {
            new_job.set<JB_cwd>(value);
         } else if (strcmp(param, "N") == 0) {
            // jobs without job name are rejected
            if (value == nullptr) {
               answer_list_add_sprintf(&local_answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_NAME_S,
                                       param);
               ret = false;
            } else {
               new_job.set<JB_job_name>(value);
            }
         } else if (strcmp(param, "pe_name") == 0) {
            new_job.set<JB_pe>(value);
         } else if (strcmp(param, "P") == 0) {
            new_job.set<JB_project>(value);
         }
      }

      /*
       * Handle path list parameters
       *    -o -i -e -S
       */
      if (ret) {
         if (strcmp(param, "o") == 0) {
            ret = ocs_jsv_set_path_list_param<JB_stdout_path_list>(new_job, &local_answer_list, param, value);
         } else if (strcmp(param, "i") == 0) {
            ret = ocs_jsv_set_path_list_param<JB_stdin_path_list>(new_job, &local_answer_list, param, value);
         } else if (strcmp(param, "e") == 0) {
            ret = ocs_jsv_set_path_list_param<JB_stderr_path_list>(new_job, &local_answer_list, param, value);
         } else if (strcmp(param, "S") == 0) {
            ret = ocs_jsv_set_path_list_param<JB_shell_list>(new_job, &local_answer_list, param, value);
         }
      }

      /* CMDARG<id> */
      {         
         if (ret && strncmp("CMDARG", param, 6) == 0) {
            auto arg_list = new_job.get<JB_job_args>();
            // A job without arguments has no list. As before, the arguments then go into a list that is
            // not stored in the job, so they are lost (the list is freed now instead of leaking).
            ocs::tom::ListPtr<ocs::tom::ST> detached_arg_list;
            if (!arg_list) {
               detached_arg_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
               arg_list = detached_arg_list.get();
            }
            uint32_t id = 0;
            uint32_t length;
            if (const char *id_string = param + 6; !isdigit(id_string[0])) {
               if (value) {

                  length = static_cast<uint32_t>(arg_list.size());
                  ret &= ulong_parse_from_string(&id, &local_answer_list, value);
                  if (ret) {
                     if (id > length) {
                        uint32_t to_create = id - length;

                        while (to_create > 0) {
                           arg_list.emplace_back<ST_name>("");
                           to_create--;
                        }
                     } else {
                        uint32_t to_remove = length - id;
                        while (to_remove > 0) {
                           arg_list.erase(arg_list.back());
                           to_remove--;
                        }
                     }
                  }
               }
            } else {
               length = static_cast<uint32_t>(arg_list.size());
               ret &= ulong_parse_from_string(&id, &local_answer_list, id_string);

               if (id > length) {
                  uint32_t to_create = 0;
                  to_create = id - length + 1;
                  while (to_create > 0) {
                     arg_list.emplace_back<ST_name>("");
                     to_create--;
                  }
               }

               uint32_t i = 0;
               for (const auto elem : arg_list) {
                  if (i == id) {
                     elem.set<ST_name>((value != nullptr) ? value : "");
                     break;
                  }
                  i++;
               }
            }
         } 
      }

      /* -a */
      {
         if (ret && strcmp("a", param) == 0) {
            uint32_t timeval = 0;

            if (value != nullptr) {
               if (int local_ret = ulong_parse_date_time_from_string(&timeval, &local_answer_list, value); !local_ret) {
                  ret = false;
               }
            }
            if (ret) {
               new_job.set<JB_execution_time>(sge_gmt32_to_gmt64(timeval));
            }
         }
      }


      /* -ac */
      {
         if (ret && strcmp("ac", param) == 0) {
            ocs::tom::ListPtr<ocs::tom::VA> context_list;

            if (value != nullptr) {
               const int local_ret = var_list_parse_from_string(context_list, value, 0);
               if (local_ret != 0) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_JSV_PARSE_VAL_SS, param, value);
                  ret = false;
               }
            }
            if (ret) {
               // the JSV answers with the whole context, which replaces the instructions (CS-3058)
               new_job.set<JB_context>(std::move(context_list));
               new_job.set<JB_context_change>(nullptr);
            }
         }
      }

      /* -ar */
      {
         if (ret && strcmp("ar", param) == 0) {
            uint32_t id = 0;

            if (value != nullptr) {
               const auto [parsed, ar_id_list] = ulong_list_parse_from_string(&local_answer_list, value, ",");
               ret &= parsed;
               if (ret) {
                  if (const auto first = ar_id_list->front(); first) {
                     id = first.get<ULNG_value>();
                  }
               }
            }
            if (ret) {
               new_job.set<JB_ar>(id);
            }
         }
      }

      /* -c <interval> */
      /* -c <occasion> */
      {
         if (ret && strcmp("c_interval", param) == 0) {
            uint32_t timeval = 0;

            if (value != nullptr) {
               if (int local_ret = ulong_parse_date_time_from_string(&timeval, &local_answer_list, value); !local_ret) {
                  ret = false;
               }
            }
            if (ret) {
               new_job.set<JB_checkpoint_interval>(timeval);
            }
         }
         if (ret && strcmp("c_occasion", param) == 0) {
            if (int local_ret = sge_parse_checkpoint_attr(value); local_ret != 0) {
               new_job.set<JB_checkpoint_interval>(local_ret);
            } else {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS,
                                       param, value);
               ret = false;
            }
         }
      }

      /* -display */
      {
         if (ret && strcmp("display", param) == 0) {
            const auto env_list = new_job.get<JB_env_list>();
            auto display = env_list.find_first<VA_variable>("DISPLAY");

            if (value != nullptr) {
               if (!display) {
                  display = new_job.emplace_back_in<JB_env_list, VA_variable>("DISPLAY");
               }
               display.set<VA_value>(value);
            } else {
               if (display) {
                  env_list.erase(display);
               }
            }
         }
      }

      /* -dl */
      {
         if (ret && strcmp("dl", param) == 0) {
            uint32_t timeval = 0;

            if (value != nullptr) {
               if (int local_ret = ulong_parse_date_time_from_string(&timeval, &local_answer_list, value); !local_ret) {
                  ret = false;
               }
            }
            if (ret) {
               new_job.set<JB_deadline>(sge_gmt32_to_gmt64(timeval));
            }
         }
      }

      /* -h */
      {
         if (ret && strcmp("h", param) == 0) {
            int hold_state = MINUS_H_TGT_NONE;
            const ocs::tom::ConstList<ocs::tom::RN> n_hold = new_job.get<JB_ja_u_h_ids>();
            const ocs::tom::ConstList<ocs::tom::RN> u_hold = new_job.get<JB_ja_n_h_ids>();
            const ocs::tom::ConstList<ocs::tom::RN> id_list = n_hold ? n_hold : u_hold;

            if (value != nullptr) {
               hold_state = sge_parse_hold_list(value, QSUB);
               if (hold_state == -1) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_JSV_PARSE_VAL_SS, param, value);
                  ret = false;
               }
            }
            if (hold_state == MINUS_H_TGT_NONE) {
               new_job.set<JB_ja_n_h_ids>(id_list.clone(""));
               new_job.set<JB_ja_u_h_ids>(nullptr);
            } else {
               new_job.set<JB_ja_u_h_ids>(id_list.clone(""));
               new_job.set<JB_ja_n_h_ids>(nullptr);
            }
         }
      }

      /* -hold_jid */
      {
         if (ret && strcmp("hold_jid", param) == 0) {
            auto [parsed, jref_list] = ocs_jsv_parse_jid_request_list(&local_answer_list, param, value);
            ret = parsed;
            new_job.set<JB_jid_request_list>(std::move(jref_list));
         }
      }

      /* -hold_jid_ad */
      {
         if (ret && strcmp("hold_jid_ad", param) == 0) {
            auto [parsed, jref_list] = ocs_jsv_parse_jid_request_list(&local_answer_list, param, value);
            ret = parsed;
            new_job.set<JB_ja_ad_request_list>(std::move(jref_list));
         }
      }

      /* -js */
      {
         if (ret && strcmp("js", param) == 0) {
            uint32_t shares = 0;

            if (value != nullptr) {
               if (!parse_ulong_val(nullptr, &shares, ocs::CEntry::Type::INT, value, nullptr, 0)) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_JSV_PARSE_VAL_SS, param, value);
                  ret = false;
               }
            }
            if (ret) {
               new_job.set<JB_jobshare>(shares);
            }
         }
      }

      /* -l */
      if (ret) {
         bool is_l_request = true;
         bool is_hard = true;
         int scope = JRS_SCOPE_GLOBAL;

         if (strcmp("global_l_hard", param) == 0 || strcmp("l_hard", param) == 0) {
            is_hard = true;
            scope = JRS_SCOPE_GLOBAL;
         } else if (strcmp("global_l_soft", param) == 0 || strcmp("l_soft", param) == 0) {
            is_hard = false;
            scope = JRS_SCOPE_GLOBAL;
         } else if (strcmp("master_l_hard", param) == 0) {
            is_hard = true;
            scope = JRS_SCOPE_MASTER;
         } else if (strcmp("slave_l_hard", param) == 0) {
            is_hard = true;
            scope = JRS_SCOPE_SLAVE;
         } else {
            is_l_request = false;
         }

         if (is_l_request) {
            ocs::tom::ListPtr<ocs::tom::CE> resource_list;

            if (value != nullptr) {
               resource_list = centry_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::CE>(), value, false);
               if (!resource_list) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_JSV_PARSE_VAL_SS, param, value);
                  ret = false;
               }
            }
            job_set_resource_list(new_job, std::move(resource_list), scope, is_hard);
         }
      }

      /* -m */
      if (ret && strcmp("m", param) == 0) {
         int mail_options = NO_MAIL;

         if (value != nullptr) {
            mail_options = sge_parse_mail_options(&local_answer_list, value, QSUB);
            if (!mail_options) {
               ret = false;
            }
         }
         if (ret) {
            if (mail_options & NO_MAIL) {
               new_job.set<JB_mail_options>(0);
            } else {
               new_job.set<JB_mail_options>(mail_options);
            }
         }
      }

      /* -masterq ; -soft -q ; -hard -q*/
      if (ret) {
         bool is_q_request = true;
         bool is_hard = true;
         int scope = JRS_SCOPE_GLOBAL;

         if (strcmp("global_q_hard", param) == 0 || strcmp("q_hard", param) == 0) {
            is_hard = true;
            scope = JRS_SCOPE_GLOBAL;
         } else if (strcmp("global_q_soft", param) == 0 || strcmp("q_soft", param) == 0) {
            is_hard = false;
            scope = JRS_SCOPE_GLOBAL;
         } else if (strcmp("master_q_hard", param) == 0 || strcmp("masterq", param) == 0) {
            is_hard = true;
            scope = JRS_SCOPE_MASTER;
         } else if (strcmp("slave_q_hard", param) == 0) {
            is_hard = true;
            scope = JRS_SCOPE_SLAVE;
         } else {
            is_q_request = false;
         }

         if (is_q_request) {
            ocs::tom::ListPtr<ocs::tom::QR> id_list;

            if (value != nullptr) {
               ocs::tom::ListData *parsed = nullptr; // ocs_parse_destination_identifier_list() has no typed form
               const int local_ret = ocs_parse_destination_identifier_list(&parsed, value);
               id_list = ocs::tom::ListPtr<ocs::tom::QR>::from_typed(parsed);
               if (local_ret != 0) {
                  answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOXOPTION_SS, value, param);
                  ret = false;
               }
            }
            job_set_queue_list(new_job, std::move(id_list), scope, is_hard);
         }
      }

      /* -M */
      if (ret && strcmp("M", param) == 0) {
         ocs::tom::ListPtr<ocs::tom::MR> mail_list;

         if (value != nullptr) {
            auto [local_ret, parsed] = mailrec_parse(value);
            mail_list = std::move(parsed);
            if (local_ret != 0) {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_JSV_PARSE_VAL_SS, param, value);
               ret = false;
            }
         }
         if (ret) {
            new_job.set<JB_mail_list>(std::move(mail_list));
         }
      }

      /* -p */
      if (ret && strcmp("p", param) == 0) {
         int priority = 0;

         if (value != nullptr) {
            ret = ulong_parse_priority(&local_answer_list, &priority, value);
         }
         if (ret) {
            uint32_t pri = BASE_PRIORITY + priority;
            job_normalize_priority(new_job, pri);
         }
      }

      /* -tc */
      if (ret && strcmp("tc", param) == 0) {
         int max_tasks = 0;

         if (value != nullptr) {
            ret = ulong_parse_task_concurrency(&local_answer_list, &max_tasks, value);
         }
         if (ret) {
            new_job.set<JB_ja_task_concurrency>(max_tasks);
         }
      }

      /*
       * -bamount
       * and all other -b...-switches that are also related to -binding
       */
      {
         const auto binding_elem = ocs::Job::binding_get_or_create_elem(new_job, answer_list);

         if (binding_elem) {
            if (ret && strcmp("bamount", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_amount>(std::atoi(value));
               } else {
                  binding_elem.set<BN_amount>(0);
               }
            }

            if (ret && strcmp("bstrategy", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_strategy>(ocs::BindingStrategy::from_string(value));
               } else {
                  binding_elem.set<BN_strategy>(ocs::BindingStrategy::Strategy::NONE);
               }
            }

            if (ret && strcmp("btype", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_new_type>(ocs::BindingType::from_string(value));
               } else {
                  binding_elem.set<BN_new_type>(ocs::BindingType::Type::NONE);
               }
            }

            if (ret && strcmp("bunit", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_unit>(ocs::BindingUnit::from_string(value));
               } else {
                  binding_elem.set<BN_unit>(ocs::BindingUnit::Unit::NONE);
               }
            }

            if (ret && strcmp("bstart", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_start>(ocs::BindingStart::from_string(value));
               } else {
                  binding_elem.set<BN_start>(ocs::BindingStart::Start::NONE);
               }
            }

            if (ret && strcmp("bstop", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_stop>(ocs::BindingStop::from_string(value));
               } else {
                  binding_elem.set<BN_stop>(ocs::BindingStop::Stop::NONE);
               }
            }

            if (ret && strcmp("bsort", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_sort>(value);
               } else {
                  binding_elem.set<BN_sort>(nullptr);
               }
            }

            if (ret && strcmp("bfilter", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_filter>(value);
               } else {
                  binding_elem.set<BN_filter>(nullptr);
               }
            }

            if (ret && strcmp("binstance", param) == 0) {
               if (value != nullptr) {
                  binding_elem.set<BN_instance>(ocs::BindingInstance::from_string(value));
               } else {
                  binding_elem.set<BN_instance>(ocs::BindingInstance::Instance::NONE);
               }
            }
         }

         // If no binding parameters are set, remove the binding element again
         if (binding_elem.get<BN_amount>() == 0) {
            ocs::Binding::clear<JB_binding>(new_job);
         } else {
            ocs::Job::binding_set_missing_defaults(new_job, answer_list);
         }
      }

      /*
       * -pe name n-m
       */
      if (ret && strcmp("pe_name", param) == 0) {
         if (value) {
            new_job.set<JB_pe>(value);
         } else {
            new_job.set<JB_pe>(nullptr);
         }
      }
      if (ret && strcmp("pe_min", param) == 0) {
         uint32_t min = 0;

         if (value != nullptr) {
            if (!parse_ulong_val(nullptr, &min, ocs::CEntry::Type::INT, value, nullptr, 0)) {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS,
                                       param, value);
               ret = false;
            }
         }
         if (ret) {
            auto range = new_job.get<JB_pe_range>().front();

            if (!range) {
               range = new_job.emplace_back_in<JB_pe_range, RN_min>(min);
            }
            if (range) {
               range.set<RN_min>(min);
            }
         }
      }
      if (ret && strcmp("pe_max", param) == 0) {
         uint32_t max = 0;

         if (value != nullptr) {
            if (!parse_ulong_val(nullptr, &max, ocs::CEntry::Type::INT, value, nullptr, 0)) {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS,
                                       param, value);
               ret = false;
            }
         }
         if (ret) {
            auto range = new_job.get<JB_pe_range>().front();

            if (!range) {
               range = new_job.emplace_back_in<JB_pe_range, RN_max>(max);
            }
            if (range) {
               range.set<RN_max>(max);
            }
         }
      }

      /*
       *-t n-m:s
       */
      if (ret && strcmp("t_min", param) == 0) {
         if (value != nullptr) {
            uint32_t min;
            if (!parse_ulong_val(nullptr, &min, ocs::CEntry::Type::INT, value, nullptr, 0)) {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       "invalid t_min " SFQ " was passed by JSV", value);
               ret = false;
            } else {
               auto range = new_job.get<JB_ja_structure>().front();

               if (!range) {
                  range = new_job.emplace_back_in<JB_ja_structure, RN_min>(min);
               }
               if (range) {
                  range.set<RN_min>(min);
               }
            }
         }
      }
      if (ret && strcmp("t_max", param) == 0) {
         if (value != nullptr) {
            uint32_t max;
            if (!parse_ulong_val(nullptr, &max, ocs::CEntry::Type::INT, value, nullptr, 0)) {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_VAL_SS,
                                       param, value);
               ret = false;
            } else {
               auto range = new_job.get<JB_ja_structure>().front();

               if (!range) {
                  range = new_job.emplace_back_in<JB_ja_structure, RN_max>(max);
               }
               if (range) {
                  range.set<RN_max>(max);
               }
            }
         }
      }
      if (ret && strcmp("t_step", param) == 0) {
         if (value != nullptr) {
            uint32_t step;
            if (!parse_ulong_val(nullptr, &step, ocs::CEntry::Type::INT, value, nullptr, 0)) {
               answer_list_add_sprintf(&local_answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_JSV_PARSE_VAL_SS, param, value);
               ret = false;
            } else {
               auto range = new_job.get<JB_ja_structure>().front();

               if (!range) {
                  range = new_job.emplace_back_in<JB_ja_structure, RN_step>(step);
               }
               if (range) {
                  range.set<RN_step>(step);
               }
            }
         }
      }

      /* -w */
      if (ret && strcmp("w", param) == 0) {
         int level = 0;

         if (value != nullptr) {
            ret = job_parse_validation_level(&level, value, QSUB, &local_answer_list);

            DPRINTF("result of parsing is %d\n", ret);

         }
         if (ret) {
            new_job.set<JB_verify_suitable_queues>(level);
         }
      }

      /*
       * if we are in test mode the we have to check the expected result.
       * in test mode we will reject jobs if we did not get the expected
       * result otherwise
       * we will accept the job with the ret value set above including
       * the created error messages.
       */
      if (jsv.get<JSV_test>() && !skip_check) {
         const char *result_string = jsv.get<JSV_result>();

         if (uint32_t result_pos = jsv.get<JSV_test_pos>(); strlen(result_string) > result_pos) {
            char result_char = result_string[result_pos];

            if (bool result = (result_char == '1') ? true : false; result != ret) {
               answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR,
                                       "\"PARAM %s %s\" was unexcpectedly %s\n",
                                       param, value != nullptr ? value : " ", ret ? "accepted" : "rejected");
               ret = false;
            } else {
               ret = true;
            }
         }
         jsv.set<JSV_test_pos>(jsv.get<JSV_test_pos>() + 1);
      }
      answer_list_append_list(answer_list, &local_answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Handle a SEND line of a JSV script, which asks for more information about the job
 *
 * Only SEND ENV is known: the environment of the job is sent with the next STARTED answer.
 *
 * @param jsv the JSV
 * @param answer_list receives an error message
 * @param c the command, SEND
 * @param s the subcommand, ENV
 * @param a not used
 * @return true if the subcommand is known, false otherwise
 */
static bool
jsv_handle_send_command(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const dstring *c, const dstring *s, const dstring *a) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (const char *subcommand = sge_dstring_get_string(s); strcmp(subcommand, "ENV") == 0) {
      jsv.set<JSV_send_env>(true);
   } else {
      /*
       * Invalid subcommand. JSV seems to wait for information which
       * is not available in this version. Job will be rejected.
       */
      answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_PARSE_OBJECT_S,
                              sge_dstring_get_string(s));

      jsv.set<JSV_send_env>(false);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Handle a RESULT line of a JSV script, which accepts, corrects or rejects the job
 *
 * @param jsv the JSV; JSV_accept and JSV_done are set
 * @param answer_list receives the rejection message or an error message
 * @param c the command, RESULT
 * @param s the subcommand, STATE
 * @param a the state (ACCEPT, CORRECT, REJECT, REJECT_WAIT), optionally followed by a message
 * @return true if the line could be understood, false otherwise
 */
static bool
jsv_handle_result_command(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          const dstring *c, const dstring *s, const dstring *a) {
   DENTER(TOP_LAYER);
   bool ret = true;

   dstring m = DSTRING_INIT;
   dstring st = DSTRING_INIT; 
   jsv_split_token(a, &st, &m);

   const char *sub_command = sge_dstring_get_string(s);
   const char *state = sge_dstring_get_string(&st);
   const char *message = sge_dstring_get_string(&m);
   if (sub_command != nullptr && strcmp(sub_command, "STATE") == 0 && state != nullptr) {
      if (strcmp(state, "ACCEPT") == 0) {
         DPRINTF("Job is accepted\n");
         jsv.set<JSV_accept>(true);
         jsv.set<JSV_done>(true);
      } else if (strcmp(state, "CORRECT") == 0) {
         DPRINTF("Job is corrected\n");
         jsv.set<JSV_accept>(true);
         jsv.set<JSV_done>(true);
      } else if (strcmp(state, "REJECT") == 0) {
         DPRINTF("Job is rejected\n");
         if (message != nullptr) {
            // the message is text from the JSV script, never a format
            answer_list_add(answer_list, message, STATUS_DENIED, ANSWER_QUALITY_ERROR);
         } else {
            answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_REJECTED);
         }
         jsv.set<JSV_accept>(false);
         jsv.set<JSV_done>(true);
      } else if (strcmp(state, "REJECT_WAIT") == 0) {
         DPRINTF("Job is rejected temporarily\n");
         if (message != nullptr) {
            // the message is text from the JSV script, never a format
            answer_list_add(answer_list, message, STATUS_DENIED, ANSWER_QUALITY_ERROR);
         } else {
            answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_TMPREJECT);
         }
         jsv.set<JSV_accept>(false);
         jsv.set<JSV_done>(true);
      } else {
         answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_STATE_S, a);
         ret = false;
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_COMMAND_S, sub_command);
      ret = false;
   }
   
   /* disable sending of env variables for the next job verification */
   jsv.set<JSV_send_env>(false);

   sge_dstring_free(&m);
   sge_dstring_free(&st);
   DRETURN(ret);
}

/**
 * @brief Send a queue request list to a JSV script as one PARAM line
 *
 * @param jsv the JSV
 * @param answer_list receives an error message
 * @param prefix the command, PARAM
 * @param name the parameter name
 * @param queue_list the queue list (QR); nothing is sent for nullptr
 * @return true
 */
static bool
jsv_send_queue_list(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                    const char *prefix, const char *name, const ocs::tom::ConstList<ocs::tom::QR> queue_list) {
   DENTER(TOP_LAYER);

   if (queue_list) {
      dstring buffer = DSTRING_INIT;
      sge_dstring_sprintf(&buffer, "%s %s", prefix, name);

      bool first = true;
      for (const auto queue : queue_list) {
         const char *queue_pattern = queue.get<QR_name>();

         sge_dstring_append_char(&buffer, first ? ' ' : ',');
         sge_dstring_sprintf_append(&buffer, "%s", queue_pattern);
         first = false;
      }
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      sge_dstring_free(&buffer);
   }
   DRETURN(true);
}

/**
 * @brief Send a resource request list to a JSV script as one PARAM line
 *
 * @param jsv the JSV
 * @param answer_list receives an error message
 * @param prefix the command, PARAM
 * @param name the parameter name
 * @param resource_list the resource list (CE); nothing is sent for nullptr
 * @return true
 */
static bool
jsv_send_resource_list(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       const char *prefix, const char *name, const ocs::tom::ConstList<ocs::tom::CE> resource_list) {
   DENTER(TOP_LAYER);
   if (resource_list) {
      dstring buffer = DSTRING_INIT;
      sge_dstring_sprintf(&buffer, "%s %s ", prefix, name);
      centry_list_append_to_dstring(resource_list, &buffer);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      sge_dstring_free(&buffer);
   }
   DRETURN(true);
}

/**
 * @brief Send a path list to a JSV script as one PARAM line
 *
 * @param jsv the JSV
 * @param answer_list receives an error message
 * @param prefix the command, PARAM
 * @param name the parameter name
 * @param path_list the path list (PN); nothing is sent for nullptr
 * @return true
 */
static bool
jsv_send_path_list(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                   const char *prefix, const char *name, const ocs::tom::ConstList<ocs::tom::PN> path_list) {
   DENTER(TOP_LAYER);
   if (!path_list) {
      DRETURN(true);
   }
   dstring buffer = DSTRING_INIT;
   sge_dstring_clear(&buffer);
   sge_dstring_sprintf(&buffer, "%s %s", prefix, name);

   bool first = true;
   for (const auto shell : path_list) {
      sge_dstring_append_char(&buffer, first ? ' ' : ',');
      if (const char *hostname = shell.get<PN_host>();
          hostname != nullptr) {
         sge_dstring_append(&buffer, hostname);
         sge_dstring_append_char(&buffer, ':');
      }
      sge_dstring_append(&buffer, shell.get<PN_path>());
      first = false;
   }
   jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   sge_dstring_free(&buffer);
   DRETURN(true);
}

/**
 * @brief Answer the STARTED line of a JSV script with the parameters of the job
 *
 * Sends one PARAM line per attribute of the old job. The context is sent as the
 * job will have it, with its context change request applied (CS-3058).
 *
 * @param jsv the JSV (JSV), holding the old job
 * @param answer_list receives errors
 * @param c the command, STARTED
 * @param s not used
 * @param a not used
 * @return true if all parameters were sent, false on an error
 */
static bool
jsv_handle_started_command(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const dstring *c, const dstring *s, const dstring *a) {
   DENTER(TOP_LAYER);
   const auto  prefix = "PARAM";
   dstring buffer = DSTRING_INIT;
   bool ret = true;
   const auto old_job = ocs_jsv_get_job<JSV_old_job>(jsv);

   // reset variables which are only used in test cases
   jsv.set<JSV_test>(false);
   jsv.set<JSV_result>("");

   // JSV VERSION <major>.<minor> (read-only)
   sge_dstring_clear(&buffer);
   sge_dstring_sprintf(&buffer, "%s VERSION 1.0", prefix);
   jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

   // JSV CONTEXT "client"|"server" (read-only)
   sge_dstring_clear(&buffer);
   sge_dstring_sprintf(&buffer, "%s CONTEXT %s", prefix,
                       (strcmp(jsv.get<JSV_context>(), JSV_CONTEXT_CLIENT) == 0 ? "client" : "server"));
   jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   
   // JSV CLIENT <program_name> (read-only)
   sge_dstring_clear(&buffer);
   sge_dstring_sprintf(&buffer, "%s CLIENT %s", prefix, to_cstr(component_get_component_id()));
   jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

   // JSV USER <user_name> (read-only)
   sge_dstring_clear(&buffer);
   sge_dstring_sprintf(&buffer, "%s USER %s", prefix, old_job.get<JB_owner>());
   jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

   // JSV GROUP <group_name> (read-only)
   sge_dstring_clear(&buffer);
   sge_dstring_sprintf(&buffer, "%s GROUP %s", prefix, old_job.get<JB_group>());
   jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

   // JSV JOB_ID <jid> (optional; read-only)
   if (uint32_t jid = old_job.get<JB_job_number>(); jid > 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s JOB_ID " sge_u32, prefix, jid);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // JSV COMMAND
   //    -b y ... <command>      => format := <command>
   //    -b n ... <job_script>   => format := <file>
   //    -b n                    => format := "STDIN"
   {
      const char *script_name = old_job.get<JB_script_file>();

      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s CMDNAME %s", prefix, (script_name != nullptr) ? script_name : "NONE");
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // JSV SCRIPT_ARGS
   {
      const ocs::tom::ConstList<ocs::tom::ST> list = old_job.get<JB_job_args>();
      int i = 0;

      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s CMDARGS " sge_u32, prefix, static_cast<uint32_t>(list.size()));
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

      for (const auto elem : list) {
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s CMDARG%d %s", prefix, i, elem.get<ST_name>());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
         i++;
      }
   }
   
   // -a
   // PARAM a <date_time> (optional; <date_time> := CCYYMMDDhhmm.SS)
   if (time_t clocks = static_cast<time_t>(old_job.get<JB_execution_time>()) / 1000000; clocks > 0) {
      struct tm time_struct = {0};

      localtime_r(&clocks, &time_struct);
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s a %04d%02d%02d%02d%02d.%02d", prefix, time_struct.tm_year + 1900,
                          time_struct.tm_mon, time_struct.tm_mday, time_struct.tm_hour, time_struct.tm_min,
                          time_struct.tm_sec);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -ac variable[=value],... (optional; the context with the instructions of -ac, -dc and -sc applied)
   if (const auto context_list = ocs_context_get_effective(old_job); context_list) {
      bool first = true;

      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "PARAM ac");
      for (const auto context : *context_list) {
         const char *name = context.get<VA_variable>();
         const char *value = context.get<VA_value>();

         sge_dstring_sprintf_append(&buffer, (first) ? " " : ",");
         first = false;
         if (value != nullptr) {
            sge_dstring_sprintf_append(&buffer, "%s=%s", name, value);
         } else {
            sge_dstring_sprintf_append(&buffer, "%s", name);
         }
      }
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -ar (optional)
   if (uint32_t ar_id = old_job.get<JB_ar>(); ar_id > 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s ar " sge_u32, prefix, ar_id);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -A <account_string> (optional)
   if (const char *account_string = old_job.get<JB_account>(); account_string != nullptr) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s A %s", prefix, account_string);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -b y|n
   // PARAM b y|n (optional; only available if -b y was specified)
   if (job_is_binary(old_job)) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s b %c", prefix, job_is_binary(old_job) ? 'y' : 'n');
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -c n|s|m|x   or
   // -c <interval>
   // PARAM c_occasion <occasion_string> (optional; <occasion_string> := ['n']['s']['m']['x']
   // PARAM c_interval <interval> (optional; <interval> := <2_digits>:<2_digits>:<2_digits>)
   if (uint32_t interval = old_job.get<JB_checkpoint_interval>(); interval > 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s c_interval ", prefix);
      double_print_time_to_dstring((double)interval, &buffer);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }
   if (uint32_t attr = old_job.get<JB_checkpoint_attr>(); attr > 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s c_occasion ", prefix);
      job_get_ckpt_attr(attr, &buffer);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -ckpt name (optional)
   if (const char *ckpt = old_job.get<JB_checkpoint_name>(); ckpt != nullptr) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s ckpt %s", prefix, ckpt);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -cwd
   // Different to commandline. If -cwd was specified it will be exported to the JSV by passing
   // the complete path. To remove the path the JSV has to pass an empty path. (optional)
   if (const char *cwd = old_job.get<JB_cwd>(); cwd != nullptr) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s cwd %s", prefix, cwd);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   /* -C (not handled in JSV) */
   /* -dc (handled as part of -ac parameter) */
   /* -display <display_name> (handled below where -v/-V is handled) */

   // -dl <date_time> (optional)
   if (time_t clocks = static_cast<time_t>(old_job.get<JB_deadline>()) / 1000000; clocks > 0) {
      struct tm time_struct = {0};

      localtime_r(&clocks, &time_struct);
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s dl %04d%02d%02d%02d%02d.%02d", prefix, time_struct.tm_year + 1900,
                          time_struct.tm_mon, time_struct.tm_mday, time_struct.tm_hour, time_struct.tm_min,
                          time_struct.tm_sec);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   /* -hard (handled as [global|master|server]_[l|q]_hard */

   // -hold_jid wc_job_list (optional)
   if (const ocs::tom::ConstList<ocs::tom::JRE> hold_jid_list = old_job.get<JB_jid_request_list>();
       hold_jid_list) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s hold_jid", prefix);
      bool first = true;
      for (const auto hold_jid : hold_jid_list) {
         sge_dstring_append_char(&buffer, first ? ' ' : ',');
         if (const char *name = hold_jid.get<JRE_job_name>(); name != nullptr) {
            sge_dstring_sprintf_append(&buffer, "%s", name);
         } else {
            sge_dstring_sprintf_append(&buffer, sge_u32, hold_jid.get<JRE_job_number>());
         }
         first = false;
      }
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -hold_jid_ad wc_job_list (optional)
   if (const ocs::tom::ConstList<ocs::tom::JRE> hold_jid_list = old_job.get<JB_ja_ad_request_list>();
       hold_jid_list) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s hold_jid_ad", prefix);
      bool first = true;
      for (const auto hold_jid : hold_jid_list) {
         sge_dstring_append_char(&buffer, first ? ' ' : ',');
         if (const char *name = hold_jid.get<JRE_job_name>(); name != nullptr) {
            sge_dstring_sprintf_append(&buffer, "%s", name);
         } else {
            uint32_t jid = hold_jid.get<JRE_job_number>();
            sge_dstring_sprintf_append(&buffer, sge_u32, jid);
         }
         first = false;
      }
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -h (optional; only available if job is in user hold)
   //
   // in difference with the qsub -h switch the setting is provided as (PARAM h u) where 'u' means "user hold"
   // 'n' would mean "no hold" but in this case the parameter is not provided to the JSV script
   if (const ocs::tom::ConstList<ocs::tom::RN> hold_list = old_job.get<JB_ja_u_h_ids>(); hold_list) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s h u", prefix);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -inherit (not handled in JSV)

   // -j y | n (optional; only available when -j y was specified)
   if (const bool merge = old_job.get<JB_merge_stderr>()) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s j %c", prefix, merge ? 'y' : 'n');
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -js job_share (optional)
   if (uint32_t job_share = old_job.get<JB_jobshare>(); job_share > 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s js " sge_u32, prefix, job_share);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -l (optional)
   //
   // -scope GLOBAL -hard -l =>
   // PARAM global_l_hard <centry_list>
   // PARAM l_hard <centry_list> (to be compatible with old JSV scripts)
   //
   // -scope GLOBAL -soft -l =>
   // PARAM global_l_soft <centry_list>
   // PARAM l_soft <centry_list> (to be compatible with old JSV scripts)
   //
   // -scope MASTER -hard -l =>
   // PARAM master_l_hard <centry_list>
   //
   // -scope SLAVE -hard -l =>
   // PARAM slave_l_hard <centry_list>
   {
      const std::vector<std::tuple<const int, const bool, std::vector<std::string>>> resource_table = {
         std::make_tuple(JRS_SCOPE_GLOBAL, true, std::vector<std::string>{"global_l_hard", "l_hard"}),
         std::make_tuple(JRS_SCOPE_GLOBAL, false, std::vector<std::string>{"global_l_soft", "l_soft"}),
         std::make_tuple(JRS_SCOPE_MASTER, true, std::vector<std::string>{"master_l_hard"}),
         std::make_tuple(JRS_SCOPE_SLAVE, true, std::vector<std::string>{"slave_l_hard"}),
      };
      for (const auto&[scope, hard, attr_names] : resource_table) {
         const auto resource_list = job_get_resource_list(old_job, scope, hard);
         for (const auto &attr_name : attr_names) {
            jsv_send_resource_list(jsv, answer_list, prefix, attr_name.c_str(), resource_list);
         }
      }
   }

   // -m [b][e][a][s] or n (optional; only provided to JSV script if != 'n'
   if (uint32_t mail_options = old_job.get<JB_mail_options>(); mail_options > 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s m ", prefix);
      sge_mailopt_to_dstring(mail_options, &buffer);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -M <mail_addr>, ... (optional)
   if (const ocs::tom::ConstList<ocs::tom::MR> mail_list = old_job.get<JB_mail_list>(); mail_list) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s M", prefix);
      bool first = true;
      for (const auto mail : mail_list) {
         const char *user = mail.get<MR_user>();
         sge_dstring_append_char(&buffer, first ? ' ' : ',');
         sge_dstring_append(&buffer, user);
         if (const char *host = mail.get<MR_host>(); host != nullptr) {
            sge_dstring_sprintf_append(&buffer, "@%s", host);
         }
         first = false;
      }
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -notify y|n (optional; only available when -notify y was specified)
   if (const bool notify = old_job.get<JB_notify>(); notify) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s notify %c", prefix, notify ? 'y' : 'n');
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -now y|n (not available in JSV)

   // -N <job_name>
   // optional; only available if specified during job submission
   if (const char *name = old_job.get<JB_job_name>(); name != nullptr) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s N %s", prefix, name);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -noshell y | n (not handled in JSV)
   // -nostdin (not handled in JSV)

   // -o <output_path_list> (optional)
   // -e <output_path_list> (optional)
   // -i <input_path_list> (optional)
   // -S <shell_path_list> (optional)
   // path_list := [hostname':']path,...
   jsv_send_path_list(jsv, answer_list, prefix, "o", old_job.get<JB_stdout_path_list>());
   jsv_send_path_list(jsv, answer_list, prefix, "e", old_job.get<JB_stderr_path_list>());
   jsv_send_path_list(jsv, answer_list, prefix, "i", old_job.get<JB_stdin_path_list>());
   jsv_send_path_list(jsv, answer_list, prefix, "S", old_job.get<JB_shell_list>());

   // -P project_name (optional; only available if specified during submission)
   if (const char *project = old_job.get<JB_project>(); project != nullptr) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s P %s", prefix, project);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -p priority (optional; only provided if specified during submission and != 0)
   if (int priority = static_cast<int>(old_job.get<JB_priority>()) - 1024; priority != 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s p %d", prefix, priority);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   // -tc task concurrency (optional; only provided if specified during submission
   if (int max_tasks = static_cast<int>(old_job.get<JB_ja_task_concurrency>()); max_tasks != 0) {
      sge_dstring_clear(&buffer);
      sge_dstring_sprintf(&buffer, "%s tc %d", prefix, max_tasks);
      jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
   }

   /*
    * -bamount <number>
    * and all other binding information specified with -b...-switches at the command line
    */
   {
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(old_job, answer_list);

      if (binding_elem) {
         // -bamount
         uint32_t amount = binding_elem.get<BN_amount>();
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bamount " sge_u32, prefix, amount);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -btype
         ocs::BindingType::Type type = ocs::Job::binding_get_type(old_job);
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s btype %s", prefix, ocs::BindingType::to_string(type).c_str());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -bunit
         ocs::BindingUnit::Unit unit = ocs::Job::binding_get_unit(old_job);
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bunit %s", prefix, ocs::BindingUnit::to_string(unit).c_str());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -bstrategy
         ocs::BindingStrategy::Strategy strategy = ocs::Job::binding_get_strategy(old_job);
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bstrategy %s", prefix, ocs::BindingStrategy::to_string(strategy).c_str());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -bstart
         ocs::BindingStart::Start start = ocs::Job::binding_get_start(old_job);
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bstart %s", prefix, ocs::BindingStart::to_string(start).c_str());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -bstop
         ocs::BindingStop::Stop stop = ocs::Job::binding_get_stop(old_job);
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bstop %s", prefix, ocs::BindingStop::to_string(stop).c_str());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -binstance
         ocs::BindingInstance::Instance instance = ocs::Job::binding_get_instance(old_job);
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s binstance %s", prefix, ocs::BindingInstance::to_string(instance).c_str());
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -bfilter
         const char *filter = binding_elem.get<BN_filter>();
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bfilter %s", prefix, filter != nullptr ? filter : NONE_STR);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));

         // -bsort
         const char *sort = binding_elem.get<BN_sort>();
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s bsort %s", prefix, sort != nullptr ? sort : NONE_STR);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
   }

   /* 
    * -pe name n[-[m]] (optional)
    *
    * PARAM pe_name <pe_name>
    * PARAM pe_min <min_number>
    * PARAM pe_max <max_number>
    *
    *                min_number  max_number
    *    -pe pe 4    4           4
    *    -pe pe 4-8  4           8
    *    -pe pe 4-   4           9999999
    *    -pe pe -8   1           8
    */
   {
      const char *pe_name = old_job.get<JB_pe>();
      const auto range = old_job.get<JB_pe_range>().front();

      if (pe_name != nullptr) {
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s pe_name %s", prefix, pe_name);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
      if (range) {
         uint32_t min = range.get<RN_min>();
         uint32_t max = range.get<RN_max>();

         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s pe_min " sge_u32, prefix, min);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s pe_max " sge_u32, prefix, max);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
   }

   /* -pty y|n (not available in JSV) */

   // -q wc_queue_list (optional; see man page sge_types(1) for wc_queue_list specification)
   //
   // -scope GLOBAL -hard -q =>
   // PARAM global_q_hard <wc_queue_list>
   // PARAM q_hard <wc_queue_list> (for compatibility)
   //
   // -scope GLOBAL -soft -q =>
   // PARAM global_q_soft <wc_queue_list>
   // PARAM q_soft <wc_queue_list> (for compatibility)
   //
   // -scope MASTER (-hard) -q =>
   // PARAM master_q_hard <wc_queue_list>
   // PARAM masterq <wc_queue_list> (for compatibility)
   //
   // -scope SLAVE (-hard) -q =>
   // PARAM slave_q_hard <wc_queue_list>
   {
      const auto hard_queue_list = job_get_queue_list(old_job, JRS_SCOPE_GLOBAL, true);
      for (std::vector<std::string> global_hard_attr_names = {"global_q_hard", "q_hard"};
           const auto &attr_name : global_hard_attr_names) {
         jsv_send_queue_list(jsv, answer_list, prefix, attr_name.c_str(), hard_queue_list);
      }

      const auto soft_queue_list = job_get_queue_list(old_job, JRS_SCOPE_GLOBAL, false);
      for (std::vector<std::string> global_soft_attr_names = {"global_q_soft", "q_soft"};
           const auto &attr_name : global_soft_attr_names) {
         jsv_send_queue_list(jsv, answer_list, prefix, attr_name.c_str(), soft_queue_list);
      }

      const auto master_hard_queue_list = job_get_queue_list(old_job, JRS_SCOPE_MASTER, true);
      for (std::vector<std::string> master_hard_attr_names = {"master_q_hard", "masterq"};
           const auto &attr_name : master_hard_attr_names) {
         jsv_send_queue_list(jsv, answer_list, prefix, attr_name.c_str(), master_hard_queue_list);
      }

      const auto slave_hard_queue_list = job_get_queue_list(old_job, JRS_SCOPE_SLAVE, true);
      jsv_send_queue_list(jsv, answer_list, prefix, "slave_q_hard", slave_hard_queue_list);
   }

   /* 
    * -R y|n 
    * optional; only available if specified during submission and value is y
    */
   {
      if (bool reserve = old_job.get<JB_reserve>() ? true : false; reserve) {
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s R %c", prefix, reserve ? 'y' : 'n');
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
   }

   /* 
    * -r y|n 
    * optional; only available if specified during submission and value is y 
    */
   {
      if (uint32_t restart = old_job.get<JB_restart>(); restart == 1) {
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s r %c", prefix, (restart == 1) ? 'y' : 'n');
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
   }

   /* -sc (handled as part of -ac) */

   /* 
    * -shell y|n 
    * optional; only available if -shell n was specified    
    */
   {
      if (bool no_shell = job_is_no_shell(old_job); no_shell) {
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s shell %c", prefix, !no_shell ? 'y' : 'n');
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
   }

   /* -soft (handled as l_soft and q_soft) */

   /* -sync y|n (not available in JSV) */

   /*
    * -t min[-max[:step]] (optional; only available if specified during submission 
    * and if values differ from "1-1:1") 
    * PARAM t_min <number>
    * PARAM t_max <number>
    * PARAM t_step <number>
    */
   {
      if (const auto ja_structure = old_job.get<JB_ja_structure>().front(); ja_structure) {
         uint32_t min, max, step;
         range_get_all_ids(ja_structure, &min, &max, &step);

         /*
          * if -t is not specified then all values will be 1 therefore we have to 
          * provide the values to JSV only if one value differes from 1
          */
         if (max != 1 || min != 1 || step != 1) {
            sge_dstring_clear(&buffer);
            sge_dstring_sprintf(&buffer, "%s t_min " sge_u32, prefix, min);
            jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
            sge_dstring_clear(&buffer);
            sge_dstring_sprintf(&buffer, "%s t_max " sge_u32, prefix, max);
            jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
            sge_dstring_clear(&buffer);
            sge_dstring_sprintf(&buffer, "%s t_step " sge_u32, prefix, step);
            jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
         }
      }
   }

   /* -terse (ignored in JSV. it is just to late to change this) */
   /* -u username,... (not handled in JSV because only available for qalter) */
   /* -v variable[=value],... (handles also -V; done below after all params are handled */
   /* -verbose (not available in JSV) */
   /* -V (handled as part of -v) */

   /* 
    * -w e|w|n|v|p 
    * optional; only sent to JSV if != 'n') 
    */
   {
      if (uint32_t verify = old_job.get<JB_verify_suitable_queues>(); verify > 0) {
         sge_dstring_clear(&buffer);
         sge_dstring_sprintf(&buffer, "%s w ", prefix);
         job_get_verify_attr(verify, &buffer);
         jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
      }
   }

   /* command (handled as PARAM SCRIPT NAME above) */
   /* command_args (handeled as PARAM SCRIPT above) */
   /* xterm_args (handeled as PARAM SCRIPT above) */

   /* 
    * handle -v -V and -display here 
    * TODO: EB: JSV: PARSING
    */  
   {
      ocs::tom::ListPtr<ocs::tom::VA> env_list;

      /* make a copy of the environment */
      var_list_copy_env_vars_and_value(env_list, old_job.get<JB_env_list>());

      /* remove certain variables which don't come from the user environment */
      var_list_remove_prefix_vars(env_list, VAR_PREFIX);
      var_list_remove_prefix_vars(env_list, VAR_PREFIX_NR);

      /* 
       * if there is a DISPLAY variable and if the client is qsh/qrsh
       * then we will send the DISPLAY value as if it originally came 
       * from -display switch.
       */
      if (const auto display = env_list->find_first<VA_variable>("DISPLAY"); display) {
         const char *value = display.get<VA_value>();

         if (ProgName prog_id = component_get_component_id();
             value != nullptr && (strcmp(to_cstr(prog_id), "qsh") == 0 || strcmp(to_cstr(prog_id), "qrsh") == 0)) {
            sge_dstring_clear(&buffer);
            sge_dstring_sprintf(&buffer, "PARAM display %s", value);
            jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
         } 
      }

      /* send the variables to the JSV but only if it was requested */
      if (jsv.get<JSV_send_env>()) {
         for (const auto env : *env_list) {
            const char *value = env.get<VA_value>();
            const char *name = env.get<VA_variable>();
            size_t length, i;
      
            sge_dstring_clear(&buffer);
            sge_dstring_sprintf(&buffer, "ENV ADD %s ", name);
            length = (value != nullptr) ? strlen(value) : 0;
            for (i = 0; i < length; i++) {
               char in[] = {
                  '\\', '\n', '\r', '\t', '\a', '\b', '\v', '\0'
               };
               const char *out[] = {
                  "\\", "\\n", "\\r", "\\t", "\\a", "\\b", "\\v", ""
               };
               int j = 0;
               bool already_handled = false;
   
               while (in[j] != '\0') {
                  if (in[j] == value[i]) {
                     sge_dstring_append(&buffer, out[j]);
                     already_handled = true;
                  }
                  j++;
               }
               if (!already_handled) {
                  sge_dstring_append_char(&buffer, value[i]); 
               }
            }
            jsv_send_command(jsv, answer_list, sge_dstring_get_string(&buffer));
         }
      }
   }

   /* script got all parameters. now verification can start */
   if (ret) {
      ret &= jsv_send_command(jsv, answer_list, "BEGIN");
   }

   /* cleanup */
   sge_dstring_free(&buffer);

   DRETURN(ret);
}

/**
 * @brief Handle a LOG line of a JSV script
 *
 * A server JSV writes the message to the qmaster messages file, a client JSV prints it.
 *
 * @param jsv the JSV
 * @param answer_list not used
 * @param c the command, LOG
 * @param s the level, INFO, WARNING or ERROR
 * @param a the message
 * @return true
 */
static bool
jsv_handle_log_command(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       const dstring *c, const dstring *s, const dstring *a) {
   DENTER(TOP_LAYER);
   const char *args = sge_dstring_get_string(a);

   if (args == nullptr) {
      /* empty message will print an empty line (only newline) */
      args = "";
   }
   if (strcmp(jsv.get<JSV_context>(), JSV_CONTEXT_CLIENT) != 0) {
      if (const char *sub_command = sge_dstring_get_string(s); strcmp(sub_command, "INFO") == 0) {
         INFO("%s", args);
      } else if (strcmp(sub_command, "WARNING") == 0) {
         WARNING("%s", args);
      } else if (strcmp(sub_command, "ERROR") == 0) {
         ERROR("%s", args);
      } else {
         const char *command = sge_dstring_get_string(s);
         WARNING(MSG_JSV_LOG_SS, command, sub_command);
      }
   } else {
      printf("%s\n", args);
   }
   DRETURN(true);
}

/**
 * @brief Handle an ENV line of a JSV script, which adds, modifies or deletes an environment variable of the job
 *
 * @param jsv the JSV, holding the new job
 * @param answer_list receives an error message
 * @param c the command, ENV
 * @param s the modification, ADD, MOD or DEL
 * @param a the variable name, followed by the value for ADD and MOD
 * @return true if the line was applied, false on an error
 */
static bool
jsv_handle_env_command(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       const dstring *c, const dstring *s, const dstring *a) {
   DENTER(TOP_LAYER);

   bool ret = true;
   dstring variable = DSTRING_INIT;
   dstring value = DSTRING_INIT;
   bool skip_check = false;
   ocs::tom::ListPtr<ocs::tom::AN> local_answer_list;
   const auto new_job = ocs_jsv_get_job<JSV_new_job>(jsv);

   jsv_split_token(a, &variable, &value);
   const char *mod = sge_dstring_get_string(s);
   const char *var = sge_dstring_get_string(&variable);
   const char *val = sge_dstring_get_string(&value);

   DPRINTF("got from JSV \"%s %s %s\"", mod, var, (val != nullptr) ? val : "");

   if (strcmp(var, "__JSV_TEST_RESULT") == 0) {
      jsv.set<JSV_test>(true);
      jsv.set<JSV_test_pos>(0);
      jsv.set<JSV_result>(val);
      skip_check = true;
   }

   if (skip_check != true) {
      if (mod != nullptr && var != nullptr &&
          (((strcmp(mod, "MOD") == 0 || strcmp(mod, "ADD") == 0) && val != nullptr) ||
           (strcmp(mod, "DEL") == 0 && val == nullptr))) {
         const auto env_list = new_job.get<JB_env_list>();
         ocs::tom::Element<ocs::tom::VA> env_variable;

         if (var != nullptr) {
            env_variable = env_list.find_first<VA_variable>(var);
         }
         if (strcmp("ADD", mod) == 0 || strcmp("MOD", mod) == 0) {
            if (!env_variable) {
               env_variable = new_job.emplace_back_in<JB_env_list, VA_variable>(var);
            }
            env_variable.set<VA_value>(val);
         } else if (strcmp("DEL", mod) == 0) {
            if (env_variable) {
               env_list.erase(env_variable);
            }
         } else {
            answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, "\"ENV %s %s %s\" is invalid\n",
                                    (mod != nullptr) ? mod : "<null>", (var != nullptr) ? var : "<null>",
                                    (val != nullptr) ? val : "<null>");
            ret = false;
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, "\"ENV %s %s %s\" is invalid\n",
                                 (mod != nullptr) ? mod : "<null>", (var != nullptr) ? var : "<null>",
                                 (val != nullptr) ? val : "<null>");
         ret = false;
      }
   }

   /*
    * if we are in test mode the we have to check the expected result.
    * in test mode we will reject jobs if we did not get the expected
    * result otherwise
    * we will accept the job with the ret value set above including
    * the created error messages.
    */
   if (jsv.get<JSV_test>() && !skip_check) {
      const char *result_string = jsv.get<JSV_result>();

      if (const uint32_t result_pos = jsv.get<JSV_test_pos>();
          strlen(result_string) > result_pos) {
         if (const bool result = (result_string[result_pos] == '1') ? true : false;
             result != ret) {
            answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR,
                                    "\"ENV %s %s %s\" was unexpectedly %s\n",
                                    (mod != nullptr) ? mod : "<null>", (var != nullptr) ? var : "<null>",
                                    (val != nullptr) ? val : "<null>", ret ? "accepted" : "rejected");
            ret = false;
         } else {
            ret = true;
         }
      }
      jsv.set<JSV_test_pos>(jsv.get<JSV_test_pos>() + 1);
   }
   answer_list_append_list(answer_list, &local_answer_list);

   sge_dstring_free(&variable);
   sge_dstring_free(&value);
   DRETURN(ret);
}

/**
 * @brief Starts communicating with a JSV script
 *
 * Start a communication cycle to verify one job. The job to
 * be verified has to be store in 'jsv' as attribute 'JSV_new_job'.
 * Depending on the response of the JSV instance certain attributes
 * in the 'jsv' will be changes (JSV_restart, JSV_soft_shutdown,
 * JSV_done, JSV_new_job)
 *
 * @param jsv JSV instance
 * @param answer_list AN list
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: jsv_do_communication() is MT safe
 */
bool jsv_do_communication(const ocs::tom::Element<ocs::tom::JSV> jsv, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   /*
    * Try to read some error messages from stderr. There still has no command been send
    * to JSV but the initialization code might also cause errors....
    */
   char input[10000];
   while (fscanf(static_cast<FILE *>(jsv.get<JSV_err>()), "%[^\n]\n", input) == 1) {
      ERROR(MSG_JSV_LOGMSG_S, input);
      // SGE_EVENT holds a line from the JSV script, never a format
      answer_list_add(answer_list, SGE_EVENT, STATUS_DENIED, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (ret) {
      DPRINTF("JSV - START will be sent\n");
      ret &= jsv_send_command(jsv, answer_list, "START");
   }
   if (ret) {
      uint64_t start_time = sge_get_gmt64();
      bool do_retry = true;
      uint64_t jsv_timeout = sge_gmt32_to_gmt64(10);
      
      if (strcmp(jsv.get<JSV_context>(), JSV_CONTEXT_CLIENT) == 0 && getenv("SGE_JSV_TIMEOUT") != nullptr) {
         if (atoi(getenv("SGE_JSV_TIMEOUT")) > 0) {
            jsv_timeout = sge_gmt32_to_gmt64(atoi(getenv("SGE_JSV_TIMEOUT")));
            DPRINTF("JSV_TIMEOUT value of " sge_u64 " µs being used from environment variable\n", jsv_timeout);
         }         
      } else {
         jsv_timeout = sge_gmt32_to_gmt64(mconf_get_jsv_timeout());
         DPRINTF("JSV_TIMEOUT value of " sge_u64 " µs being used from qmaster parameter\n", jsv_timeout);
      }

      jsv.set<JSV_done>(false);
      jsv.set<JSV_soft_shutdown>(true);
      while (!jsv.get<JSV_done>()) {
         if (sge_get_gmt64() - start_time > jsv_timeout) {
            DPRINTF("JSV - master waited longer than " sge_u64 " µs to get response from JSV\n", jsv_timeout);
            /*
             * In case of a timeout we try it a second time. In that case we kill
             * the old instance and start a new one before we continue
             * with the verification. Otherwise we report an error which will
             * automatically reject the job which should be verified.
             */
            if (do_retry) {
               DPRINTF("JSV - will retry verification\n");
               jsv.set<JSV_restart>(false);
               jsv.set<JSV_accept>(false);
               jsv.set<JSV_done>(false);
               DPRINTF("JSV process will be stopped now\n");
               ret &= jsv_stop(jsv, answer_list, false);
               if (ret) {
                  DPRINTF("JSV process will be started now\n");
                  ret &= jsv_start(jsv, answer_list);
               }
               if (ret) {
                  DPRINTF("JSV process gets START command\n");
                  while (fscanf(static_cast<FILE *>(jsv.get<JSV_err>()), "%[^\n]\n", input) == 1) {
                     ERROR(MSG_JSV_LOGMSG_S, input);
                  }
                  ret &= jsv_send_command(jsv, answer_list, "START");
               }
               start_time = sge_get_gmt64();
               do_retry = false;
            } else {
               DPRINTF("JSV - reject due to timeout in communication process\n");
               answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR,
                                       "got no response from JSV script " SFQ, jsv.get<JSV_command>());
               jsv.set<JSV_restart>(true);
               jsv.set<JSV_soft_shutdown>(false);
               jsv.set<JSV_done>(true);
            }
         } else {
            const auto err_stream = static_cast<FILE *>(jsv.get<JSV_err>());
            const auto out_stream = static_cast<FILE *>(jsv.get<JSV_out>());

            /* 
             * read a line from the script or wait some time before you try again
             * but do this only if there was no message an the stderr stream.
             */
            if (ret) {
               if (fscanf(out_stream, "%[^\n]\n", input) == 1) {
                  dstring sub_command = DSTRING_INIT;
                  dstring command = DSTRING_INIT;
                  dstring args = DSTRING_INIT;
                  jsv_command_t commands[] = {
                     {"PARAM", jsv_handle_param_command},
                     {"ENV", jsv_handle_env_command},
                     {"LOG", jsv_handle_log_command},
                     {"RESULT", jsv_handle_result_command},
                     {"SEND", jsv_handle_send_command},
                     {"STARTED", jsv_handle_started_command},
                     {nullptr, nullptr}
                  };
                  bool handled = false;
                  int i = -1;

                  DPRINTF("JSV << \"%s\"\n", input);

                  jsv_split_commandline(input, &command, &sub_command, &args);
                  const char *command_str = sge_dstring_get_string(&command);

                  while(commands[++i].command != nullptr) {
                     if (strcmp(command_str, commands[i].command) == 0) {
                        handled = true;
                        ret &= commands[i].func(jsv, answer_list, &command, &sub_command, &args);

                        if (!ret || jsv.get<JSV_restart>() || jsv.get<JSV_accept>()) {
                           jsv.set<JSV_done>(true);
                        }
                        break;
                     }
                  }

                  if (!handled) {
                     answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, MSG_JSV_JCOMMAND_S,
                                             command_str);
                     jsv.set<JSV_accept>(false);
                     jsv.set<JSV_restart>(true);
                     jsv.set<JSV_done>(true);
                  }

                  /*
                   * set start time for new iteration
                   */
                  start_time = sge_get_gmt64();
                  sge_dstring_free(&sub_command);
                  sge_dstring_free(&command);
                  sge_dstring_free(&args);
               } else {
                  sge_usleep(10000);
               }
            }

            /* 
             * try to read a line from the error stream. If there is something then restart the
             * script before next check, do not communicate with script anymore during shutdown.
             * The last message in the stderr stream will be sent as answer to the calling function.
             */
            while (fscanf(err_stream, "%[^\n]\n", input) == 1) {
               ERROR(MSG_JSV_LOGMSG_S, input);
               // SGE_EVENT holds a line from the JSV script, never a format
               answer_list_add(answer_list, SGE_EVENT, STATUS_DENIED, ANSWER_QUALITY_ERROR);
               ret = false;
            }
            if (!ret && !jsv.get<JSV_done>()) {
               answer_list_add_sprintf(answer_list, STATUS_DENIED, ANSWER_QUALITY_ERROR, "JSV stderr is - %s", input);
               jsv.set<JSV_restart>(true);
               jsv.set<JSV_soft_shutdown>(false);
               jsv.set<JSV_done>(true);
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Convert a field into a switch name as used in the JSV protocol (usually a qalter switch).
 *
 * The attribute comes from the descriptor of the job at run time, so it stays a field number.
 *
 * @param field_nm the field
 * @param job A JB object
 * @return Pointer to a static string with the switch name or nullptr if the attribute is not supported.
 */
static const char *
jsv_field2switch_name(const int field_nm, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const char *ret = nullptr;

   if (field_nm == JB_execution_time) {
      ret = "a";
   } else if (field_nm == JB_context || field_nm == JB_context_change) {
      ret = "ac"; // although it might also be a ds or sc request we return ac here
   } else if (field_nm == JB_ar) {
      ret = "ar";
   } else if (field_nm == JB_account) {
      ret = "A";
   } else if (field_nm == JB_binding) {
      ret = "binding";
   } else if (field_nm == JB_checkpoint_interval) {
      ret = "c_interval";
   } else if (field_nm == JB_checkpoint_attr) {
      ret = "c_occasion";
   } else if (field_nm == JB_checkpoint_name) {
      ret = "ckpt";
   } else if (field_nm == JB_cwd) {
      ret = "cwd";
   } else if (field_nm == JB_deadline) {
      ret = "dl";
   } else if (field_nm == JB_stderr_path_list) {
      ret = "e";
   } else if (field_nm == JB_jid_request_list) {
      ret = "hold_jid";
   } else if (field_nm == JB_ja_ad_request_list) {
      ret = "hold_jid_ad";
   } else if (field_nm == JB_ja_tasks) {
      ret = "h";
   } else if (field_nm == JB_stdin_path_list) {
      ret = "i";
   } else if (field_nm == JB_merge_stderr) {
      ret = "j";
   } else if (field_nm == JB_jobshare) {
      ret = "js";
   } else if (field_nm == JB_request_set_list) {
      ret = nullptr;
   } else if (field_nm == JB_mail_options) {
      ret = "m";
   } else if (field_nm == JB_notify) {
      ret = "notify";
   } else if (field_nm == JB_mail_list) {
      ret = "M";
   } else if (field_nm == JB_job_name) {
      // This is a special case for JB_job_name parameter.
      // A) null
      //    qalter ... <jid>         => qalter with job id
      // B) <string>
      //    qalter -N <string> <jid> => qalter with job id using -N option to change name
      // C) :<job_name>:
      //    qalter ... <job_name>    => qalter with job name instead of job id
      // D) :<job_name>:<string2>
      //    qalter -N <string2> <job_name> => qalter with job name instead of
      //                                      job id using -N option to change name
      #define JOB_NAME_DEL ':'
      if (const char *job_name = job.get<JB_job_name>(); job_name != nullptr) {
         if (job_name[0] == JOB_NAME_DEL) {
            if (const char *help_str = strchr(&(job_name[1]), JOB_NAME_DEL);
                help_str != nullptr && help_str[1] != '\0') {
               ret = "N"; // case D
            }
         } else {
            ret = "N"; // case B
         }
      }
   } else if (field_nm == JB_stdout_path_list) {
      ret = "o";
   } else if (field_nm == JB_project) {
      ret = "P";
   } else if (field_nm == JB_priority) {
      ret = "p";
   } else if (field_nm == JB_pe) {
      ret = "pe_name";
   } else if (field_nm == JB_pe_range) {
      ret = "pe_min";
   } else if (field_nm == JB_reserve) {
      ret = "R";
   } else if (field_nm == JB_restart) {
      ret = "r";
   } else if (field_nm == JB_shell_list) {
      ret = "S";
   } else if (field_nm == JB_ja_structure) {
      ret = "t";
   } else if (field_nm == JB_env_list) {
      ret = "v"; /* v will be returned even if V was specified */
   } else if (field_nm == JB_verify_suitable_queues) {
      ret = "w";
   } else if (field_nm == JB_script_file) {
      ret = "CMDNAME";
   }
   DRETURN(ret);
}

/**
 * @brief Check if a modification request is rejected by the JSV script.
 *
 * @param answer_list The answer list to append the error message to.
 * @param job The job to check.
 * @return true if the modification request is rejected, false otherwise.
 */
bool
jsv_is_modify_rejected(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   bool ret = false;

   if (job != nullptr) {
      const char *jsv_allowed_mod = mconf_get_jsv_allowed_mod();
      const char *jsv_url = mconf_get_jsv_url();

      if (jsv_url && strcasecmp(jsv_url, "none") != 0) {

         // Check now if there are allowed modifications.
         if (jsv_allowed_mod && strcmp(jsv_allowed_mod, "none") != 0) {
            // Both lists are created up front: an empty list counts as no switch, as no list did before.
            auto allowed_switches = str_list_parse_from_string(jsv_allowed_mod, ",");
            if (!allowed_switches) {
               allowed_switches = ocs::tom::ListPtr<ocs::tom::ST>::create("");
            }
            auto got_switches = ocs::tom::ListPtr<ocs::tom::ST>::create("");

            // Transform fields into a list of corresponding qalter switch names
            // or switch combinations as accepted in JSV scripts. There are some exceptions that are handled
            // in the following code. The fields come from the descriptor of the (reduced) job at run time.
            const ocs::tom::FieldDescr *descr = job.descr();
            for (const auto f : ocs::tom::present_fields(descr)) {
               // JB_context and JB_context_change both map to ac, which is listed once
               if (const char *switch_name = jsv_field2switch_name(descr[f.pos].nm, job);
                   switch_name != nullptr && !got_switches->find_first<ST_name>(switch_name)) {
                  got_switches->emplace_back<ST_name>(switch_name);
               }
            }

            // resource list specific switches
            if (job_get_resource_list(job, JRS_SCOPE_GLOBAL, true)) {
               got_switches->emplace_back<ST_name>("global_l_hard");
            }
            if (job_get_resource_list(job, JRS_SCOPE_GLOBAL, false)) {
               got_switches->emplace_back<ST_name>("global_l_soft");
            }
            if (job_get_resource_list(job, JRS_SCOPE_MASTER, true)) {
               got_switches->emplace_back<ST_name>("master_l_hard");
            }
            if (job_get_resource_list(job, JRS_SCOPE_SLAVE, true)) {
               got_switches->emplace_back<ST_name>("slave_l_hard");
            }

            // queue list specific switches
            if (job_get_queue_list(job, JRS_SCOPE_GLOBAL, true)) {
               got_switches->emplace_back<ST_name>("global_q_hard");
            }
            if (job_get_queue_list(job, JRS_SCOPE_GLOBAL, false)) {
               got_switches->emplace_back<ST_name>("global_q_soft");
            }
            if (job_get_queue_list(job, JRS_SCOPE_MASTER, true)) {
               got_switches->emplace_back<ST_name>("master_q_hard");
            }
            if (job_get_queue_list(job, JRS_SCOPE_SLAVE, true)) {
               got_switches->emplace_back<ST_name>("slave_q_hard");
            }

            // Even if not specified on commandline. The information of the  -w switch is always passed
            // to qalter. We must allow it even if it was not specified.
            if (!allowed_switches->find_first<ST_name>("w")) {
               allowed_switches->emplace_back<ST_name>("w");
            }

            // Allow -t switch automatically if -h is used. The corresponding information of -t in the job
            // data structure is used to send the information of -h.
            if (allowed_switches->find_first<ST_name>("h")) {
               if (!allowed_switches->find_first<ST_name>("t")) {
                  allowed_switches->emplace_back<ST_name>("t");
               }
            }

            // Remove the allowed switches from the list of switches which were applied to the job we got.
            for (const auto allowed : *allowed_switches) {
               const char *name = allowed.get<ST_name>();
               const auto got_matches = got_switches->find_all<ST_name>(name);

               for (auto got_next = got_matches.begin(); got_next != got_matches.end();) {
                  const auto got = *got_next;
                  // the next match is looked up before the current one is erased
                  ++got_next;

                  got_switches->erase(got);
               }
            }

            // If there are no remaining switches then the request will not be rejected.
            if (got_switches->empty()) {
               ret = false;
            } else {
               dstring switches = DSTRING_INIT;
               bool first = true;

               for (const auto not_allowed : *got_switches) {
                  if (first) {
                     first = false;
                  } else {
                     sge_dstring_append_char(&switches, ',');
                  } 
                  sge_dstring_append(&switches, not_allowed.get<ST_name>());
               }
               ERROR(MSG_JSV_SWITCH_S, sge_dstring_get_string(&switches));
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               sge_dstring_free(&switches);
               ret = true;
            }
         } else {
            // JSV is active but no modification allowed
            ERROR(SFNMAX, MSG_JSV_ALLOWED);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = true;
         }
      }
      sge_free(&jsv_allowed_mod);
      sge_free(&jsv_url);
   }
   DRETURN(ret);
}


