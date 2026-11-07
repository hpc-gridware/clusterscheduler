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

#include <atomic>
#include <cassert>
#include <cstdint>

#include "cull/cull_list.h"
#include "cull/cull_hashP.h"

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
/// first word of a descriptor packed as a reference to its type (N-W-3); no field count can take it
constexpr uint32_t CULL_PACK_DESCR_TYPE_REF = 0xFFFFFF00;
/// first word of a descriptor that enters the descriptor table of the buffer (N-W-4)
constexpr uint32_t CULL_PACK_DESCR_TABLE_DEF = 0xFFFFFF01;
/// first word of a descriptor given as the index of an earlier table entry (N-W-4)
constexpr uint32_t CULL_PACK_DESCR_TABLE_REF = 0xFFFFFF02;
/// a table entry lists the indices of its fields in one type; mt comes from the receiver's type (N-W-5)
constexpr uint32_t CULL_PACK_DESCR_BY_INDEX = 0;
/// a table entry lists its fields as (nm, mt), for descriptors that span several types
constexpr uint32_t CULL_PACK_DESCR_BY_FIELD = 1;
static_assert(CULL_PACK_DESCR_TYPE_REF > MAX_DESCR_SIZE, "the type reference marker must not be a field count");
/** @} */

/**
 * @brief One element of a cull list
 *
 * An element carries its own descriptor, so it can exist outside any list —
 * `lCreateElem()` produces exactly that. Its field values live in a separate
 * array; ocs_cull_value() in cull_multitypeP.h gives the value of a descriptor
 * position.
 */
struct _lListElem {
   lListElem *next;   ///< next element, or nullptr at the end of the list
   lListElem *prev;   ///< previous element, or nullptr at the head
   uintptr_t owner;   ///< two parts in one word: the state in the two low bits, the list of a bound element in the upper bits (see @ref cull_elem_status)
   const lDescr *descr; ///< the object type, shared: a static block or a counted descriptor (see ocs_cull_descr.h)
   lMultiType *cont;  ///< the field values, at the slots ocs_cull_slot() gives for the descriptor positions
};

/**
 * @brief The value slot of the field at a descriptor position (CS-439)
 *
 * The slot is the position plus the entry's slot offset. Every single field
 * access to `cont` goes through here, so a descriptor whose positions and
 * slots differ needs no change at the call sites.
 *
 * @param dp the descriptor
 * @param pos a valid field position in @p dp
 * @return the index into `cont` that holds the field's value
 */
inline int ocs_cull_slot(const lDescr *dp, const int pos) {
   return pos + dp[pos].off;
}

/**
 * @brief A field of a descriptor that has a value: its position and its slot (CS-439)
 */
struct ocs_cull_present_field {
   int pos;  ///< the field position in the descriptor
   int slot; ///< the index into `cont` that holds the field's value
};

/**
 * @brief The header of a static or counted descriptor (CS-439)
 * @param dp the entries of a static or counted descriptor; a plain array has no header
 * @return its header
 */
inline const cull_descr_header *ocs_cull_descr_header_of(const lDescr *dp) {
   return reinterpret_cast<const cull_descr_header *>(dp) - 1;
}

/**
 * @brief The reverse map of a counted descriptor with absent fields (CS-439, N-B-6)
 * @param dp the entries of a counted descriptor whose header has np < n
 * @return the field position per slot, np entries behind the terminating entry
 */
inline const int16_t *ocs_cull_descr_rmap(const lDescr *dp) {
   return reinterpret_cast<const int16_t *>(dp + ocs_cull_descr_header_of(dp)->n + 1);
}

/**
 * @brief The fields of a descriptor that have a value, in slot order (CS-439)
 *
 * Value loops such as free, copy, pack, unpack and dump run over this range
 * rather than over the descriptor entries, so they visit exactly the values an
 * element holds. A counted descriptor with absent fields is walked over its
 * slots through the reverse map in its header, so a reduction to a few fields
 * of a long type costs a few steps (N-B-6). A full descriptor, one without
 * #CULL_IS_REDUCED, has no absent field, and its slots are its positions. Any
 * other descriptor is walked over its entries, skipping absent ones.
 *
 * @code
 * for (const auto f : ocs_cull_present_fields(ep->descr)) {
 *    free_value(ep->descr[f.pos].mt, ep->cont[f.slot]);
 * }
 * @endcode
 */
class ocs_cull_present_fields {
public:
   /// end of the range
   struct sentinel {};

   /// position in the range
   class iterator {
   public:
      /**
       * @brief An iterator at the first field that has a value
       * @param dp the descriptor
       * @param rmap the reverse map to walk the slots by, or nullptr to walk the entries
       * @param np the number of slots, used with a reverse map
       * @param full true if dp has no absent field, so the entry walk needs no check
       */
      iterator(const lDescr *dp, const int16_t *rmap, const int np, const bool full)
         : dp_(dp), rmap_(rmap), np_(np), i_(0), full_(full) {
         if (rmap_ == nullptr && !full_) {
            skip_absent();
         }
      }

      /**
       * @brief The field at the iterator
       * @return its position and slot
       */
      ocs_cull_present_field operator*() const {
         if (rmap_ != nullptr) {
            return {rmap_[i_], i_};
         }
         if (full_) {
            return {i_, i_};
         }
         return {i_, ocs_cull_slot(dp_, i_)};
      }

      /**
       * @brief Advance to the next field that has a value
       * @return the iterator
       */
      iterator &operator++() {
         ++i_;
         if (rmap_ == nullptr && !full_) {
            skip_absent();
         }
         return *this;
      }

      /**
       * @brief Whether the iterator has not reached the end
       * @return true while it is at a field
       */
      bool operator!=(sentinel) const {
         if (rmap_ != nullptr) {
            return i_ < np_;
         }
         return mt_get_type(dp_[i_].mt) != lEndT;
      }

   private:
      /**
       * @brief Move an entry walk past absent entries
       */
      void skip_absent() {
         while (mt_get_type(dp_[i_].mt) != lEndT && dp_[i_].off == OCS_CULL_ABSENT) {
            ++i_;
         }
      }

      const lDescr *dp_;    ///< the descriptor
      const int16_t *rmap_; ///< the reverse map, or nullptr for an entry walk
      int np_;              ///< the number of slots of a slot walk
      int i_;               ///< the current slot of a slot walk, or position of an entry walk
      bool full_;           ///< an entry walk over a descriptor without absent fields
   };

   /**
    * @brief The range over a descriptor
    * @param dp the descriptor, not nullptr
    */
   explicit ocs_cull_present_fields(const lDescr *dp) : dp_(dp) {}

   /**
    * @brief The first field that has a value
    * @return an iterator at it, or at the end
    */
   iterator begin() const {
      // only a reduced descriptor has absent fields (N-B-1)
      if ((dp_[0].mt & CULL_IS_REDUCED) == 0) {
         return {dp_, nullptr, 0, true};
      }
      if ((dp_[0].mt & CULL_DESCR_INTERNED) != 0) {
         const cull_descr_header *h = ocs_cull_descr_header_of(dp_);
         if (h->np < h->n) {
            return {dp_, ocs_cull_descr_rmap(dp_), static_cast<int>(h->np), false};
         }
      }
      return {dp_, nullptr, 0, false};
   }

   /**
    * @brief The end of the range
    * @return the sentinel
    */
   sentinel end() const {
      return {};
   }

private:
   const lDescr *dp_; ///< the descriptor
};

/**
 * @brief The hash table of one hashed field of a list
 *
 * A list that may be hashed holds one slot per field of its type that is marked
 * #CULL_HASH. The slot array is allocated by the first table build of the list,
 * so a list that is never searched has none; the table in a slot exists only
 * once it is built. The slot carries the field's descriptor position, which
 * maps a position to its table by a scan over the few hashed fields of a type.
 *
 * The table pointer is atomic because concurrent readers of a list may each
 * build the table on their first lookup: get() reads it with acquire semantics,
 * so a reader that sees a table also sees it fully built. Modifications of a
 * list never run concurrently with other access to it, so set() needs no more
 * than release semantics.
 */
struct cull_hash_slot {
   int pos;                     ///< position of the hashed field in the list's descriptor
   std::atomic<cull_htable> ht; ///< the table over that field, or nullptr while none is built

   /**
    * @brief The table of the slot
    * @return the table, or nullptr while none is built
    */
   cull_htable get() const {
      return ht.load(std::memory_order_acquire);
   }

   /**
    * @brief Store a table in the slot, by a thread that modifies the list
    * @param t the table, or nullptr to empty the slot
    */
   void set(const cull_htable t) {
      ht.store(t, std::memory_order_release);
   }
};
static_assert(std::atomic<cull_htable>::is_always_lock_free, "a hash slot must not hide a mutex");

/**
 * @brief A cull list: a doubly linked list of elements of one object type
 *
 * Every element shares the list's descriptor, so a list is homogeneous. The
 * element count is kept rather than walked, and both ends are held so
 * appending is O(1). The hash tables over its elements belong to the list,
 * not to the descriptor, so that a descriptor carries nothing list specific.
 */
struct _lList {
   uint32_t nelem;      ///< number of elements currently in the list
   uint32_t nslots;     ///< number of hashed fields of the type, 0 if the list is never hashed
   char *listname;      ///< name of the list, used in dumps and error messages
   const lDescr *descr; ///< the object type every element has; one reference, see ocs_cull_descr.h
   lListElem *first;    ///< first element, or nullptr when the list is empty
   lListElem *last;     ///< last element, or nullptr when the list is empty
   /// nslots hash slots, or nullptr until the first table build; atomic and
   /// mutable because concurrent first lookups on a const list publish it,
   /// which does not change the list logically, see #ocs_cull_hash_lookup_table
   mutable std::atomic<cull_hash_slot *> ht;
};
static_assert(std::atomic<cull_hash_slot *>::is_always_lock_free, "the slot array pointer must not hide a mutex");

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

/**
 * @brief The slot array of a list
 *
 * Read with acquire semantics, so that a reader that sees an array published
 * by a concurrent first lookup also sees its slots constructed.
 *
 * @param lp the list, not nullptr
 * @return the lp->nslots slots, or nullptr while no table of the list was built
 */
inline cull_hash_slot *list_slots(const lList *lp) {
   return lp->ht.load(std::memory_order_acquire);
}

/**
 * @brief The hash slot of a field of a list
 *
 * A linear scan over the hashed fields of the list's type, usually one to
 * three, which is cheaper than mapping the position through the descriptor.
 *
 * @param lp the list, not nullptr
 * @param pos position of the field in the list's descriptor
 * @return the slot, or nullptr when the field is not hashed in this list or the list has no slot array yet
 */
inline cull_hash_slot *list_hash_slot(const lList *lp, const int pos) {
   cull_hash_slot *slots = list_slots(lp);
   if (slots == nullptr) {
      return nullptr;
   }
   for (uint32_t i = 0; i < lp->nslots; i++) {
      if (slots[i].pos == pos) {
         return &slots[i];
      }
   }
   return nullptr;
}

/**
 * @brief The hash table of a field of a list
 *
 * @param lp the list, not nullptr
 * @param pos position of the field in the list's descriptor
 * @return the table, or nullptr when the field is not hashed or its table is not built
 */
inline cull_htable list_hash_table(const lList *lp, const int pos) {
   const cull_hash_slot *slot = list_hash_slot(lp, pos);
   return slot != nullptr ? slot->get() : nullptr;
}

/**
 * @brief The hash table that holds a field of an element
 *
 * Only a bound element is hashed, and its tables are those of the list that
 * holds it, found through the owner word. A free element, a sub-object and an
 * unpacked element that is not chained in yet are in no table.
 *
 * @param ep the element, not nullptr
 * @param pos position of the field in the element's descriptor
 * @return the table, or nullptr when the element is not bound or the field is not hashed
 */
inline cull_htable elem_hash_table(const lListElem *ep, const int pos) {
   // most fields are not hashed: their flag answers without looking at the list
   if (!mt_do_hashing(ep->descr[pos].mt)) {
      return nullptr;
   }
   const lList *lp = elem_list(ep);
   return lp != nullptr ? list_hash_table(lp, pos) : nullptr;
}
