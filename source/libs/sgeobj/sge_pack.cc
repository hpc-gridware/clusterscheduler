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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Packing CULL conditions for transport
 *
 * @see sge_pack.h
 */

#include <string>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_list.h"
#include "cull/cull_pack.h"

#include "sgeobj/sge_pack.h"
#include "tom/ocs_Element.h"

#include "msg_common.h"

/**
 * @brief Pack a selection condition into an element, so it can be sent
 *
 * @param where the condition to pack
 * @return the element of type `SGE_WHERE`, or an empty handle when the condition cannot be packed
 */
ocs::tom::ElementPtr<ocs::tom::PACK> ocs_where_to_elem(const lCondition *where) {
   DENTER(CULL_LAYER);

   ocs::tom::ElementPtr<ocs::tom::PACK> where_elem;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (cull_pack_cond(&pb, where) == PACK_SUCCESS) {
         where_elem = ocs::tom::ElementPtr<ocs::tom::PACK>::create();
         ocs::tom::set<PACK_id>(where_elem, SGE_WHERE);
         const std::string hex = ocs::tom::bytes_to_hex(pb.head_ptr, static_cast<int>(pb.bytes_used));
         ocs::tom::set<PACK_string>(where_elem, hex.c_str());
      }
   }
   clear_packbuffer(&pb);

   DRETURN(where_elem);
}

/**
 * @brief ocs_where_to_elem() for an untyped caller
 * @param where the condition to pack
 * @return the element, owned by the caller, or nullptr when the condition cannot be packed
 */
lListElem *lWhereToElem(const lCondition *where) {
   return ocs_where_to_elem(where).release();
}

/**
 * @brief Rebuild a selection condition from the element ocs_where_to_elem() produced
 *
 * @param where the element to unpack; an empty handle or an element of another kind is an error
 * @return the condition, to be released with `lFreeWhere()`, or nullptr on error
 */
lCondition *ocs_where_from_elem(const ocs::tom::ConstElement<ocs::tom::PACK> where) {
   DENTER(CULL_LAYER);

   lCondition *cond = nullptr;

   if (ocs::tom::get<PACK_id>(where) == SGE_WHERE) {
      sge_pack_buffer pb;
      char *buffer = nullptr;
      int ret = 0;
      const char *hex = ocs::tom::get<PACK_string>(where);
      const int size = hex != nullptr ? ocs::tom::hex_to_bytes(hex, &buffer) : 0;
      if (size <= 0) {
         // the bytes of a bad digit belong to the caller as well
         sge_free(&buffer);
         ERROR(SFNMAX, MSG_PACK_INVALIDPACKDATA);
      } else if ((ret = init_packbuffer_from_buffer(&pb, buffer, size, false)) == PACK_SUCCESS) {
         cull_unpack_cond(&pb, &cond);
         clear_packbuffer(&pb);
      } else {
         sge_free(&buffer);
         ERROR(MSG_PACK_ERRORUNPACKING_S, cull_pack_strerror(ret));
      }
   } else {
      ERROR(MSG_PACK_WRONGPACKTYPE_UI, ocs::tom::get<PACK_id>(where), SGE_WHERE);
   }

   DRETURN(cond);
}

/**
 * @brief ocs_where_from_elem() for an untyped caller
 * @param where the element to unpack, of `PACK_Type`, or nullptr
 * @return the condition, to be released with `lFreeWhere()`, or nullptr on error
 */
lCondition *lWhereFromElem(const lListElem *where) {
   return ocs_where_from_elem(ocs::tom::ConstElement<ocs::tom::PACK>(where));
}

/**
 * @brief Pack a field selection into an element, so it can be sent
 *
 * @param what the selection to pack
 * @return the element of type `SGE_WHAT`, or an empty handle when the selection cannot be packed
 */
ocs::tom::ElementPtr<ocs::tom::PACK> ocs_what_to_elem(const lEnumeration *what) {
   DENTER(CULL_LAYER);

   ocs::tom::ElementPtr<ocs::tom::PACK> what_elem;
   sge_pack_buffer pb;

   if (init_packbuffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (cull_pack_enum(&pb, what) == PACK_SUCCESS) {
         what_elem = ocs::tom::ElementPtr<ocs::tom::PACK>::create();
         ocs::tom::set<PACK_id>(what_elem, SGE_WHAT);
         const std::string hex = ocs::tom::bytes_to_hex(pb.head_ptr, static_cast<int>(pb.bytes_used));
         ocs::tom::set<PACK_string>(what_elem, hex.c_str());
      }
   }
   clear_packbuffer(&pb);

   DRETURN(what_elem);
}

/**
 * @brief ocs_what_to_elem() for an untyped caller
 * @param what the selection to pack
 * @return the element, owned by the caller, or nullptr when the selection cannot be packed
 */
lListElem *lWhatToElem(const lEnumeration *what) {
   return ocs_what_to_elem(what).release();
}

/**
 * @brief Rebuild a field selection from the element ocs_what_to_elem() produced
 *
 * @param what the element to unpack; an empty handle or an element of another kind is an error
 * @return the selection, to be released with `lFreeWhat()`, or nullptr on error
 */
lEnumeration *ocs_what_from_elem(const ocs::tom::ConstElement<ocs::tom::PACK> what) {
   DENTER(CULL_LAYER);

   lEnumeration *cond = nullptr;

   if (ocs::tom::get<PACK_id>(what) == SGE_WHAT) {
      sge_pack_buffer pb;
      char *buffer = nullptr;
      int ret = 0;
      const char *hex = ocs::tom::get<PACK_string>(what);
      const int size = hex != nullptr ? ocs::tom::hex_to_bytes(hex, &buffer) : 0;
      if (size <= 0) {
         // the bytes of a bad digit belong to the caller as well
         sge_free(&buffer);
         ERROR(SFNMAX, MSG_PACK_INVALIDPACKDATA);
      } else if ((ret = init_packbuffer_from_buffer(&pb, buffer, size, false)) == PACK_SUCCESS) {
         cull_unpack_enum(&pb, &cond);
         clear_packbuffer(&pb);
      } else {
         sge_free(&buffer);
         ERROR(MSG_PACK_ERRORUNPACKING_S, cull_pack_strerror(ret));
      }
   } else {
      ERROR(MSG_PACK_WRONGPACKTYPE_UI, ocs::tom::get<PACK_id>(what), SGE_WHAT);
   }
   DRETURN(cond);
}

/**
 * @brief ocs_what_from_elem() for an untyped caller
 * @param what the element to unpack, of `PACK_Type`, or nullptr
 * @return the selection, to be released with `lFreeWhat()`, or nullptr on error
 */
lEnumeration *lWhatFromElem(const lListElem *what) {
   return ocs_what_from_elem(ocs::tom::ConstElement<ocs::tom::PACK>(what));
}
