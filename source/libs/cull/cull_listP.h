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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Internal layout of a cull list and its elements
 */

#include <cassert>
#include <cstdint>

#include "cull/cull_list.h"

/**
 * @defgroup cull_elem_status Element state
 * @brief The state of an element, held in the two low bits of _lListElem::owner
 *
 * The state records who owns an element, which decides whether it may be
 * appended to a list and who has to free it. A bound element also carries the
 * list that holds it, in the upper bits of the same word.
 *
 * _lListElem::owner is one word with two parts that are valid at the same time
 * (not a union, where only one interpretation holds at a time):
 *
 * @code
 *  63                                       2 1 0
 * +------------------------------------------+---+
 * | address of the list, without its 2 bits  | S |   S = state, 0..3
 * +------------------------------------------+---+
 *   owner & ~CULL_ELEM_KIND_MASK -> the list    owner & CULL_ELEM_KIND_MASK -> the state
 * @endcode
 *
 * The two low bits are free because every list is at least 4-byte aligned, so
 * the two lowest bits of its address are always 0 (checked by a static_assert
 * below). Only a bound element uses the upper part; in every other state it is
 * 0. Two separate fields, a pointer and a byte, would grow the element from 40
 * to 48 bytes (64 bytes per malloc chunk instead of 48), which is why one word
 * carries both.
 *
 * Read and write the word only through elem_kind(), elem_list() and the
 * elem_set_*() functions; never access the bits directly.
 * @{
 */

/// the state of an element
enum cull_elem_kind : uintptr_t {
   FREE_ELEM = 0,        ///< not part of a list and not a sub-object; the holder must free it
   BOUND_ELEM = 1,       ///< contained in a list, which owns it and whose address the owner word carries
   OBJECT_ELEM = 2,      ///< a sub-object of another element's field
   TRANS_BOUND_ELEM = 3  ///< temporary state while unpacking; bound elements and sub-objects carry it afterwards so that functions such as `lAppendElem()`, which reject bound objects, do not fail
};

/// the bits of _lListElem::owner that hold the state
constexpr uintptr_t CULL_ELEM_KIND_MASK = 3;

/* the state as the pack format carries it, the former status values */
constexpr uint32_t CULL_PACK_FREE_ELEM = 1 << 0;        ///< FREE_ELEM on the wire
constexpr uint32_t CULL_PACK_BOUND_ELEM = 1 << 1;       ///< BOUND_ELEM on the wire
constexpr uint32_t CULL_PACK_TRANS_BOUND_ELEM = 1 << 2; ///< TRANS_BOUND_ELEM on the wire
constexpr uint32_t CULL_PACK_OBJECT_ELEM = 1 << 3;      ///< OBJECT_ELEM on the wire
/** @} */

/**
 * @brief One element of a cull list
 *
 * An element carries its own descriptor, so it can exist outside any list —
 * `lCreateElem()` produces exactly that. Its field values live in a separate
 * array indexed the same way as the descriptor.
 */
struct _lListElem {
   lListElem *next;   ///< next element, or nullptr at the end of the list
   lListElem *prev;   ///< previous element, or nullptr at the head
   uintptr_t owner;   ///< two parts in one word: the state in the two low bits, the list of a bound element in the upper bits (see @ref cull_elem_status)
   lDescr *descr;     ///< the object type, as an array of field descriptors
   lMultiType *cont;  ///< the field values, indexed the same way as descr
};

/**
 * @brief A cull list: a doubly linked list of elements of one object type
 *
 * Every element shares the list's descriptor, so a list is homogeneous. The
 * element count is kept rather than walked, and both ends are held so
 * appending is O(1).
 */
struct _lList {
   uint32_t nelem;    ///< number of elements currently in the list
   char *listname;    ///< name of the list, used in dumps and error messages
   lDescr *descr;     ///< the object type every element has
   lListElem *first;  ///< first element, or nullptr when the list is empty
   lListElem *last;   ///< last element, or nullptr when the list is empty
};

// The owner word replaces the former 4-byte status and the 4 bytes of padding
// behind it, so the element keeps its five words.
static_assert(sizeof(lListElem) == 40, "the owner tag must not grow the element");
// The state lives in the two low bits of a list address, which are 0 only if
// every list is at least 4-byte aligned.
static_assert(alignof(lList) >= 4 && alignof(lListElem) >= 4,
              "the two low bits of a list address must be free for the element state");

/**
 * @brief The state of an element
 *
 * Masks out the list part of the owner word and keeps the two state bits.
 *
 * @param ep the element, not nullptr
 * @return the state held in the low bits of its owner word
 */
inline cull_elem_kind elem_kind(const lListElem *ep) {
   return static_cast<cull_elem_kind>(ep->owner & CULL_ELEM_KIND_MASK);
}

/**
 * @brief The list that holds an element
 *
 * Masks out the two state bits of the owner word; what remains is the address
 * of the list exactly as elem_set_bound() stored it. In every state but
 * BOUND_ELEM the upper part is 0, so nullptr is returned without masking.
 *
 * @param ep the element, not nullptr
 * @return the list for a bound element, nullptr in every other state
 */
inline lList *elem_list(const lListElem *ep) {
   if (elem_kind(ep) != BOUND_ELEM) {
      return nullptr;
   }
   return reinterpret_cast<lList *>(ep->owner & ~CULL_ELEM_KIND_MASK);
}

/**
 * @brief Mark an element as free: in no list and no field
 *
 * Writes the whole owner word, not only the state bits: the list part becomes
 * 0, so a dechained element keeps no stale pointer to the list it left.
 *
 * @param ep the element, not nullptr
 */
inline void elem_set_free(lListElem *ep) {
   ep->owner = FREE_ELEM;
}

/**
 * @brief Mark an element as bound into a list
 *
 * The only setter that fills both parts of the owner word: the address of the
 * list in the upper bits, BOUND_ELEM in the two low bits. The address must have
 * its two low bits free; the alignment of lList guarantees that, the assert
 * checks it in debug builds.
 *
 * @param ep the element, not nullptr
 * @param lp the list that now holds the element, not nullptr
 */
inline void elem_set_bound(lListElem *ep, const lList *lp) {
   const auto address = reinterpret_cast<uintptr_t>(lp);
   assert((address & CULL_ELEM_KIND_MASK) == 0);
   ep->owner = address | BOUND_ELEM;
}

/**
 * @brief Mark an element as the sub-object in a field of another element
 *
 * Writes the whole owner word: the list part becomes 0. A sub-object does not
 * record its parent, hash maintenance needs only the list of a bound element.
 *
 * @param ep the element, not nullptr
 */
inline void elem_set_object(lListElem *ep) {
   ep->owner = OBJECT_ELEM;
}

/**
 * @brief Mark an unpacked element as transitionally bound until it is chained in
 *
 * Writes the whole owner word: the list part becomes 0. The element was bound
 * or a sub-object at the sender; the list it will be chained into is not known
 * yet, lAppendElem() or lInsertElem() bind it then.
 *
 * @param ep the element, not nullptr
 */
inline void elem_set_trans_bound(lListElem *ep) {
   ep->owner = TRANS_BOUND_ELEM;
}

/**
 * @brief The value the pack format carries for a state
 *
 * @param kind the state
 * @return the former status value of that state
 */
inline uint32_t elem_kind_to_wire(const cull_elem_kind kind) {
   switch (kind) {
      case BOUND_ELEM:
         return CULL_PACK_BOUND_ELEM;
      case OBJECT_ELEM:
         return CULL_PACK_OBJECT_ELEM;
      case TRANS_BOUND_ELEM:
         return CULL_PACK_TRANS_BOUND_ELEM;
      case FREE_ELEM:
      default:
         return CULL_PACK_FREE_ELEM;
   }
}

/**
 * @brief The state for a value read from the pack format
 *
 * @param wire the packed status value
 * @param kind receives the state
 * @return false if the value names no state
 */
inline bool elem_kind_from_wire(const uint32_t wire, cull_elem_kind *kind) {
   switch (wire) {
      case CULL_PACK_FREE_ELEM:
         *kind = FREE_ELEM;
         return true;
      case CULL_PACK_BOUND_ELEM:
         *kind = BOUND_ELEM;
         return true;
      case CULL_PACK_TRANS_BOUND_ELEM:
         *kind = TRANS_BOUND_ELEM;
         return true;
      case CULL_PACK_OBJECT_ELEM:
         *kind = OBJECT_ELEM;
         return true;
      default:
         return false;
   }
}
