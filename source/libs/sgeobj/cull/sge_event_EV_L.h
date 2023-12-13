#ifndef SGE_EV_L_H
#define SGE_EV_L_H
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
*    SGE_ULONG(EV_id) - @todo add summary
*    @todo add description
*
*    SGE_STRING(EV_name) - @todo add summary
*    @todo add description
*
*    SGE_HOST(EV_host) - @todo add summary
*    @todo add description
*
*    SGE_STRING(EV_commproc) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_commid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_uid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_d_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_flush_delay) - @todo add summary
*    @todo add description
*
*    SGE_LIST(EV_subscribed) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(EV_changed) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_busy_handling) - @todo add summary
*    @todo add description
*
*    SGE_STRING(EV_session) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_last_heard_from) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_last_send_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_next_send_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_next_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_busy) - @todo add summary
*    @todo add description
*
*    SGE_LIST(EV_events) - @todo add summary
*    @todo add description
*
*    SGE_REF(EV_sub_array) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EV_state) - @todo add summary
*    @todo add description
*
*    SGE_REF(EV_update_function) - @todo add summary
*    @todo add description
*
*/

enum {
   EV_id = EV_LOWERBOUND,
   EV_name,
   EV_host,
   EV_commproc,
   EV_commid,
   EV_uid,
   EV_d_time,
   EV_flush_delay,
   EV_subscribed,
   EV_changed,
   EV_busy_handling,
   EV_session,
   EV_last_heard_from,
   EV_last_send_time,
   EV_next_send_time,
   EV_next_number,
   EV_busy,
   EV_events,
   EV_sub_array,
   EV_state,
   EV_update_function
};

LISTDEF(EV_Type)
   SGE_ULONG(EV_id, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_STRING(EV_name, CULL_DEFAULT)
   SGE_HOST(EV_host, CULL_DEFAULT)
   SGE_STRING(EV_commproc, CULL_DEFAULT)
   SGE_ULONG(EV_commid, CULL_DEFAULT)
   SGE_ULONG(EV_uid, CULL_DEFAULT)
   SGE_ULONG(EV_d_time, CULL_DEFAULT)
   SGE_ULONG(EV_flush_delay, CULL_DEFAULT)
   SGE_LIST(EV_subscribed, EVS_Type, CULL_DEFAULT)
   SGE_BOOL(EV_changed, CULL_DEFAULT)
   SGE_ULONG(EV_busy_handling, CULL_DEFAULT)
   SGE_STRING(EV_session, CULL_DEFAULT)
   SGE_ULONG(EV_last_heard_from, CULL_DEFAULT)
   SGE_ULONG(EV_last_send_time, CULL_DEFAULT)
   SGE_ULONG(EV_next_send_time, CULL_DEFAULT)
   SGE_ULONG(EV_next_number, CULL_DEFAULT)
   SGE_ULONG(EV_busy, CULL_DEFAULT)
   SGE_LIST(EV_events, ET_Type, CULL_DEFAULT)
   SGE_REF(EV_sub_array, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(EV_state, CULL_DEFAULT)
   SGE_REF(EV_update_function, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(EVN)
   NAME("EV_id")
   NAME("EV_name")
   NAME("EV_host")
   NAME("EV_commproc")
   NAME("EV_commid")
   NAME("EV_uid")
   NAME("EV_d_time")
   NAME("EV_flush_delay")
   NAME("EV_subscribed")
   NAME("EV_changed")
   NAME("EV_busy_handling")
   NAME("EV_session")
   NAME("EV_last_heard_from")
   NAME("EV_last_send_time")
   NAME("EV_next_send_time")
   NAME("EV_next_number")
   NAME("EV_busy")
   NAME("EV_events")
   NAME("EV_sub_array")
   NAME("EV_state")
   NAME("EV_update_function")
NAMEEND

#define EV_SIZE sizeof(EVN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
