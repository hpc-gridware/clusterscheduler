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
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfExportController.h"

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
   const int name_nm = model.get_key_nm();

   /* Created up front, so that an export matching no object sends an entry
    * holding an empty list rather than no entry at all - the difference between
    * "nothing to export" and "this was not an export", which the caller has to
    * be able to tell apart (see ProcedureModel::get_export_objects()). */
   objects_ = lCreateList("export objects", VA_Type);

   for_each_ep_lv(ep, model.get_object_list()) {
      std::ostringstream text;

      if (desc.read.write_as_list) {
         lList *one = lCreateList("export object", lGetElemDescr(ep));
         lAppendElem(one, lCopyElem(ep));
         view.show_object_list(text, one, desc);
         lFreeList(&one);
      } else {
         view.show_object(text, ep, desc);
      }

      const char *name = (name_nm != NoName) ? qconf_get_name(ep, name_nm) : nullptr;
      lListElem *pair = lAddElemStr(&objects_, VA_variable, (name != nullptr) ? name : "", VA_Type);
      lSetString(pair, VA_value, text.str().c_str());
   }

   DPRINTF("rendered %d object(s) for export\n", lGetNumberOfElem(objects_));

   DRETURN_VOID;
}
