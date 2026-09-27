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
 * @brief The host group views of the `qconf` stored procedure (CS-2816)
 */

#include <cstdarg>

#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/ocs_Pattern.h"
#include "basis_types.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"

#include "spool/flatfile/ocs_spool_json.h"

#include "ocs_QConfHGroupView.h"
#include "msg_qconf.h"

/** @brief Write one host group as JSON, with its member classes split out
 *
 * The derived lists are computed from the member list at render time, which is
 * why this cannot go through spool_flatfile_write_object(): that one renders the
 * fields of the object, and these are three keys that are not fields. A group
 * without matchers is rendered by the generic path, so nothing changes for an
 * installation that does not use them.
 *
 * Errors are dropped the same way QConfViewBase::show_object() drops them: a
 * view is handed no answer list, because by the time one is chosen the framework
 * has no way left to report. Anything that can fail meaningfully is checked in
 * the model.
 *
 * @param os   the stream to write to
 * @param ep   the host group
 * @param desc its row in the descriptor table, for the field list
 */
void
ocs::QConfHGroupShowViewJSON::show_object(std::ostream &os, const lListElem *ep,
                                          const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   lList *derived_hosts = nullptr;
   lList *derived_groups = nullptr;
   lList *derived_matchers = nullptr;

   if (!hgroup_split_members(ep, &derived_hosts, &derived_groups, &derived_matchers)) {
      /* no matchers: the generic rendering is the whole answer */
      lFreeList(&derived_hosts);
      lFreeList(&derived_groups);
      lFreeList(&derived_matchers);
      QConfViewBase::show_object(os, ep, desc);
      DRETURN_VOID;
   }

   const ocs_json_derived_list derived[] = {
      {"hosts",      derived_hosts,    HR_name},
      {"hostgroups", derived_groups,   HR_name},
      {"matchers",   derived_matchers, HR_name},
      {nullptr,      nullptr,          0}
   };

   const qconf_build_fields_fn provider =
      (desc.read.fields != nullptr) ? desc.read.fields : desc.build_fields;
   bool owned = false;
   spooling_field *fields = provider(SP_FORM_JSON, &owned);

   lList *alp = nullptr;
   dstring document = DSTRING_INIT;
   if (spool_json_write_object_ex(&alp, ep, fields, derived, &document)) {
      const char *text = sge_dstring_get_string(&document);
      if (text != nullptr) {
         os << text;
      }
   }
   sge_dstring_free(&document);
   if (owned) {
      sge_free(&fields);
   }
   lFreeList(&alp);

   lFreeList(&derived_hosts);
   lFreeList(&derived_groups);
   lFreeList(&derived_matchers);

   DRETURN_VOID;
}

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

/** @brief Write a catalogue message to a stream
 *
 * The message macros expand to a catalogue lookup rather than to a string
 * literal, so they cannot be streamed directly and the newline cannot be
 * concatenated onto them. The client solves this with qconf_msg(); this is the
 * same thing for the library.
 *
 * @param os  the stream to write to
 * @param fmt the message, a printf format
 */
static void
hgroup_msg(std::ostream &os, const char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   dstring buffer = DSTRING_INIT;
   sge_dstring_vsprintf(&buffer, fmt, ap);
   va_end(ap);
   const char *text = sge_dstring_get_string(&buffer);
   if (text != nullptr) {
      os << text;
   }
   sge_dstring_free(&buffer);
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

/** @brief `qconf -shgrp_why`: word the route the model found
 *
 * @param os  the stream to write to
 * @param why what the model decided
 */
void
ocs::QConfHGroupWhyView::show_why(std::ostream &os, const QConfModelBase::HGroupWhy &why) {
   DENTER(TOP_LAYER);

   const char *host = why.host.c_str();
   const char *in = why.found_in.empty() ? nullptr : why.found_in.c_str();

   switch (why.route) {
      case Matcher::Route::LITERAL:
         hgroup_msg(os, MSG_QCONF_WHY_LITERAL_S, host);
         break;
      case Matcher::Route::GROUP_REFERENCE:
         hgroup_msg(os, MSG_QCONF_WHY_GROUP_SS, host, in != nullptr ? in : "");
         break;
      case Matcher::Route::MATCHER:
         if (in != nullptr) {
            hgroup_msg(os, MSG_QCONF_WHY_MATCHER_GROUP_SSS, host, why.matcher.c_str(), in);
         } else {
            hgroup_msg(os, MSG_QCONF_WHY_MATCHER_SS, host, why.matcher.c_str());
         }
         break;
      case Matcher::Route::NOT_A_MEMBER:
         hgroup_msg(os, MSG_QCONF_WHY_NOT_S, host);
         break;
   }
   os << "\n";

   DRETURN_VOID;
}
