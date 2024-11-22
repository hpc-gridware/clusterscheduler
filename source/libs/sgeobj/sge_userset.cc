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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <fnmatch.h>

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

/****** sgeobj/userset/userset_list_validate_acl_list() ***********************
*  NAME
*     userset_list_validate_acl_list() -- validate an acl list 
*
*  SYNOPSIS
*     int 
*     userset_list_validate_acl_list(lList *acl_list, lList **alpp)
*
*  FUNCTION
*     Checks if all entries of an acl list (e.g. user list of a pe) 
*     are contained in the master userset list.
*
*  INPUTS
*     lList *acl_list       - the acl list to check
*     lList **alpp          - answer list pointer
*
*  RESULT
*     int - STATUS_OK, if everything is OK
*******************************************************************************/
int 
userset_list_validate_acl_list(const lList *acl_list, lList **alpp, const lList *master_userset_list) {
   DENTER(TOP_LAYER);
   const lListElem *usp;

   for_each_ep(usp, acl_list) {
      if (!lGetElemStr(master_userset_list, US_name, lGetString(usp, US_name))) {
         ERROR(MSG_CQUEUE_UNKNOWNUSERSET_S, lGetString(usp, US_name) ? lGetString(usp, US_name) : "<nullptr>");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   DRETURN(STATUS_OK);
}


/****** sge_userset/userset_list_validate_access() *****************************
*  NAME
*     userset_list_validate_access() -- all user sets names in list must exist  
*
*  SYNOPSIS
*     int userset_list_validate_access(lList *acl_list, int nm, lList **alpp) 
*
*  FUNCTION
*     All the user set names in the acl_list must be defined in the qmaster
*     user set lists. The user set is diferentiated from user names by @ sign
*
*  INPUTS
*     lList *acl_list - the acl list to check
*     int nm          - field name
*     lList **alpp    - answer list pointer
*
*  RESULT
*     int - STATUS_OK if no error,  STATUS_EUNKNOWN otherwise
*
*  NOTES
*     MT-NOTE: userset_list_validate_access() is not MT safe 
*
*******************************************************************************/
int
userset_list_validate_access(const lList *acl_list, int nm, lList **alpp, const lList *master_userset_list) {
   DENTER(TOP_LAYER);

   const lListElem *usp;
   for_each_ep(usp, acl_list) {
      char *user = (char *) lGetString(usp, nm);
      if (is_hgroup_name(user)){
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
/****** sgeobj/userset/userset_validate_entries() *******************************
*  NAME
*     userset_validate_entries() -- verify entries of a user set
*
*  SYNOPSIS
*     int userset_validate_entries(lListElem *userset, lList **alpp, 
*                                  int start_up) 
*
*  FUNCTION
*     Validates all entries of a userset.
*
*  INPUTS
*     lListElem *userset - the userset to check
*     lList **alpp       - answer list pointer, if answer is expected. 
*                          In any case, errors are output using the 
*                          ERROR macro.
*
*  RESULT
*     int - STATUS_OK, if everything is OK
*******************************************************************************/
int userset_validate_entries(lListElem *userset, lList **alpp) {
   DENTER(TOP_LAYER);

   const lListElem *ep;
   int name_pos = lGetPosInDescr(UE_Type, UE_name);
   for_each_ep(ep, lGetList(userset, US_entries)) {
      if (!lGetPosString(ep, name_pos)) {
         ERROR(SFNMAX, MSG_US_INVALIDUSERNAME);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   DRETURN(STATUS_OK);
}

/****** sgeobj/userset/userset_get_type_string() **********************************
*  NAME
*     userset_get_type_string() -- get readable type definition
*
*  SYNOPSIS
*     const char* 
*     userset_get_type_string(const lListElem *userset, lList **answer_list,
*                           dstring *buffer) 
*
*  FUNCTION
*     Returns a readable string of the userset type bitfield.
*
*  INPUTS
*     const lListElem *userset - the userset containing the requested 
*                                information
*     dstring *buffer          - string buffer to hold the result string
*
*  RESULT
*     const char* - resulting string
*
*  SEE ALSO
*     sgeobj/userset/userset_set_type_string()
*******************************************************************************/
const char *
userset_get_type_string(const lListElem *userset, lList **answer_list, dstring *buffer) {
   DENTER(TOP_LAYER);

   SGE_CHECK_POINTER_NULL(userset, answer_list);
   SGE_CHECK_POINTER_NULL(buffer, answer_list);

   bool append = false;
   u_long32 type = lGetUlong(userset, US_type);

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

/****** sgeobj/userset/userset_set_type_string() ******************************
*  NAME
*     userset_set_type_string() -- set userset type from string 
*
*  SYNOPSIS
*     bool 
*     userset_set_type_string(lListElem *userset, lList **answer_list, 
*                           const char *value) 
*
*  FUNCTION
*     Takes a string representation for the userset type, 
*
*  INPUTS
*     lListElem *userset    - the userset to change
*     lList **answer_list - errors will be reported here
*     const char *value   - new value for userset type
*
*  RESULT
*     bool - true on success, 
*            false on error, error message will be in answer_list
*
*  SEE ALSO
*     sgeobj/userset/userset_get_type_string()
******************************************************************************/
bool 
userset_set_type_string(lListElem *userset, lList **answer_list, const char *value) {
   DENTER(TOP_LAYER);

   SGE_CHECK_POINTER_FALSE(userset, answer_list);

   bool ret = true;
   u_long32 type = 0;

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

const char *
userset_list_append_to_dstring(const lList *this_list, dstring *string) {
   const char *ret = nullptr;

   DENTER(BASIS_LAYER);
   if (string != nullptr) {
      const lListElem *elem;
      bool printed = false;

      for_each_ep(elem, this_list) {
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

bool
find_name_as_group(const char *group_name_without_prefix, const lList *user_group_list) {
   DENTER(TOP_LAYER);
   bool found = false;

   dstring group_entry = DSTRING_INIT;
   const char *group = sge_dstring_sprintf(&group_entry, "@%s", group_name_without_prefix);
   if (lGetElemStr(user_group_list, UE_name, group) != nullptr) {
      found = true;
   } else if (sge_is_pattern(group_name_without_prefix)) {
      const lListElem *acl_entry;
      for_each_ep(acl_entry, user_group_list) {
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

bool
find_name_as_user(const char *user_name, const lList *user_group_list) {
   DENTER(TOP_LAYER);
   bool found = false;
   if (lGetElemStr(user_group_list, UE_name, user_name) != nullptr) {
      found = true;
   } else if (sge_is_pattern(user_name)) {
      const lListElem *acl_entry;
      for_each_ep(acl_entry, user_group_list) {
         const char *entry_name = lGetString(acl_entry, UE_name);
         if (entry_name != nullptr && fnmatch(user_name, entry_name, 0) == 0) {
            found = true;
            break;
         }
      }
   }
   DRETURN(found);
}

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
      const lListElem *grp_elem;

      for_each_ep(grp_elem, grp_list) {
         found = find_name_as_group(lGetString(grp_elem, ST_name), user_list);
         if (found) {
            break;
         }
      }
   }

   DRETURN(found ? 1 : 0);
}

/* sge_contained_in_access_list_() returns
   1  yes it is contained in the acl
   0  no
   -1 got nullptr for user/group

   user, group: may be nullptr
*/
int
sge_contained_in_access_list_(const char *user, const char *group, const lList *grp_list,
                              const lList *acl, const lList *acl_list) {
   DENTER(TOP_LAYER);

   const lListElem *acl_search;
   for_each_ep(acl_search, acl) {
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

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   sge_has_access - determines if a user/group has access to a queue

   returns
      1 for true
      0 for false

*/
int
sge_has_access(const char *user, const char *group, const lList *grp_list,
               const lListElem *q, const lList *acl_list) {
   return sge_has_access_(user, group, grp_list, lGetList(q, QU_acl), lGetList(q, QU_xacl), acl_list);
}

/* needed to test also sge_queue_type structures without converting the
** whole queue
*/
int
sge_has_access_(const char *user, const char *group, const lList *grp_list,
                const lList *q_acl, const lList *q_xacl, const lList *acl_list) {
   int ret;

   DENTER(TOP_LAYER);

   ret = sge_contained_in_access_list_(user, group, grp_list, q_xacl, acl_list);
   if (ret < 0 || ret == 1) { /* also deny when an xacl entry was not found in acl_list */
      DRETURN(0);
   }

   if (!q_acl) {  /* no entry in allowance list - ok everyone */
       DRETURN(1);
   }

   ret = sge_contained_in_access_list_(user, group, grp_list, q_acl, acl_list);
   if (ret < 0) {
      DRETURN(0);
   }
   if (ret) {
      /* we're explicitly allowed to access */
      DRETURN(1);
   }

   /* there is an allowance list but the owner isn't there -> denied */
   DRETURN(0);
}

