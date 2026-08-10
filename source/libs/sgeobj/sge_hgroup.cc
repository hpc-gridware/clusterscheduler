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
 * @brief Host groups: named sets of hosts, written with a leading `@`
 *
 * A group may contain other groups, so resolving one is recursive. Three
 * group names are reserved and maintained by the daemons rather than by an
 * administrator - see #ADMIN_HOSTGROUP and its siblings.
 *
 * @see sge_hgroup.h
 */
/*
  This module is used for group building
 */

#include <cstdio>
#include <cstring>

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "comm/commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"

#include <cinttypes>
#include "msg_common.h"

/// Debug layer the host group traces are written to
#define HGROUP_LAYER TOP_LAYER

/**
 * @brief Refresh the resolved host list of one group
 *
 * CS-2451: resolves the nested references of @p hgroup once and stores the
 * flat result in HGRP_cached_hosts, so a membership test becomes a hash
 * lookup on HR_name instead of a walk of the tree.
 * Qmaster-side maintenance only. Readers never compute a cache; they use it
 * when HGRP_cache_version is non-zero and fall back to the walk when it is 0.
 * The version counter is a PRESENCE FLAG, not a stamp to compare against
 * anything (see HGRP.json). It only has to be non-zero and never 0, which is
 * why the counter starts at 1 and skips 0 on wrap.
 *
 * @param hgroup the group to refresh (HGRP_Type)
 * @param answer_list for returning errors
 * @param master_hgroup_list list the references are resolved against
 *
 * @return true on success; on failure the cache is left INVALID (version 0) rather than stale, so consumers fall back to the walk
 *
 * @note MT-NOTE: hgroup_update_cache() is not MT safe -- call it under the write
 *       lock, like every other writer of the HGRP master list
 */
bool hgroup_update_cache(lListElem *hgroup, lList **answer_list, const lList *master_hgroup_list) {
   DENTER(HGROUP_LAYER);

   static lUlong next_version = 1;
   bool ret = true;

   if (hgroup == nullptr || master_hgroup_list == nullptr) {
      DRETURN(false);
   }

   lList *used_hosts = nullptr;

   // resolve the whole nested tree once
   ret = hgroup_find_all_references(hgroup, answer_list, master_hgroup_list, &used_hosts, nullptr);

   if (ret) {
      lSetList(hgroup, HGRP_cached_hosts, used_hosts);   // takes ownership
      if (++next_version == 0) {
         next_version = 1;                               // 0 means "not computed"
      }
      lSetUlong(hgroup, HGRP_cache_version, next_version);
   } else {
      // Leave no half-resolved cache behind: an invalid cache must look
      // uncomputed, never plausible-but-wrong.
      lFreeList(&used_hosts);
      lSetList(hgroup, HGRP_cached_hosts, nullptr);
      lSetUlong(hgroup, HGRP_cache_version, 0);
   }

   DRETURN(ret);
}

/**
 * @brief Refresh every group's resolved host list
 *
 * Used at qmaster startup, after the host group list has been read from the
 * spool area. Every group is resolved independently, so the order in the
 * list does not matter.
 *
 * @param master_hgroup_list the list to refresh in place
 * @param answer_list for returning errors
 *
 * @return true if every group could be resolved
 *
 * @note MT-NOTE: hgroup_list_update_caches() is not MT safe
 */
bool hgroup_list_update_caches(lList *master_hgroup_list, lList **answer_list) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   lListElem *hgroup;
   for_each_rw(hgroup, master_hgroup_list) {
      ret &= hgroup_update_cache(hgroup, answer_list, master_hgroup_list);
   }
   DRETURN(ret);
}

/**
 * @brief Does this group carry a usable resolved list?
 *
 * CS-2451. True when HGRP_cache_version is non-zero, i.e. qmaster has
 * resolved this group and the result travelled with the element.
 * The version is what makes the answer possible at all: cull stores an empty
 * list as nullptr, so HGRP_cached_hosts alone cannot distinguish a group that
 * resolves to no hosts from one that was never resolved.
 *
 * @param hgroup HGRP_Type object, may be nullptr
 *
 * @return true if hgroup_cache_contains_host() may be used on this element
 *
 * @note MT-NOTE: hgroup_has_host_cache() is MT safe
 *
 *       HGRP_cached_hosts and HGRP_cache_version are a UNIT. A GDI "what" filter
 *       that selects the version without the list would present an empty cache as
 *       valid, and every membership test against that group would answer "no".
 *       Every descriptor in the tree is currently lWhat("%T(ALL)"); if a reduced
 *       one is ever introduced -- RBAC output reduction is the likely reason --
 *       it must take both fields or neither.
 */
bool hgroup_has_host_cache(const lListElem *hgroup) {
   return hgroup != nullptr && lGetUlong(hgroup, HGRP_cache_version) != 0;
}

/**
 * @brief Is host a member, according to the cache?
 *
 * CS-2451. Answers the membership question with a single hash lookup instead
 * of a walk of the nested group tree.
 * The lookup is equivalent to the walk's sge_hostcmp() comparison, not merely
 * similar: cull normalises host keys with sge_hostcpy() + sge_strtoupper()
 * (cull_multitype.cc), and sge_hostcpy() honours the same ignore_fqdn and
 * default_domain rules as sge_hostcmp() (sge_hostname.cc). HR_name carries
 * the HASH flag, so the lookup is O(1).
 *
 * @param hgroup HGRP_Type object with a valid cache
 * @param hostname host to look for
 *
 * @return true if the host is in the group, directly or through nesting
 *
 * @note MT-NOTE: hgroup_cache_contains_host() is MT safe
 *
 *       Only meaningful when hgroup_has_host_cache() is true; on an element without
 *       a cache it reports false, which is the WRONG answer rather than a safe one.
 *       Always guard the call.
 */
bool hgroup_cache_contains_host(const lListElem *hgroup, const char *hostname) {
   if (hgroup == nullptr || hostname == nullptr) {
      return false;
   }
   return lGetElemHost(lGetList(hgroup, HGRP_cached_hosts), HR_name, hostname) != nullptr;
}

/**
 * @brief Is the host a member, directly or through nesting?
 *
 * CS-2438. The two functions above composed the way consumers actually want
 * them: use the cache when the element carries one, resolve the nested tree
 * when it does not.
 * This exists so that the guard is written once. hgroup_cache_contains_host()
 * on an element without a cache answers "no", which is the wrong answer
 * rather than a safe one -- and its callers are the GDI permission path
 * (host_is_admin_host()) and the delete-semantics check in the qmaster, where
 * a wrong "no" denies a request or reports a member as absent.
 *
 * @param hgroup HGRP_Type object, may be nullptr
 * @param hostname host to look for
 * @param master_hgroup_list list the nested references resolve against
 *
 * @return true if the host is in the group, directly or transitively
 *
 * @note MT-NOTE: hgroup_contains_host() is MT safe
 *
 *       The fallback allocates and walks; it is not the hot path and must not
 *       become one. If a caller finds itself here on every request, the cache is
 *       not being maintained -- fix that rather than optimising this.
 */
bool hgroup_contains_host(const lListElem *hgroup, const char *hostname, const lList *master_hgroup_list) {
   DENTER(HGROUP_LAYER);

   if (hgroup == nullptr || hostname == nullptr) {
      DRETURN(false);
   }

   if (hgroup_has_host_cache(hgroup)) {
      DRETURN(hgroup_cache_contains_host(hgroup, hostname));
   }

   lList *answer_list = nullptr;
   lList *used_hosts = nullptr;
   bool ret = false;

   if (hgroup_find_all_references(hgroup, &answer_list, master_hgroup_list, &used_hosts, nullptr)) {
      ret = lGetElemHost(used_hosts, HR_name, hostname) != nullptr;
   }
   lFreeList(&used_hosts);
   lFreeList(&answer_list);

   DRETURN(ret);
}

/**
 * @brief Is this one of the reserved host groups?
 *
 * True for `@admin_hosts`, `@submit_hosts` and `@exec_hosts` (CS-2438). These
 * back what used to be the AH_LIST/SH_LIST data models and the execution
 * host list, so they may not be deleted and carry extra rules on write.
 * Comparison is case-sensitive, matching how the reserved usersets are
 * compared in sge_userset_qmaster.cc: the names are fixed literals the
 * product creates itself, not something a user types in a locale.
 *
 * @param name host group name including the leading '@'
 *
 * @return true if reserved
 *
 * @note MT-NOTE: hgroup_is_reserved() is MT safe
 */
bool hgroup_is_reserved(const char *name) {
   return name != nullptr &&
          (strcmp(name, ADMIN_HOSTGROUP) == 0 ||
           strcmp(name, SUBMIT_HOSTGROUP) == 0 ||
           strcmp(name, EXEC_HOSTGROUP) == 0);
}

/**
 * @brief May nobody write this group?
 *
 * True only for `@exec_hosts`, which the qmaster derives from the execution
 * host list. Write access is refused for every role including manager --
 * the spec states this independently of RBAC (04_Logical_View.md, "Protected
 * Object Keys"), because a hand-edited copy would silently disagree with the
 * exec host list it is supposed to mirror.
 *
 * @param name host group name including the leading '@'
 *
 * @return true if the group is maintained by the system
 *
 * @note MT-NOTE: hgroup_is_system_maintained() is MT safe
 */
bool hgroup_is_system_maintained(const char *name) {
   return name != nullptr && strcmp(name, EXEC_HOSTGROUP) == 0;
}

/**
 * @brief Determine if the name is a valid hgroup name
 *
 * Determine if the given name is a valid hostgroup name. If not
 * add an approbiate error to the answer_list
 *
 * @param answer_list answer list where errors are stored
 * @param name name of the hostgroup
 *
 * @return result true  -  name contains a valid name for a hostgroup false - name is not a valid name for a hostrgroup
 *
 * @note MT-NOTE: check_hgroup_name() is not MT safe
 */
bool hgroup_check_name(lList **answer_list, const char *name) {
   if (!ocs::is_hgroup_name(name)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_HGRP_INVALIDHOSTGROUPNAME_S, name);
      return false;
   }
   if (verify_obj_name(
      answer_list,&name[1], MAX_VERIFY_STRING,
      "hostgroup") != STATUS_OK) {
      return false;
   }
   return true;
}

/**
 * @brief Find a group by name
 *
 * Find a 'group' in 'this_list'.
 *
 * @param this_list HGRP_Type list
 * @param group group name
 *
 * @return found element or nullptr
 */
lListElem *
hgroup_list_locate(const lList *this_list, const char *group) {
   DENTER(HGROUP_LAYER);

   lListElem *ret = nullptr;
   
   ret = lGetElemHostRW(this_list, HGRP_name, group);
   DRETURN(ret);
}


/**
 * @brief Create a new hgroup
 *
 * Create a new hostgroup.
 *
 * @param answer_list AN_Type list
 * @param name name
 * @param href_or_groupref list of hosts for this hgroup
 * @param is_name_validate if true, the hgrp name is validated. Should be done all the time, there is only one case in qconf in that the name has to be ignored.
 *
 * @return new element or nullptr
 */
lListElem *
hgroup_create(lList **answer_list, const char *name, lList *href_or_groupref, bool is_name_validate) {
   DENTER(HGROUP_LAYER);

   lListElem *ret = nullptr;  /* HGRP_Type */

   if (name != nullptr) {
      if(!is_name_validate || hgroup_check_name(answer_list, name) ) {
         ret = lCreateElem(HGRP_Type);
         if (ret != nullptr) {
            lSetHost(ret, HGRP_name, name);
            lSetList(ret, HGRP_host_list, href_or_groupref);
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MEM_MEMORYALLOCFAILED_S, __func__);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         }
     }
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }
   DRETURN(ret);
}

/**
 * @brief Add a host or group reference
 *
 * Add a host or group reference.
 *
 * @param this_elem HGRP_Type elem
 * @param answer_list AN_Type list
 * @param href_or_groupref HR_Type list
 *
 * @return error state true  - Success false - Error
 */
bool hgroup_add_references(lListElem *this_elem, lList **answer_list,
                           const lList *href_or_groupref) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem != nullptr && href_or_groupref != nullptr) {
      lList *href_list = nullptr;   /* HR_Type */

      lXchgList(this_elem, HGRP_host_list, &href_list);
      for_each_ep_lv(href, href_or_groupref) {
         const char *name = lGetHost(href, HR_name);
   
         ret &= href_list_add(&href_list, answer_list, name);
         if (!ret) {
            break;
         }
      } 
      lXchgList(this_elem, HGRP_host_list, &href_list);
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Find referenced host and groups
 *
 * Find directly or indirectly referenced hgroup names.
 * 'master_list' has to be the list of all existing hgroups.
 * 'used_hosts' and 'used_groups' will contain the names of
 * hosts and groups referenced by 'this_elem'.
 *
 * @param this_elem HGRP_Type
 * @param answer_list AN_Type list
 * @param master_list HGRP_Type list
 * @param used_hosts HR_Type list
 * @param used_groups HR_Type list
 *
 * @return error state true  - Success false - Error BUGS Extremely poor performance. Try not to use this function.
 */
bool hgroup_find_all_references(const lListElem *this_elem, lList **answer_list,
                                const lList *master_list, lList **used_hosts,
                                lList **used_groups) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem != nullptr && master_list != nullptr) {
      lList *href_list = nullptr;   /* HR_Type */

      const char *name = lGetHost(this_elem, HGRP_name);
      ret &= href_list_add(&href_list, answer_list, name);

      if (ret) {
         ret &= href_list_find_all_references(href_list, answer_list, master_list, used_hosts, used_groups);
      }
      lFreeList(&href_list);
   }
   DRETURN(ret);
}

/**
 * @brief Find directly referenced hosts and groups
 *
 * Find all hgroups which are directly referenced by 'this_elem'
 * 'master_list' has to be the list of all existing hgroups.
 * 'used_hosts' and 'used_groups' will contain the names of
 * hosts and groups after a call to this function.
 *
 * @param this_elem HGRP_Type
 * @param answer_list AN_Type
 * @param master_list HGRP_Type
 * @param used_hosts HR_Type
 * @param used_groups HR_Type
 *
 * @return Error state true  - Success false - Error
 */
bool hgroup_find_references(const lListElem *this_elem, lList **answer_list,
                            const lList *master_list, lList **used_hosts,
                            lList **used_groups) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem != nullptr && master_list != nullptr) {
      const char *name = lGetHost(this_elem, HGRP_name);
      lList *href_list = nullptr;   /* HR_Type */

      ret &= href_list_add(&href_list, answer_list, name);

      if (ret) {
         ret &= href_list_find_references(href_list, answer_list, master_list, 
                                          used_hosts, used_groups);
      }
      lFreeList(&href_list);
   }
   DRETURN(ret);
}

/**
 * @brief Find groups refering to this group
 *
 * Find all hostgroups from 'master_list' which reference the
 * hostgroup 'this_elem'. The name of these hostgroups will be
 * returned in the hreference list 'occupants_groups'.
 * 'answer_list' will contain error messages if the function is
 * not successful
 *
 * @param this_elem HGRP_Type element
 * @param answer_list AN_Type list
 * @param master_list list of all existing HGRP_Type elements
 * @param occupants_groups HR_Type list
 *
 * @return exit state true  - Success false - Error
 */
bool hgroup_find_all_referencees(const lListElem *this_elem,
                                 lList **answer_list, const lList *master_list,
                                 lList **occupants_groups) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem != nullptr && occupants_groups != nullptr) {
      lList *href_list = nullptr;
      const char *name;

      name = lGetHost(this_elem, HGRP_name);
      ret &= href_list_add(&href_list, answer_list, name);

      if (ret) {
         ret &= href_list_find_all_referencees(href_list, answer_list,
                                             master_list, occupants_groups);
      }
      lFreeList(&href_list);
   }
   DRETURN(ret);
}

/**
 * @brief Find groups refering to this group
 *
 * Find all hostgroups from 'master_list' which reference the
 * hostgroup 'this_elem'. The name of these hostgroups will be
 * returned in the hreference list 'occupants_groups'.
 * 'answer_list' will contain error messages if the function is
 * not successful
 *
 * @param this_elem HGRP_Type
 * @param answer_list AN_Type
 * @param master_hgroup_list HGRP_Type master list
 * @param master_cqueue_list CQ_Type
 * @param occupants_groups HR_Type
 * @param occupants_queues ST_Type
 *
 * @return Error state true  - Success false - Error
 */
bool hgroup_find_referencees(const lListElem *this_elem,
                             lList **answer_list,
                             const lList *master_hgroup_list,
                             const lList *master_cqueue_list,
                             lList **occupants_groups,
                             lList **occupants_queues) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem != nullptr) {
      if (occupants_groups != nullptr) {
         const char *name = lGetHost(this_elem, HGRP_name);
         lList *href_list = nullptr;

         ret &= href_list_add(&href_list, answer_list, name);
         if (ret) {
            ret &= href_list_find_referencees(href_list, answer_list,
                                              master_hgroup_list, 
                                              occupants_groups);
         }
         lFreeList(&href_list);
      }
      if (ret && occupants_queues != nullptr) {
         ret &= cqueue_list_find_hgroup_references(master_cqueue_list, 
                                                   answer_list,
                                                   this_elem, 
                                                   occupants_queues);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Do hostgroups really exist
 *
 * This functions returns true if all hostgroups given by the parameter
 * "href_list" exist in "this_list". If one or more objects are missing
 * a corresponding error message will be returned via "answer_list".
 *
 * @param this_list HGRP_Type
 * @param answer_list AN_Type
 * @param href_list HR_Type
 *
 * @return true or false
 */
bool hgroup_list_exists(const lList *this_list, lList **answer_list,
                        const lList *href_list) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (href_list != nullptr && this_list != nullptr) {
      for_each_ep_lv(href, href_list) {
         const char *name = lGetHost(href, HR_name);

         if (ocs::is_hgroup_name(name)) {
            lListElem *hgroup = hgroup_list_locate(this_list, name);
         
            if (hgroup == nullptr) {
               ret = false;
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_DOESNOTEXIST_SS, "host group", name);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               break;      
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Finds hostnames
 *
 * Selects all hostgroups of "this_list" which match the pattern
 * "hgroup_pattern". All hostnames which are directly or indirectly
 * referenced will be added to "used_hosts"
 *
 * @param this_list HGRP_Type
 * @param answer_list AN_Type
 * @param hgroup_pattern fnmatch pattern
 * @param used_hosts HR_Type
 *
 * @return error state true  - Success false - Error
 */
bool hgroup_list_find_matching_and_resolve(const lList *this_list,
                                           lList **answer_list,
                                           const char *hgroup_pattern,
                                           lList **used_hosts) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_list != nullptr && hgroup_pattern != nullptr) {
      const bool hgroup_pattern_is_expression = ocs::is_expression(hgroup_pattern);

      for_each_ep_lv(hgroup, this_list) {
         const char *hgroup_name = lGetHost(hgroup, HGRP_name);
         
         /* use hostgroup expression */
         if (!sge_eval_expression(ocs::CEntry::Type::HOST,hgroup_pattern, hgroup_name, nullptr, true, hgroup_pattern_is_expression)) {
            lList *tmp_used_hosts = nullptr;
            ret = hgroup_find_all_references(hgroup, nullptr, this_list,
                                             &tmp_used_hosts, nullptr);
            for_each_ep_lv(tmp_href, tmp_used_hosts) {
               if (used_hosts != nullptr) {
                  const char *hostname = lGetHost(tmp_href, HR_name);

                  lAddElemHost(used_hosts, HR_name, hostname, HR_Type);
               }
            }
            lFreeList(&tmp_used_hosts);
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find hgroups which match pattern
 *
 * Selects all hostgroups of "this_list" which match the pattern
 * "hgroup_pattern". All matching hostgroup names will be added to
 * "href_list"
 *
 * @param this_list HGRP_Type list
 * @param answer_list AN_Type list
 * @param hgroup_pattern hostgroup pattern
 * @param[out] href_list HR_Type list receiving the matching names
 *
 * @return error state true  - success false - error
 */
bool hgroup_list_find_matching(const lList *this_list, lList **answer_list,
                               const char *hgroup_pattern, lList **href_list) {
   DENTER(HGROUP_LAYER);
   bool ret = true;

   if (this_list != nullptr && hgroup_pattern != nullptr) {
      const bool hgroup_pattern_is_expression = ocs::is_expression(hgroup_pattern);

      for_each_ep_lv(hgroup, this_list) {
         const char *hgroup_name = lGetHost(hgroup, HGRP_name);

   /* use hostgroup expression */
         if (!sge_eval_expression(ocs::CEntry::Type::HOST, hgroup_pattern, hgroup_name, nullptr, true, hgroup_pattern_is_expression)) {
            if (href_list != nullptr) {
               lAddElemHost(href_list, HR_name, hgroup_name, HR_Type);
            }
         }
      }
   }
   DRETURN(ret);
}
