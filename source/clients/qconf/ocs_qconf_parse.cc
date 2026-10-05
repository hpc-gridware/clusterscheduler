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
#include "sge_qconf_hgroup.h"
#include "ocs_qconf_centry.h"
#include "ocs_qconf_cqueue.h"
#include "ocs_qconf_rqs.h"
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
static lListElem *edit_exechost(lListElem *ep, uid_t uid, gid_t gid);
static int edit_usersets(std::ostream &os, lList *arglp);

/************************************************************************/
static int delete_configs(const lList *conf_list);
static int add_modify_config(const char *cfn, const char *filename, uint32_t flags);
static lList* edit_sched_conf(lList *confl, uid_t uid, gid_t gid);
static lListElem* edit_project(lListElem *ep, uid_t uid, gid_t gid);
static lListElem* edit_user(lListElem *ep, uid_t uid, gid_t gid);
static lListElem *edit_sharetree(lListElem *ep, uid_t uid, gid_t gid);

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

/** @brief True if the name attribute @a name_nm of @a descr is a host field
 *  (lHostT) rather than a plain string (CS-2304: e.g. EH_name). */
static bool
qconf_name_is_host(const lDescr *descr, int name_nm) {
   return lGetType(descr, name_nm) == lHostT;
}

/** @brief Read an object's name attribute as a string, whether it is a plain
 *  string field or a host field (CS-2304). */
static const char *
qconf_get_name(const lListElem *ep, int name_nm) {
   if (lGetType(lGetElemDescr(ep), name_nm) == lHostT) {
      return lGetHost(ep, name_nm);
   }
   return lGetString(ep, name_nm);
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
 * @brief Whether @p name is present in a name list (CS-2315).
 *
 * Host-typed name fields are compared with the host comparison, plain ones with
 * a string comparison - mirroring qconf_object_exists().
 *
 * @param names   list of name-only elements (from qconf_get_name_list)
 * @param descr   CULL descriptor of the object type
 * @param name_nm name attribute of the object
 * @param name    name to look for
 * @return true if the list contains an element with that name
 */
static bool
qconf_name_in_list(const lList *names, const lDescr *descr, int name_nm, const char *name) {
   if (names == nullptr || name == nullptr) {
      return false;
   }
   return qconf_name_is_host(descr, name_nm)
      ? (lGetElemHost(names, name_nm, name) != nullptr)
      : (lGetElemStr(names, name_nm, name) != nullptr);
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
   lList *alp = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   fields_out[0] = NoName;

   /* CS-2299 H4: "-" reads the object from stdin. spool_flatfile_read_object()
    * only closes the stream when it opened it itself, so passing stdin is safe. */
   bool from_stdin = (filename != nullptr && strcmp(filename, "-") == 0);
   lListElem *ep = spool_flatfile_read_object(&alp, descr, nullptr, fields,
                                              fields_out, parse_values, sfi,
                                              qconf_opt_format,
                                              from_stdin ? stdin : nullptr,
                                              filename);
   if (answer_list_output(&alp)) {
      lFreeElem(&ep);
      return nullptr;
   }
   if (ep != nullptr && spool_get_unprocessed_field(fields, fields_out, &alp) != NoName) {
      answer_list_output(&alp);
      lFreeElem(&ep);
      return nullptr;
   }
   /* CS-2304: optional transform of the parsed object (e.g. resolve a hostname)
    * before the name is used for the existence check / send. */
   if (ep != nullptr && prepare != nullptr && prepare(ep, &alp) != STATUS_OK) {
      answer_list_output(&alp);
      lFreeElem(&ep);
      return nullptr;
   }
   if (ep != nullptr && validate != nullptr && validate(ep, &alp) != STATUS_OK) {
      answer_list_output(&alp);
      lFreeElem(&ep);
      return nullptr;
   }
   lFreeList(&alp);
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
    * The apply path always parses values; only a delete may skip it. */
   qconf_file_ctx ctx = {&os, &d, fields, true, nullptr,
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
   qconf_file_ctx ctx = {&os, &d, fields, d.del_parse_values,
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
qconf_dispatch_procedure(std::ostream &os, ocs::QConfParameterClient &param, lList **alpp) {
   DENTER(TOP_LAYER);

   bool ok;
   if (param.get_exec_context() == ocs::ProcedureParameter::ExecContext::SERVER) {
      ocs::ProcedureModel model;
      ok = model.make_snapshot(alpp, param);

      /* CS-2821: the one consumer of the reduction channel so far. Nothing acts
       * on it yet - the write guard of CS-2823 will - but asking for it here
       * means the value is read on every call and shows up in a traced run,
       * which is what proves the entry survived the GDI round trip rather than
       * only the unit test's own bundle. */
      DPRINTF("procedure reduction mask: " sge_u32 "\n", model.get_reduction());

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
 * @return what happened, see QConfShowStatus
 */
static QConfShowStatus
qconf_show_object(std::ostream &os, ocs::QConfParameterClient &param, QConfObj o, const char *name) {
   DENTER(TOP_LAYER);

   /* CS-2799: the body of this function is a stored procedure call now. The
    * signature and the status contract stay, because the option blocks above
    * disagree about what a failure means and each keeps its own reaction.
    *
    * The status is recovered from the answer list: the model reports a missing
    * object with STATUS_EEXIST, everything else is a fetch failure. That is the
    * only categorisation that survives the trip back from qmaster, and it is
    * the one the callers distinguish. */
   const char *switch_name = ocs::QConfModelBase::switch_for(o, ocs::QConfReadKind::SINGLE_OBJECT);
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
   const bool ok = qconf_dispatch_procedure(os, param, &alp);
   if (ok) {
      lFreeList(&alp);
      DRETURN(QConfShowStatus::OK);
   }

   const bool not_found = answer_list_has_status(&alp, STATUS_EEXIST);
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
 * @return 0 on success, non-zero on error
 */
static int
qconf_apply_obj(std::ostream &os, QConfObj o, const char *path) {
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

/** @brief Upsert one object, driven by the descriptor.
 *
 * The target, descriptor, name field and GDI sub-command all come from the
 * table, so the editor branches no longer name the object type four times.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param o  the object type
 * @param ep the element to send; consumed by the call
 * @return 0 on success, non-zero on error
 */
static int
qconf_send_upsert_obj(std::ostream &os, QConfObj o, lListElem *ep) {
   const qconf_object_desc &d = qconf_desc(o);
   return qconf_send_upsert(os, d.target, d.descr, d.name_nm, ep, d.sub_cmd);
}

/** @brief Write an object to a temporary file for the editor.
 *
 * The field list and the spooling instruction come from the descriptor, so the
 * editor branches no longer name them. The list is freed here when the
 * provider allocated it.
 *
 * @param o    the object type
 * @param alpp answer list for errors
 * @param ep   the object to write
 * @return the temporary file name (caller frees), or nullptr on error
 */
static char *
qconf_write_tmp_obj(QConfObj o, lList **alpp, const lListElem *ep) {
   const qconf_object_desc &d = qconf_desc(o);
   bool owned = false;
   spooling_field *fields = d.build_fields(qconf_opt_format, &owned);
   auto *filename = (char *)spool_flatfile_write_object(alpp, ep, false, fields, d.sfi,
                                                        SP_DEST_TMP, qconf_opt_format,
                                                        nullptr, false);
   if (owned) {
      sge_free(&fields);
   }
   return filename;
}

/** @brief Read back the file the editor wrote, and consume it.
 *
 * Removes the temporary file and frees @p filename whatever the outcome, so the
 * caller does not have to unwind it on every branch. Returns nullptr when the
 * file did not parse or left a field unprocessed - the reason is in @p alpp and
 * has already been printed.
 *
 * The field list has to outlive the read, because spool_get_unprocessed_field()
 * needs it to name the field that was not consumed; it is freed only afterwards.
 *
 * @param o        the object type
 * @param alpp     answer list for errors
 * @param filename the temporary file; set to nullptr on return
 * @return the parsed object (caller owns), or nullptr on error
 */
static lListElem *
qconf_read_tmp_obj(QConfObj o, lList **alpp, char **filename) {
   const qconf_object_desc &d = qconf_desc(o);
   bool owned = false;
   spooling_field *fields = d.build_fields(qconf_opt_format, &owned);
   int fields_out[MAX_NUM_FIELDS];
   fields_out[0] = NoName;

   lListElem *ep = spool_flatfile_read_object(alpp, d.descr, nullptr, fields, fields_out,
                                              true, d.sfi, qconf_opt_format, nullptr, *filename);
   unlink(*filename);
   sge_free(filename);

   if (answer_list_output(alpp)) {
      lFreeElem(&ep);
   }
   if (ep != nullptr && spool_get_unprocessed_field(fields, fields_out, alpp) != NoName) {
      lFreeElem(&ep);
      answer_list_output(alpp);
   }
   /* the descriptor knows the object's validator; the file path already runs it
    * through qconf_read_object_file(), so the editor path does the same */
   if (ep != nullptr && d.validate != nullptr && d.validate(ep, alpp) != STATUS_OK) {
      lFreeElem(&ep);
      answer_list_output(alpp);
   }
   if (owned) {
      sge_free(&fields);
   }
   return ep;
}

/** @brief Why an interactive edit did not yield an object. */
enum class QConfEditStatus {
   OK,            ///< the object was edited and parsed
   WRITE_FAILED,  ///< the template could not be written for the editor
   EDIT_FAILED,   ///< the editor itself reported an error
   UNCHANGED,     ///< the editor left the file untouched
   READ_FAILED    ///< the result did not parse, or left a field unprocessed
};

/** @brief Offer an object to $EDITOR and read the result back.
 *
 * Deliberately **exit-free**: every failure is reported through the return
 * value and @p alpp, never by terminating the process. Part of this logic is
 * meant to run inside qmaster once the stored procedure conversion reaches the
 * write path (CS-2792), where sge_exit() would take the daemon down with it.
 * The caller decides what a failure means - see qconf_edit_fail_exit() for what
 * the command line client does.
 *
 * The temporary file is consumed in every case, including every error path.
 *
 * @param o    the object type
 * @param alpp answer list for errors
 * @param epp  in: the object to offer, consumed; out: the edited object or nullptr
 * @param uid  uid to run the editor as
 * @param gid  gid to run the editor as
 * @return OK, or the reason no object came back
 */
static QConfEditStatus
qconf_edit_obj(QConfObj o, lList **alpp, lListElem **epp, uid_t uid, gid_t gid) {
   char *filename = qconf_write_tmp_obj(o, alpp, *epp);
   lFreeElem(epp);

   if (answer_list_output(alpp) || filename == nullptr) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      return QConfEditStatus::WRITE_FAILED;
   }

   int status = sge_edit(filename, uid, gid);
   if (status != 0) {
      unlink(filename);
      sge_free(&filename);
      return (status < 0) ? QConfEditStatus::EDIT_FAILED : QConfEditStatus::UNCHANGED;
   }

   *epp = qconf_read_tmp_obj(o, alpp, &filename);
   return (*epp == nullptr) ? QConfEditStatus::READ_FAILED : QConfEditStatus::OK;
}

/** @brief Report an edit failure the way the option branches always did, and exit.
 *
 * The one place where the interactive path terminates the process, kept apart
 * from qconf_edit_obj() so that the editing logic stays reusable. Does not
 * return - sge_error_and_exit() calls sge_exit().
 *
 * @param st the failure reported by qconf_edit_obj()
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
 * @brief Add or modify the resource quota set(s) defined at @a path (CS-2307).
 *
 * A single file keeps the existing (possibly multi-rule-set) behaviour. A
 * directory merges every file's rule sets into one SET_ALL request; if any file
 * fails to parse nothing is applied (whole-list semantics are inherently strict).
 * Honours -dry and prints a one-line summary.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param path file or directory to apply
 * @param cmd  ocs::gdi::Command::ADD or MOD
 * @param name optional rqs-name list for the single-file -Mrqs form, or nullptr
 * @return 0 on success, non-zero on failure
 */
static int
qconf_rqs_apply(std::ostream &os, const char *path, ocs::gdi::Command cmd, const char *name) {
   bool is_dir = sge_is_directory(path);

   /* preserve the existing single-file behaviour verbatim (only when actually
    * sending - a -dry single file falls through to the report path below) */
   if (!is_dir && !qconf_opt_dry_run) {
      lList *alp = nullptr;
      bool ok = (cmd == ocs::gdi::Command::ADD)
         ? rqs_add_from_file(&alp, path)
         : rqs_modify_from_file(&alp, path, name);
      int ret = show_answer_list(alp);
      lFreeList(&alp);
      return (ok && ret == 0) ? 0 : 1;
   }

   qconf_rqs_ctx ctx = {&os, lCreateList("rqs", RQS_Type), 0};
   qconf_for_each_file(path, qconf_rqs_collect, &ctx);

   if (ctx.n_fail > 0) {
      ERROR(MSG_QCONF_STRICTNOTHINGAPPLIED_SI, path, ctx.n_fail);
      lFreeList(&ctx.all);
      return ctx.n_fail;
   }

   int n_ok = lGetNumberOfElem(ctx.all);
   if (qconf_opt_dry_run) {
      const lListElem *r;
      for_each_ep(r, ctx.all) {
         qconf_info(os, cmd == ocs::gdi::Command::ADD ? MSG_QCONF_DRYRUNWOULDADD_S
                                                         : MSG_QCONF_DRYRUNWOULDMODIFY_S,
                           lGetString(r, RQS_name));
      }
      lFreeList(&ctx.all);
      return 0;
   }

   int ret = 0;
   if (n_ok > 0) {
      lList *alp = nullptr;
      rqs_add_del_mod_via_gdi(ctx.all, &alp, cmd, ocs::gdi::SubCommand::SET_ALL);
      ret += show_answer_list(alp);
      lFreeList(&alp);
   }
   if (is_dir) {
      qconf_info(os, MSG_QCONF_ADDMODSUMMARY_SII, path, n_ok, 0);
   }
   lFreeList(&ctx.all);
   return ret;
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
      /* rqs_add_del_mod_via_gdi() only sends ADD/MOD/REPLACE (it gates the GDI
       * call on the verify result, which is skipped for DEL); delete via a
       * direct GDI call, exactly like the "-drqs" name-list path does. */
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
 * field-name-based qconf_apply_path/qconf_delete_path. add_modify_config() and
 * delete_configs() are forward-declared at the top of this file. */

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
   uint32_t flags;   ///< add_modify_config() flags (apply path only)
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

   /* CS-2299 H3: add_modify_config() always contacts qmaster, so the -dry
    * preview is handled here - report the intended action and send nothing. */
   if (qconf_opt_dry_run) {
      bool exists = qconf_object_exists(ocs::gdi::Target::CONF_LIST, CONF_Type,
                                        CONF_name, hostname);
      qconf_info(*ctx->os, exists ? MSG_QCONF_DRYRUNWOULDMODIFY_S : MSG_QCONF_DRYRUNWOULDADD_S,
                        hostname);
      lFreeElem(&host_ep);
      ctx->n_ok++;
      return 0;
   }

   int ret = add_modify_config(hostname, filepath, ctx->flags);
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
 * Each file is upserted (add_modify_config flag 3); the host is the basename.
 * A directory prints the H1 one-line summary.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param path  config file or directory of config files
 * @return the number of files that failed
 */
static int
qconf_conf_apply_path(std::ostream &os, const char *path) {
   qconf_conf_ctx ctx = {&os, 3 /* modify if exists, add if not */, nullptr, 0, 0};
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
   qconf_conf_ctx ctx = {&os, 0, lCreateList("conf del", CONF_Type), 0, 0};
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

/**
 * @brief Whether a share tree currently exists (CS-2312 C2 upsert probe).
 *
 * The share tree is a singleton stored as a (possibly empty) list of nodes, so
 * "exists" means the STN_LIST GET returns at least one node. Used to choose
 * between a GDI ADD (no tree yet) and MOD (replace the existing tree) so that
 * -astree/-Astree and -mstree/-Mstree become interchangeable upserts.
 *
 * @return true if a share tree is configured, false otherwise
 */
static bool
qconf_sharetree_exists() {
   lList *lp = nullptr;
   lEnumeration *what = lWhat("%T(%I)", STN_Type, STN_id);
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
   lFreeWhat(&what);
   lFreeList(&alp);
   bool exists = (lp != nullptr && lGetNumberOfElem(lp) > 0);
   lFreeList(&lp);
   return exists;
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
   const bool ok = qconf_dispatch_procedure(os, param, &alp);

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
   lCondition *where = nullptr;
   lList *lp=nullptr, *arglp=nullptr, *alp=nullptr, *newlp=nullptr;
   lListElem *hep=nullptr, *ep=nullptr;
   const lListElem *aep=nullptr;
   lListElem *newep = nullptr;
   const char *host = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
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
            char *cal_name = (char *)"template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               cal_name = *spp;
            }

            /* get a generic calendar */
            ep = sge_generic_cal(cal_name);
            /* hand it to the editor and read the result back; the engine
             * is exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::CAL, &alp, &ep, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ep;
} else { /* -Acal: CS-2299 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CAL, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -acal editor path: CS-2299 C2 — upsert (modify if it already exists) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::CAL, ep);
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
            char *ckpt_name = (char *)"template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               ckpt_name = *spp;
            }

            /* get a generic ckpt configuration */
            ep = sge_generic_ckpt(ckpt_name);
            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::CK, &alp, &ep, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ep;
} else { /* -Ackpt: CS-2300 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CK, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -ackpt editor path: CS-2300 C2 — upsert (modify if it already exists) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::CK, ep);
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ae [server_name]" */
      if (strcmp("-ae", *spp) == 0) {
         char *hostname = nullptr;

         cp = nullptr;
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         if (!sge_next_is_an_opt(spp)) {
            lListElem *host_ep = nullptr;

            spp = sge_parser_get_next(spp);

            /* try to resolve hostname */
            host_ep = lCreateElem(EH_Type);
            lSetHost(host_ep, EH_name, *spp);

            switch (sge_resolve_host(host_ep, EH_name)) {
               case CL_RETVAL_OK:
                  break;
               default:
                  fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(host_ep, EH_name));
                  fprintf(stderr, "\n");
                  lFreeElem(&host_ep);
                  DRETURN(1);
            }

            hostname = sge_strdup(hostname, lGetHost(host_ep, EH_name));
            lFreeElem(&host_ep);
         } else {
            /* no template name given - then use "template" as name */
            hostname = sge_strdup(hostname, SGE_TEMPLATE_NAME);
         }
         /* get a template host entry .. */
         where = lWhere("%T( %Ih=%s )", EH_Type, EH_name, hostname);
         what = lWhat("%T(ALL)", EH_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &arglp, where, what);
         lFreeWhat(&what);
         lFreeWhere(&where);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_free(&hostname);
            lFreeList(&alp);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         if (arglp == nullptr || lGetNumberOfElem(arglp) == 0) {
            fprintf(stderr, MSG_EXEC_XISNOEXECHOST_S, hostname);
            fprintf(stderr, "\n");
            sge_free(&hostname);
            lFreeList(&alp);
            lFreeList(&arglp);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         sge_free(&hostname);
         lFreeList(&alp);

         /* edit the template */
         lListElem *argep = lFirstRW(arglp);
         ep = edit_exechost(argep, uid, gid);
         if (ep == nullptr) {
            lFreeList(&arglp);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         switch (sge_resolve_host(ep, EH_name)) {
         case CL_RETVAL_OK:
            break;
         default:
            fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(ep, EH_name));
            fprintf(stderr, "\n");
            lFreeList(&arglp);
            DRETURN(1);
         }

         hostname = sge_strdup(hostname, lGetHost(ep, EH_name));
         lFreeList(&arglp);

         lp = lCreateList("hosts to add", EH_Type);
         lAppendElem(lp, ep);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::ADD, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
         lFreeList(&lp);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_free(&hostname);
            spp++;
            sge_parse_return = 1;
            continue;
         }

         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK) {
            fprintf(stderr, MSG_EXEC_ADDEDHOSTXTOEXECHOSTLIST_S, hostname);
         } else {
            fprintf(stderr, "%s", lGetString(ep, AN_text));
         }
         fprintf(stderr, "\n");

         sge_free(&hostname);
         lFreeList(&alp);
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
            char *role_name = (char *)"template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               role_name = *spp;
            }

            ep = ocs::Role::create_template();
            lSetString(ep, RL_name, role_name);
            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::RL, &alp, &ep, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ep;
} else { /* -Arole: CS-2302 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::RL, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -arole editor path: CS-2302 C2 — upsert (modify if it already exists) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::RL, ep);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-arqs rqs_name" */
      if (strcmp("-arqs", *spp) == 0) {
         const char *name = "template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         rqs_add(&alp, name);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&lp);
            DRETURN(1);
         } else {
            os << qconf_cstr(lGetString(aep, AN_text)) << "\n";
         }
         lFreeList(&alp);

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

         if (qconf_rqs_apply(os, file, ocs::gdi::Command::ADD, nullptr) != 0) {
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
            char *pe_name = (char *)"template";

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            if (!sge_next_is_an_opt(spp)) {
               spp = sge_parser_get_next(spp);
               pe_name = *spp;
            }

            /* get a generic parallel environment */
            ep = pe_create_template(pe_name);
            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::PE, &alp, &ep, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ep;
} else { /* -Ap: CS-2301 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::PE, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -ap editor path: CS-2301 C2 — upsert (modify if it already exists) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::PE, ep);
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

         /* get a template for editing, pre-filled with the requested name */
         ep = getUserTemplate();
         lSetString(ep, UU_name, user_name);

         newep = edit_user(ep, uid, gid);
         lFreeElem(&ep);

         /* CS-2308 C2: upsert (modify if the user already exists) */
         if (newep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::UU, newep);
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

         /* get a template for editing, pre-filled with the requested name */
         ep = getPrjTemplate();
         lSetString(ep, PR_name, prj_name);

         newep = edit_project(ep, uid, gid);
         lFreeElem(&ep);

         /* CS-2309 C2: upsert (modify if the project already exists) */
         if (newep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::PR, newep);
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
      /* "-astree", "-Astree file":  add sharetree */

      if ((strcmp("-astree", *spp) == 0) || (strcmp("-Astree", *spp) == 0)) {
         lListElem *unspecified = nullptr;

         if (strcmp("-astree", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

            /* get the sharetree .. */
            what = lWhat("%T(ALL)", STN_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
            lFreeWhat(&what);

            sge_parse_return |= show_answer_list(alp);
            if (sge_parse_return) {
               lFreeList(&alp);
               spp++;
               continue;
            }
            lFreeList(&alp);

            ep = lFirstRW(lp);
            if (!(ep = edit_sharetree(ep, uid, gid)))
               continue;

            lFreeList(&lp);
         } else { /* -Astree */
            /* JSON uses the nested-tree field list (childnodes are full child
             * objects); ASCII uses the flat id-referenced list */
            spooling_field *fields = (qconf_opt_format == SP_FORM_JSON)
                                     ? sge_build_STN_json_field_list()
                                     : sge_build_STN_field_list(false, true);

            spp = sge_parser_get_next(spp);

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                            fields, fields_out, true,
                                            &qconf_name_value_list_sfi,
                                            qconf_opt_format, nullptr, *spp);

            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            /* the nested JSON form carries no node ids -> assign them (as the flat
             * ASCII reader implicitly does) so the tree is well-formed for GDI */
            if (ep != nullptr && qconf_opt_format == SP_FORM_JSON) {
               id_sharetree(&alp, ep, 0, nullptr);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
            }

            sge_free(&fields);

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE)) {
                  continue;
               }
            }
         }

         /* Make sure that no nodes are left unspecified.  An unspecified node
          * happens when a node appears in another node's child list, but does
          * not appear itself. */
         unspecified = sge_search_unspecified_node(ep);

         if (unspecified != nullptr) {
            fprintf(stderr, MSG_STREE_NOVALIDNODEREF_U, lGetUlong(unspecified, STN_id));
            fprintf(stderr, "\n");

            lFreeElem(&ep);
            spp++;
            continue;
         }

         newlp = lCreateList("sharetree add", STN_Type);
         lAppendElem(newlp, ep);

         /* CS-2312 C2: the share tree is a singleton - add and modify are the
          * same upsert. Use MOD when a tree already exists, ADD otherwise, so
          * -astree/-Astree no longer fail when a tree is already configured. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST,
                                         qconf_sharetree_exists() ? ocs::gdi::Command::MOD
                                                                  : ocs::gdi::Command::ADD,
                                         ocs::gdi::SubCommand::NONE, &newlp, nullptr, what);
         lFreeWhat(&what);

         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_TREE_CHANGEDSHARETREE);
         else
            fprintf(stderr, "%s\n", lGetString(ep, AN_text));
         lFreeList(&newlp);
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
         lList *answer_list = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         if (!centry_list_modify(&answer_list)) {
            sge_parse_return = 1;
         }
         sge_parse_return |= show_answer_list(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mcal cal-name" */

      if ((strcmp("-mcal", *spp) == 0) ||
          (strcmp("-Mcal", *spp) == 0)) {
         if (!strcmp("-mcal", *spp)) {
            lListElem *cal_src = nullptr;

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            where = lWhere("%T( %I==%s )", CAL_Type, CAL_name, *spp);
            what = lWhat("%T(ALL)", CAL_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CAL_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
               cal_src = lDechainElem(lp, lFirstRW(lp));
            } else {
               /* CS-2299 C2: modifying a non-existent calendar implicitly adds
                * it — offer a generic template for editing instead of failing. */
               cal_src = sge_generic_cal(*spp);
            }
            lFreeList(&lp);

            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::CAL, &alp, &cal_src, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = cal_src;
} else { /* -Mcal: CS-2299 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CAL, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -mcal editor path: CS-2299 C2 — upsert (add if it did not exist) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::CAL, ep);
         }

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-Mc complex file" */

      if (strcmp("-Mc", *spp) == 0) {
         lList *answer_list = nullptr;
         char* file = nullptr;

         /* no adminhost/manager check needed here */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            file = *spp;
         } else {
            sge_error_and_exit(MSG_FILE_NOFILEARGUMENTGIVEN);
         }
         if (!centry_list_modify_from_file(&answer_list, file)) {
            sge_parse_return = 1;
         }
         sge_parse_return |= show_answer_list(answer_list);
         lFreeList(&answer_list);
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-mckpt ckpt_name" or "-Mckpt fname" */

      if ((strcmp("-mckpt", *spp) == 0) ||
          (strcmp("-Mckpt", *spp) == 0)) {
         if (strcmp("-mckpt", *spp) == 0) {
            lListElem *ckpt_src = nullptr;

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            /* get last version of this checkpoint object from qmaster */
            where = lWhere("%T( %I==%s )", CK_Type, CK_name, *spp);
            what = lWhat("%T(ALL)", CK_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CK_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
               ckpt_src = lDechainElem(lp, lFirstRW(lp));
            } else {
               /* CS-2300 C2: modifying a non-existent checkpoint implicitly adds
                * it — offer a generic template for editing instead of failing. */
               ckpt_src = sge_generic_ckpt(*spp);
            }
            lFreeList(&lp);

            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::CK, &alp, &ckpt_src, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ckpt_src;
} else { /* -Mckpt: CS-2300 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::CK, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -mckpt editor path: CS-2300 C2 — upsert (add if it did not exist) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::CK, ep);
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

         for_each_rw_lv (argep, arglp) {
            /* resolve hostname */
            if (sge_resolve_host(argep, EH_name) != CL_RETVAL_OK) {
               fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(argep, EH_name));
               fprintf(stderr, "\n");
               sge_parse_return = 1;
               continue;
            }
            host = lGetHost(argep, EH_name);

            /* get the existing host entry .. */
            where = lWhere("%T( %Ih=%s )", EH_Type, EH_name, host);
            what = lWhat("%T(ALL)", EH_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            if (show_answer(alp) == 1) {
               lFreeList(&alp);
               sge_parse_return = 1;
               continue;
            }

            if (lGetNumberOfElem(lp) == 0) {
               fprintf(stderr, MSG_EXEC_XISNOTANEXECUTIONHOST_S, host);
               fprintf(stderr, "\n");
               sge_parse_return = 1;
               continue;
            }
            lFreeList(&alp);

            ep = edit_exechost(lFirstRW(lp), uid, gid);
            if (ep == nullptr) {
               continue;
            }
            lFreeList(&lp);
            lp = lCreateList("host to mod", EH_Type);
            lAppendElem(lp, ep);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
            lFreeList(&lp);

            if (show_answer(alp) == 1) {
               lFreeList(&alp);
               sge_parse_return = 1;
               continue;
            }
            lFreeList(&alp);
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
            lListElem *role_src = nullptr;

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            where = lWhere("%T( %I==%s )", RL_Type, RL_name, *spp);
            what = lWhat("%T(ALL)", RL_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::RL_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
               role_src = lDechainElem(lp, lFirstRW(lp));
            } else {
               /* CS-2302 C2: modifying a non-existent role implicitly adds it —
                * offer a generic template for editing instead of failing. */
               role_src = ocs::Role::create_template();
               lSetString(role_src, RL_name, *spp);
            }
            lFreeList(&lp);

            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::RL, &alp, &role_src, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = role_src;
} else { /* -Mrole: CS-2302 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::RL, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -mrole editor path: CS-2302 C2 — upsert (add if it did not exist) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::RL, ep);
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
         rqs_modify(&alp, name);
         sge_parse_return |= show_answer_list(alp);

         lFreeList(&alp);

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

         if (qconf_rqs_apply(os, file, ocs::gdi::Command::MOD, name) != 0) {
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
            lListElem *pe_src = nullptr;

            qconf_is_manager_on_admin_host(username, qualified_hostname);

            spp = sge_parser_get_next(spp);

            /* get last version of this pe from qmaster */
            where = lWhere("%T( %I==%s )", PE_Type, PE_name, *spp);
            what = lWhat("%T(ALL)", PE_Type);
            alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::PE_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
            lFreeWhere(&where);
            lFreeWhat(&what);

            aep = lFirst(alp);
            answer_exit_if_not_recoverable(aep);
            if (answer_get_status(aep) != STATUS_OK) {
               fprintf(stderr, "%s\n", lGetString(aep, AN_text));
               lFreeList(&alp);
               sge_parse_return = 1;
               spp++;
               continue;
            }
            lFreeList(&alp);

            if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
               pe_src = lDechainElem(lp, lFirstRW(lp));
            } else {
               /* CS-2301 C2: modifying a non-existent PE implicitly adds it —
                * offer a generic template for editing instead of failing. */
               pe_src = pe_create_template(*spp);
            }
            lFreeList(&lp);

            /* write pe to temp file */
            /* hand it to the editor and read the result back; the engine is
             * exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::PE, &alp, &pe_src, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = pe_src;
} else { /* -Mp: CS-2301 C3 — accept a file OR a directory, upsert each */
            spp = sge_parser_get_next(spp);
            if (qconf_apply_obj(os, QConfObj::PE, *spp) != 0) {
               sge_parse_return = 1;
            }
            spp++;
            continue;
         }

         /* -mp editor path: CS-2301 C2 — upsert (add if it did not exist) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::PE, ep);
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
      /* "-Msconf" */
      if (strcmp("-Msconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         fields_out[0] = NoName;
         ep = spool_flatfile_read_object(&alp, SC_Type, nullptr,
                                         SC_fields, fields_out, true, &qconf_comma_sfi,
                                         qconf_opt_format, nullptr, *spp);

         if (answer_list_output(&alp)) {
            lFreeElem(&ep);
         }

         if (ep != nullptr) {
            missing_field = spool_get_unprocessed_field(SC_fields, fields_out, &alp);
         }

         if (missing_field != NoName) {
            lFreeElem(&ep);
            answer_list_output(&alp);
            sge_parse_return = 1;
         }

         if (ep != nullptr) {
            lp = lCreateList("scheduler config", SC_Type);
            lAppendElem (lp, ep);

            if (!sconf_validate_config (&alp, lp)) {
               lFreeList(&lp);
               answer_list_output(&alp);
            }
         }

         /* else we let the check for lp != nullptr catch the error below */

         if ((lp == nullptr) && (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE))) {
            continue;
         }

         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SC_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_SCHEDD_CHANGEDSCHEDULERCONFIGURATION);
         else { /* error */
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            sge_parse_return = 1;
         }

         lFreeList(&alp);
         lFreeList(&lp);

         spp++;
         continue;

      }

/*----------------------------------------------------------------------------*/
      /* "-msconf"  modify scheduler configuration */

      if (strcmp("-msconf", *spp) == 0) {
         qconf_is_manager_on_admin_host(username, qualified_hostname);

         /* get the scheduler configuration .. */
         what = lWhat("%T(ALL)", SC_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SC_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
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

         if (!(newlp=edit_sched_conf(lp, uid, gid)))
            continue;

         lFreeList(&lp);
         what = lWhat("%T(ALL)", SC_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SC_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &newlp, nullptr, what);
         lFreeWhat(&what);
         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_SCHEDD_CHANGEDSCHEDULERCONFIGURATION);
         else {
            fprintf(stderr, "%s\n", lGetString(ep, AN_text));
            sge_parse_return = 1;
         }
         lFreeList(&newlp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mstree", "-Mstree file": modify sharetree */

      if ((strcmp("-mstree", *spp) == 0) || (strcmp("-Mstree", *spp) == 0)) {
         lListElem *unspecified = nullptr;

         if (strcmp("-mstree", *spp) == 0) {
            qconf_is_manager_on_admin_host(username, qualified_hostname);

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
            if (!(ep = edit_sharetree(ep, uid, gid)))
               continue;

            lFreeList(&lp);
         } else {
            /* JSON uses the nested-tree field list (childnodes are full child
             * objects); ASCII uses the flat id-referenced list - same as -Astree */
            spooling_field *fields = (qconf_opt_format == SP_FORM_JSON)
                                     ? sge_build_STN_json_field_list()
                                     : sge_build_STN_field_list(false, true);

            spp = sge_parser_get_next(spp);

            fields_out[0] = NoName;
            ep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                            fields, fields_out, true,
                                            &qconf_name_value_list_sfi,
                                            qconf_opt_format, nullptr, *spp);

            if (answer_list_output(&alp)) {
               lFreeElem(&ep);
            }

            /* the nested JSON form carries no node ids -> assign them (as the flat
             * ASCII reader implicitly does) so the tree is well-formed for GDI */
            if (ep != nullptr && qconf_opt_format == SP_FORM_JSON) {
               id_sharetree(&alp, ep, 0, nullptr);
            }

            if (ep != nullptr) {
               missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
            }

            sge_free(&fields);

            if (missing_field != NoName) {
               lFreeElem(&ep);
               answer_list_output(&alp);
               sge_parse_return = 1;
            }

            if (ep == nullptr) {
               if (sge_error_and_exit(MSG_FILE_ERRORREADINGINFILE))
                  continue;
            }
         }

         /* Make sure that no nodes are left unspecified.  An unspecified node
          * happens when a node appears in another node's child list, but does
          * not appear itself. */
         unspecified = sge_search_unspecified_node(ep);

         if (unspecified != nullptr) {
            fprintf(stderr, MSG_STREE_NOVALIDNODEREF_U, lGetUlong(unspecified, STN_id));
            fprintf(stderr, "\n");
            sge_parse_return = 1;

            lFreeElem(&ep);
            spp++;
            continue;
         }

         newlp = lCreateList("sharetree modify", STN_Type);
         lAppendElem(newlp, ep);

         /* CS-2312 C2: singleton upsert - ADD when no tree exists yet, MOD
          * otherwise, so -mstree/-Mstree no longer fail when none is configured. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST,
                                         qconf_sharetree_exists() ? ocs::gdi::Command::MOD
                                                                  : ocs::gdi::Command::ADD,
                                         ocs::gdi::SubCommand::NONE, &newlp, nullptr, what);
         lFreeWhat(&what);
         ep = lFirstRW(alp);
         answer_exit_if_not_recoverable(ep);
         if (answer_get_status(ep) == STATUS_OK)
            fprintf(stderr, "%s\n", MSG_TREE_CHANGEDSHARETREE);
         else {
            fprintf(stderr, "%s\n", lGetString(ep, AN_text));
            sge_parse_return = 1;
         }
         lFreeList(&alp);
         lFreeList(&newlp);
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

         if (edit_usersets(os, lp) != 0) {
            sge_parse_return = 1;
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
         lListElem *user_src = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         /* get user */
         where = lWhere("%T( %I==%s )", UU_Type, UU_name, *spp);
         what = lWhat("%T(ALL)", UU_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::UU_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&lp);
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
            user_src = lDechainElem(lp, lFirstRW(lp));
         } else {
            /* CS-2308 C2: modifying a non-existent user implicitly adds it —
             * offer a template pre-filled with the name instead of failing. */
            user_src = getUserTemplate();
            lSetString(user_src, UU_name, *spp);
         }
         lFreeList(&lp);

         /* edit user */
         newep = edit_user(user_src, uid, gid);
         lFreeElem(&user_src);

         /* CS-2308 C2: upsert (add if it did not exist) */
         if (newep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::UU, newep);
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-mprj projectname" */

      if (strcmp("-mprj", *spp) == 0) {
         lListElem *prj_src = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);

         /* get project */
         where = lWhere("%T( %I==%s )", PR_Type, PR_name, *spp);
         what = lWhat("%T(ALL)", PR_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::PR_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
            prj_src = lDechainElem(lp, lFirstRW(lp));
         } else {
            /* CS-2309 C2: modifying a non-existent project implicitly adds it —
             * offer a template pre-filled with the name instead of failing. */
            prj_src = getPrjTemplate();
            lSetString(prj_src, PR_name, *spp);
         }
         lFreeList(&lp);

         /* edit project */
         newep = edit_project(prj_src, uid, gid);
         lFreeElem(&prj_src);

         /* CS-2309 C2: upsert (add if it did not exist) */
         if (newep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::PR, newep);
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
               /* CS-2311 C2: -aconf/-mconf both upsert (add if missing, modify
                * if present) via the editor (flag 3). */
               if (add_modify_config(hostname, nullptr, 3) != 0) {
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
         lListElem *ce_src = nullptr;

         qconf_is_manager_on_admin_host(username, qualified_hostname);

         spp = sge_parser_get_next(spp);
         qconf_is_manager(username);

         where = lWhere("%T( %I==%s )", CE_Type, CE_name, *spp);
         what = lWhat("%T(ALL)", CE_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CE_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
         lFreeWhere(&where);
         lFreeWhat(&what);

         aep = lFirst(alp);
         answer_exit_if_not_recoverable(aep);
         if (answer_get_status(aep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            lFreeList(&alp);
            sge_parse_return = 1;
            spp++;
            continue;
         }
         lFreeList(&alp);

         if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
            ce_src = lDechainElem(lp, lFirstRW(lp));
         } else {
            /* CS-2303 C2: modifying a non-existent complex entry implicitly adds
             * it — offer a generic template for editing instead of failing. */
            ce_src = centry_create(&alp, *spp);
         }
         lFreeList(&lp);

            /* hand it to the editor and read the result back; the engine
             * is exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::CE, &alp, &ce_src, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ce_src;
/* -mce editor path: CS-2303 C2 — upsert (add if it did not exist) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::CE, ep);
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
         char *ce_name = (char *)"template";

         qconf_is_adminhost(qualified_hostname);
         qconf_is_manager(username);

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            ce_name = *spp;
         }

         ep = centry_create(&alp, ce_name);
            /* hand it to the editor and read the result back; the engine
             * is exit-free, the client decides that a failure is fatal */
            QConfEditStatus est = qconf_edit_obj(QConfObj::CE, &alp, &ep, uid, gid);
            if (est != QConfEditStatus::OK) {
               sge_parse_return = 1;
               qconf_edit_fail_exit(est);
               continue;
            }
            ep = ep;
/* -ace editor path: CS-2303 C2 — upsert (modify if it already exists) */
         if (ep != nullptr) {
            sge_parse_return |= qconf_send_upsert_obj(os, QConfObj::CE, ep);
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

      /* "-mhgrp user"  */
      if (strcmp("-mhgrp", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         hgroup_modify(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

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
         lList *answer_list = nullptr;
         const char* group = "@template";
         bool is_validate_name = false; /* This boolean is needed to create a
                                           hgrp templete. One could have done
                                           it with a string compare deep in
                                           the code. I prefered this way. */

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            group = *spp;
            is_validate_name = true;
         }

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         hgroup_add(&answer_list, group, is_validate_name);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

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
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);

         qconf_is_manager_on_admin_host(username, qualified_hostname);
         cqueue_modify(&answer_list, *spp);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

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
         lList *answer_list = nullptr;
         const char *name = "template";

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         qconf_is_manager_on_admin_host(username, qualified_hostname);
         cqueue_add(&answer_list, name);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

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

/** @brief Offer an exec host to the editor and return the edited object.
 *
 * @param ep  the host to edit; consumed
 * @param uid uid to run the editor as
 * @param gid gid to run the editor as
 * @return the edited host, or nullptr when the result could not be read
 */
static lListElem *edit_exechost(lListElem *ep, uid_t uid, gid_t gid) {
   DENTER(TOP_LAYER);
   lList *alp = nullptr;

   /* The caller keeps ownership of ep - it either frees it itself or it is
    * still chained into a list - while qconf_edit_obj() consumes what it is
    * given, so the engine works on a copy. */
   lListElem *work = lCopyElem(ep);

   QConfEditStatus st = qconf_edit_obj(QConfObj::EH, &alp, &work, uid, gid);
   lFreeList(&alp);

   /* a read failure is reported to the caller as a nullptr, the way this
    * function has always behaved; only the editor itself is fatal */
   if (st != QConfEditStatus::OK && st != QConfEditStatus::READ_FAILED) {
      qconf_edit_fail_exit(st);
   }
   DRETURN(work);
}

/* ------------------------------------------------------------ */

static lList *edit_sched_conf(lList *confl, uid_t uid, gid_t gid) {
   DENTER(TOP_LAYER);

   int status;
   char *fname = nullptr;
   lList *alp=nullptr, *newconfl=nullptr;
   lListElem *ep = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   fname = (char *)spool_flatfile_write_object(&alp, lFirst(confl), false,
                                       SC_fields, &qconf_comma_sfi,
                                       SP_DEST_TMP, qconf_opt_format,
                                       fname, false);
   if (answer_list_output(&alp)) {
      fprintf(stderr, "%s\n", MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION);
      sge_free(&fname);
      sge_exit(1);
   }

   status = sge_edit(fname, uid, gid);

   if (status < 0) {
      unlink(fname);
      sge_free(&fname);

      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      unlink(fname);
      sge_free(&fname);

      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }

   fields_out[0] = NoName;
   ep = spool_flatfile_read_object(&alp, SC_Type, nullptr,
                                   SC_fields, fields_out, true, &qconf_comma_sfi,
                                   qconf_opt_format, nullptr, fname);

   if (answer_list_output(&alp)) {
      lFreeElem(&ep);
   }

   if (ep != nullptr) {
      missing_field = spool_get_unprocessed_field(SC_fields, fields_out, &alp);
   }

   if (missing_field != NoName) {
      lFreeElem(&ep);
      answer_list_output(&alp);
   }

   if (ep != nullptr) {
      newconfl = lCreateList("scheduler config", SC_Type);
      lAppendElem(newconfl, ep);
   }

   if ((newconfl != nullptr) && !sconf_validate_config(&alp, newconfl)) {
      lFreeList(&newconfl);
      answer_list_output(&alp);
   }

   if (newconfl == nullptr) {
      fprintf(stderr, MSG_QCONF_CANTREADCONFIG_S, "can't parse config");
      fprintf(stderr, "\n");
      unlink(fname);
      sge_free(&fname);
      sge_exit(1);
   }
   lFreeList(&alp);

   unlink(fname);
   sge_free(&fname);

   DRETURN(newconfl);
}

/* ------------------------------------------------------------ */

/** @brief Offer a user to the editor and return the edited object.
 *
 * Exits when the result cannot be read, naming the object - the wording this
 * branch has always used.
 *
 * @param ep  the user to edit; consumed
 * @param uid uid to run the editor as
 * @param gid gid to run the editor as
 * @return the edited user
 */
static lListElem *edit_user(lListElem *ep, uid_t uid, gid_t gid) {
   DENTER(TOP_LAYER);
   lList *alp = nullptr;

   /* The caller keeps ownership of ep - it either frees it itself or it is
    * still chained into a list - while qconf_edit_obj() consumes what it is
    * given, so the engine works on a copy. */
   lListElem *work = lCopyElem(ep);

   QConfEditStatus st = qconf_edit_obj(QConfObj::UU, &alp, &work, uid, gid);
   lFreeList(&alp);

   if (st == QConfEditStatus::READ_FAILED) {
      /* the user branch has always named the object in this message rather
       * than using the generic "error reading in file" */
      fprintf(stderr, MSG_QCONF_CANTREADX_S, MSG_OBJ_USER);
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   if (st != QConfEditStatus::OK) {
      qconf_edit_fail_exit(st);
   }
   DRETURN(work);
}

/* ------------------------------------------------------------ */
/** @brief Offer a project to the editor and return the edited object.
 *
 * Exits when the result cannot be read, naming the object - the wording this
 * branch has always used.
 *
 * @param ep  the project to edit; consumed
 * @param uid uid to run the editor as
 * @param gid gid to run the editor as
 * @return the edited project
 */
static lListElem *edit_project(lListElem *ep, uid_t uid, gid_t gid) {
   DENTER(TOP_LAYER);
   lList *alp = nullptr;

   /* The caller keeps ownership of ep - it either frees it itself or it is
    * still chained into a list - while qconf_edit_obj() consumes what it is
    * given, so the engine works on a copy. */
   lListElem *work = lCopyElem(ep);

   QConfEditStatus st = qconf_edit_obj(QConfObj::PR, &alp, &work, uid, gid);
   lFreeList(&alp);

   if (st == QConfEditStatus::READ_FAILED) {
      /* the project branch has always named the object in this message rather
       * than using the generic "error reading in file" */
      fprintf(stderr, MSG_QCONF_CANTREADX_S, MSG_JOB_PROJECT);
      fprintf(stderr, "\n");
      sge_exit(1);
   }
   if (st != QConfEditStatus::OK) {
      qconf_edit_fail_exit(st);
   }
   DRETURN(work);
}

/****************************************************************/
static lListElem *edit_sharetree(lListElem *ep, uid_t uid, gid_t gid) {
   DENTER(TOP_LAYER);

   int status;
   lListElem *newep = nullptr;
   const char *filename = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   bool is_missing = false;

   if (ep == nullptr) {
      is_missing = true;
   }
   if (is_missing) {
      ep = getSNTemplate();
   }

   /* JSON uses the nested-tree field list (no node ids - childnodes are full
    * child objects); ASCII uses the flat id-referenced list. The "sharetree"
    * type name is passed explicitly because the STN object type is mis-detected
    * from its content. */
   fields = (qconf_opt_format == SP_FORM_JSON)
            ? sge_build_STN_json_field_list()
            : sge_build_STN_field_list(false, true);

   filename = spool_flatfile_write_object(&alp, ep, false,
                                          fields, &qconf_name_value_list_sfi,
                                          SP_DEST_TMP, qconf_opt_format,
                                          nullptr, false, "sharetree");
   if (is_missing) {
      lFreeElem(&ep);
   }

   if (answer_list_output(&alp)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      sge_free(&fields);
      sge_error_and_exit(nullptr);
   }

   lFreeList(&alp);

   status = sge_edit(filename, uid, gid);

   if (status < 0) {
      sge_free(&fields);
      unlink(filename);
      sge_free(&filename);
      if (sge_error_and_exit(MSG_PARSE_EDITFAILED)) {
         DRETURN(nullptr);
      }
   }

   if (status > 0) {
      sge_free(&fields);
      unlink(filename);
      sge_free(&filename);
      if (sge_error_and_exit(MSG_FILE_FILEUNCHANGED)) {
         DRETURN(nullptr);
      }
   }

   fields_out[0] = NoName;
   newep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                      fields, fields_out,  true,
                                      &qconf_name_value_list_sfi,
                                      qconf_opt_format, nullptr, filename);

   if (answer_list_output(&alp)) {
      lFreeElem(&newep);
   }

   /* the nested JSON form carries no node ids -> assign them (as the flat ASCII
    * reader implicitly does) so the tree is well-formed for GDI */
   if (newep != nullptr && qconf_opt_format == SP_FORM_JSON) {
      id_sharetree(&alp, newep, 0, nullptr);
   }

   if (newep != nullptr) {
      missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
   }

   sge_free(&fields);

   if (missing_field != NoName) {
      lFreeElem(&newep);
      answer_list_output(&alp);
   }

   unlink(filename);
   sge_free(&filename);

   if (newep == nullptr) {
      /* JG: TODO: do we need the following output?
       * Isn't an error message already written by spool_flatfile_read_object?
       */
      fprintf(stderr, MSG_QCONF_CANTREADSHARETREEX_S, "");
      fprintf(stderr, "\n");
      sge_exit(1);
   }

   DRETURN(newep);
}

/* ------------------------------------------------------------ */






/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   get all acls listed in acl_arg
   from qmaster and print them

   returns
      0 on success
      1 if arglp contains acl names not present at qmaster
      -1 failed to get any acl
*/

/**************************************************************
   get all usersets listed in arglp
   from qmaster and edit them

   This is pure client code, so output to stderr/out is allowed

   returns
      0 on success
      1 if arglp contains userset names not present at qmaster
      -1 failed to get any userset
      -2 if failed due to disk error
 * @param os the stream the output is written to (CS-1841)
 **************************************************************/
static int edit_usersets(std::ostream &os, lList *arglp) {
   DENTER(TOP_LAYER);
   lList *usersets = nullptr;
   lListElem *ep=nullptr;
   const lListElem *aep=nullptr;
   lListElem *changed_ep=nullptr;
   int status;
   const char *userset_name = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   char *fname = nullptr;
   ocs::gdi::Command cmd;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   /* get all usersets named in arglp, put them into usersets */
   if (sge_client_get_acls(nullptr, arglp, &usersets)) {
      DRETURN(-1);
   }

   for_each_ep_lv(argep, arglp) {
      alp = nullptr;
      userset_name = lGetString(argep, US_name);

      ep = lGetElemStrRW(usersets, US_name, userset_name);
      if (ep == nullptr) {
         ep = lAddElemStr(&usersets, US_name, userset_name, US_Type);
         /* initialize type field in case of sge */
         lSetUlong(ep, US_type, US_ACL|US_DEPT);
         cmd = ocs::gdi::Command::ADD;
      } else {
         cmd = ocs::gdi::Command::MOD;
      }

      fname = (char *)spool_flatfile_write_object(&alp, ep, false, US_fields,
                                           &qconf_param_sfi, SP_DEST_TMP,
                                           qconf_opt_format, fname, false);
      if (answer_list_output(&alp)) {
         fprintf(stderr, "%s\n", MSG_FILE_ERRORWRITINGUSERSETTOFILE);
         DRETURN(-2);
      }

      status = sge_edit(fname, uid, gid);

      if (status < 0) {
         unlink(fname);
         fprintf(stderr, "%s\n", MSG_PARSE_EDITFAILED);
         lFreeList(&usersets);
         DRETURN(-2);  /* why should the next edit have more luck */
      }

      if (status > 0) {
         unlink(fname);
         os << qconf_cstr(MSG_FILE_FILEUNCHANGED) << "\n";
         continue;
      }

      fields_out[0] = NoName;
      changed_ep = spool_flatfile_read_object(&alp, US_Type, nullptr,
                                      US_fields, fields_out,  true, &qconf_param_sfi,
                                      qconf_opt_format, nullptr, fname);

      if (answer_list_output(&alp)) {
         lFreeElem(&changed_ep);
      }

      if (changed_ep != nullptr) {
         missing_field = spool_get_unprocessed_field(US_fields, fields_out, &alp);
      }

      if (missing_field != NoName) {
         lFreeElem(&changed_ep);
         answer_list_output(&alp);
      }

      if (changed_ep == nullptr) {
         fprintf(stderr, MSG_FILE_ERRORREADINGUSERSETFROMFILE_S, fname);
         fprintf(stderr, "\n");
         continue;   /* May be the user made a mistake. Just proceed with
                        the next */
      }

      /* Create List; append Element; and do a modification gdi call */
      lp = lCreateList("userset list", US_Type);
      lAppendElem(lp, changed_ep);
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST, cmd, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
      lFreeList(&lp);

      for_each_ep(aep, alp) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
   }

   lFreeList(&usersets);
   sge_free(&fname);
   DRETURN(0);
}


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

/*------------------------------------------------------------------------*
 * add_modify_config
 ** flags = 1 = add, 2 = modify, 3 = modify if exists, add if not
 *------------------------------------------------------------------------*/
static int add_modify_config(const char *cfn, const char *filename, uint32_t flags) {
   DENTER(TOP_LAYER);

   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   lListElem *ep = nullptr;
   int failed=0;
   char *tmpname = nullptr;
   int status;
   spooling_field *fields = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   where = lWhere("%T(%Ih=%s)", CONF_Type, CONF_name, cfn);
   what = lWhat("%T(ALL)", CONF_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   failed = false;
   ep = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      lFreeList(&alp);
      lFreeList(&lp);
      DRETURN(1);
   }

   lFreeList(&alp);

   ep = lCopyElem(lFirst(lp));
   lFreeList(&lp);

   if (ep && (flags == 1)) {
      fprintf(stderr, MSG_ANSWER_CONFIGXALREADYEXISTS_S, cfn);
      fprintf(stderr, "\n");
      lFreeElem(&ep);
      DRETURN(2);
   }
   if (!ep && (flags == 2)) {
      fprintf(stderr, MSG_ANSWER_CONFIGXDOESNOTEXIST_S, cfn);
      fprintf(stderr, "\n");
      lFreeElem(&ep);
      DRETURN(3);
   }

   if (filename == nullptr) {
      bool local_failed = false;

      /* get config or make an empty config entry if none exists */
      if (ep == nullptr) {
         ep = lCreateElem(CONF_Type);
         lSetHost(ep, CONF_name, cfn);
      }

      fields = sge_build_CONF_field_list(false);
      tmpname = (char *)spool_flatfile_write_object(&alp, ep, false, fields,
                                            &qconf_sfi, SP_DEST_TMP, qconf_opt_format,
                                            tmpname, false);

      lFreeElem(&ep);
      status = sge_edit(tmpname, uid, gid);

      if (status != 0) {
         unlink(tmpname);
         local_failed = true;
         sge_free(&fields);
         sge_free(&tmpname);
      }
      if (status < 0) {
         fprintf(stderr, "%s\n", MSG_PARSE_EDITFAILED);
         sge_free(&fields);
         DRETURN(local_failed);
      }
      else if (status > 0) {
         fprintf(stderr, "%s\n", MSG_ANSWER_CONFIGUNCHANGED);
         sge_free(&fields);
         DRETURN(local_failed);
      }

      fields_out[0] = NoName;
      ep = spool_flatfile_read_object(&alp, CONF_Type, nullptr,
                                      fields, fields_out, false, &qconf_sfi,
                                      qconf_opt_format, nullptr, tmpname);

      if (answer_list_output(&alp)) {
         lFreeElem(&ep);
         local_failed = true;
      }

      if (ep != nullptr) {
         missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
      }

      sge_free(&fields);

      if (missing_field != NoName) {
         lFreeElem(&ep);
         answer_list_output(&alp);
         local_failed = true;
      }

      /* If the configuration is legitematly nullptr, create an empty object. */
      if (!local_failed && (ep == nullptr)) {
         ep = lCreateElem(CONF_Type);
      }

      if (ep != nullptr) {
         lSetHost(ep, CONF_name, cfn);
      } else {
         fprintf(stderr, "%s\n", MSG_ANSWER_ERRORREADINGTEMPFILE);
         unlink(tmpname);
         sge_free(&tmpname);
         local_failed = true;
         DRETURN(local_failed);
      }
      unlink(tmpname);
      sge_free(&tmpname);
   } else {
      lFreeElem(&ep);

      fields_out[0] = NoName;
      fields = sge_build_CONF_field_list(false);
      ep = spool_flatfile_read_object(&alp, CONF_Type, nullptr,
                                      fields, fields_out, false, &qconf_sfi,
                                      qconf_opt_format, nullptr, filename);

      if (answer_list_output(&alp)) {
         lFreeElem(&ep);
      }

      if (ep != nullptr) {
         missing_field = spool_get_unprocessed_field(fields, fields_out, &alp);
      }

      sge_free(&fields);

      if (missing_field != NoName) {
         lFreeElem(&ep);
         answer_list_output(&alp);
      }

      if (ep != nullptr) {
         lSetHost(ep, CONF_name, cfn);
      }

      if (!ep) {
         fprintf(stderr, MSG_ANSWER_ERRORREADINGCONFIGFROMFILEX_S, filename);
         fprintf(stderr, "\n");
         failed = true;
         DRETURN(failed);
      }
   }

   lp = lCreateList("modified configuration", CONF_Type);
   lAppendElem(lp, ep);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
   lFreeList(&lp);

   /* report results */
   failed = show_answer_list(alp);

   lFreeList(&alp);

   DRETURN(failed);
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
