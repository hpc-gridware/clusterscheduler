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
 * @brief Arguments of one `qconf` stored procedure call
 */

#include <cstring>
#include <string>
#include <vector>

#include "ocs_ProcedureParameter.h"

/** @brief An unbounded trailing argument list, ended by the next switch
 *
 * Used as #qconf_opt_spec::max_args. The list ends where a token starts with
 * `-` or the command line does, which is the rule the hand written option
 * blocks of `sge_parse_qconf()` used through `sge_next_is_an_opt()`.
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

namespace ocs {
   /** @brief The arguments of one `qconf` stored procedure call
    *
    * `qconf` is multi-operation - `qconf -sq a -dq b` is one invocation - while
    * a procedure call is one operation. So this holds **one** operation plus the
    * switches that apply to the whole command line; the ordered list lives in
    * QConfParameterClient, which is a client concept.
    *
    * The output format is not repeated here: `-fmt plain|json` is exactly
    * ProcedureParameter::OutputFormat under another name.
    *
    * @ingroup libprocedure
    */
   class QConfParameter : public ProcedureParameter {
#pragma region Data Types

   public:
      /** @brief How TIME and MEM values are rendered in JSON output, `-fmtval` */
      enum class ValueFormat : uint32_t {
         COMPACT,   ///< the human readable unit and colon strings, `"2.000G"`, `"0:5:0"` - the default
         NUMERIC    ///< plain seconds and bytes
      };
#pragma endregion

#pragma region Constants

   public:
      /** @name Names of the entries in the marshalled parameter bundle
       * @{
       */
      static constexpr auto OPERATION = "operation";           ///< the switch that was typed
      static constexpr auto OPERATION_ARGS = "operation_args"; ///< its arguments, in order
      static constexpr auto INVOCATION_FLAGS = "invocation_flags";  ///< -cb, -dry, -f, -strict as a bitmask
      static constexpr auto VALUE_FORMAT = "value_format";     ///< -fmtval
      /** @} */

      /** @name Sub-procedure names (CS-2816)
       *
       * Most qconf reads are one shape: fetch by the descriptor table, render
       * through its `sfi`. The bespoke views are not, and they do not belong in
       * qconf_read_table - they render themselves, and some of them need a whole
       * object list where the switch names a single object. They become
       * sub-procedures instead, dispatched on `{procedure, sub_procedure}` the
       * way qstat's four output formats already are: one model, one view and one
       * controller per shape.
       *
       * The empty sub-procedure name stays the table-driven read of groups A
       * and B, so nothing that already works has to change.
       * @{
       */
      static constexpr auto HGROUP_SHOW = "hostgroup-show";         ///< `qconf -shgrp`
      static constexpr auto HGROUP_TREE = "hostgroup-tree";         ///< `qconf -shgrp_tree`
      static constexpr auto HGROUP_RESOLVED = "hostgroup-resolved"; ///< `qconf -shgrp_resolved`
      static constexpr auto HGROUP_WHY = "hostgroup-why";           ///< `qconf -shgrp_why`
      static constexpr auto CENTRY_LIST = "complex-list";           ///< `qconf -sc`
      static constexpr auto CQUEUE_SHOW = "cqueue-show";            ///< `qconf -sq`
      static constexpr auto CATEGORY_LIST = "category-list";        ///< `qconf -scatl`
      static constexpr auto QUEUE_SICK = "queue-sick";              ///< `qconf -sds`
      static constexpr auto OBJECT_FILTER = "object-filter";        ///< `qconf -sobjl`
      static constexpr auto EVENT_CLIENT_LIST = "event-clients";    ///< `qconf -secl`
      static constexpr auto PROCESSOR_LIST = "processors";          ///< `qconf -sep`
      static constexpr auto THREAD_POOL_LIST = "thread-pools";      ///< `qconf -stl`
      static constexpr auto SHARETREE_TREE = "sharetree-tree";      ///< `qconf -sst`, `-bonsai`
      static constexpr auto SHARETREE_NODES = "sharetree-nodes";    ///< `qconf -sstnode`, `-rsstnode`
      static constexpr auto OBJECT_EXPORT = "object-export";        ///< `qconf -S<obj>`

      /** @brief Whether a sub-procedure name is one of the host group views
       * @param name the name, empty for the table-driven read
       * @return true when it needs the host group snapshot
       */
      static bool is_hgroup_sub_procedure(const std::string &name) {
         return name == HGROUP_SHOW || name == HGROUP_TREE ||
                name == HGROUP_RESOLVED || name == HGROUP_WHY;
      }
      /** @} */

      /** @name Bits of the #INVOCATION_FLAGS bundle entry
       * @{
       */
      static constexpr uint32_t FLAG_BINDING = 0x1u;   ///< `-cb`
      static constexpr uint32_t FLAG_DRY_RUN = 0x2u;   ///< `-dry`
      static constexpr uint32_t FLAG_FORCE = 0x4u;     ///< `-f`
      static constexpr uint32_t FLAG_STRICT = 0x8u;    ///< `-strict`
      static constexpr uint32_t FLAG_SINGLE_LINE = 0x10u;   ///< `SGE_SINGLE_LINE` in the client's environment
      static constexpr uint32_t FLAG_FOR_EDIT = 0x20u;      ///< the read feeds an editor, see get_for_edit()
      static constexpr uint32_t FLAG_FOR_EXPORT = 0x40u;    ///< the read feeds an export, see get_for_export()
      /** @} */
#pragma endregion

#pragma region Data

   protected:
      bool binding_ = false;    ///< `-cb`: also show the socket and core columns
      bool dry_run_ = false;    ///< `-dry`: validate and report, send nothing
      bool force_ = false;      ///< `-f`: overwrite, and skip the bulk-delete prompt
      bool strict_ = false;     ///< `-strict`: apply nothing unless every file is valid
      bool single_line_ = false;   ///< `SGE_SINGLE_LINE`: do not break long lines
      bool for_edit_ = false;   ///< the rendered text is going to an editor and comes back
      bool for_export_ = false; ///< the rendered text is going into an export file
      ValueFormat value_format_ = ValueFormat::COMPACT;   ///< `-fmtval`
      qconf_op operation_;      ///< the one operation this call carries

   public:
      /** @brief Whether `-cb` was given
       * @return true when the socket and core columns are wanted
       */
      [[nodiscard]] bool get_binding() const { return binding_; }

      /** @brief Whether `-dry` was given
       * @return true when nothing may be sent to qmaster
       */
      [[nodiscard]] bool get_dry_run() const { return dry_run_; }

      /** @brief Whether `-f` was given
       * @return true when existing files may be overwritten and prompts skipped
       */
      [[nodiscard]] bool get_force() const { return force_; }

      /** @brief Whether `-strict` was given
       * @return true when nothing may be applied unless every file is valid
       */
      [[nodiscard]] bool get_strict() const { return strict_; }

      /** @brief Whether the client's environment asks for unbroken lines
       *
       * `SGE_SINGLE_LINE` is a preference of the user running the client, so it
       * is read where the client runs and travels with the call. Reading it
       * again while rendering would consult qmaster's environment instead
       * (CS-2799).
       *
       * @return true when long lines must not be wrapped
       */
      [[nodiscard]] bool get_single_line() const { return single_line_; }

      /** @brief Whether this read renders a text that will be written back
       *
       * A read that feeds an editor has to render exactly the attributes the
       * write path can parse, which is not the same set the type shows: `-se`
       * adds the runtime `load_values` and `processors` to what it displays,
       * and no write accepts them. With the show list the text would come back
       * carrying two attributes qmaster then refuses, so `-me` would fail on
       * every exec host. The flag picks the type's write field list instead,
       * which is the same list `-S<obj>` exports with and for the same reason.
       *
       * It is set by the interactive modify (CS-2823) and by nothing else;
       * every other read renders what the type shows.
       *
       * @return true when the editable form is wanted
       */
      [[nodiscard]] bool get_for_edit() const { return for_edit_; }

      /** @brief Ask for the editable form of the object
       * @param for_edit true when the text is going to an editor
       */
      void set_for_edit(bool for_edit) { for_edit_ = for_edit; }

      /** @brief Whether this read renders the files of an export
       *
       * An export has the same demand on the field list as an editor and for
       * the same reason - what it writes must re-import - so it renders the
       * write list too, see get_for_edit(). What it does not want is the
       * decoration a show adds around the object: the configuration's
       * `#<host>:` header is a comment that tells the blocks of
       * `-sconf a,b` apart, and in a file of its own it would come back as an
       * unknown attribute.
       *
       * It also decides what an empty result means. A show of a type with no
       * objects is an error - "no calendar defined" - and a bulk export of the
       * same cluster is not: it writes an empty directory and reports zero
       * objects, which is what the client did while it fetched for itself, and
       * a backup script must not fail over a type the cluster does not use.
       *
       * Set by the export switches (CS-2826) and by nothing else.
       *
       * @return true when the read produces export files
       */
      [[nodiscard]] bool get_for_export() const { return for_export_; }

      /** @brief Ask for the export form of the object
       * @param for_export true when the text becomes an export file
       */
      void set_for_export(bool for_export) { for_export_ = for_export; }

      /** @brief How TIME and MEM values are rendered in JSON
       * @return compact strings or plain numbers
       */
      [[nodiscard]] ValueFormat get_value_format() const { return value_format_; }

      /** @brief The operation this call carries
       * @return the switch and its arguments
       */
      [[nodiscard]] const qconf_op &get_operation() const { return operation_; }

      /** @brief Set the operation this call carries
       * @param operation the switch and its arguments
       */
      void set_operation(const qconf_op &operation) { operation_ = operation; }
#pragma endregion

#pragma region Marshaling

   protected:
      void set_bundle(const lList *bundle) override;

   public:
      [[nodiscard]] lList *get_bundle() override;
#pragma endregion

#pragma region Constructor/Destructor

   public:
      QConfParameter(const lList *bundle, gdi::Packet *packet);

      /** @brief Build empty parameters, to be filled in by the client
       * @param procedure_name the command being run, `"qconf"`
       */
      explicit QConfParameter(std::string procedure_name)
         : ProcedureParameter(std::move(procedure_name), nullptr) {}

      ~QConfParameter() override = default;
#pragma endregion
   };
}
