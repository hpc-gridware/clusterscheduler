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
 * @brief Reading a flat `key value` configuration file into a name/value list
 *
 * The file is parsed into a list of name/value pairs (CF); a caller looks
 * up single keys with #get_conf_value and takes the entries it consumed out of
 * the list, so whatever is left over afterwards is exactly the set of keys
 * nobody understood. #add_nm_to_set records which attributes a configuration
 * mentioned.
 *
 * @see config.h
 */

#include <cstdio>
#include <cstring>
#include <cctype>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull.h"

#include "sgeobj/config.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/**
 * @brief Read a configuration file into a name/value list
 *
 * Each line becomes one element: the text before `delimitor` goes into
 * `CF_name`, the text after it into `CF_value`. Without #RCL_NO_VALUE a line
 * carrying only a name is an error. With `sublists`, a value `{` starts a
 * sub-list that is read recursively into `CF_sublist` and ends with a line `}`.
 *
 * @param fp the file to read
 * @param[out] alpp receives the error message; a nullptr answer list signals success
 * @param sublists true to read `{` ... `}` blocks into `CF_sublist`
 * @param delimitor separates name and value; may be nullptr, it is passed on to `sge_strtok`
 * @param flag `RCL_NO_VALUE` to accept a line with a name but no value
 * @param[out] buffer scratch buffer the parser reads lines into
 * @param buffer_size the size of `buffer`
 * @return 0 on success or -1 on an error, and the entries read so far, also on an error; an empty
 *         handle when no entry was read
 */
std::pair<int, ocs::tom::ListPtr<ocs::tom::CF>>
read_config_list(FILE *fp, ocs::tom::ListPtr<ocs::tom::AN> *alpp, const bool sublists, const char *delimitor,
                 const int flag, char *buffer, const int buffer_size) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::CF> config;
   struct saved_vars_s *last = nullptr;
   const bool force_value = (flag & RCL_NO_VALUE) == 0;
   int ret = 0;

   while (fgets(buffer, buffer_size, fp)) {
      if (last) {
         sge_free_saved_vars(last);
         last = nullptr;
      }
      /*
      ** skip empty and comment lines
      */
      if (buffer[0] == '#')
         continue;
      char *s;
      for(s = buffer; *s == ' ' || *s == '\t'; s++)
         ;
      if (*s == '\n')
         continue;

      /*
      ** get name and value
      */
      char *name;
      if (*s != '"' || !(name = sge_strtok_r(s+1, "\"", &last))) {
         if (!(name = sge_strtok_r(buffer, delimitor, &last)))
            break;
      }
      char *value = sge_strtok_r(nullptr, "\n", &last);

      /* handle end of sub-list */
      if (sublists && strcmp(name, "}") == 0 && !value) {
         break;
      }

      if (!value && force_value)
         break;

      /* skip leading delimitors */
      if (value) {
         while (*value && (delimitor? 
            (nullptr != strchr(delimitor, *value)):
            isspace((int) *value)))
            value++;
      }

      if(!value || !(*value)) {
         if (force_value) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CONFIGNOARGUMENTGIVEN_S , name);
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = -1;
            break;
         }
         value = nullptr;
      } else {
         /* skip trailing delimitors */
         char *tmp = &value[strlen(value)-1];

         while (tmp>value && (delimitor?
            (nullptr != strchr(delimitor, *tmp)):
            isspace((int) *tmp))) {
            *tmp = '\0';
            tmp--;
         }
      }

      /* handle sub-list */
      if (sublists && value && strcmp(value, "{") == 0) {
         auto [sub_ret, sub_config] = read_config_list(fp, alpp, sublists, delimitor, flag, buffer, buffer_size);
         if (sub_ret < 0) {
            ret = -1;
            break;
         }
         if (!config) {
            config = ocs::tom::ListPtr<ocs::tom::CF>::create("");
         }
         auto ep = config->emplace_back<CF_name>(name);
         // CF_sublist has no sub-type in the model, so the field takes the raw list
         ocs::tom::set<CF_sublist>(ep, sub_config.release());
      } else {
         if (!config) {
            config = ocs::tom::ListPtr<ocs::tom::CF>::create("");
         }
         auto ep = config->emplace_back<CF_name>(name);
         ocs::tom::set<CF_value>(ep, value);
      }
   }

   if (last) {
      sge_free_saved_vars(last);
   }
   DRETURN(std::make_pair(ret, std::move(config)));
}

/**
 * @brief The string value of one configuration entry
 *
 * @param[out] alpp receives the message naming the missing key; may be nullptr
 * @param config the parsed configuration
 * @param key the entry to look for
 * @return the value, or nullptr when the key is absent
 */
const char *get_conf_value(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::ConstList<ocs::tom::CF> config,
                           const char *key) {
   DENTER(CULL_LAYER);

   const auto ep = config.find_first<CF_name>(key);
   if (ep == nullptr) {
      if (alpp) {
         char error[1000];
         snprintf(error, sizeof(error), MSG_GDI_CONFIGMISSINGARGUMENT_S, key);
         answer_list_add(alpp, error, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      }
      DRETURN(nullptr);
   }

   const char *value = ocs::tom::get<CF_value>(ep);
   DPRINTF("%s = %s\n", key, value?value:"<null ptr>");

   DRETURN(value);
}

/**
 * @brief Record that an attribute was set, refusing a duplicate
 *
 * The `fields` array is how a caller learns which attributes a configuration
 * actually mentioned, so that the rest can be left at their previous values.
 *
 * @param[in,out] fields the `NoName` terminated array to append to; nullptr is
 *                       accepted and does nothing
 * @param name_nm the attribute to record
 * @return 0 on success, -1 when the attribute was already recorded
 */
int add_nm_to_set(
int fields[],
int name_nm 
) {
   DENTER(CULL_LAYER);

   int i = 0;

   if (!fields) {
      DRETURN(0); /* we leave here in most cases */
   }

   /* seek end and check whether 'name_nm' 
      is already in 'fields' */
   while (fields[i]!=NoName && fields[i]!=name_nm)
      i++;
   
   if (fields[i]==name_nm) {
      DRETURN(-1);
   }

   fields[i] = name_nm;      
   fields[++i] = NoName;      

   DRETURN(0);
}

