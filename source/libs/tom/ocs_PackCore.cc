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

#include "tom/ocs_PackCore.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include <netinet/in.h>
#include <sys/types.h>

#if defined(SOLARIS) || defined(DARWIN)
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#endif

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"

#include "tom/cull_hash.h"
#include "tom/cull_lerrnoP.h"
#include "tom/cull_listP.h"
#include "tom/cull_multitypeP.h"
#include "tom/cull_stats.h"
#include "tom/cull_whatP.h"
#include "tom/cull_whereP.h"
#include "tom/msg_cull.h"
#include "tom/ocs_cull_descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_SelectCore.h"

namespace ocs::tom {

   PackException::PackException(const int code, const std::string &message)
      : ocs::RuntimeException(message), code_(code) {}

   PackException::~PackException() = default;

   const char *pack_error_text(const int code) noexcept {
      switch (code) {
         case PACK_SUCCESS:
            return MSG_CULL_PACK_SUCCESS;
         case PACK_ENOMEM:
            return MSG_CULL_PACK_ENOMEM;
         case PACK_FORMAT:
            return MSG_CULL_PACK_FORMAT;
         case PACK_BADARG:
            return MSG_CULL_PACK_BADARG;
         case PACK_VERSION:
            return MSG_CULL_PACK_VERSION;
         case PACK_AUTHINFO:
            return MSG_CULL_PACK_AUTHINFO;
         default:
            return "";
      }
   }

   namespace {

      /// size of a packed double, in bytes
      constexpr std::size_t DoubleSize = 8;

      /**
       * @brief Throw the exception of a failure
       * @param code the PACK_* code
       * @param what what failed
       */
      [[noreturn, gnu::cold]] void fail(const int code, const char *what) {
         throw PackException(code, std::string(what) + ": " + pack_error_text(code));
      }

      /**
       * @brief Check that there is a buffer
       * @param pb the buffer
       * @param what the operation, for the message
       */
      void need_buffer(const sge_pack_buffer *pb, const char *what) {
         if (pb == nullptr) [[unlikely]] {
            fail(PACK_BADARG, what);
         }
      }

      /**
       * @brief Make room for bytes to be written, growing the buffer in chunks
       *
       * Unlike before, a failed growth keeps the buffer as it was, so that
       * clearing it still frees what it held.
       *
       * @param pb the buffer, not one that only counts
       * @param n the bytes to be written
       * @throw PackException with PACK_ENOMEM if the buffer cannot grow
       */
      void make_room(sge_pack_buffer *pb, const std::size_t n) {
         if (pb->bytes_used + n <= pb->mem_size) {
            return;
         }
         std::size_t size = pb->mem_size;
         while (pb->bytes_used + n > size) {
            size += CHUNK;
         }
         auto *head = static_cast<char *>(sge_realloc(pb->head_ptr, size, 0));
         if (head == nullptr) {
            fail(PACK_ENOMEM, "growing a pack buffer");
         }
         pb->head_ptr = head;
         pb->mem_size = size;
         pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
      }

      /**
       * @brief Write bytes, or only count them
       * @param pb the buffer
       * @param bytes the bytes
       * @param n how many
       */
      void put_bytes(sge_pack_buffer *pb, const void *bytes, const std::size_t n) {
         if (!pb->just_count) {
            make_room(pb, n);
            memcpy(pb->cur_ptr, bytes, n);
            pb->cur_ptr = &(pb->cur_ptr[n]);
         }
         pb->bytes_used += n;
      }

      /**
       * @brief Check that bytes are left to be read
       * @param pb the buffer
       * @param n how many are to be read
       * @param what the operation, for the message
       */
      void need_bytes(const sge_pack_buffer *pb, const std::size_t n, const char *what) {
         if (pb->bytes_used + n > pb->mem_size) {
            fail(PACK_FORMAT, what);
         }
      }

      /**
       * @brief Read bytes
       * @param pb the buffer, with @p n bytes left
       * @param bytes receives them
       * @param n how many
       */
      void get_bytes(sge_pack_buffer *pb, void *bytes, const std::size_t n) {
         memcpy(bytes, pb->cur_ptr, n);
         pb->cur_ptr = &(pb->cur_ptr[n]);
         pb->bytes_used += n;
      }

   } // namespace

   void init_buffer(sge_pack_buffer *pb, std::size_t initial_size, const bool just_count,
                    const bool with_auth_info) {
      DENTER(PACK_LAYER);

      if (pb == nullptr) {
         ERROR(MSG_CULL_ERRORININITPACKBUFFER_S, MSG_CULL_PACK_FORMAT);
         fail(PACK_FORMAT, "initializing a pack buffer");
      }

      memset(pb, 0, sizeof(sge_pack_buffer));
      if (just_count) {
         pb->just_count = true;
         DRETURN_VOID;
      }

      if (initial_size == 0) {
         initial_size = CHUNK;
      } else {
         initial_size += 2 * INTSIZE;   // room for the padding and the version
      }

      // the sender information of this process, and room for it
      if (with_auth_info) {
         pb->auth_info = component_get_auth_info();
         if (pb->auth_info == nullptr) {
            // component_get_auth_info() has logged the error
            fail(PACK_AUTHINFO, "initializing a pack buffer");
         }
      }
      initial_size += pb->auth_info != nullptr ? strlen(pb->auth_info) + 1 : 1;

      pb->head_ptr = sge_malloc(initial_size);
      if (pb->head_ptr == nullptr) {
         ERROR(MSG_CULL_NOTENOUGHMEMORY_D, static_cast<int>(initial_size));
         sge_free(&(pb->auth_info));
         fail(PACK_ENOMEM, "initializing a pack buffer");
      }
      pb->cur_ptr = pb->head_ptr;
      pb->mem_size = initial_size;
      pb->bytes_used = 0;
      pb->just_count = false;
      pb->version = CULL_VERSION;
      put_uint32(pb, 0);              // a zero padding: the error handling of former versions
      put_uint32(pb, pb->version);    // the version of the format
      put_str(pb, pb->auth_info);

      DRETURN_VOID;
   }

   void init_buffer_from(sge_pack_buffer *pb, char *buf, const std::uint32_t buflen, const bool with_auth_info) {
      DENTER(PACK_LAYER);

      if (pb == nullptr) {
         fail(PACK_FORMAT, "reading a pack buffer");
      }

      memset(pb, 0, sizeof(sge_pack_buffer));
      pb->head_ptr = buf;
      pb->cur_ptr = buf;
      pb->mem_size = buflen;
      pb->bytes_used = 0;

      if (buflen == 0) {
         pb->version = CULL_VERSION;
         DRETURN_VOID;
      }

      // the padding, the version and the sender information
      const std::uint32_t pad = get_uint32(pb);
      const std::uint32_t version = get_uint32(pb);
      pb->auth_info = get_str(pb);
      if (pad != 0 || version != CULL_VERSION) {
         ERROR(MSG_CULL_PACK_WRONG_VERSION_XX, static_cast<unsigned int>(version), CULL_VERSION);
         fail(PACK_VERSION, "reading a pack buffer");
      }
      pb->version = static_cast<int>(version);

      if (with_auth_info) {
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (!component_parse_auth_info(&error_dstr, pb->auth_info, &pb->uid, pb->username, MAX_USER_GROUP,
                                        &pb->gid, pb->groupname, MAX_USER_GROUP, &pb->grp_amount,
                                        &pb->grp_array)) {
            ERROR(SFNMAX, sge_dstring_get_string(&error_dstr));
            fail(PACK_AUTHINFO, "reading a pack buffer");
         }
      }

      DRETURN_VOID;
   }

   void clear_buffer(sge_pack_buffer *pb) noexcept {
      if (pb != nullptr) {
         sge_free(&pb->head_ptr);
         sge_free(&pb->auth_info);
         sge_free(&pb->grp_array);
         free_descr_table(pb);
      }
   }

   void put_uint32(sge_pack_buffer *pb, const std::uint32_t value) {
      need_buffer(pb, "packing a number");
      const std::uint32_t net = htonl(value);
      put_bytes(pb, &net, INTSIZE);
   }

   void reput_uint32(sge_pack_buffer *pb, const std::uint32_t value) {
      need_buffer(pb, "packing a number again");
      if (!pb->just_count) {
         const std::uint32_t net = htonl(value);
         memcpy(pb->cur_ptr, &net, INTSIZE);
         pb->cur_ptr = &(pb->cur_ptr[INTSIZE]);
      }
   }

   void put_uint64(sge_pack_buffer *pb, const std::uint64_t value) {
      need_buffer(pb, "packing a number");
      const std::uint64_t net = htobe64(value);
      put_bytes(pb, &net, 2 * INTSIZE);
   }

   void put_double(sge_pack_buffer *pb, const double value) {
      need_buffer(pb, "packing a double");
      std::uint64_t bits;
      memcpy(&bits, &value, DoubleSize);
      const std::uint64_t net = htobe64(bits);
      put_bytes(pb, &net, DoubleSize);
   }

   void put_str(sge_pack_buffer *pb, const char *str) {
      need_buffer(pb, "packing a string");
      if (str == nullptr) {
         const char terminator = '\0';
         put_bytes(pb, &terminator, 1);
      } else {
         put_bytes(pb, str, strlen(str) + 1);
      }
   }

   void put_buf(sge_pack_buffer *pb, const char *buf, const std::uint32_t size) {
      need_buffer(pb, "packing bytes");
      put_bytes(pb, buf, size);
   }

   std::uint32_t get_uint32(sge_pack_buffer *pb) {
      need_buffer(pb, "unpacking a number");
      need_bytes(pb, INTSIZE, "unpacking a number");
      std::uint32_t net;
      get_bytes(pb, &net, INTSIZE);
      return ntohl(net);
   }

   std::uint64_t get_uint64(sge_pack_buffer *pb) {
      need_buffer(pb, "unpacking a number");
      need_bytes(pb, 2 * INTSIZE, "unpacking a number");
      std::uint64_t net;
      get_bytes(pb, &net, 2 * INTSIZE);
      return be64toh(net);
   }

   double get_double(sge_pack_buffer *pb) {
      need_buffer(pb, "unpacking a double");
      need_bytes(pb, DoubleSize, "unpacking a double");
      std::uint64_t net;
      get_bytes(pb, &net, DoubleSize);
      const std::uint64_t bits = be64toh(net);
      double value;
      memcpy(&value, &bits, DoubleSize);
      return value;
   }

   char *get_str(sge_pack_buffer *pb) {
      need_buffer(pb, "unpacking a string");
      // at least the terminator must be left
      const std::size_t remaining = pb->mem_size - pb->bytes_used;
      if (pb->bytes_used >= pb->mem_size) {
         fail(PACK_FORMAT, "unpacking a string");
      }
      if (pb->cur_ptr[0] == '\0') {
         pb->cur_ptr = &(pb->cur_ptr[1]);
         pb->bytes_used++;
         return nullptr;
      }
      // the terminator within the bytes left, never beyond them
      const void *nul = memchr(pb->cur_ptr, '\0', remaining);
      if (nul == nullptr) {
         fail(PACK_FORMAT, "unpacking a string");
      }
      const std::size_t n = static_cast<std::size_t>(static_cast<const char *>(nul) - pb->cur_ptr) + 1;
      char *str = strdup(pb->cur_ptr);
      if (str == nullptr) {
         fail(PACK_ENOMEM, "unpacking a string");
      }
      pb->bytes_used += n;
      pb->cur_ptr = &(pb->cur_ptr[n]);
      return str;
   }

   char *get_buf(sge_pack_buffer *pb, const std::uint32_t size) {
      need_buffer(pb, "unpacking bytes");
      if (size == 0) {
         return nullptr;
      }
      need_bytes(pb, size, "unpacking bytes");
      char *buf = sge_malloc(size);
      if (buf == nullptr) {
         fail(PACK_ENOMEM, "unpacking bytes");
      }
      get_bytes(pb, buf, size);
      return buf;
   }

   bool buffers_equal(const sge_pack_buffer *a, const sge_pack_buffer *b) noexcept {
      if (a == nullptr || b == nullptr) {
         return true;
      }
      return a->bytes_used == b->bytes_used && memcmp(a->head_ptr, b->head_ptr, a->bytes_used) == 0;
   }

   bool check_sender(sge_pack_buffer *pb, dstring *error, const bool local_uid_gid, const bool reresolve_user,
                     const bool reresolve_supp_grp) {
      DENTER(PACK_LAYER);
      bool ret = true;

      // the sender must run as this process' user and group, or as root, between components such as
      // sge_qmaster and sge_execd
      if (local_uid_gid) {
         if ((pb->uid != component_get_uid() && pb->uid != 0) || (pb->gid != component_get_gid() && pb->gid != 0)) {
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_UIDGIDMISMATCH_UUUU, pb->uid, pb->gid, component_get_uid(),
                                component_get_gid());
            ret = false;
         }
      }

      if (ret && reresolve_user) {
         char buffer[MAX_USER_GROUP]{};
         if (sge_uid2user(pb->uid, buffer, sizeof(buffer), MAX_NIS_RETRIES) != 0) {
            // sge_uid2user() logs the error
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_COULDNOTRESOLVEUSER_U, pb->uid);
            ret = false;
         } else if (strcmp(pb->username, buffer) != 0) {
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_CORRECTINGUSERNAME_SS, pb->username, buffer);
            sge_strlcpy(pb->username, buffer, MAX_USER_GROUP);
         }
         if (sge_gid2group(pb->gid, buffer, sizeof(buffer), MAX_NIS_RETRIES) != 0) {
            // sge_gid2group() logs the error
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_COULDNOTRESOLVEGROUP_U, pb->gid);
            ret = false;
         } else if (strcmp(pb->groupname, buffer) != 0) {
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_CORRECTINGGROUPNAME_SS, pb->groupname, buffer);
            sge_strlcpy(pb->groupname, buffer, MAX_USER_GROUP);
         }
      }

      if (ret && reresolve_supp_grp) {
         int amount;
         ocs_grp_elem_t *grp_array;
         if (ocs_get_groups(pb->username, pb->gid, &amount, &grp_array, error)) {
            sge_free(&pb->grp_array);
            pb->grp_amount = amount;
            pb->grp_array = grp_array;
         } else {
            ret = false;
         }
      }

      DRETURN(ret);
   }

   // ================================================================ objects

} // namespace ocs::tom

/**
 * @brief The descriptor table of a pack buffer (N-W-4)
 *
 * Both sides number the descriptors of a buffer in the order they are written
 * and read. The sender looks a shared descriptor up by its address, which is
 * stable because the table holds a reference on it; the receiver finds an
 * entry by its index. Each entry holds one reference, given up when the buffer
 * is cleared.
 */
struct ocs_cull_descr_table {
   std::vector<const lDescr *> entries; ///< the shared descriptors, by table index
};

namespace ocs::tom {

   namespace {

      /// how a field selection is encoded
      enum : std::uint32_t {
         PackWhatAll = 0,   ///< every field: the selection was ALL, no array follows
         PackWhatNone,      ///< no field at all, no array follows
         PackWhatArray      ///< an explicit list of fields follows
      };

      /**
       * @brief Time a pack or unpack operation for the profiling of SGE_PROF_PACKING, also when it throws
       */
      class PackingProfile {
      public:
         /** @brief Start the measurement */
         PackingProfile() {
            PROF_START_MEASUREMENT(SGE_PROF_PACKING);
         }

         /** @brief Stop the measurement */
         ~PackingProfile() {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         }

         PackingProfile(const PackingProfile &) = delete;
         PackingProfile &operator=(const PackingProfile &) = delete;
      };

      /**
       * @brief Fail with a format error, setting the error number as the old code did
       * @param lerrno the error number
       * @param what what failed
       */
      [[noreturn, gnu::cold]] void format_error(const int lerrno, const char *what) {
         LERROR(lerrno);
         fail(PACK_FORMAT, what);
      }

      /**
       * @brief Fail for lack of memory, setting the error number as the old code did
       * @param what what failed
       */
      [[noreturn, gnu::cold]] void no_memory(const char *what) {
         LERROR(LEMALLOC);
         fail(PACK_ENOMEM, what);
      }

#ifdef OCS_CULL_STATS
      /**
       * @brief Count a packed descriptor or field selection (CS-2993); nothing in counting mode
       * @param pb the buffer it was packed into
       * @param nm the first field of the descriptor it describes
       * @param start `bytes_used` before packing it
       */
      void count_descr(const sge_pack_buffer *pb, const int nm, const std::size_t start) {
         if (!pb->just_count) {
            CULL_STATS_TYPE(nm, PACK_DESCR, 1);
            CULL_STATS_TYPE(nm, PACK_DESCR_B, pb->bytes_used - start);
         }
      }

      /**
       * @brief Count how a shared descriptor was packed (CS-2997); nothing in counting mode
       * @param pb the buffer it was packed into
       * @param nm the first field of the descriptor
       * @param c DESCR_PACK_TYPE_REF, DESCR_PACK_TABLE_DEF or DESCR_PACK_TABLE_REF
       */
      void count_shared(const sge_pack_buffer *pb, const int nm, const ocs::cull_stats::Counter c) {
         if (!pb->just_count) {
            ocs::cull_stats::add_type(nm, c, 1);
         }
      }

      /**
       * @brief Count the bytes of a packed field value (CS-2993); sub-lists and sub-objects count their own fields
       * @param pb the buffer it was packed into
       * @param nm the field
       * @param type the field's type
       * @param start `bytes_used` before packing it
       */
      void count_value(const sge_pack_buffer *pb, const int nm, const int type, const std::size_t start) {
         if (!pb->just_count && type != lListT && type != lObjectT) {
            CULL_STATS_FIELD(nm, PACK_VALUE_B, pb->bytes_used - start);
         }
      }
#endif

      void pack_value(sge_pack_buffer *pb, const lMultiType *src, const lEnumeration *what, int type, int flags);
      void unpack_value(sge_pack_buffer *pb, lMultiType *dst, int type, int flags);
      lListElem *unpack_elem_n(sge_pack_buffer *pb, const lDescr *descr, std::uint32_t wire_n, int flags);
      void pack_object(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, int flags);
      lListElem *unpack_object(sge_pack_buffer *pb, int flags);

      /**
       * @brief Free the values a partly unpacked value array holds, and the array
       * @param cp the values, or nullptr
       * @param descr the descriptor they belong to
       */
      void free_values(lMultiType *cp, const lDescr *descr) noexcept {
         if (cp == nullptr) {
            return;
         }
         for (const auto f : ocs_cull_present_fields(descr)) {
            lMultiType &value = cp[f.slot];
            switch (mt_get_type(descr[f.pos].mt)) {
               case lStringT:
                  sge_free(&(value.str));
                  break;
               case lHostT:
                  sge_free(&(value.host));
                  break;
               case lListT:
                  free_list(value.glp);
                  break;
               case lObjectT:
                  if (value.obj != nullptr) {
                     free_elem(value.obj);
                  }
                  break;
               default:
                  break;
            }
         }
         sge_free(&cp);
      }

      /**
       * @brief Free an element that was not completely unpacked: its values, its descriptor if it holds one, itself
       * @param ep the element, or nullptr
       */
      void free_partial_elem(lListElem *ep) noexcept {
         if (ep == nullptr) {
            return;
         }
         if (ep->descr != nullptr) {
            free_values(ep->cont, ep->descr);
            if (elem_kind(ep) == FREE_ELEM || elem_kind(ep) == OBJECT_ELEM) {
               ocs_cull_descr_release(&(ep->descr));
            }
         }
         sge_free(&ep);
      }

      /** @brief Frees an element that was not completely unpacked */
      struct PartialElemDeleter {
         /**
          * @brief Free it
          * @param ep the element
          */
         void operator()(lListElem *ep) const noexcept {
            free_partial_elem(ep);
         }
      };

      /** @brief Frees a list that was not completely unpacked */
      struct ListDeleter {
         /**
          * @brief Free it
          * @param lp the list
          */
         void operator()(lList *lp) const noexcept {
            free_list(lp);
         }
      };

      /** @brief Frees a selection that was not completely unpacked */
      struct WhatDeleter {
         /**
          * @brief Free it
          * @param ep the selection
          */
         void operator()(lEnumeration *ep) const noexcept {
            free_what(ep);
         }
      };

      /** @brief Frees a condition that was not completely unpacked */
      struct WhereDeleter {
         /**
          * @brief Free it
          * @param cp the condition
          */
         void operator()(lCondition *cp) const noexcept {
            free_where(cp);
         }
      };

      /** @brief Gives up a reference on a shared descriptor */
      struct DescrReleaser {
         /**
          * @brief Give it up
          * @param dp the descriptor
          */
         void operator()(const lDescr *dp) const noexcept {
            ocs_cull_descr_release(&dp);
         }
      };

      using PartialElem = std::unique_ptr<lListElem, PartialElemDeleter>;   ///< an element being unpacked
      using OwnedList = std::unique_ptr<lList, ListDeleter>;                ///< a list being unpacked
      using OwnedWhat = std::unique_ptr<lEnumeration, WhatDeleter>;         ///< a selection being unpacked
      using OwnedWhere = std::unique_ptr<lCondition, WhereDeleter>;         ///< a condition being unpacked
      using OwnedDescr = std::unique_ptr<const lDescr, DescrReleaser>;      ///< a reference on a descriptor

      /**
       * @brief The descriptor table of a buffer, created on first use
       * @param pb the buffer
       * @return its table
       * @throw PackException with PACK_ENOMEM
       */
      ocs_cull_descr_table *pack_table(sge_pack_buffer *pb);

      /**
       * @brief Write a value
       * @param pb the buffer
       * @param src the value
       * @param what the selection for a sub-list or sub-object, or nullptr
       * @param type the field's type
       * @param flags the flags for sub-lists and sub-objects
       * @throw PackException with PACK_FORMAT for a type that cannot be packed, PACK_ENOMEM
       */
      void pack_value(sge_pack_buffer *pb, const lMultiType *src, const lEnumeration *what, const int type,
                      const int flags) {
         switch (type) {
            case lBoolT:
               put_uint32(pb, src->b);
               break;
            case lUlongT:
               put_uint32(pb, src->ul);
               break;
            case lUlong64T:
               put_uint64(pb, src->ul64);
               break;
            case lStringT:
               put_str(pb, src->str);
               break;
            case lHostT:
               put_str(pb, src->host);
               break;
            case lListT:
               pack_list(pb, src->glp, what, flags);
               break;
            case lObjectT:
               pack_object(pb, src->obj, what, flags);
               break;
            case lDoubleT:
               put_double(pb, src->db);
               break;
            case lRefT:
               break;
            default:   // lIntT and lLongT are not packed, other types do not exist
               fail(PACK_FORMAT, "packing a value of a type that is not packed");
         }
      }

      /**
       * @brief Read a value
       * @param pb the buffer
       * @param dst receives the value; what it owns stays with it also when a later value fails
       * @param type the field's type
       * @param flags the flags for sub-lists and sub-objects
       * @throw PackException with PACK_FORMAT for bytes that are cut off or a type that is not packed, PACK_ENOMEM
       */
      void unpack_value(sge_pack_buffer *pb, lMultiType *dst, const int type, const int flags) {
         switch (type) {
            case lBoolT:
               dst->b = get_uint32(pb);
               break;
            case lUlongT:
               dst->ul = get_uint32(pb);
               break;
            case lUlong64T:
               dst->ul64 = get_uint64(pb);
               break;
            case lStringT:
               dst->str = get_str(pb);
               break;
            case lHostT:
               dst->host = get_str(pb);
               break;
            case lListT:
               dst->glp = unpack_list(pb, flags);
               break;
            case lObjectT:
               dst->obj = unpack_object(pb, flags);
               break;
            case lDoubleT:
               dst->db = get_double(pb);
               break;
            case lRefT:
               dst->ref = nullptr;
               break;
            default:   // lIntT and lLongT are not packed, other types do not exist
               fail(PACK_FORMAT, "unpacking a value of a type that is not packed");
         }
      }

      ocs_cull_descr_table *pack_table(sge_pack_buffer *pb) {
         if (pb->dtab == nullptr) {
            pb->dtab = new (std::nothrow) ocs_cull_descr_table;
            if (pb->dtab == nullptr) {
               fail(PACK_ENOMEM, "creating the descriptor table of a pack buffer");
            }
         }
         return pb->dtab;
      }

      /**
       * @brief The static type a descriptor packed field by field is a version of
       *
       * A descriptor that is not reduced and lists the leading fields of a
       * static type was packed by a release whose type had these fields only:
       * types grow at the end within a minor release (N-W-6). Listing all fields
       * of the type and more is a format error, because it comes from a newer
       * release.
       *
       * @param dp the descriptor as read, not reduced
       * @param n the number of its fields
       * @return the static type, or nullptr if @p dp does not list leading fields of one
       * @throw PackException with PACK_FORMAT if @p dp is longer than its type
       */
      const lDescr *prefix_type(const lDescr *dp, const std::uint32_t n) {
         if (n == 0) {
            return nullptr;
         }
         int pos = 0;
         const lDescr *type = ocs_cull_descr_static_containing(dp[0].nm, &pos);
         if (type == nullptr || pos != 0) {
            return nullptr;
         }
         const auto type_n = static_cast<std::uint32_t>(descr_length(type));
         for (std::uint32_t i = 0; i < n && i < type_n; i++) {
            if (dp[i].nm != type[i].nm || mt_get_type(dp[i].mt) != mt_get_type(type[i].mt)) {
               return nullptr;
            }
         }
         if (n > type_n) {
            format_error(LEDESCRNULL, "unpacking a descriptor longer than its type");
         }
         return type;
      }

      /**
       * @brief Read an object type descriptor into the descriptor a list or an element holds
       *
       * A type reference yields the static block of the type (N-W-3); a table
       * reference the entry of the descriptor table (N-W-4); a table definition
       * by index or by field a new entry (N-W-5); a descriptor field by field
       * outside the table, of flag-filtered data, the static type it is a
       * version of, or the shared descriptor equal to it (N-W-6, N-B-1). The
       * fields are listed in type order (N-B-8).
       *
       * @param pb the buffer
       * @param flags the flags the values were packed with; flag-filtered data never carries a type reference
       * @param[out] wire_n receives the number of fields whose values follow per element
       * @param expected the static type a free element of flag-filtered data must have, or nullptr
       * @return the descriptor, holding a reference for the caller
       * @throw PackException with PACK_FORMAT for a malformed descriptor, PACK_ENOMEM
       */
      const lDescr *unpack_descr(sge_pack_buffer *pb, const int flags, std::uint32_t *wire_n,
                                 const lDescr *expected) {
         *wire_n = 0;

         // the number of fields, or the marker of a reference
         std::uint32_t n = get_uint32(pb);

         if (n == CULL_PACK_DESCR_TYPE_REF) {
            if (flags != 0) {
               format_error(LEDESCRNULL, "a type reference in flag-filtered data");
            }
            const std::uint32_t first_nm = get_uint32(pb);
            const std::uint32_t count = get_uint32(pb);
            // fewer fields than the local type: the sender's type had only these (N-W-6)
            const lDescr *block = ocs_cull_descr_static_for(static_cast<int>(first_nm));
            if (block == nullptr || count == 0 || static_cast<int>(count) > descr_length(block)) {
               format_error(LEDESCRNULL, "a type reference to an unknown or shorter type");
            }
            *wire_n = count;
            return block;
         }

         // an index into the table of the buffer, or a new entry for it
         bool enter_table = false;
         if (n == CULL_PACK_DESCR_TABLE_REF || n == CULL_PACK_DESCR_TABLE_DEF) {
            if (flags != 0) {
               format_error(LEDESCRNULL, "a descriptor table entry in flag-filtered data");
            }
            if (n == CULL_PACK_DESCR_TABLE_REF) {
               const std::uint32_t index = get_uint32(pb);
               if (pb->dtab == nullptr || index >= pb->dtab->entries.size()) {
                  format_error(LEDESCRNULL, "a reference to a descriptor table entry that does not exist");
               }
               const lDescr *dp = ocs_cull_descr_ref(pb->dtab->entries[index]);
               *wire_n = static_cast<std::uint32_t>(lCountPresent(dp));
               return dp;
            }

            const std::uint32_t kind = get_uint32(pb);
            if (kind == CULL_PACK_DESCR_BY_INDEX) {
               // the fields by their index in one type, in type order; mt is taken from the local type (N-W-5, N-B-8)
               const std::uint32_t type_nm = get_uint32(pb);
               const std::uint32_t count = get_uint32(pb);
               const lDescr *type = ocs_cull_descr_static_for(static_cast<int>(type_nm));
               const int type_n = type != nullptr ? descr_length(type) : 0;
               if (type == nullptr || count > static_cast<std::uint32_t>(type_n)) {
                  format_error(LEDESCRNULL, "a descriptor of an unknown type or with too many fields");
               }
               std::vector<bool> keep(type_n, false);
               std::int64_t last = -1;
               for (std::uint32_t i = 0; i < count; i++) {
                  const std::uint32_t index = get_uint32(pb);
                  if (index >= static_cast<std::uint32_t>(type_n) || static_cast<std::int64_t>(index) <= last) {
                     format_error(LEDESCRNULL, "a descriptor whose fields are not in type order");
                  }
                  last = index;
                  keep[index] = true;
               }
               if (count == 0) {
                  format_error(LEDESCRNULL, "a descriptor without fields");
               }
               CULL_STATS_TYPE(type[0].nm, DCOPY_UNPACK, 1);
               CULL_STATS_TYPE(type[0].nm, DCOPY_UNPACK_B, sizeof(lDescr) * (count + 1));
               OwnedDescr shared(ocs_cull_descr_build(type, keep));
               if (shared == nullptr) {
                  no_memory("building a descriptor");
               }
               ocs_cull_descr_table *tab = pack_table(pb);
               tab->entries.push_back(shared.get());
               *wire_n = count;
               // the table keeps the reference of the build, the caller gets one of its own
               return ocs_cull_descr_ref(shared.release());
            }
            if (kind != CULL_PACK_DESCR_BY_FIELD) {
               format_error(LEDESCRNULL, "a descriptor table entry of an unknown kind");
            }
            // field by field, as without the table; the result enters the table
            n = get_uint32(pb);
            enter_table = true;
         }

         // n + 1 does not work because this casts n to int
         if (n > MAX_DESCR_SIZE - 1) {
            no_memory("unpacking a descriptor with too many fields");
         }

         std::vector<lDescr> dp(n + 1);
         dp[n].nm = NoName;
         dp[n].mt = lEndT;

         // n fields, in type order (N-B-8)
         bool wire_reduced = false;
         for (std::uint32_t i = 0; i < n; i++) {
            dp[i].nm = static_cast<int>(get_uint32(pb));
            if (i > 0 && dp[i].nm <= dp[i - 1].nm) {
               format_error(LEDESCRNULL, "a descriptor whose fields are not in type order");
            }
            // a sender never packs the process local reference count flag; clear it anyway
            dp[i].mt = static_cast<int>(get_uint32(pb)) & ~CULL_DESCR_INTERNED;
            wire_reduced = wire_reduced || (dp[i].mt & CULL_IS_REDUCED) != 0;
         }
         *wire_n = n;

         // a full type packed field by field is the local static type, also when the sender's type was shorter (N-W-6)
         if (!enter_table && !wire_reduced) {
            const lDescr *type = prefix_type(dp.data(), n);
            if (type != expected && expected != nullptr && ocs_cull_descr_is_static(expected)) {
               format_error(LEDESCRNULL, "a descriptor of another type than expected");
            }
            if (type != nullptr) {
               return type;
            }
         }
         CULL_STATS_TYPE(dp[0].nm, DCOPY_UNPACK, 1);
         CULL_STATS_TYPE(dp[0].nm, DCOPY_UNPACK_B, sizeof(lDescr) * (n + 1));

         // the fields on the wire are held by a full-length descriptor of their type, with mt from the local type
         // as for a table entry by index (N-B-1, N-B-8); fields of no known type, or of several, are a format error
         int first = 0;
         const lDescr *type = n > 0 ? ocs_cull_descr_static_containing(dp[0].nm, &first) : nullptr;
         const int type_n = type != nullptr ? descr_length(type) : 0;
         std::vector<bool> keep(type_n, false);
         bool known = type != nullptr;
         for (std::uint32_t i = 0; known && i < n; i++) {
            const int pos = dp[i].nm - type[0].nm;
            known = pos >= 0 && pos < type_n && type[pos].nm == dp[i].nm &&
                    mt_get_type(type[pos].mt) == mt_get_type(dp[i].mt);
            if (known) {
               keep[pos] = true;
            }
         }
         if (!known) {
            format_error(LEDESCRNULL, "a descriptor with fields of no known type");
         }
         OwnedDescr built(ocs_cull_descr_build(type, keep));
         if (built == nullptr) {
            no_memory("building a descriptor");
         }
         if (enter_table) {
            ocs_cull_descr_table *tab = pack_table(pb);
            tab->entries.push_back(ocs_cull_descr_ref(built.get()));
         }
         return built.release();
      }

      /**
       * @brief Write a shared descriptor through the type reference or the descriptor table
       *
       * A static block is a type reference (N-W-3). A descriptor already in the
       * table of the buffer is its index. Any other one enters the table: by the
       * indices of its fields in their type when all fields belong to one type,
       * so that the receiver takes `mt` from its own type (N-W-5), or else field
       * by field.
       *
       * @param pb the buffer
       * @param dp a static or counted descriptor
       * @throw PackException with PACK_ENOMEM
       */
      void pack_shared_descr(sge_pack_buffer *pb, const lDescr *dp) {
         const int n = lCountPresent(dp);

         if (!ocs_cull_descr_is_counted(dp)) {
            put_uint32(pb, CULL_PACK_DESCR_TYPE_REF);
            put_uint32(pb, dp[0].nm);
            put_uint32(pb, n);
            CULL_STATS_CODE(count_shared(pb, dp[0].nm, ocs::cull_stats::DESCR_PACK_TYPE_REF);)
            return;
         }

         ocs_cull_descr_table *tab = pack_table(pb);
         for (std::size_t k = 0; k < tab->entries.size(); k++) {
            if (tab->entries[k] == dp) {
               put_uint32(pb, CULL_PACK_DESCR_TABLE_REF);
               put_uint32(pb, static_cast<std::uint32_t>(k));
               CULL_STATS_CODE(count_shared(pb, dp[0].nm, ocs::cull_stats::DESCR_PACK_TABLE_REF);)
               return;
            }
         }
         tab->entries.push_back(ocs_cull_descr_ref(dp));
         CULL_STATS_CODE(count_shared(pb, dp[0].nm, ocs::cull_stats::DESCR_PACK_TABLE_DEF);)

         // by index if every field belongs to the type of the first one, with the same data type
         int pos = 0;
         const lDescr *type = n > 0 ? ocs_cull_descr_static_containing(dp[0].nm, &pos) : nullptr;
         std::vector<std::uint32_t> idx;
         for (const auto f : ocs_cull_present_fields(dp)) {
            if (type == nullptr) {
               break;
            }
            int p = 0;
            if (ocs_cull_descr_static_containing(dp[f.pos].nm, &p) != type ||
                mt_get_type(type[p].mt) != mt_get_type(dp[f.pos].mt)) {
               type = nullptr;
               break;
            }
            idx.push_back(static_cast<std::uint32_t>(p));
         }

         put_uint32(pb, CULL_PACK_DESCR_TABLE_DEF);
         if (type != nullptr) {
            put_uint32(pb, CULL_PACK_DESCR_BY_INDEX);
            put_uint32(pb, type[0].nm);
            put_uint32(pb, n);
            for (const std::uint32_t p : idx) {
               put_uint32(pb, p);
            }
            return;
         }
         put_uint32(pb, CULL_PACK_DESCR_BY_FIELD);
         put_uint32(pb, n);
         for (const auto f : ocs_cull_present_fields(dp)) {
            // the reference count flag is process local and never leaves the process
            put_uint32(pb, dp[f.pos].nm);
            put_uint32(pb, dp[f.pos].mt & ~CULL_DESCR_INTERNED);
         }
      }

      /**
       * @brief Write the values of an element
       *
       * A field marked CULL_NO_TRANSFER keeps its place, so the receiver sees
       * an object of unchanged shape; only its value is that of a new element.
       *
       * @param pb the buffer
       * @param cp the values
       * @param dp the descriptor
       * @param what the selection, or nullptr for every field
       * @param flags write only the fields whose flags match, 0 for all
       * @throw PackException with PACK_ENOMEM, PACK_FORMAT
       */
      void pack_values(sge_pack_buffer *pb, const lMultiType *cp, const lDescr *dp, const lEnumeration *what,
                       const int flags) {
         static constexpr lMultiType no_transfer_value{};

         CULL_STATS_CODE(if (!pb->just_count) { CULL_STATS_TYPE(dp->nm, PACK_ELEMS, 1); })
         if (what == nullptr) {
            for (const auto f : ocs_cull_present_fields(dp)) {
               const lDescr &d = dp[f.pos];
               // with flags only the fields matching them, e.g. CULL_SPOOL
               if (flags == 0 || (d.mt & flags) != 0) {
                  const lMultiType *value = mt_do_transfer(d.mt) ? &cp[f.slot] : &no_transfer_value;
                  CULL_STATS_CODE(const std::size_t stats_start = pb->bytes_used;)
                  pack_value(pb, value, nullptr, mt_get_type(d.mt), flags);
                  CULL_STATS_CODE(count_value(pb, d.nm, mt_get_type(d.mt), stats_start);)
               }
            }
         } else {
            for (int i = 0; what[i].nm != NoName; i++) {
               if (flags == 0 || (what[i].mt & flags) != 0) {
                  // also when the client asked for the field by name, not through ALL
                  const bool do_transfer = mt_do_transfer(what[i].mt);
                  const lMultiType *value = do_transfer ? &cp[ocs_cull_slot(dp, what[i].pos)] : &no_transfer_value;
                  const lEnumeration *sub_what = do_transfer ? what[i].ep : nullptr;
                  CULL_STATS_CODE(const std::size_t stats_start = pb->bytes_used;)
                  pack_value(pb, value, sub_what, mt_get_type(what[i].mt), flags);
                  CULL_STATS_CODE(count_value(pb, what[i].nm, mt_get_type(what[i].mt), stats_start);)
               }
            }
         }
      }

      /**
       * @brief Read the values of an element into its value array
       *
       * The buffer holds the values of the first @p wire_n fields of the
       * descriptor that have a value: all of them, or those of a shorter version
       * of the type (N-W-6). The fields beyond keep their initial values (N-B-8).
       * The array is attached to the element before the first value is read, so
       * that the element owns every value read when a later one fails.
       *
       * @param pb the buffer
       * @param ep the element, with its descriptor
       * @param wire_n the number of fields whose values follow
       * @param flags read only the fields whose flags match, 0 for all
       * @throw PackException with PACK_FORMAT, PACK_ENOMEM
       */
      void unpack_values(sge_pack_buffer *pb, lListElem *ep, const std::uint32_t wire_n, const int flags) {
         const lDescr *dp = ep->descr;
         const int n = lCountPresent(dp);
         if (wire_n > static_cast<std::uint32_t>(n)) {
            format_error(LEDESCRNULL, "more values than the descriptor has fields");
         }
         ep->cont = static_cast<lMultiType *>(calloc(1, sizeof(lMultiType) * (n + 1)));
         if (ep->cont == nullptr) {
            no_memory("unpacking the values of an element");
         }
         std::uint32_t i = 0;
         for (const auto f : ocs_cull_present_fields(dp)) {
            if (i++ == wire_n) {
               break;
            }
            // with flags only the fields matching them, e.g. CULL_SPOOL
            if (flags == 0 || (dp[f.pos].mt & flags) != 0) {
               unpack_value(pb, &(ep->cont[f.slot]), mt_get_type(dp[f.pos].mt), flags);
            }
         }
      }

      /**
       * @brief Read an element, knowing how many values a bound one carries
       * @param pb the buffer
       * @param descr the descriptor of a bound element; for a free element of flag-filtered data the expected one,
       *        or nullptr
       * @param wire_n the number of fields whose values follow for a bound element (N-W-6)
       * @param flags read only the fields whose flags match, 0 for all
       * @return the element, owned by the caller
       * @throw PackException with PACK_FORMAT, PACK_BADARG for a bound element without a descriptor, PACK_ENOMEM
       */
      lListElem *unpack_elem_n(sge_pack_buffer *pb, const lDescr *descr, std::uint32_t wire_n, const int flags) {
         PackingProfile profile;

         PartialElem ep(static_cast<lListElem *>(calloc(1, sizeof(lListElem))));
         if (ep == nullptr) {
            fail(PACK_ENOMEM, "unpacking an element");
         }

         cull_elem_kind kind;
         if (!elem_kind_from_wire(get_uint32(pb), &kind)) {
            fail(PACK_FORMAT, "unpacking an element of an unknown kind");
         }

         // an element bound or a sub-object at the sender is transitionally bound here until it is chained in, so
         // that appending does not reject it as bound
         if (kind == FREE_ELEM) {
            elem_set_free(ep.get());
            // a free element holds the shared descriptor equal to the one on the wire
            ep->descr = unpack_descr(pb, flags, &wire_n, flags != 0 ? descr : nullptr);
         } else {
            elem_set_trans_bound(ep.get());
            // an element that was not free takes its descriptor from outside
            if (descr == nullptr) {
               fail(PACK_BADARG, "unpacking a bound element without a descriptor");
            }
            ep->descr = descr;
         }

         unpack_values(pb, ep.get(), wire_n, flags);
         return ep.release();
      }

      /**
       * @brief Write a sub-object with its descriptor
       *
       * A nested selection reduces the sub-object as it reduces a sub-list: the
       * descriptor is written as the selection, and only the selected fields
       * follow (CS-3001).
       *
       * @param pb the buffer
       * @param ep the sub-object, or nullptr
       * @param what the selection, or nullptr for every field
       * @param flags write only the fields whose flags match, 0 for all
       * @throw PackException with PACK_ENOMEM, PACK_FORMAT, PACK_BADARG
       */
      void pack_object(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, const int flags) {
         put_uint32(pb, ep != nullptr);
         if (ep != nullptr) {
            if (what == nullptr) {
               pack_descr(pb, ep->descr, flags);
            } else {
               pack_what_as_descr(pb, what, ep->descr, flags);
            }
            pack_elem(pb, ep, what, flags);
         }
      }

      /**
       * @brief Read a sub-object with its descriptor
       * @param pb the buffer
       * @param flags read only the fields whose flags match, 0 for all
       * @return the sub-object, owned by the caller, or nullptr for one that was null
       * @throw PackException with PACK_FORMAT, PACK_ENOMEM
       */
      lListElem *unpack_object(sge_pack_buffer *pb, const int flags) {
         if (get_uint32(pb) == 0) {
            return nullptr;
         }
         std::uint32_t wire_n = 0;
         OwnedDescr descr(unpack_descr(pb, flags, &wire_n, nullptr));
         lListElem *ep = unpack_elem_n(pb, descr.get(), wire_n, flags);
         // the sub-object takes over the reference on the descriptor it borrowed while being unpacked
         if (ep->descr == descr.get()) {
            static_cast<void>(descr.release());
         }
         elem_set_object(ep);
         return ep;
      }

   } // namespace

   std::size_t descr_table_size(const sge_pack_buffer *pb) noexcept {
      return pb == nullptr || pb->dtab == nullptr ? 0 : pb->dtab->entries.size();
   }

   void free_descr_table(sge_pack_buffer *pb) noexcept {
      if (pb == nullptr || pb->dtab == nullptr) {
         return;
      }
      for (const lDescr *dp : pb->dtab->entries) {
         ocs_cull_descr_release(&dp);
      }
      delete pb->dtab;
      pb->dtab = nullptr;
   }

   void pack_descr(sge_pack_buffer *pb, const lDescr *descr, const int flags) {
      need_buffer(pb, "packing a descriptor");
      if (descr == nullptr) {
         fail(PACK_BADARG, "packing a null descriptor");
      }
      CULL_STATS_CODE(const std::size_t stats_start = pb->bytes_used;)

      if (flags == 0) {
         // the shared descriptor: what a list or a free element holds, or the one equal to a plain array
         const bool held = ocs_cull_descr_is_counted(descr) || ocs_cull_descr_is_static(descr);
         OwnedDescr acquired(held ? nullptr : ocs_cull_descr_acquire(descr));
         const lDescr *shared = held ? descr : acquired.get();
         if (shared == nullptr) {
            fail(PACK_ENOMEM, "packing a descriptor");
         }
         pack_shared_descr(pb, shared);
         CULL_STATS_CODE(count_descr(pb, descr->nm, stats_start);)
         return;
      }

      // the number of fields that have a value, without the end mark, and those fields
      put_uint32(pb, lCountPresent(descr));
      for (const auto f : ocs_cull_present_fields(descr)) {
         put_uint32(pb, descr[f.pos].nm);
         // the reference count flag is process local and never leaves the process
         put_uint32(pb, descr[f.pos].mt & ~CULL_DESCR_INTERNED);
      }
      CULL_STATS_CODE(count_descr(pb, descr->nm, stats_start);)
   }

   void pack_what_as_descr(sge_pack_buffer *pb, const lEnumeration *what, const lDescr *descr, const int flags) {
      need_buffer(pb, "packing a selection as a descriptor");
      CULL_STATS_CODE(const std::size_t stats_start = pb->bytes_used;)

      // a selection on the full type applies to a reduction holding its fields (N-B-11)
      if (!ocs_cull_what_fits(what, descr)) {
         LERROR(LEENUMDESCR);
         fail(PACK_BADARG, "packing a selection of fields the descriptor does not hold");
      }

      PackingProfile profile;
      if (flags == 0 && what[0].pos != WHAT_NONE) {
         bool owned;
         const lDescr *shared = ocs_cull_what_descr(what, descr, &owned);
         if (shared == nullptr) {
            fail(PACK_ENOMEM, "packing a selection as a descriptor");
         }
         OwnedDescr release(owned ? shared : nullptr);
         pack_shared_descr(pb, shared);
         CULL_STATS_CODE(count_descr(pb, descr->nm, stats_start);)
         return;
      }
      // the number of fields, without the end mark
      put_uint32(pb, count_what(what, descr));
      switch (what[0].pos) {
         case WHAT_ALL:
            for (const auto f : ocs_cull_present_fields(descr)) {
               put_uint32(pb, descr[f.pos].nm);
               put_uint32(pb, descr[f.pos].mt & ~CULL_DESCR_INTERNED);
            }
            break;
         case WHAT_NONE:
            break;
         default: {
            // the fields like those of a descriptor; a selection of some fields is reduced, so that the receiver
            // never takes its leading fields for a shorter version of the type (N-W-6)
            const std::uint32_t reduced = count_what(what, descr) < descr_length(descr) ? CULL_IS_REDUCED : 0;
            for (int i = 0; what[i].nm != NoName; i++) {
               put_uint32(pb, what[i].nm);
               put_uint32(pb, (what[i].mt & ~CULL_DESCR_INTERNED) | reduced);
            }
            break;
         }
      }
      CULL_STATS_CODE(count_descr(pb, descr->nm, stats_start);)
   }

   void pack_elem(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, const int flags) {
      need_buffer(pb, "packing an element");
      if (ep == nullptr) {
         fail(PACK_BADARG, "packing a null element");
      }
      PackingProfile profile;
      if (ep->descr == nullptr) {
         ocs::TerminationManager::trigger_abort();
      }

      put_uint32(pb, elem_kind_to_wire(elem_kind(ep)));
      if (elem_kind(ep) == FREE_ELEM) {
         if (what == nullptr) {
            pack_descr(pb, ep->descr, flags);
         } else {
            pack_what_as_descr(pb, what, ep->descr, flags);
         }
      }
      pack_values(pb, ep->cont, ep->descr, what, flags);
   }

   lListElem *unpack_elem(sge_pack_buffer *pb, const lDescr *descr, const int flags) {
      need_buffer(pb, "unpacking an element");
      return unpack_elem_n(pb, descr, descr != nullptr ? static_cast<std::uint32_t>(lCountPresent(descr)) : 0, flags);
   }

   void pack_list(sge_pack_buffer *pb, const lList *lp, const lEnumeration *what, const int flags) {
      need_buffer(pb, "packing a list");
      PackingProfile profile;

      if (lp == nullptr) {
         put_uint32(pb, 0);
         return;
      }
      put_uint32(pb, 1);
      put_str(pb, lp->listname);
      if (what == nullptr) {
         pack_descr(pb, lp->descr, flags);
      } else {
         pack_what_as_descr(pb, what, lp->descr, flags);
      }
      for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         // each element is preceded by a 1: another element follows
         put_uint32(pb, 1);
         pack_elem(pb, ep, what, flags);
      }
      // after the last element a 0: no more elements follow
      put_uint32(pb, 0);
   }

   lList *unpack_list(sge_pack_buffer *pb, const int flags) {
      need_buffer(pb, "unpacking a list");
      const UniqueCheckSuspension suspension;
      PackingProfile profile;

      // a list that was null
      if (get_uint32(pb) == 0) {
         return nullptr;
      }
      CULL_STATS_CODE(const std::uint64_t stats_start = ocs::cull_stats::now_ns();)

      OwnedList lp(static_cast<lList *>(calloc(1, sizeof(lList))));
      if (lp == nullptr) {
         fail(PACK_ENOMEM, "unpacking a list");
      }
      lp->listname = get_str(pb);

      // the descriptor; the list holds the shared descriptor equal to it
      std::uint32_t wire_n = 0;
      lp->descr = unpack_descr(pb, flags, &wire_n, nullptr);

      // one hash slot per hashed field; a table is built by the first lookup on its field
      if (!cull_hash_init_slots(lp.get(), true)) {
         fail(PACK_ENOMEM, "unpacking a list");
      }

      // as long as a 1 follows, an element follows
      while (get_uint32(pb) != 0) {
         PartialElem ep(unpack_elem_n(pb, lp->descr, wire_n, flags));
         append_elem(lp.get(), ep.get());
         static_cast<void>(ep.release());
      }

      CULL_STATS_CODE(const std::uint64_t stats_end = ocs::cull_stats::now_ns();
                      CULL_STATS_TYPE(lp->descr->nm, UNPACK_LISTS, 1);
                      CULL_STATS_TYPE(lp->descr->nm, UNPACK_ELEMS, lp->nelem);
                      CULL_STATS_TYPE(lp->descr->nm, UNPACK_NS, stats_end - stats_start);)
      return lp.release();
   }

   namespace {

      /**
       * @brief Skip a string on a copy of a buffer, without allocating it
       * @param pb the copy
       * @throw PackException with PACK_FORMAT if the buffer ends before the terminator
       */
      void skip_str(sge_pack_buffer *pb) {
         if (pb->bytes_used >= pb->mem_size) {
            fail(PACK_FORMAT, "peeking past a string");
         }
         const void *nul = memchr(pb->cur_ptr, '\0', pb->mem_size - pb->bytes_used);
         if (nul == nullptr) {
            fail(PACK_FORMAT, "peeking past a string");
         }
         const std::size_t n = static_cast<std::size_t>(static_cast<const char *>(nul) - pb->cur_ptr) + 1;
         pb->cur_ptr = &(pb->cur_ptr[n]);
         pb->bytes_used += n;
      }

      /**
       * @brief The type a descriptor on the wire belongs to, read on a copy of a buffer
       *
       * Reads only as far as needed to name the type; the table of the buffer
       * is consulted for a reference, never changed.
       *
       * @param pb the copy, positioned in front of the descriptor
       * @return the first field number of the type
       * @throw PackException with PACK_FORMAT for bytes that are cut off or a descriptor of no known type
       */
      int peek_descr_type(sge_pack_buffer *pb) {
         const std::uint32_t n = get_uint32(pb);
         int nm = NoName;
         if (n == CULL_PACK_DESCR_TYPE_REF) {
            nm = static_cast<int>(get_uint32(pb));
         } else if (n == CULL_PACK_DESCR_TABLE_REF) {
            const std::uint32_t index = get_uint32(pb);
            if (pb->dtab == nullptr || index >= pb->dtab->entries.size()) {
               fail(PACK_FORMAT, "peeking at a reference to a descriptor table entry that does not exist");
            }
            nm = pb->dtab->entries[index][0].nm;
         } else if (n == CULL_PACK_DESCR_TABLE_DEF) {
            const std::uint32_t kind = get_uint32(pb);
            if (kind == CULL_PACK_DESCR_BY_INDEX) {
               nm = static_cast<int>(get_uint32(pb));
            } else if (kind == CULL_PACK_DESCR_BY_FIELD) {
               if (get_uint32(pb) == 0) {
                  fail(PACK_FORMAT, "peeking at a descriptor without fields");
               }
               nm = static_cast<int>(get_uint32(pb));
            } else {
               fail(PACK_FORMAT, "peeking at a descriptor table entry of an unknown kind");
            }
         } else {
            // a descriptor field by field: the type of its first field
            if (n == 0) {
               fail(PACK_FORMAT, "peeking at a descriptor without fields");
            }
            nm = static_cast<int>(get_uint32(pb));
         }
         int pos = 0;
         const lDescr *type = ocs_cull_descr_static_containing(nm, &pos);
         if (type == nullptr) {
            fail(PACK_FORMAT, "peeking at a descriptor of no known type");
         }
         return type[0].nm;
      }

   } // namespace

   int peek_list_type(const sge_pack_buffer *pb) {
      need_buffer(pb, "peeking at a list");
      // a copy of the position: the buffer itself is not moved
      sge_pack_buffer copy = *pb;
      if (get_uint32(&copy) == 0) {
         return NoName;
      }
      skip_str(&copy);
      return peek_descr_type(&copy);
   }

   int peek_elem_type(const sge_pack_buffer *pb) {
      need_buffer(pb, "peeking at an element");
      sge_pack_buffer copy = *pb;
      cull_elem_kind kind;
      if (!elem_kind_from_wire(get_uint32(&copy), &kind)) {
         fail(PACK_FORMAT, "peeking at an element of an unknown kind");
      }
      if (kind != FREE_ELEM) {
         return NoName;
      }
      return peek_descr_type(&copy);
   }

   void pack_what(sge_pack_buffer *pb, const lEnumeration *what) {
      need_buffer(pb, "packing a selection");
      PackingProfile profile;

      put_uint32(pb, what != nullptr);
      if (what == nullptr) {
         return;
      }
      // ALL, NONE or a selection of fields
      std::uint32_t flag;
      switch (what[0].pos) {
         case WHAT_ALL:
            flag = PackWhatAll;
            break;
         case WHAT_NONE:
            flag = PackWhatNone;
            break;
         default:
            flag = PackWhatArray;
      }
      put_uint32(pb, flag);

      if (flag == PackWhatArray) {
         // the number of fields, without the end mark, and the fields
         int n = 0;
         while (what[n].nm != NoName) {
            n++;
         }
         put_uint32(pb, n);
         for (int i = 0; mt_get_type(what[i].nm) != NoName && mt_get_type(what[i].mt) != lEndT; i++) {
            put_uint32(pb, what[i].pos);
            put_uint32(pb, what[i].mt);
            put_uint32(pb, what[i].nm);
            if (what[i].ep == nullptr) {
               put_uint32(pb, 0);
            } else {
               put_uint32(pb, 1);
               pack_what(pb, what[i].ep);
            }
         }
      }
   }

   lEnumeration *unpack_what(sge_pack_buffer *pb) {
      need_buffer(pb, "unpacking a selection");
      PackingProfile profile;

      // a selection that was null
      if (get_uint32(pb) == 0) {
         return nullptr;
      }

      const std::uint32_t flag = get_uint32(pb);
      if (flag != PackWhatArray) {
         OwnedWhat what(static_cast<lEnumeration *>(calloc(2, sizeof(lEnumeration))));
         if (what == nullptr) {
            fail(PACK_ENOMEM, "unpacking a selection");
         }
         switch (flag) {
            case PackWhatAll:
               what.get()[0].pos = WHAT_ALL;
               break;
            case PackWhatNone:
               what.get()[0].pos = WHAT_NONE;
               break;
            default:
               break;
         }
         what.get()[0].nm = -99;
         what.get()[0].mt = -99;
         what.get()[0].ep = nullptr;
         what.get()[1].nm = NoName;
         what.get()[1].mt = lEndT;
         what.get()[1].ep = nullptr;
         return what.release();
      }

      // the number of fields, without the end mark
      const std::uint32_t n = get_uint32(pb);
      if (n + 1 > MAX_DESCR_SIZE) {
         no_memory("unpacking a selection of too many fields");
      }
      OwnedWhat what(static_cast<lEnumeration *>(calloc(n + 1, sizeof(lEnumeration))));
      if (what == nullptr) {
         fail(PACK_ENOMEM, "unpacking a selection");
      }
      // the end mark first, so that freeing a selection cut off after some fields finds its end
      lEnumeration *ep = what.get();
      for (std::uint32_t i = 0; i <= n; i++) {
         ep[i].nm = NoName;
         ep[i].mt = lEndT;
      }
      for (std::uint32_t i = 0; i < n; i++) {
         const auto pos = static_cast<int>(get_uint32(pb));
         const auto mt = static_cast<int>(get_uint32(pb));
         const auto nm = static_cast<int>(get_uint32(pb));
         ep[i].pos = pos;
         ep[i].mt = mt;
         ep[i].nm = nm;
         // the position, the type and the flags of a field are taken from the local type, not from the sender
         // (N-W-5, N-W-11)
         int local_pos = 0;
         if (const lDescr *type = ocs_cull_descr_static_containing(ep[i].nm, &local_pos); type != nullptr) {
            ep[i].pos = local_pos;
            ep[i].mt = type[local_pos].mt;
         }
         if (get_uint32(pb) == 1) {
            ep[i].ep = unpack_what(pb);
         }
      }
      ep[n].nm = NoName;
      ep[n].mt = lEndT;
      ep[n].ep = nullptr;
      return what.release();
   }

   void pack_where(sge_pack_buffer *pb, const lCondition *cp) {
      need_buffer(pb, "packing a condition");
      PackingProfile profile;

      put_uint32(pb, cp != nullptr);
      if (cp == nullptr) {
         return;
      }
      // the operator, which tells what follows
      put_uint32(pb, cp->op);
      switch (cp->op) {
         case EQUAL:
         case NOT_EQUAL:
         case LOWER_EQUAL:
         case LOWER:
         case GREATER_EQUAL:
         case GREATER:
         case BITMASK:
         case STRCASECMP:
         case PATTERNCMP:
         case SUBSCOPE:
         case HOSTNAMECMP:
            put_uint32(pb, cp->operand.cmp.pos);
            put_uint32(pb, cp->operand.cmp.mt);
            put_uint32(pb, cp->operand.cmp.nm);
            if (mt_get_type(cp->operand.cmp.mt) != lListT) {
               pack_value(pb, &(cp->operand.cmp.val), nullptr, mt_get_type(cp->operand.cmp.mt), 0);
            } else {
               pack_where(pb, cp->operand.cmp.val.cp);
            }
            break;
         case NEG:
            pack_where(pb, cp->operand.log.first);
            break;
         case AND:
         case OR:
            pack_where(pb, cp->operand.log.first);
            pack_where(pb, cp->operand.log.second);
            break;
         default:
            fail(PACK_FORMAT, "packing a condition with an unknown operator");
      }
   }

   lCondition *unpack_where(sge_pack_buffer *pb) {
      need_buffer(pb, "unpacking a condition");
      PackingProfile profile;

      // a condition that was null
      if (get_uint32(pb) == 0) {
         return nullptr;
      }
      OwnedWhere cp(static_cast<lCondition *>(calloc(1, sizeof(lCondition))));
      if (cp == nullptr) {
         no_memory("unpacking a condition");
      }
      cp->op = static_cast<int>(get_uint32(pb));
      switch (cp->op) {
         case EQUAL:
         case NOT_EQUAL:
         case LOWER_EQUAL:
         case LOWER:
         case GREATER_EQUAL:
         case GREATER:
         case BITMASK:
         case STRCASECMP:
         case PATTERNCMP:
         case SUBSCOPE:
         case HOSTNAMECMP:
            cp->operand.cmp.pos = static_cast<int>(get_uint32(pb));
            cp->operand.cmp.mt = static_cast<int>(get_uint32(pb));
            cp->operand.cmp.nm = static_cast<int>(get_uint32(pb));
            if (mt_get_type(cp->operand.cmp.mt) != lListT) {
               unpack_value(pb, &(cp->operand.cmp.val), mt_get_type(cp->operand.cmp.mt), 0);
            } else {
               cp->operand.cmp.val.cp = unpack_where(pb);
            }
            break;
         case NEG:
            cp->operand.log.first = unpack_where(pb);
            break;
         case AND:
         case OR:
            cp->operand.log.first = unpack_where(pb);
            cp->operand.log.second = unpack_where(pb);
            break;
         default:
            fail(PACK_FORMAT, "unpacking a condition with an unknown operator");
      }
      return cp.release();
   }

   std::string bytes_to_hex(const char *bytes, const int size) {
      static constexpr char digits[] = "0123456789ABCDEF";
      std::string hex;
      hex.reserve(static_cast<std::size_t>(size) * 2);
      for (int i = 0; i < size; i++) {
         hex += digits[bytes[i] & 0x0F];
         hex += digits[(bytes[i] & 0xF0) >> 4];
      }
      return hex;
   }

   int hex_to_bytes(const char *hex, char **bytes) {
      static constexpr char digits[] = "0123456789ABCDEF";
      const int size = static_cast<int>(strlen(hex) / 2);
      *bytes = sge_malloc(size);
      memset(*bytes, 0, size);
      int lower_part = 0;
      int upper_part = 0;
      for (int i = 0; i < size; i++) {
         int a;
         for (a = 0; a < 16; a++) {
            if (digits[a] == hex[i * 2]) {
               lower_part = a;
               break;
            }
         }
         if (a == 16) {
            return i * -2;
         }
         for (a = 0; a < 16; a++) {
            if (digits[a] == hex[i * 2 + 1]) {
               upper_part = a;
               break;
            }
         }
         if (a == 16) {
            return (i * -2) - 1;
         }
         (*bytes)[i] = static_cast<char>((upper_part << 4) + lower_part);
      }
      return size;
   }

} // namespace ocs::tom
