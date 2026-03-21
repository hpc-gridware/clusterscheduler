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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_cull_xml.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_ulong.h"

#include "sched/sge_schedd_text.h"

#include "ocs_QStatJobViewXML.h"

#include "ocs_QStatJobModel.h"

ocs::QStatJobViewXML::QStatJobViewXML() {

}

void
ocs::QStatJobViewXML::report_jobs_and_reasons_with_job_request(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   lList *XML_out = lCreateList("detailed_job_info", XMLE_Type);

   /* add job infos */
   lListElem *xmlElem = lCreateElem(XMLE_Type);
   lListElem *attrElem = lCreateElem(XMLA_Type);
   lSetString(attrElem, XMLA_Name, "djob_info");
   lSetObject(xmlElem, XMLE_Element, attrElem);
   lSetBool(xmlElem, XMLE_Print, true);
   lSetList(xmlElem, XMLE_List, model.jlp);
   lAppendElem(XML_out, xmlElem);

   /* add messages */
   xmlElem = lCreateElem(XMLE_Type);
   attrElem = lCreateElem(XMLA_Type);
   lSetString(attrElem, XMLA_Name, "messages");
   lSetObject(xmlElem, XMLE_Element, attrElem);
   lSetBool(xmlElem, XMLE_Print, true);
   lSetList(xmlElem, XMLE_List, model.ilp);
   lAppendElem(XML_out, xmlElem);

   lListElem *xml_elem = xml_getHead("detailed_job_info", XML_out, nullptr);
   lWriteElemXMLTo(xml_elem, stdout);
   lFreeElem(&xml_elem);
   model.jlp = nullptr;
   model.ilp = nullptr;

   DRETURN_VOID;
}

void ocs::QStatJobViewXML::report_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   lListElem *xml_elem = nullptr;
   lListElem* mes;
   const lListElem *sme;
   lList *mlp = nullptr;
   lListElem *jid_ulng = nullptr;

   /* need to modify list to display correct message */
   sme = lFirst(model.ilp);
   if (sme) {
      mlp = lGetListRW(sme, SME_message_list);
   }
   for_each_rw(mes, mlp) {
      lPSortList (lGetListRW(mes, MES_job_number_list), "I+", ULNG_value);

      for_each_rw(jid_ulng, lGetList(mes, MES_job_number_list)) {
         u_long32 mid;

         mid = lGetUlong(mes, MES_message_number);
         lSetString(mes,MES_message,sge_schedd_text(mid+SCHEDD_INFO_OFFSET));
      }

   }

   /* print out xml info from list */

   xml_elem = xml_getHead("message", model.ilp, nullptr);
   lWriteElemXMLTo(xml_elem, stdout);
   lFreeElem(&xml_elem);
   model.ilp = nullptr;

   DRETURN_VOID;
}
