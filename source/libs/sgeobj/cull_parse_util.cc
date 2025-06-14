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

#include <cstdio>
#include <cstring>
#include <strings.h>

#include "cull/cull.h"
#include "cull/cull_list.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_component.h"

#include "sge_daemonize.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/cull/sge_parse_SPA_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"

static int fprint_name_value_list(FILE *fp, char *name, lList *thresholds, int print_slots,
     int nm_name, int nm_strval, int nm_doubleval);
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
int cull_parse_string_list(
char **pstrlist,
const char *listname,
lDescr *descr,
int *interpretation_rule,
lList **pplist 
) {
   lList *list;
   lListElem *ep;
   int *rule;
   int type;

   DENTER(BASIS_LAYER);
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
         case lFloatT:
            {
               lFloat f;

               if (sscanf(*pstrlist, "%f", &f) != 1) {
                  DPRINTF("cull_parse_string_list: error interpreting float: %s\n", *pstrlist);
                  lFreeList(&list);
                  DRETURN(-6);
               }
               lSetFloat(ep, *rule, f);
            }
            break;

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

         case lCharT:
            {
               lChar c;

               if (sscanf(*pstrlist, "%c", &c) != 1) {
                  DPRINTF("cull_parse_string_list: error interpreting char: %s\n", *pstrlist);
                  lFreeList(&list);
                  DRETURN(-10);
               }
               lSetChar(ep, *rule, c);
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
int cull_parse_definition_list(
char *str,
lList **lpp,
const char *name,
lDescr *descr,
int *interpretation_rule 
) {
   char **pstr;
   int ret;

   DENTER(BASIS_LAYER);
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
**   cull_merge_definition_list
** PARAMETER
**   lpp_old       - pointer to list which is to be changed, pointer cannot be nullptr
**                   if list is nullptr then list is created if lp_new contains any
                     elements
**   lp_new        - list to update *lpp_old with, or nullptr
**   nm_var        - field containing the variable, must be in both lists, can
**                   be any valid type except list
**   nm_value      - field containing the value, must be in both lists, can
**                   be any valid type except list
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   merges two lists of form var=value,var=value,...
**   does NOT remove duplicate entries within one list,
**   for this purpose see cull_compress_definition_list
*/
int cull_merge_definition_list(
lList **lpp_old,
lList *lp_new,
int nm_var,
int nm_value 
) {
   lListElem *ep_old;
   lListElem *ep_new;
   int type;
   int is_there;

   DENTER(CULL_LAYER);
   
   if (!lp_new) {
      DRETURN(0);
   }
   
   if (!lpp_old) {
      DRETURN(-1);
   }
   /*
   ** problem: type equal check missing here, look at lAddList
   ** problem: also check if (both) have nm_var and nm_value
   */
   
   if (lGetType(lGetListDescr(*lpp_old), nm_var) != 
       lGetType(lGetListDescr(lp_new), nm_var)) {
      DPRINTF("cull_merge_definition_list: conflicting types for nm_var\n");
      DRETURN(-2);
   }
   if (lGetType(lGetListDescr(*lpp_old), nm_value) != 
       lGetType(lGetListDescr(lp_new), nm_value)) {
      DPRINTF("cull_merge_definition_list: conflicting types for nm_value\n");
      DRETURN(-3);
   }

   if (!*lpp_old) {
      *lpp_old = lCreateList("copied list", lGetListDescr(lp_new));
      if (!*lpp_old) {
         DPRINTF("memory allocation fault\n");
         DRETURN(-4);
      }
   }

   
   /*
   ** look for each variable if it is already in the list
   ** if it is, change the value to the new value
   ** if it isn't, append it
   */
   for_each_rw(ep_new, lp_new) {
      is_there = 0;
      for_each_rw(ep_old, *lpp_old) {
         type = lGetType(lGetListDescr(lp_new), nm_var);
         switch (type) {
         case lFloatT:
            if (lGetFloat(ep_new, nm_var) == lGetFloat(ep_old, nm_var)) {
               is_there = 1;
            }
            break;
         case lDoubleT:
            if (lGetDouble(ep_new, nm_var) == lGetDouble(ep_old, nm_var)) {
               is_there = 1;
            }
            break;
         case lUlongT:
            if (lGetUlong(ep_new, nm_var) == lGetUlong(ep_old, nm_var)) {
               is_there = 1;
            }
            break;
         case lLongT:
            if (lGetLong(ep_new, nm_var) == lGetLong(ep_old, nm_var)) {
               is_there = 1;
            }
            break;
         case lCharT:
            if (lGetChar(ep_new, nm_var) == lGetChar(ep_old, nm_var)) {
               is_there = 1;
            }
            break;
         case lIntT:
            if (lGetInt(ep_new, nm_var) == lGetInt(ep_old, nm_var)) {
               is_there = 1;
            }
            break;
         case lStringT:
            if ( /* lGetString(ep_new, nm_var) &&  ??? andreas */
                !sge_strnullcmp(lGetString(ep_new, nm_var), lGetString(ep_old, nm_var))) {
               is_there = 1;
            }
            break;
         case lHostT:
            if ( !sge_strnullcmp(lGetHost(ep_new, nm_var), lGetHost(ep_old, nm_var))) {
               is_there = 1;
            }
            break;

         case lListT:
            DPRINTF(("cull_merge_definition_list: " \
               "list type not implemented with this function\n"));
            DRETURN(-4);
         default:
            DPRINTF("cull_merge_definition_list: invalid type\n");
            DRETURN(-5);
         } /* end switch */
         if (is_there) {
            break;
         }
      }
      if (ep_new == ep_old) {
         /*
         ** do nothing
         */
      }
      else if (is_there) {
         type = lGetType(lGetListDescr(lp_new), nm_value);
         switch (type) {
         case lFloatT:
            lSetFloat(ep_old, nm_value, lGetFloat(ep_new, nm_value));
            break;
         case lDoubleT:
            lSetDouble(ep_old, nm_value, lGetDouble(ep_new, nm_value));
            break;
         case lUlongT:
            lSetUlong(ep_old, nm_value, lGetUlong(ep_new, nm_value));
            break;
         case lLongT:
            lSetLong(ep_old, nm_value, lGetLong(ep_new, nm_value));
            break;
         case lCharT:
            lSetChar(ep_old, nm_value, lGetChar(ep_new, nm_value));
            break;
         case lIntT:
            lSetInt(ep_old, nm_value, lGetInt(ep_new, nm_value));
            break;
         case lStringT:
            lSetString(ep_old, nm_value, lGetString(ep_new, nm_value));
            break;
         case lHostT:
            lSetHost(ep_old, nm_value, lGetHost(ep_new, nm_value));
            break;

         case lListT:
            DPRINTF(("cull_merge_definition_list: " \
               "list type not implemented with function\n"));
            DRETURN(-6);
         default:
            DPRINTF("cull_merge_definition_list: invalid type to set\n");
            DRETURN(-7);
         } /* end switch */
      }
      else {
         lAppendElem(*lpp_old, lCopyElem(ep_new));
      }
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
int cull_compress_definition_list(
lList *lp,
int nm_var,
int nm_value,
int double_keys 
) {
   const lListElem *ep_one;
   lListElem *ep_other;
   int is_there;
   int type;

   DENTER(BASIS_LAYER);

   for_each_ep(ep_one, lp) {
      for (ep_other = lFirstRW(lp); ep_other; ) {
         if (ep_one == ep_other) {
            break;
         }
         is_there = 0;
         type = lGetType(lGetListDescr(lp), nm_var);
         switch (type) {
         case lFloatT:
            if (lGetFloat(ep_one, nm_var) == lGetFloat(ep_other, nm_var)) {
               is_there = 1;
            }
            break;
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
         case lCharT:
            if (lGetChar(ep_one, nm_var) == lGetChar(ep_other, nm_var)) {
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



/*
** NAME
**   cull_parse_simple_list
** PARAMETER
**   str                 - string to be parsed
**   lpp                 - list pointer-pointer to be set the list,
**                         or nullptr if first token is NONE
**   name                - name for the cull list
**   descr               - type of the cull list
**   interpretation_rule - fields to be filled, normally 1 entry
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   parses a simple list like a,b,c...
**   string value NONE as first token is interpreted as no list
*/
int cull_parse_simple_list(
char *str,
lList **lpp,
char *name,
lDescr *descr,
int *interpretation_rule 
) {
   char **pstr;
   int ret;

   DENTER(BASIS_LAYER);
   if (!str || !lpp) {
      DPRINTF("cull_parse_simple_list: nullptr pointer received\n");
      DRETURN(-1);
    }

   /*
   ** = is not a delimiter here
   */
   pstr = string_list(str, " ,\t\n\f\r\v",  nullptr);
   if (!pstr) {
      DPRINTF("cull_parse_simple_list: could not parse string\n");
      DRETURN(-2);
   }
   if (!strcasecmp("NONE", pstr[0])) {
      *lpp = nullptr;
      sge_free(&pstr);
      DPRINTF("cull_parse_simple_list: String is NONE, no list, not an error\n");
      DRETURN(0);
   }
   
   ret = cull_parse_string_list(pstr, name, descr, interpretation_rule, lpp);
   sge_free(&pstr);
   if (ret) {
      DPRINTF("cull_parse_simple_list: cull_parse_string_list returns %d\n", ret);
      DRETURN(-3);
   }
   DRETURN(0);
}


/*
** NAME
**   uni_print_list
** PARAMETER
**   fp                    -   pointer to file to write string list to or nullptr
**   buff                  -   buffer to write string list to, can be nullptr
**                             if fp is not, data is then written to fp
**   max_len               -   maximum bytes to be written, can be 0 if
**                             fp is given
**   lp                    -   list to write to file or buffer
**   interpretation_rule   -   list elements to be written,
**                             all elements EXCEPT LISTS can be given here,
**                             to say it once more: sublists are not supported
**   pdelis                -   delimiter string to be inserted between elements
**                             pdelis[0] : string used to separate fields
**                             pdelis[1] : string used to separate records
**                             pdelis[2] : string inserted after end of list,
**                                         e.g. a newline
**   flags                 -   FLG_NO_DELIS_STRINGS - leave out delimiters
**                             before zero or nullptr strings
**                             FLG_NO_DELIS_NUMBERS - leave out delimiters
**                             and the following "0" if a number is 0
**                             Be careful in using these 2 options. If more
**                             than one field is optional, this can lead to
**                             ambiguous parsing!
**
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   prints certain fields of a list to a given stream or buffer, separated by 
**   delimiters, be careful: sublists are NOT yet implemented
*/
int uni_print_list(FILE *fp, char *buff, u_long32 buff_size, const lList *lp, int *which_elements_rule, const char *pdelis[], unsigned long flags) {
   DENTER(BASIS_LAYER);
   lListElem *ep;
   int *rule;
   int type;
   const lDescr *descr;
   int begin = 1;
   int cb = 0;
   u_long32 cb_sum = 0;
   char str[256];
   const char *cp;


   /*
   ** problem: one might allow nullptr deli as no deli
   */
   if (!which_elements_rule || !pdelis) {
      DPRINTF("uni_print_list: nullptr pointer received\n");
      DRETURN(-1);
   }
   if (!fp && !buff) {
      DPRINTF("uni_print_list: must have either file or buffer\n");
      DRETURN(-1);
   }
   if (buff && !buff_size) {
      DPRINTF("uni_print_list: zero len output required\n");
      DRETURN(-1);
   }

   if (!lp) {
      if (buff_size && (cb_sum + (sizeof("NONE") - 1) > buff_size)) {
         DPRINTF("max_len too small even for zero list\n");
         DRETURN(-1);
      }
      if (fp) {
/*          cb = FPRINTF((fp, "NONE")); */
         FPRINTF_ASSIGN(cb, (fp, "NONE"));
      }
      else {
         cb = sizeof("NONE") - 1;
         strcpy(buff, "NONE");
      }
      buff += cb;
      cb_sum += cb;
      if (pdelis[2] && *pdelis[2]) {
         if (buff_size && (cb_sum + strlen(pdelis[2]) > buff_size)) {
            DPRINTF("max_len too small even for zero list plus delimiter\n");
            DRETURN(-1);
         }
         if (fp) {
            cb = fprintf(fp, "%s", pdelis[2]);
         } else {
            cb = strlen(pdelis[2]);
            snprintf(buff, buff_size, "%s", pdelis[2]);
         }
         buff += cb;
         cb_sum += cb;
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

   for_each_rw(ep, lp) {

      if (!begin && pdelis[1] && *pdelis[1]) {
         if (buff_size && (cb_sum + strlen(pdelis[1]) > buff_size)) {
            DPRINTF("max_len too small\n");
            DRETURN(-1);
         }
         if (fp) {
/*             cb = FPRINTF((fp, "%s", pdelis[1])); */
            FPRINTF_ASSIGN(cb, (fp, "%s", pdelis[1]));
         }
         else {
            cb = strlen(pdelis[1]);
            snprintf(buff, buff_size, "%s", pdelis[1]);
         }
         if (cb <= 0) {
            DPRINTF("uni_print_list: error writing delimiter 1\n");
            DRETURN(-4);
         }
         buff += cb;
         cb_sum += cb;
      }

      cb = 0;
      for (rule = which_elements_rule; *rule; rule++) {
         /*
         ** before writing the delimiter, we look ahead
         */
         cp = str;
         type = lGetType(descr, *rule);

         switch (type) {
         case lFloatT:

            snprintf(str, sizeof(str), "%.10g", lGetFloat(ep, *rule));
            break;

         case lDoubleT:
            snprintf(str, sizeof(str), "%.10g", lGetDouble(ep, *rule));
            break;
       
         case lUlongT:
            snprintf(str, sizeof(str), sge_u32, lGetUlong(ep, *rule));
            break;

         case lLongT:
            snprintf(str, sizeof(str), "%ld", lGetLong(ep, *rule));
            break;

         case lCharT:
            snprintf(str, sizeof(str), "%c", lGetChar(ep, *rule));
            break;

         case lIntT:
            snprintf(str, sizeof(str), "%d", lGetInt(ep, *rule));
            break;

         case lStringT:
            cp = lGetString(ep, *rule);
            if (!cp) {
               if (flags & FLG_NO_VALUE_AS_EMPTY) {
                  cp = "";
               } else {
                  cp = "NONE";
               }
            }
            break;

         case lHostT:
            cp = lGetHost(ep, *rule);
            if (!cp) {
               if (flags & FLG_NO_VALUE_AS_EMPTY) {
                  cp = "";
               } else {
                  cp = "NONE";
               }
            }
            break;


         case lListT:
            /*
            ** list types are skipped at the moment
            */
            *str = 0;
            DPRINTF("skipped list type");
            break;

         default:
            DPRINTF("encountered unknown list field type %d\n", type);
            DRETURN(-19);
         } /* end switch */

         /*
         ** now that we know what the field is we can decide to suppress
         ** the delimiter
         ** at the moment only trailing delimiters are suppressed
         ** one might also want to suppress delimiters in eg.  [host]:path
         ** but if there are more than one optional field and delimiters are
         ** left out, then unique interpretation is lost, so be careful
         */
         if (pdelis[0] && *pdelis[0] && (rule != which_elements_rule)
             && (!(flags & FLG_NO_DELIS_STRINGS) || (*cp && cb) || (flags & FLG_NO_VALUE_AS_EMPTY))
             && (!(flags & FLG_NO_DELIS_NUMBERS) || !L_IS_NUM_TYPE(type) || strcmp(cp, "0"))) {
            if (buff_size && (cb_sum + strlen(pdelis[0]) > buff_size)) {
               DPRINTF("max_len too small\n");
               DRETURN(-1);
            }
            if (fp) {
/*                cb = FPRINTF((fp, "%s", pdelis[0])); */
               FPRINTF_ASSIGN(cb, (fp, "%s", pdelis[0]));
            }
            else {
               cb = strlen(pdelis[0]);
               snprintf(buff, buff_size, "%s", pdelis[0]);
            }
            if (cb <= 0) {
               DPRINTF("uni_print_list: error writing delimiter\n");
               DRETURN(-5);
            }
            buff += cb;
            cb_sum += cb;
         }

         if (buff_size && (cb_sum + strlen(cp) > buff_size)) {
            DPRINTF("max_len too small\n");
            DRETURN(-1);
         }
         if (*cp) {
            if (fp) {
               FPRINTF_ASSIGN(cb, (fp, "%s", cp));
            }
            else {
               cb = strlen(cp);
               snprintf(buff, buff_size, "%s", cp);
            }
            if (cb <= 0) {
               DPRINTF("uni_print_list: error writing to file\n");
               DRETURN(-6);
            }
            buff += cb;
            cb_sum += cb;
         }
         begin = 0;

      } /* end for this list element */
   } /* end for_each */

   if (!begin && pdelis[2] && *pdelis[2]) {
      if (buff_size && (cb_sum + strlen(pdelis[2]) > buff_size)) {
         DPRINTF("max_len too small\n");
         DRETURN(-1);
      }
      if (fp) {
         FPRINTF_ASSIGN(cb, (fp, "%s", pdelis[2]));
      }
      else {
         cb = strlen(pdelis[2]);
         snprintf(buff, buff_size, "%s", pdelis[2]);
      }
      if (cb <= 0) {
         DPRINTF("uni_print_list: error writing delimiter 1\n");
         DRETURN(-4);
      }
      buff += cb;
      cb_sum += cb;
   }

   DRETURN(0);
FPRINTF_ERROR:
   DRETURN(-7);
}

/****** cull_parse_util/fprint_cull_list() *************************************
*  NAME
*     fprint_cull_list() --  Prints str and field 
*
*  SYNOPSIS
*     int fprint_cull_list(FILE *fp, char *str, lList *lp, int fi) 
*
*  FUNCTION
*     Prints str and field 'fi' (must be string) of
*     every element of lList lp to file fp separated
*     by blanks. If fp is nullptr, "NONE" will be printed.
*
*  INPUTS
*     FILE *fp  - a file
*     char *str - a string name of list 
*     lList *lp - a list
*     int fi    - an element from the list to be printed 
*
*  RESULT
*     int - 0 on success, -1 otherwise
*
*  NOTES
*     MT-NOTE: fprint_cull_list() is MT safe 
*
*******************************************************************************/
int fprint_cull_list(FILE *fp, char *str, lList *lp, int fi)
{
   const lListElem *ep;

   DENTER(TOP_LAYER);

   FPRINTF((fp, "%s", str));

   if(!lp) {
      FPRINTF((fp, "NONE\n"));
   }
   else {
      for_each_ep(ep, lp) {
         FPRINTF((fp, "%s", lGetString(ep, fi)));
         if (lNext(ep))
            FPRINTF((fp, " "));
      }
      FPRINTF((fp, "\n"));
   }

   DRETURN(0);

FPRINTF_ERROR:
   DRETURN(-1);
}                   


/****** cull_parse_util/fprint_thresholds() ************************************
*  NAME
*     fprint_thresholds() -- Print a name=value list of type CE_Type
*
*  SYNOPSIS
*     int fprint_thresholds(FILE *fp, char *name, lList *thresholds, int 
*     print_slots) 
*
*  FUNCTION
*     A CE_Type list is printed to 'fp' in a name=value,name=value,... 
*     fashion. If print_slots is 0 an entry with name "slots" is skipped.
*     The 'name' is printed prior the actual list to 'fp'.
*
*  INPUTS
*     FILE *fp          - The file pointer 
*     char *name        - The name printed before the list
*     lList *thresholds - The CE_Type list.
*     int print_slots   - Flag indicating whether "slots" is skipped or not.
*
*  RESULT
*     int - 0 on success 
*           -1 on fprintf() errors
*
*  NOTES
*     MT-NOTE: fprint_thresholds() is MT safe
*******************************************************************************/
int fprint_thresholds(
FILE *fp,
char *name,
lList *thresholds,
int print_slots 
) {
   return fprint_name_value_list(fp, name, thresholds, print_slots, CE_name, CE_stringval, CE_doubleval);
}

/****** cull_parse_util/fprint_resource_utilizations() *************************
*  NAME
*     fprint_resource_utilizations() -- Print a name=value list of type RUE_Type
*
*  SYNOPSIS
*     int fprint_resource_utilizations(FILE *fp, char *name, lList *thresholds, 
*     int print_slots) 
*
*  FUNCTION
*     A RUE_Type list is printed to 'fp' in a name=value,name=value,... 
*     fashion. If print_slots is 0 an entry with name "slots" is skipped.
*     The 'name' is printed prior the actual list to 'fp'.
*
*  INPUTS
*     FILE *fp          - The file pointer 
*     char *name        - The name printed before the list
*     lList *thresholds - The RUE_Type list.
*     int print_slots   - Flag indicating whether "slots" is skipped or not.
*
*  RESULT
*     int - 0 on success 
*           -1 on fprintf() errors
*
*  NOTES
*     MT-NOTE: fprint_resource_utilizations() is MT safe
*******************************************************************************/
int fprint_resource_utilizations(
FILE *fp,
char *name,
lList *thresholds,
int print_slots 
) {
   return fprint_name_value_list(fp, name, thresholds, print_slots, RUE_name, -1, RUE_utilized_now);
}

/****** cull_parse_util/fprint_name_value_list() *******************************
*  NAME
*     fprint_name_value_list() -- Print name=value list of any type.
*
*  SYNOPSIS
*     static int fprint_name_value_list(FILE *fp, char *name, lList 
*     *thresholds, int print_slots, int nm_name, int nm_strval, int 
*     nm_doubleval) 
*
*  FUNCTION
*     A list with name (String) and value (Double) CULL fields is printed 
*     to 'fp' in a name=value,name=value,... fashion. If print_slots is 0 
*     an entry with name "slots" is skipped. The 'name' is printed prior 
*     the actual list to 'fp'. In 'nm_name'/'nm_strval' the CULL names must
*     be passed. Optionally a string representation of the value is printed 
*     if non-nullptr and if 'nm_strval' is not -1.
*
*  INPUTS
*     FILE *fp          - The file pointer
*     char *name        - The name printed before the list
*     lList *thresholds - The list
*     int print_slots   - Flag indicating whether "slots" is skipped or not.
*     int nm_name       - The CULL nm for the name (String).
*     int nm_strval     - The CULL nm for the value (Double).
*     int nm_doubleval  - If existing in the list the CULL nm for a string 
*                         representation of the value (String) or -1 otherwise.
*
*  RESULT
*     int - 0 on success
*           -1 on fprintf() errors
*
*  NOTES
*     MT-NOTE: fprint_name_value_list() is MT safe
*******************************************************************************/
static int fprint_name_value_list(
FILE *fp,
char *name,
lList *thresholds,
int print_slots,
int nm_name,
int nm_strval,
int nm_doubleval
) {
   const lListElem *lep;
   int printed = 0;
   const char *s;
   char buffer[1024];

   DENTER(TOP_LAYER);

   FPRINTF((fp, "%s", name));

   for_each_ep(lep, thresholds) {
      if (print_slots || strcmp("slots", lGetString(lep, nm_name))) {
         if (printed) {
            FPRINTF((fp, ","));
         }

         if (nm_strval == -1 || !(s=lGetString(lep, nm_strval))) {
            snprintf(buffer, sizeof(buffer), "%f", lGetDouble(lep, nm_doubleval));
            s = buffer;
         }

         FPRINTF((fp, "%s=%s", lGetString(lep, nm_name), s));
         printed++;
      }
   }
   if (printed == 0) {
      FPRINTF((fp, "NONE\n"));
   } else {
      FPRINTF((fp, "\n"));
   }

   DRETURN(0);

FPRINTF_ERROR:
   DRETURN(-1);
}            

void parse_list_hardsoft(lList *cmdline, const char *option, lListElem *job, u_long32 scope,
                         int hard_field, int soft_field) {
   DENTER(TOP_LAYER);
   bool handled_option = false;
   lList *hard_list = nullptr;
   lList *soft_list = nullptr;

   lListElem *jrs = job_get_request_setRW(job, scope);
   if (jrs != nullptr) {
      hard_list = lCopyList(nullptr, lGetList(jrs, hard_field));
      if (soft_field) {
         soft_list = lCopyList(nullptr, lGetList(jrs, soft_field));
      }
   }

   lListElem *ep, *next_ep;
   const void *iterator = nullptr;
   next_ep = lGetElemStrFirstRW(cmdline, SPA_switch_val, option, &iterator);
   while ((ep = next_ep) != nullptr) {
      next_ep = lGetElemStrNextRW(cmdline, SPA_switch_val, option, &iterator);

      if (int(lGetChar(ep, SPA_argval_lCharT)) == int(scope)) {
         lList *lp = nullptr;
         lXchgList(ep, SPA_argval_lListT, &lp);
         if (lp != nullptr) {
            if (!soft_field || lGetInt(ep, SPA_argval_lIntT) < 2) {
               if (hard_list == nullptr) {
                  hard_list = lp;
               } else {
                  lAddList(hard_list, &lp);
               }
            } else {
               if (soft_list == nullptr) {
                  soft_list = lp;
               } else {
                  lAddList(soft_list, &lp);
               }
            }
         }
         lRemoveElem(cmdline, &ep);
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
      lListElem *global = lGetSubUlongRW(job, JRS_scope, JRS_SCOPE_GLOBAL, JB_request_set_list);
      if (global != nullptr) {
         lList *global_queue_requests = nullptr;
         lXchgList(global, JRS_hard_queue_list, &global_queue_requests);
         job_set_hard_queue_list(job, global_queue_requests, JRS_SCOPE_SLAVE);
         // also swap the soft requests, should there be any
         // this will then trigger an error as we do not support soft slave queue requests
         global_queue_requests = nullptr;
         lXchgList(global, JRS_soft_queue_list, &global_queue_requests);
         job_set_soft_queue_list(job, global_queue_requests, JRS_SCOPE_SLAVE);
      }
   }

   DRETURN_VOID;
}

int 
parse_list_simple(lList *cmdline, const char *option, lListElem *job, int field,
                  int nm_var, int nm_value, u_long32 flags) 
{
   lList *destlist = nullptr;
   lList *lp = nullptr;
   lListElem *ep;

   DENTER(TOP_LAYER);

   destlist = lCopyList("job_sublist", lGetList(job, field));

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, option))) {
      DPRINTF("OPTION: %s\n", option);
      lp = nullptr;
      lXchgList(ep, SPA_argval_lListT, &lp);

      parse_list_simpler(lp, &destlist, option, job, field, nm_var, nm_value, flags);

      lRemoveElem(cmdline, &ep);
   } 

   lSetList(job, field, destlist);

   DRETURN(0);
}

int 
parse_list_simpler(lList *lp, lList **destlist, const char *option, lListElem *job, int field,
                  int nm_var, int nm_value, u_long32 flags) 
{
   if (lp != nullptr) {
      if (flags & FLG_LIST_APPEND || flags & FLG_LIST_MERGE_DOUBLE_KEY) {
         if (lp) {  
            if (!*destlist) {
               *destlist = lp;
            } else {
               lAddList(*destlist, &lp);
               
               if (flags & FLG_LIST_MERGE_DOUBLE_KEY) {
                  cull_compress_definition_list(*destlist, nm_var, nm_value, 1);
               }
            }
         }
      } else if (flags & FLG_LIST_MERGE) {
         if (lp != nullptr) {
            if (!*destlist) {
               *destlist = lp; 
            } else {
               cull_merge_definition_list(destlist, lp, nm_var, nm_value);
               lFreeList(&lp);
            }
         }
      } else {
         if (*destlist) {
            lFreeList(destlist);
         }
         *destlist = lp;
      } 
   }
   return 0;
}

/****** cull_parse_util/cull_parse_path_list() **************************************
*  NAME
*     cull_parse_path_list() -- parse a path list 
*
*  SYNOPSIS
*     int cull_parse_path_list(lList **lpp, char *path_str) 
*
*  FUNCTION
*     Parse a path list of the format: [[host]:]path[,[[host]:]path...]
*
*  INPUTS
*     lList **lpp    - parsed list PN_Type 
*     char *path_str - input string 
*
*  RESULT
*     int - error code 
*        0 = okay
*        1 = error 
*
*  NOTES
*     MT-NOTE: cull_parse_path_list() is MT safe
*******************************************************************************/
int cull_parse_path_list(lList **lpp, const char *path_str) 
{
   char *path = nullptr;
   char *cell = nullptr;
   char **str_str = nullptr;
   char **pstr = nullptr;
   lListElem *ep = nullptr;
   char *path_string = nullptr;
   bool ret_error = false;

   DENTER(TOP_LAYER);

   ret_error = (lpp == nullptr) ? true : false;

   if(!ret_error){
      path_string = sge_strdup(nullptr, path_str);
      ret_error = (path_string == nullptr) ? true : false;
   }
   if(!ret_error){
      str_str = string_list(path_string, ",", nullptr);
      ret_error = (str_str == nullptr || *str_str == nullptr) ? true : false;
   }
   if ( (!ret_error) && (!*lpp)) {
      *lpp = lCreateList("path_list", PN_Type);
      ret_error = (*lpp == nullptr) ? true : false;
   }

   if(!ret_error){
      for (pstr = str_str; *pstr; pstr++) {
      /* cell given ? */
         if (*pstr[0] == ':') {  /* :path */
            cell = nullptr;
            path = *pstr+1;
         } else if ((path = strstr(*pstr, ":"))){ /* host:path */
            path[0] = '\0';
            cell = strdup(*pstr);
            path[0] = ':';
            path += 1;
         } else { /* path */
            cell = nullptr;
            path = *pstr;
         }

         ep = lCreateElem(PN_Type);
         lAppendElem(*lpp, ep);

         lSetString(ep, PN_path, path);
        if (cell) {
            lSetHost(ep, PN_host, cell);
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
   DRETURN(ret_error? 1 : 0);
}

/****** cull_parse_util/cull_parse_jid_hold_list() *****************************
*  NAME
*     cull_parse_jid_hold_list() -- parse a jid list 
*
*  SYNOPSIS
*     int cull_parse_jid_hold_list(lList **lpp, const char *str) 
*
*  FUNCTION
*     parse a jid list of the fomat jid[,jid,...]
*
*  INPUTS
*     lList **lpp - ST_Type result list 
*     const char *str   - input string to be parsed 
*
*  RESULT
*     int - 
*
*  NOTES
*     MT-NOTE: cull_parse_jid_hold_list() is MT safe 
*******************************************************************************/
int 
cull_parse_jid_hold_list(lList **lpp, const char *str) 
{
   int rule[] = {ST_name, 0};
   char **str_str = nullptr;
   int i_ret;
   char *s;

   DENTER(TOP_LAYER);

   if (!lpp) {
      DRETURN(1);
   }

   s = sge_strdup(nullptr, str);
   if (!s) {
      *lpp = nullptr;
      DRETURN(3);
   }
   str_str = string_list(s, ",", nullptr);
   if (!str_str || !*str_str) {
      *lpp = nullptr;
      sge_free(&s);
      DRETURN(2);
   }
   i_ret = cull_parse_string_list(str_str, "jid_hold list", ST_Type, rule, lpp);
   
   if (i_ret) {
      sge_free(&s);
      sge_free(&str_str);
      DRETURN(3);
   }

   sge_free(&s);
   sge_free(&str_str);
   DRETURN(0);
}

/****** cull_parse_util/sge_parse_hold_list() **********************************
*  NAME
*     sge_parse_hold_list() -- parse -h switch of qsub and qalter 
*
*  SYNOPSIS
*     int sge_parse_hold_list(char *hold_str, u_long32 prog_number) 
*
*  FUNCTION
*     Parse the hold flags of -h switches which can be used with 
*     qaub and qalter 
*
*  INPUTS
*     char *hold_str       - string tobe parsed
*     u_long32 prog_number - program number 
*
*  RESULT
*     int - hold state
*        -1 in case of error
*
*  NOTES
*     MT-NOTE: sge_parse_hold_list() is MT safe 
*******************************************************************************/
int 
sge_parse_hold_list(const char *hold_str, u_long32 prog_number) {
   int i, j;
   int target = 0;
   int op_code = 0;

   DENTER(TOP_LAYER);

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

