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
#include <sstream>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_advance_reservation.h"

#include "qrstat/ocs_QRStatViewPlain.h"

#include <format>

ocs::QRStatViewPlain::QRStatViewPlain(QRStatParameter &parameter) : QRStatViewBase(parameter) {
};

void
ocs::QRStatViewPlain::report_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_ar(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << "----------";
      os << "----------";
      os << "----------";
      os << "----------";
      os << "----------";
      os << "----------";
      os << "----------";
      os << "----------" << "\n";
   } else if (!header_printed) {
      std::ostringstream oss;
      oss << std::format("{:<9.9} ", "ar-id")
          << std::format("{:<10.10} ", "name")
          << std::format("{:<12.12} ", "owner")
          << std::format("{:<5.5} ", "state")
          << std::format("{:<19.19} ", "start-at")
          << std::format("{:<19.19} ", "end-at")
          << std::format("{:<12.12}", "duration");

      const size_t length = oss.str().length();
      os << oss.str() << "\n"
         << std::string(length, '-') << "\n";

      header_printed = true;
   }

   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_ar(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_ar_node_ulong(std::ostream &os, const char *name, u_long32 value) {
   DENTER(TOP_LAYER);

   if (show_summary) {
      os << std::format("{:9} ", value);
   } else {
      os << std::format("{:<30.30} ", name) << value << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_unknown_ar(std::ostream &os) {
#if 0
   DENTER(TOP_LAYER);
   if (!header_printed) {
      fprintf(stdout, "Following advance reservations do not exist:\n");
   }
   DRETURN_VOID;
#endif
}

void
ocs::QRStatViewPlain::report_finish_unknown_ar(std::ostream &os) {
#if 0
   DENTER(TOP_LAYER);
   DRETURN_VOID;
#endif
}

void
ocs::QRStatViewPlain::report_ar_node_ulong_unknown(std::ostream &os, const char *name, u_long32 value) {
#if 0
   DENTER(TOP_LAYER);

   if (header_printed) {
      fprintf(stdout, ", ");
   } else {
      header_printed = true;
   }
   fprintf(stdout, sge_u32, value);
   DRETURN_VOID;
#endif
}

void
ocs::QRStatViewPlain::report_ar_node_duration(std::ostream &os, const char *name, u_long64 value) {
   DENTER(TOP_LAYER);
   const u_long32 value32 = sge_gmt64_to_gmt32(value);

   const int days    = value32 / 86400;
   const int hours   = (value32 / 3600) % 24;
   const int minutes = (value32 / 60) % 60;
   const int seconds = value32 % 60;

   if (!show_summary) {
      os << std::format("{:<30} ", name ? name : "");
   }
   if (days > 0) {
      os << std::format("{}:{:02}:{:02}:{:02}", days, hours, minutes, seconds);
   } else {
      os << std::format("{:02}:{:02}:{:02}", hours, minutes, seconds);
   }
   if (!show_summary) {
      os << "\n";
   }


   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_ar_node_string(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);

   if (value == nullptr) {
      value = "";
   }
   if (show_summary) {
      if (strcmp("owner", name) == 0) {
         os << std::format("{:<12.12} ", value);
      } else if (strcmp("name", name) == 0) {
         os << std::format("{:<10.10} ", value);
      } else if (strcmp("message", name) == 0) {
         os << "\n       " << value;
      }
   } else {
      os << std::format("{:<30.30} ", name) << value << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_ar_node_time(std::ostream &os, const char *name, u_long64 value) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(time_string, 64);

   if (show_summary) {
      if (strcmp("start_time", name) == 0 || strcmp("end_time", name) == 0) {
         sge_ctime64_short(value, &time_string);
         os << std::format("{:<19.19} ", sge_dstring_get_string(&time_string));
      }
   } else {
      sge_ctime64(value, &time_string);
      os << std::format("{:<30.30} ", name) << sge_dstring_get_string(&time_string) << "\n";
   }

   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_ar_node_state(std::ostream &os, const char *name, u_long32 state) {
   DENTER(TOP_LAYER);

   dstring state_string = DSTRING_INIT;
   ar_state2dstring(static_cast<ar_state_t>(state), &state_string);
   if (show_summary) {
      os << std::format("{:<5.5} ", sge_dstring_get_string(&state_string));
   } else {
      os << std::format("{:<30.30} ", name) << sge_dstring_get_string(&state_string) << "\n";
   }
   sge_dstring_free(&state_string);
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_resource_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << std::format("{:<30.30} ", "resource_list");
      first_resource = true;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_resource_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_resource_list_node(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << (first_resource ? "" : ",") <<  name << "=" << value;
      if (first_resource) {
         first_resource = false;
      }
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_ar_node_boolean(std::ostream &os, const char *name, bool value) {
   DENTER(TOP_LAYER);
   const char* chvalue = value ? "true" : "false";

   if (show_summary) {
      os << "       " << chvalue;
   } else {
      os << std::format("{:<30.30} ", name) << chvalue << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_exec_queue_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << std::format("{:<30.30} ", "exec_queue_list");
      first_exec_queue = true;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_exec_queue_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_exec_queue_list_node(std::ostream &os, const char *name, u_long32 value) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << (first_exec_queue ? "" : ",") << name << "=" << value;
      if (first_exec_queue) {
         first_exec_queue = false;
      }
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_exec_binding_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << std::format("{:<30.30} ", "exec_binding_list");
      first_exec_queue = true;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_exec_binding_list(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_exec_binding_list_node(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << (first_exec_queue ? "" : ", ") << name << "=" << value;
      if (first_exec_queue) {
         first_exec_queue = false;
      }
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_granted_parallel_environment(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << std::format("{:<30.30} ", "granted_parallel_environment");
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_granted_parallel_environment(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_granted_parallel_environment_node(std::ostream &os, const char *name, const char *slots_range) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << name << " slots " << slots_range;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_mail_list(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << std::format("{:<30.30} ", "mail_list");
      first_mail = true;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_mail_list(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_mail_list_node(std::ostream &os, const char *name, const char *host) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << (first_mail ? "" : ",") << (name ? name : "") << "@" << (host ? host : "");
      if (first_mail) {
         first_mail = false;
      }
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_acl_list(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << std::format("{:<30.30} ", "acl_list");
      first_acl = true;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_acl_list(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_acl_list_node(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << (first_acl ? "" : ",") << name;
      if (first_acl) {
         first_acl = false;
      }
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_start_xacl_list(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << std::format("{:<30.30} ", "xacl_list");
      first_xacl = true;
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_finish_xacl_list(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << "\n";
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_xacl_list_node(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (!show_summary) {
      os << (first_xacl ? "" : ",") << name;
      if (first_xacl) {
         first_xacl = false;
      }
   }
   DRETURN_VOID;
}

void
ocs::QRStatViewPlain::report_newline(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}
