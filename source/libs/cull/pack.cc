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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The packbuffer: a platform independent byte stream
 */

#include "cull/pack.h"

#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <netinet/in.h>

#if defined(SOLARIS) || defined(DARWIN)
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#endif

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
/// Suppresses the monitoring code in the rmon macros for this file
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "cull/msg_cull.h"
#include "tom/ocs_PackCore.h"

#include <cinttypes>

#if 0
#   undef PACK_LAYER
#   define PACK_LAYER BASIS_LAYER
#endif


/**
 * @defgroup cull_packing Packbuffers
 * @brief A platform independent exchange format for cull data
 *
 * Data is written into a packbuffer, individual words in network byte order,
 * so hosts of different architectures can exchange it. The contents can
 * optionally be compressed.
 *
 * @note Other platform independent formats, like XML, should be implemented.
 *
 * @see @ref cull_pack_version
 */


/* -------------------------------

   get chunk_size

 */
/**
 * @brief The size a packbuffer grows by when it runs out of room
 *
 * @return the chunk size in bytes
 */
int pack_get_chunk() {
   return CHUNK;
}

/**
 * @brief Initialize packing buffer
 *
 * Initialize a packing buffer.
 * Allocates the necessary memory. If more memory is needed during the use
 * of the packbuffer, it will be reallocated increasing the size by
 * chunk_size (see function pack_set_chunk).
 *
 * Since version 6.0, version information is provided in the packbuffer and
 * is included in sent messages.
 * For best possible backward interoperability with former versions, an
 * integer with value 0 is padded before the version information as first
 * word in the packbuffer. This triggeres error handling in former versions.
 *
 * Functions using packing buffers in GDI or related code should use the
 * function ocs::gdi::Client::sge_gdi_packet_get_pb_size() to find the correct
 * "initial_size".
 *
 * @param pb the packbuffer to initialize
 * @param initial_size the amount of memory to be allocated at initialization. If a value of 0 is given as initial_size, a size of chunk_size (global variable, see function pack_set_chunk) will be used.
 * @param just_count if true, no memory will be allocated and the "just_count" property of the packbuffer will be set.
 * @param with_auth_info if true, the packbuffer will be initialized with authentication information (user and group information) This is the case for all pack buffers which are used for communication between components. For spooling operations no auth_info is needed.
 *
 * @return PACK_SUCCESS on success PACK_ENOMEM  if memory allocation fails PACK_FORMAT  if no valid packbuffer is passed
 *
 * @note MT-NOTE: init_packbuffer() is MT safe (assumptions)
 *
 * @see `pack_set_chunk()`, `sge_gdi_packet_get_pb_size()`
 *
 * @note A thin wrapper over ocs::tom::init_buffer().
 */
int
init_packbuffer(sge_pack_buffer *pb, size_t initial_size, bool just_count, bool with_auth_info) {
   try {
      ocs::tom::init_buffer(pb, initial_size, just_count, with_auth_info);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Wrap an existing byte block in a packbuffer for reading
 *
 * The read pointer is set to the beginning. @p buf is not copied and stays
 * owned by the caller.
 *
 * @param pb the buffer to initialise
 * @param buf the bytes to read from
 * @param buflen how many bytes @p buf holds
 * @param with_auth_info true when the block starts with authentication information
 * @return #PACK_SUCCESS, #PACK_VERSION on a version mismatch, or #PACK_AUTHINFO
 *
 * @note MT-NOTE: init_packbuffer_from_buffer() is MT safe (assumptions)
 *
 * @note A thin wrapper over ocs::tom::init_buffer_from().
 */
int
init_packbuffer_from_buffer(sge_pack_buffer *pb, char *buf, uint32_t buflen, bool with_auth_info) {
   try {
      ocs::tom::init_buffer_from(pb, buf, buflen, with_auth_info);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**************************************************************/
/* MT-NOTE: clear_packbuffer() is MT safe */
/**
 * @brief Release a packbuffer's memory and the identity it carries
 *
 * @param pb the buffer to clear; may be nullptr
 *
 * @note A thin wrapper over ocs::tom::clear_buffer().
 */
void clear_packbuffer(sge_pack_buffer *pb) {
   ocs::tom::clear_buffer(pb);
}

/**
 * @brief Does the buffer hold anything beyond its version header?
 *
 * @param pb the buffer to test
 * @return non-zero when payload has been written
 */
int pb_filled(
        sge_pack_buffer *pb
) {
   /* do we have more bytes used than the version information? */
   return (pb_used(pb) > (2 * INTSIZE));
}

/**
 * @brief How much room is left in the buffer?
 *
 * @param pb the buffer to measure
 * @return the number of allocated but unused bytes
 */
int pb_unused(
        sge_pack_buffer *pb
) {
   return (pb->mem_size - pb->bytes_used);
}

/**
 * @brief How much of the buffer is filled?
 *
 * @param pb the buffer to measure
 * @return the number of bytes written so far
 */
int pb_used(
        sge_pack_buffer *pb
) {
   return pb->bytes_used;
}

/* --------------------------------------------------------- */
/*                                                           */
/*            low level functions for packing                */
/*                                                           */
/* --------------------------------------------------------- */

/**
 * @brief Append a 32 bit integer, in network byte order
 *
 * @param pb the buffer to append to
 * @param i the value to write
 * @return #PACK_SUCCESS, or #PACK_ENOMEM when the buffer could not grow
 *
 * @note A thin wrapper over ocs::tom::put_uint32().
 */
int packint(sge_pack_buffer *pb, uint32_t i) {
   try {
      ocs::tom::put_uint32(pb, i);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Overwrite the 32 bit integer at the current position
 *
 * @param pb the buffer to write into
 * @param i the value to write
 * @return #PACK_SUCCESS, or #PACK_ENOMEM when the buffer could not grow
 *
 * @note A thin wrapper over ocs::tom::reput_uint32().
 */
int repackint(sge_pack_buffer *pb, uint32_t i) {
   try {
      ocs::tom::reput_uint32(pb, i);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Append a 64 bit integer, in network byte order
 *
 * @param pb the buffer to append to
 * @param i the value to write
 * @return #PACK_SUCCESS, or #PACK_ENOMEM when the buffer could not grow
 *
 * @note A thin wrapper over ocs::tom::put_uint64().
 */
int packint64(sge_pack_buffer *pb, uint64_t i) {
   try {
      ocs::tom::put_uint64(pb, i);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Append a double, in a platform independent representation
 *
 * @param pb the buffer to append to
 * @param d the value to write
 * @return #PACK_SUCCESS, or #PACK_ENOMEM when the buffer could not grow
 *
 * @note A thin wrapper over ocs::tom::put_double().
 */
int packdouble(sge_pack_buffer *pb, double d) {
   try {
      ocs::tom::put_double(pb, d);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Append a string, including its terminating NUL
 *
 * @param pb the buffer to append to
 * @param str the string to write; nullptr is written as an empty string
 * @return #PACK_SUCCESS, or #PACK_ENOMEM when the buffer could not grow
 *
 * @note A thin wrapper over ocs::tom::put_str().
 */
int packstr(sge_pack_buffer *pb, const char *str) {
   try {
      ocs::tom::put_str(pb, str);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Append a raw block of bytes
 *
 * @param pb the buffer to append to
 * @param buf_ptr the bytes to write
 * @param buf_size how many bytes to write
 * @return #PACK_SUCCESS, or #PACK_ENOMEM when the buffer could not grow
 *
 * @note A thin wrapper over ocs::tom::put_buf().
 */
int packbuf(sge_pack_buffer *pb, const char *buf_ptr, uint32_t buf_size) {
   try {
      ocs::tom::put_buf(pb, buf_ptr, buf_size);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}


/* --------------------------------------------------------- */
/*                                                           */
/*            low level functions for unpacking              */
/*                                                           */
/* --------------------------------------------------------- */

/**
 * @brief Read a 32 bit integer written by #packint
 *
 * @param pb the buffer to read from
 * @param[out] ip receives the value
 * @return #PACK_SUCCESS, #PACK_FORMAT when the buffer is exhausted or malformed, or #PACK_ENOMEM
 *
 * @note A thin wrapper over ocs::tom::get_uint32().
 */
int unpackint(sge_pack_buffer *pb, uint32_t *ip) {
   try {
      *ip = ocs::tom::get_uint32(pb);
   } catch (const ocs::tom::PackException &e) {
      *ip = 0;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a 64 bit integer written by #packint64
 *
 * @param pb the buffer to read from
 * @param[out] ip receives the value
 * @return #PACK_SUCCESS, #PACK_FORMAT when the buffer is exhausted or malformed, or #PACK_ENOMEM
 *
 * @note A thin wrapper over ocs::tom::get_uint64().
 */
int unpackint64(sge_pack_buffer *pb, uint64_t *ip) {
   try {
      *ip = ocs::tom::get_uint64(pb);
   } catch (const ocs::tom::PackException &e) {
      *ip = 0;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a double written by #packdouble
 *
 * @param pb the buffer to read from
 * @param[out] dp receives the value
 * @return #PACK_SUCCESS, #PACK_FORMAT when the buffer is exhausted or malformed, or #PACK_ENOMEM
 *
 * @note A thin wrapper over ocs::tom::get_double().
 */
int unpackdouble(sge_pack_buffer *pb, double *dp) {
   try {
      *dp = ocs::tom::get_double(pb);
   } catch (const ocs::tom::PackException &e) {
      *dp = 0;
      return e.code();
   }
   return PACK_SUCCESS;
}

/** @brief Unpack a NUL-terminated string from the packbuffer.
 *
 * Validates that at least one byte remains and that a terminating NUL lies
 * within the remaining (untrusted, network-supplied) buffer *before* reading
 * or copying. This prevents the out-of-bounds reads that an unbounded
 * `cur_ptr[0]` / `strlen()` would perform at the buffer boundary (CWE-125,
 * CS-2342). An empty string (leading NUL) yields a NULL result, mirroring the
 * previous behaviour; all sibling unpackers bounds-check before reading.
 *
 * @param pb   packbuffer to read from; cur_ptr/bytes_used advanced on success
 * @param str  out: newly allocated copy of the string (caller frees), or NULL
 *             for an empty string
 * @return PACK_SUCCESS, PACK_FORMAT (truncated / no in-bounds terminator), or
 *         PACK_ENOMEM
 *
 * @note A thin wrapper over ocs::tom::get_str().
 */
int unpackstr(sge_pack_buffer *pb, char **str) {
   try {
      *str = ocs::tom::get_str(pb);
   } catch (const ocs::tom::PackException &e) {
      // *str stays as it was, as before
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a raw byte block written by #packbuf
 *
 * @param pb the buffer to read from
 * @param[out] buf_ptr receives the block, allocated by this function; the caller owns it
 * @param buf_size number of bytes to read
 * @return #PACK_SUCCESS, #PACK_FORMAT when the buffer is exhausted or malformed, or #PACK_ENOMEM
 *
 * @note A thin wrapper over ocs::tom::get_buf().
 */
int unpackbuf(sge_pack_buffer *pb, char **buf_ptr, int buf_size) {
   if (buf_size == 0) {
      return PACK_SUCCESS;
   }
   try {
      *buf_ptr = ocs::tom::get_buf(pb, static_cast<uint32_t>(buf_size));
   } catch (const ocs::tom::PackException &e) {
      // *buf_ptr stays as it was, as before
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief The message belonging to a pack error code
 *
 * @param errnum one of #PACK_SUCCESS and its neighbours
 * @return the message; static storage, do not free
 *
 * @note A thin wrapper over ocs::tom::pack_error_text().
 */
const char *cull_pack_strerror(int errnum) {
   return ocs::tom::pack_error_text(errnum);
}

/**
 * @brief Check if both buffers are equivalent
 *
 * Check if size and content of both packbuffers is equivalent
 *
 * @param pb1 packbuffer
 * @param pb2 packbuffer
 *
 * @return equivalent? true  - yes false - no
 *
 * @note A thin wrapper over ocs::tom::buffers_equal().
 */
bool
pb_are_equivalent(sge_pack_buffer *pb1, sge_pack_buffer *pb2) {
   return ocs::tom::buffers_equal(pb1, pb2);
}

/**
 * @brief Print content of packbuffer
 *
 * Print content of packbuffer into file
 *
 * @param pb packbuffer pointer
 * @param only_header show only summary information
 * @param file file stream (e.g. stderr)
 *
 */
void
pb_print_to(sge_pack_buffer *pb, bool only_header, FILE *file) {
   size_t i;

   fprintf(file, "head_ptr: %p\n", (void *)pb->head_ptr);
   fprintf(file, "cur_ptr: %p\n", (void *)pb->cur_ptr);
   fprintf(file, "mem_size: %d\n", (int) pb->mem_size);
   fprintf(file, "bytes_used: %d\n", (int) pb->bytes_used);
   fprintf(file, "buffer:\n");
   if (!only_header) {
      for (i = 0; i < pb->bytes_used; i++) {
         fprintf(file, "%3d ", pb->head_ptr[i]);
         if ((i + 1) % 15 == 0) {
            fprintf(file, "\n");
         }
      }
      fprintf(file, "\n");
   }
}

/**
 * @brief re-resolve and check user information
 *
 * The function can do multiple verifications on user data and optionally do re-resolving.
 * If local_uid_gid is true then we expect uid and gid of the request to match the uid and gid of the current
 * process. This is done to ensure daemon to daemon communication is coming from the right user.
 * If reresolve_user is true then user information will be re-resolved. If the re-resolved information differs from
 * the data in the request then it is corrected and an informational message is returned in the error dstring.
 * If reresolve_supp_grp is true the the supplementary groups will be re-resolved.
 * Errors happening during the operations described above will be reported in the error dstring
 * and false will be returned.
 *
 * @param pb pack buffer with usage information
 * @param error dstring for reporting errors
 * @param local_uid_gid only accept requests with the same uid and gid?
 * @param reresolve_user  re-resolve and correct the user and group name?
 * @param reresolve_supp_grp  re-resolve the supplementary group ids?
 * @return true if all was ok, possibly names were corrected, false on errors
 *
 * @note A thin wrapper over ocs::tom::check_sender().
 */
bool
cull_reresolve_check_user(sge_pack_buffer *pb, dstring *error, bool local_uid_gid, bool reresolve_user, bool reresolve_supp_grp) {
   return ocs::tom::check_sender(pb, error, local_uid_gid, reresolve_user, reresolve_supp_grp);
}
