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
 
#include "sgeobj/cull/sge_suser_SU_L.h"

lListElem *suser_list_add(lList **suser_list, lList **answer_list,
                          const char *suser_name);

lListElem *suser_list_find(const lList *suser_list, const char *suser_name);

void suser_increase_job_counter(lListElem *suser);

void suser_decrease_job_counter(lListElem *suser);

u_long32 suser_get_job_counter(lListElem *suser);

int suser_check_new_job(const lListElem *job, u_long32 max_u_jobs, lList *master_suser_list);

u_long32 suser_job_count(const lListElem *job, const lList *master_suser_list);

int suser_register_new_job(const lListElem *job, u_long32 max_u_jobs, int force_registration, lList *master_suser_list);

void suser_unregister_job(const lListElem *job, const lList *master_suser_list);
