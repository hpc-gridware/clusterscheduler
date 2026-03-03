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

#include "ocs_QHostReportHandlerXML.h"

void
ocs::QHostReportHandlerXML::start(std::ostream &os) {
   os << "<?xml version='1.0'?>" << std::endl;
   os << "<qhost xmlns:xsd=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qhost/qhost.xsd\">" << std::endl;
}

void
ocs::QHostReportHandlerXML::end(std::ostream &os) {
   os << "</qhost>" << std::endl;
}

void
ocs::QHostReportHandlerXML::host_start(std::ostream &os, const char* host_name) {
   os << " <host name='" << EscapedString(host_name) << "'>" << std::endl;
}

void
ocs::QHostReportHandlerXML::host_value(std::ostream &os, const char *name, const char *value) {
   os << "   <hostvalue name='" << EscapedString(name) << "'>" << EscapedString(value) << "</hostvalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::host_value(std::ostream &os, const char* name, const u_long32 value) {
   os << "   <hostvalue name='" << EscapedString(name) << "'>" << value << "</hostvalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::host_end(std::ostream &os) {
   os << " </host>" << std::endl;
}

void
ocs::QHostReportHandlerXML::resource_value(std::ostream &os, const char* dominance, const char* name, const char* value) {
   os << "   <resourcevalue name='" << EscapedString(name) << "' dominance='" << EscapedString(dominance) << "'>" << EscapedString(value) << "</resourcevalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::queue_start(std::ostream &os, const char* qname) {
   os << " <queue name='" << ocs::EscapedString(qname) << "'>" << std::endl;
}

void
ocs::QHostReportHandlerXML::queue_value(std::ostream &os, const char* qname, const char* name, const char *value) {
   os << "   <queuevalue qname='" << EscapedString(qname) << "' name='" << EscapedString(name) << "'>" << EscapedString(value) << "</queuevalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::queue_value(std::ostream &os, const char* qname, const char* name, const u_long32 value) {
   os << "   <queuevalue qname='" << EscapedString(qname) << "' name='" << EscapedString(name) << "'>" << value << "</queuevalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::queue_end(std::ostream &os) {
   os << " </queue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::job_start(std::ostream &os, const char* jobname) {
   os << " <job name='" << EscapedString(jobname) << "'>" << std::endl;
}

void
ocs::QHostReportHandlerXML::job_value(std::ostream &os, const char* jobname, const char* name, const char *value) {
   os << "   <jobvalue jobid='" << EscapedString(jobname) << "' name='" << EscapedString(name) << "'>" << EscapedString(value) << "</jobvalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::job_value(std::ostream &os, const char* jobname, const char* name, u_long64 value) {
   os << "   <jobvalue jobid='" << EscapedString(jobname) << "' name='" << EscapedString(name) << "'>" << value << "</jobvalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::job_value(std::ostream &os, const char* jobname, const char* name, double value) {
   os << "   <jobvalue jobid='" << EscapedString(jobname) << "' name='" << EscapedString(name) << "'>" << std::fixed << std::setprecision(6) << value << "</jobvalue>" << std::endl;
}

void
ocs::QHostReportHandlerXML::job_end(std::ostream &os) {
   os << " </job>" << std::endl;
}
