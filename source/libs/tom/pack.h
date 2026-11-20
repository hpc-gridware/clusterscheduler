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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The pack buffer primitives: initialise, fill and read a packbuffer
 *
 * The buffer type, its version control and the result codes are in tom/ocs_PackBuffer.h.
 *
 * Not public (CS-3103, M6): part of the old function interface, reached through
 * cull/cull_old_api.h (as cull/pack.h) by libs/cull, libs/tom and the tests. Product
 * code fills and reads a packbuffer with the functions of tom/ocs_PackCore.h.
 */



#include "tom/ocs_PackBuffer.h"

int
init_packbuffer(sge_pack_buffer *pb, size_t initial_size, bool just_count = false, bool with_auth_info = true);

int
init_packbuffer_from_buffer(sge_pack_buffer *pb, char *buf, uint32_t buflen, bool with_auth_info = true);

void
clear_packbuffer(sge_pack_buffer *pb);

void
ocs_cull_pack_table_free(sge_pack_buffer *pb);

size_t
ocs_cull_pack_table_size(const sge_pack_buffer *pb);

int pb_filled(sge_pack_buffer *pb);

int pb_unused(sge_pack_buffer *pb);

int pb_used(sge_pack_buffer *pb);

bool pb_are_equivalent(sge_pack_buffer *pb1, sge_pack_buffer *pb2);

int repackint(sge_pack_buffer *, uint32_t);

int packint(sge_pack_buffer *, uint32_t);

int packint64(sge_pack_buffer *, uint64_t);

int packdouble(sge_pack_buffer *, double);

int packstr(sge_pack_buffer *, const char *);

int packbuf(sge_pack_buffer *, const char *, uint32_t);

int unpackint(sge_pack_buffer *, uint32_t *);

int unpackint64(sge_pack_buffer *, uint64_t *);

int unpackdouble(sge_pack_buffer *, double *);

int unpackstr(sge_pack_buffer *, char **);

int unpackbuf(sge_pack_buffer *, char **, int);


const char *cull_pack_strerror(int errnum);

bool cull_reresolve_check_user(sge_pack_buffer *pb, dstring *error, bool local_uid_gid, bool reresolve_user, bool reresolve_supp_grp);
