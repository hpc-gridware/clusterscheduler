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
*    SGE_ULONG(EVR_operation) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EVR_timestamp) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EVR_event_client_id) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EVR_event_number) - @todo add summary
*    @todo add description
*
*    SGE_STRING(EVR_session) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(EVR_event_client) - @todo add summary
*    @todo add description
*
*    SGE_LIST(EVR_event_list) - @todo add summary
*    @todo add description
*
*/

enum {
   EVR_operation = EVR_LOWERBOUND,
   EVR_timestamp,
   EVR_event_client_id,
   EVR_event_number,
   EVR_session,
   EVR_event_client,
   EVR_event_list
};

LISTDEF(EVR_Type)
   SGE_ULONG(EVR_operation, CULL_DEFAULT)
   SGE_ULONG(EVR_timestamp, CULL_DEFAULT)
   SGE_ULONG(EVR_event_client_id, CULL_DEFAULT)
   SGE_ULONG(EVR_event_number, CULL_DEFAULT)
   SGE_STRING(EVR_session, CULL_DEFAULT)
   SGE_OBJECT(EVR_event_client, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(EVR_event_list, ET_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(EVRN)
   NAME("EVR_operation")
   NAME("EVR_timestamp")
   NAME("EVR_event_client_id")
   NAME("EVR_event_number")
   NAME("EVR_session")
   NAME("EVR_event_client")
   NAME("EVR_event_list")
NAMEEND

#define EVR_SIZE sizeof(EVRN)/sizeof(char *)


