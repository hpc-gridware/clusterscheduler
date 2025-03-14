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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/



#ifndef __BASIS_TYPES_H

#include <cstdio>
#include "basis_types.h"

#endif

#define CHUNK  (1024*1024)

/****** cull/pack/-Versioncontrol ***************************************
*
*  NAME
*     Versioncontrol -- handling of different cull versions
*
*  SYNOPSIS
*     #define CULL_VERSION 0x10010000
*
*  FUNCTION
*     Enhancements of the product may make it necessary to change the
*     way how (in which format) data is represented for spooling and  
*     communication.
*
*     To ensure that components of different cull versions, that cannot
*     communicate, cleanly reject data of a different format, a cull
*     version check has been implemented.
*
*     A cull version id has been introduced that will be checked when
*     cull packbuffers are read.
*
*     Currently the 2 highest bytes of the 4 byte integer used to
*     encode version information are used for a version number.
*     The 2 lower bytes could be used to encode for example a 
*     subversion / subprotocol like cull binary format (the current 
*     implementation) and XML format.
*
*  NOTES
*     Older Grid Engine versions that had no version control cannot 
*     properly handle messages with version information.
*     Therefore the version information is preceeded by a 0 value
*     4 byte integer, which will result in some sort of error handling
*     for all known/used message formats.
*
*     Current CULL_VERSION:   0x10020000
*                             Fixed a bug with packing of lObject type:
*                             Descriptor was sent twice.
*     
*     Former  CULL_VERSIONs:  0x10010000
*                             Added information about attribute changes.
*                             0x10000000
*                             Introduction of version control.
*
*  SEE ALSO
*     cull/pack/--CULL_Packing
*
****************************************************************************
*/
#define CULL_VERSION 0x10021000
#include <uti/sge_uidgid.h>

#define MAX_USER_GROUP 512
typedef struct {
   char *head_ptr;
   char *cur_ptr;
   size_t mem_size;
   size_t bytes_used;
   bool just_count;
   int version;
   char *auth_info;
   uid_t uid;
   gid_t gid;
   char username[MAX_USER_GROUP];
   char groupname[MAX_USER_GROUP];
   int grp_amount;
   ocs_grp_elem_t *grp_array;

} sge_pack_buffer;

int
init_packbuffer(sge_pack_buffer *pb, size_t initial_size, bool just_count = false, bool with_auth_info = true);

int
init_packbuffer_from_buffer(sge_pack_buffer *pb, char *buf, u_long32 buflen, bool with_auth_info = true);

void
clear_packbuffer(sge_pack_buffer *pb);

int pb_filled(sge_pack_buffer *pb);

int pb_unused(sge_pack_buffer *pb);

int pb_used(sge_pack_buffer *pb);

bool pb_are_equivalent(sge_pack_buffer *pb1, sge_pack_buffer *pb2);

void pb_print_to(sge_pack_buffer *pb, bool only_header, FILE *);

int repackint(sge_pack_buffer *, u_long32);

int packint(sge_pack_buffer *, u_long32);

int packint64(sge_pack_buffer *, u_long64);

int packdouble(sge_pack_buffer *, double);

int packstr(sge_pack_buffer *, const char *);

int packbuf(sge_pack_buffer *, const char *, u_long32);

int unpackint(sge_pack_buffer *, u_long32 *);

int unpackint64(sge_pack_buffer *, u_long64 *);

int unpackdouble(sge_pack_buffer *, double *);

int unpackstr(sge_pack_buffer *, char **);

int unpackbuf(sge_pack_buffer *, char **, int);

void debugpack(int on_off);

/*
   these return values should be supported by all
   un-/packing functions
*/
enum {
   PACK_SUCCESS = 0,
   PACK_ENOMEM = -1,
   PACK_FORMAT = -2,
   PACK_BADARG = -3,
   PACK_VERSION = -4,
   PACK_AUTHINFO = -5
};

const char *cull_pack_strerror(int errnum);

bool cull_reresolve_check_user(sge_pack_buffer *pb, dstring *error, bool local_uid_gid, bool reresolve_user, bool reresolve_supp_grp);
