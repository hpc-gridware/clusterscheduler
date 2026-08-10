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

#include "sge_options.h"
#include "parse_qsub.h"
#include "msg_common.h"

/*-------------------------------------------------------------------------*/
/* use cstring_list_parse_from_string() if you need a parsing function */
static void sge_parse_string_list(lList **lp, const char *str, int field, lDescr *descr) {
   DENTER(TOP_LAYER);

   const char *cp;

   cp = sge_strtok(str, ",");
   lAddElemStr(lp, field, cp, descr);
   while((cp = sge_strtok(nullptr, ","))) {
      lAddElemStr(lp, field, cp, descr);
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
   lSetUlong(ep, SPA_number, opt_number);
   lSetString(ep, SPA_switch_val, opt_switch);
   lSetString(ep, SPA_switch_arg, opt_switch_arg);
   lSetUlong(ep, SPA_occurrence, BIT_SPA_OCC_NOARG);
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

   lListElem *ep;
   
   if (popt_list == nullptr) {
       DRETURN(nullptr);
   }

   ep = lAddElemStr(popt_list, SPA_switch_val, opt_switch, SPA_Type);

   if (ep != nullptr) {
      lSetUlong(ep, SPA_number, opt_number);
      lSetUlong(ep, SPA_argtype, opt_type);
      lSetString(ep, SPA_switch_arg, opt_switch_arg);
      lSetUlong(ep, SPA_occurrence, BIT_SPA_OCC_ARG);
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
      if(!lGetElemStr(*ppcmdline, SPA_switch_val, shortopt)) {
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
char **rp;
stringT str;
lListElem *ep; /* SPA_Type */
   DENTER(TOP_LAYER);

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
   char **rp;
   lListElem *ep; /* SPA_Type */

   DENTER(TOP_LAYER);

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
char **rp;
lListElem *ep = nullptr; /* SPA_Type */

   DENTER(TOP_LAYER);

   rp = sp;
   while( (*rp) && (**rp != '-') ) {
      /* string under rp is parameter, no option! */
      if(!ep)
         ep = sge_add_arg(ppcmdline, 0, lListT, opt, nullptr);
      lAddElemStr(lGetListRef(ep, SPA_argval_lListT), ST_name, *rp, ST_Type);
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

   if((ep = lGetElemStrLikeRW(*ppcmdline, SPA_switch_val, opt))) {
      actual_opt = sge_strdup(nullptr, lGetString(ep, SPA_switch_val));
      while(ep) {
         /* remove _all_ flags of same type */
         lRemoveElem(*ppcmdline, &ep);
         ep = lGetElemStrLikeRW(*ppcmdline, SPA_switch_val, actual_opt);
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
lDescr *type,
int field 
) {
   DENTER(TOP_LAYER);
   lListElem *ep;


   if((ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt))) {
      while(ep) {
         /* collect all opts of same type, this is what 'multi' means in funcname!  */
         for_each_ep_lv(sep, lGetList(ep, SPA_argval_lListT)) {
            sge_parse_string_list(ppdestlist, lGetString(sep, ST_name), field, type);
         }
         lRemoveElem(*ppcmdline, &ep);
         ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt);
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

   while ((ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt))) {
      lListElem *arrayDef = lNextRW(ep);
      const lList *arrayDefList = nullptr;

      ret = true;
      is_run_once = true;
      if ((arrayDef != nullptr) && lGetUlong(arrayDef, SPA_number) ==  t_OPT) {
         arrayDefList = lGetList(arrayDef, SPA_argval_lListT);
      }
      for_each_rw_lv(sep, lGetList(ep, SPA_argval_lListT)) {
         const lList *tempArrayList = nullptr;
     
         if ((arrayDefList != nullptr) && (lNext(sep) == nullptr)) {
            tempArrayList = arrayDefList;
         }   
         if (sge_parse_jobtasks(ppdestlist, &idp, lGetString(sep, ST_name), nullptr,
             include_names, tempArrayList) == -1) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_JOB_XISINVALIDJOBTASKID_S, lGetString(sep, ST_name));

            lRemoveElem(*ppcmdline, &ep);
            DRETURN(false);
         }
         lSetUlong(idp, ID_action, action);
      }
      if (arrayDefList != nullptr) {
         lRemoveElem(*ppcmdline, &arrayDef);
         arrayDef = nullptr;
         arrayDefList = nullptr;
      }
      lRemoveElem(*ppcmdline, &ep);
   }
   
   if (is_run_once && (ep = lGetElemUlongRW(*ppcmdline, SPA_number, t_OPT )) != nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_JOB_LONELY_TOPTION_S, lGetString(ep, SPA_switch_arg));
      while ((ep = lGetElemUlongRW(*ppcmdline, SPA_number, t_OPT )) != nullptr) {
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

   if((ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt))) {
      ep2 = lFirstRW(lGetList(ep, SPA_argval_lListT));
      if (ep2)
         *str = sge_strdup(nullptr, lGetString(ep2, ST_name));
      else
         *str = nullptr;
      
      if (lGetNumberOfElem(lGetList(ep, SPA_argval_lListT)) > 1) {
         lRemoveElem(lGetListRW(ep, SPA_argval_lListT), &ep2);
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

   ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt);
   if(ep != nullptr) {
      *value = sge_strdup(nullptr, lGetString(ep, SPA_argval_lStringT));

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

   ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt);
   if(ep != nullptr) {
      *value = lGetUlong(ep, SPA_argval_lUlongT); 

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

   ep = lGetElemStrRW(*ppcmdline, SPA_switch_val, opt);
   if(ep != nullptr) {
      *value = nullptr;
      lXchgList(ep, SPA_argval_lListT, value);

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
      const char *letter_string = lGetString(str_elem, ST_name);
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
