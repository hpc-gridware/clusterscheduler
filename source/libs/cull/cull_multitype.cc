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
 * @brief Reading and writing the fields of a cull element
 */

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
/// Suppresses the monitoring code in the rmon macros for this file
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"

#include "cull/msg_cull.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_listP.h"
#include "cull/cull_stats.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_hash.h"

#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_SoftPath.h"

#define CULL_BASIS_LAYER CULL_LAYER ///< rmon layer this file logs under

namespace {

   /**
    * @brief The position of a field for a getter by name, through the core of the typed object model
    *
    * The core checks; this maps its exception back to what the old getters
    * did on every error: log the message of the error and abort.
    *
    * @param ep the element
    * @param name the field number
    * @param type the field type the getter reads
    * @param wrong_type_fmt the getter's message for a field of another field type
    * @return the field's position; does not return on an error
    */
   int legacy_getter_pos(const lListElem *ep, const int name, const ocs::tom::FieldType type,
                         const char *wrong_type_fmt) {
      DENTER(CULL_BASIS_LAYER);
      try {
         DRETURN(ocs::tom::checked_pos(ep, name, type));
      } catch (const ocs::tom::FieldException &e) {
         switch (e.reason()) {
            case ocs::tom::FieldException::Reason::NullElement:
               CRITICAL(MSG_CULL_POINTER_NULLELEMENTFORX_S, lNm2Str(name));
               break;
            case ocs::tom::FieldException::Reason::WrongType: {
               const int pos = ocs::tom::field_pos(ep->descr, name);
               incompatibleType2(wrong_type_fmt, lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
               break;
            }
            default:
               CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(name));
               break;
         }
         ocs::TerminationManager::trigger_abort();
      }
      DRETURN(-1);
   }

   /** @brief What an old setter by name does with a field the element does not hold */
   enum class LegacyMissing {
      Silent,   ///< return -1 without an error number
      NegPos,   ///< return -1 with LENEGPOS
      Abort     ///< log the "no such name" message and abort
   };

   /**
    * @brief The position of a field for a setter by name, through the core of the typed object model
    *
    * The core checks; this maps its exception back to what the old setter of
    * the field type did: -1 with LEELEMNULL for a null element, the setter's
    * own behaviour for a field the element does not hold, and an abort for a
    * field of another field type.
    *
    * @param ep the element
    * @param name the field number
    * @param type the field type the setter writes
    * @param missing what the setter does with a field the element does not hold
    * @param wrong_type_fmt the setter's message for a field of another field type
    * @param no_such_name_fmt the setter's message for a missing field, if it aborts on one
    * @param setter the setter's name, for the debug message of a silent miss and the record of a soft path
    * @return the field's position, or -1 where the old setter returned -1
    */
   int legacy_setter_pos(const lListElem *ep, const int name, const ocs::tom::FieldType type,
                         const LegacyMissing missing, const char *wrong_type_fmt, const char *no_such_name_fmt,
                         const char *setter) {
      DENTER(CULL_BASIS_LAYER);
      try {
         DRETURN(ocs::tom::checked_pos(ep, name, type));
      } catch (const ocs::tom::FieldException &e) {
         switch (e.reason()) {
            case ocs::tom::FieldException::Reason::NullElement:
               ocs::tom::report_soft_path(setter, name, ocs::tom::SoftPathReason::NullElement);
               LERROR(LEELEMNULL);
               break;
            case ocs::tom::FieldException::Reason::WrongType: {
               const int pos = ocs::tom::field_pos(ep->descr, name);
               incompatibleType2(wrong_type_fmt, lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
               break;
            }
            default:
               if (missing != LegacyMissing::Abort) {
                  ocs::tom::report_soft_path(setter, name,
                                             e.reason() == ocs::tom::FieldException::Reason::ForeignField
                                                ? ocs::tom::SoftPathReason::ForeignField
                                                : ocs::tom::SoftPathReason::MissingField);
               }
               if (missing == LegacyMissing::NegPos) {
                  LERROR(LENEGPOS);
               } else if (missing == LegacyMissing::Abort) {
                  incompatibleType2(no_such_name_fmt, name, lNm2Str(name));
               } else {
                  DPRINTF("!!!!!!!!!! %s(): %s not found in element !!!!!!!!!!\n", setter, lNm2Str(name));
               }
               break;
         }
      }
      DRETURN(-1);
   }

} // namespace

/**
 * @brief Get Position of name within element
 *
 * Get Position of field 'name' within 'element'
 *
 * @param element element
 * @param name field name id
 * @param do_abort call do_abort if do_abort=1
 *
 * @return position or -1 in case of an error
 */
int lGetPosViaElem(const lListElem *element, int name, int do_abort) {
   DENTER(CULL_BASIS_LAYER);

   int pos = -1;

   if (!element) {
      if (do_abort) {
         CRITICAL(MSG_CULL_POINTER_NULLELEMENTFORX_S, lNm2Str(name));
         ocs::TerminationManager::trigger_abort();
      }
      DRETURN(-1);
   }
   pos = lGetPosInDescr(element->descr, name);

   if (do_abort && (pos < 0)) {
      /* someone has called lGetPosViaElem() with invalid name */
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(name));
      ocs::TerminationManager::trigger_abort();
   }

   DRETURN(pos);
}

/**
 * @brief Returns the string representation of a type id
 *
 * @param mt the field type, e.g. lUlongT
 * @return the name, e.g. "lUlongT", or "unknown multitype"
 *
 * @note A thin wrapper over ocs::tom::type_name() of the core (CS-3077).
 */
const char *lMt2Str(int mt) {
   return ocs::tom::type_name(mt);
}

/**
 * @brief Returns the length of the type of a descriptor (N-B-7)
 *
 * The number of entries without the terminating #lEndT entry. The number of
 * values an element holds is lCountPresent().
 *
 * @param dp pointer to descriptor
 *
 * @return length or -1 on error
 *
 * @note MT-NOTE: lCountDescr() is MT safe
 *
 * @note A thin wrapper over ocs::tom::descr_length().
 */
int lCountDescr(const lDescr *dp) {
   if (!dp) {
      LERROR(LEDESCRNULL);
      return -1;
   }
   return ocs::tom::descr_length(dp);
}

/**
 * @brief Position of a field in a descriptor, by a full search
 *
 * Walks the entries instead of computing the position, so it also works on a
 * descriptor without header. A field the descriptor does not hold is not found.
 *
 * @param dp descriptor
 * @param name the field
 *
 * @return position or -1 if not found or absent
 */
int _lGetPosInDescr(const lDescr *dp, int name) {
   const lDescr *ldp;

   if (!dp) {
      LERROR(LEDESCRNULL);
      return -1;
   }

   for (ldp = dp; ldp->nm != name && ldp->nm != NoName; ldp++) { ;
   }

   if (ldp->nm == NoName || ldp->off == OCS_CULL_ABSENT) {
      LERROR(LENAMENOT);
      return -1;
   }

   return ldp - dp;
}

/**
 * @brief Position of a field in a descriptor (CS-439, N-B-2)
 *
 * One path for every descriptor, full or reduced, since all have the length of
 * their type: the position is the field number minus that of entry 0, checked
 * against the length from the header and against the entry, so a field of
 * another type is not found. A field a reduced descriptor does not hold is not
 * found either (N-B-4).
 *
 * @param dp a static or counted descriptor, as lists, elements and types hold
 * @param name the field
 *
 * @return position or -1 if not found or absent
 */
int lGetPosInDescr(const lDescr *dp, int name) {
   if (!dp) {
      LERROR(LEDESCRNULL);
      return -1;
   }
   CULL_STATS_TYPE(dp->nm, POS_CALLS, 1);

   // the core computes the position; a field the reduced descriptor does not hold is a miss here as well
   return ocs::tom::present_pos(dp, name);
}

/**
 * @brief Returns type at position
 *
 * Returns the type at specified position in a descriptor array. The
 * Position must be inside the valid range of the descriptor. Returns
 * NoName if descriptor is nullptr or pos < 0.
 *
 * @param dp Descriptor
 * @param pos Position
 *
 * @return Type
 */
int lGetPosType(const lDescr *dp, int pos) {
   if (!dp) {
      LERROR(LEDESCRNULL);
      return (int) NoName;
   }
   if (pos < 0) {
      return (int) NoName;
   }
   return mt_get_type(dp[pos].mt);
}

/**
 * @brief Returns the sub-list held in a field, without copying, read only
 *
 * @param ep the element
 * @param name the field
 * @return the sub-list, or nullptr when the field is unset
 */
lList **lGetListRef(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lListT)
      incompatibleType("lGetPosListRef");

   DRETURN(&(ocs_cull_value(ep, pos).glp));
}

/**
 * @brief Returns the string held at a field position, without copying, read only
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 */
char **lGetPosStringRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lStringT)
      incompatibleType("lGetPosStringRef");

   DRETURN(&(ocs_cull_value(ep, pos).str));
}

/**
 * @brief Returns the host name held at a field position, without copying, read only
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 */
char **lGetPosHostRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType("lGetPosHostRef");

   DRETURN(&(ocs_cull_value(ep, pos).host));
}


/* 
   FOR THE lGet{Type} FUNCTIONS THERE IS NO REAL ERRORHANDLING
   IF EP IS nullptr THERE WILL BE A COREDUMP
   THIS IS NECESSARY, OTHERWISE IT WOULD BE DIFFICULT TO CASCADE
   THE GET FUNCTIONS
   SO HIGHER LEVEL FUNCTIONS OR THE USER SHOULD CHECK IF THE
   ARGUMENTS ARE ALLRIGHT.
 */

/**
 * @brief Returns the int value at position
 *
 * Returns the int value at position 'pos'
 *
 * @param ep element pointer
 * @param pos position id
 *
 * @return int
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lInt lGetPosInt(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lIntT)
      incompatibleType("lGetPosInt");

   DRETURN((lInt) ocs_cull_value(ep, pos).i);
}

/**
 * @brief Returns the int value for field name
 *
 * Returns the int value for field name
 *
 * @param ep element
 * @param name field name id
 *
 * @return int
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lInt lGetInt(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Int32, MSG_CULL_GETINT_WRONGTYPEFORFIELDXY_SS);

   DRETURN((lInt) ocs_cull_value(ep, pos).i);
}

/**
 * @brief Returns the ulong value at position pos
 *
 * Returns the ulong value at position pos
 *
 * @param ep element
 * @param pos pos value
 *
 * @return ulong
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lUlong lGetPosUlong(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSULONG_GOTINVALIDPOSITION);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT)
      incompatibleType("lGetPosUlong");
   DRETURN((lUlong) ocs_cull_value(ep, pos).ul);
}

/**
 * @brief Return 'uint32_t' value for specified fieldname
 *
 * Return the content of the field specified by fieldname 'name' of
 * list element 'ep'. The type of the field 'name' has to be of
 * type 'uint32_t'.
 *
 * @param ep Pointer to list element
 * @param name field name
 *
 * @return uint32_t value
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lUlong lGetUlong(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Uint32, MSG_CULL_GETULONG_WRONGTYPEFORFIELDXY_SS);

   DRETURN((lUlong) ocs_cull_value(ep, pos).ul);
}

/**
 * @brief Returns the ulong64 value at position pos
 *
 * Returns the ulong64 value at position pos
 *
 * @param ep element
 * @param pos pos value
 *
 * @return ulong64
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lUlong64 lGetPosUlong64(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong64() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSULONG64_GOTINVALIDPOSITION);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T)
      incompatibleType("lGetPosUlong64");
   DRETURN((lUlong64) ocs_cull_value(ep, pos).ul64);
}

/**
 * @brief Return 'uint64_t' value for specified fieldname
 *
 * Return the content of the field specified by fieldname 'name' of
 * list element 'ep'. The type of the field 'name' has to be of
 * type 'uint64_t'.
 *
 * @param ep Pointer to list element
 * @param name field name
 *
 * @return uint64_t value
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lUlong64 lGetUlong64(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Uint64, MSG_CULL_GETULONG64_WRONGTYPEFORFIELDXY_SS);

   DRETURN((lUlong64) ocs_cull_value(ep, pos).ul64);
}

/**
 * @brief Returns the string ptr value at position pos
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
const char *lGetPosString(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetString() */
      /* makro with an invalid nm        */
      DPRINTF("!!!!!!!!!!!! lGetPosString() got an invalid pos !!!!!!!!!\n");
      DRETURN(nullptr);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT)
      incompatibleType("lGetPosString");

   DRETURN((lString) ocs_cull_value(ep, pos).str);
}

/**
 * @brief Returns the hostname value at position pos
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
const char *lGetPosHost(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetString() */
      /* makro with an invalid nm        */
      DPRINTF("!!!!!!!!!!!!!! lGetPosHost() got an invalid pos !!!!!!!!!\n");
      DRETURN(nullptr);
   }

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType("lGetPosHost");
   DRETURN((lHost) ocs_cull_value(ep, pos).host);
}

/**
 * @brief Return type of field within descriptor
 *
 * Return type of field within descriptor.
 *
 * @param dp descriptor
 * @param nm field name id
 *
 * @return Type id or lEndT
 *
 * @note A thin wrapper over ocs::tom::field_type() of the core (CS-3077).
 */
int lGetType(const lDescr *dp, int nm) {
   if (dp == nullptr) {
      LERROR(LEDESCRNULL);
      return lEndT;
   }
   CULL_STATS_TYPE(dp->nm, POS_CALLS, 1);
   return ocs::tom::field_type(dp, nm);
}

/**
 * @brief Return string for specified fieldname
 *
 * Return the content of the field specified by fieldname 'name' of
 * list element 'ep'. The type of the field 'name' has to be of
 * type string.
 *
 * @param ep Pointer to list element
 * @param name field name
 *
 * @return string pointer (no copy)
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
const char *lGetString(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::String, MSG_CULL_GETSTRING_WRONGTYPEFORFILEDXY_SS);

   DRETURN((lString) ocs_cull_value(ep, pos).str);
}

/*
* NOTES
*     MT-NOTE: MT-safety depends on lGetString (which has no MT-NOTE)
*              lGetStringNotNull() itself is MT-safe
*/
/**
 * @brief The string held in a field, never nullptr
 *
 * @param ep the element
 * @param name the field
 * @return the string, or the empty string when the field is unset
 */
const char *lGetStringNotNull(const lListElem *ep, int name) {
   const char *ret = lGetString(ep, name);
   if (ret == nullptr) {
      ret = "";
   }
   return ret;
}

/**
 * @brief Return hostname string for specified field
 *
 * This procedure returns the hostname string for the field name,
 * but doesn't copy the string (runtime type checking)
 *
 * @param ep list element pointer
 * @param name name of list element
 *
 * @return value of list entry
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
const char *lGetHost(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Host, MSG_CULL_GETHOST_WRONGTYPEFORFILEDXY_SS);

   DRETURN((lHost) ocs_cull_value(ep, pos).host);
}

/**
 * @brief Returns the CULL object at position pos (no copy)
 *
 * @param ep the element
 * @param pos the field position
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lListElem *lGetPosObject(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSOBJECT_GOTANINVALIDPOS);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lObjectT)
      incompatibleType("lGetPosObject");

   DRETURN((lListElem *) ocs_cull_value(ep, pos).obj);
}

/**
 * @brief Returns the CULL list at position pos (no copy)
 *
 * @param ep the element
 * @param pos the field position
 * @return the sub-list, or nullptr when the field is unset
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lList *lGetPosList(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSLIST_GOTANINVALIDPOS);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT)
      incompatibleType("lGetPosList");

   DRETURN((lList *) ocs_cull_value(ep, pos).glp);
}

/**
 * @brief Returns the CULL object for a field name
 *
 * @param ep the element
 * @param name the field
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lListElem *lGetObject(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Object, MSG_CULL_GETOBJECT_WRONGTYPEFORFIELDXY_SS);
   DRETURN((lListElem *) ocs_cull_value(ep, pos).obj);
}

/**
 * @brief Returns the CULL list for a field name
 *
 * @param ep the element
 * @param name the field
 * @return the sub-list, or nullptr when the field is unset
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lList *lGetListRW(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::List, MSG_CULL_GETLIST_WRONGTYPEFORFIELDXY_SS);

   DRETURN((lList *) ocs_cull_value(ep, pos).glp);
}

/**
 * @brief Returns the sub-list held in a field, read only
 *
 * @param ep the element
 * @param nm the field
 * @return the sub-list, or nullptr when the field is unset
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
const lList *lGetList(const lListElem *ep, int nm) {
   return lGetListRW(ep, nm);
}

/**
 * @brief Returns the CULL list for a field name
 *
 * @param ep the element
 * @param name the field
 * @param list_name see the read/write variant
 * @param descr see the read/write variant
 * @return the sub-list, or nullptr when the field is unset
 *
 * @see Typed form: ocs::tom::Element<T>::get_or_create<F>(); a thin wrapper over ocs::tom::get_or_create_list() of the core.
 */
lList *lGetOrCreateList(lListElem *ep, int name,
                        const char *list_name, const lDescr *descr) {
   if (ep == nullptr) {
      ocs::tom::report_soft_path("lGetOrCreateList", name, ocs::tom::SoftPathReason::NullElement);
      return nullptr;
   }
   try {
      return ocs::tom::get_or_create_list(ep, lGetPosViaElem(ep, name, SGE_DO_ABORT),
                                          list_name != nullptr ? list_name : "No list name specified", descr);
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lGetOrCreateList", e);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Returns a double value at pos
 *
 * Returns a double value at pos
 *
 * @param ep element
 * @param pos pos
 *
 * @return double value
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lDouble lGetPosDouble(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lDoubleT)
      incompatibleType("lGetPosDouble");
   DRETURN(ocs_cull_value(ep, pos).db);
}

/**
 * @brief Returns the double value for field name
 *
 * Returns the double value for field name
 *
 * @param ep element
 * @param name field name value
 *
 * @return double value
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lDouble lGetDouble(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Double, MSG_CULL_GETDOUBLE_WRONGTYPEFORFIELDXY_SS);
   DRETURN(ocs_cull_value(ep, pos).db);
}

/**
 * @brief Returns the long value at position pos
 *
 * Returns the long value at position pos
 *
 * @param ep element
 * @param pos position
 *
 * @return long
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lLong lGetPosLong(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lLongT)
      incompatibleType("lGetPosLong");
   DRETURN(ocs_cull_value(ep, pos).l);
}

/**
 * @brief Returns the long value for a field name
 *
 * Returns the long value for a field name
 *
 * @param ep element
 * @param name name
 *
 * @return long
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lLong lGetLong(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Int64, MSG_CULL_GETLONG_WRONGTYPEFORFIELDXY_SS);
   DRETURN(ocs_cull_value(ep, pos).l);
}

/**
 * @brief Returns the boolean value at position pos
 *
 * Returns the boolean value at position pos
 *
 * @param ep element
 * @param pos position
 *
 * @return boolean
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lBool lGetPosBool(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lBoolT)
      incompatibleType("lGetPosBool");
   DRETURN(ocs_cull_value(ep, pos).b);
}

/**
 * @brief Returns the boolean value for a field name
 *
 * Returns the boolean value for a field name
 *
 * @param ep element
 * @param name field name
 *
 * @return boolean
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lBool lGetBool(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Bool, MSG_CULL_GETBOOL_WRONGTYPEFORFIELDXY_SS);
   DRETURN(ocs_cull_value(ep, pos).b);
}

/**
 * @brief Returns the reference at position pos
 *
 * Returns the reference at position pos
 *
 * @param ep element
 * @param pos position
 *
 * @return reference (pointer)
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lRef lGetPosRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lRefT) {
      incompatibleType("lGetPosRef");
   }
   DRETURN(ocs_cull_value(ep, pos).ref);
}

/**
 * @brief Returns the reference for a field name
 *
 * Returns the reference for a field name
 *
 * @param ep element
 * @param name field name value
 *
 * @return reference
 *
 * @see Typed form: ocs::tom::get<F>(ep) and ocs::tom::ConstElement<T>::get<F>(), or get<V>(nm) for a field known only at run time.
 */
lRef lGetRef(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Ref, MSG_CULL_GETREF_WRONGTYPEFORFIELDXY_SS);
   DRETURN(ocs_cull_value(ep, pos).ref);
}

/**
 * @brief Sets the int value
 *
 * Sets in the element 'ep' at position 'pos' the int 'value'
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosInt(lListElem *ep, int pos, int value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      ocs::tom::report_soft_path("lSetPosInt", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosInt", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lIntT) {
      incompatibleType("lSetPosInt");
      DRETURN(-1);
   }

   if (ocs_cull_value(ep, pos).i != value) {
      ocs_cull_value(ep, pos).i = value;
   }

   DRETURN(0);
}

/**
 * @brief Sets an int within an element
 *
 * Sets an int within an element
 *
 * @param ep element
 * @param name field name id
 * @param value new value
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetInt(lListElem *ep, int name, int value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Int32, LegacyMissing::NegPos, MSG_CULL_SETINT_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetInt");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosInt(ep, pos, value));
}

/**
 * @brief Get ulong at a certain position
 *
 * Get ulong at a certain position
 *
 * @param ep element
 * @param pos position
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosUlong(lListElem *ep, int pos, lUlong value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      ocs::tom::report_soft_path("lSetPosUlong", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosUlong", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType("lSetPosUlong");
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).ul) {
      if (ocs::tom::needs_hashed_store(ep, pos)) {
         try {
            ocs::tom::store_hashed_number(ep, pos, value);
         } catch (const ocs::tom::FieldException &e) {
            ocs::tom::legacy_abort("lSetPosUlong", e);
         }
      } else {
         ocs_cull_value(ep, pos).ul = value;
      }
   }

   DRETURN(0);
}

/**
 * @brief Set ulong value at the given field name id
 *
 * Set ulong value at the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new value
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetUlong(lListElem *ep, int name, lUlong value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Uint32, LegacyMissing::Silent, MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetUlong");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosUlong(ep, pos, value));
}

/**
 * @brief Adds a lUlong offset to the lUlong field
 *
 * The 'offset' is added to the lUlong field 'name' of
 * the CULL element 'ep'.
 *
 * @param ep element
 * @param name field name id
 * @param offset the offset
 *
 * @return 0 on success, -1 when @p ep is nullptr or @p name is not one of its
 *         fields. A field of another type is a programming error and aborts the
 *         process (#incompatibleType2).
 *
 * @note A thin wrapper over ocs::tom::add_number().
 * @see Typed form: ocs::tom::Element<T>::add<F>(), which returns the new value, or ocs::tom::add<F>(ep, offset).
 */
int lAddUlong(lListElem *ep, int name, lUlong offset) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Uint32, LegacyMissing::Silent, MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, nullptr, "lAddUlong");
   if (pos < 0) {
      DRETURN(-1);
   }

   try {
      ocs::tom::add_number(ep, pos, ocs_cull_value(ep, pos), offset);
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lAddUlong", e);
   }

   DRETURN(0);
}

/**
 * @brief Set ulong value at the given field name id
 *
 * Set ulong64 value at the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 */
/** @brief Read a 64 bit unsigned field without being able to observe it torn
 *
 * For a field that several threads write while others read it, and that carries
 * no invariant with any other field of the element. The canonical case is a time
 * of last use: competing writers each store a valid recent time, nothing is
 * derived from it, and a reader only has to be sure it never sees half of one
 * value and half of another.
 *
 * That is a guarantee about a single load, not about ordering with anything
 * else, so the access is relaxed. Whoever needs the value to be ordered against
 * other state needs a lock, not this.
 *
 * Relying on an aligned 64 bit load being indivisible in practice would not do:
 * it is not what the language guarantees, and it does not hold on every platform
 * this product is built for.
 *
 * @param ep the element to read from
 * @param name field name id
 *
 * @return the value
 *
 * @see #lGetUlong64, #lSetUlong64Atomic
 */
lUlong64 lGetUlong64Atomic(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, name, ocs::tom::FieldType::Uint64, MSG_CULL_GETULONG64_WRONGTYPEFORFIELDXY_SS);

   // std::atomic_ref would be the idiomatic form, but it cannot be used here:
   // FreeBSD 14 ships libc++ 18, which does not implement it at all
   // (__cpp_lib_atomic_ref is not defined), so the build fails on that platform.
   // The builtins below are understood by gcc and clang alike, need no library
   // support and emit the same instruction.
   static_assert(__atomic_always_lock_free(sizeof(lUlong64), nullptr),
                 "lUlong64 cannot be accessed atomically without a lock on this platform");
   static_assert(alignof(lMultiType) >= sizeof(lUlong64),
                 "a cull value is not aligned well enough to be accessed atomically");

   DRETURN(__atomic_load_n(&ocs_cull_value(ep, pos).ul64, __ATOMIC_RELAXED));
}

/** @brief Write a 64 bit unsigned field without a reader being able to see it torn
 *
 * The counterpart of #lGetUlong64Atomic, and subject to one restriction the
 * ordinary setter does not have: a field **declared** hashed is refused.
 * #lSetUlong64 keeps the hash table in step by removing the old entry and
 * inserting the new one, and an atomic store cannot do that - it would leave the
 * table pointing at a value that is no longer there. A field written this way is
 * therefore a field nobody looks up by value.
 *
 * The declaration is what decides, not whether a table is present: the tables
 * are built when an element enters a list, so a free element would otherwise be
 * allowed what the same element is refused a moment later.
 *
 * @param ep the element to write to
 * @param name field name id
 * @param value the value to store
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see #lSetUlong64, #lGetUlong64Atomic
 */
int lSetUlong64Atomic(lListElem *ep, int name, lUlong64 value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Uint64, LegacyMissing::Silent, MSG_CULL_SETULONG64_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetUlong64Atomic");
   if (pos < 0) {
      DRETURN(-1);
   }

   // by the declaration and not by whether a table exists: the tables are built
   // when an element enters a list, so asking for the table would let the same
   // field be written atomically while the element is still free and refuse it
   // afterwards
   if (mt_do_hashing(ep->descr[pos].mt)) {
      CRITICAL("lSetUlong64Atomic() refuses the hashed field " SFQ, lNm2Str(name));
      DRETURN(-1);
   }

   // see lGetUlong64Atomic() on why this is a builtin and not std::atomic_ref
   __atomic_store_n(&ocs_cull_value(ep, pos).ul64, value, __ATOMIC_RELAXED);

   DRETURN(0);
}

int lSetUlong64(lListElem *ep, int name, lUlong64 value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Uint64, LegacyMissing::Silent, MSG_CULL_SETULONG64_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetUlong64");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosUlong64(ep, pos, value));
}

/**
 * @brief Adds a lUlong64 offset to the lUlong64 field
 *
 * The 'offset' is added to the lUlong64 field 'name' of
 * the CULL element 'ep'.
 *
 * @param ep element
 * @param name field name id
 * @param offset the offset
 *
 * @return 0 on success, -1 when @p ep is nullptr or @p name is not one of its
 *         fields. A field of another type is a programming error and aborts the
 *         process (#incompatibleType2).
 *
 * @note A thin wrapper over ocs::tom::add_number().
 * @see Typed form: ocs::tom::Element<T>::add<F>(), which returns the new value, or ocs::tom::add<F>(ep, offset).
 */
int lAddUlong64(lListElem *ep, int name, lUlong64 offset) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Uint64, LegacyMissing::Silent, MSG_CULL_SETULONG64_WRONGTYPEFORFIELDXY_SS, nullptr, "lAddUlong64");
   if (pos < 0) {
      DRETURN(-1);
   }

   try {
      ocs::tom::add_number(ep, pos, ocs_cull_value(ep, pos), offset);
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lAddUlong64", e);
   }

   DRETURN(0);
}

/**
 * @brief Get ulong64 at a certain position
 *
 * Get ulong64 at a certain position
 *
 * @param ep element
 * @param pos position
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosUlong64(lListElem *ep, int pos, lUlong64 value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      ocs::tom::report_soft_path("lSetPosUlong64", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosUlong64", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T) {
      incompatibleType("lSetPosUlong64");
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).ul64) {
      if (ocs::tom::needs_hashed_store(ep, pos)) {
         try {
            ocs::tom::store_hashed_number(ep, pos, value);
         } catch (const ocs::tom::FieldException &e) {
            ocs::tom::legacy_abort("lSetPosUlong64", e);
         }
      } else {
         ocs_cull_value(ep, pos).ul64 = value;
      }
   }

   DRETURN(0);
}

/**
 * @brief Sets the string at a certain position
 *
 * Sets the string at a certain position.
 *
 * @param ep element
 * @param pos position
 * @param value string value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosString(lListElem *ep, int pos, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      ocs::tom::report_soft_path("lSetPosString", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosString", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT) {
      incompatibleType("lSetPosString");
      DRETURN(-1);
   }

   bool stored = false;
   try {
      stored = ocs::tom::store_string(ep, pos, value);
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lSetPosString", e);
   }
   if (!stored) {
      LERROR(LESTRDUP);
      DRETURN(-1);
   }

   DRETURN(0);
}

/**
 * @brief Sets the hostname at a certain position
 *
 * Sets the hostname at a certain position
 *
 * @param ep element
 * @param pos position
 * @param value new hostname
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosHost(lListElem *ep, int pos, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      ocs::tom::report_soft_path("lSetPosHost", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosHost", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lHostT) {
      incompatibleType("lSetPosHost");
      DRETURN(-1);
   }

   bool stored = false;
   try {
      stored = ocs::tom::store_string(ep, pos, value);
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lSetPosHost", e);
   }
   if (!stored) {
      LERROR(LESTRDUP);
      DRETURN(-1);
   }

   DRETURN(0);
}

/**
 * @brief Sets the string at the given field name id
 *
 * Sets the string at the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new string
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetString(lListElem *ep, int name, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::String, LegacyMissing::Abort, MSG_CULL_SETSTRING_WRONGTYPEFORFIELDXY_SS, MSG_CULL_SETSTRING_NOSUCHNAMEXYINDESCRIPTOR_IS, "lSetString");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosString(ep, pos, value));
}

/**
 * @brief Set hostname for field name in element
 *
 * Sets in the element ep for field name the char * value.
 * Also duplicates the pointed to char array
 * (runtime type checking)
 *
 * @param ep list element pointer
 * @param name name of list element (e.g. EH_name)
 * @param value new value for list element
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetHost(lListElem *ep, int name, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Host, LegacyMissing::Abort, MSG_CULL_SETHOST_WRONGTYPEFORFIELDXY_SS, MSG_CULL_SETHOST_NOSUCHNAMEXYINDESCRIPTOR_IS, "lSetHost");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosHost(ep, pos, value));
}

/**
 * @brief Set list element at position pos
 *
 * Sets in the element 'ep' at position 'pos' the list element 'value'.
 * Doesn't copy the object. Does runtime type checking.
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosObject(lListElem *ep, int pos, lListElem *value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      ocs::tom::report_soft_path("lSetPosObject", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosObject", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lObjectT) {
      incompatibleType("lSetPosObject");
      DRETURN(-1);
   }

   if (!ocs::tom::store_object(ep, pos, value)) {
      ocs::tom::report_soft_path("lSetPosObject", ep->descr[pos].nm, ocs::tom::SoftPathReason::BoundElement);
      LERROR(LEBOUNDELEM);
      DRETURN(-1);
   }

   DRETURN(0);
}

/**
 * @brief Set list at position pos
 *
 * Sets in the element 'ep' at position 'pos' the lists 'value'.
 * Doesn't copy the list. Does runtime type checking.
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosList(lListElem *ep, int pos, lList *value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      ocs::tom::report_soft_path("lSetPosList", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosList", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT) {
      incompatibleType("lSetPosList");
      DRETURN(-1);
   }

   ocs::tom::store_list(ep, pos, value);

   DRETURN(0);
}

/**
 * @brief Exchange field name value string pointer
 *
 * Exchange the string pointer, which has the given field name value.
 *
 * @param ep element
 * @param name field name value
 * @param str pointer to a string
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 */
int lXchgString(lListElem *ep, int name, char **str) {
   DENTER(CULL_BASIS_LAYER);

   char *tmp;
   if (str == nullptr) {
      ocs::tom::report_soft_path("lXchgString", name, ocs::tom::SoftPathReason::NullArgument);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::String, LegacyMissing::NegPos, MSG_CULL_XCHGLIST_WRONGTYPEFORFIELDXY_SS, nullptr, "lXchgString");
   if (pos < 0) {
      DRETURN(-1);
   }

   if (*str != ocs_cull_value(ep, pos).str) {
      tmp = ocs_cull_value(ep, pos).str;
      ocs_cull_value(ep, pos).str = *str;
      *str = tmp;
   }

   DRETURN(0);

}

/**
 * @brief Exchange field name value list pointer
 *
 * Exchange the list pointer which has the given field name value.
 *
 * @param ep element
 * @param name field name value
 * @param lpp pointer to CULL list
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::Element<T>::exchange().
 */
int lXchgList(lListElem *ep, int name, lList **lpp) {
   DENTER(CULL_BASIS_LAYER);

   if (lpp == nullptr) {
      ocs::tom::report_soft_path("lXchgList", name, ocs::tom::SoftPathReason::NullArgument);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::List, LegacyMissing::NegPos, MSG_CULL_XCHGLIST_WRONGTYPEFORFIELDXY_SS, nullptr, "lXchgList");
   if (pos < 0) {
      DRETURN(-1);
   }

   ocs::tom::exchange_list(ep, pos, *lpp);
   DRETURN(0);

}

/**
 * @brief Exchange two lists within two elements
 *
 * Exchange two lists within two elements.
 *
 * @param to element one
 * @param nm_to field name id of a list attribute of 'to'
 * @param from element two
 * @param nm_from field name id of a list attribute of 'from'
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::Element<T>::swap().
 */
int lSwapList(lListElem *to, int nm_to, lListElem *from, int nm_from) {
   DENTER(CULL_BASIS_LAYER);

   lList *tmp = nullptr;

   if (lXchgList(from, nm_from, &tmp) == -1) {
      DRETURN(-1);
   }
   if (lXchgList(to, nm_to, &tmp) == -1) {
      DRETURN(-1);
   }
   if (lXchgList(from, nm_from, &tmp) == -1) {
      DRETURN(-1);
   }


   DRETURN(0);
}

/**
 * @brief Sets a list at the given field name id
 *
 * Sets a list at the given field name id. List will not be copyed.
 *
 * @param ep element
 * @param name field name id
 * @param value new list pointer
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetObject(lListElem *ep, int name, lListElem *value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Object, LegacyMissing::Silent, MSG_CULL_SETLIST_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetObject");
   if (pos < 0) {
      DRETURN(-1);
   }

   if (value != nullptr && elem_kind(value) != FREE_ELEM && elem_kind(value) != TRANS_BOUND_ELEM) {
      ocs::tom::report_soft_path("lSetObject", name, ocs::tom::SoftPathReason::BoundElement);
      LERROR(LEBOUNDELEM);
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).obj) {
      /* free old element */
      if (ocs_cull_value(ep, pos).obj) {
         lFreeElem(&(ocs_cull_value(ep, pos).obj));
      }

      /* set new list */
      ocs_cull_value(ep, pos).obj = value;

      /* mark lListElem as bound */
      if (value != nullptr) {
         elem_set_object(value);
      }
   }

   DRETURN(0);
}

/**
 * @brief Sets a list at the given field name id
 *
 * Sets a list at the given field name id. List will not be copied.
 *
 * @param ep element
 * @param name field name id
 * @param value new list pointer
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @note MT-NOTE: lAddSubList() is MT safe
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetList(lListElem *ep, int name, lList *value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::List, LegacyMissing::Silent, MSG_CULL_SETLIST_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetList");
   if (pos < 0) {
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).glp) {
      /* free old list */
      lFreeList(&(ocs_cull_value(ep, pos).glp));

      /* set new list */
      ocs_cull_value(ep, pos).glp = value;
   }

   DRETURN(0);
}

/**
 * @brief Set double value at given position
 *
 * Set double value at given position.
 *
 * @param ep element
 * @param pos position
 * @param value new double value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosDouble(lListElem *ep, int pos, lDouble value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      ocs::tom::report_soft_path("lSetPosDouble", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosDouble", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lDoubleT) {
      incompatibleType("lSetPosDouble");
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).db) {
      ocs_cull_value(ep, pos).db = value;
   }

   DRETURN(0);
}


/**
 * @brief Set double value with given field name id
 *
 * Set double value with given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new double value
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetDouble(lListElem *ep, int name, lDouble value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Double, LegacyMissing::NegPos, MSG_CULL_SETDOUBLE_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetDouble");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosDouble(ep, pos, value));
}

/**
 * @brief Adds an offset to a double field
 *
 * @p value is added to the double field @p name of the element @p ep.
 *
 * @param ep element
 * @param name field name id
 * @param value the amount to add
 *
 * @return 0 on success, -1 with LEELEMNULL for a null element or LENEGPOS for a
 *         field the element does not hold. A field of another type is a
 *         programming error and aborts the process (#incompatibleType2).
 *
 * @note A thin wrapper over ocs::tom::add_number().
 * @see Typed form: ocs::tom::Element<T>::add<F>(), which returns the new value, or ocs::tom::add<F>(ep, offset).
 */
int lAddDouble(lListElem *ep, int name, lDouble value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Double, LegacyMissing::NegPos, MSG_CULL_SETDOUBLE_WRONGTYPEFORFIELDXY_SS, nullptr, "lAddDouble");
   if (pos < 0) {
      DRETURN(-1);
   }

   ocs::tom::add_number(ep, pos, ocs_cull_value(ep, pos), value);

   DRETURN(0);
}


/**
 * @brief Set long value at given position
 *
 * Set long value at given position.
 *
 * @param ep element
 * @param pos position
 * @param value new long value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosLong(lListElem *ep, int pos, lLong value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      ocs::tom::report_soft_path("lSetPosLong", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosLong", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lLongT) {
      incompatibleType("lSetPosLong");
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).l) {
      ocs_cull_value(ep, pos).l = value;
   }

   DRETURN(0);
}

/**
 * @brief Set long value with given field name id
 *
 * Set long value with given field name id.
 *
 * @param ep element
 * @param name field name id
 * @param value value
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetLong(lListElem *ep, int name, lLong value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Int64, LegacyMissing::NegPos, MSG_CULL_SETLONG_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetLong");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosLong(ep, pos, value));
}

/**
 * @brief Sets the character a the given position
 *
 * Sets the character a the given position.
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosBool(lListElem *ep, int pos, lBool value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      ocs::tom::report_soft_path("lSetPosBool", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosBool", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lBoolT) {
      incompatibleType("lSetPosBool");
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).b) {
      ocs_cull_value(ep, pos).b = value;
   }

   DRETURN(0);
}

/**
 * @brief Sets character with the given field name id
 *
 * Sets character with the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new character
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetBool(lListElem *ep, int name, lBool value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Bool, LegacyMissing::NegPos, MSG_CULL_SETBOOL_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetBool");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosBool(ep, pos, value));
}

/**
 * @brief Set pointer at given position
 *
 * Set pointer at given position
 *
 * @param ep element
 * @param pos position
 * @param value pointer
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetPosRef(lListElem *ep, int pos, lRef value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      ocs::tom::report_soft_path("lSetPosRef", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      ocs::tom::report_soft_path("lSetPosRef", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NegativePosition);
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lRefT) {
      incompatibleType("lSetPosRef");
      DRETURN(-1);
   }

   if (value != ocs_cull_value(ep, pos).ref) {
      ocs_cull_value(ep, pos).ref = value;
   }

   DRETURN(0);
}

/**
 * @brief Set pointer with the given field name id
 *
 * Set pointer with the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new pointer
 *
 * @return 0 on success, -1 on error. A field of another type is a programming
 *         error and aborts the process (#incompatibleType2).
 *
 * @see Typed form: ocs::tom::set<F>(ep, value) and ocs::tom::Element<T>::set<F>(), or set(nm, value) for a field known only at run time.
 */
int lSetRef(lListElem *ep, int name, lRef value) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, name, ocs::tom::FieldType::Ref, LegacyMissing::NegPos, MSG_CULL_SETREF_WRONGTYPEFORFIELDXY_SS, nullptr, "lSetRef");
   if (pos < 0) {
      DRETURN(-1);
   }

   DRETURN(lSetPosRef(ep, pos, value));
}

/**
 * @brief Add an element by key through the core, mapping its exceptions as the old add functions did
 *
 * A field that is not of the key's type gives the old CRITICAL message and
 * nullptr, for every key type alike (before CS-3077 the number variants
 * aborted instead); a duplicate key in a unique field aborts, as before.
 *
 * @param function the old function's name, for the record of a soft path and the abort
 * @param wrong_field_msg the old message for a field of another type, with the field name as argument
 * @param nm the key field
 * @param add calls the core
 * @return the element, or nullptr
 */
template <typename Add>
static lListElem *
ocs_add_by_key(const char *function, const char *wrong_field_msg, const int nm, Add add) {
   try {
      return add();
   } catch (const ocs::tom::FieldException &e) {
      switch (e.reason()) {
         case ocs::tom::FieldException::Reason::AbsentField:
         case ocs::tom::FieldException::Reason::ForeignField:
         case ocs::tom::FieldException::Reason::WrongType:
            CRITICAL(wrong_field_msg, lNm2Str(nm));
            break;
         default:
            ocs::tom::legacy_abort(function, e);
      }
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Delete the first element with a key through the core, as the old delete functions do
 *
 * Every key type alike: 0 for a null list reference, 1 for a list that is
 * nullptr already, 1 if an element was removed, 0 if none holds the key
 * (before CS-3077 the number variants returned 1 here as well). A list that
 * becomes empty is freed and set to nullptr.
 *
 * @param lpp the list reference
 * @param function the old function's name, for the record of a soft path and the abort
 * @param remove calls the core on the list
 * @return 1 or 0, as described
 */
template <typename Remove>
static int
ocs_del_by_key(lList **lpp, const char *function, Remove remove) {
   if (lpp == nullptr) {
      ocs::tom::report_soft_path(function, ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullArgument);
      return 0;
   }
   if (*lpp == nullptr) {
      return 1;
   }
   try {
      if (!remove(*lpp)) {
         return 0;
      }
   } catch (const ocs::tom::FieldException &e) {
      if (e.reason() == ocs::tom::FieldException::Reason::DuplicateKey) {
         ocs::tom::legacy_abort(function, e);
      }
      // a field of another type finds nothing, as the old lookup did
      return 0;
   }
   if (lGetNumberOfElem(*lpp) == 0) {
      lFreeList(lpp);
   }
   return 1;
}

/**
 * @brief The reference to the list held in a list field, for adding and deleting in a sub-list
 *
 * Every add and delete in a sub-list treats a null element alike: nothing is
 * done, and the soft path is recorded. An element without the list field
 * aborts, as every lookup in a sub-list does (before CS-3077 the add
 * functions gave nullptr with a message instead).
 *
 * @param ep the element, or nullptr
 * @param snm the list field
 * @param function the name of the calling function, for the record of the soft path
 * @return the reference, or nullptr for a null element
 */
static lList **
ocs_sub_list_ref(lListElem *ep, const int snm, const char *function) {
   if (ep == nullptr) {
      ocs::tom::report_soft_path(function, snm, ocs::tom::SoftPathReason::NullElement);
      return nullptr;
   }
   return &ocs_cull_value(ep, lGetPosViaElem(ep, snm, SGE_DO_ABORT)).glp;
}

/**
 * @brief Adds a string to the string sublist
 *
 * This function add a new element into a sublist snm of an
 * element ep. The field nm of this added element will get the
 * initial value specified with str.
 *
 * @param ep list element
 * @param nm field id contained in the element which will be created
 * @param str initial value if nm
 * @param snm field id of the sublist within ep
 * @param dp Type of the new element
 *
 * @return nullptr in case of error otherwise pointer to the added element
 *
 * @see Typed form: element.get_or_create<F>().emplace_back<G>().
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
lListElem *lAddSubStr(lListElem *ep, int nm, const char *str, int snm, const lDescr *dp) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lAddSubStr");
   return lpp != nullptr ? lAddElemStr(lpp, nm, str, dp) : nullptr;
}

/**
 * @brief Adds a string to the string sublist
 *
 * This function add a new element into a sublist snm of an
 * element ep. The field nm of this added element will get the
 * initial value specified with str.
 *
 * @param ep list element
 * @param nm field id contained in the element which will be created
 * @param str initial value if nm
 * @param snm field id of the sublist within ep
 * @param dp Type of the new element
 *
 * @return nullptr in case of error otherwise pointer to the added element
 *
 * @see Typed form: element.get_or_create<F>().emplace_back<G>().
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
lListElem *lAddSubHost(lListElem *ep, int nm, const char *str, int snm, const lDescr *dp) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lAddSubHost");
   return lpp != nullptr ? lAddElemHost(lpp, nm, str, dp) : nullptr;
}


/**
 * @brief Adds a string to the string list
 *
 * This function adds a new element of type dp to the list referenced
 * by lpp. The field nm will get the initial value str.
 *
 * @param lpp list reference
 * @param nm field id
 * @param str initial value
 * @param dp Type of the object which will be added
 *
 * @return lListElem* -
 *
 * @see Typed form: ocs::tom::List<T>::emplace_back<F>(); a thin wrapper over ocs::tom::add_elem_str() of the core.
 */
lListElem *lAddElemStr(lList **lpp, int nm, const char *str, const lDescr *dp) {
   if (!lpp || !str || !dp) {
      ocs::tom::report_soft_path("lAddElemStr", nm, ocs::tom::SoftPathReason::NullArgument);
      return nullptr;
   }
   return ocs_add_by_key("lAddElemStr", MSG_CULL_ADDELEMSTRERRORXRUNTIMETYPE_S, nm, [&] { return ocs::tom::add_elem_str(*lpp, nm, str, dp); });
}

/**
 * @brief Adds a hostname to a hostname list
 *
 * Adds a hostname to a hostname list
 *
 * @param lpp list reference
 * @param nm hostname field id
 * @param str new hostname
 * @param dp descriptor of new element
 *
 * @return new element or nullptr
 *
 * @see Typed form: ocs::tom::List<T>::emplace_back<F>(); a thin wrapper over ocs::tom::add_elem_host() of the core.
 */
lListElem *lAddElemHost(lList **lpp, int nm, const char *str, const lDescr *dp) {
   if (!lpp || !str || !dp) {
      ocs::tom::report_soft_path("lAddElemHost", nm, ocs::tom::SoftPathReason::NullArgument);
      return nullptr;
   }
   return ocs_add_by_key("lAddElemHost", MSG_CULL_ADDELEMHOSTERRORXRUNTIMETYPE_S, nm, [&] { return ocs::tom::add_elem_host(*lpp, nm, str, dp); });
}

/**
 * @brief Removes an element from a sublist
 *
 * This function removes an element specified by a string field
 * nm and the string str supposed to be in the sublist snm of the
 * element ep.
 *
 * @param ep element
 * @param nm field id
 * @param str string
 * @param snm field id of a sublist of ep
 *
 * @return 1 element was found and removed 0 in case of an error
 *
 * @see Typed form: element.get<F>().erase<G>(key).
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
int lDelSubStr(lListElem *ep, int nm, const char *str, int snm) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lDelSubStr");
   return lpp != nullptr ? lDelElemStr(lpp, nm, str) : 0;
}

/**
 * @brief Removes element specified by a string field nm
 *
 * This function removes an element from the list referenced by
 * lpp, which is identified by the field nm and the string str
 *
 * @param lpp list reference
 * @param nm field id
 * @param str string
 *
 * @return 1 if the element was found and removed 0 in case of an error
 *
 * @see Typed form: ocs::tom::List<T>::erase<F>(key), which leaves an emptied list in place; a thin wrapper over ocs::tom::remove_elem_str() of the core.
 */
int lDelElemStr(lList **lpp, int nm, const char *str) {
   if (str == nullptr) {
      ocs::tom::report_soft_path("lDelElemStr", nm, ocs::tom::SoftPathReason::NullArgument);
      return 0;
   }
   return ocs_del_by_key(lpp, "lDelElemStr", [&](lList *lp) { return ocs::tom::remove_elem_str(lp, nm, str); });
}

/**
 * @brief The list held in a list field of an element, for the lookups in a sub-list
 *
 * Every lookup in a sub-list (lGetSubStr(), lGetSubUlong(), lGetSubUlong64(),
 * lGetSubHost(), lGetSubCaseStr()) treats a null element alike: it finds
 * nothing, and the soft path is recorded in a debug build. An element without
 * the list field aborts, as before.
 *
 * @param ep the element, or nullptr
 * @param snm the list field
 * @param function the name of the calling lookup, for the record of the soft path
 * @return the list, or nullptr for a null element or an empty field
 */
static const lList *
ocs_sub_list(const lListElem *ep, const int snm, const char *function) {
   if (ep == nullptr) {
      ocs::tom::report_soft_path(function, snm, ocs::tom::SoftPathReason::NullElement);
      return nullptr;
   }
   return ocs_cull_value(ep, lGetPosViaElem(ep, snm, SGE_DO_ABORT)).glp;
}

/**
 * @brief Returns element specified by a string field nm
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first<G>().
 *
 * @note Every lookup in a sub-list treats a null element alike and finds nothing (#ocs_sub_list).
 */
lListElem *lGetSubStrRW(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetElemStrRW(ocs_sub_list(ep, snm, "lGetSubStr"), nm, str);
}

/**
 * @brief Returns the sub-element whose string field matches, read only
 *
 * @param ep the element
 * @param nm the field holding the sub-list
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first<G>().
 */
const lListElem *
lGetSubStr(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetSubStrRW(ep, nm, str, snm);
}

/**
 * @brief Returns element specified by a string field nm
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
lListElem *lGetElemStrRW(const lList *lp, int nm, const char *str) {
   DENTER(CULL_LAYER);

   const void *iterator = nullptr;
   lListElem *ret = nullptr;
   ret = lGetElemStrFirstRW(lp, nm, str, &iterator);
   DRETURN(ret);
}

/**
 * @brief Returns the element whose string field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
const lListElem *lGetElemStr(const lList *lp, int nm, const char *str) {
   return lGetElemStrRW(lp, nm, str);
}


/**
 * @brief Find first element with a certain string
 *
 * Returns the first element within 'lp' where the attribute
 * with field name id 'nm' is equivalent with 'str'. 'iterator'
 * will be filled with context information which will make it
 * possible to use 'iterator' with lGetElemStrNext() to get
 * the next element.
 *
 * A key held twice in a field declared unique is a logic error: the
 * process aborts with a message naming type, field, key and list (N-H-18).
 *
 * @param lp list
 * @param nm field name id
 * @param str string to be compared
 * @param iterator iterator
 *
 * @return first element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
lListElem *lGetElemStrFirstRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(CULL_LAYER);

   int pos;
   int data_type;
   const lDescr *listDescriptor;


   if (!str) {
      DPRINTF("error: nullptr ptr passed to lGetElemStrFirst\n");
      DRETURN(nullptr);
   }

   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   listDescriptor = lGetListDescr(lp);

   /* get position of nm in sdp */
   pos = lGetPosInDescr(listDescriptor, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the core searches; a repeated key of a unique field ends the process here, as every logic error of the old
   // interface does (N-H-18, N-L-2)
   try {
      DRETURN(ocs::tom::lookup_first_str(lp, pos, str, iterator));
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lGetElemStrFirst", e);
   }
}

/**
 * @brief Returns the first element whose string field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemStrFirst(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemStrFirstRW(lp, nm, str, iterator);
}

/**
 * @brief Get next element with a certain string
 *
 * Returns a element within list 'lp' where the attribute with
 * field name id 'nm' is equivalent with 'str'. The function
 * uses 'iterator' as input. 'iterator' containes context
 * information which where fillen in in a previous call of
 * lGetElemStrFirst().
 *
 * @param lp list
 * @param nm string field name id
 * @param str string
 * @param iterator iterator
 *
 * @return next element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 *
 * @note The checks are here; the lookup is a thin wrapper over ocs::tom::lookup_next_str().
 */
lListElem *lGetElemStrNextRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(CULL_LAYER);

   int pos, data_type;
   const lDescr *listDescriptor;


   if (*iterator == nullptr) {
      return nullptr;
   }

   if (!str) {
      DPRINTF("error: nullptr ptr passed to lGetElemStr\n");
      DRETURN(nullptr);
   }

   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   listDescriptor = lGetListDescr(lp);

   /* get position of nm in sdp */
   pos = lGetPosInDescr(listDescriptor, nm);
   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      DPRINTF("error: lGetElemStrNext called to field which is no lStringT type\n");
      DRETURN(nullptr);
   }

   DRETURN(ocs::tom::lookup_next_str(lp, pos, str, iterator));
}

/**
 * @brief Returns the next element whose string field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemStrNext(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemStrNextRW(lp, nm, str, iterator);
}

/**
 * @brief Returns element specified by a wildcard
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::List<T>::find_first_like(); a thin wrapper over ocs::tom::lookup_first_str_like() of the core.
 */
lListElem *lGetElemStrLikeRW(const lList *lp, int nm, const char *str) {
   DENTER(CULL_LAYER);
   try {
      DRETURN(ocs::tom::lookup_first_str_like(lp, nm, str));
   } catch (const ocs::tom::FieldException &e) {
      if (e.reason() == ocs::tom::FieldException::Reason::AbsentField) {
         CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      } else {
         CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      }
   }
   DRETURN(nullptr);
}

/**
 * @brief Returns the element whose string field matches a pattern, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first_like().
 */
const lListElem *lGetElemStrLike(const lList *lp, int nm, const char *str) {
   return lGetElemStrLikeRW(lp, nm, str);
}

/**
 * @brief Adds ulong to the ulong sublist of element ep
 *
 * This function adds a new element into the sublist snm of the
 * element ep. The field nm of the added element will get the
 * initial value val.
 *
 * @param ep element
 * @param nm field which will get value val
 * @param val initial value for nm
 * @param snm sublist within ep where the element will be added
 * @param dp Type of the new element (e.g. JB_Type)
 *
 * @return nullptr in case of error or the pointer to the new element
 *
 * @see Typed form: element.get_or_create<F>().emplace_back<G>().
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
lListElem *lAddSubUlong(lListElem *ep, int nm, lUlong val, int snm, const lDescr *dp) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lAddSubUlong");
   return lpp != nullptr ? lAddElemUlong(lpp, nm, val, dp) : nullptr;
}

/**
 * @brief Adds a ulong to the ulong list
 *
 * Adds an new element to a list lpp where one field nm within
 * the new element gets an initial value val
 *
 * @param lpp list
 * @param nm field in the new element which will get value val
 * @param val initial value for nm
 * @param dp type of the list (e.g. JB_Type)
 *
 * @return nullptr on error or pointer to the added element
 *
 * @see Typed form: ocs::tom::List<T>::emplace_back<F>(); a thin wrapper over ocs::tom::add_elem_ulong() of the core.
 */
lListElem *lAddElemUlong(lList **lpp, int nm, lUlong val, const lDescr *dp) {
   if (!lpp || !dp) {
      ocs::tom::report_soft_path("lAddElemUlong", nm, ocs::tom::SoftPathReason::NullArgument);
      return nullptr;
   }
   return ocs_add_by_key("lAddElemUlong", MSG_CULL_ADDELEMULONGERRORXRUNTIMETYPE_S, nm, [&] { return ocs::tom::add_elem_ulong(*lpp, nm, val, dp); });
}

/**
 * @brief Removes an element from a sublist
 *
 * This function removes an element specified by a ulong field nm
 * and the ulong val supposed to be in the sublist snm of the
 * element ep
 *
 * @param ep element
 * @param nm field id
 * @param val value
 * @param snm field id of the sublist in ep
 *
 * @return 1 element was found and removed 0 in case of an error
 *
 * @see Typed form: element.get<F>().erase<G>(key).
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
int lDelSubUlong(lListElem *ep, int nm, lUlong val, int snm) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lDelSubUlong");
   return lpp != nullptr ? lDelElemUlong(lpp, nm, val) : 0;
}

/**
 * @brief Removes elem specified by a ulong field nm
 *
 * This function removes an element specified by a ulong field nm
 * with the value val from the list referenced by lpp. When the list
 * becomes empty it is freed and *lpp is set to nullptr.
 *
 * @param lpp reference to a list
 * @param nm field id
 * @param val value of nm; 0 is a valid value
 *
 * @return 0 when lpp is nullptr or no element holds the key, 1 when an element was
 *         removed or *lpp is nullptr (before CS-3077: 1 whether or not an element was found)
 *
 * @see Typed form: ocs::tom::List<T>::erase<F>(key), which leaves an emptied list in place; a thin wrapper over ocs::tom::remove_elem_ulong() of the core.
 */
int lDelElemUlong(lList **lpp, int nm, lUlong val) {
   return ocs_del_by_key(lpp, "lDelElemUlong", [&](lList *lp) { return ocs::tom::remove_elem_ulong(lp, nm, val); });
}

/**
 * @brief Element specified by a ulong field nm
 *
 * returns an element specified by a ulong field nm an the ulong
 * value val from the sublist snm of the element ep
 *
 * @param ep element pointer
 * @param nm field id which is part of a sublist element of ep
 * @param val unsigned long value
 * @param snm field id of a list which is part of ep
 *
 * @return nullptr if element was not found or in case of an error otherwise pointer to the element
 *
 * @see Typed form: element.get<F>().find_first<G>().
 *
 * @note Every lookup in a sub-list treats a null element alike and finds nothing (#ocs_sub_list).
 */
lListElem *lGetSubUlongRW(const lListElem *ep, int nm, lUlong val, int snm) {
   return lGetElemUlongRW(ocs_sub_list(ep, snm, "lGetSubUlong"), nm, val);
}

/**
 * @brief Returns the sub-element whose 32 bit field matches, read only
 *
 * @param ep the element
 * @param nm the field
 * @param val the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first<G>().
 */
const lListElem *lGetSubUlong(const lListElem *ep, int nm, lUlong val, int snm) {
   return lGetSubUlongRW(ep, nm, val, snm);
}

/**
 * @brief Returns element specified by a ulong field nm
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
lListElem *lGetElemUlongRW(const lList *lp, int nm, lUlong val) {
   const void *iterator = nullptr;
   return lGetElemUlongFirstRW(lp, nm, val, &iterator);
}

/**
 * @brief Returns the element whose 32 bit field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
const lListElem *lGetElemUlong(const lList *lp, int nm, lUlong val) {
   return lGetElemUlongRW(lp, nm, val);
}

/**
 * @brief Find first ulong within a list
 *
 * Return the first element of list 'lp' where the attribute
 * with field name id 'nm' is equivalent with 'val'. Context
 * information will be stored in 'iterator'. 'iterator' might
 * be used in lGetElemUlongNext() to get the next element.
 *
 * A key held twice in a field declared unique is a logic error: the
 * process aborts with a message naming type, field, key and list (N-H-18).
 *
 * @param lp list
 * @param nm ulong field anme id
 * @param val ulong value
 * @param iterator iterator
 *
 * @return element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
lListElem *lGetElemUlongFirstRW(const lList *lp, int nm, lUlong val, const void **iterator) {
   DENTER(CULL_LAYER);
   int pos;


   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONGERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the core searches; a repeated key of a unique field ends the process here (N-H-18, N-L-2)
   try {
      DRETURN(ocs::tom::lookup_first_ulong(lp, pos, val, iterator));
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lGetElemUlongFirst", e);
   }
}

/**
 * @brief Returns the first element whose 32 bit field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemUlongFirst(const lList *lp, int nm, lUlong val, const void **iterator) {
   return lGetElemUlongFirstRW(lp, nm, val, iterator);
}

/**
 * @brief Find next ulong element within a list
 *
 * This function might be used after a call to lGetElemUlongFirst().
 * It expects 'iterator' to contain context information which
 * makes it possible to find the next element within list 'lp'
 * where the attribute with field name id 'nm' is equivalent with
 * 'val'.
 *
 * @param lp list
 * @param nm ulong field name id
 * @param val value
 * @param iterator iterator
 *
 * @return next element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 *
 * @note The checks are here; the lookup is a thin wrapper over ocs::tom::lookup_next_ulong().
 */
lListElem *lGetElemUlongNextRW(const lList *lp, int nm, lUlong val, const void **iterator) {
   DENTER(CULL_LAYER);

   int pos;

   if (*iterator == nullptr) {
      return nullptr;
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONGERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the field's type, which the old linear search checked on reading each element
   if (mt_get_type(lp->descr[pos].mt) != lUlongT) {
      incompatibleType("lGetPosUlong");
   }
   DRETURN(ocs::tom::lookup_next_ulong(lp, pos, val, iterator));
}

/**
 * @brief Returns the next element whose 32 bit field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemUlongNext(const lList *lp, int nm, lUlong val, const void **iterator) {
   return lGetElemUlongNextRW(lp, nm, val, iterator);
}

/**
 * @brief Adds ulong64 to the ulong64 sublist of element ep
 *
 * This function adds a new element into the sublist snm of the
 * element ep. The field nm of the added element will get the
 * initial value val.
 *
 * @param ep element
 * @param nm field which will get value val
 * @param val initial value for nm
 * @param snm sublist within ep where the element will be added
 * @param dp Type of the new element (e.g. JB_Type)
 *
 * @return nullptr in case of error or the pointer to the new element
 *
 * @see Typed form: element.get_or_create<F>().emplace_back<G>().
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
lListElem *lAddSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm, const lDescr *dp) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lAddSubUlong64");
   return lpp != nullptr ? lAddElemUlong64(lpp, nm, val, dp) : nullptr;
}

/**
 * @brief Adds a ulong64 to the ulong64 list
 *
 * Adds an new element to a list lpp where one field nm within
 * the new element gets an initial value val
 *
 * @param lpp list
 * @param nm field in the new element which will get value val
 * @param val initial value for nm
 * @param dp type of the list (e.g. JB_Type)
 *
 * @return nullptr on error or pointer to the added element
 *
 * @see Typed form: ocs::tom::List<T>::emplace_back<F>(); a thin wrapper over ocs::tom::add_elem_ulong64() of the core.
 */
lListElem *lAddElemUlong64(lList **lpp, int nm, lUlong64 val, const lDescr *dp) {
   if (!lpp || !dp) {
      ocs::tom::report_soft_path("lAddElemUlong64", nm, ocs::tom::SoftPathReason::NullArgument);
      return nullptr;
   }
   return ocs_add_by_key("lAddElemUlong64", MSG_CULL_ADDELEMULONG64ERRORXRUNTIMETYPE_S, nm, [&] { return ocs::tom::add_elem_ulong64(*lpp, nm, val, dp); });
}

/**
 * @brief Removes an element from a sublist
 *
 * This function removes an element specified by a ulong field nm
 * and the ulong64 val supposed to be in the sublist snm of the
 * element ep
 *
 * @param ep element
 * @param nm field id
 * @param val value
 * @param snm field id of the sublist in ep
 *
 * @return 1 element was found and removed 0 in case of an error
 *
 * @see Typed form: element.get<F>().erase<G>(key).
 * @note A null element does nothing; an element without the list field aborts, as every lookup in a
 *       sub-list does (#ocs_sub_list_ref; before CS-3077 the add functions gave nullptr, the delete
 *       functions aborted on a null element).
 */
int lDelSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm) {
   lList **lpp = ocs_sub_list_ref(ep, snm, "lDelSubUlong64");
   return lpp != nullptr ? lDelElemUlong64(lpp, nm, val) : 0;
}

/**
 * @brief Removes elem specified by a ulong64 field nm
 *
 * This function removes an element specified by a ulong64 field nm
 * with the value val from the list referenced by lpp. When the list
 * becomes empty it is freed and *lpp is set to nullptr.
 *
 * @param lpp reference to a list
 * @param nm field id
 * @param val value of nm; 0 is a valid value
 *
 * @return 0 when lpp is nullptr or no element holds the key, 1 when an element was
 *         removed or *lpp is nullptr (before CS-3077: 1 whether or not an element was found)
 *
 * @see Typed form: ocs::tom::List<T>::erase<F>(key), which leaves an emptied list in place; a thin wrapper over ocs::tom::remove_elem_ulong64() of the core.
 */
int lDelElemUlong64(lList **lpp, int nm, lUlong64 val) {
   return ocs_del_by_key(lpp, "lDelElemUlong64", [&](lList *lp) { return ocs::tom::remove_elem_ulong64(lp, nm, val); });
}

/**
 * @brief Element specified by a ulong64 field nm
 *
 * returns an element specified by a ulong64 field nm an the ulong64
 * value val from the sublist snm of the element ep
 *
 * @param ep element pointer
 * @param nm field id which is part of a sublist element of ep
 * @param val unsigned long value
 * @param snm field id of a list which is part of ep
 *
 * @return nullptr if element was not found or in case of an error otherwise pointer to the element
 *
 * @see Typed form: element.get<F>().find_first<G>().
 *
 * @note Every lookup in a sub-list treats a null element alike and finds nothing (#ocs_sub_list).
 */
lListElem *lGetSubUlong64RW(const lListElem *ep, int nm, lUlong64 val, int snm) {
   return lGetElemUlong64RW(ocs_sub_list(ep, snm, "lGetSubUlong64"), nm, val);
}

/**
 * @brief Returns the sub-element whose 64 bit field matches, read only
 *
 * @param ep the element
 * @param nm the field
 * @param val the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first<G>().
 */
const lListElem *lGetSubUlong64(const lListElem *ep, int nm, lUlong64 val, int snm) {
   return lGetSubUlong64RW(ep, nm, val, snm);
}

/**
 * @brief Returns element specified by a ulong64 field nm
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
lListElem *lGetElemUlong64RW(const lList *lp, int nm, lUlong64 val) {
   const void *iterator = nullptr;
   return lGetElemUlong64FirstRW(lp, nm, val, &iterator);
}

/**
 * @brief Returns the element whose 64 bit field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
const lListElem *lGetElemUlong64(const lList *lp, int nm, lUlong64 val) {
   return lGetElemUlong64RW(lp, nm, val);
}

/**
 * @brief Find first ulong64 within a list
 *
 * Return the first element of list 'lp' where the attribute
 * with field name id 'nm' is equivalent with 'val'. Context
 * information will be stored in 'iterator'. 'iterator' might
 * be used in lGetElemUlong64Next() to get the next element.
 *
 * A key held twice in a field declared unique is a logic error: the
 * process aborts with a message naming type, field, key and list (N-H-18).
 *
 * @param lp list
 * @param nm ulong64 field name id
 * @param val ulong64 value
 * @param iterator iterator
 *
 * @return element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
lListElem *lGetElemUlong64FirstRW(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   DENTER(CULL_LAYER);
   int pos;


   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONG64ERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the core searches; a repeated key of a unique field ends the process here (N-H-18, N-L-2)
   try {
      DRETURN(ocs::tom::lookup_first_ulong64(lp, pos, val, iterator));
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lGetElemUlong64First", e);
   }
}

/**
 * @brief Returns the first element whose 64 bit field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemUlong64First(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   return lGetElemUlong64FirstRW(lp, nm, val, iterator);
}

/**
 * @brief Find next ulong64 element within a list
 *
 * This function might be used after a call to lGetElemUlong64First().
 * It expects 'iterator' to contain context information which
 * makes it possible to find the next element within list 'lp'
 * where the attribute with field name id 'nm' is equivalent with
 * 'val'.
 *
 * @param lp list
 * @param nm ulong64 field name id
 * @param val value
 * @param iterator iterator
 *
 * @return next element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 *
 * @note The checks are here; the lookup is a thin wrapper over ocs::tom::lookup_next_ulong64().
 */
lListElem *lGetElemUlong64NextRW(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   DENTER(CULL_LAYER);

   int pos;

   if (*iterator == nullptr) {
      return nullptr;
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONG64ERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the field's type, which the old linear search checked on reading each element
   if (mt_get_type(lp->descr[pos].mt) != lUlong64T) {
      incompatibleType("lGetPosUlong64");
   }
   DRETURN(ocs::tom::lookup_next_ulong64(lp, pos, val, iterator));
}

/**
 * @brief Returns the next element whose 64 bit field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemUlong64Next(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   return lGetElemUlong64NextRW(lp, nm, val, iterator);
}

/**
 * @brief Returns elem specified by a string field nm
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first_nocase<G>().
 *
 * @note Every lookup in a sub-list treats a null element alike and finds nothing (#ocs_sub_list).
 */
lListElem *lGetSubCaseStr(const lListElem *ep, int nm, const char *str,
                          int snm) {
   return lGetElemCaseStrRW(ocs_sub_list(ep, snm, "lGetSubCaseStr"), nm, str);
}

/**
 * @brief Returns element specified by a string field
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::List<T>::find_first_nocase(); a thin wrapper over ocs::tom::lookup_first_str_nocase() of the core.
 */
lListElem *lGetElemCaseStrRW(const lList *lp, int nm, const char *str) {
   DENTER(CULL_LAYER);
   try {
      DRETURN(ocs::tom::lookup_first_str_nocase(lp, nm, str));
   } catch (const ocs::tom::FieldException &) {
      CRITICAL(MSG_CULL_GETELEMCASESTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
   }
   DRETURN(nullptr);
}

/**
 * @brief Returns the element whose string field matches, ignoring case, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first_nocase().
 */
const lListElem *lGetElemCaseStr(const lList *lp, int nm, const char *str) {
   return lGetElemCaseStrRW(lp, nm, str);
}

/**
 * @brief Returns an element specified by a hostname
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
lListElem *lGetElemHostRW(const lList *lp, int nm, const char *str) {
   const void *iterator = nullptr;
   return lGetElemHostFirstRW(lp, nm, str, &iterator);
}

/**
 * @brief Returns the element whose host field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_first() and ocs::tom::List<T>::find_first().
 */
const lListElem *lGetElemHost(const lList *lp, int nm, const char *str) {
   return lGetElemHostRW(lp, nm, str);
}

/**
 * @brief LGetElemHostFirst for hostnames
 *
 * lGetElemHostFirst for hostnames
 *
 * A key held twice in a field declared unique is a logic error: the
 * process aborts with a message naming type, field, key and list (N-H-18).
 *
 * @param lp list
 * @param nm hostname field id
 * @param str hostname
 * @param iterator iterator
 *
 * @return element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
lListElem *lGetElemHostFirstRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(TOP_LAYER);

   int pos;
   int data_type;
   const lDescr *listDescriptor = nullptr;

   /* check for null pointers */
   if ((str == nullptr) || (lp == nullptr)) {
      DPRINTF("error: nullptr ptr passed to lGetElemHostFirst\n");
      DRETURN(nullptr);
   }

   /* run time type checking */
   listDescriptor = lGetListDescr(lp);
   pos = lGetPosInDescr(listDescriptor, nm);
   data_type = lGetPosType(listDescriptor, pos);
   if ((pos < 0) || (data_type != lHostT)) {
      if (data_type != lHostT) {
         DPRINTF(":::::::::::::::: lGetElemHostFirst - data type is not lHostT !!! :::::::\n");
      }
      CRITICAL(MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the core searches; a repeated key of a unique field ends the process here (N-H-18, N-L-2)
   try {
      DRETURN(ocs::tom::lookup_first_host(lp, pos, str, iterator));
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lGetElemHostFirst", e);
   }
}

/**
 * @brief Returns the first element whose host field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemHostFirst(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemHostFirstRW(lp, nm, str, iterator);
}

/**
 * @brief LGetElemHostNext() for hostnames
 *
 * lGetElemHostNext() for hostnames
 *
 * @param lp list
 * @param nm hostname field id
 * @param str hostname
 * @param iterator iterator
 *
 * @return element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 *
 * @note The checks are here; the lookup is a thin wrapper over ocs::tom::lookup_next_host().
 */
lListElem *lGetElemHostNextRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(TOP_LAYER);

   int pos;
   const lDescr *listDescriptor = nullptr;

   /* check for null *iterator and */
   /* check for null pointers */
   if ((str == nullptr) || (lp == nullptr) || (*iterator == nullptr)) {
      DPRINTF("error: nullptr ptr passed to lGetElemHostNext\n");
      DRETURN(nullptr);
   }

   /* run time type checking */
   listDescriptor = lGetListDescr(lp);
   pos = lGetPosInDescr(listDescriptor, nm);
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   // the field's type, which the old linear search checked on reading each element
   if (mt_get_type(lp->descr[pos].mt) != lHostT) {
      incompatibleType("lGetPosHost");
   }
   DRETURN(ocs::tom::lookup_next_host(lp, pos, str, iterator));
}

/**
 * @brief Returns the next element whose host field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: ocs::tom::ConstList<T>::find_all() and ocs::tom::List<T>::find_all().
 */
const lListElem *lGetElemHostNext(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemHostNextRW(lp, nm, str, iterator);
}

/**
 * @brief Returns elem specified by a string field nm
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first<G>().
 *
 * @note Every lookup in a sub-list treats a null element alike and finds nothing (#ocs_sub_list).
 */
lListElem *lGetSubHostRW(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetElemHostRW(ocs_sub_list(ep, snm, "lGetSubHost"), nm, str);
}

/**
 * @brief Returns the sub-element whose host field matches, read only
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 *
 * @see Typed form: element.get<F>().find_first<G>().
 */
const lListElem *lGetSubHost(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetSubHostRW(ep, nm, str, snm);
}

/**
 * @brief Removes elem specified by a lHostT field nm
 *
 * removes an element specified by a string field nm and the
 * hostname str from the list referenced by lpp.
 * If it is the last element within lpp the list itself will be
 * deleted.
 *
 * @param lpp list
 * @param nm field id
 * @param str string
 *
 * @return 1 if the host element was found and removed 0 in case of an error
 *
 * @see Typed form: ocs::tom::List<T>::erase<F>(key), which leaves an emptied list in place; a thin wrapper over ocs::tom::remove_elem_host() of the core.
 */
int lDelElemHost(lList **lpp, int nm, const char *str) {
   if (str == nullptr) {
      ocs::tom::report_soft_path("lDelElemHost", nm, ocs::tom::SoftPathReason::NullArgument);
      return 0;
   }
   return ocs_del_by_key(lpp, "lDelElemHost", [&](lList *lp) { return ocs::tom::remove_elem_host(lp, nm, str); });
}

/**
 * @brief Returns name at position
 *
 * Returns the name at specified position in a descriptor array. The
 * Position must be inside the valid range of the descriptor. Returns
 * NoName if descriptor is nullptr or pos < 0.
 *
 * @param dp Descriptor
 * @param pos Position
 *
 * @return Name
 */
int lGetPosName(const lDescr *dp, int pos) {

   if (!dp) {
      LERROR(LEDESCRNULL);
      return (int) NoName;
   }
   if (pos < 0) {
      return (int) NoName;
   }
   return dp[pos].nm;
}

/**
 * @brief Is any bit of a mask set in a 32 bit field?
 *
 * Every caller passes a single bit, for which "any" and "all" coincide.
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to test
 * @return true when at least one bit of @p bitmask is set in the field. A field
 *         of another type is a programming error and aborts the process
 *         (#incompatibleType2).
 */
bool lMatchUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_getter_pos(ep, nm, ocs::tom::FieldType::Uint32, MSG_CULL_GETULONG_WRONGTYPEFORFIELDXY_SS);

   DRETURN((ocs_cull_value(ep, pos).ul & bitmask) > 0 ? true : false);
}

/**
 * @brief Set bits in a 32 bit field
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to set
 * @return 0 on success, -1 when @p ep is nullptr. A field of another type is a
 *         programming error and aborts the process (#incompatibleType2).
 */
int lOrUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, nm, ocs::tom::FieldType::Uint32, LegacyMissing::Silent, MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, nullptr, "lOrUlongBitMask");
   if (pos < 0) {
      DRETURN(-1);
   }

   ocs_cull_value(ep, pos).ul |= bitmask;

   DRETURN(0);
}
/**
 * @brief Keep only the bits of a mask in a 32 bit field
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to keep
 * @return 0 on success, -1 when @p ep is nullptr. A field of another type is a
 *         programming error and aborts the process (#incompatibleType2).
 */
int lAndUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, nm, ocs::tom::FieldType::Uint32, LegacyMissing::Silent, MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, nullptr, "lAndUlongBitMask");
   if (pos < 0) {
      DRETURN(-1);
   }

   ocs_cull_value(ep, pos).ul &= bitmask;

   DRETURN(0);
}
/**
 * @brief Clear bits in a 32 bit field
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to clear
 * @return 0 on success, -1 when @p ep is nullptr. A field of another type is a
 *         programming error and aborts the process (#incompatibleType2).
 */
int lClearUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   const int pos = legacy_setter_pos(ep, nm, ocs::tom::FieldType::Uint32, LegacyMissing::Silent, MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, nullptr, "lClearUlongBitMask");
   if (pos < 0) {
      DRETURN(-1);
   }

   ocs_cull_value(ep, pos).ul &= ~bitmask;

   DRETURN(0);

}

