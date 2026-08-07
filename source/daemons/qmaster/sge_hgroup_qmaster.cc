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
#include "sgeobj/sge_href.h"
#include "sgeobj/ocs_DataStore.h"

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
hgroup_mod_hostlist(lListElem *hgroup, lList **answer_list, lListElem *reduced_elem,
                    ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
                    lList **add_hosts, lList **rem_hosts, lList **occupant_groups);

static void
hgroup_rollback(lListElem *this_elem);

/****** qmaster/hgroup/hgroup_nesting_that_contains() *************************
*  NAME
*     hgroup_nesting_that_contains() -- which nested group reaches this host?
*
*  FUNCTION
*     CS-2438 chunk 4. Walks the DIRECT entries of a reserved group, and for
*     each "@group" reference among them asks whether that group contains the
*     host. Appends the names of those that do, comma-separated.
*
*     Only used to build an error message, so it is allowed to be the slow path.
*
*  INPUTS
*     const lList *current            - direct HR_Type entries of the reserved group
*     const char *hostname            - host that could not be removed directly
*     const lList *master_hgroup_list - list the references resolve against
*     dstring *groups                 - receives the comma-separated names
*
*  RESULT
*     bool - true if at least one nested group contains the host
*******************************************************************************/
static bool
hgroup_nesting_that_contains(const lList *current, const char *hostname,
                             const lList *master_hgroup_list, dstring *groups)
{
   const lListElem *href;
   bool found = false;

   for_each_ep(href, current) {
      const char *entry = lGetHost(href, HR_name);

      if (entry == nullptr || !ocs::is_hgroup_name(entry)) {
         continue;
      }
      const lListElem *nested = hgroup_list_locate(master_hgroup_list, entry);

      if (hgroup_contains_host(nested, hostname, master_hgroup_list)) {
         if (found) {
            sge_dstring_append(groups, ", ");
         }
         sge_dstring_append(groups, entry);
         found = true;
      }
   }

   return found;
}

/****** qmaster/hgroup/hgroup_reserved_delta_is_strict() **********************
*  NAME
*     hgroup_reserved_delta_is_strict() -- 9.1 membership errors for -ah/-dh/-as/-ds
*
*  FUNCTION
*     CS-2438. attr_mod_sub_list() is deliberately TOLERANT: appending a member
*     that is already there, or removing one that is not, is reported as an INFO
*     answer with STATUS_OK and the request otherwise succeeds. That is right for
*     "qconf -aattr" on an ordinary host group, and it is what made this check
*     necessary -- once chunk 3 redirected "qconf -ah/-dh/-as/-ds" onto that
*     path, both cases started exiting 0 where 9.1 exited 1, and "-ah" reported
*     a host as added that it had not added.
*
*     Silently succeeding is the wrong answer for these two groups in
*     particular. They are the admin and submit host lists: a script that says
*     "qconf -ds host || alert" has to keep failing when the removal did not
*     happen.
*
*     Scoped to @admin_hosts and @submit_hosts, so "-aattr/-dattr" on every
*     other host group keeps the tolerant behaviour it has always had.
*     @exec_hosts needs no entry -- it is system-maintained and every write to
*     it is refused earlier.
*
*     Checked against the PRE-state, unlike the qmaster-host guard below which
*     checks the end state: "was this host already a member" is a question about
*     the state before the merge, and after attr_mod_sub_list() has run the two
*     cases are indistinguishable from a successful one. Called after
*     href_list_resolve_hostnames() so a short name and its FQDN compare equal.
*
*  INPUTS
*     const lListElem *hgroup           - the group as it is BEFORE the merge
*     lList **answer_list               - for returning the errors
*     const lList *delta_list           - HR_Type entries the request carries
*     ocs::gdi::SubCommand sub_command  - APPEND, REMOVE or neither
*     const lList *master_hgroup_list   - to resolve nested references (chunk 4)
*
*  RESULT
*     bool - false if any entry was already/not a member; the request must fail
*
*  NOTES
*     The two messages are not symmetric, and that is not a slip: 9.1 produced
*     MSG_SGETEXT_ALREADYEXISTS_SS with the GDI object name ("adminhost") from
*     sge_c_gdi.cc and MSG_SGETEXT_DOESNOTEXIST_SS with the spelt-out name
*     ("administrative host") from sge_del_host(). Both are reproduced as they
*     were rather than unified, so no script parsing either one has to change.
*******************************************************************************/
static bool
hgroup_reserved_delta_is_strict(const lListElem *hgroup, lList **answer_list,
                                const lList *delta_list, ocs::gdi::SubCommand sub_command,
                                const lList *master_hgroup_list)
{
   const char *add_name;   /* as sge_c_gdi.cc named it   */
   const char *del_name;   /* as sge_del_host() named it */
   bool ret = true;

   DENTER(TOP_LAYER);

   const char *group = lGetHost(hgroup, HGRP_name);
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

   const lList *current = lGetList(hgroup, HGRP_host_list);
   const lListElem *href;

   for_each_ep(href, delta_list) {
      const char *name = lGetHost(href, HR_name);

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

static bool
hgroup_mod_hostlist(lListElem *hgroup, lList **answer_list, lListElem *reduced_elem,
                    ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, lList **add_hosts,
                    lList **rem_hosts, lList **occupant_groups) {
   bool ret = true;
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);

   DENTER(TOP_LAYER);
   if (hgroup != nullptr && reduced_elem != nullptr) {
      int pos = lGetPosViaElem(reduced_elem, HGRP_host_list, SGE_NO_ABORT);

      if (pos >= 0) {
         lList *list = lGetPosList(reduced_elem, pos);
         lList *old_href_list = lCopyList("", lGetList(hgroup, HGRP_host_list));
         const lList *href_list = nullptr;
         lList *add_groups = nullptr;
         lList *rem_groups = nullptr;

         if (ret) {
            ret &= href_list_resolve_hostnames(list, answer_list, true);
         }
         if (ret) {
            ret &= hgroup_reserved_delta_is_strict(hgroup, answer_list, list, sub_command,
                                                   master_hgroup_list);
         }
         if (ret) {
            attr_mod_sub_list(answer_list, hgroup, HGRP_host_list, HR_name,
                              reduced_elem, cmd, sub_command, SGE_ATTR_HOSTLIST,
                              SGE_OBJ_HGROUP, 0, nullptr);
            href_list = lGetList(hgroup, HGRP_host_list);
         }
         if (ret) {
            ret &= href_list_find_diff(href_list, answer_list, old_href_list,
                                       add_hosts, rem_hosts, &add_groups,
                                       &rem_groups);
         }
         if (ret && add_groups != nullptr) {
            ret &= hgroup_list_exists(master_hgroup_list, answer_list, add_groups);
         }
         if (ret) {
            ret &= href_list_find_effective_diff(answer_list, add_groups,
                                                 rem_groups, master_hgroup_list,
                                                 add_hosts, rem_hosts);
         }
         if (ret) {
            ret &= href_list_resolve_hostnames(*add_hosts, answer_list, false);
         }

         /*
          * Try to find cycles in the definition
          */
         if (ret) {
            ret &= hgroup_find_all_referencees(hgroup, answer_list,
                                               master_hgroup_list, occupant_groups);
            ret &= href_list_add(occupant_groups, answer_list,
                                 lGetHost(hgroup, HGRP_name));
            if (ret) {
               if (*occupant_groups != nullptr && add_groups != nullptr) {
                  const lListElem *add_group = nullptr;

                  for_each_ep(add_group, add_groups) {
                     const char *name = lGetHost(add_group, HR_name);

                     if (href_list_has_member(*occupant_groups, name)) {
                        break;
                     }
                  }
                  if (add_group == nullptr) {
                     /*
                      * No cycle found => success
                      */
                     ;
                  } else {
                     snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_HGROUP_CYCLEINDEF_SS, lGetHost(add_group, HR_name), lGetHost(hgroup, HGRP_name));
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
            lList *tmp_hosts = nullptr;

            ret &= href_list_find_all_references(old_href_list, answer_list,
                                                 master_hgroup_list, &tmp_hosts, nullptr);
            ret &= href_list_remove_existing(add_hosts, answer_list, tmp_hosts);
            lFreeList(&tmp_hosts);

            ret &= href_list_find_all_references(href_list, answer_list,
                                                 master_hgroup_list, &tmp_hosts, nullptr);
            ret &= href_list_remove_existing(rem_hosts, answer_list, tmp_hosts);
            lFreeList(&tmp_hosts);
         }

#if 1 /* debug */
         if (ret) {
            href_list_debug_print(*add_hosts, "add_hosts: ");
            href_list_debug_print(*rem_hosts, "rem_hosts: ");
         }
#endif

         /*
          * Cleanup
          */
         lFreeList(&old_href_list);
         lFreeList(&add_groups);
         lFreeList(&rem_groups);
      }
   }
   DRETURN(ret);
}

void
hgroup_commit(lListElem *hgroup, uint64_t gdi_session) {
   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   lList *cqueue_list = lGetListRW(hgroup, HGRP_cqueue_list);
   lListElem *next_cqueue = nullptr;
   lListElem *cqueue = nullptr;

   DENTER(TOP_LAYER);
   next_cqueue = lFirstRW(cqueue_list);
   while ((cqueue = next_cqueue)) {
      const char *name = lGetString(cqueue, CQ_name);
      lListElem *org_queue = lGetElemStrRW(master_cqueue_list, CQ_name, name);

      next_cqueue = lNextRW(cqueue);
      cqueue_commit(cqueue, gdi_session);
      lDechainElem(cqueue_list, cqueue);
      lRemoveElem(master_cqueue_list, &org_queue);
      lAppendElem(master_cqueue_list, cqueue);
   }
   lSetList(hgroup, HGRP_cqueue_list, nullptr);
   DRETURN_VOID;
}

static void
hgroup_rollback(lListElem *this_elem) {
   DENTER(TOP_LAYER);
   lSetList(this_elem, HGRP_cqueue_list, nullptr);
   DRETURN_VOID;
}

/* CS-2438 chunk 7: same rollback, reachable from the @exec_hosts sync in
 * sge_host_qmaster.cc, which builds the pending queue copies the same way. */
void
hgroup_rollback_cqueues(lListElem *hgroup) {
   hgroup_rollback(hgroup);
}

int
hgroup_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **answer_list, lListElem *hgroup, lListElem *reduced_elem, int add,
           const char *remote_user, const char *remote_host, gdi_object_t *object,
           ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
           monitoring_t *monitor) {
   bool ret = true;
   int pos;
   lList *master_hgroup_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_HGROUP);
   lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);

   DENTER(TOP_LAYER);

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
   pos = lGetPosViaElem(reduced_elem, HGRP_name, SGE_NO_ABORT);
   if (pos >= 0) {
      const char *name = lGetPosHost(reduced_elem, pos);

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
         const char *existing_name = add ? nullptr : lGetHost(hgroup, HGRP_name);
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
         /* Check groupname for new hostgroups */
         if (hgroup_check_name(answer_list, name)) {
            lSetHost(hgroup, HGRP_name, name);
         } else {
            const lListElem *aep;
            for_each_ep(aep, *answer_list) {
               ERROR(SFNMAX, lGetString(aep, AN_text));
            }
            ret = false;
         }

      } else {
         const char *old_name = lGetHost(hgroup, HGRP_name);

         /* Reject modify requests which try to change the groupname */
         if (sge_hostcmp(old_name, name)) {
            ERROR(SFNMAX, MSG_HGRP_NONAMECHANGE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   } else {
      ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(HGRP_name), __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /*
    * Is there a list of host references
    */
   if (ret) {
      pos = lGetPosViaElem(reduced_elem, HGRP_host_list, SGE_NO_ABORT);
      if (pos >= 0) {
         lList *add_hosts = nullptr;
         lList *rem_hosts = nullptr;
         lList *occupant_groups = nullptr;

         DPRINTF("got new HGRP_host_list\n");

         if (ret) {
            ret &= hgroup_mod_hostlist(hgroup, answer_list, reduced_elem, cmd, sub_command, &add_hosts, &rem_hosts, &occupant_groups);
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
         if (ret && strcmp(lGetHost(hgroup, HGRP_name), ADMIN_HOSTGROUP) == 0) {
            const char *qualified_hostname = component_get_qualified_hostname();
            const lList *host_list = lGetList(hgroup, HGRP_host_list);

            if (href_list_locate(host_list, qualified_hostname) == nullptr) {
               ERROR(MSG_HGRP_RESERVED_NOQMASTER_SS, qualified_hostname, ADMIN_HOSTGROUP);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }

         bool is_referenced_by_cqueue = false;

         if (ret) {
            const lListElem *cqueue;

            for_each_ep(cqueue, master_cqueue_list) {
               if (cqueue_is_a_href_referenced(cqueue, occupant_groups, true)) {
                  lListElem *new_cqueue = nullptr;
                  lList *real_add_hosts = nullptr;
                  lList *real_rem_hosts = nullptr;
                  lList *before_mod_list = nullptr;
                  lList *after_mod_list = nullptr;
                  const lList *href_list = nullptr;
                  const char *name = nullptr;
                  lListElem *org_hgroup = nullptr;

                  is_referenced_by_cqueue = true;

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

                  href_list = lGetList(cqueue, CQ_hostlist);
                  name = lGetHost(hgroup, HGRP_name);
                  org_hgroup = lGetElemHostRW(master_hgroup_list, HGRP_name, name);

                  ret &= href_list_find_all_references(href_list, answer_list, master_hgroup_list, &before_mod_list,
                                                       nullptr);

                  /*
                   * !!! Modify master list temorarily 
                   *     (find rollback below)
                   */
                  if (org_hgroup != nullptr) {
                     lDechainElem(master_hgroup_list, org_hgroup);
                  }
                  lAppendElem(master_hgroup_list, hgroup);
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
                     ret &= href_list_compare(rem_hosts, answer_list,
                                              after_mod_list,
                                              &real_rem_hosts,
                                              nullptr, nullptr, nullptr);
                     ret &= href_list_compare(add_hosts, answer_list,
                                              before_mod_list,
                                              &real_add_hosts,
                                              nullptr, nullptr, nullptr);
                  }

                  /*
                   * Make a copy of CQ
                   */
                  if (ret) {
                     lList *cqueue_list = lGetListRW(hgroup, HGRP_cqueue_list);

                     if (cqueue_list == nullptr) {
                        cqueue_list = lCreateList("", CQ_Type);
                        lSetList(hgroup, HGRP_cqueue_list, cqueue_list);
                     }
                     new_cqueue = lCopyElem(cqueue);
                     if (new_cqueue != nullptr && cqueue_list != nullptr) {
                        lAppendElem(cqueue_list, new_cqueue);
                     } else {
                        ret = false;
                     }
                  }

                  /*
                   * Mopdify QIs of CQ
                   */
                  if (ret) {
                     bool refresh_all_values = ((add_hosts != nullptr) || (rem_hosts != nullptr)) ? true : false;

                     ret &= cqueue_handle_qinstances(packet, task, new_cqueue, answer_list, reduced_elem,
                                                     real_add_hosts, real_rem_hosts, refresh_all_values, monitor,
                                                     master_hgroup_list, master_cqueue_list);
                  }

                  /*
                   * Free all temorarily allocated memory
                   */
                  lFreeList(&after_mod_list);
                  lFreeList(&before_mod_list);
                  lFreeList(&real_add_hosts);
                  lFreeList(&real_rem_hosts);

                  /*
                   * !!! Rollback of masterlist modification
                   */
                  lDechainElem(master_hgroup_list, hgroup);
                  if (org_hgroup != nullptr) {
                     lAppendElem(master_hgroup_list, org_hgroup);
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
         if (ret && is_referenced_by_cqueue) {
            ret &= host_list_add_missing_href(packet, task, master_ehost_list, answer_list, add_hosts, monitor);
         }

         lFreeList(&add_hosts);
         lFreeList(&rem_hosts);
         lFreeList(&occupant_groups);
      }
   }

   if (ret) {
      DRETURN(0);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

int
hgroup_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *this_elem, lList **answer_list, char *remote_user, char *remote_host) {
   int ret = true;
   lList *master_hgroup_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_HGROUP);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);
   /*
    * Check all incoming parameter
    */
   if (this_elem != nullptr && remote_user != nullptr && remote_host != nullptr) {
      const char *name = lGetHost(this_elem, HGRP_name);

      /*
       * What is the name ob the hostgroup which should be removed?
       */
      if (name != nullptr) {
         lListElem *hgroup;

         /*
          * The reserved host groups back the admin/submit host lists and the
          * execution host list (CS-2438). Deleting one would drop every admin
          * or submit host at once, so it is refused here -- the same guard the
          * reserved usersets get in sge_del_userset(). Checked before the
          * existence lookup so the answer does not depend on whether the group
          * happens to be spooled yet.
          */
         if (hgroup_is_reserved(name)) {
            ERROR(MSG_HGRP_RESERVED_NODELETE_S, name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         /*
          * Does this hostgroup exist?
          */
         hgroup = hgroup_list_locate(master_hgroup_list, name);
         if (hgroup != nullptr) {
            lList *href_list = nullptr;
            lList *qref_list = nullptr;

            /*
             * Is it still referenced in another hostgroup or cqueue?
             */
            ret &= hgroup_find_referencees(hgroup, answer_list, master_hgroup_list,
                                           master_cqueue_list, &href_list, &qref_list);
            if (ret) {
               if (href_list != nullptr) {
                  dstring string = DSTRING_INIT;

                  href_list_append_to_dstring(href_list, &string);
                  ERROR(MSG_HGROUP_REFINHGOUP_SS, name, sge_dstring_get_string(&string));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  sge_dstring_free(&string);
                  ret = false;
               }
               if (qref_list != nullptr) {
                  dstring string = DSTRING_INIT;

                  str_list_append_to_dstring(qref_list, &string, ' ');
                  ERROR(MSG_CQUEUE_REFINHGOUP_SS, name, sge_dstring_get_string(&string));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  sge_dstring_free(&string);
                  ret = false;
               }
            }
            lFreeList(&href_list);
            lFreeList(&qref_list);

            /*
             * Try to unlink the concerned spoolfile
             */
            if (ret) {
               if (sge_event_spool(answer_list, 0, sgeE_HGROUP_DEL,
                                   0, 0, name, nullptr, nullptr, nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {
                  /*
                   * Let's remove the object => Success!
                   */

                  lRemoveElem(master_hgroup_list, &hgroup);

                  INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, remote_user, remote_host, name, "host group entry");
                  answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
               } else {
                  ERROR(MSG_CANTSPOOL_SS, "host group entry", name);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            }
         } else {
            ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, "host group", name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      } else {
         ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(HGRP_name), __func__);
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

/****** qmaster/hgroup/hgroup_refresh_caches() ********************************
*  NAME
*     hgroup_refresh_caches() -- recompute this group's cache and its referencees'
*
*  FUNCTION
*     CS-2451: the resolved host list of this group just changed, and with it
*     that of every group referencing it -- transitively, since @a may contain
*     @b which contains @c. Refresh all of them BEFORE any event goes out, so
*     no event carries a cache that disagrees with the master list.
*
*     The caller must have chained the new object into the master list already,
*     so resolving here sees the new state.
*
*  INPUTS
*     lListElem *hgroup           - the group whose membership changed
*     lList *master_hgroup_list   - the (already updated) master list
*     lList **referencees         - OUT, HR_Type names of the refreshed
*                                   referencees; the CALLER frees it, because it
*                                   still needs them to send one event each
*
*  NOTES
*     MT-NOTE: call under the write lock, like every other writer of the list
*******************************************************************************/
void
hgroup_refresh_caches(lListElem *hgroup, lList *master_hgroup_list, lList **referencees)
{
   lList *cache_answer_list = nullptr;

   DENTER(TOP_LAYER);

   hgroup_update_cache(hgroup, &cache_answer_list, master_hgroup_list);
   if (hgroup_find_all_referencees(hgroup, &cache_answer_list, master_hgroup_list, referencees)) {
      const lListElem *href;

      /* a diamond (@top -> @a, @b -> @x) lists @x once per path */
      lUniqHost(*referencees, HR_name);

      for_each_ep(href, *referencees) {
         lListElem *referencee = hgroup_list_locate(master_hgroup_list, lGetHost(href, HR_name));

         if (referencee != nullptr) {
            hgroup_update_cache(referencee, &cache_answer_list, master_hgroup_list);
         }
      }
   }
   answer_list_output(&cache_answer_list);

   DRETURN_VOID;
}

/****** qmaster/hgroup/hgroup_send_referencee_events() ************************
*  NAME
*     hgroup_send_referencee_events() -- one HGROUP_MOD per refreshed referencee
*
*  FUNCTION
*     CS-2451. One event per referencee so the mirrors pick up their refreshed
*     cache. sge_add_event(), not sge_event_spool(): HGRP_cached_hosts is not
*     spooled and nothing in the referencees' own configuration changed, so
*     rewriting their spool files would be pure I/O.
*
*     Send these AFTER the event for the group that actually changed.
*******************************************************************************/
void
hgroup_send_referencee_events(const lList *referencees, lList *master_hgroup_list, uint64_t gdi_session)
{
   const lListElem *href;

   for_each_ep(href, referencees) {
      const char *ref_name = lGetHost(href, HR_name);
      lListElem *referencee = hgroup_list_locate(master_hgroup_list, ref_name);

      if (referencee != nullptr) {
         sge_add_event(0, sgeE_HGROUP_MOD, 0, 0, ref_name, nullptr, nullptr, referencee, gdi_session);
      }
   }
}

int
hgroup_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *hgroup, lListElem *old_hgroup, gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   const char *name = lGetHost(hgroup, HGRP_name);
   lList *cqueue_list = nullptr;
   lList *master_hgroup_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_HGROUP);
   lList *referencees = nullptr;   /* HR_Type */

   DENTER(TOP_LAYER);

   hgroup_refresh_caches(hgroup, master_hgroup_list, &referencees);

   /* we will have the cqueue_list in the final event */
   lXchgList(hgroup, HGRP_cqueue_list, &cqueue_list);
   /*
    * HGRP modify or add event
    */
   sge_add_event(0, old_hgroup ? sgeE_HGROUP_MOD : sgeE_HGROUP_ADD, 0, 0, name, nullptr, nullptr, hgroup, packet->gdi_session);

   lXchgList(hgroup, HGRP_cqueue_list, &cqueue_list);

   hgroup_send_referencee_events(referencees, master_hgroup_list, packet->gdi_session);
   lFreeList(&referencees);

   /*
    * QI add or delete events. Finalize operation.
    */
   hgroup_commit(hgroup, packet->gdi_session);

   DRETURN(0);
}


int
hgroup_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **answer_list, lListElem *this_elem, gdi_object_t *object) {
   bool tmp_ret = true;
   bool dbret;
   const char *name = lGetHost(this_elem, HGRP_name);
   const lList *cqueue_list = lGetList(this_elem, HGRP_cqueue_list);
   const lListElem *cqueue = nullptr;
   dstring key_dstring = DSTRING_INIT;
   lList *spool_answer_list = nullptr;

   DENTER(TOP_LAYER);

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
      for_each_ep(cqueue, cqueue_list) {
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         const lListElem *qinstance = nullptr;
         const char *cqname = lGetString(cqueue, CQ_name);

         for_each_ep(qinstance, qinstance_list) {
            uint32_t tag = lGetUlong(qinstance, QU_tag);

            if (tag == SGE_QI_TAG_ADD || tag == SGE_QI_TAG_MOD) {
               const char *key = sge_dstring_sprintf(&key_dstring, "%s/%s", cqname, lGetHost(qinstance, QU_qhostname));
               dbret = spool_write_object(&spool_answer_list, spool_get_default_context(),
                                          qinstance, key, SGE_TYPE_QINSTANCE, true);
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
                                 this_elem, name, SGE_TYPE_HGROUP, true);
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
