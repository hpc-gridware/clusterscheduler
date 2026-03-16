/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cull_xml.h"
#include "sgeobj/sge_job.h"

#include "sched/sge_schedd_text.h"

#include "ocs_client_print.h"
#include "ocs_qstat_filter.h"

/* ----------------------- qselect xml handler ------------------------------ */

/* --------------- Cluster Queue Summary To XML Handler -------------------*/

void xml_qstat_show_job_info(lList **list, lList **answer_list, ocs::QStatParameter &parameter) {
   const lListElem *answer = nullptr;
   lListElem *xml_elem = nullptr;
   bool error = false;
   lListElem* mes;
   const lListElem *sme;
   lList *mlp = nullptr;
   lListElem *jid_ulng = nullptr;
   DENTER(TOP_LAYER);

   for_each_ep(answer, *answer_list) {
      if (lGetUlong(answer, AN_status) != STATUS_OK) {
         error = true;
         break;
      }
   }

   if (error) {
      xml_elem = xml_getHead("communication_error", *answer_list, nullptr);
      lWriteElemXMLTo(xml_elem, stdout, 
         (parameter.full_listing_ & QSTAT_DISPLAY_BINDING) == QSTAT_DISPLAY_BINDING ? -1 : JB_binding);
      lFreeElem(&xml_elem);
   }
   else {
      /* need to modify list to display correct message */
      
      sme = lFirst(*list);
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
      
      xml_elem = xml_getHead("message", *list, nullptr);
      lWriteElemXMLTo(xml_elem, stdout, 
         (parameter.full_listing_ & QSTAT_DISPLAY_BINDING) == QSTAT_DISPLAY_BINDING ? -1 : JB_binding);
      lFreeElem(&xml_elem);
      *list = nullptr;
   }

   lFreeList(answer_list);
 
   DRETURN_VOID;
}

void xml_qstat_show_job(lList **job_list, lList **msg_list, lList **answer_list, lList **id_list, ocs::QStatParameter &parameter){
   DENTER(TOP_LAYER);
   const lListElem *answer = nullptr;
   lListElem *xml_elem = nullptr;
   bool error = false;
   bool suppress_binding_data = (parameter.full_listing_ & QSTAT_DISPLAY_BINDING) == QSTAT_DISPLAY_BINDING ? false : true;

   for_each_ep(answer, *answer_list) {
      if (lGetUlong(answer, AN_status) != STATUS_OK) {
         error = true;
         break;
      }
   }

   if (suppress_binding_data) {
      DPRINTF("Data concerning binding will not ne shown\n");
   }

   if (error) {
      xml_elem = xml_getHead("communication_error", *answer_list, nullptr);
      lWriteElemXMLTo(xml_elem, stdout, suppress_binding_data ? JB_binding : -1);
      lFreeElem(&xml_elem);
   } else {
      if (lGetNumberOfElem(*job_list) == 0) {
         xml_elem = xml_getHead("unknown_jobs", *id_list, nullptr);
         lWriteElemXMLTo(xml_elem, stdout, suppress_binding_data ? JB_binding : -1);
         lFreeElem(&xml_elem);
         *id_list = nullptr;
      } else {
         lList *XML_out = lCreateList("detailed_job_info", XMLE_Type);
         lListElem *xmlElem = nullptr;
         lListElem *attrElem = nullptr;
        
         /* add job infos */
         xmlElem = lCreateElem(XMLE_Type);
         attrElem = lCreateElem(XMLA_Type); 
         lSetString(attrElem, XMLA_Name, "djob_info");
         lSetObject(xmlElem, XMLE_Element, attrElem);
         lSetBool(xmlElem, XMLE_Print, true);
         lSetList(xmlElem, XMLE_List, *job_list);
         lAppendElem(XML_out, xmlElem);

         /* add messages */
         xmlElem = lCreateElem(XMLE_Type);
         attrElem = lCreateElem(XMLA_Type);         
         lSetString(attrElem, XMLA_Name, "messages");
         lSetObject(xmlElem, XMLE_Element, attrElem);
         lSetBool(xmlElem, XMLE_Print, true);
         lSetList(xmlElem, XMLE_List, *msg_list);
         lAppendElem(XML_out, xmlElem);
         
         xml_elem = xml_getHead("detailed_job_info", XML_out, nullptr);
         lWriteElemXMLTo(xml_elem, stdout, suppress_binding_data ? JB_binding : -1);
         lFreeElem(&xml_elem);
         *job_list = nullptr;
         *msg_list = nullptr;
      }
   }

   lFreeList(answer_list);
   DRETURN_VOID;
}


