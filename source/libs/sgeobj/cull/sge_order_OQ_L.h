#ifndef SGE_OQ_L_H
#define SGE_OQ_L_H
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
* @brief Queue Info in an Order
*
* Job start orders and also orders for reprioritization contain per queue information.
* One or more objects of type OrderQueue are contained in the queuelist attribute of an order.
*
*    SGE_ULONG(OQ_slots) - Slots
*    The number of slots a job occupies in the queue / queue instance.
*
*    SGE_STRING(OQ_dest_queue) - Qinstance Name
*    The full queue instance name (cluster queue name + @ + host name).
*
*    SGE_ULONG(OQ_dest_version) - Qinstance Version
*    Version of the queue instance. Is used to detect if the queue has been modified during scheduling.
*    In this case the scheduling result is skipped.
*
*    SGE_DOUBLE(OQ_ticket) - Total Tickets
*    Total SGEEE tickets for slots.
*
*    SGE_DOUBLE(OQ_oticket) - Override Tickets
*    Total SGEEE override tickets.
*
*    SGE_DOUBLE(OQ_fticket) - Functional Tickets
*    Total SGEEE functional tickets.
*
*    SGE_DOUBLE(OQ_sticket) - Sharetree Tickets
*    Total SGEEE sharetree tickets.
*
*/

enum {
   OQ_slots = OQ_LOWERBOUND,
   OQ_dest_queue,
   OQ_dest_version,
   OQ_ticket,
   OQ_oticket,
   OQ_fticket,
   OQ_sticket
};

LISTDEF(OQ_Type)
   SGE_ULONG(OQ_slots, CULL_DEFAULT)
   SGE_STRING(OQ_dest_queue, CULL_DEFAULT)
   SGE_ULONG(OQ_dest_version, CULL_DEFAULT)
   SGE_DOUBLE(OQ_ticket, CULL_DEFAULT)
   SGE_DOUBLE(OQ_oticket, CULL_DEFAULT)
   SGE_DOUBLE(OQ_fticket, CULL_DEFAULT)
   SGE_DOUBLE(OQ_sticket, CULL_DEFAULT)
LISTEND

NAMEDEF(OQN)
   NAME("OQ_slots")
   NAME("OQ_dest_queue")
   NAME("OQ_dest_version")
   NAME("OQ_ticket")
   NAME("OQ_oticket")
   NAME("OQ_fticket")
   NAME("OQ_sticket")
NAMEEND

#define OQ_SIZE sizeof(OQN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
