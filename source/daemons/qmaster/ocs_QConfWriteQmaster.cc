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

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "spool/flatfile/sge_flatfile.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "ocs_QConfWriteQmaster.h"
#include "sge_c_gdi.h"
#include "sge_centry_qmaster.h"

/** @brief Whether a replacing write can delete objects of this target (CS-2823)
 *
 * There is no generic delete to drive. `sge_c_gdi_del()` is a switch over the
 * target and every type has its own function with its own preconditions - a
 * queue checks for jobs, a host for queues, a complex for references - so a
 * replace can only offer the targets qconf_delete_one() below actually knows.
 * Asked before anything is applied, so a type without a delete is refused
 * rather than half written.
 *
 * @param target the GDI target of the type being written
 * @return true when qconf_delete_one() can delete one of its objects
 */
static bool
qconf_can_delete(const ocs::gdi::Target target) {
   return target == ocs::gdi::Target::CE_LIST;
}

/** @brief Delete one object a replacing write found missing from the text
 *
 * The same function the GDI delete handler calls for this target, with an
 * element carrying nothing but the name - which is all a delete is given there
 * either, because "qconf -dce x" sends exactly that.
 *
 * The type's own refusals therefore still apply and still produce their own
 * messages: a complex that a queue or a rule set references is not deleted, and
 * a built-in is not deleted at all. The replace keeps the second one from being
 * asked for in the first place (qconf_ce_retain_builtin), the first one is a
 * real conflict and has to be reported.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param target the GDI target of the type being written
 * @param name the object to delete
 * @param answer_list receives the reason when the delete was refused
 * @return true when the object is gone
 */
static bool
qconf_delete_one(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::gdi::Target target,
                 const char *name, lList **answer_list) {
   DENTER(TOP_LAYER);

   lListElem *ep = nullptr;
   int ret = STATUS_EUNKNOWN;

   switch (target) {
      case ocs::gdi::Target::CE_LIST:
         ep = lCreateElem(CE_Type);
         lSetString(ep, CE_name, name);
         ret = sge_del_centry(packet, task, ep, answer_list, packet->user, packet->host);
         lFreeElem(&ep);
         break;
      default:
         // qconf_can_delete() is asked first, so this is unreachable
         answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         break;
   }

   DRETURN(ret == STATUS_OK);
}

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

   /* A replace needs a text that is the whole truth about the type, and only a
    * list text is that: for a single object "everything the text does not hold"
    * would be the rest of the type (CS-2823). */
   if (parameter.is_replace() && !parameter.is_list()) {
      answer_list_add(answer_list, MSG_QCONF_WRITEREPLACENOTLIST, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   /* And it needs a delete. There is no generic one: sge_c_gdi_del() switches on
    * the target and every type has its own, so the replace can only offer what
    * qconf_delete_one() below knows. Refused rather than silently applying the
    * additions and keeping everything else, which would be a replace in name. */
   /* A selection says "only these objects", a replace says "the text is all
    * there is". Together they would delete every object the selection left out,
    * which no caller can mean. No switch produces the pair; refused so that
    * none ever does by accident. */
   if (parameter.is_replace() && !parameter.get_select().empty()) {
      answer_list_add(answer_list, MSG_QCONF_WRITEREPLACEWITHSELECT, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (parameter.is_replace() && !qconf_can_delete(desc.target)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_QCONF_WRITENOREPLACE_S,
                              ocs::gdi::to_string(desc.target).c_str());
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

   /* What an item's text holds: one object, or a list of them for the resource
    * quota sets and the complex list. An item is therefore a label and a
    * *vector* of objects - one long for every other type, which is why phase 2
    * below needs no second shape (CS-2823). */
   struct ParsedItem {
      std::string label;                 ///< what the caller called this item
      std::vector<lListElem *> elements; ///< its objects, owned until phase 2 frees them
   };

   /* Phase 1: read every item. Nothing is written yet, not even in the default
    * mode - the two phases are what makes -strict possible at all, and running
    * them for every mode keeps one code path instead of two. */
   std::vector<ParsedItem> parsed;
   for (const lListElem *item = lFirst(parameter.get_items()); item != nullptr; item = lNext(item)) {
      const char *label = lGetString(item, VA_variable);
      const char *text = lGetString(item, VA_value);

      if (text == nullptr) {
         /* unpackstr() maps an empty string to a null pointer, so this is also
          * what an item with no text at all arrives as */
         /* The label is the file name for a file, and nothing for the
          * interactive round trip, which has no file. Without a label the
          * message must not start with the colon that introduces one. */
         if (label != nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_QCONF_WRITEEMPTYTEXT_S, label);
         } else {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    SFNMAX, MSG_QCONF_WRITEEMPTYTEXT);
         }
         result->parse_failed++;
         continue;
      }

      /* The label stands in for the file name the reader would otherwise name in
       * its messages. It is the caller's string and means nothing here, which is
       * exactly why it is the right one: the caller gets back "this item failed,
       * because that", and only the caller knows what it called the item. */
      ParsedItem entry{label != nullptr ? label : "", {}};

      if (parameter.is_list()) {
         /* The layout of a list is not the layout of one object: the complex
          * table has a header line and aligned columns (qconf_ce_list_sfi),
          * where a single complex entry is a block of "name value" lines. A row
          * with a list form is parsed in it; a type whose list is just its
          * objects one after another - the rule sets, in braces - keeps the
          * one instruction it has. */
         const spool_flatfile_instr *list_sfi =
            (desc.read.list_sfi != nullptr) ? desc.read.list_sfi : desc.sfi;
         lList *lp = qconf_parse_list(answer_list, desc.descr, fields, format,
                                      label, text, desc.validate, list_sfi, desc.prepare,
                                      desc.parse_values);
         if (lp == nullptr) {
            // the reader, the transform or the validator wrote the reason
            result->parse_failed++;
            continue;
         }
         /* The checks that need the whole list in hand, per type and per item:
          * a duplicate name for the rule sets, a duplicate name or shortcut
          * plus the fields nobody else refuses for the complex list. Per item
          * as it always was - a directory never compared across files and still
          * does not. A failure here fails the reading phase, which for a list
          * text is all-or-nothing, so the text is rejected whole. */
         if (desc.write.verify_list != nullptr &&
             desc.write.verify_list(lp, answer_list) != STATUS_OK) {
            lFreeList(&lp);
            result->parse_failed++;
            continue;
         }

         /* Only the named objects, when the caller named some: "qconf -Mrqs
          * <file> <names>" changes those and leaves every other rule set alone.
          * The filter used to run in the client over what it had parsed; the
          * client parses nothing any more, so it travels as a parameter and the
          * filter runs where the parse is. A name the text does not hold fails
          * the item, as it failed the command before.
          *
          * A round trip is **not** filtered, although it carries the same
          * names: there they are the scope the read was asked for, used after
          * the writing to report what the text left out. Filtering by them
          * would write nothing at all when the administrator renamed the
          * object in the editor - which is exactly how "-arqs" creates one. */
         if (!parameter.get_select().empty() && !parameter.is_round_trip()) {
            lList *wanted = nullptr;
            lString2List(parameter.get_select().c_str(), &wanted, ST_Type, ST_name, ", ");
            bool all_found = true;
            for_each_ep_lv(name_elem, wanted) {
               const char *name = lGetString(name_elem, ST_name);
               lListElem *found = (name != nullptr)
                                  ? lGetElemStrRW(lp, object->key_nm, name) : nullptr;
               if (found == nullptr) {
                  answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                          MSG_RQS_NOTFOUNDINFILE_SS,
                                          name != nullptr ? name : "",
                                          label != nullptr ? label : "");
                  all_found = false;
                  break;
               }
               lDechainElem(lp, found);
               entry.elements.push_back(found);
            }
            lFreeList(&wanted);
            lFreeList(&lp);          /* what was not named stays untouched */
            if (!all_found) {
               for (lListElem *ep: entry.elements) {
                  lListElem *tmp = ep;
                  lFreeElem(&tmp);
               }
               result->parse_failed++;
               continue;
            }
         } else {
            lListElem *ep;
            while ((ep = lFirstRW(lp)) != nullptr) {
               lDechainElem(lp, ep);
               entry.elements.push_back(ep);
            }
            lFreeList(&lp);
         }
      } else {
         lListElem *ep = qconf_parse_object(answer_list, desc.descr, fields, format,
                                            label, text, desc.validate, desc.sfi, desc.prepare,
                                            desc.parse_values);
         if (ep == nullptr) {
            // the reader, the transform or the validator wrote the reason
            result->parse_failed++;
            continue;
         }
         entry.elements.push_back(ep);
      }

      parsed.push_back(std::move(entry));
   }

   if (fields_owned) {
      spool_free_spooling_fields(fields);
   }

   /* -strict is a parse level gate, which is what it means in the client today:
    * an item that could not be read rejects the batch, one that could not be
    * applied does not. The caller names the path it was given in its own report,
    * so the message is its own; what it gets from here is the count.
    *
    * A type whose text holds a list is strict whether or not it was asked for.
    * That is the behaviour of the path this replaces - "whole-list semantics are
    * inherently strict", as the client's own comment put it: a directory of
    * resource quota files with one unreadable file applied nothing. */
   if (result->parse_failed > 0 && (parameter.is_strict() || parameter.is_list())) {
      for (auto &item: parsed) {
         for (lListElem *ep: item.elements) {
            lListElem *tmp = ep;
            lFreeElem(&tmp);
         }
      }
      DRETURN(false);
   }

   /* Phase 2: apply what was read, or say what applying would do.
    *
    * Two counters, because an item and an object are not the same thing for a
    * type whose text holds a list: `applied` counts items that went in
    * completely, which is what the caller's "N of M files" arithmetic needs,
    * and `elements` counts the objects, which is what a summary over resource
    * quota sets prints. For every other type each item is one object and the
    * two are equal. */
   const lList *master_list = *ocs::DataStore::get_master_list(object->list_type);
   bool all_ok = (result->parse_failed == 0);
   std::vector<std::string> parsed_names;   ///< what the texts held, for the report below
   for (auto &item: parsed) {
      size_t applied_here = 0;

      for (lListElem *ep: item.elements) {
         const char *name = qconf_get_name(ep, object->key_nm);
         if (name != nullptr) {
            parsed_names.emplace_back(name);
         }
         const bool exists = qconf_name_in_list(master_list, desc.descr, object->key_nm, name);

         if (parameter.is_dry()) {
            answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                    exists ? MSG_QCONF_WRITEWOULDMODIFY_S : MSG_QCONF_WRITEWOULDADD_S,
                                    name != nullptr ? name : "");
            applied_here++;
            result->elements++;
         } else {
            /* add == 0 modifies and fails when the object is gone, add == 1 adds
             * and fails when it is already there - sge_gdi_add_mod_generic()
             * enforces both against the list it is about to change. Deciding it
             * from the master list here is what the client used to decide from
             * its bulk name GET, and it is now decided where the list actually
             * is. */
            const int ret = sge_gdi_add_mod_generic(packet, task, answer_list, ep, exists ? 0 : 1,
                                                    object, packet->user, packet->host,
                                                    ocs::gdi::Command::MOD_PROCEDURE, desc.sub_cmd,
                                                    nullptr, monitor);
            if (ret == STATUS_OK) {
               applied_here++;
               result->elements++;
            } else {
               all_ok = false;
            }
         }

         lListElem *tmp = ep;
         lFreeElem(&tmp);
      }

      if (applied_here == item.elements.size()) {
         result->applied++;
      } else {
         all_ok = false;
      }
   }

   /* Phase 3: what the text did not hold is deleted, for a replacing write.
    *
    * The mirror image of the report below, and the reason the two are exclusive:
    * both compare the stored objects against the names the texts held, and then
    * one deletes what is left over while the other says it is still there. The
    * complex list is a replace because that is how a complex is removed from a
    * file-managed configuration - there is no "-dc", only a row taken out of
    * the table - and a modify of the rule sets is not, because a shortened text
    * there used to wreck the quota configuration (CS-2823).
    *
    * The names are collected before anything is deleted: a delete changes the
    * very master list this walks.
    *
    * A parse failure stops it. Nothing was written then, so every stored object
    * would count as "not in the text" and the replace would empty the type -
    * which is the one way this could do real damage.
    *
    * Deletes are not counted in result->elements: that count is what a caller
    * prints as "N objects written", and an object that is gone was not written. */
   if (parameter.is_replace() && result->parse_failed == 0) {
      lList *to_delete = nullptr;
      dstring kept = DSTRING_INIT;
      int num_kept = 0;

      for (const lListElem *ep = lFirst(*ocs::DataStore::get_master_list(object->list_type));
           ep != nullptr; ep = lNext(ep)) {
         const char *name = qconf_get_name(ep, object->key_nm);
         if (name == nullptr) {
            continue;
         }
         bool in_text = false;
         for (const auto &parsed_name: parsed_names) {
            if (parsed_name == name) {
               in_text = true;
               break;
            }
         }
         if (in_text) {
            continue;
         }

         if (desc.write.retain_on_replace != nullptr && desc.write.retain_on_replace(ep)) {
            if (num_kept > 0) {
               sge_dstring_append(&kept, ", ");
            }
            sge_dstring_append(&kept, name);
            num_kept++;
            continue;
         }
         lAddElemStr(&to_delete, ST_name, name, ST_Type);
      }

      /* Say what was kept. Staying silent would hide an edit that was not
       * carried out: whoever removed the row had an intent, and it is not being
       * fulfilled (CS-2523). */
      if (num_kept > 0 && desc.write.retain_msg != nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                 desc.write.retain_msg, sge_dstring_get_string(&kept));
      }
      sge_dstring_free(&kept);

      for_each_ep_lv(name_elem, to_delete) {
         const char *name = lGetString(name_elem, ST_name);
         if (parameter.is_dry()) {
            answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                    MSG_QCONF_WRITEWOULDDELETE_S, name);
         } else if (!qconf_delete_one(packet, task, desc.target, name, answer_list)) {
            all_ok = false;
         }
      }
      lFreeList(&to_delete);
   }

   /* CS-2823: what the round trip's text left out is still there, and the
    * administrator who deleted it in the editor has to be told - silently
    * ignoring the deletion would leave them believing it happened, and applying
    * it would make an editor a delete command. The scope is what the read was
    * asked for: the names the caller selected, or every object of the type when
    * it asked for all of them. Only a list type can lose an object this way; a
    * single object round trip has nothing to leave out. After a text that did
    * not parse there is nothing to say either: nothing was written, so every
    * object would be "still there" and the list would bury the parse error.
    * A replacing write is excluded because phase 3 already acted on the same
    * comparison: there is nothing left over to report, it was deleted. */
   if (parameter.is_list() && !parameter.is_replace() && parameter.is_round_trip() &&
       !parameter.is_dry() && result->parse_failed == 0) {
      lList *in_scope = nullptr;
      if (parameter.get_select().empty()) {
         for (const lListElem *ep = lFirst(*ocs::DataStore::get_master_list(object->list_type));
              ep != nullptr; ep = lNext(ep)) {
            lAddElemStr(&in_scope, ST_name, qconf_get_name(ep, object->key_nm), ST_Type);
         }
      } else {
         lString2List(parameter.get_select().c_str(), &in_scope, ST_Type, ST_name, ", ");
      }

      for_each_ep_lv(name_elem, in_scope) {
         const char *name = lGetString(name_elem, ST_name);
         if (name == nullptr) {
            continue;
         }
         bool in_text = false;
         for (const auto &item: parsed_names) {
            if (item == name) {
               in_text = true;
               break;
            }
         }
         if (!in_text &&
             qconf_name_in_list(*ocs::DataStore::get_master_list(object->list_type),
                                desc.descr, object->key_nm, name)) {
            answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                    MSG_QCONF_WRITENOTREMOVED_S, name);
         }
      }
      lFreeList(&in_scope);
   }

   DRETURN(all_ok);
}
