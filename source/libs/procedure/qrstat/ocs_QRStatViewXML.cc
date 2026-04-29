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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_advance_reservation.h"

#include "ocs_QRStatParameterClient.h"
#include "qrstat/ocs_QRStatViewXML.h"

ocs::QRStatViewXML::QRStatViewXML(const QRStatParameter &parameter) : QRStatViewBase(parameter) {
}

void
ocs::QRStatViewXML::report_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "<?xml version='1.0'?>\n";
   os << "<qrstat xmlns:xsd=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qrstat/qrstat.xsd\">\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "</qrstat>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "   <ar_summary>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "   </ar_summary>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_node_ulong(std::ostream &os, const char *name, uint32_t value) {
   DENTER(TOP_LAYER);
   os << "      <" << name << ">" << value << "</" << name << ">\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_node_duration(std::ostream &os, const char *name, u_long value) {
   DENTER(TOP_LAYER);
   const uint32_t value32 = sge_gmt64_to_gmt32(value);
   const uint32_t days    = value32 / 86400;
   const uint32_t hours   = (value32 / 3600) % 24;
   const uint32_t minutes = (value32 / 60) % 60;
   const uint32_t seconds = value32 % 60;

   os << "      <" << name << ">";
   if (days > 0) {
      os << std::format("{}:{:02}:{:02}:{:02}", days, hours, minutes, seconds);
   } else {
      os << std::format("{:02}:{:02}:{:02}", hours, minutes, seconds);
   }
   os << "</" << name << ">\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_node_string(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   if (value != nullptr) {
      os << "      <" << name << ">" << value << "</" << name << ">\n";
   } else {
      os << "      <" << name << "/>\n";
   }
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_node_time(std::ostream &os, const char *name, uint64_t value) {
   DENTER(TOP_LAYER);
   DSTRING_STATIC(time_string, 64);
   append_time(value, &time_string, true);
   os << "      <" << name << ">" << sge_dstring_get_string(&time_string) << "</" << name << ">\n";
   sge_dstring_free(&time_string);
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_node_state(std::ostream &os, const char *name, uint32_t state) {
   DENTER(TOP_LAYER);
   dstring state_string = DSTRING_INIT;
   ar_state2dstring((ar_state_t)state, &state_string);
   os << "      <" << name << ">" << sge_dstring_get_string(&state_string) << "</" << name << ">\n";
   sge_dstring_free(&state_string);
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_resource_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <resource_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_resource_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </resource_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_resource_list_node_str(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   os << "         <resource name=\"" << name << "\" type=\"" << value << "\"/>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_resource_list_node_double(std::ostream &os, const char *name, const double value) {
   DENTER(TOP_LAYER);
   os << "         <resource name=\"" << name << "\" type=\"" << value << "\"/>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_resource_list_node_uint64(std::ostream &os, const char *name, const uint64_t value) {
   DENTER(TOP_LAYER);
   os << "         <resource name=\"" << name << "\" type=\"" << value << "\"/>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_resource_list_node_bool(std::ostream &os, const char *name, bool value) {
   DENTER(TOP_LAYER);
   os << "         <resource name=\"" << name << "\" type=\"" << value << "\"/>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_ar_node_boolean(std::ostream &os, const char *name, bool value) {
   DENTER(TOP_LAYER);
   os << "      <" << name << ">" << (value ? "true" : "false") << "</" << name << ">\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_exec_queue_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <exec_queue_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_exec_queue_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </exec_queue_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_exec_queue_list_node(std::ostream &os, const char *name, u_int value) {
   DENTER(TOP_LAYER);
   os << "         <exec_queue><queue_instance>" << name << "</queue_instance><slots>" << value << "</slots></exec_queue>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_exec_binding_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <exec_binding_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_exec_binding_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </exec_binding_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_exec_binding_list_node(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   os << "         <exec_binding><exec_host>" << name << "</exec_host><binding>" << value << "</binding></exec_binding>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_granted_parallel_environment_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <granted_parallel_environment>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_granted_parallel_environment_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </granted_parallel_environment>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_granted_parallel_environment_node(std::ostream &os, const char *name, const char *slots_range) {
   DENTER(TOP_LAYER);
   os << "         <parallel_environment>" << (name != nullptr ? name : "") << "</parallel_environment>\n";
   os << "         <slots>" << (slots_range != nullptr ? slots_range : "") << "</slots>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_mail_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <mail_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_mail_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </mail_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_mail_list_node(std::ostream &os, const char *name, const char *host) {
   DENTER(TOP_LAYER);
   os << "         <mail  user=\"" << name << "\" host=\"" << (host ? host : "nullptr") << "\"/>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_acl_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <acl_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_acl_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </acl_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_acl_list_node(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   os << "         <acl  user=\"" << name << "\"/>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_xacl_list_start(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      <xacl_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_xacl_list_finish(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "      </xacl_list>\n";
   DRETURN_VOID;
}

void ocs::QRStatViewXML::report_xacl_list_node(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   os << "         <acl  user=\"" << name << "\"/>\n";
   DRETURN_VOID;
}
