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

      // @todo
      lWriteElemXMLTo(xml_elem, stdout, -1);
      lFreeElem(&xml_elem);
   }
}

void ocs::QStatGroupViewXML::report_cqueue(std::ostream &os, const char* cq_name, cqueue_summary_t *summary, QStatParameter &parameter) {

   lListElem *elem = nullptr;
   lList *attributeList = nullptr;
   bool show_states = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) ? true : false;

   elem = lCreateElem(XMLE_Type);
   attributeList = lCreateList("attributes", XMLE_Type);
   lSetList(elem, XMLE_List, attributeList);

   xml_append_Attr_S(attributeList, "name", cq_name);
   if (summary->is_load_available) {
      xml_append_Attr_D(attributeList, "load", summary->load);
   }
   xml_append_Attr_U(attributeList, "used", summary->used);
   xml_append_Attr_U(attributeList, "resv", summary->resv);
   xml_append_Attr_U(attributeList, "available", summary->available);
   xml_append_Attr_U(attributeList, "total", summary->total);
   xml_append_Attr_U(attributeList, "temp_disabled", summary->temp_disabled);
   xml_append_Attr_U(attributeList, "manual_intervention", summary->manual_intervention);
   if (show_states) {
      xml_append_Attr_U(attributeList, "suspend_manual", summary->suspend_manual);
      xml_append_Attr_U(attributeList, "suspend_threshold", summary->suspend_threshold);
      xml_append_Attr_U(attributeList, "suspend_on_subordinate", summary->suspend_on_subordinate);
      xml_append_Attr_U(attributeList, "suspend_calendar", summary->suspend_calendar);
      xml_append_Attr_U(attributeList, "unknown", summary->unknown);
      xml_append_Attr_U(attributeList, "load_alarm", summary->load_alarm);
      xml_append_Attr_U(attributeList, "disabled_manual", summary->disabled_manual);
      xml_append_Attr_U(attributeList, "disabled_calendar", summary->disabled_calendar);
      xml_append_Attr_U(attributeList, "ambiguous", summary->ambiguous);
      xml_append_Attr_U(attributeList, "orphaned", summary->orphaned);
      xml_append_Attr_U(attributeList, "error", summary->error);
   }
   if (elem) {
      if (xml_elems == nullptr){
         xml_elems = lCreateList("cluster_queue_summary", XMLE_Type);
      }
      lAppendElem(xml_elems, elem);
   }
}
