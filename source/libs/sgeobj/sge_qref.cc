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

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "comm/commlib.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/msg_sgeobjlib.h"

#include <cinttypes>
#include "msg_common.h"
#include "msg_clients_common.h"

/// Debug layer the queue reference traces are written to
#define QREF_LAYER TOP_LAYER


static bool
qref_list_resolve_cqueue_names(const lList *cq_qref_list, 
                               lList **answer_list,
                               lList **qref_list, 
                               bool *found_something,
                               const lList *cqueue_list,
                               bool resolve_cqueue);

static bool
qref_list_resolve_qinstance_names(const lList *cq_qref_list, 
                                  lList **answer_list,
                                  dstring *host_or_hgroup,
                                  lList **qref_list, 
                                  bool *found_something,
                                  const lList *cqueue_list);

static bool
qref_list_resolve_qdomain_names(const lList *cq_qref_list,
                                lList **answer_list,
                                dstring *host_or_hgroup,
                                lList **qref_list,
                                bool *found_something,
                                const lList *cqueue_list,
                                const lList *hgroup_list,
                                bool resolve_qdomain);

static bool
qref_list_resolve_cqueue_names(const lList *cq_qref_list,
                               lList **answer_list,
                               lList **qref_list,
                               bool *found_something,
                               const lList *cqueue_list,
                               bool resolve_cqueue) {
   DENTER(QREF_LAYER);
   bool ret = true;

   for_each_ep_lv(cq_qref, cq_qref_list) {
      const char *cq_name = lGetString(cq_qref, QR_name);

      if (resolve_cqueue) {
         const lListElem *cqueue = nullptr;
         const lList *qinstance_list = nullptr;

         cqueue = lGetElemStr(cqueue_list, CQ_name, cq_name); 
         qinstance_list = lGetList(cqueue, CQ_qinstances);
         for_each_ep_lv(qinstance, qinstance_list) {
            lAddElemStr(qref_list, QR_name, lGetString(qinstance, QU_full_name), QR_Type);
            *found_something = true;
         }
      } else {
         lAddElemStr(qref_list, QR_name, cq_name, QR_Type);
      }
   }
   DRETURN(ret);
}

static bool
qref_list_resolve_qinstance_names(const lList *cq_qref_list,
                                  lList **answer_list,
                                  dstring *host_or_hgroup,
                                  lList **qref_list,
                                  bool *found_something,
                                  const lList *cqueue_list) {
   DENTER(QREF_LAYER);
   bool ret = true;

   for_each_ep_lv(cq_qref, cq_qref_list) {
      const char *cqueue_name = nullptr;
      const char *hostname_pattern = nullptr;
      const lListElem *cqueue = nullptr;
      const lList *qinstance_list = nullptr;
      lList *qi_ref_list = nullptr;

      cqueue_name = lGetString(cq_qref, QR_name);
      cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
      hostname_pattern = sge_dstring_get_string(host_or_hgroup);
      qinstance_list = lGetList(cqueue, CQ_qinstances);
      qinstance_list_find_matching(qinstance_list, answer_list,
                                   hostname_pattern, &qi_ref_list);

      for_each_ep_lv(qi_qref, qi_ref_list) {
         const char *qi_name = lGetString(qi_qref, QR_name);

         lAddElemStr(qref_list, QR_name, qi_name, QR_Type);
         *found_something = true;
      }
      lFreeList(&qi_ref_list);
   }
   DRETURN(ret);
}

static bool
qref_list_resolve_qdomain_names(const lList *cq_qref_list,
                                lList **answer_list,
                                dstring *host_or_hgroup,
                                lList **qref_list,
                                bool *found_something,
                                const lList *cqueue_list,
                                const lList *hgroup_list,
                                bool resolve_qdomain) {
   DENTER(QREF_LAYER);

   bool ret = true;
   const char *hgroup_pattern = nullptr;
   lList *href_list = nullptr;
   dstring buffer = DSTRING_INIT;

   hgroup_pattern = sge_dstring_get_string(host_or_hgroup);
   /*
    * Find all hostgroups which match 'hgroup_pattern'
    * Possibly resolve them.
    */
   if (resolve_qdomain) {
      hgroup_list_find_matching_and_resolve(hgroup_list, answer_list,
                                            hgroup_pattern, &href_list);
   } else {
      hgroup_list_find_matching(hgroup_list, answer_list,
                                hgroup_pattern, &href_list);
   }
   for_each_ep_lv(cq_qref, cq_qref_list) {
      const char *cqueue_name = lGetString(cq_qref, QR_name);
      const lListElem *cqueue = nullptr;
      const lList *qinstance_list = nullptr;

      cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
      qinstance_list = lGetList(cqueue, CQ_qinstances);
      for_each_ep_lv(href, href_list) {
         if (resolve_qdomain) {
            const char *hostname = lGetHost(href, HR_name);
            const lListElem *qinstance = nullptr;

            qinstance = lGetElemHost(qinstance_list, 
                                     QU_qhostname, hostname);
            if (qinstance != nullptr) {
               const char *qinstance_name = nullptr;

               qinstance_name = qinstance_get_name(qinstance, 
                                                   &buffer);
               lAddElemStr(qref_list, QR_name, 
                           qinstance_name, QR_Type);
               *found_something = true;
            }
         } else {
            const char *hgroup_name = lGetHost(href, HR_name);
            const char *qinstance_name = nullptr;

            qinstance_name = sge_dstring_sprintf(&buffer, SFN "@" SFN,
                                                 cqueue_name, hgroup_name);
            lAddElemStr(qref_list, QR_name,
                        qinstance_name, QR_Type);
            *found_something = true;
         }
      }
   }
   sge_dstring_free(&buffer);
   lFreeList(&href_list);
   DRETURN(ret);
}

/**
 * @brief Add a queue reference to the list
 *
 * Add the queue reference "qref_string" to the QR_type list "this_list".
 * Errors will be reported via return value and "answer_list".
 *
 * @param this_list QR_Type
 * @param answer_list AN_Type
 * @param qref_string queue reference
 *
 * @return error state true  - success false - error
 */
bool qref_list_add(lList **this_list, lList **answer_list, const char *qref_string) {
   DENTER(QREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && qref_string != nullptr) {
      lListElem *new_elem; 

      new_elem = lAddElemStr(this_list, QR_name, qref_string, QR_Type);
      if (new_elem == nullptr) {
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
 * @param src_qref_list QR_Type list (input: pattern)
 * @param answer_list AN_Type list
 * @param qref_list QR_Type list (output: resolved qrefs)
 * @param found_something a pattern matched? (output)
 * @param cqueue_list master CQ_Type list
 * @param hgroup_list master HGRP_Type list
 * @param resolve_cqueue resolve cqueue pattern?
 * @param resolve_qdomain resolve qdomain pattern?
 *
 * @return error state true  - success false - error
 */
bool qref_list_resolve(const lList *src_qref_list, lList **answer_list,
                       lList **qref_list, bool *found_something,
                       const lList *cqueue_list, const lList *hgroup_list,
                       bool resolve_cqueue, bool resolve_qdomain) {
   DENTER(QREF_LAYER);

   bool ret = true;
   dstring cqueue_name = DSTRING_INIT;
   dstring host_or_hgroup = DSTRING_INIT;

   if (src_qref_list != nullptr) {
      *found_something = false;
      for_each_ep_lv(qref_pattern, src_qref_list) {
         const char *name = nullptr;
         bool has_hostname;
         bool has_domain;
         const char *cq_pattern = nullptr;
         lList *cq_ref_list = nullptr;
         bool tmp_found_something = false;
 
         /*
          * Find all existing parts of the qref-pattern
          */ 
         name = lGetString(qref_pattern, QR_name); 
         if (!cqueue_name_split(name, &cqueue_name, &host_or_hgroup,
                           &has_hostname, &has_domain)) {
            /* we've got an syntax error */ 
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX,
               ANSWER_QUALITY_ERROR, MSG_CQUEUE_NOQMATCHING_S, name);
            lFreeList(&cq_ref_list);
            continue;
         }                 
         cq_pattern = sge_dstring_get_string(&cqueue_name);

         cqueue_list_find_all_matching_references(cqueue_list, answer_list,
                                                  cq_pattern, &cq_ref_list);

         /*
          * Depending on the type of pattern -> resolve QC or QI names
          */
         if (has_domain) {
            ret &= qref_list_resolve_qdomain_names(cq_ref_list, answer_list,
                                                   &host_or_hgroup, qref_list,
                                                   &tmp_found_something,
                                                   cqueue_list, hgroup_list,
                                                   resolve_qdomain);
         } else if (has_hostname) {
            ret &= qref_list_resolve_qinstance_names(cq_ref_list, answer_list,
                                                     &host_or_hgroup, qref_list,
                                                     &tmp_found_something, 
                                                     cqueue_list); 
         } else {
            ret &= qref_list_resolve_cqueue_names(cq_ref_list, answer_list,
                                                  qref_list, 
                                                  &tmp_found_something,
                                                  cqueue_list, 
                                                  resolve_qdomain);
         }
         if (tmp_found_something) {
            *found_something = true;
         } 
         lFreeList(&cq_ref_list);
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
 * @param hgroup_list host group list
 *
 * @return true if rejected
 *
 * @note MT-NOTE: qref_cq_rejected() is MT safe
 */
bool qref_cq_rejected(const char *qref_pattern, const char *cqname,
                      const char *hostname, const lList *hgroup_list) {
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
 * @brief Check, if -q qref_list rejects (cluster) queue
 *
 * Check if -q qref_list rejects cluster queue and hostname, if passed.
 * If nullptr is passed as hostname, cluster queue verfication is performed
 * only.
 *
 * @param qref_list QR_Type list as usef for -q qref_list
 * @param cqname cluster queue name
 * @param hostname exeuction hostname
 * @param hgroup_list host group list
 *
 * @return true if rejected
 *
 * @note MT-NOTE: qref_list_cq_rejected() is MT safe
 */
static bool
qref_eh_rejected(const char *qref_pattern, const char *hostname, const lList *hgroup_list) {
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
 * @param qref_list the queue references to check
 * @param hostname the host in question
 * @param hgroup_list the host groups, needed to resolve a group reference
 * @return true when the host is rejected by all of them
 */
bool qref_list_eh_rejected(const lList *qref_list, const char *hostname, const lList *hgroup_list) {
   DENTER(TOP_LAYER);

   if (hostname == nullptr) {
      DRETURN(true);
   }

   if (qref_list == nullptr) {
      DRETURN(false);
   }

   for_each_ep_lv(qref_pattern, qref_list) {
      const char *name = lGetString(qref_pattern, QR_name);
      if (!qref_eh_rejected(name, hostname, hgroup_list)) {
         DRETURN(false);
      }
   }

   DRETURN(true);
}


/**
 * @brief Does a queue reference list exclude one cluster queue on one host?
 *
 * @param qref_list the queue references to check
 * @param cqname the cluster queue in question
 * @param hostname the host in question; nullptr checks the cluster queue alone
 * @param hgroup_list the host groups, needed to resolve a group reference
 * @return true when the queue is rejected by all of them
 */
bool qref_list_cq_rejected(const lList *qref_list, const char *cqname,
                           const char *hostname, const lList *hgroup_list) {
   DENTER(TOP_LAYER);

   if (cqname == nullptr) {
      DRETURN(true);
   }

   if (qref_list == nullptr) {
      DRETURN(false);
   }

   for_each_ep_lv(qref_pattern, qref_list) {
      const char *name = lGetString(qref_pattern, QR_name);
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
 * @param hgroup hostgroup to search (HGRP_Type)
 * @param hostname the host in question
 * @param hgroup_list hostgroup list (HGRP_Type)
 *
 * @return True if the host is not a member.
 *
 * @note MT-NOTE: qref_hgroup_rejected() is MT safe
 *
 * @see #href_list_find_references
 */
static bool
qref_hgroup_rejected(const lListElem *hgroup, const char *hostname, const lList *hgroup_list) {
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

   for_each_ep_lv(href, lGetList(hgroup, HGRP_host_list)) {
      const char *member = lGetHost(href, HR_name);

      if (ocs::is_hgroup_name(member)) {
         /* nested hostgroup - look it up by name, do not match it as a pattern */
         const lListElem *sub_hgroup = hgroup_list_locate(hgroup_list, member);

         if (sub_hgroup != nullptr && !qref_hgroup_rejected(sub_hgroup, hostname, hgroup_list)) {
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
 * @param hgroup_list hostgroup list (HGRP_Type)
 *
 * @return True if rejected.
 *
 * @note MT-NOTE: qref_list_host_rejected() is MT safe
 */
bool qref_list_host_rejected(const char *href, const char *hostname, const lList *hgroup_list) {
   DENTER(BASIS_LAYER);

   if (ocs::is_hgroup_name(href)) { /* wc_hostgroup */
      const char *wc_hostgroup = &href[1];

      if (ocs::is_expression(wc_hostgroup)) {
         /* the reference is an expression - evaluate it against every hostgroup name */
         for_each_ep_lv(hgroup, hgroup_list) {
            const char *hgroup_name = lGetHost(hgroup, HGRP_name);

            DPRINTF("found hostgroup \"%s\" wc_hostgroup: \"%s\"\n", hgroup_name, wc_hostgroup);

            if (sge_eval_expression(ocs::CEntry::Type::HOST, wc_hostgroup, &hgroup_name[1], nullptr, true, true) == 0) {
               if (!qref_hgroup_rejected(hgroup, hostname, hgroup_list)) {
                  DRETURN(false);
               }
            }
         }
      } else {
         /* plain hostgroup name - hash lookup instead of scanning the list */
         const lListElem *hgroup = hgroup_list_locate(hgroup_list, href);

         if (hgroup != nullptr && !qref_hgroup_rejected(hgroup, hostname, hgroup_list)) {
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
 * @brief Remove some elements from list
 *
 * Remove all elements from "this_list" where either the cluster
 * queue name is equivalent with the cluster queue part of "fullname"
 * or if the hostname is different from the hostname part of
 * "fullname"
 *
 * @param this_list QR_Type
 * @param full_name queue instance name
 *
 * @return error state true  - success false - error
 */
bool qref_list_trash_some_elemts(lList **this_list, const char *full_name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_list != nullptr) {
      lListElem *qref = nullptr;
      lListElem *next_qref = nullptr;
      dstring cqueue_buffer = DSTRING_INIT;
      dstring host_or_hgroup_buffer = DSTRING_INIT;
      dstring cqueue_buffer1 = DSTRING_INIT;
      dstring host_or_hgroup_buffer1 = DSTRING_INIT;
      const char *cqueue1 = nullptr;
      const char *host1 = nullptr;

      if (!cqueue_name_split(full_name, &cqueue_buffer1, &host_or_hgroup_buffer1, nullptr,
                        nullptr)) {
         /* we got an syntax error while splitting */               
         ret = false;
      } else {                
         cqueue1 = sge_dstring_get_string(&cqueue_buffer1); 
         host1 = sge_dstring_get_string(&host_or_hgroup_buffer1);

         next_qref = lFirstRW(*this_list);
         while ((qref = next_qref) != nullptr) {
            const char *name = nullptr;
            const char *cqueue = nullptr;
            const char *host = nullptr;

            next_qref = lNextRW(qref);

            name = lGetString(qref, QR_name);
            if (!cqueue_name_split(name, &cqueue_buffer, &host_or_hgroup_buffer, nullptr,
                              nullptr)) {
               sge_dstring_clear(&cqueue_buffer);
               sge_dstring_clear(&host_or_hgroup_buffer);
               ret = false;
               break;
            }
            cqueue = sge_dstring_get_string(&cqueue_buffer);
            host = sge_dstring_get_string(&host_or_hgroup_buffer);

            /*
             * Same cluster queue or different host?
             */
            if (!sge_strnullcmp(cqueue1, cqueue) || sge_strnullcmp(host1, host)) {
               lRemoveElem(*this_list, &qref);
            }

            sge_dstring_clear(&cqueue_buffer);
            sge_dstring_clear(&host_or_hgroup_buffer);
         }
         if (lGetNumberOfElem(*this_list) == 0) {
            lFreeList(this_list);
         }

         sge_dstring_free(&cqueue_buffer);
         sge_dstring_free(&host_or_hgroup_buffer);
         sge_dstring_free(&cqueue_buffer1);
         sge_dstring_free(&host_or_hgroup_buffer1);
      }
   }   
   DRETURN(ret);
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
 * @param this_list QR_Type
 * @param answer_list AN_Type
 * @param master_cqueue_list the cluster queues a reference may name
 * @param master_hgroup_list the host groups a reference may name
 * @param master_centry_list the complex entries a reference may name
 *
 * @return error state true  - success false - error
 */
bool qref_list_is_valid(const lList *this_list, lList **answer_list, const lList *master_cqueue_list,
                        const lList *master_hgroup_list, const lList *master_centry_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_list != nullptr) {

      /*
       * qname has to be requestable
       */
      if (centry_list_are_queues_requestable(master_centry_list)) {

         /*
          * At least one qinstance has to exist for each pattern
          */
         for_each_rw_lv(qref_elem, this_list) {
            bool found_something = false;
            bool found_matching_qinstance = false;
            const char *qref_pattern = nullptr;
            lList *resolved_qref_list = nullptr;
            lList *qref_list = nullptr;
            const lListElem *resolved_qref = nullptr;
            qref_resolve_hostname(qref_elem);
            qref_pattern = lGetString(qref_elem, QR_name);

            lAddElemStr(&qref_list, QR_name, qref_pattern, QR_Type);
            /* queue name expression support */
            qref_list_resolve(qref_list, answer_list, &resolved_qref_list,
                              &found_something, master_cqueue_list, master_hgroup_list, true, true);
            for_each_ep(resolved_qref, resolved_qref_list) {
               const char *resolved_qref_name = lGetString(resolved_qref, QR_name);

               if (cqueue_list_locate_qinstance(master_cqueue_list, resolved_qref_name) != nullptr) {
                  found_matching_qinstance = true;
               }
            }
            lFreeList(&qref_list);
            lFreeList(&resolved_qref_list);
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
 * @param[in,out] this_list the references to rewrite
 */
void qref_list_resolve_hostname(lList *this_list) {
   DENTER(TOP_LAYER);
   for_each_rw_lv(qref, this_list) {
      qref_resolve_hostname(qref);
   }
   DRETURN_VOID;
}

/* QR_name might be a pattern */
/**
 * @brief Resolve the host part of one queue reference
 *
 * A reference is written `cqueue@host`, and the host has to be brought into its
 * unique form before it is compared or stored.
 *
 * @param[in,out] this_elem the reference to rewrite
 */
void
qref_resolve_hostname(lListElem *this_elem) {
   DENTER(TOP_LAYER);
   dstring cqueue_name = DSTRING_INIT;
   dstring host_or_hgroup = DSTRING_INIT;
   const char *name = lGetString(this_elem, QR_name);
   bool has_hostname;
   bool has_domain;

   if (cqueue_name_split(name, &cqueue_name, &host_or_hgroup, &has_hostname, &has_domain)) {
      const char *unresolved_name = sge_dstring_get_string(&host_or_hgroup);
      /* Find all CQ names which match 'cq_pattern' */
      if (has_hostname && !ocs::is_expression(unresolved_name)) {
         char resolved_name[CL_MAXHOSTNAMELEN+1];
         int back = getuniquehostname(unresolved_name, resolved_name, 0);

         if (back == CL_RETVAL_OK) {
            dstring new_qref_pattern = DSTRING_INIT;
            if (sge_dstring_strlen(&cqueue_name) == 0) {
                sge_dstring_sprintf(&new_qref_pattern, "@%s", resolved_name);
            } else {
               sge_dstring_sprintf(&new_qref_pattern, "%s@%s", sge_dstring_get_string(&cqueue_name), resolved_name);
            }
            lSetString(this_elem, QR_name, sge_dstring_get_string(&new_qref_pattern));
            sge_dstring_free(&new_qref_pattern);
         }
      }
   }

   sge_dstring_free(&cqueue_name);
   sge_dstring_free(&host_or_hgroup);

   DRETURN_VOID;
}

/**
 * @brief Parse a queue reference list
 *
 * Parse 'dest_str' and create a QR_type list.
 *
 * @param lpp QR_Type list
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

   i_ret = cull_parse_string_list(str_str, "destin_ident_list", QR_Type, rule, lpp);
   if (i_ret) {
      sge_free(&s);
      sge_free(&str_str);
      DRETURN(3);
   }

   sge_free(&s);
   sge_free(&str_str);
   DRETURN(0);
}
