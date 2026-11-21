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
 * @brief Writing elements, lists, selections and conditions as text for debugging, in the core of the typed
 *        object model
 *
 * The text that lWriteElemTo(), lWriteElemToStr(), lWriteListTo(),
 * lWriteWhatTo(), lWriteWhatToDString() and lWriteWhereTo() wrote; those old
 * functions are thin wrappers over these (N-L-1, N-L-2). The text is that of
 * before, byte for byte; LEGACY-16 pins it. The text serves debugging only
 * and is not read back; dump_elem() and its siblings write the form that is.
 */

#include <cstdio>

#include "uti/sge_dstring.h"

#include "tom/ocs_Types.h"

namespace ocs::tom {

   /**
    * @brief Append an element as text, as lWriteElemToStr() did
    * @param buffer the string to append to
    * @param ep the element
    * @param level the nesting level, three blanks each
    * @throw ListCoreError with LEELEMNULL if @p ep is null
    */
   void print_elem(dstring *buffer, const lListElem *ep, int level);

   /**
    * @brief Append a list with its name, its number of elements and its elements as text, as lWriteListTo() did
    * @param buffer the string to append to
    * @param lp the list
    * @param level the nesting level, three blanks each
    * @throw ListCoreError with LELISTNULL if @p lp is null
    */
   void print_list(dstring *buffer, const lList *lp, int level);

   /**
    * @brief Append a selection with its nested selections as text, as lWriteWhatToDString() did
    * @param buffer the string to append to
    * @param what the selection
    * @param level the nesting level, three blanks each
    * @throw ListCoreError with LEENUMNULL if @p what is null
    */
   void print_what(dstring *buffer, const lEnumeration *what, int level);

   /**
    * @brief Append a condition as text, as lWriteWhereTo() did
    *
    * An operator the core does not know sets LEOPUNKNOWN and ends the text of
    * its own sub-condition only, as before; an operand that is null sets
    * LECONDNULL and is left out.
    *
    * @param buffer the string to append to
    * @param cp the condition
    * @param level the nesting level, three blanks each
    * @throw ListCoreError with LECONDNULL if @p cp is null
    */
   void print_where(dstring *buffer, const lCondition *cp, int level);

   /**
    * @brief Write the text a print function appends to a string into a stream, freeing the string also on an
    *        exception
    * @param fp the stream, or nullptr to write nothing
    * @param print appends the text, e.g. a call of print_elem()
    * @throw whatever @p print throws; nothing is written then
    */
   template <typename Print>
   void print_to(FILE *fp, const Print &print) {
      struct Text {
         dstring buffer = DSTRING_INIT;   ///< the text

         /** @brief Free the text */
         ~Text() {
            sge_dstring_free(&buffer);
         }
      } text;
      print(&text.buffer);
      const char *str = sge_dstring_get_string(&text.buffer);
      if (fp != nullptr && str != nullptr) {
         fputs(str, fp);
      }
   }

} // namespace ocs::tom
