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
 * Portions of this code are Copyright 2011 Univa Corporation.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/ocs_TerminationManager.h"

#include "cull/cull_lerrnoP.h"
#include "cull/cull_list.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_EscapedString.h"
#include "sgeobj/sge_cull_xml.h"

static void lWriteElemXML_(const lListElem *ep, int nesting_level, std::ostream &os);

static void lWriteListXML_(const lList *lp, int nesting_level, std::ostream &os);

static bool lAttributesToString_(const lList *attr_list, dstring *attr);

static void lWriteXMLHead_(const lListElem *ep, int nesting_level, std::ostream &os);

static lListElem *append_Attr_S(lList *attributeList, const char *name, const char *value);

lListElem* xml_getHead(const char *name, lList *list, lList *attributes) {
   lListElem *xml_head = nullptr;

   xml_head = lCreateElem(XMLH_Type);

   if (xml_head) {
      
      lSetString(xml_head, XMLH_Version, "<?xml version='1.0'?>");
      lSetString(xml_head, XMLH_Name, name);
      lSetList(xml_head, XMLH_Attribute, attributes);
      lSetList(xml_head, XMLH_Element, list);
      xml_addAttribute(xml_head, "xmlns:xsd", "https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qstat/qstat.xsd");

/* we do not support stylesheets yet */
/*    xml_addStylesheet(xml_head, "xmlns:xsl", "http://www.w3.org/1999/XSL/Transform", "1.0");*/
   }
   
   return xml_head;
}

void xml_addStylesheet(lListElem *xml_head, const char* name, const char *url, const char *version) {
   lListElem *stylesheet_elem = lCreateElem(XMLS_Type);
   lList *stylesheet_list = nullptr;
   
   if (stylesheet_elem) {
      lSetString(stylesheet_elem, XMLS_Name, name);
      lSetString(stylesheet_elem, XMLS_Value, url);
      lSetString(stylesheet_elem, XMLS_Version, version);
      stylesheet_list = lGetListRW(xml_head, XMLH_Stylesheet);
      if (!stylesheet_list) {
         lSetList(xml_head, XMLH_Stylesheet, (stylesheet_list = lCreateList("Stylesheet", XMLS_Type)));
      }
      
      lAppendElem(stylesheet_list, stylesheet_elem);
   }   
}

void xml_addAttributeD(lListElem *xml_elem, const char *name, double value){
   DSTRING_STATIC(string, 512);
   xml_addAttribute(xml_elem, name, sge_dstring_sprintf(&string, "%f", value));
}

void xml_addAttribute(lListElem *xml_elem, const char *name, const char *value){
   lListElem *attr_elem = lCreateElem(XMLA_Type);
   lList *attr_list = nullptr;
   dstring mod_value = DSTRING_INIT;
   bool is_mod_value; 
   DENTER(CULL_LAYER);

   is_mod_value = escape_string(value, &mod_value); 
   
   if (attr_elem) {
      lSetString(attr_elem, XMLA_Name, name);
      lSetString(attr_elem, XMLA_Value, (is_mod_value?sge_dstring_get_string(&mod_value):""));
      if (lGetPosViaElem(xml_elem, XMLH_Attribute, SGE_NO_ABORT) != -1) {   
         attr_list = lGetListRW(xml_elem, XMLH_Attribute);
         if (!attr_list)
            lSetList(xml_elem, XMLH_Attribute, (attr_list = lCreateList("Attributes", XMLA_Type)));
      }
      else if (lGetPosViaElem(xml_elem, XMLE_Attribute, SGE_NO_ABORT) != -1) {
         attr_list = lGetListRW(xml_elem, XMLE_Attribute);
         if (!attr_list)
            lSetList(xml_elem, XMLE_Attribute, (attr_list = lCreateList("Attributes", XMLA_Type)));
      }
      else {
         sge_dstring_free(&mod_value);
         CRITICAL("xml_addAttribute() called on wrong cull structure");
         ocs::TerminationManager::trigger_abort();
      }
      lAppendElem(attr_list, attr_elem);
   }
   sge_dstring_free(&mod_value);
   DRETURN_VOID;
}

/****** cull/list/lWriteListXMLTo() **********************************************
*  NAME
*     lWriteListXMLTo() -- Write a list to a file stream 
*
*  SYNOPSIS
*     void lWriteListXMLTo(const lList *lp, FILE *fp) 
*
*  FUNCTION
*     Write a list to a file stream in XML format
*
*  INPUTS
*     const lList *lp   - list 
*     int nesting_level - current nesting level
*     FILE *fp          - file stream 
*
*  NOTE:
*    MT-NOTE: is thread save, works only on the objects which are passed in 
*
*******************************************************************************/
static void lWriteListXML_(const lList *lp, int nesting_level, std::ostream &os)
{
   const lListElem *ep;
   char indent[128];
   int i;
   bool is_XML_elem = false;
   dstring attr = DSTRING_INIT;
   bool is_attr = false;

   DENTER(CULL_LAYER);
   
   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN_VOID;
   }

   {
      int max = nesting_level * 2;
      if (max > 128)
      max = 128;
      for (i = 0; i < max; i++)
         indent[i] = ' ';
      indent[i] = '\0';
   }

   for_each_ep(ep, lp) {
      is_XML_elem = false;
      is_attr = false;

      if (lGetPosViaElem(ep, XMLE_Attribute, SGE_NO_ABORT) != -1) {
         sge_dstring_clear(&attr);
         is_attr = lAttributesToString_(lGetList(ep, XMLE_Attribute), &attr);  
         is_XML_elem = true;
      }
      
      if (is_XML_elem && (lGetBool(ep, XMLE_Print)))  {
         lListElem *elem = lGetObject(ep, XMLE_Element);
         if (lGetString(elem, XMLA_Value) != nullptr){
            os << indent << "<" << lGetString(elem, XMLA_Name) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">";
            os << lGetString(elem, XMLA_Value);
            lWriteListXML_(lGetList(ep, XMLE_List), nesting_level + 1, os);
            os << "</" << lGetString(elem, XMLA_Name) << ">\n";
         } else {
            os << indent << "<" << lGetString(elem, XMLA_Name) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">\n";
            lWriteListXML_(lGetList(ep, XMLE_List), nesting_level + 1, os);
            os << indent << "</" << lGetString(elem, XMLA_Name) << ">\n";
         }
      }
      else {
         const char* listName = lGetListName(lp);
         if (strcmp (listName, "No list name specified") == 0){
            listName = "element";
         }
         os << indent << "<" << listName << (is_attr ? sge_dstring_get_string(&attr) : "") << ">\n";
         lWriteElemXML_(ep, nesting_level + 1, os);
         os << indent << "</" << listName << ">\n";
      }
   }
   sge_dstring_free(&attr);
   DRETURN_VOID;
}

/****** cull/list/lWriteElemXMLTo() **********************************************
*  NAME
*     lWriteElemXMLTo() -- Write a element to file stream 
*
*  SYNOPSIS
*     void lWriteElemXMLTo(const lListElem *ep, FILE *fp) 
*
*  FUNCTION
*     Write a element to file stream in XML format 
*
*  INPUTS
*     const lListElem *ep  - element 
*     FILE *fp             - file stream 
*     int ignore_cull_name - ignore the specified cull name if != -1 
*   
*  NOTE:
*    MT-NOTE: is thread save, works only on the objects which are passed in 
******************************************************************************/
void lWriteElemXMLTo(const lListElem *ep, std::ostream &os) {
   DENTER(CULL_LAYER);
   lWriteElemXML_(ep, 0, os);
   DRETURN_VOID;
}
void lWriteElemXMLTo(const lListElem *ep, FILE *fp) {
   DENTER(CULL_LAYER);
   std::ostringstream os;
   lWriteElemXML_(ep, 0, os);
   fprintf(fp, "%s", os.str().c_str());
   DRETURN_VOID;
}

static void lWriteElemXML_(const lListElem *ep, int nesting_level, std::ostream &os) {
   int i;
   char space[128];
   lList *tlp;
   lListElem *tep;
   const char *str;
   const char *attr_name;
   int max = nesting_level *2;

   DENTER(CULL_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN_VOID;
   }

   if (max > 128) {
      max = 128;
   }

   for (i = 0; i < max; i++) {
      space[i] = ' ';
   }
   space[i] = '\0';

   if (lGetPosViaElem(ep, XMLH_Version, SGE_NO_ABORT) != -1) {   
      lWriteXMLHead_(ep, nesting_level, os);
   } else if (lGetPosViaElem(ep, XMLE_Attribute, SGE_NO_ABORT) !=-1 ) {
      if (lGetBool(ep, XMLE_Print)) {
         dstring attr = DSTRING_INIT;
         lListElem *elem = lGetObject(ep, XMLE_Element);
         bool is_attr = lAttributesToString_(lGetList(ep, XMLE_Attribute), &attr);

         os << space << "<" << lGetString(elem, XMLA_Name) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">";
         os << lGetString(elem, XMLA_Value);
         lWriteListXML_(lGetList(ep, XMLE_List), nesting_level + 1, os);
         os << "</" << lGetString(elem, XMLA_Name) << ">\n";

         sge_dstring_free(&attr);
      } else{
         lWriteElemXML_(lGetObject(ep, XMLE_Element), nesting_level, os);
         lWriteListXML_(lGetList(ep, XMLE_List), nesting_level, os);
      }
   } else {  
      for (i = 0; mt_get_type(ep->descr[i].mt) != lEndT; i++) {
         /* ignore empty lists */
         switch (mt_get_type(ep->descr[i].mt)) {
         case lListT :
            tlp = lGetPosList(ep, i);
            if (lGetNumberOfElem(tlp) == 0)
               continue;
            break;
         case lStringT:
            if (lGetPosString(ep, i) == nullptr)
               continue;
            break;
         case lRefT:
            /* connot print a ref */
            continue;
         }

         attr_name = lNm2Str(ep->descr[i].nm);
         os << space << "<" << attr_name << ">";
         switch (mt_get_type(ep->descr[i].mt)) {
         case lIntT:
            os << lGetPosInt(ep, i);
            break;
         case lUlongT:
            os << lGetPosUlong(ep, i);
            break;
         case lUlong64T:
         {
            u_long64 value = lGetPosUlong64(ep, i);

            // hack: assume it is a timestamp when the attribute name contains "time"
            if (strstr(attr_name, "time") != nullptr) {
               static bool compat = sge_getenv("SGE_QSTAT_SGE_COMPATIBILITY") != nullptr;
               if (compat) {
                  os << (value / 1000000);
               } else {
                  if (value == 0) {
                     os << "0";
                  } else {
                     DSTRING_STATIC(dstr, 128);
                     os << sge_ctime64_xml(value, &dstr);
                  }
               }
            } else {
               os << value;
            }
         }
            break;
         case lStringT:
            {
               dstring string = DSTRING_INIT;
               str = lGetPosString(ep, i);
               if (escape_string(str, &string)) {
                  os << sge_dstring_get_string(&string);
                  sge_dstring_free(&string);
               }
            }
            break;

         case lHostT:
            {
               dstring string = DSTRING_INIT;
               str = lGetPosHost(ep, i);
               if (escape_string(str, &string)) {
                  os << sge_dstring_get_string(&string);
                  sge_dstring_free(&string);
               }
            }
            break;

         case lListT:
            tlp = lGetPosList(ep, i);
            if (tlp) {
               os << "\n";
               lWriteListXML_(tlp, nesting_level + 1, os);
               os << space;
            }
            break;

         case lObjectT:
            tep = lGetPosObject(ep, i);
            if (tep) {
               os << "\n";
               lWriteElemXML_(tep, nesting_level, os);
               os << space;
            }
            break;
         case lFloatT:
            os << lGetPosFloat(ep,  i);
            break;
         case lDoubleT:
            os << lGetPosDouble(ep, i);
            break;
         case lLongT:
            os << lGetPosLong(ep,  i);
            break;
         case lBoolT:
            os << (lGetPosBool(ep, i) ? "true": "false");
            break;
         case lCharT:
            os << lGetPosChar(ep,  i);
            break;
         case lRefT:
            /* cannot be printed */
            break;
         default:
            unknownType("lWriteElem");
         }
         os << "</" << attr_name << ">\n";
      }
   }
   DRETURN_VOID;
}

lListElem *xml_append_Attr_D(lList *attributeList, const char *name, double value) {
   char buffer[20];
   snprintf(buffer, sizeof(buffer), "%.5f", value);
   return append_Attr_S(attributeList, name, buffer);
}

lListElem *xml_append_Attr_D8(lList *attributeList, const char *name, double value) {
   char buffer[20];
   if (value > 99999999)
      snprintf(buffer, sizeof(buffer), "%.3g", value);
   else
      snprintf(buffer, sizeof(buffer), "%.0f", value);
   return append_Attr_S(attributeList, name, buffer);
}

lListElem *xml_append_Attr_S(lList *attributeList, const char *name, const char *value){
   dstring string = DSTRING_INIT;
   lListElem *xml_Elem = nullptr;
   
   if (escape_string(value, &string)) {
      xml_Elem = append_Attr_S(attributeList, name, sge_dstring_get_string(&string));
      sge_dstring_free(&string);
   }
   else
      xml_Elem = append_Attr_S(attributeList, name, "");
         
   return xml_Elem;         
}

lListElem *xml_append_Attr_I(lList *attributeList, const char *name, int value) {
   char buffer[20];
   snprintf(buffer, sizeof(buffer), "%d", value);
   return append_Attr_S(attributeList, name, buffer);
}

lListElem *xml_append_Attr_U(lList *attributeList, const char *name, u_long32 value) {
   char buffer[20];
   snprintf(buffer, sizeof(buffer), sge_u32, value);
   return append_Attr_S(attributeList, name, buffer);
}

static bool lAttributesToString_(const lList *attr_list, dstring *attr){
   const lListElem *attr_elem = nullptr;
   
   if(attr == nullptr)
      return false;

   if (lGetNumberOfElem(attr_list) == 0){
      return false;
   }
   else {
      for_each_ep(attr_elem, attr_list) {
         const char *name = lGetString(attr_elem, XMLA_Name);
         const char *value = lGetString(attr_elem, XMLA_Value);

         sge_dstring_sprintf_append(attr, " %s=\"%s\"", name, value);
      }
   }
   return true;
}

static void lWriteXMLHead_(const lListElem *ep, int nesting_level, std::ostream &os) {
   DENTER(CULL_LAYER);

   if (!ep){
      DRETURN_VOID;
   }

   dstring attr = DSTRING_INIT;
   bool is_attr = lAttributesToString_(lGetList(ep, XMLH_Attribute), &attr);

   os << lGetString(ep, XMLH_Version) << "\n";

   const lListElem *elem = nullptr;
   for_each_ep(elem, lGetList(ep, XMLH_Stylesheet)) {
      os << "<xsl:stylesheet " << lGetString(elem, XMLS_Name) << "=\"" << lGetString(elem, XMLS_Value) << "\" version=\"" << lGetString(elem, XMLS_Version) << "\">\n";
   }

   const char *name = lGetString(ep, XMLH_Name);
   os << "<" << name << " " << (is_attr ? sge_dstring_get_string(&attr) : "") << ">\n";
   lWriteListXML_(lGetList(ep, XMLH_Element), nesting_level +1, os);
   os << "</" << name << ">\n";

   sge_dstring_free(&attr);
   DRETURN_VOID;
}

static lListElem *append_Attr_S(lList *attributeList, const char *name, const char *value) {
   lListElem *elem = nullptr;
   lListElem *parent = nullptr;
   if (!value)
      return parent;
   parent = lCreateElem(XMLE_Type);
   if (parent) {
      elem = lCreateElem(XMLA_Type);
      if (elem){
         lSetString(elem, XMLA_Name, name);
         lSetString(elem, XMLA_Value, value);
         lSetObject(parent, XMLE_Element, elem);
      }
      lSetBool(parent, XMLE_Print, true);
      lAppendElem(attributeList, parent);
   }
   return parent;
}

bool escape_string(const char *string, dstring *target){
   DENTER(CULL_LAYER);

   if (target == nullptr) {
      DPRINTF("no target string in excape_string()\n");
      ocs::TerminationManager::trigger_abort();
   }

   if (string == nullptr){
      DRETURN(false);
   }

   // we use a ostringstream to add the given string
   std::ostringstream oss;
   oss << ocs::EscapedString(string);
   sge_dstring_append(target, oss.str().c_str());
   DRETURN(true);
}

