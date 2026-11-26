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
 * The clients first turn `argv` into a list of `SPA` elements - one per
 * switch, with its argument - and then consume that list option by option.
 * The second stage removes what it consumed, so an option nobody asked for is
 * still in the list at the end and can be reported.
 *
 * @see parse.h
 */

#include <cstring>
#include <strings.h>
#include <string>

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
 * @param ppcmdline the list reference, not nullptr; the list is checked to be an SPA list
 * @param step the step
 * @return what the step returns
 */
template <typename Step>
static auto ocs_parse_on_untyped_cmdline(ocs::tom::ListData **ppcmdline, Step step) {
   /// gives the list back to the untyped caller when the step is done
   struct GiveBack {
      ocs::tom::ListData **ppcmdline;                          ///< the caller's list reference
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
   // CS-3171: the reentrant tokenizer, sge_strtok() keeps its state in static variables
   struct saved_vars_s *context = nullptr;
   for (const char *cp = sge_strtok_r(str, ",", &context); cp != nullptr; cp = sge_strtok_r(nullptr, ",", &context)) {
      if (!ocs::tom::FieldTraits<F>::unique || list->template find_first<F>(cp) == nullptr) {
         if (!list) {
            list = ocs::tom::ListPtr<Type>::create("");
         }
         list->template emplace_back<F>(cp);
      }
   }
   sge_free_saved_vars(context);

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
   new_ep->set<SPA_number>(opt_number);
   new_ep->set<SPA_switch_val>(opt_switch);
   new_ep->set<SPA_switch_arg>(opt_switch_arg);
   new_ep->set<SPA_occurrence>(BIT_SPA_OCC_NOARG);
   auto ep = opt_list->push_back(std::move(new_ep));

   DRETURN(ep);
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
      ep.set<SPA_number>(opt_number);
      ep.set<SPA_argtype>(opt_type);
      ep.set<SPA_switch_arg>(opt_switch_arg);
      ep.set<SPA_occurrence>(BIT_SPA_OCC_ARG);
   }

   DRETURN(ep);
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
 * @param[in,out] cmdline receives the parsed option with its arguments as an `ST` list, created when it is empty
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
      const auto ep = sge_add_arg(cmdline, 0, MT_LIST, shortopt, nullptr);
      while (*rp && **rp != '-') {
         /* string at *rp is argument to current option */
         ocs::tom::emplace_back<ST_name>(ep.list_ref<SPA_argval_lListT>(), *rp);
         rp++;
      }
   }
   DRETURN(rp);
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
 * @param[in,out] cmdline receives the parsed option with its arguments as an `ST` list, created when it is empty
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

      const auto ep = sge_add_arg(cmdline, 0, MT_LIST, shortopt, nullptr);
      while (*rp && **rp != '-') {
         /* string at *rp is argument to current option */
         ocs::tom::emplace_back<ST_name>(ep.list_ref<SPA_argval_lListT>(), *rp);
         rp++;
      }
   }
   DRETURN(rp);
}


/**
 * @brief Parse the parameters up to the next switch as the argument of a pseudo switch
 *
 * Parameters are parsed until the next option ("-...") is reached.
 *
 * @param sp the current argument
 * @param opt the name the parameters are recorded under
 * @param[in,out] cmdline receives the parameters as an `ST` list, created when it is empty
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
         ep = sge_add_arg(cmdline, 0, MT_LIST, opt, nullptr);
      }
      ocs::tom::emplace_back<ST_name>(ep.list_ref<SPA_argval_lListT>(), *rp);
      rp++;
   }
   DRETURN(rp);
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
      // copy: the element holding the switch name is erased in the loop
      const std::string actual_opt{ep.get<SPA_switch_val>()};
      while (ep != nullptr) {
         /* remove _all_ flags of same type */
         cmdline.erase(ep);
         ep = cmdline.find_first_like<SPA_switch_val>(actual_opt.c_str());
      }
      *pflag = 1;
      DRETURN(true);
   }
   DRETURN(false);
}


/**
 * @brief Take a switch and its list argument out of the parsed option list
 *
 * The switch may occur several times and each argument may itself be a comma
 * separated list; everything is collected into one destination list. The
 * arguments of the switch are an `ST` list, as parse_until_next_opt()
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
         for (const auto sep : ep.get_as<SPA_argval_lListT, ocs::tom::ST>()) {
            sge_parse_string_list<F>(dest, sep.get<ST_name>());
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
 * @param[in,out] destlist receives the collected ids, created when the handle is empty
 * @param include_names true to accept a job name where an id is expected
 * @param action what the caller intends to do with the jobs, recorded per entry
 * @return true when the switch was present and every argument was understood
 */
bool parse_multi_jobtaskslist(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                              ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                              ocs::tom::ListPtr<ocs::tom::ID> &destlist, const bool include_names,
                              const uint32_t action) {
   DENTER(TOP_LAYER);
   ocs::tom::Element<ocs::tom::SPA> ep;
   bool ret = false;
   bool is_run_once = false;

   while ((ep = cmdline.find_first<SPA_switch_val>(opt)) != nullptr) {
      const auto array_def = ep.next();
      ocs::tom::ConstList<ocs::tom::RN> array_def_list;

      ret = true;
      is_run_once = true;
      if (array_def != nullptr && array_def.get<SPA_number>() == t_OPT) {
         array_def_list = array_def.get_as<SPA_argval_lListT, ocs::tom::RN>();
      }
      for (const auto sep : ep.get_as<SPA_argval_lListT, ocs::tom::ST>()) {
         ocs::tom::ConstList<ocs::tom::RN> temp_array_list;

         if (array_def_list && sep.next() == nullptr) {
            temp_array_list = array_def_list;
         }
         const auto [parse_ret, idp] = sge_parse_jobtasks(destlist, sep.get<ST_name>(), nullptr,
                                                          include_names, temp_array_list);
         if (parse_ret == -1) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_JOB_XISINVALIDJOBTASKID_S, sep.get<ST_name>());

            cmdline.erase(ep);
            DRETURN(false);
         }
         idp.set<ID_action>(action);
      }
      if (array_def_list) {
         cmdline.erase(array_def);
      }
      cmdline.erase(ep);
   }

   if (is_run_once && (ep = cmdline.find_first<SPA_number>(t_OPT)) != nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_JOB_LONELY_TOPTION_S, ep.get<SPA_switch_arg>());
      while ((ep = cmdline.find_first<SPA_number>(t_OPT)) != nullptr) {
         cmdline.erase(ep);
      }

      DRETURN(false);
   }

   DRETURN(ret);
}


/**
 * @brief Take a switch and its string argument out of the parsed option list
 *
 * The arguments of the switch are an `ST` list. If it holds more than
 * one, only the first is taken and the switch stays for the next call.
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @return whether the switch was present, and its first argument (no value when there is none)
 */
ocs::ParsedString parse_string(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                               [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal) {
   DENTER(TOP_LAYER);

   ocs::ParsedString ret;
   const auto ep = cmdline.find_first<SPA_switch_val>(opt);
   if (ep != nullptr) {
      const auto args = ep.get_as<SPA_argval_lListT, ocs::tom::ST>();
      const auto ep2 = args.front();
      if (ep2 != nullptr) {
         if (const char *name = ep2.get<ST_name>(); name != nullptr) {
            ret.value.emplace(name);
         }
      }

      if (args.size() > 1) {
         args.erase(ep2);
      } else {
         cmdline.erase(ep);
      }

      ret.found = true;
   }
   DRETURN(ret);
}


/**
 * @brief Take a switch and its string argument out of the parsed option list
 *
 * @param cmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used
 * @return whether the switch was present, and its argument (no value when there is none)
 */
ocs::ParsedString parse_string_arg(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *opt,
                                   [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *ppal) {
   DENTER(TOP_LAYER);

   ocs::ParsedString ret;
   if (const auto ep = cmdline.find_first<SPA_switch_val>(opt); ep != nullptr) {
      if (const char *arg = ep.get<SPA_argval_lStringT>(); arg != nullptr) {
         ret.value.emplace(arg);
      }

      cmdline.erase(ep);
      ret.found = true;
   }
   DRETURN(ret);
}

/**
 * @brief Take a switch and its list of numbers out of the parsed option list
 *
 * The arguments of the switch are an `ULNG` list, as the command line
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
      // the parser records the numbers of the switch as an ULNG list, which is adopted checked
      ret = ocs::tom::adopt<ocs::tom::ULNG>(ep.exchange<SPA_argval_lListT>(ocs::tom::ListPtr<ocs::tom::ULNG>{}));

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
      const char *letter_string = str_elem.get<ST_name>();
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

/**
 * @brief Parse a comma or blank separated list of names into a bit mask
 *
 * The position of a name in `set_specifier` is its bit position, which is what
 * lets a stored bit field be rendered back into names. A name given twice, an
 * unknown name and an empty result are errors; on an error `value` keeps the
 * bits understood before.
 *
 * @param str the names to parse
 * @param set_specifier the accepted names, terminated by nullptr
 * @param[out] value receives the bit mask
 * @param name what the field is called, used in the error message
 * @param[out] alpp receives the message naming the unknown word
 * @param none_allowed true to accept `NONE` as the empty mask
 * @return true when every name was understood
 *
 * @note MT-NOTE: sge_parse_bitfield_str() is MT safe (CS-3171: uses sge_strtok_r())
 */
bool sge_parse_bitfield_str(const char *str, const char *const *set_specifier,
                            uint32_t *value, const char *name, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                            const bool none_allowed) {
   DENTER(TOP_LAYER);

   const char *s;
   const char *const *cpp;
   uint32_t bitmask;
   /* isspace() character plus "," */
   static const char delim[] = ", \t\v\n\f\r";
   *value = 0;

   if (none_allowed && !strcasecmp(str, "none")) {
      DRETURN(true);
   }

   // CS-3171: the reentrant tokenizer, the queue and userset types are parsed in the worker threads of qmaster
   struct saved_vars_s *context = nullptr;
   for (s = sge_strtok_r(str, delim, &context); s; s = sge_strtok_r(nullptr, delim, &context)) {

      bitmask = 1;
      for (cpp=set_specifier; *cpp != nullptr; cpp++) {
         if (!strcasecmp(*cpp, s)) {

            if ( *value & bitmask ) {
               /* whops! same specifier, already supplied! */
               answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_GDI_READCONFIGFILESPECGIVENTWICE_SS, *cpp, name);
               sge_free_saved_vars(context);
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
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_GDI_READCONFIGFILEUNKNOWNSPEC_SS,
                                 s, name);
         sge_free_saved_vars(context);
         DRETURN(false);
      }

   }
   sge_free_saved_vars(context);

   if (*value == 0) {
      /* empty or no specifier for userset type */
      answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_GDI_READCONFIGFILEEMPTYSPEC_S, name);
      DRETURN(false);

   }
   DRETURN(true);
}
