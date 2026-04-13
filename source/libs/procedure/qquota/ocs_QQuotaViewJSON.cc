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

#include <ostream>

#include "ocs_QQuotaViewJSON.h"

#include <ranges>

#include "ocs_QQuotaViewBase.h"

ocs::QQuotaViewJSON::QQuotaViewJSON(const QQuotaParameter &parameter) : QQuotaViewBase(parameter) {
}

ocs::QQuotaViewJSON::~QQuotaViewJSON() = default;

void
ocs::QQuotaViewJSON::report_started(std::ostream &os) {
   // start
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;

   // header
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n";
   os << std::string(indent * 3, ' ') << "\"$id\": \"https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qquota-root.schema.json\",\n";

   // start of quota usage array
   os << std::string(indent * 3, ' ') << "\"quota_usage\": [\n";
   indent++;
}

void
ocs::QQuotaViewJSON::report_finished(std::ostream &os) {
   // terminate last rule if there was one
   if (!first_rule) {
      os << "\n";
   }

   // end of quota array
   indent--;
   os << std::string(indent * 3, ' ') << "]\n";

   // end of json output
   indent--;
   os << std::string(indent * 3, ' ') << "}\n";
}

void
ocs::QQuotaViewJSON::report_limit_rule_begin(std::ostream &os, const char *rqs_name, const char *rule_name) {
   // terminate last rule  if there was one
   if (!first_rule) {
      os << ",\n";
   } else {
      first_rule = false;
   }

   // start of rule
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;

   // rule attributes
   os << std::string(indent * 3, ' ') << "\"rqs_name\": " << raw2quotedJSON(rqs_name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"rule_name\": " << raw2quotedJSON(rule_name);
}

void
ocs::QQuotaViewJSON::report_limit_rule_finished(std::ostream &os) {
   // terminate still open filter
   if (!last_filter_name.empty()) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "]\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }
   last_filter_name.clear();
}

void
ocs::QQuotaViewJSON::report_limit_string_value(std::ostream &os, const char *filter_name, const char *value, const bool exclude) {

   if (last_filter_name == filter_name) {
      // if we have the same filer as previously, then just append the name to the array
      os << ",\n";
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;
      os << std::string(indent * 3, ' ') << "\"exclude\": " << (exclude ? "true" : "false") << ",\n";
      os << std::string(indent * 3, ' ') << "\"name\": \"" << value << "\"\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
   } else {
      // new filter then close the last one if we had one
      if (!last_filter_name.empty()) {
         os << "\n";
         indent--;
         os << std::string(indent * 3, ' ') << "]";

      }
      os << ",\n";
      os << std::string(indent * 3, ' ') << "\"" << filter_name << "\": [\n";
      indent++;
      os << std::string(indent * 3, ' ') << "{\n";
      indent++;
      os << std::string(indent * 3, ' ') << "\"exclude\": " << (exclude ? "true" : "false") << ",\n";
      os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(value) << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
   }
   last_filter_name = filter_name;
}

void
ocs::QQuotaViewJSON::report_resource_value(std::ostream &os, const char* resource, const uint64_t max, const uint64_t used) {
   // show the resource and the max limit.
   // `used` is optional. not available for static limits (non-consumables)
   os << ",\n";
   os << std::string(indent * 3, ' ') << "\"resource\": " << raw2quotedJSON(resource) << ",\n";
   if (used != 0) {
      os << std::string(indent * 3, ' ') << "\"used\": " << used << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"max\": " << max;
}

