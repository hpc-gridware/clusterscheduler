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
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "comm/commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
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

      if (!href_list_has_member(list, host_or_group)) {
         if (ocs::is_hgroup_name(host_or_group)) {
            if (add_groups != nullptr) {
               ret = href_list_add(add_groups, answer_list, host_or_group);
            }
         } else if (add_hosts != nullptr) {
            ret = href_list_add(add_hosts, answer_list, host_or_group);
         }
      } else {
         if (ocs::is_hgroup_name(host_or_group)) {
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
   lListElem *ret = nullptr;  /* HR_Type */

   DENTER(HOSTREF_LAYER);
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
                               lList **used_groups) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr && master_list != nullptr) {
      /*
       * Handle each reference which was given by the calling context
       */
      for_each_ep_lv(href, this_list) {
         const char *name = lGetHost(href, HR_name);
         bool is_group = ocs::is_hgroup_name(name);
         lListElem *hgroup = nullptr;  /* HGRP_name */

         /*
          * Try to locate the concerned hgroup object
          * or add host
          */
         if (is_group) {
            hgroup = hgroup_list_locate(master_list, name);
         } else {
            if (used_hosts != nullptr) {
               href_list_add(used_hosts, answer_list, name);
            }
         }

         if (hgroup != nullptr) {
            const lList *href_list2 = lGetList(hgroup, HGRP_host_list);

            /* 
             * Add each element contained in the sublist of the hostgroup
             * we found previously to one of the result lists.
             */
            for_each_ep_lv(href2, href_list2) {
               const char *name2 = lGetHost(href2, HR_name);

               if (ocs::is_hgroup_name(name2)) {
                  if (used_groups != nullptr) {
                     href_list_add(used_groups, answer_list, name2); 
                  }
               } else {
                  if (used_hosts != nullptr) {
                     href_list_add(used_hosts, answer_list, name2); 
                  }
               }   
            }
         }
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
                                   lList **used_groups) {
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
                                       used_hosts, used_groups);

      /* 
       * If there are subgroups then try to find their direct referenced
       * groups and hosts, subgroups ...
       *
       * Recursive!
       */
      if (ret && used_groups != nullptr && *used_groups != nullptr) {
         lList *used_sub_groups = nullptr;
         lList *used_sub_hosts = nullptr;

         ret &= href_list_find_all_references(*used_groups, answer_list,
                                              master_list, &used_sub_hosts,
                                              &used_sub_groups);
         if (ret) {
            if (used_hosts != nullptr && used_sub_hosts != nullptr) {
               if (*used_hosts != nullptr) {
                  lAddList(*used_hosts, &used_sub_hosts);
               } else {
                  *used_hosts = used_sub_hosts;
                  used_sub_hosts = nullptr;
               }
            }
            if (used_groups != nullptr && used_sub_groups != nullptr) {
               if (*used_groups != nullptr) {
                  lAddList(*used_groups, &used_sub_groups);
               } else {
                  *used_groups = used_sub_groups;
                  used_sub_groups = nullptr;
               }
            }
         } 
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
 * @brief Resolve hostnames
 *
 * Resolve hostnames contained in 'this_list'. Depending on the
 * 'ignore_errors' parameter the function will either fail if a
 * host is not resolvable or this will be ignored.
 *
 * @param this_list HR_Type list
 * @param answer_list AN_Type list
 * @param ignore_errors ignore if a host is not resolvable
 *
 * @return error state true  - Success false - Error
 */
bool href_list_resolve_hostnames(lList *this_list, lList **answer_list,
                                 bool ignore_errors) {
   DENTER(HOSTREF_LAYER);

   bool ret = true;

   if (this_list != nullptr) {
      for_each_rw_lv (href, this_list) {
         const char *name = lGetHost(href, HR_name);

         if (!ocs::is_hgroup_name(name)) {
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
         lRemoveElem(this_list, &elem);
      }
   }
   DRETURN_VOID;
}
