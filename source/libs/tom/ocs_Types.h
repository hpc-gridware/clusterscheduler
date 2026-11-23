#pragma once

#include <cstdio>

#include "uti/sge_dstring.h"
#include "tom/ocs_PackBuffer.h"
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
 * @brief The storage layer of the typed object model: types, field types and flags, descriptor layout
 *
 * Moved from cull/cull_list.h into the core by CS-3088. The structure names (lList, lListElem, lDescr, ...)
 * and the cull_descr_* names still carry the prefix of the old library; CS-3197 renames them.
 */

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "tom/detail/ocs_HashP.h"

#define NoName -1 ///< field name meaning "no field"; terminates a descriptor

typedef struct _lDescr lDescr;             ///< one field of an object type; see @ref _lDescr
typedef struct _lList lList;               ///< a list of elements; see @ref _lList
typedef struct _lListElem lListElem;       ///< one element, in a list or on its own; see @ref _lListElem
typedef struct _lCondition lCondition;     ///< a selection condition, built with `where()` or `where_compare()`
typedef struct _lEnumeration lEnumeration; ///< a field selection, built with `what<...>()` or `what_of()`
typedef union _lMultiType lMultiType;      ///< the value of one field, of any field type
typedef struct _lSortOrder lSortOrder;     ///< a sort specification, built with `make_sort_order()`

/**
 * @brief The data type of a cull field
 *
 * @note If you change this enum, change `multitypes[]` in ocs_ElementCore.cc
 *       to match — the array is indexed by these values.
 */
enum _enum_lMultiType {
   MT_END,     ///< marks the end of a descriptor; not a real type
   MT_DOUBLE,  ///< a double precision float
   MT_UINT32,  ///< a 32 bit unsigned value
   MT_INT64,   ///< a signed long
   MT_BOOL,    ///< a boolean
   MT_INT32,   ///< an int; not supported at GDI level
   MT_STRING,  ///< a string, owned by the element
   MT_LIST,    ///< a sub-list, @ref lList
   MT_OBJECT,  ///< a single sub-object, an lListElem
   MT_REF,     ///< an opaque pointer; never spooled
   MT_HOST,    ///< a host name, owned by the element
   MT_UINT64   ///< a 64 bit unsigned value
};

/// Name of each @ref _enum_lMultiType, indexed by the enumerator; used in error messages and dumps
extern const char *multitypes[];

/**
 * @addtogroup mt_field_attributes
 * @{
 *
 * The flags of a field, in the bits of its mt word above the type byte (#mt_get_type()), without gaps. The
 * field flags come first, in the order of ocs::tom::Flag, which takes its values from them; the two bits after
 * them are kept by the list core itself. 9.2 may renumber them: the mt word is packed and spooled, and lines of
 * different minor releases do not talk to each other.
 */
inline constexpr int MT_FLAG_SHIFT = 8;                                   ///< bit of the first flag, above the type byte
inline constexpr int MT_DEFAULT = 0;                                      ///< no flag
inline constexpr int MT_PRIMARY_KEY = 1 << (MT_FLAG_SHIFT + 0);           ///< part of the primary key; implies neither uniqueness nor hashing
inline constexpr int MT_HASH = 1 << (MT_FLAG_SHIFT + 1);                  ///< lookups by this field use a hash table; non unique unless #MT_UNIQUE is also given
inline constexpr int MT_UNIQUE = 1 << (MT_FLAG_SHIFT + 2);                ///< the value is unique across a list
inline constexpr int MT_NO_TRANSFER = 1 << (MT_FLAG_SHIFT + 3);           ///< the value stays inside the process; neither packed nor sent in events
inline constexpr int MT_CONFIGURE = 1 << (MT_FLAG_SHIFT + 4);             ///< the field may be changed by configuration functions; not yet implemented
inline constexpr int MT_SPOOL = 1 << (MT_FLAG_SHIFT + 5);                 ///< the field is written when the object is spooled
inline constexpr int MT_SPOOL_SUBLIST = 1 << (MT_FLAG_SHIFT + 6);         ///< spooled also where the type is the sub-type of another one
inline constexpr int MT_SPOOL_PROJECT = 1 << (MT_FLAG_SHIFT + 7);         ///< the field is spooled with a project
inline constexpr int MT_FIELD_FLAGS = (1 << (MT_FLAG_SHIFT + 8)) - (1 << MT_FLAG_SHIFT);   ///< the bits of the field flags
inline constexpr int MT_DESCR_INTERNED = 1 << (MT_FLAG_SHIFT + 8);        ///< entry 0 of a reference counted descriptor (tom/ocs_Descr.h); process local, never packed
inline constexpr int MT_IS_REDUCED = 1 << (MT_FLAG_SHIFT + 9);            ///< the descriptor holds only part of the type's fields
/** @} */
static_assert(MT_IS_REDUCED < (1 << 30), "the flags fit into the mt word");

#define BASIC_UNIT 50 ///< growth unit of a descriptor; don't touch
#define MAX_DESCR_SIZE  (4*BASIC_UNIT) ///< largest number of fields an object type may have

/*
 * An object type is a static descriptor block: a cull_descr_header followed by the
 * fields and the terminating #MT_END entry, built at compile time by
 * ocs::tom::descr_make() as an inline variable, so every translation unit sees the
 * same block and its contents are constant expressions. The descriptor of a type
 * points at its first field. The product types are generated (sgeobj/tom); the
 * types the module tests define by hand use the macros of test/common/ocs_test_listdef.h.
 */

/* 
 * For lists, objects and references the type of the subordinate object(s) 
 * must be specified.
 * If multiple types are thinkable or non cull data types are referenced,
 * use the following define CULL_ANY_SUBTYPE as type
 */
#define CULL_ANY_SUBTYPE 0

/**
 * @brief One field of an object type
 *
 * An object type is described by an array of these, terminated by a field
 * whose type is #MT_END. The array is what `LISTDEF`/`LISTEND` build.
 */
struct _lDescr {
   int nm;         ///< the field number, or #NoName in the terminating entry
   int mt;         ///< the field's type and attributes: a @ref _enum_lMultiType or-ed with @ref mt_field_attributes flags
   int off;        ///< the field's value slot relative to its index (slot = index + off), or #OCS_CULL_ABSENT; 0 in a full descriptor (CS-439)
};
static_assert(sizeof(lDescr) == 12, "a descriptor entry is field number, type and slot offset without padding");

/** @brief Slot offset of a field that a reduced descriptor does not hold (CS-439, N-B-1) */
constexpr int OCS_CULL_ABSENT = INT32_MIN;

/**
 * @brief Header in front of the fields of a static or counted descriptor
 *
 * The fields follow the header at a fixed offset (see #cull_descr_block), so
 * the header of a descriptor can be found from its first field. A counted
 * descriptor with absent fields (np < n) holds behind its terminating entry the
 * reverse map: one `int16_t` per slot, the position of the field held there
 * (CS-439, N-B-6).
 */
struct cull_descr_header {
   std::atomic<uint32_t> refs; ///< reference count; 0 for a static block, which is never released
   uint32_t n;                 ///< number of fields, without the terminating #MT_END entry
   uint32_t np;                ///< number of fields that have a value, the slots of an element
   uint64_t hash;              ///< FNV-1a over field numbers, types and slot offsets, see ocs::tom::descr_hash()
};
static_assert(sizeof(cull_descr_header) == 24, "the fields must start 24 bytes after the header");

/**
 * @brief A descriptor as one block: header, fields, terminating #MT_END entry
 *
 * @tparam N number of entries including the terminating one
 */
template<size_t N>
struct cull_descr_block {
   cull_descr_header h; ///< header of the block
   lDescr e[N];         ///< the fields, the last one is {#NoName, #MT_END}
};

namespace ocs::tom {
   /**
    * @brief FNV-1a hash over the field numbers, types and slot offsets of a descriptor
    *
    * The process-local flag #MT_DESCR_INTERNED is not hashed, so a counted copy
    * hashes like its source.
    *
    * @param e the fields
    * @param n number of fields to hash
    * @return the hash
    */
   constexpr uint64_t descr_hash(const lDescr *e, const size_t n) {
      uint64_t h = 0xcbf29ce484222325ULL;
      for (size_t i = 0; i < n; i++) {
         for (const uint32_t v : {static_cast<uint32_t>(e[i].nm), static_cast<uint32_t>(e[i].mt & ~MT_DESCR_INTERNED),
                                  static_cast<uint32_t>(e[i].off)}) {
            for (int b = 0; b < 4; b++) {
               h ^= (v >> (8 * b)) & 0xff;
               h *= 0x100000001b3ULL;
            }
         }
      }
      return h;
   }

   /**
    * @brief Builds a static descriptor block from the fields, element by element
    *
    * @tparam N number of entries including the terminating one
    * @tparam I indices 0 .. N-1
    * @param e the fields, terminated by {#NoName, #MT_END}
    * @return the block with header and fields
    */
   template<size_t N, size_t... I>
   consteval cull_descr_block<N> descr_make_impl(const lDescr (&e)[N], std::index_sequence<I...>) {
      return {{0, N - 1, N - 1, ocs::tom::descr_hash(e, N - 1)}, {e[I]...}};
   }

   /**
    * @brief Builds the static descriptor block of an object type at compile time
    *
    * Used by #LISTDEF/#LISTEND; N is deduced from the braced field list.
    *
    * @tparam N number of entries including the terminating one
    * @param e the fields, terminated by {#NoName, #MT_END}
    * @return the block with header and fields
    */
   template<size_t N>
   consteval cull_descr_block<N> descr_make(const lDescr (&e)[N]) {
      static_assert(N >= 1, "a descriptor needs at least its terminating entry");
      return ocs::tom::descr_make_impl(e, std::make_index_sequence<N>{});
   }
} // namespace ocs::tom

#define mt_get_type(mt) ((mt) & 0x000000FF) ///< the @ref _enum_lMultiType out of a field's flags
#define mt_do_hashing(mt) (((mt) & MT_HASH) ? true : false) ///< is #MT_HASH set on this field?
#define mt_is_unique(mt) (((mt) & MT_UNIQUE) ? true : false) ///< is #MT_UNIQUE set on this field?
#define mt_do_transfer(mt) (((mt) & MT_NO_TRANSFER) ? false : true) ///< may the value of this field leave the process? (#MT_NO_TRANSFER not set)

#define WHAT_ALL                        -1 ///< _lEnumeration::pos value meaning "every field of the type"
#define WHAT_NONE                       -2 ///< _lEnumeration::pos value meaning "no field at all"

