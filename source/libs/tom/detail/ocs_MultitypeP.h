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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Internal representation of a field value
 */

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_ListP.h"

/**
 * @brief The value of one field
 *
 * Which member is live is decided by the field's type in the descriptor —
 * see @ref _enum_lMultiType and #mt_get_type. Reading the wrong member is not
 * detected by the compiler, which is why the accessors in ocs_ElementCore.cc
 * check the type first and abort on a mismatch.
 */
union _lMultiType {
   double db;         ///< value when the field is #MT_DOUBLE
   uint32_t ul;       ///< value when the field is #MT_UINT32
   uint64_t ul64;     ///< value when the field is #MT_UINT64
   long l;            ///< value when the field is #MT_INT64
   bool b;            ///< value when the field is #MT_BOOL
   int i;             ///< value when the field is #MT_INT32
   char *str;         ///< value when the field is #MT_STRING; owned by the element
   lList *glp;        ///< value when the field is #MT_LIST; owned by the element
   lListElem *obj;    ///< value when the field is #MT_OBJECT; owned by the element
   void *ref;         ///< value when the field is #MT_REF; never owned, never spooled
   char *host;        ///< value when the field is #MT_HOST; owned by the element
   lCondition *cp;    ///< a condition, used when a descriptor carries a where clause
};

namespace ocs::tom::detail {
   void bad_slot(const lListElem *ep, int pos);

   /**
    * @brief The value of the field at a descriptor position of an element (CS-439)
    *
    * Like `ep->cont[pos]` before, the value is writable through a const element:
    * the const-ness of an element has never extended to its values. Debug builds
    * check that the position has a slot among the element's values, and abort
    * otherwise.
    *
    * @param ep the element
    * @param pos a valid field position in the element's descriptor
    * @return the value
    */
   inline lMultiType &value(const lListElem *ep, const int pos) {
#if defined(ENABLE_DEBUG_CHECKS)
      const tom_descr_header *h = ocs::tom::descr_header_of(ep->descr);
      if (pos < 0 || static_cast<uint32_t>(pos) >= h->n || ep->descr[pos].off == TOM_ABSENT ||
          static_cast<uint32_t>(ocs::tom::detail::slot_index(ep->descr, pos)) >= h->np) {
         ocs::tom::detail::bad_slot(ep, pos);
      }
#endif
      return ep->cont[ocs::tom::detail::slot_index(ep->descr, pos)];
   }
} // namespace ocs::tom::detail
