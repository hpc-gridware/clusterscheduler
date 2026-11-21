#pragma once
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

/** @file
 * @brief Declarations for rendering CULL objects as XML
 *
 * @see sge_cull_xml.cc
 */

#include <cstdio>
#include <ostream>
#include <utility>

#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLE.h"
#include "sgeobj/tom/ocs_tom_XMLH.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

#include <cinttypes>

void lWriteElemXMLTo(ocs::tom::ConstRawElement ep, FILE *fp);
void lWriteElemXMLTo(ocs::tom::ConstRawElement ep, std::ostream &os);

ocs::tom::ElementPtr<ocs::tom::XMLH> ocs_xml_create_head(const char *name,
                                                         ocs::tom::ListPtr<ocs::tom::XMLA> &&attributes);

/**
 * @brief Create the root element of an XML document
 *
 * @tparam T the object type of the elements nested in the document, e.g. `ocs::tom::XMLE`
 * @param name the root element's tag name
 * @param list the elements nested inside it; the document takes them over
 * @param attributes the root element's attributes; the document takes them over
 * @return the new document root
 */
template <ocs::tom::ObjectType T>
ocs::tom::ElementPtr<ocs::tom::XMLH> xml_getHead(const char *name, ocs::tom::ListPtr<T> &&list,
                                                 ocs::tom::ListPtr<ocs::tom::XMLA> &&attributes) {
   auto elements = std::move(list);
   auto xml_head = ocs_xml_create_head(name, std::move(attributes));
   ocs::tom::set<XMLH_Element>(xml_head, std::move(elements));
   return xml_head;
}

void xml_addAttributeD(ocs::tom::Element<ocs::tom::XMLE> xml_elem, const char *name, double value);
void xml_addAttribute(ocs::tom::Element<ocs::tom::XMLH> xml_elem, const char *name, const char *value);
void xml_addAttribute(ocs::tom::Element<ocs::tom::XMLE> xml_elem, const char *name, const char *value);

ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_D(ocs::tom::List<ocs::tom::XMLE> attributeList, const char *name,
                                                    double value);
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_D8(ocs::tom::List<ocs::tom::XMLE> attributeList, const char *name,
                                                     double value);
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_S(ocs::tom::List<ocs::tom::XMLE> attributeList, const char *name,
                                                    const char *value);
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_I(ocs::tom::List<ocs::tom::XMLE> attributeList, const char *name,
                                                    int value);
ocs::tom::Element<ocs::tom::XMLE> xml_append_Attr_U(ocs::tom::List<ocs::tom::XMLE> attributeList, const char *name,
                                                    uint32_t value);
bool escape_string(const char *string, dstring *target);

