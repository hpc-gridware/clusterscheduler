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

#include "sgeobj/sge_daemonize.h"

int
sge_add_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **lpp, lList **alpp, char *ruser, char *rhost);

int
sge_mod_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **lpp, lList **alpp, char *ruser, char *rhost);

int
sge_del_sharetree(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **lpp, lList **alpp, char *ruser, char *rhost);

int
update_sharetree(lList *dst, const lList *src);

lListElem *
getNode(const lList *share_tree, const char *name, int node_type, int recurse);

int
check_sharetree(lList **alpp, lListElem *node, const lList *user_list, const lList *project_list, lListElem *project,
                lList **found);
