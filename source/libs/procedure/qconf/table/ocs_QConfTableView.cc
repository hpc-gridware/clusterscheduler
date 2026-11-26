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
 * @brief See the header (CS-2817)
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "spool/flatfile/ocs_spool_json.h"
#include "spool/flatfile/sge_flatfile.h"

#include "ocs_QConfTableView.h"
#include "sgeobj/sge_answer.h"

/**
 * @brief Write every complex entry as an aligned table
 *
 * A view is handed no answer list, so the one the two spool calls want is local
 * and dropped, exactly as QConfViewBase::show_object() does it. Anything that can
 * fail meaningfully is checked in the model.
 *
 * @param os   the stream to write to
 * @param list the complex entries
 * @param desc the CE row, for the field list and the instruction
 * @return none
 */
void
ocs::QConfTableView::show_table(std::ostream &os, const ocs::tom::ListData *list,
                                     const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   /* The hand written centry_list_show() passed qconf_opt_format straight
    * through, so `-sc -fmt json` has always produced JSON rather than the table.
    * Rendering ASCII regardless broke it, and the -ectx comparison could not see
    * that: all three contexts produced the same wrong output. */
   const spool_flatfile_format format =
      (output_format_ == ProcedureParameter::OutputFormat::JSON) ? SP_FORM_JSON : SP_FORM_ASCII;

   const qconf_build_fields_fn provider =
      (desc.read.fields != nullptr) ? desc.read.fields : desc.build_fields;
   bool owned = false;
   spooling_field *shared = provider(format, more_info_, &owned);
   // a built list comes from c_alloc() in libs/spool, a shared table is not ours to free
   const ocs::uti::c_ptr<spooling_field[]> owned_shared{owned ? shared : nullptr};

   /* align_list() writes the widths back, so it must not see the shared table */
   const ocs::uti::c_ptr<spooling_field[]> copy = qconf_copy_fields(shared);
   spooling_field *const fields = copy.get();

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   /* aligned unconditionally, as the hand written code did - the widths are
    * meaningless for JSON, but computing them changes nothing there */
   spool_flatfile_align_list(&alp, list, fields, 3);

   if (format == SP_FORM_JSON && desc.read.json_list_id != nullptr) {
      /* A type whose list has a schema id of its own: the categories are written
       * as a "category-list" holding "category" objects, so the enveloped array
       * does not collide with the single record of -scat. An empty list still
       * yields the enveloped empty array, which keeps the output valid JSON. */
      dstring out = DSTRING_INIT;
      spool_json_write_typed_list_ex(&alp, list, fields, desc.read.json_list_id,
                                     desc.json_type_name, &out);
      const char *text = sge_dstring_get_string(&out);
      os << ((text != nullptr) ? text : "");
      sge_dstring_free(&out);
   } else {
      // the name of the stream written, from strdup() in libs/spool
      const ocs::uti::c_ptr<const char> filename{spool_flatfile_write_list(&alp, list, fields, desc.read.list_sfi,
                                                                           SP_DEST_STREAM, format, nullptr,
                                                                           false, &os, line_breaks_)};
   }
   alp.reset();

   DRETURN_VOID;
}
