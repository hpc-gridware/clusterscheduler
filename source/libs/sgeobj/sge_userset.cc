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
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_List.h"

#include "msg_common.h"

#include <sge_str.h>

/**
 * @brief Create a new userset
 *
 * Returns a userset carrying nothing but its name and the type every userset
 * created through the configuration commands has had: both an access list and a
 * department. It is the template `qconf -mu` offers for a name that does not
 * exist yet, and the one qmaster builds for it (CS-2823).
 *
 * The two usersets qmaster creates for itself - the manager and the operator
 * list - are access lists only and are built where they are needed.
 *
 * @param answer_list AN_Type list
 * @param name the name the userset carries
 *
 * @return US_Type object, or an empty handle with the reason in the answer list
 */
ocs::tom::ElementPtr<ocs::tom::US>
userset_create(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::US> ret;

   if (name != nullptr) {
      ret = ocs::tom::ElementPtr<ocs::tom::US>::create();
      ocs::tom::set<US_name>(ret, name);
      ocs::tom::set<US_type>(ret, US_ACL | US_DEPT);
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }

   DRETURN(ret);
}

/**
 * @brief Validate an acl list
 *
 * Checks if all entries of an acl list (e.g. user list of a pe)
 * are contained in the master userset list.
 *
 * @param acl_list the acl list to check (US_Type), may be an empty handle
 * @param alpp answer list pointer
 * @param master_userset_list the defined usersets to check against (US_Type)
 *
 * @return STATUS_OK, if everything is OK
 */
int userset_list_validate_acl_list(const ocs::tom::ConstList<ocs::tom::US> acl_list,
                                   ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                   const ocs::tom::ConstList<ocs::tom::US> master_userset_list) {
   DENTER(TOP_LAYER);

   for (const auto usp : acl_list) {
      const char *name = ocs::tom::get<US_name>(usp);
      if (!ocs::tom::find_first<US_name>(master_userset_list, name)) {
         ERROR(MSG_CQUEUE_UNKNOWNUSERSET_S, name ? name : "<nullptr>");
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
 * The acl list stays untyped: its element type and field are given at run
 * time by @p nm.
 *
 * @param acl_list the acl list to check
 * @param nm field name
 * @param alpp answer list pointer
 * @param master_userset_list the defined usersets to check against (US_Type)
 *
 * @return STATUS_OK if no error,  STATUS_EUNKNOWN otherwise
 *
 * @note MT-NOTE: userset_list_validate_access() is not MT safe
 */
int userset_list_validate_access(const lList *acl_list, const int nm, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                 const ocs::tom::ConstList<ocs::tom::US> master_userset_list) {
   DENTER(TOP_LAYER);

   const lListElem *usp;
   for_each_ep(usp, acl_list) {
      char *user = (char *) lGetString(usp, nm);
      if (ocs::is_hgroup_name(user)){
         user++;  /* jump ower the @ sign */
         if (!ocs::tom::find_first<US_name>(master_userset_list, user)) {
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
 * @param userset the userset to check (US_Type)
 * @param alpp answer list pointer, if answer is expected. In any case, errors are output using the ERROR macro.
 *
 * @return STATUS_OK, if everything is OK
 */
int userset_validate_entries(const ocs::tom::ConstElement<ocs::tom::US> userset,
                             ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   for (const auto ep : ocs::tom::get<US_entries>(userset)) {
      if (!ocs::tom::get<UE_name>(ep)) {
         ERROR(SFNMAX, MSG_US_INVALIDUSERNAME);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   DRETURN(STATUS_OK);
}


/**
 * @brief Render userset type bits as their names
 *
 * Appends the names of the set bits, separated by blanks. Nothing is appended
 * when no bit with a name is set; the caller decides what stands for such a
 * value (the spooled form is the plain number then).
 *
 * @param type the bit field (`US_type`)
 * @param[out] buffer receives the names, appended
 * @return the buffer's text, or nullptr if no name was appended
 *
 * @see #ocs_userset_type_parse
 */
const char *ocs_userset_type_append_to_dstring(const uint32_t type, dstring *buffer) {
   DENTER(TOP_LAYER);

   bool append = false;

   for (int i = 0; userset_types[i] != nullptr; i++) {
      if ((type & (1 << i)) != 0) {
         if (append) {
            sge_dstring_append(buffer, " ");
         }
         sge_dstring_append(buffer, userset_types[i]);
         append = true;
      }
   }
   DRETURN(append ? sge_dstring_get_string(buffer) : nullptr);
}


/**
 * @brief Parse a userset type from its names
 *
 * A missing or empty value is an error, as is a name given twice or an
 * unknown name; the bits understood before such a name are stored all the
 * same, a missing value stores no bit.
 *
 * @param[out] answer_list errors will be reported here
 * @param value the type names, or nullptr
 * @return whether the value was understood, and the bits to store in either case
 *
 * @note MT-NOTE: ocs_userset_type_parse() is not MT safe, as sge_parse_bitfield_str() is not
 *
 * @see #ocs_userset_type_append_to_dstring
 */
std::pair<bool, uint32_t> ocs_userset_type_parse(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value) {
   DENTER(TOP_LAYER);

   bool ret = true;
   uint32_t type = 0;

   if (value != nullptr && *value != 0) {
      if (!sge_parse_bitfield_str(value, userset_types, &type, "userset type", answer_list, false)) {
         ret = false;
      }
   } else { /* value == nullptr || *value == 0 */
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_READCONFIGFILEEMPTYSPEC_S , "userset type");
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   DRETURN(std::make_pair(ret, type));
}


/**
 * @brief Render a userset list as a blank separated list of names
 *
 * @param this_list the usersets to render (US_Type), may be an empty handle
 * @param[out] string receives the text, appended
 * @return the resulting text, nullptr if @p string is nullptr
 */
const char *userset_list_append_to_dstring(const ocs::tom::ConstList<ocs::tom::US> this_list, dstring *string) {
   DENTER(BASIS_LAYER);

   const char *ret = nullptr;

   if (string != nullptr) {
      bool printed = false;

      for (const auto elem : this_list) {
         sge_dstring_append(string, ocs::tom::get<US_name>(elem));
         if (elem.next()) {
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
 * @param user_group_list the userset's entries (UE_Type), may be an empty handle
 * @return true if an entry names the group or a pattern of an entry matches it
 */
static bool find_name_as_group(const char *group_name_without_prefix,
                               const ocs::tom::ConstList<ocs::tom::UE> user_group_list) {
   DENTER(TOP_LAYER);
   bool found = false;

   dstring group_entry = DSTRING_INIT;
   const char *group = sge_dstring_sprintf(&group_entry, "@%s", group_name_without_prefix);
   if (ocs::tom::find_first<UE_name>(user_group_list, group)) {
      found = true;
   } else if (ocs::is_pattern(group_name_without_prefix)) {
      for (const auto acl_entry : user_group_list) {
         const char *entry_name = ocs::tom::get<UE_name>(acl_entry);
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
 * @param user_group_list the userset's entries (UE_Type), may be an empty handle
 * @return true if an entry names the user or a pattern of an entry matches it
 */
static bool find_name_as_user(const char *user_name, const ocs::tom::ConstList<ocs::tom::UE> user_group_list) {
   DENTER(TOP_LAYER);
   bool found = false;
   if (ocs::tom::find_first<UE_name>(user_group_list, user_name)) {
      found = true;
   } else if (ocs::is_pattern(user_name)) {
      for (const auto acl_entry : user_group_list) {
         const char *entry_name = ocs::tom::get<UE_name>(acl_entry);
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
 * The supplementary groups stay an untyped list: callers pass a bare nullptr
 * for them together with typed access lists.
 *
 * @param user the user name
 * @param group the user's primary group
 * @param grp_list the user's supplementary groups (ST_Type), checked; may be nullptr
 * @param acl the access list to check (US_Type)
 * @return 1 when the user is contained, 0 when not
 */
int sge_contained_in_access_list(const char *user, const char *group, const lList *grp_list,
                                 const ocs::tom::ConstElement<ocs::tom::US> acl) {
   DENTER(TOP_LAYER);
   bool found = false;
   const auto user_list = ocs::tom::get<US_entries>(acl);

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
      for (const auto grp_elem : ocs::tom::ConstList<ocs::tom::ST>(grp_list)) {
         found = find_name_as_group(ocs::tom::get<ST_name>(grp_elem), user_list);
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
 * @param grp_list the user's supplementary groups (ST_Type), checked; may be nullptr
 * @param acl the access lists to check, by name (US_Type), may be an empty handle
 * @param acl_list the defined usersets the names resolve against (US_Type)
 * @return 1 when the user is contained in at least one, 0 when not, -1 when
 *         both `user` and `group` were nullptr
 */
int sge_contained_in_access_list_(const char *user, const char *group, const lList *grp_list,
                                  const ocs::tom::ConstList<ocs::tom::US> acl,
                                  const ocs::tom::ConstList<ocs::tom::US> acl_list) {
   DENTER(TOP_LAYER);

   for (const auto acl_search : acl) {
      const auto acl_found = ocs::tom::find_first<US_name>(acl_list, ocs::tom::get<US_name>(acl_search));
      if (acl_found) {
         /* ok - there is such an access list */
         if (sge_contained_in_access_list(user, group, grp_list, acl_found)) {
            DRETURN(1);
         }
      } else {
         DPRINTF("cannot find userset list entry \"%s\"\n", ocs::tom::get<US_name>(acl_search));
      }
   }
   DRETURN(0);
}


/**
 * @brief Check if a user has access to a queue
 *
 * @param user The user name
 * @param group The primary group name
 * @param grp_list The supplementary group list (ST_Type), checked; may be nullptr
 * @param q The queue instance (QU_Type)
 * @param acl_list The list of ACLs (US_Type)
 * @return 1 if the user has access, 0 otherwise
 */
int sge_has_access(const char *user, const char *group, const lList *grp_list,
                   const ocs::tom::ConstElement<ocs::tom::QU> q, const ocs::tom::ConstList<ocs::tom::US> acl_list) {
   return sge_has_access_(user, group, grp_list, ocs::tom::get<QU_acl>(q), ocs::tom::get<QU_xacl>(q), acl_list);
}


/**
 * @brief Check if a user has access to an object
 *
 * The supplementary groups stay an untyped list: callers pass a bare nullptr
 * for them together with typed access lists.
 *
 * @param user The user name
 * @param group The primary group name
 * @param grp_list The supplementary group list (ST_Type), checked; may be nullptr
 * @param q_acl The ACL (US_Type), may be an empty handle
 * @param q_xacl The XACL (US_Type), may be an empty handle
 * @param acl_list The list of ACLs (US_Type)
 * @return 1 if the user has access, 0 otherwise
 */
int sge_has_access_(const char *user, const char *group, const lList *grp_list,
                    const ocs::tom::ConstList<ocs::tom::US> q_acl, const ocs::tom::ConstList<ocs::tom::US> q_xacl,
                    const ocs::tom::ConstList<ocs::tom::US> acl_list) {
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


