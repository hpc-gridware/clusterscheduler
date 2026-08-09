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
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface of the qconf host group switches
 */

#include "sgeobj/sge_daemonize.h"

lListElem *hgroup_get_via_gdi(lList **answer_list, const char *group);

/** @brief Send one host group to qmaster
 *
 * @param this_elem the host group (`HGRP_Type`) to send
 * @param answer_list used to return error messages
 * @param gdi_command `ADD`, `MOD` or `DEL`
 * @return true on success; false with `answer_list` filled otherwise *
 * @warning This declaration takes `uint32_t`, but the definition in the sibling
 *          `.cc` takes `ocs::gdi::Command`, which is an `enum class` and so a
 *          distinct type. The two are therefore different overloads: this one
 *          is declared, never defined, and never called - every caller passes
 *          an `ocs::gdi::Command` and binds to the definition. A caller that
 *          did pass a plain `uint32_t` would fail to link. Left as it is;
 *          changing a declaration is a code change.
 */
bool hgroup_add_del_mod_via_gdi(lListElem *this_elem, lList **answer_list, uint32_t gdi_command);

bool hgroup_show(lList **answer_list, const char *name);

bool hgroup_show_structure(lList **answer_list, const char *name, bool show_tree);

bool hgroup_add(lList **answer_list, const char *name, bool is_name_validate);

bool hgroup_modify(lList **answer_list, const char *name);

bool hgroup_delete(lList **answer_list, const char *name);

bool hgroup_add_from_file(lList **answer_list, const char *filename);

bool hgroup_modify_from_file(lList **answer_list, const char *filename);
