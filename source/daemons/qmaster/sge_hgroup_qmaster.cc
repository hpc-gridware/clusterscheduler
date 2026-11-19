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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Host groups, and the queue instances that follow their membership
 */

#include <cstdio>
#include <cctype>
#include <cstring>

#include "uti/ocs_Pattern.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_List.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_c_gdi.h"
#include "sge_hgroup_qmaster.h"
#include "evm/sge_event_master.h"
#include "sge_cqueue_qmaster.h"
#include "sge_host_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"

static bool
hgroup_mod_hostlist(ocs::tom::Element<ocs::tom::HGRP> hgroup, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                    ocs::tom::Element<ocs::tom::HGRP> reduced_elem, ocs::gdi::Command cmd,
                    ocs::gdi::SubCommand sub_command, ocs::tom::ListPtr<ocs::tom::HR> &add_hosts,
                    ocs::tom::ListPtr<ocs::tom::HR> &rem_hosts, ocs::tom::ListPtr<ocs::tom::HR> &occupant_groups);

static void
hgroup_rollback(ocs::tom::Element<ocs::tom::HGRP> this_elem);

/**
 * @brief Which nested group reaches this host?
 *
 * CS-2438 chunk 4. Walks the DIRECT entries of a reserved group, and for
 * each "@group" reference among them asks whether that group contains the
 * host. Appends the names of those that do, comma-separated.
 * Only used to build an error message, so it is allowed to be the slow path.
 *
 * @param current direct HR_Type entries of the reserved group
 * @param hostname host that could not be removed directly
 * @param master_hgroup_list list the references resolve against
 * @param groups receives the comma-separated names
 *
 * @return true if at least one nested group contains the host
 */
static bool
hgroup_nesting_that_contains(const ocs::tom::ConstList<ocs::tom::HR> current, const char *hostname,
                             const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *groups) {
   DENTER(TOP_LAYER);

   bool found = false;

   for (const auto href : current) {
      const char *entry = ocs::tom::get<HR_name>(href);

      if (entry == nullptr || !ocs::is_hgroup_name(entry)) {
         continue;
      }
      const ocs::tom::ConstElement<ocs::tom::HGRP> nested = hgroup_list_locate(master_hgroup_list, entry);

      if (hgroup_contains_host(nested, hostname, master_hgroup_list)) {
         if (found) {
            sge_dstring_append(groups, ", ");
         }
         sge_dstring_append(groups, entry);
         found = true;
      }
   }

   DRETURN(found);
}

/**
 * @brief 9.1 membership errors for -ah/-dh/-as/-ds
 *
 * CS-2438. attr_mod_sub_list() is deliberately TOLERANT: appending a member
 * that is already there, or removing one that is not, is reported as an INFO
 * answer with STATUS_OK and the request otherwise succeeds. That is right for
 * "qconf -aattr" on an ordinary host group, and it is what made this check
 * necessary -- once chunk 3 redirected "qconf -ah/-dh/-as/-ds" onto that
 * path, both cases started exiting 0 where 9.1 exited 1, and "-ah" reported
 * a host as added that it had not added.
 * Silently succeeding is the wrong answer for these two groups in
 * particular. They are the admin and submit host lists: a script that says
 * "qconf -ds host || alert" has to keep failing when the removal did not
 * happen.
 * Scoped to @admin_hosts and @submit_hosts, so "-aattr/-dattr" on every
 * other host group keeps the tolerant behaviour it has always had.
 * @exec_hosts needs no entry -- it is system-maintained and every write to
 * it is refused earlier.
 * Checked against the PRE-state, unlike the qmaster-host guard below which
 * checks the end state: "was this host already a member" is a question about
 * the state before the merge, and after attr_mod_sub_list() has run the two
 * cases are indistinguishable from a successful one. Called after
 * href_list_resolve_hostnames() so a short name and its FQDN compare equal.
 *
 * @param hgroup the group as it is BEFORE the merge
 * @param answer_list for returning the errors
 * @param delta_list HR_Type entries the request carries
 * @param sub_command APPEND, REMOVE or neither
 * @param master_hgroup_list to resolve nested references (chunk 4)
 *
 * @return false if any entry was already/not a member; the request must fail
 *
 * @note The two messages are not symmetric, and that is not a slip: 9.1 produced
 *       MSG_SGETEXT_ALREADYEXISTS_SS with the GDI object name ("adminhost") from
 *       sge_c_gdi.cc and MSG_SGETEXT_DOESNOTEXIST_SS with the spelt-out name
 *       ("administrative host") from sge_del_host(). Both are reproduced as they
 *       were rather than unified, so no script parsing either one has to change.
 */
static bool
hgroup_reserved_delta_is_strict(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const ocs::tom::ConstList<ocs::tom::HR> delta_list,
                                const ocs::gdi::SubCommand sub_command,
                                const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(TOP_LAYER);

   const char *add_name;   /* as sge_c_gdi.cc named it   */
   const char *del_name;   /* as sge_del_host() named it */
   bool ret = true;

   const char *group = ocs::tom::get<HGRP_name>(hgroup);
   if (group == nullptr) {
      DRETURN(true);
   }
   if (strcmp(group, ADMIN_HOSTGROUP) == 0) {
      add_name = "adminhost";
      del_name = "administrative host";
   } else if (strcmp(group, SUBMIT_HOSTGROUP) == 0) {
      add_name = "submithost";
      del_name = "submit host";
   } else {
      DRETURN(true);
   }

   const bool append = (sub_command & ocs::gdi::SubCommand::APPEND) == ocs::gdi::SubCommand::APPEND;
   const bool remove = (sub_command & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE;

   /* -mhgrp/-Mhgrp replace the list wholesale; there is no per-entry
    * add-or-remove intent to report on, and 9.1 had no equivalent error */
   if (!append && !remove) {
      DRETURN(true);
   }

   ocs::tom::ConstList<ocs::tom::HR> current = ocs::tom::get<HGRP_host_list>(hgroup);

   for (const auto href : delta_list) {
      const char *name = ocs::tom::get<HR_name>(href);

      if (name == nullptr) {
         continue;
      }
      const bool is_member = (href_list_locate(current, name) != nullptr);

      if (append && is_member) {
         ERROR(MSG_SGETEXT_ALREADYEXISTS_SS, add_name, name);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         ret = false;
      } else if (remove && !is_member) {
         /*
          * CS-2438 chunk 4. "Not a direct member" has two very different causes,
          * and reporting both as "does not exist" is what the plan called the
          * worst possible outcome for a security-relevant list: the admin is
          * told the host is not there while it still resolves as an admin or
          * submit host through a nested group. Name the group that grants it,
          * and point at the command that can change the nesting.
          */
         dstring nesting = DSTRING_INIT;

         if (hgroup_nesting_that_contains(current, name, master_hgroup_list, &nesting)) {
            ERROR(MSG_HGRP_RESERVED_NESTED_SSS, name, group, sge_dstring_get_string(&nesting));
         } else {
            ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, del_name, name);
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         ret = false;
         sge_dstring_free(&nesting);
      }
   }

   DRETURN(ret);
}

/** @brief Apply a change of the member list to a host group and work out its effect
 *
 * Resolves and checks the incoming members, merges them into the group and
 * computes which hosts the effective membership gains and loses, and which
 * groups reach this one (for the cycle check and the queues to update).
 *
 * @param hgroup the host group being modified
 * @param answer_list receives messages for the caller
 * @param reduced_elem the reduced element the client sent
 * @param cmd the command being executed, unused
 * @param sub_command what kind of modification this is
 * @param[out] add_hosts receives the hosts the effective membership gains
 * @param[out] rem_hosts receives the hosts the effective membership loses
 * @param[out] occupant_groups receives the groups reaching this one, and this one
 * @return true on success
 */
static bool
hgroup_mod_hostlist(const ocs::tom::Element<ocs::tom::HGRP> hgroup, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                    const ocs::tom::Element<ocs::tom::HGRP> reduced_elem, [[maybe_unused]] const ocs::gdi::Command cmd,
                    const ocs::gdi::SubCommand sub_command, ocs::tom::ListPtr<ocs::tom::HR> &add_hosts,
                    ocs::tom::ListPtr<ocs::tom::HR> &rem_hosts, ocs::tom::ListPtr<ocs::tom::HR> &occupant_groups) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();

   if (hgroup != nullptr && reduced_elem != nullptr) {
      if (ocs::tom::has<HGRP_host_list>(reduced_elem)) {
         const ocs::tom::List<ocs::tom::HR> list = ocs::tom::get<HGRP_host_list>(reduced_elem);
         auto old_href_list = ocs::tom::get<HGRP_host_list>(hgroup).clone("");
         ocs::tom::ConstList<ocs::tom::HR> href_list = nullptr;
         ocs::tom::ListPtr<ocs::tom::HR> add_groups;
         ocs::tom::ListPtr<ocs::tom::HR> rem_groups;

         if (ret) {
            /*
             * CS-2680. This is the last point at which the incoming member list
             * can still be changed - attr_mod_sub_list() below dechains these
             * very elements into the master element. Literal members are
             * resolved here, matchers are normalised and validated here, and
             * both report what was done to them.
             *
             * N-I-10, N-L-20: a removal is normalised like everything else, so
             * that the written form finds the stored one, but it is not judged.
             * This is the only caller that can tell the two apart.
             */
            const bool introducing =
                    (sub_command & ocs::gdi::SubCommand::REMOVE) != ocs::gdi::SubCommand::REMOVE;

            ret &= href_list_resolve_hostnames(list, answer_list, true, introducing, hgroup);
         }
         if (ret) {
            ret &= hgroup_reserved_delta_is_strict(hgroup, answer_list, list, sub_command,
                                                   master_hgroup_list);
         }
         if (ret) {
            attr_mod_sub_list<HGRP_host_list, HR_name>(answer_list, hgroup, reduced_elem, sub_command,
                                                       SGE_ATTR_HOSTLIST, SGE_OBJ_HGROUP);
            /*
             * CS-2680, N-N-5. Members denoting the same set are collapsed into
             * one, and the collapse is reported. Run on the *result* list, so
             * that it covers duplicates within a list submitted as a whole as
             * well as against the existing one - the attribute-wise path
             * already drops the latter, the whole-list path dropped neither.
             */
            href_list_make_uniq(ocs::tom::get<HGRP_host_list>(hgroup), answer_list);
            href_list = ocs::tom::get<HGRP_host_list>(hgroup);
         }
         if (ret) {
            /*
             * CS-2677, N-O-4. The resulting member list is examined rather than
             * the incoming one, so that removing such an entry stays possible
             * while adding one does not.
             */
            for (const auto member : href_list) {
               const char *member_name = ocs::tom::get<HR_name>(member);

               if (hgroup_is_queue_group(member_name)) {
                  ERROR(MSG_HGRP_QUEUEGROUP_NOREF_S, member_name);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  ret = false;
                  break;
               }
            }
         }
         if (ret) {
            ret &= href_list_find_diff(href_list, answer_list, old_href_list.get(),
                                       &add_hosts, &rem_hosts, &add_groups,
                                       &rem_groups);
         }
         if (ret && add_groups != nullptr) {
            ret &= hgroup_list_exists(master_hgroup_list, answer_list, add_groups.get());
         }
         if (ret) {
            ret &= href_list_find_effective_diff(answer_list, add_groups.get(),
                                                 rem_groups.get(), master_hgroup_list,
                                                 &add_hosts, &rem_hosts);
         }
         if (ret) {
            // the hosts this change adds to the effective membership; matchers
            // are not among them, so the distinction does not arise here
            ret &= href_list_resolve_hostnames(add_hosts.get(), answer_list, false, true);
         }

         /*
          * Try to find cycles in the definition
          */
         if (ret) {
            ret &= hgroup_find_all_referencees(hgroup, answer_list,
                                               master_hgroup_list, &occupant_groups);
            ret &= href_list_add(occupant_groups, answer_list,
                                 ocs::tom::get<HGRP_name>(hgroup));
            if (ret) {
               if (occupant_groups != nullptr && add_groups != nullptr) {
                  ocs::tom::ConstElement<ocs::tom::HR> add_group = nullptr;

                  for (const auto group : add_groups.get()) {
                     const char *name = ocs::tom::get<HR_name>(group);

                     if (href_list_has_member(occupant_groups.get(), name)) {
                        add_group = group;
                        break;
                     }
                  }
                  if (add_group == nullptr) {
                     /*
                      * No cycle found => success
                      */
                     ;
                  } else {
                     snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_HGROUP_CYCLEINDEF_SS, static_cast<const char *>(ocs::tom::get<HR_name>(add_group)), static_cast<const char *>(ocs::tom::get<HGRP_name>(hgroup)));
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     ret = false;
                  }
               }
            }
         }

         /*
          * Make sure that:
          *   - added hosts where not already part the old hostlist
          *   - removed hosts are not part of the new hostlist
          */
         if (ret) {
            ocs::tom::ListPtr<ocs::tom::HR> tmp_hosts;

            ret &= href_list_find_all_references(old_href_list.get(), answer_list,
                                                 master_hgroup_list, &tmp_hosts, nullptr);
            ret &= href_list_remove_existing(add_hosts, answer_list, tmp_hosts.get());
            tmp_hosts.reset();

            ret &= href_list_find_all_references(href_list, answer_list,
                                                 master_hgroup_list, &tmp_hosts, nullptr);
            ret &= href_list_remove_existing(rem_hosts, answer_list, tmp_hosts.get());
            tmp_hosts.reset();
         }

#if 1 /* debug */
         if (ret) {
            href_list_debug_print(add_hosts.get(), "add_hosts: ");
            href_list_debug_print(rem_hosts.get(), "rem_hosts: ");
         }
#endif
      }
   }
   DRETURN(ret);
}

/** @brief Make the pending changes to a host group permanent
 *
 * @param hgroup the host group
 * @param gdi_session the session the change belongs to
 */
void
hgroup_commit(const ocs::tom::Element<ocs::tom::HGRP> hgroup, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
   ocs::tom::List<ocs::tom::CQ> cqueue_list = ocs::tom::get<HGRP_cqueue_list>(hgroup);
   ocs::tom::Element<ocs::tom::CQ> next_cqueue = cqueue_list.front();
   ocs::tom::Element<ocs::tom::CQ> cqueue = nullptr;

   while ((cqueue = next_cqueue)) {
      const char *name = ocs::tom::get<CQ_name>(cqueue);
      const ocs::tom::Element<ocs::tom::CQ> org_queue = master_cqueue_list.find_first<CQ_name>(name);

      next_cqueue = cqueue.next();
      cqueue_commit(cqueue, gdi_session);
      auto committed = cqueue_list.extract(cqueue);
      if (org_queue) {
         master_cqueue_list.erase(org_queue);
      }
      master_cqueue_list.push_back(std::move(committed));
   }
   ocs::tom::set<HGRP_cqueue_list>(hgroup, nullptr);
   DRETURN_VOID;
}

/** @brief Discard the pending changes to a host group
 *
 * @param this_elem the object
 */
static void
hgroup_rollback(const ocs::tom::Element<ocs::tom::HGRP> this_elem) {
   DENTER(TOP_LAYER);
   ocs::tom::set<HGRP_cqueue_list>(this_elem, nullptr);
   DRETURN_VOID;
}

/** @brief Discard the pending queue copies a failed host group change built
 *
 * CS-2438 chunk 7: same rollback as hgroup_rollback(), reachable from the
 * `@exec_hosts` sync in `sge_host_qmaster.cc`, which builds the pending queue
 * copies the same way.
 *
 * @param hgroup the host group
 */
void
hgroup_rollback_cqueues(const ocs::tom::Element<ocs::tom::HGRP> hgroup) {
   DENTER(TOP_LAYER);
   hgroup_rollback(hgroup);
   DRETURN_VOID;
}

/** @brief Apply one attribute change to a host group
 *
 * The modifier of host groups, bound in ocs_gdi_hooks<ocs::tom::HGRP>; see ocs_gdi_add_mod() in
 * sge_c_gdi.cc for the sequence it is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param hgroup the host group
 * @param reduced_elem the reduced element the client sent
 * @param add 1 for add, 0 for modify
 * @param remote_user the requesting user
 * @param remote_host the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
hgroup_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
           const ocs::tom::Element<ocs::tom::HGRP> hgroup, const ocs::tom::Element<ocs::tom::HGRP> reduced_elem,
           int add, const char *remote_user, const char *remote_host, gdi_object_t *object, ocs::gdi::Command cmd,
           ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const auto master_hgroup_list = ocs::DataStore::master_list_rw<SGE_TYPE_HGROUP>();
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>();
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();

   /*
    * CS-2451: only HGRP_name and HGRP_host_list are ever read out of
    * reduced_elem -- the element the CLIENT sent. That must stay true.
    *
    * HGRP_cached_hosts is a qmaster-maintained resolution of HGRP_host_list,
    * and hgroup_success() recomputes it from scratch after every write. If any
    * code here started copying the field out of the incoming element instead,
    * a client could name hosts in a group without them being members -- and
    * since RQS scopes and (with CS-2438) the admin/submit host lists resolve
    * through host groups, that is a way to forge host membership rather than
    * merely a stale cache.
    */

   /* Did we get a hostgroupname?  */
   if (ocs::tom::has<HGRP_name>(reduced_elem)) {
      const char *name = ocs::tom::get<HGRP_name>(reduced_elem);

      /*
       * @exec_hosts is derived from the execution host list, so no GDI write
       * may touch it -- for every role including manager (CS-2438; the spec
       * states this independently of RBAC, 04_Logical_View.md "Protected Object
       * Keys"). The qmaster's own maintenance of the group does not run through
       * hgroup_mod(), so it is unaffected by this refusal.
       *
       * Both names are examined: the incoming one, and on modify the name of
       * the group actually being changed, so the group cannot be reached by
       * renaming something else onto it.
       */
      {
         const char *existing_name = add ? nullptr : static_cast<const char *>(ocs::tom::get<HGRP_name>(hgroup));
         const char *reserved = nullptr;

         if (hgroup_is_system_maintained(name)) {
            reserved = name;
         } else if (hgroup_is_system_maintained(existing_name)) {
            reserved = existing_name;
         }
         if (reserved != nullptr) {
            ERROR(MSG_HGRP_RESERVED_READONLY_S, reserved);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }

      if (add) {
         /*
          * CS-2677, N-O-1: a queue host group is created with its cluster
          * queue and by nothing else. hgroup_check_name() would refuse the
          * "@@" name anyway -- as an invalid name, which says nothing about
          * why. This says why, and it is the message the specification names.
          */
         if (hgroup_is_queue_group(name)) {
            ERROR(MSG_HGRP_QUEUEGROUP_NOADD_S, name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /* Check groupname for new hostgroups */
         if (hgroup_check_name(answer_list, name)) {
            ocs::tom::set<HGRP_name>(hgroup, name);
         } else {
            // the answer list API stays raw by design
            for (const auto aep : ocs::tom::ConstList<ocs::tom::AN>(*answer_list)) {
               ERROR(SFNMAX, ocs::tom::get<AN_text>(aep));
            }
            ret = false;
         }

      } else {
         const char *old_name = ocs::tom::get<HGRP_name>(hgroup);

         /* Reject modify requests which try to change the groupname */
         if (sge_hostcmp(old_name, name)) {
            ERROR(SFNMAX, MSG_HGRP_NONAMECHANGE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   } else {
      ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, ocs::tom::FieldTraits<HGRP_name>::info.name, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /*
    * Is there a list of host references
    */
   if (ret) {
      if (ocs::tom::has<HGRP_host_list>(reduced_elem)) {
         ocs::tom::ListPtr<ocs::tom::HR> add_hosts;
         ocs::tom::ListPtr<ocs::tom::HR> rem_hosts;
         ocs::tom::ListPtr<ocs::tom::HR> occupant_groups;

         DPRINTF("got new HGRP_host_list\n");

         /*
          * CS-2680, N-V-5. The warning below is owed only when a matcher is
          * *introduced*, not on every later edit of a group that carries one, so
          * the member list as it stands before the change has to be kept. At
          * this point it still is the one of the stored group: the driver hands
          * the modifier a copy of the old object.
          */
         auto members_before = ocs::tom::get<HGRP_host_list>(hgroup).clone("");

         if (ret) {
            ret &= hgroup_mod_hostlist(hgroup, answer_list, reduced_elem, cmd, sub_command, add_hosts, rem_hosts,
                                       occupant_groups);
         }

         /*
          * The qmaster host must stay a member of @admin_hosts (CS-2438). This
          * ports MSG_SGETEXT_CANTDELADMINQMASTER_S from sge_del_host(), which
          * guarded the same thing while the admin hosts were their own list.
          *
          * Checked on the END STATE, after hgroup_mod_hostlist() has applied the
          * change, rather than by rejecting a particular operation -- the lesson
          * CS-2394 recorded for userset_mod(): every route in (-mhgrp, -Mhgrp,
          * the sub-command paths, and the classic -dh once chunk 3 redirects it)
          * ends here, so one end-state check covers them all and none of them has
          * to be enumerated.
          *
          * Direct membership only: a qmaster host reachable through a nested
          * group would still be an admin host, but the guarantee the old check
          * gave was about the entry itself, and chunk 4 makes nested removal an
          * error of its own.
          */
         if (ret && strcmp(ocs::tom::get<HGRP_name>(hgroup), ADMIN_HOSTGROUP) == 0) {
            const char *qualified_hostname = component_get_qualified_hostname();
            ocs::tom::ConstList<ocs::tom::HR> host_list = ocs::tom::get<HGRP_host_list>(hgroup);

            if (href_list_locate(host_list, qualified_hostname) == nullptr) {
               ERROR(MSG_HGRP_RESERVED_NOQMASTER_SS, qualified_hostname, ADMIN_HOSTGROUP);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }

         bool is_referenced_by_cqueue = false;
         dstring referencing_queues = DSTRING_INIT;

         if (ret) {
            for (const auto cqueue : master_cqueue_list) {
               if (cqueue_is_a_href_referenced(cqueue, occupant_groups.get(), true)) {
                  ocs::tom::Element<ocs::tom::CQ> new_cqueue = nullptr;
                  ocs::tom::ListPtr<ocs::tom::HR> real_add_hosts;
                  ocs::tom::ListPtr<ocs::tom::HR> real_rem_hosts;
                  ocs::tom::ListPtr<ocs::tom::HR> before_mod_list;
                  ocs::tom::ListPtr<ocs::tom::HR> after_mod_list;
                  ocs::tom::ConstList<ocs::tom::HR> href_list = nullptr;
                  const char *name = nullptr;
                  ocs::tom::Element<ocs::tom::HGRP> org_hgroup = nullptr;
                  ocs::tom::ElementPtr<ocs::tom::HGRP> org_hgroup_held;

                  is_referenced_by_cqueue = true;

                  if (sge_dstring_strlen(&referencing_queues) > 0) {
                     sge_dstring_append(&referencing_queues, ", ");
                  }
                  sge_dstring_append(&referencing_queues, ocs::tom::get<CQ_name>(cqueue));

                  /*
                   * Find CQs lists of referenced hosts before and after
                   * the hgroup modification
                   *
                   * Both href_list_find_all_references() calls below MUST walk
                   * the host group tree. They must never be switched to the
                   * resolved-host cache (HGRP_cached_hosts / HGRP_cache_version,
                   * CS-2451): between the lDechainElem() and the rollback further
                   * down, the temporarily inserted element has no valid cache
                   * while the referencing groups still hold the old state. A
                   * cached answer would hand back the pre-modification host set
                   * as after_mod_list, the add/remove delta below would come out
                   * empty, and the CQ would silently keep the wrong queue
                   * instances. See the NOTES at href_list_find_all_references().
                   */

                  name = ocs::tom::get<HGRP_name>(hgroup);
                  org_hgroup = master_hgroup_list.find_first<HGRP_name>(ocs::tom::Host(name));

                  /*
                   * CS-2677: the host list is fetched again after the swap, not
                   * hoisted out of the two calls. For a queue that merely
                   * REFERENCES this group its own list is unaffected and one
                   * fetch would do -- but the host list of a cluster queue is
                   * now the member list of "@@<queue>", and for the queue that
                   * OWNS the group being modified that list is the very thing
                   * that changes. A hoisted pointer still refers to the old
                   * element's list, both resolutions then answer the same, the
                   * difference comes out empty, and the queue silently keeps
                   * queue instances for hosts it no longer reaches.
                   */
                  href_list = cqueue_get_hostlist(cqueue, master_hgroup_list);
                  ret &= href_list_find_all_references(href_list, answer_list, master_hgroup_list, &before_mod_list,
                                                       nullptr);

                  /*
                   * !!! Modify master list temorarily 
                   *     (find rollback below)
                   */
                  if (org_hgroup != nullptr) {
                     org_hgroup_held = master_hgroup_list.extract(org_hgroup);
                  }
                  // the master list only borrows the element until the rollback below, the caller keeps owning it
                  master_hgroup_list.push_back(ocs::tom::ElementPtr<ocs::tom::HGRP>::from_typed(hgroup));
                  href_list = cqueue_get_hostlist(cqueue, master_hgroup_list);
                  ret &= href_list_find_all_references(href_list,
                                                       answer_list,
                                                       master_hgroup_list,
                                                       &after_mod_list,
                                                       nullptr);

                  /*
                   * Find the real set of hosts to be added/removed from
                   * the list of QIs of the CQ
                   */
                  if (ret) {
                     ret &= href_list_compare(rem_hosts.get(), answer_list,
                                              after_mod_list.get(),
                                              &real_rem_hosts,
                                              nullptr, nullptr, nullptr);
                     ret &= href_list_compare(add_hosts.get(), answer_list,
                                              before_mod_list.get(),
                                              &real_add_hosts,
                                              nullptr, nullptr, nullptr);
                  }

                  /*
                   * Make a copy of CQ
                   */
                  if (ret) {
                     const ocs::tom::List<ocs::tom::CQ> cqueue_list =
                             ocs::tom::get_or_create<HGRP_cqueue_list>(hgroup, "");

                     new_cqueue = cqueue_list.push_back(cqueue.clone());
                  }

                  /*
                   * Mopdify QIs of CQ
                   */
                  if (ret) {
                     bool refresh_all_values = ((add_hosts != nullptr) || (rem_hosts != nullptr)) ? true : false;

                     ret &= cqueue_handle_qinstances(packet, task, new_cqueue, answer_list, reduced_elem,
                                                     real_add_hosts.get(), real_rem_hosts.get(), refresh_all_values,
                                                     monitor, master_hgroup_list, master_cqueue_list);
                  }

                  /*
                   * Free all temorarily allocated memory
                   */
                  after_mod_list.reset();
                  before_mod_list.reset();
                  real_add_hosts.reset();
                  real_rem_hosts.reset();

                  /*
                   * !!! Rollback of masterlist modification
                   * (the borrowed element goes back to the caller without being freed)
                   */
                  static_cast<void>(master_hgroup_list.extract(hgroup).release());
                  if (org_hgroup_held != nullptr) {
                     master_hgroup_list.push_back(std::move(org_hgroup_held));
                  }

                  /*
                   * Skip other CQs if this failed
                   */
                  if (!ret) {
                     break;
                  }
               }
            }
            if (!ret) {
               hgroup_rollback(hgroup);
            }
         }

         /*
          * Client and scheduler code expects existing EH_Type elements
          * for all hosts used in CQ_hostlist. Therefore it is neccessary
          * to create all not existing EH_Type elements.
          *
          * CS-2438: only when a cluster queue actually references this group.
          * The EH_Type elements exist to back the queue instances created in the
          * loop above; a group that no queue references produces no instances
          * and needs none. Creating them unconditionally turned every member of
          * the reserved "@admin_hosts"/"@submit_hosts" groups into an execution
          * host as a side effect of "qconf -ah"/"-as" -- which 9.1 never did,
          * because the flat AH_LIST/SH_LIST never went through this path -- and
          * those phantom hosts then also appeared in "@exec_hosts", which is
          * derived from the execution host list.
          *
          * A queue that starts referencing the group later is covered by the
          * cqueue path: cqueue_mod_hostlist() resolves added group references to
          * their effective host set (href_list_find_effective_diff()) and calls
          * host_list_add_missing_href() with it.
          */
         /*
          * CS-2680, N-V-5 and N-V-6. Both warnings are owed when a matcher is
          * *introduced* into a group, and neither on a later edit of a group
          * that already carries one, nor when one is taken out, nor on a rewrite
          * that changes nothing. That distinction is a statement about the write
          * as a whole, so it is made here and not where a member is validated:
          * that place sees one member at a time and cannot tell an addition from
          * a removal.
          *
          * What each one names is the reach the administrator has just granted.
          * In a group a cluster queue reaches, every host the matcher captures
          * receives a queue instance; in the scope of a reserved host group,
          * anything that can obtain a matching name becomes an administrative or
          * a submit host. The change is carried out either way.
          *
          * CS-2690: the reach is what is warned about, so the question is one of
          * SCOPE and not of identity. A matcher in a group that "@admin_hosts"
          * references grants exactly what one written into "@admin_hosts" grants,
          * and it is the way an administrator would naturally organise this.
          * Asking only whether this group IS the reserved one left that silent -
          * while the queue warning beside it had the transitive case right from
          * the start, from the same occupant_groups set. "@submit_hosts" was
          * silent in every order, because only "@admin_hosts" was ever compared.
          *
          * occupant_groups holds every group that reaches this one, transitively,
          * and this group itself (see the cycle check above), so one lookup
          * answers both. Which of the four texts is used follows from the
          * distinction the reader needs: a message saying that "@admin_hosts now
          * contains a matcher" would be false when the matcher sits three groups
          * away, and one naming the path would be noise when it does not.
          */
         if (ret) {
            const char *group_name = ocs::tom::get<HGRP_name>(hgroup);
            const bool is_admin  = group_name != nullptr && strcmp(group_name, ADMIN_HOSTGROUP) == 0;
            const bool is_submit = group_name != nullptr && strcmp(group_name, SUBMIT_HOSTGROUP) == 0;
            const bool via_admin  = !is_admin  && href_list_has_member(occupant_groups.get(), ADMIN_HOSTGROUP);
            const bool via_submit = !is_submit && href_list_has_member(occupant_groups.get(), SUBMIT_HOSTGROUP);

            for (ocs::tom::ConstElement<ocs::tom::HR> member : ocs::tom::get<HGRP_host_list>(hgroup)) {
               const char *member_name = ocs::tom::get<HR_name>(member);

               if (!ocs::is_matcher(member_name) ||
                   href_list_locate(members_before.get(), member_name) != nullptr) {
                  continue;
               }
               const char *payload = ocs::matcher_payload(member_name);

               if (is_referenced_by_cqueue) {
                  answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_WARNING,
                                          MSG_MATCHER_IN_QUEUE_GROUP_SSS,
                                          group_name != nullptr ? group_name : "", member_name,
                                          sge_dstring_get_string(&referencing_queues));
               }
               if (is_admin) {
                  answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_WARNING,
                                          MSG_MATCHER_IN_ADMIN_HOSTS_SSS, group_name,
                                          payload, member_name);
               } else if (via_admin) {
                  answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_WARNING,
                                          MSG_MATCHER_VIA_ADMIN_HOSTS_SSSS, group_name,
                                          member_name, ADMIN_HOSTGROUP, payload);
               }
               if (is_submit) {
                  answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_WARNING,
                                          MSG_MATCHER_IN_SUBMIT_HOSTS_SSS, group_name,
                                          payload, member_name);
               } else if (via_submit) {
                  answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_WARNING,
                                          MSG_MATCHER_VIA_SUBMIT_HOSTS_SSSS, group_name,
                                          member_name, SUBMIT_HOSTGROUP, payload);
               }
            }
         }

         if (ret && is_referenced_by_cqueue) {
            ret &= host_list_add_missing_href(packet, task, master_ehost_list, answer_list, add_hosts.get(), monitor);
         }

         sge_dstring_free(&referencing_queues);
      }
   }

   if (ret) {
      DRETURN(0);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

/** @brief Delete a host group
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param this_elem the object
 * @param answer_list receives messages for the caller
 * @param remote_user the requesting user
 * @param remote_host the requesting host
 * @return STATUS_OK on success
 */
int
hgroup_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::HGRP> this_elem,
           ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char *remote_user, char *remote_host) {
   DENTER(TOP_LAYER);

   int ret = true;
   const auto master_hgroup_list = ocs::DataStore::master_list_rw<SGE_TYPE_HGROUP>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

   /*
    * Check all incoming parameter
    */
   if (this_elem != nullptr && remote_user != nullptr && remote_host != nullptr) {
      const char *name = ocs::tom::get<HGRP_name>(this_elem);

      /*
       * What is the name ob the hostgroup which should be removed?
       */
      if (name != nullptr) {

         /*
          * The reserved host groups back the admin/submit host lists and the
          * execution host list (CS-2438). Deleting one would drop every admin
          * or submit host at once, so it is refused here -- the same guard the
          * reserved usersets get in sge_del_userset(). Checked before the
          * existence lookup so the answer does not depend on whether the group
          * happens to be spooled yet.
          *
          * CS-2763: this reaches a folded spelling too. hgroup_is_reserved()
          * used to compare with strcmp(), so "@ADMIN_HOSTS" walked past this
          * guard, was found by the lookup below -- HGRP_name is an SGE_HOST
          * field -- and got as far as the spool call, which failed only because
          * the file carries the stored spelling. That accident was the only
          * thing standing between a manager and the deletion of every admin
          * host at once; the predicate now folds case, as the object layer does.
          */
         if (hgroup_is_reserved(name)) {
            ERROR(MSG_HGRP_RESERVED_NODELETE_S, name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /*
          * CS-2677, N-O-2: likewise for a queue host group. It is removed with
          * its cluster queue, and removing it on its own would leave a queue
          * without the object holding its host list.
          */
         if (hgroup_is_queue_group(name)) {
            ERROR(MSG_HGRP_QUEUEGROUP_NODEL_S, name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /*
          * Does this hostgroup exist?
          */
         const ocs::tom::Element<ocs::tom::HGRP> hgroup = hgroup_list_locate(master_hgroup_list, name);
         if (hgroup != nullptr) {
            ocs::tom::ListPtr<ocs::tom::HR> href_list;
            ocs::tom::ListPtr<ocs::tom::ST> qref_list;

            /*
             * Is it still referenced in another hostgroup or cqueue?
             */
            ret &= hgroup_find_referencees(hgroup, answer_list, master_hgroup_list,
                                           master_cqueue_list, &href_list, &qref_list);
            if (ret) {
               if (href_list != nullptr) {
                  dstring string = DSTRING_INIT;

                  href_list_append_to_dstring(href_list.get(), &string);
                  ERROR(MSG_HGROUP_REFINHGOUP_SS, name, sge_dstring_get_string(&string));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  sge_dstring_free(&string);
                  ret = false;
               }
               if (qref_list != nullptr) {
                  dstring string = DSTRING_INIT;

                  str_list_append_to_dstring(qref_list.get(), &string, ' ');
                  ERROR(MSG_CQUEUE_REFINHGOUP_SS, name, sge_dstring_get_string(&string));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  sge_dstring_free(&string);
                  ret = false;
               }
            }
            href_list.reset();
            qref_list.reset();

            /*
             * Try to unlink the concerned spoolfile
             *
             * CS-2763: with the STORED name, not the one the request carried.
             * HGRP_name is an SGE_HOST field, so hgroup_list_locate() above
             * finds the group under any spelling -- but the spool file and the
             * event key are the stored spelling, and handing this call
             * "@MixedCase" as "@mixedcase" made the unlink miss a file that was
             * there and refused a delete that should have succeeded. The same
             * name is the key of the sgeE_HGROUP_DEL event, where a wrong
             * spelling would tell every event client about an object it does
             * not hold under that name.
             */
            if (ret) {
               /*
                * A COPY, not the pointer into the element: lRemoveElem() below
                * frees the element, and the message after it would then read
                * freed memory. It did -- "qconf -dhgrp @fake_hostgroup" reported
                * removing "C", whatever happened to be left at that address.
                */
               dstring stored_name_dstring = DSTRING_INIT;
               sge_dstring_copy_string(&stored_name_dstring, ocs::tom::get<HGRP_name>(hgroup));
               const char *stored_name = sge_dstring_get_string(&stored_name_dstring);

               if (sge_event_spool(answer_list, 0, sgeE_HGROUP_DEL,
                                   0, 0, stored_name, nullptr, nullptr, nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {
                  /*
                   * Let's remove the object => Success!
                   */

                  master_hgroup_list.erase(hgroup);

                  INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, remote_user, remote_host, stored_name, "host group entry");
                  answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
               } else {
                  ERROR(MSG_CANTSPOOL_SS, "host group entry", stored_name);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
               sge_dstring_free(&stored_name_dstring);
            }
         } else {
            ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, "host group", name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      } else {
         ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, ocs::tom::FieldTraits<HGRP_name>::info.name, __func__);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   } else {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   if (ret) {
      DRETURN(STATUS_OK);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

/**
 * @brief Recompute this group's cache and its referencees'
 *
 * CS-2451: the resolved host list of this group just changed, and with it
 * that of every group referencing it -- transitively, since `@a` may contain
 * `@b` which contains `@c`. Refresh all of them BEFORE any event goes out, so
 * no event carries a cache that disagrees with the master list.
 * The caller must have chained the new object into the master list already,
 * so resolving here sees the new state.
 *
 * @param hgroup the group whose membership changed
 * @param master_hgroup_list the (already updated) master list
 * @param[in,out] referencees HR_Type names of the refreshed referencees; the CALLER keeps it, because it still
 *        needs them to send one event each
 *
 * @note MT-NOTE: call under the write lock, like every other writer of the list
 */
void hgroup_refresh_caches(const ocs::tom::Element<ocs::tom::HGRP> hgroup,
                           const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                           ocs::tom::ListPtr<ocs::tom::HR> &referencees) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> cache_answer_list;

   hgroup_update_cache(hgroup, &cache_answer_list, master_hgroup_list);
   if (hgroup_find_all_referencees(hgroup, &cache_answer_list, master_hgroup_list, &referencees)) {
      /* A diamond (@top -> @a, @b -> @x) used to list @x once per path; since
       * CS-2755 href_list_find_all_referencees() merges entry by entry and no
       * longer produces the duplicate. Kept as a backstop, and because this list
       * goes on to the caller, which sends one event per entry. */
      referencees.get().unique<HR_name>();

      for (const auto href : referencees.get()) {
         const ocs::tom::Element<ocs::tom::HGRP> referencee =
                 hgroup_list_locate(master_hgroup_list, ocs::tom::get<HR_name>(href));

         if (referencee != nullptr) {
            hgroup_update_cache(referencee, &cache_answer_list, master_hgroup_list);
         }
      }
   }
   answer_list_output(&cache_answer_list);

   DRETURN_VOID;
}

/**
 * @brief Recompute every group that carries a matcher, after the candidate set moved
 *
 * CS-2680, N-C-1. The resolved membership of a matcher-carrying group is the set
 * of execution hosts its patterns capture, so it depends on the execution host
 * list even when the group references nothing that mirrors it. Today's triggers
 * do not cover that case: they follow references, and there is no reference to
 * follow.
 *
 * This is what closes the roll-out. A node that registers becomes an execution
 * host, and with it a member of every matcher-carrying group whose pattern fits
 * it - with no administrative act anywhere in between.
 *
 * A group carrying no matcher is not touched (N-C-3), so a cluster that
 * configures none pays a prefix comparison over groups times members and nothing
 * else. Whether a group carries one is derived rather than stored: a stored flag
 * would be redundant state that can drift from the member list.
 *
 * @param master_hgroup_list the (already updated) master list
 * @param[in,out] refreshed HR_Type names of every group refreshed here and of its
 *        referencees, appended to what the caller already collected; the CALLER
 *        keeps it and sends one event per name
 *
 * @note MT-NOTE: call under the write lock, like every other writer of the list
 *
 * @see #hgroup_refresh_caches
 */
void hgroup_refresh_matcher_caches(const ocs::tom::List<ocs::tom::HGRP> master_hgroup_list,
                                   ocs::tom::ListPtr<ocs::tom::HR> &refreshed) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   for (const auto hgroup : master_hgroup_list) {
      const char *group_name = ocs::tom::get<HGRP_name>(hgroup);
      bool carries_matcher = false;

      for (ocs::tom::ConstElement<ocs::tom::HR> member : ocs::tom::get<HGRP_host_list>(hgroup)) {
         if (ocs::is_matcher(ocs::tom::get<HR_name>(member))) {
            carries_matcher = true;
            break;
         }
      }
      if (!carries_matcher) {
         continue;
      }

      /* the group itself and, through the existing refresh, everything that
       * reaches it */
      ocs::tom::ListPtr<ocs::tom::HR> referencees;

      hgroup_refresh_caches(hgroup, master_hgroup_list, referencees);
      href_list_add(refreshed, &answer_list, group_name);

      for (const auto href : referencees.get()) {
         href_list_add(refreshed, &answer_list, ocs::tom::get<HR_name>(href));
      }
   }
   answer_list_output(&answer_list);

   DRETURN_VOID;
}

/**
 * @brief One HGROUP_MOD per refreshed referencee
 *
 * CS-2451. One event per referencee so the mirrors pick up their refreshed
 * cache. sge_add_event(), not sge_event_spool(): HGRP_cached_hosts is not
 * spooled and nothing in the referencees' own configuration changed, so
 * rewriting their spool files would be pure I/O.
 * Send these AFTER the event for the group that actually changed.
 *
 * @param referencees the host groups whose cache was refreshed (`HR_Type`)
 * @param master_hgroup_list the master host group list
 * @param gdi_session the session the change belongs to
 */
void hgroup_send_referencee_events(const ocs::tom::ConstList<ocs::tom::HR> referencees,
                                   const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                                   const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   for (const auto href : referencees) {
      const char *ref_name = ocs::tom::get<HR_name>(href);
      const ocs::tom::Element<ocs::tom::HGRP> referencee = hgroup_list_locate(master_hgroup_list, ref_name);

      if (referencee != nullptr) {
         sge_add_event(0, sgeE_HGROUP_MOD, 0, 0, ref_name, nullptr, nullptr, referencee, gdi_session);
      }
   }
   DRETURN_VOID;
}

/** @brief Announce a host group change once it is safely spooled
 *
 * The on_success of host groups, bound in ocs_gdi_hooks<ocs::tom::HGRP> (sge_c_gdi.cc).
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param hgroup the host group
 * @param old_hgroup the host group as it was, or an empty handle on add
 * @param object the table entry for this object type
 * @param post what is left for after the last object of the request, unused
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
hgroup_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::HGRP> hgroup,
               const ocs::tom::ConstElement<ocs::tom::HGRP> old_hgroup, gdi_object_t *object,
               [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *name = ocs::tom::get<HGRP_name>(hgroup);
   const auto master_hgroup_list = ocs::DataStore::master_list_rw<SGE_TYPE_HGROUP>();
   ocs::tom::ListPtr<ocs::tom::HR> referencees;

   hgroup_refresh_caches(hgroup, master_hgroup_list, referencees);

   /* we will have the cqueue_list in the final event */
   auto cqueue_list = hgroup.exchange<HGRP_cqueue_list>(ocs::tom::ListPtr<ocs::tom::CQ>());
   /*
    * HGRP modify or add event
    */
   sge_add_event(0, old_hgroup ? sgeE_HGROUP_MOD : sgeE_HGROUP_ADD, 0, 0, name, nullptr, nullptr, hgroup,
                 packet->gdi_session);

   static_cast<void>(hgroup.exchange<HGRP_cqueue_list>(std::move(cqueue_list)));

   hgroup_send_referencee_events(referencees.get(), master_hgroup_list, packet->gdi_session);
   referencees.reset();

   /*
    * QI add or delete events. Finalize operation.
    */
   hgroup_commit(hgroup, packet->gdi_session);

   DRETURN(0);
}


/** @brief Write a host group to the spool
 *
 * The writer of host groups, bound in ocs_gdi_hooks<ocs::tom::HGRP> (sge_c_gdi.cc).
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param answer_list receives messages for the caller
 * @param this_elem the object
 * @param object the table entry for this object type
 * @return 0 on success
 */
int
hgroup_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
             const ocs::tom::Element<ocs::tom::HGRP> this_elem, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   bool tmp_ret = true;
   bool dbret;
   const char *name = ocs::tom::get<HGRP_name>(this_elem);
   ocs::tom::ConstList<ocs::tom::CQ> cqueue_list = ocs::tom::get<HGRP_cqueue_list>(this_elem);
   dstring key_dstring = DSTRING_INIT;
   ocs::tom::ListPtr<ocs::tom::AN> spool_answer_list;

   /* start a transaction for spooling of all affected objects */
   dbret = spool_transaction(&spool_answer_list, spool_get_default_context(),
                             STC_begin);
   answer_list_output(&spool_answer_list);
   if (!dbret) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_OPENTRANSACTION_FAILED);
      tmp_ret = false;
   }

   if (tmp_ret) {
      for (const auto cqueue : cqueue_list) {
         ocs::tom::ConstList<ocs::tom::QU> qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
         const char *cqname = ocs::tom::get<CQ_name>(cqueue);

         for (const auto qinstance : qinstance_list) {
            uint32_t tag = ocs::tom::get<QU_tag>(qinstance);

            if (tag == SGE_QI_TAG_ADD || tag == SGE_QI_TAG_MOD) {
               const char *key = sge_dstring_sprintf(&key_dstring, "%s/%s", cqname,
                                                     static_cast<const char *>(ocs::tom::get<QU_qhostname>(qinstance)));
               dbret = spool_write_object(&spool_answer_list, spool_get_default_context(),
                                          qinstance, key, SGE_TYPE_QINSTANCE);
               answer_list_output(&spool_answer_list);

               if (!dbret) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_PERSISTENCE_WRITE_FAILED_S, key);
                  tmp_ret = false;
                  break;
               }
            }
         }
      }
   }

   sge_dstring_free(&key_dstring);

   if (tmp_ret) {
      dbret = spool_write_object(&spool_answer_list, spool_get_default_context(),
                                 this_elem, name, SGE_TYPE_HGROUP);
      answer_list_output(&spool_answer_list);
      if (!dbret) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_PERSISTENCE_WRITE_FAILED_S, name);
         tmp_ret = false;
      }
   }

   /* commit or rollback database transaction */
   dbret = spool_transaction(&spool_answer_list, spool_get_default_context(),
                             tmp_ret ? STC_commit : STC_rollback);
   answer_list_output(&spool_answer_list);
   if (!dbret) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR, MSG_PERSISTENCE_CLOSINGTRANSACTION_FAILED);
      tmp_ret = false;
   }

   /* commit or rollback hostgroup action */
   if (!tmp_ret) {
      hgroup_rollback(this_elem);
   }

   DRETURN(tmp_ret ? 0 : 1);
}

