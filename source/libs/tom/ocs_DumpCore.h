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
 * @brief Dumping as text, reading the text back, and elements in files, in the core of the typed object model
 *
 * The text form of descriptors, elements, sub-objects and lists that
 * lDumpElem() and its siblings wrote, read back by the scanner of
 * lUndumpElem() and its siblings, and the packed element files of the classic
 * spooling (lWriteElemToDisk(), lReadElemFromDisk()). The functions are
 * untyped and throw; the old functions of the list library are thin wrappers
 * that give back -1, 1 or nullptr as before (N-L-1, N-L-2). The text and the
 * error numbers are those of before; LEGACY-15 pins both.
 *
 * A `"` inside a string is written escaped. As before, a `"` at the start or
 * the end of a string, and one of two in a row, is lost: the string is split at
 * its quotes with sge_strtok(), which drops empty pieces. The dump serves
 * debugging and tests, so the text is kept as it was.
 *
 * Every failure sets the error number of the list library exactly as the old
 * code did, the outermost reader last, and throws a ListCoreError carrying
 * it. A writer whose last write fails throws without an error number, as the
 * old one returned -1 without one.
 */

#include <cstdio>

#include "tom/ocs_Types.h"

namespace ocs::tom {

   /**
    * @brief Write a descriptor as text, as lDumpDescr() did
    * @param fp the stream
    * @param descr the descriptor
    * @param indent the nesting level, three blanks each
    * @throw ListCoreError with LEFILENULL or LEDESCRNULL, or without an error number if the last write failed
    */
   void dump_descr(FILE *fp, const lDescr *descr, int indent);

   /**
    * @brief Write an element as text, as lDumpElemFp() did
    * @param fp the stream
    * @param ep the element
    * @param indent the nesting level
    * @throw ListCoreError with LEFILENULL or LEELEMNULL, or without an error number if the last write failed
    */
   void dump_elem(FILE *fp, const lListElem *ep, int indent);

   /**
    * @brief Write a sub-object with its descriptor as text, as lDumpObject() did
    * @param fp the stream
    * @param ep the object
    * @param indent the nesting level
    * @throw ListCoreError with LEFILENULL or LEELEMNULL, or without an error number if the last write failed
    */
   void dump_object(FILE *fp, const lListElem *ep, int indent);

   /**
    * @brief Write a list with its name, its descriptor and its elements as text, as lDumpList() did
    * @param fp the stream
    * @param lp the list
    * @param indent the nesting level
    * @throw ListCoreError with LEFILENULL or LELISTNULL, or without an error number if the last write failed
    */
   void dump_list(FILE *fp, const lList *lp, int indent);

   /**
    * @brief Write an element as text into a file, as lDumpElem() did
    * @param path the file, created or truncated
    * @param ep the element
    * @param indent the nesting level
    * @throw ListCoreError with LEOPEN, LECLOSE, or as dump_elem()
    */
   void dump_elem_to(const char *path, const lListElem *ep, int indent);

   /**
    * @brief Read a descriptor written by dump_descr(), as lUndumpDescr() did
    *
    * The text lists the fields that have a value; the result has the full
    * length of their type, with the others absent (N-B-1).
    *
    * @param fp the stream
    * @return the descriptor, allocated with sge_malloc() and owned by the caller
    * @throw ListCoreError with LEFILENULL, LESYNTAX, LEFIELDREAD, LEFGETDESCR or LEMALLOC
    */
   lDescr *undump_descr(FILE *fp);

   /**
    * @brief Read an element written by dump_elem(), as lUndumpElemFp() did
    * @param fp the stream
    * @param descr the descriptor of the element
    * @return the element, owned by the caller
    * @throw ListCoreError with LEFILENULL, LEDESCRNULL, LECREATEELEM, LECOUNTDESCR, LESYNTAX or LEFIELDREAD
    */
   lListElem *undump_elem(FILE *fp, const lDescr *descr);

   /**
    * @brief Read a sub-object written by dump_object(), as lUndumpObject() did
    * @param fp the stream
    * @return the object, a free element owned by the caller
    * @throw ListCoreError with LEFILENULL, LESYNTAX, LEFGETDESCR, LECOUNTDESCR or LEUNDUMPELEM
    */
   lListElem *undump_object(FILE *fp);

   /**
    * @brief Read a list written by dump_list(), as lUndumpList() did
    *
    * The fields the descriptor of the text and @p descr share are copied;
    * the others are left out.
    *
    * @param fp the stream
    * @param name the name of the list, or nullptr for the name in the text
    * @param descr the descriptor of the list, or nullptr for the one in the text
    * @return the list, owned by the caller
    * @throw ListCoreError with LEFILENULL, LESYNTAX, LEFIELDREAD, LEFGETDESCR, LECREATELIST, LECOUNTDESCR,
    *        LEMALLOC, LEUNDUMPELEM, LECREATEELEM, LECOPYSWITCH or LEAPPENDELEM
    */
   lList *undump_list(FILE *fp, const char *name, const lDescr *descr);

   /**
    * @brief Read an element from a file written by dump_elem_to(), as lUndumpElem() did
    * @param path the file
    * @param descr the descriptor of the element
    * @return the element, owned by the caller
    * @throw ListCoreError with LEOPEN, or as undump_elem()
    */
   lListElem *undump_elem_from(const char *path, const lDescr *descr);

   /**
    * @brief Pack an element into a file, as lWriteElemToDisk() did
    *
    * The file is `prefix/name`, or the one of the two given. Every failure is
    * logged with the message of before.
    *
    * @param ep the element
    * @param prefix the directory, or nullptr
    * @param name the file name, or nullptr
    * @param obj_name the kind of object, for the messages
    * @throw ocs::RuntimeException with the message logged, if no file is named, the element cannot be packed or
    *        the file cannot be written
    */
   void write_elem_to_disk(const lListElem *ep, const char *prefix, const char *name, const char *obj_name);

   /**
    * @brief Unpack an element from a file written by write_elem_to_disk(), as lReadElemFromDisk() did
    * @param prefix the directory, or nullptr
    * @param name the file name, or nullptr
    * @param descr the descriptor of the element
    * @param obj_name the kind of object, for the messages
    * @return the element, owned by the caller
    * @throw ocs::RuntimeException with the message logged, if no file is named, it cannot be read or does not
    *        hold an element
    */
   lListElem *read_elem_from_disk(const char *prefix, const char *name, const lDescr *descr, const char *obj_name);

} // namespace ocs::tom
