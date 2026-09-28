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

#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"

#include "spool/flatfile/ocs_spool_json.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "ocs_QConfCQueueShowView.h"

#include "msg_common.h"

/** @brief What `qconf_cstr()` does in the client, for the library side
 *
 * Second copy of this rule in the procedure library, after the host group view.
 * The client's helper has 81 call sites and stays where it is; a third copy here
 * would be the point to promote it to one place.
 *
 * @param s the string, may be nullptr
 * @return @p s, or `(null)`
 */
static constexpr const char *
cqueue_text(const char *s) noexcept {
   return (s != nullptr) ? s : "(null)";
}

/** @brief Write the complex_values field, leaving out the slots entry
 *
 * The format replicates the `qconf_sub_name_value_space_sfi` spooling
 * instruction. If that instruction changes, or if `qconf_sfi` starts using a
 * different sub-instruction, this has to follow, or the complex values print in
 * a different shape from every other attribute.
 *
 * Two defects came along with the move (CS-2818) and are fixed here: the numeric
 * fallback read `CE_doubleval` from the queue instance rather than from the
 * complex entry, a field that is not in that descriptor, so it printed zero; and
 * the value was appended through a `strdup()` that nothing freed, although
 * sge_dstring_append() copies.
 *
 * @param ep     the QU element holding the complex values
 * @param nm     the field, `QU_consumable_config_list`
 * @param buffer the dstring the field is printed into
 * @param alp    unused; this writer cannot fail
 * @return 1, the flatfile writer's "written" answer
 */
static int
write_QU_consumable_config_list(const lListElem *ep, int nm, dstring *buffer,
                                [[maybe_unused]] lList **alp) {
   DENTER(TOP_LAYER);
   const lList *lp = lGetList(ep, nm);
   bool first = true;
   bool has_elems = false;

   for_each_ep_lv(vep, lp) {
      const char *name = lGetString(vep, CE_name);

      if (strcmp(name, "slots") != 0) {
         /* whether any element of the list was printed decides the NONE below */
         has_elems = true;

         /* a separating space for every element after the first */
         if (first) {
            first = false;
         } else {
            sge_dstring_append(buffer, " ");
         }

         sge_dstring_append(buffer, name);
         sge_dstring_append(buffer, "=");

         const char *strval = lGetString(vep, CE_stringval);
         if (strval != nullptr) {
            sge_dstring_append(buffer, strval);
         } else {
            sge_dstring_sprintf_append(buffer, "%f", lGetDouble(vep, CE_doubleval));
         }
      }
   }

   /* CR 6433628: NONE rather than an empty value when there are none */
   if (!has_elems) {
      sge_dstring_append(buffer, NONE_STR);
   }
   DRETURN(1);
}

/** @brief Give the complex_values field of a QU field list its own writer
 *
 * So that the "slots" complex value is left out of the printed list.
 *
 * @param fields the QU field list about to be used for spooling
 */
static void
insert_custom_complex_values_writer(spooling_field *fields) {
   DENTER(TOP_LAYER);

   int count = 0;
   while (fields[count].nm != NoName && fields[count].nm != QU_consumable_config_list) {
      count++;
   }
   if (fields[count].nm == QU_consumable_config_list) {
      fields[count].write_func = write_QU_consumable_config_list;
   }

   DRETURN_VOID;
}

/** @brief Write a list of queue instances
 *
 * JSON gets one array for the whole list, so that a query matching several hosts
 * stays a single document; plain output writes one block per instance, separated
 * by blank lines. The type name `qinstance` is passed explicitly because it
 * cannot be told from the object's content - it is mis-detected as an execution
 * host.
 *
 * @param os          the stream to write to
 * @param answer_list used to return error messages
 * @param qi_list     the instances to write
 * @return true on success; false with @p answer_list filled otherwise
 */
bool
ocs::QConfCQueueShowView::write_qinstance_list(std::ostream &os, lList **answer_list,
                                              const lList *qi_list) {
   DENTER(TOP_LAYER);

   /* sge_build_QU_field_list() allocates, so this array is ours to modify -
      unlike the shared tables the providers hand out, see qconf_copy_fields() */
   spooling_field *fields = sge_build_QU_field_list(true, false);
   /* Bugfix Issuezilla #1198: keep slots out of the complex values list */
   insert_custom_complex_values_writer(fields);

   const spool_flatfile_format format =
      (output_format_ == ProcedureParameter::OutputFormat::JSON) ? SP_FORM_JSON : SP_FORM_ASCII;

   if (format == SP_FORM_JSON) {
      dstring out = DSTRING_INIT;
      spool_json_write_typed_list(answer_list, qi_list, fields, "qinstance", &out);
      os << cqueue_text(sge_dstring_get_string(&out));
      sge_dstring_free(&out);
   } else {
      bool is_first = true;
      for_each_ep_lv(qi, qi_list) {
         if (!is_first) {
            os << "\n";
         }
         is_first = false;
         const char *fn = spool_flatfile_write_object(answer_list, qi, false, fields,
                                                      &qconf_sfi, SP_DEST_STREAM,
                                                      format, nullptr, false, "qinstance",
                                                      &os, line_breaks_);
         sge_free(&fn);
      }
   }
   sge_free(&fields);

   DRETURN(!answer_list_has_error(answer_list));
}

/** @brief Write one cluster queue configuration
 *
 * @param os          the stream to write to
 * @param answer_list used to return error messages
 * @param cqueue      the queue
 * @return true on success; false with @p answer_list filled otherwise
 */
bool
ocs::QConfCQueueShowView::write_cqueue(std::ostream &os, lList **answer_list,
                                       const lListElem *cqueue) {
   DENTER(TOP_LAYER);

   const spool_flatfile_format format =
      (output_format_ == ProcedureParameter::OutputFormat::JSON) ? SP_FORM_JSON : SP_FORM_ASCII;
   const char *outname = spool_flatfile_write_object(answer_list, cqueue, false, CQ_fields,
                                                     &qconf_sfi, SP_DEST_STREAM, format,
                                                     nullptr, false, nullptr, &os, line_breaks_);
   sge_free(&outname);

   DRETURN(!answer_list_has_error(answer_list));
}

/** @brief Write what `qconf -sq` was asked for
 *
 * The model has already resolved every queue reference into the group of objects
 * it stands for, and reported a reference that matched nothing. So this walks the
 * groups in order and writes each one in its shape: queue instances as one group,
 * which JSON turns into a single array, or the cluster queue configurations,
 * blank-line separated.
 *
 * @param os          the stream to write to
 * @param answer_list used to return error messages
 * @param model       the snapshot, holding the resolved groups
 * @return true on success; false with @p answer_list filled otherwise
 */
bool
ocs::QConfCQueueShowView::show_queues(std::ostream &os, lList **answer_list,
                                      const QConfModelBase &model) {
   DENTER(TOP_LAYER);

   bool is_first = true;
   for (const QConfModelBase::CQueueOutput &group : model.get_cqueue_output()) {
      if (group.qinstances != nullptr) {
         if (!write_qinstance_list(os, answer_list, group.qinstances)) {
            DRETURN(false);
         }
         continue;
      }
      for_each_ep_lv(cqueue, group.cqueues) {
         /* a blank line between queues, and between the groups of two
          * references, which is what the client printed */
         if (is_first) {
            is_first = false;
         } else {
            os << "\n";
         }
         if (!write_cqueue(os, answer_list, cqueue)) {
            DRETURN(false);
         }
      }
   }

   DRETURN(true);
}
