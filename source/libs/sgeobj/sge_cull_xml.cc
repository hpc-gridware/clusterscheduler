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

/** @file
 * @brief Rendering CULL objects as XML, for the clients' `-xml` switch
 *
 * Values are XML escaped as they are added, so callers pass plain text
 * throughout.
 *
 * @see sge_cull_xml.h
 */

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
#include "uti/sge_stdlib.h"

#include "cull/cull_lerrnoP.h"
#include "cull/cull_list.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_EscapedString.h"
#include "sgeobj/sge_cull_xml.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLS.h"
#include "sgeobj/tom/ocs_tom_XMLH.h"

static void lWriteElemXML_(const lListElem *ep, int nesting_level, std::ostream &os);

static void lWriteListXML_(const lList *lp, int nesting_level, std::ostream &os);

static bool lAttributesToString_(const lList *attr_list, dstring *attr);

static void lWriteXMLHead_(const lListElem *ep, int nesting_level, std::ostream &os);

static lListElem *append_Attr_S(lList *attributeList, const char *name, const char *value);

/**
 * @brief Create the root element of an XML document
 *
 * @param name the root element's tag name
 * @param list the elements nested inside it; ownership passes to the document
 * @param attributes the root element's attributes; ownership passes to it
 * @return the new document root
 */
lListElem* xml_getHead(const char *name, lList *list, lList *attributes) {

   auto xml_head = ocs::tom::ElementPtr<ocs::tom::XMLH>::create();

   if (xml_head) {
      
      ocs::tom::set<XMLH_Version>(xml_head, "<?xml version='1.0'?>");
      ocs::tom::set<XMLH_Name>(xml_head, name);
      ocs::tom::set<XMLH_Attribute>(xml_head, attributes);
      ocs::tom::set<XMLH_Element>(xml_head, list);
      xml_addAttribute(*xml_head, "xmlns:xsd", "https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qstat/qstat.xsd");

/* we do not support stylesheets yet */
/*    xml_addStylesheet(xml_head, "xmlns:xsl", "http://www.w3.org/1999/XSL/Transform", "1.0");*/
   }
   
   return xml_head.release();
}

/**
 * @brief Add a stylesheet processing instruction to an XML document
 *
 * @param[in,out] xml_head the document root to add to
 * @param name the stylesheet's type, e.g. `text/xsl`
 * @param url where the stylesheet is found
 * @param version the stylesheet version
 */
void xml_addStylesheet(lListElem *xml_head, const char* name, const char *url, const char *version) {
   auto stylesheet_elem = ocs::tom::ElementPtr<ocs::tom::XMLS>::create();
   lList *stylesheet_list = nullptr;
   
   if (stylesheet_elem) {
      ocs::tom::set<XMLS_Name>(stylesheet_elem, name);
      ocs::tom::set<XMLS_Value>(stylesheet_elem, url);
      ocs::tom::set<XMLS_Version>(stylesheet_elem, version);
      stylesheet_list = ocs::tom::get<XMLH_Stylesheet>(xml_head);
      if (!stylesheet_list) {
         ocs::tom::set<XMLH_Stylesheet>(xml_head, (stylesheet_list = lCreateList("Stylesheet", XMLS_Type)));
      }
      
      lAppendElem(stylesheet_list, std::move(stylesheet_elem));
   }   
}

/**
 * @brief Add a double valued attribute to an XML element
 *
 * @param[in,out] xml_elem the element to add to
 * @param name the attribute name
 * @param value the value, rendered with `%f`
 */
void xml_addAttributeD(lListElem *xml_elem, const char *name, double value){
   DSTRING_STATIC(string, 512);
   xml_addAttribute(xml_elem, name, sge_dstring_sprintf(&string, "%f", value));
}

/**
 * @brief Add a string valued attribute to an XML element
 *
 * The value is XML escaped on the way in, so the caller passes plain text.
 *
 * @param[in,out] xml_elem the element to add to
 * @param name the attribute name
 * @param value the value
 */
void xml_addAttribute(lListElem *xml_elem, const char *name, const char *value){
   DENTER(CULL_LAYER);

   auto attr_elem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   lList *attr_list = nullptr;
   dstring mod_value = DSTRING_INIT;
   bool is_mod_value; 
   is_mod_value = escape_string(value, &mod_value); 
   
   if (attr_elem) {
      ocs::tom::set<XMLA_Name>(attr_elem, name);
      ocs::tom::set<XMLA_Value>(attr_elem, (is_mod_value?sge_dstring_get_string(&mod_value):""));
      if (ocs::tom::has<XMLH_Attribute>(xml_elem)) {   
         attr_list = ocs::tom::get<XMLH_Attribute>(xml_elem);
         if (!attr_list)
            ocs::tom::set<XMLH_Attribute>(xml_elem, (attr_list = lCreateList("Attributes", XMLA_Type)));
      }
      else if (ocs::tom::has<XMLE_Attribute>(xml_elem)) {
         attr_list = ocs::tom::get<XMLE_Attribute>(xml_elem);
         if (!attr_list)
            ocs::tom::set<XMLE_Attribute>(xml_elem, (attr_list = lCreateList("Attributes", XMLA_Type)));
      }
      else {
         sge_dstring_free(&mod_value);
         CRITICAL("xml_addAttribute() called on wrong cull structure");
         ocs::TerminationManager::trigger_abort();
      }
      lAppendElem(attr_list, std::move(attr_elem));
   }
   sge_dstring_free(&mod_value);
   DRETURN_VOID;
}

/**
 * @brief Write a list to a file stream
 *
 * Write a list to a file stream in XML format
 *
 * @param lp list
 * @param nesting_level current nesting level
 * @param fp file stream NOTE: MT-NOTE: is thread save, works only on the objects which are passed in
 */
static void lWriteListXML_(const lList *lp, int nesting_level, std::ostream &os) {
   DENTER(CULL_LAYER);
   char indent[128];
   int i;
   bool is_XML_elem = false;
   dstring attr = DSTRING_INIT;
   bool is_attr = false;


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

   for_each_ep_lv(ep, lp) {
      is_XML_elem = false;
      is_attr = false;

      if (ocs::tom::has<XMLE_Attribute>(ep)) {
         sge_dstring_clear(&attr);
         is_attr = lAttributesToString_(ocs::tom::get<XMLE_Attribute>(ep), &attr);  
         is_XML_elem = true;
      }
      
      if (is_XML_elem && (ocs::tom::get<XMLE_Print>(ep)))  {
         const lListElem *elem = ocs::tom::get<XMLE_Element>(ep);
         if (ocs::tom::get<XMLA_Value>(elem) != nullptr){
            os << indent << "<" << ocs::tom::get<XMLA_Name>(elem) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">";
            os << ocs::tom::get<XMLA_Value>(elem);
            lWriteListXML_(ocs::tom::get<XMLE_List>(ep), nesting_level + 1, os);
            os << "</" << ocs::tom::get<XMLA_Name>(elem) << ">\n";
         } else {
            os << indent << "<" << ocs::tom::get<XMLA_Name>(elem) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">\n";
            lWriteListXML_(ocs::tom::get<XMLE_List>(ep), nesting_level + 1, os);
            os << indent << "</" << ocs::tom::get<XMLA_Name>(elem) << ">\n";
         }
      }
      else {
         const char* listName = lGetListName(lp);
         if (listName == nullptr || strcmp(listName, "No list name specified") == 0 || strcmp(listName, "") == 0){
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

/**
 * @brief Write a element to file stream
 *
 * Write a element to file stream in XML format
 *
 * @param ep element
 * @param[out] os stream to write to
 *
 * @note MT-NOTE: is thread safe, works only on the objects which are passed in
 */
void lWriteElemXMLTo(const lListElem *ep, std::ostream &os) {
   DENTER(CULL_LAYER);
   lWriteElemXML_(ep, 0, os);
   DRETURN_VOID;
}
/**
 * @brief Write an element to a file stream in XML format
 *
 * @param ep element
 * @param[out] fp file stream to write to
 *
 * @note MT-NOTE: is thread safe, works only on the objects which are passed in
 */
void lWriteElemXMLTo(const lListElem *ep, FILE *fp) {
   DENTER(CULL_LAYER);
   std::ostringstream os;
   lWriteElemXML_(ep, 0, os);
   fprintf(fp, "%s", os.str().c_str());
   DRETURN_VOID;
}

/**
 * @brief Write an element as XML
 *
 * An XML head element is written as the document head, an XML element through
 * its attributes, name and value, any other element field by field over the
 * fields that have a value. Empty lists, unset strings and references are left
 * out.
 *
 * @param ep the element
 * @param nesting_level indentation depth
 * @param os the stream to write to
 */
static void lWriteElemXML_(const lListElem *ep, int nesting_level, std::ostream &os) {
   DENTER(CULL_LAYER);

   int i;
   char space[128];
   lList *tlp;
   lListElem *tep;
   const char *str;
   const char *attr_name;
   int max = nesting_level *2;

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

   if (ocs::tom::has<XMLH_Version>(ep)) {   
      lWriteXMLHead_(ep, nesting_level, os);
   } else if (ocs::tom::has<XMLE_Attribute>(ep) ) {
      if (ocs::tom::get<XMLE_Print>(ep)) {
         dstring attr = DSTRING_INIT;
         const lListElem *elem = ocs::tom::get<XMLE_Element>(ep);
         bool is_attr = lAttributesToString_(ocs::tom::get<XMLE_Attribute>(ep), &attr);

         os << space << "<" << ocs::tom::get<XMLA_Name>(elem) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">";
         os << ocs::tom::get<XMLA_Value>(elem);
         lWriteListXML_(ocs::tom::get<XMLE_List>(ep), nesting_level + 1, os);
         os << "</" << ocs::tom::get<XMLA_Name>(elem) << ">\n";

         sge_dstring_free(&attr);
      } else{
         lWriteElemXML_(ocs::tom::get<XMLE_Element>(ep), nesting_level, os);
         lWriteListXML_(ocs::tom::get<XMLE_List>(ep), nesting_level, os);
      }
   } else {  
      for (const auto f : ocs_cull_present_fields(ep->descr)) {
         /* ignore empty lists */
         switch (mt_get_type(ep->descr[f.pos].mt)) {
         case lListT :
            tlp = lGetPosList(ep, f.pos);
            if (lGetNumberOfElem(tlp) == 0)
               continue;
            break;
         case lStringT:
            if (lGetPosString(ep, f.pos) == nullptr)
               continue;
            break;
         case lRefT:
            /* connot print a ref */
            continue;
         }

         attr_name = lNm2Str(ep->descr[f.pos].nm);
         os << space << "<" << attr_name << ">";
         switch (mt_get_type(ep->descr[f.pos].mt)) {
         case lIntT:
            os << lGetPosInt(ep, f.pos);
            break;
         case lUlongT:
            os << lGetPosUlong(ep, f.pos);
            break;
         case lUlong64T:
         {
            uint64_t value = lGetPosUlong64(ep, f.pos);

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
               str = lGetPosString(ep, f.pos);
               if (escape_string(str, &string)) {
                  os << sge_dstring_get_string(&string);
                  sge_dstring_free(&string);
               }
            }
            break;

         case lHostT:
            {
               dstring string = DSTRING_INIT;
               str = lGetPosHost(ep, f.pos);
               if (escape_string(str, &string)) {
                  os << sge_dstring_get_string(&string);
                  sge_dstring_free(&string);
               }
            }
            break;

         case lListT:
            tlp = lGetPosList(ep, f.pos);
            if (tlp) {
               os << "\n";
               lWriteListXML_(tlp, nesting_level + 1, os);
               os << space;
            }
            break;

         case lObjectT:
            tep = lGetPosObject(ep, f.pos);
            if (tep) {
               os << "\n";
               lWriteElemXML_(tep, nesting_level, os);
               os << space;
            }
            break;
         case lDoubleT:
            os << lGetPosDouble(ep, f.pos);
            break;
         case lLongT:
            os << lGetPosLong(ep, f.pos);
            break;
         case lBoolT:
            os << (lGetPosBool(ep, f.pos) ? "true": "false");
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

/**
 * @brief Append an attribute holding a double, rendered with five decimal places
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
lListElem *xml_append_Attr_D(lList *attributeList, const char *name, double value) {
   char buffer[20];
   snprintf(buffer, sizeof(buffer), "%.5f", value);
   return append_Attr_S(attributeList, name, buffer);
}

/**
 * @brief Append an attribute holding a double, rendered in at most eight characters - fixed point below 99999999, otherwise three significant digits
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
lListElem *xml_append_Attr_D8(lList *attributeList, const char *name, double value) {
   char buffer[20];
   if (value > 99999999)
      snprintf(buffer, sizeof(buffer), "%.3g", value);
   else
      snprintf(buffer, sizeof(buffer), "%.0f", value);
   return append_Attr_S(attributeList, name, buffer);
}

/**
 * @brief Append an attribute holding a string, XML escaped
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
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

/**
 * @brief Append an attribute holding an int
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
lListElem *xml_append_Attr_I(lList *attributeList, const char *name, int value) {
   char buffer[20];
   snprintf(buffer, sizeof(buffer), "%d", value);
   return append_Attr_S(attributeList, name, buffer);
}

/**
 * @brief Append an attribute holding an unsigned integer
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
lListElem *xml_append_Attr_U(lList *attributeList, const char *name, uint32_t value) {
   char buffer[20];
   snprintf(buffer, sizeof(buffer), sge_u32, value);
   return append_Attr_S(attributeList, name, buffer);
}

static bool lAttributesToString_(const lList *attr_list, dstring *attr){
   if(attr == nullptr)
      return false;

   if (lGetNumberOfElem(attr_list) == 0){
      return false;
   } else {
      for_each_ep_lv(attr_elem, attr_list) {
         const char *name = ocs::tom::get<XMLA_Name>(attr_elem);
         const char *value = ocs::tom::get<XMLA_Value>(attr_elem);

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
   bool is_attr = lAttributesToString_(ocs::tom::get<XMLH_Attribute>(ep), &attr);

   os << ocs::tom::get<XMLH_Version>(ep) << "\n";

   for (ocs::tom::ConstElement<ocs::tom::XMLS> elem : ocs::tom::get<XMLH_Stylesheet>(ep)) {
      os << "<xsl:stylesheet " << ocs::tom::get<XMLS_Name>(elem) << "=\"" << ocs::tom::get<XMLS_Value>(elem) << "\" version=\"" << ocs::tom::get<XMLS_Version>(elem) << "\">\n";
   }

   const char *name = ocs::tom::get<XMLH_Name>(ep);
   os << "<" << name << " " << (is_attr ? sge_dstring_get_string(&attr) : "") << ">\n";
   lWriteListXML_(ocs::tom::get<XMLH_Element>(ep), nesting_level +1, os);
   os << "</" << name << ">\n";

   sge_dstring_free(&attr);
   DRETURN_VOID;
}

static lListElem *append_Attr_S(lList *attributeList, const char *name, const char *value) {
   lListElem *parent = nullptr;
   if (!value)
      return parent;
   parent = lCreateElem(XMLE_Type);
   if (parent) {
      auto elem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
      if (elem){
         ocs::tom::set<XMLA_Name>(elem, name);
         ocs::tom::set<XMLA_Value>(elem, value);
         ocs::tom::set<XMLE_Element>(parent, std::move(elem));
      }
      ocs::tom::set<XMLE_Print>(parent, true);
      lAppendElem(attributeList, parent);
   }
   return parent;
}

/**
 * @brief Replace the characters XML reserves with their entities
 *
 * @param string the text to escape; nullptr yields false
 * @param[out] target receives the escaped text
 * @return true when something was written
 */
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

