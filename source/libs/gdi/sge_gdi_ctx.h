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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_error_class.h"
#include "uti/sge_profiling.h"
#include "uti/sge_uidgid.h"

#include "comm/commlib.h"

#include "gdi/sge_gdi_packet_type.h"

#include "uti/sge.h"

int 
sge_gdi2_setup(int component_id, u_long32 thread_id, lList **answer_list);

int 
sge_setup2(int progid, u_long32 thread_id, lList **alpp, bool is_qmaster_intern_client);

bool
sge_daemonize_prepare();

void
sge_daemonize_finalize();

int
sge_daemonize(int *keep_open, unsigned long nr_of_fds);

int
sge_gdi_ctx_class_prepare_enroll(lList **answer_list);
