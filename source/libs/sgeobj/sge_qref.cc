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
 * @brief Queue references: `cqueue`, `@hostgroup` or `cqueue@host`
 *
 * Wherever a configuration or a request names queues, it does so through
 * these. Resolving one means expanding host groups and bringing host names
 * into their unique form.
 *
 * @see sge_qref.h
 */

#include <cstring>
#include <string>
#include <unordered_set>

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "comm/commlib.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "tom/ocs_List.h"

#include <cinttypes>
#include "msg_common.h"
#include "msg_clients_common.h"

/// Debug layer the queue reference traces are written to
#define QREF_LAYER TOP_LAYER


/**
 * @brief Resolve cluster queue references into cluster queue or queue instance references
 *
 * @param cq_qref_list QR list of cluster queue names, may be an empty handle
 * @param answer_list AN list
 * @param qref_list in-out: QR list receiving the references
 * @param found_something set to true when a queue instance reference was added
 * @param cqueue_list CQ master list
 * @param resolve_cqueue true to resolve each cluster queue into its queue instances
 *
 * @return always true
 */
static bool
qref_list_resolve_cqueue_names(const ocs::tom::ConstList<ocs::tom::QR> cq_qref_list,
                               ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lList **qref_list,
                               bool *found_something,
                               const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                               const bool resolve_cqueue) {
   DENTER(QREF_LAYER);
   bool ret = true;

   for (const auto cq_qref : cq_qref_list) {
      const char *cq_name = cq_qref.get<QR_name>();

      if (resolve_cqueue) {
         const auto cqueue = cqueue_list.find_first<CQ_name>(cq_name);
         const auto qinstance_list = cqueue.get<CQ_qinstances>();
         for (const auto qinstance : qinstance_list) {
            ocs::tom::emplace_back<QR_name>(qref_list, qinstance.get<QU_full_name>());
            *found_something = true;
         }
      } else {
         ocs::tom::emplace_back<QR_name>(qref_list, cq_name);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Resolve a host pattern of a queue reference into queue instance references
 *
 * @param cq_qref_list QR list of cluster queue names, may be an empty handle
 * @param answer_list AN list
 * @param host_or_hgroup the host pattern
 * @param qref_list in-out: QR list receiving the references
 * @param found_something set to true when a reference was added
 * @param cqueue_list CQ master list
 *
 * @return always true
 */
static bool
qref_list_resolve_qinstance_names(const ocs::tom::ConstList<ocs::tom::QR> cq_qref_list,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *host_or_hgroup,
                                  lList **qref_list,
                                  bool *found_something,
                                  const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list) {
   DENTER(QREF_LAYER);
   bool ret = true;

   for (const auto cq_qref : cq_qref_list) {
      const char *cqueue_name = cq_qref.get<QR_name>();
      const auto cqueue = cqueue_list.find_first<CQ_name>(cqueue_name);
      const char *hostname_pattern = sge_dstring_get_string(host_or_hgroup);
      const auto qinstance_list = cqueue.get<CQ_qinstances>();
      const auto qi_ref_list = qinstance_list_find_matching(qinstance_list, answer_list, hostname_pattern);

      for (const auto qi_qref : *qi_ref_list) {
         const char *qi_name = qi_qref.get<QR_name>();

         ocs::tom::emplace_back<QR_name>(qref_list, qi_name);
         *found_something = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Resolve a host group pattern of a queue reference into queue references
 *
 * Finds the host groups of @p hgroup_list matching the pattern in
 * @p host_or_hgroup and adds, for every cluster queue of @p cq_qref_list,
 * either the queue instance on each host the groups resolve to
 * (@p resolve_qdomain) or one `<cqueue>@<hgroup>` reference per group.
 *
 * @param cq_qref_list QR list of cluster queue names, may be an empty handle
 * @param answer_list AN list
 * @param host_or_hgroup the host group pattern
 * @param qref_list in-out: QR list receiving the references
 * @param found_something set to true when a reference was added
 * @param cqueue_list CQ master list
 * @param hgroup_list HGRP master list
 * @param resolve_qdomain true to resolve the groups into queue instances
 *
 * @return always true
 */
static bool
qref_list_resolve_qdomain_names(const ocs::tom::ConstList<ocs::tom::QR> cq_qref_list,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *host_or_hgroup,
                                lList **qref_list,
                                bool *found_something,
                                const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list,
                                const bool resolve_qdomain) {
   DENTER(QREF_LAYER);

   bool ret = true;
   // receives the host group search result, created with the first match
   ocs::tom::ListPtr<ocs::tom::HR> href_list;
   dstring buffer = DSTRING_INIT;

   const char *hgroup_pattern = sge_dstring_get_string(host_or_hgroup);
   /*
    * Find all hostgroups which match 'hgroup_pattern'
    * Possibly resolve them.
    */
   if (resolve_qdomain) {
      hgroup_list_find_matching_and_resolve(hgroup_list, answer_list, hgroup_pattern, &href_list);
   } else {
      hgroup_list_find_matching(hgroup_list, answer_list, hgroup_pattern, &href_list);
   }
   for (const auto cq_qref : cq_qref_list) {
      const char *cqueue_name = cq_qref.get<QR_name>();

      const auto cqueue = cqueue_list.find_first<CQ_name>(cqueue_name);
      const auto qinstance_list = cqueue.get<CQ_qinstances>();
      for (const auto href : *href_list) {
         if (resolve_qdomain) {
            const char *hostname = href.get<HR_name>();
            const auto qinstance = qinstance_list.find_first<QU_qhostname>(ocs::tom::Host(hostname));

            if (qinstance) {
               const char *qinstance_name = qinstance_get_name(qinstance, &buffer);

               ocs::tom::emplace_back<QR_name>(qref_list, qinstance_name);
               *found_something = true;
            }
         } else {
            const char *hgroup_name = href.get<HR_name>();
            const char *qinstance_name = sge_dstring_sprintf(&buffer, SFN "@" SFN, cqueue_name, hgroup_name);

            ocs::tom::emplace_back<QR_name>(qref_list, qinstance_name);
            *found_something = true;
         }
      }
   }
   sge_dstring_free(&buffer);
   DRETURN(ret);
}

/**
 * @brief Add a queue reference to the list
 *
 * Add the queue reference "qref_string" to the QR_type list "this_list",
 * unless the list already holds it (CS-3053).
 * Errors will be reported via return value and "answer_list".
 *
 * @param this_list in-out: the QR list, created if *this_list is nullptr
 * @param answer_list AN
 * @param qref_string queue reference
 *
 * @return error state true  - success false - error
 */
bool qref_list_add(lList **this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *qref_string) {
   DENTER(QREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && qref_string != nullptr) {
      if (ocs::tom::find_first<QR_name>(*this_list, qref_string)) {
         DRETURN(ret);
      }

      const auto new_elem = ocs::tom::emplace_back<QR_name>(this_list, qref_string);
      if (!new_elem) {
         answer_list_add(answer_list, MSG_GDI_OUTOFMEMORY,
                         STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT,
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Resolves a list of queue reference patterns
 *
 * Resolves a list of queue reference patterns. "src_qref_list"
 * is the list of input patterns to be resolved. "qref_list" contains
 * all resolved names which matched at least one of the given
 * patterns. The master lists "cqueue_list" and "hgroup_list" are
 * needed to resolve the pattern. "resolve_cqueue" and
 * "resolve_qdomain" can be used to define how qreferenes are
 * resolved.
 * Examples:
 *    <CQ-pattern> (e.g. "*")
 *       resolve_cqueue is false
 *          => cq1 cq2
 *       resolve_cqueue is true
 *          => `cq1@hostA1` `cq1@hostA2` `cq1@hostB1` `cq1@hostB2`
 *             `cq2@hostA1` `cq2@hostA2` `cq2@hostB1` `cq2@hostB2`
 *    <QD-pattern> (e.q "`*@@hgrp*`")
 *       resolve_qdomain is false
 *          => `cq1@@hgrpA` `cq1@@hgrpB` `cq2@@hgrpA` `cq2@@hgrpB`
 *       resolve_qdomain is true
 *          => `cq1@hostA1` `cq1@hostA2` `cq1@hostB1` `cq1@hostB2`
 *             `cq2@hostA1` `cq2@hostA2` `cq2@hostB1` `cq2@hostB2`
 *    <QI-pattern> (e.g "`cq*@host`?1")
 *          => `cq1@hostA1` `cq1@hostB1`
 *             `cq2@hostA1` `cq2@hostB1`
 *
 * @param src_qref_list QR list (input: pattern), may be an empty handle
 * @param answer_list AN list
 * @param qref_list in-out: QR list (output: resolved qrefs)
 * @param found_something a pattern matched? (output; untouched for an empty @p src_qref_list)
 * @param cqueue_list master CQ list
 * @param hgroup_list master HGRP list
 * @param resolve_cqueue resolve cqueue pattern?
 * @param resolve_qdomain resolve qdomain pattern?
 *
 * @return error state true  - success false - error
 */
bool qref_list_resolve(const ocs::tom::ConstList<ocs::tom::QR> src_qref_list,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lList **qref_list, bool *found_something,
                       const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                       const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list,
                       const bool resolve_cqueue, const bool resolve_qdomain) {
   DENTER(QREF_LAYER);

   bool ret = true;
   dstring cqueue_name = DSTRING_INIT;
   dstring host_or_hgroup = DSTRING_INIT;

   if (src_qref_list) {
      *found_something = false;
      for (const auto qref_pattern : src_qref_list) {
         bool has_hostname;
         bool has_domain;
         bool tmp_found_something = false;

         /*
          * Find all existing parts of the qref-pattern
          */
         const char *name = qref_pattern.get<QR_name>();
         if (!cqueue_name_split(name, &cqueue_name, &host_or_hgroup,
                           &has_hostname, &has_domain)) {
            /* we've got an syntax error */
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX,
               ANSWER_QUALITY_ERROR, MSG_CQUEUE_NOQMATCHING_S, name);
            continue;
         }
         const char *cq_pattern = sge_dstring_get_string(&cqueue_name);

         const auto cq_ref_list = cqueue_list_find_all_matching_references(cqueue_list, answer_list, cq_pattern);

         /*
          * Depending on the type of pattern -> resolve QC or QI names
          */
         if (has_domain) {
            ret &= qref_list_resolve_qdomain_names(*cq_ref_list, answer_list,
                                                   &host_or_hgroup, qref_list,
                                                   &tmp_found_something,
                                                   cqueue_list, hgroup_list,
                                                   resolve_qdomain);
         } else if (has_hostname) {
            ret &= qref_list_resolve_qinstance_names(*cq_ref_list, answer_list,
                                                     &host_or_hgroup, qref_list,
                                                     &tmp_found_something,
                                                     cqueue_list);
         } else {
            ret &= qref_list_resolve_cqueue_names(*cq_ref_list, answer_list,
                                                  qref_list,
                                                  &tmp_found_something,
                                                  cqueue_list,
                                                  resolve_qdomain);
         }
         if (tmp_found_something) {
            *found_something = true;
         }
      }
   }

   sge_dstring_free(&host_or_hgroup);
   sge_dstring_free(&cqueue_name);
   DRETURN(ret);
}


/**
 * @brief Check, if -q qref_list rejects (cluster) queue
 *
 * Check if patter in -q qref_list rejects cluster queue and hostname,
 * if passed. If nullptr is passed as hostname, cluster queue verfication
 * is performed only.
 *
 * @param qref_pattern a wildcard pattern as defined for -q qref_list
 * @param cqname cluster queue name
 * @param hostname execution hostname (may be nullptr)
 * @param hgroup_list host group list (HGRP), may be an empty handle when hostname is nullptr
 *
 * @return true if rejected
 *
 * @note MT-NOTE: qref_cq_rejected() is MT safe
 */
bool qref_cq_rejected(const char *qref_pattern, const char *cqname,
                      const char *hostname, const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list) {
   DENTER(TOP_LAYER);

   const char *s;

   if ((s=strchr(qref_pattern, '@'))) {
      /* use qref part before '@' as wc_cqueue pattern */
      int boo;
      char *wc_cqueue = strdup(qref_pattern);
      wc_cqueue[ s - qref_pattern ] = '\0';
      /* reject the cluster queue expression support */
      boo = sge_eval_expression(ocs::CEntry::Type::STR, wc_cqueue, cqname, nullptr);
      sge_free(&wc_cqueue);
      if (!boo) {
         if (!hostname || !qref_list_host_rejected(&s[1], hostname, hgroup_list)) {
            DRETURN(false);
         }
      }
   } else {
      /* use entire qref as wc_queue */
     /* cqueue expression support */
      if (!sge_eval_expression(ocs::CEntry::Type::STR, qref_pattern, cqname, nullptr)) {
         DRETURN(false);
      }
   }

   DRETURN(true);
}


/**
 * @brief Check, if a -q pattern rejects an execution host
 *
 * A pattern without a host part names no host and so rejects none.
 *
 * @param qref_pattern a wildcard pattern as defined for -q qref_list
 * @param hostname exeuction hostname
 * @param hgroup_list host group list (HGRP)
 *
 * @return true if rejected
 *
 * @note MT-NOTE: qref_eh_rejected() is MT safe
 */
static bool
qref_eh_rejected(const char *qref_pattern, const char *hostname,
                 const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list) {
   DENTER(TOP_LAYER);

   const char *s;

   if (!(s=strchr(qref_pattern, '@'))) {
      DRETURN(false);
   }

   if (!qref_list_host_rejected(&s[1], hostname, hgroup_list)) {
       DRETURN(false);
   }

   DRETURN(true);
}

/**
 * @brief Does a queue reference list exclude a host entirely?
 *
 * Used to skip a host early: when no entry of the list can match any queue on
 * it, the scheduler need not look at its queues at all.
 *
 * @param qref_list the queue references to check (QR), may be an empty handle
 * @param hostname the host in question
 * @param hgroup_list the host groups, needed to resolve a group reference (HGRP)
 * @return true when the host is rejected by all of them
 */
bool qref_list_eh_rejected(const ocs::tom::ConstList<ocs::tom::QR> qref_list, const char *hostname,
                           const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list) {
   DENTER(TOP_LAYER);

   if (hostname == nullptr) {
      DRETURN(true);
   }

   if (!qref_list) {
      DRETURN(false);
   }

   for (const auto qref_pattern : qref_list) {
      const char *name = qref_pattern.get<QR_name>();
      if (!qref_eh_rejected(name, hostname, hgroup_list)) {
         DRETURN(false);
      }
   }

   DRETURN(true);
}


/**
 * @brief Does a queue reference list exclude one cluster queue on one host?
 *
 * @param qref_list the queue references to check (QR), may be an empty handle
 * @param cqname the cluster queue in question
 * @param hostname the host in question; nullptr checks the cluster queue alone
 * @param hgroup_list the host groups, needed to resolve a group reference (HGRP)
 * @return true when the queue is rejected by all of them
 */
bool qref_list_cq_rejected(const ocs::tom::ConstList<ocs::tom::QR> qref_list, const char *cqname,
                           const char *hostname, const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list) {
   DENTER(TOP_LAYER);

   if (cqname == nullptr) {
      DRETURN(true);
   }

   if (!qref_list) {
      DRETURN(false);
   }

   for (const auto qref_pattern : qref_list) {
      const char *name = qref_pattern.get<QR_name>();
      if (!qref_cq_rejected(name, cqname, hostname, hgroup_list)) {
         DRETURN(false);
      }
   }

   DRETURN(true);
}


/**
 * @brief Check if a hostgroup contains a host
 *
 * Checks if "hostname" is a member of "hgroup", following nested
 * hostgroup references.
 * Members are resolved *exactly*, never as patterns. sge_types(1) defines
 * the content of a hostlist as
 *    host_identifier := host_name | hostgroup_name
 * - there is no wildcard type on this side. Wildcard types (wc_host,
 * wc_hostgroup, expression) are defined for the reference side only, i.e.
 * for "-q wc_qdomain" and the RQS "hosts {...}" scopes.
 * This mirrors href_list_find_references(), which is what
 * "qconf -shgrp_resolved" resolves with. Before CS-2450 the members were
 * passed back into qref_list_host_rejected() and thus matched as
 * expressions against *all* hostgroups, so the same configuration resolved
 * to different host sets depending on the code path.
 * Cycles cannot occur: sge_hgroup_qmaster.cc rejects them when a hostgroup
 * is added.
 *
 * @param hgroup hostgroup to search (HGRP)
 * @param hostname the host in question
 * @param hgroup_list hostgroup list (HGRP)
 *
 * @return True if the host is not a member.
 *
 * @note MT-NOTE: qref_hgroup_rejected() is MT safe
 *
 * @see #href_list_find_references
 */
static bool
qref_hgroup_rejected(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname,
                     const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list) {
   DENTER(BASIS_LAYER);

   /*
    * CS-2451: this is the hot path -- called per scope entry, per rule, per RQS,
    * per queue instance, per job. When qmaster has resolved the group, the whole
    * nested tree is already flattened in HGRP_cached_hosts and the walk below
    * collapses into one hash lookup.
    *
    * The two answer identically by construction: the cache is built by
    * hgroup_find_all_references(), which flattens the same tree, skips the same
    * unresolvable group references, and stores only hosts -- and the lookup uses
    * the same host comparison semantics as sge_hostcmp() (see
    * hgroup_cache_contains_host()).
    *
    * No cache -> fall through to the walk. Correctness never depends on this.
    */
   if (hgroup_has_host_cache(hgroup)) {
      DRETURN(!hgroup_cache_contains_host(hgroup, hostname));
   }

   for (const auto href : hgroup.get<HGRP_host_list>()) {
      const char *member = href.get<HR_name>();

      if (ocs::is_hgroup_name(member)) {
         /* nested hostgroup - look it up by name, do not match it as a pattern */
         const auto sub_hgroup = hgroup_list_locate(hgroup_list, member);

         if (sub_hgroup && !qref_hgroup_rejected(sub_hgroup, hostname, hgroup_list)) {
            DRETURN(false);
         }
      } else {
         /* stored host name - compare it, do not match it as a pattern.
          * sge_eval_expression() uses sge_hostcmp() for a pattern-free
          * Type::HOST value anyway, so this is the same comparison. */
         if (sge_hostcmp(member, hostname) == 0) {
            DRETURN(false);
         }
      }
   }

   DRETURN(true);
}

/**
 * @brief Check if -q ??`@href` rejects host
 *
 * Checks if a -q ??`@href` rejects host. The href may be either
 * wc_hostgroup or wc_host.
 * "href" is the *reference* side, so expression semantics apply to it.
 * The members of a matched hostgroup are resolved exactly by
 * qref_hgroup_rejected() (CS-2450).
 * A reference without pattern characters is looked up via
 * hgroup_list_locate(), i.e. a hash lookup on HGRP_name instead of a scan
 * of the whole master hostgroup list. That is the common case and this
 * function runs per RQS rule, per queue instance, per job.
 *
 * @param href Host reference from -q ??`@href`
 * @param hostname the host in question
 * @param hgroup_list hostgroup list (HGRP), may be an empty handle
 *
 * @return True if rejected.
 *
 * @note MT-NOTE: qref_list_host_rejected() is MT safe
 */
bool qref_list_host_rejected(const char *href, const char *hostname,
                             const ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list) {
   DENTER(BASIS_LAYER);

   if (ocs::is_hgroup_name(href)) { /* wc_hostgroup */
      const char *wc_hostgroup = &href[1];

      if (ocs::is_expression(wc_hostgroup)) {
         /* the reference is an expression - evaluate it against every hostgroup name */
         for (const auto hgroup : hgroup_list) {
            const char *hgroup_name = hgroup.get<HGRP_name>();

            DPRINTF("found hostgroup \"%s\" wc_hostgroup: \"%s\"\n", hgroup_name, wc_hostgroup);

            if (sge_eval_expression(ocs::CEntry::Type::HOST, wc_hostgroup, &hgroup_name[1], nullptr, true, true) == 0) {
               if (!qref_hgroup_rejected(hgroup, hostname, hgroup_list)) {
                  DRETURN(false);
               }
            }
         }
      } else {
         /* plain hostgroup name - hash lookup instead of scanning the list */
         const auto hgroup = hgroup_list_locate(hgroup_list, href);

         if (hgroup && !qref_hgroup_rejected(hgroup, hostname, hgroup_list)) {
            DRETURN(false);
         }
      }
   } else { /* wc_host */
      /* use host expression */
      if (sge_eval_expression(ocs::CEntry::Type::HOST, href, hostname, nullptr)==0) {
            DRETURN(false);
      }
   }

   DPRINTF("-q ?@%s rejected by \"%s\"\n", hostname, href);

   DRETURN(true);
}


/**
 * @brief Check queue reference list
 *
 * This function will be used to check the hard and soft queue list.
 * which will be defined during job submittion or redefined via
 * qalter and qmon.
 * This function will return success when:
 *    - queues are requestable and
 *    - if the contained queue-pattern matches at least one
 *      queue instance
 *
 * The host names of the references are resolved in place, and a reference
 * that then repeats an earlier one is removed.
 *
 * @param this_list QR list, may be an empty handle; changed in place although it is passed read-only
 * @param answer_list AN
 * @param master_cqueue_list the cluster queues a reference may name
 * @param master_hgroup_list the host groups a reference may name
 * @param master_centry_list the complex entries a reference may name
 *
 * @return error state true  - success false - error
 */
bool qref_list_is_valid(const ocs::tom::ConstList<ocs::tom::QR> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                        const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                        const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_list) {
      // the host names are brought into their unique form first; "q@h" and "q@h.domain" may then be the same
      // reference, which is kept once - like the resolution, this changes the list in place (CS-3053)
      qref_list_resolve_hostname(ocs::tom::List<ocs::tom::QR>::from_typed(const_cast<lList *>(this_list.raw())));

      /*
       * qname has to be requestable
       */
      if (centry_list_are_queues_requestable(master_centry_list)) {

         /*
          * At least one qinstance has to exist for each pattern
          */
         for (const auto qref_elem : this_list) {
            bool found_something = false;
            bool found_matching_qinstance = false;
            const char *qref_pattern = qref_elem.get<QR_name>();

            auto qref_list = ocs::tom::ListPtr<ocs::tom::QR>::create("qref list");
            qref_list->emplace_back<QR_name>(qref_pattern);
            // out-parameter of qref_list_resolve(), which takes a raw in-out list
            lList *resolved_qref_list_raw = nullptr;
            /* queue name expression support */
            qref_list_resolve(*qref_list, answer_list, &resolved_qref_list_raw,
                              &found_something, master_cqueue_list, master_hgroup_list, true, true);
            const auto resolved_qref_list = ocs::tom::ListPtr<ocs::tom::QR>::from_typed(resolved_qref_list_raw);
            for (const auto resolved_qref : *resolved_qref_list) {
               const char *resolved_qref_name = resolved_qref.get<QR_name>();

               if (cqueue_list_locate_qinstance(master_cqueue_list, resolved_qref_name) != nullptr) {
                  found_matching_qinstance = true;
               }
            }
            if (!found_matching_qinstance) {
               ERROR(MSG_QREF_QUNKNOWN_S, qref_pattern == nullptr ? "" : qref_pattern);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }
      } else {
         ERROR(SFNMAX, MSG_QREF_QNOTREQUESTABLE);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }
   DRETURN(ret);
}


/**
 * @brief Resolve the host part of every queue reference in a list
 *
 * A reference that resolves to one the list already holds, e.g. `q@h` next to
 * `q@h.domain`, is removed instead of renamed, so the first one stays and the
 * list never holds a reference twice, not even for a moment (CS-3053, N-H-18).
 *
 * @param[in,out] this_list the references to rewrite (QR), an empty handle is accepted
 */
void qref_list_resolve_hostname(const ocs::tom::List<ocs::tom::QR> this_list) {
   DENTER(TOP_LAYER);
   auto next = this_list.front();
   while (const auto qref = next) {
      next = qref.next();
      const std::string resolved = ocs_qref_resolved_name(qref.get<QR_name>());
      if (resolved == qref.get<QR_name>()) {
         continue;
      }
      const auto held = this_list.find_first<QR_name>(resolved.c_str());
      if (held && held.raw() != qref.raw()) {
         this_list.erase(qref);
      } else {
         qref.set<QR_name>(resolved.c_str());
      }
   }
   DRETURN_VOID;
}


/**
 * @brief The name of a queue reference with its host part in its unique form
 *
 * A reference is written `cqueue@host`; the host is resolved unless it is an
 * expression or a host group. Anything else is returned as it is.
 *
 * @param name the reference, nullptr is accepted
 * @return the resolved reference; empty for nullptr
 */
std::string
ocs_qref_resolved_name(const char *name) {
   DENTER(TOP_LAYER);
   std::string ret = name != nullptr ? name : "";
   dstring cqueue_name = DSTRING_INIT;
   dstring host_or_hgroup = DSTRING_INIT;
   bool has_hostname;
   bool has_domain;

   if (name != nullptr && cqueue_name_split(name, &cqueue_name, &host_or_hgroup, &has_hostname, &has_domain)) {
      const char *unresolved_name = sge_dstring_get_string(&host_or_hgroup);
      if (has_hostname && !ocs::is_expression(unresolved_name)) {
         char resolved_name[CL_MAXHOSTNAMELEN+1];
         if (getuniquehostname(unresolved_name, resolved_name, 0) == CL_RETVAL_OK) {
            const char *cqueue = sge_dstring_get_string(&cqueue_name);
            ret = std::string(sge_dstring_strlen(&cqueue_name) == 0 ? "" : cqueue) + "@" + resolved_name;
         }
      }
   }

   sge_dstring_free(&cqueue_name);
   sge_dstring_free(&host_or_hgroup);
   DRETURN(ret);
}

/**
 * @brief Parse a queue reference list
 *
 * Parse 'dest_str' and create a QR_type list. A reference given twice is
 * kept once.
 *
 * @param lpp QR list
 * @param dest_str input string
 *
 * @return error state
 *
 * @note MT-NOTE: cull_parse_destination_identifier_list() is MT safe
 */
int cull_parse_destination_identifier_list(lList **lpp, const char *dest_str) {
   DENTER(TOP_LAYER);

   int rule[] = {QR_name, 0};
   char **str_str = nullptr;
   int i_ret;
   char *s;

   if (lpp == nullptr) {
      DRETURN(1);
   }

   s = sge_strdup(nullptr, dest_str);
   if (s == nullptr) {
      *lpp = nullptr;
      DRETURN(3);
   }

   str_str = string_list(s, ",", nullptr);
   if (str_str == nullptr || *str_str == nullptr) {
      *lpp = nullptr;
      sge_free(&s);
      DRETURN(2);
   }

   // a reference given twice is kept once, in its first position; dropped before the list is built, so the list
   // never holds it twice, not even for a moment (CS-3053, N-H-18)
   {
      std::unordered_set<std::string> seen;
      char **kept = str_str;
      for (char **token = str_str; *token != nullptr; token++) {
         if (seen.insert(*token).second) {
            *kept++ = *token;
         }
      }
      *kept = nullptr;
   }

   i_ret = cull_parse_string_list(str_str, "destin_ident_list", ocs::tom::QR::descriptor.e, rule, lpp);
   if (i_ret) {
      sge_free(&s);
      sge_free(&str_str);
      DRETURN(3);
   }

   sge_free(&s);
   sge_free(&str_str);
   DRETURN(0);
}
