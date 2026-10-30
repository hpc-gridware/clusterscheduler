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
 * @brief What every switch family of `qconf` shares, see the header
 */

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <dirent.h>
#include <ostream>
#include <string>
#include <vector>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "spool/flatfile/sge_flatfile.h"
#include "sge.h"
#include "usage.h"
#include "gdi/ocs_gdi_Client.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_common.h"
#include "ocs_qconf_parse.h"

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
bool qconf_opt_dry_run = false;   /* H3 (-dry):    validate/report, do not send */

bool qconf_opt_force = false;     /* H6 (-f):      skip the bulk-delete prompt */

bool qconf_opt_strict = false;    /* H2 (-strict): apply nothing unless all files valid */

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

void
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

bool
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

bool
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
 * @param prepare  optional transform run on the object after the read (e.g.
 *                 resolve a hostname), or nullptr - CS-2304
 * @param parse_values the parse_values flag of spool_flatfile_read_object()
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

int
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

int
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

int
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

bool
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

/* ===== CS-2311: cluster/host configuration helpers ========================
 * The config object keys on the host name, which is the file *basename* (not a
 * field inside the file), and -Aconf/-Mconf already accept a comma-separated
 * file list. So config gets its own file/dir helpers (like RQS) rather than the
 * field-name-based qconf_delete_path. What is left of them is
 * the path handling: since CS-2823 the writing itself is the procedure's, and
 * the basename travels with the request as the object's name, because a
 * configuration's text does not carry it. delete_configs() is forward-declared
 * at the top of this file. */

const char *
qconf_conf_host_of(const char *filepath, lListElem *host_ep) {
   const char *slash = strrchr(filepath, '/');
   const char *base = slash != nullptr ? slash + 1 : filepath;
   lSetHost(host_ep, EH_name, base);
   sge_resolve_host(host_ep, EH_name);   /* best-effort, like -Aconf/-dconf */
   return lGetHost(host_ep, EH_name);
}

bool
qconf_missing_argument(const qconf_op &op) {
   if (op.spec == nullptr || op.args.size() >= static_cast<size_t>(op.spec->min_args)) {
      return false;
   }
   ERROR(MSG_QCONF_NOOPTIONARGPROVIDEDTOX_S, op.args.empty() ? op.token.c_str() : op.args.back().c_str());
   sge_usage(QCONF, stderr);
   return true;
}

void
qconf_client_check(QConfClientCheck check, const char *username, const char *hostname) {
   switch (check) {
      case QConfClientCheck::NONE:
         break;
      case QConfClientCheck::MANAGER:
         qconf_is_manager(username);
         break;
      case QConfClientCheck::MANAGER_ON_ADMIN_HOST:
         qconf_is_manager_on_admin_host(username, hostname);
         break;
      case QConfClientCheck::ADMIN_HOST_THEN_MANAGER:
         qconf_is_adminhost(hostname);
         qconf_is_manager(username);
         break;
   }
}

/** @brief Resolve one host name the way the exec host switches always did
 * @param name the name as typed
 * @param resolved receives the resolved name
 * @return true when it resolved; false after the message
 */
static bool
qconf_resolve_one_host(const char *name, std::string &resolved) {
   lListElem *host_ep = lCreateElem(EH_Type);
   lSetHost(host_ep, EH_name, name);
   const bool ok = (sge_resolve_host(host_ep, EH_name) == CL_RETVAL_OK);
   if (ok) {
      resolved = lGetHost(host_ep, EH_name);
   } else {
      fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(host_ep, EH_name));
      fprintf(stderr, "\n");
   }
   lFreeElem(&host_ep);
   return ok;
}

void
qconf_names_ae(const qconf_op &op, std::vector<std::string> &names, bool &failed) {
   if (op.args.empty()) {
      names.emplace_back(SGE_TEMPLATE_NAME);
      return;
   }
   std::string resolved;
   if (qconf_resolve_one_host(op.args[0].c_str(), resolved)) {
      names.push_back(resolved);
   } else {
      failed = true;
   }
}

void
qconf_names_one_host(const qconf_op &op, std::vector<std::string> &names, bool &failed) {
   std::string resolved;
   if (qconf_resolve_one_host(op.args[0].c_str(), resolved)) {
      names.push_back(resolved);
   } else {
      failed = true;
   }
}

void
qconf_names_host_list(const qconf_op &op, std::vector<std::string> &names, bool &failed) {
   std::string arg = op.args[0];
   lList *hosts = nullptr;
   parse_name_list_to_cull("hosts to change", &hosts, EH_Type, EH_name, arg.data());
   for_each_rw_lv(host_ep, hosts) {
      if (sge_resolve_host(host_ep, EH_name) != CL_RETVAL_OK) {
         fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetHost(host_ep, EH_name));
         fprintf(stderr, "\n");
         failed = true;
         continue;
      }
      names.emplace_back(lGetHost(host_ep, EH_name));
   }
   lFreeList(&hosts);
}

void
qconf_names_userset_list(const qconf_op &op, std::vector<std::string> &names, [[maybe_unused]] bool &failed) {
   std::string arg = op.args[0];
   lList *usersets = nullptr;
   parse_name_list_to_cull("usersets", &usersets, US_Type, US_name, arg.data());
   const lListElem *us_ep;
   for_each_ep(us_ep, usersets) {
      const char *name = lGetString(us_ep, US_name);
      names.emplace_back(name != nullptr ? name : "");
   }
   lFreeList(&usersets);
}

void
qconf_names_user_list(const qconf_op &op, std::vector<std::string> &names, [[maybe_unused]] bool &failed) {
   lList *users = nullptr;
   lString2List(op.args[0].c_str(), &users, ST_Type, ST_name, ", ");
   const lListElem *user_ep;
   for_each_ep(user_ep, users) {
      names.emplace_back(lGetString(user_ep, ST_name));
   }
   lFreeList(&users);
}

/** @brief The host list of the configuration switches, each host resolved (CS-2967)
 *
 * "host1,host2,..." as sge_strtok() takes it apart - up to the first empty
 * entry -, "global" when no list is given, and the alias "global" in any case
 * spelled as the global configuration's name. A host that does not resolve is
 * reported with the reason; it is left out unless @p keep_unresolved, which is
 * what "-mconf" always did - it edits the configuration under the name as typed.
 *
 * @param op the operation
 * @param names receives the configuration names
 * @param failed set when a host was left out
 * @param keep_unresolved serve a host that does not resolve under its name as typed
 */
static void
qconf_names_conf_hosts(const qconf_op &op, std::vector<std::string> &names, bool &failed,
                       const bool keep_unresolved) {
   std::string list = op.args.empty() ? SGE_GLOBAL_NAME : op.args[0];
   lListElem *host_ep = lCreateElem(EH_Type);
   for (const char *cp = sge_strtok(list.data(), ","); cp != nullptr && *cp != '\0';
        cp = sge_strtok(nullptr, ",")) {
      lSetHost(host_ep, EH_name, cp);
      const int ret = sge_resolve_host(host_ep, EH_name);
      if (ret != CL_RETVAL_OK) {
         fprintf(stderr, MSG_SGETEXT_CANTRESOLVEHOST_SS, cp, cl_get_error_text(ret));
         fprintf(stderr, "\n");
         if (!keep_unresolved) {
            failed = true;
            continue;
         }
      }
      const char *hostname = lGetHost(host_ep, EH_name);
      names.emplace_back((strcasecmp(hostname, "global") == 0) ? SGE_GLOBAL_NAME : hostname);
   }
   lFreeElem(&host_ep);
}

void
qconf_names_conf_strict(const qconf_op &op, std::vector<std::string> &names, bool &failed) {
   qconf_names_conf_hosts(op, names, failed, false);
}

void
qconf_names_conf_lenient(const qconf_op &op, std::vector<std::string> &names, bool &failed) {
   qconf_names_conf_hosts(op, names, failed, true);
}

void
qconf_names_conf_paths(const qconf_op &op, std::vector<std::string> &names, [[maybe_unused]] bool &failed) {
   std::string list = op.args.empty() ? SGE_GLOBAL_NAME : op.args[0];
   for (const char *cp = sge_strtok(list.data(), ","); cp != nullptr && *cp != '\0';
        cp = sge_strtok(nullptr, ",")) {
      names.emplace_back(cp);
   }
}
