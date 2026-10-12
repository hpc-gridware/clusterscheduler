/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief The writing half of the qconf write procedure, see the header
 */

#include <string>
#include <utility>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "spool/flatfile/sge_flatfile.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "ocs_QConfWriteQmaster.h"
#include "sge_c_gdi.h"

/** @brief Apply a set of objects, parsing each one first, see the header
 * @param packet the client request
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param obj which object type is written
 * @param parameter the items and the batch policy
 * @param answer_list receives one entry per item
 * @param result receives the two counts
 * @return true when the batch was applied as the policy demands
 */
bool
qconf_write_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                           const QConfObj obj, const ocs::QConfWriteParameter &parameter,
                           lList **answer_list, ocs::QConfWriteResult *result) {
   DENTER(TOP_LAYER);

   const qconf_object_desc &desc = qconf_desc(obj);

   /* The dispatch entry declares the target and sge_c_gdi_procedure() refuses a
    * request that is addressed elsewhere, so the target reaching us is the one
    * the entry named. What that check cannot see is whether the entry's target
    * and the object type it hands us are the same thing: they are written in two
    * places, the table of procedures and the traits of the handler. Compared
    * here, where both are in hand, because a mismatch would have the permission
    * checked against one object list and the write performed on another - the
    * one failure the whole command/target binding of CS-2822 exists to stop. */
   if (task->target != desc.target) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_QCONF_WRITEWRONGTARGET_SS,
                              ocs::gdi::to_string(task->target).c_str(),
                              ocs::gdi::to_string(desc.target).c_str());
      DRETURN(false);
   }

   gdi_object_t *object = get_gdi_object(desc.target);
   if (object == nullptr) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   /* A file holding a list rather than a single object - the resource quota sets
    * - needs a different parse and a different add/modify decision, and has its
    * own path in the client to this day. Refused rather than mis-parsed; it is
    * part of CS-2823. */
   if (desc.as_list) {
      answer_list_add_sprintf(answer_list, STATUS_ENOIMP, ANSWER_QUALITY_ERROR,
                              MSG_QCONF_WRITELISTUNSUPPORTED_S, object->object_name);
      DRETURN(false);
   }

   /* The text is in the form a read rendered it, which is what the caller's
    * -fmt asked for - so the same value decides how it is parsed back. */
   spool_flatfile_format format = SP_FORM_ASCII;
   switch (parameter.get_output_format()) {
      case ocs::ProcedureParameter::OutputFormat::XML:
         format = SP_FORM_XML;
         break;
      case ocs::ProcedureParameter::OutputFormat::JSON:
         format = SP_FORM_JSON;
         break;
      case ocs::ProcedureParameter::OutputFormat::PLAIN:
      default:
         format = SP_FORM_ASCII;
         break;
   }

   bool fields_owned = false;
   spooling_field *fields = desc.build_fields(format, &fields_owned);

   /* Phase 1: read every item. Nothing is written yet, not even in the default
    * mode - the two phases are what makes -strict possible at all, and running
    * them for every mode keeps one code path instead of two. */
   std::vector<std::pair<std::string, lListElem *>> parsed;
   for (const lListElem *item = lFirst(parameter.get_items()); item != nullptr; item = lNext(item)) {
      const char *label = lGetString(item, VA_variable);
      const char *text = lGetString(item, VA_value);

      if (text == nullptr) {
         /* unpackstr() maps an empty string to a null pointer, so this is also
          * what an item with no text at all arrives as */
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QCONF_WRITEEMPTYTEXT_S, label != nullptr ? label : "");
         result->parse_failed++;
         continue;
      }

      /* The label stands in for the file name the reader would otherwise name in
       * its messages. It is the caller's string and means nothing here, which is
       * exactly why it is the right one: the caller gets back "this item failed,
       * because that", and only the caller knows what it called the item. */
      lListElem *ep = qconf_parse_object(answer_list, desc.descr, fields, format,
                                         label, text, desc.validate, desc.sfi, desc.prepare, true);
      if (ep == nullptr) {
         // the reader, the transform or the validator wrote the reason
         result->parse_failed++;
         continue;
      }
      parsed.emplace_back(label != nullptr ? label : "", ep);
   }

   if (fields_owned) {
      spool_free_spooling_fields(fields);
   }

   /* -strict is a parse level gate, which is what it means in the client today:
    * an item that could not be read rejects the batch, one that could not be
    * applied does not. The caller names the path it was given in its own report,
    * so the message is its own; what it gets from here is the count. */
   if (parameter.is_strict() && result->parse_failed > 0) {
      for (auto &[label, ep]: parsed) {
         lFreeElem(&ep);
      }
      DRETURN(false);
   }

   // Phase 2: apply what was read, or say what applying would do
   const lList *master_list = *ocs::DataStore::get_master_list(object->list_type);
   bool all_ok = (result->parse_failed == 0);
   for (auto &[label, ep]: parsed) {
      const char *name = qconf_get_name(ep, object->key_nm);
      const bool exists = qconf_name_in_list(master_list, desc.descr, object->key_nm, name);

      if (parameter.is_dry()) {
         answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                 exists ? MSG_QCONF_WRITEWOULDMODIFY_S : MSG_QCONF_WRITEWOULDADD_S,
                                 name != nullptr ? name : "");
         result->applied++;
         lFreeElem(&ep);
         continue;
      }

      /* add == 0 modifies and fails when the object is gone, add == 1 adds and
       * fails when it is already there - sge_gdi_add_mod_generic() enforces both
       * against the list it is about to change. Deciding it from the master list
       * here is what the client used to decide from its bulk name GET, and it is
       * now decided where the list actually is. */
      const int ret = sge_gdi_add_mod_generic(packet, task, answer_list, ep, exists ? 0 : 1, object,
                                              packet->user, packet->host,
                                              ocs::gdi::Command::MOD_PROCEDURE, desc.sub_cmd,
                                              nullptr, monitor);
      if (ret == STATUS_OK) {
         result->applied++;
      } else {
         all_ok = false;
      }
      lFreeElem(&ep);
   }

   DRETURN(all_ok);
}
