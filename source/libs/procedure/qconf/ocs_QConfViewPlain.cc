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

#include "uti/ocs_Pattern.h"
#include "uti/sge_spool.h"

/**
 * @brief Read the name of one list element
 *
 * The field is read as a string or as a host name depending on how the
 * element's descriptor declares it, which is what the hand written engine did.
 * The field is the one the read names (#qconf_object_desc::name_nm, or the
 * member's name field), not the primary key of the descriptor: the event
 * clients are listed by EV_host, whose primary key is EV_id.
 *
 * @param ep     the element, of an object type known only at run time
 * @param key_nm the field holding the name
 * @return the name, or nullptr when the field is unset, not held by the element
 *         or of an unexpected type
 */
static const char *
qconf_name_of(const ocs::tom::ConstRawElement ep, const int key_nm) {
   DENTER(TOP_LAYER);

   // a field the element does not hold had no type in the old lookup: the same message as for an unexpected one
   if (ep.has(key_nm)) {
      const ocs::tom::FieldDescr *descr = ep.descr();
      switch (ocs::tom::field_type_of(descr[ocs::tom::field_pos(descr, key_nm)].mt)) {
         case ocs::tom::FieldType::String:
            DRETURN(ep.get<const char *>(key_nm));
         case ocs::tom::FieldType::Host:
            DRETURN(ep.get<ocs::tom::Host>(key_nm).c_str());
         default:
            break;
      }
   }
   DPRINTF("show_name_list: unexpected data type\n");
   DRETURN(nullptr);
}

/**
 * @brief One name per line
 *
 * A name starting with the comment character is skipped, as it always was.
 *
 * The reserved host groups ask for one more thing (CS-2680, carried over in
 * CS-2820): host names and `@group` references first, matchers after them, each
 * block still in the sort order of the whole list. The output is no longer
 * necessarily a list of host names, and a script handing it to a command that
 * expects hosts has to filter the matchers out - one block boundary reduces that
 * to a single line. A configuration without a matcher renders identically,
 * character for character, because the second pass then prints nothing.
 *
 * @param os     the stream to write to
 * @param list   the objects, already sorted
 * @param key_nm the field holding the name
 * @param desc   the descriptor of the type the names belong to
 * @return none
 */
void
ocs::QConfViewPlain::show_name_list(std::ostream &os, const ocs::tom::ConstRawList list, const int key_nm,
                                    const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   const int passes = desc.read.member.split_matchers ? 2 : 1;
   for (int pass = 0; pass < passes; pass++) {
      const bool matchers = (pass == 1);
      for (const auto ep : list) {
         const char *line = qconf_name_of(ep, key_nm);

         if (line == nullptr || line[0] == COMMENT_CHAR) {
            continue;
         }
         if (passes == 2 && ocs::is_matcher(line) != matchers) {
            continue;
         }
         os << line << "\n";
      }
   }

   DRETURN_VOID;
}
