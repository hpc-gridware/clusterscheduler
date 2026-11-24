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

#include "tom/ocs_ListCore.h"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "uti/sge_stdlib.h"

#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/detail/ocs_Stats.h"
#include "tom/ocs_Descr.h"

#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"

namespace ocs::tom {

   ListCoreError::ListCoreError(const std::string &message) : ocs::RuntimeException(message) {}

   ListCoreError::~ListCoreError() = default;

   namespace {

      /**
       * @brief Throw the logic error of a null list
       */
      [[noreturn, gnu::cold]] void throw_null_list() {
         throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
      }

      /**
       * @brief Throw the logic error of a null element
       * @param descr the descriptor of the list the element was meant for, or nullptr
       */
      [[noreturn, gnu::cold]] void throw_null_elem(const lDescr *descr) {
         throw_field_exception(FieldException::Reason::NullElement, NoName, descr);
      }

      /**
       * @brief Check that an element to be added is free
       * @param ep the element
       * @throw FieldException with reason BoundElement if it is bound in a list or an object
       */
      void check_free(const lListElem *ep) {
         if (elem_kind(ep) == BOUND_ELEM || elem_kind(ep) == OBJECT_ELEM) [[unlikely]] {
            throw_field_exception(FieldException::Reason::BoundElement, NoName, ep->descr);
         }
      }

      /**
       * @brief Check that an element to be linked into a list has the layout of the list's descriptor (CS-3112)
       *
       * A linked element borrows the list's descriptor, so its values must lie
       * at the slots that descriptor gives. Always on: the same descriptor
       * pointer, the common case, costs one compare; only different pointers
       * compare the entries.
       *
       * @param lp the list
       * @param ep the element
       * @throw FieldException with reason WrongLayout if the layouts differ
       */
      void check_layout(const lList *lp, const lListElem *ep) {
         if (!same_layout(ep->descr, lp->descr)) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongLayout, NoName, ep->descr, lp->descr);
         }
      }

      /**
       * @brief Check that an element is in a list
       * @param lp the list
       * @param ep the element
       * @throw FieldException with reason NullList, NullElement or ForeignList
       */
      void check_member(const lList *lp, const lListElem *ep) {
         if (lp == nullptr) [[unlikely]] {
            throw_null_list();
         }
         if (ep == nullptr) [[unlikely]] {
            throw_null_elem(lp->descr);
         }
         // descriptors are shared between lists, so only the owner tells the list of an element
         if (elem_list(ep) != lp) [[unlikely]] {
            throw_field_exception(FieldException::Reason::ForeignList, NoName, ep->descr);
         }
      }

      /**
       * @brief Make a free element bound to a list it was just linked into
       *
       * The element gives its descriptor reference up and borrows the list's,
       * enters the hash tables and is counted.
       *
       * @param lp the list
       * @param ep the element, linked into @p lp already
       */
      void bind(lList *lp, lListElem *ep) {
         if (elem_kind(ep) == FREE_ELEM) {
            ocs::tom::release_descr(ep->descr);
         }
         elem_set_bound(ep, lp);
         ep->descr = lp->descr;
         ocs::tom::detail::hash_elem(ep);
         lp->nelem++;
      }

      /**
       * @brief Unlink an element from its list, without freeing it and without touching the hash tables
       * @param lp the list
       * @param ep the element, in @p lp
       */
      void unlink(lList *lp, lListElem *ep) {
         if (ep->prev) {
            ep->prev->next = ep->next;
         } else {
            lp->first = ep->next;
         }
         if (ep->next) {
            ep->next->prev = ep->prev;
         } else {
            lp->last = ep->prev;
         }
         ep->prev = ep->next = nullptr;
         lp->nelem--;
      }

   } // namespace

   lListElem *create_elem(const lDescr *descr) {
      if (descr == nullptr || mt_get_type(descr[0].mt) == MT_END) {
         throw_field_exception(FieldException::Reason::NullDescriptor, NoName, nullptr);
      }

      auto *ep = reinterpret_cast<lListElem *>(sge_malloc(sizeof(lListElem)));
      if (ep == nullptr) {
         throw ListCoreError("out of memory creating an element");
      }
      ep->next = nullptr;
      ep->prev = nullptr;

      // A shared descriptor, the static block of a type or a counted one, is taken as it is: a reduced one comes
      // from ocs::tom::descr_layout(), which marks every entry (N-B-5). Only a plain array is measured and, if
      // need be, normalised; creating an element must not walk the fields of a shared one, twice, each time.
      const bool shared = ocs::tom::descr_is_counted(descr) || ocs::tom::descr_static_for(descr[0].nm) == descr;
#if defined(ENABLE_DEBUG_CHECKS)
      if (shared && (descr->mt & MT_IS_REDUCED) != 0) {
         for (int i = 0; mt_get_type(descr[i].mt) != MT_END; i++) {
            if ((descr[i].mt & MT_IS_REDUCED) == 0) {
               abort();
            }
         }
      }
#endif

      // every field inherits the reduced flag of the type; a descriptor that does
      // not carry it in every field yet is normalised in a temporary copy
      lDescr *normalised = nullptr;
      if (!shared && (descr->mt & MT_IS_REDUCED) != 0) {
         const int n = descr_length(descr);
         for (int i = 0; i <= n; i++) {
            if ((descr[i].mt & MT_IS_REDUCED) == 0) {
               normalised = reinterpret_cast<lDescr *>(sge_malloc(sizeof(lDescr) * (n + 1)));
               if (normalised == nullptr) {
                  sge_free(&ep);
                  throw ListCoreError("out of memory creating an element");
               }
               memcpy(normalised, descr, sizeof(lDescr) * (n + 1));
               TOM_STATS_TYPE(descr[0].nm, DCOPY_ELEM, 1);
               TOM_STATS_TYPE(descr[0].nm, DCOPY_ELEM_B, sizeof(lDescr) * (n + 1));
               for (int j = 0; j <= n; j++) {
                  normalised[j].mt |= MT_IS_REDUCED;
               }
               break;
            }
         }
      }
      ep->descr = ocs::tom::descr_acquire(normalised != nullptr ? normalised : descr);
      sge_free(&normalised);
      if (ep->descr == nullptr) {
         sge_free(&ep);
         throw ListCoreError("out of memory creating an element");
      }

      elem_set_free(ep);
      // the held descriptor is static or counted, so its header knows the number of values
      ep->cont = static_cast<lMultiType *>(calloc(1, sizeof(lMultiType) * ocs::tom::descr_header_of(ep->descr)->np));
      if (ep->cont == nullptr) {
         ocs::tom::release_descr(ep->descr);
         sge_free(&ep);
         throw ListCoreError("out of memory creating an element");
      }

      return ep;
   }

   lList *create_list(const char *name, const lDescr *descr, const bool hash) {
      if (name == nullptr) {
         name = "No list name specified";
      }
      if (descr == nullptr || mt_get_type(descr[0].mt) == MT_END) {
         throw_field_exception(FieldException::Reason::NullDescriptor, NoName, nullptr);
      }

      auto *lp = reinterpret_cast<lList *>(sge_malloc(sizeof(lList)));
      if (lp == nullptr) {
         throw ListCoreError("out of memory creating a list");
      }
      if ((lp->listname = strdup(name)) == nullptr) {
         sge_free(&lp);
         throw ListCoreError("out of memory copying a list name");
      }
      lp->nelem = 0;
      lp->first = nullptr;
      lp->last = nullptr;
      // the list holds a reference on the shared descriptor instead of a copy
      if ((lp->descr = ocs::tom::descr_acquire(descr)) == nullptr) {
         sge_free(&(lp->listname));
         sge_free(&lp);
         throw ListCoreError("out of memory creating a list");
      }
      // the hash tables belong to the list: one slot per hashed field if it may be hashed
      if (!ocs::tom::detail::hash_init_slots(lp, hash)) {
         ocs::tom::release_descr(lp->descr);
         sge_free(&(lp->listname));
         sge_free(&lp);
         throw ListCoreError("out of memory creating a list");
      }

      return lp;
   }

   void free_elem(lListElem *&ep) {
      if (ep == nullptr) {
         return;
      }
      if (ep->descr == nullptr) [[unlikely]] {
         throw_field_exception(FieldException::Reason::NullDescriptor, NoName, nullptr);
      }

      // remove a bound element from the hash tables of its list
      ocs::tom::detail::hash_remove_elem(ep);

      for (const auto f : ocs::tom::present_fields(ep->descr)) {
         lMultiType &value = ep->cont[f.slot];
         switch (mt_get_type(ep->descr[f.pos].mt)) {
            case MT_INT32:
            case MT_UINT32:
            case MT_UINT64:
            case MT_DOUBLE:
            case MT_INT64:
            case MT_BOOL:
            case MT_REF:
               break;
            case MT_STRING:
               if (value.str != nullptr) {
                  sge_free(&(value.str));
               }
               break;
            case MT_HOST:
               if (value.host != nullptr) {
                  sge_free(&(value.host));
               }
               break;
            case MT_LIST:
               if (value.glp != nullptr) {
                  free_list(value.glp);
               }
               break;
            case MT_OBJECT:
               if (value.obj != nullptr) {
                  free_elem(value.obj);
               }
               break;
            default:
               break;
         }
      }

      // a bound element borrows the descriptor of its list; the others hold a reference
      if (elem_kind(ep) == FREE_ELEM || elem_kind(ep) == OBJECT_ELEM) {
         ocs::tom::release_descr(ep->descr);
      }
      if (ep->cont != nullptr) {
         sge_free(&(ep->cont));
      }

      sge_free(&ep);
   }

   void free_list(lList *&lp) {
      if (lp == nullptr) {
         return;
      }
      // remove all hash tables first, it is more efficient than maintaining them while emptying
      ocs::tom::detail::hash_free_tables(lp);
      while (lp->first) {
         lListElem *ep = lp->first;
         unlink(lp, ep);
         free_elem(ep);
      }
      ocs::tom::detail::hash_free_slots(lp);
      ocs::tom::release_descr(lp->descr);
      if (lp->listname) {
         sge_free(&(lp->listname));
      }
      sge_free(&lp);
   }

   lListElem *copy_elem(const lListElem *ep, const bool hash, const bool skip_no_transfer) {
      if (ep == nullptr) {
         throw_null_elem(nullptr);
      }
      lListElem *copy;
      try {
         copy = create_elem(ep->descr);
      } catch (const ListCoreError &) {
         throw ListCoreError("out of memory copying an element");
      }
      for (const auto f : ocs::tom::present_fields(ep->descr)) {
         try {
            copy_field(ep, copy, f.pos, f.pos, hash, nullptr, skip_no_transfer);
         } catch (...) {
            free_elem(copy);
            throw ListCoreError("copying a field of an element failed");
         }
      }
      elem_set_free(copy);
      return copy;
   }

   lList *copy_list(const char *name, const lList *src, const bool hash, const bool skip_no_transfer) {
      if (src == nullptr) {
         throw_null_list();
      }
      if (name == nullptr) {
         name = src->listname;
      }
      lList *dst;
      try {
         // the copy is created without hash slots, so that filling it maintains no table
         dst = create_list(name, src->descr, false);
      } catch (const ListCoreError &) {
         throw ListCoreError("out of memory copying a list");
      }
      // the source was checked for duplicate keys when it was filled; checking each copy again would be quadratic
      const UniqueCheckSuspension suspension;
      for (const lListElem *sep = src->first; sep; sep = sep->next) {
         try {
            append_elem(dst, copy_elem(sep, true, skip_no_transfer));
         } catch (const ListCoreError &) {
            free_list(dst);
            throw ListCoreError("copying an element of a list failed");
         }
      }
      if (hash) {
         // the list may be hashed from now on; its tables are built by the first lookups
         ocs::tom::make_hashable(dst);
      }
      return dst;
   }

   void copy_field(const lListElem *src, lListElem *dst, const int src_pos, const int dst_pos, const bool hash,
                   const lEnumeration *sub_what, const bool skip_no_transfer) {
      if (src == nullptr || dst == nullptr) {
         throw_null_elem(nullptr);
      }
      lMultiType &to = ocs::tom::detail::value(dst, dst_pos);
      const lMultiType &from = ocs::tom::detail::value(src, src_pos);

      // A field marked MT_NO_TRANSFER holds state that is meaningful only in the process that built it.
      // A copy that will be handed out, an event payload, keeps what create_elem() gave it.
      if (skip_no_transfer && !mt_do_transfer(dst->descr[dst_pos].mt)) {
         to = lMultiType{};
         return;
      }

      switch (mt_get_type(dst->descr[dst_pos].mt)) {
         case MT_UINT32:
            to.ul = from.ul;
            break;
         case MT_UINT64:
            to.ul64 = from.ul64;
            break;
         case MT_STRING:
         case MT_HOST:
            // the union holds a string and a host at the same place
            to.str = nullptr;
            if (from.str != nullptr && (to.str = strdup(from.str)) == nullptr) {
               throw ListCoreError("out of memory copying a string");
            }
            break;
         case MT_DOUBLE:
            to.db = from.db;
            break;
         case MT_LIST:
            to.glp = nullptr;
            if (from.glp != nullptr) {
               if (sub_what == nullptr) {
                  to.glp = copy_list(from.glp->listname, from.glp, hash, skip_no_transfer);
               } else {
                  lList *selected = select_list(from.glp->listname, from.glp, nullptr, sub_what, hash, skip_no_transfer);
                  // an emptied sub-list of a list that may be hashed is no list, as the selection always gave it
                  if (hash && selected->nelem == 0) {
                     free_list(selected);
                  }
                  to.glp = selected;
               }
            }
            break;
         case MT_OBJECT:
            to.obj = nullptr;
            if (from.obj != nullptr) {
               lListElem *copy = sub_what == nullptr ? copy_elem(from.obj, hash, skip_no_transfer)
                                                     : select_elem(from.obj, sub_what, hash, skip_no_transfer);
               elem_set_object(copy);
               to.obj = copy;
            }
            break;
         case MT_INT32:
            to.i = from.i;
            break;
         case MT_INT64:
            to.l = from.l;
            break;
         case MT_BOOL:
            to.b = from.b;
            break;
         case MT_REF:
            to.ref = from.ref;
            break;
         default:
            throw ListCoreError("copying a field of an unknown type");
      }
   }

   const char *list_name(const lList *lp) noexcept {
      return lp != nullptr && lp->listname != nullptr ? lp->listname : "No list name specified";
   }

   void insert_elem(lList *lp, lListElem *after, lListElem *ep) {
      if (lp == nullptr) {
         throw_null_list();
      }
      if (ep == nullptr) {
         throw_null_elem(lp->descr);
      }
      check_free(ep);
      check_layout(lp, ep);
      check_unique_on_add(lp, ep);

      if (after != nullptr) {
         ep->prev = after;
         ep->next = after->next;
         after->next = ep;
         if (ep->next) {
            ep->next->prev = ep;
         } else {
            lp->last = ep;
         }
      } else {
         ep->prev = nullptr;
         ep->next = lp->first;
         if (!lp->first) {
            lp->last = ep;
         } else {
            lp->first->prev = ep;
         }
         lp->first = ep;
      }
      bind(lp, ep);
   }

   void append_elem(lList *lp, lListElem *ep) {
      if (lp == nullptr) {
         throw_null_list();
      }
      if (ep == nullptr) {
         throw_null_elem(lp->descr);
      }
      check_free(ep);
      check_layout(lp, ep);
      check_unique_on_add(lp, ep);

      if (lp->last) {
         lp->last->next = ep;
         ep->prev = lp->last;
         lp->last = ep;
         ep->next = nullptr;
      } else {
         lp->last = lp->first = ep;
         ep->prev = ep->next = nullptr;
      }
      bind(lp, ep);
   }

   void dechain_elem(lList *lp, lListElem *ep) {
      check_member(lp, ep);
      unlink(lp, ep);
      // remove the hash entries while the element still borrows the list's descriptor
      ocs::tom::detail::hash_remove_elem(ep);
      // a free element holds a reference of its own
      ep->descr = ocs::tom::descr_ref(ep->descr);
      elem_set_free(ep);
   }

   void remove_elem(lList *lp, lListElem *&ep) {
      check_member(lp, ep);
      unlink(lp, ep);
      free_elem(ep);
   }

   void append_list(lList *dst, lList *src) {
      if (dst == nullptr || src == nullptr) {
         throw_null_list();
      }
      // the elements move with their values, so the layouts must match, not only the types (CS-3112)
      if (!same_layout(src->descr, dst->descr)) [[unlikely]] {
         throw_field_exception(src->descr[0].nm != dst->descr[0].nm ? FieldException::Reason::WrongType
                                                                    : FieldException::Reason::WrongLayout,
                               NoName, src->descr, dst->descr);
      }
      while (src->first) {
         lListElem *ep = src->first;
         dechain_elem(src, ep);
         try {
            append_elem(dst, ep);
         } catch (...) {
            // the element goes back to where it was, so nothing is lost
            insert_elem(src, nullptr, ep);
            throw;
         }
      }
   }

   void exchange_list(lListElem *ep, const int pos, lList *&lp) noexcept {
      lList *&held = ocs::tom::detail::value(ep, pos).glp;
      if (lp != held) {
         std::swap(lp, held);
      }
   }

   lListElem *take_object(lListElem *ep, const int pos) noexcept {
      lListElem *const held = ocs::tom::detail::value(ep, pos).obj;
      if (held != nullptr) {
         elem_set_free(held);
         ocs::tom::detail::value(ep, pos).obj = nullptr;
      }
      return held;
   }

   namespace {

      /**
       * @brief The position of a key field in a descriptor, checking its type
       * @param descr the descriptor
       * @param nm the field number
       * @param type the field type the key has
       * @return the position
       * @throw FieldException with reason AbsentField or WrongType
       */
      int key_pos(const lDescr *descr, const int nm, const int type) {
         const int pos = present_pos(descr, nm);
         if (pos < 0) [[unlikely]] {
            throw_field_exception(FieldException::Reason::AbsentField, nm, descr);
         }
         if (mt_get_type(descr[pos].mt) != type) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongType, nm, descr);
         }
         return pos;
      }

      /**
       * @brief Create an element with a key and append it to a list, creating the list if needed
       *
       * A list that exists already must hold elements of the descriptor's
       * type with the key field; before CS-3045 an element of another type
       * was appended, and the list held two types. The element is created
       * with the list's descriptor, which for a list that exists already may
       * be a reduction of @p descr; before CS-3112 it was created with
       * @p descr, and its values lay at other slots than the list's
       * descriptor gives.
       *
       * @param lp the list, created if nullptr
       * @param pos the position of the key field in @p descr, the same in every descriptor of its type
       * @param descr the descriptor of a list created here and its element; an existing list gives its own
       * @param list_name the name of a list created here
       * @param store sets the key into the new element
       * @return the element, now in the list
       * @throw FieldException with reason WrongType if the list is of another type or reduced without the key
       *        field, and as create_elem() and append_elem()
       */
      template <typename Store>
      lListElem *add_with_key(lList *&lp, const int pos, const lDescr *descr, const char *list_name, Store store) {
         const bool created = lp == nullptr;
         if (!created && (lp->descr[0].nm != descr[0].nm || lp->descr[pos].off == TOM_ABSENT)) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongType, descr[pos].nm, lp->descr);
         }
         if (created) {
            lp = create_list(list_name, descr, true);
         }
         lListElem *ep = nullptr;
         try {
            // the element shares the list's descriptor, which holds the key field, checked above
            ep = create_elem(lp->descr);
            store(ep);
            append_elem(lp, ep);
         } catch (...) {
            free_elem(ep);
            if (created) {
               free_list(lp);
            }
            throw;
         }
         return ep;
      }

   } // namespace

   lListElem *add_elem_str(lList *&lp, const int nm, const char *key, const lDescr *descr) {
      if (descr == nullptr) {
         throw_field_exception(FieldException::Reason::NullDescriptor, nm, nullptr);
      }
      const int pos = key_pos(descr, nm, MT_STRING);
      if (key == nullptr) {
         throw_field_exception(FieldException::Reason::NullKey, nm, descr);
      }
      return add_elem_at<FieldType::String>(lp, pos, key, descr);
   }

   lListElem *add_elem_host(lList *&lp, const int nm, const char *key, const lDescr *descr) {
      if (descr == nullptr) {
         throw_field_exception(FieldException::Reason::NullDescriptor, nm, nullptr);
      }
      const int pos = key_pos(descr, nm, MT_HOST);
      if (key == nullptr) {
         throw_field_exception(FieldException::Reason::NullKey, nm, descr);
      }
      return add_elem_at<FieldType::Host>(lp, pos, key, descr);
   }

   lListElem *add_elem_ulong(lList *&lp, const int nm, const uint32_t key, const lDescr *descr) {
      if (descr == nullptr) {
         throw_field_exception(FieldException::Reason::NullDescriptor, nm, nullptr);
      }
      const int pos = key_pos(descr, nm, MT_UINT32);
      return add_elem_at<FieldType::Uint32>(lp, pos, key, descr);
   }

   lListElem *add_elem_ulong64(lList *&lp, const int nm, const uint64_t key, const lDescr *descr) {
      if (descr == nullptr) {
         throw_field_exception(FieldException::Reason::NullDescriptor, nm, nullptr);
      }
      const int pos = key_pos(descr, nm, MT_UINT64);
      return add_elem_at<FieldType::Uint64>(lp, pos, key, descr);
   }

   namespace {

      /**
       * @brief Remove and free a found element, if any
       * @param lp the list
       * @param ep the element found, or nullptr
       * @return true if an element was removed
       */
      bool remove_found(lList *lp, lListElem *ep) {
         if (ep == nullptr) {
            return false;
         }
         remove_elem(lp, ep);
         return true;
      }

   } // namespace

   bool remove_elem_str(lList *lp, const int nm, const char *key) {
      if (key == nullptr) {
         throw_field_exception(FieldException::Reason::NullKey, nm, lp != nullptr ? lp->descr : nullptr);
      }
      if (lp == nullptr) {
         return false;
      }
      return remove_elem_at<FieldType::String>(lp, key_pos(lp->descr, nm, MT_STRING), key);
   }

   bool remove_elem_host(lList *lp, const int nm, const char *key) {
      if (key == nullptr) {
         throw_field_exception(FieldException::Reason::NullKey, nm, lp != nullptr ? lp->descr : nullptr);
      }
      if (lp == nullptr) {
         return false;
      }
      return remove_elem_at<FieldType::Host>(lp, key_pos(lp->descr, nm, MT_HOST), key);
   }

   bool remove_elem_ulong(lList *lp, const int nm, const uint32_t key) {
      if (lp == nullptr) {
         return false;
      }
      return remove_elem_at<FieldType::Uint32>(lp, key_pos(lp->descr, nm, MT_UINT32), key);
   }

   bool remove_elem_ulong64(lList *lp, const int nm, const uint64_t key) {
      if (lp == nullptr) {
         return false;
      }
      return remove_elem_at<FieldType::Uint64>(lp, key_pos(lp->descr, nm, MT_UINT64), key);
   }

   template <FieldType T>
      requires KeyFieldType<T>
   lListElem *add_elem_at(lList *&lp, const int pos, const KeyArgOf<T> key, const lDescr *descr) {
      if constexpr (T == FieldType::Uint32) {
         return add_with_key(lp, pos, descr, "ulong_sublist", [pos, key](lListElem *ep) {
            ocs::tom::detail::value(ep, pos).ul = key;
         });
      } else if constexpr (T == FieldType::Uint64) {
         return add_with_key(lp, pos, descr, "ulong64_sublist", [pos, key](lListElem *ep) {
            ocs::tom::detail::value(ep, pos).ul64 = key;
         });
      } else {
         return add_with_key(lp, pos, descr, "", [pos, key](lListElem *ep) {
            if (!store_string(ep, pos, key)) {
               throw ListCoreError("out of memory copying a key");
            }
         });
      }
   }

   template <FieldType T>
      requires KeyFieldType<T>
   bool remove_elem_at(lList *lp, const int pos, const KeyArgOf<T> key) {
      if (lp == nullptr) {
         return false;
      }
      const void *iterator = nullptr;
      return remove_found(lp, lookup_first<T>(lp, pos, key, &iterator));
   }

   // adding and deleting by key exist for the four key field types; callers link against these instantiations
   template lListElem *add_elem_at<FieldType::Uint32>(lList *&, int, KeyArgOf<FieldType::Uint32>, const lDescr *);
   template lListElem *add_elem_at<FieldType::Uint64>(lList *&, int, KeyArgOf<FieldType::Uint64>, const lDescr *);
   template lListElem *add_elem_at<FieldType::String>(lList *&, int, KeyArgOf<FieldType::String>, const lDescr *);
   template lListElem *add_elem_at<FieldType::Host>(lList *&, int, KeyArgOf<FieldType::Host>, const lDescr *);
   template bool remove_elem_at<FieldType::Uint32>(lList *, int, KeyArgOf<FieldType::Uint32>);
   template bool remove_elem_at<FieldType::Uint64>(lList *, int, KeyArgOf<FieldType::Uint64>);
   template bool remove_elem_at<FieldType::String>(lList *, int, KeyArgOf<FieldType::String>);
   template bool remove_elem_at<FieldType::Host>(lList *, int, KeyArgOf<FieldType::Host>);

   lList *get_or_create_list(lListElem *ep, const int pos, const char *name, const lDescr *descr) {
      if (mt_get_type(ep->descr[pos].mt) != MT_LIST) [[unlikely]] {
         throw_field_exception(FieldException::Reason::WrongType, ep->descr[pos].nm, ep->descr);
      }
      lList *&held = ocs::tom::detail::value(ep, pos).glp;
      if (held == nullptr) {
         lList *lp = create_list(name != nullptr ? name : ocs::tom::field_name(ep->descr[pos].nm), descr, true);
         store_list(ep, pos, lp);
      }
      return held;
   }

} // namespace ocs::tom
