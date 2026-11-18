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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The share tree, and carrying usage across a replacement
 */

#include "sgeobj/sge_daemonize.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "sgeobj/sge_answer.h"

int
sge_add_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::STN> ep, lList **lpp,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *ruser, const char *rhost);

int
sge_mod_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::STN> ep, lList **lpp,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *ruser, const char *rhost);
OCS_TRANSITIONAL int sge_mod_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **lpp,
                                       ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *ruser, const char *rhost);

int
sge_del_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **lpp, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                  char *ruser, char *rhost);

int
update_sharetree(lList *dst, const lList *src);

ocs::tom::ConstElement<ocs::tom::STN>
getNode(ocs::tom::ConstList<ocs::tom::STN> share_tree, const char *name, int node_type, int recurse);

int
check_sharetree(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::STN> node,
                ocs::tom::ConstList<ocs::tom::UU> user_list,
                ocs::tom::ConstList<ocs::tom::PR> project_list, ocs::tom::ConstElement<ocs::tom::PR> project,
                ocs::tom::ListPtr<ocs::tom::STN> &found);
