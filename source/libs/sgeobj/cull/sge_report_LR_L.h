#ifndef SGE_LR_L_H
#define SGE_LR_L_H
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
* @brief Load Report
*
* A LoadReport object represents the value for a single load variable on a specific host.
*
*    SGE_STRING(LR_name) - Load Variable Name
*    Name of the load variable. In order for the load value to be processed in sge_qmaster
*    a complex variable with this name must have been configured.
*
*    SGE_STRING(LR_value) - Load Variable Value
*    Value of the variable on a specific host.
*
*    SGE_ULONG(LR_global) - Is Global
*    Specifies if it is a global load variable.
*    1 means that it is a global load value (host is global in this case),
*    0 means that it is a host specific load value.
*    @todo: make it a boolean
*
*    SGE_ULONG(LR_is_static) - Is Static
*    Specifies if it is a static load variable.
*    Static load variables represent seldomly changing variables, e.g. arch, n_proc, mem_total.
*    0 means a non static load value
*    1 means a static load value
*    2 is a special internal value: remove the load value
*
*    SGE_HOST(LR_host) - Host Name
*    Name of the host on which the load value is valid. Specific host name or keyword global for global load values.
*
*/

enum {
   LR_name = LR_LOWERBOUND,
   LR_value,
   LR_global,
   LR_is_static,
   LR_host
};

LISTDEF(LR_Type)
   SGE_STRING(LR_name, CULL_HASH)
   SGE_STRING(LR_value, CULL_DEFAULT)
   SGE_ULONG(LR_global, CULL_DEFAULT)
   SGE_ULONG(LR_is_static, CULL_DEFAULT)
   SGE_HOST(LR_host, CULL_HASH)
LISTEND

NAMEDEF(LRN)
   NAME("LR_name")
   NAME("LR_value")
   NAME("LR_global")
   NAME("LR_is_static")
   NAME("LR_host")
NAMEEND

#define LR_SIZE sizeof(LRN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
