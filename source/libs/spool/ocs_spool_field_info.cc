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
 * @brief The accessor sets of the spooling records, one per field type
 *
 * The walkers read and write a field through the #ocs_spool_accessors its #ocs_spool_field_ops record points at.
 * There is one set per ocs::tom::FieldType, not one per field, so the code here exists once for all fields of all
 * object types.
 *
 * Type fidelity is guaranteed when a row is built, not here: a record is made by ocs_spool_make_ops() from the
 * model's field table for exactly its field, so the accessor set it names is that of the field's type. The
 * accessors look the field up by its number in the element's own descriptor (lGetPosViaElem(), without abort);
 * a field number is unique across the object types, so an element of another type does not hold the field, nor
 * does a reduced element the field is left out of. Such an element - and a nullptr - takes the soft path: a getter
 * gives zero or nullptr, a setter stores nothing and frees a list or an element it was to take over. Nothing here
 * throws: the walkers run in the worker threads of qmaster and at its start. The accessors by position of the
 * list library do the rest; their check of the field type cannot fail, as the record's type is the field's.
 *
 * Called per field by the walkers, so the accessors have no DENTER, as the walkers' per field helpers have none.
 */

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_object.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/ocs_spool_field_info.h"

namespace {

   /**
    * @brief The position of a field in an element, for the accessors
    * @param ep the element, or nullptr
    * @param nm the field number
    * @return the position, -1 if the element is nullptr or does not hold the field
    */
   int ocs_spool_pos(const lListElem *ep, const int nm) {
      return lGetPosViaElem(ep, nm, SGE_NO_ABORT);
   }

   /**
    * @brief Read an unsigned 32 bit field
    * @param ep the element
    * @param nm the field number
    * @return the value in `u32`, 0 if the element does not hold the field
    */
   ocs_spool_value ocs_spool_get_uint32(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.u32 = pos >= 0 ? lGetPosUlong(ep, pos) : 0;
      return value;
   }

   /**
    * @brief Store an unsigned 32 bit field
    * @param ep the element
    * @param nm the field number
    * @param value the value in `u32`
    */
   void ocs_spool_set_uint32(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosUlong(ep, pos, value.u32);
      }
   }

   /**
    * @brief Read an unsigned 64 bit field
    * @param ep the element
    * @param nm the field number
    * @return the value in `u64`, 0 if the element does not hold the field
    */
   ocs_spool_value ocs_spool_get_uint64(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.u64 = pos >= 0 ? lGetPosUlong64(ep, pos) : 0;
      return value;
   }

   /**
    * @brief Store an unsigned 64 bit field
    * @param ep the element
    * @param nm the field number
    * @param value the value in `u64`
    */
   void ocs_spool_set_uint64(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosUlong64(ep, pos, value.u64);
      }
   }

   /**
    * @brief Read a signed 32 bit field
    * @param ep the element
    * @param nm the field number
    * @return the value in `i32`, 0 if the element does not hold the field
    */
   ocs_spool_value ocs_spool_get_int32(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.i32 = pos >= 0 ? lGetPosInt(ep, pos) : 0;
      return value;
   }

   /**
    * @brief Store a signed 32 bit field
    * @param ep the element
    * @param nm the field number
    * @param value the value in `i32`
    */
   void ocs_spool_set_int32(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosInt(ep, pos, value.i32);
      }
   }

   /**
    * @brief Read a signed 64 bit field
    * @param ep the element
    * @param nm the field number
    * @return the value in `i64`, 0 if the element does not hold the field
    */
   ocs_spool_value ocs_spool_get_int64(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.i64 = pos >= 0 ? lGetPosLong(ep, pos) : 0;
      return value;
   }

   /**
    * @brief Store a signed 64 bit field
    * @param ep the element
    * @param nm the field number
    * @param value the value in `i64`
    */
   void ocs_spool_set_int64(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosLong(ep, pos, value.i64);
      }
   }

   /**
    * @brief Read a double field
    * @param ep the element
    * @param nm the field number
    * @return the value in `dbl`, 0.0 if the element does not hold the field
    */
   ocs_spool_value ocs_spool_get_double(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.dbl = pos >= 0 ? lGetPosDouble(ep, pos) : 0.0;
      return value;
   }

   /**
    * @brief Store a double field
    * @param ep the element
    * @param nm the field number
    * @param value the value in `dbl`
    */
   void ocs_spool_set_double(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosDouble(ep, pos, value.dbl);
      }
   }

   /**
    * @brief Read a bool field
    * @param ep the element
    * @param nm the field number
    * @return the value in `b`, false if the element does not hold the field
    */
   ocs_spool_value ocs_spool_get_bool(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.b = pos >= 0 && lGetPosBool(ep, pos);
      return value;
   }

   /**
    * @brief Store a bool field
    * @param ep the element
    * @param nm the field number
    * @param value the value in `b`
    */
   void ocs_spool_set_bool(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosBool(ep, pos, value.b);
      }
   }

   /**
    * @brief Read a string field
    * @param ep the element
    * @param nm the field number
    * @return the string in `str`, pointing into the element; nullptr if unset or if the element does not hold
    *         the field
    */
   ocs_spool_value ocs_spool_get_string(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.str = pos >= 0 ? lGetPosString(ep, pos) : nullptr;
      return value;
   }

   /**
    * @brief Store a string field
    *
    * A string that cannot be copied leaves the field as it was, as lSetPosString() does.
    *
    * @param ep the element
    * @param nm the field number
    * @param value the string in `str`, or nullptr; copied
    */
   void ocs_spool_set_string(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosString(ep, pos, value.str);
      }
   }

   /**
    * @brief Read a host name field
    * @param ep the element
    * @param nm the field number
    * @return the host name in `str`, pointing into the element; nullptr if unset or if the element does not hold
    *         the field
    */
   ocs_spool_value ocs_spool_get_host(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      ocs_spool_value value{};
      value.str = pos >= 0 ? lGetPosHost(ep, pos) : nullptr;
      return value;
   }

   /**
    * @brief Store a host name field
    *
    * A host name that cannot be copied leaves the field as it was, as lSetPosHost() does.
    *
    * @param ep the element
    * @param nm the field number
    * @param value the host name in `str`, or nullptr; copied
    */
   void ocs_spool_set_host(lListElem *ep, const int nm, const ocs_spool_value value) {
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosHost(ep, pos, value.str);
      }
   }

   /**
    * @brief Read a list field
    * @param ep the element
    * @param nm the field number
    * @return the list, nullptr if unset or if the element does not hold the field
    */
   const lList *ocs_spool_get_list(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      return pos >= 0 ? lGetPosList(ep, pos) : nullptr;
   }

#if defined(ENABLE_DEBUG_CHECKS)
   /**
    * @brief Abort when a list or an element of another sub-type is about to be stored into a field (debug builds)
    *
    * The typed setters checked the sub-type with their handles (WrongSubType); the shared setters are handed
    * lists and elements the reader created with the field's sub-type, so a mismatch is a programming error.
    *
    * @param nm the list or object field
    * @param descr the descriptor of the list or element to store, nullptr for none
    * @param what "list" or "element", for the message
    */
   void ocs_spool_check_sub_type(const int nm, const lDescr *descr, const char *what) {
      DENTER(TOP_LAYER);
      const lDescr *sub = object_get_subtype(nm);
      if (descr != nullptr && sub != nullptr && descr[0].nm != sub[0].nm) {
         CRITICAL("spooling stores a %s of another sub-type into field %s", what, lNm2Str(nm));
         ocs::TerminationManager::trigger_abort();
      }
      DRETURN_VOID;
   }
#endif

   /**
    * @brief Store a list field, taking the list over
    *
    * The list held before is freed. The list is the reader's, created with the field's sub-type, so it is stored
    * as it is, as the typed setter did with ListPtr::from_typed().
    *
    * @param ep the element
    * @param nm the field number
    * @param list the list, or nullptr; taken over, and freed if the element does not hold the field; in a debug
    *             build a list of another sub-type aborts the process
    */
   void ocs_spool_set_list(lListElem *ep, const int nm, lList *list) {
#if defined(ENABLE_DEBUG_CHECKS)
      ocs_spool_check_sub_type(nm, list != nullptr ? lGetListDescr(list) : nullptr, "list");
#endif
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosList(ep, pos, list);
      } else {
         lFreeList(&list);
      }
   }

   /**
    * @brief Read an object field
    * @param ep the element
    * @param nm the field number
    * @return the element of the field, nullptr if unset or if the element does not hold the field
    */
   const lListElem *ocs_spool_get_object(const lListElem *ep, const int nm) {
      const int pos = ocs_spool_pos(ep, nm);
      return pos >= 0 ? lGetPosObject(ep, pos) : nullptr;
   }

   /**
    * @brief Store an object field, taking the element over
    *
    * The element held before is freed. The element is the reader's, created with the field's sub-type and bound
    * nowhere; an element bound elsewhere is refused by lSetPosObject() and stays with its holder.
    *
    * @param ep the element
    * @param nm the field number
    * @param object the element, or nullptr; taken over, and freed if the element does not hold the field
    */
   void ocs_spool_set_object(lListElem *ep, const int nm, lListElem *object) {
#if defined(ENABLE_DEBUG_CHECKS)
      ocs_spool_check_sub_type(nm, object != nullptr ? lGetElemDescr(object) : nullptr, "element");
#endif
      const int pos = ocs_spool_pos(ep, nm);
      if (pos >= 0) {
         lSetPosObject(ep, pos, object);
      } else {
         lFreeElem(&object);
      }
   }

} // namespace

/* *INDENT-OFF* */

const ocs_spool_accessors ocs_spool_access_uint32{&ocs_spool_get_uint32, &ocs_spool_set_uint32,
                                                  nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_uint64{&ocs_spool_get_uint64, &ocs_spool_set_uint64,
                                                  nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_int32{&ocs_spool_get_int32, &ocs_spool_set_int32,
                                                 nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_int64{&ocs_spool_get_int64, &ocs_spool_set_int64,
                                                 nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_double{&ocs_spool_get_double, &ocs_spool_set_double,
                                                  nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_bool{&ocs_spool_get_bool, &ocs_spool_set_bool,
                                                nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_string{&ocs_spool_get_string, &ocs_spool_set_string,
                                                  nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_host{&ocs_spool_get_host, &ocs_spool_set_host,
                                                nullptr, nullptr, nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_list{nullptr, nullptr, &ocs_spool_get_list, &ocs_spool_set_list,
                                                nullptr, nullptr};
const ocs_spool_accessors ocs_spool_access_object{nullptr, nullptr, nullptr, nullptr,
                                                  &ocs_spool_get_object, &ocs_spool_set_object};
const ocs_spool_accessors ocs_spool_access_none{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

/* *INDENT-ON* */
