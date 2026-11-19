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
#include <type_traits>
#include <utility>
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "spool/flatfile/sge_flatfile.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "msg_common.h"
#include "procedure/qconf/msg_qconf.h"
#include "procedure/qconf/ocs_QConfObject.h"
#include "ocs_QConfWriteQmaster.h"
#include "configuration_qmaster.h"
#include "sched_conf_qmaster.h"
#include "sge_c_gdi.h"
#include "sge_centry_qmaster.h"
#include "sge_sharetree_qmaster.h"

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
                 const char *name, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::CE> ep = nullptr;
   int ret = STATUS_EUNKNOWN;

   switch (target) {
      case ocs::gdi::Target::CE_LIST:
         ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
         ocs::tom::set<CE_name>(ep, name);
         ret = sge_del_centry(packet, task, *ep, answer_list, packet->user, packet->host);
         ep.reset();
         break;
      default:
         // qconf_can_delete() is asked first, so this is unreachable
         answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         break;
   }

   DRETURN(ret == STATUS_OK);
}

/** @brief A master list of the object type of a table entry
 *
 * The data store hands out its lists untyped; the list of @p list_type holds
 * the objects of the entry's type, see qconf_master_list_type().
 *
 * @tparam E the entry type of the object type
 * @param list_type the master list of the entry
 * @return the list, an empty handle when there is none
 */
template <typename E>
static ocs::tom::ConstList<typename E::type>
qconf_master_list_of(const sge_object_type list_type) {
   // the entry names the master list holding objects of its type
   return ocs::tom::ConstList<typename E::type>::from_typed(*ocs::DataStore::get_master_list(list_type));
}

/** @brief Whether storing this object would change nothing (CS-2823)
 *
 * A replacing write hands the qmaster the whole table, so most of what it holds
 * is already stored. The client used to drop those elements before it sent
 * anything - object_has_differences() in centry_list_add_del_mod_via_gdi() freed
 * an element equal to the stored one and put only the rest into the modify list -
 * and the generic path has no such step, so every entry of the text was modified:
 * a "qconf -Mc" over an unchanged complex reported 52 modifications, spooled 52
 * objects and sent 52 sgeE_CENTRY_MOD events where the predecessor did nothing at
 * all. That the audit trail then claims changes nobody made is the part which
 * makes this more than a cost.
 *
 * Only the complex is filtered, because only the complex lost a filter. The other
 * replacing writes never had one: the "file unchanged" refusal of "-mq" and
 * "-mhgrp" belongs to the editor flow, not to a write from a file, and it is
 * still in place. Widening this to every type would also stop a write the caller
 * may want for its effect - a configuration written unchanged still raises
 * sgeE_CONFIG_MOD and so still makes the execution daemons reread it - and that
 * is a decision of its own, not a regression to undo.
 *
 * @tparam E the entry type of the object type being written
 * @param master_list the list the type is stored in
 * @param name the object's name, nullptr for a singleton
 * @param ep the object as the text holds it
 * @return true when an object of that name is stored and equal to @p ep
 */
template <typename E>
static bool
qconf_write_is_unchanged([[maybe_unused]] const ocs::tom::ConstList<typename E::type> master_list,
                         [[maybe_unused]] const char *name,
                         [[maybe_unused]] const ocs::tom::ConstElement<typename E::type> ep) {
   DENTER(TOP_LAYER);
   if constexpr (std::is_same_v<typename E::type, ocs::tom::CE>) {
      if (name == nullptr) {
         DRETURN(false);
      }

      const auto stored = centry_list_locate(master_list, name);
      if (!stored) {
         DRETURN(false);
      }

      /* Argument order as the client had it: the element from the text first, the
       * stored one second. The answer list stays out of it, because a difference is
       * not a problem to report - it is the normal case. object_has_differences()
       * compares any two elements of one type and has no typed form. */
      DRETURN(!object_has_differences(ep, nullptr, stored));
   } else {
      DRETURN(false);
   }
}

/** @brief Hand one parsed object to the function that applies its type (CS-2823)
 *
 * Most types are applied by ocs_gdi_add_mod() with their typed hooks, and for
 * them this is a pass-through. The three types that kept their own editor to
 * the end are exactly the ones without a generic add and modify:
 * `sge_c_gdi_mod()` special-cases them above the generic branch, they have no
 * hooks, and their key and list type are empty in their `gdi_object_t` row.
 * Any other type without hooks (event clients, categories) is refused as the
 * generic path refuses it. So the special cases are repeated
 * here rather than invented: one call each, the same function the GDI MOD of
 * that target performs.
 *
 * It is a choice on the object type here and not a hook on the descriptor for
 * the reason qconf_delete_one() is: these are qmaster functions and the
 * descriptor lives in a library, which cannot link against them. The object
 * type decides what the target did before: each of the three targets belongs to
 * exactly one entry of the table.
 *
 * The share tree needs no separate add. sge_add_sharetree() is a one-line
 * forwarder to sge_mod_sharetree(), which creates the master list when it is
 * missing, so for a tree "add" and "modify" are the same call - which is why
 * @p exists only reaches the generic path.
 *
 * The generic path's `on_success` hook may need to say something about the
 * object it just stored that only the caller can act on - the complex entry
 * says "this consumable has to be debited anew" - and it says it by noting it in
 * @p post. Dropping that note is how CS-2823 broke the re-debit after a
 * complex change: `lAddElemStr()` returns on a null pointer after a `DPRINTF`,
 * so the note was lost without a trace. The notes are therefore threaded through
 * to qconf_write_from_procedure(), which runs the post-processing once the whole
 * batch is applied, exactly as `sge_c_gdi_mod()` does for a GDI MOD.
 *
 * @tparam E the entry type of the object type being written
 * @param packet the client request
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param desc the object type's descriptor
 * @param object the type's gdi_object_t row, for the generic path
 * @param ep the parsed object
 * @param exists whether an object of that name is already stored
 * @param answer_list receives the reason on failure
 * @param post collects what the type's `on_success` hook needs post-processed
 * @return STATUS_OK when the object was applied
 */
template <typename E>
static int
qconf_apply_one(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                const qconf_object_desc &desc, gdi_object_t *object, const ocs::tom::Element<typename E::type> ep,
                const bool exists, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs_gdi_post_processing *post) {
   DENTER(TOP_LAYER);
   using T = typename E::type;

   if constexpr (std::is_same_v<T, ocs::tom::CONF>) {
      /* Upserts by itself, which is why "-aconf" and "-mconf" are one
       * operation in the client (CS-2311) and why the add/modify decision
       * above does not reach it. */
      DRETURN(sge_mod_configuration(ep, answer_list, packet->user, packet->host, packet->gdi_session));
   } else if constexpr (std::is_same_v<T, ocs::tom::SC>) {
      DRETURN(sge_mod_sched_configuration(packet, task, ep, answer_list, packet->user, packet->host));
   } else if constexpr (std::is_same_v<T, ocs::tom::STN>) {
      DRETURN(sge_mod_sharetree(packet, task, ep, ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE),
                                answer_list, packet->user, packet->host));
   } else if constexpr (OcsGdiGenericType<T>) {
      /* add == 0 modifies and fails when the object is gone, add == 1 adds
       * and fails when it is already there - ocs_gdi_add_mod()
       * enforces both against the list it is about to change. */
      DRETURN(ocs_gdi_add_mod<T>(packet, task, answer_list, ep, exists ? 0 : 1,
                                 object, packet->user, packet->host,
                                 ocs::gdi::Command::MOD_PROCEDURE, desc.sub_cmd,
                                 post, monitor));
   } else {
      /* a type without a generic add and modify, refused as the generic path
       * refuses a table entry without one */
      DRETURN(ocs_gdi_add_mod_unsupported(answer_list, object));
   }
}

/** @brief What a batch write carries from one phase to the next, whatever the object type
 *
 * The parts of qconf_write_entry() that do not need the object type work on
 * this, so they are compiled once instead of once per object type.
 */
struct OcsQConfWriteState {
   bool all_ok = true;                          ///< whether every item and object went in
   std::vector<std::string> parsed_names;       ///< what the texts held, for the leftover handling
   size_t stored_objects = 0;                   ///< objects written or deleted, for the summary
   size_t left_objects = 0;                     ///< objects the store already held as asked
   ocs::tom::ListPtr<ocs::tom::ST> to_delete;   ///< leftovers a replace deletes, created with the first
   dstring kept = DSTRING_INIT;                 ///< leftovers a replace keeps, for its message
   int num_kept = 0;                            ///< how many names @ref kept holds
};

/** @brief Refuse a write request that cannot be carried out, before anything is read
 *
 * Every check here looks at the request and the object type's row only, so
 * nothing has been parsed or written when one of them refuses.
 *
 * @param task the GDI task being answered
 * @param desc the object type's row
 * @param list_type the master list of the object type, from its entry
 * @param parameter the items and the batch policy
 * @param answer_list receives the reason of a refusal
 * @return the type's gdi_object_t row, nullptr when the request was refused
 */
static gdi_object_t *
ocs_qconf_write_check_request(const ocs::gdi::Task *task, const qconf_object_desc &desc,
                              const sge_object_type list_type, const ocs::QConfWriteParameter &parameter,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

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
      DRETURN(nullptr);
   }

   gdi_object_t *object = get_gdi_object(desc.target);
   if (object == nullptr) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   /* A name on the request gives what the text holds its identity, for the one
    * type whose text does not carry it (see QConfWriteParameter::set_name). A
    * text holding a list would have every object in it renamed to that one name,
    * so the pair is a request error rather than something to interpret. */
   if (!parameter.get_name().empty() && parameter.is_list()) {
      answer_list_add(answer_list, MSG_QCONF_WRITENAMEWITHLIST, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   /* The key field and the master list come from the entry, not from the
    * gdi_object_t row. That row carries both for the types whose modify is a
    * hook on it, and empties - key 0, SGE_TYPE_NONE - for the three whose
    * modify is a special case in sge_c_gdi_mod() instead: CONF, SC and STN. So
    * it knows these two facts for every type that never needed them. Taken from
    * the entry, a key of 0 cannot silently turn the existence check, the
    * name collection and the leftover report into no-ops (CS-2823). */
   if (list_type == SGE_TYPE_NONE) {
      /* A guard, not a reachable path: the one type without a master list is
       * the event clients, and they are read only. It exists so that a new
       * singleton reaches this as an error rather than writing into list zero.
       * Reported like the dispatch's other internal errors, without a
       * catalogue entry. The function named is the one the check lived in
       * before it moved here, so the message is unchanged. */
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "no master list for the object type in " SFQ, "qconf_write_entry");
      DRETURN(nullptr);
   }

   /* A replace needs a text that is the whole truth about the type, and only a
    * list text is that: for a single object "everything the text does not hold"
    * would be the rest of the type (CS-2823). */
   if (parameter.is_replace() && !parameter.is_list()) {
      answer_list_add(answer_list, MSG_QCONF_WRITEREPLACENOTLIST, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   /* And it needs a delete. There is no generic one: sge_c_gdi_del() switches on
    * the target and every type has its own, so the replace can only offer what
    * qconf_delete_one() knows. Refused rather than silently applying the
    * additions and keeping everything else, which would be a replace in name. */
   /* A selection says "only these objects", a replace says "the text is all
    * there is". Together they would delete every object the selection left out,
    * which no caller can mean. No switch produces the pair; refused so that
    * none ever does by accident. */
   if (parameter.is_replace() && !parameter.get_select().empty()) {
      answer_list_add(answer_list, MSG_QCONF_WRITEREPLACEWITHSELECT, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   if (parameter.is_replace() && !qconf_can_delete(desc.target)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_QCONF_WRITENOREPLACE_S,
                              ocs::gdi::to_string(desc.target).c_str());
      DRETURN(nullptr);
   }

   DRETURN(object);
}

/** @brief The format the texts of a write are parsed in
 *
 * The text is in the form a read rendered it, which is what the caller's
 * -fmt asked for - so the same value decides how it is parsed back.
 *
 * @param parameter the items and the batch policy
 * @return the flatfile format matching the request's output format
 */
static spool_flatfile_format
ocs_qconf_write_format(const ocs::QConfWriteParameter &parameter) {
   DENTER(TOP_LAYER);
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
   DRETURN(format);
}

/** @brief Report an item that arrived without a text
 *
 * The label is the file name for a file, and nothing for the interactive round
 * trip, which has no file. Without a label the message must not start with the
 * colon that introduces one.
 *
 * @param label what the caller called the item, nullptr when it has no name
 * @param answer_list receives the message
 * @return none
 */
static void
ocs_qconf_write_empty_text(const char *label, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   if (label != nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_QCONF_WRITEEMPTYTEXT_S, label);
   } else {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              SFNMAX, MSG_QCONF_WRITEEMPTYTEXT);
   }
   DRETURN_VOID;
}

/** @brief Report a selected name that the text of an item does not hold
 *
 * @param name the selected name, nullptr is reported as an empty name
 * @param label what the caller called the item, nullptr is reported as an empty label
 * @param answer_list receives the message
 * @return none
 */
static void
ocs_qconf_write_not_in_file(const char *name, const char *label, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                           MSG_RQS_NOTFOUNDINFILE_SS,
                           name != nullptr ? name : "",
                           label != nullptr ? label : "");
   DRETURN_VOID;
}

/** @brief Account for one parsed object before it is applied
 *
 * Notes the object's name for the leftover handling, and settles every object
 * that is not applied at all: the dry run says what applying would do, and an
 * object already stored as the text asks for is left alone, see
 * qconf_write_is_unchanged(). Both still count as applied - the item was
 * understood and the store holds, or would hold, what was asked for.
 *
 * @param desc the object type's row
 * @param parameter the items and the batch policy
 * @param name the object's name, nullptr for a type not addressed by name
 * @param exists whether an object of that name is already stored
 * @param unchanged whether the stored object equals the parsed one
 * @param state the batch state, receives the name and the counts
 * @param answer_list receives what the dry run would do
 * @param result receives the object count
 * @return true when the object still has to be applied
 */
static bool
ocs_qconf_write_settle(const qconf_object_desc &desc, const ocs::QConfWriteParameter &parameter,
                       const char *name, const bool exists, const bool unchanged, OcsQConfWriteState *state,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::QConfWriteResult *result) {
   DENTER(TOP_LAYER);

   if (name != nullptr) {
      state->parsed_names.emplace_back(name);
   }

   if (parameter.is_dry()) {
      answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                              unchanged ? MSG_QCONF_WRITEWOULDLEAVE_S
                                        : (exists ? MSG_QCONF_WRITEWOULDMODIFY_S
                                                  : MSG_QCONF_WRITEWOULDADD_S),
                              qconf_object_label(desc, name));
      result->elements++;
      DRETURN(false);
   }
   if (unchanged) {
      result->elements++;
      state->left_objects++;
      DRETURN(false);
   }
   DRETURN(true);
}

/** @brief Account for the outcome of applying one object
 *
 * @param ret what the type's apply function returned
 * @param state the batch state, receives the store count or the failure
 * @param result receives the object count
 * @return true when the object was applied
 */
static bool
ocs_qconf_write_tally(const int ret, OcsQConfWriteState *state, ocs::QConfWriteResult *result) {
   DENTER(TOP_LAYER);
   if (ret == STATUS_OK) {
      result->elements++;
      state->stored_objects++;
      DRETURN(true);
   }
   state->all_ok = false;
   DRETURN(false);
}

/** @brief Run what the whole batch needs done once it is stored
 *
 * The step sge_c_gdi_mod() performs after its own element loop (CS-2823). It
 * runs once and not per object because a replacing write modifies the whole
 * table: a complex file may change several entries, and the re-debit walks
 * every queue and host once, not once per entry.
 *
 * Only an entry whose `consumable` flag or default request actually changed
 * reaches `post.redebit_centries` - centry_success() decides that - so an
 * untouched table costs nothing, and a failed object contributes nothing
 * because its `on_success` hook never ran. A failure elsewhere in the batch
 * does not cancel it either: what was stored is stored, and the bookings have
 * to match it. Nothing collects in a dry run, which never applies anything.
 *
 * It is spelled out here and not a hook on the descriptor for the reason
 * qconf_apply_one() is a choice on the type: these are qmaster functions and
 * the descriptor lives in a library that cannot link against them.
 *
 * @param packet the client request
 * @param post what the applied objects need post-processed, emptied here
 * @return none
 */
static void
ocs_qconf_write_post_process(ocs::gdi::Packet *packet, ocs_gdi_post_processing *post) {
   DENTER(TOP_LAYER);
   if (!post->redebit_centries->empty()) {
      /* Rebuilds the consumable debitation of every queue and host against
       * the new definition, bumps the queue version, and dumps the
       * queue_consumable/host_consumable reporting records - all three of
       * which were silently skipped while the list was dropped. */
      centry_redebit_consumables(post->redebit_centries.get(), packet->gdi_session);
   }
   post->redebit_centries.reset();
   DRETURN_VOID;
}

/** @brief Whether one of the texts of the batch held a name
 *
 * @param state the batch state holding the parsed names
 * @param name the name to look for
 * @return true when a text held an object of that name
 */
static bool
ocs_qconf_write_is_parsed(const OcsQConfWriteState &state, const char *name) {
   DENTER(TOP_LAYER);
   for (const auto &parsed_name: state.parsed_names) {
      if (parsed_name == name) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

/** @brief Note a stored object that a replacing write found missing from the texts
 *
 * @param state the batch state, receives the name for the delete or the kept list
 * @param name the object's name
 * @param retain whether the type's `retain_on_replace` hook keeps the object
 * @return none
 */
static void
ocs_qconf_write_note_leftover(OcsQConfWriteState *state, const char *name, const bool retain) {
   DENTER(TOP_LAYER);
   if (retain) {
      if (state->num_kept > 0) {
         sge_dstring_append(&state->kept, ", ");
      }
      sge_dstring_append(&state->kept, name);
      state->num_kept++;
      DRETURN_VOID;
   }
   if (!state->to_delete) {
      // created with the first name, as the untyped emplace_back() created it
      state->to_delete = ocs::tom::ListPtr<ocs::tom::ST>::create("");
   }
   state->to_delete->emplace_back<ST_name>(name);
   DRETURN_VOID;
}

/** @brief Report the kept leftovers of a replacing write and delete the others
 *
 * Says what was kept first: staying silent would hide an edit that was not
 * carried out - whoever removed the row had an intent, and it is not being
 * fulfilled (CS-2523). Then deletes what ocs_qconf_write_note_leftover()
 * collected, or says in a dry run that it would.
 *
 * Deletes are not counted in result->elements: that count is what a caller
 * prints as "N objects written", and an object that is gone was not written.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param desc the object type's row
 * @param parameter the items and the batch policy
 * @param state the batch state holding the leftovers; receives the store count or the failure
 * @param answer_list receives the messages
 * @return none
 */
static void
ocs_qconf_write_delete_leftovers(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const qconf_object_desc &desc,
                                 const ocs::QConfWriteParameter &parameter, OcsQConfWriteState *state,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   if (state->num_kept > 0 && desc.write.retain_msg != nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                              desc.write.retain_msg, sge_dstring_get_string(&state->kept));
   }
   sge_dstring_free(&state->kept);

   for (const auto name_elem : *state->to_delete) {
      const char *name = ocs::tom::get<ST_name>(name_elem);
      if (parameter.is_dry()) {
         answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                 MSG_QCONF_WRITEWOULDDELETE_S, name);
      } else if (qconf_delete_one(packet, task, desc.target, name, answer_list)) {
         state->stored_objects++;
      } else {
         state->all_ok = false;
      }
   }
   state->to_delete.reset();

   DRETURN_VOID;
}

/** @brief The names of every stored object of a type
 *
 * A nameless object adds nothing, as the untyped emplace_back() did not; the
 * list comes with the first name.
 *
 * @tparam E the entry type of the object type
 * @param list_type the master list of the object type
 * @return the names, an empty handle when no stored object has one
 */
template <typename E>
static ocs::tom::ListPtr<ocs::tom::ST>
ocs_qconf_stored_names(const sge_object_type list_type) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::ST> names;
   for (const auto ep : qconf_master_list_of<E>(list_type)) {
      const char *key = ocs_qconf_key_of<E>(ep);
      if (key != nullptr) {
         if (!names) {
            names = ocs::tom::ListPtr<ocs::tom::ST>::create("");
         }
         names->emplace_back<ST_name>(key);
      }
   }
   DRETURN(names);
}

/** @brief Whether an object of a name is stored
 *
 * @tparam E the entry type of the object type
 * @param list_type the master list of the object type
 * @param name the name to look for
 * @return true when the master list holds an object of that name
 */
template <typename E>
static bool
ocs_qconf_name_stored(const sge_object_type list_type, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(ocs_qconf_name_in_list<E>(qconf_master_list_of<E>(list_type), name));
}

/** @brief Report what a round trip's text left out and is still stored (CS-2823)
 *
 * What the round trip's text left out is still there, and the administrator
 * who deleted it in the editor has to be told - silently ignoring the deletion
 * would leave them believing it happened, and applying it would make an editor
 * a delete command. The scope is what the read was asked for: the names the
 * caller selected, or every object of the type when it asked for all of them.
 * Only a list type can lose an object this way; a single object round trip has
 * nothing to leave out. After a text that did not parse there is nothing to
 * say either: nothing was written, so every object would be "still there" and
 * the list would bury the parse error. A replacing write is excluded because
 * phase 3 already acted on the same comparison: there is nothing left over to
 * report, it was deleted.
 *
 * The two typed questions - every stored name, and whether a name is stored -
 * are asked through the functions the caller passes for its object type.
 *
 * @param parameter the items and the batch policy
 * @param result the counts so far, for the parse failures
 * @param state the batch state holding the parsed names
 * @param list_type the master list of the object type
 * @param stored_names gives the names of every stored object of the type
 * @param name_stored tells whether an object of a name is stored
 * @param answer_list receives one message per object left out
 * @return none
 */
static void
ocs_qconf_write_report_leftovers(const ocs::QConfWriteParameter &parameter, const ocs::QConfWriteResult &result,
                                 const OcsQConfWriteState &state, const sge_object_type list_type,
                                 ocs::tom::ListPtr<ocs::tom::ST> (*stored_names)(sge_object_type),
                                 bool (*name_stored)(sge_object_type, const char *),
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   if (!parameter.is_list() || parameter.is_replace() || !parameter.is_round_trip() ||
       parameter.is_dry() || result.parse_failed != 0) {
      DRETURN_VOID;
   }

   ocs::tom::ListPtr<ocs::tom::ST> in_scope;
   if (parameter.get_select().empty()) {
      in_scope = stored_names(list_type);
   } else {
      in_scope = ocs::tom::ListPtr<ocs::tom::ST>::from_string<ST_name>(parameter.get_select().c_str(), ", ");
   }

   for (const auto name_elem : *in_scope) {
      const char *name = ocs::tom::get<ST_name>(name_elem);
      if (name == nullptr) {
         continue;
      }
      if (!ocs_qconf_write_is_parsed(state, name) && name_stored(list_type, name)) {
         answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO,
                                 MSG_QCONF_WRITENOTREMOVED_S, name);
      }
   }
   in_scope.reset();

   DRETURN_VOID;
}

/** @brief Close a batch write: say so when it changed nothing (CS-2823)
 *
 * Every object of the text was already stored as the text describes it, so
 * nothing was written and nothing was deleted either - and without a word here
 * the command would answer an empty output with exit code 0, which is exactly
 * what a silently broken write looks like. The predecessor had the same rule
 * for the one type that filtered: when its answer list stayed empty it added
 * "Complex attribute configuration has not been changed", and the testsuite
 * reads that message as a success.
 *
 * It is a summary and not a line per object, because the quiet case is the
 * common one: re-applying an unchanged complex would otherwise print more
 * lines than a real modification does. A write that changed some of its
 * objects already reports each of them and gets no summary - there is nothing
 * left unsaid then. The dry run is excluded because it reports per object what
 * it would do, including what it would leave alone.
 *
 * @param parameter the items and the batch policy
 * @param state the batch state
 * @param answer_list receives the summary
 * @return true when the batch was applied as the policy demands
 */
static bool
ocs_qconf_write_summarize(const ocs::QConfWriteParameter &parameter, const OcsQConfWriteState &state,
                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   if (!parameter.is_dry() && state.all_ok && state.stored_objects == 0 && state.left_objects > 0) {
      answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO, SFNMAX,
                              MSG_QCONF_WRITENOCHANGE);
   }
   DRETURN(state.all_ok);
}

/** @brief Apply a set of objects of one type, parsing each one first
 *
 * The body of qconf_write_from_procedure(), compiled per object type: the
 * parsed objects, the master list and the hooks are typed on the entry's type.
 * Only what needs the type stays here - parsing, the selection, the existence
 * and change checks, the apply call and the walks over the master list; the
 * checks, messages and counts are the ocs_qconf_write_*() helpers above.
 *
 * @tparam E the entry type of the object type being written
 * @param packet the client request
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param type_entry the entry of the object type in #ocs_qconf_objects
 * @param parameter the items and the batch policy
 * @param answer_list receives one entry per item
 * @param result receives the two counts
 * @return true when the batch was applied as the policy demands
 */
template <typename E>
static bool
qconf_write_entry(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor, const E &type_entry,
                  const ocs::QConfWriteParameter &parameter, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                  ocs::QConfWriteResult *result) {
   DENTER(TOP_LAYER);
   using T = typename E::type;

   const qconf_object_desc &desc = type_entry.desc;
   const sge_object_type list_type = type_entry.master_list;

   gdi_object_t *object = ocs_qconf_write_check_request(task, desc, list_type, parameter, answer_list);
   if (object == nullptr) {
      DRETURN(false);
   }

   const spool_flatfile_format format = ocs_qconf_write_format(parameter);

   bool fields_owned = false;
   /* the write field list, built without the diagnostic attributes: this is the list the
    * incoming text is parsed with, and an attribute that only a display adds would make a
    * valid text unparseable */
   spooling_field *fields = desc.build_fields(format, false, &fields_owned);

   /* What an item's text holds: one object, or a list of them for the resource
    * quota sets and the complex list. An item is therefore a label and a
    * *vector* of objects - one long for every other type, which is why phase 2
    * below needs no second shape (CS-2823). */
   struct ParsedItem {
      std::string label;                              ///< what the caller called this item
      std::vector<ocs::tom::ElementPtr<T>> elements;  ///< its objects, owned until phase 2 frees them
   };

   /* Phase 1: read every item. Nothing is written yet, not even in the default
    * mode - the two phases are what makes -strict possible at all, and running
    * them for every mode keeps one code path instead of two. */
   std::vector<ParsedItem> parsed;
   for (const auto item : parameter.get_items()) {
      const char *label = ocs::tom::get<VA_variable>(item);
      const char *text = ocs::tom::get<VA_value>(item);

      if (text == nullptr) {
         /* unpackstr() maps an empty string to a null pointer, so this is also
          * what an item with no text at all arrives as */
         ocs_qconf_write_empty_text(label, answer_list);
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
         auto lp = ocs_qconf_parse_list<T>(answer_list, fields, format, label, text, type_entry.hooks.validate,
                                           list_sfi, type_entry.hooks.prepare, desc.parse_values);
         if (!lp) {
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
         if (type_entry.hooks.verify_list != nullptr &&
             type_entry.hooks.verify_list(*lp, answer_list) != STATUS_OK) {
            lp.reset();
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
            const auto wanted =
                    ocs::tom::ListPtr<ocs::tom::ST>::from_string<ST_name>(parameter.get_select().c_str(), ", ");
            bool all_found = true;
            for (const auto name_elem : *wanted) {
               const char *name = ocs::tom::get<ST_name>(name_elem);
               ocs::tom::Element<T> found;
               if constexpr (E::has_name_key) {
                  if (name != nullptr) {
                     found = lp->template find_first<E::key>(ocs_qconf_key_value<E::key>(name));
                  }
               }
               if (found == nullptr) {
                  ocs_qconf_write_not_in_file(name, label, answer_list);
                  all_found = false;
                  break;
               }
               entry.elements.push_back(lp->extract(found));
            }
            lp.reset();          /* what was not named stays untouched */
            if (!all_found) {
               // the objects taken so far are freed with the item
               result->parse_failed++;
               continue;
            }
         } else {
            for (auto ep = lp->front(); ep != nullptr; ep = lp->front()) {
               entry.elements.push_back(lp->extract(ep));
            }
            lp.reset();
         }
      } else {
         auto ep = ocs_qconf_parse_object<T>(answer_list, fields, format, label, text, type_entry.hooks.validate,
                                             desc.sfi, type_entry.hooks.prepare, desc.parse_values);
         if (ep == nullptr) {
            // the reader, the transform or the validator wrote the reason
            result->parse_failed++;
            continue;
         }
         entry.elements.push_back(std::move(ep));
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
      parsed.clear();   // frees every object read
      DRETURN(false);
   }

   /* Phase 2: apply what was read, or say what applying would do.
    *
    * **This phase is not atomic, and that is a decision** (CS-2824). Each object
    * goes through the same per-type modifier a single write uses, and those spool
    * as they go, so there is nothing to undo with: rolling back what already went
    * in would need a transaction the data store does not offer. A failure here
    * therefore sets all_ok false and the loop continues, and what the caller is
    * given instead is the typed counts below plus one answer list entry per
    * object - enough to see which ones went in and which did not.
    *
    * Two consequences worth knowing before trusting -strict. First, it is a
    * *parse* level gate (phase 1 above) and this phase never increments
    * parse_failed, so it cannot reject a batch over a failure here. Second, and
    * this is what makes that matter rather than being a technicality, almost every
    * cross object reference is checked *here* and not in phase 1: the validate
    * hooks are the client flavour of the library validators, called with
    * in_master false, startup 0 and no master lists, because the qconf client had
    * none to resolve against. So a cluster queue naming a host group that does not
    * exist, a parallel environment naming an unknown userset or a rule set naming
    * an unknown complex all parse cleanly and are refused here, after the earlier
    * objects of the batch have been written.
    *
    * Running phase 1 with the master flavour would close that gap - the procedure
    * runs in qmaster, where the master lists are right here - and would make
    * -strict mean what its name promises. It would also change what eight types
    * accept and when, so it is a ticket of its own and deliberately not a rider on
    * the migration.
    *
    * Two counters, because an item and an object are not the same thing for a
    * type whose text holds a list: `applied` counts items that went in
    * completely, which is what the caller's "N of M files" arithmetic needs,
    * and `elements` counts the objects, which is what a summary over resource
    * quota sets prints. For every other type each item is one object and the
    * two are equal. */
   const auto master_list = qconf_master_list_of<E>(list_type);
   OcsQConfWriteState state;
   state.all_ok = (result->parse_failed == 0);
   ocs_gdi_post_processing post;            ///< what the applied objects need post-processed
   for (auto &item: parsed) {
      size_t applied_here = 0;

      for (auto &ep: item.elements) {
         /* Named by the request, not by the text: the configuration's host is
          * the argument of "-mconf" or the basename of the "-Mconf" file, and
          * its text renders the host as a comment only. Set before the object is
          * read back, so everything below - the existence check, the messages -
          * sees the name the caller gave. */
         if (!parameter.get_name().empty()) {
            ocs_qconf_set_name<E>(*ep, parameter.get_name().c_str());
         }
         const char *name = ocs_qconf_key_of<E>(*ep);
         /* For a type addressed by name the question is whether that name is
          * stored; for a singleton there is no name to ask about and the answer
          * is a property of the store itself - the scheduler configuration and
          * the share tree exist, or the type is empty. Deciding it from the
          * master list here is what the client used to decide from its bulk
          * name GET, and it is now decided where the list actually is. */
         const bool exists = E::has_key ? ocs_qconf_name_in_list<E>(master_list, name) : !master_list.empty();

         /* An object already stored in exactly the state the text asks for is
          * not written at all, see qconf_write_is_unchanged() and
          * ocs_qconf_write_settle(). */
         const bool unchanged = exists && qconf_write_is_unchanged<E>(master_list, name, *ep);

         bool applied = true;
         if (ocs_qconf_write_settle(desc, parameter, name, exists, unchanged, &state, answer_list, result)) {
            applied = ocs_qconf_write_tally(qconf_apply_one<E>(packet, task, monitor, desc, object, *ep, exists,
                                                               answer_list, &post),
                                            &state, result);
         }
         if (applied) {
            applied_here++;
         }

         ep.reset();
      }

      if (applied_here == item.elements.size()) {
         result->applied++;
      } else {
         state.all_ok = false;
      }
   }

   ocs_qconf_write_post_process(packet, &post);

   /* Phase 3: what the text did not hold is deleted, for a replacing write.
    *
    * The mirror image of the round trip report below, and the reason the two
    * are exclusive: both compare the stored objects against the names the texts
    * held, and then one deletes what is left over while the other says it is
    * still there. The complex list is a replace because that is how a complex
    * is removed from a file-managed configuration - there is no "-dc", only a
    * row taken out of the table - and a modify of the rule sets is not, because
    * a shortened text there used to wreck the quota configuration (CS-2823).
    *
    * The names are collected before anything is deleted: a delete changes the
    * very master list this walks.
    *
    * A parse failure stops it. Nothing was written then, so every stored object
    * would count as "not in the text" and the replace would empty the type -
    * which is the one way this could do real damage. */
   if (parameter.is_replace() && result->parse_failed == 0) {
      // asked again: phase 2 may have created or replaced the master list
      for (const auto ep : qconf_master_list_of<E>(list_type)) {
         const char *name = ocs_qconf_key_of<E>(ep);
         if (name == nullptr || ocs_qconf_write_is_parsed(state, name)) {
            continue;
         }
         const bool retain = type_entry.hooks.retain_on_replace != nullptr && type_entry.hooks.retain_on_replace(ep);
         ocs_qconf_write_note_leftover(&state, name, retain);
      }
      ocs_qconf_write_delete_leftovers(packet, task, desc, parameter, &state, answer_list);
   }

   ocs_qconf_write_report_leftovers(parameter, *result, state, list_type, &ocs_qconf_stored_names<E>,
                                    &ocs_qconf_name_stored<E>, answer_list);

   const bool ok = ocs_qconf_write_summarize(parameter, state, answer_list);
   DRETURN(ok);
}

/** @brief Apply a set of objects, parsing each one first, see the header
 *
 * Runs qconf_write_entry() for the entry of @p obj, so that the body works on
 * the object type's handles and hooks.
 *
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
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::QConfWriteResult *result) {
   DENTER(TOP_LAYER);
   const bool ok = ocs_with_qconf_object(obj, [&](const auto &type_entry) {
      return qconf_write_entry(packet, task, monitor, type_entry, parameter, answer_list, result);
   });
   DRETURN(ok);
}
