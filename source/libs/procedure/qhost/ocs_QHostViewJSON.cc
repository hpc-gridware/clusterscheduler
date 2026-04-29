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
#include <iomanip>
#include <chrono>

#include "uti/sge_rmon_macros.h"

#include "ocs_QHostViewJSON.h"

void
ocs::QHostViewJSON::start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n"
      << std::string(indent * 3, ' ') << R"("$id": "https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qhost-root.schema.json")";
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::end(std::ostream &os) {
   DENTER(TOP_LAYER);
   // terminate resource list
   if (resource_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      resource_list_open = false;
   }

   // close job objects that are still open
   if (job_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      job_open = false;
   }

   // close job list
   if (job_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      job_list_open = false;
   }

   // close queue objects that are still open
   if (queue_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      queue_open = false;
   }

   // close queue list
   if (queue_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      queue_list_open = false;
   }

   // close host objects that are still open
   if (host_open) {
      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "}";
      indent--;
      host_open = false;
   }

   // close host list
   if (host_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      host_list_open = false;
   }


   // final close object
   os << "\n}\n";
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::host_start(std::ostream &os, const char* host_name) {
   DENTER(TOP_LAYER);
   bool first_host = false;

   // start the host list
   if (!host_list_open) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"hosts\": [\n";
      host_list_open = true;
      first_host = true;
   }

   if (resource_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      resource_list_open = false;
   }

   // close job objects that are still open
   if (job_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      job_open = false;
   }

   // close job list
   if (job_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      job_list_open = false;
   }

   // close queue objects that are still open
   if (queue_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      queue_open = false;
   }

   // close queue list
   if (queue_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      queue_list_open = false;
   }

   // close host objects that are still open
   if (host_open) {
      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "}";
      indent--;
      host_open = false;
   }

   // start a new host object
   indent++;
   if (!first_host) {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   host_open = true;

   // show the hostname as host attribute
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(host_name);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::host_end(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::host_value(std::ostream &os, const char *format_str, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << raw2quotedJSON(value);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::host_value(std::ostream &os, const char *format_str, const char* name, const uint64_t value) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << value;
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::host_value(std::ostream &os, const char *format_str, const char* name, const double value) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << value;
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::queue_start(std::ostream &os, const char *format_str, const char *qname) {
   DENTER(TOP_LAYER);
   bool first_queue = false;

   // terminate resource list
   if (resource_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      resource_list_open = false;
   }

   // start the queue list
   if (!queue_list_open) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"queues\": [\n";
      queue_list_open = true;
      first_queue = true;
   }

   // close job objects that are still open
   if (job_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      job_open = false;
   }

   // close job list
   if (job_list_open) {
      os << "\n" << std::string(indent * 3, ' ') << "]";
      job_list_open = false;
   }

   // close queue that is still open
   if (queue_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      queue_open = false;
   }

   // open a new queue object
   indent++;
   if (!first_queue) {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   queue_open = true;

   // show the queue name as attribute
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(qname);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::queue_end(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const char *value) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << raw2quotedJSON(value);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const uint32_t value) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << value;
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::job_start(std::ostream &os, const char *format_str, const uint32_t jid) {
   DENTER(TOP_LAYER);
   bool first_queue = false;

   // terminate resource list
   if (resource_list_open) {
      os << "\n";
      os << std::string(indent * 3, ' ') << "]";
      resource_list_open = false;
   }

   // start the queue list
   if (!job_list_open) {
      os << ",\n";
      os << std::string(indent * 3, ' ') << "\"jobs\": [\n";
      job_list_open = true;
      first_queue = true;
   }

   // close job that is still open
   if (job_open) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      indent--;
      job_open = false;
   }

   // open a new queue object
   indent++;
   if (!first_queue) {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   job_open = true;

   // show the jobs ID as attribute
   indent++;
   os << std::string(indent * 3, ' ') << "\"id\": " << jid;
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::job_end(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::job_value(std::ostream &os, const uint32_t jid, const char *format_str, const char* name, const char *value) {
   DENTER(TOP_LAYER);
   if (name != nullptr && value != nullptr) {
      os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << raw2quotedJSON(value);
   }
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::job_value(std::ostream &os, const uint32_t jid, const char *format_str, const char* name, const uint64_t value, const bool as_timestamp) {
   DENTER(TOP_LAYER);
   if (name != nullptr) {
      os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": ";
      if (as_timestamp) {
         os << "\"";
         show_ISO_8601_timestamp(os, value);
         os << "\"";
      } else {
         os << value;
      }
   }
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::job_value(std::ostream &os, const uint32_t jid, const char *format_str, const char* name, const double value) {
   DENTER(TOP_LAYER);
   if (name != nullptr) {
      os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << value;
   }
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details, const bool as_string) {
   DENTER(TOP_LAYER);
   bool first_resource = false;

   // start the queue list
   if (!resource_list_open) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"resources\": [\n";
      resource_list_open = true;
      first_resource = true;
   }

   // open a new queue object
   indent++;
   if (!first_resource) {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";

   // show the jobs ID as attribute
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name ? name : "") << ",\n";
   if (as_string) {
      os << std::string(indent * 3, ' ') << "\"value\": " << raw2quotedJSON(value ? value : "") << ",\n";
   } else {
      os << std::string(indent * 3, ' ') << "\"value\": " << value << ",\n";
   }
   if (details != nullptr) {
      os << std::string(indent * 3, ' ') << "\"details\": " << raw2quotedJSON(details ? details : "") << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"dominance\": " << raw2quotedJSON(dominance ? dominance : "") << "\n";

   // close the resource
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   indent--;
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::resource_value(std::ostream &os, const char* dominance, const char* name, const uint64_t value, const char *details, const bool as_string) {
   DENTER(TOP_LAYER);
   bool first_resource = false;

   // start the queue list
   if (!resource_list_open) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"resources\": [\n";
      resource_list_open = true;
      first_resource = true;
   }

   // open a new queue object
   indent++;
   if (!first_resource) {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";

   // show the jobs ID as attribute
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   if (as_string) {
      os << std::string(indent * 3, ' ') << R"("value": ")" << value << "\",\n";
   } else {
      os << std::string(indent * 3, ' ') << "\"value\": " << value << ",\n";
   }
   if (details != nullptr) {
      os << std::string(indent * 3, ' ') << "\"details\": " << raw2quotedJSON(details) << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"dominance\": " << raw2quotedJSON(dominance) << "\n";

   // close the resource
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   indent--;
   DRETURN_VOID;
}

void
ocs::QHostViewJSON::resource_value(std::ostream &os, const char* dominance, const char* name, const double value, const char *details, const bool as_string) {
   DENTER(TOP_LAYER);
   bool first_resource = false;

   // start the queue list
   if (!resource_list_open) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"resources\": [\n";
      resource_list_open = true;
      first_resource = true;
   }

   // open a new queue object
   indent++;
   if (!first_resource) {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";

   // show the jobs ID as attribute
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   if (as_string) {
      os << std::string(indent * 3, ' ') << R"("value": ")" << value << "\",\n";
   } else {
      os << std::string(indent * 3, ' ') << "\"value\": " << value << ",\n";
   }
   if (details != nullptr) {
      os << std::string(indent * 3, ' ') << "\"details\": " << raw2quotedJSON(details) << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"dominance\": " << raw2quotedJSON(dominance) << "\n";

   // close the resource
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   indent--;
   DRETURN_VOID;
}
