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

#include "sgeobj/sge_cull_xml.h"

#include "ocs_QStatGroupViewXML.h"

ocs::QStatGroupViewXML::QStatGroupViewXML() {
}

void ocs::QStatGroupViewXML::report_started(std::ostream &os, QStatParameter &parameter) {
}

void ocs::QStatGroupViewXML::report_finished(std::ostream &os, QStatParameter &parameter) {
   if (xml_elems != nullptr) {
      lListElem *xml_elem = xml_getHead("job_info", xml_elems, nullptr);
      xml_elems = nullptr;
      lWriteElemXMLTo(xml_elem, os);
      lFreeElem(&xml_elem);
   }
}

void ocs::QStatGroupViewXML::report_cqueue(std::ostream &os, const char* cq_name, Summary *summary, QStatParameter &parameter) {
   lListElem *elem = lCreateElem(XMLE_Type);
   lList *attributes = lCreateList("attributes", XMLE_Type);
   lSetList(elem, XMLE_List, attributes);

   xml_append_Attr_S(attributes, "name", cq_name);
   if (summary->is_load_available) {
      xml_append_Attr_D(attributes, "load", summary->load);
   }
   xml_append_Attr_U(attributes, "used", summary->used);
   xml_append_Attr_U(attributes, "resv", summary->resv);
   xml_append_Attr_U(attributes, "available", summary->available);
   xml_append_Attr_U(attributes, "total", summary->total);
   xml_append_Attr_U(attributes, "temp_disabled", summary->temp_disabled);
   xml_append_Attr_U(attributes, "manual_intervention", summary->manual_intervention);

   if ((parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED) {
      xml_append_Attr_U(attributes, "suspend_manual", summary->suspend_manual);
      xml_append_Attr_U(attributes, "suspend_threshold", summary->suspend_threshold);
      xml_append_Attr_U(attributes, "suspend_on_subordinate", summary->suspend_on_subordinate);
      xml_append_Attr_U(attributes, "suspend_calendar", summary->suspend_calendar);
      xml_append_Attr_U(attributes, "unknown", summary->unknown);
      xml_append_Attr_U(attributes, "load_alarm", summary->load_alarm);
      xml_append_Attr_U(attributes, "disabled_manual", summary->disabled_manual);
      xml_append_Attr_U(attributes, "disabled_calendar", summary->disabled_calendar);
      xml_append_Attr_U(attributes, "ambiguous", summary->ambiguous);
      xml_append_Attr_U(attributes, "orphaned", summary->orphaned);
      xml_append_Attr_U(attributes, "error", summary->error);
   }
   if (elem) {
      if (xml_elems == nullptr){
         xml_elems = lCreateList("cluster_queue_summary", XMLE_Type);
      }
      lAppendElem(xml_elems, elem);
   }
}
