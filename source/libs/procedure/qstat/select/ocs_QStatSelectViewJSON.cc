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

#include "qstat/select/ocs_QStatSelectViewJSON.h"

ocs::QStatSelectViewJSON::QStatSelectViewJSON(const QStatParameter &parameter) : QStatSelectViewBase(parameter) {
}

void
ocs::QStatSelectViewJSON::report_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::string(indent * 3, ' ') << "{\n";

   indent++;
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n";
   os << std::string(indent * 3, ' ') << "\"$id\": \"https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qselect-root.schema.json\",\n";

   os << std::string(indent * 3, ' ') << "\"queues\": [\n";
   indent++;
   DRETURN_VOID;
}

void
ocs::QStatSelectViewJSON::report_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_queue) {
      os << "\n";
   }
   // final close object
   indent--;
   os << std::string(indent * 3, ' ') << "]\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}\n";
   DRETURN_VOID;
}

void
ocs::QStatSelectViewJSON::report_queue(std::ostream &os, const char* qname) {
   if (!first_queue) {
      os << ",\n";
   } else {
      first_queue = false;
   }
   os << std::string(indent * 3, ' ') << "\"" << qname << "\"";
}
