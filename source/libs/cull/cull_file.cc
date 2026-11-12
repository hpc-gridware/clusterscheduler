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
 * @brief Writing an element into a file and reading it back, packed
 *
 * Thin wrappers over ocs::tom::write_elem_to_disk() and
 * ocs::tom::read_elem_from_disk(), which log every failure as before and
 * throw; these functions give back 1 or nullptr as before (N-L-1, N-L-2).
 */

#include "cull/cull_file.h"
#include "tom/ocs_DumpCore.h"
#include "uti/ocs_Exception.h"

/**
 * @brief Write a packed element into a file
 *
 * The file is prefix/name, or the one of the two given.
 *
 * @param ep the element
 * @param prefix the directory, or nullptr
 * @param name the file name, or nullptr
 * @param obj_name the kind of object, for the messages
 *
 * @return 0 on success, 1 on an error, which is logged
 *
 * @note A thin wrapper over ocs::tom::write_elem_to_disk().
 * @see Typed form: ocs::tom::ConstElement::write_to_disk()
 */
int lWriteElemToDisk(const lListElem *ep, const char *prefix, const char *name, const char *obj_name) {
   try {
      ocs::tom::write_elem_to_disk(ep, prefix, name, obj_name);
   } catch (const ocs::RuntimeException &) {
      return 1;
   }
   return 0;
}

/**
 * @brief Read a packed element from a file written by #lWriteElemToDisk
 *
 * @param prefix the directory, or nullptr
 * @param name the file name, or nullptr
 * @param type the descriptor of the element
 * @param obj_name the kind of object, for the messages
 *
 * @return the element, or nullptr on an error, which is logged
 *
 * @note A thin wrapper over ocs::tom::read_elem_from_disk().
 * @see Typed form: ocs::tom::ElementPtr::read_from_disk()
 */
lListElem *lReadElemFromDisk(const char *prefix, const char *name, const lDescr *type, const char *obj_name) {
   try {
      return ocs::tom::read_elem_from_disk(prefix, name, type, obj_name);
   } catch (const ocs::RuntimeException &) {
      return nullptr;
   }
}
