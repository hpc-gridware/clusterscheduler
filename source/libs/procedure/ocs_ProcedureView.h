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

/** @file
 * @brief Base of the views: rendering a model as plain text, XML or JSON
 */

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
      static std::string raw2JSON(const std::string& input);

      static std::string raw2quotedJSON(const std::string &input);

      static std::string raw2quotedJSON(const char *input);

      static bool is_JSON_number(const char *value);

      static int add_saturating_int(int a, int b);

      static uint32_t add_saturating_u32(uint32_t a, uint32_t b);

      static void show_ISO_8601_timestamp(std::ostream &os, uint64_t time);

      static void show_resource_as_JSON_type(std::ostream &os, const lListElem *resource);

   public:
      /** @brief Build a view for one procedure call
       * @param parameter the call's parameters; the base ignores them, subclasses
       *        use them to pick the output format
       */
      explicit ProcedureView(const ProcedureParameter &parameter) {};
      virtual ~ProcedureView() = default;

      virtual void show(std::ostream &os, const char *output);
   };
}
