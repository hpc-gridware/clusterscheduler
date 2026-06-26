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
#include <cfloat>                  // DBL_MAX (unlimited resource value, CS-2318)
#include <limits>

#include "uti/sge.h"               // INFINITY_STR
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
            if (c < 0x20 || c >= 0x80) {
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

std::string ocs::ProcedureView::raw2quotedJSON(const char *input) {
   DENTER(TOP_LAYER);
   // NULL-safe: a NULL server string renders as an empty JSON string instead of
   // constructing std::string(nullptr) (CWE-476).
   DRETURN(raw2quotedJSON(input != nullptr ? std::string(input) : std::string()));
}

/**
 * @brief Test whether @p value is a syntactically valid JSON number.
 *
 * A value may be emitted unquoted into a JSON document only if it is a valid
 * JSON number per RFC 8259: an optional leading `-`, an integer part with no
 * leading zeros, an optional `.`fraction and an optional `e`/`E` exponent. This
 * deliberately rejects tokens that `strtod()` would otherwise accept but that
 * are not valid JSON (`inf`, `nan`, `infinity`, hex floats like `0x1p4`, a
 * leading `+` or `.`, thousands separators), which would corrupt the output if
 * echoed unquoted (CS-2365, CWE-74). Any value for which this returns false must
 * be quoted/escaped instead.
 *
 * @param value the candidate token (NULL/empty returns false)
 * @return true if @p value is a valid JSON number and safe to emit unquoted
 */
bool ocs::ProcedureView::is_JSON_number(const char *value) {
   // grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
   if (value == nullptr || *value == '\0') {
      return false;
   }
   const char *p = value;

   // optional minus (JSON allows no leading '+')
   if (*p == '-') {
      ++p;
   }

   // integer part: "0" alone, or [1-9][0-9]* (no leading zeros)
   if (*p == '0') {
      ++p;
   } else if (*p >= '1' && *p <= '9') {
      while (*p >= '0' && *p <= '9') {
         ++p;
      }
   } else {
      return false;   // no digits, or a leading '.'
   }

   // optional fraction: '.' followed by at least one digit
   if (*p == '.') {
      ++p;
      if (!(*p >= '0' && *p <= '9')) {
         return false;
      }
      while (*p >= '0' && *p <= '9') {
         ++p;
      }
   }

   // optional exponent: [eE] [+-]? digit+
   if (*p == 'e' || *p == 'E') {
      ++p;
      if (*p == '+' || *p == '-') {
         ++p;
      }
      if (!(*p >= '0' && *p <= '9')) {
         return false;
      }
      while (*p >= '0' && *p <= '9') {
         ++p;
      }
   }

   return *p == '\0';   // no trailing characters
}

/**
 * @brief Add two ints, clamping to INT_MAX/INT_MIN instead of overflowing.
 *
 * Signed-integer overflow is undefined behaviour; this returns a saturated
 * result instead (e.g. as a loop bound where an already-saturated value is
 * incremented — CS-2367, CWE-190).
 *
 * @param a first addend
 * @param b second addend
 * @return a + b, clamped to [INT_MIN, INT_MAX]
 */
int ocs::ProcedureView::add_saturating_int(int a, int b) {
   if (b > 0 && a > std::numeric_limits<int>::max() - b) {
      return std::numeric_limits<int>::max();
   }
   if (b < 0 && a < std::numeric_limits<int>::min() - b) {
      return std::numeric_limits<int>::min();
   }
   return a + b;
}

/**
 * @brief Add two uint32_t, clamping to UINT32_MAX instead of wrapping.
 *
 * Unsigned overflow wraps (modular) rather than being UB, but a wrapped total is
 * still wrong; this saturates instead — used for slot-count accumulators that
 * sum across many objects (CS-2368, CWE-190).
 *
 * @param a first addend
 * @param b second addend
 * @return a + b, clamped to UINT32_MAX on overflow
 */
uint32_t ocs::ProcedureView::add_saturating_u32(uint32_t a, uint32_t b) {
   if (a > std::numeric_limits<uint32_t>::max() - b) {
      return std::numeric_limits<uint32_t>::max();
   }
   return a + b;
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
      const char *strval = lGetString(resource, CE_stringval);
      os << "\"" << raw2JSON(strval != nullptr ? strval : "") << "\"";
   } else if (as_bool) {
      os << (lGetDouble(resource, CE_doubleval) > 0.0 ? "true" : "false");
   } else {
      // Numeric resource (DOUBLE, or INT/TIME/MEM/RSMAP). An unlimited value is stored
      // as DBL_MAX; emit it as the string INFINITY_STR (the same token qconf -fmt json
      // uses, see spool_json_write_ce_value()) instead of letting it overflow the
      // uint64_t cast (undefined behaviour) or print 1.79769e+308 (CS-2318).
      const double dval = lGetDouble(resource, CE_doubleval);
      if (dval == DBL_MAX) {
         os << '"' << INFINITY_STR << '"';
      } else if (as_double) {
         os << dval;
      } else {
         os << static_cast<uint64_t>(dval);
      }
   }
   DRETURN_VOID;
}

void ocs::ProcedureView::show(std::ostream &os, const char *output) {
   DENTER(TOP_LAYER);
   os << output;
   DRETURN_VOID;
}
