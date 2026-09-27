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
 * @brief The host group views of the `qconf` stored procedure (CS-2816)
 *
 * The host group reads are the first qconf output the object descriptor table
 * cannot express, so they are a sub-procedure rather than a row in
 * qconf_read_table - the shape qstat has used for its four output formats all
 * along: one model, and a view per shape.
 *
 * For `-shgrp` the plain form *is* the table's: the hand written code rendered
 * it with HGRP_fields and qconf_sfi, which is exactly what the HGRP row holds,
 * so QConfViewPlain serves it unchanged. Only the JSON form needs its own view.
 */

#include "ocs_QConfModelBase.h"
#include "ocs_QConfViewJSON.h"

namespace ocs {
   /** @brief `qconf -shgrp -fmt json`: the member list with its classes split out
    *
    * CS-2680 gave the machine readable form three keys of their own - hosts,
    * hostgroups and matchers - so that a program need not guess the class of an
    * entry from its text. They are written *beside* the unchanged `hostlist`,
    * which is the definition as stored and what `-Mhgrp` reads back, so the
    * round trip stays idempotent.
    *
    * Only a group carrying a matcher gets them. Splitting the entries of every
    * other group would change the output of installations that never heard of
    * the feature, and a member list without matchers holds nothing a reader
    * cannot already tell apart by the leading `@`.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupShowViewJSON : public QConfViewJSON {
   public:
      explicit QConfHGroupShowViewJSON(const QConfParameter &parameter) : QConfViewJSON(parameter) {}
      ~QConfHGroupShowViewJSON() override = default;

      void show_object(std::ostream &os, const lListElem *ep, const qconf_object_desc &desc) override;
   };

   /** @brief `qconf -shgrp_tree` and `-shgrp_resolved`
    *
    * A host group may contain other host groups, so there are two useful views:
    * the tree, which shows where each host comes from, and the resolved list,
    * which shows what the group finally means.
    *
    * Both need every host group, not the one named, which is why they cannot be
    * a row in qconf_read_table. Neither has a JSON form: the hand written option
    * blocks ignored `-fmt` and printed plain text, and that is kept.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupStructureView : public QConfViewBase {
   public:
      explicit QConfHGroupStructureView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfHGroupStructureView() override = default;

      /** @brief Not reached: the structure controller renders through the two
       *         methods below, never through the generic name list */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm) override {
      }

      void show_tree(std::ostream &os, const lList *hgroup_list, const char *name);
      void show_resolved(std::ostream &os, const lListElem *hgroup, const lList *hgroup_list);
   };

   /** @brief `qconf -shgrp_why`: by which route a host is a member
    *
    * The route was decided in the model, where the graph and the answer list are;
    * this only words it. No JSON form, as above.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupWhyView : public QConfViewBase {
   public:
      explicit QConfHGroupWhyView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfHGroupWhyView() override = default;

      /** @brief Not reached, see QConfHGroupStructureView::show_name_list() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm) override {
      }

      void show_why(std::ostream &os, const QConfModelBase::HGroupWhy &why);
   };
}
