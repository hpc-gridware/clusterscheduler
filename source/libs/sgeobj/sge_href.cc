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
 * @brief Host reference lists: names that are either a host or a host group
 *
 * Many configuration objects accept both. A host reference (`HR_Type`) stores
 * the name without deciding which it is; resolving a list splits it into the
 * hosts it names directly and the hosts reached through its groups.
 */


#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "comm/commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/ocs_Matcher.h"
#include "sgeobj/msg_sgeobjlib.h"

#include <cinttypes>
#include "msg_common.h"

/// Debug layer the host reference traces are written to
#define HOSTREF_LAYER BASIS_LAYER

/**
 * @brief Add host or hostgroup reference
 *
 * Add a host or hostgroup given by 'host_or_group' into the list
 * 'this_list'. If the function is successful then the function
 * returns 'true' otherwise it will add an entry into 'answer_list'
 * and return with 'false'. If 'this_list' does not exist than it
 * will be created.
 *
 * @param this_list HR_Type list
 * @param answer_list AN_Type list
 * @param host_or_group host or group name
 *
 * @return error state true - Success false - Error
 */
bool href_list_add(lList **this_list, lList **answer_list, const char *host_or_group) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && host_or_group != nullptr) {
      if (!href_list_has_member(*this_list, host_or_group)) {
         lListElem *h_or_g;   /* HR_Type */

         h_or_g = lAddElemHost(this_list, HR_name, host_or_group, HR_Type);
         if (h_or_g == nullptr) {
            answer_list_add(answer_list, MSG_GDI_OUTOFMEMORY,
                            STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Append one host reference list to another, without creating a duplicate
 *
 * The key of these lists is unique and hashed, so two entries under one key are
 * a state the list is not supposed to reach - and one it cannot be brought out
 * of, because removing either of them drops the key from the hash and leaves the
 * survivor unfindable. Adding entry by entry never creates it.
 *
 * @param this_list the list to append to; nullptr is ignored
 * @param answer_list AN_Type list
 * @param other the entries to append; the caller frees it
 *
 * @return error state true - Success false - Error
 *
 * @see #href_list_add
 */
static bool
href_list_merge(lList **this_list, lList **answer_list, const lList *other) {
   bool ret = true;

   if (this_list == nullptr || other == nullptr) {
      return ret;
   }
   for_each_ep_lv(href, other) {
      ret = href_list_add(this_list, answer_list, lGetHost(href, HR_name));
      if (!ret) {
         break;
      }
   }
   return ret;
}

/**
 * @brief Is reference already in list
 *
 * Is the given host or hostgroup ('host_or_group') already
 * contained in the reference list?
 *
 * @param this_list HR_Type list
 * @param host_or_group hostname or hgroup
 *
 * @return error state true  - Success false - Error
 */
bool href_list_has_member(const lList *this_list, const char *host_or_group) {
   DENTER(HOSTREF_LAYER);

   bool ret = false;

   if (this_list != nullptr && host_or_group != nullptr) {
      if (href_list_locate(this_list, host_or_group) != nullptr) {
         ret = true;
      }
   } else {
      /*
       * If one of the argumets was not given by the callee we may be sure
       * that 'host_or_group' is not member of 'this_list'!
       */
      ;
   }
   DRETURN(ret);
}

/**
 * @brief Finds additional entries in list
 *
 * This function will find differences between two hostref lists
 * given by 'this_list' and 'list'. Hosts and hostgroups which are
 * only in 'this_list' can be found in 'add_hosts' and 'add_groups'.
 * References which are contained in both lists can be found in
 * 'equity_hosts' and 'equity_groups' after a call to this function.
 *
 * If the calling function is not interested in one ore more of the
 * result lists than nullptr should be used as parameter. The calling
 * function is responsible to free all result lists.
 *
 * If the callee is also interested in the references which are
 * only part of 'list' than this function can not be used.
 * href_list_find_diff() should be used in this case.
 *
 * @param this_list HR_Type list to comapre
 * @param answer_list AN_Type list
 * @param list 2nd HR_Type list to be compared
 * @param add_hosts HR_Type list
 * @param add_groups HR_Type list
 * @param equity_hosts HR_Type list
 * @param equity_groups HR_Type list
 *
 * @return error state true  - Success false - Error
 *
 * @see #href_list_find_diff
 */
bool href_list_compare(const lList *this_list, lList **answer_list,
                       const lList *list, lList **add_hosts,
                       lList **add_groups, lList **equity_hosts,
                       lList **equity_groups) {
   DENTER(HOSTREF_LAYER);
   bool ret = true;

   for_each_rw_lv(this_elem, this_list) {
      const char *host_or_group = lGetHost(this_elem, HR_name);

      // Three classes, not two. A matcher belongs in neither output: the host
      // side of this difference is what missing execution host objects are
      // created from, so a matcher landing there would bring an execution host
      // named after it into being - and through the reserved execution host
      // group that host would become part of the very candidate set matchers
      // are resolved against.
      const ocs::MemberClass member_class = ocs::classify_member(host_or_group);

      if (member_class == ocs::MemberClass::MATCHER) {
         continue;
      }

      const bool is_group = (member_class == ocs::MemberClass::GROUP_REFERENCE);

      if (!href_list_has_member(list, host_or_group)) {
         if (is_group) {
            if (add_groups != nullptr) {
               ret = href_list_add(add_groups, answer_list, host_or_group);
            }
         } else if (add_hosts != nullptr) {
            ret = href_list_add(add_hosts, answer_list, host_or_group);
         }
      } else {
         if (is_group) {
            if (equity_groups != nullptr) {
               ret = href_list_add(equity_groups, answer_list, host_or_group);
            }
         } else if (equity_hosts != nullptr) {
            ret = href_list_add(equity_hosts, answer_list, host_or_group);
         }
      }
      if (!ret) {
         break;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Difference between two lists
 *
 * Will identify differences between 'this_list' and 'list'.
 * hosts which are only in 'this_list' will be copied into 'add_hosts'
 * hosts which are only in 'list' will be copied into 'rem_hosts'
 * groups which are only in 'this_list' will be copied to 'add_groups'
 * groups which are only in 'this' will be copied to 'rem_groups'
 *
 * The calling context is responsible to free all result lists.
 * If the callee is not interested in one or more of the resultlist
 * than nullptr should be used as parameter for this function.
 *
 * @param this_list HR_Type list to be compared
 * @param answer_list AN_Type list
 * @param list 2nd HR_Type list to be compared
 * @param add_hosts HR_Type list
 * @param rem_hosts HR_Type list
 * @param add_groups HR_Type list
 * @param rem_groups HR_Type list
 *
 * @return error state true  - Success false - Error
 *
 * @see #href_list_find_diff
 */
bool href_list_find_diff(const lList *this_list, lList **answer_list,
                         const lList *list, lList **add_hosts,
                         lList **rem_hosts, lList **add_groups, lList **rem_groups) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   ret &= href_list_compare(this_list, answer_list, list,
                            add_hosts, add_groups, nullptr, nullptr);
   ret &= href_list_compare(list, answer_list, this_list,
                               rem_hosts, rem_groups, nullptr, nullptr);
   DRETURN(ret);
}

/**
 * @brief Resolves groups and creates diff
 *
 * Resolves host names of all groups contained in "add_groups"
 * and "rem_groups". Hostnames not part of both resulting hostgroup
 * sets will be stored in "add_hosts" and "rem_hosts".
 *
 * @param answer_list AN_Type list
 * @param add_groups HR_Type list1 (hgroups)
 * @param rem_groups HR_Type list2 (hgroups)
 * @param master_list HGRP_Type list of all hgroups
 * @param add_hosts resolved "add_groups" hosts not part of "rem_groups"
 * @param rem_hosts resolved "rem_groups" hosts not part of "add_hosts"
 *
 * @return error state true  - Success false - Error
 */
bool href_list_find_effective_diff(lList **answer_list, const lList *add_groups,
                                   const lList *rem_groups, const lList *master_list,
                                   lList **add_hosts, lList **rem_hosts) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (ret && add_groups != nullptr) {
      ret &= href_list_find_all_references(add_groups, answer_list,
                                           master_list, add_hosts, nullptr);
   }
   if (ret && rem_groups != nullptr) {
      ret &= href_list_find_all_references(rem_groups, answer_list,
                                           master_list, rem_hosts, nullptr);
   }
   if (ret && add_hosts != nullptr && *add_hosts != nullptr &&
       rem_hosts != nullptr && *rem_hosts != nullptr) {
      lList *tmp_rem_hosts = nullptr;
      lList *tmp_add_hosts = nullptr;

      ret &= href_list_find_diff(*add_hosts, answer_list,
                                 *rem_hosts, &tmp_add_hosts,
                                 &tmp_rem_hosts, nullptr, nullptr);
      if (ret) {
         lFreeList(add_hosts);
         lFreeList(rem_hosts);
         *add_hosts = tmp_add_hosts;
         *rem_hosts = tmp_rem_hosts;
         tmp_add_hosts = nullptr;
         tmp_rem_hosts = nullptr;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find an entry in the reference list
 *
 * Find an entry in the reference list.
 *
 * @param this_list HR_Type
 * @param name host or groupname
 *
 * @return Pointer to host or hostgroup element or nullptr
 */
lListElem *
href_list_locate(const lList *this_list, const char *name) {
   DENTER(HOSTREF_LAYER);

   lListElem *ret = nullptr;  /* HR_Type */

   if (this_list != nullptr && name != nullptr) {
      ret = lGetElemHostRW(this_list, HR_name, name);
   }
   DRETURN(ret);
}

/**
 * @brief Find referenced hosts and groups
 *
 * Finds hosts and hostgroups, which are directy referenced
 * in the hostgroups mentioned in 'this_list'. 'master_list' is
 * the list of all existing hostgroups. Directly referenced hosts
 * and hostgroups will be added to 'used_hosts' and 'used_groups'.
 * In case of any errors 'answer_list' will be filled.
 *
 * @param this_list HR_Type
 * @param answer_list AN_Type
 * @param master_list HGRP_Type
 * @param used_hosts HR_Type
 * @param used_groups HR_Type
 *
 * @return error state true  - Success false - Error
 */
bool href_list_find_references(const lList *this_list, lList **answer_list,
                               const lList *master_list, lList **used_hosts,
                               lList **used_groups, lList **used_matchers,
                               bool expand_matchers) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && master_list != nullptr) {
      /*
       * CS-2680, N-K-1. A matcher describes a set of hosts, and the only way to
       * turn that into a set is to filter one that is configured -- no operation
       * anywhere returns the names matching a pattern. That set is the execution
       * host list, which the reserved group mirrors, so it is already in the list
       * this function was handed. No group means no candidates, and matchers then
       * capture nothing; a matcher inside that group is refused when written
       * (N-B-2), so expanding it can never lead back here.
       */
      const lListElem *exec_hgroup = hgroup_list_locate(master_list, EXEC_HOSTGROUP);
      const lList *candidates = (exec_hgroup != nullptr) ? lGetList(exec_hgroup, HGRP_host_list) : nullptr;

      /*
       * Handle each reference which was given by the calling context
       */
      for_each_ep_lv(href, this_list) {
         const char *name = lGetHost(href, HR_name);
         lListElem *hgroup = nullptr;  /* HGRP_name */

         /*
          * Try to locate the concerned hgroup object, record the matcher, or
          * add the host. Three classes, not two -- a matcher is neither a host
          * nor a group, and reaching either output as a name is the fault N-W-5
          * is directed against.
          */
         switch (ocs::classify_member(name)) {
            case ocs::MemberClass::GROUP_REFERENCE:
               hgroup = hgroup_list_locate(master_list, name);
               break;
            case ocs::MemberClass::MATCHER:
               if (used_matchers != nullptr) {
                  href_list_add(used_matchers, answer_list, name);
               }
               break;
            case ocs::MemberClass::LITERAL_HOST:
               if (used_hosts != nullptr) {
                  href_list_add(used_hosts, answer_list, name);
               }
               break;
         }

         if (hgroup != nullptr) {
            const lList *href_list2 = lGetList(hgroup, HGRP_host_list);

            /*
             * Add each element contained in the sublist of the hostgroup
             * we found previously to one of the result lists.
             */
            for_each_ep_lv(href2, href_list2) {
               const char *name2 = lGetHost(href2, HR_name);

               switch (ocs::classify_member(name2)) {
                  case ocs::MemberClass::GROUP_REFERENCE:
                     if (used_groups != nullptr) {
                        href_list_add(used_groups, answer_list, name2);
                     }
                     break;
                  case ocs::MemberClass::MATCHER:
                     if (used_matchers != nullptr) {
                        href_list_add(used_matchers, answer_list, name2);
                     }
                     break;
                  case ocs::MemberClass::LITERAL_HOST:
                     if (used_hosts != nullptr) {
                        href_list_add(used_hosts, answer_list, name2);
                     }
                     break;
               }
            }

            /* N-W-3: what the matchers of that group capture is part of its hosts */
            if (used_hosts != nullptr && expand_matchers) {
               ret &= ocs::Matcher::expand(href_list2, candidates, used_hosts, answer_list);
            }
         }
      }

      /* ... and likewise for the matchers of the list handed in */
      if (used_hosts != nullptr && expand_matchers) {
         ret &= ocs::Matcher::expand(this_list, candidates, used_hosts, answer_list);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find referenced hosts and hgroups
 *
 * Finds all hosts and hostgroups which are directly and indirectly
 * referenced by the hostgroups mentioned in 'this_list'.
 * 'master_list' is the list of all existing hostgroups. Referenced
 * hosts and hostgroups will be added to 'used_hosts' and 'used_groups'.
 * In case of any errors 'answer_list' will be filled.
 *
 * @param this_list RN_Type
 * @param answer_list AN_Type
 * @param master_list HGRP_Type
 * @param used_hosts RN_Type
 * @param used_groups RN_Type
 *
 * @return error state true  - Success false - Error
 *
 * @note This function walks the host group tree. It deliberately does NOT consult
 *       the resolved-host cache (HGRP_cached_hosts / HGRP_cache_version, CS-2451),
 *       even though every group carries one.
 *
 *       If that is ever changed, the two calls in hgroup_mod()'s swap block
 *       (sge_hgroup_qmaster.cc, around the lDechainElem()/lAppendElem() pair) MUST
 *       keep walking the tree. Inside that window the temporarily inserted element
 *       has no valid cache while the referencing groups still carry the old state,
 *       so a cached answer would return the PRE-modification host set as the
 *       "after" set. The cluster queue would then compute empty add/remove deltas
 *       and silently keep the wrong queue instances -- a data error, not a crash,
 *       and one no existing check would notice.
 *
 *       Add a "bool use_cache = true" parameter and pass false at those two call
 *       sites, in the same commit that starts using the cache here. Do NOT instead
 *       invalidate the caches at the start of hgroup_mod(): on rollback they would
 *       stay invalid with nothing left to rebuild them.
 *
 *       Measured 2026-08-07 (sperf_hgroup_resolution, medians above the no-RQS
 *       floor): the cache already collapsed the membership test in the hot path
 *       (qref_hgroup_rejected()), leaving 0.05-0.35 s that this function could
 *       still address, while the largest remaining item -- pattern scope at 0.54 s
 *       -- is out of its reach, because the reference side must keep matching group
 *       names as expressions (CS-2450). That is why this was left as a tree walk.
 */
bool href_list_find_all_references(const lList *this_list, lList **answer_list,
                                   const lList *master_list, lList **used_hosts,
                                   lList **used_groups, lList **used_matchers,
                                   bool expand_matchers) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && master_list != nullptr) {
      lList *tmp_used_groups = nullptr;
      bool free_tmp_list = false;

      if (used_groups == nullptr) {
         used_groups = &tmp_used_groups;
         free_tmp_list = true;
      }

      /*
       * Find all direct referenced hgroups and hosts
       */
      ret &= href_list_find_references(this_list, answer_list, master_list,
                                       used_hosts, used_groups, used_matchers, expand_matchers);

      /* 
       * If there are subgroups then try to find their direct referenced
       * groups and hosts, subgroups ...
       *
       * Recursive!
       */
      if (ret && used_groups != nullptr && *used_groups != nullptr) {
         lList *used_sub_groups = nullptr;
         lList *used_sub_hosts = nullptr;
         lList *used_sub_matchers = nullptr;

         ret &= href_list_find_all_references(*used_groups, answer_list,
                                              master_list, &used_sub_hosts,
                                              &used_sub_groups,
                                              used_matchers != nullptr ? &used_sub_matchers : nullptr,
                                              expand_matchers);
         if (ret) {
            /*
             * CS-2680. Merged entry by entry rather than by concatenating the
             * lists, and that is not a matter of taste.
             *
             * A name reachable by two paths would otherwise appear once per
             * path. That took a diamond while every path was a chain of literal
             * names and is the ordinary case once matchers are involved, since a
             * group and one it references may carry overlapping patterns. And
             * the key of these lists is unique and hashed, so two entries under
             * one key are a state the list is not supposed to reach: removing
             * one of them afterwards drops the key from the hash and leaves the
             * survivor unfindable -- which on this list means the membership test
             * missing a host that is plainly in it.
             *
             * href_list_add() asks before it adds, so the state never arises.
             * The cost is a hash lookup per entry instead of a splice, on lists
             * whose length is the membership of a host group.
             */
            ret &= href_list_merge(used_hosts, answer_list, used_sub_hosts);
            ret &= href_list_merge(used_groups, answer_list, used_sub_groups);
            /* N-W-4: the matchers encountered come back beside the hosts and the
             * groups, because an interface cannot show what it is not told */
            ret &= href_list_merge(used_matchers, answer_list, used_sub_matchers);
         }
         lFreeList(&used_sub_hosts);
         lFreeList(&used_sub_groups);
         lFreeList(&used_sub_matchers);
      }

      if (free_tmp_list) {
         lFreeList(&tmp_used_groups);
      }
   } 
   DRETURN(ret);
}

/**
 * @brief Find occupying hosts and hgroups
 *
 * Finds hostgroup references which directly occupy at least one of the
 * hostgroups mentioned in 'this_list'. 'master_list' is the list of
 * all existing hostgroups. Directly occupying hostgroups will be
 * added to 'occupant_groups'. In case of any errors 'answer_list'
 * will be filled.
 *
 * @param this_list HR_Type
 * @param answer_list AN_Type
 * @param master_list HGRP_Type
 * @param occupant_groups HR_Type
 *
 * @return error state true  - Success false - Error
 */
bool href_list_find_referencees(const lList *this_list, lList **answer_list,
                                const lList *master_list, lList **occupant_groups) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && occupant_groups != nullptr) {
      const lListElem *href;  /* HR_Type */

      for_each_ep(href, this_list) {
         const char *name = lGetHost(href, HR_name);

         if (ocs::is_hgroup_name(name)) {
            const lListElem *hgroup;   /* HGRP_Type */

            for_each_ep(hgroup, master_list) {
               const lList *href_list = lGetList(hgroup, HGRP_host_list);
               lListElem *href = href_list_locate(href_list, name);

               if (href != nullptr) {
                  const char *name = lGetHost(hgroup, HGRP_name);

                  href_list_add(occupant_groups, answer_list, name);
               }
            }
         } 
      }
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Find occupying hosts and groups
 *
 * Finds recursivly all hostgroups which occupy the hostgroups mentioned in
 * 'this_list'. 'master_list' is the list of all
 * existing hostgroups. Occupying hostgroups will be
 * added to 'occupant_groups'. In case of any errors 'answer_list'
 * will be filled.
 *
 * @param this_list RH_Type
 * @param answer_list AN_Type
 * @param master_list HGRP_Type
 * @param occupant_groups RH_Type
 *
 * @return error state true  - Success false - Error
 */
bool href_list_find_all_referencees(const lList *this_list, lList **answer_list,
                                    const lList *master_list,
                                    lList **occupant_groups) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && occupant_groups != nullptr) {

      /*
       * Find parents of all given hgroups
       */
      ret &= href_list_find_referencees(this_list, answer_list,
                                        master_list, occupant_groups);

      if (*occupant_groups != nullptr && ret) {
         lList *occupant_sub_groups = nullptr;  /* HR_Type */

         /*
          * Find grandparents, ...
          *
          * Recursive!
          */
         ret &= href_list_find_all_referencees(*occupant_groups, answer_list,
                                               master_list, 
                                               &occupant_sub_groups);

         if (occupant_sub_groups != nullptr && ret) {
            lAddList(*occupant_groups, &occupant_sub_groups);
            occupant_sub_groups = nullptr;
         } 
      }
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Bring every member of a list into the form it is stored in
 *
 * The three member classes are treated differently, and the classification is a
 * prefix comparison (see ocs::classify_member()):
 *
 *   - a **group reference** is stored as written;
 *   - a **literal host name** is resolved and replaced by the resolver's
 *     canonical name. Depending on `ignore_errors` a name that does not resolve
 *     either fails the call or is left alone;
 *   - a **matcher** is never resolved - it describes a set of hosts rather than
 *     naming one, so there is nothing to look up. It goes through
 *     ocs::Matcher::prepare() instead, which normalises its payload by the
 *     domain rules in force, validates the result, and reports what it did.
 *
 * `ignore_errors` covers **resolution** only. A malformed matcher is a
 * validation error and always fails the call: the tolerant caller is tolerant
 * so that a host which is temporarily not resolvable does not kill an unrelated
 * change, not so that an unusable member gets stored.
 *
 * `introducing` says whether these members are going into a member list or
 * coming out of one. It reaches ocs::Matcher::prepare(), which normalises either
 * way - so that the written and the stored form of a matcher hit the same entry
 * - but judges only what is going in. There is no default: every caller knows
 * which it is doing, and getting it wrong is not visible in the output.
 *
 * @param this_list HR_Type list
 * @param answer_list AN_Type list
 * @param ignore_errors ignore if a host is not resolvable
 * @param introducing true when the members are being put into a list, false when
 *        they are being taken out
 * @param hgroup the host group the list belongs to, for the messages about a
 *        matcher; may be nullptr where the caller has none
 *
 * @return error state true  - Success false - Error
 */
bool href_list_resolve_hostnames(lList *this_list, lList **answer_list,
                                 bool ignore_errors, bool introducing,
                                 const lListElem *hgroup) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr) {
      for_each_rw_lv (href, this_list) {
         const char *name = lGetHost(href, HR_name);

         switch (ocs::classify_member(name)) {
            case ocs::MemberClass::GROUP_REFERENCE:
               break;

            case ocs::MemberClass::MATCHER: {
               dstring prepared = DSTRING_INIT;

               if (ocs::Matcher::prepare(name, hgroup, introducing, &prepared, answer_list)) {
                  const char *stored = sge_dstring_get_string(&prepared);

                  if (stored != nullptr) {
                     lSetHost(href, HR_name, stored);
                  }
               } else {
                  ret = false;
               }
               sge_dstring_free(&prepared);
               break;
            }

            case ocs::MemberClass::LITERAL_HOST: {
               char resolved_name[CL_MAXHOSTNAMELEN+1];
               int back = getuniquehostname(name, resolved_name, 0);

               if (back == CL_RETVAL_OK) {
                  lSetHost(href, HR_name, resolved_name);
               } else {
                  if (!ignore_errors) {
                     INFO(MSG_HGRP_UNKNOWNHOST, name);
                     answer_list_add(answer_list, SGE_EVENT,
                                     STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     ret = false;
                  }
               }
               break;
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Print href-list to dstring
 *
 * Print href-list to dstring
 *
 * @param this_list HR_Type
 * @param string dynamic string
 *
 * @return Error state true  - Success false - Error
 */
bool href_list_append_to_dstring(const lList *this_list, dstring *string) {
   DENTER(HOSTREF_LAYER);

   const char *const delim = " ";
   bool ret = true;

   if (this_list != nullptr && string != nullptr) {
      const lListElem *href;  /* HR_Type */
      bool is_first = true;

      for_each_ep(href, this_list) {
         const char *name = lGetHost(href, HR_name);

         if (!is_first) {
            sge_dstring_append(string, delim);
         }
         sge_dstring_append(string, name);
         is_first = false; 
      }
   } else {
      ret = false;
   } 
   DRETURN(ret);
}

/**
 * @brief Removes entries from list
 *
 * Removes all entries contained in "list" will be removed from
 * "this_list" if they exist.
 *
 * @param this_list HR_Type list
 * @param answer_list AN_Type list
 * @param list HR_Type list
 *
 * @return Error state true  - Success false - Error
 */
bool href_list_remove_existing(lList **this_list, lList **answer_list,
                               lList *list) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && *this_list != nullptr && list != nullptr) {
      const lListElem * href = nullptr;

      for_each_ep(href, list) {
         const char *hostname = lGetHost(href, HR_name);
         lListElem *existing_href = lGetElemHostRW(*this_list, HR_name, hostname);

         if (existing_href != nullptr) {
            lRemoveElem(*this_list, &existing_href);
         }
      }
      if (lGetNumberOfElem(*this_list) == 0) {
         lFreeList(this_list);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Prints HR_Type list into TOP_LAYER
 *
 * Prints prefix and HR_Type "this_list" into TOP_LAYER of debug
 * output if this_list exists.
 *
 * @param this_list HR_Type list
 * @param prefix prefix string
 */
void href_list_debug_print(const lList *this_list, const char *prefix) {
   DENTER(TOP_LAYER);

   const lListElem *href = nullptr;
   dstring message = DSTRING_INIT;
   bool is_first_hostname = true;
   for_each_ep(href, this_list) {
      const char *hostname = lGetHost(href, HR_name);

      if (is_first_hostname) {
         sge_dstring_copy_string(&message, prefix);
         is_first_hostname = false;
      } else {
         sge_dstring_append(&message, ", ");
      }
      sge_dstring_append(&message, hostname);
   }
   if (!is_first_hostname) {
      sge_dstring_append(&message, "\n");
      DPRINTF(sge_dstring_get_string(&message));
   }
   sge_dstring_free(&message);
   DRETURN_VOID;
}

/**
 * @brief Remove duplicates
 *
 * Remove duplicates from "this_list"
 *
 * @param this_list HR_Type list
 * @param answer_list AN_Type list
 */
void href_list_make_uniq(lList *this_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   lListElem *elem = nullptr;
   lListElem *next_elem = nullptr;

   next_elem = lFirstRW(this_list);
   while((elem = next_elem) != nullptr) {
      lListElem *elem2 = nullptr;
      const void *iterator = nullptr;

      next_elem = lNextRW(elem);
      elem2 = lGetElemHostFirstRW(this_list, HR_name, lGetHost(elem, HR_name), &iterator);
      if (elem2 != nullptr && elem != elem2) {
         // The collapse is carried out and reported. Two entries denoting the
         // same set have no place in a security relevant object, but dropping
         // one silently is exactly the behaviour that object cannot afford.
         answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                 MSG_HGRP_DUPLICATE_COLLAPSED_S, lGetHost(elem, HR_name));
         lRemoveElem(this_list, &elem);
      }
   }
   DRETURN_VOID;
}
