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
 * @brief The `qconf` command line as data: what each switch takes, and what was given
 */

/** @defgroup qconf_ops The parsed qconf command line
 * @brief Turning `argv` into an ordered list of operations (CS-2798)
 *
 * `sge_parse_qconf()` used to be one loop that executed every switch the moment
 * it recognised it. A stored procedure call needs the arguments as **data**, so
 * that `ProcedureParameter::get_bundle()` can marshal them into an `SPP_Type`
 * list and qmaster can reconstruct them - which is impossible while parsing and
 * executing are the same pass.
 *
 * This module is the parsing half. It knows one thing the old loop only knew
 * implicitly, spread over 163 blocks: **how many arguments each switch takes**.
 * That knowledge lives in one table here, and that table is what step 8
 * (CS-2799) marshals.
 *
 * @see @ref qconf_object_desc, the equivalent table for the object types
 * @{
 */

#include <cstring>
#include <string>
#include <vector>

#include "spool/flatfile/sge_flatfile.h"      /* spool_flatfile_format */
#include "spool/flatfile/ocs_spool_json.h"    /* ocs_json_value_format */

/** @brief An unbounded trailing argument list, ended by the next switch
 *
 * Used as #qconf_opt_spec::max_args. The list ends where a token starts with
 * `-` or the command line does, which is the rule the hand written blocks used
 * through `sge_next_is_an_opt()`.
 */
constexpr int QCONF_ARGS_GREEDY = -1;

/** @brief What one `qconf` switch is allowed to take
 *
 * One row per switch. The arities were read off the option blocks of
 * `sge_parse_qconf()`: a mandatory argument is one the block fetches
 * unconditionally, an optional one is fetched behind a `sge_next_is_an_opt()`
 * guard, and a greedy tail is a `while (!sge_next_is_an_opt(...))` loop.
 */
struct qconf_opt_spec {
   const char *name;       ///< the switch as it is typed, e.g. `-sq`
   int min_args;           ///< arguments that must follow; fewer is an error
   int max_args;           ///< arguments that may follow, or #QCONF_ARGS_GREEDY
   int prefix_len;         ///< 0 for an exact match; otherwise the bytes `strncmp()` compares
   bool invocation_wide;   ///< a property of the whole call, not an operation of its own
};

/** @brief One switch of the command line, with the arguments it was given */
struct qconf_op {
   const qconf_opt_spec *spec = nullptr;   ///< the row that matched
   std::string token;                      ///< what was actually typed; differs from
                                           ///< qconf_opt_spec::name for the prefix
                                           ///< matched switches, where the suffix
                                           ///< carries meaning (`-ke`, `-kej`)
   std::vector<std::string> args;          ///< the arguments, in order

   /** @brief Is this the given switch?
    * @param name a switch name as it appears in the table
    * @return true when this operation was built from that row
    */
   [[nodiscard]] bool is(const char *name) const {
      return spec != nullptr && strcmp(spec->name, name) == 0;
   }

   /** @brief One argument
    * @param i its index
    * @return the argument, or nullptr when fewer were given
    */
   [[nodiscard]] const char *arg(size_t i) const {
      return i < args.size() ? args[i].c_str() : nullptr;
   }

   /** @brief How many arguments were given
    * @return the count; between qconf_opt_spec::min_args and its maximum
    */
   [[nodiscard]] size_t arg_count() const { return args.size(); }

   char **as_argv();

private:
   std::vector<char *> argv_;   ///< backing store of as_argv()
};

/** @brief Everything one `qconf` command line says
 *
 * The invocation-wide flags come from a first pass over `argv` and hold for the
 * whole call regardless of where they appear - `-fmt json` at the end still
 * governs a `-sq` at the front. The operations come from a second pass and are
 * executed in the order they were typed.
 */
struct qconf_invocation {
   bool binding = false;      ///< `-cb`: also show the socket and core columns
   bool dry_run = false;      ///< `-dry`: validate and report, send nothing
   bool force = false;        ///< `-f`: overwrite, and skip the bulk-delete prompt
   bool strict = false;       ///< `-strict`: apply nothing unless every file is valid
   spool_flatfile_format format = SP_FORM_ASCII;                       ///< `-fmt`
   ocs_json_value_format value_format = OCS_JSON_VALUES_COMPACT;       ///< `-fmtval`
   std::vector<qconf_op> ops;                                          ///< in command line order
};

/** @brief Why parsing stopped */
enum class QConfParseStatus {
   OK,                 ///< the whole command line was understood
   MISSING_ARGUMENT,   ///< a switch was given fewer arguments than it needs
   UNKNOWN_OPTION,     ///< a token is not a switch this client knows
   BAD_FORMAT,         ///< `-fmt` was given something other than `plain` or `json`
   BAD_FORMAT_VALUE,   ///< `-fmtval` was given something other than `compact` or `numeric`
   NO_FORMAT_VALUE,    ///< `-fmt` was given without a value at all
};

const qconf_opt_spec *qconf_opt_lookup(const char *token);

QConfParseStatus qconf_parse_args(qconf_invocation &inv, char **argv, std::string &bad_token);

/** @} */
