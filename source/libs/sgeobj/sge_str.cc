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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/                                   

/** @file
 * @brief String lists, and the group list conversions around them
 *
 * @see sge_str.h
 */

#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"

/// Debug layer the string list traces are written to
#define STR_LAYER BASIS_LAYER

/**
 * @brief Append strings to dstring
 *
 * Append the strings contained in "this_list" to the dstring
 * "string". Separate them by the character contained in
 * "delimiter".
 * If "this_list" is nullptr or contains no elements, "NONE" will
 * be added to the dstring.
 *
 * @param this_list ST list, may be empty
 * @param string dynamic string
 * @param delimiter delimiter
 *
 * @return pointer to the given "string"-buffer
 *
 * @see #str_list_parse_from_string
 */
const char *
str_list_append_to_dstring(const ocs::tom::ConstList<ocs::tom::ST> this_list, dstring *string, const char delimiter) {
   DENTER(STR_LAYER);
   const char *ret = nullptr;

   if (string != nullptr) {
      bool printed = false;

      for (const auto elem : this_list) {
         sge_dstring_append(string, ocs::tom::get<ST_name>(elem));
         if (elem.next() != nullptr) {
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


/**
 * @brief Parse a list of strings
 *
 * Parse a list of strings from "string". The strings have to be
 * separated by a token contained in "delimitor". For each string
 * an element of type ST is added to the list.
 *
 * @param string string to be parsed
 * @param delimitor delimitor string
 *
 * @return the new list; an empty handle if "string" or "delimitor" is nullptr or "string" holds no token
 *
 * @note MT-NOTE: str_list_parse_from_string() is MT safe
 *
 * @see #str_list_append_to_dstring
 */
ocs::tom::ListPtr<ocs::tom::ST> str_list_parse_from_string(const char *string, const char *delimitor) {
   DENTER(STR_LAYER);

   ocs::tom::ListPtr<ocs::tom::ST> ret;

   if (string != nullptr && delimitor != nullptr) {
      struct saved_vars_s *context = nullptr;

      const char *token = sge_strtok_r(string, delimitor, &context);
      while (token != nullptr) {
         if (!ret) {
            ret = ocs::tom::ListPtr<ocs::tom::ST>::create("");
         }
         ret->emplace_back<ST_name>(token);
         token = sge_strtok_r(nullptr, delimitor, &context);
      }
      sge_free_saved_vars(context);
   }
   DRETURN(ret);
}

/**
 * @brief Are all strings valid
 *
 * Does each element in "this_list" contain a valid string (!= nullptr).
 *
 * @param this_list ST list, may be empty
 * @param answer_list AN list
 *
 * @return always true; an invalid string only adds an error to "answer_list"
 */
bool str_list_is_valid(const ocs::tom::ConstList<ocs::tom::ST> this_list,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(STR_LAYER);
   bool ret = true;

   for (const auto elem : this_list) {
      if (ocs::tom::get<ST_name>(elem) == nullptr) {
         answer_list_add(answer_list, MSG_STR_INVALIDSTR, STATUS_ENOKEY, ANSWER_QUALITY_ERROR);
         break;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Replace the placeholder for "the current user" in a string list
 *
 * A user may write `$user` where a user name is expected; this substitutes the
 * name the request came from. A `*` frees the whole list. An empty handle
 * becomes a list holding only "username".
 *
 * @param[in,out] this_list the list to rewrite
 * @param[out] answer_list receives error messages
 * @param username the name to substitute
 * @return true when the list could be rewritten
 */
bool str_list_transform_user_list(ocs::tom::ListPtr<ocs::tom::ST> &this_list,
                                  [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *username) {
   DENTER(STR_LAYER);
   bool ret = true;

   if (this_list) {
      for (const auto elem : *this_list) {
         const char *string = ocs::tom::get<ST_name>(elem);

         if (string != nullptr) {
            /*
             * '$user' will be replaced by the current unix username
             * '*'     empties the list
             */
            if (strcasecmp(string, "$user") == 0) {
               ocs::tom::set<ST_name>(elem, username);
            } else if (strcmp(string, "*") == 0) {
               this_list.reset();
               break;
            }
         }
      }
   } else if (username != nullptr) {
      this_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
      this_list->emplace_back<ST_name>(username);
   }
   DRETURN(ret);
}

/**
 * @brief Turn a supplementary group array into a CULL list
 *
 * @param amount how many entries `grp_array` has
 * @param grp_array the groups to convert
 * @return the new list, an empty handle if "amount" is 0
 */
ocs::tom::ListPtr<ocs::tom::ST>
grp_list_array2list(const int amount, const ocs_grp_elem_t *grp_array) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::ST> grp_list;

   for (int i = 0; i < amount; i++) {
      if (!grp_list) {
         grp_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
      }
      auto new_grp = grp_list->emplace_back<ST_name>(grp_array[i].name);

      ocs::tom::set<ST_id>(new_grp, grp_array[i].id);
   }
   DRETURN(grp_list);
}
