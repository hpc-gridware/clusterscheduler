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
 * @brief Reading and classifying one line of a load sensor's output
 */

#include "uti/sge_io.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_load_sensor_line.h"

/**
 * @brief Read the next line a load sensor wrote and classify it
 *
 * The line is read with ocs_read_line(), so it can have any length (CS-3209); empty lines and the
 * indentation of a line are skipped. A `host:name:value` line is split at the first two colons:
 * host and name must not be empty and must not contain a colon, the value must not be empty and
 * takes the rest of the line, colons included. That is what the former
 * `sscanf("%[^:]:%[^:]:%[^\n]")` accepted, now without fixed-size buffers.
 *
 * @param file the stream the load sensor writes to
 * @param[out] line the line as it was read; unchanged if there was none
 * @param[out] host the host field of a VALUE line; unchanged otherwise
 * @param[out] name the name field of a VALUE line; unchanged otherwise
 * @param[out] value the value field of a VALUE line; unchanged otherwise
 * @return NONE if no line was available, BEGIN for `begin` or `start`, END for `end`, VALUE for
 *         a `host:name:value` line and FORMAT_ERROR for any other line
 *
 * @note MT-NOTE: ocs_ls_read_line() is MT safe
 */
ocs_ls_line_type
ocs_ls_read_line(FILE *file, std::string &line, std::string &host, std::string &name, std::string &value) {
   DENTER(TOP_LAYER);

   if (!ocs_read_line(file, line)) {
      DRETURN(ocs_ls_line_type::NONE);
   }
   if (line == "begin" || line == "start") {
      DRETURN(ocs_ls_line_type::BEGIN);
   }
   if (line == "end") {
      DRETURN(ocs_ls_line_type::END);
   }

   const auto host_end = line.find(':');
   if (host_end == std::string::npos || host_end == 0) {
      DRETURN(ocs_ls_line_type::FORMAT_ERROR);
   }
   const auto name_end = line.find(':', host_end + 1);
   if (name_end == std::string::npos || name_end == host_end + 1 || name_end + 1 == line.size()) {
      DRETURN(ocs_ls_line_type::FORMAT_ERROR);
   }

   host = line.substr(0, host_end);
   name = line.substr(host_end + 1, name_end - host_end - 1);
   value = line.substr(name_end + 1);
   DRETURN(ocs_ls_line_type::VALUE);
}
