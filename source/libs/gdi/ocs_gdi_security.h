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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull_list.h"
#include "uti/sge_dstring.h"
#include "uti/sge_security.h"

#ifdef KERBEROS
#   include "krb_lib.h"
#endif

#ifdef KERBEROS
int kerb_job(lListElem *jelem, struct dispatch_entry *de);
#endif

void tgt2cc(lListElem *jep, const char *rhost);
void tgtcclr(lListElem *jep, const char *rhost);
int set_sec_cred(const char *sge_root, const char *mastername, lListElem *job, lList **alpp);
void delete_credentials(const char *sge_root, lListElem *jep);
bool cache_sec_cred(const char *sge_root, lListElem *jep, const char *rhost);
int store_sec_cred(const char *sge_root, lListElem *jep, int do_authentication, lList **alpp);
int store_sec_cred2(const char* sge_root, 
                    const char* unqualified_hostname, 
                    lListElem *jelem, 
                    int do_authentication, 
                    int *general, 
                    dstring *err_str);
