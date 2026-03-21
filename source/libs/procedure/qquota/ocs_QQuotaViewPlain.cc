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

#include "uti/sge_rmon_macros.h"

#include "ocs_QQuotaViewBase.h"
#include "ocs_QQuotaViewPlain.h"

#include "msg_clients_common.h"

ocs::QQuotaViewPlain::QQuotaViewPlain(const QQuotaParameter &parameter) : QQuotaViewBase(parameter) {
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
ocs::QQuotaViewPlain::report_limit_rule_begin(std::ostream &os, const char *limit_name) {
   DENTER(TOP_LAYER);
   if (printheader) {
      printheader = false;
      os << std::format("{:<20.20} ", MSG_HEADER_RULE)
         << std::format("{:<20.20} ", MSG_HEADER_LIMIT)
         << MSG_HEADER_FILTER << "\n";
      os << std::string(80, '-') << "\n";
   }

   os << std::format("{:<20.20} ", limit_name);
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_limit_string_value(std::ostream &os, const char *name, const char *value, bool exclude) {
   DENTER(TOP_LAYER);

   if (last_name != name) {
      switched = true;
   }
   if (switched) {
      if (first) {
         first = false;
      } else {
         filter_stream << " ";
      }
      filter_stream << name << " ";
      filter_stream << (exclude ? "!" : "") << value;
      switched = false;
   } else {
      filter_stream << "," << (exclude ? "!" : "") << value;
   }
   last_exclude = exclude;
   last_name = name;
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_limit_rule_finished(std::ostream &os, const char *limit_name) {
   DENTER(TOP_LAYER);
   if (filter_stream.str().empty()) {
      os << "-";
   } else {
      os << filter_stream.str();
      filter_stream.str("");
      last_name = "";
      first = true;
   }
   os << "\n";
   DRETURN_VOID;
}

void
ocs::QQuotaViewPlain::report_resource_value(std::ostream &os, const char *resource, const char *limit, const char *value) {
   DENTER(TOP_LAYER);

   std::ostringstream oss;
   if (value == nullptr) {
      oss << resource << "=" << limit;
   } else {
      oss << resource << "=" << value << "/" <<  limit;
   }
   os << std::format("{:<20.20} ", oss.str());
   DRETURN_VOID;
}


