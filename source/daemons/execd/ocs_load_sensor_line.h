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
#pragma once

/** @file
 * @brief Reading and classifying one line of a load sensor's output
 */

#include <cstdio>
#include <string>

/** @brief What a line a load sensor wrote means in the load sensor protocol */
enum class ocs_ls_line_type {
   NONE,         ///< no line was available
   BEGIN,        ///< `begin` or `start`: a new load report starts
   END,          ///< `end`: the load report is complete
   VALUE,        ///< a `host:name:value` line
   FORMAT_ERROR  ///< any other line
};

/** @brief Read the next line a load sensor wrote and classify it; see the definition */
ocs_ls_line_type
ocs_ls_read_line(FILE *file, std::string &line, std::string &host, std::string &name, std::string &value);
