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
#include <iomanip>

#include "uti/sge_rmon_macros.h"

#include "qstat/group/ocs_QStatGroupViewJSON.h"

#include "qstat/ocs_QStatParameter.h"

void ocs::QStatGroupViewJSON::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << std::string(indent * 3, ' ') << "{\n";

   indent++;
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n";
   os << std::string(indent * 3, ' ') << "\"$id\": \"https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qstat-cqueue.schema.json\",\n";

   os << std::string(indent * 3, ' ') << "\"queues\": [\n";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatGroupViewJSON::report_finished(std::ostream &os, QStatParameter &parameter) {
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

void ocs::QStatGroupViewJSON::report_cqueue(std::ostream &os, const char* cq_name, Summary *summary, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (!first_queue) {
      os << ",\n";
   } else {
      first_queue = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << "\"" << cq_name << "\",\n";
   os << std::string(indent * 3, ' ') << "\"load\": " << summary->load << ",\n";
   os << std::string(indent * 3, ' ') << "\"used\": " << summary->used << ",\n";
   os << std::string(indent * 3, ' ') << "\"resv\": " << summary->resv << ",\n";
   os << std::string(indent * 3, ' ') << "\"available\": " << summary->available << ",\n";
   os << std::string(indent * 3, ' ') << "\"total\": " << summary->total << ",\n";
   os << std::string(indent * 3, ' ') << "\"temp_disabled\": " << summary->temp_disabled << ",\n";
   os << std::string(indent * 3, ' ') << "\"manual_intervention\": " << summary->manual_intervention;

   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      os << ",\n";
      os << std::string(indent * 3, ' ') << "\"suspend_manual\": " << summary->suspend_manual << ",\n";
      os << std::string(indent * 3, ' ') << "\"suspend_threshold\": " << summary->suspend_threshold << ",\n";
      os << std::string(indent * 3, ' ') << "\"suspend_on_subordinate\": " << summary->suspend_on_subordinate << ",\n";
      os << std::string(indent * 3, ' ') << "\"suspend_calendar\": " << summary->suspend_calendar << ",\n";
      os << std::string(indent * 3, ' ') << "\"unknown\": " << summary->unknown << ",\n";
      os << std::string(indent * 3, ' ') << "\"load_alarm\": " << summary->load_alarm << ",\n";
      os << std::string(indent * 3, ' ') << "\"disabled_manual\": " << summary->disabled_manual << ",\n";
      os << std::string(indent * 3, ' ') << "\"disabled_calendar\": " << summary->disabled_calendar << ",\n";
      os << std::string(indent * 3, ' ') << "\"ambiguous\": " << summary->ambiguous << ",\n";
      os << std::string(indent * 3, ' ') << "\"orphaned\": " << summary->orphaned << ",\n";
      os << std::string(indent * 3, ' ') << "\"error\": " << summary->error << "\n";
   } else {
      os << "\n";
   }

   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}
