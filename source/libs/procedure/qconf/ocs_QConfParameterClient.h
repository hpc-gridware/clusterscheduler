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
 * @brief Turning a `qconf` command line into parameters and an operation list
 */

/** @defgroup qconf_ops The parsed qconf command line
 * @brief What each switch takes, and what was given (CS-2798)
 *
 * `sge_parse_qconf()` used to be one loop that executed every switch the moment
 * it recognised it. A stored procedure call needs the arguments as **data**, so
 * that `ProcedureParameter::get_bundle()` can marshal them into an `SPP_Type`
 * list and qmaster can reconstruct them - which is impossible while parsing and
 * executing are the same pass.
 *
 * This is the parsing half. It knows one thing the old loop only knew
 * implicitly, spread over 163 blocks: **how many arguments each switch takes**.
 * That knowledge lives in one table here.
 *
 * @{
 */

#include <cstdlib>
#include <string>
#include <vector>

#include "ocs_QConfParameter.h"

/** @brief Why parsing stopped */
enum class QConfParseStatus {
   OK,                 ///< the whole command line was understood
   MISSING_ARGUMENT,   ///< a switch was given fewer arguments than it needs
   UNKNOWN_OPTION,     ///< a token is not a switch this client knows
   BAD_FORMAT,         ///< `-fmt` was given something other than `plain` or `json`
   BAD_FORMAT_VALUE,   ///< `-fmtval` was given something other than `compact` or `numeric`
   NO_FORMAT_VALUE,    ///< `-fmt` was given without a value at all
   BAD_EXEC_CONTEXT,   ///< `-ectx` was given something other than `client` or `server`
};

const qconf_opt_spec *qconf_opt_lookup(const char *token);

namespace ocs {
   /** @brief The client side half: parse `argv`, then hand out the operations
    *
    * `qconf` is multi-operation - `qconf -sq a -dq b` is one invocation - while
    * a procedure call is one operation. The ordered list therefore lives here,
    * and the client turns each operation that is to run in the server into a
    * QConfParameter of its own.
    *
    * @ingroup libprocedure
    */
   class QConfParameterClient : public QConfParameter {
#pragma region Data

   protected:
      std::vector<qconf_op> operations_;   ///< in command line order

   public:
      /** @brief The operations the command line asked for
       * @return them, in the order they were typed
       */
      [[nodiscard]] std::vector<qconf_op> &get_operations() { return operations_; }
#pragma endregion

#pragma region Parsing

   protected:
      QConfParseStatus parse_flags(char **argv, std::string &bad_token);

   public:
      QConfParseStatus parse_parameters(char **argv, std::string &bad_token);
#pragma endregion

#pragma region Constructor/Destructor

   public:
      /** @brief Build empty parameters, to be filled in by parse_parameters()
       *
       * The client's environment is captured here, because this is the only
       * place in the call's life where it is the user's environment: from
       * get_bundle() on, the parameters may be rendered inside qmaster.
       *
       * @param procedure_name the command being run, `"qconf"`
       */
      explicit QConfParameterClient(std::string procedure_name)
         : QConfParameter(std::move(procedure_name)) {
         single_line_ = getenv("SGE_SINGLE_LINE") != nullptr;
      }

      ~QConfParameterClient() override = default;
#pragma endregion
   };
}

/** @} */
