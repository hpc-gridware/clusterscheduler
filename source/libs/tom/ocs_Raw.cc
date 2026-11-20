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
 * @brief The run-time typed interface of the typed object model: the functions that are not templates
 *
 * Thin fronts of the core: each checks what the core does not check for an
 * owner or a field given at run time (the sub-type of a stored list or
 * object, the type of an element added to a list), then calls the core and
 * moves the ownership. The strong guarantee of the list ops comes from the
 * order: the owner gives up its element or list only after the core took it.
 */

#include "tom/ocs_Raw.h"

#include <utility>
#include <vector>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_profiling.h"

#include "tom/cull_hash.h"
#include "tom/cull_lerrnoP.h"
#include "tom/cull_listP.h"
#include "tom/ocs_cull_descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_PrintCore.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SoftPath.h"

namespace ocs::tom {

   namespace {

      /**
       * @brief Time the writes of select_pack() for the profiling of SGE_PROF_PACKING, as lSelectHashPack() timed
       *        them, also when a write throws
       */
      class PackingTime {
      public:
         /** @brief Start the measurement */
         PackingTime() {
            PROF_START_MEASUREMENT(SGE_PROF_PACKING);
         }

         /** @brief Stop the measurement */
         ~PackingTime() {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         }

         PackingTime(const PackingTime &) = delete;
         PackingTime &operator=(const PackingTime &) = delete;
      };

      /**
       * @brief Check that a list or element to be stored into a field is of the sub-type the model names for it
       *
       * The typed setters know the sub-type at compile time; a field given at
       * run time asks the installed model. Without a model, or for a field the
       * model names no sub-type for, every type is accepted.
       *
       * @param nm the list or object field
       * @param value_descr the descriptor of the list or element to be stored, or nullptr for none
       * @throw FieldException with reason WrongSubType if @p value_descr is of another type than the sub-type
       */
      void check_sub_type(const int nm, const lDescr *value_descr) {
         if (value_descr == nullptr) {
            return;
         }
         const lDescr *sub = sub_type(nm);
         if (sub != nullptr && value_descr[0].nm != sub[0].nm) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongSubType, nm, value_descr, sub);
         }
      }

      /**
       * @brief Check a list and an element before the element is linked into the list
       *
       * The core checks the layout; a different type is reported here as
       * WrongType, as the typed owner's checked constructor reports it.
       *
       * @param lp the list
       * @param ep the element
       * @throw FieldException with reason NullList, NullElement or WrongType
       */
      void check_insert(const lList *lp, const lListElem *ep) {
         if (lp == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
         }
         if (ep == nullptr) [[unlikely]] {
            throw_field_exception(FieldException::Reason::NullElement, NoName, lp->descr);
         }
         if (ep->descr[0].nm != lp->descr[0].nm) [[unlikely]] {
            throw_field_exception(FieldException::Reason::WrongType, NoName, ep->descr, lp->descr);
         }
      }

      /**
       * @brief Write the text a print function of the core appends to a string into a stream, as the old
       *        lWrite*To() functions do
       *
       * The core throws ListCoreError only for a null object, before it has appended anything, and has set the
       * CULL error number then; the old functions caught it and wrote nothing, and so does this one.
       *
       * @tparam Print the type of the print function
       * @param fp the stream, or nullptr to write nothing
       * @param print appends the text, e.g. a call of print_list()
       */
      template <typename Print>
      void write_text(FILE *fp, const Print &print) {
         try {
            print_to(fp, print);
         } catch (const ListCoreError &) {
            // a null object: nothing written, the core has set the error number
         }
      }

   } // namespace

   void set(lListElem *ep, const int nm, RawListPtr list) {
      const int pos = checked_pos(ep, nm, FieldType::List);
      check_sub_type(nm, list.get().descr());
      store_list(ep, pos, list.release());
   }

   /**
    * @brief Store an element into an object field given at run time, freeing the element it held
    *
    * A bound element belongs to the list or the object it is bound to. The owner gives it up before the
    * first check, so that no throw - NullElement, ForeignField, AbsentField, WrongType, WrongSubType or
    * BoundElement - lets the by-value parameter free it. A free element stays with the parameter until it
    * is stored, so that the parameter frees it on a throw.
    *
    * @param ep the element, or nullptr
    * @param nm the field number of an object field
    * @param object the element to store, or an empty owner to clear the field
    * @throw FieldException with reason NullElement, ForeignField, AbsentField, WrongType, WrongSubType or
    *        BoundElement
    */
   void set(lListElem *ep, const int nm, RawElementPtr object) {
      lListElem *value = object.get().raw();
      if (value != nullptr && elem_kind(value) != FREE_ELEM && elem_kind(value) != TRANS_BOUND_ELEM) {
         // bound (or the sub-object of a field, maybe of this one): never the parameter's to free
         static_cast<void>(object.release());
      }
      const int pos = checked_pos(ep, nm, FieldType::Object);
      check_sub_type(nm, value != nullptr ? value->descr : nullptr);
      if (!store_object(ep, pos, value)) [[unlikely]] {
         throw_field_exception(FieldException::Reason::BoundElement, nm, ep->descr);
      }
      // stored: the element took it over (or held it already)
      static_cast<void>(object.release());
   }

   RawListPtr exchange(lListElem *ep, const int nm, RawListPtr list) {
      const int pos = checked_pos(ep, nm, FieldType::List);
      check_sub_type(nm, list.get().descr());
      lList *lp = list.release();
      exchange_list(ep, pos, lp);
      return RawListPtr(lp);
   }

   RawElementPtr take_raw_object(lListElem *ep, const int nm) {
      const int pos = checked_pos(ep, nm, FieldType::Object);
      return RawElementPtr(take_object(ep, pos));
   }

   RawElementPtr create_raw_elem(const lDescr *descr) {
      return RawElementPtr(create_elem(descr));
   }

   RawListPtr create_raw_list(const lDescr *descr, const Hashing hashing, const char *name) {
      return RawListPtr(create_list(name, descr, hashing == Hashing::On));
   }

   RawElementPtr clone(const lListElem *ep) {
      if (ep == nullptr) {
         return {};
      }
      return RawElementPtr(copy_elem(ep, true, false));
   }

   RawListPtr clone(const lList *lp, const Hashing hashing) {
      if (lp == nullptr) {
         return {};
      }
      return RawListPtr(copy_list(nullptr, lp, hashing == Hashing::On, false));
   }

   RawWherePtr clone(const lCondition *cp) {
      return RawWherePtr(copy_where(cp));
   }

   RawWhatPtr clone(const lEnumeration *ep) {
      if (ep == nullptr) {
         return {};
      }
      return RawWhatPtr(copy_what(ep));
   }

   RawWherePtr detail::raw_where_compare(const lDescr *descr, const int nm, const CompareOp op, const int value_type,
                                         const lMultiType &value) {
      try {
         return RawWherePtr(make_compare(descr, nm, op, value_type, value));
      } catch (const FieldException &e) {
         if (e.reason() == FieldException::Reason::WrongType) {
            // lWhere() aborted the process for a value of another type than the field
            throw;
         }
      } catch (const ListCoreError &) {
         // lWhere() reported every failure of a comparison as a condition that did not parse
      }
      // a null descriptor or a field it does not hold: no condition, with the error number lWhere() left last
      LERROR(LEPARSECOND);
      return {};
   }

   RawWhatPtr what_of(const lDescr *descr, const std::span<const int> fields) {
      std::vector<WhatEntry> entries;
      entries.reserve(fields.size());
      for (const int nm : fields) {
         entries.push_back({nm, nullptr});
      }
      try {
         return RawWhatPtr(make_what(descr, entries));
      } catch (const FieldException &) {
         // a null descriptor, no field, or a field the descriptor does not hold, all LENAMENOT in _lWhat()
         LERROR(LENAMENOT);
      } catch (const ListCoreError &e) {
         LERROR(e.lerrno());
      }
      return {};
   }

   RawWhatPtr what_all() {
      try {
         return RawWhatPtr(make_what_special(WHAT_ALL));
      } catch (const ListCoreError &e) {
         LERROR(e.lerrno());
      }
      return {};
   }

   void append(lList *lp, RawElementPtr &&ep) {
      check_insert(lp, ep.get().raw());
      append_elem(lp, ep.get().raw());
      // the list owns the element now
      static_cast<void>(ep.release());
   }

   void insert_after(lList *lp, lListElem *after, RawElementPtr &&ep) {
      check_insert(lp, ep.get().raw());
      if (after != nullptr && elem_list(after) != lp) [[unlikely]] {
         throw_field_exception(FieldException::Reason::ForeignList, NoName, after->descr);
      }
      insert_elem(lp, after, ep.get().raw());
      // the list owns the element now
      static_cast<void>(ep.release());
   }

   RawElementPtr take(lList *lp, lListElem *ep) {
      dechain_elem(lp, ep);
      return RawElementPtr(ep);
   }

   void erase(lList *lp, lListElem *ep) {
      lListElem *victim = ep;
      remove_elem(lp, victim);
   }

   void splice(lList *dst, RawListPtr &&src) {
      if (dst == nullptr) [[unlikely]] {
         throw_field_exception(FieldException::Reason::NullList, NoName, nullptr);
      }
      lList *other = src.get().raw();
      if (other == nullptr) {
         return;
      }
      // checked before anything moves, so that a throw leaves both lists as they were
      if (!same_layout(other->descr, dst->descr)) [[unlikely]] {
         throw_field_exception(other->descr[0].nm != dst->descr[0].nm ? FieldException::Reason::WrongType
                                                                      : FieldException::Reason::WrongLayout,
                               NoName, other->descr, dst->descr);
      }
#ifdef ENABLE_DEBUG_CHECKS
      for (const lListElem *ep = other->first; ep != nullptr; ep = ep->next) {
         check_unique_on_add(dst, ep);
      }
#endif
      append_list(dst, other);
      src.reset();
   }

   RawListPtr select(const lList *lp, const lCondition *where, const lEnumeration *what, const Hashing hashing) {
      if (lp == nullptr) {
         return {};
      }
      return RawListPtr(select_list(list_name(lp), lp, where, what, hashing == Hashing::On, false));
   }

   RawElementPtr select(const lListElem *ep, const lEnumeration *what, const Hashing hashing) {
      if (ep == nullptr) {
         return {};
      }
      return RawElementPtr(select_elem(ep, what, hashing == Hashing::On, false));
   }

   bool matches(const lListElem *ep, const lCondition *where) {
      return compare(ep, where);
   }

   RawListPtr select_as(const char *name, const lList *lp, const lCondition *where, const lEnumeration *what,
                        const Hashing hashing) {
      if (lp == nullptr) {
         report_soft_path("select_as", SoftPathNoName, SoftPathReason::NullList);
         return {};
      }
      const bool hash = hashing == Hashing::On;
      if (what == nullptr) {
         // no selection: the whole list under its own name, the condition not applied - as lSelectHashPack() did
         try {
            return RawListPtr(copy_list(lp->listname, lp, hash, false));
         } catch (const FieldException &e) {
            // the list is not null, so no reason is left that lCopyListHash() survived
            legacy_abort("select_as", e);
         } catch (const ListCoreError &e) {
            LERROR(e.lerrno());
         }
         return {};
      }
      RawListPtr result;
      try {
         result = RawListPtr(select_list(name, lp, where, what, hash, false));
      } catch (const FieldException &) {
         // a null list or selection, both excluded above: nothing to select from
         report_soft_path("select_as", SoftPathNoName, SoftPathReason::NullList);
         return {};
      } catch (const ListCoreError &e) {
         // e.g. a selection the list's type does not hold
         LERROR(e.lerrno());
         return {};
      }
      // an empty result of a list that may be hashed is no list, as it always was
      if (hash && result.get().raw()->nelem == 0) {
         LERROR(LEGETNROFELEM);
         result.reset();
      }
      return result;
   }

   RawElementPtr select_elem_as(const lListElem *ep, const lCondition *where, const lEnumeration *what,
                                const Hashing hashing) {
      if (ep == nullptr) {
         return {};
      }
      const bool hash = hashing == Hashing::On;
      if (what == nullptr) {
         // no selection: a copy of the whole element, the condition not applied - as lSelectElemPack() did
         try {
            return RawElementPtr(copy_elem(ep, hash, false));
         } catch (const FieldException &) {
            report_soft_path("select_elem_as", SoftPathNoName, SoftPathReason::NullElement);
            LERROR(LEELEMNULL);
         } catch (const ListCoreError &e) {
            LERROR(e.lerrno());
         }
         return {};
      }
      try {
         if (!compare(ep, where)) {
            return {};
         }
         return RawElementPtr(select_elem(ep, what, hash, false));
      } catch (const FieldException &) {
         LERROR(LEELEMNULL);
      } catch (const ListCoreError &e) {
         LERROR(e.lerrno());
      }
      return {};
   }

   RawListPtr select_into_descr(const char *name, const lList *lp, const lCondition *where, const lDescr *descr,
                                const lEnumeration *what, const Hashing hashing) {
      if (lp == nullptr || descr == nullptr) {
         return {};
      }
      const bool hash = hashing == Hashing::On;
      RawListPtr result;
      try {
         result = RawListPtr(create_list(name, descr, false));
      } catch (const FieldException &) {
         LERROR(LECREATELIST);
         return {};
      } catch (const ListCoreError &) {
         LERROR(LECREATELIST);
         return {};
      }
      lList *dst = result.get().raw();

      // a selection from a list that was checked for duplicate keys when it was filled cannot create one
      const UniqueCheckSuspension suspension;
      for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         // the element form, with the list's descriptor, as lSelectDPack() called lSelectElemDPack()
         RawElementPtr copy = select_elem_into_descr(ep, where, dst->descr, what, hashing);
         if (!copy) {
            continue;
         }
         try {
            append_elem(dst, copy.get().raw());
            static_cast<void>(copy.release());
         } catch (const FieldException &) {
            // a new free element of the list's own descriptor: as lAppendElem() refusing it
            LERROR(LEAPPENDELEM);
            return {};
         }
      }

      if (hash) {
         // the list may be hashed from now on; its tables are built by the first lookups
         cull_hash_create_hashtables(dst);
         // no match is no list, as lSelectDPack() decided it
         if (dst->nelem == 0) {
            LERROR(LEGETNROFELEM);
            return {};
         }
      }
      return result;
   }

   RawElementPtr select_elem_into_descr(const lListElem *ep, const lCondition *where, const lDescr *descr,
                                        const lEnumeration *what, const Hashing hashing) {
      if (ep == nullptr || descr == nullptr) {
         return {};
      }
      // compare() throws only for a null element, excluded above; a comparison that cannot be made is false
      if (!compare(ep, where)) {
         return {};
      }
      RawElementPtr copy;
      try {
         copy = RawElementPtr(create_elem(descr));
      } catch (const FieldException &) {
         LERROR(LECOUNTDESCR);
         return {};
      } catch (const ListCoreError &e) {
         LERROR(e.lerrno());
         return {};
      }
      if (what == nullptr) {
         // lCopyElemPartialPack() refused a missing selection
         LERROR(LEENUMNULL);
         return {};
      }
      try {
         copy_elem_partial(copy.get().raw(), ep, what, hashing == Hashing::On, false);
      } catch (const FieldException &) {
         LERROR(LEELEMNULL);
         return {};
      } catch (const ListCoreError &e) {
         LERROR(e.lerrno());
         return {};
      }
      return copy;
   }

   void select_pack(sge_pack_buffer *pb, const char *name, const lList *lp, const lCondition *where,
                    const lEnumeration *what) {
      if (what == nullptr) {
         // no selection: the whole list under its own name, the condition not applied - as cull_pack_list()
         try {
            pack_list(pb, lp, nullptr, 0);
         } catch (const PackException &) {
            // not reported, as lSelectHashPack() did not report it
         }
         return;
      }

      // the header: whether there is a list, its name and the selection as descriptor; a failure ends the write
      try {
         const PackingTime timing;
         put_uint32(pb, lp != nullptr ? 1 : 0);
         if (lp == nullptr) {
            return;
         }
         put_str(pb, name != nullptr ? name : lp->listname);
         pack_what_as_descr(pb, what, lp->descr, 0);
      } catch (const PackException &) {
         return;
      }

      for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         // compare() throws only for a null element; a comparison that cannot be made is false
         if (!compare(ep, where)) {
            continue;
         }
         // each element is preceded by a 1; if that fails the element is skipped
         try {
            const PackingTime timing;
            put_uint32(pb, 1);
         } catch (const PackException &) {
            continue;
         }
         // a failed element write is not reported, as lCopyElemPartialPack() did not report it
         try {
            switch (what[0].pos) {
               case WHAT_ALL:
                  pack_elem(pb, ep, nullptr, 0);
                  break;
               case WHAT_NONE:
                  break;
               default:
                  pack_elem(pb, ep, what, 0);
                  break;
            }
         } catch (const PackException &) {
         }
      }

      // after the last element a 0: no more elements follow
      try {
         const PackingTime timing;
         put_uint32(pb, 0);
      } catch (const PackException &) {
      }
   }

   RawListPtr unpack_raw_list(sge_pack_buffer *pb, const int flags) {
      return RawListPtr(unpack_list(pb, flags));
   }

   RawElementPtr unpack_raw_elem(sge_pack_buffer *pb, const lDescr *descr, const int flags) {
      return RawElementPtr(unpack_elem(pb, descr, flags));
   }

   void write_to(FILE *fp, const ConstRawList list) {
      write_text(fp, [list](dstring *buffer) { print_list(buffer, list.raw(), 0); });
   }

   void write_to(FILE *fp, const ConstRawElement element) {
      write_text(fp, [element](dstring *buffer) { print_elem(buffer, element.raw(), 0); });
   }

   void write_to(FILE *fp, const lCondition *where) {
      write_text(fp, [where](dstring *buffer) { print_where(buffer, where, 0); });
   }

   void write_to(FILE *fp, const lEnumeration *what) {
      write_text(fp, [what](dstring *buffer) { print_what(buffer, what, 0); });
   }

   void free_raw_list(lList *&lp) noexcept {
      try {
         free_list(lp);
      } catch (const FieldException &) {
         // an element without descriptor: the list is corrupted, lFreeList() aborted here
         set_lerrno(LEDESCRNULL);
         TerminationManager::trigger_abort();
      }
   }

   void free_raw_elem(lListElem *&ep) noexcept {
      try {
         free_elem(ep);
      } catch (const FieldException &) {
         // an element without descriptor: lFreeElem() aborted here
         set_lerrno(LEDESCRNULL);
         TerminationManager::trigger_abort();
      }
   }

   std::size_t remaining(const ConstRawElement element) noexcept {
      std::size_t n = 0;
      if (element) {
         for (const lListElem *next = element.raw()->next; next != nullptr; next = next->next) {
            n++;
         }
      }
      return n;
   }

   bool is_free(const ConstRawElement element) noexcept {
      return element && elem_kind(element.raw()) == FREE_ELEM;
   }

   void make_hashable(const RawList list) noexcept {
      cull_hash_create_hashtables(list.raw());
   }

   void release_descr(const lDescr *&descr) noexcept {
      ocs_cull_descr_release(&descr);
   }

   bool descr_registry_shared(void *handle, std::string &own, std::string &seen) {
      return ocs_cull_descr_registry_shared(handle, &own, &seen);
   }

} // namespace ocs::tom
