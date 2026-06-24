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

#include <sstream>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_EscapedString.h"

#include "ocs_QQuotaViewBase.h"
#include "ocs_QQuotaViewXML.h"


ocs::QQuotaViewXML::QQuotaViewXML(const QQuotaParameter &parameter) : QQuotaViewBase(parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

ocs::QQuotaViewXML::~QQuotaViewXML() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QQuotaViewXML::report_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "<?xml version='1.0'?>\n";
   os << "<qquota_result xmlns=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qquota/qquota.xsd\">\n";
   DRETURN_VOID;
}

void
ocs::QQuotaViewXML::report_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "</qquota_result>\n";
   DRETURN_VOID;
}

void
ocs::QQuotaViewXML::report_limit_rule_begin(std::ostream &os, const char* rqs_name, const char *rule_name) {
   DENTER(TOP_LAYER);
   std::ostringstream oss;
   oss << rqs_name << "/" << rule_name;
   os << " <qquota_rule name='" << EscapedString(oss.str().c_str()) << "'>\n";
   DRETURN_VOID;
}

void
ocs::QQuotaViewXML::report_limit_string_value(std::ostream &os, const char *name, const char *value, const bool exclude) {
   DENTER(TOP_LAYER);
   if (exclude) {
      os << "   <x" << EscapedString(name) << ">";
   } else {
      os << "   <" << EscapedString(name) << ">";
   }
   os << EscapedString(value);
   if (exclude) {
      os << "</x" << EscapedString(name) << ">\n";
   } else {
      os << "</" << EscapedString(name) << ">\n";
   }
   DRETURN_VOID;
}

void
ocs::QQuotaViewXML::report_limit_rule_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << " </qquota_rule>\n";
   DRETURN_VOID;
}

void
ocs::QQuotaViewXML::report_resource_value(std::ostream &os, const char *resource, CEntry::Type /*type*/, uint64_t max, uint64_t used) {
   DENTER(TOP_LAYER);
   os << "   <limit resource='" << EscapedString(resource) << "' ";
   os << "limit='" << max << "'";

   os << " value='" << used << "'";
   os << "/>\n";
   DRETURN_VOID;
}

