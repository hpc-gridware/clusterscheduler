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
 * @brief Rendering one `qconf` object, whatever the format
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"

#include "spool/flatfile/sge_flatfile.h"

#include "ocs_QConfViewBase.h"

/** @brief Write several objects of one type (CS-2819)
 *
 * Two shapes, and which one a type takes is in its descriptor rather than in the
 * switch: `-srqs` hands the whole list to the writer in one go, because
 * `qconf_rqs_sfi` wraps every record in braces and a loop would lose them, while
 * `-su` prints one block per object with a blank line between, the way every
 * other multi-object output of this client looks.
 *
 * @param os   the stream to write to
 * @param list the objects, in the order they are to appear
 * @param desc the descriptor of their type
 */
/**
 * @brief Pick the field list a read renders with
 *
 * A type shows what it shows and writes what it writes, and for one type those
 * differ: `-se` displays the runtime `load_values` and `processors`, which no
 * write accepts. A read whose text is going to an editor therefore renders the
 * write list, so that what comes back parses - see QConfParameter::get_for_edit().
 *
 * @param desc     the row of the object type
 * @param for_edit whether the text will be written back
 * @return the provider of the field list to render with
 */
static qconf_build_fields_fn
qconf_view_fields(const qconf_object_desc &desc, const bool for_edit) {
   if (desc.read.fields != nullptr && !for_edit) {
      return desc.read.fields;
   }
   return desc.build_fields;
}

void
ocs::QConfViewBase::show_object_list(std::ostream &os, const lList *list,
                                     const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   const spool_flatfile_format format =
      (output_format_ == ProcedureParameter::OutputFormat::JSON) ? SP_FORM_JSON : SP_FORM_ASCII;

   const qconf_build_fields_fn provider = qconf_view_fields(desc, for_edit_);
   bool owned = false;
   spooling_field *fields = provider(format, &owned);

   lList *alp = nullptr;
   if (desc.read.write_as_list) {
      const char *filename = spool_flatfile_write_list(&alp, list, fields, desc.sfi,
                                                       SP_DEST_STREAM, format, nullptr, false,
                                                       &os, line_breaks_);
      sge_free(&filename);
   } else {
      bool is_first = true;
      for_each_ep_lv(ep, list) {
         if (!is_first) {
            os << "\n";
         }
         is_first = false;
         const char *filename = spool_flatfile_write_object(&alp, ep, desc.is_root, fields, desc.sfi,
                                                            SP_DEST_STREAM, format, nullptr, false,
                                                            desc.json_type_name, &os, line_breaks_);
         sge_free(&filename);
      }
   }
   if (owned) {
      sge_free(&fields);
   }
   lFreeList(&alp);

   DRETURN_VOID;
}

/**
 * @brief Write one object through the flatfile writer
 *
 * Plain and JSON differ only in the format the writer is handed, so unlike
 * show_name_list() this needs no per-format subclass. The field list comes from
 * the object type's provider - `-se` is the one type whose read list differs
 * from the write one, because it shows the runtime load values that an export
 * has to leave out to round-trip.
 *
 * The object arrives ready to render: the JSON prepare hook, and the complex
 * entries it needs, are the model's business.
 *
 * @param os   the stream to write to
 * @param ep   the object
 * @param desc the row of its type
 */
void
ocs::QConfViewBase::show_object(std::ostream &os, const lListElem *ep,
                                const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   const spool_flatfile_format format =
      (output_format_ == ProcedureParameter::OutputFormat::JSON) ? SP_FORM_JSON
                                                                      : SP_FORM_ASCII;

   /* CS-2820: the configuration prefixes its object with a "#<name>:" comment so
    * that the blocks of "-sconf host1,host2" can be told apart. It is a comment,
    * so it has no place in JSON - which is what CS-2313a decided when the JSON
    * form was built. */
   if (desc.read.name_header && format != SP_FORM_JSON) {
      const char *name = (desc.name_nm != NoName) ? lGetHost(ep, desc.name_nm) : nullptr;
      os << "#" << ((name != nullptr) ? name : "") << ":\n";
   }

   const qconf_build_fields_fn provider = qconf_view_fields(desc, for_edit_);
   bool owned = false;
   spooling_field *fields = provider(format, &owned);

   lList *alp = nullptr;
   const char *filename = spool_flatfile_write_object(&alp, ep, desc.is_root, fields, desc.sfi,
                                                      SP_DEST_STREAM, format, nullptr, false,
                                                      desc.json_type_name, &os, line_breaks_);
   sge_free(&filename);
   if (owned) {
      sge_free(&fields);
   }
   lFreeList(&alp);

   DRETURN_VOID;
}
