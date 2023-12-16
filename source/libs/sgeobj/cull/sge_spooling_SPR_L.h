#ifndef SGE_SPR_L_H
#define SGE_SPR_L_H
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
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(SPR_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SPR_url) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_option_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_startup_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_shutdown_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_maintenance_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_trigger_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_transaction_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_list_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_read_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_read_keys_func) - read keys from the database
*    reads all keys from a spooling database
*    matching beginning with a certain pattern
*    @see e.g. spool_berkeleydb_read_keys()
*
*    SGE_REF(SPR_write_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_delete_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_validate_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_validate_list_func) - @todo add summary
*    @todo add description
*
*    SGE_REF(SPR_clientdata) - @todo add summary
*    @todo add description
*
*/

enum {
   SPR_name = SPR_LOWERBOUND,
   SPR_url,
   SPR_option_func,
   SPR_startup_func,
   SPR_shutdown_func,
   SPR_maintenance_func,
   SPR_trigger_func,
   SPR_transaction_func,
   SPR_list_func,
   SPR_read_func,
   SPR_read_keys_func,
   SPR_write_func,
   SPR_delete_func,
   SPR_validate_func,
   SPR_validate_list_func,
   SPR_clientdata
};

LISTDEF(SPR_Type)
   SGE_STRING(SPR_name, CULL_UNIQUE | CULL_HASH)
   SGE_STRING(SPR_url, CULL_DEFAULT)
   SGE_REF(SPR_option_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_startup_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_shutdown_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_maintenance_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_trigger_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_transaction_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_list_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_read_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_read_keys_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_write_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_delete_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_validate_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_validate_list_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_clientdata, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(SPRN)
   NAME("SPR_name")
   NAME("SPR_url")
   NAME("SPR_option_func")
   NAME("SPR_startup_func")
   NAME("SPR_shutdown_func")
   NAME("SPR_maintenance_func")
   NAME("SPR_trigger_func")
   NAME("SPR_transaction_func")
   NAME("SPR_list_func")
   NAME("SPR_read_func")
   NAME("SPR_read_keys_func")
   NAME("SPR_write_func")
   NAME("SPR_delete_func")
   NAME("SPR_validate_func")
   NAME("SPR_validate_list_func")
   NAME("SPR_clientdata")
NAMEEND

#define SPR_SIZE sizeof(SPRN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
