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

#include <sstream>

#include "cull/cull.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfExportController.h"

/**
 * @brief Render one fetched object and append it, paired with its name, to the export pairs
 *
 * The part of the export that does not depend on the object type, so that it
 * is compiled once rather than once per type: the rendering through the view,
 * which takes untyped elements and lists anyway, and the pair it produces.
 *
 * @param objects the pairs to append to
 * @param view    the rendering
 * @param desc    the object type's row
 * @param ep      the object
 * @param one     a list holding a copy of @p ep alone, rendered instead of @p ep for a type whose text
 *                holds a list; nullptr for every other type
 * @param name    the name the object's file is called after, nullptr for a singleton
 * @return none
 */
static void
ocs_qconf_export_one(const ocs::tom::List<ocs::tom::VA> objects, ocs::QConfViewBase &view,
                     const qconf_object_desc &desc, const lListElem *ep, const lList *one, const char *name) {
   DENTER(TOP_LAYER);
   std::ostringstream text;

   if (one != nullptr) {
      view.show_object_list(text, one, desc);
   } else {
      view.show_object(text, ep, desc);
   }

   const auto pair = objects.emplace_back<VA_variable>((name != nullptr) ? name : "");
   ocs::tom::set<VA_value>(pair, text.str().c_str());
   DRETURN_VOID;
}

/**
 * @brief Render every fetched object into a file text of its own
 *
 * One pass over the snapshot, one text per object, paired with the name its
 * file is called after. A singleton has no name field and contributes an empty
 * one: its destination is the path the user gave, so the caller never needs a
 * name back - and inventing one would make it look as if the type had objects
 * to choose between.
 *
 * A type whose text holds a list - the resource quota sets - is rendered
 * through show_object_list() with a list of one, because its `sfi` wraps every
 * record in braces and a bare object would come back out of its frame. That is
 * the same one-element detour the client made while it rendered for itself.
 *
 * Per object type only the walk over the typed snapshot, the copy into the list
 * of one and the name stay here; the rendering and the pair are
 * ocs_qconf_export_one().
 *
 * @param parameter what the call asked for; unused, the export form is the view's
 * @param model     the snapshot
 * @param view      the rendering
 * @return none
 */
void
ocs::QConfExportController::process_request([[maybe_unused]] QConfParameter &parameter,
                                            QConfModelBase &model, QConfViewBase &view) {
   DENTER(TOP_LAYER);

   const qconf_object_desc &desc = *model.get_desc();

   /* Created up front, so that an export matching no object sends an entry
    * holding an empty list rather than no entry at all - the difference between
    * "nothing to export" and "this was not an export", which the caller has to
    * be able to tell apart (see ProcedureModel::get_export_objects()). */
   objects_ = ocs::tom::ListPtr<ocs::tom::VA>::create("export objects");

   ocs_with_qconf_object(model.get_object(), [this, &model, &view, &desc](const auto &entry) {
      using E = ocs_qconf_object_entry_t<decltype(entry)>;
      using T = typename E::type;
      // the snapshot was fetched with the descriptor of the read's object type
      for (const auto ep : ocs::tom::ConstList<T>::from_typed(model.get_object_list())) {
         ocs::tom::ListPtr<T> one;
         if (desc.read.write_as_list) {
            // the element's own descriptor, as the fetch may have reduced it
            one = ocs::tom::ListPtr<T>::create_like("export object", ep);
            one->push_back(ep.clone());
         }
         ocs_qconf_export_one(*objects_, view, desc, ep, one.get(), ocs_qconf_key_of<E>(ep));
         one.reset();
      }
   });

   DPRINTF("rendered %d object(s) for export\n", static_cast<int>(objects_->size()));

   DRETURN_VOID;
}
