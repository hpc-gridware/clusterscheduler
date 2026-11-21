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
 * @brief Declarations for string lists, and the group list conversions around them
 *
 * @see sge_str.cc
 */

#include "uti/sge_uidgid.h"

#include "tom/ocs_Types.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/sge_answer.h"

const char *str_list_append_to_dstring(ocs::tom::ConstList<ocs::tom::ST> this_list, dstring *string, char delimiter);

ocs::tom::ListPtr<ocs::tom::ST> str_list_parse_from_string(const char *string, const char *delimitor);

bool str_list_is_valid(ocs::tom::ConstList<ocs::tom::ST> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool str_list_transform_user_list(ocs::tom::ListPtr<ocs::tom::ST> &this_list,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *username);

ocs::tom::ListPtr<ocs::tom::ST> grp_list_array2list(int amount, const ocs_grp_elem_t *grp_array);
