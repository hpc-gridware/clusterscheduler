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
 * @brief Resource quota sets: limits that cut across the object model
 *
 * A rule filters on up to five characteristics - user, project, PE, queue,
 * host - and applies a limit to whatever matches. `RQR_level` decides whether
 * that limit is one shared pool or one per host, queue or queue instance.
 *
 * @see sge_resource_quota.h
 */


#include <cstring>
#include <fnmatch.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_log.h"
#include "uti/sge_hostname.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_resource_quota.h"

#include "ocs_ResourceQuota.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "uti/sge.h"

/**
 * @brief Parse a RQRF Object from string
 *
 * Converts a spooled RQRF Object to a CULL Element
 *
 * @param buffer string to be converted
 * @param alp answer_list
 *
 * @return the filter (RQRF_Type), an empty handle on error or if @p buffer is nullptr
 *
 * @note MT-NOTE: rqs_parse_filter_from_string() is MT safe
 */
ocs::tom::ElementPtr<ocs::tom::RQRF> rqs_parse_filter_from_string(const char *buffer,
                                                                  ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   DENTER(TOP_LAYER);

   const char delims[] = "\t \v\r,{}";

   if (buffer == nullptr) {
      DRETURN(nullptr);
   }

   auto filter = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();

   if (buffer[0] == '{') {
      /* We have a expanded list */
      ocs::tom::set<RQRF_expand>(filter, true);
      if (buffer[strlen(buffer) - 1] != '}') {
         ERROR(SFNMAX, MSG_RESOURCEQUOTA_NOVALIDEXPANDEDLIST);
         answer_list_add(alp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(nullptr);
      }
   } else {
      ocs::tom::set<RQRF_expand>(filter, false);
   }

   const auto names = ocs::tom::ListPtr<ocs::tom::ST>::from_string<ST_name>(buffer, delims);
   ocs::tom::ListPtr<ocs::tom::ST> scope_list;
   ocs::tom::ListPtr<ocs::tom::ST> xscope_list;

   for (const auto scope : *names) {
      const char *name = ocs::tom::get<ST_name>(scope);
      if (name[0] == '!') {
         /* The '!' prefixes a name, so a name has to follow it. Without this
          * check "!{a,b}" parses into an exclusion of the empty name plus the
          * inclusions a and b: the braces are eaten as delimiters and the rule
          * ends up limiting the very two names it was written to exempt, which
          * is the opposite of what it says. Up to 9.1 such a write was refused
          * by accident rather than by rule - the client parsed, packed the
          * empty name, unpackstr() turned it into a null pointer in transit and
          * rqs_verify_filter() rejected the null pointer in qmaster. Parsing in
          * qmaster leaves no pack round trip to do that, so the syntax is
          * refused here, where the text that caused it is still at hand. */
         if (name[1] == '\0') {
            answer_list_add_sprintf(alp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_RESOURCEQUOTA_EXCLUDE_NONAME_S, buffer);
            DRETURN(nullptr);
         }
         if (!xscope_list) {
            xscope_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
         }
         xscope_list->emplace_back<ST_name>(name + 1);
      } else {
         if (!scope_list) {
            scope_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
         }
         scope_list->emplace_back<ST_name>(name);
      }
   }

   ocs::tom::set<RQRF_scope>(filter, std::move(scope_list));
   ocs::tom::set<RQRF_xscope>(filter, std::move(xscope_list));

   DRETURN(filter);
}


/**
 * @brief RQRF Element to string
 *
 * Converts a RQRF element to string for spooling
 *
 * @param filter Element to be converted (RQRF_Type), may be an empty handle
 * @param buffer buffer for the element string
 * @param alp answer_list
 *
 * @return true if at least one name was written, false otherwise
 *
 * @note MT-NOTE: rqs_append_filter_to_dstring() is MT safe
 */
bool rqs_append_filter_to_dstring(const ocs::tom::ConstElement<ocs::tom::RQRF> filter, dstring *buffer,
                                  ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   DENTER(TOP_LAYER);

   bool first = true;
   bool ret = false;

   if (!filter) {
      DRETURN(ret);
   }

   const bool expand = ocs::tom::get<RQRF_expand>(filter);

   if (expand) {
      sge_dstring_append_char(buffer, '{');
   }

   for (const auto scope : ocs::tom::get<RQRF_scope>(filter)) {
      ret = true;
      if (!first) {
         sge_dstring_append_char(buffer, ',');
      } else {
         first = false;
      }
      sge_dstring_append(buffer, ocs::tom::get<ST_name>(scope));
   }

   for (const auto scope : ocs::tom::get<RQRF_xscope>(filter)) {
      ret = true;
      if (!first) {
         sge_dstring_append_char(buffer, ',');
      } else {
         first = false;
      }
      sge_dstring_append_char(buffer, '!');
      sge_dstring_append(buffer, ocs::tom::get<ST_name>(scope));
   }

   if (expand) {
      sge_dstring_append_char(buffer, '}');
   }

   DRETURN(ret);
}


/**
 * @brief Set default values to given rqs
 *
 * This function sets the default values to a resource quota set.
 * The default rule set is:
 * {
 *   name = `must be already set`
 *   enabled = false
 *   limit to slots=0
 * }
 *
 * A rule list the set holds already is freed and replaced.
 *
 * @param rqs Already created object to be modified (RQS_Type), may be an empty handle
 *
 * @return modified Object
 *
 * @note MT-NOTE: rqs_set_defaults() is MT safe
 */
ocs::tom::Element<ocs::tom::RQS> rqs_set_defaults(const ocs::tom::Element<ocs::tom::RQS> rqs) {
   DENTER(TOP_LAYER);

   if (rqs) {
      /* Create Rule List */
      auto rule_list = ocs::tom::ListPtr<ocs::tom::RQR>::create("Rule_List");
      auto rule = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto limit_list = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      ocs::tom::set<RQRL_name>(limit, "slots");
      ocs::tom::set<RQRL_value>(limit, "0");
      limit_list->push_back(std::move(limit));
      ocs::tom::set<RQR_limit>(rule, std::move(limit_list));
      rule_list->push_back(std::move(rule));

      /* Set RQS_enabled */
      ocs::tom::set<RQS_enabled>(rqs, false);

      /* Set RQS_rule; the setter frees the rule list the set had */
      ocs::tom::set<RQS_rule>(rqs, std::move(rule_list));
   }
   DRETURN(rqs);
}

/**
 * @brief Verify a filter in rqs rule
 *
 * Checks validity of a filter rule.
 * The object names referenced in the scope and xscope parts of the rule
 * are verified.
 *
 * An entry without a name is refused on both sides. A missing name is a null
 * pointer, an empty one is the empty string, and the two reach this check by
 * different routes: the first from a client that packed an empty name (see
 * unpackstr()), the second from a parse that produced one, for instance from
 * the "!" of an exclusion that prefixes nothing. Either way the entry names no
 * object, and in an exclusion filter an entry that matches nothing widens the
 * limit instead of narrowing it.
 *
 * @tparam F the filter field of the rule to check, e.g. RQR_filter_users
 * @param rule the rule to check (RQR_Type), may be an empty handle
 * @param answer_list to pass back error messages
 * @param message error message to add to anwer_list in case of error
 *
 * @return true if everything is OK, else false
 *
 * @note MT-NOTE: rqs_verify_filter() is MT safe
 *
 * @see #rqs_verify_attributes
 */
template <auto F>
static bool rqs_verify_filter(const ocs::tom::ConstElement<ocs::tom::RQR> rule,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *message) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (!rule) {
      ret = false;
   } else if (const auto filter = ocs::tom::get<F>(rule)) {
      for (const auto ep : ocs::tom::get<RQRF_scope>(filter)) {
         const char *name = ocs::tom::get<ST_name>(ep);
         if (name == nullptr || *name == '\0') {
            answer_list_add(answer_list, message, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
      }
      for (const auto ep : ocs::tom::get<RQRF_xscope>(filter)) {
         const char *name = ocs::tom::get<ST_name>(ep);
         if (name == nullptr || *name == '\0') {
            answer_list_add(answer_list, message, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Bring the entries of a host filter into the form they are stored in
 *
 * The reference side takes expressions, and a literal there is merely an
 * expression without metacharacters - but a literal still has to be resolved, so
 * that a short name and the fully qualified one denote the same host whichever
 * way the filter was written. Which entries that applies to is a question of
 * class, and since CS-2680 there are three of them.
 *
 * Neither a host group reference nor a matcher is resolved. The first names a
 * group, the second describes a set; asking the name service about either is how
 * the wrong question gets asked. A group reference reached the resolver before
 * this and failed there silently, which cost nothing but was never right.
 *
 * The matcher notation is accepted as an equivalent spelling of the pattern it
 * carries (N-B-5). Two forms are refused rather than stored:
 *
 *   - an **empty pattern**, which would match no host at all;
 *   - the reserved **address prefixes**, which this version does not support.
 *
 * Both would otherwise be stored, displayed back unchanged and quietly match
 * nothing - and in an exclusion filter, matching nothing widens the limit
 * instead of narrowing it. That is exactly the failure shape a limit must not
 * have, so it is refused where it is written.
 *
 * @param scope the scope or xscope of a host filter (ST_Type), rewritten in place; may be an empty handle
 * @param[out] answer_list receives the refusal
 *
 * @return true when every entry is acceptable
 *
 * @see #rqs_verify_attributes, ocs::reference_pattern
 */
static bool rqs_verify_host_scope(const ocs::tom::List<ocs::tom::ST> scope,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   for (const auto host : scope) {
      const char *name = ocs::tom::get<ST_name>(host);

      switch (ocs::classify_member(name)) {
         case ocs::MemberClass::GROUP_REFERENCE:
            break;

         case ocs::MemberClass::MATCHER: {
            const ocs::MatcherKind kind = ocs::matcher_kind(name);

            if (kind != ocs::MatcherKind::HOST) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_RESOURCEQUOTA_MATCHER_PREFIX_SS,
                                       kind == ocs::MatcherKind::IP ? ocs::MATCHER_PREFIX_IP
                                                                    : ocs::MATCHER_PREFIX_IP6, name);
               ret = false;
            } else if (ocs::matcher_payload(name)[0] == '\0') {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_RESOURCEQUOTA_MATCHER_EMPTY_S, name);
               ret = false;
            }
            break;
         }

         case ocs::MemberClass::LITERAL_HOST:
            // sge_resolve_host() takes the field number at run time and stays untyped
            sge_resolve_host(host, ST_name);
            break;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Verify the attributes of a rqs Object
 *
 * This function verifies the attributes of a given rqs object. A valid rqs
 * object has a name and at least one rule set. After verification it sets the
 * double limit value.
 * Addition checks are done if in_master is true.
 *
 * @param rqs Object that should be verified (RQS_Type)
 * @param answer_list answer list in case of errors
 * @param in_master flag if called by qmaster or by qconf
 * @param master_centry_list the complex entries a limit may name (CE_Type), may be an empty handle
 *
 * @return true on success false on error
 *
 * @note MT-NOTE: rqs_verify_attributes() is MT safe
 */
bool rqs_verify_attributes(const ocs::tom::Element<ocs::tom::RQS> rqs, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const bool in_master, const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /* every rule set needs a RQS_name */
   if (ocs::tom::get<RQS_name>(rqs) == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_RESOURCEQUOTA_NONAME);
      ret = false;
   }

   /* every rule set needs at least one rule */
   const auto rules = ocs::tom::get<RQS_rule>(rqs);
   if (ret && rules.empty()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_RESOURCEQUOTA_NORULES);
      ret = false;
   }

   if (ret && in_master) {
      for (const auto rule : rules) {
         bool host_expand = false;
         bool queue_expand = false;
         const auto limit_list = ocs::tom::get<RQR_limit>(rule);

         /* check user, project and pe filters */
         if (!rqs_verify_filter<RQR_filter_users>(rule, answer_list, MSG_RESOURCEQUOTA_INVALIDUSERFILTER)) {
            ret = false;
            break;
         }

         if (!rqs_verify_filter<RQR_filter_projects>(rule, answer_list, MSG_RESOURCEQUOTA_INVALIDPROJECTFILTER)) {
            ret = false;
            break;
         }

         if (!rqs_verify_filter<RQR_filter_pes>(rule, answer_list, MSG_RESOURCEQUOTA_INVALIDPEFILTER)) {
            ret = false;
            break;
         }

         /* check host and queue filters, set rule level. Needed by schedd */
         if (const auto filter = ocs::tom::get<RQR_filter_hosts>(rule)) {
            host_expand = ocs::tom::get<RQRF_expand>(filter);

            if (!rqs_verify_host_scope(ocs::tom::get<RQRF_xscope>(filter), answer_list) ||
                !rqs_verify_host_scope(ocs::tom::get<RQRF_scope>(filter), answer_list)) {
               ret = false;
               break;
            }
         }
         if (const auto filter = ocs::tom::get<RQR_filter_queues>(rule)) {
            queue_expand = ocs::tom::get<RQRF_expand>(filter);
         }

         if (!host_expand && !queue_expand) {
            ocs::tom::set<RQR_level>(rule, RQR_GLOBAL);
         } else if (host_expand && !queue_expand) {
            /* per host */
            ocs::tom::set<RQR_level>(rule, RQR_HOST);
         } else if (!host_expand && queue_expand) {
            /* per queue */
            ocs::tom::set<RQR_level>(rule, RQR_CQUEUE);
         } else {
            ocs::tom::set<RQR_level>(rule, RQR_QUEUEI);
         }

         // check for duplicate limits in the to-section of a limits rule
         if (ocs::ResourceQuota::has_duplicates(limit_list, answer_list, "limit")) {
            ret = false;
            break;
         }

         for (const auto limit : limit_list) {
            const char *name = ocs::tom::get<RQRL_name>(limit);
            const char *strval = ocs::tom::get<RQRL_value>(limit);
            const auto centry = centry_list_locate(master_centry_list, name);

            if (!centry) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_NOTEXISTING_ATTRIBUTE_SS, SGE_RQS_NAME, name);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }

            ocs::tom::set<RQRL_name>(limit, ocs::tom::get<CE_name>(centry));

            if (strval == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_RESOURCEQUOTA_NORULES);
               ret = false;
               break;
            }

            if (strchr(strval, '$') != nullptr) {
               if (ocs::tom::get<RQR_level>(rule) == RQR_HOST || ocs::tom::get<RQR_level>(rule) == RQR_QUEUEI) {
                  /* the value is a dynamical limit */
                  if (!validate_load_formula(ocs::tom::get<RQRL_value>(limit), answer_list,
                                             master_centry_list, SGE_ATTR_DYNAMICAL_LIMIT)) {
                     ret = false;
                     break;
                  }

                  ocs::tom::set<RQRL_type>(limit, ocs::tom::get<CE_valtype>(centry));
                  ocs::tom::set<RQRL_dynamic>(limit, true);
               } else {
                  answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_RESOURCEQUOTA_DYNAMICLIMITNOTSUPPORTED);
                  ret = false;
                  break;
               }
               /* The evaluation of the value needs to be done at scheduling time. Per default it's zero */
            } else {
               const auto tmp_ce = centry.clone();
               /* fix limit, fill up missing attributes */
               ocs::tom::set<RQRL_dynamic>(limit, false);

               ocs::tom::set<CE_stringval>(tmp_ce, strval);
               if (centry_fill_and_check(*tmp_ce, answer_list, false, false)) {
                  ret = false;
                  break;
               }

               ocs::tom::set<RQRL_value>(limit, ocs::tom::get<CE_stringval>(tmp_ce));
               ocs::tom::set<RQRL_dvalue>(limit, ocs::tom::get<CE_doubleval>(tmp_ce));
               ocs::tom::set<RQRL_type>(limit, ocs::tom::get<CE_valtype>(tmp_ce));
            }
         }
         if (!ret) {
            break;
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief Locate a specific resource quota set by name
 *
 * This function searches the rule set list for a specific resource quota
 * set. The search criteria is the name of the resource quota set.
 *
 * @param lp rule set list to be searched in (RQS_Type), may be an empty handle
 * @param name rule set name of interest
 *
 * @return if found the reference to the resource quota set, else an empty handle
 *
 * @note MT-NOTE: rqs_list_locate() is MT safe
 */
ocs::tom::Element<ocs::tom::RQS> rqs_list_locate(const ocs::tom::ConstList<ocs::tom::RQS> lp, const char *name) {
   DENTER(TOP_LAYER);

   const auto ep = ocs::tom::find_first_rw<RQS_name>(lp, name);

   DRETURN(ep);
}


/**
 * @brief Locate a specific rule by name
 *
 * This function searches a rule list for a specific rule. The search criteria
 * is the name or the index of the rule.
 * The index is used if the name was successfully converted to an integer by
 * the atoi() function. If atoi() was not able to convert the name the rule is
 * searched by a sting compare.
 *
 * @param lp list to be searched in (RQR_Type), may be an empty handle
 * @param name rule name of interest
 *
 * @return reference to found rule, an empty handle if rule was not found
 *
 * @note MT-NOTE: rqs_rule_locate() is MT safe
 */
ocs::tom::Element<ocs::tom::RQR> rqs_rule_locate(const ocs::tom::List<ocs::tom::RQR> lp, const char *name) {
   DENTER(TOP_LAYER);

   if (name == nullptr) {
      DRETURN(nullptr);
   }

   const int get_pos = atoi(name);
   int act_pos = 1;

   for (const auto ep : lp) {
      const char *rule_name = ocs::tom::get<RQR_name>(ep);
      if (get_pos != -1 && act_pos == get_pos) {
         DRETURN(ep);
      } else if (rule_name != nullptr && !strcasecmp(name, rule_name)) {
         DRETURN(ep);
      }
      act_pos++;
   }

   DRETURN(nullptr);
}

/**
 * @brief Qconf xattr list preformat
 *
 * This function preformates the given list created by xattr. The xattr switch
 * allows to address single rules by using the special rule set name "rule_set_name/rule_name".
 * The rule name can be the name for named rules or the index of the rule.
 * This function splits such a name into the single rule set name and set the correct
 * rule name.
 *
 * The signature stays untyped: the function is called through a pointer of
 * the qconf attribute table.
 *
 * @param this_list list to be modified (RQS_Type), checked; may be nullptr
 * @param answer_list answer list
 *
 * @return true on success false on error
 *
 * @note MT-NOTE: rqs_xattr_pre_gdi() is MT safe
 */
bool rqs_xattr_pre_gdi(lList *this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char delim[] = "/";

   for (const auto rqs : ocs::tom::List<ocs::tom::RQS>(this_list)) {
      const auto names = ocs::tom::ListPtr<ocs::tom::ST>::from_string<ST_name>(ocs::tom::get<RQS_name>(rqs), delim);
      if (names->size() == 2) {
         const auto set_name = names->front();
         const auto rule_name = set_name.next();

         ocs::tom::set<RQS_name>(rqs, ocs::tom::get<ST_name>(set_name));
         for (const auto rule : ocs::tom::get<RQS_rule>(rqs)) {
            ocs::tom::set<RQR_name>(rule, ocs::tom::get<ST_name>(rule_name));
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Creates a rue name
 *
 * Creates the rue name used for debiting a specific job request. The name
 * consists of the five touples devided by a /. The order is user, project,
 * pe, queue, host.
 * Filters that count for a sum
 * of hosts are saved as a empty string because they don't need to be matched.
 * For example the rule
 *   limit users `*` queues `all.q,my.q` to slots=10
 * may result in the rue name
 *   user1///all.q//
 *
 * @param name out: rue_name
 * @param rule resource quota rule (RQR_Type), may be an empty handle
 * @param user user name
 * @param project project name
 * @param host host name
 * @param queue queue name
 * @param pe pe name
 *
 * @return false for an empty rule, true otherwise
 *
 * @note MT-NOTE: rqs_get_rue_string() is MT safe
 */
bool rqs_get_rue_string(dstring *name, const ocs::tom::ConstElement<ocs::tom::RQR> rule, const char *user,
                        const char *project, const char *host, const char *queue, const char *pe) {
   DENTER(BASIS_LAYER);

   if (!rule) {
      DRETURN(false);
   }

   sge_dstring_clear(name);

   if (const auto filter = ocs::tom::get<RQR_filter_users>(rule)) {
      if (user != nullptr && ocs::tom::get<RQRF_expand>(filter)) {
         sge_dstring_append(name, user);
      }
   }
   sge_dstring_append(name, "/");

   if (const auto filter = ocs::tom::get<RQR_filter_projects>(rule)) {
      if (project != nullptr && ocs::tom::get<RQRF_expand>(filter)) {
         sge_dstring_append(name, project);
      }
   }
   sge_dstring_append(name, "/");

   if (const auto filter = ocs::tom::get<RQR_filter_pes>(rule)) {
      if (pe != nullptr && ocs::tom::get<RQRF_expand>(filter)) {
         sge_dstring_append(name, pe);
      }
   }
   sge_dstring_append(name, "/");

   if (const auto filter = ocs::tom::get<RQR_filter_queues>(rule)) {
      if (queue != nullptr && ocs::tom::get<RQRF_expand>(filter)) {
         sge_dstring_append(name, queue);
      }
   }
   sge_dstring_append(name, "/");

   if (const auto filter = ocs::tom::get<RQR_filter_hosts>(rule)) {
      if (host != nullptr && ocs::tom::get<RQRF_expand>(filter)) {
         char buffer[10240];
         sge_hostcpy(buffer, host);
         sge_dstring_append(name, buffer);
      }
   }
   sge_dstring_append(name, "/");

   DPRINTF("rue_name: %s\n", sge_dstring_get_string(name));

   DRETURN(true);
}

/**
 * @brief Debit slots in all relevant rule sets
 *
 * iterater over all rules in the given rule set and debit the amount of slots
 * in the relevant rule
 *
 * @param rqs resource quota set (RQS_Type)
 * @param job job request (JB_Type)
 * @param granted granted destination identifier (JG_Type)
 * @param pe granted pe (PE_Type), may be an empty handle
 * @param centry_list consumable resouces list (CE_Type)
 * @param acl_list the usersets a rule's filter may name (US_Type)
 * @param hgrp_list the host groups a rule's filter may name (HGRP_Type)
 * @param slots slot amount
 * @param is_master_task true when this booking includes the job's master task
 * @param do_per_host_booking true when a per host consumable still has to be booked on this host
 *
 * @return amount of modified rule
 *
 * @note MT-NOTE: rqs_debit_consumable() is not MT safe
 */
int rqs_debit_consumable(const ocs::tom::Element<ocs::tom::RQS> rqs, const ocs::tom::ConstElement<ocs::tom::JB> job,
                         const ocs::tom::ConstElement<ocs::tom::JG> granted,
                         const ocs::tom::ConstElement<ocs::tom::PE> pe,
                         const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         const ocs::tom::ConstList<ocs::tom::US> acl_list,
                         const ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list, const int slots,
                         const bool is_master_task, const bool do_per_host_booking) {
   DENTER(TOP_LAYER);

   int mods = 0;
   const char *hostname = ocs::tom::get<JG_qhostname>(granted);
   const char *username = ocs::tom::get<JB_owner>(job);
   const char *groupname = ocs::tom::get<JB_group>(job);
   const auto grp_list = ocs::tom::get<JB_grp_list>(job);
   const char *queue_instance = ocs::tom::get<JG_qname>(granted);
   const char *project = ocs::tom::get<JB_project>(job);

   if (!ocs::tom::get<RQS_enabled>(rqs)) {
      DRETURN(0);
   }

   /* remove the host name part of the queue instance name */
   char *qname = cqueue_get_name_from_qinstance(queue_instance);

   const char *pe_name = nullptr;
   if (pe) {
      pe_name = ocs::tom::get<PE_name>(pe);
   }

   const auto rule = rqs_get_matching_rule(rqs, username, groupname, grp_list, project, pe_name, hostname, qname,
                                           acl_list, hgrp_list, nullptr);

   if (rule) {
      /* debit usage */
      dstring rue_name = DSTRING_INIT;

      rqs_get_rue_string(&rue_name, rule, username, project, hostname, qname, pe_name);

      mods = rqs_debit_rule_usage(job, pe, rule, &rue_name, slots, centry_list, ocs::tom::get<RQS_name>(rqs),
                                  is_master_task, do_per_host_booking);

      sge_dstring_free(&rue_name);
   }

   sge_free(&qname);

   DRETURN(mods);
}


/**
 * @brief Found relevant rule for job request
 *
 * This function searches in a resource quota set the relevant rule.
 *
 * The rule comes back mutable although the set is read only, as the raw code
 * did: the callers debit usage in it or set its dynamical limits.
 *
 * @param rqs rule set (RQS_Type)
 * @param user user name
 * @param group the user's primary group
 * @param grp_list the user's supplementary groups (ST_Type), may be an empty handle
 * @param project project name
 * @param pe pe name
 * @param host host name
 * @param queue queue name
 * @param userset_list master user set list (US_Type)
 * @param hgroup_list master host group list (HGRP_Type)
 * @param rule_name out: name or matching rule
 *
 * @return the matching rule, an empty handle if no rule matches
 *
 * @note MT-NOTE: rqs_get_matching_rule() is MT safe
 */
ocs::tom::Element<ocs::tom::RQR> rqs_get_matching_rule(const ocs::tom::ConstElement<ocs::tom::RQS> rqs,
                                                       const char *user, const char *group,
                                                       const ocs::tom::ConstList<ocs::tom::ST> grp_list,
                                                       const char *project, const char *pe, const char *host,
                                                       const char *queue,
                                                       const ocs::tom::ConstList<ocs::tom::US> userset_list,
                                                       const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list,
                                                       dstring *rule_name) {
   DENTER(BASIS_LAYER);

   int i = 0;

   for (const auto rule : ocs::tom::get<RQS_rule>(rqs)) {
      i++;

      if (!rqs_is_matching_rule(rule, user, group, grp_list, project, pe, host, queue, userset_list, hgroup_list)) {
         continue;
      }
      if (ocs::tom::get<RQR_name>(rule)) {
         DPRINTF("Using resource quota %s\n", ocs::tom::get<RQR_name>(rule));
         sge_dstring_sprintf(rule_name, "%s/%s", ocs::tom::get<RQS_name>(rqs), ocs::tom::get<RQR_name>(rule));
      } else {
         DPRINTF("Using resource quota %d\n", i);
         sge_dstring_sprintf(rule_name, "%s/%d", ocs::tom::get<RQS_name>(rqs), i);
      }
      /* if all filter object matches this is our rule */
      DRETURN(ocs::tom::Element<ocs::tom::RQR>::from_typed(const_cast<lListElem *>(rule.raw())));
   }
   DRETURN(nullptr);
}

/**
 * @brief Debit usage in a resource quota rule
 *
 * Debit an amount of slots in all limits of one resource quota rule
 *
 * @param job job request (JB_Type), may be an empty handle
 * @param pe the job's parallel environment (PE_Type), may be an empty handle
 * @param rule resource quota rule (RQR_Type)
 * @param rue_name rue name that counts
 * @param slots amount of slots to debit
 * @param centry_list consumable resource list (CE_Type)
 * @param obj_name name of the limit
 * @param is_master_task true when this booking includes the job's master task
 * @param do_per_host_booking true when a per host consumable still has to be booked on this host
 *
 * @return amount of debited limits
 *
 * @note MT-NOTE: rqs_debit_rule_usage() is MT safe
 */
// @todo: pass rue_name as const char *
int rqs_debit_rule_usage(const ocs::tom::ConstElement<ocs::tom::JB> job, const ocs::tom::ConstElement<ocs::tom::PE> pe,
                         const ocs::tom::Element<ocs::tom::RQR> rule, dstring *rue_name, const int slots,
                         const ocs::tom::ConstList<ocs::tom::CE> centry_list, const char *obj_name,
                         const bool is_master_task, const bool do_per_host_booking) {
   DENTER(TOP_LAYER);
   int mods = 0;

   for (const auto limit : ocs::tom::get<RQR_limit>(rule)) {

      const char *centry_name = ocs::tom::get<RQRL_name>(limit);

      const auto centry = centry_list_locate(centry_list, centry_name);
      if (!centry) {
         /* ignoring not defined centry */
         continue;
      }

      uint32_t consumable = ocs::tom::get<CE_consumable>(centry);
      if (!consumable_do_booking(consumable, is_master_task, do_per_host_booking)) {
         continue;
      }

      auto rue_elem = ocs::tom::find_first_in_rw<RQRL_usage, RUE_name>(limit, sge_dstring_get_string(rue_name));
      if (!rue_elem) {
         rue_elem = ocs::tom::emplace_back_in<RQRL_usage, RUE_name>(limit, sge_dstring_get_string(rue_name));
         /* RUE_utilized_now is implicitly set to zero */
      }


      if (job) {
         bool did_booking = false;
         int debit_slots = consumable_get_debit_slots(consumable, slots);

         // has contribution from global requests? Then we can do the booking for master and slave task in one step.
         double dval = 0.0;
         bool tmp_ret = job_get_contribution_by_scope(job, nullptr, centry_name, &dval, centry, JRS_SCOPE_GLOBAL);
         if (tmp_ret) {
            DPRINTF("debiting %f of %s on rqs %s for %s %d slots\n", dval, centry_name,
                    obj_name, sge_dstring_get_string(rue_name), debit_slots);
            if (dval != 0.0) {
               ocs::tom::add<RUE_utilized_now>(rue_elem, debit_slots * dval);
               mods++;
               did_booking = true;
            }
         } else if (pe) {
            // no global contribution, need to check master and slave
            // we use the original slots value for slave_debit_slots
            // reason: for host consumables, debit_slots is 1, which will be adjusted below to 0
            //         if we have then a slave request for a host consumable, it will not be booked!
            int slave_debit_slots = slots;
            if (is_master_task) {
               // if the master task is part of this booking, check if we have a master request
               dval = 0.0;
               tmp_ret = job_get_contribution_by_scope(job, nullptr, centry_name, &dval, centry, JRS_SCOPE_MASTER);
               if (tmp_ret) {
                  DPRINTF("debiting %f of %s on rqs %s for %s %d slots\n", dval, centry_name,
                          obj_name, sge_dstring_get_string(rue_name), slot_signum(debit_slots));
                  if (dval != 0.0) {
                     // book it for one slot (the master task)
                     ocs::tom::add<RUE_utilized_now>(rue_elem, slot_signum(debit_slots) * dval);
                     mods++;
                     did_booking = true;
                  }
               }

               // if we did the master task booking
               // adjust the slot count for the slave booking
               adjust_slave_task_debit_slots(pe, slave_debit_slots);
            }
            // now do booking for the (remaining) slave tasks, if any
            if (slave_debit_slots != 0) {
               dval = 0.0;
               // if we are on the master host, and we shall ignore slave requests to booking only for slots
               if (is_master_task && ocs::tom::get<PE_ignore_slave_requests_on_master_host>(pe)) {
                  tmp_ret = true;
                  if (sge_strnullcmp(centry_name, SGE_ATTR_SLOTS) == 0) {
                     dval = 1.0;
                  }
               } else {
                  tmp_ret = job_get_contribution_by_scope(job, nullptr, centry_name, &dval, centry,
                                                          JRS_SCOPE_SLAVE);
               }
               if (tmp_ret && dval != 0.0) {
                  DPRINTF("debiting %f of %s on rqs %s for %s %d slots\n", dval, centry_name,
                           obj_name, sge_dstring_get_string(rue_name), slave_debit_slots);
                  // book it for the remaining slave tasks
                  slave_debit_slots = consumable_get_debit_slots(consumable, slave_debit_slots);
                  ocs::tom::add<RUE_utilized_now>(rue_elem, slave_debit_slots * dval);
                  mods++;
                  did_booking = true;
               }
            }
         }

         // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
         if (!did_booking && ocs::tom::get<CE_relop>(centry) == CMPLXEXCL_OP) {
            dval = 1.0;
            DPRINTF("debiting (non-exclusive) %f of %s on rqs %s for %s %d slots\n", dval, centry_name,
                    obj_name, sge_dstring_get_string(rue_name), debit_slots);
            ocs::tom::add<RUE_utilized_now_nonexclusive>(rue_elem, debit_slots * dval);
            mods++;
         }

         // If the booking element's value is back to 0, it is no longer needed. Remove it.
         // @todo: is non-existance of the booking element important for some RQS algorithm? If not, keep it
         //        as we do in the other places where we do booking (rc_debit_consumable, rc_|rqs_add_job_utilization)
         //        This can spare us a significant number of malloc and free calls.
         if (ocs::tom::get<RUE_utilized_now>(rue_elem) == 0
             && ocs::tom::get<RUE_utilized_now_nonexclusive>(rue_elem) == 0
             && !ocs::tom::get<RUE_utilized>(rue_elem) && !ocs::tom::get<RUE_utilized_nonexclusive>(rue_elem)) {
            ocs::tom::get<RQRL_usage>(limit).erase(rue_elem);
         }
      }
   }

   DRETURN(mods);
}

/**
 * @brief Match user or host scope
 *
 * This function verifies a user or host scope. The function allows for every
 * scope entry and for the value a wildcard definition. Hostgroups and Usergroups
 * are resolved and matched against the value
 *
 * @param scope Scope to match (ST_Type), may be an empty handle
 * @param filter_type filter type (FILTER_USERS or FILTER_HOSTS)
 * @param value value to match
 * @param master_userset_list master userset list (US_Type)
 * @param master_hgroup_list master hostgroup list (HGRP_Type)
 * @param group the user's primary group
 * @param is_xscope true when @p scope is the exclusion side of the filter
 * @param grp_list the user's supplementary groups (ST_Type), may be an empty handle
 *
 * @return true, if value was found in scope false, if value was not found in scope
 *
 * @note MT-NOTE: rqs_match_user_host_scope() is MT safe
 */
static bool rqs_match_user_host_scope(const ocs::tom::ConstList<ocs::tom::ST> scope, const int filter_type,
                                      const char *value, const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                                      const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                                      const char *group, const bool is_xscope,
                                      const ocs::tom::ConstList<ocs::tom::ST> grp_list) {
   DENTER(TOP_LAYER);

   bool found = false;

   if (!ocs::is_pattern(value)) {
      /* used in scheduler/qmaster and qquota */
      if (ocs::tom::find_first<ST_name>(scope, value)) {
         found = true;
      } else {
         for (const auto ep : scope) {
            /*
             * CS-2680, N-B-5. A host filter entry is stored as it was written,
             * so the matcher notation still carries its prefix; here the prefix
             * is spelling and what it denotes is the pattern behind it. Only for
             * the host filter: this function also serves the user filter, and a
             * user named "host:x" is a user.
             */
            const char *cp = filter_type == FILTER_HOSTS
                             ? ocs::reference_pattern(ocs::tom::get<ST_name>(ep))
                             : ocs::tom::get<ST_name>(ep);
            const char *group_name = nullptr;
            const char *query = nullptr;

            if (fnmatch(cp, value, 0) == 0) {
               found = true;
               break;
            }

            const bool value_is_hgroup = ocs::is_hgroup_name(value);
            const bool cp_is_hgroup = ocs::is_hgroup_name(cp);
            if (!value_is_hgroup && cp_is_hgroup) {
               group_name = cp;
               query = value;
            } else if (value_is_hgroup && !cp_is_hgroup) {
               group_name = value;
               query = cp;
            }

            if (group_name != nullptr && query != nullptr) {
               DPRINTF("group_name=%s, query=%s\n", group_name, query);
               if (filter_type == FILTER_USERS) {
                  /* the userset name does not contain the preattached \@ sign */
                  group_name++;
                  if (!ocs::is_pattern(group_name)) {
                     if (const auto userset = ocs::tom::find_first<US_name>(master_userset_list, group_name)) {
                        if (sge_contained_in_access_list(query, group, grp_list, userset) == 1) {
                           found = true;
                           break;
                        }
                     }
                  } else {
                     for (const auto userset : master_userset_list) {
                        if (fnmatch(group_name, ocs::tom::get<US_name>(userset), 0) == 0) {
                           if (sge_contained_in_access_list(query, group, grp_list, userset) == 1) {
                              found = true;
                              break;
                           }
                        }
                     }
                     if (found) {
                        break;
                     }
                  }
               } else { /* FILTER_HOSTS */
                  if (!ocs::is_pattern(group_name)) {
                     if (const auto hgroup = hgroup_list_locate(master_hgroup_list, group_name)) {
                        ocs::tom::ListPtr<ocs::tom::HR> host_list;
                        hgroup_find_all_references(hgroup, nullptr, master_hgroup_list, &host_list, nullptr);
                        if (host_list && ocs::tom::find_first<HR_name>(*host_list, query)) {
                           found = true;
                           break;
                        } else if (ocs::is_pattern(query)) {
                           for (const auto host_ep : *host_list) {
                              if (fnmatch(query, ocs::tom::get<HR_name>(host_ep), 0) == 0) {
                                 found = true;
                                 break;
                              }
                           }
                           if (found) {
                              break;
                           }
                        }
                     }
                  } else {
                     for (const auto hgroup : master_hgroup_list) {
                        if (fnmatch(group_name, ocs::tom::get<HGRP_name>(hgroup), 0) == 0) {
                           ocs::tom::ListPtr<ocs::tom::HR> host_list;
                           hgroup_find_all_references(hgroup, nullptr, master_hgroup_list, &host_list, nullptr);
                           if (host_list && ocs::tom::find_first<HR_name>(*host_list, query)) {
                              found = true;
                              break;
                           } else if (ocs::is_pattern(query)) {
                              for (const auto host_ep : *host_list) {
                                 if (fnmatch(query, ocs::tom::get<HR_name>(host_ep), 0) == 0) {
                                    found = true;
                                    break;
                                 }
                              }
                              if (found) {
                                 break;
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   } else {
      /* only used in qquota */
      for (const auto ep : scope) {
         // see the note in the branch above: the prefix is spelling, and only
         // the host filter has it
         const char *cp = filter_type == FILTER_HOSTS
                          ? ocs::reference_pattern(ocs::tom::get<ST_name>(ep))
                          : ocs::tom::get<ST_name>(ep);
         const char *group_name = nullptr;
         const char *query = nullptr;

         if (fnmatch(value, cp, 0) == 0) {
            if (!is_xscope) {
               found = true;
               break;
            } else {
               if (strcmp(value, cp) == 0) {
                  /* amount of sets is equal */
                  found = true;
                  break;
               } else {
                  /* amount of sets does not overlap. We can not reject in
                     xscope context and have to wave through */
                  found = false;
                  break;
               }
            }
         }
         if (ocs::is_pattern(cp) && (fnmatch(cp, value, 0) == 0)) {
            found = true;
            break;
         }
         const bool value_is_hgroup = ocs::is_hgroup_name(value);
         const bool cp_is_hgroup = ocs::is_hgroup_name(cp);
         if (!value_is_hgroup && cp_is_hgroup) {
            group_name = cp;
            query = value;
         } else if (value_is_hgroup && !cp_is_hgroup) {
            group_name = value;
            query = cp;
         }

         if (group_name != nullptr && query != nullptr) {
            DPRINTF("group_name=%s, query=%s\n", group_name, query);
            if (filter_type == FILTER_USERS) {
               /* the userset name does not contain the preattached \@ sign */
               group_name++;
               for (const auto userset : master_userset_list) {
                  if (fnmatch(group_name, ocs::tom::get<US_name>(userset), 0) == 0) {
                     if (sge_contained_in_access_list(query, group, grp_list, userset) == 1) {
                        found = true;
                        break;
                     }
                  }
               }
               if (found) {
                  break;
               }
            } else {
               for (const auto hgroup : master_hgroup_list) {
                  if (fnmatch(group_name, ocs::tom::get<HGRP_name>(hgroup), 0) == 0) {
                     ocs::tom::ListPtr<ocs::tom::HR> host_list;
                     hgroup_find_all_references(hgroup, nullptr, master_hgroup_list, &host_list, nullptr);
                     for (const auto host_ep : *host_list) {
                        if (fnmatch(query, ocs::tom::get<HR_name>(host_ep), 0) == 0) {
                           found = true;
                           break;
                        }
                     }
                     if (found) {
                        break;
                     }
                  }
               }
               if (found) {
                  break;
               }
            }
         }
      }
   }
   DRETURN(found);
}

/**
 * @brief Matches a rule with the filter touples
 *
 * The function verifies for every filter touple if the request matches
 * the configured resource quota rule. If only one touple does not match
 * the whole rule will not match
 *
 * @param rule resource quota rule (RQR_Type)
 * @param user user name
 * @param group the user's primary group
 * @param grp_list the user's supplementary groups (ST_Type), may be an empty handle
 * @param project project name
 * @param pe pe name
 * @param host host name
 * @param queue queue name
 * @param master_userset_list master user set list (US_Type)
 * @param master_hgroup_list master hostgroup list (HGRP_Type)
 *
 * @return true if the rule does match false if the rule does not match
 *
 * @note MT-NOTE: rqs_is_matching_rule() is MT safe
 */
bool rqs_is_matching_rule(const ocs::tom::ConstElement<ocs::tom::RQR> rule, const char *user, const char *group,
                          const ocs::tom::ConstList<ocs::tom::ST> grp_list, const char *project, const char *pe,
                          const char *host, const char *queue,
                          const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                          const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(TOP_LAYER);

   // typed empty handles: a bare nullptr would be ambiguous with the untyped rqs_filter_match()
   const ocs::tom::ConstList<ocs::tom::US> no_usersets;
   const ocs::tom::ConstList<ocs::tom::HGRP> no_hgroups;
   const ocs::tom::ConstList<ocs::tom::ST> no_groups;

   if (!rqs_filter_match(ocs::tom::get<RQR_filter_users>(rule), FILTER_USERS, user, master_userset_list, no_hgroups,
                         group, grp_list)) {
      DRETURN(false);
   }
   if (!rqs_filter_match(ocs::tom::get<RQR_filter_projects>(rule), FILTER_PROJECTS, project, no_usersets, no_hgroups,
                         nullptr, no_groups)) {
      DRETURN(false);
   }
   if (!rqs_filter_match(ocs::tom::get<RQR_filter_pes>(rule), FILTER_PES, pe, no_usersets, no_hgroups, nullptr,
                         no_groups)) {
      DRETURN(false);
   }
   if (!rqs_filter_match(ocs::tom::get<RQR_filter_queues>(rule), FILTER_QUEUES, queue, no_usersets, no_hgroups,
                         nullptr, no_groups)) {
      DRETURN(false);
   }
   if (!rqs_filter_match(ocs::tom::get<RQR_filter_hosts>(rule), FILTER_HOSTS, host, no_usersets, master_hgroup_list,
                         nullptr, no_groups)) {
      DRETURN(false);
   }

   DRETURN(true);
}


/**
 * @brief Match name with host scope
 *
 * The function matches the passed name with the host scope. Name
 * may not only be a hostname, but also host group name or a wildcard
 * expression. For performance reasons qref_list_host_rejected() is
 * used for matching, if we got no pattern and no hostgroup.
 *
 * @param scope A scope list (ST_Type), may be an empty handle
 * @param name hostname/hostgroup name/wildcard expression
 * @param master_hgroup_list the host group list (HGRP_Type)
 * @param is_xscope true when @p scope is the exclusion side of the filter
 * @param grp_list the user's supplementary groups (ST_Type), may be an empty handle
 *
 * @return Returns true if 'name' matches
 *
 * @note MT-NOTE: rqs_match_host_scope() is MT safe
 */
static bool rqs_match_host_scope(const ocs::tom::ConstList<ocs::tom::ST> scope, const char *name,
                                 const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, const bool is_xscope,
                                 const ocs::tom::ConstList<ocs::tom::ST> grp_list) {
   DENTER(TOP_LAYER);

   if (ocs::tom::find_first<ST_name>(scope, "*")) {
      DRETURN(true);
   }

   if (ocs::is_pattern(name) || ocs::is_hgroup_name(name)) {
      DRETURN(rqs_match_user_host_scope(scope, FILTER_HOSTS, name, nullptr, master_hgroup_list, nullptr, is_xscope,
                                        grp_list));
   }

   /* at this stage we know 'name' is a simple hostname */
   for (const auto ep : scope) {
      /*
       * CS-2680, N-B-5. The entry is stored as it was written, so a matcher
       * notation still carries its prefix here; on this side the prefix is
       * spelling and the pattern behind it is what the entry denotes.
       */
      const char *pattern = ocs::reference_pattern(ocs::tom::get<ST_name>(ep));

      if (!qref_list_host_rejected(pattern, name, master_hgroup_list)) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}


/**
 * @brief Compares value with configured filter
 *
 * This function compares for the given filter if the value does match
 * the configured one. Wildcards are allowed for the filter as well as for
 * the value.
 *
 * The filter type is a run-time value and selects how the value is matched.
 *
 * @param filter filter element (RQRF_Type), may be an empty handle, which matches everything
 * @param filter_type filter type
 * @param value value to match
 * @param master_userset_list master userset list (US_Type)
 * @param master_hgroup_list master hostgroup list (HGRP_Type)
 * @param group the user's primary group
 * @param grp_list the user's supplementary groups (ST_Type), may be an empty handle
 *
 * @return true if the value does match false if the value does not match
 *
 * @note MT-NOTE: rqs_filter_match() is MT safe
 */
bool rqs_filter_match(const ocs::tom::ConstElement<ocs::tom::RQRF> filter, const int filter_type, const char *value,
                      const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                      const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, const char *group,
                      const ocs::tom::ConstList<ocs::tom::ST> grp_list) {
   DENTER(BASIS_LAYER);
   bool ret = true;

   if (filter) {
      const auto scope = ocs::tom::get<RQRF_scope>(filter);
      const auto xscope = ocs::tom::get<RQRF_xscope>(filter);

      switch (filter_type) {
         case FILTER_HOSTS:
            DPRINTF("matching hosts with %s\n", value);
            /* inverse logic because of xscope */
            ret = rqs_match_host_scope(xscope, value, master_hgroup_list, true, grp_list) ? false: true;
            if (ret && scope != nullptr) {
               if (!rqs_match_host_scope(scope, value, master_hgroup_list, false, grp_list)) {
                  ret = false;
               }
            }
            break;

         case FILTER_USERS:
         {
            DPRINTF("matching users or hosts with %s\n", value);
            /* inverse logic because of xscope */
            ret = rqs_match_user_host_scope(xscope, filter_type, value, master_userset_list, nullptr, group, true,
                                            grp_list) ? false: true;
            if (ret && scope != nullptr) {
               if (!rqs_match_user_host_scope(scope, filter_type, value, master_userset_list, nullptr, group, false,
                                              grp_list)) {
                  ret = false;
               }
            }
            break;
         }
         case FILTER_PROJECTS:
         case FILTER_PES:
         case FILTER_QUEUES:
            DPRINTF("matching projects, pes or queues with %s\n", value? value: "nullptr");
            if (ocs::tom::find_first<ST_name>(xscope, value)) {
               ret = false;
            } else {
               for (const auto ep : xscope) {
                  const char *cp = ocs::tom::get<ST_name>(ep);
                  if (value == nullptr || (strcmp(value, "*") == 0)) {
                     break;
                  }
                  DPRINTF("xscope: strcmp(%s,%s)\n", cp, value);
                  if ((strcmp(cp, "*") == 0) || (fnmatch(cp, value, 0) == 0) || (fnmatch(value,cp, 0) == 0)) {
                     DPRINTF("match\n");
                     ret = false;
                     break;
                  }
                  DPRINTF("no match\n");
               }
            }
            if (ret != false) {
               bool found = false;
               if (ocs::tom::find_first<ST_name>(scope, value)) {
                  found = true;
               } else {
                  for (const auto ep : scope) {
                     const char *cp = ocs::tom::get<ST_name>(ep);

                     if (value == nullptr) {
                        break;
                     }
                     DPRINTF("scope: strcmp(%s,%s)\n", cp, value);
                     if ((strcmp(cp, "*") == 0) || (fnmatch(cp, value, 0) == 0) || (fnmatch(value,cp, 0) == 0)) {
                        found = true;
                        break;
                     }
                  }
               }
               if (scope != nullptr && !found) {
                  ret = false;
               }
            }
            break;
      }
   }

   DRETURN(ret);
}


/**
 * @brief Search for a centry reference in
 *
 * This function search a centry reference in a resource quota set
 *
 * @param rqs resource quota set (RQS_Type)
 * @param centry complex entry (CE_Type)
 *
 * @return true if found false if not found
 *
 * @note MT-NOTE: sge_centry_referenced_in_rqs() is MT safe
 */
bool sge_centry_referenced_in_rqs(const ocs::tom::ConstElement<ocs::tom::RQS> rqs,
                                  const ocs::tom::ConstElement<ocs::tom::CE> centry) {
   DENTER(TOP_LAYER);

   bool ret = false;
   const char *centry_name = ocs::tom::get<CE_name>(centry);

   for (const auto rule : ocs::tom::get<RQS_rule>(rqs)) {
      for (const auto limit : ocs::tom::get<RQR_limit>(rule)) {
         const char *limit_name = ocs::tom::get<RQRL_name>(limit);
         DPRINTF("limit name %s\n", limit_name);
         if (strchr(limit_name, '$') != nullptr) {
            /* dynamical limit */
            if (load_formula_is_centry_referenced(limit_name, centry)) {
               ret = true;
               break;
            }
         } else {
            /* static limit */
            if (strcmp(limit_name, centry_name) == 0) {
               ret = true;
               break;
            }
         }
      }
      if (ret) {
         break;
      }
   }

   DRETURN(ret);
}
