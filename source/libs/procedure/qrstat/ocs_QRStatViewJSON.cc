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

#include "ocs_QRStatViewJSON.h"

#include <ostream>
#include <sstream>
#include <format>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_advance_reservation.h"

#include "qrstat/ocs_QRStatViewJSON.h"

ocs::QRStatViewJSON::QRStatViewJSON(const QRStatParameter &parameter) : QRStatViewBase(parameter) {
};

void
ocs::QRStatViewJSON::report_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::string(indent * 3, ' ') << "{\n";

   indent++;
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n";
   os << std::string(indent * 3, ' ') << "\"$id\": \"https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qrstat-root.schema.json\",\n";

   os << std::string(indent * 3, ' ') << "\"ars\": [\n";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar) {
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
ocs::QRStatViewJSON::report_ar_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (first_ar) {
      first_ar = false;
   } else {
      os << ",\n";
   }

   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "}";
   first_ar_attr = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_node_ulong(std::ostream &os, const char *name, const uint32_t value) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << value;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_node_duration(std::ostream &os, const char *name, const uint64_t value) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << value / 1000000;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_node_string(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << raw2quotedJSON(value != nullptr ? value : "");
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_node_time(std::ostream &os, const char *name, uint64_t value) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": \"";
   show_ISO_8601_timestamp(os, value);
   os << "\"";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_node_state(std::ostream &os, const char *name, uint32_t state) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   DSTRING_STATIC(state_string, 32);
   ar_state2dstring(static_cast<ar_state_t>(state), &state_string);
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": "
      << raw2quotedJSON(sge_dstring_get_string(&state_string));
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_ar_node_boolean(std::ostream &os, const char *name, bool value) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": " << (value ? "true" : "false");
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_resource_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"resource_list\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_resource_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_resource = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_resource_list_node_str(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   if (!first_resource) {
      os << ",\n";
   } else {
      os << "\n";
      first_resource = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"value\": " << raw2quotedJSON(value) << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_resource_list_node_double(std::ostream &os, const char *name, const double value) {
   DENTER(TOP_LAYER);
   if (!first_resource) {
      os << ",\n";
   } else {
      os << "\n";
      first_resource = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"value\": " << value << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_resource_list_node_uint64(std::ostream &os, const char *name, const uint64_t value) {
   DENTER(TOP_LAYER);
   if (!first_resource) {
      os << ",\n";
   } else {
      os << "\n";
      first_resource = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"value\": " << value << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_resource_list_node_bool(std::ostream &os, const char *name, const bool value) {
   DENTER(TOP_LAYER);
   if (!first_resource) {
      os << ",\n";
   } else {
      os << "\n";
      first_resource = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"value\": " << (value ? "true" : "false") << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_exec_queue_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"exec_queue_list\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_exec_queue_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_queue = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_exec_queue_list_node(std::ostream &os, const char *name, uint32_t value) {
   DENTER(TOP_LAYER);
   if (!first_queue) {
      os << ",\n";
   } else {
      os << "\n";
      first_queue = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"slots\": " << value << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_exec_binding_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"exec_binding_list\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_exec_binding_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_binding = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_exec_binding_list_node(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   if (!first_binding) {
      os << ",\n";
   } else {
      os << "\n";
      first_binding = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"binding\": " << raw2quotedJSON(value) << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_granted_parallel_environment_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"granted_parallel_environment\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_granted_parallel_environment_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_binding = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_granted_parallel_environment_node(std::ostream &os, const char *name, const char *slots_range) {
   DENTER(TOP_LAYER);
   if (!first_binding) {
      os << ",\n";
   } else {
      os << "\n";
      first_binding = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"range\": " << raw2quotedJSON(slots_range) << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_mail_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"mail_list\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_mail_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_mail = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_mail_list_node(std::ostream &os, const char *name, const char *host) {
   DENTER(TOP_LAYER);
   if (!first_mail) {
      os << ",\n";
   } else {
      os << "\n";
      first_mail = false;
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"user\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"host\": " << raw2quotedJSON(host) << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_acl_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"acl_list\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_acl_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_acl = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_acl_list_node(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (!first_acl) {
      os << ",\n";
   } else {
      os << "\n";
      first_acl = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name);
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_xacl_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!first_ar_attr) {
      os << ",\n";
   } else {
      first_ar_attr = false;
   }
   os << std::string(indent * 3, ' ') << "\"xacl_list\": [";
   indent++;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_xacl_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n";
   os << std::string(indent * 3, ' ') << "]";
   first_xacl = true;
   DRETURN_VOID;
}

void
ocs::QRStatViewJSON::report_xacl_list_node(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (!first_xacl) {
      os << ",\n";
   } else {
      os << "\n";
      first_xacl = false;
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name);
   DRETURN_VOID;
}
