#ifndef SGE_XMLH_L_H
#define SGE_XMLH_L_H
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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(XMLH_Version) - @todo add summary
*    @todo add description
*
*    SGE_STRING(XMLH_Name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLH_Stylesheet) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLH_Attribute) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLH_Element) - @todo add summary
*    @todo add description
*
*/

enum {
   XMLH_Version = XMLH_LOWERBOUND,
   XMLH_Name,
   XMLH_Stylesheet,
   XMLH_Attribute,
   XMLH_Element
};

LISTDEF(XMLH_Type)
   SGE_STRING(XMLH_Version, CULL_DEFAULT)
   SGE_STRING(XMLH_Name, CULL_DEFAULT)
   SGE_LIST(XMLH_Stylesheet, XMLS_Type, CULL_DEFAULT)
   SGE_LIST(XMLH_Attribute, XMLA_Type, CULL_DEFAULT)
   SGE_LIST(XMLH_Element, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLHN)
   NAME("XMLH_Version")
   NAME("XMLH_Name")
   NAME("XMLH_Stylesheet")
   NAME("XMLH_Attribute")
   NAME("XMLH_Element")
NAMEEND

#define XMLH_SIZE sizeof(XMLHN)/sizeof(char *)


#endif
