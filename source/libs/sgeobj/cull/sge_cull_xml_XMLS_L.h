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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
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
*    SGE_STRING(XMLS_Name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(XMLS_Value) - @todo add summary
*    @todo add description
*
*    SGE_STRING(XMLS_Version) - @todo add summary
*    @todo add description
*
*/

enum {
   XMLS_Name = XMLS_LOWERBOUND,
   XMLS_Value,
   XMLS_Version
};

LISTDEF(XMLS_Type)
   SGE_STRING(XMLS_Name, CULL_DEFAULT)
   SGE_STRING(XMLS_Value, CULL_DEFAULT)
   SGE_STRING(XMLS_Version, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLSN)
   NAME("XMLS_Name")
   NAME("XMLS_Value")
   NAME("XMLS_Version")
NAMEEND

#define XMLS_SIZE sizeof(XMLSN)/sizeof(char *)


