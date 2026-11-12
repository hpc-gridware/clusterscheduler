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
 * @brief The pack buffer of the typed object model: PackBuffer
 *
 * An owning handle on a pack buffer, which frees what the buffer holds when it
 * goes out of scope, as std::fstream closes its file:
 *
 * @code
 * ocs::tom::PackBuffer out;
 * jobs.pack(out.raw());
 * ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(out.data(), out.size());
 * ocs::tom::ListPtr<ocs::tom::JB> copy = ocs::tom::ListPtr<ocs::tom::JB>::unpack(in.raw());
 * @endcode
 *
 * The handles pack into and unpack from the raw buffer, so that code that
 * still uses the old interface (GDI, communication) can hand its buffer in.
 * Every failure throws a PackException (ocs_PackCore.h).
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_PackCore.h"
#include "uti/sge_stdlib.h"

namespace ocs::tom {

   /**
    * @brief An owning handle on a pack buffer, move-only
    */
   class PackBuffer {
   public:
      /**
       * @brief A buffer to pack into, as init_packbuffer() prepares one
       * @param with_sender true to write the sender information of this process, as a buffer sent to another
       *        component needs it
       * @param initial_size the bytes to allocate first, 0 for the default chunk
       * @throw PackException with PACK_AUTHINFO if there is no sender information, PACK_ENOMEM
       */
      explicit PackBuffer(const bool with_sender = false, const std::size_t initial_size = 0) {
         init_buffer(&pb_, initial_size, false, with_sender);
      }

      /**
       * @brief A buffer that only counts the bytes a pack would write
       * @return the buffer
       */
      [[nodiscard]] static PackBuffer counting() {
         PackBuffer buffer(Empty{});
         init_buffer(&buffer.pb_, 0, true, false);
         return buffer;
      }

      /**
       * @brief A buffer to unpack from, holding a copy of bytes received, as init_packbuffer_from_buffer() prepares one
       * @param bytes the bytes, starting with the header a buffer to pack into wrote
       * @param size how many
       * @param with_sender true to check and read the sender information
       * @return the buffer
       * @throw PackException with PACK_FORMAT for a header cut off, PACK_VERSION for a wrong version,
       *        PACK_AUTHINFO, PACK_ENOMEM
       */
      [[nodiscard]] static PackBuffer reading(const char *bytes, const std::size_t size, const bool with_sender = false) {
         PackBuffer buffer(Empty{});
         char *copy = sge_malloc(size > 0 ? size : 1);
         if (copy == nullptr) {
            throw PackException(PACK_ENOMEM, "copying the bytes of a pack buffer");
         }
         if (size > 0) {
            memcpy(copy, bytes, size);
         }
         // the buffer owns the copy from here on, also when reading the header fails
         init_buffer_from(&buffer.pb_, copy, static_cast<std::uint32_t>(size), with_sender);
         return buffer;
      }

      PackBuffer(const PackBuffer &) = delete;
      PackBuffer &operator=(const PackBuffer &) = delete;

      /**
       * @brief Take over the buffer of another handle
       * @param other the handle, empty afterwards
       */
      PackBuffer(PackBuffer &&other) noexcept : pb_(other.pb_) {
         memset(&other.pb_, 0, sizeof(other.pb_));
      }

      /**
       * @brief Free the buffer held and take over the one of another handle
       * @param other the handle, empty afterwards
       * @return this handle
       */
      PackBuffer &operator=(PackBuffer &&other) noexcept {
         if (this != &other) {
            clear_buffer(&pb_);
            pb_ = other.pb_;
            memset(&other.pb_, 0, sizeof(other.pb_));
         }
         return *this;
      }

      /** @brief Free what the buffer holds, as clear_packbuffer() does */
      ~PackBuffer() {
         clear_buffer(&pb_);
      }

      /**
       * @brief The buffer, for the pack and unpack functions of the handles and of the old interface
       * @return the buffer
       */
      [[nodiscard]] sge_pack_buffer &raw() noexcept {
         return pb_;
      }

      /**
       * @brief The bytes written, or read so far, header included
       * @return the number of bytes
       */
      [[nodiscard]] std::size_t size() const noexcept {
         return pb_.bytes_used;
      }

      /**
       * @brief The bytes of the buffer; nullptr for a buffer that only counts
       * @return the first byte
       */
      [[nodiscard]] const char *data() const noexcept {
         return pb_.head_ptr;
      }

   private:
      /** @brief Tag of the constructor of an empty buffer */
      struct Empty {};

      /**
       * @brief An empty buffer, to be prepared by a factory
       */
      explicit PackBuffer(Empty) noexcept {
         memset(&pb_, 0, sizeof(pb_));
      }

      sge_pack_buffer pb_{};   ///< the buffer, owned
   };

   /**
    * @brief Whether the next list in a buffer is of the type @p T, without reading it
    *
    * For a receiver that does not know from the protocol what comes:
    * `if (next_list_is<JB>(pb)) { ListPtr<JB> jobs = ListPtr<JB>::unpack(pb); }`.
    *
    * @tparam T the struct of the object type
    * @param pb the buffer, positioned in front of a list
    * @return true if a list of @p T follows; false for another type and for a list that was null
    * @throw PackException with PACK_FORMAT for bytes that are cut off or a descriptor of no known type
    * @see Core: ocs::tom::peek_list_type()
    */
   template <ObjectType T>
   [[nodiscard]] bool next_list_is(const sge_pack_buffer &pb) {
      return peek_list_type(&pb) == T::fields[0].nm;
   }

   /**
    * @brief Whether the next element in a buffer is of the type @p T, without reading it
    *
    * An element that was bound at the sender does not name its type; this is
    * false for it.
    *
    * @tparam T the struct of the object type
    * @param pb the buffer, positioned in front of an element
    * @return true if a free element of @p T follows
    * @throw PackException with PACK_FORMAT for bytes that are cut off or a descriptor of no known type
    * @see Core: ocs::tom::peek_elem_type()
    */
   template <ObjectType T>
   [[nodiscard]] bool next_elem_is(const sge_pack_buffer &pb) {
      return peek_elem_type(&pb) == T::fields[0].nm;
   }

} // namespace ocs::tom
