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

#include "tom/detail/ocs_Errno.h"
#include "tom/ocs_Types.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_EscapedString.h"
#include "sgeobj/sge_cull_xml.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLE.h"
#include "sgeobj/tom/ocs_tom_XMLS.h"
#include "sgeobj/tom/ocs_tom_XMLH.h"

static void lWriteElemXML_(ocs::tom::ConstRawElement ep, int nesting_level, std::ostream &os);

static void lWriteListXML_(ocs::tom::ConstRawList lp, int nesting_level, std::ostream &os);

static bool lAttributesToString_(ocs::tom::ConstList<ocs::tom::XMLA> attr_list, dstring *attr);

static void lWriteXMLHead_(ocs::tom::ConstElement<ocs::tom::XMLH> ep, int nesting_level, std::ostream &os);

static ocs::tom::Element<ocs::tom::XMLE> append_Attr_S(ocs::tom::List<ocs::tom::XMLE> attributeList, const char *name,
                                                       const char *value);

/**
 * @brief Create the root element of an XML document, without the elements nested in it
 *
 * The part of xml_getHead() that does not depend on the type of the nested
 * elements: the version, the name, the attributes and the schema reference.
 *
 * @param name the root element's tag name
 * @param attributes the root element's attributes; the document takes them over
 * @return the new document root
 */
ocs::tom::ElementPtr<ocs::tom::XMLH> ocs_xml_create_head(const char *name,
                                                         ocs::tom::ListPtr<ocs::tom::XMLA> &&attributes) {
   DENTER(CULL_LAYER);
   auto attribute_list = std::move(attributes);
   auto xml_head = ocs::tom::ElementPtr<ocs::tom::XMLH>::create();

   ocs::tom::set<XMLH_Version>(xml_head, "<?xml version='1.0'?>");
   ocs::tom::set<XMLH_Name>(xml_head, name);
   ocs::tom::set<XMLH_Attribute>(xml_head, std::move(attribute_list));
   xml_addAttribute(*xml_head, "xmlns:xsd",
                    "https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/"
                    "schemas/qstat/qstat.xsd");

   DRETURN(xml_head);
}

/**
 * @brief Add a double valued attribute to an XML element
 *
 * @param[in,out] xml_elem the element to add to
 * @param name the attribute name
 * @param value the value, rendered with `%f`
 */
void xml_addAttributeD(const ocs::tom::Element<ocs::tom::XMLE> xml_elem, const char *name, const double value) {
   DENTER(CULL_LAYER);
   DSTRING_STATIC(string, 512);
   xml_addAttribute(xml_elem, name, sge_dstring_sprintf(&string, "%f", value));
   DRETURN_VOID;
}

/**
 * @brief Add a string valued attribute to the attribute list of an XML head or element
 *
 * The value is XML escaped on the way in, so the caller passes plain text.
 * The attribute list is created when the element has none yet. An empty
 * handle aborts, as the former untyped function did for an element of a wrong
 * structure.
 *
 * @tparam F the attribute list field, `XMLH_Attribute` or `XMLE_Attribute`
 * @tparam T the object type of the element, deduced
 * @param[in,out] xml_elem the element to add to
 * @param name the attribute name
 * @param value the value
 */
template <auto F, ocs::tom::ObjectType T>
static void ocs_xml_add_attribute(const ocs::tom::Element<T> xml_elem, const char *name, const char *value) {
   DENTER(CULL_LAYER);

   if (!xml_elem) {
      CRITICAL("xml_addAttribute() called on wrong cull structure");
      ocs::TerminationManager::trigger_abort();
      DRETURN_VOID;
   }

   dstring mod_value = DSTRING_INIT;
   const bool is_mod_value = escape_string(value, &mod_value);

   auto attr_elem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   ocs::tom::set<XMLA_Name>(attr_elem, name);
   ocs::tom::set<XMLA_Value>(attr_elem, (is_mod_value ? sge_dstring_get_string(&mod_value) : ""));
   ocs::tom::get_or_create<F>(xml_elem, "Attributes").push_back(std::move(attr_elem));

   sge_dstring_free(&mod_value);
   DRETURN_VOID;
}

/**
 * @brief Add a string valued attribute to an XML document head
 *
 * The value is XML escaped on the way in, so the caller passes plain text.
 *
 * @param[in,out] xml_elem the document head to add to
 * @param name the attribute name
 * @param value the value
 */
void xml_addAttribute(const ocs::tom::Element<ocs::tom::XMLH> xml_elem, const char *name, const char *value) {
   DENTER(CULL_LAYER);
   ocs_xml_add_attribute<XMLH_Attribute>(xml_elem, name, value);
   DRETURN_VOID;
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
void xml_addAttribute(const ocs::tom::Element<ocs::tom::XMLE> xml_elem, const char *name, const char *value) {
   DENTER(CULL_LAYER);
   ocs_xml_add_attribute<XMLE_Attribute>(xml_elem, name, value);
   DRETURN_VOID;
}

/**
 * @brief Write a list to a stream
 *
 * Write a list to a stream in XML format. The list holds elements of any
 * type; an XML element to be printed is written as its tag, any other element
 * field by field, wrapped into a tag named after the list.
 *
 * @param lp the list, of any type; an empty handle writes nothing
 * @param nesting_level current nesting level
 * @param[out] os the stream to write to
 *
 * @note MT-NOTE: is thread safe, works only on the objects which are passed in
 */
static void lWriteListXML_(const ocs::tom::ConstRawList lp, const int nesting_level, std::ostream &os) {
   DENTER(CULL_LAYER);
   char indent[128];
   int i;
   bool is_XML_elem = false;
   dstring attr = DSTRING_INIT;
   bool is_attr = false;


   if (!lp) {
      ocs::tom::set_lerrno(LELISTNULL);
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

   // the list holds elements of any type; one holding the attribute list of an XML element is written as such
   for (const auto ep : lp) {
      is_XML_elem = false;
      is_attr = false;

      ocs::tom::ConstElement<ocs::tom::XMLE> xml_elem;
      if (ocs::tom::has<XMLE_Attribute>(ep.raw())) {
         xml_elem = ocs::tom::ConstElement<ocs::tom::XMLE>(ep.raw());
         sge_dstring_clear(&attr);
         is_attr = lAttributesToString_(ocs::tom::get<XMLE_Attribute>(xml_elem), &attr);
         is_XML_elem = true;
      }
      
      if (is_XML_elem && (ocs::tom::get<XMLE_Print>(xml_elem)))  {
         // an element to be printed holds the name and the value of its tag
         const ocs::tom::ConstElement<ocs::tom::XMLA> elem(ocs::tom::get<XMLE_Element>(xml_elem));
         if (ocs::tom::get<XMLA_Value>(elem) != nullptr){
            os << indent << "<" << ocs::tom::get<XMLA_Name>(elem) << (is_attr ? sge_dstring_get_string(&attr) : "")
               << ">";
            os << ocs::tom::get<XMLA_Value>(elem);
            lWriteListXML_(ocs::tom::get<XMLE_List>(xml_elem), nesting_level + 1, os);
            os << "</" << ocs::tom::get<XMLA_Name>(elem) << ">\n";
         } else {
            os << indent << "<" << ocs::tom::get<XMLA_Name>(elem) << (is_attr ? sge_dstring_get_string(&attr) : "")
               << ">\n";
            lWriteListXML_(ocs::tom::get<XMLE_List>(xml_elem), nesting_level + 1, os);
            os << indent << "</" << ocs::tom::get<XMLA_Name>(elem) << ">\n";
         }
      }
      else {
         // the name, or "No list name specified" for a list without one
         const char* listName = ocs::tom::list_name(lp);
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
 * @param ep the element, of any type; an empty handle writes nothing
 * @param[out] os stream to write to
 *
 * @note MT-NOTE: is thread safe, works only on the objects which are passed in
 */
void lWriteElemXMLTo(const ocs::tom::ConstRawElement ep, std::ostream &os) {
   DENTER(CULL_LAYER);
   lWriteElemXML_(ep, 0, os);
   DRETURN_VOID;
}
/**
 * @brief Write an element to a file stream in XML format
 *
 * @param ep the element, of any type; an empty handle writes nothing
 * @param[out] fp file stream to write to
 *
 * @note MT-NOTE: is thread safe, works only on the objects which are passed in
 */
void lWriteElemXMLTo(const ocs::tom::ConstRawElement ep, FILE *fp) {
   DENTER(CULL_LAYER);
   std::ostringstream os;
   lWriteElemXML_(ep, 0, os);
   fprintf(fp, "%s", os.str().c_str());
   DRETURN_VOID;
}

/**
 * @brief Write the value of one field of an element that is no XML head or XML element
 *
 * Switches on the field's type from the element's descriptor, so every read
 * matches the field and none throws.
 *
 * @param ep the element, which holds @p nm
 * @param nm the field
 * @param type the field's type in the element's descriptor
 * @param attr_name the field's name, the tag the value is written in
 * @param nesting_level indentation depth of the element
 * @param space the indentation of the element's fields, @p nesting_level times two blanks
 * @param os the stream to write to
 */
static void ocs_xml_write_field_value(const ocs::tom::ConstRawElement ep, const int nm,
                                      const ocs::tom::FieldType type, const char *attr_name,
                                      const int nesting_level, const char *space, std::ostream &os) {
   switch (type) {
      case ocs::tom::FieldType::Int32:
         os << ep.get<std::int32_t>(nm);
         break;
      case ocs::tom::FieldType::Uint32:
         os << ep.get<std::uint32_t>(nm);
         break;
      case ocs::tom::FieldType::Uint64: {
         const auto value = ep.get<std::uint64_t>(nm);

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
      } break;
      case ocs::tom::FieldType::String: {
         dstring string = DSTRING_INIT;
         if (escape_string(ep.get<const char *>(nm), &string)) {
            os << sge_dstring_get_string(&string);
            sge_dstring_free(&string);
         }
      } break;
      case ocs::tom::FieldType::Host: {
         dstring string = DSTRING_INIT;
         if (escape_string(ep.get<ocs::tom::Host>(nm).c_str(), &string)) {
            os << sge_dstring_get_string(&string);
            sge_dstring_free(&string);
         }
      } break;
      case ocs::tom::FieldType::List:
         if (const auto tlp = ep.get<ocs::tom::ConstRawList>(nm)) {
            os << "\n";
            lWriteListXML_(tlp, nesting_level + 1, os);
            os << space;
         }
         break;
      case ocs::tom::FieldType::Object:
         if (const auto tep = ep.get<ocs::tom::ConstRawElement>(nm)) {
            os << "\n";
            lWriteElemXML_(tep, nesting_level, os);
            os << space;
         }
         break;
      case ocs::tom::FieldType::Double:
         os << ep.get<double>(nm);
         break;
      case ocs::tom::FieldType::Int64:
         os << ep.get<std::int64_t>(nm);
         break;
      case ocs::tom::FieldType::Bool:
         os << (ep.get<bool>(nm) ? "true" : "false");
         break;
      case ocs::tom::FieldType::Ref:
         /* cannot be printed */
         break;
   }
}

/**
 * @brief Write an element as XML
 *
 * An XML head element is written as the document head, an XML element through
 * its attributes, name and value, any other element field by field over the
 * fields that have a value. Empty lists, unset strings and references are left
 * out. The field walk is generic: it goes over the fields the element's
 * descriptor holds, in their order, with each field's type from the
 * descriptor and its name from the object model.
 *
 * @param ep the element, of any type; an empty handle writes nothing
 * @param nesting_level indentation depth
 * @param os the stream to write to
 */
static void lWriteElemXML_(const ocs::tom::ConstRawElement ep, const int nesting_level, std::ostream &os) {
   DENTER(CULL_LAYER);

   int i;
   char space[128];
   int max = nesting_level *2;

   if (!ep) {
      ocs::tom::set_lerrno(LEELEMNULL);
      DRETURN_VOID;
   }

   if (max > 128) {
      max = 128;
   }

   for (i = 0; i < max; i++) {
      space[i] = ' ';
   }
   space[i] = '\0';

   // ep is of any type; an element holding the fields of an XML head or element is written as such
   if (ocs::tom::has<XMLH_Version>(ep.raw())) {
      lWriteXMLHead_(ocs::tom::ConstElement<ocs::tom::XMLH>(ep.raw()), nesting_level, os);
   } else if (ocs::tom::has<XMLE_Attribute>(ep.raw())) {
      const ocs::tom::ConstElement<ocs::tom::XMLE> xml_elem(ep.raw());
      if (ocs::tom::get<XMLE_Print>(xml_elem)) {
         dstring attr = DSTRING_INIT;
         // an element to be printed holds the name and the value of its tag
         const ocs::tom::ConstElement<ocs::tom::XMLA> elem(ocs::tom::get<XMLE_Element>(xml_elem));
         const bool is_attr = lAttributesToString_(ocs::tom::get<XMLE_Attribute>(xml_elem), &attr);

         os << space << "<" << ocs::tom::get<XMLA_Name>(elem) << (is_attr ? sge_dstring_get_string(&attr) : "") << ">";
         os << ocs::tom::get<XMLA_Value>(elem);
         lWriteListXML_(ocs::tom::get<XMLE_List>(xml_elem), nesting_level + 1, os);
         os << "</" << ocs::tom::get<XMLA_Name>(elem) << ">\n";

         sge_dstring_free(&attr);
      } else{
         lWriteElemXML_(ocs::tom::get<XMLE_Element>(xml_elem), nesting_level, os);
         lWriteListXML_(ocs::tom::get<XMLE_List>(xml_elem), nesting_level, os);
      }
   } else {
      const lDescr *descr = ep.descr();
      for (const auto f : ocs_cull_present_fields(descr)) {
         const int nm = descr[f.pos].nm;
         const ocs::tom::FieldType type = ocs::tom::field_type_of(descr[f.pos].mt);

         // empty lists, unset strings and references are left out
         if ((type == ocs::tom::FieldType::List && ep.get<ocs::tom::ConstRawList>(nm).empty()) ||
             (type == ocs::tom::FieldType::String && ep.get<const char *>(nm) == nullptr) ||
             type == ocs::tom::FieldType::Ref) {
            continue;
         }

         // a name of the model is a constant, so it outlives the nested writes below
         const char *attr_name = ocs::tom::field_name(nm);
         os << space << "<" << attr_name << ">";
         ocs_xml_write_field_value(ep, nm, type, attr_name, nesting_level, space, os);
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
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_D(const ocs::tom::List<ocs::tom::XMLE> attributeList,
                                                    const char *name, const double value) {
   DENTER(CULL_LAYER);
   char buffer[20];
   snprintf(buffer, sizeof(buffer), "%.5f", value);
   DRETURN(append_Attr_S(attributeList, name, buffer));
}

/**
 * @brief Append an attribute holding a double, rendered in at most eight characters - fixed point below
 *        99999999, otherwise three significant digits
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_D8(const ocs::tom::List<ocs::tom::XMLE> attributeList,
                                                     const char *name, const double value) {
   DENTER(CULL_LAYER);
   char buffer[20];
   if (value > 99999999)
      snprintf(buffer, sizeof(buffer), "%.3g", value);
   else
      snprintf(buffer, sizeof(buffer), "%.0f", value);
   DRETURN(append_Attr_S(attributeList, name, buffer));
}

/**
 * @brief Append an attribute holding a string, XML escaped
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_S(const ocs::tom::List<ocs::tom::XMLE> attributeList,
                                                    const char *name, const char *value) {
   DENTER(CULL_LAYER);
   dstring string = DSTRING_INIT;
   ocs::tom::Element<ocs::tom::XMLE> xml_Elem;
   
   if (escape_string(value, &string)) {
      xml_Elem = append_Attr_S(attributeList, name, sge_dstring_get_string(&string));
      sge_dstring_free(&string);
   } else {
      xml_Elem = append_Attr_S(attributeList, name, "");
   }

   DRETURN(xml_Elem);
}

/**
 * @brief Append an attribute holding an int
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_I(const ocs::tom::List<ocs::tom::XMLE> attributeList,
                                                    const char *name, const int value) {
   DENTER(CULL_LAYER);
   char buffer[20];
   snprintf(buffer, sizeof(buffer), "%d", value);
   DRETURN(append_Attr_S(attributeList, name, buffer));
}

/**
 * @brief Append an attribute holding an unsigned integer
 *
 * @param[in,out] attributeList the attribute list to append to
 * @param name the attribute name
 * @param value the value
 * @return the new attribute element
 */
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_U(const ocs::tom::List<ocs::tom::XMLE> attributeList,
                                                    const char *name, const uint32_t value) {
   DENTER(CULL_LAYER);
   char buffer[20];
   snprintf(buffer, sizeof(buffer), sge_u32, value);
   DRETURN(append_Attr_S(attributeList, name, buffer));
}

/**
 * @brief Render an attribute list as the attributes of an XML tag
 *
 * @param attr_list the attributes; may be an empty handle
 * @param[out] attr receives ` name="value"` per attribute, appended
 * @return true when there was an attribute to render, false for none or a missing @p attr
 */
static bool lAttributesToString_(const ocs::tom::ConstList<ocs::tom::XMLA> attr_list, dstring *attr) {
   DENTER(CULL_LAYER);
   if (attr == nullptr) {
      DRETURN(false);
   }

   if (attr_list.empty()) {
      DRETURN(false);
   }
   for (const auto attr_elem : attr_list) {
      const char *name = ocs::tom::get<XMLA_Name>(attr_elem);
      const char *value = ocs::tom::get<XMLA_Value>(attr_elem);

      sge_dstring_sprintf_append(attr, " %s=\"%s\"", name, value);
   }
   DRETURN(true);
}

/**
 * @brief Write an XML document head, its stylesheets and the elements nested in it
 *
 * @param ep the document head; an empty handle writes nothing
 * @param nesting_level indentation depth of the nested elements, less one
 * @param os the stream to write to
 */
static void lWriteXMLHead_(const ocs::tom::ConstElement<ocs::tom::XMLH> ep, const int nesting_level,
                           std::ostream &os) {
   DENTER(CULL_LAYER);

   if (!ep){
      DRETURN_VOID;
   }

   dstring attr = DSTRING_INIT;
   const bool is_attr = lAttributesToString_(ocs::tom::get<XMLH_Attribute>(ep), &attr);

   os << ocs::tom::get<XMLH_Version>(ep) << "\n";

   for (const auto elem : ocs::tom::get<XMLH_Stylesheet>(ep)) {
      os << "<xsl:stylesheet " << ocs::tom::get<XMLS_Name>(elem) << "=\"" << ocs::tom::get<XMLS_Value>(elem)
         << "\" version=\"" << ocs::tom::get<XMLS_Version>(elem) << "\">\n";
   }

   const char *name = ocs::tom::get<XMLH_Name>(ep);
   os << "<" << name << " " << (is_attr ? sge_dstring_get_string(&attr) : "") << ">\n";
   lWriteListXML_(ocs::tom::get<XMLH_Element>(ep), nesting_level +1, os);
   os << "</" << name << ">\n";

   sge_dstring_free(&attr);
   DRETURN_VOID;
}

/**
 * @brief Append an XML element to be printed, holding a tag name and its value
 *
 * @param[in,out] attributeList the list to append to; an empty handle appends nothing
 * @param name the tag name
 * @param value the value, already escaped; nullptr appends nothing
 * @return the new element, or an empty handle when nothing was appended
 */
static ocs::tom::Element<ocs::tom::XMLE> append_Attr_S(const ocs::tom::List<ocs::tom::XMLE> attributeList,
                                                       const char *name, const char *value) {
   DENTER(CULL_LAYER);
   if (value == nullptr || !attributeList) {
      DRETURN(nullptr);
   }
   auto elem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   ocs::tom::set<XMLA_Name>(elem, name);
   ocs::tom::set<XMLA_Value>(elem, value);

   auto parent = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
   ocs::tom::set<XMLE_Element>(parent, std::move(elem));
   ocs::tom::set<XMLE_Print>(parent, true);
   DRETURN(attributeList.push_back(std::move(parent)));
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

