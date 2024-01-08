#ifndef SGE_HS_L_H
#define SGE_HS_L_H
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

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief scaling of host load values
*
* used for scaling host load values
* per load variable a scaling factor can be defined
*
*    SGE_STRING(HS_name) - load variable name
*    The Name of the load variable / the complex variable.
*
*    SGE_DOUBLE(HS_value) - scaling factor
*    A scaling factor as double value. Load values are multiplied by this factor.
*
*/

enum {
   HS_name = HS_LOWERBOUND,
   HS_value
};

LISTDEF(HS_Type)
   SGE_STRING(HS_name, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_DOUBLE(HS_value, CULL_SUBLIST)
LISTEND

NAMEDEF(HSN)
   NAME("HS_name")
   NAME("HS_value")
NAMEEND

#define HS_SIZE sizeof(HSN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
