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
 * @brief Usersets: access lists and departments
 *
 * The same object serves both purposes, told apart by `US_type`. An entry
 * names either a user or, with a leading `@`, a UNIX group.
 *
 * @see sge_userset.h
 */
#include <cstdio>
#include <fnmatch.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_common.h"

#include <sge_str.h>

const char* userset_types[] = {
   "ACL",   /* US_ACL   */
   "DEPT",  /* US_DEPT  */
   nullptr
};

/**
 * @brief Validate an acl list
 *
 * Checks if all entries of an acl list (e.g. user list of a pe)
 * are contained in the master userset list.
 *
 * @param acl_list the acl list to check
 * @param alpp answer list pointer
 * @param master_userset_list the defined usersets to check against
 *
 * @return STATUS_OK, if everything is OK
 */
int 
userset_list_validate_acl_list(const lList *acl_list, lList **alpp, const lList *master_userset_list) {
   DENTER(TOP_LAYER);

   for_each_ep_lv(usp, acl_list) {
      if (!lGetElemStr(master_userset_list, US_name, lGetString(usp, US_name))) {
         ERROR(MSG_CQUEUE_UNKNOWNUSERSET_S, lGetString(usp, US_name) ? lGetString(usp, US_name) : "<nullptr>");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   DRETURN(STATUS_OK);
}


/**
 * @brief All user sets names in list must exist
 *
 * All the user set names in the acl_list must be defined in the qmaster
 * user set lists. The user set is diferentiated from user names by @ sign
 *
 * @param acl_list the acl list to check
 * @param nm field name
 * @param alpp answer list pointer
 * @param master_userset_list the defined usersets to check against
 *
 * @return STATUS_OK if no error,  STATUS_EUNKNOWN otherwise
 *
 * @note MT-NOTE: userset_list_validate_access() is not MT safe
 */
int
userset_list_validate_access(const lList *acl_list, int nm, lList **alpp, const lList *master_userset_list) {
   DENTER(TOP_LAYER);

   const lListElem *usp;
   for_each_ep(usp, acl_list) {
      char *user = (char *) lGetString(usp, nm);
      if (ocs::is_hgroup_name(user)){
         user++;  /* jump ower the @ sign */
         if (!lGetElemStr(master_userset_list, US_name, user)) {
            ERROR(MSG_CQUEUE_UNKNOWNUSERSET_S, user ? user : "<nullptr>");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }

   DRETURN(STATUS_OK);
}
/**
 * @brief Verify entries of a user set
 *
 * Validates all entries of a userset.
 *
 * @param userset the userset to check
 * @param alpp answer list pointer, if answer is expected. In any case, errors are output using the ERROR macro.
 *
 * @return STATUS_OK, if everything is OK
 */
int userset_validate_entries(lListElem *userset, lList **alpp) {
   DENTER(TOP_LAYER);

   int name_pos = lGetPosInDescr(UE_Type, UE_name);
   for_each_ep_lv(ep, lGetList(userset, US_entries)) {
      if (!lGetPosString(ep, name_pos)) {
         ERROR(SFNMAX, MSG_US_INVALIDUSERNAME);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   DRETURN(STATUS_OK);
}

/**
 * @brief Get readable type definition
 *
 * Returns a readable string of the userset type bitfield.
 *
 * @param userset the userset containing the requested information
 * @param buffer string buffer to hold the result string
 * @param answer_list errors will be reported here
 *
 * @return resulting string
 *
 * @see #userset_set_type_string
 */
const char *
userset_get_type_string(const lListElem *userset, lList **answer_list, dstring *buffer) {
   DENTER(TOP_LAYER);

   SGE_CHECK_POINTER_NULL(userset, answer_list);
   SGE_CHECK_POINTER_NULL(buffer, answer_list);

   bool append = false;
   uint32_t type = lGetUlong(userset, US_type);

   sge_dstring_clear(buffer);
   for (int i = 0; userset_types[i] != nullptr; i++) {
      if ((type & (1 << i)) != 0) {
         if (append) {
            sge_dstring_append(buffer, " ");
         }
         sge_dstring_append(buffer, userset_types[i]);
         append = true;
      }
   }
   DRETURN(sge_dstring_get_string(buffer));
}

/**
 * @brief Set userset type from string
 *
 * Takes a string representation for the userset type,
 *
 * @param userset the userset to change
 * @param answer_list errors will be reported here
 * @param value new value for userset type
 *
 * @return true on success, false on error, error message will be in answer_list
 *
 * @see #userset_get_type_string
 */
bool 
userset_set_type_string(lListElem *userset, lList **answer_list, const char *value) {
   DENTER(TOP_LAYER);

   SGE_CHECK_POINTER_FALSE(userset, answer_list);

   bool ret = true;
   uint32_t type = 0;

   if (value != nullptr && *value != 0) {
      if (!sge_parse_bitfield_str(value, userset_types, &type, 
                                  "userset type", answer_list, false)) {
         ret = false;
      }
   } else { /* value == nullptr || *value == 0 */
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_READCONFIGFILEEMPTYSPEC_S , "userset type");
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
 
   lSetUlong(userset, US_type, type);

   DRETURN(ret);
}

/**
 * @brief Render a userset list as a comma separated list of names
 *
 * @param this_list the usersets to render
 * @param[out] string receives the text, appended
 * @return the resulting text
 */
const char *
userset_list_append_to_dstring(const lList *this_list, dstring *string) {
   const char *ret = nullptr;

   DENTER(BASIS_LAYER);
   if (string != nullptr) {
      bool printed = false;

      for_each_ep_lv(elem, this_list) {
         sge_dstring_append(string, lGetString(elem, US_name));
         if (lNext(elem)) {
            sge_dstring_append(string, " ");
         }
         printed = true;
      }
      if (!printed) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }
   DRETURN(ret);
}

/**
 * @brief Find a group name in a userset's entry list
 *
 * A userset entry naming a UNIX group is written with a leading `@`; the
 * caller strips it before calling this.
 *
 * @param group_name_without_prefix the group name, without the `@`
 * @param user_group_list the userset's entries
 * @return the matching entry, or nullptr
 */
bool
find_name_as_group(const char *group_name_without_prefix, const lList *user_group_list) {
   DENTER(TOP_LAYER);
   bool found = false;

   dstring group_entry = DSTRING_INIT;
   const char *group = sge_dstring_sprintf(&group_entry, "@%s", group_name_without_prefix);
   if (lGetElemStr(user_group_list, UE_name, group) != nullptr) {
      found = true;
   } else if (ocs::is_pattern(group_name_without_prefix)) {
      for_each_ep_lv(acl_entry, user_group_list) {
         const char *entry_name = lGetString(acl_entry, UE_name);
         if (entry_name != nullptr && fnmatch(group, entry_name, 0) == 0) {
            found = true;
            break;
         }
      }
   }
   sge_dstring_free(&group_entry);
   DRETURN(found);
}

/**
 * @brief Find a user name in a userset's entry list
 *
 * @param user_name the user to look for
 * @param user_group_list the userset's entries
 * @return the matching entry, or nullptr
 */
bool
find_name_as_user(const char *user_name, const lList *user_group_list) {
   DENTER(TOP_LAYER);
   bool found = false;
   if (lGetElemStr(user_group_list, UE_name, user_name) != nullptr) {
      found = true;
   } else if (ocs::is_pattern(user_name)) {
      for_each_ep_lv(acl_entry, user_group_list) {
         const char *entry_name = lGetString(acl_entry, UE_name);
         if (entry_name != nullptr && fnmatch(user_name, entry_name, 0) == 0) {
            found = true;
            break;
         }
      }
   }
   DRETURN(found);
}

/**
 * @brief Is a user covered by one access list?
 *
 * An entry matches either the user name or one of the user's groups, the
 * latter written with a leading `@`.
 *
 * @param user the user name
 * @param group the user's primary group
 * @param grp_list the user's supplementary groups
 * @param acl the access list to check
 * @return 1 when the user is contained, 0 when not
 */
int
sge_contained_in_access_list(const char *user, const char *group, const lList *grp_list, const lListElem *acl) {
   DENTER(TOP_LAYER);
   bool found = false;
   const lList *user_list = lGetList(acl, US_entries);

   // user name
   if (user != nullptr) {
      found = find_name_as_user(user, user_list);
   }

   // primary group name
   if (!found && group != nullptr) {
      found = find_name_as_group(group, user_list);
   }

   // supplementary groups
   if (!found && grp_list != nullptr && mconf_get_enable_sup_grp_eval()) {
      for_each_ep_lv(grp_elem, grp_list) {
         found = find_name_as_group(lGetString(grp_elem, ST_name), user_list);
         if (found) {
            break;
         }
      }
   }

   DRETURN(found ? 1 : 0);
}

/**
 * @brief Is a user covered by any of several access lists?
 *
 * @param user the user name; may be nullptr
 * @param group the user's primary group; may be nullptr
 * @param grp_list the user's supplementary groups
 * @param acl the access lists to check, by name
 * @param acl_list the defined usersets the names resolve against
 * @return 1 when the user is contained in at least one, 0 when not, -1 when
 *         both `user` and `group` were nullptr
 */
int
sge_contained_in_access_list_(const char *user, const char *group, const lList *grp_list,
                              const lList *acl, const lList *acl_list) {
   DENTER(TOP_LAYER);

   for_each_ep_lv(acl_search, acl) {
      const lListElem *acl_found = lGetElemStr(acl_list, US_name, lGetString(acl_search, US_name));
      if (acl_found != nullptr) {
         /* ok - there is such an access list */
         if (sge_contained_in_access_list(user, group, grp_list, acl_found)) {
            DRETURN(1);
         }
      } else {
      	DPRINTF("cannot find userset list entry \"%s\"\n", lGetString(acl_search, US_name));
      }
   }
   DRETURN(0);
}

/** @brief Check if a user has access to a queue
 *
 * @param user The user name
 * @param group The primary group name
 * @param grp_list The supplementary group list
 * @param q The object
 * @param acl_list The list of ACLs
 * @return 1 if the user has access, 0 otherwise
 */
int
sge_has_access(const char *user, const char *group, const lList *grp_list, const lListElem *q, const lList *acl_list) {
   return sge_has_access_(user, group, grp_list, lGetList(q, QU_acl), lGetList(q, QU_xacl), acl_list);
}

/** @brief Check if a user has access to an object
 *
 * @param user The user name
 * @param group The primary group name
 * @param grp_list The supplementary group list
 * @param q_acl The ACL
 * @param q_xacl The XACL
 * @param acl_list The list of ACLs
 * @return 1 if the user has access, 0 otherwise
 */
int
sge_has_access_(const char *user, const char *group, const lList *grp_list,
                const lList *q_acl, const lList *q_xacl, const lList *acl_list) {
   DENTER(TOP_LAYER);

   // deny access when a xacl entry was not found in acl_list
   int ret = sge_contained_in_access_list_(user, group, grp_list, q_xacl, acl_list);
   if (ret < 0 || ret == 1) {
      DRETURN(0);
   }

   // no entry in allowance list - ok everyone
   if (!q_acl) {
       DRETURN(1);
   }

   ret = sge_contained_in_access_list_(user, group, grp_list, q_acl, acl_list);
   if (ret < 0) {
      DRETURN(0);
   }
   // we're explicitly allowed to access
   if (ret) {
      DRETURN(1);
   }

   // there is an allowance list but the owner isn't there -> denied
   DRETURN(0);
}

