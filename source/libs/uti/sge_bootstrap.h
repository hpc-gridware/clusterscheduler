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

#include "basis_types.h"
#include "sge_dstring.h"
#include "setup_path.h"

void
bootstrap_mt_init(void);

const char *
bootstrap_get_admin_user(void);

void
bootstrap_set_admin_user(const char *admin_user);

const char *
bootstrap_get_default_domain(void);

void
bootstrap_set_default_domain(const char *default_domain);

bool
bootstrap_get_ignore_fqdn(void);

void
bootstrap_set_ignore_fqdn(bool ignore_fqdn);

const char *
bootstrap_get_spooling_method(void);

void
bootstrap_set_spooling_method(const char *spooling_method);

const char *
bootstrap_get_spooling_lib(void);

void
bootstrap_set_spooling_lib(const char *spooling_lib);

const char *
bootstrap_get_spooling_params(void);

void
bootstrap_set_spooling_params(const char *spooling_params);

const char *
bootstrap_get_binary_path(void);

void
bootstrap_set_binary_path(const char *binary_path);

const char *
bootstrap_get_qmaster_spool_dir(void);

void
bootstrap_set_qmaster_spool_dir(const char *qmaster_spool_dir);

const char *
bootstrap_get_security_mode(void);

void
bootstrap_set_security_mode(const char *security_mode);

bool
bootstrap_get_job_spooling(void);

void
bootstrap_set_job_spooling(bool job_spooling);

int
bootstrap_get_listener_thread_count(void);

void
bootstrap_set_listener_thread_count(int thread_count);

int
bootstrap_get_worker_thread_count(void);

void
bootstrap_set_worker_thread_count(int thread_count);

int
bootstrap_get_scheduler_thread_count(void);

void
bootstrap_set_scheduler_thread_count(int thread_count);

const char *
bootstrap_get_sge_root(void);

void
bootstrap_set_sge_root(const char *sge_root);

const char *
bootstrap_get_sge_cell(void);

void
bootstrap_set_sge_cell(const char *sge_cell);

u_long32
bootstrap_get_sge_qmaster_port(void);

void bootstrap_set_sge_qmaster_port(u_long32 sge_qmaster_port);

u_long32
bootstrap_get_sge_execd_port(void);

void
bootstrap_set_sge_execd_port(u_long32 sge_execd_port);

bool
bootstrap_is_from_services(void);

void
bootstrap_set_from_services(bool from_services);

bool
bootstrap_is_qmaster_internal(void);

void
bootstrap_set_qmaster_internal(bool qmaster_internal);

void
bootstrap_log_parameter(void);