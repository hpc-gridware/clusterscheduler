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
 * @brief See the header (CS-2816)
 */

#include <cstdarg>

#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"

#include "ocs_QConfHGroupStructureView.h"

/** @brief What `qconf_cstr()` does in the client, for the library side
 *
 * `printf("%s", nullptr)` is undefined behaviour and glibc happens to print
 * `(null)`; CS-1841 kept that marker rather than turn a printed marker into a
 * crash when the print sites moved to a stream. The client's helper has 81 call
 * sites and stays where it is - this is the same rule for the one place in the
 * library that needs it.
 *
 * @param s the string, may be nullptr
 * @return @p s, or `(null)`
 */
static constexpr const char *
hgroup_text(const char *s) noexcept {
   return (s != nullptr) ? s : "(null)";
}

/** @brief One line of the host group tree, then its children
 *
 * @param os          the stream to write to
 * @param hgroup_list every host group, for looking references up
 * @param name        the entry to write
 * @param indent      how deep it sits
 */
static void
hgroup_tree_elem(std::ostream &os, const lList *hgroup_list, const char *name, int indent) {
   DENTER(TOP_LAYER);

   const char *const indent_string = "   ";
   for (int i = 0; i < indent; i++) {
      os << indent_string;
   }

   /*
    * CS-2680, N-I-6. A matcher is a leaf: it references nothing that could be
    * expanded, and looking it up as a group name would be asking the wrong
    * question. It is marked, because the tree is where the shape of a group is
    * read off and a rule among the names should not read like one more name.
    */
   if (ocs::is_matcher(name)) {
      os << hgroup_text(name) << " (matcher)\n";
      DRETURN_VOID;
   }
   os << hgroup_text(name) << "\n";

   const lListElem *hgroup = lGetElemHost(hgroup_list, HGRP_name, name);
   if (hgroup != nullptr) {
      const lList *sub_list = lGetList(hgroup, HGRP_host_list);

      for_each_ep_lv(href, sub_list) {
         hgroup_tree_elem(os, hgroup_list, lGetHost(href, HR_name), indent + 1);
      }
   }

   DRETURN_VOID;
}

/** @brief `qconf -shgrp_tree`: the group and everything it contains, indented
 *
 * @param os          the stream to write to
 * @param hgroup_list every host group
 * @param name        the group to start from
 */
void
ocs::QConfHGroupStructureView::show_tree(std::ostream &os, const lList *hgroup_list,
                                         const char *name) {
   DENTER(TOP_LAYER);
   hgroup_tree_elem(os, hgroup_list, name, 0);
   DRETURN_VOID;
}

/** @brief `qconf -shgrp_resolved`: which hosts the group finally means
 *
 * CS-2680, N-I-5. The resolved membership says which hosts the system knows are
 * members, never which hosts the group would admit. For a group carrying
 * matchers those are two different things, and the gap may be total - a submit
 * host group whose population is disjoint from the execution host list resolves
 * to nothing while admitting a host on every request. So the matchers are shown
 * with it, and the list above is never left standing on its own as "the
 * membership". Without matchers nothing is added and the output is unchanged.
 *
 * @param os          the stream to write to
 * @param hgroup      the group
 * @param hgroup_list every host group, for following references
 */
void
ocs::QConfHGroupStructureView::show_resolved(std::ostream &os, const lListElem *hgroup,
                                             const lList *hgroup_list) {
   DENTER(TOP_LAYER);

   lList *answer_list = nullptr;   // as in show_object(): a view has none to report into
   lList *sub_host_list = nullptr;
   lList *sub_hgroup_list = nullptr;
   lList *sub_matcher_list = nullptr;
   dstring string = DSTRING_INIT;

   hgroup_find_all_references(hgroup, &answer_list, hgroup_list, &sub_host_list,
                              &sub_hgroup_list, &sub_matcher_list);
   href_list_make_uniq(sub_host_list, &answer_list);
   href_list_append_to_dstring(sub_host_list, &string);
   if (sge_dstring_get_string(&string) != nullptr) {
      os << sge_dstring_get_string(&string) << "\n";
   }

   if (sub_matcher_list != nullptr) {
      dstring matchers = DSTRING_INIT;

      href_list_make_uniq(sub_matcher_list, &answer_list);
      href_list_append_to_dstring(sub_matcher_list, &matchers);
      os << "matchers: " << hgroup_text(sge_dstring_get_string(&matchers)) << "\n";
      sge_dstring_free(&matchers);
   }

   sge_dstring_free(&string);
   lFreeList(&sub_host_list);
   lFreeList(&sub_hgroup_list);
   lFreeList(&sub_matcher_list);
   lFreeList(&answer_list);

   DRETURN_VOID;
}
