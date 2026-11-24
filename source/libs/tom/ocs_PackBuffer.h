#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The packbuffer type, its version control and the result codes of the pack functions
 *
 * Split off tom/pack.h (CS-3103, M6), so that code which only passes a
 * packbuffer on, or checks a result code, does not see the pack functions.
 * The names stay as they are until the data types move into the typed object
 * model under new names.
 */

#ifndef __BASIS_TYPES_H

#include <cstdio>
#include <cinttypes>

#endif

#define CHUNK  (1024*1024) ///< bytes a packbuffer grows by when it runs out of room
/**
 * @defgroup tom_pack_version Packbuffer version control
 * @brief How components of different pack versions reject each other's data
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
 * History of #TOM_PACK_VERSION, newest first:
 *
 * | Version | Change |
 * |---|---|
 * | `0x10023000` | current; reduced descriptors go through a descriptor table per buffer (CS-2997) |
 * | `0x10022000` | a full descriptor is packed as a reference to its type (CS-2997) |
 * | `0x10021000` | raised for MUNGE authentication (CS-995) |
 * | `0x10020000` | fixed packing of the `lListElem *` type: the descriptor was sent twice |
 * | `0x10010000` | added information about attribute changes |
 * | `0x10000000` | introduction of version control |
 *
 * @see @ref tom_packing
 */

#define TOM_PACK_VERSION 0x10023000 ///< format id written into every packbuffer; see @ref tom_pack_version
#include <uti/sge_uidgid.h>

#define INTSIZE     4 ///< size of a packed integer, in 8 bit bytes
#define INTOFF      0 ///< offset of the first byte of a packed integer

#define MAX_USER_GROUP 512 ///< buffer size for a user or group name in a packbuffer
/**
 * @brief A byte stream that lists and elements are packed into and unpacked from
 *
 * Words go in in network byte order, so hosts of different architectures can
 * exchange the result. The buffer grows as needed; in `just_count` mode
 * nothing is written and only the required size is measured, which is how a
 * caller sizes a buffer before filling it.
 *
 * Every buffer carries the sender's identity, so the receiver can check who
 * sent it — see #tom_reresolve_check_user.
 *
 * @see @ref tom_packing
 */
namespace ocs::tom {
   /// the descriptor table of a pack buffer (N-W-4), see tom/ocs_PackCore.cc
   struct descr_table;
} // namespace ocs::tom

typedef struct {
   char *head_ptr;                    ///< start of the buffer
   char *cur_ptr;                     ///< read or write position within the buffer
   size_t mem_size;                   ///< bytes allocated
   size_t bytes_used;                 ///< bytes actually filled
   bool just_count;                   ///< true to only measure the size, writing nothing
   int version;                       ///< #TOM_PACK_VERSION this buffer was written with
   char *auth_info;                   ///< the encoded authentication information
   uid_t uid;                         ///< user id of the sender
   gid_t gid;                         ///< primary group id of the sender
   char username[MAX_USER_GROUP];     ///< user name of the sender
   char groupname[MAX_USER_GROUP];    ///< primary group name of the sender
   int grp_amount;                    ///< number of supplementary groups in #grp_array
   ocs_grp_elem_t *grp_array;         ///< supplementary groups of the sender
   /// descriptors written or read so far, nullptr until the first; freed by #clear_packbuffer
   ocs::tom::descr_table *dtab;

} sge_pack_buffer;

/// Result of a pack or unpack operation; every such function returns one of these
enum {
   PACK_SUCCESS = 0,   ///< the operation succeeded
   PACK_ENOMEM = -1,   ///< the buffer could not be grown
   PACK_FORMAT = -2,   ///< the buffer is exhausted or its contents are malformed
   PACK_BADARG = -3,   ///< an argument was invalid, e.g. a nullptr buffer
   PACK_VERSION = -4,  ///< the buffer carries a different #TOM_PACK_VERSION
   PACK_AUTHINFO = -5  ///< the authentication information could not be read or verified
};
