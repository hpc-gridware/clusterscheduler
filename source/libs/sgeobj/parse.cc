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

/** @file
 * @brief Command line parsing, in two stages
 *
 * The clients first turn `argv` into a list of `SPA_Type` elements - one per
 * switch, with its argument - and then consume that list option by option.
 * The second stage removes what it consumed, so an option nobody asked for is
 * still in the list at the end and can be reported.
 *
 * @see parse.h
 */

#include <cstring>
#include <strings.h>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "sge_options.h"
#include "parse_qsub.h"
#include "msg_common.h"

/**
 * @brief Run a parser step on an option list held by an untyped caller
 *
 * The list "ppcmdline" refers to is taken over by an owning handle for the
 * step and handed back afterwards, also when the step throws, so the step may
 * create the list as the old functions did.
 *
 * @tparam Step the step, called with an `ocs::tom::ListPtr<ocs::tom::SPA> &`
 * @param ppcmdline the list reference, not nullptr; the list is checked to be an SPA_Type list
 * @param step the step
 * @return what the step returns
 */
template <typename Step>
static auto ocs_parse_on_untyped_cmdline(lList **ppcmdline, Step step) {
   /// gives the list back to the untyped caller when the step is done
   struct GiveBack {
      lList **ppcmdline;                          ///< the caller's list reference
      ocs::tom::ListPtr<ocs::tom::SPA> &cmdline;  ///< the handle that holds the list meanwhile
      /** @brief Hand the list back */
      ~GiveBack() {
         *ppcmdline = cmdline.release();
      }
   };

   auto cmdline = ocs::tom::ListPtr<ocs::tom::SPA>(*ppcmdline);
   *ppcmdline = nullptr;
   const GiveBack give_back{ppcmdline, cmdline};
   return step(cmdline);
}

/**
 * @brief Append the names of a comma separated string to a list
 *
 * Use cstring_list_parse_from_string() if you need a parsing function. If the
 * attribute is unique, a name already in the list is not added again (CS-3053).
 *
 * @tparam F the string field that receives a name
 * @param[in,out] list the list to append to, created if it is empty
 * @param str the comma separated names
 */
template <auto F>
static void sge_parse_string_list(ocs::tom::ListPtr<typename ocs::tom::FieldTraits<F>::Type> &list, const char *str) {
   DENTER(TOP_LAYER);

   using Type = typename ocs::tom::FieldTraits<F>::Type;
   for (const char *cp = sge_strtok(str, ","); cp != nullptr; cp = sge_strtok(nullptr, ",")) {
      if (!ocs::tom::FieldTraits<F>::unique || list->template find_first<F>(cp) == nullptr) {
         if (!list) {
            list = ocs::tom::ListPtr<Type>::create("");
         }
         list->template emplace_back<F>(cp);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Record a command line switch that takes no argument
 *
 * @param[in,out] opt_list the parsed option list, created when it is empty
 * @param opt_number the switch's id
 * @param opt_switch the switch as it was written
 * @param opt_switch_arg the argument text, if the caller wants to keep it
 * @return the new element
 *
 * @note MT-NOTE: sge_add_noarg() is MT safe
 */
ocs::tom::Element<ocs::tom::SPA> sge_add_noarg(ocs::tom::ListPtr<ocs::tom::SPA> &opt_list, const uint32_t opt_number,
                                               const char *opt_switch, const char *opt_switch_arg) {
   DENTER(TOP_LAYER);

   if (!opt_list) {
      opt_list = ocs::tom::ListPtr<ocs::tom::SPA>::create("option list");
   }

   auto new_ep = ocs::tom::ElementPtr<ocs::tom::SPA>::create();
   ocs::tom::set<SPA_number>(new_ep, opt_number);
   ocs::tom::set<SPA_switch_val>(new_ep, opt_switch);
   ocs::tom::set<SPA_switch_arg>(new_ep, opt_switch_arg);
   ocs::tom::set<SPA_occurrence>(new_ep, BIT_SPA_OCC_NOARG);
   auto ep = opt_list->push_back(std::move(new_ep));

   DRETURN(ep);
}

/**
 * @brief sge_add_noarg() for an untyped caller
 *
 * @param[in,out] popt_list the parsed option list (`SPA_Type`), checked; created when it is nullptr
 * @param opt_number the switch's id
 * @param opt_switch the switch as it was written
 * @param opt_switch_arg the argument text, if the caller wants to keep it
 * @return the new element, or nullptr if "popt_list" is nullptr
 */
lListElem *sge_add_noarg(lList **popt_list, const uint32_t opt_number, const char *opt_switch,
                         const char *opt_switch_arg) {
   if (popt_list == nullptr) {
      return nullptr;
   }
   return ocs_parse_on_untyped_cmdline(popt_list, [&](ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
      return sge_add_noarg(cmdline, opt_number, opt_switch, opt_switch_arg).raw();
   });
}

/**
 * @brief Record a command line switch that takes an argument
 *
 * @param[in,out] opt_list the parsed option list, created when it is empty
 * @param opt_number the switch's id
 * @param opt_type the argument's type
 * @param opt_switch the switch as it was written
 * @param opt_switch_arg the argument text
 * @return the new element, or an empty handle if "opt_switch" is nullptr
 *
 * @note MT-NOTE: sge_add_arg() is MT safe
 */
ocs::tom::Element<ocs::tom::SPA> sge_add_arg(ocs::tom::ListPtr<ocs::tom::SPA> &opt_list, const uint32_t opt_number,
                                             const uint32_t opt_type, const char *opt_switch,
                                             const char *opt_switch_arg) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SPA> ep;

   if (opt_switch != nullptr) {
      if (!opt_list) {
         opt_list = ocs::tom::ListPtr<ocs::tom::SPA>::create("");
      }
      ep = opt_list->emplace_back<SPA_switch_val>(opt_switch);
      ocs::tom::set<SPA_number>(ep, opt_number);
      ocs::tom::set<SPA_argtype>(ep, opt_type);
      ocs::tom::set<SPA_switch_arg>(ep, opt_switch_arg);
      ocs::tom::set<SPA_occurrence>(ep, BIT_SPA_OCC_ARG);
   }

   DRETURN(ep);
}

/**
 * @brief sge_add_arg() for an untyped caller
 *
 * @param[in,out] popt_list the parsed option list (`SPA_Type`), checked; created when it is nullptr
 * @param opt_number the switch's id
 * @param opt_type the argument's type
 * @param opt_switch the switch as it was written
 * @param opt_switch_arg the argument text
 * @return the new element, or nullptr if "popt_list" or "opt_switch" is nullptr
 */
lListElem *sge_add_arg(lList **popt_list, const uint32_t opt_number, const uint32_t opt_type, const char *opt_switch,
                       const char *opt_switch_arg) {
   if (popt_list == nullptr) {
      return nullptr;
   }
   return ocs_parse_on_untyped_cmdline(popt_list, [&](ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
      return sge_add_arg(cmdline, opt_number, opt_type, opt_switch, opt_switch_arg).raw();
   });
}

/**
 * @brief Parse a switch that takes no argument
 *
 * A switch given twice is recorded once.
 *
 * @param sp the current argument
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[in,out] cmdline receives the parsed option, created when it is empty
 * @param[out] alpp receives the error message
 * @return a pointer to the next argument
 */
char **parse_noopt(char **sp, const char *shortopt, const char *longopt, ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                   [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   if ((!strcmp(shortopt, *sp)) || (longopt && !strcmp(longopt, *sp))) {
      if (cmdline->find_first<SPA_switch_val>(shortopt) == nullptr) {
         sge_add_noarg(cmdline, 0, shortopt, nullptr);
      }
      sp++;
   }
   DRETURN(sp);
}

/**
 * @brief parse_noopt() for an untyped caller
 *
 * @param sp the current argument
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked; created when it is nullptr
 * @param[out] alpp receives the error message
 * @return as parse_noopt()
 */
char **parse_noopt(char **sp, const char *shortopt, const char *longopt, lList **ppcmdline,
                   ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   return ocs_parse_on_untyped_cmdline(ppcmdline, [&](ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
      return parse_noopt(sp, shortopt, longopt, cmdline, alpp);
   });
}

/**
 * @brief Parse a switch and everything up to the next switch as its argument
 *
 * Used where an option takes a list, e.g. `-l a=1,b=2`. Arguments are parsed
 * until another option is reached (beginning with '-'). If shortopt or longopt
 * ends in a '*', every option starting with the characters before it matches
 * (e.g. 'OAport' matches 'OA*').
 *
 * @param sp the current argument
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[in,out] cmdline receives the parsed option with its arguments as an `ST_Type` list, created when it is empty
 * @param[out] alpp receives the error message
 * @return a pointer to the next argument
 */
char **parse_until_next_opt(char **sp, const char *shortopt, const char *longopt,
                            ocs::tom::ListPtr<ocs::tom::SPA> &cmdline, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   char **rp = sp;
   if ((!strcmp(shortopt, *sp)) || (longopt && !strcmp(longopt, *sp))
       || ((shortopt[strlen(shortopt) - 1] == '*')
           && !strncmp(shortopt, *sp, strlen(shortopt) - 1))
       || (longopt && (longopt[strlen(longopt) - 1] == '*')
           && !strncmp(longopt, *sp, strlen(longopt) - 1))) {
      if (!*(++rp) || (**rp == '-') || (!**rp)) {
         stringT str;
         snprintf(str, sizeof(str), MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *sp);
         answer_list_add(alpp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(rp);
      }
      const auto ep = sge_add_arg(cmdline, 0, lListT, shortopt, nullptr);
      while (*rp && **rp != '-') {
         /* string at *rp is argument to current option */
         ocs::tom::emplace_back<ST_name>(ocs::tom::list_ref<SPA_argval_lListT>(ep), *rp);
         rp++;
      }
   }
   DRETURN(rp);
}

/**
 * @brief parse_until_next_opt() for an untyped caller
 *
 * @param sp the current argument
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked; created when it is nullptr
 * @param[out] alpp receives the error message
 * @return as parse_until_next_opt()
 */
char **parse_until_next_opt(char **sp, const char *shortopt, const char *longopt, lList **ppcmdline,
                            ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   return ocs_parse_on_untyped_cmdline(ppcmdline, [&](ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
      return parse_until_next_opt(sp, shortopt, longopt, cmdline, alpp);
   });
}

/**
 * @brief Parse a switch and everything up to the next switch
 *
 * Like #parse_until_next_opt, but without a check for a missing argument and
 * without the '*' patterns.
 *
 * @param sp the current argument
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[in,out] cmdline receives the parsed option with its arguments as an `ST_Type` list, created when it is empty
 * @param[out] alpp not used
 * @return a pointer to the next argument
 */
char **parse_until_next_opt2(char **sp, const char *shortopt, const char *longopt,
                             ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                             [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   char **rp = sp;
   if ((!strcmp(shortopt, *sp)) || (longopt && !strcmp(longopt, *sp))) {
      ++rp;

      const auto ep = sge_add_arg(cmdline, 0, lListT, shortopt, nullptr);
      while (*rp && **rp != '-') {
         /* string at *rp is argument to current option */
         ocs::tom::emplace_back<ST_name>(ocs::tom::list_ref<SPA_argval_lListT>(ep), *rp);
         rp++;
      }
   }
   DRETURN(rp);
}

/**
 * @brief parse_until_next_opt2() for an untyped caller
 *
 * @param sp the current argument
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked; created when it is nullptr
 * @param[out] alpp not used
 * @return as parse_until_next_opt2()
 */
char **parse_until_next_opt2(char **sp, const char *shortopt, const char *longopt, lList **ppcmdline,
                             ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   return ocs_parse_on_untyped_cmdline(ppcmdline, [&](ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
      return parse_until_next_opt2(sp, shortopt, longopt, cmdline, alpp);
   });
}

/**
 * @brief Parse the parameters up to the next switch as the argument of a pseudo switch
 *
 * Parameters are parsed until the next option ("-...") is reached.
 *
 * @param sp the current argument
 * @param opt the name the parameters are recorded under
 * @param[in,out] cmdline receives the parameters as an `ST_Type` list, created when it is empty
 * @param[out] alpp not used
 * @return a pointer to the next argument
 */
char **parse_param(char **sp, const char *opt, ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                   [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   char **rp = sp;
   ocs::tom::Element<ocs::tom::SPA> ep;
   while ((*rp) && (**rp != '-')) {
      /* string under rp is parameter, no option! */
      if (ep == nullptr) {
         ep = sge_add_arg(cmdline, 0, lListT, opt, nullptr);
      }
      ocs::tom::emplace_back<ST_name>(ocs::tom::list_ref<SPA_argval_lListT>(ep), *rp);
      rp++;
   }
   DRETURN(rp);
}

/**
 * @brief parse_param() for an untyped caller
 *
 * @param sp the current argument
 * @param opt the name the parameters are recorded under
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked; created when it is nullptr
 * @param[out] alpp not used
 * @return as parse_param()
 */
char **parse_param(char **sp, const char *opt, lList **ppcmdline, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   return ocs_parse_on_untyped_cmdline(ppcmdline, [&](ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
      return parse_param(sp, opt, cmdline, alpp);
   });
}

/**
 * @brief Take a flag switch out of the parsed option list
 *
 * Every occurrence of the switch is removed from "cmdline", so the list left
 * over afterwards is the set of options nobody consumed. The switch may end
 * in '*' to match every switch starting with the characters before it.
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used yet
 * @param[out] pflag set to 1 when the switch was present, untouched otherwise
 * @return true when the switch was present
 */
bool parse_flag(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal, uint32_t *pflag) {
   DENTER(BASIS_LAYER);

   auto ep = cmdline.find_first_like<SPA_switch_val>(opt);
   if (ep != nullptr) {
      char *actual_opt = sge_strdup(nullptr, ocs::tom::get<SPA_switch_val>(ep));
      while (ep != nullptr) {
         /* remove _all_ flags of same type */
         cmdline.erase(ep);
         ep = cmdline.find_first_like<SPA_switch_val>(actual_opt);
      }
      sge_free(&actual_opt);
      *pflag = 1;
      DRETURN(true);
   }
   DRETURN(false);
}

/**
 * @brief parse_flag() for an untyped caller
 *
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked
 * @param opt the switch to look for
 * @param[out] ppal not used yet
 * @param[out] pflag set to 1 when the switch was present
 * @return as parse_flag()
 */
bool parse_flag(lList **ppcmdline, const char *opt, ocs::tom::ListPtr<ocs::tom::AN> *ppal, uint32_t *pflag) {
   return parse_flag(ocs::tom::List<ocs::tom::SPA>(*ppcmdline), opt, ppal, pflag);
}

/**
 * @brief Take a switch and its list argument out of the parsed option list
 *
 * The switch may occur several times and each argument may itself be a comma
 * separated list; everything is collected into one destination list. The
 * arguments of the switch are an `ST_Type` list, as parse_until_next_opt()
 * records them.
 *
 * @tparam F the string field the collected names go into
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @param[in,out] dest receives the collected names, created when it is empty
 * @return true when the switch was present
 */
template <auto F>
bool parse_multi_stringlist(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                            [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal,
                            ocs::tom::ListPtr<typename ocs::tom::FieldTraits<F>::Type> &dest) {
   DENTER(TOP_LAYER);

   auto ep = cmdline.find_first<SPA_switch_val>(opt);
   if (ep != nullptr) {
      while (ep != nullptr) {
         /* collect all opts of same type, this is what 'multi' means in funcname!  */
         for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>(ocs::tom::get<SPA_argval_lListT>(ep))) {
            sge_parse_string_list<F>(dest, ocs::tom::get<ST_name>(sep));
         }
         cmdline.erase(ep);
         ep = cmdline.find_first<SPA_switch_val>(opt);
      }
      DRETURN(true);
   }
   DRETURN(false);
}

/// parse_multi_stringlist() into a string list
template bool parse_multi_stringlist<ST_name>(ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                                              ocs::tom::ListPtr<ocs::tom::AN> *ppal,
                                              ocs::tom::ListPtr<ocs::tom::ST> &dest);
/// parse_multi_stringlist() into a queue reference list
template bool parse_multi_stringlist<QR_name>(ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                                              ocs::tom::ListPtr<ocs::tom::AN> *ppal,
                                              ocs::tom::ListPtr<ocs::tom::QR> &dest);

/**
 * @brief Take a switch and its job/task id list out of the parsed option list
 *
 * An argument may name a whole job or single array tasks of it
 * (`jobid.taskrange`), and the switch may occur several times. A `-t` switch
 * right after the switch gives the task range of its last argument.
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] alpp receives error messages
 * @param[in,out] ppdestlist receives the collected ids (`ID_Type`), created when it is nullptr
 * @param include_names true to accept a job name where an id is expected
 * @param action what the caller intends to do with the jobs, recorded per entry
 * @return true when the switch was present and every argument was understood
 */
bool parse_multi_jobtaskslist(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                              ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                              lList **ppdestlist, const bool include_names, const uint32_t action) {
   DENTER(TOP_LAYER);
   ocs::tom::Element<ocs::tom::SPA> ep;
   bool ret = false;
   bool is_run_once = false;

   while ((ep = cmdline.find_first<SPA_switch_val>(opt)) != nullptr) {
      const auto array_def = ep.next();
      ocs::tom::ConstList<ocs::tom::RN> array_def_list;

      ret = true;
      is_run_once = true;
      if (array_def != nullptr && ocs::tom::get<SPA_number>(array_def) == t_OPT) {
         array_def_list = ocs::tom::ConstList<ocs::tom::RN>(ocs::tom::get<SPA_argval_lListT>(array_def));
      }
      for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>(ocs::tom::get<SPA_argval_lListT>(ep))) {
         ocs::tom::ConstList<ocs::tom::RN> temp_array_list;

         if (array_def_list && sep.next() == nullptr) {
            temp_array_list = array_def_list;
         }
         const auto [parse_ret, idp] = sge_parse_jobtasks(ppdestlist, ocs::tom::get<ST_name>(sep), nullptr,
                                                          include_names, temp_array_list);
         if (parse_ret == -1) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_JOB_XISINVALIDJOBTASKID_S, ocs::tom::get<ST_name>(sep));

            cmdline.erase(ep);
            DRETURN(false);
         }
         ocs::tom::set<ID_action>(idp, action);
      }
      if (array_def_list) {
         cmdline.erase(array_def);
      }
      cmdline.erase(ep);
   }

   if (is_run_once && (ep = cmdline.find_first<SPA_number>(t_OPT)) != nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_JOB_LONELY_TOPTION_S, ocs::tom::get<SPA_switch_arg>(ep));
      while ((ep = cmdline.find_first<SPA_number>(t_OPT)) != nullptr) {
         cmdline.erase(ep);
      }

      DRETURN(false);
   }

   DRETURN(ret);
}

/**
 * @brief parse_multi_jobtaskslist() for an untyped caller
 *
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked
 * @param opt the switch to look for
 * @param[out] alpp receives error messages
 * @param[in,out] ppdestlist receives the collected ids
 * @param include_names true to accept a job name where an id is expected
 * @param action what the caller intends to do with the jobs, recorded per entry
 * @return as parse_multi_jobtaskslist()
 */
bool parse_multi_jobtaskslist(lList **ppcmdline, const char *opt, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                              lList **ppdestlist, const bool include_names, const uint32_t action) {
   return parse_multi_jobtaskslist(ocs::tom::List<ocs::tom::SPA>(*ppcmdline), opt, alpp, ppdestlist, include_names,
                                   action);
}

/**
 * @brief Take a switch and its string argument out of the parsed option list
 *
 * The arguments of the switch are an `ST_Type` list. If it holds more than
 * one, only the first is taken and the switch stays for the next call.
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @param[out] str receives a freshly allocated copy of the argument, or nullptr if there is none
 * @return 1 when the switch was present
 */
int parse_string(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                 [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal, char **str) {
   DENTER(TOP_LAYER);

   const auto ep = cmdline.find_first<SPA_switch_val>(opt);
   if (ep != nullptr) {
      const auto args = ocs::tom::List<ocs::tom::ST>(ocs::tom::get<SPA_argval_lListT>(ep));
      const auto ep2 = args.front();
      if (ep2 != nullptr) {
         *str = sge_strdup(nullptr, ocs::tom::get<ST_name>(ep2));
      } else {
         *str = nullptr;
      }

      if (args.size() > 1) {
         args.erase(ep2);
      } else {
         cmdline.erase(ep);
      }

      DRETURN(true);
   }
   DRETURN(false);
}

/**
 * @brief parse_string() for an untyped caller
 *
 * @param[in,out] ppcmdline the parsed option list (`SPA_Type`), checked
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @param[out] str receives a freshly allocated copy of the argument
 * @return as parse_string()
 */
int parse_string(lList **ppcmdline, const char *opt, ocs::tom::ListPtr<ocs::tom::AN> *ppal, char **str) {
   return parse_string(ocs::tom::List<ocs::tom::SPA>(*ppcmdline), opt, ppal, str);
}

/**
 * @brief Take a switch and its string argument out of the parsed option list
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @param[out] value receives a freshly allocated copy of the argument
 * @return 1 when the switch was present
 */
int parse_string_arg(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                     [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal, char **value) {
   DENTER(TOP_LAYER);
   bool ret = false;

   if (const auto ep = cmdline.find_first<SPA_switch_val>(opt); ep != nullptr) {
      *value = sge_strdup(nullptr, ocs::tom::get<SPA_argval_lStringT>(ep));

      cmdline.erase(ep);
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Take a switch and its list of numbers out of the parsed option list
 *
 * The arguments of the switch are an `ULNG_Type` list, as the command line
 * parser records them for `-ar`.
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @return the numbers of the first occurrence of the switch, possibly an empty handle; std::nullopt when the
 *         switch is not present
 */
std::optional<ocs::tom::ListPtr<ocs::tom::ULNG>>
parse_u_longlist(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                 [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal) {
   DENTER(TOP_LAYER);

   std::optional<ocs::tom::ListPtr<ocs::tom::ULNG>> ret;

   if (const auto ep = cmdline.find_first<SPA_switch_val>(opt); ep != nullptr) {
      lList *value = nullptr;
      ocs::tom::exchange<SPA_argval_lListT>(ep, &value);
      // the parser records the numbers of the switch as an ULNG_Type list, so it is taken over unchecked
      ret = ocs::tom::ListPtr<ocs::tom::ULNG>::from_typed(value);

      cmdline.erase(ep);
   }
   DRETURN(ret);
}

/**
 * @brief Parse the `-g` grouping options of the query clients
 *
 * @param string_list the option words as they were written
 * @param[out] answer_list receives the message naming an unknown word
 * @return the combined `GROUP_*` bit mask
 */
uint32_t
parse_group_options(const ocs::tom::ConstList<ocs::tom::ST> string_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   uint32_t group_opt = GROUP_DEFAULT;

   for (const auto str_elem : string_list) {
      const char *letter_string = ocs::tom::get<ST_name>(str_elem);
      size_t i, len;
      len = strlen(letter_string);

      for (i = 0; i < len; i++) {
         char letter = letter_string[i];

         if (letter == 'd') {
            group_opt |= GROUP_NO_TASK_GROUPS;
         } else if (letter == 'c') {
            group_opt |= GROUP_CQ_SUMMARY;
         } else if (letter == 't') {
            group_opt |= GROUP_NO_PETASK_GROUPS;
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QSTAT_WRONGGCHAR_C, letter);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         }
      }
   }
   DRETURN(group_opt);
}

/* ------------------------------------------ 
   JG: TODO: make ADOC header
   parses an enumeration of specifiers into value
   the first specifier is interpreted
   as 1, second as 2, third as 4 ..
   
   return value

   0 ok
   -1 error

*/
/**
 * @brief Parse a comma separated list of names into a bit mask
 *
 * The position of a name in `set_specifier` is its bit position, which is what
 * lets a stored bit field be rendered back into names.
 *
 * @param str the names to parse
 * @param set_specifier the accepted names, terminated by nullptr
 * @param[out] value receives the bit mask
 * @param name what the field is called, used in the error message
 * @param[out] alpp receives the message naming the unknown word
 * @param none_allowed true to accept `NONE` as the empty mask
 * @return true when every name was understood
 */
bool sge_parse_bitfield_str(const char *str, const char *set_specifier[],
                            uint32_t *value, const char *name, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                            bool none_allowed) {
   DENTER(TOP_LAYER);

   const char *s;
   const char **cpp;
   uint32_t bitmask;
   /* isspace() character plus "," */
   static const char delim[] = ", \t\v\n\f\r";
   *value = 0;

   if (none_allowed && !strcasecmp(str, "none")) {
      DRETURN(true);
   }

   for (s = sge_strtok(str, delim); s; s=sge_strtok(nullptr, delim)) {

      bitmask = 1;
      for (cpp=set_specifier; *cpp != nullptr; cpp++) {
         if (!strcasecmp(*cpp, s)) {

            if ( *value & bitmask ) {
               /* whops! same specifier, already supplied! */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_READCONFIGFILESPECGIVENTWICE_SS, *cpp, name);
               answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }

            *value |= bitmask;
            break;
         }
         else   
            bitmask <<= 1;
      }

      if ( *cpp == nullptr ) {
         /* whops! unknown specifier */
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_READCONFIGFILEUNKNOWNSPEC_SS, s, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }

   }

   if (*value == 0) {
      /* empty or no specifier for userset type */
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_READCONFIGFILEEMPTYSPEC_S, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);

   }
   DRETURN(true);
}
