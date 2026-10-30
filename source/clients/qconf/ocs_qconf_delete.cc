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
 * @brief The delete switches of `qconf`, see the header
 */

#include <ostream>
#include <string>

#include "uti/sge_log.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "gdi/ocs_gdi_Client.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_delete.h"
#include "ocs_qconf_common.h"
#include "ocs_qconf_parse.h"

/**
 * @brief Delete every object named in the file(s) at a path (file or directory) (CS-2298 C5).
 *
 * The name attribute is read from each file and the rest of the content is
 * ignored. A directory expands to a bulk delete: it is confirmed interactively
 * unless -f is given (CS-2299 H6), can be previewed with -dry (CS-2299 H3), and a
 * name that no longer exists is skipped rather than failed (CS-2299 H5).
 *
 * @param os the stream the output is written to (CS-1841)
 * @param d       everything that identifies the object type: GDI target, CULL
 *                descriptor, name attribute, flatfile instruction and the
 *                transform run on each object after the read (CS-2795)
 * @param fields  spooling field list describing the file format
 * @param path    file or directory naming the objects to delete
 * @param alpp    the answer list of the operation
 * @return the number of objects that failed to be deleted
 */
static int
qconf_delete_path(std::ostream &os, const qconf_object_desc &d, spooling_field *fields, const char *path,
                  lList **alpp) {
   const lDescr *descr = d.descr;
   const int name_nm = d.name_nm;
   qconf_file_ctx ctx = {&os, &d, fields, d.parse_values,
                         lCreateList("qconf del", descr), nullptr, 0, 0, alpp};
   const bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_collect_name, &ctx, alpp);

   if (is_dir) {
      /* CS-2299 H1: report the read phase before the batch delete. */
      qconf_info(os, MSG_QCONF_DELCOLLECTSUMMARY_SII, path, ctx.n_ok, ctx.n_fail);
   }

   const int n_names = lGetNumberOfElem(ctx.names);
   if (n_names > 0 && qconf_opt_dry_run) {
      /* CS-2299 H3: list what would be deleted, contact no one. */
      for_each_ep_lv(nep, ctx.names) {
         qconf_info(os, MSG_QCONF_DRYRUNWOULDDELETE_S, qconf_get_name(nep, name_nm));
      }
      lFreeList(&ctx.names);
      return ctx.n_fail;
   }
   if (n_names > 0 && is_dir && !qconf_opt_force) {
      /* CS-2299 H6: a directory expands to a bulk delete - confirm first. */
      const bool ok = qconf_confirm(qconf_format(MSG_QCONF_CONFIRMDELETE_I, n_names).c_str(), alpp);
      if (!ok) {
         qconf_answer(alpp, ANSWER_QUALITY_WARNING, MSG_QCONF_DELETEABORTED_I, n_names);
         lFreeList(&ctx.names);
         return ctx.n_fail;
      }
   }
   if (n_names > 0) {
      lList *alp = ocs::gdi::Client::sge_gdi(d.target, ocs::gdi::Command::DEL,
                                             ocs::gdi::SubCommand::NONE, &ctx.names,
                                             nullptr, nullptr);
      ctx.n_fail += qconf_answer_take(alpp, &alp);
   }
   lFreeList(&ctx.names);
   return ctx.n_fail;
}

/** @brief Delete the objects named by an already parsed list.
 *
 * Split out of qconf_delete_names() because -dhgrp parses its argument with
 * parse_name_list_to_cull() rather than lString2List(), which reports an empty
 * argument instead of accepting it.
 *
 * @param o   the object type
 * @param lpp the name list; consumed, set to nullptr
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 when the master reported an error
 */
static int
qconf_delete_list(const QConfObj o, lList **lpp, lList **alpp) {
   DENTER(TOP_LAYER);
   const qconf_object_desc &d = qconf_desc(o);

   lList *alp = ocs::gdi::Client::sge_gdi(d.target, ocs::gdi::Command::DEL, ocs::gdi::SubCommand::NONE,
                                          lpp, nullptr, nullptr);
   const int ret = qconf_answer_take(alpp, &alp);
   lFreeList(lpp);
   DRETURN(ret);
}

/** @brief Delete objects of the given type by name.
 *
 * @param o     the object type
 * @param names comma or blank separated list of names; ignored for a type
 *              without a name field, which holds exactly one object
 * @param alpp  the answer list of the operation
 * @return 0 on success, 1 when the master reported an error
 */
static int
qconf_delete_names(const QConfObj o, const char *names, lList **alpp) {
   const qconf_object_desc &d = qconf_desc(o);

   // a type without a name field is deleted as a whole, so there is nothing to name
   lList *lp = nullptr;
   if (d.name_nm != NoName) {
      lString2List(names, &lp, const_cast<lDescr *>(d.descr), d.name_nm, ", ");
   }
   return qconf_delete_list(o, &lp, alpp);
}

/** @brief Delete the objects named by a file or directory, driven by the descriptor.
 * @param os the stream the output is written to (CS-1841)
 * @param o    the object type
 * @param path file or directory to read
 * @param alpp the answer list of the operation
 * @return 0 on success, non-zero on error
 */
static int
qconf_delete_obj(std::ostream &os, const QConfObj o, const char *path, lList **alpp) {
   const qconf_object_desc &d = qconf_desc(o);
   bool owned = false;
   /* the write field list, which a diagnostic attribute has no business in: the file is
    * read to learn the names of the objects to delete */
   spooling_field *fields = d.build_fields(qconf_opt_format, false, &owned);
   const int ret = qconf_delete_path(os, d, fields, path, alpp);
   if (owned) {
      sge_free(&fields);
   }
   return ret;
}

/* ===== CS-2307: resource quota set helpers =================================
 * RQS files hold a *list* of rule sets (read with spool_flatfile_read_list and
 * applied with SET_ALL), so they get their own helpers rather than the
 * single-object qconf_delete_path above. The directory case
 * reuses qconf_for_each_file and emits the same catalogue messages/flags. */

/** @brief Context for qconf_rqs_collect: the merged RQS list + read-failure count. */
struct qconf_rqs_ctx {
   std::ostream *os;   ///< where the per-file messages go (CS-1841)
   lList *all;   ///< merged RQS_Type list collected from every file
   int n_fail;   ///< files that failed to read
   lList **alpp; ///< the answer list of the operation (CS-2971)
};

/** @brief qconf_for_each_file callback: read one file's resource quota sets and
 *  move them into the merged list (CS-2307)
 *
 * @param filepath the file to read
 * @param vctx     the qconf_rqs_ctx of the batch
 * @return 0 when the file was read, 1 otherwise
 */
static int
qconf_rqs_collect(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_rqs_ctx *>(vctx);
   lList *alp = nullptr;
   lList *l = spool_flatfile_read_list(&alp, RQS_Type, RQS_fields, nullptr, true,
                                       &qconf_rqs_sfi, qconf_opt_format, nullptr, filepath);
   if (qconf_answer_take(ctx->alpp, &alp) != 0 || l == nullptr) {
      lFreeList(&l);
      ctx->n_fail++;
      return 1;
   }
   for (lListElem *e = lFirstRW(l); e != nullptr; e = lFirstRW(l)) {
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
 * @param alpp the answer list of the operation
 * @return 0 on success, non-zero on failure
 */
static int
qconf_rqs_delete(std::ostream &os, const char *path, lList **alpp) {
   qconf_rqs_ctx ctx = {&os, lCreateList("rqs", RQS_Type), 0, alpp};
   const bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_rqs_collect, &ctx, alpp);
   int ret = ctx.n_fail;

   /* keep only the names that still exist (idempotent delete) */
   lList *del = lCreateList("rqs del", RQS_Type);
   for_each_ep_lv(r, ctx.all) {
      const char *nm = lGetString(r, RQS_name);
      if (qconf_object_exists(ocs::gdi::Target::RQS_LIST, RQS_Type, RQS_name, nm)) {
         lAddElemStr(&del, RQS_name, nm, RQS_Type);
      } else {
         qconf_answer(alpp, ANSWER_QUALITY_WARNING, MSG_QCONF_DELSKIPPEDNOTEXIST_SS, nm, path);
      }
   }
   lFreeList(&ctx.all);

   const int n = lGetNumberOfElem(del);
   if (n > 0 && qconf_opt_dry_run) {
      for_each_ep_lv(r, del) {
         qconf_info(os, MSG_QCONF_DRYRUNWOULDDELETE_S, lGetString(r, RQS_name));
      }
      lFreeList(&del);
      return ret;
   }
   if (n > 0 && is_dir && !qconf_opt_force) {
      const bool ok = qconf_confirm(qconf_format(MSG_QCONF_CONFIRMDELETE_I, n).c_str(), alpp);
      if (!ok) {
         qconf_answer(alpp, ANSWER_QUALITY_WARNING, MSG_QCONF_DELETEABORTED_I, n);
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
      ret += qconf_answer_take(alpp, &alp);
   }
   lFreeList(&del);
   return ret;
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
      qconf_answer(ctx->alpp, ANSWER_QUALITY_WARNING, MSG_QCONF_DELSKIPPEDNOTEXIST_SS, hostname, filepath);
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
 * @param alpp  the answer list of the operation
 * @return the number of configs that failed to be deleted
 */
static int
qconf_conf_delete_path(std::ostream &os, const char *path, lList **alpp) {
   qconf_conf_ctx ctx = {&os, lCreateList("conf del", CONF_Type), 0, 0, alpp};
   const bool is_dir = sge_is_directory(path);
   qconf_for_each_file(path, qconf_conf_collect_host, &ctx, alpp);

   const int n = lGetNumberOfElem(ctx.hosts);
   if (n > 0 && qconf_opt_dry_run) {
      for_each_ep_lv(e, ctx.hosts) {
         qconf_info(os, MSG_QCONF_DRYRUNWOULDDELETE_S, lGetHost(e, CONF_name));
      }
      lFreeList(&ctx.hosts);
      return 0;
   }
   if (n > 0 && is_dir && !qconf_opt_force) {
      const bool ok = qconf_confirm(qconf_format(MSG_QCONF_CONFIRMDELETE_I, n).c_str(), alpp);
      if (!ok) {
         qconf_answer(alpp, ANSWER_QUALITY_WARNING, MSG_QCONF_DELETEABORTED_I, n);
         lFreeList(&ctx.hosts);
         return 0;
      }
   }
   const int ret = delete_configs(ctx.hosts, alpp);
   lFreeList(&ctx.hosts);
   return ret;
}

/** @brief How a table driven delete switch names what it deletes */
enum class QConfDeleteKind {
   NAMES,   ///< "-d<obj> name,...": qconf_delete_names()
   FILE     ///< "-D<obj> file|dir": qconf_delete_obj()
};

/** @brief One delete switch served by qconf_run_delete() */
struct qconf_delete_entry {
   const char *switch_name;   ///< the switch as typed
   QConfObj object;           ///< the object type it deletes
   QConfDeleteKind kind;      ///< names or file
   int (*run)(std::ostream &os, const qconf_op &op, lList **alpp); ///< a delete of its own instead of the kind's engine, or nullptr
};

/** @brief "-dhgrp group,...": the host groups of the list, as one delete request (CS-2306 C4)
 *
 * The list is taken apart by parse_name_list_to_cull(), as it always was.
 *
 * @param os unused
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_delete_list()
 */
static int
qconf_delete_hgroup_list([[maybe_unused]] std::ostream &os, const qconf_op &op, lList **alpp) {
   std::string arg = op.args[0];
   lList *groups = nullptr;
   parse_name_list_to_cull("host groups to del", &groups, HGRP_Type, HGRP_name, arg.data());
   return qconf_delete_list(QConfObj::HGRP, &groups, alpp);
}

/** @brief "-dconf host,...": the configurations of the hosts of the list (CS-2967)
 *
 * A host that does not resolve is reported and its name deleted as typed, and
 * no list at all is refused with the message it always had.
 *
 * @param os unused
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return 0 when every configuration was deleted, 1 otherwise
 */
static int
qconf_delete_conf_hosts([[maybe_unused]] std::ostream &os, const qconf_op &op, lList **alpp) {
   if (op.args.empty()) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, "%s", MSG_ANSWER_NEEDHOSTNAMETODELLOCALCONFIG);
      return 1;
   }
   int ret = 0;
   std::string list = op.args[0];
   lListElem *host_ep = lCreateElem(EH_Type);
   lList *conf_list = nullptr;
   for (const char *cp = sge_strtok(list.data(), ","); cp != nullptr && *cp != '\0';
        cp = sge_strtok(nullptr, ",")) {
      lSetHost(host_ep, EH_name, cp);
      if (sge_resolve_host(host_ep, EH_name) != CL_RETVAL_OK) {
         qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_SGETEXT_CANTRESOLVEHOST_S, cp);
         ret = 1;
      }
      lAddElemHost(&conf_list, CONF_name, lGetHost(host_ep, EH_name), CONF_Type);
   }
   if (delete_configs(conf_list, alpp) != 0) {
      ret = 1;
   }
   lFreeList(&conf_list);
   lFreeElem(&host_ep);
   return ret;
}

/** @brief "-Dconf file|dir": the configurations the files are named after
 * @param os the stream the output is written to
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_conf_delete_path()
 */
static int
qconf_delete_conf_path(std::ostream &os, const qconf_op &op, lList **alpp) {
   return qconf_conf_delete_path(os, op.args[0].c_str(), alpp);
}

/** @brief "-Drqs file|dir": the resource quota sets a file names, through the list reader of their type
 * @param os the stream the output is written to
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_rqs_delete()
 */
static int
qconf_delete_rqs_path(std::ostream &os, const qconf_op &op, lList **alpp) {
   return qconf_rqs_delete(os, op.args[0].c_str(), alpp);
}

/** @brief The delete switches that are one engine call each (CS-2952)
 *
 * Every "-d<obj>" and "-D<obj>" whose block did nothing but call
 * qconf_delete_names() or qconf_delete_obj(); the permission is qmaster's (CS-2973).
 * "-dhgrp", "-Drqs", "-dconf" and "-Dconf" are rows with a delete of their own
 * (CS-2967). The ones with handling of their own stay blocks: "-de" and
 * "-dh"/"-ds"; "-du"/"-dm"/"-do" and "-dstnode" are actions, see
 * qconf_action_table.
 */
static constexpr qconf_delete_entry qconf_delete_table[] = {
   {"-dcal",   QConfObj::CAL,  QConfDeleteKind::NAMES},
   {"-dckpt",  QConfObj::CK,   QConfDeleteKind::NAMES},
   {"-drole",  QConfObj::RL,   QConfDeleteKind::NAMES},
   {"-drqs",   QConfObj::RQS,  QConfDeleteKind::NAMES},
   {"-dp",     QConfObj::PE,   QConfDeleteKind::NAMES},
   {"-dul",    QConfObj::US,   QConfDeleteKind::NAMES},
   {"-duser",  QConfObj::UU,   QConfDeleteKind::NAMES},
   {"-dprj",   QConfObj::PR,   QConfDeleteKind::NAMES},
   {"-dstree", QConfObj::STN,  QConfDeleteKind::NAMES},
   {"-dce",    QConfObj::CE,   QConfDeleteKind::NAMES},
   {"-dq",     QConfObj::CQ,   QConfDeleteKind::NAMES},
   {"-Dcal",   QConfObj::CAL,  QConfDeleteKind::FILE},
   {"-Dckpt",  QConfObj::CK,   QConfDeleteKind::FILE},
   {"-De",     QConfObj::EH,   QConfDeleteKind::FILE},
   {"-Drole",  QConfObj::RL,   QConfDeleteKind::FILE},
   {"-Dp",     QConfObj::PE,   QConfDeleteKind::FILE},
   {"-Dce",    QConfObj::CE,   QConfDeleteKind::FILE},
   {"-Dhgrp",  QConfObj::HGRP, QConfDeleteKind::FILE},
   {"-Du",     QConfObj::US,   QConfDeleteKind::FILE},
   {"-Duser",  QConfObj::UU,   QConfDeleteKind::FILE},
   {"-Dprj",   QConfObj::PR,   QConfDeleteKind::FILE},
   {"-Dq",     QConfObj::CQ,   QConfDeleteKind::FILE},
   {"-dhgrp",  QConfObj::HGRP, QConfDeleteKind::NAMES, qconf_delete_hgroup_list},
   {"-Drqs",   QConfObj::RQS,  QConfDeleteKind::FILE, qconf_delete_rqs_path},
   {"-dconf",  QConfObj::CONF, QConfDeleteKind::NAMES, qconf_delete_conf_hosts},
   {"-Dconf",  QConfObj::CONF, QConfDeleteKind::FILE, qconf_delete_conf_path},
};

QConfOpResult
qconf_run_delete(std::ostream &os, const qconf_op &op, lList **alpp) {
   DENTER(TOP_LAYER);

   const qconf_delete_entry *entry = nullptr;
   for (const qconf_delete_entry &e : qconf_delete_table) {
      if (op.token == e.switch_name) {
         entry = &e;
         break;
      }
   }
   if (entry == nullptr) {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   if (qconf_missing_argument(op, alpp)) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }


   const char *arg = op.args.empty() ? nullptr : op.args.front().c_str();
   const int failed = (entry->run != nullptr) ? entry->run(os, op, alpp)
                      : (entry->kind == QConfDeleteKind::NAMES) ? qconf_delete_names(entry->object, arg, alpp)
                                                                : qconf_delete_obj(os, entry->object, arg, alpp);

   DRETURN(failed != 0 ? QConfOpResult::FAILED : QConfOpResult::OK);
}
