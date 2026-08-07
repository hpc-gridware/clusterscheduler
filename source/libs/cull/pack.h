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
 * @brief The packbuffer and its version control
 */



#ifndef __BASIS_TYPES_H

#include <cstdio>
#include <cinttypes>

#endif

#define CHUNK  (1024*1024) ///< bytes a packbuffer grows by when it runs out of room
/**
 * @defgroup cull_pack_version Packbuffer version control
 * @brief How components of different cull versions reject each other's data
 *
 * Enhancements may change the format in which data is represented for spooling
 * and communication. So that components which cannot understand each other
 * reject the data cleanly instead of misreading it, every packbuffer carries a
 * version id, checked when the buffer is read.
 *
 * The two high bytes of the four byte id hold the version number. The two low
 * bytes are reserved for a subprotocol - for example the current binary format
 * versus an XML one.
 *
 * @note Grid Engine versions predating version control cannot handle messages
 *       carrying it. The version word is therefore preceded by a zero four byte
 *       integer, which makes every known older format fail cleanly.
 *
 * History of #CULL_VERSION, newest first:
 *
 * | Version | Change |
 * |---|---|
 * | `0x10021000` | current; raised for MUNGE authentication (CS-995) |
 * | `0x10020000` | fixed packing of the `lObject` type: the descriptor was sent twice |
 * | `0x10010000` | added information about attribute changes |
 * | `0x10000000` | introduction of version control |
 *
 * @see @ref cull_packing
 */

#define CULL_VERSION 0x10021000 ///< format id written into every packbuffer; see @ref cull_pack_version
#include <uti/sge_uidgid.h>

#define INTSIZE     4 ///< size of a packed integer, in 8 bit bytes
#define INTOFF      0 ///< offset of the first byte of a packed integer

#define MAX_USER_GROUP 512 ///< buffer size for a user or group name in a packbuffer
/**
 * @brief A byte stream that cull data is packed into and unpacked from
 *
 * Words go in in network byte order, so hosts of different architectures can
 * exchange the result. The buffer grows as needed; in `just_count` mode
 * nothing is written and only the required size is measured, which is how a
 * caller sizes a buffer before filling it.
 *
 * Every buffer carries the sender's identity, so the receiver can check who
 * sent it — see #cull_reresolve_check_user.
 *
 * @see @ref cull_packing
 */
typedef struct {
   char *head_ptr;                    ///< start of the buffer
   char *cur_ptr;                     ///< read or write position within the buffer
   size_t mem_size;                   ///< bytes allocated
   size_t bytes_used;                 ///< bytes actually filled
   bool just_count;                   ///< true to only measure the size, writing nothing
   int version;                       ///< #CULL_VERSION this buffer was written with
   char *auth_info;                   ///< the encoded authentication information
   uid_t uid;                         ///< user id of the sender
   gid_t gid;                         ///< primary group id of the sender
   char username[MAX_USER_GROUP];     ///< user name of the sender
   char groupname[MAX_USER_GROUP];    ///< primary group name of the sender
   int grp_amount;                    ///< number of supplementary groups in #grp_array
   ocs_grp_elem_t *grp_array;         ///< supplementary groups of the sender

} sge_pack_buffer;

int
init_packbuffer(sge_pack_buffer *pb, size_t initial_size, bool just_count = false, bool with_auth_info = true);

int
init_packbuffer_from_buffer(sge_pack_buffer *pb, char *buf, uint32_t buflen, bool with_auth_info = true);

void
clear_packbuffer(sge_pack_buffer *pb);

int pb_filled(sge_pack_buffer *pb);

int pb_unused(sge_pack_buffer *pb);

int pb_used(sge_pack_buffer *pb);

bool pb_are_equivalent(sge_pack_buffer *pb1, sge_pack_buffer *pb2);

void pb_print_to(sge_pack_buffer *pb, bool only_header, FILE *);

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

/**
 * @brief Switch tracing of every pack and unpack operation on or off
 *
 * @param on_off non-zero to trace, zero to stop
 */
void debugpack(int on_off);

/// Result of a pack or unpack operation; every such function returns one of these
enum {
   PACK_SUCCESS = 0,   ///< the operation succeeded
   PACK_ENOMEM = -1,   ///< the buffer could not be grown
   PACK_FORMAT = -2,   ///< the buffer is exhausted or its contents are malformed
   PACK_BADARG = -3,   ///< an argument was invalid, e.g. a nullptr buffer
   PACK_VERSION = -4,  ///< the buffer carries a different #CULL_VERSION
   PACK_AUTHINFO = -5  ///< the authentication information could not be read or verified
};

const char *cull_pack_strerror(int errnum);

bool cull_reresolve_check_user(sge_pack_buffer *pb, dstring *error, bool local_uid_gid, bool reresolve_user, bool reresolve_supp_grp);
