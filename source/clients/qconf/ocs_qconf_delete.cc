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
#include <vector>

#include "uti/sge_log.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "procedure/qconf/action/ocs_QConfActionParameter.h"
#include "procedure/qconf/write/ocs_QConfWriteModelClient.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_delete.h"
#include "ocs_qconf_common.h"
#include "ocs_qconf_parse.h"

/** @brief Join names or files for one argument of the delete action
 * @param lines the names or the files
 * @return them, separated by QConfActionParameter::LINE_SEPARATOR
 */
static std::string
qconf_delete_join(const std::vector<std::string> &lines) {
   std::string joined;
   for (const std::string &line : lines) {
      if (!joined.empty()) {
         joined += ocs::QConfActionParameter::LINE_SEPARATOR;
      }
      joined += line;
   }
   return joined;
}

/** @brief Hand the names to the delete action in qmaster (CS-2983)
 *
 * qmaster checks the permission for the object list, then deletes name after
 * name under the lock - through the function a GDI delete uses, so a name it
 * refuses is reported and the others are deleted all the same. With the files
 * the names were read from, a name that does not exist is skipped with a
 * warning naming its file (CS-2299 H5); without them it is refused, as it is
 * for a name list. Under "-dry" qmaster reports what it would delete.
 *
 * @param o       the object type
 * @param names   the names; empty for the share tree, which is deleted as a whole
 * @param sources for each name the file it was read from, or empty for a name list
 * @param alpp    the answer list of the operation
 * @return 0 when every object was deleted, 1 otherwise
 */
static int
qconf_delete_send(const QConfObj o, const std::vector<std::string> &names, const std::vector<std::string> &sources,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   ocs::QConfActionParameter action(ocs::QConfActionParameter::OBJECT_DELETE);
   action.set_arg(ocs::QConfActionParameter::ARG_NAMES, qconf_delete_join(names));
   if (!sources.empty()) {
      action.set_arg(ocs::QConfActionParameter::ARG_SOURCES, qconf_delete_join(sources));
      action.set_arg(ocs::QConfActionParameter::ARG_SKIP_MISSING, ocs::QConfActionParameter::ARG_TRUE);
   }
   if (qconf_opt_dry_run) {
      action.set_arg(ocs::QConfActionParameter::ARG_DRY, ocs::QConfActionParameter::ARG_TRUE);
   }

   ocs::QConfWriteModelClient model(qconf_desc(o).target);
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const bool ok = model.make_snapshot(&answer_list, action);
   const int shown = qconf_answer_take(alpp, &answer_list);

   DRETURN((!ok || shown != 0) ? 1 : 0);
}

/** @brief The names of a parsed name list of a known type
 * @tparam F the name field of the elements
 * @param list the elements
 * @return the names, in the order given; elements without a name are skipped
 */
template <auto F>
static std::vector<std::string>
qconf_delete_names_of(const ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> list) {
   DENTER(TOP_LAYER);
   std::vector<std::string> names;
   for (const auto ep : list) {
      const char *name = static_cast<const char *>(ocs::tom::get<F>(ep));
      if (name != nullptr) {
         names.emplace_back(name);
      }
   }
   DRETURN(names);
}

/** @brief Delete the objects whose names a batch read from files collected
 *
 * A directory expands to a bulk delete: it is confirmed interactively unless -f
 * is given (CS-2299 H6), and -dry previews it (CS-2299 H3), which asks nothing.
 *
 * @param os      the stream the output is written to (CS-1841)
 * @param o       the object type
 * @param path    the file or directory the names were read from
 * @param names   the collected names
 * @param sources for each name the file it was read from
 * @param n_fail  the files that could not be read
 * @param alpp    the answer list of the operation
 * @return the number of files that could not be read, plus one when a delete failed
 */
static int
qconf_delete_collected(std::ostream &os, const QConfObj o, const char *path, const std::vector<std::string> &names,
                       const std::vector<std::string> &sources, const int n_fail,
                       ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   const bool is_dir = sge_is_directory(path);
   const int n_names = static_cast<int>(names.size());

   if (is_dir) {
      /* CS-2299 H1: report the read phase before the batch delete. */
      qconf_info(os, MSG_QCONF_DELCOLLECTSUMMARY_SII, path, n_names, n_fail);
   }
   if (n_names == 0) {
      return n_fail;
   }
   if (is_dir && !qconf_opt_dry_run && !qconf_opt_force) {
      /* CS-2299 H6: a directory expands to a bulk delete - confirm first. */
      const bool ok = qconf_confirm(qconf_format(MSG_QCONF_CONFIRMDELETE_I, n_names).c_str(), alpp);
      if (!ok) {
         qconf_answer(alpp, ANSWER_QUALITY_WARNING, MSG_QCONF_DELETEABORTED_I, n_names);
         return n_fail;
      }
   }
   return n_fail + qconf_delete_send(o, names, sources, alpp);
}

/**
 * @brief Delete every object named in the file(s) at a path (file or directory) (CS-2298 C5).
 *
 * The name attribute is read from each file and the rest of the content is
 * ignored.
 *
 * @param os the stream the output is written to (CS-1841)
 * @param o       the object type
 * @param path    file or directory naming the objects to delete
 * @param alpp    the answer list of the operation
 * @return the number of files that failed, plus one when a delete failed
 */
static int
qconf_delete_obj(std::ostream &os, const QConfObj o, const char *path, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   const qconf_object_desc &d = qconf_desc(o);
   bool owned = false;
   /* the write field list, which a diagnostic attribute has no business in: the file is
    * read to learn the names of the objects to delete */
   spooling_field *fields = d.build_fields(qconf_opt_format, false, &owned);
   std::vector<std::string> names;
   std::vector<std::string> sources;
   qconf_file_ctx ctx = {&os, o, fields, d.parse_values, &names, &sources, 0, 0, alpp};
   qconf_for_each_file(path, qconf_collect_name, &ctx, alpp);
   if (owned) {
      sge_free(&fields);
   }
   return qconf_delete_collected(os, o, path, names, sources, ctx.n_fail, alpp);
}

/* ===== CS-2307: resource quota set helpers =================================
 * RQS files hold a *list* of rule sets (read with spool_flatfile_read_list), so
 * they are read by a callback of their own; what is collected is deleted the way
 * every other type's names are (CS-2983). */

/** @brief Context for the per-file callbacks of a type's own: the collected names, their files and the read-failure count. */
struct qconf_names_ctx {
   std::vector<std::string> *names;   ///< the names of the rule sets of every file
   std::vector<std::string> *sources; ///< for each name the file it was read from
   int n_fail;                        ///< files that failed to read
   ocs::tom::ListPtr<ocs::tom::AN> *alpp;                      ///< the answer list of the operation (CS-2971)
};

/** @brief qconf_for_each_file callback: read one file's resource quota sets and
 *  queue their names (CS-2307)
 *
 * The list is read with `RQS`, so it is taken over without the descriptor check.
 *
 * @param filepath the file to read
 * @param vctx     the qconf_names_ctx of the batch
 * @return 0 when the file was read, 1 otherwise
 */
static int
qconf_rqs_collect(const char *filepath, void *vctx) {
   DENTER(TOP_LAYER);

   auto *ctx = static_cast<qconf_names_ctx *>(vctx);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const auto rqs_list = ocs::tom::ListPtr<ocs::tom::RQS>::from_typed(
      spool_flatfile_read_list(&alp, ocs::tom::RQS::descriptor.e, RQS_fields, nullptr, true, &qconf_rqs_sfi,
                               qconf_opt_format, nullptr, filepath));
   if (qconf_answer_take(ctx->alpp, &alp) != 0 || rqs_list == nullptr) {
      ctx->n_fail++;
      DRETURN(1);
   }
   for (const auto rqs : *rqs_list) {
      ctx->names->emplace_back(ocs::tom::get<RQS_name>(rqs));
      ctx->sources->emplace_back(filepath);
   }

   DRETURN(0);
}

/**
 * @brief Per-file callback: queue the host configuration a file is named after (CS-2311 C5).
 *
 * The host key is the file basename, resolved as -Aconf/-dconf resolve it.
 *
 * @param filepath path of the config file naming the host
 * @param vctx     the qconf_names_ctx of the batch
 * @return 0 always
 */
static int
qconf_conf_collect_host(const char *filepath, void *vctx) {
   auto *ctx = static_cast<qconf_names_ctx *>(vctx);
   auto host_ep = ocs::tom::ElementPtr<ocs::tom::EH>::create();
   ctx->names->emplace_back(qconf_conf_host_of(filepath, *host_ep));
   ctx->sources->emplace_back(filepath);
   return 0;
}

/** @brief Delete the objects a file or directory names, read by a callback of the type's own
 *
 * The resource quota sets and the host configurations, whose files are not one
 * object with a name field each.
 *
 * @param os the stream the output is written to
 * @param o the object type
 * @param path the file or directory
 * @param collect the callback that reads one file's names
 * @param alpp the answer list of the operation
 * @return the number of files that failed, plus one when a delete failed
 */
static int
qconf_delete_path_collected(std::ostream &os, const QConfObj o, const char *path,
                            int (*collect)(const char *, void *), ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   std::vector<std::string> names;
   std::vector<std::string> sources;
   qconf_names_ctx ctx = {&names, &sources, 0, alpp};
   qconf_for_each_file(path, collect, &ctx, alpp);
   return qconf_delete_collected(os, o, path, names, sources, ctx.n_fail, alpp);
}

/** @brief How a table driven delete switch names what it deletes */
enum class QConfDeleteKind {
   NAMES,   ///< "-d<obj> name,...": the names of the list
   FILE     ///< "-D<obj> file|dir": the names the files carry
};

/** @brief One delete switch served by qconf_run_delete() */
struct qconf_delete_entry {
   const char *switch_name;   ///< the switch as typed
   QConfObj object;           ///< the object type it deletes
   QConfDeleteKind kind;      ///< names or file
   int (*run)(std::ostream &os, const qconf_op &op,
              ocs::tom::ListPtr<ocs::tom::AN> *alpp); ///< a delete of its own instead of the kind's engine, or nullptr
};

/** @brief "-dhgrp group,...": the host groups of the list (CS-2306 C4)
 *
 * The list is taken apart by parse_name_list_to_cull(), as it always was.
 *
 * @param os unused
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_delete_send()
 */
static int
qconf_delete_hgroup_list([[maybe_unused]] std::ostream &os, const qconf_op &op, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   std::string arg = op.args[0];
   const auto groups = parse_name_list_to_cull<HGRP_name>("host groups to del", arg.data());
   return qconf_delete_send(QConfObj::HGRP, qconf_delete_names_of<HGRP_name>(*groups), {}, alpp);
}

/** @brief "-de host,...": the execution hosts of the list
 *
 * The list is taken apart by parse_name_list_to_cull(), as the block that
 * served it did; qmaster resolves each host (CS-2753 made it one request).
 *
 * @param os unused
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_delete_send()
 */
static int
qconf_delete_exechost_list([[maybe_unused]] std::ostream &os, const qconf_op &op,
                           ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   std::string arg = op.args[0];
   const auto hosts = parse_name_list_to_cull<EH_name>("host to del", arg.data());
   return qconf_delete_send(QConfObj::EH, qconf_delete_names_of<EH_name>(*hosts), {}, alpp);
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
qconf_delete_conf_hosts([[maybe_unused]] std::ostream &os, const qconf_op &op, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   if (op.args.empty()) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, "%s", MSG_ANSWER_NEEDHOSTNAMETODELLOCALCONFIG);
      return 1;
   }
   int ret = 0;
   std::string list = op.args[0];
   auto host_ep = ocs::tom::ElementPtr<ocs::tom::EH>::create();
   std::vector<std::string> names;
   for (const char *cp = sge_strtok(list.data(), ","); cp != nullptr && *cp != '\0';
        cp = sge_strtok(nullptr, ",")) {
      ocs::tom::set<EH_name>(host_ep, cp);
      if (ocs_resolve_host<EH_name>(*host_ep) != CL_RETVAL_OK) {
         qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_SGETEXT_CANTRESOLVEHOST_S, cp);
         ret = 1;
      }
      names.emplace_back(static_cast<const char *>(ocs::tom::get<EH_name>(host_ep)));
   }
   host_ep.reset();
   if (!names.empty() && qconf_delete_send(QConfObj::CONF, names, {}, alpp) != 0) {
      ret = 1;
   }
   return ret;
}

/** @brief "-Dconf file|dir": the configurations the files are named after
 * @param os the stream the output is written to
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_delete_path_collected()
 */
static int
qconf_delete_conf_path(std::ostream &os, const qconf_op &op, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   return qconf_delete_path_collected(os, QConfObj::CONF, op.args[0].c_str(), qconf_conf_collect_host, alpp);
}

/** @brief "-Drqs file|dir": the resource quota sets a file names, through the list reader of their type
 * @param os the stream the output is written to
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return the status of qconf_delete_path_collected()
 */
static int
qconf_delete_rqs_path(std::ostream &os, const qconf_op &op, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   return qconf_delete_path_collected(os, QConfObj::RQS, op.args[0].c_str(), qconf_rqs_collect, alpp);
}

/** @brief The delete switches (CS-2952, CS-2983)
 *
 * Every "-d<obj>" and "-D<obj>": the names of a list, or the names the files
 * carry, handed to the delete action in qmaster; the permission is qmaster's
 * (CS-2973). "-dhgrp", "-de", "-dconf", "-Dconf" and "-Drqs" are rows with a
 * reader of their own (CS-2967). "-dh"/"-ds" change the members of a reserved
 * host group, and "-du"/"-dm"/"-do" and "-dstnode" change parts of objects: all
 * of them are actions, see qconf_action_table.
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
   {"-de",     QConfObj::EH,   QConfDeleteKind::NAMES, qconf_delete_exechost_list},
   {"-Drqs",   QConfObj::RQS,  QConfDeleteKind::FILE, qconf_delete_rqs_path},
   {"-dconf",  QConfObj::CONF, QConfDeleteKind::NAMES, qconf_delete_conf_hosts},
   {"-Dconf",  QConfObj::CONF, QConfDeleteKind::FILE, qconf_delete_conf_path},
};

bool
qconf_delete_honours_dry(const qconf_op &op) {
   /* since CS-2983 every delete has a dry mode: qmaster answers it */
   for (const qconf_delete_entry &e : qconf_delete_table) {
      if (op.token == e.switch_name) {
         return true;
      }
   }
   return false;
}

QConfOpResult
qconf_run_delete(std::ostream &os, const qconf_op &op, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
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

   int failed;
   if (entry->run != nullptr) {
      failed = entry->run(os, op, alpp);
   } else if (entry->kind == QConfDeleteKind::FILE) {
      failed = qconf_delete_obj(os, entry->object, op.args.front().c_str(), alpp);
   } else {
      /* a type without a name field is deleted as a whole, so there is nothing to name */
      const auto names = ocs_with_qconf_object(entry->object, [&op](const auto &object) {
         using E = ocs_qconf_object_entry_t<decltype(object)>;
         std::vector<std::string> list_names;
         if constexpr (E::has_name_key) {
            if (!op.args.empty()) {
               const auto lp = ocs::tom::ListPtr<typename E::type>::template from_string<E::key>(
                  op.args.front().c_str(), ", ");
               list_names = qconf_delete_names_of<E::key>(lp);
            }
         }
         return list_names;
      });
      failed = qconf_delete_send(entry->object, names, {}, alpp);
   }

   DRETURN(failed != 0 ? QConfOpResult::FAILED : QConfOpResult::OK);
}
