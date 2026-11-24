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
 * @brief Packing conditions for transport
 *
 * @see sge_pack.h
 */

#include <string>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Types.h"
#include "tom/ocs_PackCore.h"

#include "sgeobj/sge_pack.h"
#include "tom/ocs_Element.h"

#include "msg_common.h"

/**
 * @brief Run a pack function of the typed object model and report its success instead of throwing
 * @tparam F a callable taking the buffer
 * @param pb the buffer to pack into
 * @param pack the pack function, which throws ocs::tom::PackException on an error
 * @return true if it packed without error, as `PACK_SUCCESS` of tom_pack_cond() and tom_pack_enum() was
 */
template <typename F>
static bool
ocs_pack_soft(sge_pack_buffer *pb, F &&pack) {
   try {
      pack(pb);
   } catch (const ocs::tom::PackException &) {
      return false;
   }
   return true;
}

/**
 * @brief Pack a selection condition into an element, so it can be sent
 *
 * @param where the condition to pack
 * @return the element of type `SGE_WHERE`, or an empty handle when the condition cannot be packed
 */
ocs::tom::ElementPtr<ocs::tom::PACK> ocs_where_to_elem(const lCondition *where) {
   DENTER(TOM_LAYER);

   ocs::tom::ElementPtr<ocs::tom::PACK> where_elem;
   sge_pack_buffer pb;
   if (ocs::tom::try_init_buffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (ocs_pack_soft(&pb, [where](sge_pack_buffer *buffer) { ocs::tom::pack_where(buffer, where); })) {
         where_elem = ocs::tom::ElementPtr<ocs::tom::PACK>::create();
         where_elem->set<PACK_id>(SGE_WHERE);
         const std::string hex = ocs::tom::bytes_to_hex(pb.head_ptr, static_cast<int>(pb.bytes_used));
         where_elem->set<PACK_string>(hex.c_str());
      }
   }
   ocs::tom::clear_buffer(&pb);

   DRETURN(where_elem);
}

/**
 * @brief Rebuild a selection condition from the element ocs_where_to_elem() produced
 *
 * @param where the element to unpack; an empty handle or an element of another kind is an error
 * @return the condition, to be released with `lFreeWhere()`, or nullptr on error
 */
lCondition *ocs_where_from_elem(const ocs::tom::ConstElement<ocs::tom::PACK> where) {
   DENTER(TOM_LAYER);

   lCondition *cond = nullptr;

   if (where.get<PACK_id>() == SGE_WHERE) {
      sge_pack_buffer pb;
      char *buffer = nullptr;
      int ret = 0;
      const char *hex = where.get<PACK_string>();
      const int size = hex != nullptr ? ocs::tom::hex_to_bytes(hex, &buffer) : 0;
      if (size <= 0) {
         // the bytes of a bad digit belong to the caller as well
         sge_free(&buffer);
         ERROR(SFNMAX, MSG_PACK_INVALIDPACKDATA);
      } else if ((ret = ocs::tom::try_init_buffer_from(&pb, buffer, size, false)) == PACK_SUCCESS) {
         try {
            cond = ocs::tom::unpack_where(&pb);
         } catch (const ocs::tom::PackException &) {
            // bytes cut off, an unknown operator or no memory: no condition, as tom_unpack_cond() gave
         }
         ocs::tom::clear_buffer(&pb);
      } else {
         sge_free(&buffer);
         ERROR(MSG_PACK_ERRORUNPACKING_S, ocs::tom::pack_error_text(ret));
      }
   } else {
      ERROR(MSG_PACK_WRONGPACKTYPE_UI, where.get<PACK_id>(), SGE_WHERE);
   }

   DRETURN(cond);
}

/**
 * @brief Pack a field selection into an element, so it can be sent
 *
 * @param what the selection to pack
 * @return the element of type `SGE_WHAT`, or an empty handle when the selection cannot be packed
 */
ocs::tom::ElementPtr<ocs::tom::PACK> ocs_what_to_elem(const lEnumeration *what) {
   DENTER(TOM_LAYER);

   ocs::tom::ElementPtr<ocs::tom::PACK> what_elem;
   sge_pack_buffer pb;

   if (ocs::tom::try_init_buffer(&pb, 1024, false, false) == PACK_SUCCESS) {
      if (ocs_pack_soft(&pb, [what](sge_pack_buffer *buffer) { ocs::tom::pack_what(buffer, what); })) {
         what_elem = ocs::tom::ElementPtr<ocs::tom::PACK>::create();
         what_elem->set<PACK_id>(SGE_WHAT);
         const std::string hex = ocs::tom::bytes_to_hex(pb.head_ptr, static_cast<int>(pb.bytes_used));
         what_elem->set<PACK_string>(hex.c_str());
      }
   }
   ocs::tom::clear_buffer(&pb);

   DRETURN(what_elem);
}

/**
 * @brief Rebuild a field selection from the element ocs_what_to_elem() produced
 *
 * @param what the element to unpack; an empty handle or an element of another kind is an error
 * @return the selection, to be released with `lFreeWhat()`, or nullptr on error
 */
lEnumeration *ocs_what_from_elem(const ocs::tom::ConstElement<ocs::tom::PACK> what) {
   DENTER(TOM_LAYER);

   lEnumeration *cond = nullptr;

   if (what.get<PACK_id>() == SGE_WHAT) {
      sge_pack_buffer pb;
      char *buffer = nullptr;
      int ret = 0;
      const char *hex = what.get<PACK_string>();
      const int size = hex != nullptr ? ocs::tom::hex_to_bytes(hex, &buffer) : 0;
      if (size <= 0) {
         // the bytes of a bad digit belong to the caller as well
         sge_free(&buffer);
         ERROR(SFNMAX, MSG_PACK_INVALIDPACKDATA);
      } else if ((ret = ocs::tom::try_init_buffer_from(&pb, buffer, size, false)) == PACK_SUCCESS) {
         try {
            cond = ocs::tom::unpack_what(&pb);
         } catch (const ocs::tom::PackException &) {
            // bytes cut off or no memory: no selection, as tom_unpack_enum() gave
         }
         ocs::tom::clear_buffer(&pb);
      } else {
         sge_free(&buffer);
         ERROR(MSG_PACK_ERRORUNPACKING_S, ocs::tom::pack_error_text(ret));
      }
   } else {
      ERROR(MSG_PACK_WRONGPACKTYPE_UI, what.get<PACK_id>(), SGE_WHAT);
   }
   DRETURN(cond);
}
