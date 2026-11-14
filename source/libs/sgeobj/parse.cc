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
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "tom/ocs_Element.h"

#include "sge_options.h"
#include "parse_qsub.h"
#include "msg_common.h"

/*-------------------------------------------------------------------------*/
/**
 * @brief Append the names of a comma separated string to a list
 *
 * Use cstring_list_parse_from_string() if you need a parsing function. If the
 * attribute is unique, a name already in the list is not added again (CS-3053).
 *
 * @param[in,out] lp the list to append to, created if *lp is nullptr
 * @param str the comma separated names
 * @param field the string attribute that receives a name
 * @param descr the type of the list
 */
static void sge_parse_string_list(lList **lp, const char *str, const int field, const lDescr *descr) {
   DENTER(TOP_LAYER);

   const int pos = lGetPosInDescr(descr, field);
   const bool unique = pos >= 0 && mt_is_unique(descr[pos].mt);

   for (const char *cp = sge_strtok(str, ","); cp != nullptr; cp = sge_strtok(nullptr, ",")) {
      if (!unique || lGetElemStr(*lp, field, cp) == nullptr) {
         lAddElemStr(lp, field, cp, descr);
      }
   }

   DRETURN_VOID;
}

/***************************************************************************/
/* MT-NOTE: sge_add_noarg() is MT safe */
/**
 * @brief Record a command line switch that takes no argument
 *
 * @param[in,out] popt_list the parsed option list, created when it is nullptr
 * @param opt_number the switch's id
 * @param opt_switch the switch as it was written
 * @param opt_switch_arg the argument text, if the caller wants to keep it
 * @return the new element, or nullptr when it could not be created
 *
 * @note MT-NOTE: sge_add_noarg() is MT safe
 */
lListElem *sge_add_noarg(
lList **popt_list,
uint32_t opt_number,
const char *opt_switch,
const char *opt_switch_arg 
) {
   lListElem *ep;

   if (!popt_list) {
      return nullptr;
   }
   if (!*popt_list) {
      *popt_list = lCreateList("option list", SPA_Type);
      if (!*popt_list) {
         return nullptr;
      }
   }

   ep = lCreateElem(SPA_Type);
   if (!ep) {
      return nullptr;
   }
   ocs::tom::set<SPA_number>(ep, opt_number);
   ocs::tom::set<SPA_switch_val>(ep, opt_switch);
   ocs::tom::set<SPA_switch_arg>(ep, opt_switch_arg);
   ocs::tom::set<SPA_occurrence>(ep, BIT_SPA_OCC_NOARG);
   lAppendElem(*popt_list, ep);
   return ep;
}

/***************************************************************************/

/* MT-NOTE: sge_add_arg() is MT safe */
/**
 * @brief Record a command line switch that takes an argument
 *
 * @param[in,out] popt_list the parsed option list, created when it is nullptr
 * @param opt_number the switch's id
 * @param opt_type the argument's type
 * @param opt_switch the switch as it was written
 * @param opt_switch_arg the argument text
 * @return the new element, or nullptr when it could not be created
 *
 * @note MT-NOTE: sge_add_arg() is MT safe
 */
lListElem *sge_add_arg(
lList **popt_list,
uint32_t opt_number,
uint32_t opt_type,
const char *opt_switch,
const char *opt_switch_arg
) {
   DENTER(TOP_LAYER);

   
   if (popt_list == nullptr) {
       DRETURN(nullptr);
   }

   ocs::tom::Element<ocs::tom::SPA> ep = ocs::tom::emplace_back<SPA_switch_val>(popt_list, opt_switch);

   if (ep != nullptr) {
      ocs::tom::set<SPA_number>(ep, opt_number);
      ocs::tom::set<SPA_argtype>(ep, opt_type);
      ocs::tom::set<SPA_switch_arg>(ep, opt_switch_arg);
      ocs::tom::set<SPA_occurrence>(ep, BIT_SPA_OCC_ARG);
   }

   DRETURN(ep);
}

/***************************************************************************/


/****
 **** parse_noopt
 ****
 **** parse a option from the commandline (sp). The option
 **** is given by shortopt and longopt (optional). 
 **** The parsed option is stored in ppcmdline (SPA_Type).
 **** An errormessage is appended to the answer-list (alpp).
 **** The function returns a pointer to the next argument.
 ****/
/**
 * @brief Parse a switch that takes no argument
 *
 * @param[in,out] sp the current argument; the parser advances past what it consumed
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[out] ppcmdline receives the parsed option, as an `SPA_Type` element
 * @param[out] alpp receives the error message
 * @return a pointer to the next argument
 */
char **parse_noopt(
char **sp,
const char *shortopt,
const char *longopt,
lList **ppcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

   if ( (!strcmp(shortopt, *sp)) || (longopt && !strcmp(longopt, *sp)) ) {
      if(!ocs::tom::find_first<SPA_switch_val>(*ppcmdline, shortopt)) {
         sge_add_noarg(ppcmdline, 0, shortopt, nullptr);
      }
      sp++;
   }
   DRETURN(sp);
}

/****
 **** parse_until_next_opt
 ****
 **** parse an option from the commandline (sp). The option 
 **** is given by shortopt and longopt (optional).
 **** Arguments are parsed until another option is reached
 **** (beginning with '-'). The parsed option is stored
 **** in ppcmdline (SPA_Type). An errormessage will be
 **** appended to the answer-list (alpp).
 **** The function returns a pointer to the next argument.
 **** If shortopt or longopt contains a '*' as its last (!)
 **** character, then all options matching the first given
 **** charcters are valid (e.g. 'OAport' matches 'OA*')
 ****/
/**
 * @brief Parse a switch and everything up to the next switch as its argument
 *
 * Used where an option takes a list, e.g. `-l a=1,b=2`.
 *
 * @param[in,out] sp the current argument; the parser advances past what it consumed
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[out] ppcmdline receives the parsed option, as an `SPA_Type` element
 * @param[out] alpp receives the error message
 * @return a pointer to the next argument
 */
char **parse_until_next_opt(
char **sp,
const char *shortopt,
const char *longopt,
lList **ppcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

char **rp;
stringT str;
lListElem *ep; /* SPA_Type */
   rp = sp;
   if ( (!strcmp(shortopt, *sp)) || (longopt && !strcmp(longopt, *sp)) 
        || ((shortopt[strlen(shortopt)-1] == '*')
           && !strncmp(shortopt, *sp, strlen(shortopt)-1)) 
        || (longopt && (longopt[strlen(longopt)-1] == '*')
           && !strncmp(longopt, *sp, strlen(longopt)-1)) ) {
      if(!*(++rp) || (**rp == '-') || (!**rp)) {
         snprintf(str, sizeof(str), MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *sp);
         answer_list_add(alpp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(rp);
      }
      ep = sge_add_arg(ppcmdline, 0, lListT, shortopt, nullptr);
      while (*rp && **rp != '-') {
         /* string at *rp is argument to current option */
         lAddSubStr(ep, ST_name, *rp, SPA_argval_lListT, ST_Type);
         rp++;
      }
   }
   DRETURN(rp);
}


/****
 **** parse_until_next_opt2
 ****
 **** parse an option from the commandline (sp). The option 
 **** is given by shortopt and longopt (optional).
 **** Arguments are parsed until another option is reached
 **** (beginning with '-'). The parsed option is stored
 **** in ppcmdline (SPA_Type). An errormessage will be
 **** appended to the answer-list (alpp).
 **** The function returns a pointer to the next argument.
 ****/
/**
 * @brief Parse a switch and everything up to the next switch, requiring an argument
 *
 * Like #parse_until_next_opt, but an empty argument is an error rather than
 * an accepted switch without a value.
 *
 * @param[in,out] sp the current argument; the parser advances past what it consumed
 * @param shortopt the short form of the switch
 * @param longopt the long form, or nullptr
 * @param[out] ppcmdline receives the parsed option, as an `SPA_Type` element
 * @param[out] alpp receives the error message
 * @return a pointer to the next argument
 */
char **parse_until_next_opt2(
char **sp,
const char *shortopt,
const char *longopt,
lList **ppcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

   char **rp;
   lListElem *ep; /* SPA_Type */

   rp = sp;
   if ( (!strcmp(shortopt, *sp)) || (longopt && !strcmp(longopt, *sp)) ) {
      ++rp;
      
      ep = sge_add_arg(ppcmdline, 0, lListT, shortopt, nullptr);
      while (*rp && **rp != '-') {
         /* string at *rp is argument to current option */
         lAddSubStr(ep, ST_name, *rp, SPA_argval_lListT, ST_Type);
         rp++;
      }
   }
   DRETURN(rp);
}

/****
 **** parse_param
 ****
 **** parse a list of parameters from the commandline (sp).
 **** Parameters are parsed until the next option ("-...")
 **** is reached. The parsed parameters are stored in
 **** ppcmdline (SPA_Type).
 **** The function returns a pointer to the next argument.
 ****/
/**
 * @brief Parse a switch whose argument is a free form parameter string
 *
 * @param[in,out] sp the current argument; the parser advances past what it consumed
 * @param opt the switch to match
 * @param[out] ppcmdline receives the parsed option, as an `SPA_Type` element
 * @param[out] alpp receives the error message
 * @return a pointer to the next argument
 */
char **parse_param(
char **sp,
const char *opt,
lList **ppcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

char **rp;
lListElem *ep = nullptr; /* SPA_Type */

   rp = sp;
   while( (*rp) && (**rp != '-') ) {
      /* string under rp is parameter, no option! */
      if(!ep)
         ep = sge_add_arg(ppcmdline, 0, lListT, opt, nullptr);
      ocs::tom::emplace_back<ST_name>(ocs::tom::list_ref<SPA_argval_lListT>(ep), *rp);
      rp++;
   }
   DRETURN(rp);
}

/****
 **** parse_flag
 ****
 **** look in the ppcmdline-list for a flag given
 **** by 'opt'. If it is found, true is returned and
 **** flag is set to 1. If it is not found, false
 **** is returned and flag will be untouched.
 ****
 **** If the switch occures more than one time, every
 **** occurence is removed from the ppcmdline-list.
 ****
 **** The answerlist ppal is not used yet.
 ****/
/**
 * @brief Take a flag switch out of the parsed option list
 *
 * Every occurrence of the switch is removed from `ppcmdline`, so the list left
 * over afterwards is the set of options nobody consumed.
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal not used yet
 * @param[out] pflag set to 1 when the switch was present
 * @return true when the switch was present
 */
bool parse_flag(
lList **ppcmdline,
const char *opt,
lList **ppal,
uint32_t *pflag
) {
   DENTER(BASIS_LAYER);

lListElem *ep;
char* actual_opt;

   if((ep = ocs::tom::find_first_like_rw<SPA_switch_val>(*ppcmdline, opt))) {
      actual_opt = sge_strdup(nullptr, ocs::tom::get<SPA_switch_val>(ep));
      while(ep) {
         /* remove _all_ flags of same type */
         lRemoveElem(*ppcmdline, &ep);
         ep = ocs::tom::find_first_like_rw<SPA_switch_val>(*ppcmdline, actual_opt);
      }
      sge_free(&actual_opt);
      *pflag = 1;
      DRETURN(true);
   } else {
      DRETURN(false);
   }
}

/****
 **** parse_multi_stringlist
 ****
 **** looks in the ppcmdline-list for a option given
 **** by 'opt'. If it is found, true is returned,
 **** otherwise false.
 **** Arguments after the option switch are parsed
 **** into the ppdestlist-list (given field and type).
 **** There can be multiple occurences of this switch.
 **** The arguments are collected. 
 **** The arguments can be eiter comma-separated. 
 ****/ 
/**
 * @brief Take a switch and its list argument out of the parsed option list
 *
 * The switch may occur several times and each argument may itself be a comma
 * separated list; everything is collected into one destination list.
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal receives error messages
 * @param[out] ppdestlist receives the collected values
 * @param type the descriptor of the elements to create
 * @param field the attribute the value is stored in
 * @return true when the switch was present
 */
bool parse_multi_stringlist(
lList **ppcmdline,
const char *opt,
lList **ppal,
lList **ppdestlist,
const lDescr *type,
int field 
) {
   DENTER(TOP_LAYER);
   lListElem *ep;


   if((ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt))) {
      while(ep) {
         /* collect all opts of same type, this is what 'multi' means in funcname!  */
         for_each_ep_lv(sep, ocs::tom::get<SPA_argval_lListT>(ep)) {
            sge_parse_string_list(ppdestlist, ocs::tom::get<ST_name>(sep), field, type);
         }
         lRemoveElem(*ppcmdline, &ep);
         ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt);
      }
      DRETURN(true);
   } else {
      DRETURN(false);
   }
}

/**
 * @brief Take a switch and its job/task id list out of the parsed option list
 *
 * An argument may name a whole job or single array tasks of it
 * (`jobid.taskrange`), and the switch may occur several times.
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] alpp receives error messages
 * @param[out] ppdestlist receives the collected ids
 * @param include_names true to accept a job name where an id is expected
 * @param action what the caller intends to do with the jobs, recorded per entry
 * @return true when the switch was present
 */
bool parse_multi_jobtaskslist(
lList **ppcmdline,
const char *opt,
lList **alpp,
lList **ppdestlist,
bool include_names,
uint32_t action
) {
   DENTER(TOP_LAYER);
   lListElem *ep, *idp;
   bool ret = false;
   bool is_run_once = false;

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt))) {
      lListElem *arrayDef = lNextRW(ep);
      const lList *arrayDefList = nullptr;

      ret = true;
      is_run_once = true;
      if ((arrayDef != nullptr) && ocs::tom::get<SPA_number>(arrayDef) ==  t_OPT) {
         arrayDefList = ocs::tom::get<SPA_argval_lListT>(arrayDef);
      }
      for_each_rw_lv(sep, ocs::tom::get<SPA_argval_lListT>(ep)) {
         const lList *tempArrayList = nullptr;
     
         if ((arrayDefList != nullptr) && (lNext(sep) == nullptr)) {
            tempArrayList = arrayDefList;
         }   
         if (sge_parse_jobtasks(ppdestlist, &idp, ocs::tom::get<ST_name>(sep), nullptr,
             include_names, tempArrayList) == -1) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_JOB_XISINVALIDJOBTASKID_S, ocs::tom::get<ST_name>(sep));

            lRemoveElem(*ppcmdline, &ep);
            DRETURN(false);
         }
         ocs::tom::set<ID_action>(idp, action);
      }
      if (arrayDefList != nullptr) {
         lRemoveElem(*ppcmdline, &arrayDef);
         arrayDef = nullptr;
         arrayDefList = nullptr;
      }
      lRemoveElem(*ppcmdline, &ep);
   }
   
   if (is_run_once && (ep = ocs::tom::find_first_rw<SPA_number>(*ppcmdline, t_OPT )) != nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_JOB_LONELY_TOPTION_S, ocs::tom::get<SPA_switch_arg>(ep));
      while ((ep = ocs::tom::find_first_rw<SPA_number>(*ppcmdline, t_OPT )) != nullptr) {
         lRemoveElem(*ppcmdline, &ep);
      }  
      
      DRETURN(false);
   }
   
   DRETURN(ret);
}

/**
 * @brief Take a switch and its string argument out of the parsed option list
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal receives error messages
 * @param[out] str receives a freshly allocated copy of the argument
 * @return 1 when the switch was present
 */
int parse_string(
lList **ppcmdline,
const char *opt,
lList **ppal,
char **str 
) {
   DENTER(TOP_LAYER);

   lListElem *ep, *ep2;

   if((ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt))) {
      ep2 = lFirstRW(ocs::tom::get<SPA_argval_lListT>(ep));
      if (ep2)
         *str = sge_strdup(nullptr, ocs::tom::get<ST_name>(ep2));
      else
         *str = nullptr;
      
      if (lGetNumberOfElem(ocs::tom::get<SPA_argval_lListT>(ep)) > 1) {
         lRemoveElem(ocs::tom::get<SPA_argval_lListT>(ep), &ep2);
      } else {
         lRemoveElem(*ppcmdline, &ep);
      }
     
      DRETURN(true);
   } else {
      DRETURN(false);
   }
}

/**
 * @brief Take a switch and its argument out of the parsed option list, without copying
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal receives error messages
 * @param[out] value receives the argument; it belongs to the option list
 * @return 1 when the switch was present
 */
int parse_string_arg(lList **ppcmdline, const char *opt, lList **ppal, char **value) {
   DENTER(TOP_LAYER);
   bool ret = false;
   lListElem *ep = nullptr;

   ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt);
   if(ep != nullptr) {
      *value = sge_strdup(nullptr, ocs::tom::get<SPA_argval_lStringT>(ep));

      lRemoveElem(*ppcmdline, &ep);
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Take a switch and its numeric argument out of the parsed option list
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal receives error messages
 * @param[out] value receives the number
 * @return 1 when the switch was present
 */
int parse_uint32_t(lList **ppcmdline, const char *opt, lList **ppal, uint32_t *value) {
   DENTER(TOP_LAYER);

   bool ret = false;
   lListElem *ep = nullptr;

   ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt);
   if(ep != nullptr) {
      *value = ocs::tom::get<SPA_argval_lUlongT>(ep); 

      lRemoveElem(*ppcmdline, &ep);
      ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Take a switch and its list of numbers out of the parsed option list
 *
 * @param[in,out] ppcmdline the parsed option list to consume from
 * @param opt the switch to look for
 * @param[out] ppal receives error messages
 * @param[out] value receives the numbers
 * @return 1 when the switch was present
 */
int parse_u_longlist(lList **ppcmdline, const char *opt, lList **ppal, lList **value) {
   DENTER(TOP_LAYER);

   bool ret = false;
   lListElem *ep = nullptr;

   ep = ocs::tom::find_first_rw<SPA_switch_val>(*ppcmdline, opt);
   if(ep != nullptr) {
      *value = nullptr;
      ocs::tom::exchange<SPA_argval_lListT>(ep, value);

      lRemoveElem(*ppcmdline, &ep);
      ret = true;
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
parse_group_options(lList *string_list, lList **answer_list) {
   DENTER(TOP_LAYER);
   uint32_t group_opt = GROUP_DEFAULT;

   for_each_ep_lv(str_elem, string_list) {
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
                            uint32_t *value, const char *name, lList **alpp,
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
