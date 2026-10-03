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
 * @brief The plain text rendering of a `qconf` read operation
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "ocs_QConfViewPlain.h"

#include "uti/sge_spool.h"

/**
 * @brief One name per line
 *
 * The field is read as a string or as a host name depending on how the object
 * type declares it, which is what the hand written engine did; a name starting
 * with the comment character is skipped, as it always was.
 *
 * @param os     the stream to write to
 * @param list   the objects, already sorted
 * @param key_nm the field holding the name
 * @param desc   the descriptor of the type the names belong to
 */
void
ocs::QConfViewPlain::show_name_list(std::ostream &os, const lList *list, int key_nm,
                                    [[maybe_unused]] const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   const lListElem *ep;
   for_each_ep(ep, list) {
      const lDescr *descr = lGetElemDescr(ep);
      const int pos = lGetPosInDescr(descr, key_nm);
      const char *line = nullptr;

      switch (lGetPosType(descr, pos)) {
         case lStringT:
            line = lGetString(ep, key_nm);
            break;
         case lHostT:
            line = lGetHost(ep, key_nm);
            break;
         default:
            DPRINTF("show_name_list: unexpected data type\n");
            break;
      }
      if (line != nullptr && line[0] != COMMENT_CHAR) {
         os << line << "\n";
      }
   }

   DRETURN_VOID;
}
