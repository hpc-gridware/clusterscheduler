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
 * @brief Parsing textual lists into CULL lists, and printing them back
 *
 * The two forms users write are a plain list (`a,b,c`) and a definition list
 * (`name=value,name=value`). Both are parsed through an *interpretation rule*:
 * an array of attribute numbers saying which attribute each parsed word goes
 * into, which is what lets one parser fill any element type.
 *
 * @see cull_parse_util.h
 */

#include <cstdio>
#include <cstring>
#include <sstream>
#include <ostream>
#include <format>
#include <string>

#include "cull/cull.h"
#include "cull/cull_list.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_component.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/cull/sge_parse_SPA_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/*
** NAME
**   cull_parse_string_list
** PARAMETER
**   pstrlist             -  pointer to string list to be interpreted
**   listname             -  listname to be given to the cull output list
**   descr                -  cull list descriptor, i.e. list type
**   interpretation_rule  -  array of integers terminated by 0, each int
**                           represents one list field to be set, when thru,
**                           a new list element is created and interpretation
**                           starts anew with first entry
**   pplist               -  list pointer-pointer to be set to output cull list
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   parses a list of strings
**   this list might have been created by string_list
**
** NOTES
**   MT-NOTE: cull_parse_string_list() is MT safe
*/
/*
** problem: if nullptr ends string list, then nullptr cannot be used
** for fields that should not be set
*/
/**
 * @brief Parse an argv style array of strings into a CULL list
 *
 * @param pstrlist the nullptr terminated strings to parse
 * @param listname the name the resulting list is created under
 * @param descr the descriptor of the elements to create
 * @param interpretation_rule the attribute order the words are parsed into
 * @param[out] pplist receives the list
 * @return 0 on success
 */
int cull_parse_string_list(
char **pstrlist,
const char *listname,
const lDescr *descr,
int *interpretation_rule,
lList **pplist 
) {
   DENTER(BASIS_LAYER);

   lList *list;
   lListElem *ep;
   int *rule;
   int type;

   if (!pstrlist || !descr || !interpretation_rule || !pplist) {
      DPRINTF("cull_parse_string_list: nullptr pointer received\n");
      DRETURN(-1);
   }
   if (*interpretation_rule == 0) {
      DPRINTF("cull_parse_string_list: zero interpretation rule\n");
      DRETURN(-2);
   }
   list = lCreateList(listname, descr);
   if (!list) {
      DPRINTF("cull_parse_string_list: cannot create list\n");
      DRETURN(-3);      
   }
   ep = lCreateElem(descr);
   if (!ep) {
      DPRINTF("cull_parse_string_list: cannot create element\n");
      lFreeList(&list);
      DRETURN(-4);      
   }
   lAppendElem(list, ep);
   /*
   ** process the string list till it ends
   */
   for (rule = interpretation_rule; *pstrlist; pstrlist++) {
      if (*rule == 0) {
         rule = interpretation_rule;
         ep = lCreateElem(descr);
         if (!ep) {
            DPRINTF("cull_parse_string_list: cannot create another element\n");
            lFreeList(&list);
            DRETURN(-5);      
         }
         lAppendElem(list, ep);
      }
      type = lGetType(descr, *rule);
      switch (type) {
         /*
         ** the format specifiers were copied from some cull routines
         ** where similar things happen
         */
         case lDoubleT:
            {
               lDouble dd;

               if (sscanf(*pstrlist, "%99lg", &dd) != 1) {
                  DPRINTF("cull_parse_string_list: error interpreting double: %s\n", *pstrlist);
                  lFreeList(&list);
                  DRETURN(-7);
               }
               lSetDouble(ep, *rule, dd);
            }
            break;
       
         case lUlongT:
            {
               lUlong ul;

               if (sscanf(*pstrlist, sge_u32, &ul) != 1) {
                  DPRINTF("cull_parse_string_list: error interpreting ulong: %s\n", *pstrlist);
                  lFreeList(&list);
                  DRETURN(-8);
               }
               lSetUlong(ep, *rule, ul);
            }
            break;


         case lLongT:
            {
               lLong l;

               if (sscanf(*pstrlist, "%ld", &l) != 1) {
                  DPRINTF("cull_parse_string_list: error interpreting long: %s\n", *pstrlist);
                  lFreeList(&list);
                  DRETURN(-9);
               }
               lSetLong(ep, *rule, l);
            }
            break;

         case lIntT:
            {
               lInt i;

               if (sscanf(*pstrlist, "%d", &i) != 1) {
                  DPRINTF("cull_parse_string_list: error interpreting int: %s\n", *pstrlist);
                  lFreeList(&list);
                  DRETURN(-11);
               }
               lSetInt(ep, *rule, i);
            }
            break;

         case lStringT:
            if (strcasecmp("NONE", *pstrlist) != 0){
               lSetString(ep, *rule, *pstrlist);
            }
            break;
     
         case lHostT:
            if (strcasecmp("NONE", *pstrlist) != 0){
               lSetHost(ep, *rule, *pstrlist);
            }
            break;

         case lListT:
            /*
            ** list types are skipped at the moment
            */
            DPRINTF("skipped list type");
            break;

         default:
            DPRINTF("encountered unknown list field type %d\n", type);
            lFreeList(&list);
            DRETURN(-12);

      } /* end switch */
      rule++;
   } /* end for */

   if (*rule != 0) {
      DPRINTF("invalid number of entries specified\n");
      lFreeList(&list);
      DRETURN(-13);
   } 
   *pplist = list;
   DPRINTF("list parsed: \n");

   DRETURN(0);
}


/*
** NAME
**   cull_parse_definition_list
** PARAMETER
**   str                 - string to be parsed
**   lpp                 - list pointer-pointer to be set the list,
**                         or nullptr if first token is NONE
**   name                - name for the cull list
**   descr               - type of the cull list
**   interpretation_rule - fields to be filled, normally 2 entries
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   parses list of form var=value,var=value,...
*/
/**
 * @brief Parse a `name=value,name=value` string into a CULL list
 *
 * @param str the text to parse; it is modified in place by the tokenizer
 * @param[out] lpp receives the list
 * @param name the name the resulting list is created under
 * @param descr the descriptor of the elements to create
 * @param interpretation_rule the attributes the name and the value are stored in
 * @return 0 on success
 */
int cull_parse_definition_list(
char *str,
lList **lpp,
const char *name,
const lDescr *descr,
int *interpretation_rule 
) {
   DENTER(BASIS_LAYER);

   char **pstr;
   int ret;

   if (!str || !lpp) {
      DRETURN(-1);
   }
   pstr = string_list(str, " ,=\t\n\f\r\v", nullptr);
   if (!pstr) {
      DRETURN(-2);
   }
   if (!strcasecmp("NONE", pstr[0]) || !strcasecmp("UNDEFINED", pstr[0])) {
      *lpp = nullptr;
      sge_free(&pstr);
      DRETURN(0);
   }
   ret = cull_parse_string_list(pstr, name, descr, interpretation_rule, lpp);
   sge_free(&pstr);
   if (ret) {
      DRETURN(-3);
   }
   DRETURN(0);
}


/*
** NAME
**   cull_compress_definition_list
** PARAMETER
**   lp            - list to compress
**   nm_var        - field containing the variable, can
**                   be any valid type except list
**   nm_value      - field containing the value, can
**                   be any valid type except list
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   removes duplicate entries within a definition list
**   e.g. a=1,a=2 -> a=2
**   removes the unnecessary elements
*/
/**
 * @brief Collapse repeated entries of a definition list
 *
 * The last entry for a key wins, which is what a user writing the same option
 * twice expects.
 *
 * @param[in,out] lp the list to compress
 * @param nm_var the attribute holding an entry's name
 * @param nm_value the attribute holding its value
 * @param double_keys non-zero when the key is the name and the value together
 * @return 0 on success
 */
int cull_compress_definition_list(
lList *lp,
int nm_var,
int nm_value,
int double_keys 
) {
   DENTER(BASIS_LAYER);

   lListElem *ep_other;
   int is_there;
   int type;

   for_each_ep_lv(ep_one, lp) {
      for (ep_other = lFirstRW(lp); ep_other; ) {
         if (ep_one == ep_other) {
            break;
         }
         is_there = 0;
         type = lGetType(lGetListDescr(lp), nm_var);
         switch (type) {
         case lDoubleT:
            if (lGetDouble(ep_one, nm_var) == lGetDouble(ep_other, nm_var)) {
               is_there = 1;
            }
            break;
         case lUlongT:
            if (lGetUlong(ep_one, nm_var) == lGetUlong(ep_other, nm_var)) {
               is_there = 1;
            }
            break;
         case lLongT:
            if (lGetLong(ep_one, nm_var) == lGetLong(ep_other, nm_var)) {
               is_there = 1;
            }
            break;
         case lIntT:
            if (lGetInt(ep_one, nm_var) == lGetInt(ep_other, nm_var)) {
               is_there = 1;
            }
            break;
         case lStringT:
            if (double_keys) {
               if (!sge_strnullcmp(lGetString(ep_one, nm_var), lGetString(ep_other, nm_var)) 
                && !sge_strnullcmp(lGetString(ep_one, nm_value), lGetString(ep_other, nm_value)))
                  is_there = 1;
            } else {
               if (lGetString(ep_one, nm_var) && 
                   !sge_strnullcmp(lGetString(ep_one, nm_var), 
                      lGetString(ep_other, nm_var))) 
                  is_there = 1;
            }
            break;

         case lHostT:
            if (double_keys) {
               if (!sge_strnullcmp(lGetHost(ep_one, nm_var), lGetHost(ep_other, nm_var)) 
                && !sge_strnullcmp(lGetHost(ep_one, nm_value), lGetHost(ep_other, nm_value)))
                  is_there = 1;
            } else {
               if (lGetHost(ep_one, nm_var) && 
                   !sge_strnullcmp(lGetHost(ep_one, nm_var), lGetHost(ep_other, nm_var))) 
                  is_there = 1;
            }
            break;

         case lListT:
            DPRINTF(("cull_compress_definition_list: " \
               "list type not implemented with this function\n"));
            DRETURN(-4);
         default:
            DPRINTF("cull_compress_definition_list: invalid type\n");
            DRETURN(-5);
         } /* end switch */
         
         ep_other = lNextRW(ep_other);
         if (is_there) {
            lListElem *prev = lPrevRW(ep_other);
            /*
            ** ep_other must always point to a valid element, 
            ** or next "increase" fails, the element that comes later on
            ** in the list is kept, because ep_other < ep_one always
            */
            lRemoveElem(lp, &prev);
         }
      }
   }
   DRETURN(0);
}



/**
 * @brief Print selected fields of a list into a text stream.
 *
 * This function serializes the elements of an @c lList into a textual
 * representation and writes the result to the provided @c std::ostream.
 * The fields to be printed and their order are defined by
 * @p which_elements_rule.
 *
 * For each element in the list, the specified fields are converted to
 * their string representation and concatenated using delimiters from
 * @p pdelis.
 *
 * @param os Output stream where the formatted list is written.
 * @param lp Pointer to the list to be printed. If @c nullptr, the string "NONE" is written.
 * @param which_elements_rule Array of field identifiers (terminated by 0)
 *           that defines which elements of each list entry are printed
 *           and in which order.
 * @param pdelis Array of delimiter strings:
 *        - pdelis[0]: delimiter between fields within one list element
 *        - pdelis[1]: delimiter between list elements
 *        - pdelis[2]: delimiter appended after the entire list
 * @param flags Formatting flags controlling delimiter suppression and
 *        handling of empty values:
 *        - FLG_NO_VALUE_AS_EMPTY: print empty string instead of "NONE"
 *        - FLG_NO_DELIS_STRINGS: suppress delimiters for empty string fields
 *        - FLG_NO_DELIS_NUMBERS: suppress delimiters for numeric zero values
 *
 * @return 0 on success,
 *         negative value on error:
 *         - -1: invalid input parameters
 *         - -2: empty or invalid rule set
 *         - -3: list descriptor missing
 *         - -4/-5/-6: stream write error
 *         - -19: unknown field type encountered
 *
 * @note List-type fields (@c lListT) are currently skipped.
 *
 * @note Output is written directly to the stream without size limitations.
 *       Stream state should be checked by the caller if needed.
 */
int uni_print_list(std::ostream &os, const lList *lp, const int *which_elements_rule, const char *pdelis[], unsigned long flags) {
   DENTER(BASIS_LAYER);

   const int* rule;
   int type;
   const lDescr* descr;
   bool begin = true;
   std::string field;
   const char* cp = nullptr;
   std::size_t prev_len = 0;

   if (!which_elements_rule || !pdelis) {
      DPRINTF("uni_print_list: nullptr pointer received\n");
      DRETURN(-1);
   }

   if (!lp) {
      os << "NONE";
      if (!os) {
         DPRINTF("uni_print_list: error writing NONE\n");
         DRETURN(-6);
      }

      if (pdelis[2] && *pdelis[2]) {
         os << pdelis[2];
         if (!os) {
            DPRINTF("uni_print_list: error writing delimiter 2\n");
            DRETURN(-4);
         }
      }
      DRETURN(0);
   }

   if (*which_elements_rule == 0) {
      DPRINTF("uni_print_list: zero interpretation rule\n");
      DRETURN(-2);
   }

   descr = lGetListDescr(lp);
   if (!descr) {
      DPRINTF("uni_print_list: list has no descriptor\n");
      DRETURN(-3);
   }

   for_each_rw_lv(ep, lp) {
      if (!begin && pdelis[1] && *pdelis[1]) {
         os << pdelis[1];
         if (!os) {
            DPRINTF("uni_print_list: error writing delimiter 1\n");
            DRETURN(-4);
         }
      }

      prev_len = 0;

      for (rule = which_elements_rule; *rule; ++rule) {
         field.clear();
         cp = nullptr;
         type = lGetType(descr, *rule);

         switch (type) {
            case lDoubleT:
               field = std::format("{:.10g}", lGetDouble(ep, *rule));
               cp = field.c_str();
               break;

            case lUlongT:
               field = std::format("{}", lGetUlong(ep, *rule));
               cp = field.c_str();
               break;

            case lLongT:
               field = std::format("{}", lGetLong(ep, *rule));
               cp = field.c_str();
               break;

            case lIntT:
               field = std::format("{}", lGetInt(ep, *rule));
               cp = field.c_str();
               break;

            case lStringT:
               cp = lGetString(ep, *rule);
               if (!cp) {
                  cp = (flags & FLG_NO_VALUE_AS_EMPTY) ? "" : "NONE";
               }
               break;

            case lHostT:
               cp = lGetHost(ep, *rule);
               if (!cp) {
                  cp = (flags & FLG_NO_VALUE_AS_EMPTY) ? "" : "NONE";
               }
               break;

            case lListT:
               DPRINTF("skipped list type");
               cp = "";
               break;

            default:
               DPRINTF("encountered unknown list field type %d\n", type);
               DRETURN(-19);
         }

         if (pdelis[0] && *pdelis[0] &&
             (rule != which_elements_rule) &&
             (!(flags & FLG_NO_DELIS_STRINGS) || ((*cp && prev_len > 0) || (flags & FLG_NO_VALUE_AS_EMPTY))) &&
             (!(flags & FLG_NO_DELIS_NUMBERS) || !L_IS_NUM_TYPE(type) || std::strcmp(cp, "0"))) {
            os << pdelis[0];
            if (!os) {
               DPRINTF("uni_print_list: error writing delimiter\n");
               DRETURN(-5);
            }
         }

         if (*cp) {
            os << cp;
            if (!os) {
               DPRINTF("uni_print_list: error writing to stream\n");
               DRETURN(-6);
            }
         }

         prev_len = std::strlen(cp);
         begin = false;
      }
   }

   if (!begin && pdelis[2] && *pdelis[2]) {
      os << pdelis[2];
      if (!os) {
         DPRINTF("uni_print_list: error writing delimiter 2\n");
         DRETURN(-4);
      }
   }

   DRETURN(0);
}

/**
 * @brief Legacy wrapper for uni_print_list(std::ostream&, ...).
 *
 * This function provides backward-compatible output to either a C-style
 * @c FILE pointer or a character buffer. Internally, it delegates formatting
 * to the @c std::ostream-based implementation and copies the result to the
 * requested output target.
 *
 * @param fp Output file pointer. If non-null, the result is written via @c fprintf.
 * @param buff Output buffer. Used if @p fp is null.
 * @param buff_size Size of @p buff in bytes.
 * @param lp Pointer to the list to be printed.
 * @param which_elements_rule Field selection rule (see ostream overload).
 * @param pdelis Delimiters (see ostream overload).
 * @param flags Formatting flags (see ostream overload).
 * @return Same as uni_print_list(std::ostream&, ...).
 *
 * @see uni_print_list(std::ostream&, const lList*, const int*, const char*[], unsigned long)
 *
 * @note If @p buff is used, the output must fit into @p buff_size, otherwise an error is returned.
 */
int uni_print_list(FILE *fp, char *buff, uint32_t buff_size, const lList *lp, const int *which_elements_rule, const char *pdelis[], unsigned long flags) {
   DENTER(BASIS_LAYER);

   std::ostringstream ss;
   const int ret = uni_print_list(ss, lp, which_elements_rule, pdelis, flags);
   if (ret != 0) {
      DRETURN(ret);
   }

   const std::string out = ss.str();

   if (fp) {
      if (fprintf(fp, "%s", out.c_str()) < 0) {
         DRETURN(-7);
      }
   } else {
      if (!buff || buff_size == 0) {
         DRETURN(-1);
      }
      if (out.size() + 1 > buff_size) {
         DPRINTF("max_len too small\n");
         DRETURN(-1);
      }
      std::memcpy(buff, out.c_str(), out.size() + 1);
   }

   DRETURN(0);
}


/**
 * @brief Parse a switch that appears in both a hard and a soft form
 *
 * `-l` and `-q` may be written before or after `-soft`, which decides whether
 * the request has to be satisfied or only should be. This sorts the parsed
 * entries into the two attributes accordingly. An entry given in several
 * switches is merged by the rule of its type (#ocs_object_list_add_unique): a
 * queue reference is kept once, a resource request keeps its last value.
 *
 * @param cmdline the parsed option list to consume from
 * @param option the switch to look for
 * @param[in,out] job the job the requests are stored in
 * @param scope which request set the entries belong to
 * @param hard_field the request set attribute the hard requests go into
 * @param soft_field the request set attribute the soft requests go into, 0 if the switch has no soft form
 */
void parse_list_hardsoft(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *option,
                         const ocs::tom::Element<ocs::tom::JB> job, const uint32_t scope, const int hard_field,
                         const int soft_field) {
   DENTER(TOP_LAYER);
   bool handled_option = false;
   // the request lists are of the type of the runtime fields "hard_field" and "soft_field", so they stay untyped
   lList *hard_list = nullptr;
   lList *soft_list = nullptr;

   auto jrs = job_get_request_setRW(job, scope);
   if (jrs != nullptr) {
      hard_list = lCopyList(nullptr, lGetList(jrs, hard_field));
      if (soft_field) {
         soft_list = lCopyList(nullptr, lGetList(jrs, soft_field));
      }
   }

   ocs::tom::Element<ocs::tom::SPA> ep;
   const void *iterator = nullptr;
   ocs::tom::Element<ocs::tom::SPA> next_ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, option, &iterator);
   while ((ep = next_ep) != nullptr) {
      next_ep = ocs::tom::find_next_rw<SPA_switch_val>(cmdline, option, &iterator);

      if (int(ocs::tom::get<SPA_argval_lLongT>(ep)) == int(scope)) {
         // the argument list is of the type of the request field, so it stays untyped
         lList *lp = nullptr;
         ocs::tom::exchange<SPA_argval_lListT>(ep, &lp);
         if (lp != nullptr) {
            // merged without ever holding a key twice (N-H-18)
            if (!soft_field || ocs::tom::get<SPA_argval_lIntT>(ep) < 2) {
               ocs_object_list_add_unique(&hard_list, &lp);
            } else {
               ocs_object_list_add_unique(&soft_list, &lp);
            }
         }
         cmdline.erase(ep);
         handled_option = true;
      }
   }

   // now store back the request lists to the job
   if (hard_list != nullptr || soft_list != nullptr) {
      // create the job_request_set if necessary
      if (jrs == nullptr) {
         jrs = job_get_or_create_request_setRW(job, scope);
      }
      lSetList(jrs, hard_field, hard_list);
      if (soft_field) {
         lSetList(jrs, soft_field, soft_list);
      }
   }

   // in case of masterq: the global -q requests become the slave -q requests
   if (handled_option && sge_strnullcmp(option, "-masterq") == 0) {
      const auto global = job.get<JB_request_set_list>().find_first<JRS_scope>(JRS_SCOPE_GLOBAL);
      if (global != nullptr) {
         job_set_hard_queue_list(job, global.exchange<JRS_hard_queue_list>(nullptr), JRS_SCOPE_SLAVE);
         // also swap the soft requests, should there be any
         // this will then trigger an error as we do not support soft slave queue requests
         job_set_soft_queue_list(job, global.exchange<JRS_soft_queue_list>(nullptr), JRS_SCOPE_SLAVE);
      }
   }

   DRETURN_VOID;
}


/**
 * @brief Parse a path list
 *
 * Parse a path list of the format: [[host]:]path[,[[host]:]path...]
 *
 * @param path_str input string
 *
 * @return error code 0 = okay 1 = error, and the parsed list (`PN_Type`), an empty handle on an error
 *
 * @note MT-NOTE: cull_parse_path_list() is MT safe
 */
std::pair<int, ocs::tom::ListPtr<ocs::tom::PN>> cull_parse_path_list(const char *path_str) {
   DENTER(TOP_LAYER);

   char **str_str = nullptr;
   ocs::tom::ListPtr<ocs::tom::PN> list;

   char *path_string = sge_strdup(nullptr, path_str);
   bool ret_error = path_string == nullptr;
   if (!ret_error) {
      str_str = string_list(path_string, ",", nullptr);
      ret_error = str_str == nullptr || *str_str == nullptr;
   }

   if (!ret_error) {
      list = ocs::tom::ListPtr<ocs::tom::PN>::create("path_list");
      for (char **pstr = str_str; *pstr; pstr++) {
         char *path;
         char *cell;

         /* cell given ? */
         if (*pstr[0] == ':') {  /* :path */
            cell = nullptr;
            path = *pstr + 1;
         } else if ((path = strstr(*pstr, ":"))) { /* host:path */
            path[0] = '\0';
            cell = strdup(*pstr);
            path[0] = ':';
            path += 1;
         } else { /* path */
            cell = nullptr;
            path = *pstr;
         }

         const auto ep = list->push_back(ocs::tom::ElementPtr<ocs::tom::PN>::create());
         ocs::tom::set<PN_path>(ep, path);
         if (cell) {
            ocs::tom::set<PN_host>(ep, cell);
            sge_free(&cell);
         }
      }
   }
   if (path_string) {
      sge_free(&path_string);
   }
   if (str_str) {
      sge_free(&str_str);
   }
   DRETURN(std::make_pair(ret_error ? 1 : 0, std::move(list)));
}

/**
 * @brief Parse a jid list
 *
 * parse a jid list of the format jid[,jid,...]
 *
 * @param str input string to be parsed
 *
 * @return 0 on success, 2 when the string holds no job id, 3 when it could not be parsed; and the
 *         `ST_Type` result list, an empty handle on an error
 *
 * @note MT-NOTE: cull_parse_jid_hold_list() is MT safe
 */
std::pair<int, ocs::tom::ListPtr<ocs::tom::ST>> cull_parse_jid_hold_list(const char *str) {
   DENTER(TOP_LAYER);

   int rule[] = {ST_name, 0};

   char *s = sge_strdup(nullptr, str);
   if (!s) {
      DRETURN(std::make_pair(3, ocs::tom::ListPtr<ocs::tom::ST>()));
   }
   char **str_str = string_list(s, ",", nullptr);
   if (!str_str || !*str_str) {
      sge_free(&s);
      DRETURN(std::make_pair(2, ocs::tom::ListPtr<ocs::tom::ST>()));
   }
   // cull_parse_string_list() is generic over the descriptor; it fills the list only on success
   lList *parsed = nullptr;
   const int i_ret = cull_parse_string_list(str_str, "jid_hold list", ST_Type, rule, &parsed);
   // the list was created with ST_Type above, so it is taken over unchecked
   auto list = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(parsed);

   sge_free(&s);
   sge_free(&str_str);
   DRETURN(std::make_pair(i_ret != 0 ? 3 : 0, std::move(list)));
}

/**
 * @brief Parse -h switch of qsub and qalter
 *
 * Parse the hold flags of -h switches which can be used with
 * qaub and qalter
 *
 * @param hold_str string tobe parsed
 * @param prog_number program number
 *
 * @return hold state -1 in case of error
 *
 * @note MT-NOTE: sge_parse_hold_list() is MT safe
 */
int 
sge_parse_hold_list(const char *hold_str, uint32_t prog_number) {
   DENTER(TOP_LAYER);

   int i, j;
   int target = 0;
   int op_code = 0;

   i = strlen(hold_str);

   for (j = 0; j < i; j++) {
      switch (hold_str[j]) {
      case 'n':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = MINUS_H_TGT_USER|MINUS_H_TGT_OPERATOR|MINUS_H_TGT_SYSTEM;
         break;
      case 's':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_SYSTEM;         
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_SYSTEM;
         }   
         break;
      case 'o':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_OPERATOR;         
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_OPERATOR;
         }
         break;
         
      case 'u':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_USER;
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_USER;
         }
         break;
      case 'S':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_SYSTEM;
         break;
      case 'U':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_USER;
         break;
      case 'O':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_OPERATOR;
         break;
      default:
         target = -1;
      }

      if (target == -1)
         break;
   }

   if (target != -1)
      target |= op_code;

   DRETURN(target);
}

