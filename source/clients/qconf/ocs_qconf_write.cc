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
 * @brief The add and modify switches of `qconf`, see the header
 */

#include <cerrno>
#include <cstring>
#include <ostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "uti/sge_dstring.h"
#include "uti/sge_edit.h"
#include "uti/sge_tmpnam.h"
#include "uti/sge_log.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "sgeobj/sge_answer.h"
#include "spool/flatfile/sge_flatfile.h"
#include "procedure/qconf/ocs_QConfModelBase.h"
#include "procedure/qconf/write/ocs_QConfWriteModelClient.h"
#include "qconf/ocs_QConfParameterClient.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_write.h"
#include "ocs_qconf_common.h"
#include "ocs_qconf_read.h"
#include "ocs_qconf_parse.h"

/** @brief Add or modify every object at a path through the write procedure (CS-2822)
 *
 * The only path there is since CS-2824, when the last three types joined it.
 * What is left here is what is genuinely the client's: walking the path, reading
 * the files, and the summary line that names the path. The parse, the add
 * against modify decision and the writes are qmaster's, in one request.
 *
 * **A batch of several objects is not atomic, and that is a decision** - the one
 * CS-2824 asked for rather than left to fall out. qmaster applies the objects one
 * by one through the same per-type modifier a single write uses, and those spool
 * as they go, so undoing what already went in would need a transaction the data
 * store does not have. Making a batch atomic is its own change with its own
 * release note, not a side effect of moving the send here. What a caller gets
 * instead is enough to act on:
 *
 *  - an all-or-nothing gate at the **parse** level: "-strict", and
 *    unconditionally for a type whose text holds a list;
 *  - counts that are typed rather than inferred from the answer list - applied,
 *    parse_failed and elements - so "N of M" is arithmetic and not a guess;
 *  - one answer list entry per object, naming what went in and what did not;
 *  - "-dry", which shows the whole plan before any of it happens.
 *
 * That the gate is at the *parse* level is the part worth reading twice, because
 * the name "-strict" promises more than it keeps: a text that parses and is then
 * refused by the modifier - a cluster queue naming a host group that does not
 * exist, say - is not caught by it, since the cross object reference checks run
 * in the apply and not in the parse. The phase comments in
 * ocs_QConfWriteQmaster.cc say why, and name the ticket that could change it.
 *
 * @param os the stream the summary is written to (CS-1841)
 * @param o the object type
 * @param path file or directory to apply
 * @param select for a type whose text holds a list, the names to write out of it,
 *               see qconf_apply_obj()
 * @param extra_mode write modes the switch adds, see qconf_apply_obj()
 * @param name the configuration name for a type whose text does not name its
 *             object ("-Mconf": the file's basename), nullptr otherwise
 * @return the number of objects that failed
 */
static int
qconf_apply_path_procedure(std::ostream &os, const QConfObj o, const char *path,
                           const char *select = nullptr, uint32_t extra_mode = 0u,
                           const char *name = nullptr) {
   DENTER(TOP_LAYER);

   const bool is_dir = sge_is_directory(path);

   ocs::QConfWriteParameter param;
   param.set_output_format(qconf_opt_format == SP_FORM_JSON
                              ? ocs::ProcedureParameter::OutputFormat::JSON
                              : ocs::ProcedureParameter::OutputFormat::PLAIN);
   param.set_mode((qconf_opt_strict ? ocs::QConfWriteParameter::MODE_STRICT : 0u) |
                  (qconf_opt_dry_run ? ocs::QConfWriteParameter::MODE_DRY : 0u) |
                  extra_mode);
   const bool as_list = (extra_mode & ocs::QConfWriteParameter::MODE_LIST) != 0;
   if (select != nullptr) {
      param.set_select(select);
   }
   /* The one type whose text does not name its object - for "-Mconf" the name
    * is the file's basename, resolved by the caller, see
    * QConfWriteParameter::set_name() */
   if (name != nullptr) {
      param.set_name(name);
   }

   qconf_text_ctx ctx = {&param, 0};
   qconf_for_each_file(path, qconf_collect_text, &ctx);

   /* A file this side could not even open counts as a file that failed to parse,
    * which is what it was when the parse happened here - so -strict rejects the
    * batch before it is sent. The mode cannot be left to qmaster alone: qmaster
    * never learns of a file it was not sent.
    *
    * A type whose text holds a list rejects such a batch whether or not -strict
    * was given: its objects come in whole texts, and the path this replaced
    * applied nothing when one file of a directory could not be read (CS-2823). */
   if ((qconf_opt_strict || (as_list && is_dir)) && ctx.n_fail > 0) {
      ERROR(MSG_QCONF_STRICTNOTHINGAPPLIED_SI, path, ctx.n_fail);
      DRETURN(ctx.n_fail);
   }

   const int n_items = param.get_item_count();
   uint32_t applied = 0;
   uint32_t parse_failed = 0;
   uint32_t elements = 0;

   /* Nothing readable at the path means nothing to send - but still a summary
    * below, because an empty directory has always produced one. */
   if (n_items > 0) {
      ocs::QConfWriteModelClient model(qconf_desc(o).target);
      lList *alp = nullptr;
      model.make_snapshot(&alp, param);

      /* The counts come back typed rather than being inferred from the answers,
       * because the two kinds of failure are not distinguishable there and
       * -strict turns on exactly that difference - see
       * ProcedureParameter::WRITE_APPLIED. */
      if (!model.get_write_result(applied, parse_failed, &elements)) {
         /* No write result means the request never reached the procedure -
          * refused on permission, say. Nothing was applied, and the reason is in
          * the answer list. */
         DPRINTF("no write result, treating all %d item(s) as failed\n", n_items);
      }

      /* show_answer_list() and not answer_list_output(): the per-item messages
       * are what qmaster put into the answer list, and this is where every
       * qconf write surfaces them - see qconf_edit_obj_procedure(). */
      show_answer_list(alp);
      lFreeList(&alp);

      if ((qconf_opt_strict || (as_list && is_dir)) && parse_failed > 0) {
         /* qmaster applied nothing. The message names the path, which is this
          * side's, and it is the one place where the two phases have to be told
          * apart - hence the typed count rather than a count of error entries.
          *
          * The same message for a **directory** of a list type without -strict,
          * because that is what the administrator is told today: one unreadable
          * file among several reports "nothing applied" and applies nothing. A
          * single file does not get the line - there the per item message is the
          * whole story, and adding one would be a new sentence for an unchanged
          * behaviour (CS-2823). */
         ERROR(MSG_QCONF_STRICTNOTHINGAPPLIED_SI, path, static_cast<int>(parse_failed));
         DRETURN(static_cast<int>(parse_failed));
      }
   }

   const int n_ok = static_cast<int>(applied);
   const int n_fail = n_items - n_ok + ctx.n_fail;
   if (is_dir) {
      /* CS-2299 H1: one summary line per directory batch. Still printed here and
       * still to this stream - it names the path, which is this side's.
       *
       * A type whose text holds a list counts objects, not files: that is what
       * the resource quota path printed before it came here, and "3 of 3" over
       * two files would be a different number than the administrator asked
       * about. The failures stay items - a file either went in whole or did
       * not (CS-2823). */
      qconf_info(os, MSG_QCONF_ADDMODSUMMARY_SII, path,
                 as_list ? static_cast<int>(elements) : n_ok, n_fail);
   }

   DRETURN(n_fail);
}

/** @brief Add or modify from a file or directory, through the write procedure.
 *
 * Every object type with a file side writes through the procedure as of CS-2824,
 * so what is left here is the name its callers use and the defaults they rely
 * on. The switch that used to pick between this and a client-side parse is gone
 * with its last entries - the cluster queue, the host group and the userset,
 * whose editor moved in CS-2823 while their "-A"/"-M" side stayed behind. That
 * left those three objects with two writers, and the one that parsed here could
 * not see that the read had been reduced, which is the whole reason the write
 * moved. Its own comment had asked for the switch to disappear with the last
 * entry rather than become a permanent column, so it does.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param o    the object type
 * @param path file or directory to read
 * @param select for a type whose text holds a list, the names to write out of it -
 *               "-Mrqs <file> <names>" and nothing else uses it; nullptr writes
 *               everything the text holds (CS-2823)
 * @param extra_mode write modes the switch adds: MODE_LIST when its text holds a
 *               list of objects, and MODE_REPLACE when what the text leaves out
 *               is to be deleted - "-Mc" is both, "-Mce" is neither, and they
 *               are the same object type (CS-2823)
 * @param name the configuration name for a type whose text does not name its
 *             object, see qconf_apply_path_procedure(); nullptr otherwise
 * @return 0 on success, non-zero on error
 */
static int
qconf_apply_obj(std::ostream &os, QConfObj o, const char *path, const char *select = nullptr,
                uint32_t extra_mode = 0u, const char *name = nullptr) {
   return qconf_apply_path_procedure(os, o, path, select, extra_mode, name);
}

/** @brief Why an interactive edit did not yield an object. */
enum class QConfEditStatus {
   OK,            ///< the object was edited and parsed
   WRITE_FAILED,  ///< the template could not be written for the editor
   EDIT_FAILED,   ///< the editor itself reported an error
   UNCHANGED,     ///< the editor left the file untouched
   READ_FAILED    ///< the result did not parse, or left a field unprocessed
};

/* Whether a text the editor left unchanged is an error is decided per switch,
 * and it has to stay that way, because the predecessor's answers differ. The
 * table, read out of 9.1 and verified against it:
 *
 *   -ace, -mce   0  the mtime gate is "status >= 0" and the centry flow has no
 *                   content comparison at all, so the unchanged object is simply
 *                   sent again (centry_provide_modify_context())
 *   -mc          0  same gate; the per element filter of
 *                   centry_list_add_del_mod_via_gdi() then drops every row, so
 *                   nothing is sent and nothing is reported
 *   -ahgrp, -aq  0  same gate, and the add passes ignore_unchanged_message, so
 *                   the template is accepted as it stands
 *   -mhgrp, -mq  1  same gate, but the modify does compare: with no differences
 *                   object_has_differences() fails and the answer is
 *                   MSG_FILE_NOTCHANGED, "Object has not been changed"
 *   everything   1  the gate is "status == 0" - for the rule quota sets
 *   else            explicitly (rqs_provide_modify_context()), for the rest
 *                   because they never had a gate of their own
 *
 * So "status >= 0" was never a decision that an unchanged text is acceptable. It
 * says that the file's timestamp is not the authority, and for the host group and
 * the cluster queue the content comparison right behind it is - which then
 * refuses an unchanged modify, with a different message than "file unchanged" but
 * the same exit code. The complex is the one type that really does accept it
 * unconditionally, because its flow compares nothing, and that is an
 * inconsistency inside 9.1 rather than the rule.
 *
 * What 9.2 therefore has to reproduce is five switches, not seven: -ace, -mce,
 * -mc, -ahgrp and -aq. For -mhgrp and -mq the exit code is already right and is
 * left alone - 9.2 reaches it through the timestamp instead of the comparison,
 * which it has to, because the client may not parse the text (see
 * qconf_edit_text()). Carrying the flag per call site is how 9.1 carried it, and
 * no rule shorter than the table above is true.
 */

/** @brief Offer a rendered object text to $EDITOR and read the result back
 *
 * The shape a stored procedure write needs: a procedure read produces text and
 * a procedure write consumes text, so the client carries it to the editor and
 * back without rendering or parsing anything itself. It replaced the object
 * level editor of the GET/MOD path, which CS-2823 removed with its last
 * caller.
 *
 * That is not a convenience. It is the only arrangement that survives output
 * reduction: when a value the caller may not see is rendered as a placeholder,
 * only qmaster knows which fields those were. A client that parsed the text
 * would hand the placeholder back as if it were a value and overwrite the
 * original.
 *
 * Deliberately **exit-free**: every failure is reported through the return
 * value, never by terminating the process, because the same logic runs inside
 * qmaster, where sge_exit() would take the daemon down with it. The caller
 * decides what a failure means - see qconf_edit_fail_exit() for what the
 * command line client does. The temporary file is consumed on every path
 * including every error path.
 *
 * @param text in: the text to offer; out: the text that came back
 * @param uid  uid to run the editor as
 * @param gid  gid to run the editor as
 * @param unchanged_is_ok whether a text the editor left alone is read back and
 *             returned like a changed one, see qconf_unchanged_is_tolerated()
 * @return OK, or the reason no text came back
 */
static QConfEditStatus
qconf_edit_text(std::string &text, uid_t uid, gid_t gid, bool unchanged_is_ok) {
   DENTER(TOP_LAYER);

   char *filename = sge_malloc(sizeof(char) * SGE_PATH_MAX);
   if (filename == nullptr) {
      ERROR(SFNMAX, MSG_MEMORY_MALLOCFAILED);
      DRETURN(QConfEditStatus::WRITE_FAILED);
   }
   int fd = -1;
   dstring error_message = DSTRING_INIT;
   if (sge_tmpnam(filename, &fd, &error_message) == nullptr) {
      ERROR(SFNMAX, sge_dstring_get_string(&error_message));
      sge_dstring_free(&error_message);
      sge_free(&filename);
      DRETURN(QConfEditStatus::WRITE_FAILED);
   }
   sge_dstring_free(&error_message);

   /* Every failure below says why. WRITE_FAILED is reported by
    * qconf_edit_fail_exit() through sge_error_and_exit(nullptr), which prints
    * nothing of its own - so a silent return here leaves the user with a
    * non-zero exit and no reason at all. */
   errno = 0;
   FILE *fp = fdopen(fd, "w");
   if (fp == nullptr) {
      ERROR(MSG_ERROROPENINGFILEFORWRITING_SS, filename, strerror(errno));
      close(fd);
      unlink(filename);
      sge_free(&filename);
      DRETURN(QConfEditStatus::WRITE_FAILED);
   }
   errno = 0;
   fprintf(fp, "%s", text.c_str());
   const bool written = (fclose(fp) == 0);
   if (!written) {
      ERROR(MSG_ERROROPENINGFILEFORWRITING_SS, filename, strerror(errno));
      unlink(filename);
      sge_free(&filename);
      DRETURN(QConfEditStatus::WRITE_FAILED);
   }

   /* A status above zero is "unchanged", and for a tolerant type that is not a
    * failure: the file is read back and written exactly as a changed one would
    * be, which is what the per type flows did before CS-2823. */
   const int status = sge_edit(filename, uid, gid);
   if (status < 0 || (status > 0 && !unchanged_is_ok)) {
      unlink(filename);
      sge_free(&filename);
      DRETURN((status < 0) ? QConfEditStatus::EDIT_FAILED : QConfEditStatus::UNCHANGED);
   }

   const bool read_ok = qconf_read_file_text(filename, text);
   if (!read_ok) {
      ERROR(MSG_ERROROPENINGFILEFORREADING_SS, filename, strerror(errno));
   }
   unlink(filename);
   sge_free(&filename);

   DRETURN(read_ok ? QConfEditStatus::OK : QConfEditStatus::READ_FAILED);
}

/** @brief Report an edit failure the way the option branches always did, and exit.
 *
 * The one place where the interactive path terminates the process, kept apart
 * from qconf_edit_text() so that the editing logic stays reusable. Does not
 * return - sge_error_and_exit() calls sge_exit().
 *
 * @param st the failure reported by qconf_edit_text()
 */
static void
qconf_edit_fail_exit(QConfEditStatus st) {
   switch (st) {
      case QConfEditStatus::WRITE_FAILED:
         sge_error_and_exit(nullptr);
         break;
      case QConfEditStatus::EDIT_FAILED:
         sge_error_and_exit(MSG_PARSE_EDITFAILED);
         break;
      case QConfEditStatus::UNCHANGED:
         sge_error_and_exit(MSG_FILE_FILEUNCHANGED);
         break;
      case QConfEditStatus::READ_FAILED:
         sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE);
         break;
      case QConfEditStatus::OK:
         break;
   }
}

/** @brief Offer a text to the editor and write the result through the procedure
 *
 * The tail both the interactive add and the interactive modify end in: qmaster
 * parses the edited text, validates it and applies it through the object's own
 * modifier, spooler and event hooks.
 *
 * It is a **set of one**. CS-2822 started with a procedure of its own for the
 * calendar; the file batch made it one procedure for every object type and any
 * number of objects, and an interactive write is that with the number one - so
 * there is one writing path and not two that could drift.
 *
 * @param o    the object type
 * @param text the object text; edited in place
 * @param uid  uid to run the editor as
 * @param gid  gid to run the editor as
 * @param unchanged_is_ok whether a text the editor left alone is written anyway,
 *             see the table above qconf_edit_text()
 * @param select for a type whose text holds a list, the names the read was asked for -
 *               the scope qmaster reports against; nullptr when it read everything
 * @param extra_mode write modes the switch adds on top of MODE_ROUND_TRIP, see
 *               qconf_apply_obj()
 * @param name   the configuration name for a type whose text does not name its
 *               object, see qconf_apply_path_procedure(); nullptr otherwise
 * @return 0 when the object was written, 1 otherwise - which an unchanged text
 *         is unless @p unchanged_is_ok says it is not
 */
static int
qconf_edit_and_write_obj(QConfObj o, std::string &text, uid_t uid, gid_t gid,
                         bool unchanged_is_ok, const char *select = nullptr,
                         uint32_t extra_mode = 0u, const char *name = nullptr) {
   DENTER(TOP_LAYER);

   const QConfEditStatus est = qconf_edit_text(text, uid, gid, unchanged_is_ok);
   if (est != QConfEditStatus::OK) {
      qconf_edit_fail_exit(est);
      DRETURN(1);
   }

   ocs::QConfWriteParameter mod_param;
   /* The read rendered in the caller's -fmt, so the write has to parse in it -
    * they are two halves of one round trip, and the format that decided the one
    * decides the other (CS-2823). */
   mod_param.set_output_format(qconf_opt_format == SP_FORM_JSON
                                  ? ocs::ProcedureParameter::OutputFormat::JSON
                                  : ocs::ProcedureParameter::OutputFormat::PLAIN);
   /* This text is what a read handed out, not a file: see MODE_ROUND_TRIP, which
    * is what makes an object missing from it a report rather than an error. */
   mod_param.set_mode(ocs::QConfWriteParameter::MODE_ROUND_TRIP | extra_mode);
   if (select != nullptr) {
      mod_param.set_select(select);
   }
   /* The one type whose text does not name its object, see
    * QConfWriteParameter::set_name() */
   if (name != nullptr) {
      mod_param.set_name(name);
   }
   mod_param.add_item(nullptr, text);
   ocs::QConfWriteModelClient model(qconf_desc(o).target);

   lList *alp = nullptr;
   const bool ok = model.make_snapshot(&alp, mod_param);

   /* show_answer_list() and not answer_list_output(), and no response text of
    * our own: the message a write produces is the one qmaster put into the
    * answer list, and this is how every other qconf write surfaces it - see
    * qconf_apply_path_procedure(). Measured on 2026-10-06: the answer list does arrive
    * with the INFO in it, answer_list_output() just does not put it where the
    * user sees it, and printing an invented confirmation instead both lost the
    * wording and moved it from stderr to stdout. */
   const int shown = show_answer_list(alp);
   lFreeList(&alp);

   DRETURN((ok && shown == 0) ? 0 : 1);
}

/** @brief Edit one object through the stored procedures (CS-2822, CS-2823)
 *
 * The round trip in the shape it is meant to have: the object is read by a
 * procedure, so qmaster rendered it and could have reduced it; the text goes to
 * the editor unchanged; the edited text goes back as a procedure, where qmaster
 * parses it and applies it through the object's own modifier, spooler and event
 * hooks. The client renders nothing and parses nothing.
 *
 * It serves `-m<obj>` and `-a<obj>` alike, because they are one operation: a
 * name that exists is offered as it stands, and a name that does not is
 * answered by the read with the type's template and created on exit (CS-2299 C2
 * and its siblings, now qconf_read_desc::create_template). The switch name is
 * carried only so that the refusals below can name it.
 *
 * Two refusals are deliberate and are the reason the function exists rather
 * than being a few lines at the option:
 *
 *  - a **reduced** read is not offered for editing at all. Asking someone to
 *    edit a text whose values are placeholders invites them to save the
 *    placeholder as the value.
 *  - the **client execution context** is declined outright. It reads with a
 *    plain GET, which returns the whole object and cannot reduce, so accepting
 *    it would make the first refusal depend on which context the caller chose -
 *    which is the same as not having it. CS-2856 removes the choice; until then
 *    the write path says no.
 *
 * **An add is this same function, and it is never refused by the first of those
 * two** (CS-2824). "-a<obj>" and "-m<obj>" have been one call since CS-2823, so
 * there is no second path to audit - and the read of a name that is not stored is
 * answered with the type's template, which is built for that answer and carries
 * nobody's data, so there is nothing in it that could have been removed. That is
 * enforced and not merely true today: qmaster reports REDUCTION_NONE whenever the
 * answer was a template, in exec_procedure() in sge_c_gdi_procedure.cc.
 *
 * The order of the two decisions is what makes it safe, and it is this way round
 * on purpose: the refusal sits between the read and the editor, so a "-m<obj>" on
 * an object that is stored **and** reduced returns here and never reaches the
 * editor. It cannot be turned into an add on the way.
 *
 * Nothing in it is type-specific: the GDI target comes from the descriptor row,
 * and so does what a missing object means. The exec host is the one type among
 * the eight whose row has no template, which is how "a host is not created from
 * a name" is expressed - for "-me" and "-ae" alike.
 *
 * @param param the invocation, used for the procedure read
 * @param o     the object type
 * @param sw    the switch being served, named in the refusals
 * @param name  the object to edit
 * @param uid   uid to run the editor as
 * @param gid   gid to run the editor as
 * @param unchanged_is_ok whether a text the editor left alone is written anyway;
 *              true for "-ace", "-mce", "-ahgrp" and "-aq", see the table above
 *              qconf_edit_text()
 * @return 0 when the object was written, 1 otherwise - which an unchanged text
 *         is unless @p unchanged_is_ok says it is not
 */
static int
qconf_edit_obj_procedure(ocs::QConfParameterClient &param, QConfObj o,
                         const char *sw, const char *name, uid_t uid, gid_t gid,
                         bool unchanged_is_ok = false) {
   DENTER(TOP_LAYER);

   if (param.get_exec_context() != ocs::ProcedureParameter::ExecContext::SERVER) {
      ERROR(MSG_QCONF_MODNEEDSSERVERCTX_S, sw);
      DRETURN(1);
   }

   /* The read has to render what the write can parse, which for the exec host
    * is not what "-se" displays - see QConfParameter::get_for_edit(). The same
    * flag is what makes the read answer a name that does not exist with a
    * template instead of a complaint. The parameter belongs to the whole
    * invocation and one command line can carry several operations ("qconf -me
    * host -se host"), so it is set for this read and cleared again right after
    * it: the flag describes the read, not the call. */
   std::ostringstream rendered;
   uint32_t reduction = ocs::ProcedureParameter::REDUCTION_NONE;
   param.set_for_edit(true);
   const QConfShowStatus st = qconf_show_object(rendered, param, o, name, &reduction);
   param.set_for_edit(false);

   if (st != QConfShowStatus::OK) {
      /* The read put the reason into the answer list and has printed it. For a
       * type with a template a missing name cannot be the reason - it was
       * answered with one; for the exec host it is the "x is not an execution
       * host" of its own descriptor row. */
      DRETURN(1);
   }

   if (reduction != ocs::ProcedureParameter::REDUCTION_NONE) {
      /* A singleton reaches this with no name - "-msconf" and "-mstree" have
       * none to give - and the message takes one, so it is called by its type.
       * Same reason as the nameless list form below (CS-2823). */
      ERROR(MSG_QCONF_REDUCEDNOTMODIFIABLE_SS, sw, qconf_object_label(qconf_desc(o), name));
      DRETURN(1);
   }

   std::string text = rendered.str();
   /* read.name_header is the descriptor's way of saying that the text does not
    * carry the object's name - the view writes it as a comment instead, which
    * the parse ignores. For such a type the name has to travel with the write,
    * or the edited configuration would arrive nameless (CS-2823). */
   const char *write_name = qconf_desc(o).read.name_header ? name : nullptr;
   DRETURN(qconf_edit_and_write_obj(o, text, uid, gid, unchanged_is_ok, nullptr, 0u, write_name));
}

/** @brief Edit a whole list of objects through the stored procedures (CS-2823)
 *
 * The sibling of qconf_edit_obj_procedure() for a type whose text holds a list:
 * the resource quota sets, where "-mrqs" without a name shows every rule set in
 * one text and "-mrqs a,b" shows those two. The read, the editor and the write
 * are the same three steps, and the two refusals are the same two - a reduced
 * read is not offered for editing, and the client execution context is declined
 * because it cannot reduce.
 *
 * What differs is the unit. The text holds N objects, so the write is told the
 * scope the read was asked for: qmaster applies what the text holds and reports
 * what it left out, instead of deleting it. That is the behaviour change of
 * CS-2823 for "-mrqs", decided on 2026-10-15 - an administrator could wreck the
 * whole quota configuration by saving a shortened text, and under output
 * reduction they would have deleted what they were not allowed to see.
 *
 * A name that is no object yet is answered by the read with the type's
 * template, one per name, which is what makes "-arqs" the same operation as
 * "-mrqs".
 *
 * The read is named by the caller rather than looked up, because the two users
 * read through different doors: "-mrqs" through the type's own OBJECT_LIST
 * switch, "-mc" through the CENTRY_LIST sub-procedure, whose aligned table is
 * neither a name list nor one object (CS-2817). Both hand back a text holding a
 * list, which is all this function needs of them.
 *
 * @param param the invocation, used for the procedure read
 * @param o     the object type, which must be one whose text holds a list
 * @param sw    the switch being served, named in the refusals
 * @param read_token the read switch whose text is offered to the editor
 * @param read_sub   the sub-procedure that read serves, or nullptr for a plain read
 * @param names the objects to edit, comma separated, or nullptr for all of them
 * @param uid   uid to run the editor as
 * @param gid   gid to run the editor as
 * @param extra_mode write modes on top of MODE_LIST, see qconf_apply_obj()
 * @param unchanged_is_ok whether a text the editor left alone is written anyway;
 *              true for "-mc" and false for "-mrqs", see the table above
 *              qconf_edit_text()
 * @return 0 when the text was written, 1 otherwise - which an unchanged text
 *         is unless @p unchanged_is_ok says it is not
 */
static int
qconf_edit_list_procedure(ocs::QConfParameterClient &param, QConfObj o,
                          const char *sw, const char *read_token, const char *read_sub,
                          const char *names, uid_t uid, gid_t gid, uint32_t extra_mode = 0u,
                          bool unchanged_is_ok = false) {
   DENTER(TOP_LAYER);

   if (param.get_exec_context() != ocs::ProcedureParameter::ExecContext::SERVER) {
      ERROR(MSG_QCONF_MODNEEDSSERVERCTX_S, sw);
      DRETURN(1);
   }

   if (read_token == nullptr) {
      /* a type without a list read cannot be edited as a list - a programming
       * error, not something a caller can provoke */
      DRETURN(1);
   }

   qconf_op op;
   op.token = read_token;
   if (names != nullptr) {
      op.args.emplace_back(names);
   }
   param.set_operation(op);
   if (read_sub != nullptr) {
      param.set_sub_procedure_name(read_sub);
   }

   std::ostringstream rendered;
   uint32_t reduction = ocs::ProcedureParameter::REDUCTION_NONE;
   lList *alp = nullptr;
   param.set_for_edit(true);
   const bool ok = qconf_dispatch_procedure(rendered, param, &alp, &reduction);
   param.set_for_edit(false);
   if (read_sub != nullptr) {
      /* The name belongs to this read, not to the invocation: one command line
       * can carry several operations, so it is cleared again right away - as
       * the -for-edit flag is. */
      param.set_sub_procedure_name("");
   }

   if (!ok) {
      /* the reason is in the answer list - a name the type cannot build a
       * template for, or a read that failed outright */
      show_answer_list(alp);
      lFreeList(&alp);
      DRETURN(1);
   }
   lFreeList(&alp);

   if (reduction != ocs::ProcedureParameter::REDUCTION_NONE) {
      /* What was asked for, which for the nameless form is the whole list - the
       * message takes a name, and an empty one would read "cannot modify \"\"". */
      const char *what = (names != nullptr) ? names : qconf_desc(o).read.list_name;
      ERROR(MSG_QCONF_REDUCEDNOTMODIFIABLE_SS, sw, what != nullptr ? what : "");
      DRETURN(1);
   }

   std::string text = rendered.str();
   DRETURN(qconf_edit_and_write_obj(o, text, uid, gid, unchanged_is_ok, names,
                                    ocs::QConfWriteParameter::MODE_LIST | extra_mode));
}

/**
 * @brief Per-file apply callback: upsert one host config (CS-2311 C2+C3).
 *
 * The host key is the file basename; the file content is the configuration.
 *
 * @param filepath path of the config file to apply
 * @param vctx     pointer to the qconf_conf_ctx for this batch
 * @return 0 on success, 1 on failure
 */
static int
qconf_conf_apply_one(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_conf_ctx *>(vctx);
   lListElem *host_ep = lCreateElem(EH_Type);
   const char *hostname = qconf_conf_host_of(filepath, host_ep);

   /* CS-2299 H3 used to preview -dry here, because add_modify_config() always
    * contacted qmaster. The procedure carries MODE_DRY itself and answers with
    * the same two sentences - MSG_QCONF_WRITEWOULDADD_S and
    * MSG_QCONF_WRITEWOULDMODIFY_S are the client messages word for word - so the
    * preview is one place now, and the probe that decided it here is one bare
    * GET less: it asked whether the host has a configuration and could not see
    * whether it was allowed to know (CS-2823).
    *
    * The parse moved to qmaster; what stays here is the mapping that is a
    * property of the path and not of the object - the host is the file's
    * basename, and it travels with the request because a configuration's text
    * does not carry its name (CS-2823). */
   int ret = qconf_apply_obj(*ctx->os, QConfObj::CONF, filepath, nullptr, 0u, hostname);
   lFreeElem(&host_ep);
   if (ret != 0) {
      ctx->n_fail++;
      return 1;
   }
   ctx->n_ok++;
   return 0;
}

/**
 * @brief Apply one config file or a whole directory of them (CS-2311 C3).
 *
 * Each file is upserted - sge_mod_configuration() adds what is missing and
 * modifies what is there; the host is the basename.
 * A directory prints the H1 one-line summary.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param path  config file or directory of config files
 * @return the number of files that failed
 */
static int
qconf_conf_apply_path(std::ostream &os, const char *path) {
   qconf_conf_ctx ctx = {&os, nullptr, 0, 0};
   bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_conf_apply_one, &ctx);
   if (is_dir) {
      qconf_info(os, MSG_QCONF_ADDMODSUMMARY_SII, path, ctx.n_ok, ctx.n_fail);
   }
   return ctx.n_fail;
}

/** @brief How a table driven add or modify switch writes */
enum class QConfWriteKind {
   EDIT,       ///< "-a<obj>"/"-m<obj> [name]": qconf_edit_obj_procedure()
   EDIT_LIST,  ///< "-arqs"/"-mrqs"/"-mc": qconf_edit_list_procedure(), the objects as one text
   APPLY       ///< "-A<obj>"/"-M<obj> file|dir [names]": qconf_apply_obj()
};

/** @brief "-Arqs"/"-Mrqs"/"-Mc" without a file: the message these always had
 *
 * Their file is optional in the option table, so the parser hands the
 * operation over complete and the generic "no option argument" never fires;
 * they reported the missing file on their own.
 *
 * @param op the operation
 * @return true when the file is missing and it was reported
 */
static bool
qconf_write_no_file(const qconf_op &op) {
   if (!op.args.empty()) {
      return false;
   }
   fprintf(stderr, "%s\n", MSG_FILE_NOFILEARGUMENTGIVEN);
   return true;
}

/** @brief One add or modify switch served by qconf_run_write() */
struct qconf_write_entry {
   const char *switch_name;   ///< the switch as typed
   QConfObj object;           ///< the object type it writes
   QConfWriteKind kind;       ///< editor or file
   const char *default_name;  ///< what an EDIT row edits when no name is given
   bool unchanged_is_ok;      ///< EDIT: write a text the editor left alone
   QConfClientCheck check;    ///< the check it makes in the client
   qconf_names_hook names;    ///< EDIT: the names to edit one by one, nullptr for the argument as typed
   uint32_t mode;             ///< EDIT_LIST/APPLY: write modes beyond the default, a mask of MODE_*
   const char *list_read;     ///< EDIT_LIST: the read of the list, nullptr for the type's OBJECT_LIST read
   const char *list_sub;      ///< EDIT_LIST: the sub-procedure of that read, or nullptr
   bool (*missing)(const qconf_op &op); ///< a report of its own for a missing argument, or nullptr
   bool blank_line_between;   ///< with names: the objects are separated by a blank line
   int (*apply)(std::ostream &os, const char *path); ///< APPLY: the reader of its own, nullptr for qconf_apply_obj()
};

/** @brief The add and modify switches that are one engine call each (CS-2954)
 *
 * Every "-a<obj>", "-m<obj>", "-A<obj>" and "-M<obj>" whose block did nothing
 * but call qconf_edit_obj_procedure() or qconf_apply_obj(), possibly after a
 * permission check. The option table decides whether a name may be given:
 * "-a<obj>" takes an optional one and edits default_name without it, "-m<obj>"
 * requires one, and the singletons ("-astree", "-mstree", "-msconf") take none
 * and edit with nullptr. "-ae", "-me" and "-mu" are rows with a names hook:
 * the host resolved (or the template for "-ae" without a name), or each name
 * of a list edited on its own (CS-2967). "-arqs"/"-mrqs"/"-mc" edit a list of
 * objects as one text and "-Arqs"/"-Mrqs"/"-Mc" apply one, with the modes of
 * their type (CS-2967), and the host configuration is served host by host or
 * file by file (CS-2967). The ones with handling of their own stay blocks: the
 * reserved host group switches; the membership switches and "-astnode"/
 * "-mstnode" are actions, see qconf_action_table.
 *
 * "-mce" checked qconf_is_manager() again after qconf_is_manager_on_admin_host(),
 * which already exits for a non-manager; the row keeps the first check only.
 */
static const qconf_write_entry qconf_write_table[] = {
   {"-acal",   QConfObj::CAL,  QConfWriteKind::EDIT,  "template",  false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-ackpt",  QConfObj::CK,   QConfWriteKind::EDIT,  "template",  false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-arole",  QConfObj::RL,   QConfWriteKind::EDIT,  "template",  false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-ap",     QConfObj::PE,   QConfWriteKind::EDIT,  "template",  false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-auser",  QConfObj::UU,   QConfWriteKind::EDIT,  "template",  false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-aprj",   QConfObj::PR,   QConfWriteKind::EDIT,  "template",  false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-ace",    QConfObj::CE,   QConfWriteKind::EDIT,  "template",  true,  QConfClientCheck::ADMIN_HOST_THEN_MANAGER},
   {"-ahgrp",  QConfObj::HGRP, QConfWriteKind::EDIT,  "@template", true,  QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-aq",     QConfObj::CQ,   QConfWriteKind::EDIT,  "template",  true,  QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-astree", QConfObj::STN,  QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mcal",   QConfObj::CAL,  QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mckpt",  QConfObj::CK,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mrole",  QConfObj::RL,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mp",     QConfObj::PE,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-muser",  QConfObj::UU,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mprj",   QConfObj::PR,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mce",    QConfObj::CE,   QConfWriteKind::EDIT,  nullptr,     true,  QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mhgrp",  QConfObj::HGRP, QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mq",     QConfObj::CQ,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-msconf", QConfObj::SC,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-mstree", QConfObj::STN,  QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-ae",     QConfObj::EH,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST, qconf_names_ae},
   {"-me",     QConfObj::EH,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST, qconf_names_host_list},
   {"-mu",     QConfObj::US,   QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST, qconf_names_userset_list},
   /* CS-2967: the lists. "-arqs"/"-mrqs" edit the rule sets as one text, a
    * missing one being reported as still there rather than deleted; "-mc" edits
    * the complex table, whose replace mode is how a complex is removed, and
    * writes it even when unchanged. The file switches carry the same modes. */
   {"-arqs",   QConfObj::RQS,  QConfWriteKind::EDIT_LIST, "template", false, QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    nullptr, 0u, nullptr, nullptr, nullptr},
   {"-mrqs",   QConfObj::RQS,  QConfWriteKind::EDIT_LIST, nullptr,    false, QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    nullptr, 0u, nullptr, nullptr, nullptr},
   {"-mc",     QConfObj::CE,   QConfWriteKind::EDIT_LIST, nullptr,    true,  QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    nullptr, ocs::QConfWriteParameter::MODE_REPLACE, "-sc", ocs::QConfParameter::CENTRY_LIST, nullptr},
   {"-Arqs",   QConfObj::RQS,  QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    nullptr, ocs::QConfWriteParameter::MODE_LIST, nullptr, nullptr, qconf_write_no_file},
   {"-Mrqs",   QConfObj::RQS,  QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    nullptr, ocs::QConfWriteParameter::MODE_LIST, nullptr, nullptr, qconf_write_no_file},
   {"-Mc",     QConfObj::CE,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE,
    nullptr, ocs::QConfWriteParameter::MODE_LIST | ocs::QConfWriteParameter::MODE_REPLACE, nullptr, nullptr,
    qconf_write_no_file},
   /* CS-2967: the host configuration. One object per host, so a host list is
    * served host by host with a blank line between them, and the files of
    * "-Aconf"/"-Mconf" are named after their hosts, read by
    * qconf_conf_apply_path(). */
   {"-aconf",  QConfObj::CONF, QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    qconf_names_conf_strict, 0u, nullptr, nullptr, nullptr, true, nullptr},
   {"-mconf",  QConfObj::CONF, QConfWriteKind::EDIT,  nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST,
    qconf_names_conf_lenient, 0u, nullptr, nullptr, nullptr, true, nullptr},
   {"-Aconf",  QConfObj::CONF, QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE,
    qconf_names_conf_paths, 0u, nullptr, nullptr, nullptr, true, qconf_conf_apply_path},
   {"-Mconf",  QConfObj::CONF, QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE,
    qconf_names_conf_paths, 0u, nullptr, nullptr, nullptr, true, qconf_conf_apply_path},
   {"-Acal",   QConfObj::CAL,  QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Ackpt",  QConfObj::CK,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Ae",     QConfObj::EH,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Arole",  QConfObj::RL,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Ap",     QConfObj::PE,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Au",     QConfObj::US,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Auser",  QConfObj::UU,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Aprj",   QConfObj::PR,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Astree", QConfObj::STN,  QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Ace",    QConfObj::CE,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-Ahgrp",  QConfObj::HGRP, QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-Aq",     QConfObj::CQ,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-Mcal",   QConfObj::CAL,  QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mckpt",  QConfObj::CK,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Me",     QConfObj::EH,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mrole",  QConfObj::RL,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mp",     QConfObj::PE,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mu",     QConfObj::US,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Muser",  QConfObj::UU,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mprj",   QConfObj::PR,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mstree", QConfObj::STN,  QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::NONE},
   {"-Mce",    QConfObj::CE,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-Mhgrp",  QConfObj::HGRP, QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-Mq",     QConfObj::CQ,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
   {"-Msconf", QConfObj::SC,   QConfWriteKind::APPLY, nullptr,     false, QConfClientCheck::MANAGER_ON_ADMIN_HOST},
};

QConfOpResult
qconf_run_write(std::ostream &os, ocs::QConfParameterClient &param, const qconf_op &op,
                const char *username, const char *hostname, uid_t uid, gid_t gid) {
   DENTER(TOP_LAYER);

   const qconf_write_entry *entry = nullptr;
   for (const qconf_write_entry &e : qconf_write_table) {
      if (op.token == e.switch_name) {
         entry = &e;
         break;
      }
   }
   if (entry == nullptr) {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   if (entry->missing != nullptr && entry->missing(op)) {
      DRETURN(QConfOpResult::FAILED);
   }
   if (qconf_missing_argument(op)) {
      DRETURN(QConfOpResult::FAILED);
   }

   qconf_client_check(entry->check, username, hostname);

   int failed;
   if (entry->names != nullptr) {
      /* one round trip per name; a name that fails does not stop the others */
      std::vector<std::string> names;
      bool left_out = false;
      entry->names(op, names, left_out);
      failed = left_out ? 1 : 0;
      bool first = true;
      for (const std::string &name : names) {
         if (entry->blank_line_between && !first) {
            os << "\n";
         }
         first = false;
         int one;
         if (entry->kind == QConfWriteKind::APPLY) {
            one = (entry->apply != nullptr) ? entry->apply(os, name.c_str())
                                            : qconf_apply_obj(os, entry->object, name.c_str(), nullptr, entry->mode);
         } else {
            one = qconf_edit_obj_procedure(param, entry->object, entry->switch_name, name.c_str(),
                                           uid, gid, entry->unchanged_is_ok);
         }
         if (one != 0) {
            failed = 1;
         }
      }
   } else if (entry->kind == QConfWriteKind::EDIT) {
      const char *name = op.args.empty() ? entry->default_name : op.args.front().c_str();
      failed = qconf_edit_obj_procedure(param, entry->object, entry->switch_name, name,
                                        uid, gid, entry->unchanged_is_ok);
   } else if (entry->kind == QConfWriteKind::EDIT_LIST) {
      const char *names = op.args.empty() ? entry->default_name : op.args.front().c_str();
      const char *read = (entry->list_read != nullptr)
                         ? entry->list_read
                         : ocs::QConfModelBase::switch_for(entry->object, ocs::QConfReadKind::OBJECT_LIST);
      failed = qconf_edit_list_procedure(param, entry->object, entry->switch_name, read, entry->list_sub,
                                         names, uid, gid, entry->mode, entry->unchanged_is_ok);
   } else {
      /* the second argument, where the option table allows one, selects which
       * objects of the file are written ("-Mrqs file names") */
      const char *select = (op.args.size() > 1) ? op.args[1].c_str() : nullptr;
      failed = qconf_apply_obj(os, entry->object, op.args.front().c_str(), select, entry->mode);
   }

   DRETURN(failed != 0 ? QConfOpResult::FAILED : QConfOpResult::OK);
}
