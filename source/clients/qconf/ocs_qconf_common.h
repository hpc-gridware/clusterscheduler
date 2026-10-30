#pragma once
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
 * @brief What every switch family of `qconf` shares (CS-2972)
 *
 * The hardening options, the message helpers, the file iteration, the
 * client side permission checks, the name hooks that turn the argument of a
 * switch into object names, and the result every dispatcher reports.
 */

#include <ostream>
#include <string>
#include <vector>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Target.h"
#include "spool/sge_spooling_utilities.h"
#include "procedure/qconf/ocs_QConfObject.h"
#include "procedure/qconf/ocs_QConfParameter.h"
#include "procedure/qconf/write/ocs_QConfWriteParameter.h"

/** @brief H3 (-dry):    validate/report, do not send */
extern bool qconf_opt_dry_run;

/** @brief H6 (-f):      skip the bulk-delete prompt */
extern bool qconf_opt_force;

/** @brief H2 (-strict): apply nothing unless all files valid */
extern bool qconf_opt_strict;

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
 */
void
qconf_info(std::ostream &os, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

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
bool
qconf_confirm(const char *prompt);

/**
 * @brief Test whether an object of a given name already exists in qmaster.
 *
 * @param target  GDI target list to query (e.g. CAL_LIST)
 * @param descr   CULL descriptor of the object type
 * @param name_nm name attribute of the object (e.g. CAL_name)
 * @param name    object name to look up
 * @return true if an object of that name exists, false otherwise
 */
bool
qconf_object_exists(ocs::gdi::Target target, const lDescr *descr, int name_nm,
                    const char *name);

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
int
qconf_for_each_file(const char *path, int (*per_file)(const char *, void *), void *ctx);

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
 * @brief Per-file callback: read one object and queue its name for deletion (CS-2298 C5).
 *
 * A name that no longer exists is reported and skipped rather than queued
 * (CS-2299 H5). Updates the n_ok / n_fail counters in the context.
 *
 * @param filepath path of the object file to read
 * @param vctx     pointer to the qconf_file_ctx for this batch
 * @return 0 on success (including a skipped non-existent name), 1 on read failure
 */
int
qconf_collect_name(const char *filepath, void *vctx);

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
int
qconf_collect_text(const char *filepath, void *vctx);

/** @brief Read a whole file into a string
 * @param filename the file to read
 * @param text receives the content, cleared first
 * @return true when the file was read without error
 */
bool qconf_read_file_text(const char *filename, std::string &text);

/** @brief Resolve the host key of a configuration file
 *
 * The files of "-Aconf"/"-Mconf"/"-Dconf" are named after their hosts, so the
 * basename is the host, resolved in place.
 *
 * @param filepath the configuration file
 * @param host_ep receives the resolved host in EH_name
 * @return the resolved host name, owned by @p host_ep
 */
const char *
qconf_conf_host_of(const char *filepath, lListElem *host_ep);

/** @brief Context for the config apply/delete per-file callbacks. */
struct qconf_conf_ctx {
   std::ostream *os;   ///< where the per-file messages go (CS-1841)
   lList *hosts;     ///< delete path: collected CONF_name host elements
   int n_ok;         ///< files applied / hosts collected (CS-2299 H1)
   int n_fail;       ///< files that failed
};

/** @brief Report an operation whose command line ended before its mandatory arguments (CS-2969)
 *
 * The parser hands such an operation over truncated on purpose
 * (QConfParseStatus::MISSING_ARGUMENT), so that whoever serves it reports it in
 * the switch's own words. The option blocks did that by running into the end of
 * their slice in sge_parser_get_next(); the table dispatchers have no slice to
 * walk, so they ask here, and report what sge_parser_get_next() reported - the
 * last word that was given, and the usage.
 *
 * @param op the operation a table is about to serve
 * @return true when an argument is missing and the operation has been reported
 */
bool
qconf_missing_argument(const qconf_op &op);

/** @brief What one of the table driven dispatchers made of an operation
 *
 * One answer instead of two signals: before CS-2950 a dispatcher returned
 * whether it had handled the operation and raised a separate exit status for
 * whether that had failed, so that the loop could go on after a failure. Since
 * a failure stops the command line, "handled" and "failed" are simply two of
 * the three outcomes.
 */
enum class QConfOpResult {
   NOT_HANDLED,  ///< not a switch of this dispatcher; the next one or a block serves it
   OK,           ///< served, and it succeeded
   FAILED        ///< served, and it failed; the command line stops
};

/** @brief The permission check a switch makes in the client before it is served
 *
 * qmaster decides in any case: a configuration write needs an admin host there,
 * and a manager for every object but the userset, which an operator may change.
 * One kind of check is therefore enough here (CS-2952) - for the userset rows it
 * is stricter than qmaster. Which switches check at all is still the history of
 * each ticket, and CS-2973 moves the decision into qmaster altogether.
 */
enum class QConfClientCheck {
   NONE,                    ///< no check in the client
   MANAGER_ON_ADMIN_HOST    ///< qconf_is_manager_on_admin_host()
};

/**
 * @brief Make the permission check a table row asks for
 *
 * Each check function exits the client on a refusal, so returning from here
 * means the caller may go on.
 *
 * @param check    the check to make
 * @param username the calling user
 * @param hostname the calling host
 */
void
qconf_client_check(QConfClientCheck check, const char *username, const char *hostname);

/** @brief Turn the argument of a switch into the names it is served for, one call each (CS-2967)
 *
 * The hook column of qconf_write_table and qconf_sub_procedure_table. A row
 * without one is served once, with its argument as typed; a row with one is
 * served once per name the hook returns. A name the hook cannot use it reports
 * itself and leaves out, setting @p failed, so the others are still served -
 * which is what the blocks did.
 *
 * @param op the operation
 * @param names receives the names to serve
 * @param failed set when a name had to be left out
 */
using qconf_names_hook = void (*)(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-ae [host]": the template when no name is given, the resolved host otherwise
 *
 * The template is an entry in the exec host list and not a host, so it is the
 * one name that must not be resolved.
 *
 * @param op the operation
 * @param names receives the name to edit
 * @param failed set when the host does not resolve
 */
void
qconf_names_ae(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-se host": the one host, resolved
 * @param op the operation
 * @param names receives the name to show
 * @param failed set when the host does not resolve
 */
void
qconf_names_one_host(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-me host,...": every host of the list, each resolved; one that does not is left out
 * @param op the operation
 * @param names receives the names to edit
 * @param failed set when a host does not resolve
 */
void
qconf_names_host_list(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-mu userset,...": every userset of the list
 *
 * One round trip per name, as edit_usersets() had it; a name that is no
 * userset yet is answered by the read with the template and created on exit.
 *
 * @param op the operation
 * @param names receives the names to edit
 * @param failed unused, every name is served
 */
void
qconf_names_userset_list(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-suser user,...": every user of the list
 * @param op the operation
 * @param names receives the names to show
 * @param failed unused, every name is served
 */
void
qconf_names_user_list(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-sconf"/"-aconf": the host list, a host that does not resolve left out
 * @param op the operation
 * @param names receives the configuration names
 * @param failed set when a host was left out
 */
void
qconf_names_conf_strict(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-mconf": the host list, a host that does not resolve kept under its name as typed
 * @param op the operation
 * @param names receives the configuration names
 * @param failed unused, every host is served
 */
void
qconf_names_conf_lenient(const qconf_op &op, std::vector<std::string> &names, bool &failed);

/** @brief "-Aconf"/"-Mconf": the file or directory list, "global" when none is given
 *
 * The files are named after the hosts, so a missing list means the file of the
 * global configuration, as it always did.
 *
 * @param op the operation
 * @param names receives the paths
 * @param failed unused
 */
void
qconf_names_conf_paths(const qconf_op &op, std::vector<std::string> &names, bool &failed);
