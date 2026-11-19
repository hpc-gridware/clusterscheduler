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
 * @brief Packing and unpacking of elements, lists, selections and conditions
 *
 * Thin wrappers over the pack core of the typed object model
 * (tom/ocs_PackCore.h), which throws; these functions return the PACK_* code
 * of the exception, and nullptr in the result, as before (N-L-1, N-L-2).
 */

#include <cstring>
#include <string>

#include "uti/sge_stdlib.h"

#include "cull/cull_list.h"
#include "cull/cull_multitype.h"
#include "cull/cull_pack.h"
#include "tom/ocs_PackCore.h"

/**
 * @brief Give up the descriptor table of a buffer
 * @param pb the buffer; its table is released and set to nullptr
 *
 * @note A thin wrapper over ocs::tom::free_descr_table(); the exception is returned as its PACK_* code.
 */
void ocs_cull_pack_table_free(sge_pack_buffer *pb) {
   ocs::tom::free_descr_table(pb);
}

/**
 * @brief Number of entries in the descriptor table of a buffer
 * @param pb the buffer
 * @return the number of distinct reduced descriptors written to or read from it
 *
 * @note A thin wrapper over ocs::tom::descr_table_size(); the exception is returned as its PACK_* code.
 */
size_t ocs_cull_pack_table_size(const sge_pack_buffer *pb) {
   return ocs::tom::descr_table_size(pb);
}

/**
 * @brief Pack an object type descriptor
 *
 * A static descriptor is written as a reference to its type, a reduced one
 * through the descriptor table of the buffer; with flags it is written field by
 * field (N-W-3, N-W-4, N-W-8).
 *
 * @param pb the buffer to append to
 * @param dp the descriptor to pack
 * @param flags the flags the values are packed with, 0 for all values
 * @return #PACK_SUCCESS, #PACK_ENOMEM or #PACK_BADARG
 *
 * @note A thin wrapper over ocs::tom::pack_descr(); the exception is returned as its PACK_* code.
 */
int cull_pack_descr(sge_pack_buffer *pb, const lDescr *dp, const int flags) {
   try {
      ocs::tom::pack_descr(pb, dp, flags);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a field selection as if it were a descriptor
 *
 * Lets the receiver build a reduced object type from the selection alone.
 *
 * @param pb the buffer to append to
 * @param what the selection to pack
 * @param descr the descriptor the selection refers to
 * @param flags the flags the values are packed with, 0 for all values
 * @return #PACK_SUCCESS, #PACK_ENOMEM, #PACK_FORMAT, or #PACK_BADARG if a selected field is not present in descr
 *
 * @note A thin wrapper over ocs::tom::pack_what_as_descr(); the exception is returned as its PACK_* code.
 */
int cull_pack_enum_as_descr(sge_pack_buffer *pb, const lEnumeration *what, const lDescr *descr, const int flags) {
   try {
      ocs::tom::pack_what_as_descr(pb, what, descr, flags);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a whole element, including its descriptor
 *
 * @param pb the buffer to append to
 * @param ep the element to pack
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::ConstElement::pack()
 *
 * @note A thin wrapper over ocs::tom::pack_elem(); the exception is returned as its PACK_* code.
 */
int cull_pack_elem(sge_pack_buffer *pb, const lListElem *ep) {
   try {
      ocs::tom::pack_elem(pb, ep, nullptr, 0);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack the selected fields of an element
 *
 * @param pb the buffer to append to
 * @param ep the element to pack
 * @param what which fields to pack, or nullptr for all of them
 * @param flags packing options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 *
 * @note A thin wrapper over ocs::tom::pack_elem(); the exception is returned as its PACK_* code.
 */
int cull_pack_elem_partial(sge_pack_buffer *pb, const lListElem *ep, const lEnumeration *what, int flags) {
   try {
      ocs::tom::pack_elem(pb, ep, what, flags);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read an element written by #cull_pack_elem
 *
 * @param pb the buffer to read from
 * @param[out] epp receives the element, owned by the caller; nullptr on an error
 * @param dp nullptr for a free element, the descriptor of a bound one
 * @return #PACK_SUCCESS, #PACK_ENOMEM, #PACK_BADARG, or #PACK_FORMAT
 * @see Typed form: ocs::tom::ElementPtr::unpack()
 *
 * @note A thin wrapper over ocs::tom::unpack_elem(); the exception is returned as its PACK_* code.
 */
int cull_unpack_elem(sge_pack_buffer *pb, lListElem **epp, const lDescr *dp) {
   try {
      *epp = ocs::tom::unpack_elem(pb, dp, 0);
   } catch (const ocs::tom::PackException &e) {
      *epp = nullptr;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read an element written by #cull_pack_elem_partial
 *
 * A free element takes its object type from the buffer. With flags, a full type
 * there must be dp or a shorter version of it (N-W-6), else the element is
 * rejected as a format error.
 *
 * @param pb the buffer to read from
 * @param[out] epp receives the element, owned by the caller; nullptr on an error
 * @param dp the object type of a bound element; for a free element of flag-filtered data the expected one, or nullptr
 * @param flags unpacking options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, #PACK_BADARG, or #PACK_FORMAT
 *
 * @note A thin wrapper over ocs::tom::unpack_elem(); the exception is returned as its PACK_* code.
 */
int cull_unpack_elem_partial(sge_pack_buffer *pb, lListElem **epp, const lDescr *dp, int flags) {
   try {
      *epp = ocs::tom::unpack_elem(pb, dp, flags);
   } catch (const ocs::tom::PackException &e) {
      *epp = nullptr;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a whole list, including its descriptor
 *
 * @param pb the buffer to append to
 * @param lp the list to pack, or nullptr
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::ConstList::pack()
 *
 * @note A thin wrapper over ocs::tom::pack_list(); the exception is returned as its PACK_* code.
 */
int cull_pack_list(sge_pack_buffer *pb, const lList *lp) {
   try {
      ocs::tom::pack_list(pb, lp, nullptr, 0);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack the selected fields of every element of a list
 *
 * @param pb the buffer to append to
 * @param lp the list to pack, or nullptr
 * @param what which fields to pack, or nullptr for all of them
 * @param flags packing options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 *
 * @note A thin wrapper over ocs::tom::pack_list(); the exception is returned as its PACK_* code.
 */
int cull_pack_list_partial(sge_pack_buffer *pb, const lList *lp, lEnumeration *what, int flags) {
   try {
      ocs::tom::pack_list(pb, lp, what, flags);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a list written by #cull_pack_list
 *
 * @param pb the buffer to read from
 * @param[out] lpp receives the list, owned by the caller; nullptr on an error
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::ListPtr::unpack()
 *
 * @note A thin wrapper over ocs::tom::unpack_list(); the exception is returned as its PACK_* code.
 */
int cull_unpack_list(sge_pack_buffer *pb, lList **lpp) {
   try {
      *lpp = ocs::tom::unpack_list(pb, 0);
   } catch (const ocs::tom::PackException &e) {
      *lpp = nullptr;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a list written by #cull_pack_list_partial
 *
 * A list from another process may hold a key of a unique field twice; the
 * receiver verifies what it takes in. The debug checks for duplicate keys are
 * suspended while it is read (N-H-18).
 *
 * @param pb the buffer to read from
 * @param[out] lpp receives the list, owned by the caller; nullptr on an error
 * @param flags unpacking options
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 *
 * @note A thin wrapper over ocs::tom::unpack_list(); the exception is returned as its PACK_* code.
 */
int cull_unpack_list_partial(sge_pack_buffer *pb, lList **lpp, int flags) {
   try {
      *lpp = ocs::tom::unpack_list(pb, flags);
   } catch (const ocs::tom::PackException &e) {
      *lpp = nullptr;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a field selection
 *
 * @param pb the buffer to append to
 * @param enp the selection to pack, or nullptr
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::What::pack()
 *
 * @note A thin wrapper over ocs::tom::pack_what(); the exception is returned as its PACK_* code.
 */
int cull_pack_enum(sge_pack_buffer *pb, const lEnumeration *enp) {
   try {
      ocs::tom::pack_what(pb, enp);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a field selection written by #cull_pack_enum
 *
 * @param pb the buffer to read from
 * @param[out] enpp receives the selection, owned by the caller; nullptr on an error
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::What::unpack()
 *
 * @note A thin wrapper over ocs::tom::unpack_what(); the exception is returned as its PACK_* code.
 */
int cull_unpack_enum(sge_pack_buffer *pb, lEnumeration **enpp) {
   try {
      *enpp = ocs::tom::unpack_what(pb);
   } catch (const ocs::tom::PackException &e) {
      *enpp = nullptr;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a selection condition
 *
 * @param pb the buffer to append to
 * @param cp the condition to pack, or nullptr
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::Where::pack()
 *
 * @note A thin wrapper over ocs::tom::pack_where(); the exception is returned as its PACK_* code.
 */
int cull_pack_cond(sge_pack_buffer *pb, const lCondition *cp) {
   try {
      ocs::tom::pack_where(pb, cp);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Read a selection condition written by #cull_pack_cond
 *
 * @param pb the buffer to read from
 * @param[out] cpp receives the condition, owned by the caller; nullptr on an error
 * @return #PACK_SUCCESS, #PACK_ENOMEM, or #PACK_FORMAT
 * @see Typed form: ocs::tom::Where::unpack()
 *
 * @note A thin wrapper over ocs::tom::unpack_where(); the exception is returned as its PACK_* code.
 */
int cull_unpack_cond(sge_pack_buffer *pb, lCondition **cpp) {
   try {
      *cpp = ocs::tom::unpack_where(pb);
   } catch (const ocs::tom::PackException &e) {
      *cpp = nullptr;
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Store a byte array as text in a string field: two hex digits per byte, the low half first
 *
 * @param byteArray the bytes; nothing is stored for nullptr
 * @param size how many
 * @param elem the element; nothing is stored for nullptr
 * @param name the string field
 *
 * @note The text is made by ocs::tom::bytes_to_hex().
 */
void setByteArray(const char *byteArray, int size, lListElem *elem, int name) {
   if (!byteArray || !elem) {
      return;
   }
   lSetString(elem, name, ocs::tom::bytes_to_hex(byteArray, size).c_str());
}

/**
 * @brief Read a byte array stored by #setByteArray
 *
 * @param[out] byte receives the bytes, allocated and owned by the caller; also on a bad digit
 * @param elem the element
 * @param name the string field
 * @return the number of bytes; 0 for a null argument or an unset field; for a bad digit the negative of its
 *         position, with -1 counted from the second digit of a byte
 *
 * @note The bytes are read by ocs::tom::hex_to_bytes().
 */
int getByteArray(char **byte, const lListElem *elem, int name) {
   if (!byte || !elem) {
      return 0;
   }
   const char *string = lGetString(elem, name);
   if (string == nullptr) {
      // unset string attribute: return 0 so callers treat it as invalid pack
      // data instead of dereferencing NULL in strlen() (CWE-476, CS-2341)
      return 0;
   }
   return ocs::tom::hex_to_bytes(string, byte);
}
