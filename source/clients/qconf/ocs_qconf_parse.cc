/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief qconf - one handler per configuration switch, and the table that dispatches to them
 */
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cerrno>
#include <cctype>
#include <format>
#include <iostream>
#include <memory>
#include <ostream>
#include <fnmatch.h>
#include <dirent.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_edit.h"
#include "uti/sge_tmpnam.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_hostname.h"
#include "uti/sge_parse_num_par.h"   /* CS-2313a: parse_ulong_val for typed load values */

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/config.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_utility.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/ocs_spool_json.h"   /* CS-2313a: ocs_json_value_format_opt, name-list writer */
#include "spool/flatfile/sge_flatfile_obj.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "sge.h"
#include "sge_options.h"
#include "usage.h"
#include "ocs_qconf_acl.h"
#include "procedure/ocs_ProcedureController.h"
#include "procedure/ocs_ProcedureModel.h"
#include "procedure/ocs_ProcedureView.h"
#include "procedure/qconf/ocs_QConfWriteModelClient.h"

#include "qconf/ocs_QConfController.h"
#include "qconf/table/ocs_QConfTableController.h"
#include "qconf/table/ocs_QConfTableView.h"
#include "qconf/threadpool/ocs_QConfThreadPoolController.h"
#include "qconf/threadpool/ocs_QConfThreadPoolView.h"
#include "qconf/sharetree/ocs_QConfShareTreeNodesController.h"
#include "qconf/sharetree/ocs_QConfShareTreeTreeController.h"
#include "qconf/sharetree/ocs_QConfShareTreeView.h"
#include "qconf/processors/ocs_QConfProcessorController.h"
#include "qconf/processors/ocs_QConfProcessorView.h"
#include "qconf/eventclient/ocs_QConfEventClientController.h"
#include "qconf/eventclient/ocs_QConfEventClientView.h"
#include "qconf/objl/ocs_QConfObjectFilterController.h"
#include "qconf/objl/ocs_QConfObjectFilterView.h"
#include "qconf/sick/ocs_QConfSickController.h"
#include "qconf/sick/ocs_QConfSickView.h"
#include "qconf/cqueue/ocs_QConfCQueueShowController.h"
#include "qconf/cqueue/ocs_QConfCQueueShowView.h"
#include "qconf/hgroup/ocs_QConfHGroupResolvedController.h"
#include "qconf/hgroup/ocs_QConfHGroupShowController.h"
#include "qconf/hgroup/ocs_QConfHGroupShowViewJSON.h"
#include "qconf/hgroup/ocs_QConfHGroupStructureView.h"
#include "qconf/hgroup/ocs_QConfHGroupTreeController.h"
#include "qconf/hgroup/ocs_QConfHGroupWhyController.h"
#include "qconf/hgroup/ocs_QConfHGroupWhyView.h"
#include "qconf/ocs_QConfModelClient.h"
#include "qconf/ocs_QConfObject.h"
#include "qconf/ocs_QConfViewJSON.h"
#include "qconf/ocs_QConfViewPlain.h"
#include "qconf/ocs_QConfParameterClient.h"
#include "ocs_qconf_parse.h"
#include "ocs_qconf_cqueue.h"
#include "sgeobj/ocs_Role.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qconf.h"

static int sge_next_is_an_opt(char **ptr);
static int sge_error_and_exit(const char *ptr);

/* ------------------------------------------------------------- */

/* ------------------------------------------------------------- */
static void parse_name_list_to_cull(const char *name, lList **lpp, lDescr *dp, int nm, char *s);
static bool del_host_of_type(lList *arglp, ocs::gdi::Target target);
static bool mod_reserved_hgroup(std::ostream &os, lList *arglp, const char *group,
                                ocs::gdi::SubCommand sub_command, const char *what);
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp, lListElem **epp, ocs::gdi::SubCommand sub_command, struct object_info_entry *info_entry);

/************************************************************************/
static int delete_configs(const lList *conf_list);

/************************************************************************/
static int qconf_is_manager(const char *user);
static int qconf_is_adminhost(const char *host);
static int qconf_is_manager_on_admin_host(const char *user, const char *host);
/************************************************************************/

static const char *write_attr_tmp_file(const char *name, const char *value, 
                                       const char *delimiter, dstring *error_message);

/***************************************************************************/
static char **sge_parser_get_next(char **arg) {
   DENTER(TOP_LAYER);
   if (!*(arg+1)) {
      ERROR(MSG_QCONF_NOOPTIONARGPROVIDEDTOX_S , *arg);
      sge_usage(QCONF, stderr);
      sge_exit(1);
   }

   DRETURN(++arg);
}

/* ===== CS-2298: shared helpers for qconf add/modify/delete enhancements =====
 *
 * These are object-agnostic (parameterised by the GDI target, the CULL
 * descriptor, the object's spooling fields and its name attribute), so every
 * per-object branch can reuse the same upsert / directory / file-delete logic.
 */

/* CS-2299 hardening flags, set by the argv pre-scan in sge_parse_qconf().
 * They are global because every shared helper below consults them and the
 * option loop processes switches left-to-right (a flag must take effect
 * regardless of where on the command line it appears). */
static bool qconf_opt_dry_run = false;   /* H3 (-dry):    validate/report, do not send */
static bool qconf_opt_force = false;     /* H6 (-f):      skip the bulk-delete prompt */
static bool qconf_opt_strict = false;    /* H2 (-strict): apply nothing unless all files valid */
spool_flatfile_format qconf_opt_format = SP_FORM_ASCII;  ///< see the declaration in `ocs_qconf_parse.h`

const char *
qconf_cstr(const char *s) {
   return s != nullptr ? s : "(null)";
}

void
qconf_msg(std::ostream &os, const char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   dstring buffer = DSTRING_INIT;
   sge_dstring_vsprintf(&buffer, fmt, ap);
   va_end(ap);
   const char *text = sge_dstring_get_string(&buffer);
   if (text != nullptr) {
      os << text;
   }
   sge_dstring_free(&buffer);
}

/**
 * @brief Print a message-catalogue line (plus newline) to a stream.
 *
 * Used for the always-visible informational output (batch summaries, dry-run
 * previews). These are not errors or warnings, and LOG_INFO is suppressed at the
 * default client log level, so they go to the output stream like qconf's other
 * normal output.
 *
 * @param os  the stream to write to
 * @param fmt a MSG_* catalogue (printf-style) format string
 * @param ... arguments for @p fmt
 * @return none
 */
static void
qconf_info(std::ostream &os, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void
qconf_info(std::ostream &os, const char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   dstring buffer = DSTRING_INIT;
   sge_dstring_vsprintf(&buffer, fmt, ap);
   va_end(ap);
   const char *text = sge_dstring_get_string(&buffer);
   if (text != nullptr) {
      os << text;
   }
   sge_dstring_free(&buffer);
   os << "\n";
}

/**
 * @brief Ask the user to confirm a prompt on the terminal (CS-2299 H6).
 *
 * @param prompt a MSG_* line to print, including its own "(y/n)" suffix
 * @return true on an explicit yes; false on no, EOF or a non-tty stdin
 *
 * @note Deliberately still on stdout/stdin rather than on the output stream
 *       (CS-1841): a prompt and its answer belong to a terminal, and there is
 *       nothing to ask once this runs inside qmaster.
 */
static bool
qconf_confirm(const char *prompt) {
   printf("%s", prompt);
   fflush(stdout);
   char buf[16];
   if (fgets(buf, sizeof(buf), stdin) == nullptr) {
      printf("\n");
      return false;
   }
   return (buf[0] == 'y' || buf[0] == 'Y');
}

/**
 * @brief Test whether an object of a given name already exists in qmaster.
 *
 * @param target  GDI target list to query (e.g. CAL_LIST)
 * @param descr   CULL descriptor of the object type
 * @param name_nm name attribute of the object (e.g. CAL_name)
 * @param name    object name to look up
 * @return true if an object of that name exists, false otherwise
 */
static bool
qconf_object_exists(ocs::gdi::Target target, const lDescr *descr, int name_nm,
                    const char *name) {
   lList *lp = nullptr;
   /* CS-2304: host fields need a host comparison (%Ih), not a string one. */
   lCondition *where = qconf_name_is_host(descr, name_nm)
      ? lWhere("%T(%Ih=%s)", descr, name_nm, name)
      : lWhere("%T(%I==%s)", descr, name_nm, name);
   lEnumeration *what = lWhat("%T(%I)", descr, name_nm);
   lList *alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);
   bool exists = (lp != nullptr && lGetNumberOfElem(lp) > 0);
   lFreeList(&lp);
   lFreeList(&alp);
   return exists;
}

/**
 * @brief GET the names of every object of a type in one request (CS-2315).
 *
 * The batched apply path uses a single bulk name query to decide ADD vs MOD for
 * a whole directory, instead of one existence GET per object. The returned list
 * holds name-only elements (the @p name_nm field); the caller owns it.
 *
 * @param target  GDI target list (e.g. CAL_LIST)
 * @param descr   CULL descriptor of the object type
 * @param name_nm name attribute of the object
 * @return a (possibly empty) list of name-only elements, or nullptr on error
 */
static lList *
qconf_get_name_list(ocs::gdi::Target target, const lDescr *descr, int name_nm) {
   lList *lp = nullptr;
   lEnumeration *what = lWhat("%T(%I)", descr, name_nm);
   lList *alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
   lFreeWhat(&what);
   lFreeList(&alp);
   return lp;
}

/**
 * @brief Count the non-OK answers in an answer list (CS-2315).
 *
 * qmaster appends one answer per processed element, so the number of error
 * answers is the number of objects that failed in a batched ADD/MOD.
 *
 * @param alp answer list returned by a (multi-)request
 * @return the number of answers whose status is not STATUS_OK
 */
static int
qconf_count_failures(const lList *alp) {
   int n = 0;
   const lListElem *aep;
   for_each_ep(aep, alp) {
      if (lGetUlong(aep, AN_quality) == ANSWER_QUALITY_END) {
         continue;
      }
      if (lGetUlong(aep, AN_status) != STATUS_OK) {
         n++;
      }
   }
   return n;
}

/**
 * @brief Send an object to qmaster with upsert semantics (CS-2298 C2).
 *
 * Issues a MOD when an object of the same name already exists, otherwise an ADD.
 * Honours the -dry flag (reports the action and sends nothing). Consumes @p ep
 * in every case.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param target  GDI target list (e.g. CAL_LIST)
 * @param descr   CULL descriptor of the object type
 * @param name_nm name attribute of the object
 * @param ep      the object element to send; consumed (freed) by this call
 * @param sub_cmd GDI sub-command (defaults to NONE; cqueue uses SET_ALL - CS-2305)
 * @return 0 on success, non-zero on error
 */
static int
qconf_send_upsert(std::ostream &os, ocs::gdi::Target target, const lDescr *descr, int name_nm,
                  lListElem *ep, ocs::gdi::SubCommand sub_cmd = ocs::gdi::SubCommand::NONE) {
   const char *name = qconf_get_name(ep, name_nm);
   bool exists = (name != nullptr && qconf_object_exists(target, descr, name_nm, name));

   if (qconf_opt_dry_run) {
      /* CS-2299 H3: report the intended action without contacting qmaster. */
      qconf_info(os, exists ? MSG_QCONF_DRYRUNWOULDMODIFY_S : MSG_QCONF_DRYRUNWOULDADD_S,
                        name != nullptr ? name : "");
      lFreeElem(&ep);
      return 0;
   }

   ocs::gdi::Command cmd = exists ? ocs::gdi::Command::MOD : ocs::gdi::Command::ADD;

   lList *lp = lCreateList("qconf upsert", descr);
   lAppendElem(lp, ep);
   lList *alp = ocs::gdi::Client::sge_gdi(target, cmd, sub_cmd,
                                          &lp, nullptr, nullptr);
   int ret = show_answer_list(alp);
   lFreeList(&alp);
   lFreeList(&lp);
   return ret;
}

/**
 * @brief Read and validate a single object from an ASCII flatfile.
 *
 * A @p filename of "-" reads from stdin (CS-2299 H4). On any read or
 * unprocessed-field error the answer list is shown and nullptr is returned. When
 * @p validate is non-null it is run on the parsed object and a non-STATUS_OK
 * result is treated the same way (so -strict catches semantic errors during the
 * validation pass, before anything is sent).
 *
 * @param descr    CULL descriptor of the object type
 * @param fields   spooling field list describing the file format
 * @param filename path to read, or "-" for stdin
 * @param validate optional object validator returning STATUS_OK on success, or nullptr
 * @param sfi      flatfile spooling instruction (defaults to qconf_sfi; complex
 *                 entries use qconf_ce_sfi - CS-2303)
 * @return the parsed object (caller owns) or nullptr on error
 */
static lListElem *
qconf_read_object_file(const lDescr *descr, spooling_field *fields,
                       const char *filename,
                       int (*validate)(const lListElem *, lList **),
                       const spool_flatfile_instr *sfi = &qconf_sfi,
                       int (*prepare)(lListElem *, lList **) = nullptr,
                       bool parse_values = true) {
   /* The chain itself moved to qconf_parse_object() in the procedure library,
    * because qmaster needs the same one for a write procedure and two copies
    * would drift. What stays here is the client's half of it: the format comes
    * from this process's -fmt, and the messages are printed rather than carried
    * back to a caller. */
   lList *alp = nullptr;
   lListElem *ep = qconf_parse_object(&alp, descr, fields, qconf_opt_format, filename, nullptr,
                                      validate, sfi, prepare, parse_values);
   answer_list_output(&alp);
   return ep;
}

/**
 * @brief Apply a callback to a file, or to every file in a directory (CS-2298 C3/C5).
 *
 * When @p path is a regular file, @p per_file is invoked once for it. When it is
 * a directory, @p per_file is invoked for every non-hidden regular file inside
 * it (nested directories are skipped).
 *
 * @param path     file or directory to process
 * @param per_file callback invoked as per_file(filepath, ctx), returning a failure count
 * @param ctx      opaque context passed through to @p per_file
 * @return the total number of failures (an opendir error counts as one)
 */
static int
qconf_for_each_file(const char *path, int (*per_file)(const char *, void *), void *ctx) {
   if (!sge_is_directory(path)) {
      return per_file(path, ctx);
   }

   DIR *dir = opendir(path);
   if (dir == nullptr) {
      ERROR(MSG_QCONF_CANTOPENDIRECTORY_SS, path, strerror(errno));
      return 1;
   }
   int failures = 0;
   struct dirent *de;
   while ((de = readdir(dir)) != nullptr) {
      if (de->d_name[0] == '.') {
         continue;   /* skip ., .. and hidden files */
      }
      dstring full = DSTRING_INIT;
      sge_dstring_sprintf(&full, "%s/%s", path, de->d_name);
      const char *fp = sge_dstring_get_string(&full);
      if (!sge_is_directory(fp)) {   /* skip nested directories */
         failures += per_file(fp, ctx);
      }
      sge_dstring_free(&full);
   }
   closedir(dir);
   return failures;
}

/** @brief Context shared by the per-file callbacks driven by qconf_for_each_file(). */

struct qconf_file_ctx {
   std::ostream *os;               ///< where the per-file messages go (CS-1841)
   const qconf_object_desc *desc;  ///< everything that identifies the object type (CS-2795)
   spooling_field *fields;    ///< spooling field list, resolved from the descriptor's provider
   bool parse_values;         ///< spool_flatfile_read_object parse_values flag; the delete path
                              ///< takes it from the descriptor, the apply path always parses
   lList *names;              ///< delete path: collected name elements
   lList *elems;              ///< -strict apply: parsed objects pending send (CS-2299 H2)
   int n_ok;                  ///< files applied / names collected (CS-2299 H1)
   int n_fail;                ///< files that failed (CS-2299 H1)
};

/**
 * @brief Per-file callback: read one object and upsert it (CS-2298 C3).
 *
 * Updates the n_ok / n_fail counters in the context.
 *
 * @note Superseded by the batched qconf_apply_path() (CS-2315), which collects
 *       all objects and sends them in one GDI multi-request rather than one
 *       send per file. Kept for reference / possible reuse.
 *
 * @param filepath path of the object file to apply
 * @param vctx     pointer to the qconf_file_ctx for this batch
 * @return 0 on success, 1 on failure
 */
[[maybe_unused]] static int
qconf_apply_one(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_file_ctx *>(vctx);
   lListElem *ep = qconf_read_object_file(ctx->desc->descr, ctx->fields, filepath, ctx->desc->validate, ctx->desc->sfi, ctx->desc->prepare, ctx->parse_values);
   if (ep == nullptr) {
      ctx->n_fail++;
      return 1;
   }
   if (qconf_send_upsert(*ctx->os, ctx->desc->target, ctx->desc->descr, ctx->desc->name_nm, ep, ctx->desc->sub_cmd) != 0) {
      ctx->n_fail++;
      return 1;
   }
   ctx->n_ok++;
   return 0;
}

/**
 * @brief Per-file callback: read one object and queue its name for deletion (CS-2298 C5).
 *
 * A name that no longer exists is reported and skipped rather than queued
 * (CS-2299 H5). Updates the n_ok / n_fail counters in the context.
 *
 * @param filepath path of the object file to read
 * @param vctx     pointer to the qconf_file_ctx for this batch
 * @return 0 on success (including a skipped non-existent name), 1 on read failure
 */
static int
qconf_collect_name(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_file_ctx *>(vctx);
   /* deletion only needs the name, so no object validator is applied here */
   lListElem *ep = qconf_read_object_file(ctx->desc->descr, ctx->fields, filepath, nullptr, ctx->desc->sfi, ctx->desc->prepare, ctx->parse_values);
   if (ep == nullptr) {
      ctx->n_fail++;
      return 1;
   }
   const char *name = qconf_get_name(ep, ctx->desc->name_nm);
   if (name != nullptr) {
      /* CS-2299 H5: deleting is idempotent — a name that no longer exists is a
       * warning, not a batch failure, and is dropped from the delete request. */
      if (qconf_object_exists(ctx->desc->target, ctx->desc->descr, ctx->desc->name_nm, name)) {
         /* CS-2304: host fields must be added with lAddElemHost, not lAddElemStr. */
         if (qconf_name_is_host(ctx->desc->descr, ctx->desc->name_nm)) {
            lAddElemHost(&ctx->names, ctx->desc->name_nm, name, ctx->desc->descr);
         } else {
            lAddElemStr(&ctx->names, ctx->desc->name_nm, name, ctx->desc->descr);
         }
         ctx->n_ok++;
      } else {
         WARNING(MSG_QCONF_DELSKIPPEDNOTEXIST_SS, name, filepath);
      }
   }
   lFreeElem(&ep);
   return 0;
}

/**
 * @brief Per-file callback: read one object and queue it for the -strict apply pass (CS-2299 H2).
 *
 * The parsed element is appended to ctx->elems for sending later, once the whole
 * batch has been validated. Updates the n_fail counter on a read error.
 *
 * @param filepath path of the object file to read
 * @param vctx     pointer to the qconf_file_ctx for this batch
 * @return 0 on success, 1 on read failure
 */
static int
qconf_collect_elem(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_file_ctx *>(vctx);
   lListElem *ep = qconf_read_object_file(ctx->desc->descr, ctx->fields, filepath, ctx->desc->validate, ctx->desc->sfi, ctx->desc->prepare, ctx->parse_values);
   if (ep == nullptr) {
      ctx->n_fail++;
      return 1;
   }
   lAppendElem(ctx->elems, ep);
   return 0;
}

/** @brief Read a whole file into a string, see the definition below. */
static bool qconf_read_file_text(const char *filename, std::string &text);

/** @brief Context of the batch being assembled for the write procedure (CS-2822). */
struct qconf_text_ctx {
   ocs::QConfWriteParameter *param;   ///< the batch being filled
   int n_fail;                        ///< files this side could not read at all
};

/** @brief qconf_for_each_file() callback: hand one file's text to the batch
 *
 * The file is not parsed here. That is the whole point of the write procedure:
 * only qmaster knows what it masked when it rendered the object, so only qmaster
 * may read the text back - see ocs::QConfWriteParameter.
 *
 * What is still this side's business is the file itself, and a file that cannot
 * be opened is reported with the same message the flat file reader used to
 * report it with, because that is what the user has always seen.
 *
 * @param filepath the file to read, `-` for stdin
 * @param vctx the batch being filled
 * @return 0 when the text was added, 1 when the file could not be read
 */
static int
qconf_collect_text(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_text_ctx *>(vctx);

   std::string text;
   bool ok;
   if (strcmp(filepath, "-") == 0) {
      // CS-2299 H4: "-" reads the object from stdin
      ok = true;
      char chunk[4096];
      size_t n;
      while ((n = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
         text.append(chunk, n);
      }
      ok = (ferror(stdin) == 0);
   } else {
      ok = qconf_read_file_text(filepath, text);
   }

   if (!ok) {
      ERROR(MSG_ERROROPENINGFILEFORREADING_SS, filepath, strerror(errno));
      ctx->n_fail++;
      return 1;
   }

   ctx->param->add_item(filepath, text);
   return 0;
}

/** @brief Add or modify every object at a path through the write procedure (CS-2822)
 *
 * The successor of qconf_apply_path() for the object types that have a write
 * procedure. What is left here is what is genuinely the client's: walking the
 * path, reading the files, and the summary line that names the path. The parse,
 * the add against modify decision and the writes are qmaster's, in one request.
 *
 * @param os the stream the summary is written to (CS-1841)
 * @param o the object type
 * @param path file or directory to apply
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

/**
 * @brief Add or modify every object found at a path (file or directory) with
 *        upsert semantics (CS-2298 C2+C3).
 *
 * CS-2315: the whole batch contacts qmaster a constant number of times instead
 * of once per object. Every file is read (and prepared/validated) into one list;
 * a single bulk name GET decides ADD vs MOD per object; and the adds and mods are
 * sent as one GDI multi-request (one connection, qmaster processes each element
 * individually and returns all answers in one reply). So a directory of N objects
 * costs ~2 contacts (1 GET + 1 multi-request) rather than 2N.
 *
 * With the -strict flag the whole batch is validated before anything is sent and
 * nothing is applied if any file is invalid (CS-2299 H2). For a directory a
 * one-line summary is printed (CS-2299 H1). This is a client-side pre-apply gate,
 * not a server-side transaction: qmaster still commits each element separately, so
 * a partial failure can leave some objects applied.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param target   GDI target list (e.g. CAL_LIST)
 * @param descr    CULL descriptor of the object type
 * @param fields   spooling field list describing the file format
 * @param name_nm  name attribute of the object
 * @param path     file or directory to apply
 * @param validate optional per-object validator returning STATUS_OK on success, or nullptr
 * @param sfi      flatfile spooling instruction (defaults to qconf_sfi - CS-2303)
 * @param prepare  optional transform run on each object after read (e.g. resolve
 *                 a hostname), or nullptr - CS-2304
 * @param sub_cmd  GDI sub-command for ADD/MOD (defaults to NONE; cqueue SET_ALL - CS-2305)
 * @param parse_values spool read parse_values flag (defaults to true; cqueue false - CS-2305)
 * @return the number of objects that failed
 */
static int
qconf_apply_path(std::ostream &os, const qconf_object_desc &d, spooling_field *fields, const char *path) {
   const lDescr *descr = d.descr;
   const int name_nm = d.name_nm;
   bool is_dir = sge_is_directory(path);

   /* 1. Read every file (running prepare/validate per object) into one list.
    *
    * Whether attribute values are parsed is the object type's business, not the
    * path's: cluster queues are read with it off (CS-2305), because their values
    * carry the host-specific syntax that the parse would eat. Hard-coding true
    * here was a regression of CS-2795 - it rejected every queue file whose value
    * uses a backslash continuation, since the continuation then swallowed the
    * following attribute line. */
   qconf_file_ctx ctx = {&os, &d, fields, d.parse_values, nullptr,
                         lCreateList("qconf apply", descr), 0, 0};
   qconf_for_each_file(path, qconf_collect_elem, &ctx);
   int n_read_fail = ctx.n_fail;   /* files that did not parse (skipped) */

   /* CS-2299 H2 (-strict): a single malformed file rejects the whole batch. */
   if (qconf_opt_strict && n_read_fail > 0) {
      ERROR(MSG_QCONF_STRICTNOTHINGAPPLIED_SI, path, n_read_fail);
      lFreeList(&ctx.elems);
      return n_read_fail;
   }

   /* 2. One bulk name GET, then partition the parsed objects into add vs mod. */
   lList *existing = qconf_get_name_list(d.target, descr, name_nm);
   lList *add_list = lCreateList("qconf add", descr);
   lList *mod_list = lCreateList("qconf mod", descr);
   int n_dry = 0;
   lListElem *e;
   while ((e = lFirstRW(ctx.elems)) != nullptr) {
      lDechainElem(ctx.elems, e);
      const char *name = qconf_get_name(e, name_nm);
      bool exists = qconf_name_in_list(existing, descr, name_nm, name);
      if (qconf_opt_dry_run) {
         /* CS-2299 H3: report the intended action, contact no one. */
         qconf_info(os, exists ? MSG_QCONF_DRYRUNWOULDMODIFY_S : MSG_QCONF_DRYRUNWOULDADD_S,
                           name != nullptr ? name : "");
         lFreeElem(&e);
         n_dry++;
      } else {
         lAppendElem(exists ? mod_list : add_list, e);
      }
   }
   lFreeList(&existing);
   lFreeList(&ctx.elems);

   if (qconf_opt_dry_run) {
      if (is_dir) {
         qconf_info(os, MSG_QCONF_ADDMODSUMMARY_SII, path, n_dry, n_read_fail);
      }
      lFreeList(&add_list);
      lFreeList(&mod_list);
      return n_read_fail;
   }

   /* 3. Send the mods and adds as one GDI multi-request (one connection). The
    * lists are consumed by request() (do_copy = false steals the pointer). */
   int n_mod = lGetNumberOfElem(mod_list);
   int n_add = lGetNumberOfElem(add_list);
   int n_send = n_mod + n_add;
   int remaining = (n_mod > 0 ? 1 : 0) + (n_add > 0 ? 1 : 0);
   ocs::gdi::Request gdi_multi{};
   lList *send_alp = nullptr;
   int mod_id = -1, add_id = -1;

   if (n_mod > 0) {
      ocs::gdi::Mode mode = (--remaining > 0) ? ocs::gdi::Mode::RECORD : ocs::gdi::Mode::SEND;
      mod_id = gdi_multi.request(&send_alp, mode, d.target, ocs::gdi::Command::MOD, d.sub_cmd,
                                 &mod_list, nullptr, nullptr, false);
      if (mode == ocs::gdi::Mode::SEND) {
         gdi_multi.wait();
      }
   }
   if (n_add > 0) {
      ocs::gdi::Mode mode = (--remaining > 0) ? ocs::gdi::Mode::RECORD : ocs::gdi::Mode::SEND;
      add_id = gdi_multi.request(&send_alp, mode, d.target, ocs::gdi::Command::ADD, d.sub_cmd,
                                 &add_list, nullptr, nullptr, false);
      if (mode == ocs::gdi::Mode::SEND) {
         gdi_multi.wait();
      }
   }

   /* 4. Collect every per-element answer into one list, report it, count failures. */
   lList *result_alp = nullptr;
   if (n_mod > 0) {
      lList *resp = nullptr;
      gdi_multi.get_response(&resp, ocs::gdi::Command::MOD, d.sub_cmd, d.target, mod_id, nullptr);
      answer_list_append_list(&result_alp, &resp);
   }
   if (n_add > 0) {
      lList *resp = nullptr;
      gdi_multi.get_response(&resp, ocs::gdi::Command::ADD, d.sub_cmd, d.target, add_id, nullptr);
      answer_list_append_list(&result_alp, &resp);
   }
   answer_list_append_list(&result_alp, &send_alp);

   int n_send_fail = qconf_count_failures(result_alp);
   if (n_send_fail > n_send) {
      /* a transport-level failure can yield more answers than elements */
      n_send_fail = n_send;
   }
   show_answer_list(result_alp);
   lFreeList(&result_alp);
   lFreeList(&add_list);   /* no-op: consumed by request() */
   lFreeList(&mod_list);

   int n_ok = n_send - n_send_fail;
   int n_fail = n_read_fail + n_send_fail;
   if (is_dir) {
      /* CS-2299 H1: one summary line per directory batch. */
      qconf_info(os, MSG_QCONF_ADDMODSUMMARY_SII, path, n_ok, n_fail);
   }
   return n_fail;
}

/**
 * @brief Delete every object named in the file(s) at a path (file or directory) (CS-2298 C5).
 *
 * The name attribute is read from each file and the rest of the content is
 * ignored. A directory expands to a bulk delete: it is confirmed interactively
 * unless -f is given (CS-2299 H6), can be previewed with -dry (CS-2299 H3), and a
 * name that no longer exists is skipped rather than failed (CS-2299 H5).
 *
 * @param os the stream the output is written to (CS-1841)
 * @param target  GDI target list (e.g. CAL_LIST)
 * @param descr   CULL descriptor of the object type
 * @param fields  spooling field list describing the file format
 * @param name_nm name attribute of the object
 * @param path    file or directory naming the objects to delete
 * @param sfi     flatfile spooling instruction (defaults to qconf_sfi - CS-2303)
 * @param prepare optional transform run on each object after read (e.g. resolve
 *                a hostname), or nullptr - CS-2304
 * @return the number of objects that failed to be deleted
 */
static int
qconf_delete_path(std::ostream &os, const qconf_object_desc &d, spooling_field *fields, const char *path) {
   const lDescr *descr = d.descr;
   const int name_nm = d.name_nm;
   qconf_file_ctx ctx = {&os, &d, fields, d.parse_values,
                         lCreateList("qconf del", descr), nullptr, 0, 0};
   bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_collect_name, &ctx);

   if (is_dir) {
      /* CS-2299 H1: report the read phase before the batch delete. */
      qconf_info(os, MSG_QCONF_DELCOLLECTSUMMARY_SII, path, ctx.n_ok, ctx.n_fail);
   }

   int n_names = lGetNumberOfElem(ctx.names);
   if (n_names > 0 && qconf_opt_dry_run) {
      /* CS-2299 H3: list what would be deleted, contact no one. */
      const lListElem *nep;
      for_each_ep(nep, ctx.names) {
         qconf_info(os, MSG_QCONF_DRYRUNWOULDDELETE_S, qconf_get_name(nep, name_nm));
      }
      lFreeList(&ctx.names);
      return ctx.n_fail;
   }
   if (n_names > 0 && is_dir && !qconf_opt_force) {
      /* CS-2299 H6: a directory expands to a bulk delete - confirm first. */
      dstring p = DSTRING_INIT;
      sge_dstring_sprintf(&p, MSG_QCONF_CONFIRMDELETE_I, n_names);
      bool ok = qconf_confirm(sge_dstring_get_string(&p));
      sge_dstring_free(&p);
      if (!ok) {
         WARNING(MSG_QCONF_DELETEABORTED_I, n_names);
         lFreeList(&ctx.names);
         return ctx.n_fail;
      }
   }
   if (n_names > 0) {
      lList *alp = ocs::gdi::Client::sge_gdi(d.target, ocs::gdi::Command::DEL,
                                             ocs::gdi::SubCommand::NONE, &ctx.names,
                                             nullptr, nullptr);
      ctx.n_fail += show_answer_list(alp);
      lFreeList(&alp);
   }
   lFreeList(&ctx.names);
   return ctx.n_fail;
}

/* ================= CS-23xx: bulk export ( qconf -S<obj> name|dir ) =================
 *
 * The inverse of qconf_apply_path(): pull objects from qmaster with a read-only GDI
 * GET and write each to its own re-importable flatfile. Each file is written with the
 * same IMPORT field list that -A<obj>/-M<obj> read, so an exported file round-trips
 * by construction (KTD2). The argument disambiguates like the import side: an existing
 * directory or a trailing '/' selects directory mode (one file per object, named after
 * it); otherwise it is a single object name written to ./<name>.
 */

/**
 * @brief True if @p name is safe to use verbatim as an export file name (R5/KTD7).
 *
 * Rejects empty, ".", "..", a name containing '/', and a name beginning with '-'
 * (which would be reparsed as an option on re-import). Never sanitizes - an unsafe
 * name is reported as an error for that object.
 */
static bool
qconf_name_is_fs_safe(const char *name) {
   if (name == nullptr || name[0] == '\0') {
      return false;
   }
   if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      return false;
   }
   if (name[0] == '-') {
      return false;
   }
   if (strchr(name, '/') != nullptr) {
      return false;
   }
   return true;
}

/** @brief Provider of an object type's IMPORT field list for export (KTD2/KTD8).
 *
 *  Returns the field list to write with and sets *owned to true when the engine must
 *  free it afterwards (a dynamically built list, e.g. exechost/project/user) or false
 *  for a shared static table (e.g. CQ_fields). The format lets sharetree pick its
 *  JSON vs ASCII field list. */

/**
 * @brief Write one object element to @p dest as a re-importable flatfile (KTD4).
 *
 * Honors -f: an existing destination is skipped with a warning (non-fatal) unless
 * force is set, in which case it is unlinked first because COPY mode does not
 * truncate. The content is written to a tmp file (the only path-less flatfile
 * destination) then relocated with rename(), falling back to
 * sge_copy_append(src, dst, COPY) + unlink(src) across filesystems.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param as_list  write the element wrapped in a one-element list (rqs rule-set
 *                 framing) instead of as a bare object.
 * @param is_root  spool_flatfile_write_object is_root flag (sharetree writes the
 *                 root node; ignored when @p as_list is set).
 * @return 0 on success or a (non-fatal) skip, 1 on a write/relocate failure.
 */
static int
qconf_export_write_one(std::ostream &os, const lListElem *ep, const spooling_field *fields,
                       const spool_flatfile_instr *sfi, const char *json_type_name,
                       bool as_list, bool is_root, const char *dest) {
   if (sge_is_file(dest)) {
      if (!qconf_opt_force) {
         WARNING(MSG_QCONF_EXPORTSKIPPEDEXISTS_S, dest);
         return 0;   /* -f overwrites (R4/KTD6); without it this is a non-fatal skip */
      }
      unlink(dest);   /* COPY fallback does not O_TRUNC (KTD4) */
   }

   lList *alp = nullptr;
   char *tmp;
   if (as_list) {
      lList *one = lCreateList("export", lGetElemDescr(ep));
      /* build a throwaway single-element list around a copy of ep */
      lAppendElem(one, lCopyElem(ep));
      tmp = (char *)spool_flatfile_write_list(&alp, one, fields, sfi, SP_DEST_TMP,
                                              qconf_opt_format, nullptr, false);
      lFreeList(&one);
   } else {
      tmp = (char *)spool_flatfile_write_object(&alp, ep, is_root, fields, sfi,
                                                SP_DEST_TMP, qconf_opt_format,
                                                nullptr, false, json_type_name);
   }
   if (tmp == nullptr || answer_list_output(&alp)) {
      ERROR(MSG_QCONF_EXPORTWRITEFAILED_S, dest);
      if (tmp != nullptr) {
         unlink(tmp);
         sge_free(&tmp);
      }
      lFreeList(&alp);
      return 1;
   }
   lFreeList(&alp);

   int ret = 0;
   if (rename(tmp, dest) != 0) {
      /* cross-filesystem rename fails with EXDEV: copy then drop the tmp source.
       * sge_copy_append() takes (src, dst, mode) - src first (KTD4). */
      if (sge_copy_append(tmp, dest, SGE_MODE_COPY) != 0) {
         ERROR(MSG_QCONF_EXPORTWRITEFAILED_S, dest);
         ret = 1;
      }
      unlink(tmp);
   }
   sge_free(&tmp);
   if (ret == 0) {
      qconf_info(os, MSG_QCONF_EXPORTWROTE_S, dest);
   }
   return ret;
}

/**
 * @brief Build the destination path for one exported object (R3).
 *
 * @param dir    directory in bulk mode, or nullptr in single mode (cwd).
 * @param name   object name (already FS-safe checked).
 * @param suffix ".json" under -fmt json, "" otherwise.
 */
static void
qconf_export_dest(dstring *dest, const char *dir, const char *name, const char *suffix) {
   sge_dstring_clear(dest);
   if (dir != nullptr) {
      sge_dstring_sprintf(dest, "%s/%s%s", dir, name, suffix);
   } else {
      sge_dstring_sprintf(dest, "%s%s", name, suffix);
   }
}

/**
 * @brief Export objects of one type to a file or a directory of files (R1-R5,R8).
 *
 * Directory mode (arg is an existing directory or ends with '/') exports every
 * object of the type, one file each, creating the directory if missing. Single mode
 * exports the one object named by @p path to ./<name>. The write uses @p build_fields
 * (the import field list), and per-object failures are counted and reported but do
 * not abort the run (continue-and-report, KTD9).
 *
 * @param os the stream the output is written to (CS-1841)
 * @param target       GDI target list (e.g. CQ_LIST)
 * @param descr        CULL descriptor of the object type
 * @param name_nm      name attribute of the object
 * @param build_fields import-field-list provider (KTD8)
 * @param sfi          flatfile spooling instruction
 * @param json_type_name explicit JSON $id type, or nullptr to derive from content
 * @param as_list      rqs-style list framing (see qconf_export_write_one)
 * @param path         the name|dir argument
 * @param exclude1,2   bulk: object names to exclude from the GET (template/global)
 * @return the number of objects that failed to export
 */
static int
qconf_export_path(std::ostream &os, ocs::gdi::Target target, const lDescr *descr, int name_nm,
                  qconf_build_fields_fn build_fields,
                  const spool_flatfile_instr *sfi, const char *json_type_name,
                  bool as_list, const char *path,
                  const char *exclude1 = nullptr, const char *exclude2 = nullptr) {
   const char *suffix = (qconf_opt_format == SP_FORM_JSON) ? ".json" : "";
   size_t plen = strlen(path);
   bool is_bulk = sge_is_directory(path) || (plen > 0 && path[plen - 1] == '/');

   bool owned = false;
   spooling_field *fields = build_fields(qconf_opt_format, &owned);

   int failures = 0;

   if (is_bulk) {
      /* normalize the directory string (drop a single trailing '/') */
      char dir[4096];
      sge_strlcpy(dir, path, sizeof(dir));
      size_t dl = strlen(dir);
      if (dl > 1 && dir[dl - 1] == '/') {
         dir[dl - 1] = '\0';
      }

      if (!sge_is_directory(dir)) {
         /* KTD3: 4-arg, non-recursive, exit_on_error=false */
         if (sge_mkdir(dir, 0755, false, false) != 0 || !sge_is_directory(dir)) {
            ERROR(MSG_QCONF_EXPORTCANTMKDIR_S, dir);
            if (owned) { sge_free(&fields); }
            return 1;
         }
      }

      /* KTD5: GET all, excluding template/global in the where clause. */
      lCondition *where = nullptr;
      if (exclude1 != nullptr && exclude2 != nullptr) {
         where = lWhere("%T(!(%Ic=%s || %Ic=%s))", descr, name_nm, exclude1, name_nm, exclude2);
      } else if (exclude1 != nullptr) {
         where = lWhere("%T(!(%Ic=%s))", descr, name_nm, exclude1);
      }
      lEnumeration *what = lWhat("%T(ALL)", descr);
      lList *lp = nullptr;
      lList *alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::GET,
                                             ocs::gdi::SubCommand::NONE, &lp, where, what);
      lFreeWhere(&where);
      lFreeWhat(&what);
      if (show_answer_list(alp) != 0) {
         failures++;
      }
      lFreeList(&alp);

      /* R5/KTD7: case-insensitive basename collision guard across the run. The
       * seen list holds the lowercased names already used. */
      lList *seen = lCreateList("export seen", ST_Type);
      char lcbuf[1024];
      dstring dest = DSTRING_INIT;
      int n_total = lGetNumberOfElem(lp);
      lListElem *ep;
      for_each_rw (ep, lp) {
         const char *name = qconf_get_name(ep, name_nm);
         if (!qconf_name_is_fs_safe(name)) {
            ERROR(MSG_QCONF_EXPORTUNSAFENAME_S, name != nullptr ? name : "");
            failures++;
            continue;
         }
         sge_strlcpy(lcbuf, name, sizeof(lcbuf));
         sge_strtolower(lcbuf, sizeof(lcbuf));
         if (lGetElemStr(seen, ST_name, lcbuf) != nullptr) {
            ERROR(MSG_QCONF_EXPORTCOLLISION_SS, name, lcbuf);
            failures++;
            continue;
         }
         lAddElemStr(&seen, ST_name, lcbuf, ST_Type);

         qconf_export_dest(&dest, dir, name, suffix);
         failures += qconf_export_write_one(os, ep, fields, sfi, json_type_name, as_list,
                                            false, sge_dstring_get_string(&dest));
      }
      sge_dstring_free(&dest);
      lFreeList(&seen);
      lFreeList(&lp);

      qconf_info(os, MSG_QCONF_EXPORTSUMMARY_SII, dir, n_total - failures, failures);
   } else {
      /* single object named by path -> ./<name>[.json] */
      const char *name = path;
      if (!qconf_name_is_fs_safe(name)) {
         ERROR(MSG_QCONF_EXPORTUNSAFENAME_S, name);
         if (owned) { sge_free(&fields); }
         return 1;
      }
      lCondition *where = qconf_name_is_host(descr, name_nm)
         ? lWhere("%T(%Ih=%s)", descr, name_nm, name)
         : lWhere("%T(%I==%s)", descr, name_nm, name);
      lEnumeration *what = lWhat("%T(ALL)", descr);
      lList *lp = nullptr;
      lList *alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::GET,
                                             ocs::gdi::SubCommand::NONE, &lp, where, what);
      lFreeWhere(&where);
      lFreeWhat(&what);
      if (show_answer_list(alp) != 0) {
         failures++;
      }
      lFreeList(&alp);

      lListElem *ep = lFirstRW(lp);
      if (ep == nullptr) {
         ERROR(MSG_QCONF_EXPORTNOOBJECT_S, name);
         failures++;
      } else {
         dstring dest = DSTRING_INIT;
         qconf_export_dest(&dest, nullptr, name, suffix);
         failures += qconf_export_write_one(os, ep, fields, sfi, json_type_name, as_list,
                                            false, sge_dstring_get_string(&dest));
         sge_dstring_free(&dest);
      }
      lFreeList(&lp);
   }

   if (owned) {
      sge_free(&fields);
   }
   return failures;
}

/**
 * @brief Export a true singleton (sharetree, sched_conf) to a single file (R6).
 *
 * A directory argument is rejected - singletons have no directory-of-many form.
 * The one object is fetched (GET all, take the first) and written verbatim to the
 * given path.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param is_root spool_flatfile_write_object is_root flag (sharetree uses true).
 * @return the number of failures (0 on success).
 */
static int
qconf_export_singleton(std::ostream &os, ocs::gdi::Target target, const lDescr *descr,
                       qconf_build_fields_fn build_fields,
                       const spool_flatfile_instr *sfi, const char *json_type_name,
                       bool is_root, const char *path) {
   size_t plen = strlen(path);
   if (sge_is_directory(path) || (plen > 0 && path[plen - 1] == '/')) {
      ERROR(MSG_QCONF_EXPORTNODIRALLOWED_S, path);
      return 1;
   }

   lEnumeration *what = lWhat("%T(ALL)", descr);
   lList *lp = nullptr;
   lList *alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
   lFreeWhat(&what);
   int failures = 0;
   if (show_answer_list(alp) != 0) {
      failures++;
   }
   lFreeList(&alp);

   lListElem *ep = lFirstRW(lp);
   if (ep == nullptr) {
      ERROR(MSG_QCONF_EXPORTNOOBJECT_S, path);
      lFreeList(&lp);
      return failures + 1;
   }

   bool owned = false;
   spooling_field *fields = build_fields(qconf_opt_format, &owned);
   failures += qconf_export_write_one(os, ep, fields, sfi, json_type_name, false, is_root, path);
   if (owned) {
      sge_free(&fields);
   }
   lFreeList(&lp);
   return failures;
}


/**
 * @brief Bespoke config export (R7/KTD8): config keys on the host basename, not on
 *        a field, so it cannot use the generic engine.
 *
 * Bulk mode writes the global config (file `global`) plus every host's local config
 * (file = host), a complete re-importable set. Single mode writes one host's config
 * to a file named after it; `-Sconf global` writes the global config to `global`.
 * Unlike `-sconf host,host`, `-Sconf` takes a single name or a directory only.
 *
 * @param os the stream the output is written to (CS-1841)
 * @return the number of configs that failed to export
 */
static int
qconf_conf_export(std::ostream &os, const char *path) {
   const char *suffix = (qconf_opt_format == SP_FORM_JSON) ? ".json" : "";
   size_t plen = strlen(path);
   bool is_bulk = sge_is_directory(path) || (plen > 0 && path[plen - 1] == '/');

   spooling_field *fields = sge_build_CONF_field_list(false);
   int failures = 0;

   if (is_bulk) {
      char dir[4096];
      sge_strlcpy(dir, path, sizeof(dir));
      size_t dl = strlen(dir);
      if (dl > 1 && dir[dl - 1] == '/') {
         dir[dl - 1] = '\0';
      }
      if (!sge_is_directory(dir)) {
         if (sge_mkdir(dir, 0755, false, false) != 0 || !sge_is_directory(dir)) {
            ERROR(MSG_QCONF_EXPORTCANTMKDIR_S, dir);
            sge_free(&fields);
            return 1;
         }
      }

      lEnumeration *what = lWhat("%T(ALL)", CONF_Type);
      lList *lp = nullptr;
      lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::GET,
                                             ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
      lFreeWhat(&what);
      if (show_answer_list(alp) != 0) {
         failures++;
      }
      lFreeList(&alp);

      lList *seen = lCreateList("export seen", ST_Type);
      char lcbuf[1024];
      dstring dest = DSTRING_INIT;
      int n_total = lGetNumberOfElem(lp);
      lListElem *ep;
      for_each_rw (ep, lp) {
         const char *name = lGetHost(ep, CONF_name);
         if (!qconf_name_is_fs_safe(name)) {
            ERROR(MSG_QCONF_EXPORTUNSAFENAME_S, name != nullptr ? name : "");
            failures++;
            continue;
         }
         sge_strlcpy(lcbuf, name, sizeof(lcbuf));
         sge_strtolower(lcbuf, sizeof(lcbuf));
         if (lGetElemStr(seen, ST_name, lcbuf) != nullptr) {
            ERROR(MSG_QCONF_EXPORTCOLLISION_SS, name, lcbuf);
            failures++;
            continue;
         }
         lAddElemStr(&seen, ST_name, lcbuf, ST_Type);
         qconf_export_dest(&dest, dir, name, suffix);
         failures += qconf_export_write_one(os, ep, fields, &qconf_sfi, nullptr, false, false,
                                            sge_dstring_get_string(&dest));
      }
      sge_dstring_free(&dest);
      lFreeList(&seen);
      lFreeList(&lp);
      qconf_info(os, MSG_QCONF_EXPORTSUMMARY_SII, dir, n_total - failures, failures);
   } else {
      if (!qconf_name_is_fs_safe(path)) {
         ERROR(MSG_QCONF_EXPORTUNSAFENAME_S, path);
         sge_free(&fields);
         return 1;
      }
      /* "global" resolves to SGE_GLOBAL_NAME for the lookup but the file keeps the
       * user-given basename (which re-imports to the same host key). */
      const char *cfn = (strcasecmp(path, "global") == 0) ? SGE_GLOBAL_NAME : path;
      lCondition *where = lWhere("%T(%Ih=%s)", CONF_Type, CONF_name, cfn);
      lEnumeration *what = lWhat("%T(ALL)", CONF_Type);
      lList *lp = nullptr;
      lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::GET,
                                             ocs::gdi::SubCommand::NONE, &lp, where, what);
      lFreeWhere(&where);
      lFreeWhat(&what);
      if (show_answer_list(alp) != 0) {
         failures++;
      }
      lFreeList(&alp);

      lListElem *ep = lFirstRW(lp);
      if (ep == nullptr) {
         ERROR(MSG_QCONF_EXPORTNOOBJECT_S, path);
         failures++;
      } else {
         dstring dest = DSTRING_INIT;
         qconf_export_dest(&dest, nullptr, path, suffix);
         failures += qconf_export_write_one(os, ep, fields, &qconf_sfi, nullptr, false, false,
                                            sge_dstring_get_string(&dest));
         sge_dstring_free(&dest);
      }
      lFreeList(&lp);
   }

   sge_free(&fields);
   return failures;
}



/** @brief How a show of one object ended (CS-2796).
 *
 * The show options do not agree on what to do about a failure - the classic
 * four leave the parser when the object does not exist, -se and -sprj only set
 * the return code and -suser ignores it altogether. The engine therefore
 * reports what happened and leaves the reaction to the call site, which is
 * also what CS-2799 needs: no exit call inside the engine. */
enum class QConfShowStatus {
   OK,            ///< object found and written
   GDI_FAILED,    ///< the GDI request was answered with an error
   NOT_FOUND,     ///< the request succeeded but returned no object
   WRITE_FAILED   ///< the object could not be rendered
};

/** @brief Run one read operation as a stored procedure (CS-2799).
 *
 * The one place that decides between the two execution contexts. With
 * ExecContext::SERVER the generic model ships the operation to qmaster and
 * renders the response it sends back; with CLIENT the typed model fetches over
 * GDI and the format specific view renders here. Both write into @p os.
 *
 * @param os    the stream the output is written to
 * @param param the invocation, with its operation already set
 * @param alpp  answer list, carries the reason when the call fails
 * @return true when the operation was rendered
 */
static bool
qconf_dispatch_procedure(std::ostream &os, ocs::QConfParameterClient &param, lList **alpp,
                         uint32_t *reduction = nullptr) {
   DENTER(TOP_LAYER);

   /* CS-2822: the client context has no reduction channel at all - a plain GET
    * returns the whole object - so it answers "not reduced" and would be the
    * wrong basis for a decision that depends on it. The write path therefore
    * refuses that context rather than trusting this value; see
    * qconf_edit_obj_procedure(). */
   if (reduction != nullptr) {
      *reduction = ocs::ProcedureParameter::REDUCTION_NONE;
   }

   bool ok;
   if (param.get_exec_context() == ocs::ProcedureParameter::ExecContext::SERVER) {
      ocs::ProcedureModel model;
      ok = model.make_snapshot(alpp, param);

      /* CS-2821 opened this channel, CS-2822 is its first consumer: the write
       * path refuses to offer an object for editing that the caller can only
       * see in part. Read on every call, not only where it is wanted, so a
       * traced run shows that the entry survived the GDI round trip. */
      const uint32_t mask = model.get_reduction();
      DPRINTF("procedure reduction mask: " sge_u32 "\n", mask);
      if (reduction != nullptr) {
         *reduction = mask;
      }

      /* Rendered unconditionally since CS-2820: whatever qmaster produced is
       * printed even when it also reported a problem, because a procedure may
       * mean both - see the note in ProcedureModel::make_snapshot(). A read that
       * failed produced no text, so this prints nothing for it, and the caller
       * still sees "ok" as false and reports the error as before. */
      ocs::ProcedureView view(param);
      ocs::ProcedureController controller(os);
      controller.process_request(param, model, view);
   } else {
      ocs::QConfModelClient model;
      ok = model.make_snapshot(alpp, param);
      if (ok) {
         /* CS-2816: the client side mirror of the traits table. A sub-procedure
          * brings its own view and controller, so picking them by output format
          * alone is not enough any more. */
         const std::string &sub = param.get_sub_procedure_name();

         if (sub == ocs::QConfParameter::HGROUP_TREE) {
            ocs::QConfHGroupStructureView view(param);
            ocs::QConfHGroupTreeController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::HGROUP_RESOLVED) {
            ocs::QConfHGroupStructureView view(param);
            ocs::QConfHGroupResolvedController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::HGROUP_WHY) {
            ocs::QConfHGroupWhyView view(param);
            ocs::QConfHGroupWhyController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::CENTRY_LIST ||
                    sub == ocs::QConfParameter::CATEGORY_LIST) {
            ocs::QConfTableView view(param);
            ocs::QConfTableController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::QUEUE_SICK) {
            ocs::QConfSickView view(param);
            ocs::QConfSickController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::EVENT_CLIENT_LIST) {
            ocs::QConfEventClientView view(param);
            ocs::QConfEventClientController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::PROCESSOR_LIST) {
            ocs::QConfProcessorView view(param);
            ocs::QConfProcessorController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::THREAD_POOL_LIST) {
            ocs::QConfThreadPoolView view(param);
            ocs::QConfThreadPoolController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::SHARETREE_TREE) {
            ocs::QConfShareTreeView view(param);
            ocs::QConfShareTreeTreeController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::SHARETREE_NODES) {
            ocs::QConfShareTreeView view(param);
            ocs::QConfShareTreeNodesController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::OBJECT_FILTER) {
            ocs::QConfObjectFilterView view(param);
            ocs::QConfObjectFilterController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::CQUEUE_SHOW) {
            ocs::QConfCQueueShowView view(param);
            ocs::QConfCQueueShowController controller(os);
            controller.process_request(param, model, view);
         } else {
            std::unique_ptr<ocs::QConfViewBase> view;
            if (param.get_output_format() == ocs::ProcedureParameter::OutputFormat::JSON) {
               if (sub == ocs::QConfParameter::HGROUP_SHOW) {
                  view = std::make_unique<ocs::QConfHGroupShowViewJSON>(param);
               } else {
                  view = std::make_unique<ocs::QConfViewJSON>(param);
               }
            } else {
               view = std::make_unique<ocs::QConfViewPlain>(param);
            }

            if (sub == ocs::QConfParameter::HGROUP_SHOW) {
               ocs::QConfHGroupShowController controller(os);
               controller.process_request(param, model, *view);
            } else {
               ocs::QConfController controller(os);
               controller.process_request(param, model, *view);
            }
         }
      }
   }

   DRETURN(ok);
}

/** @brief Show one object of the given type on stdout.
 *
 * Fetches the object by name - or, for a type without a name field, the single
 * one that exists - and renders it in the layout of the descriptor.
 *
 * @param os    the stream the output is written to (CS-1841)
 * @param param the invocation, whose operation is set for the call
 * @param o    the object type
 * @param name the object name, ignored for a type without a name field
 * @param reduction where the reduction flags of the read are stored, nullptr to ignore them
 * @return what happened, see QConfShowStatus
 */
static QConfShowStatus
qconf_show_object(std::ostream &os, ocs::QConfParameterClient &param, QConfObj o, const char *name,
                  uint32_t *reduction = nullptr) {
   DENTER(TOP_LAYER);

   /* CS-2799: the body of this function is a stored procedure call now. The
    * signature and the status contract stay, because the option blocks above
    * disagree about what a failure means and each keeps its own reaction.
    *
    * The status is recovered from the answer list: the model reports a missing
    * object with STATUS_EEXIST, everything else is a fetch failure. That is the
    * only categorisation that survives the trip back from qmaster, and it is
    * the one the callers distinguish. */
   /* A type without a name field is read as a singleton, because that is the
    * only read it has: the scheduler configuration and the share tree are one
    * object each and no name selects them (CS-2823). Derived from the descriptor
    * rather than passed in, so a caller cannot ask for the kind the type does
    * not have. */
   const ocs::QConfReadKind kind = (qconf_desc(o).name_nm == NoName)
                                   ? ocs::QConfReadKind::SINGLETON
                                   : ocs::QConfReadKind::SINGLE_OBJECT;
   const char *switch_name = ocs::QConfModelBase::switch_for(o, kind);
   DPRINTF("showing object type %d through %s\n", static_cast<int>(o),
           switch_name != nullptr ? switch_name : "<none>");
   if (switch_name == nullptr) {
      DRETURN(QConfShowStatus::GDI_FAILED);
   }

   qconf_op op;
   op.token = switch_name;
   if (name != nullptr) {
      op.args.emplace_back(name);
   }
   param.set_operation(op);

   lList *alp = nullptr;
   const bool ok = qconf_dispatch_procedure(os, param, &alp, reduction);
   if (ok) {
      lFreeList(&alp);
      DRETURN(QConfShowStatus::OK);
   }

   const bool not_found = answer_list_has_status(&alp, STATUS_EEXIST);
   /* Every caller wants to see the reason. The editor path used to suppress a
    * missing object here, because it turned one into an upsert and the GET this
    * path replaced reported no match as an empty list rather than an error - a
    * difference that put an "x is not a calendar" in front of the editor
    * (CS-2822). Since CS-2823 the read answers such a name with the type's
    * template, so a missing object no longer reaches this point for a type that
    * creates one, and for a type that does not the complaint is the answer. */
   answer_list_output(&alp);
   lFreeList(&alp);
   DRETURN(not_found ? QConfShowStatus::NOT_FOUND : QConfShowStatus::GDI_FAILED);
}

/** @brief Map a show status onto the convention of the classic show options.
 *
 * -scal, -sckpt, -srole and -sp have always left the parser when the object
 * does not exist, printed the GDI error and carried on otherwise, and exited on
 * a rendering failure. The DRETURN cannot live in here, so the caller is told
 * to leave instead.
 *
 * @param st           the status the engine reported
 * @param parse_return the parser's return code, set on a GDI failure
 * @return true when the caller has to leave the parser with 1
 */
static bool
qconf_show_status_classic(QConfShowStatus st, int *parse_return) {
   switch (st) {
      case QConfShowStatus::OK:
         break;
      case QConfShowStatus::GDI_FAILED:
         *parse_return = 1;
         break;
      case QConfShowStatus::NOT_FOUND:
         return true;
      case QConfShowStatus::WRITE_FAILED:
         sge_error_and_exit(nullptr);
         break;
   }
   return false;
}

/** @brief Delete the objects named by an already parsed list.
 *
 * Split out of qconf_delete_names() because -dhgrp parses its argument with
 * parse_name_list_to_cull() rather than lString2List(), which reports an empty
 * argument instead of accepting it.
 *
 * @param o   the object type
 * @param lpp the name list; consumed, set to nullptr
 * @return 0 on success, 1 when the master reported an error
 */
static int
qconf_delete_list(QConfObj o, lList **lpp) {
   DENTER(TOP_LAYER);
   const qconf_object_desc &d = qconf_desc(o);

   lList *alp = ocs::gdi::Client::sge_gdi(d.target, ocs::gdi::Command::DEL, ocs::gdi::SubCommand::NONE,
                                          lpp, nullptr, nullptr);
   int ret = show_answer_list(alp);
   lFreeList(&alp);
   lFreeList(lpp);
   DRETURN(ret);
}

/** @brief Delete objects of the given type by name.
 *
 * @param o     the object type
 * @param names comma or blank separated list of names; ignored for a type
 *              without a name field, which holds exactly one object
 * @return 0 on success, 1 when the master reported an error
 */
static int
qconf_delete_names(QConfObj o, const char *names) {
   const qconf_object_desc &d = qconf_desc(o);
   lList *lp = nullptr;

   // a type without a name field is deleted as a whole, so there is nothing to name
   if (d.name_nm != NoName) {
      lString2List(names, &lp, const_cast<lDescr *>(d.descr), d.name_nm, ", ");
   }
   return qconf_delete_list(o, &lp);
}

/** @brief Add or modify from a file or directory, driven by the descriptor.
 *
 * The field list comes from the descriptor's provider, which allocates for the
 * object types whose list is built on demand (EH, PR, UU, STN) and hands back a
 * shared static one otherwise; @c owned says which, so this frees exactly what
 * it allocated.
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
 * @return 0 on success, non-zero on error
 */
static int
qconf_apply_obj(std::ostream &os, QConfObj o, const char *path, const char *select = nullptr,
                uint32_t extra_mode = 0u, const char *name = nullptr) {
   /* CS-2822: the object types whose write runs as a stored procedure take the
    * other path, which parses in qmaster instead of here. The list is here
    * rather than in a field of the descriptor table so that it disappears with
    * the last entry instead of becoming a permanent column. */
   switch (o) {
      case QConfObj::CAL:
      case QConfObj::CE:
      case QConfObj::CK:
      case QConfObj::EH:
      case QConfObj::PE:
      case QConfObj::PR:
      case QConfObj::RL:
      case QConfObj::RQS:
      case QConfObj::SC:
      case QConfObj::STN:
      case QConfObj::CONF:
      case QConfObj::UU:
         return qconf_apply_path_procedure(os, o, path, select, extra_mode, name);
      default:
         break;
   }

   const qconf_object_desc &d = qconf_desc(o);
   bool owned = false;
   spooling_field *fields = d.build_fields(qconf_opt_format, &owned);
   int ret = qconf_apply_path(os, d, fields, path);
   if (owned) {
      sge_free(&fields);
   }
   return ret;
}

/** @brief Delete the objects named by a file or directory, driven by the descriptor.
 * @param os the stream the output is written to (CS-1841)
 * @param o    the object type
 * @param path file or directory to read
 * @return 0 on success, non-zero on error
 */
static int
qconf_delete_obj(std::ostream &os, QConfObj o, const char *path) {
   const qconf_object_desc &d = qconf_desc(o);
   bool owned = false;
   spooling_field *fields = d.build_fields(qconf_opt_format, &owned);
   int ret = qconf_delete_path(os, d, fields, path);
   if (owned) {
      sge_free(&fields);
   }
   return ret;
}

/** @brief Export one object or a whole directory, driven by the descriptor.
 * @param os the stream the output is written to (CS-1841)
 * @param o    the object type
 * @param path object name, or a directory for the bulk export
 * @return the number of objects that failed to export
 */
static int
qconf_export_obj(std::ostream &os, QConfObj o, const char *path) {
   const qconf_object_desc &d = qconf_desc(o);
   return qconf_export_path(os, d.target, d.descr, d.name_nm, d.build_fields,
                            d.sfi, d.json_type_name, d.as_list, path,
                            d.exclude1, d.exclude2);
}

/** @brief Export a singleton object (share tree, scheduler configuration).
 * @param os the stream the output is written to (CS-1841)
 * @param o    the object type
 * @param path file to write
 * @return the number of objects that failed to export
 */
static int
qconf_export_obj_singleton(std::ostream &os, QConfObj o, const char *path) {
   const qconf_object_desc &d = qconf_desc(o);
   return qconf_export_singleton(os, d.target, d.descr, d.build_fields,
                                 d.sfi, d.json_type_name, d.is_root, path);
}

/** @brief Why an interactive edit did not yield an object. */
enum class QConfEditStatus {
   OK,            ///< the object was edited and parsed
   WRITE_FAILED,  ///< the template could not be written for the editor
   EDIT_FAILED,   ///< the editor itself reported an error
   UNCHANGED,     ///< the editor left the file untouched
   READ_FAILED    ///< the result did not parse, or left a field unprocessed
};

/** @brief Read a whole file into a string
 * @param filename the file to read
 * @param text receives the content, cleared first
 * @return true when the file was read without error
 */
static bool
qconf_read_file_text(const char *filename, std::string &text) {
   DENTER(TOP_LAYER);

   FILE *fp = fopen(filename, "r");
   if (fp == nullptr) {
      DRETURN(false);
   }
   text.clear();
   char chunk[4096];
   size_t n;
   while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
      text.append(chunk, n);
   }
   const bool ok = (ferror(fp) == 0);
   fclose(fp);

   DRETURN(ok);
}

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
 * @return OK, or the reason no text came back
 */
static QConfEditStatus
qconf_edit_text(std::string &text, uid_t uid, gid_t gid) {
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

   const int status = sge_edit(filename, uid, gid);
   if (status != 0) {
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
 * @return none
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
 * @param select for a type whose text holds a list, the names the read was asked for -
 *               the scope qmaster reports against; nullptr when it read everything
 * @param extra_mode write modes the switch adds on top of MODE_ROUND_TRIP, see
 *               qconf_apply_obj()
 * @return 0 when the object was written or the editor left it unchanged, 1 otherwise
 */
static int
qconf_edit_and_write_obj(QConfObj o, std::string &text, uid_t uid, gid_t gid,
                         const char *select = nullptr, uint32_t extra_mode = 0u,
                         const char *name = nullptr) {
   DENTER(TOP_LAYER);

   const QConfEditStatus est = qconf_edit_text(text, uid, gid);
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
    * qconf_apply_path(). Measured on 2026-10-06: the answer list does arrive
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
 * @return 0 when the object was written or the editor left it unchanged, 1 otherwise
 */
static int
qconf_edit_obj_procedure(ocs::QConfParameterClient &param, QConfObj o,
                         const char *sw, const char *name, uid_t uid, gid_t gid) {
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
   DRETURN(qconf_edit_and_write_obj(o, text, uid, gid, nullptr, 0u, write_name));
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
 * @return 0 when the text was written or the editor left it unchanged, 1 otherwise
 */
static int
qconf_edit_list_procedure(ocs::QConfParameterClient &param, QConfObj o,
                          const char *sw, const char *read_token, const char *read_sub,
                          const char *names, uid_t uid, gid_t gid, uint32_t extra_mode = 0u) {
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
   DRETURN(qconf_edit_and_write_obj(o, text, uid, gid, names,
                                    ocs::QConfWriteParameter::MODE_LIST | extra_mode));
}



/* ===== CS-2307: resource quota set helpers =================================
 * RQS files hold a *list* of rule sets (read with spool_flatfile_read_list and
 * applied with SET_ALL), so they get their own helpers rather than the
 * single-object qconf_apply_path/qconf_delete_path above. The directory case
 * reuses qconf_for_each_file and emits the same catalogue messages/flags. */

/** @brief Context for qconf_rqs_collect: the merged RQS list + read-failure count. */
struct qconf_rqs_ctx {
   std::ostream *os;   ///< where the per-file messages go (CS-1841)
   lList *all;   ///< merged RQS_Type list collected from every file
   int n_fail;   ///< files that failed to read
};

/** @brief qconf_for_each_file callback: read one file's resource quota sets and
 *  move them into the merged list (CS-2307). */
static int
qconf_rqs_collect(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_rqs_ctx *>(vctx);
   lList *alp = nullptr;
   lList *l = spool_flatfile_read_list(&alp, RQS_Type, RQS_fields, nullptr, true,
                                       &qconf_rqs_sfi, qconf_opt_format, nullptr, filepath);
   if (answer_list_output(&alp) || l == nullptr) {
      lFreeList(&l);
      ctx->n_fail++;
      return 1;
   }
   lListElem *e;
   while ((e = lFirstRW(l)) != nullptr) {
      lDechainElem(l, e);
      lAppendElem(ctx->all, e);
   }
   lFreeList(&l);
   return 0;
}

/**
 * @brief Delete the resource quota set(s) named in the file(s) at @a path (CS-2307 C5).
 *
 * Reads every rule set named in the file/dir; a name that no longer exists is
 * reported and skipped (idempotent). A directory delete is confirmed unless -f,
 * and -dry previews it.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param path file or directory naming the rule sets to delete
 * @return 0 on success, non-zero on failure
 */
static int
qconf_rqs_delete(std::ostream &os, const char *path) {
   qconf_rqs_ctx ctx = {&os, lCreateList("rqs", RQS_Type), 0};
   bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_rqs_collect, &ctx);
   int ret = ctx.n_fail;

   /* keep only the names that still exist (idempotent delete) */
   lList *del = lCreateList("rqs del", RQS_Type);
   const lListElem *r;
   for_each_ep(r, ctx.all) {
      const char *nm = lGetString(r, RQS_name);
      if (qconf_object_exists(ocs::gdi::Target::RQS_LIST, RQS_Type, RQS_name, nm)) {
         lAddElemStr(&del, RQS_name, nm, RQS_Type);
      } else {
         WARNING(MSG_QCONF_DELSKIPPEDNOTEXIST_SS, nm, path);
      }
   }
   lFreeList(&ctx.all);

   int n = lGetNumberOfElem(del);
   if (n > 0 && qconf_opt_dry_run) {
      for_each_ep(r, del) {
         qconf_info(os, MSG_QCONF_DRYRUNWOULDDELETE_S, lGetString(r, RQS_name));
      }
      lFreeList(&del);
      return ret;
   }
   if (n > 0 && is_dir && !qconf_opt_force) {
      dstring p = DSTRING_INIT;
      sge_dstring_sprintf(&p, MSG_QCONF_CONFIRMDELETE_I, n);
      bool ok = qconf_confirm(sge_dstring_get_string(&p));
      sge_dstring_free(&p);
      if (!ok) {
         WARNING(MSG_QCONF_DELETEABORTED_I, n);
         lFreeList(&del);
         return ret;
      }
   }
   if (n > 0) {
      /* A direct GDI call, exactly like the "-drqs" name-list path does. The
       * rule sets' own client module is gone with CS-2823; what is left of this
       * type outside the descriptor table is the delete, which has no procedure
       * yet. */
      lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::RQS_LIST, ocs::gdi::Command::DEL,
                                             ocs::gdi::SubCommand::NONE, &del, nullptr, nullptr);
      ret += show_answer_list(alp);
      lFreeList(&alp);
   }
   lFreeList(&del);
   return ret;
}

/* ===== CS-2311: cluster/host configuration helpers ========================
 * The config object keys on the host name, which is the file *basename* (not a
 * field inside the file), and -Aconf/-Mconf already accept a comma-separated
 * file list. So config gets its own file/dir helpers (like RQS) rather than the
 * field-name-based qconf_apply_path/qconf_delete_path. What is left of them is
 * the path handling: since CS-2823 the writing itself is the procedure's, and
 * the basename travels with the request as the object's name, because a
 * configuration's text does not carry it. delete_configs() is forward-declared
 * at the top of this file. */

/** @brief Resolve the host key of a config file: its basename, host-resolved
 *         in place into @p host_ep (EH_name). Returns the resolved host name. */
static const char *
qconf_conf_host_of(const char *filepath, lListElem *host_ep) {
   const char *slash = strrchr(filepath, '/');
   const char *base = slash != nullptr ? slash + 1 : filepath;
   lSetHost(host_ep, EH_name, base);
   sge_resolve_host(host_ep, EH_name);   /* best-effort, like -Aconf/-dconf */
   return lGetHost(host_ep, EH_name);
}

/** @brief Context for the config apply/delete per-file callbacks. */
struct qconf_conf_ctx {
   std::ostream *os;   ///< where the per-file messages go (CS-1841)
   lList *hosts;     ///< delete path: collected CONF_name host elements
   int n_ok;         ///< files applied / hosts collected (CS-2299 H1)
   int n_fail;       ///< files that failed
};

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

/**
 * @brief Per-file delete callback: queue one host config for deletion (CS-2311 C5).
 *
 * The host key is the file basename. A config that no longer exists is reported
 * and skipped rather than queued (CS-2299 H5).
 *
 * @param filepath path of the config file naming the host
 * @param vctx     pointer to the qconf_conf_ctx for this batch
 * @return 0 always (a skipped non-existent host is not a failure)
 */
static int
qconf_conf_collect_host(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_conf_ctx *>(vctx);
   lListElem *host_ep = lCreateElem(EH_Type);
   const char *hostname = qconf_conf_host_of(filepath, host_ep);
   if (qconf_object_exists(ocs::gdi::Target::CONF_LIST, CONF_Type, CONF_name, hostname)) {
      lAddElemHost(&ctx->hosts, CONF_name, hostname, CONF_Type);
      ctx->n_ok++;
   } else {
      WARNING(MSG_QCONF_DELSKIPPEDNOTEXIST_SS, hostname, filepath);
   }
   lFreeElem(&host_ep);
   return 0;
}

/**
 * @brief Delete every host config named by the file(s) at a path (CS-2311 C5).
 *
 * Honours -dry (preview), -f (skip the directory confirm) and is idempotent
 * (a host whose config no longer exists is skipped, not failed).
 *
 * @param os the stream the output is written to (CS-1841)
 * @param path  config file or directory naming the host configs to delete
 * @return the number of configs that failed to be deleted
 */
static int
qconf_conf_delete_path(std::ostream &os, const char *path) {
   qconf_conf_ctx ctx = {&os, lCreateList("conf del", CONF_Type), 0, 0};
   bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_conf_collect_host, &ctx);

   int n = lGetNumberOfElem(ctx.hosts);
   if (n > 0 && qconf_opt_dry_run) {
      const lListElem *e;
      for_each_ep(e, ctx.hosts) {
         qconf_info(os, MSG_QCONF_DRYRUNWOULDDELETE_S, lGetHost(e, CONF_name));
      }
      lFreeList(&ctx.hosts);
      return 0;
   }
   if (n > 0 && is_dir && !qconf_opt_force) {
      dstring p = DSTRING_INIT;
      sge_dstring_sprintf(&p, MSG_QCONF_CONFIRMDELETE_I, n);
      bool ok = qconf_confirm(sge_dstring_get_string(&p));
      sge_dstring_free(&p);
      if (!ok) {
         WARNING(MSG_QCONF_DELETEABORTED_I, n);
         lFreeList(&ctx.hosts);
         return 0;
      }
   }
   int ret = delete_configs(ctx.hosts);
   lFreeList(&ctx.hosts);
   return ret;
}


/*------------------------------------------------------------*/
/** @brief Work through the `qconf` command line, one switch at a time
 *
 * `qconf` accepts several switches in one invocation and applies them in the
 * order given, so this is a loop over the arguments rather than a single
 * dispatch. Most switches are handled by the table in #object_info_entry; the
 * ones that are not have their own handlers in this file.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param argv the arguments, not including `argv[0]`
 * @return 0 when every switch succeeded
 */
/**
 * @brief Run one read operation as a stored procedure, if it is one
 *
 * The switches of groups A and B render inside qmaster from CS-2799 on. Which
 * ones is decided by the procedure itself, through
 * ocs::QConfModelBase::resolve() - not by a list kept here, because what a
 * caller may read is the server's question.
 *
 * @param os        the stream the output is written to
 * @param param     the invocation, whose operation is set to @p op for the call
 * @param op        the operation to run
 * @param ret       the accumulated exit status, raised to 1 on failure
 * @return true when the operation was handled here, false to run it locally
 */
static bool
qconf_run_read_procedure(std::ostream &os, ocs::QConfParameterClient &param,
                         qconf_op &op, int *ret) {
   DENTER(TOP_LAYER);

   const ocs::qconf_read_entry *entry = ocs::QConfModelBase::resolve(op.token.c_str());
   if (entry == nullptr || entry->kind == ocs::QConfReadKind::SINGLE_OBJECT) {
      /* The named single-object reads are not intercepted here. Their options
       * disagree about what a failure means - the classic four leave the parser,
       * -sprj only sets the exit code, -suser ignores it and shows a whole comma
       * separated list - and -se resolves the host name first. Those blocks keep
       * their own handling and reach the procedure through qconf_show_object().
       *
       * Name lists and singletons have no such quirks: a failure sets the exit
       * status and the loop carries on, which is what this function does. */
      DRETURN(false);
   }

   param.set_operation(op);
   lList *alp = nullptr;
   const bool ok = qconf_dispatch_procedure(os, param, &alp, nullptr);

   if (!ok) {
      /* CS-2808: a singleton that finds nothing keeps the bare wording its hand
       * written block used - "no sharetree element", not "error: no sharetree
       * element". The prefix comes from the ERROR() macro inside answer_log(),
       * so the text cannot travel as an answer and be printed unchanged. It
       * does not have to: the wording sits in the descriptor table, which is
       * linked into the client as well, so it is written here and the answer
       * carrying the same text is dropped.
       *
       * The model raises STATUS_EEXIST for exactly this case and for nothing
       * else in a singleton read, which is what makes the answer safe to
       * swallow. */
      const qconf_object_desc &desc = qconf_desc(entry->object);
      if (entry->kind == ocs::QConfReadKind::SINGLETON &&
          desc.read.empty_msg != nullptr &&
          answer_list_has_status(&alp, STATUS_EEXIST)) {
         fprintf(stderr, "%s\n", desc.read.empty_msg);
      } else {
         /* the messages the engine used to write to stderr itself; an empty
          * list for plain output is one of them */
         answer_list_output(&alp);
      }
      *ret = 1;
   }
   lFreeList(&alp);

   DRETURN(true);
}

int sge_parse_qconf(char *argv[]) {
   DENTER(TOP_LAYER);

   char *cp = nullptr;
   char **spp = nullptr;
   int opt;
   int sge_parse_return = 0;
   lEnumeration *what = nullptr;
   lList *lp=nullptr, *arglp=nullptr, *alp=nullptr;
   lListElem *hep=nullptr, *ep=nullptr;
   const lListElem *aep=nullptr;
   const char *host = nullptr;
   const char* qualified_hostname = component_get_qualified_hostname();
   const char* username = component_get_username();
   uint32_t prog_number = component_get_component_id();
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   /* The one place the sink is chosen (CS-1841). Everything below renders into
    * this stream instead of writing to stdout, so that step 8 (CS-2799) can hand
    * in qmaster's response buffer here without touching a single print site. */
   std::ostream &os = std::cout;

   /* If no arguments were given, output the help message on stderr. */
   if (*argv == nullptr) {
      sge_usage(QCONF, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      DRETURN(1);
   }
   
   /* CS-2798: the command line is parsed as data before anything is executed.
    * What comes back are the invocation-wide switches and the ordered list of
    * operations the execution phase walks.
    *
    * Those switches have to take effect no matter where they appear, so that
    * "qconf -sq all.q -fmt json" renders as JSON although -fmt comes last.
    * parse_parameters() reads them in a pass of its own over argv, exactly as
    * the hand written pre-scan did.
    *
    * MISSING_ARGUMENT and UNKNOWN_OPTION are not acted on here but after the
    * execution phase, so that a partially valid command line still produces the
    * output of its valid operations before the complaint - the order the single
    * parse-and-execute pass produced. */
   ocs::QConfParameterClient param(to_cstr(QCONF));
   std::string bad_token;
   const QConfParseStatus parse_status = param.parse_parameters(argv, bad_token);
   switch (parse_status) {
      case QConfParseStatus::NO_FORMAT_VALUE:
         fprintf(stderr, "%s\n", MSG_GDI_USAGE_Fmt_OPT);
         DRETURN(1);
      case QConfParseStatus::BAD_FORMAT:
         fprintf(stderr, MSG_QCONF_UNKNOWNFMT_S, bad_token.c_str());
         fprintf(stderr, "\n");
         DRETURN(1);
      case QConfParseStatus::BAD_FORMAT_VALUE:
         fprintf(stderr, MSG_QCONF_UNKNOWNFMTVAL_S, bad_token.c_str());
         fprintf(stderr, "\n");
         DRETURN(1);
      case QConfParseStatus::BAD_EXEC_CONTEXT:
         fprintf(stderr, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, bad_token.c_str());
         fprintf(stderr, "\n");
         DRETURN(1);
      default:
         break;
   }

   /* CS-2823: a caller may ask qmaster to answer as if RBAC had reduced the
    * response, so that the refusal to offer a reduced object to the editor can
    * be reached before redaction exists. Shipped as a request variable the way
    * qstat ships SGE_LONG_QNAMES; see FORCE_REDUCTION_VARIABLE for why handing
    * it to the server grants nothing. */
   if (const char *forced = getenv(ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE);
       forced != nullptr) {
      param.add_variable(ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE, forced);
   }

   /* CS-2799: the parameter object is the single source of truth. The file
    * statics below are a client-side mirror, written once, for the code that
    * has not moved into libs/procedure yet - the write path and the bespoke
    * show options of group N. They are never read by anything that runs inside
    * qmaster, which is what the rule is about; a client is one process per
    * invocation. They go when that code moves. */
   qconf_opt_dry_run = param.get_dry_run();
   qconf_opt_force = param.get_force();
   qconf_opt_strict = param.get_strict();
   qconf_opt_format = (param.get_output_format() == ocs::ProcedureParameter::OutputFormat::JSON)
                      ? SP_FORM_JSON : SP_FORM_ASCII;
   ocs_json_value_format_opt =
      (param.get_value_format() == ocs::QConfParameter::ValueFormat::NUMERIC)
      ? OCS_JSON_VALUES_NUMERIC : OCS_JSON_VALUES_COMPACT;

   /* CS-2798: the execution phase. It walks the operations the parser produced,
    * in the order they were typed, and hands each option block its own argument
    * vector - { switch, arg, ..., nullptr }. That is the shape the blocks have
    * always read, so `*spp`, sge_parser_get_next() and sge_next_is_an_opt()
    * keep working unchanged; they simply no longer reach across the operation
    * they belong to. The trailing `spp++; continue;` of every block now means
    * "on to the next operation". */
   for (qconf_op &op : param.get_operations()) {
      spp = op.as_argv();

      /* CS-2799: the read path of groups A and B is a stored procedure now. In
       * SERVER context the operation is handed to qmaster, which fetches and
       * renders it - that is where RBAC knows what the caller may see. In
       * CLIENT context the same model and view run here, against data fetched
       * over GDI, which is what makes the two comparable byte for byte.
       *
       * CS-2808 added the last four of groups A and B, so the whole read path
       * of both groups goes this way. Everything else - the write path and the
       * bespoke show options of group N - falls through to the option chain
       * below and runs locally, in the order it was typed. */
      if (qconf_run_read_procedure(os, param, op, &sge_parse_return)) {
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* The four skip branches that used to sit here - for -cb, the CS-2299
       * hardening flags and the CS-2313a format switches - are gone with
       * CS-2798. Those switches are marked invocation_wide in the option table,
       * so the parser reads them in its first pass and does not put them into
       * the operation list at all. There is nothing left to step over. */

/*----------------------------------------------------------------------------*/
      /* "-acal cal-name" */

      if ((strcmp("-acal", *spp) == 0) ||
          (strcmp("-Acal", *spp) == 0)) {
         if (!strcmp("-acal", *spp)) {
            /* CS-2299 C1: the calendar name is optional; when omitted a generic
             * template named "template" is offered for editing (cf. -aq/-ahgrp). */
            const char *cal_name = "template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               cal_name = *spp;
            }

            /* CS-2823: template, editor and write procedure in one - see
             * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
             * share. The template the upsert of CS-2299 C2 edits is built in qmaster
             * now, from the read's descriptor row. */
            if (qconf_edit_obj_procedure(param, QConfObj::CAL, "-acal", cal_name, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Acal: CS-2299 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CAL, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-ackpt ckpt_name" or "-Ackpt fname" */

      if ((strcmp("-ackpt", *spp) == 0) ||
          (strcmp("-Ackpt", *spp) == 0)) {

         if (!strcmp("-ackpt", *spp)) {
            /* CS-2300 C1: the checkpoint name is optional; when omitted a generic
             * template named "template" is offered for editing. */
            const char *ckpt_name = "template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               ckpt_name = *spp;
            }

            /* CS-2823: template, editor and write procedure in one - see
             * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
             * share. The template the upsert of CS-2300 C2 edits is built in qmaster
             * now, from the read's descriptor row. */
            if (qconf_edit_obj_procedure(param, QConfObj::CK, "-ackpt", ckpt_name, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Ackpt: CS-2300 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CK, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ae [exec_server_template]"
       *
       * CS-2823: the same round trip as "-me". Despite its name this is not a
       * template add like "-acal": it *reads* an exec host - the one called
       * "template" when no name is given - and offers that for editing. The
       * switch it shares its code with was "-me" from the start; since the
       * template moved into the read, "-acal" shares it too and the three run
       * through one qconf_edit_obj_procedure(). Resolving the name stays in
       * the client, as
       * it does for "-me": the procedure read is asked for a name, so a short
       * host name has to be resolved before it is asked. The template is not a
       * host and is never resolved, which is why only the given name is.
       *
       * Two things changed with the conversion, both deliberate:
       *
       *  - a name that is no exec host is now reported as "x is not an
       *    execution host", the message "-me" and "-se" have always used. The
       *    descriptor row carries one not-found message per type, and the
       *    former wording of this branch was the only user of its own macro,
       *    so the two were merged rather than the message being made
       *    per-switch.
       *  - a write to a name that already exists modifies it instead of being
       *    refused, because the write procedure upserts. That is what "-a<obj>"
       *    has meant for every other type since CS-2299 C2 and what "-Ae" has
       *    meant for the exec host since CS-2304 C2, so the exec host's
       *    interactive add was the last one left out.
       *
       * It also gains the two refusals of the procedure path for free: a
       * reduced read is not offered for editing, and "-ectx client" is
       * declined. */
      if (strcmp("-ae", *spp) == 0) {
         char *hostname = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (sge_next_is_an_opt(spp)) {
            /* no name given: the template host. It is an entry in the exec host
             * list and not a host, so it is the one name that must not be
             * resolved. */
            hostname = sge_strdup(nullptr, SGE_TEMPLATE_NAME);
         } else {
            spp = sge_parser_get_next(spp);

            lListElem *host_ep = lCreateElem(EH_Type);
            lSetHost(host_ep, EH_name, *spp);

            if (sge_resolve_host(host_ep, EH_name) != CL_RETVAL_OK) {
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(host_ep, EH_name));
               fprintf(stderr, "\n");
               lFreeElem(&host_ep);
               DRETURN(1);
            }
            hostname = sge_strdup(nullptr, lGetHost(host_ep, EH_name));
            lFreeElem(&host_ep);
         }

         if (qconf_edit_obj_procedure(param, QConfObj::EH, "-ae", hostname, uid, gid) != 0) {
            sge_parse_return = 1;
         }
         sge_free(&hostname);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-Ae fname|dir": CS-2304 C2/C3 — add/modify exec host(s) from a file or directory */
      if (strcmp("-Ae", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::EH, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-ah server_name[,server_name,...]" */
      if (strcmp("-ah", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to add", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, ADMIN_HOSTGROUP,
                                  ocs::gdi::SubCommand::APPEND, "administrative host")) {
            sge_parse_return |= 1;
         }

         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-arole role_name" / "-Arole fname" */
      if ((strcmp("-arole", *spp) == 0) ||
          (strcmp("-Arole", *spp) == 0)) {
         if (!strcmp("-arole", *spp)) {
            /* CS-2302 C1: the role name is optional; when omitted a generic
             * template named "template" is offered for editing. */
            const char *role_name = "template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               role_name = *spp;
            }

            /* CS-2823: template, editor and write procedure in one - see
             * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
             * share. The template the upsert of CS-2302 C2 edits is built in qmaster
             * now, from the read's descriptor row. */
            if (qconf_edit_obj_procedure(param, QConfObj::RL, "-arole", role_name, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Arole: CS-2302 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::RL, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-arqs rqs_name" */
      if (strcmp("-arqs", *spp) == 0) {
         /* the name is optional; when omitted a template named "template" is
          * offered for editing and the editor gives it its real name */
         const char *name = "template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: template, editor and write procedure in one - the same
          * function "-mrqs" calls, with the templates built in qmaster from the
          * read's descriptor row. A comma separated list of names still yields
          * one template each, as rqs_add() built them here. */
         if (qconf_edit_list_procedure(param, QConfObj::RQS, "-arqs",
                                       ocs::QConfModelBase::switch_for(QConfObj::RQS,
                                                                       ocs::QConfReadKind::OBJECT_LIST),
                                       nullptr, name, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-Arqs fname|dir": CS-2307 C3 — a single (multi-rule-set) file as before,
       * or a directory whose files together define the rule sets to add */
      if (strcmp("-Arqs", *spp) == 0) {
         const char *file = nullptr;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: as every other type's "-A<obj>" - add or modify what the
          * text holds, which is what this did before through its own path. */
         if (qconf_apply_obj(os, QConfObj::RQS, file, nullptr,
                             ocs::QConfWriteParameter::MODE_LIST) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-am user_list" */

      if (strcmp("-am", *spp) == 0) {
         /* no adminhost/manager check needed here */

         /* CS-2394: managers are the members of the reserved "manager" userset;
          * -am user is equivalent to -au user manager. */
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, UE_Type, UE_name, ", ");
         lString2List(MANAGER_USERSET, &arglp, US_Type, US_name, ", ");
         sge_client_add_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);
         lFreeList(&lp);
         lFreeList(&arglp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-ao user_list" */

      if (strcmp("-ao", *spp) == 0) {
         /* no adminhost/manager check needed here */

         /* CS-2394: operators are the members of the reserved "operator" userset;
          * -ao user is equivalent to -au user operator. */
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, UE_Type, UE_name, ", ");
         lString2List(OPERATOR_USERSET, &arglp, US_Type, US_name, ", ");
         sge_client_add_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);
         lFreeList(&lp);
         lFreeList(&arglp);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-ap pe_name" */

      if ((strcmp("-ap", *spp) == 0) ||
          (strcmp("-Ap", *spp) == 0)) {

         if (!strcmp("-ap", *spp)) {
            /* CS-2301 C1: the PE name is optional; when omitted a generic
             * template named "template" is offered for editing. */
            const char *pe_name = "template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               pe_name = *spp;
            }

            /* CS-2823: template, editor and write procedure in one - see
             * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
             * share. The template the upsert of CS-2301 C2 edits is built in qmaster
             * now, from the read's descriptor row. */
            if (qconf_edit_obj_procedure(param, QConfObj::PE, "-ap", pe_name, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Ap: CS-2301 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::PE, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-auser" */

      if (strcmp("-auser", *spp) == 0) {
         /* CS-2308 C1: the user name is optional; when omitted a generic
          * template named "template" is offered for editing (cf. -acal). */
         const char *user_name = "template";

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            user_name = *spp;
         }

         /* CS-2823: template, editor and write procedure in one - see
          * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
          * share. The template the upsert of CS-2308 C2 edits is built in qmaster
          * now, from the read's descriptor row. */
         if (qconf_edit_obj_procedure(param, QConfObj::UU, "-auser", user_name, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-aprj" */

      if (strcmp("-aprj", *spp) == 0) {
         /* CS-2309 C1: the project name is optional; when omitted a generic
          * template named "template" is offered for editing (cf. -auser). */
         const char *prj_name = "template";

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            prj_name = *spp;
         }

         /* CS-2823: template, editor and write procedure in one - see
          * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
          * share. The template the upsert of CS-2309 C2 edits is built in qmaster
          * now, from the read's descriptor row. */
         if (qconf_edit_obj_procedure(param, QConfObj::PR, "-aprj", prj_name, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Auser" */

      if (strcmp("-Auser", *spp) == 0) {
         /* CS-2308 C2+C3: accept a file OR a directory, upsert each user.
          * The user object uses a dynamically built field list. */

         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::UU, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }


/*----------------------------------------------------------------------------*/

      /* "-Aprj" */

      if (strcmp("-Aprj", *spp) == 0) {
         /* CS-2309 C2+C3: accept a file OR a directory, upsert each project. */

         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::PR, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      /* "-as server_name[,server_name,...]" */
      if (strcmp("-as", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to add", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, SUBMIT_HOSTGROUP,
                                  ocs::gdi::SubCommand::APPEND, "submit host")) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-astree", "-Astree file": add the share tree
       *
       * The same upsert as "-mstree"/"-Mstree" and the same code: the share tree
       * is a singleton, sge_mod_sharetree() creates the list when there is none,
       * and CS-2312 had already made the two pairs interchangeable - what is
       * left of the distinction is the switch name. See the "-mstree" block for
       * what moved into the procedure (CS-2823). */
      if ((strcmp("-astree", *spp) == 0) || (strcmp("-Astree", *spp) == 0)) {
         if (strcmp("-astree", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (qconf_edit_obj_procedure(param, QConfObj::STN, "-astree", nullptr, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else {
            spp = sge_parser_get_next(spp);

            if (qconf_apply_obj(os, QConfObj::STN, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-astnode node_path=shares[,...]"  create/modify sharetree node */
      /* "-mstnode node_path=shares[,...]"  modify sharetree node */

      if (((strcmp("-astnode", *spp) == 0) && ((opt=astnode_OPT) != 0)) ||
          ((strcmp("-mstnode", *spp) == 0) && ((opt=mstnode_OPT) != 0))) {
         int modified = 0;
         int print_usage = 0;

         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);

         ep = lFirstRW(lp);
         if (!ep && opt == astnode_OPT) {
            ep = lAddElemStr(&lp, STN_name, "Root", STN_Type);
            if (ep) lSetUlong(ep, STN_shares, 1);
         }
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw_lv (argep, arglp) {
            lListElem *node = nullptr;
            char *buf=nullptr, *nodepath=nullptr, *sharestr=nullptr;
            int shares;
            ancestors_t ancestors;

            buf = sge_strdup(buf, lGetString(argep, STN_name));
            nodepath = sge_strtok(buf, "=");
            sharestr = sge_strtok(nullptr, "");
            if (nodepath && sharestr && sscanf(sharestr, "%d", &shares) == 1) {
               if (shares < 0) {
                  fprintf(stderr, "%s\n", MSG_QCONF_POSITIVE_SHARE_VALUE);
                  DRETURN(1);
               }
               memset(&ancestors, 0, sizeof(ancestors));
               node = search_named_node_path(ep, nodepath, &ancestors);
               if (!node && opt==astnode_OPT) {
                  char *c, *lc = nullptr;
                  lListElem *pnode, *nnode;

                  /* scan for basename of nodepath */
                  for (c=nodepath; *c; c++)
                     if (*c == '/' || *c == '.')
                        lc = c;

                  /* search for parent node of new node */
                  if (lc && *nodepath && *(lc+1)) {
                     char savelc = *lc;
                     *lc = '\000';
                     if (lc == nodepath && savelc == '/') /* root? */
                        pnode = ep;
                     else
                        pnode = search_named_node_path(ep, nodepath,
                                                       &ancestors);
                     if (pnode) {
                        lList *children = lGetListRW(pnode, STN_children);

                        nnode = lAddElemStr(&children, STN_name, lc+1, STN_Type);
                        if (nnode && !lGetList(pnode, STN_children))
                           lSetList(pnode, STN_children, children);
                        free_ancestors(&ancestors);
                        memset(&ancestors, 0, sizeof(ancestors));
                        *lc = savelc;
                        node = search_named_node_path(ep, nodepath, &ancestors);
                        if (node != nnode) {
                           fprintf(stderr, MSG_TREE_CANTADDNODEXISNONUNIQUE_S, nodepath);
                           fprintf(stderr, "\n");
                           DRETURN(1);
                        }
                     }
                  }
               }
               if (node) {
                  int i;
                  modified++;
                  lSetUlong(node, STN_shares, shares);
                  fprintf(stderr, "%s\n", MSG_TREE_SETTING);
                  for (i=0; i<ancestors.depth; i++)
                     fprintf(stderr, "/%s",
                             lGetString(ancestors.nodes[i], STN_name));
                  fprintf(stderr, "=%d\n", shares);
               } else {
                  fprintf(stderr, MSG_TREE_UNABLETOLACATEXINSHARETREE_S,
                          nodepath);
                  fprintf(stderr, "\n");
               }
               free_ancestors(&ancestors);

            } else {
               fprintf(stderr, MSG_ANSWER_XISNOTVALIDSEENODESHARESLIST_S, *spp);
               fprintf(stderr, "\n");
               print_usage = 1;
            }

            sge_free(&buf);
         }
         lFreeList(&arglp);

         if (modified) {
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
            lFreeWhat(&what);
            ep = lFirstRW(alp);
            answer_exit_if_not_recoverable(ep);
            if (answer_get_status(ep) == STATUS_OK)
               fprintf(stderr, "%s\n", MSG_TREE_MODIFIEDSHARETREE);
            else
               fprintf(stderr, "%s\n", lGetString(ep, AN_text));
            lFreeList(&alp);
         } else {
            fprintf(stderr, "%s\n", MSG_TREE_NOMIDIFIEDSHARETREE);
            if (print_usage)
               sge_usage(QCONF, stderr);
            DRETURN(1);
         }

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-au user_list list_name[,list_name,...]" */

      if (strcmp("-au", *spp) == 0) {

         /* no adminhost/manager check needed here */

         /* get user list */
         spp = sge_parser_get_next(spp);
         if (!*(spp+1)) {
            ERROR(MSG_ANSWER_NOLISTNAMEPROVIDEDTOAUX_S, *spp);
            sge_usage(QCONF, stderr);
            DRETURN(1);
         }

         lString2List(*spp, &lp, UE_Type, UE_name, ",  ");

         /* get list_name list */
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &arglp, US_Type, US_name, ", ");

         /* add all users/groups from lp to the acls in alp */
         sge_client_add_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&arglp);
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-clearusage"  clear sharetree usage */

      if (strcmp("-clearusage", *spp) == 0) {
         lList *lp2=nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* get user list */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::UU_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);

         /* get project list */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::PR_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp2, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            spp++;
            continue;
         }
         lFreeList(&alp);

         /* clear user usage */
         for_each_rw (ep, lp) {
            lSetList(ep, UU_usage, nullptr);
            lSetList(ep, UU_project, nullptr);
         }

         /* clear project usage */
         for_each_rw (ep, lp2) {
            lSetList(ep, PR_usage, nullptr);
            lSetList(ep, PR_project, nullptr);
         }

         /* update user usage */
         if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::UU_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
            answer_list_on_error_print_or_exit(&alp, stderr);
            lFreeList(&alp);
         }

         /* update project usage */
         if (lp2 && lGetNumberOfElem(lp2) > 0) {
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::PR_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp2, nullptr, nullptr);
            answer_list_on_error_print_or_exit(&alp, stderr);
            lFreeList(&alp);
         }

         lFreeList(&lp);
         lFreeList(&lp2);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-dcal calendar_name" */

      if (strcmp("-dcal", *spp) == 0) {
         /* no adminhost/manager check needed here */
         /* CS-2299 C4: accept a comma-separated list of calendar names. */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::CAL, *spp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-Dcal file|dir": CS-2299 C5 — delete the calendar(s) named in the file(s) */

      if (strcmp("-Dcal", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::CAL, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-dckpt ckpt_name" */

      if (strcmp("-dckpt", *spp) == 0) {
         /* no adminhost/manager check needed here */
         /* CS-2300 C4: accept a comma-separated list of checkpoint names. */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::CK, *spp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-Dckpt file|dir": CS-2300 C5 — delete the checkpoint(s) named in the file(s) */
      if (strcmp("-Dckpt", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::CK, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-de server_name[,server_name,...]" */
      if (strcmp("-de", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, EH_Type, EH_name, *spp);
         if (!del_host_of_type(lp, ocs::gdi::Target::EH_LIST)) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-De file|dir": CS-2304 C5 — delete the exec host(s) named in the file(s) */
      if (strcmp("-De", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::EH, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/

      /* "-dh server_name[,server_name,...]" */
      if (strcmp("-dh", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, ADMIN_HOSTGROUP,
                                  ocs::gdi::SubCommand::REMOVE, "administrative host")) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-drole role_name" */
      if (strcmp("-drole", *spp) == 0) {
         /* CS-2302 C4: accept a comma-separated list of role names. */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::RL, *spp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-Drole file|dir": CS-2302 C5 — delete the role(s) named in the file(s) */
      if (strcmp("-Drole", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::RL, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-drqs rqs_name[,rqs_name,...]" */
      if (strcmp("-drqs", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::RQS, *spp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-Drqs file|dir": CS-2307 C5 — delete the rule set(s) named in the file(s) */
      if (strcmp("-Drqs", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_rqs_delete(os, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-dm user_list" */

      if (strcmp("-dm", *spp) == 0) {
         /* no adminhost/manager check needed here */

         /* CS-2394: -dm user is equivalent to -du user manager. */
         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, UE_Type, UE_name, ", ");
         lString2List(MANAGER_USERSET, &arglp, US_Type, US_name, ", ");
         sge_client_del_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);
         lFreeList(&arglp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-do user_list" */

      if (strcmp("-do", *spp) == 0) {
         /* no adminhost/manager check needed here */

         /* CS-2394: -do user is equivalent to -du user operator. */
         spp = sge_parser_get_next(spp);

         lString2List(*spp, &lp, UE_Type, UE_name, ", ");
         lString2List(OPERATOR_USERSET, &arglp, US_Type, US_name, ", ");
         sge_client_del_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);
         lFreeList(&arglp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-dp pe-name" */

      if (strcmp("-dp", *spp) == 0) {
         /* no adminhost/manager check needed here */
         /* CS-2301 C4: accept a comma-separated list of PE names. */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::PE, *spp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-Dp file|dir": CS-2301 C5 — delete the PE(s) named in the file(s) */
      if (strcmp("-Dp", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::PE, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-ds server_name[,server_name,...]" */
      if (strcmp("-ds", *spp) == 0) {
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("host to del", &lp, HR_Type, HR_name, *spp);
         if (!mod_reserved_hgroup(os, lp, SUBMIT_HOSTGROUP,
                                  ocs::gdi::SubCommand::REMOVE, "submit host")) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-du user_list list_name[,list_name,...]" */

      if (strcmp("-du", *spp) == 0) {

         /* no adminhost/manager check needed here */

         /* get user list */
         spp = sge_parser_get_next(spp);
         if (!*(spp+1)) {
            ERROR(MSG_ANSWER_NOLISTNAMEPROVIDEDTODUX_S, *spp);
            sge_usage(QCONF, stderr);
            DRETURN(1);
         }
         lString2List(*spp, &lp, UE_Type, UE_name, ",  ");

         /* get list_name list */
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &arglp, US_Type, US_name, ", ");

         /* remove users/groups from lp from the acls in alp */
         sge_client_del_user(&alp, lp, arglp);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&lp);
         lFreeList(&alp);
         lFreeList(&arglp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-dul list_name_list" */

      if (strcmp("-dul", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::US, *spp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-duser user,..." */

      if (strcmp("-duser", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::UU, *spp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dprj project,..." */

      if (strcmp("-dprj", *spp) == 0) {
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);
         sge_parse_return |= qconf_delete_names(QConfObj::PR, *spp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dstnode node_path[,...]"  delete sharetree node(s) */

      if (strcmp("-dstnode", *spp) == 0) {
         int modified = 0;
         /* no adminhost/manager check needed here */
         spp = sge_parser_get_next(spp);

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         ep = lFirstRW(lp);
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            sge_parse_return = 1;
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw_lv (argep, arglp) {

            lListElem *node = nullptr;
            const char *nodepath = nullptr;
            ancestors_t ancestors;

            nodepath = lGetString(argep, STN_name);
            if (nodepath) {
               memset(&ancestors, 0, sizeof(ancestors_t));
               node = search_named_node_path(ep, nodepath, &ancestors);
               if (node) {
                  if (lGetList(node, STN_children) == nullptr) {
                     if (ancestors.depth > 0) {
                        int i;
                        lList *siblings = nullptr;
                        lListElem *pnode = nullptr;
                        modified++;
                        if (ancestors.depth == 1)
                           pnode = ep;
                        else
                           pnode = ancestors.nodes[ancestors.depth-2];
                        fprintf(stderr, "%s\n", MSG_TREE_REMOVING);
                        for (i=0; i<ancestors.depth; i++)
                           fprintf(stderr, "/%s",
                                   lGetString(ancestors.nodes[i], STN_name));
                        fprintf(stderr, "\n");
                        siblings = lGetListRW(pnode, STN_children);
                        lRemoveElem(siblings, &node);
                        if (lGetNumberOfElem(siblings) == 0)
                           lSetList(pnode, STN_children, nullptr);
                     } else {
                        fprintf(stderr, "%s\n", MSG_TREE_CANTDELROOTNODE);
                     }
                  } else {
                     fprintf(stderr, "%s\n", MSG_TREE_CANTDELNODESWITHCHILD);
                  }
               } else {
                  fprintf(stderr, MSG_TREE_UNABLETOLACATEXINSHARETREE_S,
                          nodepath);
                  fprintf(stderr, "\n");
               }
               free_ancestors(&ancestors);

            }
         }
         lFreeList(&arglp);

         if (modified) {
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
            lFreeWhat(&what);
            ep = lFirstRW(alp);
            answer_exit_if_not_recoverable(ep);
            if (answer_get_status(ep) == STATUS_OK)
               fprintf(stderr, "%s\n", MSG_TREE_MODIFIEDSHARETREE);
            else {
               fprintf(stderr, "%s\n", lGetString(ep, AN_text));
               sge_parse_return = 1;
            }
            lFreeList(&alp);
         } else {
            fprintf(stderr, "%s\n", MSG_TREE_NOMIDIFIEDSHARETREE);
            DRETURN(1);
         }

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dstree" */

      if (strcmp("-dstree", *spp) == 0) {
         /* no adminhost/manager check needed here */
         sge_parse_return |= qconf_delete_names(QConfObj::STN, nullptr);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-help" */

      if (strcmp("-help", *spp) == 0) {
         sge_usage(QCONF, stdout);
         DRETURN(0);
      }

/*----------------------------------------------------------------------------*/
      /* "-ks" */

      if (strcmp("-ks", *spp) == 0) {
         /* no adminhost/manager check needed here */

         alp = ocs::gdi::Client::gdi_kill(nullptr, SCHEDD_KILL);
         for_each_ep(aep, alp) {
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK)
               sge_parse_return = 1;
            answer_print_text(aep, stderr, nullptr, nullptr);
         }

         lFreeList(&alp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-km" */

      if (strcmp("-km", *spp) == 0) {
         /* no adminhost/manager check needed here */
         alp = ocs::gdi::Client::gdi_kill(nullptr, MASTER_KILL);
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* -kec <id> ... */
      /* <id> may be "all" */
      /* parse before -ke[j] */

      if (strncmp("-kec", *spp, 4) == 0) {
         int action_flag = EVENTCLIENT_KILL;
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         /* found namelist -> process */
         if (strcmp(*spp, "all") == 0) { /* kill all dynamic event clients (EV_ID_ANY) */
            alp = ocs::gdi::Client::gdi_kill(nullptr, action_flag);
         } else {
            lString2List(*spp, &lp, ID_Type, ID_str, ", ");
            alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         }
         sge_parse_return |= show_answer_list(alp);
         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/

      /* -at <name> ... */
      /* <name> may be "scheduler" */

      if (strncmp("-at", *spp, 4) == 0) {
         int action_flag = THREAD_START;
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw (ep, lp) {
            lSetUlong(ep, ID_action, SGE_THREAD_TRIGGER_START);
         }
         alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         lFreeList(&lp);
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* -kt <name> ... */
      /* <name> may be "scheduler" */

      if (strncmp("-kt", *spp, 4) == 0) {
         int action_flag = THREAD_START;
         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw(ep, lp) {
            lSetUlong(ep, ID_action, SGE_THREAD_TRIGGER_STOP);
         }
         alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         lFreeList(&lp);
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/

      /* -ke[j] <host> ... */
      /* <host> may be "all" */
      if (strncmp("-k", *spp, 2) == 0) {
         int action_flag = EXECD_KILL;
         /* no adminhost/manager check needed here */

         cp = (*spp) + 2;
         switch (*cp++) {
            case 'e':
               break;
            default:
               ERROR(MSG_ANSWER_XISNOTAVALIDOPTIONY_SU, *spp, prog_number);
               sge_usage(QCONF, stderr);
               DRETURN(1);
         }

         if (*cp == 'j') {
            cp++;
            action_flag |= JOB_KILL;
         }

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
         } else {
            if (sge_error_and_exit(MSG_QCONF_NEEDAHOSTNAMEORALL))
               continue;
         }

         if (strcmp(*spp, "all") == 0) { /* kill all dynamic event clients (EV_ID_ANY) */
            alp = ocs::gdi::Client::gdi_kill(nullptr, action_flag);
         } else {
            /* found namelist -> process */
            lString2List(*spp, &lp, EH_Type, EH_name, ", ");
            alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
         }
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mc" */

      if (strcmp("-mc", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: the complex table is a stored procedure round trip now, the
          * same three steps as "-mrqs" - qmaster renders the table, the client
          * carries it to the editor and back, qmaster parses and applies it.
          * The read is the "-sc" sub-procedure, whose aligned columns are what
          * an administrator edits.
          *
          * MODE_REPLACE, unlike "-mrqs": the complex list keeps its wholesale
          * replace, because a row taken out of the table is how a complex is
          * removed - there is no "-dc" - and both halves of the object agree on
          * it. The built-ins are retained and named, which is the property
          * CS-2523 put into the client and which now lives on the descriptor. */
         if (qconf_edit_list_procedure(param, QConfObj::CE, "-mc", "-sc",
                                       ocs::QConfParameter::CENTRY_LIST, nullptr, uid, gid,
                                       ocs::QConfWriteParameter::MODE_REPLACE) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mcal cal-name" */

      if ((strcmp("-mcal", *spp) == 0) ||
          (strcmp("-Mcal", *spp) == 0)) {
         if (!strcmp("-mcal", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            spp = sge_parser_get_next(spp);

            /* CS-2822: the interactive modify is a stored procedure round trip
             * now. qmaster renders the object, the client carries the text to
             * the editor and back, and qmaster parses and applies it. The
             * GET/MOD path this option used is gone - leaving it beside the new
             * one would mean two writers for the same object, one of which
             * cannot see a reduced read.
             *
             * -Mcal below goes through the same procedure, as a set of many
             * instead of a set of one, so the calendar has one writer and not
             * two - see qconf_apply_path_procedure(). */
            if (qconf_edit_obj_procedure(param, QConfObj::CAL, "-mcal", *spp, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Mcal: CS-2299 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CAL, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-Mc complex file" */

      if (strcmp("-Mc", *spp) == 0) {
         char *file = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         /* The file side of "-mc", through the same procedure with the same two
          * modes, so the complex list has one writer and not two (CS-2823). */
         if (qconf_apply_obj(os, QConfObj::CE, file, nullptr,
                             ocs::QConfWriteParameter::MODE_LIST |
                             ocs::QConfWriteParameter::MODE_REPLACE) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-mckpt ckpt_name" or "-Mckpt fname" */

      if ((strcmp("-mckpt", *spp) == 0) ||
          (strcmp("-Mckpt", *spp) == 0)) {
         if (strcmp("-mckpt", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            spp = sge_parser_get_next(spp);

            /* CS-2823: the stored procedure round trip, as -mcal does it, and the
             * same function "-a<obj>" calls. The implicit add of CS-2300 C2 is
             * unchanged; the template it edits comes from the read's descriptor row
             * and is built in qmaster. */
            if (qconf_edit_obj_procedure(param, QConfObj::CK, "-mckpt", *spp, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Mckpt: CS-2300 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CK, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-Me fname|dir": CS-2304 C2/C3 — add/modify exec host(s) from a file or directory */
      if (strcmp("-Me", *spp) == 0) {

         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::EH, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-me [server_name,...]" */

      if (strcmp("-me", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("hosts to change", &arglp, EH_Type, EH_name,
            *spp);

         /* CS-2823: the stored procedure round trip, through the same function
          * as "-ae" - the two are one operation and differ only in the name
          * they are given. Two things are specific to the exec host: the name
          * is resolved here, before the read is asked for it, and the read's
          * descriptor row carries no template, so an unknown host is reported
          * instead of created. An execution host is not conjured from a name;
          * it is added by installing an execd, or by "-ae" under the name of a
          * host that resolves. The switch takes a list, so each name goes
          * through the round trip on its own. */
         for_each_rw_lv (argep, arglp) {
            if (sge_resolve_host(argep, EH_name) != CL_RETVAL_OK) {
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(argep, EH_name));
               fprintf(stderr, "\n");
               sge_parse_return = 1;
               continue;
            }
            host = lGetHost(argep, EH_name);

            if (qconf_edit_obj_procedure(param, QConfObj::EH, "-me", host, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         }
         lFreeList(&arglp);
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-mrole role_name" / "-Mrole fname" */
      if ((strcmp("-mrole", *spp) == 0) ||
          (strcmp("-Mrole", *spp) == 0)) {
         if (!strcmp("-mrole", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            spp = sge_parser_get_next(spp);

            /* CS-2823: the stored procedure round trip, as -mcal does it, and the
             * same function "-a<obj>" calls. The implicit add of CS-2302 C2 is
             * unchanged; the template it edits comes from the read's descriptor row
             * and is built in qmaster. */
            if (qconf_edit_obj_procedure(param, QConfObj::RL, "-mrole", *spp, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Mrole: CS-2302 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::RL, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-mrqs rqs_name" */
      if (strcmp("-mrqs", *spp) == 0) {
         const char *name = nullptr;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: the stored procedure round trip over a list - see
          * qconf_edit_list_procedure(), which is also what "-arqs" calls. With
          * no name every rule set is shown, as before; what changed is that a
          * rule set missing from the saved text is reported as still being
          * there instead of being deleted. */
         if (qconf_edit_list_procedure(param, QConfObj::RQS, "-mrqs",
                                       ocs::QConfModelBase::switch_for(QConfObj::RQS,
                                                                       ocs::QConfReadKind::OBJECT_LIST),
                                       nullptr, name, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-Mrqs fname|dir [rqs_name,...]": CS-2307 C3 — a single file (optionally
       * limited to the named rule sets) as before, or a directory of files */
      if (strcmp("-Mrqs", *spp) == 0) {
         const char *file = nullptr;
         const char *name = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }

         /* CS-2823: the same path every other type's "-M<obj>" takes. The
          * optional name list is no longer a filter this side applies to what
          * it parsed - it travels with the request and is applied where the
          * parse happens. */
         if (qconf_apply_obj(os, QConfObj::RQS, file, name,
                             ocs::QConfWriteParameter::MODE_LIST) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-mp pe_name" */

      if ((strcmp("-mp", *spp) == 0) ||
          (strcmp("-Mp", *spp) == 0)) {

         if (!strcmp("-mp", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            spp = sge_parser_get_next(spp);

            /* CS-2823: the stored procedure round trip, as -mcal does it, and the
             * same function "-a<obj>" calls. The implicit add of CS-2301 C2 is
             * unchanged; the template it edits comes from the read's descriptor row
             * and is built in qmaster. */
            if (qconf_edit_obj_procedure(param, QConfObj::PE, "-mp", *spp, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else { /* -Mp: CS-2301 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::PE, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      if (strcmp("-sobjl", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-sobjl";
         spp = sge_parser_get_next(spp);
         op.args.emplace_back(*spp);
         spp = sge_parser_get_next(spp);
         op.args.emplace_back(*spp);
         spp = sge_parser_get_next(spp);
         op.args.emplace_back(*spp);

         /* CS-2819: a sub-procedure. Which name an entry produces depends on the
          * kind of the entry, so the matching cannot be expressed as a filter on
          * a descriptor row - it has its own model function. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::OBJECT_FILTER);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*---------------------------------------------------------------------------*/

   if ((strcmp("-mattr", *spp) == 0) || (strcmp("-Mattr", *spp) == 0) ||
       (strcmp("-aattr", *spp) == 0) || (strcmp("-Aattr", *spp) == 0) ||
       (strcmp("-rattr", *spp) == 0) || (strcmp("-Rattr", *spp) == 0) ||
       (strcmp("-dattr", *spp) == 0) || (strcmp("-Dattr", *spp) == 0)) {

/* *INDENT-OFF* */
      static object_info_entry info_entry[] = {
         {ocs::gdi::Target::CQ_LIST,         SGE_OBJ_CQUEUE,    CQ_Type,   SGE_ATTR_QNAME,     CQ_name,   nullptr,     &qconf_sfi,        cqueue_xattr_pre_gdi},
         {ocs::gdi::Target::EH_LIST,         SGE_OBJ_EXECHOST,  EH_Type,   SGE_ATTR_HOSTNAME,  EH_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::PE_LIST,         SGE_OBJ_PE,        PE_Type,   SGE_ATTR_PE_NAME,   PE_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::CK_LIST,         SGE_OBJ_CKPT,      CK_Type,   SGE_ATTR_CKPT_NAME, CK_name,   nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::HGRP_LIST,       SGE_OBJ_HGROUP,    HGRP_Type, SGE_ATTR_HGRP_NAME, HGRP_name, nullptr,     &qconf_sfi,        nullptr},
         {ocs::gdi::Target::RQS_LIST,        SGE_OBJ_RQS,       RQS_Type,  SGE_ATTR_RQS_NAME,  RQS_name,  nullptr,     &qconf_rqs_sfi,    rqs_xattr_pre_gdi},
         {ocs::gdi::Target::NO_TARGET,           nullptr,           nullptr,   nullptr,            NoName,    nullptr,     nullptr,           nullptr}
      };
/* *INDENT-ON* */

      int from_file;
      int index;
      int ret = 0;
      ocs::gdi::SubCommand sub_command = ocs::gdi::SubCommand::NONE;

      /* This does not have to be freed later */
      info_entry[0].fields = CQ_fields;
      /* These have to be freed later */
      info_entry[1].fields = sge_build_EH_field_list(false, false, false);
      /* These do not */
      info_entry[2].fields = PE_fields;
      info_entry[3].fields = CK_fields;
      info_entry[4].fields = HGRP_fields;
      info_entry[5].fields = RQS_fields;

      /* no admin host/manager check needed here */

      /* Capital letter => we will read from file */
      if (isupper((*spp)[1])) {
         from_file = 1;
      } else {
         from_file = 0;
      }

      /* Set sub command for co_gdi call */
      if ((*spp)[1] == 'm' || (*spp)[1] == 'M') {
         sub_command = ocs::gdi::SubCommand::CHANGE;
      } else if ((*spp)[1] == 'a' || (*spp)[1] == 'A') {
         sub_command = ocs::gdi::SubCommand::APPEND;
      } else if ((*spp)[1] == 'd' || (*spp)[1] == 'D') {
         sub_command = ocs::gdi::SubCommand::REMOVE;
      } else if ((*spp)[1] == 'r' || (*spp)[1] == 'R') {
         sub_command = ocs::gdi::SubCommand::SET;
      }
      spp = sge_parser_get_next(spp);

      /* is the objectname given in commandline
         supported by this function */
      index = 0;
      while(info_entry[index].object_name) {
         if (!strcmp(info_entry[index].object_name, *spp)) {
            spp = sge_parser_get_next(spp);
            break;
         }
         index++;
      }

      if (!info_entry[index].object_name) {
         fprintf(stderr, "Modification of object " SFQ " not supported\n", *spp);
         sge_free(&(info_entry[1].fields));
         DRETURN(1);
      }

      /* */
      DTRACE;
      ret = qconf_modify_attribute(&alp, from_file, &spp, &ep, sub_command, &(info_entry[index]));
      lFreeElem(&ep);

      /* Error handling */
      if (ret || lGetNumberOfElem(alp)) {
         int exit = 0;

         for_each_ep(aep, alp) {
            FILE *std_x = nullptr;

            if (lGetUlong(aep, AN_status) != STATUS_OK) {
               std_x = stderr;
               exit = 1;
            } else {
               std_x = stdout;
            }
            fprintf(std_x, "%s\n", lGetString(aep, AN_text));
         }
         lFreeList(&alp);
         if (exit) {
            sge_free(&(info_entry[1].fields));
            DRETURN(1);
         }
      }

      sge_free(&(info_entry[1].fields));

      continue;
   }


/*----------------------------------------------------------------------------*/
   /* "-purge" */
   if (strcmp("-purge", *spp) == 0) {

      static object_info_entry info_entry[] = {
         {ocs::gdi::Target::CQ_LIST, SGE_OBJ_CQUEUE, QR_Type, SGE_ATTR_QNAME, QR_name, nullptr, &qconf_sfi, cqueue_xattr_pre_gdi},
         {ocs::gdi::Target::NO_TARGET,           nullptr,        nullptr, nullptr,        NoName,  nullptr, nullptr,    nullptr}
      };

      int index = 0;
      char *object_instance = nullptr;
      char *object = nullptr;
      char *hgroup_or_hostname = nullptr;
      char *attr = nullptr;
      lListElem *cqueue = nullptr;

      /* This does not have to be freed later */
      info_entry[0].fields = CQ_fields;

      spp = sge_parser_get_next(spp);

      /* is the object given in commandline
         supported by this function */
      index = 0;
      while(info_entry[index].object_name) {
         if (!strcmp(info_entry[index].object_name, *spp)) {
            spp = sge_parser_get_next(spp);
         break;
      }
      index++;
   }

   if (!info_entry[index].object_name) {
      ERROR(MSG_QCONF_MODIFICATIONOFOBJECTNOTSUPPORTED_S, *spp);
      DRETURN(1);
   }

   /* parse command line arguments */
   attr = sge_strdup(nullptr, *spp);
   if (attr == nullptr) {
      ERROR(SFNMAX, MSG_QCONF_NOATTRIBUTEGIVEN);
      DRETURN(1);
   }
   spp = sge_parser_get_next(spp);

   object_instance = sge_strdup(nullptr, *spp);

   /* object_instance look like queue@host */
   if ((object = sge_strdup(nullptr, sge_strtok(object_instance, "@"))) != nullptr) {
       hgroup_or_hostname = sge_strdup(nullptr, sge_strtok(nullptr, nullptr));
   }

   if (object == nullptr || hgroup_or_hostname == nullptr) {
      ERROR(MSG_QCONF_GIVENOBJECTINSTANCEINCOMPLETE_S, object_instance);
      sge_free(&attr);
      sge_free(&object_instance);
      sge_free(&object);
      DRETURN(1);
   }

   /* queue_instance no longer neede */
   sge_free(&object_instance);

   if (strcmp("@/", hgroup_or_hostname) == 0) {
      ERROR(MSG_QCONF_MODIFICATIONOFHOSTNOTSUPPORTED_S, hgroup_or_hostname);
      sge_free(&attr);
      sge_free(&object);
      sge_free(&hgroup_or_hostname);
      DRETURN(1);
   }

      /* now get the queue, delete the objects and send the queue back to the master */
      cqueue = cqueue_get_via_gdi(&alp, object);

      if (cqueue == nullptr) {
         ERROR(MSG_CQUEUE_DOESNOTEXIST_S, object);
         sge_free(&attr);
         sge_free(&object);
         sge_free(&hgroup_or_hostname);
         DRETURN(1);
      }

      parse_name_list_to_cull("attribute list", &lp, US_Type, US_name, attr);
      if (cqueue_purge_host(cqueue, &alp, lp, hgroup_or_hostname)) {
         cqueue_add_del_mod_via_gdi(cqueue, &alp, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::SET_ALL);
      } else {
         WARNING(MSG_QCONF_ATTR_ARGS_NOT_FOUND, attr, hgroup_or_hostname);
      }
      lFreeList(&lp);

      /* Error handling */
      for_each_ep(aep, alp) {
            FILE *std_x = nullptr;

            if (lGetUlong(aep, AN_status) != STATUS_OK) {
               std_x = stderr;
            } else {
               std_x = stdout;
            }
            fprintf(std_x, "%s\n", lGetString(aep, AN_text));
         }

      if (cqueue != nullptr) {
         lFreeElem(&cqueue);
      }

      sge_free(&attr);
      sge_free(&object);
      sge_free(&hgroup_or_hostname);
      spp++;
      continue;
   }

/*----------------------------------------------------------------------------*/
      /* "-Msconf file": the scheduler configuration from a file
       *
       * The parse, the field list and sconf_validate_config() are gone from
       * here: qmaster parses the text and sge_mod_sched_configuration() installs
       * it through sconf_set_config(), which validates and refuses with the
       * reason in the answer list - the client check was the same check, run
       * twice (CS-2823). */
      if (strcmp("-Msconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         if (qconf_apply_obj(os, QConfObj::SC, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-msconf": the scheduler configuration in the editor
       *
       * The bare GET in front of the editor is what this step exists to remove:
       * it could not see whether the output was reduced, and wrote back what it
       * had been shown. The read now runs as a stored procedure, which refuses
       * to edit a reduced view, and the write parses in qmaster (CS-2823). The
       * scheduler configuration has no name - it is read as a singleton, which
       * qconf_show_object() takes from the descriptor. */
      if (strcmp("-msconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         if (qconf_edit_obj_procedure(param, QConfObj::SC, "-msconf", nullptr, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mstree", "-Mstree file": the share tree in the editor or from a file
       *
       * What left this block, and where it went (CS-2823):
       *
       * - the bare GET before the editor, which is the hole this step closes:
       *   it could not see a reduced output and wrote back what it was shown.
       *   The read is a stored procedure now and refuses to edit a reduced view.
       * - the choice between the two field lists. The descriptor's
       *   qconf_fields_STN() has made it by format since CS-2808, so the
       *   ternary here was the second copy of the same decision.
       * - id_sharetree() for the nested JSON form, which is now the
       *   descriptor's prepare hook and runs where the parse is.
       * - sge_search_unspecified_node(). check_sharetree() in qmaster reports
       *   exactly that - a node with no name, with the same message - and
       *   sge_mod_sharetree() calls it, so this was the same check run twice.
       * - the ADD-or-MOD probe of CS-2312. sge_add_sharetree() is a one-line
       *   forwarder to sge_mod_sharetree(), which creates the list when there is
       *   none, so the upsert needs no probe and the client needs no extra GET
       *   to decide it.
       *
       * The share tree has no name: it is read as a singleton, which
       * qconf_show_object() takes from the descriptor. */
      if ((strcmp("-mstree", *spp) == 0) || (strcmp("-Mstree", *spp) == 0)) {
         if (strcmp("-mstree", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (qconf_edit_obj_procedure(param, QConfObj::STN, "-mstree", nullptr, uid, gid) != 0) {
               sge_parse_return = 1;
            }
         } else {
            spp = sge_parser_get_next(spp);

            if (qconf_apply_obj(os, QConfObj::STN, *spp) != 0) {
               sge_parse_return = 1;
            }
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mu userset,..." */

      if (strcmp("-mu", *spp) == 0) {
         /* check for adminhost and manager privileges */
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         /* get userset */
         parse_name_list_to_cull("usersets", &lp, US_Type, US_name, *spp);

         /* CS-2823: one round trip per name, which is what the hand written
          * edit_usersets() did - it fetched them all in one GET and then opened
          * one editor per userset. "-mu" is the only interactive switch that
          * takes a list of names, and it is also the whole interactive add: a
          * name that is no userset yet is answered by the read with the type's
          * template and created on exit, as it was before.
          *
          * A name whose edit fails no longer stops the ones behind it, as
          * before - but it now makes the command exit non-zero, where
          * edit_usersets() reported such a failure on stderr and still
          * returned success. */
         const lListElem *us_ep;
         for_each_ep(us_ep, lp) {
            if (qconf_edit_obj_procedure(param, QConfObj::US, "-mu",
                                         lGetString(us_ep, US_name), uid, gid) != 0) {
               sge_parse_return = 1;
            }
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Mu fname" */

      if (strcmp("-Mu", *spp) == 0) {
         /* CS-2310 C2+C3: accept a file OR a directory, upsert each userset.
          * The userset uses the static US_fields, the param sfi and the
          * userset entry validator. */
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::US, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Au fname|dir" */

      if (strcmp("-Au", *spp) == 0) {
         /* CS-2310 C2+C3: accept a file OR a directory, upsert each userset. */
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::US, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Du file|dir": CS-2310 C5 — delete the userset(s) named in the file(s) */

      if (strcmp("-Du", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::US, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }


/*----------------------------------------------------------------------------*/

      /* "-muser username" */

      if (strcmp("-muser", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);

         /* CS-2823: the stored procedure round trip, as -mcal does it, and the
          * same function "-a<obj>" calls. The implicit add of CS-2308 C2 is
          * unchanged; the template it edits comes from the read's descriptor row
          * and is built in qmaster. */
         if (qconf_edit_obj_procedure(param, QConfObj::UU, "-muser", *spp, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mprj projectname" */

      if (strcmp("-mprj", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);

         /* CS-2823: the stored procedure round trip, as -mcal does it, and the
          * same function "-a<obj>" calls. The implicit add of CS-2309 C2 is
          * unchanged; the template it edits comes from the read's descriptor row
          * and is built in qmaster.
          *
          * A failing read sets the exit code now, which the GET path forgot to
          * do here while the "-muser" branch beside it did - the two are one
          * function, so the omission cannot come back. */
         if (qconf_edit_obj_procedure(param, QConfObj::PR, "-mprj", *spp, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }




/*----------------------------------------------------------------------------*/

      /* "-Muser file|dir" */

      if (strcmp("-Muser", *spp) == 0) {
         /* CS-2308 C2+C3: accept a file OR a directory, upsert each user. */

         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::UU, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Duser file|dir": CS-2308 C5 — delete the user(s) named in the file(s) */

      if (strcmp("-Duser", *spp) == 0) {

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::UU, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Mprj file" */

      if (strcmp("-Mprj", *spp) == 0) {
         /* CS-2309 C2+C3: accept a file OR a directory, upsert each project. */

         /* no adminhost/manager check needed here */

         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::PR, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      /* "-Dprj file|dir": CS-2309 C5 — delete the project(s) named in the file(s) */

      if (strcmp("-Dprj", *spp) == 0) {

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::PR, *spp) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sc" */

      if (strcmp("-sc", *spp) == 0) {
         lList *answer_list = nullptr;

         /* CS-2817: a sub-procedure. The table is neither a name list nor one
          * object, and its column alignment writes into the field list, which
          * must not be the shared static one inside qmaster. */
         qconf_op op;
         op.token = "-sc";
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::CENTRY_LIST);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         if (!ok) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scatl complex_name_list" */

      if (strcmp("-scatl", *spp) == 0) {
         lList *answer_list = nullptr;

         /* CS-2819: the aligned table sub-procedure, the same one -sc uses. The
          * list has a JSON schema id of its own, which the descriptor carries. */
         qconf_op op;
         op.token = "-scatl";
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::CATEGORY_LIST);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         if (!ok) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scal calendar_name" */
      if (strcmp("-scal", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_status_classic(qconf_show_object(os, param, QConfObj::CAL, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-scall" */


/*-----------------------------------------------------------------------------*/
      /* "-sconf host_list" || "-mconf host_list" || "-aconf host_list" || "-Mconf file_list" || "-Aconf file_list" */
      /* file list is also host list */
      if ((strcmp("-sconf", *spp) == 0) ||
         (strcmp("-aconf", *spp) == 0) ||
         (strcmp("-mconf", *spp) == 0) ||
         (strcmp("-Mconf", *spp) == 0) ||
         (strcmp("-Aconf", *spp) == 0)) {
         typedef enum {
            ACTION_sconf = 0,
            ACTION_aconf,
            ACTION_mconf,
            ACTION_Aconf,
            ACTION_Mconf
         } action_enum;
         action_enum action = ACTION_sconf;
         char *host_list = nullptr;
         int ret, first = 1;
         lListElem *host_ep;
         const char *hostname;

         if (!strcmp("-aconf", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            action = ACTION_aconf;
         } else if (!strcmp("-mconf", *spp)) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);
            action = ACTION_mconf;
         } else if (!strcmp("-Aconf", *spp)) {
            action = ACTION_Aconf;
         } else if (!strcmp("-Mconf", *spp)) {
            action = ACTION_Mconf;
         }

         if (!sge_next_is_an_opt(spp))  {
            spp = sge_parser_get_next(spp);
            host_list = sge_strdup(nullptr, *spp);
         } else {
            host_list = sge_strdup(nullptr, SGE_GLOBAL_NAME);
         }

         /* host_list might look like host1,host2,... */
         host_ep = lCreateElem(EH_Type);

         for ((cp = sge_strtok(host_list, ",")); cp && *cp;
             (cp = sge_strtok(nullptr, ","))) {

            if (!first) {
               os << "\n";
            }
            first = 0;

            /* CS-2311 C2+C3: -Aconf/-Mconf take a file OR a directory; the host
             * key is the file basename and each is upserted (flag 3). The basename
             * resolution happens per file inside qconf_conf_apply_path(). */
            if (action == ACTION_Aconf || action == ACTION_Mconf) {
               if (qconf_conf_apply_path(os, cp) != 0) {
                  sge_parse_return = 1;
               }
               continue;
            }

            /* -sconf/-aconf/-mconf: cp is a host name */
            lSetHost(host_ep, EH_name, cp);

            switch ((ret=sge_resolve_host(host_ep, EH_name))) {
            case CL_RETVAL_OK:
               break;
            default:
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_SS, cp, cl_get_error_text(ret));
               fprintf(stderr, "\n");
               break;
            }
            hostname = lGetHost(host_ep, EH_name);

            if (ret != CL_RETVAL_OK && (action == ACTION_sconf || action == ACTION_aconf) ) {
               sge_parse_return = 1;
               continue;
            }

            if (action == ACTION_sconf) {
               /* CS-2820: only the leaf moved. What this block shares with
                * -aconf/-mconf/-Aconf/-Mconf is the argument handling - the
                * comma separated host list, sge_resolve_host() per entry and the
                * blank line between blocks - not the object, so splitting the
                * block would have copied exactly the parts most likely to drift.
                * The "global" alias is resolved here because it is an argument
                * spelling, not a property of the object. */
               const char *cfn = (strcasecmp(hostname, "global") == 0) ? SGE_GLOBAL_NAME : hostname;

               /* Unlike -scal, a configuration that does not exist must not stop
                * the parse: the host list may name several hosts and the others
                * still have to be shown. The leaf this replaced recorded the
                * failure and carried on, so NOT_FOUND - which the helper reports
                * by returning true - sets the exit code here instead of
                * aborting. A write failure still exits, inside the helper. */
               if (qconf_show_status_classic(qconf_show_object(os, param, QConfObj::CONF, cfn),
                                             &sge_parse_return)) {
                  sge_parse_return = 1;
               }
            } else if (action == ACTION_aconf || action == ACTION_mconf) {
               /* CS-2311 C2: "-aconf" and "-mconf" both upsert - add when the
                * host has no configuration yet, modify when it has. That stays
                * true without a flag now, because sge_mod_configuration() is
                * itself an upsert (CS-2823).
                *
                * The bare GET this replaces is the hole the step closes: it
                * could not see whether the output was reduced and wrote back
                * what it had been shown. The "global" spelling is resolved the
                * same way the read resolves it, so both halves of the round trip
                * name the object alike. */
               const char *cfn = (strcasecmp(hostname, "global") == 0) ? SGE_GLOBAL_NAME : hostname;

               if (qconf_edit_obj_procedure(param, QConfObj::CONF,
                                            (action == ACTION_aconf) ? "-aconf" : "-mconf",
                                            cfn, uid, gid) != 0) {
                  sge_parse_return = 1;
               }
            }

         } /* end for */

         sge_free(&host_list);
         lFreeElem(&host_ep);

         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sckpt ckpt_name" */
      if (strcmp("-sckpt", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_status_classic(qconf_show_object(os, param, QConfObj::CK, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sckptl" */
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
      /* "-dconf config_list" */
      if (strcmp("-dconf", *spp) == 0) {
         char *host_list = nullptr;
         lListElem *host_ep = nullptr;
         lList *conf_list = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp))  {
            spp = sge_parser_get_next(spp);

            host_list = sge_strdup(nullptr, *spp);
            host_ep = lCreateElem(EH_Type);

            /* CS-2754: collect the names, then delete them in one request */
            for ((cp = sge_strtok(host_list, ",")); cp && *cp;
                (cp = sge_strtok(nullptr, ","))) {

               lSetHost(host_ep, EH_name, cp);

               switch (sge_resolve_host(host_ep, EH_name)) {
               case CL_RETVAL_OK:
                  break;
               default:
                  fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, cp);
                  fprintf(stderr, "\n");
                  sge_parse_return = 1;
                  break;
               }
               lAddElemHost(&conf_list, CONF_name, lGetHost(host_ep, EH_name), CONF_Type);
            } /* end for */

            /*
             * A failed delete counts towards the exit code, as it does for
             * every other -d<obj> switch (they all fold show_answer_list()
             * into sge_parse_return) and as -Dconf already did. -dconf alone
             * used to throw the result away and exit 0 after telling the
             * administrator that it could not delete anything.
             */
            if (delete_configs(conf_list) != 0) {
               sge_parse_return = 1;
            }

            lFreeList(&conf_list);
            sge_free(&host_list);
            lFreeElem(&host_ep);
         }
         else {
            fprintf(stderr, "%s\n", MSG_ANSWER_NEEDHOSTNAMETODELLOCALCONFIG);
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-Dconf file|dir": CS-2311 C5 — delete the host config(s) named by the
       * file(s); the host key is the file basename. */
      if (strcmp("-Dconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_conf_delete_path(os, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-se exec_server" */
      if (strcmp("-se", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         /* resolve host */
         hep = lCreateElem(EH_Type);
         lSetHost(hep, EH_name, *spp);

         switch (sge_resolve_host(hep, EH_name)) {
         case CL_RETVAL_OK:
            break;
         default:
            fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(hep, EH_name));
            fprintf(stderr, "\n");
            lFreeElem(&hep);
            DRETURN(1);
         }

         if (qconf_show_object(os, param, QConfObj::EH, lGetHost(hep, EH_name)) != QConfShowStatus::OK) {
            sge_parse_return = 1;
         }
         lFreeElem(&hep);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-secl" */
      if (strcmp("-secl", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-secl";

         /* CS-2820: a sub-procedure rather than a table row. The output is a
          * table with a hand written header and fixed column widths, which a
          * flatfile spooling instruction cannot reproduce without re-deriving
          * them from the data - and that is what a golden file would catch. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::EVENT_CLIENT_LIST);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         if (!ok) {
            /* the messages the switch used to write to stderr itself: an empty
             * list for plain output, and the reason a read failed. Unlike before,
             * both now set the exit code (CS-2820). */
            answer_list_output(&answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sel" */
/*----------------------------------------------------------------------------*/
      /* "-sep" */
      if (strcmp("-sep", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-sep";

         /* CS-2820: a sub-procedure rather than a table row, for the same reason
          * as -secl - a hand written header, two fixed rule lines and a SUM row
          * that is an aggregate over the rows rather than one of them. The -cb
          * flag travels in the parameter bundle, so it needs no second entry. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::PROCESSOR_LIST);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         if (!ok) {
            /* "no execution hosts defined" used to go to stderr without setting
             * the exit code; now it does both (CS-2820) */
            answer_list_output(&answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-srqsl " */
/*----------------------------------------------------------------------------*/
      /* "-srqs [rqs_name,...]" */
      if (strcmp("-srqs", *spp) == 0) {
         /* CS-2819: a table driven read of several objects. No argument shows
          * every resource quota set, which is what an empty filter does. */
         qconf_op op;
         op.token = "-srqs";
         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            op.args.emplace_back(*spp);
         }
         param.set_operation(op);

         lList *answer_list = nullptr;
         if (!qconf_dispatch_procedure(os, param, &answer_list)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-stl " */
      if (strcmp("-stl", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-stl";

         /* CS-2820: a sub-procedure, and the one read of the set whose data is
          * not a configuration object - the thread pools are qmaster's own
          * state. The server side model reaches them through a provider the
          * daemon registers at startup, because a library must not call into a
          * daemon. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::THREAD_POOL_LIST);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         if (!ok) {
            answer_list_output(&answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-srole role_name" */
      if (strcmp("-srole", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_status_classic(qconf_show_object(os, param, QConfObj::RL, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-srolel" */
/*-----------------------------------------------------------------------------*/
      /* "-sp pe_name" */
      if (strcmp("-sp", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_status_classic(qconf_show_object(os, param, QConfObj::PE, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-spl" */

/*-----------------------------------------------------------------------------*/
      /* "-scel" */
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

      /* "-sstnode node_path[,...]"  show sharetree node */

      if (strcmp("-sstnode", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-sstnode";
         spp = sge_parser_get_next(spp);
         op.args.emplace_back(*spp);

         /* CS-2820: a sub-procedure. The paths are taken apart in the model,
          * because a path that names no node has to be reported and the view is
          * handed no answer list - the renderer used to write that line to its
          * own stderr, which is nowhere once it runs in qmaster. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::SHARETREE_NODES);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-rsstnode node_path[,...]"  show sharetree node */

      if (strcmp("-rsstnode", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-rsstnode";
         spp = sge_parser_get_next(spp);
         op.args.emplace_back(*spp);

         /* CS-2820: a sub-procedure. The paths are taken apart in the model,
          * because a path that names no node has to be reported and the view is
          * handed no answer list - the renderer used to write that line to its
          * own stderr, which is nowhere once it runs in qmaster. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::SHARETREE_NODES);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
      /* "-bonsai" This is the undocumented switch of showing the share tree
         we keep this switch  for compatibility reasons */

      if (strcmp("-bonsai", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-bonsai";

         /* CS-2820: a sub-procedure. -bonsai is the undocumented older spelling of -sst and is kept for
          * compatibility; both render the same tree through the same procedure. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::SHARETREE_TREE);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-sst" This is the documented switch of showing the share tree */

      if (strcmp("-sst", *spp) == 0) {
         lList *answer_list = nullptr;
         qconf_op op;

         op.token = "-sst";

         /* CS-2820: a sub-procedure. the indented tree view. -sstree is a different switch: it renders
          * the share tree as an object and has had JSON since CS-2313a. */
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::SHARETREE_TREE);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
      /* "-su [list_name[,list_name,...]]" */

      if (strcmp("-su", *spp) == 0) {
         /* CS-2819: the same table driven read as -srqs; the option table demands
          * at least one name here, so the "show everything" case cannot occur. */
         spp = sge_parser_get_next(spp);

         qconf_op op;
         op.token = "-su";
         op.args.emplace_back(*spp);
         param.set_operation(op);

         lList *answer_list = nullptr;
         if (!qconf_dispatch_procedure(os, param, &answer_list)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      // -scat id
      if (strcmp("-scat", *spp) == 0) {
         lList *answer_list = nullptr;

         /* CS-2819: a table driven read whose key is a number; the id is parsed
          * where the filter is built, so the client passes the text through. */
         spp = sge_parser_get_next(spp);
         qconf_op op;
         op.token = "-scat";
         op.args.emplace_back(*spp);
         param.set_operation(op);

         if (!qconf_dispatch_procedure(os, param, &answer_list)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

      /* "-sce attribute"  */
      if (strcmp("-sce", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         /* CS-2817: a plain single-object read. The hand written code rendered it
          * with CE_fields and qconf_ce_sfi, which is what the CE row holds, so
          * this needs no sub-procedure - only a row in qconf_read_table. It is
          * dispatched here rather than through qconf_show_object(), because that
          * one reports with answer_list_output() and would add an "error: "
          * prefix where this block has always printed the bare message. */
         qconf_op op;
         op.token = "-sce";
         op.args.emplace_back(*spp);
         param.set_operation(op);
         if (!qconf_dispatch_procedure(os, param, &answer_list)) {
            show_answer(answer_list);
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mce centry"  */
      if (strcmp("-mce", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);

         /* CS-2823: the stored procedure round trip, as -mcal does it, and the
          * same function "-a<obj>" calls. The implicit add of CS-2303 C2 is
          * unchanged; the template it edits comes from the read's descriptor row
          * and is built in qmaster. */
         if (qconf_edit_obj_procedure(param, QConfObj::CE, "-mce", *spp, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Mce fname|dir": CS-2303 C2/C3 — add/modify from a file or directory */
      if (strcmp("-Mce", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::CE, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dce attribute[,attribute,...]"  */
      if (strcmp("-dce", *spp) == 0) {
         /* CS-2303 C4: accept a comma-separated list of complex entry names. */
         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);
         sge_parse_return |= qconf_delete_names(QConfObj::CE, *spp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-Dce file|dir": CS-2303 C5 — delete the complex entry(s) named in the file(s) */
      if (strcmp("-Dce", *spp) == 0) {
         qconf_is_manager(username);
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::CE, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-ace [attribute]"  */
      if (strcmp("-ace", *spp) == 0) {
         /* CS-2303 C1: the complex entry name is optional; template default. */
         const char *ce_name = "template";

         qconf_is_adminhost(qualified_hostname);
         qconf_is_manager(username);

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            ce_name = *spp;
         }

         /* CS-2823: template, editor and write procedure in one - see
          * qconf_edit_obj_procedure(), the single function "-a<obj>" and "-m<obj>"
          * share. The template the upsert of CS-2303 C2 edits is built in qmaster
          * now, from the read's descriptor row. */
         if (qconf_edit_obj_procedure(param, QConfObj::CE, "-ace", ce_name, uid, gid) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-Ace fname|dir": CS-2303 C2/C3 — add/modify from a file or directory */
      if (strcmp("-Ace", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::CE, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /*
       * Hostgroup parameters
       */

      /* "-shgrpl" */

      /* "-mhgrp group"  */
      if (strcmp("-mhgrp", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: the stored procedure round trip, the same function "-ahgrp"
          * calls. A name that is no host group yet is answered by the read with
          * the type's template and created on exit, which is what "-m<obj>" has
          * done for every other type since CS-2299 C2 and what qconf.md has
          * described all along - the host group only joined now. */
         if (qconf_edit_obj_procedure(param, QConfObj::HGRP, "-mhgrp", *spp, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

      /* "-Mhgrp fname|dir": CS-2306 C2/C3 — add/modify host group(s) from a file or directory */
      if (strcmp("-Mhgrp", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::HGRP, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-ahgrp group"  */
      if (strcmp("-ahgrp", *spp) == 0) {
         /* the name is optional; when omitted a template named "@template" is
          * offered for editing, and the editor is where it gets its real name */
         const char *group = "@template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            group = *spp;
         }

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: template, editor and write procedure in one - see
          * qconf_edit_obj_procedure(), the single function "-a<obj>" and
          * "-m<obj>" share. The template is built in qmaster from the read's
          * descriptor row, with the name check off for the reason the flag that
          * used to stand here gave - see qconf_template_HGRP(). An impossible
          * name is refused when the edited text is written. */
         if (qconf_edit_obj_procedure(param, QConfObj::HGRP, "-ahgrp", group, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

      /* "-Ahgrp fname|dir": CS-2306 C2/C3 — add/modify host group(s) from a file or directory */
      if (strcmp("-Ahgrp", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::HGRP, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-dhgrp group,..." : CS-2306 C4 — comma-separated list of host groups */
      if (strcmp("-dhgrp", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);
         /* parse_name_list_to_cull() rejects an empty argument, lString2List() would not */
         parse_name_list_to_cull("host groups to del", &lp, HGRP_Type, HGRP_name, *spp);
         sge_parse_return |= qconf_delete_list(QConfObj::HGRP, &lp);

         spp++;
         continue;
      }

      /* "-Dhgrp file|dir": CS-2306 C5 — delete the host group(s) named in the file(s) */
      if (strcmp("-Dhgrp", *spp) == 0) {
         qconf_is_manager(username);
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::HGRP, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-shgrp group"  */
      if (strcmp("-shgrp", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         /* CS-2816: a sub-procedure, because the JSON form adds keys derived
          * from the member list and the descriptor table cannot express that.
          * show_answer() stays: it writes the bare text of the last message,
          * without the "error: " prefix answer_list_output() would add, and the
          * exit status has always come from its return value. */
         qconf_op op;
         op.token = "-shgrp";
         op.args.emplace_back(*spp);
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::HGROUP_SHOW);
         (void) qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /* Are these two options still supported?  qconf doesn't recognise them */
      /* "-shgrp_tree" */
      if (strcmp("-shgrp_tree", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         /* CS-2816: a sub-procedure - this view needs every host group, not the
          * one named, so it cannot be a row in qconf_read_table. */
         qconf_op op;
         op.token = "-shgrp_tree";
         op.args.emplace_back(*spp);
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::HGROUP_TREE);
         (void) qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /* "-shgrp_resolved" */
      if (strcmp("-shgrp_resolved", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         /* CS-2816: a sub-procedure - this view needs every host group, not the
          * one named, so it cannot be a row in qconf_read_table. */
         qconf_op op;
         op.token = "-shgrp_resolved";
         op.args.emplace_back(*spp);
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::HGROUP_RESOLVED);
         (void) qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /*
       * "-shgrp_why <group> <host>"
       *
       * CS-2680, N-I-7/N-I-8. Once membership can be a rule instead of an
       * enumeration, the definition no longer answers "why is this host a
       * member" by inspection. The return value distinguishes member from
       * non-member so the command is usable in a control flow without taking
       * the output apart -- which is why a host that is simply not a member is
       * not an error here, only a non-zero exit.
       */
      if (strcmp("-shgrp_why", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         const char *why_group = *spp;
         spp = sge_parser_get_next(spp);

         /* CS-2816: a sub-procedure. The exit status used to come from a bool the
          * client got back; it now rides on the answer list, where the model adds
          * an ANSWER_QUALITY_END element for a host that is no member - non-OK
          * status, so show_answer() returns 1, and END quality, so it prints
          * nothing. A missing group is reported the loud way, as before. */
         qconf_op op;
         op.token = "-shgrp_why";
         op.args.emplace_back(why_group);
         op.args.emplace_back(*spp);
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::HGROUP_WHY);
         (void) qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /*
       * Cluster Queue parameter
       */

      /* "-sql" */

      /* "-mq cqueue"  */
      if (strcmp("-mq", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: the stored procedure round trip, the same function "-aq"
          * calls. A name that is no cluster queue yet is answered by the read
          * with the type's template and created on exit - the upsert every
          * other type has had since CS-2299 C2. */
         if (qconf_edit_obj_procedure(param, QConfObj::CQ, "-mq", *spp, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

      /* "-Mq fname|dir": CS-2305 C2/C3 — add/modify cqueue(s) from a file or directory */
      if (strcmp("-Mq", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::CQ, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-aq cqueue"  */
      if (strcmp("-aq", *spp) == 0) {
         /* the name is optional; when omitted a template named "template" is
          * offered for editing and the editor gives it its real name */
         const char *name = "template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* CS-2823: template, editor and write procedure in one - see
          * qconf_edit_obj_procedure(). The template is the one cqueue_add()
          * built in the client, now assembled in qmaster from the read's
          * descriptor row: the object plus the attribute defaults. */
         if (qconf_edit_obj_procedure(param, QConfObj::CQ, "-aq", name, uid, gid) != 0) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

      /* "-Aq fname|dir": CS-2305 C2/C3 — add/modify cqueue(s) from a file or directory */
      if (strcmp("-Aq", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_apply_obj(os, QConfObj::CQ, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* CS-23xx: "-S<obj> name|dir" — bulk export, the inverse of -A<obj>/-M<obj>.
       * Read-only (no manager check), like -s<obj>. Each handler passes the object's
       * IMPORT field list so the exported files re-import unchanged. */
      if (strcmp("-Scal", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::CAL, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Sckpt", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::CK, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Sce", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::CE, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Se", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         /* exclude the template and global pseudo-hosts (KTD5); explicit "exechost"
          * JSON $id (EH shares its name field with adminhost). */
         if (qconf_export_obj(os, QConfObj::EH, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Shgrp", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::HGRP, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Sp", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::PE, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Sprj", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::PR, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Sq", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::CQ, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Srole", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::RL, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Srqs", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         /* rqs files carry a rule-set list, so export with list framing (as_list). */
         if (qconf_export_obj(os, QConfObj::RQS, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Su", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::US, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Suser", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj(os, QConfObj::UU, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      /* singletons: a directory argument is rejected (R6) */
      if (strcmp("-Sstree", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         /* sharetree writes the root node (is_root=true) with the name/value sfi */
         if (qconf_export_obj_singleton(os, QConfObj::STN, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      if (strcmp("-Ssconf", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_export_obj_singleton(os, QConfObj::SC, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
      /* bespoke config export (R7): basename keying, global + per-host */
      if (strcmp("-Sconf", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         if (qconf_conf_export(os, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-dq cqueue,..." : CS-2305 C4 — comma-separated list of cqueue names */
      if (strcmp("-dq", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         sge_parse_return |= qconf_delete_names(QConfObj::CQ, *spp);

         spp++;
         continue;
      }

      /* "-Dq file|dir": CS-2305 C5 — delete the cqueue(s) named in the file(s) */
      if (strcmp("-Dq", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         spp = sge_parser_get_next(spp);
         if (qconf_delete_obj(os, QConfObj::CQ, *spp) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

      /* "-sq [pattern[,pattern,...]]" */
      if (strcmp("-sq", *spp) == 0) {
         lList *answer_list = nullptr;

         /* CS-2818: a sub-procedure. One switch with four output shapes - a
          * template, cluster queues, the instances of a host pattern, the
          * instances of a resolved queue domain - which the descriptor table
          * cannot express. The references stay one argument each, as typed;
          * splitting them on commas is the model's job, since it builds the
          * filter from them. */
         qconf_op op;
         op.token = "-sq";
         while (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            op.args.emplace_back(*spp);
         }
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::CQUEUE_SHOW);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         /* show_answer() prints the bare message and returns non-zero for a
          * non-OK status, which is how the hand written block set the exit code.
          * A reference that matched nothing fails the call, so nothing has been
          * rendered and the message is all there is to show. */
         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

      /* "-cq destin_id[,destin_id,...]" */
      if (strcmp("-cq", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         lString2List(*spp, &lp, ID_Type, ID_str, ", ");
         for_each_rw (ep, lp) {
            lSetUlong(ep, ID_action, QI_DO_CLEAN);
         }
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::TRIGGER, ocs::gdi::SubCommand::NONE,
                       &lp, nullptr, nullptr);
         if (answer_list_has_error(&alp)) {
            sge_parse_return = 1;
         }
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }

      if (strcmp("-sds", *spp) == 0) {
         lList *answer_list = nullptr;

         /* CS-2819: a sub-procedure. The report is the output and a report that
          * is not empty also sets the exit code, which travels as an
          * ANSWER_QUALITY_END answer - show_answer() counts it without printing
          * anything of its own. */
         qconf_op op;
         op.token = "-sds";
         param.set_operation(op);
         param.set_sub_procedure_name(ocs::QConfParameter::QUEUE_SICK);
         const bool ok = qconf_dispatch_procedure(os, param, &answer_list);
         param.set_sub_procedure_name("");

         sge_parse_return |= show_answer(answer_list);
         if (!ok) {
            sge_parse_return = 1;
         }
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-suser username" */

      if (strcmp("-suser", *spp) == 0) {
         lList *uList = nullptr;
         bool first = true;

         spp = sge_parser_get_next(spp);

         lString2List(*spp, &uList, ST_Type, ST_name , ", ");
         for_each_ep_lv(uep, uList) {
            // the users are separated by a blank line, the first one is not preceded by one
            if (first) {
               first = false;
            } else {
               os << "\n";
            }
            /* a user that does not exist has never changed the exit code here */
            (void)qconf_show_object(os, param, QConfObj::UU, lGetString(uep, ST_name));
         }

         lFreeList(&uList);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-sprj projectname" */

      if (strcmp("-sprj", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_object(os, param, QConfObj::PR, *spp) != QConfShowStatus::OK) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
      /* "-sul" */


/*----------------------------------------------------------------------------*/

       /* "-suserl" */


/*----------------------------------------------------------------------------*/

       /* "-sprjl" */


/*----------------------------------------------------------------------------*/

      /* "-tsm" trigger scheduler monitoring */

      if (strcmp("-tsm", *spp) == 0) {
         /* no adminhost/manager check needed here */
         alp = ocs::gdi::Client::gdi_tsm();
         answer_list_on_error_print_or_exit(&alp, stderr);
         lFreeList(&alp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-huh?" - unreachable: qconf_parse_args() rejects a switch that is not
       * in the table, and every switch in the table has a block above. Kept as
       * the backstop it always was, for a row added without its block. */

      ERROR(MSG_ANSWER_INVALIDOPTIONARGX_S, *spp);
      fprintf(stderr, MSG_SRC_X_HELP_USAGE_S , "qconf");
      fprintf(stderr, "\n");
      DRETURN(1);
   }

   /* The command line was only understood up to here. The operations before the
    * bad token have run, so the complaint comes after their output - which is
    * where it came out when the loop parsed and executed in one pass.
    * MISSING_ARGUMENT needs nothing: the truncated operation was handed over
    * and its own block has already reported it. */
   if (parse_status == QConfParseStatus::UNKNOWN_OPTION) {
      ERROR(MSG_ANSWER_INVALIDOPTIONARGX_S, bad_token.c_str());
      fprintf(stderr, MSG_SRC_X_HELP_USAGE_S, "qconf");
      fprintf(stderr, "\n");
      lFreeList(&alp);
      DRETURN(1);
   }

   lFreeList(&alp);
   DRETURN(sge_parse_return);
}

/***********************************************************************/

static void parse_name_list_to_cull(const char *name, lList **lpp, lDescr *dp, int nm, char *s) {
   DENTER(TOP_LAYER);

   char *cp2 = nullptr;
   lListElem *ep = nullptr;
   int pos;
   int dataType;


   *lpp = lCreateList(name, dp);
   cp2 = sge_strtok(s, ",");
   ep = lCreateElem(dp);

   pos = lGetPosInDescr(dp, nm);
   dataType = lGetPosType(dp,pos);
   switch (dataType) {
      case lStringT:
         DPRINTF("parse_name_list_to_cull: Adding lStringT type element\n");
         lSetString(ep, nm, cp2);
         break;
      case lHostT:
         DPRINTF("parse_name_list_to_cull: Adding lHostT type element\n");
         lSetHost(ep, nm, cp2);
         break;
      default:
         DPRINTF("parse_name_list_to_cull: unexpected data type\n");
         break;
   }
   lAppendElem(*lpp, ep);

   while ((cp2 = sge_strtok(nullptr, ",")) != nullptr) {
      ep = lCreateElem(dp);
      switch (dataType) {
         case lStringT:
            DPRINTF("parse_name_list_to_cull: Adding lStringT type element\n");
            lSetString(ep, nm, cp2);
            break;
         case lHostT:
            DPRINTF("parse_name_list_to_cull: Adding lHostT type element\n");
            lSetHost(ep, nm, cp2);
            sge_resolve_host(ep, EH_name);
            break;
         default:
            DPRINTF("parse_name_list_to_cull: unexpected data type\n");
            break;
      }
      lAppendElem(*lpp, ep);
   }

   DRETURN_VOID;
}

/****************************************************************************/
static int sge_next_is_an_opt(char **pptr) {
   DENTER(TOP_LAYER);

   if (!*(pptr+1)) {
      DRETURN(1);
   }

   if (**(pptr+1) == '-') {
      DRETURN(1);
   }

   DRETURN(0);
}

/****************************************************************************/
static int sge_error_and_exit(const char *ptr) {
   DENTER(TOP_LAYER);

   fflush(stderr);
   fflush(stdout);

   if (ptr) {
      fprintf(stderr, "%s\n", ptr);
      fflush(stderr);
   }

   fflush(stderr);
   sge_exit(1);
   DRETURN(1); /* to prevent warning */
}

/**
 * @brief Add/remove hosts in a reserved host group
 *
 * CS-2438. Admin and submit hosts live in the reserved host groups
 * "@admin_hosts" and "@submit_hosts", so qconf -ah/-dh/-as/-ds modify those
 * groups instead of the retired AH_LIST/SH_LIST targets.
 * The request is a GDI MOD on HGRP_LIST with the sublist sub-command
 * APPEND or REMOVE -- the mechanism behind "qconf -aattr/-dattr". It is
 * deliberately NOT a read-modify-write of HGRP_host_list in the client:
 * read-modify-write loses a concurrent "-as" issued between the read and the
 * write, which the old dedicated ADD/DEL target could not do. The qmaster
 * merges the sublist under its own lock (attr_mod_sub_list() via
 * hgroup_mod_hostlist()), so two concurrent additions cannot lose each other.
 * Privilege is unchanged: AH_LIST, SH_LIST and HGRP_LIST are labels of the
 * same manager-only case in sge_c_gdi.cc, so redirecting the target neither
 * grants nor removes any right.
 * CS-2754: ONE request for the whole name list, not one per host. The list
 * syntax always existed, the bulk behind it did not: "qconf -ah h1,...,h1000"
 * used to be a thousand round trips, each of which made the qmaster run
 * hgroup_mod() with its cache refresh and its event.
 *
 * The operation is atomic as a consequence. "qconf -ah h1,h2" with h1 already a
 * member used to add h2 and refuse h1; it now refuses both. That is the
 * behaviour these two groups are documented to have -- a script writing
 * "qconf -ds host || alert" has to fail when the removal did not happen -- and
 * it is the safer half of the trade for a security relevant list.
 *
 * The other half of the trade is the diagnosis. hgroup_reserved_delta_is_strict()
 * does report one error per offending entry, but only the LAST of them reaches
 * the client: sge_gdi_add_mod_generic() propagates lLast(tmp_alp) alone when the
 * modifier fails, because earlier elements may be success messages. So a list
 * with several bad names is refused naming one of them, where the per-host
 * version named every one. Every answer that does arrive is printed below, so
 * lifting that restriction in the qmaster is all it would take.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param arglp HR_Type list of names as parsed from the command line
 * @param group ADMIN_HOSTGROUP or SUBMIT_HOSTGROUP
 * @param sub_command APPEND to add, REMOVE to delete
 * @param what "administrative host" / "submit host", for messages
 *
 * @return true if every named host was resolved and the request succeeded
 */
static bool mod_reserved_hgroup(std::ostream &os, lList *arglp, const char *group,
                                ocs::gdi::SubCommand sub_command, const char *what) {
   DENTER(TOP_LAYER);

   bool ret = true;
   lList *hosts = nullptr;

   /* resolve first, locally, so an unusable name never reaches the request */
   for_each_rw_lv(argep, arglp) {
      const char *name = lGetHost(argep, HR_name);

      if (name == nullptr) {
         ret = false;
         continue;
      }

      /*
       * CS-2680, N-I-9. The class of the entry decides, and it is decided
       * *before* anything is resolved.
       *
       * A host group reference is passed through untouched: "-ah @group" is
       * legal (see the plan for CS-2438), and sge_resolve_host() would fail on
       * it. hgroup_mod() checks in the qmaster that the group exists and that
       * the reference introduces no cycle, so nothing has to be validated here.
       *
       * A matcher is passed through for a different reason: it describes a set
       * of hosts rather than naming one, so there is nothing to look up. The
       * qmaster normalises and validates it (ocs::Matcher::prepare()), which is
       * where the domain rules and the message catalogue are.
       *
       * Asking the classification rather than the resolver is the whole point.
       * sge_resolve_host() skips whatever ocs::is_expression() calls an
       * expression, so a matcher used to get through only when it happened to
       * carry a metacharacter: "host:gpu*" was let past by its star, while
       * "host:gpu001" - a perfectly good matcher describing exactly one name -
       * was sent to the name service and failed there.
       */
      switch (ocs::classify_member(name)) {
         case ocs::MemberClass::GROUP_REFERENCE:
         case ocs::MemberClass::MATCHER:
            break;

         case ocs::MemberClass::LITERAL_HOST:
            if (sge_resolve_host(argep, HR_name) != CL_RETVAL_OK) {
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, name);
               fprintf(stderr, "\n");
               ret = false;
               continue;
            }
            name = lGetHost(argep, HR_name);
            break;
      }

      lAddElemHost(&hosts, HR_name, name, HR_Type);
   }

   if (hosts == nullptr) {
      DRETURN(ret);
   }

   /* the names this request carries, kept for the report: the list itself
    * belongs to the request element once it is chained in below */
   lList *sent = lCopyList("hosts sent", hosts);

   /* the reduced element the qmaster merges: the group name and every entry to
    * add to or remove from its host list */
   lList *lp = lCreateList("host group to modify", HGRP_Type);
   lListElem *hgrp = lAddElemHost(&lp, HGRP_name, group, HGRP_Type);
   lSetList(hgrp, HGRP_host_list, hosts);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::MOD,
                                          sub_command, &lp, nullptr, nullptr);

   lListElem *aep = lFirstRW(alp);
   answer_exit_if_not_recoverable(aep);
   if (answer_get_status(aep) == STATUS_OK) {
      /*
       * CS-2645: every command reports either success or failure. This used
       * to confirm APPEND only, on the assumption that the retired targets
       * had said nothing on a successful -dh/-ds. They did say something:
       * their delete path printed the whole answer list of the qmaster,
       * INFO elements included, so a successful remove was confirmed with
       * "<user>@<host> removed \"<host>\" from submit host list". Staying
       * silent here was therefore not the preservation of the old behaviour
       * but a change of it, and it left a successful delete
       * indistinguishable from one that did nothing.
       *
       * Still one line per host, from the list this request carried: the
       * request became a batch, the report to the administrator did not.
       *
       * CS-2680, N-I-12. What the qmaster said on the way is printed first.
       * Notes and warnings live in that answer list - the transformation of a
       * matcher by the domain rules, the collapse of a duplicate, the catch-all,
       * the scope of a matcher in a reserved group - and dropping them made
       * "qconf -ah host:*" confer a standing grant on every host that can
       * reach the qmaster without saying a word. Everything but the last element
       * is printed; the last is the generic confirmation of the object layer,
       * which these commands deliberately replace with the per-host lines below.
       *
       * The return value stays what it is. A note or a warning must not turn a
       * change that was carried out into a reported failure.
       */
      const lListElem *aep_msg;
      const lListElem *aep_last = lLast(alp);

      for_each_ep(aep_msg, alp) {
         if (aep_msg == aep_last) {
            break;
         }
         fprintf(stderr, "%s\n", lGetString(aep_msg, AN_text));
      }

      const lListElem *host;

      for_each_ep(host, sent) {
         /* CS-2645 follow-up: this reports success, so it belongs on stdout.
          * qconf_info(os, ) writes there and appends the newline itself. */
         if (sub_command == ocs::gdi::SubCommand::APPEND) {
            qconf_info(os, MSG_QCONF_XADDEDTOYLIST_SS, lGetHost(host, HR_name), what);
         } else {
            qconf_info(os, MSG_QCONF_XREMOVEDFROMYLIST_SS, lGetHost(host, HR_name), what);
         }
      }
   } else {
      /* every answer, not just the first: the qmaster names each offending host */
      const lListElem *answer;

      for_each_ep(answer, alp) {
         fprintf(stderr, "%s\n", lGetString(answer, AN_text));
      }
      ret = false;
   }

   lFreeList(&sent);
   lFreeList(&lp);
   lFreeList(&alp);

   DRETURN(ret);
}

/* ------------------------------------------------------------ */

/* add_host_of_type() removed with CS-2438: -ah/-as now modify the reserved
 * host groups through mod_reserved_hgroup(). del_host_of_type() stays -- it is
 * still the implementation of -de for EH_LIST. */
/* ------------------------------------------------------------ */

/** @brief Delete every named host, the `qconf -de` switch
 *
 * CS-2753: ONE request for the whole name list. The list syntax always existed,
 * the bulk behind it did not -- this used to send one GDI DEL per name, and each
 * of those made the qmaster rebuild `@exec_hosts` from scratch.
 *
 * Nothing is given up for it, unlike in the host group case of CS-2754.
 * sge_c_gdi_del() walks the request element by element and appends one answer per
 * host to the task's answer list, so the report stays one line per host, and a
 * host that cannot be deleted holds up the others no more than it did before --
 * every element is still handled on its own in the qmaster.
 *
 * @param arglp the hosts to delete, as parsed from the command line
 * @param target the host list to delete from, EH_LIST
 * @return true if every host was deleted
 */
static bool del_host_of_type(lList *arglp, ocs::gdi::Target target) {
   DENTER(TOP_LAYER);

   lList *lp=nullptr, *alp=nullptr;
   lDescr *type = nullptr;
   bool ret = true;

   switch (target) {
   case ocs::gdi::Target::EH_LIST:
      type = EH_Type;
      break;
   default: ;
   }

   /* every named host in one request */
   lp = lCreateList("hosts_to_del", type);
   for_each_rw_lv (argep, arglp) {
      lAppendElem(lp, lCopyElem(argep));
   }

   /* delete them */
   alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::DEL, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);

   /* print results */
   if (answer_list_has_error(&alp)) {
      ret = false;
   }
   answer_list_on_error_print_or_exit(&alp, stderr);

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(ret);
}

/* ------------------------------------------------------------ */

/* ------------------------------------------------------------ */

/** @brief Delete every named configuration in one request
 *
 * CS-2754: one `sge_gdi(CONF_LIST, DEL)` for the whole list, where `-dconf` and
 * `-Dconf` used to send one request per host.
 *
 * The qmaster needs nothing for this: sge_c_gdi_del() walks the request element
 * by element and sge_del_configuration() appends its own answer for each, so the
 * report stays one line per configuration and one host that cannot be deleted
 * does not stop the others.
 *
 * Gone with it is the client's "if the resolved name failed, try the name as it
 * was typed" retry. It cannot survive a single request -- the answers cannot be
 * mapped back to the elements reliably, because sge_event_spool() adds an answer
 * of its own when spooling fails -- and it turned out to be redundant anyway:
 * the qmaster looks the configuration up with lGetElemHostRW(), which normalises
 * both sides through sge_hostcpy() before comparing, so a configuration spooled
 * under the short name is found when the FQDN is requested and the other way
 * round.
 *
 * @param conf_list the configurations to delete, elements carrying CONF_name
 * @return the number of configurations that could not be deleted
 */
static int delete_configs(const lList *conf_list) {
   DENTER(TOP_LAYER);

   if (lGetNumberOfElem(conf_list) == 0) {
      DRETURN(0);
   }

   lList *lp = lCreateList("configs_to_del", CONF_Type);
   const lListElem *ep;
   for_each_ep(ep, conf_list) {
      lAddElemHost(&lp, CONF_name, lGetHost(ep, CONF_name), CONF_Type);
   }

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::DEL,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);

   /* one line per configuration, as before -- the qmaster reports each of them */
   int fail = 0;
   const lListElem *aep;
   for_each_ep(aep, alp) {
      fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      answer_exit_if_not_recoverable(aep);
      if (answer_get_status(aep) != STATUS_OK) {
         fail++;
      }
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(fail);
}

static int
qconf_is_manager(const char *user) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (user == nullptr) {
      DRETURN(-1);
   }

   // ask master if current user is manager
   lList *alp = nullptr;
   bool is_manager = false;
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(&alp, &is_manager, nullptr, nullptr, nullptr);
   if (!perm_return) {
      DTRACE;
      answer_list_output(&alp);
      sge_exit(1);
   }
   lFreeList(&alp);

   // if user is no manager then exit
   if (!is_manager) {
      fprintf(stderr, MSG_SGETEXT_MUSTBEMANAGER_S , user);
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(0);
}

static int
qconf_is_adminhost(const char *host) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (host == nullptr) {
      DRETURN(-1);
   }

   // ask master if current host is admin host
   lList *alp = nullptr;
   bool is_admin_host = false;
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(&alp, nullptr, nullptr, &is_admin_host, nullptr);
   if (!perm_return) {
      DTRACE;
      answer_list_output(&alp);
      sge_exit(1);
   }
   lFreeList(&alp);

   // if host has no permission then exit
   if (!is_admin_host) {
      fprintf(stderr,MSG_SGETEXT_NOADMINHOST_S, host);
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(0);
}

static int
qconf_is_manager_on_admin_host(const char *user, const char *host) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (host == nullptr) {
      DRETURN(-1);
   }

   // ask master if current host is admin host
   lList *alp = nullptr;
   bool is_admin_host = false;
   bool is_manager = false;
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(&alp, &is_manager, nullptr, &is_admin_host, nullptr);
   if (!perm_return) {
      DTRACE;
      answer_list_output(&alp);
      sge_exit(1);
   }
   lFreeList(&alp);

   // if user is no manager then exit
   if (!is_manager) {
      fprintf(stderr, MSG_SGETEXT_MUSTBEMANAGER_S , user);
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   // if host has no permission then exit
   if (!is_admin_host) {
      fprintf(stderr, MSG_SGETEXT_NOADMINHOST_S, host);
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(0);
}

/**
 * @brief Sends a modify request to the master
 *
 * The function performs a SGE_GDI_MOD request to the qmaster.
 * It will get all necessary infomation from commandline or
 * file.
 * Depending on the parameters specified, the function will
 * modify only parts of an object. It is possible to address
 * only parts of a sublist of an object.
 *
 * @param alpp reference to an answer list where the master will store necessary messages for the user
 * @param from_file if set to 1 then the next commandline parameter (stored in spp) will contain a filename. This file contains the attributes which should be modified
 * @param spp pending list of commandline parameter
 * @param epp this reference will contain the reduced element which will be parsed from commandline or file after this function was called
 * @param sub_command bitmask which will be added to the "command" parameter of the ocs::gdi::Client::sge_gdi-request:
 * @param SGE_GDI_CHANGE modify sublist elements
 * @param SGE_GDI_APPEND add elements to a sublist
 * @param SGE_GDI_REMOVE remove sublist elements
 * @param SGE_GDI_SET replace the complete sublist
 * @param info_entry pointer to a structure with function pointers, string pointers, and CULL names
 *
 * @return [alpp] Masters answer for the gdi request 1 for error 0 for success
 */
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp,
                                  lListElem **epp, ocs::gdi::SubCommand sub_command,
                                  struct object_info_entry *info_entry) {
   DENTER(TOP_LAYER);    

   int fields[150];
   lListElem *add_qp = nullptr;
   lList *qlp = nullptr;
   lEnumeration *what = nullptr;
   
   DTRACE;

   fields[0] = NoName;

   if (from_file) {
      if (sge_next_is_an_opt(*spp))  {
          snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ANSWER_MISSINGFILENAMEASOPTIONARG_S, "qconf");
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(1);
      }                
      DTRACE;
      *epp = spool_flatfile_read_object(alpp, info_entry->cull_descriptor,
                                        nullptr, info_entry->fields, fields,
                                        true, info_entry->instr, qconf_opt_format,
                                        nullptr, **spp);
            
      if (answer_list_output(alpp)) {
         DRETURN(1);
      }
      if (*epp == nullptr){
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_FILE_ERRORREADINGINFILE);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(1);
      }
      DTRACE;
   } else {
      const char *name = nullptr;
      const char *value = nullptr;
      const char *filename = nullptr;
      DSTRING_STATIC(delim, 100);

      /* attribute name to be [admr] for info_entry obj */
      name = (const char *)strdup(**spp);
      *spp = sge_parser_get_next(*spp);
      /* attribute value to be [admr] for info_entry obj */
      value = (const char *)strdup(**spp);

      if (!strcmp(info_entry->object_name, SGE_OBJ_RQS) && !strcmp(name, "limit")) {
         sge_dstring_append(&delim, " to ");
      } else {
         sge_dstring_append_char(&delim, info_entry->instr->name_value_delimiter);
      }
      {
         dstring write_attr_tmp_file_error = DSTRING_INIT;
         filename = write_attr_tmp_file(name, value, sge_dstring_get_string(&delim), &write_attr_tmp_file_error);
         if (filename == nullptr && sge_dstring_get_string(&write_attr_tmp_file_error) != nullptr) {
            answer_list_add_sprintf(alpp, STATUS_EDISK, ANSWER_QUALITY_ERROR, sge_dstring_get_string(&write_attr_tmp_file_error));
         } else {
            *epp = spool_flatfile_read_object(alpp, info_entry->cull_descriptor, nullptr,
                                      info_entry->fields, fields, true, info_entry->instr,
                                      SP_FORM_ASCII, nullptr, filename);
            unlink(filename);
            sge_free(&filename);
         }
         sge_dstring_free(&write_attr_tmp_file_error);
      }

      /* Bugfix: Issuezilla #1005
       * Since we're writing the information from the command line to a file so
       * we can read it, the error messages that come back from the flatfile
       * spooling will sound a little odd.  To avoid this, we hijack the answer
       * list and replace the error messages with ones that will make better
       * sense. */
      if (answer_list_has_error(alpp)) {
         for_each_rw_lv(aep, *alpp) {
            if (answer_has_quality(aep, ANSWER_QUALITY_ERROR) &&
                (answer_get_status(aep) == STATUS_ESYNTAX)) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_BAD_ATTR_ARGS_SS, name, value);
               lSetString(aep, AN_text, SGE_EVENT);
            }
         }
         
         sge_free(&name);
         sge_free(&value);
         sge_dstring_free(&delim);
         
         DRETURN(1);
      }
      
      sge_free(&name);
      sge_free(&value);
      sge_dstring_free(&delim);
   }

   /* add object name to int vector and transform
      it into an lEnumeration */
   if (add_nm_to_set(fields, info_entry->nm_name) < 0) {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_CANTCHANGEOBJECTNAME_SS, "qconf", info_entry->attribute_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      lFreeElem(epp);
      DRETURN(1);
   }

   if (!(what = lIntVector2What(info_entry->cull_descriptor, fields))) {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_INTERNALFAILURE_S, "qconf");
      lFreeElem(epp);
      DRETURN(1);
   }     

   while (!sge_next_is_an_opt(*spp)) { 
      *spp = sge_parser_get_next(*spp);
      if (!qlp)
         qlp = lCreateList("list", info_entry->cull_descriptor);
      add_qp = lCopyElem(*epp);
      switch(lGetType(add_qp->descr, info_entry->nm_name)) {
         case lUlongT:
            lSetUlong(add_qp, info_entry->nm_name, atol(**spp));
            break;
         case lLongT:
            lSetLong(add_qp, info_entry->nm_name, atol(**spp));
            break;
         case lIntT:
            lSetInt(add_qp, info_entry->nm_name, atoi(**spp));
            break;
         case lDoubleT:
            lSetDouble(add_qp, info_entry->nm_name, atof(**spp));
            break;
         case lStringT:
            lSetString(add_qp, info_entry->nm_name, **spp);
            break;
         case lHostT:   
            lSetHost(add_qp, info_entry->nm_name, **spp);
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_INTERNALFAILURE_S, "qconf");
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(1);
      }
      lAppendElem(qlp, add_qp);
   }

   if (!qlp) {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_MQATTR_MISSINGOBJECTLIST_S, "qconf");
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      lFreeElem(epp);
      lFreeWhat(&what);
      DRETURN(1);
   }

   /* Bugfix: Issuezilla #1025
    * If we get a list value of NONE with -mattr, complain.  This comes after
    * the complaint about not including the object list on purpose.  That order
    * seems to make the most sense for error reporting, even though it means we
    * do some unnecessary work. */
   if ((sub_command & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE && (lGetType((*epp)->descr, fields[0]) == lListT)) {
      lList *lp = lGetListRW(*epp, fields[0]);
      
      if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_QCONF_CANT_MODIFY_NONE);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         lFreeElem(epp);
         lFreeWhat(&what);
         lFreeList(&qlp);
         lFreeList(&lp);
         DRETURN(1);
      } else if (lGetNumberOfElem(lp) == 1) {
         const lListElem *ep = lFirst(lp);
         int count = 1;
         int nm = 0;

         /* Look for the list and see if it's nullptr.  These should all be very
          * simple CULL structures which should only contain an lHostT and an
          * lListT. */
         for (nm = ep->descr[0].nm; nm != NoName; nm = ep->descr[count++].nm) {            
            if (lGetType(ep->descr, nm) == lListT) {
               // for CE_Type objects which are no RSMAPs CE_resource_map_list will be nullptr - this is OK
               if (lGetList(ep, nm) == nullptr && nm != CE_resource_map_list) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_QCONF_CANT_MODIFY_NONE);
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  lFreeElem(epp);
                  lFreeWhat(&what);
                  lFreeList(&qlp);
                  DRETURN(1);
               }
            }
         }
      }
   }
   
   if (info_entry->pre_gdi_function == nullptr ||
      info_entry->pre_gdi_function(qlp, alpp)) {
      *alpp = ocs::gdi::Client::sge_gdi(info_entry->target, ocs::gdi::Command::MOD, sub_command, &qlp,
                      nullptr, what);
   }
   lFreeElem(epp);
   lFreeWhat(&what);
   lFreeList(&qlp);
   (*spp)++;

   DRETURN(0);
}

static const char *write_attr_tmp_file(const char *name, const char *value,
                                       const char *delimiter, dstring *error_message) {
   DENTER(TOP_LAYER);

   char *filename = sge_malloc(sizeof(char) * SGE_PATH_MAX);
   int fd = -1;
   FILE *fp = nullptr;
   int my_errno;
   if (sge_tmpnam(filename, &fd, error_message) == nullptr) {
      DRETURN(nullptr);
   }

   errno = 0;
   fp = fdopen(fd, "w");
   my_errno = errno;

   if (fp == nullptr) {
      sge_dstring_sprintf(error_message, MSG_ERROROPENINGFILEFORWRITING_SS, filename, strerror(my_errno));
      DRETURN(nullptr);
   }
   
   fprintf(fp, "%s", name);
   fprintf(fp, "%s", delimiter);
   fprintf(fp, "%s\n", value);
   
   FCLOSE(fp);
   
   DRETURN((const char *)filename);
FCLOSE_ERROR:
   DRETURN(nullptr);
}
