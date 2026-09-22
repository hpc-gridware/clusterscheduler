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
 * @brief The `qconf` option table and the command line parser (CS-2798)
 */

#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "ocs_QConfParameterClient.h"

/** @brief What every `qconf` switch takes
 *
 * One row per switch, in the order `sge_parse_qconf()` tests them, so that the
 * table can be read next to the option blocks. The arities were read off those
 * blocks; where a block serves several switches they can still differ, which is
 * why the table is keyed by switch and not by block - `-acal` takes an optional
 * calendar name while `-Acal` requires a file name, out of one block.
 *
 * `-ss` appears in two blocks in `sge_parse_qconf()`; the second one is
 * unreachable and the code says so. One row is therefore right, and the first
 * block keeps handling it.
 */
static const qconf_opt_spec qconf_opt_table[] = {
   {"-cb",         0, 0,                  0, true},
   {"-dry",        0, 0,                  0, true},
   {"-f",          0, 0,                  0, true},
   {"-strict",     0, 0,                  0, true},
   {"-fmt",        1, 1,                  0, true},
   {"-fmtval",     1, 1,                  0, true},
   {"-ectx",       1, 1,                  0, true},
   {"-acal",       0, 1,                  0, false},
   {"-Acal",       1, 1,                  0, false},
   {"-ackpt",      0, 1,                  0, false},
   {"-Ackpt",      1, 1,                  0, false},
   {"-ae",         0, 1,                  0, false},
   {"-Ae",         1, 1,                  0, false},
   {"-ah",         1, 1,                  0, false},
   {"-arole",      0, 1,                  0, false},
   {"-Arole",      1, 1,                  0, false},
   {"-arqs",       0, 1,                  0, false},
   {"-Arqs",       0, 1,                  0, false},
   {"-am",         1, 1,                  0, false},
   {"-ao",         1, 1,                  0, false},
   {"-ap",         0, 1,                  0, false},
   {"-Ap",         1, 1,                  0, false},
   {"-auser",      0, 1,                  0, false},
   {"-aprj",       0, 1,                  0, false},
   {"-Auser",      1, 1,                  0, false},
   {"-Aprj",       1, 1,                  0, false},
   {"-as",         1, 1,                  0, false},
   {"-astree",     0, 0,                  0, false},
   {"-Astree",     1, 1,                  0, false},
   {"-astnode",    1, 1,                  0, false},
   {"-mstnode",    1, 1,                  0, false},
   {"-au",         2, 2,                  0, false},
   {"-clearusage", 0, 0,                  0, false},
   {"-dcal",       1, 1,                  0, false},
   {"-Dcal",       1, 1,                  0, false},
   {"-dckpt",      1, 1,                  0, false},
   {"-Dckpt",      1, 1,                  0, false},
   {"-de",         1, 1,                  0, false},
   {"-De",         1, 1,                  0, false},
   {"-dh",         1, 1,                  0, false},
   {"-drole",      1, 1,                  0, false},
   {"-Drole",      1, 1,                  0, false},
   {"-drqs",       1, 1,                  0, false},
   {"-Drqs",       1, 1,                  0, false},
   {"-dm",         1, 1,                  0, false},
   {"-do",         1, 1,                  0, false},
   {"-dp",         1, 1,                  0, false},
   {"-Dp",         1, 1,                  0, false},
   {"-ds",         1, 1,                  0, false},
   {"-du",         2, 2,                  0, false},
   {"-dul",        1, 1,                  0, false},
   {"-duser",      1, 1,                  0, false},
   {"-dprj",       1, 1,                  0, false},
   {"-dstnode",    1, 1,                  0, false},
   {"-dstree",     0, 0,                  0, false},
   {"-help",       0, 0,                  0, false},
   {"-ks",         0, 0,                  0, false},
   {"-km",         0, 0,                  0, false},
   {"-kec",        1, 1,                  4, false},
   {"-at",         1, 1,                  0, false},
   {"-kt",         1, 1,                  0, false},
   {"-k",          0, 1,                  2, false},
   {"-mc",         0, 0,                  0, false},
   {"-mcal",       1, 1,                  0, false},
   {"-Mcal",       1, 1,                  0, false},
   {"-Mc",         0, 1,                  0, false},
   {"-mckpt",      1, 1,                  0, false},
   {"-Mckpt",      1, 1,                  0, false},
   {"-Me",         1, 1,                  0, false},
   {"-me",         1, 1,                  0, false},
   {"-mrole",      1, 1,                  0, false},
   {"-Mrole",      1, 1,                  0, false},
   {"-mrqs",       0, 1,                  0, false},
   {"-Mrqs",       0, 2,                  0, false},
   {"-mp",         1, 1,                  0, false},
   {"-Mp",         1, 1,                  0, false},
   {"-sobjl",      3, 3,                  0, false},
   {"-mattr",      3, QCONF_ARGS_GREEDY,  0, false},
   {"-Mattr",      2, QCONF_ARGS_GREEDY,  0, false},
   {"-aattr",      3, QCONF_ARGS_GREEDY,  0, false},
   {"-Aattr",      2, QCONF_ARGS_GREEDY,  0, false},
   {"-rattr",      3, QCONF_ARGS_GREEDY,  0, false},
   {"-Rattr",      2, QCONF_ARGS_GREEDY,  0, false},
   {"-dattr",      3, QCONF_ARGS_GREEDY,  0, false},
   {"-Dattr",      2, QCONF_ARGS_GREEDY,  0, false},
   {"-purge",      3, QCONF_ARGS_GREEDY,  0, false},
   {"-Msconf",     1, 1,                  0, false},
   {"-msconf",     0, 0,                  0, false},
   {"-mstree",     0, 0,                  0, false},
   {"-Mstree",     1, 1,                  0, false},
   {"-mu",         1, 1,                  0, false},
   {"-Mu",         1, 1,                  0, false},
   {"-Au",         1, 1,                  0, false},
   {"-Du",         1, 1,                  0, false},
   {"-muser",      1, 1,                  0, false},
   {"-mprj",       1, 1,                  0, false},
   {"-Muser",      1, 1,                  0, false},
   {"-Duser",      1, 1,                  0, false},
   {"-Mprj",       1, 1,                  0, false},
   {"-Dprj",       1, 1,                  0, false},
   {"-sc",         0, 0,                  0, false},
   {"-scatl",      0, 0,                  0, false},
   {"-scal",       1, 1,                  0, false},
   {"-scall",      0, 0,                  0, false},
   {"-sconf",      0, 1,                  0, false},
   {"-aconf",      0, 1,                  0, false},
   {"-mconf",      0, 1,                  0, false},
   {"-Mconf",      0, 1,                  0, false},
   {"-Aconf",      0, 1,                  0, false},
   {"-sckpt",      1, 1,                  0, false},
   {"-sckptl",     0, 0,                  0, false},
   {"-sconfl",     0, 0,                  0, false},
   {"-dconf",      0, 1,                  0, false},
   {"-Dconf",      1, 1,                  0, false},
   {"-se",         1, 1,                  0, false},
   {"-secl",       0, 0,                  0, false},
   {"-sel",        0, 0,                  0, false},
   {"-sep",        0, 0,                  0, false},
   {"-sh",         0, 0,                  0, false},
   {"-srqsl",      0, 0,                  0, false},
   {"-srqs",       0, 1,                  0, false},
   {"-stl",        0, 0,                  0, false},
   {"-sm",         0, 0,                  0, false},
   {"-srole",      1, 1,                  0, false},
   {"-srolel",     0, 0,                  0, false},
   {"-sp",         1, 1,                  0, false},
   {"-spl",        0, 0,                  0, false},
   {"-scel",       0, 0,                  0, false},
   {"-so",         0, 0,                  0, false},
   {"-ssconf",     0, 0,                  0, false},
   {"-sstnode",    1, 1,                  0, false},
   {"-rsstnode",   1, 1,                  0, false},
   {"-sstree",     0, 0,                  0, false},
   {"-bonsai",     0, 0,                  0, false},
   {"-sst",        0, 0,                  0, false},
   {"-ss",         0, 0,                  0, false},
   {"-sss",        0, 0,                  0, false},
   {"-su",         1, 1,                  0, false},
   {"-scat",       1, 1,                  0, false},
   {"-sce",        1, 1,                  0, false},
   {"-mce",        1, 1,                  0, false},
   {"-Mce",        1, 1,                  0, false},
   {"-dce",        1, 1,                  0, false},
   {"-Dce",        1, 1,                  0, false},
   {"-ace",        0, 1,                  0, false},
   {"-Ace",        1, 1,                  0, false},
   {"-shgrpl",     0, 0,                  0, false},
   {"-mhgrp",      1, 1,                  0, false},
   {"-Mhgrp",      1, 1,                  0, false},
   {"-ahgrp",      0, 1,                  0, false},
   {"-Ahgrp",      1, 1,                  0, false},
   {"-dhgrp",      1, 1,                  0, false},
   {"-Dhgrp",      1, 1,                  0, false},
   {"-shgrp",      1, 1,                  0, false},
   {"-shgrp_tree", 1, 1,                  0, false},
   {"-shgrp_resolved", 1, 1,                  0, false},
   {"-shgrp_why",  2, 2,                  0, false},
   {"-sql",        0, 0,                  0, false},
   {"-mq",         1, 1,                  0, false},
   {"-Mq",         1, 1,                  0, false},
   {"-aq",         0, 1,                  0, false},
   {"-Aq",         1, 1,                  0, false},
   {"-Scal",       1, 1,                  0, false},
   {"-Sckpt",      1, 1,                  0, false},
   {"-Sce",        1, 1,                  0, false},
   {"-Se",         1, 1,                  0, false},
   {"-Shgrp",      1, 1,                  0, false},
   {"-Sp",         1, 1,                  0, false},
   {"-Sprj",       1, 1,                  0, false},
   {"-Sq",         1, 1,                  0, false},
   {"-Srole",      1, 1,                  0, false},
   {"-Srqs",       1, 1,                  0, false},
   {"-Su",         1, 1,                  0, false},
   {"-Suser",      1, 1,                  0, false},
   {"-Sstree",     1, 1,                  0, false},
   {"-Ssconf",     1, 1,                  0, false},
   {"-Sconf",      1, 1,                  0, false},
   {"-dq",         1, 1,                  0, false},
   {"-Dq",         1, 1,                  0, false},
   {"-sq",         0, QCONF_ARGS_GREEDY,  0, false},
   {"-cq",         1, 1,                  0, false},
   {"-sds",        0, 0,                  0, false},
   {"-suser",      1, 1,                  0, false},
   {"-sprj",       1, 1,                  0, false},
   {"-sul",        0, 0,                  0, false},
   {"-suserl",     0, 0,                  0, false},
   {"-sprjl",      0, 0,                  0, false},
   {"-tsm",        0, 0,                  0, false},
};
/** @brief The number of rows in #qconf_opt_table */
static constexpr size_t qconf_opt_table_size = sizeof(qconf_opt_table) / sizeof(qconf_opt_table[0]);

const qconf_opt_spec *
qconf_opt_lookup(const char *token) {
   if (token == nullptr) {
      return nullptr;
   }
   for (size_t i = 0; i < qconf_opt_table_size; i++) {
      if (strcmp(qconf_opt_table[i].name, token) == 0) {
         return &qconf_opt_table[i];
      }
   }
   for (size_t i = 0; i < qconf_opt_table_size; i++) {
      const qconf_opt_spec &spec = qconf_opt_table[i];
      if (spec.prefix_len > 0 && strncmp(spec.name, token, spec.prefix_len) == 0) {
         return &spec;
      }
   }
   return nullptr;
}

/**
 * @brief True when the next token is missing or looks like a switch
 *
 * The rule the option blocks used through `sge_next_is_an_opt()`, repeated here
 * so that the parser does not depend on the client's static helper. It decides
 * where an optional argument or a greedy tail ends.
 *
 * @param argv the argument vector
 * @param i    the index of the token to look behind
 * @return true when `argv[i + 1]` is absent or starts with `-`
 */
static bool
qconf_next_is_an_opt(char **argv, int i) {
   return argv[i + 1] == nullptr || argv[i + 1][0] == '-';
}

/**
 * @brief Read the invocation-wide switches out of the whole command line
 *
 * A first pass over every token, exactly as the pre-scan of `sge_parse_qconf()`
 * did: it looks at each token on its own and does not consume the arguments of
 * the switches it passes. That is what makes these switches position
 * independent - `qconf -sq all.q -fmt json` renders the queue as JSON although
 * `-fmt` comes last. Doing this with the arity table instead would quietly
 * change that, so it deliberately does not.
 *
 * @param argv      the argument vector, without `argv[0]`
 * @param bad_token set to the offending value when the status is not OK
 * @return OK, or which switch could not be read
 */
QConfParseStatus
ocs::QConfParameterClient::parse_flags(char **argv, std::string &bad_token) {
   DENTER(TOP_LAYER);

   for (int i = 0; argv[i] != nullptr; i++) {
      const char *token = argv[i];
      const char *value = argv[i + 1];

      if (strcmp("-cb", token) == 0) {
         binding_ = true;
      } else if (strcmp("-dry", token) == 0) {
         dry_run_ = true;
      } else if (strcmp("-f", token) == 0) {
         force_ = true;
      } else if (strcmp("-strict", token) == 0) {
         strict_ = true;
      } else if (strcmp("-fmt", token) == 0) {
         if (value == nullptr) {
            DRETURN(QConfParseStatus::NO_FORMAT_VALUE);
         } else if (strcmp(value, "plain") == 0) {
            output_format_ = OutputFormat::PLAIN;
         } else if (strcmp(value, "json") == 0) {
            output_format_ = OutputFormat::JSON;
         } else {
            bad_token = value;
            DRETURN(QConfParseStatus::BAD_FORMAT);
         }
      } else if (strcmp("-fmtval", token) == 0) {
         if (value != nullptr && strcmp(value, "compact") == 0) {
            value_format_ = ValueFormat::COMPACT;
         } else if (value != nullptr && strcmp(value, "numeric") == 0) {
            value_format_ = ValueFormat::NUMERIC;
         } else {
            bad_token = value != nullptr ? value : "";
            DRETURN(QConfParseStatus::BAD_FORMAT_VALUE);
         }
      } else if (strcmp("-ectx", token) == 0) {
         if (value != nullptr && strcmp(value, "client") == 0) {
            exec_context_ = ExecContext::CLIENT;
         } else if (value != nullptr && strcmp(value, "server") == 0) {
            exec_context_ = ExecContext::SERVER;
         } else {
            bad_token = value != nullptr ? value : "";
            DRETURN(QConfParseStatus::BAD_EXEC_CONTEXT);
         }
      }
   }

   DRETURN(QConfParseStatus::OK);
}

/**
 * @brief Turn a `qconf` command line into switches and an ordered operation list
 *
 * Two passes, for the reason given at #parse_flags: the first collects the
 * invocation-wide switches token by token, the second builds the operations,
 * consuming each switch's arguments according to the table.
 *
 * On a malformed switch the parser **stops there** and returns what it has.
 * That is deliberate: the old loop executed as it parsed, so
 * `qconf -sq all.q -dq` printed the queue and only then complained about the
 * missing argument. Handing the caller the operations parsed so far keeps that
 * order intact instead of turning the command line into an all-or-nothing
 * validation.
 *
 * The parser never terminates the process. Like the engines of CS-2796 and
 * CS-2797 it reports and lets the caller decide, because this code runs inside
 * qmaster too, where `sge_exit()` would take the daemon down with it.
 *
 * @param argv      the argument vector, without `argv[0]`
 * @param bad_token set to the offending token when the status is not OK
 * @return OK when the whole line was understood, else why it stopped
 */
QConfParseStatus
ocs::QConfParameterClient::parse_parameters(char **argv, std::string &bad_token) {
   DENTER(TOP_LAYER);

   QConfParseStatus status = parse_flags(argv, bad_token);
   if (status != QConfParseStatus::OK) {
      DRETURN(status);
   }

   for (int i = 0; argv[i] != nullptr; i++) {
      const qconf_opt_spec *spec = qconf_opt_lookup(argv[i]);
      if (spec == nullptr) {
         bad_token = argv[i];
         DRETURN(QConfParseStatus::UNKNOWN_OPTION);
      }

      qconf_op op;
      op.spec = spec;
      op.token = argv[i];

      /* The mandatory arguments. When the command line ends early the operation
       * is still handed over, truncated: the option block then runs into the
       * end of its own slice and produces exactly the message it produced when
       * it walked argv itself - the generic one from sge_parser_get_next(), or
       * its own, as -au and -du have. Reporting from here instead would level
       * those messages. */
      for (int n = 0; n < spec->min_args; n++) {
         if (argv[i + 1] == nullptr) {
            bad_token = op.token;
            if (!spec->invocation_wide) {
               operations_.push_back(std::move(op));
            }
            DRETURN(QConfParseStatus::MISSING_ARGUMENT);
         }
         op.args.emplace_back(argv[++i]);
      }

      /* the optional ones, up to the maximum, as long as no switch follows */
      while (spec->max_args == QCONF_ARGS_GREEDY ||
             static_cast<int>(op.args.size()) < spec->max_args) {
         if (qconf_next_is_an_opt(argv, i)) {
            break;
         }
         op.args.emplace_back(argv[++i]);
      }

      /* the invocation-wide switches were read in the first pass; they are not
       * operations, but their arguments still have to be stepped over */
      if (!spec->invocation_wide) {
         operations_.push_back(std::move(op));
      }
   }

   DRETURN(QConfParseStatus::OK);
}
