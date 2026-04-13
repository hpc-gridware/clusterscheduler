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
#include <format>

#include "uti/sge_rmon_macros.h"

#include "qhost/ocs_QHostViewPlain.h"

#include "msg_clients_common.h"

void
ocs::QHostViewPlain::start(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::end(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::host_start(std::ostream &os, const char* host_name) {
   DENTER(TOP_LAYER);
   if (print_host_header) {
      print_host_header = false;

      os << std::format("{:<23} {:<13.13} {:>4.4} {:>5.5} {:>5.5} {:>5.5} {:>6.6} {:>7.7} {:>7.7} {:>7.7} {:>7.7}\n",
         MSG_HEADER_HOSTNAME, MSG_HEADER_ARCH, MSG_HEADER_NPROC, MSG_HEADER_NSOC, MSG_HEADER_NCOR, MSG_HEADER_NTHR,
         MSG_HEADER_LOAD, MSG_HEADER_MEMTOT, MSG_HEADER_MEMUSE, MSG_HEADER_SWAPTO, MSG_HEADER_SWAPUS)
         << std::string(23 + 13 + 4 + 5 + 5 + 5 + 6 + 7 + 7 + 7 + 7 + 10*1, '-');
   }
   os << "\n";
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::host_end(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::host_value(std::ostream &os, const char *format_str, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   os << std::vformat(format_str, std::make_format_args(value));
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::host_value(std::ostream &os, const char *format_str, const char* name, const uint64_t value) {
   DENTER(TOP_LAYER);
   os << std::vformat(format_str, std::make_format_args(value));
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::host_value(std::ostream &os, const char *format_str, const char* name, double value) {
   DENTER(TOP_LAYER);
   os << std::vformat(format_str, std::make_format_args(value));
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::queue_start(std::ostream &os, const char *format_str, const char *qname) {
   DENTER(TOP_LAYER);
   os << "\n" << std::vformat(format_str, std::make_format_args(qname));
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::queue_end(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const char *value) {
   DENTER(TOP_LAYER);
   os << std::vformat(format_str, std::make_format_args(value));
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const uint32_t value) {
   DENTER(TOP_LAYER);
   os << std::vformat(format_str, std::make_format_args(value));
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::job_start(std::ostream &os, const char *format_str, const uint32_t jid) {
   DENTER(TOP_LAYER);
   if (format_str != nullptr) {
      os << "\n" << std::format("{}", format_str);
   }
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::job_end(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::job_value(std::ostream &os, const uint32_t jid, const char *format_str, const char* name, const char *value) {
   DENTER(TOP_LAYER);
   if (format_str != nullptr) {
      os << std::vformat(format_str, std::make_format_args(value));
   }
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::job_value(std::ostream &os, const uint32_t jid, const char *format_str, const char* name, uint64_t value, bool as_time_stamp) {
   DENTER(TOP_LAYER);
   if (format_str != nullptr) {
      os << std::vformat(format_str, std::make_format_args(value));
   }
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::job_value(std::ostream &os, const uint32_t jid, const char *format_str, const char* name, double value) {
   DENTER(TOP_LAYER);
   if (format_str != nullptr) {
      os << std::vformat(format_str, std::make_format_args(value));
   }
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details, bool as_string) {
   DENTER(TOP_LAYER);
   os << "\n" << std::format("\t{}:{}={}", dominance, name, value);
   if (details != nullptr) {
      os << std::format(" ({})", details);
   }
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::resource_value(std::ostream &os, const char* dominance, const char* name, uint64_t value, const char *details, bool as_string) {
   DENTER(TOP_LAYER);
   os << "\n" << std::format("\t{}:{}={}", dominance, name, value);
   if (details != nullptr) {
      os << std::format(" ({})", details);
   }
   DRETURN_VOID;
}

void
ocs::QHostViewPlain::resource_value(std::ostream &os, const char* dominance, const char* name, double value, const char *details, bool as_string) {
   DENTER(TOP_LAYER);
   os << "\n" << std::format("\t{}:{}={}", dominance, name, value);
   if (details != nullptr) {
      os << std::format(" ({})", details);
   }
   DRETURN_VOID;
}
