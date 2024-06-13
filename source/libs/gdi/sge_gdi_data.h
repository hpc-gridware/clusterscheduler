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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"

#include "uti/sge_dstring.h"
#include "uti/sge_csp_path.h"

void
gdi_data_mt_done();

const char *
gdi_data_get_master_host();

bool
gdi_data_is_setup();

void
gdi_data_set_setup(bool is_setup);

u_long64
gdi_data_get_timestamp_qmaster_file();

void
gdi_data_set_timestamp_qmaster_file(u_long64 timestamp_qmaster_file);

sge_error_class_t *
gdi_data_get_error_handle();

void
gdi_data_set_error_handle(sge_error_class_t *error_handle);

int
gdi_data_get_last_commlib_error();

void
gdi_data_set_last_commlib_error(int last_commlib_error);

const char *
gdi_data_get_ssl_private_key();

void
gdi_data_set_ssl_private_key(const char *ssl_private_key);

const char *
gdi_data_get_ssl_certificate();

void
gdi_data_set_ssl_certificate(const char *ssl_certificate);

sge_csp_path_class_t *
gdi_data_get_csp_path_obj();

void
gdi_data_set_csp_path_obj(sge_csp_path_class_t *csp_path_obj);

u_long32
gdi_data_get_next_request_id();

void
gdi_set_master_host(const char *master_host);

