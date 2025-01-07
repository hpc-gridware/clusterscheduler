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

#include "gdi/ocs_gdi_Packet.h"

#include "sgeobj/cull/sge_manop_UM_L.h"
#include "sgeobj/cull/sge_manop_UO_L.h"

bool user_list_is_user_grp_sgrp_in_list(const ocs::gdi::Packet *packet, const lList *usr_grp_sgrp_list, int nm);
bool user_is_ar_user(const ocs::gdi::Packet *packet, const lList *ar_users_list);
bool user_is_deadline_user(const ocs::gdi::Packet *packet, const lList *ar_users_list);
bool manop_is_manager(const ocs::gdi::Packet *packet, const lList *master_manager_list);
bool manop_is_operator(const ocs::gdi::Packet *packet, const lList *master_manager_list, const lList *master_operator_list);
