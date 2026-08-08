#pragma once
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

/** @file
 * @brief Declarations for usersets: access lists and departments
 *
 * @see sge_userset.cc
 */

#include "sgeobj/cull/sge_userset_US_L.h"
#include "sgeobj/cull/sge_userset_UE_L.h"
#include "sgeobj/cull/sge_userset_JC_L.h"

/**
 * @name Userset types, stored in `US_type`
 *
 * A userset can be both at once: an access list decides who may use a queue,
 * a department carries the share tree and ticket configuration for its
 * members.
 * @{
 */
#define US_ACL       (1<<0) ///< the userset is an access list
#define US_DEPT      (1<<1) ///< the userset is a department
/** @} */

/**
 * @name Reserved userset names
 * @{
 */
#define DEADLINE_USERS     "deadlineusers"     ///< users allowed to submit deadline jobs
#define DEFAULT_DEPARTMENT "defaultdepartment" ///< the department a user without one belongs to
#define AR_USERS           "arusers"           ///< users allowed to create advance reservations

/**
 * @brief The userset backing the manager list (CS-2394)
 *
 * Managed through `qconf -am` / `-dm`, not through the userset commands.
 */
#define MANAGER_USERSET    "manager"
/**
 * @brief The userset backing the operator list (CS-2394)
 *
 * Managed through `qconf -ao` / `-do`, not through the userset commands.
 */
#define OPERATOR_USERSET   "operator"
/** @} */

/// The userset type names, indexed by their `US_type` bit
extern const char *userset_types[];

int
userset_validate_entries(lListElem *object, lList **answer_list);

int userset_list_validate_acl_list(const lList *acl_list, lList **alpp, const lList *master_userset_list);

int userset_list_validate_access(const lList *acl_list, int nm, lList **alpp, const lList *master_userset_list);

const char *
userset_get_type_string(const lListElem *userset, lList **answer_list, dstring *buffer);

bool 
userset_set_type_string(lListElem *userset, lList **answer_list, const char *value);

const char *
userset_list_append_to_dstring(const lList *this_list, dstring *string);

int
sge_contained_in_access_list(const char *user, const char *group, const lList *grp_list, const lListElem *acl);

int sge_has_access(const char *user, const char *group, const lList *grp_list, const lListElem *q, const lList *acl_list);

int sge_has_access_(const char *user, const char *group, const lList *grp_list, const lList *q_acl,
                    const lList *q_xacl, const lList *acl_list);

int sge_contained_in_access_list_(const char *user, const char *group, const lList *grp_list,
                                  const lList *acl, const lList *acl_list);
