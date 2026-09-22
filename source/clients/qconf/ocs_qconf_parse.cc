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
#include "sgeobj/sge_attr.h"
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
#include "ocs_qconf_Category.h"
#include "ocs_qconf_acl.h"
#include "ocs_qconf_ops.h"
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
static bool show_object_list(std::ostream &os, ocs::gdi::Target, lDescr *, int, const char *);
static bool show_reserved_hgroup_hosts(std::ostream &os, const char *, const char *, const char *);
static bool show_manop_list(std::ostream &os, const char *userset_name, const char *display_name);
static int show_thread_list(std::ostream &os);
static int show_processors(std::ostream &os, bool has_binding_param);
static int show_eventclients(std::ostream &os);

/* ------------------------------------------------------------- */
static void parse_name_list_to_cull(const char *name, lList **lpp, lDescr *dp, int nm, char *s);
static bool del_host_of_type(lList *arglp, ocs::gdi::Target target);
static bool mod_reserved_hgroup(std::ostream &os, lList *arglp, const char *group,
                                ocs::gdi::SubCommand sub_command, const char *what);
static int print_acl(std::ostream &os, lList *arglp);
static int qconf_modify_attribute(lList **alpp, int from_file, char ***spp, lListElem **epp, ocs::gdi::SubCommand sub_command, struct object_info_entry *info_entry);
static lListElem *edit_exechost(lListElem *ep, uid_t uid, gid_t gid);
static int edit_usersets(std::ostream &os, lList *arglp);

/************************************************************************/
static int print_config(std::ostream &os, const char *config_name);
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
typedef spooling_field *(*qconf_build_fields_fn)(spool_flatfile_format format, bool *owned);

/** @brief The configuration object types the generic engines handle.
 *
 * The cluster configuration is deliberately absent: it is keyed on the host
 * basename rather than on a field, which is why it has its own
 * qconf_conf_export()/qconf_conf_apply_path(). */
enum class QConfObj {
   CAL, CK, CE, EH, HGRP, PE, PR, CQ, RL, RQS, US, UU, STN, SC
};

/** @brief What the read path needs on top of the common descriptor (CS-2796).
 *
 * There is one deliberate difference between showing an object and writing it
 * to a file, and it is in @c fields: -se builds its field list with
 * to_stdout=true, so it shows load_values and processors, which -Se leaves out
 * on purpose - the export has to round-trip. Pinned by exechost.show.plain vs
 * exechost.export.plain (CS-2794). nullptr means "the same as the write path".
 *
 * The layout itself is now the same for both: -suser used to render with
 * qconf_param_sfi where -Suser uses qconf_sfi, which never reached the output
 * (every UU field has width 0, so align_names does nothing, and the sublist
 * fields only exist with spool=true, which qconf does not use). -Suser is new
 * in 9.2, so there was no compatibility reason to keep the two apart. */
struct qconf_read_desc {
   qconf_build_fields_fn fields;   ///< field list for -s<obj>, nullptr to use build_fields
   const char *list_name;          ///< object name in the "no X defined" message of -s<obj>l
   const char *not_found_msg;      ///< format string of the "X is not a Y" message of -s<obj>
   void (*prepare)(lListElem *);   ///< fixup before rendering, nullptr for none
};

/** @brief Everything the generic engines need to know about one object type. */
struct qconf_object_desc {
   ocs::gdi::Target target;                          ///< GDI target list
   const lDescr *descr;                              ///< CULL descriptor
   int name_nm;                                      ///< name field, NoName for the singletons
   qconf_build_fields_fn build_fields;               ///< field list provider (may allocate)
   const spool_flatfile_instr *sfi;                  ///< flatfile spooling instruction
   const char *json_type_name;                       ///< explicit JSON $id, nullptr derives it
   int (*validate)(const lListElem *, lList **);     ///< semantic check, nullptr for none
   int (*prepare)(lListElem *, lList **);            ///< pre-send fixup, nullptr for none
   ocs::gdi::SubCommand sub_cmd;                     ///< GDI sub-command of an upsert
   bool as_list;                                     ///< file holds a list, not one object
   bool del_parse_values;                            ///< parse values when reading for a delete
   bool is_root;                                     ///< singleton export: object is the root
   const char *exclude1;                             ///< export: skip this name
   const char *exclude2;                             ///< export: skip this name too
   qconf_read_desc read;                             ///< the read path additions (CS-2796)
};

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

/* qconf_build_fields_fn providers (KTD8): objects with a shared static field table
 * report owned=false; objects whose import field list is built on demand report
 * owned=true so the engine frees it. The provider mirrors exactly the field list the
 * matching -A<obj> import handler reads with. */
static spooling_field *qconf_fields_CAL (spool_flatfile_format, bool *o) { *o = false; return CAL_fields;  }
static spooling_field *qconf_fields_CK  (spool_flatfile_format, bool *o) { *o = false; return CK_fields;   }
static spooling_field *qconf_fields_CE  (spool_flatfile_format, bool *o) { *o = false; return CE_fields;   }
static spooling_field *qconf_fields_RL  (spool_flatfile_format, bool *o) { *o = false; return RL_fields;   }
static spooling_field *qconf_fields_PE  (spool_flatfile_format, bool *o) { *o = false; return PE_fields;   }
static spooling_field *qconf_fields_HGRP(spool_flatfile_format, bool *o) { *o = false; return HGRP_fields; }
static spooling_field *qconf_fields_CQ  (spool_flatfile_format, bool *o) { *o = false; return CQ_fields;   }
static spooling_field *qconf_fields_US  (spool_flatfile_format, bool *o) { *o = false; return US_fields;   }
static spooling_field *qconf_fields_RQS (spool_flatfile_format, bool *o) { *o = false; return RQS_fields;  }
static spooling_field *qconf_fields_SC  (spool_flatfile_format, bool *o) { *o = false; return SC_fields;   }
/* import list omits runtime load_values/processors (to_stdout=false) so it round-trips (KTD2) */
static spooling_field *qconf_fields_EH  (spool_flatfile_format, bool *o) { *o = true;  return sge_build_EH_field_list(false, false, false); }
/* -se shows the host as it is now, so the runtime values are included (CS-2796) */
static spooling_field *qconf_show_fields_EH(spool_flatfile_format, bool *o) { *o = true;  return sge_build_EH_field_list(false, true, false); }

/* defined below, next to the json helpers it uses */
static void qconf_show_prepare_EH(lListElem *ep);
static spooling_field *qconf_fields_PR  (spool_flatfile_format, bool *o) { *o = true;  return sge_build_PR_field_list(false); }
static spooling_field *qconf_fields_UU  (spool_flatfile_format, bool *o) { *o = true;  return sge_build_UU_field_list(false); }
static spooling_field *qconf_fields_STN (spool_flatfile_format fmt, bool *o) {
   *o = true;
   return (fmt == SP_FORM_JSON) ? sge_build_STN_json_field_list() : sge_build_STN_field_list(false, true);
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

/**
 * @brief Validate-hook adapter for parallel environments (CS-2301).
 *
 * pe_validate() takes two extra arguments (a startup flag and the master userset
 * list) that the generic validate hook does not; this wrapper fixes them to the
 * values the qconf client uses (0 / nullptr), matching the editor paths.
 *
 * @param ep   the parsed PE object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, another status on failure
 */
static int
qconf_pe_validate(const lListElem *ep, lList **alpp) {
   return pe_validate(const_cast<lListElem *>(ep), alpp, 0, nullptr);
}

/**
 * @brief Validate-hook adapter for roles (CS-2302).
 *
 * ocs::Role::validate() returns a bool and takes a startup flag the generic
 * validate hook does not; this wrapper maps it to the hook's status convention,
 * fixing startup to false as the editor paths do.
 *
 * @param ep   the parsed role object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, STATUS_ESEMANTIC on failure
 */
static int
qconf_role_validate(const lListElem *ep, lList **alpp) {
   return ocs::Role::validate(ep, alpp, false) ? STATUS_OK : STATUS_ESEMANTIC;
}

/**
 * @brief Prepare-hook adapter for exec hosts (CS-2304): resolve the host name.
 *
 * Exec host objects must carry a resolved (canonical) host name before the name
 * is used for the upsert existence-check and the GDI send; this transform
 * resolves EH_name in place, matching what the interactive -ae/-Ae paths do.
 *
 * @param ep   the parsed exec host object (modified in place)
 * @param alpp answer list to receive a message on resolution failure
 * @return STATUS_OK on success, STATUS_ESEMANTIC if the host cannot be resolved
 */
static int
qconf_eh_resolve(lListElem *ep, lList **alpp) {
   if (sge_resolve_host(ep, EH_name) != CL_RETVAL_OK) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(ep, EH_name));
      return STATUS_ESEMANTIC;
   }
   return STATUS_OK;
}

/**
 * @brief Validate-hook adapter for cluster queues (CS-2305).
 *
 * cqueue attribute verification (cqueue_verify_attributes) is what the cqueue
 * GDI wrapper runs before ADD/MOD; this maps it to the hook's status convention.
 *
 * @param ep   the parsed cluster queue object to verify
 * @param alpp answer list to receive verification messages
 * @return STATUS_OK on success, STATUS_ESEMANTIC on failure
 */
static int
qconf_cq_verify(const lListElem *ep, lList **alpp) {
   lListElem *e = const_cast<lListElem *>(ep);
   return cqueue_verify_attributes(e, alpp, e, false, nullptr, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr)
      ? STATUS_OK : STATUS_ESEMANTIC;
}

/**
 * @brief Validate-hook adapter for usersets (CS-2310).
 *
 * userset_validate_entries() is what the interactive -Au/-Mu paths run before
 * ADD/MOD; it takes a non-const element, so this wrapper const-casts to fit the
 * generic validate hook signature.
 *
 * @param ep   the parsed userset object to validate
 * @param alpp answer list to receive validation messages
 * @return STATUS_OK on success, another status on failure
 */
static int
qconf_us_validate(const lListElem *ep, lList **alpp) {
   return userset_validate_entries(const_cast<lListElem *>(ep), alpp);
}

/* ===== CS-2795: the object descriptor table ================================
 * The generic engines above (qconf_apply_path, qconf_delete_path,
 * qconf_export_path, qconf_export_singleton) already drive every file-based
 * option from one implementation each. What every call site still repeated was
 * the tuple identifying the object: target, descriptor, name field, field list,
 * spooling instruction and the object's hooks. This table holds it once.
 *
 * Adding a new configuration object type is now one row plus its field
 * provider, instead of a tuple copied into each of the add, modify, delete and
 * export branches. */

/** @brief One row per object type, indexed by QConfObj.
 *
 * Kept in the order of the enum so that qconf_desc() is a plain index. */
static const qconf_object_desc qconf_object_table[] = {
   /* CAL  */ { ocs::gdi::Target::CAL_LIST,  CAL_Type,  CAL_name,  qconf_fields_CAL,  &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "calendar", MSG_CALENDAR_XISNOTACALENDAR_S, nullptr } },
   /* CK   */ { ocs::gdi::Target::CK_LIST,   CK_Type,   CK_name,   qconf_fields_CK,   &qconf_sfi,                 nullptr,      ckpt_validate,       nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "ckpt interface definition", MSG_CKPT_XISNOTCHKPINTERFACEDEF_S, nullptr } },
   /* CE   */ { ocs::gdi::Target::CE_LIST,   CE_Type,   CE_name,   qconf_fields_CE,   &qconf_ce_sfi,              nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "complex entry", nullptr, nullptr } },
   /* EH   */ { ocs::gdi::Target::EH_LIST,   EH_Type,   EH_name,   qconf_fields_EH,   &qconf_sfi,                 "exechost",   nullptr,             qconf_eh_resolve,  ocs::gdi::SubCommand::NONE,    false, true,  false, SGE_TEMPLATE_NAME,  SGE_GLOBAL_NAME,
              { qconf_show_fields_EH, "execution host", MSG_EXEC_XISNOTANEXECUTIONHOST_S, qconf_show_prepare_EH } },
   /* HGRP */ { ocs::gdi::Target::HGRP_LIST, HGRP_Type, HGRP_name, qconf_fields_HGRP, &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "host group list", nullptr, nullptr } },
   /* PE   */ { ocs::gdi::Target::PE_LIST,   PE_Type,   PE_name,   qconf_fields_PE,   &qconf_sfi,                 nullptr,      qconf_pe_validate,   nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "parallel environment", MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S, nullptr } },
   /* PR   */ { ocs::gdi::Target::PR_LIST,   PR_Type,   PR_name,   qconf_fields_PR,   &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "project list", MSG_PROJECT_XISNOKNWOWNPROJECT_S, nullptr } },
   /* CQ   */ { ocs::gdi::Target::CQ_LIST,   CQ_Type,   CQ_name,   qconf_fields_CQ,   &qconf_sfi,                 nullptr,      qconf_cq_verify,     nullptr,           ocs::gdi::SubCommand::SET_ALL, false, false, false, nullptr,            nullptr,
              { nullptr, "cqueue list", nullptr, nullptr } },
   /* RL   */ { ocs::gdi::Target::RL_LIST,   RL_Type,   RL_name,   qconf_fields_RL,   &qconf_sfi,                 nullptr,      qconf_role_validate, nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "role", MSG_ROLE_DOESNOTEXIST_S, nullptr } },
   /* RQS  */ { ocs::gdi::Target::RQS_LIST,  RQS_Type,  RQS_name,  qconf_fields_RQS,  &qconf_rqs_sfi,             nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    true,  true,  false, nullptr,            nullptr,
              { nullptr, "resource quota set list", nullptr, nullptr } },
   /* US   */ { ocs::gdi::Target::US_LIST,   US_Type,   US_name,   qconf_fields_US,   &qconf_param_sfi,           nullptr,      qconf_us_validate,   nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "userset list", nullptr, nullptr } },
   /* UU   */ { ocs::gdi::Target::UU_LIST,   UU_Type,   UU_name,   qconf_fields_UU,   &qconf_sfi,                 nullptr,      nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, "user list", MSG_USER_XISNOKNOWNUSER_S, nullptr } },
   /* STN  */ { ocs::gdi::Target::STN_LIST,  STN_Type,  NoName,    qconf_fields_STN,  &qconf_name_value_list_sfi, "sharetree",  nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  true,  nullptr,            nullptr,
              { nullptr, nullptr, nullptr, nullptr } },
   /* SC   */ { ocs::gdi::Target::SC_LIST,   SC_Type,   NoName,    qconf_fields_SC,   &qconf_comma_sfi,           "sched_conf", nullptr,             nullptr,           ocs::gdi::SubCommand::NONE,    false, true,  false, nullptr,            nullptr,
              { nullptr, nullptr, nullptr, nullptr } },
};

/** @brief The descriptor of one object type.
 * @param o the object type
 * @return its row in qconf_object_table
 */
static const qconf_object_desc &
qconf_desc(QConfObj o) {
   return qconf_object_table[static_cast<size_t>(o)];
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

/** @brief Show one object of the given type on stdout.
 *
 * Fetches the object by name - or, for a type without a name field, the single
 * one that exists - and renders it in the layout of the descriptor.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param o    the object type
 * @param name the object name, ignored for a type without a name field
 * @return what happened, see QConfShowStatus
 */
static QConfShowStatus
qconf_show_object(std::ostream &os, QConfObj o, const char *name) {
   DENTER(TOP_LAYER);
   const qconf_object_desc &d = qconf_desc(o);
   lList *lp = nullptr;
   lList *alp = nullptr;

   // a type without a name field holds exactly one object, so it is fetched unconditionally;
   // a host name is matched with the host comparison, which folds the domain away
   lCondition *where = nullptr;
   if (d.name_nm != NoName) {
      int pos = lGetPosInDescr(d.descr, d.name_nm);
      bool by_host = (lGetPosType(d.descr, pos) == lHostT);
      where = by_host ? lWhere("%T( %Ih=%s )", d.descr, d.name_nm, name)
                      : lWhere("%T( %I==%s )", d.descr, d.name_nm, name);
   }
   lEnumeration *what = lWhat("%T(ALL)", d.descr);
   alp = ocs::gdi::Client::sge_gdi(d.target, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);

   const lListElem *aep = lFirst(alp);
   answer_exit_if_not_recoverable(aep);
   if (answer_get_status(aep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      lFreeList(&alp);
      lFreeList(&lp);
      DRETURN(QConfShowStatus::GDI_FAILED);
   }
   lFreeList(&alp);

   // lGetNumberOfElem() would flag a null list as a CULL error, so it is checked first
   if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
      // the wording is per object type, the shape of the message is not
      if (d.read.not_found_msg != nullptr) {
         fprintf(stderr, d.read.not_found_msg, name);
         fprintf(stderr, "\n");
      }
      lFreeList(&lp);
      DRETURN(QConfShowStatus::NOT_FOUND);
   }

   // -se is the one type whose read field list differs from the write one
   qconf_build_fields_fn provider = (d.read.fields != nullptr) ? d.read.fields : d.build_fields;
   bool owned = false;
   spooling_field *fields = provider(qconf_opt_format, &owned);

   lListElem *ep = lFirstRW(lp);
   if (d.read.prepare != nullptr) {
      d.read.prepare(ep);
   }
   const char *filename = spool_flatfile_write_object(&alp, ep, d.is_root, fields, d.sfi,
                                                      SP_DEST_STREAM, qconf_opt_format, nullptr,
                                                      false, d.json_type_name, &os);
   sge_free(&filename);
   if (owned) {
      sge_free(&fields);
   }
   lFreeList(&lp);

   if (answer_list_output(&alp)) {
      DRETURN(QConfShowStatus::WRITE_FAILED);
   }
   DRETURN(QConfShowStatus::OK);
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

/** @brief Show the names of all objects of the given type, one per line.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param o the object type
 * @return true when the list was printed, false when it was empty or the request failed
 */
static bool
qconf_show_names(std::ostream &os, QConfObj o) {
   const qconf_object_desc &d = qconf_desc(o);
   return show_object_list(os, d.target, const_cast<lDescr *>(d.descr), d.name_nm, d.read.list_name);
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

/**
 * @brief Fill complex-value sublists with their CULL type/numeric value (CS-2313a).
 *
 * The raw complex_values / threshold entries stored on an exec host or queue carry
 * only a name + string value (CE_valtype is NONE). To emit native JSON numbers
 * (slots -> 32, h_vmem -> bytes, h_rt -> seconds) the writer needs CE_valtype and
 * CE_doubleval, which centry_list_fill_request() derives from the centry
 * definitions - the same fill qstat performs before display.
 *
 * Runs only for -fmt json (ASCII output uses CE_stringval and is unaffected), is
 * best-effort (unresolved entries keep their string value), and mutates only the
 * throwaway in-memory object that is about to be serialized.
 *
 * @param obj       the object whose complex sublists should be filled (modified)
 * @param ce_fields the CE_Type sublist field ids to fill (e.g. EH_consumable_config_list)
 * @param n_fields  number of entries in @p ce_fields
 */
void qconf_json_fill_complex(lListElem *obj, const int *ce_fields, int n_fields) {
   if (qconf_opt_format != SP_FORM_JSON || obj == nullptr) {
      return;
   }
   lList *alp = nullptr;
   lList *master_centry_list = centry_list_get_via_gdi(&alp);
   lFreeList(&alp);
   if (master_centry_list == nullptr) {
      return;
   }
   for (int i = 0; i < n_fields; i++) {
      lList *sub = lGetListRW(obj, ce_fields[i]);
      if (sub != nullptr && lGetNumberOfElem(sub) > 0) {
         lList *fill_alp = nullptr;
         centry_list_fill_request(sub, &fill_alp, master_centry_list, true, true, true);
         lFreeList(&fill_alp);
      }
   }
   lFreeList(&master_centry_list);
}

/**
 * @brief Render an exec host's load_values typed by their complex definition (CS-2313a).
 *
 * For -fmt json, values such as mem_total are rendered as bytes, num_proc as an
 * integer, np_load_avg as a double, ... instead of as raw strings. HL_Type load values
 * carry no type, so we build a CE_Type list (name + string value), resolve each entry's
 * type from the master complex list (tolerant of load names that are not defined
 * complexes), parse numeric values into CE_doubleval, and swap the typed CE list into
 * the load-list field. The generic JSON writer then renders it via its typed
 * complex-value path; unmatched or string-typed loads keep their string value. Runs
 * only for -fmt json and mutates only the throwaway object about to be shown.
 *
 * @param host          the (throwaway) exec host element to be shown
 * @param load_list_nm  CULL field id of the load-value list (e.g. EH_load_list)
 */
static void
qconf_json_type_load_values(lListElem *host, int load_list_nm) {
   if (qconf_opt_format != SP_FORM_JSON || host == nullptr) {
      return;
   }
   const lList *hl = lGetList(host, load_list_nm);
   if (hl == nullptr || lGetNumberOfElem(hl) == 0) {
      return;
   }
   lList *alp = nullptr;
   lList *master_centry_list = centry_list_get_via_gdi(&alp);
   lFreeList(&alp);
   if (master_centry_list == nullptr) {
      return;
   }

   lList *ce_list = lCreateList("load_values", CE_Type);
   const lListElem *hlep;
   for_each_ep(hlep, hl) {
      lListElem *ce = lCreateElem(CE_Type);
      lSetString(ce, CE_name, lGetString(hlep, HL_name));
      lSetString(ce, CE_stringval, lGetString(hlep, HL_value));
      lAppendElem(ce_list, ce);
   }

   /* fill CE_valtype from the complex definitions (skips unknown load names) */
   centry_list_fill_config(ce_list, master_centry_list);

   /* parse numeric values into CE_doubleval so the writer emits native numbers */
   for (lListElem *ce = lFirstRW(ce_list); ce != nullptr; ce = lNextRW(ce)) {
      const auto type = static_cast<ocs::CEntry::Type>(lGetUlong(ce, CE_valtype));
      const char *s = lGetString(ce, CE_stringval);
      double dval = 0.0;
      switch (type) {
         case ocs::CEntry::Type::INT:
         case ocs::CEntry::Type::TIME:
         case ocs::CEntry::Type::MEM:
         case ocs::CEntry::Type::DOUBLE:
         case ocs::CEntry::Type::BOOL:
            if (s != nullptr && parse_ulong_val(&dval, nullptr, type, s, nullptr, 0)) {
               lSetDouble(ce, CE_doubleval, dval);
            }
            break;
         default:
            break;   /* string-like / unmatched -> writer emits CE_stringval */
      }
   }

   lSetList(host, load_list_nm, ce_list);
   lFreeList(&master_centry_list);
}

/** @brief Type the numeric fields of an exec host before it is rendered.
 *
 * CS-2313a: complex_values and the load values are strings in CULL, so -fmt
 * json would quote them without this.
 *
 * @param ep the exec host about to be written
 */
static void
qconf_show_prepare_EH(lListElem *ep) {
   static const int eh_ce_fields[] = { EH_consumable_config_list };
   qconf_json_fill_complex(ep, eh_ce_fields, 1);
   qconf_json_type_load_values(ep, EH_load_list);
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
   const char *filename_stdout;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   const char* qualified_hostname = component_get_qualified_hostname();
   const char* username = component_get_username();
   uint32_t prog_number = component_get_component_id();
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   bool has_binding_param = false;

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
    * What comes back are the invocation-wide flags, taken over into the globals
    * just below, and the ordered list of operations the execution phase walks -
    * which is also what get_bundle() marshals in CS-2799.
    *
    * The flags have to take effect no matter where they appear, so that
    * "qconf -sq all.q -fmt json" renders as JSON although -fmt comes last.
    * qconf_parse_args() reads them in a pass of its own over argv, exactly as
    * the hand written pre-scan did.
    *
    * MISSING_ARGUMENT and UNKNOWN_OPTION are not acted on here but after the
    * execution phase, so that a partially valid command line still produces the
    * output of its valid operations before the complaint - the order the single
    * parse-and-execute pass produced. */
   qconf_invocation inv;
   std::string bad_token;
   const QConfParseStatus parse_status = qconf_parse_args(inv, argv, bad_token);
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
      default:
         break;
   }

   has_binding_param        = inv.binding;
   qconf_opt_dry_run        = inv.dry_run;
   qconf_opt_force          = inv.force;
   qconf_opt_strict         = inv.strict;
   qconf_opt_format         = inv.format;
   ocs_json_value_format_opt = inv.value_format;

   /* CS-2798: the execution phase. It walks the operations the parser produced,
    * in the order they were typed, and hands each option block its own argument
    * vector - { switch, arg, ..., nullptr }. That is the shape the blocks have
    * always read, so `*spp`, sge_parser_get_next() and sge_next_is_an_opt()
    * keep working unchanged; they simply no longer reach across the operation
    * they belong to. The trailing `spp++; continue;` of every block now means
    * "on to the next operation". */
   for (qconf_op &op : inv.ops) {
      spp = op.as_argv();

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
      dstring object_name = DSTRING_INIT;
      dstring attribute_pattern = DSTRING_INIT;
      dstring value_pattern = DSTRING_INIT;
      bool handle_cqueue;
      bool handle_domain;
      bool handle_qinstance;
      bool handle_exechost;

      spp = sge_parser_get_next(spp);
      sge_dstring_copy_string(&object_name, *spp);
      spp = sge_parser_get_next(spp);
      sge_dstring_copy_string(&attribute_pattern, *spp);
      spp = sge_parser_get_next(spp);
      sge_dstring_copy_string(&value_pattern, *spp);

      handle_cqueue = (strcmp(sge_dstring_get_string(&object_name), "queue") == 0) ? true : false;
      handle_domain = (strcmp(sge_dstring_get_string(&object_name), "queue_domain") == 0) ? true : false;
      handle_qinstance = (strcmp(sge_dstring_get_string(&object_name), "queue_instance") == 0) ? true : false;
      handle_exechost = (strcmp(sge_dstring_get_string(&object_name), "exechost") == 0) ? true : false;

      if (handle_exechost) {
         lEnumeration *what_all = nullptr;
         lList *list = nullptr;
         lList *answer_list = nullptr;
         const lListElem *answer_ep;

         what_all = lWhat("%T(ALL)", EH_Type);
         answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &list, nullptr, what_all);
         lFreeWhat(&what_all);

         answer_ep = lFirst(answer_list);
         answer_exit_if_not_recoverable(answer_ep);
         if (answer_get_status(answer_ep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(answer_ep, AN_text));
            lFreeList(&answer_list);
            sge_dstring_free(&object_name);
            sge_dstring_free(&attribute_pattern);
            sge_dstring_free(&value_pattern);
            DRETURN(0);
         }
         lFreeList(&answer_list);

         for_each_rw_lv (elem, list) {
            const char *hostname = nullptr;
            bool already_handled = false;

            /*
             * hostname
             */
            hostname = lGetHost(elem, EH_name);
            if (!fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_HOSTNAME, 0) &&
                !fnmatch(sge_dstring_get_string(&value_pattern), hostname, 0)) {
               os << qconf_cstr(hostname) << "\n";
               already_handled = true;
            }

            /*
             * load scaling list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_LOAD_SCALING, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_scaling_list);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_sprintf_append(&value, "%s=%.10g", lGetString(value_elem, HS_name), lGetDouble(value_elem, HS_value));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, ",");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  os << qconf_cstr(hostname) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * complex_values list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_COMPLEX_VALUES, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_consumable_config_list);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_sprintf_append(&value, "%s=", lGetString(value_elem, CE_name));
                  if (lGetString(value_elem, CE_stringval) != nullptr) {
                     sge_dstring_append(&value, lGetString(value_elem, CE_stringval));
                  } else {
                     sge_dstring_sprintf_append(&value, "%f", lGetString(value_elem, CE_doubleval));
                  }
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, ",");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  os << qconf_cstr(hostname) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * load_values list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_LOAD_VALUES, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_load_list);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_sprintf_append(&value, "%s=", lGetString(value_elem, HL_name));
                  sge_dstring_sprintf_append(&value, "%s", lGetString(value_elem, HL_value));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, ",");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  os << qconf_cstr(hostname) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * processors
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_PROCESSORS, 0)) {
               dstring value = DSTRING_INIT;
               const char *value_string = nullptr;

               sge_dstring_sprintf(&value, "%d", (int) lGetUlong(elem, EH_processors));
               value_string = sge_dstring_get_string(&value);
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  os << qconf_cstr(hostname) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }


            /*
             * user_lists list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_USER_LISTS, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_acl);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_append(&value, lGetString(value_elem, US_name));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, " ");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  os << qconf_cstr(hostname) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * user_lists list
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern), SGE_ATTR_XUSER_LISTS, 0)) {
               dstring value = DSTRING_INIT;
               const lList *value_list = nullptr;
               const lListElem *value_elem = nullptr;
               const char *value_string = nullptr;

               value_list = lGetList(elem, EH_xacl);
               value_elem = lFirst(value_list);

               while (value_elem != nullptr) {
                  sge_dstring_append(&value, lGetString(value_elem, US_name));
                  value_elem = lNext(value_elem);
                  if (value_elem != nullptr) {
                     sge_dstring_append(&value, " ");
                  }
               }
               value_string = sge_dstring_get_string(&value);
               if (value_string == nullptr) {
                  sge_dstring_copy_string(&value, "NONE");
                  value_string = sge_dstring_get_string(&value);
               }
               if (!fnmatch(sge_dstring_get_string(&value_pattern), value_string, 0)) {
                  os << qconf_cstr(hostname) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }
         }
      } else if (handle_cqueue || handle_domain || handle_qinstance) {
         lEnumeration *what_all = nullptr;
         lList *list = nullptr;
         lList *answer_list = nullptr;
         const lListElem *answer_ep;

         what_all = lWhat("%T(ALL)", CQ_Type);
         answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &list, nullptr, what_all);
         lFreeWhat(&what_all);

         answer_ep = lFirst(answer_list);
         answer_exit_if_not_recoverable(answer_ep);
         if (answer_get_status(answer_ep) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(answer_ep, AN_text));
            lFreeList(&answer_list);
            sge_dstring_free(&object_name);
            sge_dstring_free(&attribute_pattern);
            sge_dstring_free(&value_pattern);
            DRETURN(0);
         }
         lFreeList(&answer_list);

         /*
          * CS-2677: "qconf -sobjl queue hostlist <pattern>" matches against the
          * host list, which the queue does not carry any more -- it is the
          * member list of "@@<queue>". Fetch the groups and join them on, so
          * the pattern is matched against what "qconf -sq" would print.
          */
         {
            lEnumeration *hgrp_what = lWhat("%T(ALL)", HGRP_Type);
            lList *hgrp_list = nullptr;
            lList *hgrp_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &hgrp_list, nullptr, hgrp_what);

            lFreeWhat(&hgrp_what);
            answer_exit_if_not_recoverable(lFirst(hgrp_answer_list));
            cqueue_list_fill_hostlist(list, hgrp_list);
            lFreeList(&hgrp_list);
            lFreeList(&hgrp_answer_list);
         }

         for_each_rw_lv (elem, list) {
            int index = 0;
            bool already_handled = false;

            /*
             * Handle special case: qname
             */
            if (!fnmatch(sge_dstring_get_string(&attribute_pattern),
                         SGE_ATTR_QNAME, 0)) {
               if (handle_cqueue &&
                   !fnmatch(sge_dstring_get_string(&value_pattern), lGetString(elem, CQ_name), 0)) {
                  os << qconf_cstr(lGetString(elem, CQ_name)) << "\n";
                  already_handled = true;
               }
            }

            /*
             * Handle special case: hostlist
             */
            if (!already_handled &&
                !fnmatch(sge_dstring_get_string(&attribute_pattern),
                         SGE_ATTR_HOST_LIST, 0)) {
               dstring value = DSTRING_INIT;
               const lList *hostref_list = lGetList(elem, CQ_hostlist);

               if (hostref_list != nullptr) {
                  href_list_append_to_dstring(hostref_list, &value);
               } else {
                  sge_dstring_copy_string(&value, "NONE");
               }
               if (handle_cqueue &&
                   !fnmatch(sge_dstring_get_string(&value_pattern), sge_dstring_get_string(&value), 0)) {
                  os << qconf_cstr(lGetString(elem, CQ_name)) << "\n";
                  already_handled = true;
               }
               sge_dstring_free(&value);
            }

            /*
             * Handle all other CQ attributes
             */
            while (!already_handled &&
                   cqueue_attribute_array[index].cqueue_attr != NoName) {
               if (!fnmatch(sge_dstring_get_string(&attribute_pattern),
                            cqueue_attribute_array[index].name, 0)) {
                  dstring value = DSTRING_INIT;
                  const lList *attribute_list = lGetList(elem, cqueue_attribute_array[index].cqueue_attr);

                  already_handled = false;
                  for_each_ep_lv(attribute, attribute_list) {
                     const lDescr *descr = lGetListDescr(attribute_list);
                     lList *tmp_attribute_list = lCreateList("", descr);
                     lListElem *tmp_attribute = lCopyElem(attribute);
                     const char *host_hgroup = lGetHost(attribute, cqueue_attribute_array[index].href_attr);
                     bool is_cqueue;
                     bool is_domain;
                     bool is_qinstance;

                     is_cqueue = (strcmp(host_hgroup, HOSTREF_DEFAULT) == 0) ? true : false;
                     is_domain = false;
                     if (!is_cqueue) {
                        is_domain = ocs::is_hgroup_name(host_hgroup);
                     }
                     is_qinstance = (!is_domain && !is_cqueue) ? true : false;

                     lAppendElem(tmp_attribute_list, tmp_attribute);
                     lSetHost(tmp_attribute,
                              cqueue_attribute_array[index].href_attr,
                              HOSTREF_DEFAULT);

                     attr_list_append_to_dstring(tmp_attribute_list,
                                                 &value, descr, cqueue_attribute_array[index].href_attr,
                                                 cqueue_attribute_array[index].value_attr);

                     if (!fnmatch(sge_dstring_get_string(&value_pattern), sge_dstring_get_string(&value), 0)) {
                        if (handle_cqueue && is_cqueue) {
                           os << qconf_cstr(lGetString(elem, CQ_name)) << "\n";
                        } else if ((handle_domain && is_domain) || (handle_qinstance && is_qinstance)) {
                           os << qconf_cstr(lGetString(elem, CQ_name)) << "@" << qconf_cstr(host_hgroup) << "\n";
                        }
                        already_handled = true;
                     }
                     lFreeList(&tmp_attribute_list);
                     if (already_handled) {
                        break;
                     }
                  }
                  sge_dstring_free(&value);
                  if (already_handled) {
                     break;
                  }
               }
               index++;
            }
         }
         lFreeList(&list);
      }
      sge_dstring_free(&object_name);
      sge_dstring_free(&attribute_pattern);
      sge_dstring_free(&value_pattern);

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

         if (!centry_list_show(os, &answer_list)) {
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

         if (!ocs::CategoryQconf::show_list(os, &answer_list)) {
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

         if (qconf_show_status_classic(qconf_show_object(os, QConfObj::CAL, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-scall" */

      if (strcmp("-scall", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::CAL)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

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
               if (print_config(os, hostname) != 0) {
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

         if (qconf_show_status_classic(qconf_show_object(os, QConfObj::CK, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sckptl" */
      if (strcmp("-sckptl", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::CK)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sconfl" */

      if (strcmp("-sconfl", *spp) == 0) {
         if (!show_object_list(os, ocs::gdi::Target::CONF_LIST, CONF_Type, CONF_name, "config")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

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

         if (qconf_show_object(os, QConfObj::EH, lGetHost(hep, EH_name)) != QConfShowStatus::OK) {
            sge_parse_return = 1;
         }
         lFreeElem(&hep);

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-secl" */
      if (strcmp("-secl", *spp) == 0) {
         show_eventclients(os);
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sel" */
      if (strcmp("-sel", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::EH)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sep" */
      if (strcmp("-sep", *spp) == 0) {
         show_processors(os, has_binding_param);
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sh" */
      if (strcmp("-sh", *spp) == 0) {
         if (!show_reserved_hgroup_hosts(os, ADMIN_HOSTGROUP, "adminhost",
               "administrative host")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-srqsl " */
      if (strcmp("-srqsl", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::RQS)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-srqs [rqs_name,...]" */
      if (strcmp("-srqs", *spp) == 0) {
         const char *name = nullptr;
         bool ret = true;

         if (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            name = *spp;
         }
         ret = rqs_show(os, &alp, name);
         if (!ret) {
            show_answer(alp);
            sge_parse_return = 1;
         }
         lFreeList(&alp);

         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-stl " */
      if (strcmp("-stl", *spp) == 0) {
         /* show_thread_list() returns 0 on success and -1 on error (not a bool),
          * so compare against 0 - a plain !ret would flag success as an error */
         if (show_thread_list(os) != 0) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sm" */

      if (strcmp("-sm", *spp) == 0) {
         /* CS-2394: managers are the members of the reserved "manager" userset. */
         if (!show_manop_list(os, MANAGER_USERSET, "manager")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-srole role_name" */
      if (strcmp("-srole", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_status_classic(qconf_show_object(os, QConfObj::RL, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-srolel" */
      if (strcmp("-srolel", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::RL)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-sp pe_name" */
      if (strcmp("-sp", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_status_classic(qconf_show_object(os, QConfObj::PE, *spp), &sge_parse_return)) {
            DRETURN(1);
         }

         spp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-spl" */
      if (strcmp("-spl", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::PE)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scel" */
      if (strcmp("-scel", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::CE)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-so" */

      if (strcmp("-so", *spp) == 0) {
         /* CS-2394: operators are the members of the reserved "operator" userset. */
         if (!show_manop_list(os, OPERATOR_USERSET, "operator")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ssconf" */

      if (strcmp("-ssconf", *spp) == 0) {
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

         /* SC has no primary key, so the JSON $id type must be set explicitly
          * (content resolution would yield "petask") */
         filename_stdout = spool_flatfile_write_object(&alp, lFirst(lp), false, SC_fields,
                                     &qconf_comma_sfi, SP_DEST_STREAM,
                                     qconf_opt_format, nullptr, false, "sched_conf", &os);

         sge_free(&filename_stdout);
         if (answer_list_output(&alp)) {
            fprintf(stderr, "%s\n", MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION);
            sge_parse_return = 1;
            spp++;
            continue;
         }

         lFreeList(&lp);

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-sstnode node_path[,...]"  show sharetree node */

      if (strcmp("-sstnode", *spp) == 0) {
         int found = 0;

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
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw_lv (argep, arglp) {
            const char *nodepath = lGetString(argep, STN_name);

            if (nodepath) {
               found = show_sharetree_path(os, ep, nodepath);
            }
         }

         if ( found != 0 ) {
            DRETURN(1);
         }

         lFreeList(&arglp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-rsstnode node_path[,...]"  show sharetree node */

      if (strcmp("-rsstnode", *spp) == 0) {
         int found = 0;

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
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         lString2List(*spp, &arglp, STN_Type, STN_name, ", ");

         for_each_rw_lv(argep, arglp) {
            const char *nodepath = nullptr;
            nodepath = lGetString(argep, STN_name);
            if (nodepath) {
               /* CS-2800: the result decides the exit code. It used to be discarded,
                * leaving "found" at 0, so the branch below fired on success whenever
                * -rsstnode was the last option and never fired when one followed.
                * Accumulate so that a failure on any of several paths is reported. */
               found |= show_sharetree_path(os, ep, nodepath);
            }
         }

         if (found != 0) {
            DRETURN(1);
         }

         lFreeList(&arglp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-sstree" */

      if (strcmp("-sstree", *spp) == 0) {
         spooling_field *fields = nullptr;

         /* get the sharetree .. */
         what = lWhat("%T(ALL)", STN_Type);
         alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::STN_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
         lFreeWhat(&what);

         sge_parse_return |= show_answer_list(alp);
         if (sge_parse_return) {
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            continue;
         }

         ep = lFirstRW(lp);

         if (ep == nullptr) {
            fprintf(stderr, "%s\n", MSG_OBJ_NOSTREEELEM);
            sge_parse_return = 1;
            lFreeList(&alp);
            lFreeList(&lp);
            spp++;
            continue;
         }

         if (qconf_opt_format == SP_FORM_JSON) {
            /* the share tree is a tree, not a flat object: render it as a nested
             * JSON document (childnodes -> array of full child nodes, recursively) */
            spooling_field *json_fields = sge_build_STN_json_field_list();
            dstring out = DSTRING_INIT;
            spool_json_write_typed_object(&alp, ep, json_fields, "sharetree", &out);
            os << qconf_cstr(sge_dstring_get_string(&out));
            sge_dstring_free(&out);
            sge_free(&json_fields);
         } else {
            fields = sge_build_STN_field_list(false, true);
            filename_stdout = spool_flatfile_write_object(&alp, ep, true, fields,
                                        &qconf_name_value_list_sfi,
                                        SP_DEST_STREAM, qconf_opt_format,
                                        nullptr, false, nullptr, &os);
            sge_free(&fields);
            sge_free(&filename_stdout);
         }
         sge_parse_return |= show_answer_list(alp);
         if (sge_parse_return) {
            sge_error_and_exit(nullptr);
         }

         lFreeList(&alp);
         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-bonsai" This is the undocumented switch of showing the share tree
         we keep this switch  for compatibility reasons */

      if (strcmp("-bonsai", *spp) == 0) {
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

         /* CS-2800: -bonsai is an alias of -sst and has to report a missing share
          * tree the same way. Without this it printed nothing at all. */
         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         }

         show_sharetree(os, ep, nullptr);

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-sst" This is the documented switch of showing the share tree */

      if (strcmp("-sst", *spp) == 0) {
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

         if (!ep) {
            fprintf(stderr, "%s\n", MSG_TREE_NOSHARETREE);
            spp++;
            continue;
         } else {
          show_sharetree(os, ep, nullptr);
         }

         lFreeList(&lp);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ss" */
      if (strcmp("-ss", *spp) == 0) {

         if (!show_reserved_hgroup_hosts(os, SUBMIT_HOSTGROUP, "submithost",
               "submit host")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sss" - show scheduler state */

      if (strcmp("-sss", *spp) == 0) {
         /* ... */
         if (!show_object_list(os, ocs::gdi::Target::EV_LIST, EV_Type, EV_host, "scheduling host")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-su [list_name[,list_name,...]]" */

      if (strcmp("-su", *spp) == 0) {
         spp = sge_parser_get_next(spp);
         parse_name_list_to_cull("acl`s to show", &lp,
               US_Type, US_name, *spp);
         if (print_acl(os, lp) != 0) {
            sge_parse_return = 1;
         }
         lFreeList(&lp);

         spp++;
         continue;
      }

      // -scat id
      if (strcmp("-scat", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         uint64_t id = strtoull(*spp, nullptr, 10);
         if (!ocs::CategoryQconf::show(os, &answer_list, id)) {
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
         if (!centry_show(os, &answer_list, *spp)) {
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
      if (strcmp("-shgrpl", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::HGRP)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

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
         hgroup_show(os, &answer_list, *spp);
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
         hgroup_show_structure(os, &answer_list, *spp, true);
         sge_parse_return |= show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /* "-shgrp_resolved" */
      if (strcmp("-shgrp_resolved", *spp) == 0) {
         lList *answer_list = nullptr;

         spp = sge_parser_get_next(spp);
         hgroup_show_structure(os, &answer_list, *spp, false);
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
         bool is_member = false;

         spp = sge_parser_get_next(spp);
         const char *why_group = *spp;
         spp = sge_parser_get_next(spp);

         if (!hgroup_show_why(os, &answer_list, why_group, *spp, &is_member) || !is_member) {
            sge_parse_return |= 1;
         }
         show_answer(answer_list);
         lFreeList(&answer_list);

         spp++;
         continue;
      }

      /*
       * Cluster Queue parameter
       */

      /* "-sql" */
      if (strcmp("-sql", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::CQ)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

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

         while (!sge_next_is_an_opt(spp)) {
            spp = sge_parser_get_next(spp);
            lString2List(*spp, &arglp, QR_Type, QR_name, ", ");
         }

         cqueue_show(os, &alp, arglp);
         lFreeList(&arglp);
         sge_parse_return |= show_answer(alp);
         lFreeList(&alp);
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

         /* Use cqueue_list_sick()'s return value to set the exit code */
         sge_parse_return |= (cqueue_list_sick(os, &answer_list)?0:1);
         show_answer(answer_list);
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
            (void)qconf_show_object(os, QConfObj::UU, lGetString(uep, ST_name));
         }

         lFreeList(&uList);
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-sprj projectname" */

      if (strcmp("-sprj", *spp) == 0) {
         spp = sge_parser_get_next(spp);

         if (qconf_show_object(os, QConfObj::PR, *spp) != QConfShowStatus::OK) {
            sge_parse_return = 1;
         }

         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-ss" -- unreachable duplicate of the "-ss" branch above (same if-chain,
       * the first match wins); converted with it so no reader has to work out
       * which of the two is live, and so nothing here still names SH_LIST. */
      if (strcmp("-ss", *spp) == 0) {
         if (!show_reserved_hgroup_hosts(os, SUBMIT_HOSTGROUP, "submithost",
               "submit")) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }
/*----------------------------------------------------------------------------*/
      /* "-sul" */

      if (strcmp("-sul", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::US)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

       /* "-suserl" */

      if (strcmp("-suserl", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::UU)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

       /* "-sprjl" */

      if (strcmp("-sprjl", *spp) == 0) {
         if (!qconf_show_names(os, QConfObj::PR)) {
            sge_parse_return = 1;
         }
         spp++;
         continue;
      }

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
 * @param group ADMIN_HOSTGROUP or SUBMIT_HOSTGROUP ocs::gdi::SubCommand sub_command - APPEND to add, REMOVE to delete
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

/**
 * @brief Print the names of every object of one type, the `-s*l` switches
 *
 * Fetches only the key field of the target list and writes one name per line,
 * or a JSON array under `-fmt json`. The template and global pseudo hosts are
 * filtered out for `EH_name`, the global configuration for `CONF_name`, and
 * `EV_host` is narrowed to the scheduler event client.
 *
 * @param os     the stream the names are written to (CS-1841)
 * @param target the master list to read
 * @param type   the CULL type of its elements
 * @param keynm  the field holding the name
 * @param name   the object type's display name, used in the "none defined" message
 * @return true when at least one name was written; false on a GDI error or an
 *         empty list, which is reported on stderr
 */
static bool show_object_list(std::ostream &os, ocs::gdi::Target target, lDescr *type, int keynm, const char *name) {
   DENTER(TOP_LAYER);
   lEnumeration *what = nullptr;
   lCondition *where = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   int pos;
   int dataType;
   bool ret = true;

   what = lWhat("%T(%I)", type, keynm);

   switch (keynm) {
   case EH_name:
      where = lWhere("%T(!(%Ic=%s || %Ic=%s))", type, keynm, SGE_TEMPLATE_NAME, keynm, SGE_GLOBAL_NAME );
      break;
   case CONF_name:
      where = lWhere("%T(!(%I c= %s))", type, keynm, SGE_GLOBAL_NAME );
      break;
   case EV_host:
      where = lWhere("%T(%I==%u))", type, EV_id, EV_ID_SCHEDD);
      break;
   default:
      where = nullptr; /* all elements */
      break;
   }

   alp = ocs::gdi::Client::sge_gdi(target, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   lPSortList(lp, "%I+", keynm);

   lListElem *ep1 = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep1);
   if (answer_list_output(&alp)) {
      lFreeList(&lp);
      DRETURN(false);
   }

   /* CS-2313a: -fmt json renders the name list as a JSON array (in the $schema/$id
    * envelope); ASCII prints one name per line. */
   if (qconf_opt_format == SP_FORM_JSON) {
      dstring out = DSTRING_INIT;
      spool_json_write_name_list(&alp, lp, keynm, &out);
      os << qconf_cstr(sge_dstring_get_string(&out));
      sge_dstring_free(&out);
   } else if (lGetNumberOfElem(lp) > 0) {
      for_each_rw_lv (ep, lp) {
         const char *line = nullptr;
         pos = lGetPosInDescr(type, keynm);
         dataType = lGetPosType(type , pos);
         switch(dataType) {
            case lStringT:
               line = lGetString(ep, keynm);
               break;
            case lHostT:
               line = lGetHost(ep, keynm);
               break;
            default:
               DPRINTF("show_object_list: unexpected data type\n");
               break;
         }
         if (line != nullptr && line[0] != COMMENT_CHAR) {
            os << qconf_cstr(line) << "\n";
         }
      }
   } else {
      fprintf(stderr, MSG_QCONF_NOXDEFINED_S, name);
      fprintf(stderr, "\n");
      ret = false;
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(ret);
}

/**
 * @brief Qconf -sh / -ss from the reserved group
 *
 * CS-2438 chunk 5. Prints the membership of "@admin_hosts"/"@submit_hosts"
 * now that they, and not AH_LIST/SH_LIST, are the source of truth.
 * Deliberately prints the DIRECT entries -- host names and "@group"
 * references -- rather than the resolved set. For a cluster that does not
 * nest, which is every cluster on the day after the upgrade because
 * migration replays a flat list, the output is byte-identical to 9.1.
 * Where an admin did introduce nesting, the direct entries are the honest
 * answer: they are what "-dh/-ds" operate on. The resolved view already has
 * its own command, "qconf -shgrp_resolved @submit_hosts".
 * The three behaviours of show_object_list() that scripts depend on are
 * reproduced exactly, and for the same reasons:
 *   1. output sorted ascending by name, not stored order;
 *   2. entries starting with COMMENT_CHAR skipped;
 *   3. an empty list prints to STDERR and returns false, i.e. non-zero exit.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param group ADMIN_HOSTGROUP or SUBMIT_HOSTGROUP
 * @param json_type envelope name for -fmt json ("adminhost"/"submithost")
 * @param name "administrative host" / "submit host", for the message
 *
 * @return false if nothing is defined or the request failed
 *
 * @note json_type has to be passed in: the entries are HR_Type elements, which are
 *       not a registered object type, so the envelope would derive as "unknown"
 *       and both the $id and the array key would change. It is only used when the
 *       list is non-empty, because the empty list produced the generic "names" key
 *       before and has to keep producing it.
 */
static bool show_reserved_hgroup_hosts(std::ostream &os, const char *group, const char *json_type, const char *name) {
   DENTER(TOP_LAYER);
   lList *alp = nullptr, *lp = nullptr;
   bool ret = true;

   lEnumeration *what = lWhat("%T(%I %I)", HGRP_Type, HGRP_name, HGRP_host_list);
   // CS-2762: a host field is compared with "h=", which folds case as the
   // object layer does; "==" with %s is the string comparison
   lCondition *where = lWhere("%T(%I h= %s)", HGRP_Type, HGRP_name, group);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET,
                                   ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   lListElem *ep1 = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep1);
   if (answer_list_output(&alp)) {
      lFreeList(&lp);
      DRETURN(false);
   }

   /* the group is seeded on every qmaster startup and cannot be deleted, so a
    * missing element means an empty membership, handled below like an empty
    * list -- but it must not reach lGetListRW(), which aborts on a nullptr
    * element rather than returning nothing */
   lListElem *hgroup = lFirstRW(lp);
   lList *entries = (hgroup != nullptr) ? lGetListRW(hgroup, HGRP_host_list) : nullptr;

   if (entries != nullptr) {
      lPSortList(entries, "%I+", HR_name);
   }

   if (qconf_opt_format == SP_FORM_JSON) {
      dstring out = DSTRING_INIT;
      spool_json_write_name_list_ex(&alp, entries, HR_name,
                                    lGetNumberOfElem(entries) > 0 ? json_type : nullptr, &out);
      os << qconf_cstr(sge_dstring_get_string(&out));
      sge_dstring_free(&out);
   } else if (lGetNumberOfElem(entries) > 0) {
      /*
       * CS-2680. Host names and group references first, matchers after them,
       * each block sorted ascending in itself -- the list was sorted as a whole
       * above, so walking it twice preserves that within each block.
       *
       * The separation exists because the output is no longer necessarily a
       * list of host names: a script passing it to a command that expects hosts
       * has to filter the matchers out, and one block boundary reduces that to
       * a single line. For a configuration carrying no matcher the second pass
       * prints nothing and the output is unchanged, character for character.
       */
      for (int pass = 0; pass < 2; pass++) {
         const bool matchers = (pass == 1);
         const lListElem *ep;

         for_each_ep(ep, entries) {
            const char *line = lGetHost(ep, HR_name);

            if (line == nullptr || line[0] == COMMENT_CHAR) {
               continue;
            }
            if (ocs::is_matcher(line) != matchers) {
               continue;
            }
            os << qconf_cstr(line) << "\n";
         }
      }
   } else {
      fprintf(stderr, MSG_QCONF_NOXDEFINED_S, name);
      fprintf(stderr, "\n");
      ret = false;
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(ret);
}

/**
 * @brief Print the managers/operators (qconf -sm/-so)
 *
 * CS-2394: managers and operators are the members of the reserved
 * "manager"/"operator" access lists (usersets). This reads that access list
 * and prints its member names, reproducing the historical -sm/-so output: a
 * sorted plain name list (one per line), or, with -fmt json, the name-list
 * JSON envelope. Because the names now come from the userset entries
 * (UE_Type), the JSON envelope identity reflects the access-list-entry type
 * rather than the retired "manager"/"operator" object type - the plain text
 * output is unchanged.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param userset_name the reserved userset ("manager" or "operator")
 * @param display_name name used in the "no X defined" message
 *
 * @return true on success, false on error or when the list is empty
 */
static bool show_manop_list(std::ostream &os, const char *userset_name, const char *display_name) {
   DENTER(TOP_LAYER);
   lList *alp = nullptr, *lp = nullptr;
   bool ret = true;

   /* No where-filter: the userset is selected by name below, so filtering in the
    * qmaster would only save bandwidth on an interactive admin command over a
    * list the enumeration has already reduced to names. RBAC will move this work
    * into the qmaster anyway - the request is to be executed there and only the
    * reduced output transferred - so the name lookup is the part that survives. */
   lEnumeration *what = lWhat("%T(%I%I->(%I))", US_Type, US_name, US_entries, UE_name);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST, ocs::gdi::Command::GET,
                                   ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
   lFreeWhat(&what);

   lListElem *ep1 = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep1);
   if (answer_list_output(&alp)) {
      lFreeList(&lp);
      DRETURN(false);
   }

   /* the reserved userset always exists; print its members (UE_name entries).
    * The GET returns every userset, so select ours by name - never lFirst(),
    * which would print the manager list for -so. */
   lListElem *us = lGetElemStrRW(lp, US_name, userset_name);
   lList *entries = (us != nullptr) ? lGetListRW(us, US_entries) : nullptr;
   lPSortList(entries, "%I+", UE_name);

   if (qconf_opt_format == SP_FORM_JSON) {
      dstring out = DSTRING_INIT;
      /* Name the envelope explicitly: the entries are UE_Type, which is not a
       * registered object type, so deriving it would yield "unknown" for both -sm
       * and -so. display_name keeps the pre-CS-2431 "manager"/"operator" envelope
       * and its ocs-qconf-<name>-list.schema.json id. */
      spool_json_write_name_list_ex(&alp, entries, UE_name, display_name, &out);
      os << qconf_cstr(sge_dstring_get_string(&out));
      sge_dstring_free(&out);
   } else if (entries != nullptr && lGetNumberOfElem(entries) > 0) {
      for_each_ep_lv (ep, entries) {
         const char *line = lGetString(ep, UE_name);
         if (line != nullptr && line[0] != COMMENT_CHAR) {
            os << qconf_cstr(line) << "\n";
         }
      }
   } else {
      fprintf(stderr, MSG_QCONF_NOXDEFINED_S, display_name);
      fprintf(stderr, "\n");
      ret = false;
   }

   lFreeList(&alp);
   lFreeList(&lp);
   DRETURN(ret);
}

/**
 * @brief Print the qmaster thread pools and their sizes, the `-sts` switch
 *
 * @param os the stream the table is written to (CS-1841)
 * @return 0 on success, 1 when qmaster answered with an error
 */
static int
show_thread_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   // send the request
   lList *lp = nullptr;
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::DUMMY_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);

   // check the answer
   const lListElem *ep1 = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep1);
   if (answer_get_status(ep1) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep1, AN_text));
      fprintf(stderr, "\n");
      DRETURN(-1);
   }

   if (qconf_opt_format == SP_FORM_JSON) {
      /* CS-2313a: render the thread pools as a JSON array of { pool, size } objects */
      static spooling_field st_fields[] = {
         { ST_name, 0, "pool", false, nullptr, false, nullptr, nullptr, nullptr },
         { ST_id,   0, "size", false, nullptr, false, nullptr, nullptr, nullptr },
         { NoName,  0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr }
      };
      dstring out = DSTRING_INIT;
      spool_json_write_typed_list(&alp, lp, st_fields, "thread_pools", &out);
      os << qconf_cstr(sge_dstring_get_string(&out));
      sge_dstring_free(&out);
   } else if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
      os << std::format("{:<15} {}\n", MSG_TABLE_EV_POOL, MSG_TABLE_SIZE);
      os << "--------------------\n";
      for_each_ep_lv(ep, lp) {
         os << std::format("{:<15} {}\n", qconf_cstr(lGetString(ep, ST_name)), lGetUlong(ep, ST_id));
      }
   }
   lFreeList(&alp);
   lFreeList(&lp);


   DRETURN(0);
}

/**
 * @brief Print the registered event clients, the `-secl` switch
 *
 * One line per client with its id, name and host, or a JSON array under
 * `-fmt json`. An empty list is reported on stderr and is not an error.
 *
 * @param os the stream the table is written to (CS-1841)
 * @return 0 on success, 1 when qmaster answered with an error
 */
static int show_eventclients(std::ostream &os) {
   DENTER(TOP_LAYER);
   lEnumeration *what = nullptr;
   lList *alp = nullptr, *lp = nullptr;

   what = lWhat("%T(%I %I %I)", EV_Type, EV_id, EV_name, EV_host);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EV_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
   lFreeWhat(&what);

   lListElem *ep1 = lFirstRW(alp);
   answer_exit_if_not_recoverable(ep1);
   if (answer_get_status(ep1) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep1, AN_text));
      fprintf(stderr, "\n");
      DRETURN(-1);
   }

   if (lp != nullptr) {
      lPSortList(lp, "%I+", EV_id);
   }

   if (qconf_opt_format == SP_FORM_JSON) {
      /* CS-2313a: render the event clients as a JSON array of { id, name, host }
       * objects (an empty list still yields the enveloped empty array) */
      static spooling_field ev_fields[] = {
         { EV_id,   0, "id",   false, nullptr, false, nullptr, nullptr, nullptr },
         { EV_name, 0, "name", false, nullptr, false, nullptr, nullptr, nullptr },
         { EV_host, 0, "host", false, nullptr, false, nullptr, nullptr, nullptr },
         { NoName,  0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr }
      };
      dstring out = DSTRING_INIT;
      spool_json_write_typed_list(&alp, lp, ev_fields, "event_clients", &out);
      os << qconf_cstr(sge_dstring_get_string(&out));
      sge_dstring_free(&out);
   } else if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
      os << std::format("{:>8} {:<15} {:<25}\n", MSG_TABLE_EV_ID, MSG_TABLE_EV_NAME, MSG_TABLE_HOST);
      os << "--------------------------------------------------\n";
      for_each_ep_lv(ep, lp) {
         os << std::format("{:>8} ", static_cast<int>(lGetUlong(ep, EV_id)));
         os << std::format("{:<15} ", qconf_cstr(lGetString(ep, EV_name)));
         os << std::format("{:<25}\n", (lGetHost(ep, EV_host) != nullptr) ? lGetHost(ep, EV_host) : "-");
      }
   }
   else {
      fprintf(stderr, "%s\n", MSG_QCONF_NOEVENTCLIENTSREGISTERED);
   }
   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(0);
}


/**
 * @brief Print the processor counts of every execution host, the `-sep` switch
 *
 * One line per host with its processor count and architecture, followed by a
 * total. With @a has_binding_param the socket and core columns are shown as
 * well. An empty execution host list is reported on stderr.
 *
 * @param os                the stream the table is written to (CS-1841)
 * @param has_binding_param also show the socket and core columns
 * @return 0 on success, -1 when qmaster answered with an error
 */
static int show_processors(std::ostream &os, bool has_binding_param) {
   DENTER(TOP_LAYER);

   lEnumeration *what = nullptr;
   lCondition *where = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   const lListElem *ep = nullptr;
   const char *cp = nullptr;
   uint32_t sum = 0;
   uint32_t socket_sum = 0;
   uint32_t core_sum = 0;

   what = lWhat("%T(%I %I %I)", EH_Type, EH_name, EH_processors, EH_load_list);
   where = lWhere("%T(!(%Ic=%s || %Ic=%s))", EH_Type, EH_name,
                  SGE_TEMPLATE_NAME, EH_name, SGE_GLOBAL_NAME);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   ep = lFirst(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      DRETURN(-1);
   }

   if (lp != nullptr && lGetNumberOfElem(lp) > 0) {
      lPSortList(lp,"%I+", EH_name);

      if (has_binding_param) {
         os << std::format("{:<25.24}{:>10.9}{:>6.5}{:>6.5}{:>12.11}\n", MSG_TABLE_HOST, MSG_TABLE_PROCESSORS,
            MSG_TABLE_SOCKETS, MSG_TABLE_CORES, MSG_TABLE_ARCH);
      } else {
         os << std::format("{:<25.24}{:>10.9}{:>12.11}\n", MSG_TABLE_HOST, MSG_TABLE_PROCESSORS, MSG_TABLE_ARCH);
      }
      os << "===============================================";
      if (has_binding_param) {
         os << "============";
      }
      os << "\n";
      for_each_ep(ep, lp) {
         const lListElem *arch_elem = lGetSubStr(ep, HL_name, "arch", EH_load_list);
         uint32_t sockets = 0;
         uint32_t cores = 0;

         os << std::format("{:<25.24}", ((cp = lGetHost(ep, EH_name)) ? cp : ""));
         os << std::format("{:>10}", static_cast<int32_t>(lGetUlong(ep, EH_processors)));

         if (has_binding_param) {
            const lListElem *socket_elem = lGetSubStr(ep, HL_name, "m_socket", EH_load_list);
            const lListElem *core_elem = lGetSubStr(ep, HL_name, "m_core", EH_load_list);

            if (socket_elem != nullptr) {
               os << std::format("{:>6.5}", qconf_cstr(lGetString(socket_elem, HL_value)));
               sockets = atol(lGetString(socket_elem, HL_value));
            } else {
               os << std::format("{:>6.5}", "-");
            }
            if (core_elem != nullptr) {
               os << std::format("{:>6.5}", qconf_cstr(lGetString(core_elem, HL_value)));
               cores = atol(lGetString(core_elem, HL_value));
            } else {
               os << std::format("{:>6.5}", "-");
            }
         }
         if (arch_elem) {
            os << std::format("{:>12.11}", qconf_cstr(lGetString(arch_elem, HL_value)));
         }
         os << "\n";
         sum += lGetUlong(ep, EH_processors);
         socket_sum += sockets;
         core_sum += cores;
      }
      os << "===============================================";
      if (has_binding_param) {
         os << "============";
      }
      os << "\n";

      os << std::format("{:<25.24}{:>10}", MSG_TABLE_SUM_F, static_cast<int32_t>(sum));
      if (has_binding_param) {
         if (socket_sum > 0) {
            os << std::format("{:>6}", static_cast<int32_t>(socket_sum));
         } else {
            os << std::format("{:>6.5}", "-");
         }
         if (core_sum > 0) {
            os << std::format("{:>6}", static_cast<int32_t>(core_sum));
         } else {
            os << std::format("{:>6.5}", "-");
         }
      }
      os << "\n";
   } else {
      fprintf(stderr,  "%s\n", MSG_QCONF_NOEXECUTIONHOSTSDEFINED);
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(0);
}

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   get all acls listed in acl_arg
   from qmaster and print them

   returns
      0 on success
      1 if arglp contains acl names not present at qmaster
      -1 failed to get any acl
*/
static int print_acl(std::ostream &os, lList *arglp) {
   DENTER(TOP_LAYER);
   lList *acls = nullptr;
   const lListElem *ep = nullptr;
   int fail = 0;
   const char *acl_name = nullptr;
   int first_time = 1;
   const char *filename_stdout;

   /* get all acls named in arglp, put them into acls */
   if (sge_client_get_acls(nullptr, arglp, &acls)) {
      DRETURN(-1);
   }

   for_each_ep_lv(argep, arglp) {
      acl_name = lGetString(argep, US_name);

      ep = lGetElemStrRW(acls, US_name, acl_name);
      if (ep == nullptr) {
         fprintf(stderr, MSG_SGETEXT_DOESNOTEXIST_SS, "access list", acl_name);
         fprintf(stderr, "\n");
         fail = 1;
      } else {
         lList *alp = nullptr;

         if (first_time)
            first_time = 0;
         else {
            os << "\n";
         }

         filename_stdout = spool_flatfile_write_object(&alp, ep, false, US_fields, &qconf_param_sfi,
                                     SP_DEST_STREAM, qconf_opt_format, nullptr,
                                     false, nullptr, &os);
         lFreeList(&alp);
         sge_free(&filename_stdout);
      }
   }
   lFreeList(&acls);
   DRETURN(fail);
}

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

/***************************************************************************
  -sconf option
 * @param os the stream the output is written to (CS-1841)
 ***************************************************************************/
static int print_config(std::ostream &os, const char *config_name) {
   DENTER(TOP_LAYER);

   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   lList *alp = nullptr, *lp = nullptr;
   const lListElem *ep = nullptr;
   int fail=0;
   const char *cfn = nullptr;
   spooling_field *fields = nullptr;

   /* get config */
   if (!strcasecmp(config_name, "global")) {
      cfn = SGE_GLOBAL_NAME;
   } else {
      cfn = config_name;
   }

   where = lWhere("%T(%Ih=%s)", CONF_Type, CONF_name, cfn);
   what = lWhat("%T(ALL)", CONF_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CONF_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   ep = lFirst(alp);
   answer_exit_if_not_recoverable(ep);
   if (answer_get_status(ep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(ep, AN_text));
      fail = 1;
   } else {
      const char *filename_stdout;

      if (!(ep = lFirst(lp))) {
         fprintf(stderr, MSG_ANSWER_CONFIGXNOTDEFINED_S, cfn);
         fprintf(stderr, "\n");
         lFreeList(&alp);
         lFreeList(&lp);
         DRETURN(1);
      }
      /* CS-2313a: the "#name:" header is an ASCII-format comment; omit it for JSON */
      if (qconf_opt_format != SP_FORM_JSON) {
         os << "#" << qconf_cstr(cfn) << ":\n";
      }

      fields = sge_build_CONF_field_list(false);
      filename_stdout = spool_flatfile_write_object(&alp, ep, false, fields, &qconf_sfi,
                                  SP_DEST_STREAM, qconf_opt_format, nullptr, false, nullptr, &os);
      sge_free(&fields);
      sge_free(&filename_stdout);

      if (answer_list_output(&alp)) {
         sge_error_and_exit(nullptr);
      }
   }

   lFreeList(&alp);
   lFreeList(&lp);

   DRETURN(fail);
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
