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

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_EscapedString.h"

#include "qhost/ocs_QHostViewXML.h"

void
ocs::QHostViewXML::start(std::ostream &os) {
   os << "<?xml version='1.0'?>" << std::endl;
   os << "<qhost xmlns:xsd=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qhost/qhost.xsd\">" << std::endl;
   indent_++;
}

void
ocs::QHostViewXML::end(std::ostream &os) {
   indent_--;
   os << "</qhost>" << std::endl;
}

void
ocs::QHostViewXML::host_start(std::ostream &os, const char* host_name) {
   os << std::string(indent_ * 3, ' ');
   os << "<host name='" << EscapedString(host_name) << "'>" << std::endl;
   indent_++;
}

void
ocs::QHostViewXML::host_end(std::ostream &os) {
   indent_--;
   os << std::string(indent_ * 3, ' ');
   os << "</host>" << std::endl;
}

void
ocs::QHostViewXML::host_value(std::ostream &os, const char *format, const char *name, const char *value) {
   os << std::string(indent_ * 3, ' ');
   os << "<hostvalue name='" << EscapedString(name) << "'>" << EscapedString(value) << "</hostvalue>" << std::endl;
}

void
ocs::QHostViewXML::host_value(std::ostream &os, const char *format_str, const char* name, const u_long32 value) {
   os << std::string(indent_ * 3, ' ');
   os << "<hostvalue name='" << EscapedString(name) << "'>" << value << "</hostvalue>" << std::endl;
}

void
ocs::QHostViewXML::queue_start(std::ostream &os, const char *format_str, const char* qname) {
   os << std::string(indent_ * 3, ' ');
   os << "<queue name='" << EscapedString(qname) << "'>" << std::endl;
   indent_++;
}

void
ocs::QHostViewXML::queue_end(std::ostream &os) {
   indent_--;
   os << std::string(indent_ * 3, ' ');
   os << "</queue>" << std::endl;
}

void
ocs::QHostViewXML::queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const char *value) {
   os << std::string(indent_ * 3, ' ');
   os << "<queuevalue qname='" << EscapedString(qname) << "' name='" << EscapedString(name) << "'>" << EscapedString(value) << "</queuevalue>" << std::endl;
}

void
ocs::QHostViewXML::queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const u_long32 value) {
   os << std::string(indent_ * 3, ' ');
   os << "<queuevalue qname='" << EscapedString(qname) << "' name='" << EscapedString(name) << "'>" << value << "</queuevalue>" << std::endl;
}

void
ocs::QHostViewXML::job_start(std::ostream &os, const char *format_str, const u_long32 jid) {
   os << std::string(indent_ * 3, ' ');
   os << "<job name='" << jid << "'>" << std::endl;
   indent_++;
}

void
ocs::QHostViewXML::job_end(std::ostream &os) {
   indent_--;
   os << std::string(indent_ * 3, ' ');
   os << "</job>" << std::endl;
}

void
ocs::QHostViewXML::job_value(std::ostream &os, const u_long32 jid, const char *format_str, const char* name, const char *value) {
   if (name != nullptr && value != nullptr) {
      os << std::string(indent_ * 3, ' ');
      os << "<jobvalue jobid='" << jid << "' name='" << EscapedString(name) << "'>" << EscapedString(value) << "</jobvalue>" << std::endl;
   }
}

void
ocs::QHostViewXML::job_value(std::ostream &os, const u_long32 jid, const char *format_str, const char* name, const u_long64 value) {
   if (name != nullptr) {
      os << std::string(indent_ * 3, ' ');
      os << "<jobvalue jobid='" << jid << "' name='" << EscapedString(name) << "'>" << value << "</jobvalue>" << std::endl;
   }
}

void
ocs::QHostViewXML::job_value(std::ostream &os, const u_long32 jid, const char *format_str, const char* name, const double value) {
   if (name != nullptr) {
      os << std::string(indent_ * 3, ' ');
      os << "<jobvalue jobid='" << jid << "' name='" << EscapedString(name) << "'>" << std::fixed << std::setprecision(6) << value << "</jobvalue>" << std::endl;
   }
}

void
ocs::QHostViewXML::resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details) {
   if (name != nullptr && value != nullptr) {
      os << std::string(indent_ * 3, ' ');
      os << "<resourcevalue name='" << EscapedString(name) << "' dominance='" << EscapedString(dominance) << "'>" << EscapedString(value) << "</resourcevalue>" << std::endl;
   }
}

