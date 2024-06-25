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

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SOLARIS)
/* Redefines from libscf.h */
#define	SCF_PG_RESTARTER		((const char *)"restarter")
#define	SCF_PROPERTY_NEXT_STATE		((const char *)"next_state")
#define	SCF_STATE_STRING_NONE		((const char *)"none")
#define	SCF_STATE_STRING_ONLINE		((const char *)"online")

int sge_smf_used();

int sge_smf_init_libs();

int sge_smf_contract_fork(char *err_str, int err_length);

void sge_smf_temporary_disable_instance();

char *sge_smf_get_instance_state();

char *sge_smf_get_instance_next_state();
#else
void dummy();
#endif
#ifdef __cplusplus
}
#endif

