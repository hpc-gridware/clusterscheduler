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
 * @brief Dumping cull objects as text and reading the text back
 *
 * Thin wrappers over the dump core of the typed object model
 * (tom/ocs_DumpCore.h), which throws; these functions give back -1 or nullptr
 * as before and leave the error number the core set (N-L-1, N-L-2).
 */

#include <cstdio>

#include "cull/cull_dump_scan.h"
#include "tom/ocs_DumpCore.h"
#include "tom/ocs_ListCore.h"

/**
 * @brief Write a descriptor (for debugging purpose)
 *
 * @param fp file pointer
 * @param dp descriptor
 * @param indent the nesting level
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::dump_descr().
 */
int lDumpDescr(FILE *fp, const lDescr *dp, int indent) {
   try {
      ocs::tom::dump_descr(fp, dp, indent);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return -1;
   }
   return 0;
}

/**
 * @brief Read a descriptor from file (debug)
 *
 * The file lists the fields that have a value; the result has the full length
 * of their type, with the others absent (CS-439, N-B-1).
 *
 * @param fp file stream
 *
 * @return descriptor, to be freed with sge_free(), or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::undump_descr().
 */
lDescr *lUndumpDescr(FILE *fp) {
   try {
      return ocs::tom::undump_descr(fp);
   } catch (const ocs::tom::ListCoreError &) {
      return nullptr;
   }
}

/**
 * @brief Dump a given element into a file
 *
 * @param fname filename
 * @param ep element
 * @param indent the nesting level
 *
 * @return error state -1 - Error 0 - OK
 *
 * @note A thin wrapper over ocs::tom::dump_elem_to().
 */
int lDumpElem(const char *fname, const lListElem *ep, int indent) {
   try {
      ocs::tom::dump_elem_to(fname, ep, indent);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return -1;
   }
   return 0;
}

/**
 * @brief Dump a given element into FILE stream
 *
 * @param fp file stream
 * @param ep element
 * @param indent the nesting level
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::dump_elem().
 * @see Typed form: ocs::tom::ConstElement::dump()
 */
int lDumpElemFp(FILE *fp, const lListElem *ep, int indent) {
   try {
      ocs::tom::dump_elem(fp, ep, indent);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return -1;
   }
   return 0;
}

/**
 * @brief Writes an object to a FILE stream
 *
 * @param fp file stream
 * @param ep object
 * @param indent the nesting level
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::dump_object().
 */
int lDumpObject(FILE *fp, const lListElem *ep, int indent) {
   try {
      ocs::tom::dump_object(fp, ep, indent);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return -1;
   }
   return 0;
}

/**
 * @brief Writes a list to a FILE stream
 *
 * @param fp file stream
 * @param lp list
 * @param indent the nesting level
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::dump_list().
 * @see Typed form: ocs::tom::ConstList::dump()
 */
int lDumpList(FILE *fp, const lList *lp, int indent) {
   try {
      ocs::tom::dump_list(fp, lp, indent);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return -1;
   }
   return 0;
}

/**
 * @brief Read element from a file written by #lDumpElem
 *
 * @param fname name of the file to read
 * @param dp descriptor
 *
 * @return Read element, or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::undump_elem_from(); unlike before, the file is closed again.
 */
lListElem *lUndumpElem(const char *fname, const lDescr *dp) {
   try {
      return ocs::tom::undump_elem_from(fname, dp);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return nullptr;
   }
}

/**
 * @brief Read element from a stream written by #lDumpElemFp
 *
 * @param fp the stream to read from
 * @param dp descriptor
 *
 * @return Read element, or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::undump_elem().
 * @see Typed form: ocs::tom::ElementPtr::undump()
 */
lListElem *lUndumpElemFp(FILE *fp, const lDescr *dp) {
   try {
      return ocs::tom::undump_elem(fp, dp);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return nullptr;
   }
}

/**
 * @brief Reads an object written by #lDumpObject
 *
 * @param fp file pointer
 *
 * @return Read object, or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::undump_object().
 */
lListElem *lUndumpObject(FILE *fp) {
   try {
      return ocs::tom::undump_object(fp);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return nullptr;
   }
}

/**
 * @brief Reads a list written by #lDumpList
 *
 * The fields the descriptor in the file and dp share are copied, the others
 * are left out.
 *
 * @param fp file pointer
 * @param name new name of list or nullptr if the old name in the dumpfile should be used as listname
 * @param dp new list descriptor or nullptr if the old list descriptor should be used as list descriptor
 *
 * @return Read list, or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::undump_list().
 * @see Typed form: ocs::tom::ListPtr::undump()
 */
lList *lUndumpList(FILE *fp, const char *name, const lDescr *dp) {
   try {
      return ocs::tom::undump_list(fp, name, dp);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number of before
      return nullptr;
   }
}
