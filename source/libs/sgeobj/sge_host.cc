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
 * @brief Execution hosts: load values, resource bookings and name resolution
 *
 * A host name can be written in many forms, so anything that stores or
 * compares one resolves it first - see #sge_resolve_host.
 *
 * @see sge_host.h
 */
#include <cstring>

#include "comm/commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/ocs_Matcher.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userset.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_List.h"

#include "uti/ocs_Pattern.h"
#include "uti/sge.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "msg_sgeobjlib.h"
#include "msg_common.h"

#include "sge_host.h"

#include "ocs_TopologyString.h"

/**
 * @brief Find an execution host by name
 *
 * The comparison resolves host name aliases, so an alias finds the host too.
 *
 * @param host_list the hosts to search; nullptr uses the active data store's
 * @param hostname the host to look for
 * @return the host, or nullptr when it is not defined
 */
lListElem *
host_list_locate(const lList *host_list, const char *hostname) {
   DENTER(TOP_LAYER);

   lListElem *ret = nullptr;

   if (host_list != nullptr) {
      if (hostname != nullptr) {
         const lListElem *element = lFirst(host_list);

         if (element != nullptr) {
            int nm = NoName;

            if (object_has_type(element, EH_Type)) {
               nm = object_get_primary_key(EH_Type);
            }
            ret = lGetElemHostRW(host_list, nm, hostname);
         }
      } else {
         CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      }
   } else {
      /*
       * This is a normal case and no error
       * if e.g. someone tries to find a element at qmaster startup to 
       * check if a certain element should be added
       */
      ret = nullptr;
   }

   DRETURN(ret);
}

/**
 * @brief Membership in a reserved host group
 *
 * CS-2438 chunk 2b. The one place that answers "is this host in
 * @admin_hosts / @submit_hosts", for both helpers below.
 * Locating the group is all this adds; the membership question itself is
 * hgroup_contains_host(), which honours the CS-2451 contract (cache when
 * there is one, tree walk when there is not) and is shared with the
 * delete-semantics check in the qmaster.
 * Both steps are O(1) in the normal case: HGRP_name carries CULL_HASH, so
 * locating the group is a hash lookup, and so is the membership test
 * against HGRP_cached_hosts (HR_name, CULL_HASH). That matters because this
 * runs on every GDI request -- it is the bar the classic flat AH_LIST set,
 * which was itself a hashed lookup.
 *
 * @param hostname resolved host name, normally packet->host
 * @param group_name ADMIN_HOSTGROUP or SUBMIT_HOSTGROUP
 *
 * @return true if the host is a member, directly or through nesting
 *
 * @note MT-NOTE: host_is_in_reserved_hostgroup() is MT safe -- but it reads the master list of the
 *       calling thread's data store, whose elements the mirror thread frees while
 *       merging events (CS-2633). The caller must hold that store's READ lock;
 *       on the GDI permission path that is LOCK_LISTENER.
 *
 *       A missing group answers false. The group cannot be deleted (chunk 1
 *       reserves the names) and setup_qmaster.cc re-seeds it on every startup,
 *       so this is the "qmaster is not up yet" case, not a reachable steady state.
 */
static bool
host_is_in_reserved_hostgroup(const char *hostname, const char *group_name) {
   DENTER(TOP_LAYER);

   if (hostname == nullptr) {
      DRETURN(false);
   }

   const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list(*ocs::DataStore::get_master_list(SGE_TYPE_HGROUP));
   /*
    * CS-2680. Not const, and that is the signature saying what happens here: a
    * hit in the matching cache advances the time of its last use, so asking
    * whether a matcher admits this host writes. The write is atomic and needs no
    * lock of its own; only a structural change of the cache takes one, and that
    * one is neither of the data store's.
    */
   const auto hgroup = hgroup_list_locate(master_hgroup_list, group_name);

   /*
    * CS-2680. Two layers, and the order is the rule rather than an optimisation.
    *
    * The enumerated membership is asked first and answers every host the qmaster
    * knows -- which is every host that exists today, so this costs exactly what
    * it cost before the feature.
    *
    * Only when it says no is the second question asked: does a matcher of this
    * group fit the name? That question exists because enumeration structurally
    * cannot answer it for the hosts that matter most here. A freshly started
    * instance appears in no object, so it cannot be in a resolved membership,
    * and the answer would be a silent no in exactly the case a matcher was
    * written for -- the node that cannot be admitted because it is not yet an
    * execution host, and cannot become one because it is not admitted.
    *
    * Neither layer resolves a name; both compare the host name the request
    * already carries.
    */
   if (hgroup_contains_host(hgroup, hostname, master_hgroup_list)) {
      DRETURN(true);
   }

   DRETURN(ocs::Matcher::admits(hgroup, hostname, master_hgroup_list));
}

/**
 * @brief Is this host an admin host?
 *
 * CS-2438. The single place this question is answered, so that the dozen
 * call sites on the GDI permission path do not each know WHERE the answer
 * lives. Mirrors manop_is_manager() (sge_manop.cc), which does the same for
 * the reserved manager userset.
 * Fetches the master list itself rather than taking it as a parameter --
 * again like manop_is_manager() -- because a caller that has to name the
 * list is a caller that has to be edited when the list changes, which is
 * exactly what this helper exists to avoid.
 *
 * @param hostname resolved host name, normally packet->host
 *
 * @return true if the host may issue admin requests
 *
 * @note MT-NOTE: host_is_admin_host() is MT safe -- but it reads the master list of the
 *       calling thread's data store, whose elements the mirror thread frees while
 *       merging events (CS-2633). The caller must hold that store's READ lock;
 *       on the GDI permission path that is LOCK_LISTENER.
 *
 *       Backed by the reserved "@admin_hosts" host group since chunk 2b. AH_LIST
 *       is no longer consulted and is no longer written: qconf -ah/-dh maintain
 *       the group (chunk 3), and an existing cluster's members arrive through the
 *       -ah replay in load_config.sh (chunk 10). The two cannot be combined --
 *       a union with AH_LIST would keep granting admin rights to a host that
 *       qconf -dh has just removed from the group.
 */
bool host_is_admin_host(const char *hostname) {
   return host_is_in_reserved_hostgroup(hostname, ADMIN_HOSTGROUP);
}

/**
 * @brief Is this host a submit host?
 *
 * CS-2438. See host_is_admin_host() above; same role, same reason for
 * fetching the master list itself, and backed by the reserved
 * "@submit_hosts" host group since chunk 2b.
 *
 * @param hostname resolved host name, normally packet->host
 *
 * @return true if the host may submit
 *
 * @note MT-NOTE: host_is_submit_host() is MT safe -- but it reads the master list of the
 *       calling thread's data store, whose elements the mirror thread frees while
 *       merging events (CS-2633). The caller must hold that store's READ lock;
 *       on the GDI permission path that is LOCK_LISTENER.
 */
bool host_is_submit_host(const char *hostname) {
   return mconf_get_allow_any_submithosts() || host_is_configured_submit_host(hostname);
}

/**
 * @brief Is this host a submit host by configuration alone?
 *
 * The same question as host_is_submit_host(), without the ALLOW_ANY_SUBMITHOSTS
 * qmaster_param (CS-2578). For everything a submit host may do with its own jobs
 * the flag is meant to apply, and host_is_submit_host() is the one to ask. This
 * one is for the places which grant more than that and must not widen with the
 * flag - killing an event client of another client, for instance.
 *
 * @param hostname resolved host name, normally packet->host
 *
 * @return true if the host is a member of the reserved "@submit_hosts" host group
 *
 * @note MT-NOTE: host_is_configured_submit_host() is MT safe
 */
bool host_is_configured_submit_host(const char *hostname) {
   return host_is_in_reserved_hostgroup(hostname, SUBMIT_HOSTGROUP);
}

/**
 * @brief Reference other than @exec_hosts?
 *
 * CS-2438 chunk 7. Resolves a cluster queue's host list with the reserved
 * "@exec_hosts" entry taken out, and reports whether the host is still
 * reached.
 * This is what makes an exec host deletable again. @exec_hosts mirrors the
 * execution host list, so every exec host is in it by definition; a queue
 * that names @exec_hosts therefore has a queue instance on every exec host,
 * and counting those instances as references would make every exec host
 * permanently undeletable -- the very outcome chunk 1a avoided for the host
 * group check, reintroduced through the queue instances instead.
 * A queue instance that exists ONLY because of @exec_hosts is not somebody's
 * configuration naming this host. Removing the exec host removes it from
 * @exec_hosts, and the (empty) queue instance goes with it, which is what
 * host_sync_exec_hostgroup() drives through the cqueue machinery.
 * The boundary is the one chunk 1a already drew: a USER group that references
 * @exec_hosts still counts. Only the queue's own direct "@exec_hosts" entry
 * is filtered, because a user group naming it really is configuration the
 * administrator wrote and can edit.
 *
 * @param cqueue CQ_Type object
 * @param hostname host being deleted
 * @param hgrp_list HGRP_Type master list
 *
 * @return true if the queue reaches the host by some route other than the reserved group, i.e. the reference is
 *         real and must block deletion
 */
static bool
cqueue_reaches_host_without_exec_hosts(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue, const char *hostname,
                                       const ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list) {
   DENTER(TOP_LAYER);

   const auto cq_hostlist = cqueue_get_hostlist(cqueue, hgrp_list);
   // created by the first entry that passes the filter, so that no entry leaves it empty as before
   ocs::tom::ListPtr<ocs::tom::HR> filtered;
   bool reaches = false;

   for (const auto href : cq_hostlist) {
      const char *entry = ocs::tom::get<HR_name>(href);

      if (entry == nullptr || strcmp(entry, EXEC_HOSTGROUP) == 0) {
         continue;
      }
      ocs_href_list_emplace(filtered, entry);
   }

   if (filtered) {
      ocs::tom::ListPtr<ocs::tom::HR> resolved;
      const bool found = href_list_find_all_references(*filtered, nullptr, hgrp_list, &resolved, nullptr);

      if (found) {
         reaches = static_cast<bool>(ocs::tom::find_first<HR_name>(*resolved, hostname));
      }
   }

   DRETURN(reaches);
}

/**
 * @brief Is a given host referenced in other objects?
 *
 * This function returns true if the given "host" is referenced
 * in a cqueue contained in "queue_list" or in a host group.
 * If this is the case than a corresponding message will be added
 * to the "answer_list".
 *
 * The host stays an untyped element: like host_list_locate() the function
 * finds out at run time whether it is an execution host (object_has_type()).
 *
 * @param host EH_Type object
 * @param answer_list AN_Type list
 * @param queue_list CQ_Type list, may be an empty handle
 * @param hgrp_list HGRP_Type list (Master list), may be an empty handle
 *
 * @return true if the host is referenced, else false
 */
bool host_is_referenced(const lListElem *host, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const ocs::tom::ConstList<ocs::tom::CQ> queue_list,
                        const ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (host != nullptr) {
      int nm = NoName;

      if (object_has_type(host, EH_Type)) {
         nm = object_get_primary_key(EH_Type);
      }
      const char *hostname = lGetHost(host, nm);

      /* look at all the queue instances and figure out, if one still references
         the host we are looking for */
      for (const auto cqueue : queue_list) {
         const auto queue = ocs::tom::find_first_in<CQ_qinstances, QU_qhostname>(cqueue, hostname);

         if (queue) {
            const char *queuename = ocs::tom::get<CQ_name>(cqueue);

            /*
             * CS-2438: a queue instance that exists only because the queue names
             * the reserved @exec_hosts group is not a reference the administrator
             * can remove -- the group is system-maintained. Deleting the exec host
             * takes it out of @exec_hosts and the empty instance goes with it.
             */
            if (object_has_type(host, EH_Type) &&
                !cqueue_reaches_host_without_exec_hosts(cqueue, hostname, hgrp_list)) {
               continue;
            }

            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_HOSTREFINQUEUE_SS, hostname, queuename);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = true;
            break;
         }
      }

      /* if we have not found a reference yet, we keep looking in the host groups, if
         we have an exec host */
      if (!ret && object_has_type(host, EH_Type)) {
         for (const auto hgrp_elem : hgrp_list) {
            const char *hgrp_name = ocs::tom::get<HGRP_name>(hgrp_elem);

            /*
             * CS-2438: the reserved host groups do not count as a reference.
             *
             * They are not user configuration that a host could be "used by" --
             * they are the storage for what used to be separate lists, and
             * host_is_referenced() never looked at those lists:
             *
             *   @admin_hosts / @submit_hosts  membership is the old AH_LIST /
             *     SH_LIST. Deleting an exec host that was also an admin host
             *     always worked, and left it an admin host. Counting these here
             *     would not just change that -- it would DEADLOCK the deletion,
             *     because hgroup_mod() refuses to remove the qmaster host from
             *     @admin_hosts, so the obvious workaround is impossible.
             *   @exec_hosts  mirrors the execution host list, so it contains
             *     every exec host by definition; counting it would make every
             *     exec host permanently undeletable.
             *
             * A USER group that references a reserved group still counts -- that
             * really is someone's configuration naming this host, and they can
             * edit it.
             */
            if (hgroup_is_reserved(hgrp_name)) {
               continue;
            }

            /*
             * CS-2677: a queue host group is not a third party naming this host
             * -- it IS the host list of its cluster queue, and the loop over the
             * queues above has already judged it, with the "@exec_hosts" filter
             * CS-2438 requires (cqueue_reaches_host_without_exec_hosts()).
             * Counting it again here would undo that: a queue whose host list is
             * the reserved group reaches every exec host, so every exec host
             * would be undeletable -- exactly the outcome that filter exists to
             * prevent, reintroduced through the group the queue now owns.
             */
            if (hgroup_is_queue_group(hgrp_name)) {
               continue;
            }

            /*
             * CS-2680. Resolved WITHOUT expanding matchers, and that is the
             * whole point of this call. The question here is whether somebody's
             * configuration *names* this host, so that deleting it would leave a
             * member pointing at nothing. A matcher names nothing -- it describes
             * a set, and a host leaving that set is what the set is for.
             *
             * Expanding here would make every host a matcher happens to capture
             * permanently undeletable, which is the same trap the reserved groups
             * are skipped for above, and it would break the second half of the
             * requirement this feature exists for: an elastically provisioned
             * node has to be able to disappear again.
             */
            ocs::tom::ListPtr<ocs::tom::HR> host_list;
            hgroup_find_all_references(hgrp_elem, nullptr, hgrp_list, &host_list, nullptr, nullptr, false);
            if (ocs::tom::find_first<HR_name>(*host_list, hostname)) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_HOSTREFINHGRP_SS, hostname, hgrp_name);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

               ret = true;
               break;
            }
         }
      }
   }
   DRETURN(ret);
}


/* MT-NOTE: sge_resolve_host() is MT safe */
/**
 * @brief Replace a host name attribute with its resolved unique name
 *
 * Users write host names in whatever form they like; the object model stores
 * exactly one form per host, so a name has to be resolved before it is
 * compared or stored.
 *
 * @param[in,out] ep the element whose attribute is rewritten
 * @param nm the host attribute to resolve
 * @return `CL_RETVAL_OK` on success, otherwise a commlib error code
 */
int sge_resolve_host(lListElem *ep, int nm) {
   DENTER(TOP_LAYER);

   int pos;
   int ret = CL_RETVAL_OK;
   int dataType;
   char unique[CL_MAXHOSTNAMELEN];
   const char *hostname;

   memset(unique, 0, CL_MAXHOSTNAMELEN);

   if (ep == nullptr) {
      DRETURN(-1);
   }

   /* ep is no host element, if ep has no nm */
   if ((pos = lGetPosViaElem(ep, nm, SGE_NO_ABORT)) < 0) {
      DRETURN(-1);
   }

   dataType = lGetPosType(lGetElemDescr(ep), pos);
   switch (dataType) {
      case lStringT:
         hostname = lGetPosString(ep, pos);
         if (nm != CE_stringval) {
            DPRINTF("!!!!!!! sge_resolve_host: WARNING call with old lStringT data type,\n");
            DPRINTF("!!!!!!! this data type should be replaced with lHostT data type in\n");
            DPRINTF("!!!!!!! the future! Nevertheless, just a warning! Function works fine!\n");
         }
         break;

      case lHostT:
         hostname = lGetPosHost(ep, pos);
         break;

      default:
         hostname = nullptr;
         ret = CL_RETVAL_GETHOSTNAME_ERROR;
         break;
   }
   /* Check to find hostname only if it was not contained in expression */
   if (!ocs::is_expression(hostname)) {
      ret = sge_resolve_hostname(hostname, unique, nm);

      if (ret == CL_RETVAL_OK) {
         switch (dataType) {
            case lStringT:
               lSetPosString(ep, pos, unique);
               break;
            case lHostT:
               lSetPosHost(ep, pos, unique);
               break;
         }
      }
   }

   DRETURN(ret);
}

/* MT-NOTE: sge_resolve_hostname() is MT safe */
/**
 * @brief Resolve a host name to its unique form
 *
 * @param hostname the name to resolve
 * @param[out] unique receives the resolved name; must hold `CL_MAXHOSTNAMELEN` bytes
 * @param nm the attribute the name belongs to, which decides how strictly it is resolved
 * @return `CL_RETVAL_OK` on success, otherwise a commlib error code
 */
int sge_resolve_hostname(const char *hostname, char *unique, int nm) {
   DENTER(TOP_LAYER);

   int ret = CL_RETVAL_OK;

   if (hostname == nullptr) {
      DRETURN(CL_RETVAL_PARAMS);
   }

   /* 
    * these "special" names are resolved:
    *    "global", "unknown", "template")
    */
   switch (nm) {
      case CE_stringval:
         if (strcmp(hostname, SGE_UNKNOWN_NAME) != 0) {
            ret = getuniquehostname(hostname, unique, 0);
         } else {
            strcpy(unique, hostname);
         }

         break;
      case EH_name:
      case CONF_name:
         if ((strcmp(hostname, SGE_GLOBAL_NAME) != 0) &&
             (strcmp(hostname, SGE_TEMPLATE_NAME) != 0)) {
            ret = getuniquehostname(hostname, unique, 0);
         } else {
            strcpy(unique, hostname);
         }
         break;
      default:
         ret = getuniquehostname(hostname, unique, 0);
         break;
   }

   if (ret != CL_RETVAL_OK) {
      strncpy(unique, hostname, CL_MAXHOSTNAMELEN - 1);
   }

   DRETURN(ret);
}

/**
 * @brief Does an execution host refer to a complex entry at all?
 *
 * Checks the complex values, the load values and the report variables, so a
 * complex entry that any of the three names cannot be deleted.
 *
 * @param this_elem the host to check (EH_Type); an empty handle yields false
 * @param centry the complex entry to look for (CE_Type)
 * @return true when the host refers to it
 */
bool host_is_centry_referenced(const ocs::tom::ConstElement<ocs::tom::EH> this_elem,
                               const ocs::tom::ConstElement<ocs::tom::CE> centry) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (this_elem) {
      const char *name = ocs::tom::get<CE_name>(centry);
      const auto ce_values = ocs::tom::get<EH_consumable_config_list>(this_elem);
      const auto load_list = ocs::tom::get<EH_load_list>(this_elem);
      const auto rep_vars = ocs::tom::get<EH_report_variables>(this_elem);

      /* 
       * centry may be referenced in 
       *    - complex_values
       *    - load_list
       *    - report_variables
       */
      if (ocs::tom::find_first<CE_name>(ce_values, name) || ocs::tom::find_first<HL_name>(load_list, name) ||
          ocs::tom::find_first<STU_name>(rep_vars, name)) {
         ret = true;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Does an execution host define a value for a complex entry?
 *
 * Narrower than #host_is_centry_referenced: only the complex values and the
 * load values count, not the report variables.
 *
 * @param this_elem the host to check (EH_Type); an empty handle yields false
 * @param centry the complex entry to look for (CE_Type)
 * @return true when the host has a value for it
 */
bool host_is_centry_a_complex_value(const ocs::tom::ConstElement<ocs::tom::EH> this_elem,
                                    const ocs::tom::ConstElement<ocs::tom::CE> centry) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (this_elem) {
      const char *name = ocs::tom::get<CE_name>(centry);
      const auto ce_values = ocs::tom::get<EH_consumable_config_list>(this_elem);
      const auto load_list = ocs::tom::get<EH_load_list>(this_elem);

      /* 
       * centry may be referenced in 
       *    - complex_values
       *    - load_list
       */
      if (ocs::tom::find_first<CE_name>(ce_values, name) || ocs::tom::find_first<HL_name>(load_list, name)) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Merge global host settings into exec hosts
 *
 * Merges settings from the global host to the exec hosts objects.
 * Currently this applies only to the report_variables attribute.
 *
 * @param this_list the exec host list to work on (EH_Type), may be an empty handle
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: host_list_merge() is MT safe
 *
 * @see #host_merge
 */
bool host_list_merge(const ocs::tom::List<ocs::tom::EH> this_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_list) {
      const auto global_host = ocs::tom::find_first_rw<EH_name>(this_list, SGE_GLOBAL_NAME);

      /* we merge global settings into host settings */
      if (global_host) {
         /* for the global host, merged_report_variables == report_variables */
         ocs::tom::set<EH_merged_report_variables>(global_host,
                                                   ocs::tom::get<EH_report_variables>(global_host).clone(""));

         /* do merge for all hosts except global */
         for (const auto host : this_list) {
            if (host != global_host) {
               /* on error continue, but return error status */
               if (!host_merge(host, global_host)) {
                  ret = false;
               }
            }
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief Merge global host settings into an exec host
 *
 * Merges settings from the global host object into a specific exec host.
 * Use the global settings, if no host specific settings are done.
 * Currently this applies only to the report_variables attribute.
 *
 * @param host the host object to hold the merged config (EH_Type), may be an empty handle
 * @param global_host the global host object (EH_Type), may be an empty handle
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: host_merge() is MT safe
 *
 * @see #host_list_merge
 */
bool host_merge(const ocs::tom::Element<ocs::tom::EH> host, const ocs::tom::ConstElement<ocs::tom::EH> global_host) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (host && global_host) {
      const auto local_list = ocs::tom::get<EH_report_variables>(host);

      /* if we have a local list: use this one */
      if (!local_list.empty()) {
         ocs::tom::set<EH_merged_report_variables>(host, local_list.clone(""));
      } else {

         const auto global_list = ocs::tom::get<EH_report_variables>(global_host);
         /* if we have no local list, but a global one, use this one */
         if (!global_list.empty()) {
            ocs::tom::set<EH_merged_report_variables>(host, global_list.clone(""));
         } else {
            /* if no report variables are configured in local and global object,
             * delete the merged list.
             */
            ocs::tom::set<EH_merged_report_variables>(host, nullptr);
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief debit a certain RSMAP id from a host
 *
 * @param host          the host to do debiting on (EH_Type)
 * @param ce_name       name of the RSMAP complex variable
 * @param resl          the resource map object to be debited (RESL_Type)
 * @param slots         the number of slots for which we do debiting (can be negativ in undebiting)
 * @param just_check    when != nullptr we just check if debiting would be successful and return the result here;
 *                      the host is not modified then, and an id the host configures is reported as not fitting
 *                      when its utilized and requested amounts exceed the configured one
 * @return the number of modifications done (0 or 1); in the check mode 1 when the resource is utilized on the host
 */
int host_debit_rsmap(const ocs::tom::Element<ocs::tom::EH> host, const char *ce_name,
                     const ocs::tom::ConstElement<ocs::tom::RESL> resl, const int slots, bool *just_check) {
   int mods = 0;

   const auto resource_utilization = ocs::tom::get<EH_resource_utilization>(host);
   const auto resource = ocs::tom::find_first_rw<RUE_name>(resource_utilization, ce_name);

   const char *id = ocs::tom::get<RESL_value>(resl);
   const uint32_t amount = ocs::tom::get<RESL_amount>(resl);

   if (just_check != nullptr) {
      // only check if there is enough resources free, no booking - the host is not modified: an id
      // without a utilization entry yet counts as unused (CS-3124)
      // only when slots > 0? It is never called with slots < 0 and just_check
      if (!resource) {
         return mods;
      }
      const auto utilized_id = ocs::tom::find_first_in<RUE_utilized_now_resource_map_list, RESL_value>(resource, id);
      const uint32_t utilized_amount = utilized_id ? ocs::tom::get<RESL_amount>(utilized_id) : 0;
      const auto complex_value = ocs::tom::find_first_in<EH_consumable_config_list, CE_name>(host, ce_name);
      const auto configured_id = ocs::tom::find_first_in<CE_resource_map_list, RESL_value>(complex_value, id);
      // an id the host does not configure is not judged here, as before; for a configured one the amounts
      // are compared without the unsigned subtraction, which never became negative (CS-3124)
      if (configured_id &&
          static_cast<uint64_t>(utilized_amount) + amount > ocs::tom::get<RESL_amount>(configured_id)) {
         // not enough available of this id
         *just_check = false;
      }
      mods++;
      return mods;
   }

   auto utilized_id = ocs::tom::find_first_in_rw<RUE_utilized_now_resource_map_list, RESL_value>(resource, id);
   if (!utilized_id) {
      utilized_id = ocs::tom::emplace_back_in<RUE_utilized_now_resource_map_list, RESL_value>(resource, id);
   }
   if (utilized_id) {
      if (slots < 0) {
         // undebit
         ocs::tom::add<RESL_amount>(utilized_id, -amount);
      } else {
         // debit
         ocs::tom::add<RESL_amount>(utilized_id, amount);
      }
      mods++;
   }

   return mods;
}

/**
 * @brief Book or release the cores a job was bound to
 *
 * Unlike a numeric consumable, a binding is booked as a *topology string*:
 * which cores are in use matters, not how many. The booking is kept in the
 * slots resource utilization entry.
 *
 * @param[in,out] host the execution host to book on (EH_Type)
 * @param ce_name the resource the binding belongs to
 * @param resl the topology to book, as a string list element (ST_Type)
 * @param slots positive to book, negative to release
 * @param[out] just_check if != nullptr do not book, only report whether it would fit
 * @return the number of bookings that took place, or a negative value on error
 */
int host_debit_binding(const ocs::tom::Element<ocs::tom::EH> host, const char *ce_name,
                       const ocs::tom::ConstElement<ocs::tom::ST> resl, const int slots, bool *just_check) {
   DENTER(TOP_LAYER);

   // we book cores/threads in the resource utilization of slots
   const auto resource_utilization = ocs::tom::get<EH_resource_utilization>(host);
   auto resource = ocs::tom::find_first_rw<RUE_name>(resource_utilization, ce_name);

   // add resource utilization entry if it does not exist yet
   if (!resource) {
      resource = ocs::tom::emplace_back_in<EH_resource_utilization, RUE_name>(host, ce_name);
      ocs::tom::set<RUE_utilized_now_binding_inuse>(resource, nullptr);
   }

   // what is currently booked and what should be booked additionally
   const char *binding_to_use = ocs::tom::get<ST_name>(resl);
   const char *binding_in_use = ocs::tom::get<RUE_utilized_now_binding_inuse>(resource);

#if 1
   const char *action = slots > 0 ? "add" : "del";
   if (binding_in_use != nullptr) {
      ocs::TopologyString topo_in_use(binding_in_use);
      DPRINTF("host_debit_binding: before %s\n", topo_in_use.to_product_topology_string().c_str());
   } else {
      DPRINTF("host_debit_binding: before NONE\n");
   }
   if (binding_to_use != nullptr) {
      ocs::TopologyString topo_to_use(binding_to_use);
      DPRINTF("host_debit_binding: %s    %s\n", action, topo_to_use.to_product_topology_string().c_str());
   }
#endif

   // just check if the requested change makes sense then return
   int mods = 0;
   if (just_check != nullptr) {
      if (binding_in_use == nullptr || strcmp(binding_to_use, binding_in_use) != 0) {
         *just_check = true;
         mods++;
      } else {
         *just_check = false;
      }
      DRETURN(mods);
   }

   // add/remove all cores/threads that are set in binding_to_use to binding_in_use
   ocs::TopologyString topo_now;
   const char *now_str = ocs::tom::get<RUE_utilized_now_binding_inuse>(resource);
   if (now_str) {
      topo_now = ocs::TopologyString(now_str);
      ocs::TopologyString topo_to_use(binding_to_use);
      ocs::TopologyString::elem_mark_nodes_as_used_or_unused<RUE_utilized_now_binding_inuse>(resource, topo_now,
                                                                                           topo_to_use, slots > 0);
   } else {
      topo_now = ocs::TopologyString(binding_to_use);
      ocs::TopologyString::elem_mark_nodes_as_used_or_unused<RUE_utilized_now_binding_inuse>(resource, topo_now,
                                                                                           topo_now, slots > 0);
   }
   DPRINTF("host_debit_binding: after  %s\n", topo_now.to_product_topology_string().c_str());

   mods++;
   DRETURN(mods);
}

/**
 * @brief Should a per host consumable still be booked on this host?
 *
 * A parallel job occupies several slots on one host, but a `CONSUMABLE_HOST`
 * resource is booked once per host. The caller walks the granted slots in host
 * order and passes the same `last_hostname` each time; this answers true only
 * the first time a host is seen.
 *
 * @param[in,out] last_hostname the host of the previous call; updated on a new host
 * @param hostname the host of the current slot
 * @return true when this is the first slot on that host
 */
bool host_do_per_host_booking(const char **last_hostname, const char *hostname) {
   bool ret;

   if (sge_strnullcmp(*last_hostname, hostname) == 0) {
      // we already booked to this host - do not repeat
      ret = false;
   } else {
      // we did not yet book to this host, it is a new one
      ret = true;
      *last_hostname = hostname;
   }

   return ret;
}

/** @brief Get the topology string that is currently in use on a host
 *
 * This is used to get the topology string that is currently in use on a host. It is used for example
to check if a certain binding can be used on a host or if it would exceed the available resources.
 *
 * @param host the host to check (EH_Type), may be an empty handle
 * @return the topology string that is currently in use on the host, empty string if no topology is in use
 */
std::string host_get_topology_in_use(const ocs::tom::ConstElement<ocs::tom::EH> host) {
   DENTER(TOP_LAYER);

   // invalid host pointer, return empty string
   if (!host) {
      DRETURN({});
   }

   // get the resource utilization of the host, if there is none, return empty string
   const auto resource_utilization = ocs::tom::get<EH_resource_utilization>(host);
   if (!resource_utilization) {
      DRETURN({});
   }

   // get the slots resource utilization of the host, if there is none, return empty string
   const auto slots_utilization = ocs::tom::find_first<RUE_name>(resource_utilization, SGE_ATTR_SLOTS);
   if (!slots_utilization) {
      DRETURN({});
   }

   // get the currently in use binding for slots, if there is none, return empty string
   const char *binding_inuse = ocs::tom::get<RUE_utilized_now_binding_inuse>(slots_utilization);
   if (binding_inuse == nullptr) {
      DRETURN({});
   }

   // we have a binding in use, return the corresponding topology string
   const ocs::TopologyString topo_in_use(binding_inuse);
   DRETURN(topo_in_use.to_product_topology_string());
}
