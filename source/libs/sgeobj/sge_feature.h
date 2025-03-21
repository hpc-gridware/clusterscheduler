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

#include "sgeobj/cull/sge_feature_FES_L.h"

typedef enum {
   FS_SHORT,
   FS_LONG,
   FS_VERSION,
   FS_SHORT_VERSION,
   FS_LONG_VERSION
} featureset_product_name_id_t;

typedef enum {
   FEATURE_UNINITIALIZED = 0, 
   FEATURE_NO_SECURITY,             /* No security mode active */
   FEATURE_AFS_SECURITY,            /* AFS security */
   FEATURE_DCE_SECURITY,            /* DCE security */
   FEATURE_KERBEROS_SECURITY,       /* KERBEROS security */
   FEATURE_CSP_SECURITY,            /* CSP security */
   FEATURE_MUNGE_SECURITY,          /* Munge security */

   /* DON'T CHANGE THE ORDER OF THE ENTRIES ABOVE */
 
   FEATURE_LAST_ENTRY
} feature_id_t;

typedef struct {
   feature_id_t id;
   const char *name;
} featureset_names_t;            

void    feature_set_already_read_from_file(int i);
int     feature_get_already_read_from_file();
lList** feature_get_master_featureset_list();


void            feature_initialize();
int             feature_initialize_from_string(const char *mode, lList **answer_list);
void            feature_activate(feature_id_t id);
const char*     feature_get_featureset_name(feature_id_t id);
feature_id_t    feature_get_active_featureset_id();
bool            feature_is_enabled(feature_id_t id);
const char*     feature_get_product_name(featureset_product_name_id_t style, dstring *buffer);
