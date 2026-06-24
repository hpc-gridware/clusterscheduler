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

#include <format>
#include <sstream>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"

#include "sgeobj/sge_ulong.h"

#include "ocs_QQuotaViewBase.h"
#include "ocs_QQuotaViewPlain.h"

#include "msg_clients_common.h"

ocs::QQuotaViewPlain::QQuotaViewPlain(const QQuotaParameter &parameter) : QQuotaViewBase(parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

ocs::QQuotaViewPlain::~QQuotaViewPlain() = default;

void
ocs::QQuotaViewPlain::report_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_limit_rule_begin(std::ostream &os, const char *rqs_name, const char *rule_name) {
   DENTER(TOP_LAYER);
   if (print_header) {
      print_header = false;
      os << std::format("{:<20.20} ", MSG_HEADER_RULE)
         << std::format("{:<20.20} ", MSG_HEADER_LIMIT)
         << MSG_HEADER_FILTER << "\n";
      os << std::string(80, '-') << "\n";
   }

   std::ostringstream oss;
   oss << rqs_name << "/" << rule_name;
   os << std::format("{:<20.20} ", oss.str());
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_limit_string_value(std::ostream &os, const char *name, const char *value, const bool exclude) {
   DENTER(TOP_LAYER);
   if (last_name != name) {
      filter_type_changed = true;
   }
   if (filter_type_changed) {
      if (first_filter_type) {
         first_filter_type = false;
      } else {
         filter_stream << " ";
      }
      filter_stream << name << " ";
      filter_stream << (exclude ? "!" : "") << value;
      filter_type_changed = false;
   } else {
      filter_stream << "," << (exclude ? "!" : "") << value;
   }
   last_exclude = exclude;
   last_name = name;
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_limit_rule_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (filter_stream.str().empty()) {
      os << "-";
   } else {
      os << filter_stream.str();
      filter_stream.str("");
      last_name = "";
      first_filter_type = true;
   }
   os << "\n";
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_resource_value(std::ostream &os, const char *resource, CEntry::Type type, uint64_t max, uint64_t used) {
   DENTER(TOP_LAYER);
   // Format memory and time values human-readably by attribute type (e.g. memory ->
   // "4.000G", time -> "01:00:00"); everything else is a whole uint64, printed as a plain
   // integer - NOT via double_print_to_dstring()'s "%f", which would render slots=10 as
   // "10.000000". Do not truncate the field (the previous "{:<20.20}" cut a large value
   // like 4294967296 to "42949672", making a 4 GiB limit look like ~43 million) (CS-2348).
   auto value_to_dstring = [type](uint64_t v, dstring *out) {
      if (type == CEntry::Type::MEM || type == CEntry::Type::TIME) {
         double_print_to_dstring(static_cast<double>(v), out, type);
      } else {
         sge_dstring_sprintf(out, sge_u64, v);
      }
   };
   DSTRING_STATIC(max_str, 64);
   value_to_dstring(max, &max_str);
   std::ostringstream oss;
   if (used == 0) {
      oss << resource << "=" << sge_dstring_get_string(&max_str);
   } else {
      DSTRING_STATIC(used_str, 64);
      value_to_dstring(used, &used_str);
      oss << resource << "=" << sge_dstring_get_string(&used_str) << "/" << sge_dstring_get_string(&max_str);
   }
   os << std::format("{:<20} ", oss.str());
   DRETURN_VOID;
}


