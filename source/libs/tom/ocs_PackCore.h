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
 * @brief Packing and unpacking in the core of the typed object model
 *
 * The pack buffer and the reading and writing of values, elements, lists,
 * selections and conditions. The functions are untyped and throw on every
 * error; the typed handles call them, and the old functions of the list
 * library (packint(), unpackstr(), cull_pack_list(), cull_unpack_elem(), ...)
 * are thin wrappers that turn the exception back into the PACK_* code they
 * returned (N-L-1, N-L-2). The bytes on the wire are those of before; the
 * format is pinned by test_cull_pack_format.
 */

#include <cstddef>
#include <cstdint>
#include <string>

#include "tom/ocs_Types.h"
#include "tom/ocs_PackBuffer.h"
#include "uti/sge_dstring.h"
#include "uti/ocs_Exception.h"

namespace ocs::tom {

   /**
    * @brief A failure of packing or unpacking, with the PACK_* code the old functions returned for it
    */
   class PackException : public ocs::RuntimeException {
   public:
      /**
       * @brief Describe the failure
       * @param code the code: PACK_ENOMEM, PACK_FORMAT, PACK_BADARG, PACK_VERSION or PACK_AUTHINFO
       * @param message what failed
       */
      PackException(int code, const std::string &message);

      /** @brief Destroy the exception; defined out of line as the key function */
      ~PackException() override;

      /**
       * @brief The code the old function returned for this failure
       * @return one of PACK_ENOMEM, PACK_FORMAT, PACK_BADARG, PACK_VERSION, PACK_AUTHINFO
       */
      [[nodiscard]] int code() const noexcept {
         return code_;
      }

   private:
      int code_;   ///< the PACK_* code
   };

   /**
    * @brief The message of a PACK_* code, as cull_pack_strerror() gave it
    * @param code the code
    * @return the message, static; empty for an unknown code
    */
   const char *pack_error_text(int code) noexcept;

   /**
    * @brief Prepare a buffer for packing, as init_packbuffer() did
    *
    * A buffer that only counts writes nothing and measures the bytes a pack
    * would write. Any other buffer starts with the padding, the version and the
    * sender information.
    *
    * @param pb the buffer, overwritten
    * @param initial_size the bytes to allocate first, 0 for the default chunk
    * @param just_count true for a buffer that only counts
    * @param with_auth_info true to write the sender information of this process
    * @throw PackException with PACK_FORMAT if @p pb is null, PACK_AUTHINFO if there is no sender information,
    *        PACK_ENOMEM if there is no memory
    */
   void init_buffer(sge_pack_buffer *pb, std::size_t initial_size, bool just_count, bool with_auth_info);

   /**
    * @brief Prepare a buffer for unpacking bytes received, as init_packbuffer_from_buffer() did
    *
    * The buffer takes the bytes over. Its padding, version and sender
    * information are read and checked.
    *
    * @param pb the buffer, overwritten
    * @param buf the bytes, freed with the buffer
    * @param buflen how many there are
    * @param with_auth_info true to check and read the sender information
    * @throw PackException with PACK_FORMAT if @p pb is null or the header is cut off, PACK_VERSION for a wrong
    *        padding or version, PACK_AUTHINFO if the sender information cannot be read, PACK_ENOMEM
    */
   void init_buffer_from(sge_pack_buffer *pb, char *buf, std::uint32_t buflen, bool with_auth_info);

   /**
    * @brief Free what a buffer holds, as clear_packbuffer() did
    * @param pb the buffer, or nullptr
    */
   void clear_buffer(sge_pack_buffer *pb) noexcept;

   /**
    * @brief Write a 32 bit number, in network byte order
    * @param pb the buffer
    * @param value the number
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_ENOMEM if the buffer cannot grow
    */
   void put_uint32(sge_pack_buffer *pb, std::uint32_t value);

   /**
    * @brief Write a 32 bit number at the current position without growing the buffer, as repackint() did
    * @param pb the buffer, whose position was set back by the caller
    * @param value the number
    * @throw PackException with PACK_BADARG if @p pb is null
    */
   void reput_uint32(sge_pack_buffer *pb, std::uint32_t value);

   /**
    * @brief Write a 64 bit number, in network byte order
    * @param pb the buffer
    * @param value the number
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_ENOMEM if the buffer cannot grow
    */
   void put_uint64(sge_pack_buffer *pb, std::uint64_t value);

   /**
    * @brief Write a double, its bits in network byte order
    * @param pb the buffer
    * @param value the number
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_ENOMEM if the buffer cannot grow
    */
   void put_double(sge_pack_buffer *pb, double value);

   /**
    * @brief Write a string with its terminator; a null string is written as an empty one
    * @param pb the buffer
    * @param str the string, or nullptr
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_ENOMEM if the buffer cannot grow
    */
   void put_str(sge_pack_buffer *pb, const char *str);

   /**
    * @brief Write bytes as they are
    * @param pb the buffer
    * @param buf the bytes
    * @param size how many
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_ENOMEM if the buffer cannot grow
    */
   void put_buf(sge_pack_buffer *pb, const char *buf, std::uint32_t size);

   /**
    * @brief Read a 32 bit number
    * @param pb the buffer
    * @return the number
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_FORMAT if the buffer ends before it
    */
   std::uint32_t get_uint32(sge_pack_buffer *pb);

   /**
    * @brief Read a 64 bit number
    * @param pb the buffer
    * @return the number
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_FORMAT if the buffer ends before it
    */
   std::uint64_t get_uint64(sge_pack_buffer *pb);

   /**
    * @brief Read a double
    * @param pb the buffer
    * @return the number
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_FORMAT if the buffer ends before it
    */
   double get_double(sge_pack_buffer *pb);

   /**
    * @brief Read a string; an empty one is read as nullptr
    * @param pb the buffer
    * @return the string, owned by the caller and freed with sge_free(), or nullptr
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_FORMAT if the buffer ends before the
    *        terminator, PACK_ENOMEM
    */
   char *get_str(sge_pack_buffer *pb);

   /**
    * @brief Read bytes as they are
    * @param pb the buffer
    * @param size how many
    * @return the bytes, owned by the caller and freed with sge_free(), or nullptr for 0 bytes
    * @throw PackException with PACK_BADARG if @p pb is null, PACK_FORMAT if the buffer ends before them,
    *        PACK_ENOMEM
    */
   char *get_buf(sge_pack_buffer *pb, std::uint32_t size);

   /**
    * @brief Whether two buffers hold the same bytes, as pb_are_equivalent() decided it
    * @param a the first buffer, or nullptr
    * @param b the second buffer, or nullptr
    * @return true if either is null or both hold the same bytes
    */
   bool buffers_equal(const sge_pack_buffer *a, const sge_pack_buffer *b) noexcept;

   /**
    * @brief Check and complete the sender information of a buffer received, as cull_reresolve_check_user() did
    * @param pb the buffer
    * @param error receives the reason of a mismatch, or a note on a corrected name
    * @param local_uid_gid true if the sender must run with this process' user and group, or as root
    * @param reresolve_user true to resolve the user and group names again from the ids
    * @param reresolve_supp_grp true to resolve the supplementary groups again
    * @return true if the sender information holds
    */
   bool check_sender(sge_pack_buffer *pb, dstring *error, bool local_uid_gid, bool reresolve_user,
                     bool reresolve_supp_grp);

   /**
    * @brief The number of entries in the descriptor table of a buffer
    * @param pb the buffer, or nullptr
    * @return the number of distinct reduced descriptors written to or read from it
    */
   std::size_t descr_table_size(const sge_pack_buffer *pb) noexcept;

   /**
    * @brief Give up the descriptor table of a buffer
    * @param pb the buffer, or nullptr; its table is released and set to nullptr
    */
   void free_descr_table(sge_pack_buffer *pb) noexcept;

   /**
    * @brief Write an object type descriptor, as cull_pack_descr() did
    *
    * Without flags a static descriptor is written as a reference to its type
    * and a reduced one through the descriptor table of the buffer (N-W-3,
    * N-W-4); with flags it is written field by field (N-W-8).
    *
    * @param pb the buffer
    * @param descr the descriptor
    * @param flags the flags the values are packed with, 0 for all values
    * @throw PackException with PACK_BADARG for a null argument, PACK_ENOMEM
    */
   void pack_descr(sge_pack_buffer *pb, const lDescr *descr, int flags);

   /**
    * @brief Write a field selection as the descriptor it reduces another one to, as cull_pack_enum_as_descr() did
    * @param pb the buffer
    * @param what the selection
    * @param descr the descriptor it applies to
    * @param flags the flags the values are packed with, 0 for all values
    * @throw PackException with PACK_BADARG for a null argument or a selected field @p descr does not hold,
    *        PACK_ENOMEM
    */
   void pack_what_as_descr(sge_pack_buffer *pb, const lEnumeration *what, const lDescr *descr, int flags);

   /**
    * @brief Write an element, or the fields of it a selection names, as cull_pack_elem_partial() did
    * @param pb the buffer
    * @param ep the element
    * @param what the selection, or nullptr for every field
    * @param flags write only the fields whose flags match, 0 for all
    * @throw PackException with PACK_BADARG for a null argument, PACK_ENOMEM, PACK_FORMAT for a field of a
    *        type that cannot be packed
    */
   void pack_elem(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, int flags);

   /**
    * @brief Read an element, as cull_unpack_elem_partial() did
    *
    * A free element takes its descriptor from the buffer; an element that was
    * bound at the sender takes @p descr. Everything read before an error is
    * freed.
    *
    * @param pb the buffer
    * @param descr the descriptor of a bound element; for a free element of flag-filtered data the expected
    *        type, or nullptr
    * @param flags read only the fields whose flags match, 0 for all
    * @return the element, owned by the caller
    * @throw PackException with PACK_FORMAT for bytes that are cut off or no element, PACK_BADARG for a bound
    *        element without @p descr, PACK_ENOMEM
    */
   lListElem *unpack_elem(sge_pack_buffer *pb, const lDescr *descr, int flags);

   /**
    * @brief Write a list, or the fields of its elements a selection names, as cull_pack_list_partial() did
    * @param pb the buffer
    * @param lp the list, or nullptr
    * @param what the selection, or nullptr for every field
    * @param flags write only the fields whose flags match, 0 for all
    * @throw PackException with PACK_BADARG for a null buffer, PACK_ENOMEM, PACK_FORMAT
    */
   void pack_list(sge_pack_buffer *pb, const lList *lp, const lEnumeration *what, int flags);

   /**
    * @brief Read a list, as cull_unpack_list_partial() did
    *
    * The checks for duplicate keys are suspended while it is read (N-H-18).
    * Everything read before an error is freed.
    *
    * @param pb the buffer
    * @param flags read only the fields whose flags match, 0 for all
    * @return the list, owned by the caller, or nullptr for a list that was null
    * @throw PackException with PACK_FORMAT for bytes that are cut off or no list, PACK_ENOMEM
    */
   lList *unpack_list(sge_pack_buffer *pb, int flags);

   /**
    * @brief The type of the next list in a buffer, without reading it
    *
    * The position of the buffer and its descriptor table stay as they are;
    * every form of descriptor on the wire is understood (N-W-3 to N-W-6).
    *
    * @param pb the buffer, positioned in front of a list written by pack_list() or cull_pack_list()
    * @return the first field number of the type, as `T::fields[0].nm` of the typed object model, or NoName for a
    *         list that was null
    * @throw PackException with PACK_BADARG for a null buffer, PACK_FORMAT for bytes that are cut off or a
    *        descriptor of no known type
    */
   int peek_list_type(const sge_pack_buffer *pb);

   /**
    * @brief The type of the next element in a buffer, without reading it
    *
    * Only a free element carries its descriptor; one that was bound at the
    * sender is read with the descriptor of its list, which the buffer does not
    * name.
    *
    * @param pb the buffer, positioned in front of an element written by pack_elem() or cull_pack_elem()
    * @return the first field number of the type, or NoName for an element that was bound at the sender
    * @throw PackException with PACK_BADARG for a null buffer, PACK_FORMAT for bytes that are cut off, an element of
    *        no known kind or a descriptor of no known type
    */
   int peek_elem_type(const sge_pack_buffer *pb);

   /**
    * @brief Write a field selection, as cull_pack_enum() did
    * @param pb the buffer
    * @param what the selection, or nullptr
    * @throw PackException with PACK_BADARG for a null buffer, PACK_ENOMEM
    */
   void pack_what(sge_pack_buffer *pb, const lEnumeration *what);

   /**
    * @brief Read a field selection, as cull_unpack_enum() did
    * @param pb the buffer
    * @return the selection, owned by the caller, or nullptr for a selection that was null
    * @throw PackException with PACK_FORMAT for bytes that are cut off, PACK_ENOMEM
    */
   lEnumeration *unpack_what(sge_pack_buffer *pb);

   /**
    * @brief Write a condition, as cull_pack_cond() did
    * @param pb the buffer
    * @param cp the condition, or nullptr
    * @throw PackException with PACK_BADARG for a null buffer, PACK_ENOMEM, PACK_FORMAT for an unknown operator
    */
   void pack_where(sge_pack_buffer *pb, const lCondition *cp);

   /**
    * @brief Read a condition, as cull_unpack_cond() did
    * @param pb the buffer
    * @return the condition, owned by the caller, or nullptr for a condition that was null
    * @throw PackException with PACK_FORMAT for bytes that are cut off or an unknown operator, PACK_ENOMEM
    */
   lCondition *unpack_where(sge_pack_buffer *pb);

   /**
    * @brief Bytes as the text setByteArray() stores: two hex digits per byte, the low half first
    * @param bytes the bytes, not null
    * @param size how many
    * @return the text
    */
   std::string bytes_to_hex(const char *bytes, int size);

   /**
    * @brief The bytes of a text written by bytes_to_hex(), as getByteArray() read them
    * @param hex the text, not null
    * @param[out] bytes receives the bytes, allocated with sge_malloc() and owned by the caller, also on a bad digit
    * @return the number of bytes; for a bad digit the negative of its position, -1 counted from the second digit
    *         of a byte
    */
   int hex_to_bytes(const char *hex, char **bytes);

   /**
    * @name Non-throwing fronts
    * @brief The buffer functions for code that reacts to a PACK_* code instead of an exception
    *
    * Daemon code (execd, qmaster, GDI, spooling) checks or ignores the result
    * of a pack operation and must not see an exception: one escaping there
    * would end the daemon. These fronts call the functions above and turn a
    * PackException back into its code, so that the caller keeps its branch on
    * the code. They pack and unpack the same bytes.
    * @{
    */

   /**
    * @brief Prepare a buffer for packing, returning the code instead of throwing
    * @param pb the buffer, overwritten
    * @param initial_size the bytes to allocate first, 0 for the default chunk
    * @param just_count true for a buffer that only counts
    * @param with_auth_info true to write the sender information of this process
    * @return PACK_SUCCESS, or the code init_buffer() throws with
    * @see Old form: init_packbuffer()
    */
   int try_init_buffer(sge_pack_buffer *pb, std::size_t initial_size, bool just_count = false,
                       bool with_auth_info = true) noexcept;

   /**
    * @brief Prepare a buffer for unpacking bytes received, returning the code instead of throwing
    *
    * The buffer takes the bytes over also on an error (not when @p pb is null); clear_buffer() frees them.
    *
    * @param pb the buffer, overwritten
    * @param buf the bytes, freed with the buffer
    * @param buflen how many there are
    * @param with_auth_info true to check and read the sender information
    * @return PACK_SUCCESS, or the code init_buffer_from() throws with
    * @see Old form: init_packbuffer_from_buffer()
    */
   int try_init_buffer_from(sge_pack_buffer *pb, char *buf, std::uint32_t buflen, bool with_auth_info = true) noexcept;

   /**
    * @brief Write a 32 bit number, returning the code instead of throwing
    * @param pb the buffer
    * @param value the number
    * @return PACK_SUCCESS, PACK_BADARG or PACK_ENOMEM
    * @see Old form: packint()
    */
   int try_put_uint32(sge_pack_buffer *pb, std::uint32_t value) noexcept;

   /**
    * @brief Write a double, returning the code instead of throwing
    * @param pb the buffer
    * @param value the number
    * @return PACK_SUCCESS, PACK_BADARG or PACK_ENOMEM
    * @see Old form: packdouble()
    */
   int try_put_double(sge_pack_buffer *pb, double value) noexcept;

   /**
    * @brief Write a string, returning the code instead of throwing; a null string is written as an empty one
    * @param pb the buffer
    * @param str the string, or nullptr
    * @return PACK_SUCCESS, PACK_BADARG or PACK_ENOMEM
    * @see Old form: packstr()
    */
   int try_put_str(sge_pack_buffer *pb, const char *str) noexcept;

   /**
    * @brief Read a 32 bit number, returning the code instead of throwing
    * @param pb the buffer
    * @param[out] value receives the number, 0 on an error
    * @return PACK_SUCCESS, PACK_BADARG or PACK_FORMAT
    * @see Old form: unpackint()
    */
   int try_get_uint32(sge_pack_buffer *pb, std::uint32_t *value) noexcept;

   /**
    * @brief Read a double, returning the code instead of throwing
    * @param pb the buffer
    * @param[out] value receives the number, 0 on an error
    * @return PACK_SUCCESS, PACK_BADARG or PACK_FORMAT
    * @see Old form: unpackdouble()
    */
   int try_get_double(sge_pack_buffer *pb, double *value) noexcept;

   /**
    * @brief Read a string, returning the code instead of throwing; an empty one is read as nullptr
    * @param pb the buffer
    * @param[out] str receives the string, owned by the caller and freed with sge_free(); left as it was on an
    *        error
    * @return PACK_SUCCESS, PACK_BADARG, PACK_FORMAT or PACK_ENOMEM
    * @see Old form: unpackstr()
    */
   int try_get_str(sge_pack_buffer *pb, char **str) noexcept;

   /**
    * @brief The bytes written to a buffer, or read from it so far, header included
    * @param pb the buffer, not null
    * @return the number of bytes
    * @see Old form: pb_used()
    */
   [[nodiscard]] std::size_t buffer_used(const sge_pack_buffer *pb) noexcept;

   /**
    * @brief Whether a buffer holds more bytes than the padding and the version (2 * INTSIZE)
    *
    * The sender information follows them, at least as an empty string, so
    * this is true for every prepared buffer to pack into; false for a
    * counting buffer that nothing was counted into.
    *
    * @param pb the buffer, not null
    * @return true if more than 2 * INTSIZE bytes were written
    * @see Old form: pb_filled()
    */
   [[nodiscard]] bool buffer_filled(const sge_pack_buffer *pb) noexcept;

   /**
    * @brief Whether a buffer received holds bytes that were not read yet
    * @param pb the buffer, not null
    * @return true if fewer bytes were read than the buffer holds
    * @see Old form: pb_unused() > 0
    */
   [[nodiscard]] bool buffer_has_unread(const sge_pack_buffer *pb) noexcept;

   /** @} */

} // namespace ocs::tom
