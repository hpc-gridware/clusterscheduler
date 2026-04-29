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

#include <ostream>
#include <chrono>

#include "uti/sge_rmon_macros.h"

#include "ocs_ProcedureView.h"

#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/cull/sge_centry_CE_L.h"

/** @brief Escapes a string so that it can be printed to JSON output
 */
std::string ocs::ProcedureView::raw2JSON(const std::string &input) {
   DENTER(TOP_LAYER);
   std::string output;

   // rough estimate to avoid multiple reallocations
   output.reserve(input.size() + input.size() / 10);

   for (const unsigned char c: input) {
      switch (c) {
         case '\"': output += "\\\"";
            break;
         case '\\': output += "\\\\";
            break;
         case '\b': output += "\\b";
            break;
         case '\f': output += "\\f";
            break;
         case '\n': output += "\\n";
            break;
         case '\r': output += "\\r";
            break;
         case '\t': output += "\\t";
            break;
         default:
            if (c < 0x20) {
               const auto hex = "0123456789ABCDEF";

               // \u00XX
               output += "\\u00";
               output += hex[(c >> 4) & 0xF];
               output += hex[c & 0xF];
            } else {
               output += static_cast<char>(c);
            }
      }
   }
   DRETURN(output);
}

std::string ocs::ProcedureView::raw2quotedJSON(const std::string &input) {
   DENTER(TOP_LAYER);
   std::string output;
   output.reserve(input.size() + input.size() / 10);
   output += "\"";
   output += raw2JSON(input);
   output += "\"";
   DRETURN(output);
}

/** @brief convert the timestamp to ISO 8601 format
 */
void ocs::ProcedureView::show_ISO_8601_timestamp(std::ostream &os, uint64_t time) {
   DENTER(TOP_LAYER);
   using namespace std::chrono;

   // µs → time_point with µs-resolution
   const auto tp = time_point<system_clock, microseconds>(microseconds{time});

   // Sekunden + Millisekunden
   auto secs = floor<seconds>(tp);
   auto ms = duration_cast<milliseconds>(tp - secs).count();

   // show always 3 digits after the dot and not 6 - otherwise JSON schema verification will fail
   os << std::format("{:%FT%T}.{:03}Z", secs, ms);
   DRETURN_VOID;
}

void ocs::ProcedureView::show_resource_as_JSON_type(std::ostream &os, const lListElem *resource) {
   DENTER(TOP_LAYER);
   // get the dominant value of the resource for this host
   const auto type = static_cast<CEntry::Type>(lGetUlong(resource, CE_valtype));
   const bool as_string = type == CEntry::Type::STR || type == CEntry::Type::CSTR
                          || type == CEntry::Type::HOST || type == CEntry::Type::RESTR;
   const bool as_double = type == CEntry::Type::DOUBLE;
   const bool as_bool = type == CEntry::Type::BOOL;
   if (as_string) {
      os << "\"" << raw2JSON(lGetString(resource, CE_stringval)) << "\"";
   } else if (as_double) {
      os << lGetDouble(resource, CE_doubleval);
   } else if (as_bool) {
      os << (lGetDouble(resource, CE_doubleval) > 0.0 ? "true" : "false");
   } else {
      os << static_cast<uint64_t>(lGetDouble(resource, CE_doubleval));
   }
   DRETURN_VOID;
}

void ocs::ProcedureView::show(std::ostream &os, const char *output) {
   DENTER(TOP_LAYER);
   os << output;
   DRETURN_VOID;
}
