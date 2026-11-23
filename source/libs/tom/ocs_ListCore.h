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
 * @brief The list structure and ownership in the core of the typed object model
 *
 * Creating, freeing, copying, appending, inserting, removing and dechaining
 * elements and lists. The functions are untyped and throw on every error; the
 * typed handles call them after their own checks, and the old functions of
 * the list library (lCreateList(), lAppendElem(), ...) are thin wrappers that
 * map the exceptions back to what they returned before (N-L-1, N-L-2).
 *
 * Errors:
 * - a programming error throws FieldException: a null list (NullList), a null
 *   element (NullElement), no descriptor (NullDescriptor), an element bound in
 *   a list or an object (BoundElement), an element of another list
 *   (ForeignList), lists of different types (WrongType), an element or a list
 *   whose descriptor has another layout than the list's (WrongLayout), a
 *   duplicate key in a unique field in a debug build (DuplicateKey);
 * - a failure at run time throws ListCoreError, whose message says what failed.
 */

#include <cstdint>
#include <string>

#include "tom/ocs_Types.h"
#include "tom/ocs_FieldTraits.h"
#include "uti/ocs_Exception.h"

namespace ocs::tom {

   /**
    * @brief A run-time failure of the list core, such as no memory
    */
   class ListCoreError : public ocs::RuntimeException {
   public:
      /**
       * @brief Describe the failure
       * @param message what failed, returned by what()
       */
      explicit ListCoreError(const std::string &message);

      /** @brief Destroy the exception; defined out of line as the key function */
      ~ListCoreError() override;
   };

   /**
    * @brief Create an element with every field at its default, as lCreateElem()
    * @param descr the descriptor; a reduced one gives a reduced element
    * @return the element, free, owned by the caller
    * @throw FieldException with reason NullDescriptor if @p descr is null or empty
    * @throw ListCoreError if there is no memory
    */
   lListElem *create_elem(const lDescr *descr);

   /**
    * @brief Create an empty list, as lCreateListHash()
    * @param name the list name, nullptr for "No list name specified"
    * @param descr the descriptor of its elements
    * @param hash whether the list keeps hash tables for its hashed fields
    * @return the list, owned by the caller
    * @throw FieldException with reason NullDescriptor if @p descr is null or empty
    * @throw ListCoreError if there is no memory
    */
   lList *create_list(const char *name, const lDescr *descr, bool hash);

   /**
    * @brief Free an element with its strings, sub-lists and sub-objects, as lFreeElem()
    *
    * A bound element is taken out of the hash tables of its list first; the
    * caller unlinks it from the list.
    *
    * @param ep the element, nullptr afterwards; nullptr frees nothing
    * @throw FieldException with reason NullDescriptor if the element has no descriptor
    */
   void free_elem(lListElem *&ep);

   /**
    * @brief Free a list with its elements, as lFreeList()
    * @param lp the list, nullptr afterwards; nullptr frees nothing
    */
   void free_list(lList *&lp);

   /**
    * @brief Copy an element deeply, as lCopyElemHash()
    * @param ep the element
    * @param hash whether sub-lists of the copy keep hash tables
    * @param skip_no_transfer whether fields marked as not to transfer are left out
    * @return the copy, free, owned by the caller
    * @throw FieldException with reason NullElement if @p ep is null
    * @throw ListCoreError if the copy fails
    */
   lListElem *copy_elem(const lListElem *ep, bool hash, bool skip_no_transfer);

   /**
    * @brief Copy a list deeply, as lCopyListHash()
    * @param name the name of the copy, nullptr for the name of @p src
    * @param src the list
    * @param hash whether the copy keeps hash tables
    * @param skip_no_transfer whether fields marked as not to transfer are left out
    * @return the copy, owned by the caller
    * @throw FieldException with reason NullList if @p src is null
    * @throw ListCoreError if the copy fails
    */
   lList *copy_list(const char *name, const lList *src, bool hash, bool skip_no_transfer);

   /**
    * @brief Copy one field of an element into another, as lCopySwitchPack() copied it
    *
    * Numbers and references are copied, strings and host names duplicated,
    * a sub-list or sub-object copied deeply, or reduced to a selection when
    * one is given. A sub-list that a selection empties is no list when the
    * copy may be hashed, as the old selection gave it. With skip_no_transfer a
    * field marked MT_NO_TRANSFER is left at its default.
    *
    * @param src the element copied from
    * @param dst the element copied to; the field's old value is overwritten, not freed
    * @param src_pos the field's position in @p src
    * @param dst_pos the field's position in @p dst
    * @param hash true to build the hash tables of copied sub-lists
    * @param sub_what the selection for a sub-list or sub-object, or nullptr for a whole copy
    * @param skip_no_transfer true if the copy is handed out
    * @throw FieldException with reason NullElement if an element is null
    * @throw ListCoreError if there is no memory or the field has an unknown type
    */
   void copy_field(const lListElem *src, lListElem *dst, int src_pos, int dst_pos, bool hash,
                   const lEnumeration *sub_what, bool skip_no_transfer);

   /**
    * @brief The name of a list, as lGetListName() gives it for a list
    * @param lp the list, or nullptr
    * @return the name, or "No list name specified" for nullptr or a list without a name
    */
   const char *list_name(const lList *lp) noexcept;

   /**
    * @brief Insert a free element after another one or at the front, as lInsertElem()
    * @param lp the list, which takes the element over
    * @param after the predecessor in @p lp, nullptr for the front
    * @param ep the element
    * @throw FieldException with reason NullList, NullElement, BoundElement if @p ep is bound already,
    *        WrongLayout if @p ep was created with a descriptor of another layout than the list's (CS-3112),
    *        or DuplicateKey in a debug build; nothing is changed then
    */
   void insert_elem(lList *lp, lListElem *after, lListElem *ep);

   /**
    * @brief Append a free element, as lAppendElem()
    * @param lp the list, which takes the element over
    * @param ep the element
    * @throw FieldException with reason NullList, NullElement, BoundElement if @p ep is bound already,
    *        WrongLayout if @p ep was created with a descriptor of another layout than the list's (CS-3112),
    *        or DuplicateKey in a debug build; nothing is changed then
    */
   void append_elem(lList *lp, lListElem *ep);

   /**
    * @brief Take an element out of its list without freeing it, as lDechainElem()
    * @param lp the list
    * @param ep the element, free afterwards and owned by the caller
    * @throw FieldException with reason NullList, NullElement or ForeignList if @p ep is not in @p lp;
    *        nothing is changed then
    */
   void dechain_elem(lList *lp, lListElem *ep);

   /**
    * @brief Take an element out of its list and free it, as lRemoveElem()
    * @param lp the list
    * @param ep the element, nullptr afterwards
    * @throw FieldException with reason NullList, NullElement or ForeignList; nothing is changed then
    */
   void remove_elem(lList *lp, lListElem *&ep);

   /**
    * @brief Move every element of a list to the end of another, as lAppendList()
    * @param dst the list taking the elements
    * @param src the list giving them, empty afterwards
    * @throw FieldException with reason NullList, WrongType if the lists are of different types, WrongLayout
    *        if they are of one type with descriptors of different layouts, or DuplicateKey in a debug build;
    *        in the last case the elements before the duplicate are moved and
    *        the others stay in @p src
    */
   void append_list(lList *dst, lList *src);

   /**
    * @brief Put a list into a list field and take out the one held, as lXchgList() after its checks
    * @param ep the element
    * @param pos the position of the list field in the element's descriptor
    * @param lp the list to put in; the list held before afterwards
    */
   void exchange_list(lListElem *ep, int pos, lList *&lp) noexcept;

   /**
    * @brief Take the element out of an object field, as lDechainObject() after its checks
    *
    * The field holds nullptr afterwards; the element is free again and owned by the caller. Nothing is copied.
    *
    * @param ep the element holding the object field
    * @param pos the position of the object field in the element's descriptor
    * @return the element the field held, or nullptr if it held none
    */
   [[nodiscard]] lListElem *take_object(lListElem *ep, int pos) noexcept;

   /**
    * @brief Add an element with a string key, as lAddElemStr()
    *
    * Creates the list with @p descr if @p lp is nullptr, creates an element
    * with the list's descriptor, sets the key and appends the element. An
    * existing list may be reduced; it must hold the key field (CS-3112).
    *
    * @param lp the list, created if nullptr
    * @param nm the key field, a string field of @p descr
    * @param key the key, copied
    * @param descr the descriptor of a list created here; the element gets the list's descriptor
    * @return the element, now in the list
    * @throw FieldException with reason NullDescriptor, NullKey, AbsentField if @p descr does not hold
    *        @p nm, WrongType if it is not a string field or if the list is of another type or reduced
    *        without the field, or DuplicateKey in a debug build; a list created here is freed again then
    * @throw ListCoreError if there is no memory
    */
   lListElem *add_elem_str(lList *&lp, int nm, const char *key, const lDescr *descr);

   /**
    * @brief Add an element with a host key, as lAddElemHost(); see #add_elem_str
    * @param lp the list, created if nullptr
    * @param nm the key field, a host field of @p descr
    * @param key the host name, copied
    * @param descr the descriptor of a list created here; the element gets the list's descriptor
    * @return the element, now in the list
    * @throw FieldException and ListCoreError as #add_elem_str
    */
   lListElem *add_elem_host(lList *&lp, int nm, const char *key, const lDescr *descr);

   /**
    * @brief Add an element with a 32 bit key, as lAddElemUlong(); see #add_elem_str
    * @param lp the list, created if nullptr
    * @param nm the key field, a 32 bit field of @p descr
    * @param key the key
    * @param descr the descriptor of a list created here; the element gets the list's descriptor
    * @return the element, now in the list
    * @throw FieldException and ListCoreError as #add_elem_str, without NullKey
    */
   lListElem *add_elem_ulong(lList *&lp, int nm, uint32_t key, const lDescr *descr);

   /**
    * @brief Add an element with a 64 bit key, as lAddElemUlong64(); see #add_elem_str
    * @param lp the list, created if nullptr
    * @param nm the key field, a 64 bit field of @p descr
    * @param key the key
    * @param descr the descriptor of a list created here; the element gets the list's descriptor
    * @return the element, now in the list
    * @throw FieldException and ListCoreError as #add_elem_str, without NullKey
    */
   lListElem *add_elem_ulong64(lList *&lp, int nm, uint64_t key, const lDescr *descr);

   /**
    * @brief Add an element with a key at a known position, without the checks of #add_elem_str
    *
    * For callers that know the key field at compile time: the typed handles,
    * the free forms, and the functions above after their checks. Instantiated
    * in the core for the four key field types.
    *
    * @tparam T the key field's type: Uint32, Uint64, String or Host
    * @param lp the list, created if nullptr
    * @param pos the position of the key field in @p descr, a field of type @p T
    * @param key the key; a string or host name not nullptr, copied
    * @param descr the descriptor of a list created here, not nullptr; the element gets the list's descriptor
    * @return the element, now in the list
    * @throw FieldException with reason WrongType if the list is of another type or reduced without the key
    *        field, or DuplicateKey in a debug build; a list created here is freed again then
    * @throw ListCoreError if there is no memory
    */
   template <FieldType T>
      requires KeyFieldType<T>
   lListElem *add_elem_at(lList *&lp, int pos, KeyArgOf<T> key, const lDescr *descr);

   /**
    * @brief Remove and free the first element with a string key, as lDelElemStr() without freeing the list
    * @param lp the list, nullptr for no element
    * @param nm the key field, a string field of the list's type
    * @param key the key
    * @return true if an element was removed
    * @throw FieldException with reason NullKey, AbsentField, WrongType, or DuplicateKey from the lookup
    */
   bool remove_elem_str(lList *lp, int nm, const char *key);

   /**
    * @brief Remove and free the first element with a host key, as lDelElemHost(); see #remove_elem_str
    * @param lp the list, nullptr for no element
    * @param nm the key field, a host field of the list's type
    * @param key the host name
    * @return true if an element was removed
    * @throw FieldException as #remove_elem_str
    */
   bool remove_elem_host(lList *lp, int nm, const char *key);

   /**
    * @brief Remove and free the first element with a 32 bit key, as lDelElemUlong(); see #remove_elem_str
    * @param lp the list, nullptr for no element
    * @param nm the key field, a 32 bit field of the list's type
    * @param key the key
    * @return true if an element was removed
    * @throw FieldException as #remove_elem_str, without NullKey
    */
   bool remove_elem_ulong(lList *lp, int nm, uint32_t key);

   /**
    * @brief Remove and free the first element with a 64 bit key, as lDelElemUlong64(); see #remove_elem_str
    * @param lp the list, nullptr for no element
    * @param nm the key field, a 64 bit field of the list's type
    * @param key the key
    * @return true if an element was removed
    * @throw FieldException as #remove_elem_str, without NullKey
    */
   bool remove_elem_ulong64(lList *lp, int nm, uint64_t key);

   /**
    * @brief Remove and free the first element with a key at a known position, without the checks of
    *        #remove_elem_str
    *
    * For callers that checked the list's type, the field and the key at
    * compile time or before. Instantiated in the core for the four key field
    * types.
    *
    * @tparam T the key field's type: Uint32, Uint64, String or Host
    * @param lp the list, nullptr for no element
    * @param pos the position of the key field, of type @p T and present in the list's descriptor
    * @param key the key; a string or host name not nullptr
    * @return true if an element was removed
    * @throw FieldException with reason DuplicateKey from the lookup
    */
   template <FieldType T>
      requires KeyFieldType<T>
   bool remove_elem_at(lList *lp, int pos, KeyArgOf<T> key);

   /**
    * @brief The list of a list field, created if the field holds none, as lGetOrCreateList()
    * @param ep the element, not nullptr
    * @param pos the position of the list field in the element's descriptor
    * @param name the name of a list created here, nullptr for the field's name as the installed object model gives
    *        it ("nm <n>" without a model)
    * @param descr the descriptor of the elements of a list created here
    * @return the list held in the field
    * @throw FieldException with reason WrongType if the field is not a list field, NullDescriptor
    * @throw ListCoreError if there is no memory
    */
   lList *get_or_create_list(lListElem *ep, int pos, const char *name, const lDescr *descr);

   /**
    * @brief Set the list length below which the calling thread searches a string key field linearly
    *
    * The value holds for the calling thread only. A thread that works on lists of its own, such as the scheduler
    * on its run copy, can so hash short string-keyed lists without changing the other threads.
    *
    * @param n the new threshold; 0 builds a hash table on the first lookup of any hashable list
    */
   void set_thread_string_hash_threshold(std::uint32_t n);

   /**
    * @brief Set the list length below which the calling thread searches a host key field linearly
    *
    * The value holds for the calling thread only, like set_thread_string_hash_threshold().
    *
    * @param n the new threshold; 0 builds a hash table on the first lookup of any hashable list
    */
   void set_thread_host_hash_threshold(std::uint32_t n);

} // namespace ocs::tom
