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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_LIST(XMLE_Attribute) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(XMLE_Print) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(XMLE_Element) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLE_List) - @todo add summary
*    @todo add description
*
*/

enum {
   XMLE_Attribute = XMLE_LOWERBOUND,
   XMLE_Print,
   XMLE_Element,
   XMLE_List
};

LISTDEF(XMLE_Type)
   SGE_LIST(XMLE_Attribute, XMLA_Type, CULL_DEFAULT)
   SGE_BOOL(XMLE_Print, CULL_DEFAULT)
   SGE_OBJECT(XMLE_Element, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(XMLE_List, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLEN)
   NAME("XMLE_Attribute")
   NAME("XMLE_Print")
   NAME("XMLE_Element")
   NAME("XMLE_List")
NAMEEND

#define XMLE_SIZE sizeof(XMLEN)/sizeof(char *)


