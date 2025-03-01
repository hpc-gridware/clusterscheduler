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

#include "uti/sge_arch.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_feature.h"

#include "shutdown.h"
#include "msg_daemons_common.h"

void starting_up()
{
   u_long32 old_ll = log_state_get_log_level();
   dstring ds;
   dstring ds2 = DSTRING_INIT;
   dstring ds3 = DSTRING_INIT;
   char buffer[256];

   DENTER(TOP_LAYER);

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   log_state_set_log_level(LOG_INFO);
   
   if (feature_get_active_featureset_id() == FEATURE_NO_SECURITY) {
      sge_dstring_copy_string(&ds2, feature_get_product_name(FS_VERSION, &ds)); 
   } else {   
      sge_dstring_sprintf(&ds2, "%s (%s)", 
                          feature_get_product_name(FS_VERSION, &ds),
                          feature_get_featureset_name(
                                       feature_get_active_featureset_id())); 
   }
   INFO(MSG_STARTUP_STARTINGUP_SSS, feature_get_product_name(FS_SHORT, &ds3), sge_dstring_get_string(&ds2), sge_get_arch());
   if (bootstrap_get_use_munge()) {
      INFO(SFNMAX, MSG_STARTUP_USING_MUNGE);
   }

   sge_dstring_free(&ds2);
   sge_dstring_free(&ds3);
   log_state_set_log_level(old_ll);

   DRETURN_VOID;
}

/******************************************************************************/
void sge_shutdown(int i)
{
   u_long32 old_ll = log_state_get_log_level();
   dstring ds;
   dstring ds2 = DSTRING_INIT;
   char buffer[256];
   
   DENTER(TOP_LAYER);

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   log_state_set_log_level(LOG_INFO);
   if (feature_get_active_featureset_id() == FEATURE_NO_SECURITY) {
      sge_dstring_copy_string(&ds2, feature_get_product_name(FS_VERSION, &ds)); 
   } else {   
      sge_dstring_sprintf(&ds2, "%s (%s)", 
                          feature_get_product_name(FS_VERSION, &ds),
                          feature_get_featureset_name(
                                       feature_get_active_featureset_id())); 
   }
   if (i != 0) {
      INFO(MSG_SHADOWD_CONTROLLEDSHUTDOWN_SI, sge_dstring_get_string(&ds2), i);
   } else {
      INFO(MSG_SHADOWD_CONTROLLEDSHUTDOWN_S, sge_dstring_get_string(&ds2));
   }

   sge_dstring_free(&ds2);
   log_state_set_log_level(old_ll);

   sge_exit(i);
}
