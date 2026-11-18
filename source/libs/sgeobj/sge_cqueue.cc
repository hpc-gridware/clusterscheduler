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
 * @brief Cluster queues: the template the queue instances are derived from
 *
 * An administrator configures a cluster queue; qmaster derives one queue
 * instance per host from it. Most attributes are therefore stored as a list of
 * host or host group specific values, and #ocs_cqueue_attributes is the table
 * saying, for each attribute, where its list lives and where the resolved
 * value ends up in the instance.
 *
 * @see sge_cqueue.h
 */

#include <cstring>
#include <tuple>
#include <type_traits>

#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_stdlib.h"

#include "comm/commlib.h"

#include "cull/cull_list.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/ocs_cqueue_attributes.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_ABOOL.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"

#include "msg_clients_common.h"
#include "msg_common.h"

/// Debug layer the cluster queue traces are written to
#define CQUEUE_LAYER TOP_LAYER


/* EB: ADOC: add commets */

/**
 * @brief Get the CQ and host part of a QI name
 *
 * Splits a qinstance name into its components.
 * Examples:
 * QI-name         cqueue_name  host_domain  has_hostname  has_domain
 * ------------------------------------------------------------------
 * all.q           all.q        ""           false         false
 * `all.q@hostname`  all.q        hostname     true          false
 * `all.q@@hgrp`     all.q        `@hgrp`        false         true
 *
 * @param name CQ/QD or QI name
 * @param cqueue_name CQ part of the name
 * @param host_domain host or hostgroup or nothing
 * @param has_hostname is "host_domain" a hostname
 * @param has_domain if "host_domain" a hostgroup
 *
 * @return error state always true  - success
 */
bool cqueue_name_split(const char *name,
                       dstring *cqueue_name, dstring *host_domain,
                       bool *has_hostname, bool *has_domain) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (has_hostname)
      *has_hostname = false;
   if (has_domain)
      *has_domain = false;

   if (name != nullptr && cqueue_name != nullptr && host_domain != nullptr ) {
      bool at_skiped = false;

      sge_dstring_clear(cqueue_name);
      sge_dstring_clear(host_domain);

      while (*name != '\0') {
         if (!at_skiped && *name == '@') {
            at_skiped = true;
            name++;
            if (*name == '\0') {
               ret = false;
               break;
            }
            if (*name == '@') {
               if (*(name + 1) == '\0') {
                  ret = false;
                  break;
               }
               if (has_domain)
                  *has_domain = true;
               if (has_hostname)
                  *has_hostname = false;
            } else {
               if (has_domain)
                  *has_domain = false;
               if (has_hostname)
                  *has_hostname = true;
            }
            continue; 
         }
         if (!at_skiped) {
            sge_dstring_append_char(cqueue_name, name[0]);
         } else {
            sge_dstring_append_char(host_domain, name[0]);
         }
         name++;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Returns the cluster queue part of a queue
 *
 * Returns a character pointer to a newly malloced string containing the cluster
 * queue part of a queue instance name.
 * The memory needs to be free'd by the caller
 *
 * @param queue_instance queue instance or cluster queue
 *
 * @return cluster queue name
 *
 * @note MT-NOTE: cqueue_get_name_from_qinstance() is MT safe
 */
char *cqueue_get_name_from_qinstance(const char *queue_instance) {
   char *at_sign = nullptr;
   char *cqueue = nullptr;

   if ((at_sign = strchr((char *)queue_instance, '@'))) {
      int size = at_sign - queue_instance;
      cqueue = sge_malloc(sizeof(char) * (size + 1));
      cqueue = strncpy(cqueue, queue_instance, size);
      cqueue[size] = '\0';
   } else {
      cqueue = strdup(queue_instance);
   }

   return cqueue;
}

/**
 * @brief Create a new cluster queue object
 *
 * Returns a new cluster queue object with the name "name".
 *
 * @param answer_list AN_Type list; no longer written: running out of memory throws, as every create() does
 * @param name cluster queue name
 *
 * @return the CQ_Type object, or an empty handle if "name" is nullptr
 */
ocs::tom::ElementPtr<ocs::tom::CQ> cqueue_create([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                 const char *name) {
   DENTER(CQUEUE_LAYER);

   ocs::tom::ElementPtr<ocs::tom::CQ> ret;

   if (name != nullptr) {
      ret = ocs::tom::ElementPtr<ocs::tom::CQ>::create();
      ret->set<CQ_name>(name);
   }
   DRETURN(ret);
}

/**
 * @brief The host list of a cluster queue
 *
 * CS-2677. A cluster queue does not carry a host list any more -- it owns the
 * host group `@@<queue>`, and that group's member list is the host list. This
 * is the accessor that replaces `lGetList(cqueue, CQ_hostlist)`, and the only
 * way the qmaster should reach it: the field of the same name still exists,
 * but it carries an instruction on its way in and is empty on every element
 * the master list holds.
 *
 * What is returned is the member list as written -- host names, group
 * references, and whatever else a host group may contain. Callers wanting the
 * hosts it reaches resolve it with href_list_find_all_references().
 *
 * @param cqueue CQ_Type object, may be an empty handle
 * @param master_hgroup_list HGRP_Type master list, may be an empty handle
 *
 * @return the member list (HR_Type), or an empty handle when the queue has no group yet
 *
 * @note MT-NOTE: cqueue_get_hostlist() is MT safe
 */
ocs::tom::ConstList<ocs::tom::HR> cqueue_get_hostlist(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                                      const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   if (!cqueue) {
      return nullptr;
   }
   dstring name_buffer = DSTRING_INIT;
   const char *group_name = hgroup_queue_group_name(ocs::tom::get<CQ_name>(cqueue), &name_buffer);
   const auto hgroup = hgroup_list_locate(master_hgroup_list, group_name);

   sge_dstring_free(&name_buffer);

   if (!hgroup) {
      return nullptr;
   }
   return ocs::tom::get<HGRP_host_list>(hgroup);
}


/**
 * @brief Put the host list onto a cluster queue, for a client that renders it
 *
 * CS-2677, the client side of cqueue_get_hostlist(). What GDI hands out is a
 * queue without a host list -- the list is the member list of the queue's host
 * group, and travels as a host group. A client that displays or edits a queue
 * configuration wants the two joined again, in the shape the configuration
 * format has always had (N-X-1), and this joins them: on the client's own
 * copy, from the host groups it fetched in the same request.
 *
 * The field it fills is the one a write travels in, so an element completed
 * here and sent back unchanged reproduces the same member list -- which is
 * what makes the round trip idempotent (N-X-3).
 *
 * @param cqueue CQ_Type object, modified in place; may be an empty handle
 * @param master_hgroup_list HGRP_Type list, as fetched from the qmaster
 *
 * @note MT-NOTE: cqueue_fill_hostlist() is MT safe
 */
void cqueue_fill_hostlist(const ocs::tom::Element<ocs::tom::CQ> cqueue,
                          const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   const auto href_list = cqueue_get_hostlist(cqueue, master_hgroup_list);

   if (cqueue) {
      // a copy named "", or nullptr for a queue without a group, as lCopyList() gave
      ocs::tom::set<CQ_hostlist>(cqueue, href_list.clone(""));
   }
}

/**
 * @brief Put the host list onto every cluster queue of a list
 *
 * @param cqueue_list CQ_Type list, modified in place; may be an empty handle
 * @param master_hgroup_list HGRP_Type list, as fetched from the qmaster
 *
 * @note MT-NOTE: cqueue_list_fill_hostlist() is MT safe
 */
void cqueue_list_fill_hostlist(const ocs::tom::List<ocs::tom::CQ> cqueue_list,
                               const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   for (const auto cqueue : cqueue_list) {
      cqueue_fill_hostlist(cqueue, master_hgroup_list);
   }
}


/**
 * @brief Is a host/hostgroup referenced in cqueue
 *
 * Is the given "href" (host or hostgroup referenece) used in the
 * definition of the cluster queue "this_elem"? If "only_hostlist"
 * is true then only the hostlist will be tested and all
 * parameter lists will be ignored.
 *
 * @param this_elem CQ_Type, may be an empty handle
 * @param href HR_Type, may be an empty handle
 * @param only_hostlist check only hostlist and ignore all parameter lists
 *
 * @return true if it is referenced
 */
bool cqueue_is_href_referenced(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                               const ocs::tom::ConstElement<ocs::tom::HR> href, const bool only_hostlist) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem && href) {
      const char *href_name = ocs::tom::get<HR_name>(href);
      
      if (href_name != nullptr) {
         /*
          * Is it the host group carrying this queue's host list ...
          *
          * CS-2677, N-T-5. This used to be a lookup in the queue's own host
          * list. There is none any more: the host list is the member list of
          * "@@<queue>", and no host list names that group -- its own included.
          * The relation is carried by the name, so the name is what is asked.
          */
         dstring group_buffer = DSTRING_INIT;
         const char *own_group = hgroup_queue_group_name(ocs::tom::get<CQ_name>(this_elem),
                                                         &group_buffer);

         if (own_group != nullptr && strcmp(own_group, href_name) == 0) {
            ret = true;
         }
         sge_dstring_free(&group_buffer);

         /*
          * ... or is it contained on one of the attribute lists
          */
         if (!only_hostlist && !ret) {
            ret = !ocs_for_each_cqueue_attr_while([this_elem, href_name](const auto &attr) {
               using E = ocs_cqueue_attr_t<decltype(attr)>;
               return !ocs::tom::get<E::cqueue_field>(this_elem).template find_first<E::href_field>(
                  ocs::tom::Host(href_name));
            });
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Is a hgroup referenced in cqueue
 *
 * Is the given "hgroup" object referenced in the cluster queue
 * "this_elem".
 *
 * @param this_elem CQ_Type, may be an empty handle
 * @param hgroup HGRP_Type, may be an empty handle
 *
 * @return true if "hgroup" is referenced
 */
static bool cqueue_is_hgroup_referenced(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                        const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem && hgroup) {
      const char *name = ocs::tom::get<HGRP_name>(hgroup);
      
      if (name != nullptr) {
         /*
          * Is it the host group carrying this queue's host list ... (N-T-5,
          * see cqueue_is_href_referenced() for why this is a name comparison)
          */
         dstring group_buffer = DSTRING_INIT;
         const char *own_group = hgroup_queue_group_name(ocs::tom::get<CQ_name>(this_elem),
                                                         &group_buffer);
         const bool is_own_group = (own_group != nullptr && strcmp(own_group, name) == 0);

         sge_dstring_free(&group_buffer);

         if (is_own_group) {
            ret = true;
         } else {
            /*
             * ... or is it contained on one of the attribute lists
             */
            ret = !ocs_for_each_cqueue_attr_while([this_elem, name](const auto &attr) {
               using E = ocs_cqueue_attr_t<decltype(attr)>;
               return !ocs::tom::get<E::cqueue_field>(this_elem).template find_first<E::href_field>(
                  ocs::tom::Host(name));
            });
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Is one href referenced
 *
 * Returns true if at least one host contained in "href_list" is
 * referenced in the cluster queue "this_elem". If "only_hostlist"
 * is true then only the hostlist will be tested and all
 * parameter lists will be ignored.
 *
 * @param this_elem CQ_Type object, may be an empty handle
 * @param href_list HR_Type list, may be an empty handle
 * @param only_hostlist check only hostlist and ignore all parameter lists
 *
 * @return at least one object is referenced
 */
bool cqueue_is_a_href_referenced(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                 const ocs::tom::ConstList<ocs::tom::HR> href_list, const bool only_hostlist) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem && href_list) {
      for (const auto href : href_list) {
         if (cqueue_is_href_referenced(this_elem, href, only_hostlist)) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}


/**
 * @brief Set an attribute list of a cluster queue to its default entry alone
 *
 * The list is created named "" and holds one entry for #HOSTREF_DEFAULT with
 * the given value, as the template of a new cluster queue wants it; a list
 * held before is freed.
 *
 * @tparam LF the attribute list field of the cluster queue (CQ_Type)
 * @tparam HREF the host reference field of the list's entry type
 * @tparam VALUE the value field of the list's entry type
 * @tparam V the value's type: a number, a string, or an owning list handle
 * @param cqueue the cluster queue
 * @param value the value of the default entry; an owning list handle is taken over
 */
template <auto LF, auto HREF, auto VALUE, typename V>
static void ocs_cqueue_set_default_attr(const ocs::tom::Element<ocs::tom::CQ> cqueue, V value) {
   DENTER(CQUEUE_LAYER);

   using Entry = typename ocs::tom::FieldTraits<LF>::Sub;
   auto attr_list = ocs::tom::ListPtr<Entry>::create("");
   const auto attr_elem = attr_list->template emplace_back<HREF>(ocs::tom::Host(HOSTREF_DEFAULT));

   attr_elem.template set<VALUE>(std::move(value));
   ocs::tom::set<LF>(cqueue, std::move(attr_list));
   DRETURN_VOID;
}

/**
 * @brief Set default attributes
 *
 * This function initializes all attributes of an empty cluster
 * queue with default values. Please note that "this_elem" has to
 * be "empty" before this function is called.
 *
 * @param this_elem CQ_Type, may be an empty handle
 * @param answer_list AN_Type, receives a message should the default queue type not parse
 *
 * @return error state true  - success false - error
 */
bool cqueue_set_template_attributes(const ocs::tom::Element<ocs::tom::CQ> this_elem,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (this_elem) {
      /*
       * initialize uint32_t values
       */
      ocs_cqueue_set_default_attr<CQ_seq_no, AULNG_href, AULNG_value>(this_elem, uint32_t{0});
      ocs_cqueue_set_default_attr<CQ_nsuspend, AULNG_href, AULNG_value>(this_elem, uint32_t{1});
      ocs_cqueue_set_default_attr<CQ_job_slots, AULNG_href, AULNG_value>(this_elem, uint32_t{1});

      /*
       * qtype
       */
      uint32_t qtype = 0;

      sge_parse_bitfield_str("BATCH INTERACTIVE", queue_types, &qtype, "", answer_list, true);
      ocs_cqueue_set_default_attr<CQ_qtype, AQTLIST_href, AQTLIST_value>(this_elem, qtype);

      /*
       * initialize bool values
       */
      ocs_cqueue_set_default_attr<CQ_rerun, ABOOL_href, ABOOL_value>(this_elem, false);

      /*
       * initialize memory values
       */
      ocs_cqueue_set_default_attr<CQ_s_fsize, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_fsize, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_s_data, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_data, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_s_stack, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_stack, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_s_core, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_core, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_s_rss, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_rss, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_s_vmem, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_vmem, AMEM_href, AMEM_value>(this_elem, INFINITY_STR);

      /*
       * initialize time values
       */
      ocs_cqueue_set_default_attr<CQ_s_rt, ATIME_href, ATIME_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_rt, ATIME_href, ATIME_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_s_cpu, ATIME_href, ATIME_value>(this_elem, INFINITY_STR);
      ocs_cqueue_set_default_attr<CQ_h_cpu, ATIME_href, ATIME_value>(this_elem, INFINITY_STR);

      /*
       * initialize interval values
       */
      ocs_cqueue_set_default_attr<CQ_suspend_interval, AINTER_href, AINTER_value>(this_elem, "00:05:00");
      ocs_cqueue_set_default_attr<CQ_min_cpu_interval, AINTER_href, AINTER_value>(this_elem, "00:05:00");
      ocs_cqueue_set_default_attr<CQ_notify, AINTER_href, AINTER_value>(this_elem, "00:00:60");

      /*
       * initialize string values
       */
      ocs_cqueue_set_default_attr<CQ_tmpdir, ASTR_href, ASTR_value>(this_elem, "/tmp");
      ocs_cqueue_set_default_attr<CQ_shell, ASTR_href, ASTR_value>(this_elem, "/bin/sh");
      ocs_cqueue_set_default_attr<CQ_calendar, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_priority, ASTR_href, ASTR_value>(this_elem, "0");
      ocs_cqueue_set_default_attr<CQ_processors, ASTR_href, ASTR_value>(this_elem, "UNDEFINED");
      ocs_cqueue_set_default_attr<CQ_prolog, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_epilog, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_shell_start_mode, ASTR_href, ASTR_value>(this_elem, "unix_behavior");
      ocs_cqueue_set_default_attr<CQ_starter_method, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_suspend_method, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_resume_method, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_terminate_method, ASTR_href, ASTR_value>(this_elem, "NONE");
      ocs_cqueue_set_default_attr<CQ_initial_state, ASTR_href, ASTR_value>(this_elem, "default");

      /*
       * initialize string-list values
       */
      auto pe_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");

      pe_list->emplace_back<ST_name>("make");
      ocs_cqueue_set_default_attr<CQ_pe_list, ASTRLIST_href, ASTRLIST_value>(this_elem, std::move(pe_list));
      ocs_cqueue_set_default_attr<CQ_ckpt_list, ASTRLIST_href, ASTRLIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::ST>());

      /*
       * initialize AUSRLIST_Type-list values
       */
      ocs_cqueue_set_default_attr<CQ_owner_list, AUSRLIST_href, AUSRLIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::US>());
      ocs_cqueue_set_default_attr<CQ_acl, AUSRLIST_href, AUSRLIST_value>(this_elem, ocs::tom::ListPtr<ocs::tom::US>());
      ocs_cqueue_set_default_attr<CQ_xacl, AUSRLIST_href, AUSRLIST_value>(this_elem, ocs::tom::ListPtr<ocs::tom::US>());

      /*
       * initialize APRJLIST_Type-list values (only sgeee mode)
       */
      ocs_cqueue_set_default_attr<CQ_projects, APRJLIST_href, APRJLIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::PR>());
      ocs_cqueue_set_default_attr<CQ_xprojects, APRJLIST_href, APRJLIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::PR>());

      /*
       * initialize ACELIST_Type-list values
       */
      auto load_thresholds = ocs::tom::ListPtr<ocs::tom::CE>::create("");
      const auto np_load_avg = load_thresholds->emplace_back<CE_name>("np_load_avg");

      np_load_avg.set<CE_stringval>("1.75");
      ocs_cqueue_set_default_attr<CQ_load_thresholds, ACELIST_href, ACELIST_value>(this_elem,
                                                                                   std::move(load_thresholds));
      ocs_cqueue_set_default_attr<CQ_suspend_thresholds, ACELIST_href, ACELIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::CE>());
      ocs_cqueue_set_default_attr<CQ_consumable_config_list, ACELIST_href, ACELIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::CE>());

      /*
       * initialize ASOLIST_Type-list values
       */
      ocs_cqueue_set_default_attr<CQ_subordinate_list, ASOLIST_href, ASOLIST_value>(
         this_elem, ocs::tom::ListPtr<ocs::tom::SO>());
   }
   DRETURN(ret);
}


/**
 * @brief Find a cluster queue in list
 *
 * Find the cluster queue with name "name" in the list "this_list".
 *
 * @param this_list CQ_Type list, may be an empty handle
 * @param name cluster queue name
 *
 * @return cluster queue object, or an empty handle
 */
ocs::tom::Element<ocs::tom::CQ> cqueue_list_locate(const ocs::tom::ConstList<ocs::tom::CQ> this_list,
                                                   const char *name) {
   return ocs::tom::find_first_rw<CQ_name>(this_list, name);
}


/**
 * @brief Returns one qinstance from a cqueue
 *
 * Finds the queue instance locateted on the host "hostname" of a
 * given cluster queue "this_elem".
 *
 * @param this_elem CQ_Type object
 * @param hostname resolved hostname
 *
 * @return qinstance object, or an empty handle
 */
ocs::tom::Element<ocs::tom::QU> cqueue_locate_qinstance(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                                        const char *hostname) {
   const auto qinstance_list = ocs::tom::get<CQ_qinstances>(this_elem);

   return qinstance_list_locate(qinstance_list, hostname, nullptr);
}


/**
 * @brief Check the host references of one attribute list of a cluster queue
 *
 * The list needs a default entry and at most one entry per host or host group. Host names are resolved and
 * replaced by the resolved name where that works; a host group must not be the group of a cluster queue and,
 * in qmaster, must exist.
 *
 * @tparam A the attribute type of the list
 * @param list the attribute list, may be an empty handle
 * @param answer_list AN_Type list, receives the reason of a rejection
 * @param attr_name the attribute's name, for the messages
 * @param in_master true if this function is called in the master code
 * @param master_hgroup_list the host groups
 * @return true if the list is acceptable
 */
template <OcsHostAttr A>
static bool
ocs_cqueue_verify_hrefs(const ocs::tom::List<A> list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const char *attr_name, const bool in_master,
                        const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(CQUEUE_LAYER);

   constexpr auto href_f = ocs_attr_traits<A>::href;
   bool ret = true;

   /*
    * Configurations without default setting are rejected
    */
   if (!list.template find_first<href_f>(ocs::tom::Host(HOSTREF_DEFAULT))) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_NODEFVALUE_S, attr_name);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /*
    * Reject multiple settings for one domain/host
    * Resolve all hostnames
    * Verify host group names
    */
   if (ret) {
      for (const auto elem : list) {
         const char *hostname = ocs::tom::get<href_f>(elem);
         const auto first_elem = list.template find_first<href_f>(ocs::tom::Host(hostname));

         if (elem != first_elem) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_MULVALNOTALLOWED_S, hostname);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         if (ocs::is_hgroup_name(hostname)) {
            /*
             * CS-2677, N-O-4: the host group of a cluster queue is
             * that queue's host set and belongs to nobody else.
             * Checked on both sides of GDI, and independently of
             * whether the group exists -- the name is reserved, so a
             * missing one is still not something to write.
             */
            if (hgroup_is_queue_group(hostname)) {
               ERROR(MSG_HGRP_QUEUEGROUP_NOREF_S, hostname);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
            if (in_master && strcmp(hostname, HOSTREF_DEFAULT)) {
               const auto hgroup = hgroup_list_locate(master_hgroup_list, hostname);

               if (!hgroup) {
                  ERROR(MSG_CQUEUE_INVALIDDOMSETTING_SS, attr_name, hostname);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  ret = false;
                  break;
               }
            }
         } else {
            char resolved_name[CL_MAXHOSTNAMELEN+1];
            int back = getuniquehostname(hostname, resolved_name, 0);

            if (back == CL_RETVAL_OK) {
               ocs::tom::set<href_f>(elem, static_cast<const char *>(resolved_name));
            } else {
               /*
                * Due to CR 6319231, IZ 1760 this is allowed
                */
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Check all cluster queue attributes
 *
 * Check all cluster queue settings (and correct them if possible).
 *    - test that there is exact one default setting
 *    - check that there is only one setting for used hgroups/hosts
 *    - resolve hostnames
 *    - test attribute values
 *
 * The attributes are walked through #ocs_cqueue_attributes; the verify function of an entry gets the entries of
 * the attribute list typed and the master list the entry names.
 *
 * @param cqueue CQ_Type object to be verified, may be an empty handle
 * @param answer_list AN_Type list
 * @param reduced_elem reduced CQ_Type. Contains only those attributes to be checked; may be an empty handle
 * @param in_master true if this function is called in the master code
 * @param master_calendar_list the calendars an attribute may refer to
 * @param master_ckpt_list the checkpointing environments an attribute may refer to
 * @param master_pe_list the parallel environments an attribute may refer to
 * @param master_userset_list the usersets an attribute may refer to
 * @param master_project_list the projects an attribute may refer to
 * @param master_centry_list the complex entries an attribute may refer to
 * @param master_cqueue_list the cluster queues an attribute may refer to
 * @param master_hgroup_list the host groups an attribute may refer to
 *
 * @return error state true  - success false - error
 */
bool cqueue_verify_attributes(const ocs::tom::Element<ocs::tom::CQ> cqueue,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              const ocs::tom::ConstElement<ocs::tom::CQ> reduced_elem, const bool in_master,
                              const ocs::tom::ConstList<ocs::tom::CAL> master_calendar_list,
                              const ocs::tom::ConstList<ocs::tom::CK> master_ckpt_list,
                              const ocs::tom::ConstList<ocs::tom::PE> master_pe_list,
                              const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                              const ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                              const ocs::tom::ConstList<ocs::tom::CE> master_centry_list,
                              const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                              const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (cqueue && reduced_elem) {
      // the master lists a verify function may check against, picked by the type its table entry names
      const std::tuple master_lists{master_calendar_list, master_ckpt_list, master_pe_list, master_userset_list,
                                    master_project_list, master_centry_list, master_cqueue_list};

      ocs_for_each_cqueue_attr_while([&](const auto &attr) {
         using E = ocs_cqueue_attr_t<decltype(attr)>;
         using M = typename E::master_type;

         if (ocs::tom::has<E::cqueue_field>(reduced_elem)) {
            const auto list = ocs::tom::get<E::cqueue_field>(cqueue);

            ret = ocs_cqueue_verify_hrefs(list, answer_list, attr.name, in_master, master_hgroup_list);

            /*
             * Call native verify function if it is possible
             */
            if (ret && attr.verify != nullptr && (attr.verify_client || in_master)) {
               for (const auto elem : list) {
                  if constexpr (std::is_void_v<M>) {
                     ret &= attr.verify(cqueue, answer_list, elem);
                  } else {
                     ret &= attr.verify(cqueue, answer_list, elem, std::get<ocs::tom::ConstList<M>>(master_lists));
                  }
               }
            }
         }
         return ret;
      });
   }
   DRETURN(ret);
}


/**
 * @brief Find the cluster queues whose name matches a pattern
 *
 * Find all cqueues in "this_list" where the cqueue name matches
 * the pattern "cqueue_pattern" and return their names as queue
 * references.
 *
 * @param this_list CQ_Type, may be an empty handle
 * @param answer_list AN_Type; not written, the pattern is evaluated without an answer list
 * @param cqueue_pattern fnmatch pattern
 *
 * @return QR_Type list of the matching names; an empty handle if nothing matched or an argument is missing
 *
 * @note MT-NOTE: cqueue_list_find_all_matching_references() is MT safe
 */
ocs::tom::ListPtr<ocs::tom::QR>
cqueue_list_find_all_matching_references(const ocs::tom::ConstList<ocs::tom::CQ> this_list,
                                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *cqueue_pattern) {
   DENTER(CQUEUE_LAYER);

   // created by the first match, as the old out-parameter was
   ocs::tom::ListPtr<ocs::tom::QR> qref_list;

   if (this_list && cqueue_pattern != nullptr) {
      const bool cqueue_pattern_is_expression = ocs::is_expression(cqueue_pattern);

      for (const auto cqueue : this_list) {
         const char *cqueue_name = ocs::tom::get<CQ_name>(cqueue);
         /* use cqueue expression */
         if (!sge_eval_expression(ocs::CEntry::Type::STR, cqueue_pattern, cqueue_name, nullptr, true,
                                  cqueue_pattern_is_expression)) {
            if (!qref_list) {
               qref_list = ocs::tom::ListPtr<ocs::tom::QR>::create("");
            }
            qref_list->emplace_back<QR_name>(cqueue_name);
         }
      }
   }
   DRETURN(qref_list);
}


/**
 * @brief Move the values of one attribute of one queue request into another of the same cluster queue
 *
 * The values are keyed by their host or domain reference; a value for a reference the target already holds is
 * a repeat and is dropped.
 *
 * @tparam CqF the attribute list field of the cluster queue
 * @param target the request that is kept
 * @param source the request whose values are moved; its attribute list is left empty
 */
template <auto CqF>
static void
ocs_cqueue_xattr_merge_attr(const ocs::tom::Element<ocs::tom::CQ> target,
                            const ocs::tom::Element<ocs::tom::CQ> source) {
   DENTER(CQUEUE_LAYER);

   using A = typename ocs::tom::FieldTraits<CqF>::Sub;
   constexpr auto href_f = ocs_attr_traits<A>::href;

   const auto src = ocs::tom::get<CqF>(source);
   if (!src || src.empty()) {
      DRETURN_VOID;
   }
   const auto dst = ocs::tom::get<CqF>(target);
   if (!dst) {
      auto moved = source.template exchange<CqF>(ocs::tom::ListPtr<A>());
      static_cast<void>(target.template exchange<CqF>(std::move(moved)));
      DRETURN_VOID;
   }
   while (const auto value = src.front()) {
      auto owned = src.extract(value);
      if (!dst.template find_first<href_f>(ocs::tom::get<href_f>(owned))) {
         dst.push_back(std::move(owned));
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Move the attribute values of one queue request into another of the same cluster queue
 *
 * The values of each attribute are keyed by their host or domain reference; a value for a reference the target
 * already holds is a repeat and is dropped. The attributes are walked through #ocs_cqueue_attributes.
 *
 * @param target the request that is kept
 * @param source the request whose values are moved; its attribute lists are left empty
 */
static void
ocs_cqueue_xattr_merge(const ocs::tom::Element<ocs::tom::CQ> target, const ocs::tom::Element<ocs::tom::CQ> source) {
   DENTER(CQUEUE_LAYER);

   ocs_for_each_cqueue_attr([target, source](const auto &attr) {
      ocs_cqueue_xattr_merge_attr<ocs_cqueue_attr_t<decltype(attr)>::cqueue_field>(target, source);
   });
   DRETURN_VOID;
}

/**
 * @brief Give the values of one attribute of a queue request the host or domain the request names
 *
 * Every value must be a default entry; it gets @p host_domain as its reference.
 *
 * @tparam A the attribute type of the list
 * @param list the attribute list of the request, may be an empty handle
 * @param answer_list AN_Type, receives an error for every value that is not a default entry
 * @param host_domain the host or host group the request names
 * @return true if every value was a default entry
 */
template <OcsHostAttr A>
static bool
ocs_cqueue_xattr_set_href(const ocs::tom::List<A> list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          const char *host_domain) {
   DENTER(CQUEUE_LAYER);

   constexpr auto href_f = ocs_attr_traits<A>::href;
   bool ret = true;

   for (const auto elem : list) {
      const char *attr_hostname = ocs::tom::get<href_f>(elem);

      if (strcmp(HOSTREF_DEFAULT, attr_hostname)) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_CQUEUE_NONDEFNOTALLOWED);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      } else {
         ocs::tom::set<href_f>(elem, host_domain);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Prepare the queue requests of `qconf -[admr]attr queue` for the GDI request
 *
 * Each named target is one request. A target that names a queue instance or a queue domain
 * (`queue@host`, `queue@@hgroup`) gets the cluster queue name, and its attribute values, which must be default
 * entries, get the host or domain as their reference. Targets of the same cluster queue are merged into one
 * request, so that the cluster queue name, a unique key, is not repeated in the list; their values are told apart
 * by their reference.
 *
 * Each request is taken out of the list while it is renamed and put back, or merged, afterwards, so that a
 * rename never puts a name into the list a second time.
 *
 * The signature is the one of the `pre_gdi_function` of qconf's attribute table, so the list arrives untyped.
 * The attribute lists are walked through #ocs_cqueue_attributes.
 *
 * @param this_list the requests, CQ_Type; checked, may be nullptr
 * @param answer_list AN_Type
 *
 * @return true on success, false on an error, described in @p answer_list
 *
 * @note MT-NOTE: cqueue_xattr_pre_gdi() is MT safe
 */
bool cqueue_xattr_pre_gdi(lList *this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;
   dstring cqueue_name = DSTRING_INIT;
   dstring host_domain = DSTRING_INIT;

   if (this_list != nullptr) {
      // qconf builds the requests as cluster queues, so the check passes
      const ocs::tom::List<ocs::tom::CQ> requests(this_list);

      // the requests that are not yet prepared are at the front, the prepared ones are appended at the end
      for (std::size_t remaining = requests.size(); remaining > 0 && ret; remaining--) {
         auto cqueue = requests.extract(requests.front());
         const char *name = cqueue->get<CQ_name>();
         bool has_hostname = false;
         bool has_domain = false;

         if (!cqueue_name_split(name, &cqueue_name, &host_domain,
                           &has_hostname, &has_domain)) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX,
               ANSWER_QUALITY_ERROR, MSG_CQUEUE_NOQMATCHING_S, name);
            requests.push_back(std::move(cqueue));
            ret = false;
            break;
         }

         if (has_domain || has_hostname) {
            /*
             * Change QI/QD name to CQ name
             */
            cqueue->set<CQ_name>(sge_dstring_get_string(&cqueue_name));

            /*
             * Make sure that there is only a default entry
             * and change that default entry to be a QD/QI entry
             */
            const auto request = *cqueue;
            const char *domain = sge_dstring_get_string(&host_domain);

            ret = ocs_for_each_cqueue_attr_while([request, answer_list, domain](const auto &attr) {
               using E = ocs_cqueue_attr_t<decltype(attr)>;

               // a request holds the attributes the administrator gave
               return !ocs::tom::has<E::cqueue_field>(request) ||
                      ocs_cqueue_xattr_set_href(ocs::tom::get<E::cqueue_field>(request), answer_list, domain);
            });
         }

         if (const auto target = requests.find_first<CQ_name>(cqueue->get<CQ_name>()); target) {
            ocs_cqueue_xattr_merge(target, *cqueue);
            cqueue.reset();
         } else {
            requests.push_back(std::move(cqueue));
         }
      }
   }

   sge_dstring_free(&host_domain);
   sge_dstring_free(&cqueue_name);
   DRETURN(ret);
}

/**
 * @brief Checks for cqueue references
 *
 * The function goes through all cq subordinate definition and looks
 * for the cq_name handed in. If it is found, the function will return
 * true.
 *
 * @param cqueue_name cq name to look for
 * @param cqueue cq to look in (CQ_Type), may be an empty handle
 *
 * @return true - a reference was found
 *
 * @note MT-NOTE: cqueue_is_used_in_subordinate() is MT safe
 */
bool cqueue_is_used_in_subordinate(const char *cqueue_name, const ocs::tom::ConstElement<ocs::tom::CQ> cqueue) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (cqueue && cqueue_name != nullptr) {
      for (const auto sub_el : ocs::tom::get<CQ_subordinate_list>(cqueue)) {
         const auto so = ocs::tom::find_first_in<ASOLIST_value, SO_name>(sub_el, cqueue_name);

         if (so) { /* we found a reference */
            ret = true;
            break;
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief Find hgroup references
 *
 * Collects the name of each cqueue contained in "this_list"
 * where "hgroup" is referenced.
 *
 * @param this_list CQ_Type, may be an empty handle
 * @param answer_list AN_Type; not written
 * @param hgroup HGRP_Type, may be an empty handle
 *
 * @return ST_Type list of the names, created by the first one; an empty handle if none references the group
 *
 * @note MT-NOTE: cqueue_list_find_hgroup_references() is MT safe
 */
ocs::tom::ListPtr<ocs::tom::ST>
cqueue_list_find_hgroup_references(const ocs::tom::ConstList<ocs::tom::CQ> this_list,
                                   [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup) {
   DENTER(CQUEUE_LAYER);

   ocs::tom::ListPtr<ocs::tom::ST> string_list;

   if (this_list && hgroup) {
      for (const auto cqueue : this_list) {
         // a cqueue without a name is skipped, as the free emplace_back() took the soft path for it
         const char *name = ocs::tom::get<CQ_name>(cqueue);
         if (name != nullptr && cqueue_is_hgroup_referenced(cqueue, hgroup)) {
            if (!string_list) {
               string_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
            }
            string_list->emplace_back<ST_name>(name);
         }
      }
   }
   DRETURN(string_list);
}

/**
 * @brief Tags each cqueue and optionally qinstance
 *
 * Tags all cqueues contained in "this_list" with the value
 * "tag_value". Optionally all qinstances contained in the
 * CQ_qinstances-sublist will be tagged too if "tag_qinstances"
 * is true.
 *
 * @param this_list CQ_Type, may be an empty handle
 * @param tag_value value
 * @param tag_qinstances true if instances should be tagged
 *
 * @note MT-NOTE: cqueue_list_set_tag() is MT safe
 */
void cqueue_list_set_tag(const ocs::tom::List<ocs::tom::CQ> this_list, const uint32_t tag_value,
                         const bool tag_qinstances) {
   DENTER(TOP_LAYER);
   for (const auto cqueue : this_list) {
      ocs::tom::set<CQ_tag>(cqueue, tag_value);
      if (tag_qinstances) {
         const auto qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);

         qinstance_list_set_tag(qinstance_list, tag_value);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Finds a certain qinstance
 *
 * Returns a certain qinstance with the name "full_name" from
 * the master cqueue list given by "cqueue_list".
 *
 * @param cqueue_list CQ_Type, may be an empty handle
 * @param full_name qinstance name of the form `CQNAME@HOSTNAME`
 *
 * @return QU_Type element, or an empty handle
 *
 * @note MT-NOTE: cqueue_list_locate_qinstance() is MT safe
 */
ocs::tom::Element<ocs::tom::QU> cqueue_list_locate_qinstance(const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                                             const char *full_name) {
   return cqueue_list_locate_qinstance_msg(cqueue_list, full_name, true);
}


/**
 * @brief Finds a certain qinstance
 *
 * Returns a certain qinstance with the name "full_name" from
 * the master cqueue list given by "cqueue_list".
 *
 * @param cqueue_list CQ_Type, may be an empty handle
 * @param full_name qinstance name of the form `CQNAME@HOSTNAME`
 * @param raise_error true - show error messages - false - suppress error messages
 *
 * @return QU_Type element, or an empty handle
 *
 * @note MT-NOTE: cqueue_list_locate_qinstance_msg() is MT safe
 */
ocs::tom::Element<ocs::tom::QU> cqueue_list_locate_qinstance_msg(const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                                                 const char *full_name, const bool raise_error) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::QU> ret = nullptr;

   if (full_name != nullptr) {
      dstring cqueue_name_buffer = DSTRING_INIT;
      dstring host_domain_buffer = DSTRING_INIT;
      bool has_hostname = false;
      bool has_domain = false;

      cqueue_name_split(full_name, &cqueue_name_buffer, &host_domain_buffer, &has_hostname, &has_domain);
      const char *cqueue_name = sge_dstring_get_string(&cqueue_name_buffer);
      const char *hostname = sge_dstring_get_string(&host_domain_buffer);
      const auto cqueue = ocs::tom::find_first<CQ_name>(cqueue_list, cqueue_name);
      if (cqueue) {
         const auto qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);

         ret = ocs::tom::find_first_rw<QU_qhostname>(qinstance_list, hostname);
      } else {
         if (raise_error) {
            ERROR(MSG_CQUEUE_CQUEUEISNULL_SSSII, full_name, cqueue_name != nullptr ? cqueue_name : "<null>",
                  hostname != nullptr ? hostname : "<null>", (int)has_hostname, (int)has_domain);
         }
      }
      sge_dstring_free(&cqueue_name_buffer);
      sge_dstring_free(&host_domain_buffer);
   } else {
      if (raise_error) {
         ERROR(SFNMAX, MSG_CQUEUE_FULLNAMEISNULL);
      }
   }
   DRETURN(ret);
}


/**
 * @brief Purge attributes from queue
 *
 * The attribute lists other than the host list are walked through
 * #ocs_cqueue_attributes.
 *
 * @param this_elem cqueue list element (CQ_Type), may be an empty handle
 * @param answer_list answer list
 * @param attr_list US_Type list with attributes to search for hgroup_or_hostname wildcards allowed
 * @param hgroup_or_hostname host group or hostname which should be purged
 *
 * @return true when hgroup_or_hostname was found and purged, false when no matching attribute
 *         holds it; an attribute without a list counts as not found (CS-3117)
 *
 * @note MT-NOTE: cqueue_purge_host() is not MT safe
 */
bool cqueue_purge_host(const ocs::tom::Element<ocs::tom::CQ> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       const ocs::tom::ConstList<ocs::tom::US> attr_list, const char *hgroup_or_hostname) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem) {
      for (const auto ep : attr_list) {
         const char *attr_name = ocs::tom::get<US_name>(ep);
         DPRINTF(SFQ "\n", attr_name);
      
         /* purge hostlist */         
         if (!sge_eval_expression(ocs::CEntry::Type::HOST, attr_name, SGE_ATTR_HOSTLIST, nullptr)) {
            auto hostlist = this_elem.exchange<CQ_hostlist>(ocs::tom::ListPtr<ocs::tom::HR>());
            // a missing list or name is no deletion
            if (hostlist && hgroup_or_hostname != nullptr &&
                hostlist->erase<HR_name>(ocs::tom::Host(hgroup_or_hostname)) == 1) {
               DPRINTF(SFQ " deleted in " SFQ "\n", hgroup_or_hostname, SGE_ATTR_HOSTLIST);
               ret = true;
               // a list emptied by the removal is dropped, as the old removal freed it
               if (hostlist->empty()) {
                  hostlist.reset();
               }
            }
            static_cast<void>(this_elem.exchange<CQ_hostlist>(std::move(hostlist)));
         }
         
         /* purge attributes */ 
         ocs_for_each_cqueue_attr([this_elem, attr_name, hgroup_or_hostname, &ret](const auto &attr) {
            using E = ocs_cqueue_attr_t<decltype(attr)>;

            /* Does the given attr_wildcard match with the actual attr_name */
            if (!sge_eval_expression(ocs::CEntry::Type::STR, attr_name, attr.name, nullptr)) {
               // The list is taken out of the element and put back like the host list above, so the element
               // never points at a list freed on the way (CS-3117).
               auto sublist = this_elem.template exchange<E::cqueue_field>(
                  ocs::tom::ListPtr<typename E::attr_type>());
               // a missing list or name is no deletion
               if (sublist && hgroup_or_hostname != nullptr &&
                   sublist->template erase<E::href_field>(ocs::tom::Host(hgroup_or_hostname)) == 1) {
                  DPRINTF(SFQ " deleted in " SFQ "\n", hgroup_or_hostname, attr.name);
                  ret = true;
                  // a list emptied by the removal is dropped, as the old removal freed it
                  if (sublist->empty()) {
                     sublist.reset();
                  }
               }
               static_cast<void>(this_elem.template exchange<E::cqueue_field>(std::move(sublist)));
            }
         });
      }
   }

   DRETURN(ret);
}


/**
 * @brief Report the settings of one attribute list of a cluster queue that can never take effect
 *
 * A host group setting that does not cover a host of the queue's host list, and a host setting for a host
 * that is not in it, are reported.
 *
 * @tparam A the attribute type of the list
 * @param attr_list the attribute list, may be an empty handle
 * @param attr_name the attribute's name, for the report
 * @param cqueue_name the cluster queue's name, for the report
 * @param used_hosts the hosts of the queue's host list, resolved
 * @param answer_list receives error messages, may be nullptr
 * @param master_hgroup_list the host groups
 * @param[out] ds receives the human readable report, appended
 */
template <OcsHostAttr A>
static void
ocs_cqueue_sick_attr(const ocs::tom::ConstList<A> attr_list, const char *attr_name, const char *cqueue_name,
                     const ocs::tom::ConstList<ocs::tom::HR> used_hosts, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                     const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *ds) {
   DENTER(TOP_LAYER);

   /*
    * Test each attribute setting if it is really used in the
    * current configuration
    */
   for (const auto attr : attr_list) {
      const char *name = ocs::tom::get<ocs_attr_traits<A>::href>(attr);

      if (ocs::is_hgroup_name(name)) {
         if (strcmp(name, HOSTREF_DEFAULT)) {
            ocs::tom::ListPtr<ocs::tom::HR> used_hgroup_hosts;
            ocs::tom::ListPtr<ocs::tom::HR> used_hgroup_groups;
            ocs::tom::ListPtr<ocs::tom::HR> add_hosts;
            ocs::tom::ListPtr<ocs::tom::HR> equity_hosts;

            const auto hgroup = hgroup_list_locate(master_hgroup_list, name);

            /*
             * hgroup specific setting:
             *    make sure each host of hgroup is part of 
             *    resolved list
             */
            hgroup_find_all_references(hgroup, answer_list, master_hgroup_list, &used_hgroup_hosts,
                                       &used_hgroup_groups);
            href_list_compare(*used_hgroup_hosts, answer_list, used_hosts, &add_hosts, nullptr, &equity_hosts,
                              nullptr);

            if (!add_hosts->empty()) {
               DTRACE;
               sge_dstring_sprintf_append(ds, MSG_CQUEUE_DEFOVERWRITTEN_SSSSS, attr_name, name, cqueue_name, name,
                                          cqueue_name);
               sge_dstring_append(ds, "\n");
            }
         } else {
            DTRACE;
         }
      } else {
         /*
          * host specific setting:
          *    make sure the host is contained in resolved list 
          */ 
         if (!href_list_has_member(used_hosts, name)) {
            DTRACE;
            sge_dstring_sprintf_append(ds, MSG_CQUEUE_UNUSEDATTRSETTING_SS, attr_name, name, cqueue_name);
            sge_dstring_append(ds, "\n");
         } else {
            DTRACE;
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Report configuration settings that look wrong but are not errors
 *
 * Backs `qconf -sick`. Warns about attribute settings that can never take
 * effect and about host group settings that do not cover every host in the
 * queue's host list.
 *
 * The attribute lists are walked through #ocs_cqueue_attributes.
 *
 * @param cqueue the cluster queue to inspect (CQ_Type)
 * @param[out] answer_list receives error messages
 * @param master_hgroup_list the host groups, needed to resolve the host list
 * @param[out] ds receives the human readable report
 * @return true when the queue could be inspected
 */
bool cqueue_sick(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                 const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *ds) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /*
    * Warn about:
    *    - unused setting for attributes
    *    - hgroup settings were not all hosts are contained in hostlist
    */
   {
      const char *cqueue_name = ocs::tom::get<CQ_name>(cqueue);
      ocs::tom::ListPtr<ocs::tom::HR> used_hosts;
      ocs::tom::ListPtr<ocs::tom::HR> used_groups;
      ocs::tom::ListPtr<ocs::tom::AN> *answer_list = nullptr;
      
      /*
       * resolve href list of cqueue
       */
      href_list_find_all_references(ocs::tom::get<CQ_hostlist>(cqueue), answer_list,
                                    master_hgroup_list, &used_hosts,
                                    &used_groups);

      const ocs::tom::ConstList<ocs::tom::HR> resolved_hosts = *used_hosts;
      ocs_for_each_cqueue_attr([&](const auto &attr) {
         ocs_cqueue_sick_attr(ocs::tom::get<ocs_cqueue_attr_t<decltype(attr)>::cqueue_field>(cqueue), attr.name,
                              cqueue_name, resolved_hosts, answer_list, master_hgroup_list, ds);
      });
   }
   
   DRETURN(ret);
}
