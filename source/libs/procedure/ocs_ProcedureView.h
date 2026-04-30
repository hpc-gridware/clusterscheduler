#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

#include "cull/cull.h"

#include "ocs_ProcedureParameter.h"

namespace ocs {

   /** @brief Base view that renders stored procedure output to an ostream.
    *
    * The base implementation writes the model's pre-rendered text unchanged.
    * Subclasses override `show()` to wrap the content in XML or JSON structure,
    * or to perform format-specific post-processing.
    *
    * The static utility methods are shared across all format-specific subclasses.
    *
    * @ingroup libprocedure
    */
   class ProcedureView {
   public:
      /** @brief Escape a raw string for safe embedding in a JSON value.
       *
       * Escapes `"`, `\`, and control characters (including `\n`, `\t`, etc.)
       * according to the JSON specification (RFC 8259).
       */
      static std::string raw2JSON(const std::string& input);

      /** @brief Return @p input as a quoted, JSON-escaped string (including the surrounding `""`). */
      static std::string raw2quotedJSON(const std::string &input);

      /** @brief Write @p time (microseconds since epoch) as an ISO 8601 timestamp to @p os.
       *
       * Format: `YYYY-MM-DDTHH:MM:SS.mmmZ` (millisecond precision, always UTC).
       * Millisecond precision is intentional — JSON schema validators typically
       * reject sub-millisecond fractions.
       */
      static void show_ISO_8601_timestamp(std::ostream &os, uint64_t time);

      /** @brief Write a CULL resource element as its natural JSON type to @p os.
       *
       * String-typed resources (STR, CSTR, HOST, RESTR) are emitted as JSON strings,
       * DOUBLE as a JSON number, BOOL as `true`/`false`, and all others as an integer.
       */
      static void show_resource_as_JSON_type(std::ostream &os, const lListElem *resource);

   public:
      explicit ProcedureView(const ProcedureParameter &parameter) {};
      virtual ~ProcedureView() = default;

      /** @brief Write @p output to @p os.
       *
       * The base implementation writes the string as-is.  Subclasses override
       * this method to add format-specific wrapping (XML root element, JSON object, …).
       *
       * @param os      Destination stream (typically stdout).
       * @param output  Pre-rendered text from the model; may be empty but never null.
       */
      virtual void show(std::ostream &os, const char *output);
   };
}
