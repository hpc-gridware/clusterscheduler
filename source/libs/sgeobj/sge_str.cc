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

#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/msg_sgeobjlib.h"

#define STR_LAYER BASIS_LAYER

/****** sgeobj/str/str_list_append_to_dstring() *******************************
*  NAME
*     str_list_append_to_dstring() -- append strings to dstring 
*
*  SYNOPSIS
*     const char * 
*     str_list_append_to_dstring(const lList *this_list, 
*                                dstring *string, 
*                                const char delimiter) 
*
*  FUNCTION
*     Append the strings contained in "this_list" to the dstring 
*     "string". Separate them by the character contained in 
*     "delimiter". 
*     If "this_list" is nullptr or conaines no elements, "NONE" will
*     be added to the dstring.
*
*  INPUTS
*     const lList *this_list - ST_Type list 
*     dstring *string        - dynamic string 
*     const char delimiter   - delimiter  
*
*  RESULT
*     const char * - pointer to the given "string"-buffer 
*
*  SEE ALSO
*     sgeobj/str/str_list_parse_from_string()
*******************************************************************************/
const char *
str_list_append_to_dstring(const lList *this_list, dstring *string,
                           const char delimiter)
{
   const char *ret = nullptr;

   DENTER(STR_LAYER);
   if (string != nullptr) {
      const lListElem *elem = nullptr;
      bool printed = false;

      for_each_ep(elem, this_list) {
         sge_dstring_append(string, lGetString(elem, ST_name));
         if (lNext(elem) != nullptr) {
            sge_dstring_sprintf_append(string, "%c", delimiter);
         }
         printed = true;
      }
      if (!printed) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }
   DRETURN(ret);
}

/****** sgeobj/str/str_list_parse_from_string() *******************************
*  NAME
*     str_list_parse_from_string() -- Parse a list of strings 
*
*  SYNOPSIS
*     bool 
*     str_list_parse_from_string(lList **this_list, 
*                                const char *string, 
*                                const char *delimitor) 
*
*  FUNCTION
*     Parse a list of strings from "string". The strings have to be 
*     separated by a token contained in "delimitor". for each string
*     an element of type ST_Type will be added to "this_list". 
*     
*  INPUTS
*     lList **this_list     - ST_Type list
*     const char *string    - string to be parsed 
*     const char *delimitor - delimitor string 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error 
*
*  SEE ALSO
*     sgeobj/str/str_list_append_to_dstring()
* 
*  NOTES
*     MT-NOTE: str_list_parse_from_string() is MT safe
*******************************************************************************/
bool 
str_list_parse_from_string(lList **this_list,
                           const char *string, const char *delimitor)
{
   bool ret = true;

   DENTER(STR_LAYER);
   if (this_list != nullptr && string != nullptr && delimitor != nullptr) {
      struct saved_vars_s *context = nullptr;
      const char *token;

      token = sge_strtok_r(string, delimitor, &context);
      while (token != nullptr) {
         lAddElemStr(this_list, ST_name, token, ST_Type);
         token = sge_strtok_r(nullptr, delimitor, &context);
      }
      sge_free_saved_vars(context);
   }
   DRETURN(ret);
}

/****** sgeobj/str/str_list_is_valid() ****************************************
*  NAME
*     str_list_is_valid() -- Are all strings valid 
*
*  SYNOPSIS
*     bool 
*     str_list_is_valid(const lList *this_list, lList **answer_list) 
*
*  FUNCTION
*     Does each element in "this_list" contain a valid string (!= nullptr).
*
*  INPUTS
*     const lList *this_list - ST_Type list 
*     lList **answer_list    - AN_Type list 
*
*  RESULT
*     bool - result
*        true  - all strings are != nullptr
*        false - at least one entry is nullptr
*******************************************************************************/
bool
str_list_is_valid(const lList *this_list, lList **answer_list)
{
   bool ret = true;

   DENTER(STR_LAYER);
   if (this_list != nullptr) {
      const lListElem *elem;

      for_each_ep(elem, this_list) {
         if (lGetString(elem, ST_name) == nullptr) {
            answer_list_add(answer_list, MSG_STR_INVALIDSTR, 
                            STATUS_ENOKEY, ANSWER_QUALITY_ERROR);
            break;
         }
      }
   }
   DRETURN(ret);
}

bool
str_list_transform_user_list(lList **this_list, lList **answer_list, const char *username)
{
   bool ret = true;

   DENTER(STR_LAYER);
   if (this_list != nullptr && *this_list != nullptr) {
      lListElem *elem;

      for_each_rw (elem, *this_list) {
         const char *string = lGetString(elem, ST_name);

         if (string != nullptr) {
            /*
             * '$user' will be replaced by the current unix username
             * '*'     empty the remove the list 
             */
            if (strcasecmp(string, "$user") == 0) {
               lSetString(elem, ST_name, username);
            } else if (strcmp(string, "*") == 0) {
               lFreeList(this_list);
               break;
            }
         }
      }
   } else {
      lAddElemStr(this_list, ST_name, username, ST_Type);
   }
   DRETURN(ret);
}

lList *
grp_list_array2list(int amount, ocs_grp_elem_t *grp_array) {
   DENTER(TOP_LAYER);
   lList *grp_list = nullptr;

   for(int i = 0; i < amount; i++) {
      lListElem *new_grp = lAddElemStr(&grp_list, ST_name, grp_array[i].name, ST_Type);

      lSetUlong(new_grp, ST_id, grp_array[i].id);
   }
   DRETURN(grp_list);
}

