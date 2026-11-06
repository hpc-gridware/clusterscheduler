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
 * @brief Packing and unpacking cull lists, elements and descriptors
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <new>
#include <vector>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_list.h"
#include "cull/cull_hash.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_stats.h"
#include "cull/cull_whatP.h"
#include "cull/cull_whereP.h"
#include "cull/cull_pack.h"
#include "cull/ocs_cull_descr.h"
#include "cull/cull_parse.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

/* =========== implementation ================================= */

static int cull_unpack_switch(sge_pack_buffer *pb, lMultiType *dst, int type, int flags);

static int
cull_pack_switch(sge_pack_buffer *pb, const lMultiType *src, lEnumeration *what,
                 int type, int flags);

static int cull_unpack_descr(sge_pack_buffer *pb, const lDescr **dpp, int flags, uint32_t *wire_n,
                             const lDescr *expected);

static int cull_unpack_cont(sge_pack_buffer *pb, lMultiType **mpp, const lDescr *dp, uint32_t wire_n, int flags);

static int ocs_cull_unpack_elem(sge_pack_buffer *pb, lListElem **epp, const lDescr *dp, uint32_t wire_n, int flags);

static int
cull_pack_cont(sge_pack_buffer *pb, const lMultiType *cp, const lDescr *dp,
               const lEnumeration *what, int flags);

static int cull_unpack_object(sge_pack_buffer *pb, lListElem **epp, int flags);

static int cull_pack_object(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, int flags);

#ifdef OCS_CULL_STATS
/**
 * @brief Count a packed descriptor or field selection (CS-2993)
 *
 * Nothing is counted in counting mode, so that a size computation followed by
 * the real packing counts once.
 *
 * @param pb the buffer it was packed into
 * @param nm the first field of the descriptor it describes
 * @param start `bytes_used` before packing it
 */
static void cull_stats_count_descr(const sge_pack_buffer *pb, const int nm, const size_t start) {
   if (!pb->just_count) {
      CULL_STATS_TYPE(nm, PACK_DESCR, 1);
      CULL_STATS_TYPE(nm, PACK_DESCR_B, pb->bytes_used - start);
   }
}

/**
 * @brief Count how a shared descriptor was packed (CS-2997)
 *
 * Nothing is counted in counting mode, as in cull_stats_count_descr().
 *
 * @param pb the buffer it was packed into
 * @param nm the first field of the descriptor
 * @param c #DESCR_PACK_TYPE_REF, #DESCR_PACK_TABLE_DEF or #DESCR_PACK_TABLE_REF
 */
static void cull_stats_count_shared(const sge_pack_buffer *pb, const int nm, const ocs::cull_stats::Counter c) {
   if (!pb->just_count) {
      ocs::cull_stats::add_type(nm, c, 1);
   }
}

/**
 * @brief Count the bytes of a packed field value (CS-2993)
 *
 * Sublists and sub-objects are left out; their fields are counted on their own.
 * Nothing is counted in counting mode.
 *
 * @param pb the buffer it was packed into
 * @param nm the field
 * @param type the field's CULL type
 * @param start `bytes_used` before packing it
 */
static void cull_stats_count_value(const sge_pack_buffer *pb, const int nm, const int type, const size_t start) {
   if (!pb->just_count && type != lListT && type != lObjectT) {
      CULL_STATS_FIELD(nm, PACK_VALUE_B, pb->bytes_used - start);
   }
}
#endif


/* ------------------------------------------------------------

   cull_unpack_switch() - switch used to unpack a lMulitype

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT

 */
static int cull_unpack_switch(
        sge_pack_buffer *pb,
        lMultiType *dst,
        int type,
        int flags
) {
   DENTER(CULL_LAYER);

   int ret;
   uint32_t i = 0;
   uint64_t i64 = 0;

   switch (type) {

      case lBoolT:
         ret = unpackint(pb, &i);
         dst->b = i;
         break;

      case lUlongT:
         ret = unpackint(pb, &i);
         dst->ul = i;
         break;

      case lUlong64T:
         ret = unpackint64(pb, &i64);
         dst->ul64 = i64;
         break;

      case lStringT:
         ret = unpackstr(pb, &(dst->str));
         break;

      case lHostT:
         ret = unpackstr(pb, &(dst->host));
         break;

      case lListT:
         ret = cull_unpack_list_partial(pb, &(dst->glp), flags);
         break;

      case lObjectT:
         ret = cull_unpack_object(pb, &(dst->obj), flags);
         break;

      case lDoubleT:
         ret = unpackdouble(pb, &(dst->db));
         break;

      case lRefT:
         dst->ref = nullptr;
         ret = PACK_SUCCESS;
         break;

      case lIntT:
         DPRINTF("type not implemented\n");
         ret = PACK_FORMAT;
         break;
      default:
         DPRINTF("unknown type %d\n", type);
         ret = PACK_FORMAT;
         break;
   }

   DRETURN(ret);
}

/* ------------------------------------------------------------

   cull_pack_switch() - switch used to pack a lMulitype

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT

 */
static int
cull_pack_switch(sge_pack_buffer *pb, const lMultiType *src, lEnumeration *what,
                 int type, int flags) {
   DENTER(CULL_LAYER);

   int ret;

   switch (type) {

      case lBoolT:
         ret = packint(pb, src->b);
         break;

      case lUlongT:
         ret = packint(pb, src->ul);
         break;

      case lUlong64T:
         ret = packint64(pb, src->ul64);
         break;

      case lStringT:
         ret = packstr(pb, src->str);
         break;

      case lHostT:
         ret = packstr(pb, src->host);
         break;

      case lListT:
         ret = cull_pack_list_partial(pb, src->glp, what, flags);
         break;

      case lObjectT:
         ret = cull_pack_object(pb, src->obj, what, flags);
         break;

      case lDoubleT:
         ret = packdouble(pb, src->db);
         break;

      case lRefT:
         ret = PACK_SUCCESS;
         break;

      case lIntT:
      case lLongT:
         DPRINTF("not implemented\n");
         ret = PACK_FORMAT;
         break;
      default:
         DPRINTF("unknown type %d\n", type);
         ret = PACK_FORMAT;
         break;
   }

   DRETURN(ret);
}

/**
 * @brief The descriptor table of a pack buffer (N-W-4)
 *
 * Both sides number the descriptors of a buffer in the order they are written
 * and read. The sender looks a shared descriptor up by its address, which is
 * stable because the table holds a reference on it; the receiver finds an
 * entry by its index. Each entry holds one reference, given up by
 * clear_packbuffer().
 */
struct ocs_cull_descr_table {
   std::vector<const lDescr *> entries; ///< the shared descriptors, by table index
};

/**
 * @brief Give up the descriptor table of a buffer
 * @param pb the buffer; its table is released and set to nullptr
 */
void ocs_cull_pack_table_free(sge_pack_buffer *pb) {
   if (pb == nullptr || pb->dtab == nullptr) {
      return;
   }
   for (const lDescr *dp : pb->dtab->entries) {
      ocs_cull_descr_release(&dp);
   }
   delete pb->dtab;
   pb->dtab = nullptr;
}

/**
 * @brief Number of entries in the descriptor table of a buffer
 * @param pb the buffer
 * @return the number of distinct reduced descriptors written to or read from it
 */
size_t ocs_cull_pack_table_size(const sge_pack_buffer *pb) {
   return pb == nullptr || pb->dtab == nullptr ? 0 : pb->dtab->entries.size();
}

/**
 * @brief The table of a buffer, created on first use
 * @param pb the buffer
 * @return its table, or nullptr if out of memory
 */
static ocs_cull_descr_table *pack_table(sge_pack_buffer *pb) {
   if (pb->dtab == nullptr) {
      pb->dtab = new (std::nothrow) ocs_cull_descr_table;
   }
   return pb->dtab;
}

/**
 * @brief Find the static type a descriptor packed field by field is a version of
 *
 * A descriptor that is not reduced and lists the leading fields of a static
 * type, with the same field numbers and data types, was packed by a release
 * whose type had these fields only: types grow at the end within a minor
 * release (N-W-6). Listing all fields of the type and more is a format error,
 * because it comes from a newer release.
 *
 * @param dp the descriptor as read from the buffer, not reduced
 * @param n the number of fields of dp
 * @param[out] typep receives the static type, or nullptr if dp does not list leading fields of one
 * @return #PACK_SUCCESS, or #PACK_FORMAT if dp is longer than its type
 */
static int ocs_cull_unpack_prefix_type(const lDescr *dp, const uint32_t n, const lDescr **typep) {
   *typep = nullptr;
   if (n == 0) {
      return PACK_SUCCESS;
   }
   int pos = 0;
   const lDescr *type = ocs_cull_descr_static_containing(dp[0].nm, &pos);
   if (type == nullptr || pos != 0) {
      return PACK_SUCCESS;
   }
   const uint32_t type_n = static_cast<uint32_t>(lCountDescr(type));
   for (uint32_t i = 0; i < n && i < type_n; i++) {
      if (dp[i].nm != type[i].nm || mt_get_type(dp[i].mt) != mt_get_type(type[i].mt)) {
         return PACK_SUCCESS;
      }
   }
   if (n > type_n) {
      return PACK_FORMAT;
   }
   *typep = type;
   return PACK_SUCCESS;
}

/**
 * @brief Unpack an object type descriptor into the descriptor a list or an element holds
 *
 * A type reference (#CULL_PACK_DESCR_TYPE_REF) yields the static block of the
 * type without building anything (N-W-3); a type the name space of the calling
 * thread does not know, or more fields than it has, is a format error. Fewer
 * fields come from a release whose type had only these: the result is the full
 * local type, and only the sent number of values follows (N-W-6).
 *
 * A descriptor packed field by field outside the descriptor table (flag-filtered
 * data) that is not reduced and lists the leading fields of a static type is
 * that type in the same way. With an expected static type, such a descriptor
 * must be a version of it; any other one is a format error. Every remaining
 * descriptor is built with the full length of the type its fields belong to,
 * marked reduced if it does not list the whole type, and replaced by the shared
 * descriptor equal to it; fields of no type the name space of the calling
 * thread knows are a format error.
 *
 * The fields are listed in type order, every what being sorted by position, so
 * the values that follow go to the slots in order (N-B-8); fields out of that
 * order or listed twice are a format error.
 *
 * @param pb the buffer to read from
 * @param dpp the descriptor, holding a reference for the caller
 * @param flags the flags the values were packed with; flag-filtered data never carries a type reference
 * @param[out] wire_n receives the number of fields whose values follow per element
 * @param expected the static type a free element of flag-filtered data must have, or nullptr
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
static int cull_unpack_descr(
        sge_pack_buffer *pb,
        const lDescr **dpp,
        const int flags,
        uint32_t *wire_n,
        const lDescr *expected
) {
   DENTER(CULL_LAYER);

   lDescr *dp;
   int ret;
   uint32_t n = 0;
   uint32_t i = 0;
   uint32_t temp = 0;

   *dpp = nullptr;
   *wire_n = 0;

   /* read in number of lDescr fields (without end mark), or the type reference marker */
   if ((ret = unpackint(pb, &n))) {
      DRETURN(ret);
   }

   if (n == CULL_PACK_DESCR_TYPE_REF) {
      uint32_t first_nm = 0;
      uint32_t count = 0;
      if (flags != 0) {
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }
      if ((ret = unpackint(pb, &first_nm)) != PACK_SUCCESS || (ret = unpackint(pb, &count)) != PACK_SUCCESS) {
         DRETURN(ret);
      }
      // fewer fields than the local type: the sender's type had only these (N-W-6)
      const lDescr *block = ocs_cull_descr_static_for(static_cast<int>(first_nm));
      if (block == nullptr || count == 0 || static_cast<int>(count) > lCountDescr(block)) {
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }
      *dpp = block;
      *wire_n = count;
      DRETURN(PACK_SUCCESS);
   }

   /* an index into the table of the buffer, or a new entry for it */
   bool enter_table = false;
   if (n == CULL_PACK_DESCR_TABLE_REF || n == CULL_PACK_DESCR_TABLE_DEF) {
      if (flags != 0) {
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }
      if (n == CULL_PACK_DESCR_TABLE_REF) {
         uint32_t index = 0;
         if ((ret = unpackint(pb, &index)) != PACK_SUCCESS) {
            DRETURN(ret);
         }
         if (pb->dtab == nullptr || index >= pb->dtab->entries.size()) {
            LERROR(LEDESCRNULL);
            DRETURN(PACK_FORMAT);
         }
         *dpp = ocs_cull_descr_ref(pb->dtab->entries[index]);
         *wire_n = static_cast<uint32_t>(lCountPresent(*dpp));
         DRETURN(PACK_SUCCESS);
      }

      uint32_t kind = 0;
      if ((ret = unpackint(pb, &kind)) != PACK_SUCCESS) {
         DRETURN(ret);
      }
      if (kind == CULL_PACK_DESCR_BY_INDEX) {
         /* the fields by their index in one type, in type order; mt is taken from the local type (N-W-5, N-B-8) */
         uint32_t type_nm = 0;
         uint32_t count = 0;
         if ((ret = unpackint(pb, &type_nm)) != PACK_SUCCESS || (ret = unpackint(pb, &count)) != PACK_SUCCESS) {
            DRETURN(ret);
         }
         const lDescr *type = ocs_cull_descr_static_for(static_cast<int>(type_nm));
         const int type_n = type != nullptr ? lCountDescr(type) : 0;
         if (type == nullptr || count > static_cast<uint32_t>(type_n)) {
            LERROR(LEDESCRNULL);
            DRETURN(PACK_FORMAT);
         }
         std::vector<bool> keep(type_n, false);
         int64_t last = -1;
         for (i = 0; i < count; i++) {
            if ((ret = unpackint(pb, &temp)) != PACK_SUCCESS || temp >= static_cast<uint32_t>(type_n) ||
                static_cast<int64_t>(temp) <= last) {
               if (ret == PACK_SUCCESS) {
                  LERROR(LEDESCRNULL);
                  ret = PACK_FORMAT;
               }
               DRETURN(ret);
            }
            last = temp;
            keep[temp] = true;
         }
         if (count == 0) {
            LERROR(LEDESCRNULL);
            DRETURN(PACK_FORMAT);
         }
         CULL_STATS_TYPE(type[0].nm, DCOPY_UNPACK, 1);
         CULL_STATS_TYPE(type[0].nm, DCOPY_UNPACK_B, sizeof(lDescr) * (count + 1));
         const lDescr *shared = ocs_cull_descr_build(type, keep);
         ocs_cull_descr_table *tab = shared != nullptr ? pack_table(pb) : nullptr;
         if (tab == nullptr) {
            ocs_cull_descr_release(&shared);
            LERROR(LEMALLOC);
            DRETURN(PACK_ENOMEM);
         }
         tab->entries.push_back(shared);
         *dpp = ocs_cull_descr_ref(shared);
         *wire_n = count;
         DRETURN(PACK_SUCCESS);
      }
      if (kind != CULL_PACK_DESCR_BY_FIELD) {
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }
      /* field by field, as without the table; the result enters the table */
      if ((ret = unpackint(pb, &n)) != PACK_SUCCESS) {
         DRETURN(ret);
      }
      enter_table = true;
   }

   /* n+1 does not work because this casts n to int */
   if (n > MAX_DESCR_SIZE - 1) {
      LERROR(LEMALLOC);
      DRETURN(PACK_ENOMEM);
   }

   if ((dp = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1))) == nullptr) {
      LERROR(LEMALLOC);
      DRETURN(PACK_ENOMEM);
   }

   memset(dp, 0, sizeof(lDescr) * (n + 1));

   dp[n].nm = NoName;
   dp[n].mt = lEndT;

   /* read in n lDescr fields, in type order (N-B-8) */
   bool wire_reduced = false;
   for (i = 0; i < n; i++) {
      if ((ret = unpackint(pb, &temp))) {
         sge_free(&dp);
         DRETURN(ret);
      }
      dp[i].nm = temp;
      if (i > 0 && dp[i].nm <= dp[i - 1].nm) {
         sge_free(&dp);
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }

      if ((ret = unpackint(pb, &temp))) {
         sge_free(&dp);
         DRETURN(ret);
      }
      // a sender never packs the process local reference count flag; clear it anyway
      dp[i].mt = temp & ~CULL_DESCR_INTERNED;
      wire_reduced = wire_reduced || (dp[i].mt & CULL_IS_REDUCED) != 0;
   }
   *wire_n = n;

   /* a full type packed field by field is the local static type, also when the sender's type was shorter (N-W-6) */
   if (!enter_table && !wire_reduced) {
      const lDescr *type = nullptr;
      if ((ret = ocs_cull_unpack_prefix_type(dp, n, &type)) != PACK_SUCCESS ||
          (type != expected && expected != nullptr && ocs_cull_descr_is_static(expected))) {
         sge_free(&dp);
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }
      if (type != nullptr) {
         sge_free(&dp);
         *dpp = type;
         DRETURN(PACK_SUCCESS);
      }
   }
   CULL_STATS_TYPE(dp[0].nm, DCOPY_UNPACK, 1);
   CULL_STATS_TYPE(dp[0].nm, DCOPY_UNPACK_B, sizeof(lDescr) * (n + 1));

   /* the fields on the wire are held by a full-length descriptor of their type,
    * with mt from the local type as for a table entry by index (N-B-1, N-B-8);
    * fields of no known type, or of several, are a format error */
   {
      int first = 0;
      const lDescr *type = n > 0 ? ocs_cull_descr_static_containing(dp[0].nm, &first) : nullptr;
      const int type_n = type != nullptr ? lCountDescr(type) : 0;
      std::vector<bool> keep(type_n, false);
      bool known = type != nullptr;
      for (i = 0; known && i < n; i++) {
         const int pos = dp[i].nm - type[0].nm;
         known = pos >= 0 && pos < type_n && type[pos].nm == dp[i].nm &&
                 mt_get_type(type[pos].mt) == mt_get_type(dp[i].mt);
         if (known) {
            keep[pos] = true;
         }
      }
      sge_free(&dp);
      if (!known) {
         LERROR(LEDESCRNULL);
         DRETURN(PACK_FORMAT);
      }
      *dpp = ocs_cull_descr_build(type, keep);
   }
   if (*dpp == nullptr) {
      LERROR(LEMALLOC);
      DRETURN(PACK_ENOMEM);
   }
   if (enter_table) {
      ocs_cull_descr_table *tab = pack_table(pb);
      if (tab == nullptr) {
         ocs_cull_descr_release(dpp);
         LERROR(LEMALLOC);
         DRETURN(PACK_ENOMEM);
      }
      tab->entries.push_back(ocs_cull_descr_ref(*dpp));
   }

   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Pack a shared descriptor through the type reference or the descriptor table
 *
 * A static block is a type reference (N-W-3). A descriptor already in the table
 * of the buffer is its index. Any other one enters the table: by the indices of
 * its fields in their type when all fields belong to one type, so that the
 * receiver takes `mt` from its own type (N-W-5), or else field by field.
 *
 * @param pb the buffer to append to
 * @param dp a static or counted descriptor
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
static int pack_shared_descr(sge_pack_buffer *pb, const lDescr *dp) {
   int ret;
   const int n = lCountPresent(dp);

   if (!ocs_cull_descr_is_counted(dp)) {
      if ((ret = packint(pb, CULL_PACK_DESCR_TYPE_REF)) != PACK_SUCCESS ||
          (ret = packint(pb, dp[0].nm)) != PACK_SUCCESS || (ret = packint(pb, n)) != PACK_SUCCESS) {
         return ret;
      }
      CULL_STATS_CODE(cull_stats_count_shared(pb, dp[0].nm, ocs::cull_stats::DESCR_PACK_TYPE_REF);)
      return PACK_SUCCESS;
   }

   ocs_cull_descr_table *tab = pack_table(pb);
   if (tab == nullptr) {
      return PACK_ENOMEM;
   }
   for (size_t k = 0; k < tab->entries.size(); k++) {
      if (tab->entries[k] == dp) {
         if ((ret = packint(pb, CULL_PACK_DESCR_TABLE_REF)) != PACK_SUCCESS ||
             (ret = packint(pb, static_cast<uint32_t>(k))) != PACK_SUCCESS) {
            return ret;
         }
         CULL_STATS_CODE(cull_stats_count_shared(pb, dp[0].nm, ocs::cull_stats::DESCR_PACK_TABLE_REF);)
         return PACK_SUCCESS;
      }
   }
   tab->entries.push_back(ocs_cull_descr_ref(dp));
   CULL_STATS_CODE(cull_stats_count_shared(pb, dp[0].nm, ocs::cull_stats::DESCR_PACK_TABLE_DEF);)

   // by index if every field belongs to the type of the first one, with the same data type
   int pos = 0;
   const lDescr *type = n > 0 ? ocs_cull_descr_static_containing(dp[0].nm, &pos) : nullptr;
   std::vector<uint32_t> idx;
   for (const auto f : ocs_cull_present_fields(dp)) {
      if (type == nullptr) {
         break;
      }
      int p = 0;
      if (ocs_cull_descr_static_containing(dp[f.pos].nm, &p) != type || mt_get_type(type[p].mt) != mt_get_type(dp[f.pos].mt)) {
         type = nullptr;
         break;
      }
      idx.push_back(static_cast<uint32_t>(p));
   }

   if ((ret = packint(pb, CULL_PACK_DESCR_TABLE_DEF)) != PACK_SUCCESS) {
      return ret;
   }
   if (type != nullptr) {
      if ((ret = packint(pb, CULL_PACK_DESCR_BY_INDEX)) != PACK_SUCCESS || (ret = packint(pb, type[0].nm)) != PACK_SUCCESS ||
          (ret = packint(pb, n)) != PACK_SUCCESS) {
         return ret;
      }
      for (const uint32_t p : idx) {
         if ((ret = packint(pb, p)) != PACK_SUCCESS) {
            return ret;
         }
      }
      return PACK_SUCCESS;
   }
   if ((ret = packint(pb, CULL_PACK_DESCR_BY_FIELD)) != PACK_SUCCESS || (ret = packint(pb, n)) != PACK_SUCCESS) {
      return ret;
   }
   for (const auto f : ocs_cull_present_fields(dp)) {
      // the reference count flag is process local and never leaves the process
      if ((ret = packint(pb, dp[f.pos].nm)) != PACK_SUCCESS ||
          (ret = packint(pb, dp[f.pos].mt & ~CULL_DESCR_INTERNED)) != PACK_SUCCESS) {
         return ret;
      }
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack an object type descriptor
 *
 * A full descriptor, the static block of its type, is packed as a reference to
 * the type: #CULL_PACK_DESCR_TYPE_REF, the field number of its first field and
 * its field count (N-W-3). Any other descriptor, and every descriptor packed
 * with flags, is packed as its field count followed by `(nm, mt)` per field:
 * flag-filtered packing is a contract between both sides that decides from the
 * packed `mt` which values follow (N-W-8).
 *
 * @param pb the buffer to append to
 * @param dp the descriptor to pack
 * @param flags the flags the values are packed with, 0 for all values
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_pack_descr(sge_pack_buffer *pb, const lDescr *dp, const int flags) {
   DENTER(CULL_LAYER);

   int ret;
   CULL_STATS_CODE(const size_t stats_start = pb->bytes_used;)

   if (flags == 0) {
      // the shared descriptor: what a list or a free element holds, or the one equal to a plain array
      const bool held = ocs_cull_descr_is_counted(dp) || ocs_cull_descr_is_static(dp);
      const lDescr *shared = held ? dp : ocs_cull_descr_acquire(dp);
      if (shared == nullptr) {
         DRETURN(PACK_ENOMEM);
      }
      ret = pack_shared_descr(pb, shared);
      if (!held) {
         ocs_cull_descr_release(&shared);
      }
      CULL_STATS_CODE(cull_stats_count_descr(pb, dp->nm, stats_start);)
      DRETURN(ret);
   }

   /* pack the number of fields that have a value (without end mark) */
   if ((ret = packint(pb, lCountPresent(dp)))) {
      DRETURN(ret);
   }
   /* pack those fields */
   for (const auto f : ocs_cull_present_fields(dp)) {
      if ((ret = packint(pb, dp[f.pos].nm))) {
         DRETURN(ret);
      }
      // the reference count flag is process local and never leaves the process
      if ((ret = packint(pb, dp[f.pos].mt & ~CULL_DESCR_INTERNED))) {
         DRETURN(ret);
      }
   }
   CULL_STATS_CODE(cull_stats_count_descr(pb, dp->nm, stats_start);)

   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Pack a field selection as if it were a descriptor
 *
 * Lets the receiver build a reduced object type from the selection alone,
 * without the full descriptor being sent.
 *
 * Without flags the reduction is the shared descriptor the selection caches
 * (ocs_cull_what_descr()), packed through the type reference or the descriptor
 * table of the buffer like any held descriptor. With flags it is packed field by
 * field (N-W-8).
 *
 * @param pb the buffer to append to
 * @param what the selection to pack
 * @param descr the descriptor the selection refers to
 * @param flags the flags the values are packed with, 0 for all values
 * @return #PACK_SUCCESS, #PACK_ENOMEM, #PACK_FORMAT, or #PACK_BADARG if a selected
 *         field is not present in descr
 *
 * @todo remove not needed exit points
 */
int
cull_pack_enum_as_descr(sge_pack_buffer *pb, const lEnumeration *what,
                        const lDescr *descr, const int flags) {
   DENTER(CULL_LAYER);

   int i, ret;
   CULL_STATS_CODE(const size_t stats_start = pb->bytes_used;)

   /* a selection on the full type applies to a reduction holding its fields (N-B-11) */
   if (!ocs_cull_what_fits(what, descr)) {
      LERROR(LEENUMDESCR);
      DRETURN(PACK_BADARG);
   }

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   if (flags == 0 && what[0].pos != WHAT_NONE) {
      bool owned;
      const lDescr *shared = ocs_cull_what_descr(what, descr, &owned);
      if (shared == nullptr) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(PACK_ENOMEM);
      }
      ret = pack_shared_descr(pb, shared);
      if (owned) {
         ocs_cull_descr_release(&shared);
      }
      CULL_STATS_CODE(cull_stats_count_descr(pb, descr->nm, stats_start);)
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }
   /* pack the number of lDescr fields (without end mark) */
   if ((ret = packint(pb, lCountWhat(what, descr)))) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }
   switch (what[0].pos) {
      case WHAT_ALL:
         for (const auto f : ocs_cull_present_fields(descr)) {
            if ((ret = packint(pb, descr[f.pos].nm))) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
            if ((ret = packint(pb, descr[f.pos].mt & ~CULL_DESCR_INTERNED))) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
         }
         break;
      case WHAT_NONE:
         break;
      default: {
         /* pack the lEnumeration field similar to lDescr fields; a selection of some fields is reduced, so that the
          * receiver never takes its leading fields for a shorter version of the type (N-W-6) */
         const uint32_t reduced = lCountWhat(what, descr) < lCountDescr(descr) ? CULL_IS_REDUCED : 0;
         for (i = 0; what[i].nm != NoName; i++) {
            if ((ret = packint(pb, what[i].nm))) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
            if ((ret = packint(pb, (what[i].mt & ~CULL_DESCR_INTERNED) | reduced))) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
         }
         break;
      }
   }
   CULL_STATS_CODE(cull_stats_count_descr(pb, descr->nm, stats_start);)
   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);
}

/* ================================================================ */

/* ------------------------------------------------------------

   cull_pack_cont() - packs a contents array

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT

 */
static int
cull_pack_cont(sge_pack_buffer *pb, const lMultiType *cp, const lDescr *dp,
               const lEnumeration *what, int flags) {
   DENTER(CULL_LAYER);

   /* A field marked CULL_NO_TRANSFER keeps its place in the descriptor, so the
    * receiver sees an object of unchanged shape and lCountWhat() keeps agreeing
    * with the loops below. Only the value is different: what a freshly created
    * element carries. A zeroed lMultiType is exactly that — nullptr for the
    * pointer valued types, 0 for the scalar ones — and packstr(), the list and
    * the object writer all accept a null pointer, so no type needs a special
    * encoding here. */
   static constexpr lMultiType no_transfer_value{};

   CULL_STATS_CODE(if (!pb->just_count) { CULL_STATS_TYPE(dp->nm, PACK_ELEMS, 1); })
   if (what == nullptr) {
      for (const auto f : ocs_cull_present_fields(dp)) {
         const lDescr &d = dp[f.pos];
         /* if flags are given, pack only fields matching flags, e.g. CULL_SPOOL */
         if (flags == 0 || (d.mt & flags) != 0) {
            const lMultiType *value = mt_do_transfer(d.mt) ? &cp[f.slot] : &no_transfer_value;
            CULL_STATS_CODE(const size_t stats_start = pb->bytes_used;)
            const int ret = cull_pack_switch(pb, value, nullptr, mt_get_type(d.mt), flags);
            CULL_STATS_CODE(cull_stats_count_value(pb, d.nm, mt_get_type(d.mt), stats_start);)

            if (ret != PACK_SUCCESS) {
               DRETURN(ret);
            }
         }
      }
   } else {
      for (int i = 0; what[i].nm != NoName; i++) {
         if (flags == 0 || (what[i].mt & flags) != 0) {
            /* also when the client asked for the field by name, not through ALL */
            const bool do_transfer = mt_do_transfer(what[i].mt);
            const lMultiType *value = do_transfer ? &cp[ocs_cull_slot(dp, what[i].pos)] : &no_transfer_value;
            lEnumeration *const sub_what = do_transfer ? what[i].ep : nullptr;
            CULL_STATS_CODE(const size_t stats_start = pb->bytes_used;)
            const int ret = cull_pack_switch(pb, value, sub_what, mt_get_type(what[i].mt), flags);
            CULL_STATS_CODE(cull_stats_count_value(pb, what[i].nm, mt_get_type(what[i].mt), stats_start);)

            if (ret != PACK_SUCCESS) {
               DRETURN(ret);
            }
         }
      }
   }

   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Unpack the values of an element
 *
 * The buffer holds the values of the first wire_n fields of dp that have a
 * value: all of them, or the fields of a shorter version of the type that grew
 * at the end (N-W-6). The fields beyond keep their initial values, as in a new
 * element. The wire lists the fields in type order, which is their slot order,
 * so each value goes directly into its slot (N-B-8).
 *
 * @param pb the buffer to read from
 * @param[out] mpp receives the values, owned by the caller
 * @param dp the object type of the element
 * @param wire_n the number of fields whose values follow, at most the field count of dp
 * @param flags unpack only the fields whose `mt` matches, 0 for all
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
static int cull_unpack_cont(
        sge_pack_buffer *pb,
        lMultiType **mpp,
        const lDescr *dp,
        const uint32_t wire_n,
        const int flags
) {
   DENTER(CULL_LAYER);

   *mpp = nullptr;
   const int n = lCountPresent(dp);
   if (wire_n > static_cast<uint32_t>(n)) {
      LERROR(LEDESCRNULL);
      DRETURN(PACK_FORMAT);
   }
   auto *cp = static_cast<lMultiType *>(calloc(1, sizeof(lMultiType) * (n + 1)));
   if (cp == nullptr) {
      LERROR(LEMALLOC);
      DRETURN(PACK_ENOMEM);
   }

   uint32_t i = 0;
   for (const auto f : ocs_cull_present_fields(dp)) {
      if (i++ == wire_n) {
         break;
      }
      /* if flags are given, unpack only fields matching flags, e.g. CULL_SPOOL */
      if (flags == 0 || (dp[f.pos].mt & flags) != 0) {
         const int ret = cull_unpack_switch(pb, &(cp[f.slot]), mt_get_type(dp[f.pos].mt), flags);
         if (ret != PACK_SUCCESS) {
            sge_free(&cp);
            DRETURN(ret);
         }
      }
   }
   *mpp = cp;
   DRETURN(PACK_SUCCESS);
}

/* ================================================================ */

/**
 * @brief Pack a whole element, including its descriptor
 *
 * @param pb the buffer to append to
 * @param ep the element to pack
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_pack_elem(sge_pack_buffer *pb, const lListElem *ep) {
   DENTER(TOP_LAYER);

   int ret;

   ret = cull_pack_elem_partial(pb, ep, nullptr, 0);
   DRETURN(ret);
}


/**
 * @brief Pack the selected fields of an element
 *
 * @param pb the buffer to append to
 * @param ep the element to pack
 * @param what which fields to pack, or nullptr for all of them
 * @param flags packing options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int
cull_pack_elem_partial(sge_pack_buffer *pb, const lListElem *ep,
                       const lEnumeration *what, int flags) {
   DENTER(TOP_LAYER);

   int ret;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   if (ep->descr == nullptr) {
      DPRINTF("element descriptor nullptr not allowed !!!\n");
      ocs::TerminationManager::trigger_abort();
   }

   if ((ret = packint(pb, elem_kind_to_wire(elem_kind(ep)))) != PACK_SUCCESS) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

   if (elem_kind(ep) == FREE_ELEM) {
      if (what == nullptr) {
         if ((ret = cull_pack_descr(pb, ep->descr, flags)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
      } else {
         if ((ret = cull_pack_enum_as_descr(pb, what, ep->descr, flags)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
      }
   }

   ret = cull_pack_cont(pb, ep->cont, ep->descr, what, flags);

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(ret);
}

/**
 * @brief Read an element written by #cull_pack_elem
 *
 * @param pb the buffer to read from
 * @param[out] epp receives the element, owned by the caller
 * @param dp the expected object type, or nullptr to take it from the buffer
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_unpack_elem(
        sge_pack_buffer *pb,
        lListElem **epp,
        const lDescr *dp                  /* has to be nullptr in case of free elements
                                   must be the desriptor in case of bound elements */
) {
   DENTER(CULL_LAYER);

   int ret;

   ret = cull_unpack_elem_partial(pb, epp, dp, 0);
   DRETURN(ret);
}

/**
 * @brief Read an element written by #cull_pack_elem_partial
 *
 * A free element takes its object type from the buffer. With flags, a full type
 * there must be dp or a shorter version of it (N-W-6), else the element is
 * rejected as a format error.
 *
 * @param pb the buffer to read from
 * @param[out] epp receives the element, owned by the caller
 * @param dp the object type of a bound element; for a free element of flag-filtered data the expected one, or nullptr
 * @param flags unpacking options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_unpack_elem_partial(sge_pack_buffer *pb, lListElem **epp, const lDescr *dp, int flags) {
   return ocs_cull_unpack_elem(pb, epp, dp, dp != nullptr ? static_cast<uint32_t>(lCountPresent(dp)) : 0, flags);
}

/**
 * @brief Read an element, knowing how many values a bound one carries
 *
 * @param pb the buffer to read from
 * @param[out] epp receives the element, owned by the caller
 * @param dp the object type of a bound element; for a free element of flag-filtered data the expected one, or nullptr
 * @param wire_n the number of fields whose values follow for a bound element (N-W-6)
 * @param flags unpacking options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, #PACK_BADARG, or #PACK_FORMAT
 */
static int ocs_cull_unpack_elem(sge_pack_buffer *pb, lListElem **epp, const lDescr *dp, uint32_t wire_n, const int flags) {
   DENTER(CULL_LAYER);

   int ret;
   lListElem *ep = nullptr;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   *epp = nullptr;

   if ((ep = (lListElem *) calloc(1, sizeof(lListElem))) == nullptr) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_ENOMEM);
   }

   uint32_t wire;
   cull_elem_kind kind;
   if ((ret = unpackint(pb, &wire)) != PACK_SUCCESS) {
      sge_free(&ep);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }
   if (!elem_kind_from_wire(wire, &kind)) {
      sge_free(&ep);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_FORMAT);
   }

   /* An element that was bound or a sub-object at the sender is transitionally bound here
    * until it is chained in, so that lAppendElem does not reject it as bound. */
   if (kind == FREE_ELEM) {
      elem_set_free(ep);
   } else {
      elem_set_trans_bound(ep);
   }

   if (elem_kind(ep) == FREE_ELEM) {
      /* a free element holds the shared descriptor equal to the one on the wire */
      if ((ret = cull_unpack_descr(pb, &(ep->descr), flags, &wire_n, flags != 0 ? dp : nullptr)) != PACK_SUCCESS) {
         sge_free(&ep);
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }
   } else {
      /* 
         if it is not a free element we need 
         a descriptor from outside 
       */
      if ((ep->descr = dp) == nullptr) {
         sge_free(&ep);
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(PACK_BADARG);
      }
   }

   /* 
      here after setting status correctly we got 
      an element in a defined state - so we may
      call lFreeElem() in case of errors 
    */

   if ((ret = cull_unpack_cont(pb, &(ep->cont), ep->descr, wire_n, flags))) {
      if (elem_kind(ep) == FREE_ELEM || elem_kind(ep) == OBJECT_ELEM) {
         ocs_cull_descr_release(&(ep->descr));
      }
      sge_free(&ep);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

#ifdef OBSERVE
   lObserveAdd(ep, nullptr, false);
#endif

   *epp = ep;

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);
}

/* ================================================================ */

/**
 * @brief Pack a sub-object, including its descriptor
 *
 * A nested what reduces the sub-object the same way #cull_pack_list_partial
 * reduces a sublist: the descriptor is packed as the selection and only the
 * selected fields follow. The receiver takes the descriptor from the buffer, so
 * it builds the reduced object type without further information (CS-3001).
 *
 * @param pb the buffer to append to
 * @param ep the sub-object, or nullptr
 * @param what which fields to pack, or nullptr for all of them
 * @param flags packing options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
static int cull_pack_object(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, const int flags) {
   DENTER(CULL_LAYER);

   int ret;

   if ((ret = packint(pb, ep != nullptr)) != PACK_SUCCESS) {
      DRETURN(ret);
   }

   if (ep != nullptr) {
      /* pack descriptor */
      if (what == nullptr) {
         ret = cull_pack_descr(pb, ep->descr, flags);
      } else {
         ret = cull_pack_enum_as_descr(pb, what, ep->descr, flags);
      }
      if (ret != PACK_SUCCESS) {
         DRETURN(ret);
      }

      /* pack list element */
      if ((ret = cull_pack_elem_partial(pb, ep, what, flags)) != PACK_SUCCESS) {
         DRETURN(ret);
      }
   }

   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Pack a whole list, including its descriptor
 *
 * @param pb the buffer to append to
 * @param lp the list to pack
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_pack_list(sge_pack_buffer *pb, const lList *lp) {
   DENTER(CULL_LAYER);

   int ret;

   ret = cull_pack_list_partial(pb, lp, nullptr, 0);
   DRETURN(ret);
}

/**
 * @brief Pack the selected fields of every element of a list
 *
 * @param pb the buffer to append to
 * @param lp the list to pack
 * @param what which fields to pack, or nullptr for all of them
 * @param flags packing options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_pack_list_partial(sge_pack_buffer *pb, const lList *lp,
                           lEnumeration *what, int flags) {
   DENTER(CULL_LAYER);
   int ret;


   PROF_START_MEASUREMENT(SGE_PROF_PACKING);

   if (lp != nullptr && pb != nullptr) {
      if ((ret = packint(pb, 1)) != PACK_SUCCESS) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }

      if ((ret = packstr(pb, lp->listname)) != PACK_SUCCESS) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }

      /* pack descriptor */
      if (what == nullptr) {
         if ((ret = cull_pack_descr(pb, lp->descr, flags)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
      } else {
         if ((ret = cull_pack_enum_as_descr(pb, what, lp->descr, flags)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
      }
   } else {
      if ((ret = packint(pb, 0)) != PACK_SUCCESS) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }
   }

   if (lp != nullptr) {
      /* pack each list element */

      for_each_ep_lv(ep, lp) {
         // each element will be preceded by a 1 to indicate that another element follows
         if ((ret = packint(pb, 1)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }

         if ((ret = cull_pack_elem_partial(pb, ep, what, flags)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
      }

      // after the last element a 0 will be written to indicate that no more elements follow
      if ((ret = packint(pb, 0)) != PACK_SUCCESS) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Read a list written by #cull_pack_list
 *
 * @param pb the buffer to read from
 * @param[out] lpp receives the list, owned by the caller
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_unpack_list(sge_pack_buffer *pb, lList **lpp) {
   DENTER(CULL_LAYER);

   int ret;

   ret = cull_unpack_list_partial(pb, lpp, 0);
   DRETURN(ret);
}

/**
 * @brief Read a list written by #cull_pack_list_partial
 *
 * @param pb the buffer to read from
 * @param[out] lpp receives the list, owned by the caller
 * @param flags unpacking options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_unpack_list_partial(sge_pack_buffer *pb, lList **lpp, int flags) {
   DENTER(CULL_LAYER);

   int ret;
   lList *lp;
   lListElem *ep;

   uint32_t i = 0;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   *lpp = nullptr;

   if ((ret = unpackint(pb, &i))) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

   /* do we have an empty list (nullptr) ? */
   if (!i) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_SUCCESS);
   }
   CULL_STATS_CODE(const uint64_t stats_start = ocs::cull_stats::now_ns();)

   if ((lp = (lList *) calloc(1, sizeof(lList))) == nullptr) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_ENOMEM);
   }

   if ((ret = unpackstr(pb, &(lp->listname))) != PACK_SUCCESS) {
      lFreeList(&lp);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

   /* unpack descriptor; the list holds the shared descriptor equal to it */
   uint32_t wire_n = 0;
   if ((ret = cull_unpack_descr(pb, &(lp->descr), flags, &wire_n, nullptr)) != PACK_SUCCESS) {
      lFreeList(&lp);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

   /* one hash slot per hashed field; a table is built by the first lookup on its field */
   if (!cull_hash_init_slots(lp, true)) {
      lFreeList(&lp);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_ENOMEM);
   }

#ifdef OBSERVE
   lObserveAdd(lp, nullptr, true);
#endif

   /* unpack each list element */
   uint32_t has_more_elements = 0;
   // as long as we find a 1 we have to unpack an element
   if ((ret = unpackint(pb, &has_more_elements)) != PACK_SUCCESS) {
      lFreeList(&lp);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }
   while (has_more_elements) {
      if ((ret = ocs_cull_unpack_elem(pb, &ep, lp->descr, wire_n, flags)) != PACK_SUCCESS) {
         lFreeList(&lp);
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }
      lAppendElem(lp, ep);
      // as long as we find a 1 we have to unpack an element
      if ((ret = unpackint(pb, &has_more_elements)) != PACK_SUCCESS) {
         lFreeList(&lp);
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(ret);
      }
   }

   CULL_STATS_CODE(
      const uint64_t stats_end = ocs::cull_stats::now_ns();
      CULL_STATS_TYPE(lp->descr->nm, UNPACK_LISTS, 1);
      CULL_STATS_TYPE(lp->descr->nm, UNPACK_ELEMS, lGetNumberOfElem(lp));
      CULL_STATS_TYPE(lp->descr->nm, UNPACK_NS, stats_end - stats_start);
   )

   *lpp = lp;

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);
}

/* ------------------------------------------------------------

   cull_unpack_object() - unpacks a sub object 

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT

 */

static int cull_unpack_object(
        sge_pack_buffer *pb,
        lListElem **epp,
        int flags
) {
   DENTER(CULL_LAYER);

   int ret;
   const lDescr *descr;
   lListElem *ep;
   uint32_t i = 0;

   *epp = nullptr;

   if ((ret = unpackint(pb, &i))) {
      DRETURN(ret);
   }

   /* do we have an empty object (nullptr) ? */
   if (!i) {
      DRETURN(PACK_SUCCESS);
   }

   /* unpack descriptor */
   uint32_t wire_n = 0;
   if ((ret = cull_unpack_descr(pb, &descr, flags, &wire_n, nullptr)) != PACK_SUCCESS) {
      DRETURN(ret);
   }

   /* unpack each element */
   if ((ret = ocs_cull_unpack_elem(pb, &ep, descr, wire_n, flags)) != PACK_SUCCESS) {
      ocs_cull_descr_release(&descr);
      DRETURN(ret);
   }

   /* the sub-object takes over the reference on the descriptor it borrowed while unpacking */
   if (ep->descr != descr) {
      ocs_cull_descr_release(&descr);
   }
   elem_set_object(ep);

#ifdef OBSERVE
   lObserveAdd(ep, nullptr, false);
#endif

   *epp = ep;

   DRETURN(PACK_SUCCESS);
}

/// How a field selection is encoded in a packbuffer
enum {
   PackWhatAll = 0,/**< every field: the selection was #lWhatAll, no array follows */PackWhatNone,/**< no field at all, no array follows */PackWhatArray/**< an explicit list of fields follows */
};

/**
 * @brief Pack a field selection
 *
 * @param pb the buffer to append to
 * @param enp the selection to pack
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_pack_enum(
        sge_pack_buffer *pb,
        const lEnumeration *enp
) {
   DENTER(CULL_LAYER);

   int ret;
   uint32_t flag = 0;
   int i = 0, n = 0;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   if ((ret = packint(pb, enp != nullptr)))
      goto error;

   if (!enp) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_SUCCESS);
   }

   /* pack flag indicating ALL, NONE or normal enumeration (array) */
   switch (enp[0].pos) {
      case WHAT_ALL:
         flag = PackWhatAll;
         break;
      case WHAT_NONE:
         flag = PackWhatNone;
         break;
      default:
         flag = PackWhatArray;
   }

   if ((ret = packint(pb, flag)))
      goto error;

   if (flag == PackWhatArray) {
      /* compute and pack number of lEnumeration fields (without end mark) */
      for (n = 0; enp[n].nm != NoName; n++);
      if ((ret = packint(pb, n)))
         goto error;

      /* pack the lEnumeration fields */
      for (i = 0; mt_get_type(enp[i].nm) != NoName && mt_get_type(enp[i].mt) != lEndT; i++) {
         if ((ret = packint(pb, enp[i].pos)))
            goto error;
         if ((ret = packint(pb, enp[i].mt)))
            goto error;
         if ((ret = packint(pb, enp[i].nm)))
            goto error;
         if (enp[i].ep == nullptr) {
            if ((ret = packint(pb, 0))) {
               goto error;
            }
         } else {
            if ((ret = packint(pb, 1))) {
               goto error;
            }
            if ((ret = cull_pack_enum(pb, enp[i].ep))) {
               goto error;
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);

   error:
   DPRINTF("error packing enumeration\n");
   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(ret);
}

/**
 * @brief Read a field selection written by #cull_pack_enum
 *
 * @param pb the buffer to read from
 * @param[out] enpp receives the selection, owned by the caller
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_unpack_enum(
        sge_pack_buffer *pb,
        lEnumeration **enpp
) {
   DENTER(CULL_LAYER);

   int ret;
   lEnumeration *enp = nullptr;
   uint32_t flag = 0, i = 0, temp = 0;
   uint32_t n = 0;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   *enpp = nullptr;

   if ((ret = unpackint(pb, &i)))
      goto error;

   if (!i) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_SUCCESS);
   }

   if ((ret = unpackint(pb, &flag)))
      goto error;

   if (flag != PackWhatArray) {
      if (!(enp = (lEnumeration *) calloc(2, sizeof(lEnumeration)))) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(PACK_ENOMEM);
      }

      switch (flag) {
         case PackWhatAll:
            enp[0].pos = WHAT_ALL;
            break;

         case PackWhatNone:
            enp[0].pos = WHAT_NONE;
            break;

         default:
            break;
      }
      enp[0].nm = -99;
      enp[0].mt = -99;
      enp[0].ep = nullptr;
      enp[1].nm = NoName;
      enp[1].mt = lEndT;
      enp[1].ep = nullptr;
   } else {
      /* read in number of lEnumeration fields (without end mark) */
      if ((ret = unpackint(pb, &n)))
         goto error;

      if (n + 1 > MAX_DESCR_SIZE) {
         LERROR(LEMALLOC);
         DRETURN(PACK_ENOMEM);
      }

      if (!(enp = (lEnumeration *) calloc(n + 1, sizeof(lEnumeration)))) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(PACK_ENOMEM);
      }

      /* read in n lEnumeration fields */
      for (i = 0; i < n; i++) {
         lEnumeration *tmp2 = nullptr;

         if ((ret = unpackint(pb, &temp)))
            goto error;
         enp[i].pos = temp;
         if ((ret = unpackint(pb, &temp)))
            goto error;
         enp[i].mt = temp;
         if ((ret = unpackint(pb, &temp)))
            goto error;
         enp[i].nm = temp;
         /* the position, the type and the flags of a field are taken from the local type, not from the
          * sender (N-W-5, N-W-11) */
         {
            int local_pos = 0;
            const lDescr *type = ocs_cull_descr_static_containing(enp[i].nm, &local_pos);
            if (type != nullptr) {
               enp[i].pos = local_pos;
               enp[i].mt = type[local_pos].mt;
            }
         }
         if ((ret = unpackint(pb, &temp)))
            goto error;
         if (temp == 1) {
            if ((ret = cull_unpack_enum(pb, &tmp2))) {
               goto error;
            }
         }
         enp[i].ep = tmp2;
         tmp2 = nullptr;
      }

      enp[n].nm = NoName;
      enp[n].mt = lEndT;
      enp[n].ep = nullptr;
   }

   *enpp = enp;

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);

   error:

   lFreeWhat(&enp);
   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(ret);
}

/**
 * @brief Pack a selection condition
 *
 * @param pb the buffer to append to
 * @param cp the condition to pack
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_pack_cond(
        sge_pack_buffer *pb,
        const lCondition *cp
) {
   DENTER(CULL_LAYER);

   int ret;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   if ((ret = packint(pb, cp != nullptr))) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

   if (!cp) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_SUCCESS);
   }

   /* pack operator indicating the following contents */
   if ((ret = packint(pb, cp->op))) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }

   DPRINTF("cp->op: %d\n", cp->op);

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
         DPRINTF("cp->operand.cmp.pos: %d\n", cp->operand.cmp.pos);
         if ((ret = packint(pb, cp->operand.cmp.pos))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         DPRINTF("cp->operand.cmp.mt: %d\n", cp->operand.cmp.mt);
         if ((ret = packint(pb, cp->operand.cmp.mt))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         DPRINTF("cp->operand.cmp.nm: %d\n", cp->operand.cmp.nm);
         if ((ret = packint(pb, cp->operand.cmp.nm))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }

         if (mt_get_type(cp->operand.cmp.mt) != lListT) {
            if ((ret = cull_pack_switch(pb, &(cp->operand.cmp.val), nullptr,
                                        mt_get_type(cp->operand.cmp.mt), 0))) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
         } else {
            if ((ret = cull_pack_cond(pb, cp->operand.cmp.val.cp))) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
         }
         break;

      case NEG:
         if ((ret = cull_pack_cond(pb, cp->operand.log.first))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         break;

      case AND:
      case OR:
         if ((ret = cull_pack_cond(pb, cp->operand.log.first))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         if ((ret = cull_pack_cond(pb, cp->operand.log.second))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         break;

      default:
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(PACK_FORMAT);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Read a selection condition written by #cull_pack_cond
 *
 * @param pb the buffer to read from
 * @param[out] cpp receives the condition, owned by the caller
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 */
int cull_unpack_cond(
        sge_pack_buffer *pb,
        lCondition **cpp
) {
   DENTER(CULL_LAYER);

   int ret;
   uint32_t i = 0;
   lCondition *cp = nullptr;

   PROF_START_MEASUREMENT(SGE_PROF_PACKING);
   *cpp = nullptr;
   if ((ret = unpackint(pb, &i))) {
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }
   if (!i) {
      *cpp = nullptr;
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(0);
   }

   if (!(cp = (lCondition *) calloc(1, sizeof(lCondition)))) {
      LERROR(LEMALLOC);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(PACK_ENOMEM);
   }

   if ((ret = unpackint(pb, &i))) {
      lFreeWhere(&cp);
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
      DRETURN(ret);
   }
   cp->op = i;

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
         if ((ret = unpackint(pb, &i))) {
            lFreeWhere(&cp);
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         cp->operand.cmp.pos = i;
         if ((ret = unpackint(pb, &i))) {
            lFreeWhere(&cp);
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         cp->operand.cmp.mt = i;
         if ((ret = unpackint(pb, &i))) {
            lFreeWhere(&cp);
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         cp->operand.cmp.nm = i;

         if (mt_get_type(cp->operand.cmp.mt) != lListT) {
            if ((ret = cull_unpack_switch(pb, &(cp->operand.cmp.val), mt_get_type(cp->operand.cmp.mt), 0))) {
               lFreeWhere(&cp);
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
         } else {
            if ((ret = cull_unpack_cond(pb, &(cp->operand.cmp.val.cp)))) {
               lFreeWhere(&cp);
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }
         }
         break;

      case NEG:
         if ((ret = cull_unpack_cond(pb, &(cp->operand.log.first)))) {
            lFreeWhere(&cp);
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         break;

      case AND:
      case OR:
         if ((ret = cull_unpack_cond(pb, &(cp->operand.log.first)))) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            lFreeWhere(&cp);
            DRETURN(ret);
         }
         if ((ret = cull_unpack_cond(pb, &(cp->operand.log.second)))) {
            lFreeWhere(&cp);
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }
         break;

      default:
         lFreeWhere(&cp);
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(PACK_FORMAT);
   }

   *cpp = cp;

   PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   DRETURN(PACK_SUCCESS);
}

/**
 * @brief Takes a byte array, transformes it into ASCII and sets
 *
 * makes a string out of a byte array and sets that string into an element
 *
 * @param byteArray byte array
 * @param size size of the byte array
 * @param elem target element
 * @param name target attribute
 *
 */
void setByteArray(const char *byteArray, int size, lListElem *elem, int name) {
   const char *numbers = {"0123456789ABCDEF"};
   int lower_part;
   int upper_part;
   int target_size = size * 2 + 1;
   char *z_stream_str = nullptr;
   int i = 0;
   int y = 0;

   if (!byteArray || !elem)
      return;

   z_stream_str = sge_malloc(target_size);
   memset(z_stream_str, 0, target_size);

   for (i = 0; i < size; i++) {
      lower_part = (byteArray[i] & 0x0F);
      upper_part = (byteArray[i] & 0xF0) >> 4;
      z_stream_str[y++] = numbers[lower_part];
      z_stream_str[y++] = numbers[upper_part];
   }
   z_stream_str[y++] = '\0';
   lSetString(elem, name, z_stream_str);
   sge_free(&z_stream_str);
}

/** @brief Transform the hex string stored in attribute @p name into a byte array.
 *
 * Extracts the string attribute from @p elem and decodes it into a freshly
 * allocated byte array (`*byte`, which the caller must free). The string
 * attribute may be unset, in which case `lGetString()` returns NULL; that is
 * treated as "no data" and 0 is returned rather than dereferencing NULL
 * (CWE-476, CS-2341) — callers already handle a non-positive size as invalid
 * pack data. Valid input is decoded unchanged.
 *
 * @param byte  out: newly allocated byte array (caller frees); set on success
 * @param elem  source element holding the hex string
 * @param name  attribute id of the string to decode
 * @return number of bytes (>= 0) on success; 0 if the attribute is unset/empty;
 *         a negative value (the position of the first non-hex character) on a
 *         malformed string
 */
int getByteArray(char **byte, const lListElem *elem, int name) {
   const char *numbers = {"0123456789ABCDEF"};
   int lower_part = 0;
   int upper_part = 0;
   int i = 0;
   int size = 0;
   const char *string;
   if (!byte || !elem) {
      return size;
   }

   string = lGetString(elem, name);
   if (string == nullptr) {
      // unset string attribute: return 0 so callers treat it as invalid pack
      // data instead of dereferencing NULL in strlen() (CWE-476, CS-2341)
      return 0;
   }
   size = strlen(string) / 2;
   *byte = sge_malloc(size);
   memset(*byte, 0, size);

   for (i = 0; i < size; i++) {
      int a = 0;

      for (a = 0; a < 16; a++) {
         if (numbers[a] == string[i * 2]) {
            lower_part = a;
            break;
         }
      }
      if (a == 16) {
         return (i * -2);
      }

      for (a = 0; a < 16; a++) {
         if (numbers[a] == string[i * 2 + 1]) {
            upper_part = a;
            break;
         }
      }
      if (a == 16) {
         return ((i * -2) - 1);
      }
      (*byte)[i] = (upper_part << 4) + lower_part;
   }

   return size;
}

/**
 * @brief Pack a job to be sent to execd
 *
 * This function is used in qmaster and by qrsh -inherit to deliver
 * jobs to execd's.
 *
 * @param pb packing buffer
 * @param jep JB_Type
 *
 * @return PACK_SUCCESS on success
 *
 * @note MT-NOTE: pack_job_delivery() is MT safe
 */
int pack_job_delivery(sge_pack_buffer *pb, lListElem *jep) {
   int ret;

   if ((ret=cull_pack_elem(pb, jep)) != PACK_SUCCESS) {
      return ret;
   }
   return PACK_SUCCESS;
}
