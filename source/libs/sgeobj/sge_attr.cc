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
 * @brief Per host attributes: one value per host or host group
 *
 * A cluster queue stores most of its attributes as a list of host specific
 * values. Every value type needs the same set of operations; they are written
 * once as members of #ocs_attr_list_ops, whose fields come from
 * #ocs_attr_traits, and instantiated at the end of this file for every
 * attribute type.
 *
 * @see sge_attr.h
 */

#include <cstring>

#include "uti/ocs_Pattern.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_FreeAccess.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HR.h"

#include "msg_common.h"

/// Debug layer the per host attribute traces are written to
#define HOSTATTR_LAYER BASIS_LAYER

/**
 * @brief Add a new entry to a per host attribute list
 *
 * An entry for a reference the list holds already is an error, unless @p flags has #HOSTATTR_OVERWRITE: then
 * the held entry takes the new value. Any other entry is appended. A list that is empty is created, and freed
 * again if the entry could not be added.
 *
 * The old form could also reject a host group entry that makes a host's value ambiguous; its only caller never
 * asked for that (it passed no list for the ambiguous hosts), so that part is gone and #HOSTATTR_ALLOW_AMBIGUITY
 * makes no difference here.
 *
 * @tparam A the attribute type
 * @param this_list the attribute list
 * @param answer_list AN_Type list
 * @param attr the new entry; consumed on every path
 * @param flags see above
 * @return true on success
 *
 * @note MT-NOTE: ocs_attr_list_add() is MT safe
 */
template <OcsHostAttr A>
static bool
ocs_attr_list_add(ocs::tom::ListPtr<A> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                  ocs::tom::ElementPtr<A> attr, const int flags) {
   DENTER(HOSTATTR_LAYER);

   constexpr auto href_f = ocs_attr_traits<A>::href;
   bool ret = false;

   if (attr) {
      const ocs::tom::Host href = ocs::tom::get<href_f>(attr);
      bool created_list = false;
      ocs::tom::Element<A> attr_elem;

      if (!this_list) {
         this_list = ocs::tom::ListPtr<A>::create("");
         created_list = true;
      } else {
         attr_elem = this_list->template find_first<href_f>(href);
      }

      if (attr_elem) {
         if (flags & HOSTATTR_OVERWRITE) {
            // a list value is copied, as the old untyped setter copied it
            static_cast<void>(ocs::tom::copy_if_present<ocs_attr_traits<A>::value>(attr_elem, *attr));
            ret = true;
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTR_VALUEMULDEFINED_S, static_cast<const char *>(href));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         this_list->push_back(std::move(attr));
         ret = true;
      }

      if (created_list && !ret) {
         this_list.reset();
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find the value a per host attribute list gives a host
 *
 * See ocs_attr_list_find(), which calls it.
 *
 * @tparam A the attribute type
 * @param this_list the attribute list, may be an empty handle
 * @param[out] answer_list receives an error if there is no value at all
 * @param hostname the host, may be nullptr
 * @param[out] matching_host_or_group receives the host group the value comes from, if any
 * @param[out] matching_group receives the second host group if the setting is ambiguous
 * @param[out] is_ambiguous set to true if two host groups give the host a value; written only when the host
 *             has no value of its own
 * @param master_hgroup_list the host groups
 * @return the entry whose value was read last and whether a value was found
 *
 * @note MT-NOTE: ocs_attr_list_ops::find() is MT safe
 */
template <OcsHostAttr A>
ocs_attr_lookup<A>
ocs_attr_list_ops<A>::find(const ocs::tom::ConstList<A> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const char *hostname, const char **matching_host_or_group, const char **matching_group,
                           bool *is_ambiguous, const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(HOSTATTR_LAYER);

   constexpr auto href_f = ocs_attr_traits<A>::href;
   ocs_attr_lookup<A> ret;

   if (this_list && hostname != nullptr) {
      /*
       * Try to find a value for the concerned host
       */
      if (const auto own = this_list.template find_first<href_f>(ocs::tom::Host(hostname)); own) {
         ret = {own, true};
         DPRINTF("Found value for host " SFQ "\n", hostname);
      } else {
         bool already_found = false;

         *is_ambiguous = false;
         /*
          * Try to find a value for all hgroup definitions
          * if there was no host related value
          *
          * Exit the loop as soon as possible! This will be the case if
          * an ambiguous value for the concerned host will be found.
          */
         for (const auto entry : this_list) {
            const char *href_name = ocs::tom::get<href_f>(entry);

            if (strcmp(href_name, HOSTREF_DEFAULT) && ocs::is_hgroup_name(href_name)) {
               ocs::tom::ListPtr<ocs::tom::HR> tmp_href_list;
               ocs::tom::ListPtr<ocs::tom::HR> host_list;

               href_list_add(tmp_href_list, nullptr, href_name);
               href_list_find_all_references(tmp_href_list, nullptr, master_hgroup_list, &host_list, nullptr);
               if (href_list_locate(host_list, hostname) != nullptr) {
                  if (!already_found) {
                     already_found = true;
                     ret = {entry, true};
                     *matching_host_or_group = href_name;
                     DPRINTF("Found value for domain " SFQ "\n", href_name);
                  } else {
                     *is_ambiguous = true;
                     *matching_group = href_name;
                     DPRINTF("Found ambiguous value in domain " SFQ "\n", href_name);
                     ret.found = false;
                     break;
                  }
               }
            }
         }
         if (!ret.found) {
            /*
             * Use the default value
             */
            if (const auto def = this_list.template find_first<href_f>(ocs::tom::Host(HOSTREF_DEFAULT)); def) {
               DPRINTF("Using default value\n");
               ret = {def, true};
            } else {
               /*
                * Should never happen.
                */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTR_NOCONFVALUE);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
            }
         }
      }
      if (ret.found) {
         DTRACE;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find the value a per host attribute list gives a host, without looking at host groups
 *
 * See ocs_attr_list_find_href(), which calls it.
 *
 * @tparam A the attribute type
 * @param this_list the attribute list, may be an empty handle
 * @param[out] answer_list receives an error if there is neither a value for the host nor a default value
 * @param hostname the host, may be nullptr
 * @param[out] found true if the host has a value of its own, false if the default value applies
 * @return the entry whose value applies, empty if none
 *
 * @note MT-NOTE: ocs_attr_list_ops::find_href() is MT safe
 */
template <OcsHostAttr A>
ocs::tom::ConstElement<A>
ocs_attr_list_ops<A>::find_href(const ocs::tom::ConstList<A> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const char *hostname, bool *found) {
   DENTER(HOSTATTR_LAYER);

   constexpr auto href_f = ocs_attr_traits<A>::href;
   ocs::tom::ConstElement<A> ret;

   if (this_list && hostname != nullptr) {
      /*
       * Try to find a value for the concerned host
       */
      ret = this_list.template find_first<href_f>(ocs::tom::Host(hostname));
      if (ret) {
         *found = true;
         DTRACE;
      } else {
         /*
          * Use the default value
          */
         ret = this_list.template find_first<href_f>(ocs::tom::Host(HOSTREF_DEFAULT));
         if (ret) {
            *found = false;
            DTRACE;
         } else {
            /*
             * Should never happen.
             */
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTR_NOCONFVALUE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Render a per host attribute list as an administrator writes it
 *
 * See ocs_attr_list_append_to_dstring(), which calls it. The value is rendered by the typed
 * ocs_object_append_field() of the attribute's value field.
 *
 * @tparam A the attribute type
 * @param this_list the attribute list to render, may be an empty handle
 * @param[out] string receives the text, appended
 * @return true
 *
 * @note MT-NOTE: ocs_attr_list_ops::append_to_dstring() is MT safe
 */
template <OcsHostAttr A>
bool
ocs_attr_list_ops<A>::append_to_dstring(const ocs::tom::ConstList<A> this_list, dstring *string) {
   DENTER(HOSTATTR_LAYER);

   constexpr auto href_f = ocs_attr_traits<A>::href;
   constexpr auto value_f = ocs_attr_traits<A>::value;
   bool found_default = false;
   bool found_group = false;
   bool found_host = false;
   dstring host_string = DSTRING_INIT;

   if (const auto def = this_list.template find_first<href_f>(ocs::tom::Host(HOSTREF_DEFAULT)); def) {
      found_default = true;
      ocs_object_append_field<value_f>(def, string);
   }

   for (const auto attr : this_list) {
      const char *href = ocs::tom::get<href_f>(attr);

      if (href == nullptr || (found_default && !strcmp(href, HOSTREF_DEFAULT))) {
         continue;
      } else {
         dstring *ds; /* will be reference to the corresponding dstring container */

         if (ocs::is_hgroup_name(href)) {
            ds = string;
            if (found_group || found_default) {
               sge_dstring_append_char(ds, ',');
            }
            found_group = true;
         } else {
            ds = &host_string;
            if (found_host) {
               sge_dstring_append_char(ds, ',');
            }
            found_host = true;
         }

         sge_dstring_append_char(ds, '[');
         sge_dstring_append(ds, href);
         sge_dstring_append_char(ds, '=');
         ocs_object_append_field<value_f>(attr, ds);
         sge_dstring_append_char(ds, ']');
      }
   }
   if (found_host) {
      if (found_default || found_group) {
         sge_dstring_append_char(string, ',');
      }
      sge_dstring_append_dstring(string, &host_string);
   }
   if (!found_default && !found_group && !found_host) {
      sge_dstring_append(string, "NONE");
   }
   sge_dstring_free(&host_string);
   DRETURN(true);
}

/**
 * @brief Parse a per host attribute list as an administrator writes it
 *
 * See ocs_attr_list_parse_from_string(), which calls it. Each value is parsed by the typed
 * ocs_object_parse_field() of the attribute's value field.
 *
 * @tparam A the attribute type
 * @param this_list the list the entries are added to; created if it is empty
 * @param[out] answer_list receives the errors
 * @param string the text, may be nullptr
 * @param flags see ocs_attr_list_add()
 * @return true on success
 *
 * @note MT-NOTE: ocs_attr_list_ops::parse_from_string() is MT safe
 */
template <OcsHostAttr A>
bool
ocs_attr_list_ops<A>::parse_from_string(ocs::tom::ListPtr<A> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                        const char *string, const int flags) {
   DENTER(TOP_LAYER);

   bool ret = true;
   if (string != nullptr) {
      struct saved_vars_s *strtok_context = nullptr;
      char *token = nullptr;
      char *next_token = nullptr;
      bool is_first_token = true;
      bool is_last_token = false;

      /* 
       * start         := value {',' group_value} .
       * group_value   := '[' group_or_host '=' value ']' . 
       * group_or_host := ['@'] name .
       * value         := <depends on listtype>
       * 
       * example: lic=5,fast=1,[@group=lic=4,fast=0],[b=lic=0] 
       *
       * lic=5,fast=1, 
       * @group=lic=4,fast=0], 
       * b=lic=0]
       * 
       * lic=5,fast=1
       * @group=lic=4,fast=0
       * b=lic=0
       *
       * default  lic=5,fast=1
       * @group   lic=4,fast=0
       * b        lic=0
       * 
       */

      next_token = sge_strtok_r(string, "[", &strtok_context);
      while (ret && (token = next_token)) {
         size_t length; 

         next_token = sge_strtok_r(nullptr, "[", &strtok_context);
         if (next_token == nullptr) {
            is_last_token = true;
         }

         /*
          * There might be white space at the end of each token.
          */
         sge_strip_trailing_blanks(token);
         length = strlen(token);

         if (length >= 1) {
            const char *href_name = nullptr;
            char *value = nullptr;
            bool first_is_default = true;
  
            /* 
             * All except the last token has to conatin a ',' as last
             * character in the string. This ',' has to be removed.
             */
            if (ret && !is_last_token) {
               if (token[length - 1] == ',') {
                  token[length - 1] = '\0';
                  length--;
               } else {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTR_MISSINGCOMMA_S, string);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            }
            else if (ret && is_last_token && (token[length - 1] == ',')) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTR_TRAILINGCOMMA_S, string);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
               ret = false;
            }

            /*
             * There might be space after a closing brace ']'
             */
            sge_strip_trailing_blanks(token);
            length = strlen(token);

            /* 
             * All except the first token has to end with a ']'. Also
             * this charcter has to be removed.
             */
            if (ret && !is_first_token) {
               if (token[length - 1] == ']') {
                  token[length - 1] = '\0';
                  length--;
               } else {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTR_MISSINGBRACKET_S, string);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            }

            /*
             * If the first token containes a ']' as last charcter than
             * a default value is missing! This is not a error but
             * we have to parse a group or host additionally
             */ 
            if (ret && is_first_token) {
               if (token[length - 1] == ']') {
                  token[length - 1] = '\0';
                  length--;
                  first_is_default = false;
               } 
            }

            /*
             * All but the first token has to contain a hostgroup
             * or a host reference in the beginning of the string 
             */
            if (ret) {
               if (!is_first_token || !first_is_default) {
                  value = strchr(token, '=');
                  href_name = token;

                  if (value != nullptr) {
                     value[0] = '\0';
                     value++;
                  } else {
                     snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTR_EQUALSIGNEXPRECTED);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
                     ret = false;
                  }
               } else {
                  href_name = HOSTREF_DEFAULT;
                  value = token;
               }
            }

            /*
             * Parsing the token was successful. We can create a new 
             * element.
             */
            if (ret) {
               auto attr_elem = ocs::tom::ElementPtr<A>::create();

               ocs::tom::set<ocs_attr_traits<A>::href>(attr_elem, href_name);
               ret &= ocs_object_parse_field<ocs_attr_traits<A>::value>(*attr_elem, answer_list, value);
               if (ret) {
                  ret &= ocs_attr_list_add(this_list, answer_list, std::move(attr_elem), flags);
               } else {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ATTR_PARSINGERROR_S, value);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
               }
            }
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ATTR_NOVALUEGIVEN);
            answer_list_add(answer_list, SGE_EVENT,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         is_first_token = false;
      } 
      sge_free_saved_vars(strtok_context);
      strtok_context = nullptr;
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, 
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   } 

   DRETURN(ret);
}

// the operations for every attribute type, see #ocs_attr_list_ops
template struct ocs_attr_list_ops<ocs::tom::ASTR>;
template struct ocs_attr_list_ops<ocs::tom::AULNG>;
template struct ocs_attr_list_ops<ocs::tom::ABOOL>;
template struct ocs_attr_list_ops<ocs::tom::ATIME>;
template struct ocs_attr_list_ops<ocs::tom::AMEM>;
template struct ocs_attr_list_ops<ocs::tom::AINTER>;
template struct ocs_attr_list_ops<ocs::tom::AQTLIST>;
template struct ocs_attr_list_ops<ocs::tom::ASTRLIST>;
template struct ocs_attr_list_ops<ocs::tom::AUSRLIST>;
template struct ocs_attr_list_ops<ocs::tom::APRJLIST>;
template struct ocs_attr_list_ops<ocs::tom::ACELIST>;
template struct ocs_attr_list_ops<ocs::tom::ASOLIST>;
