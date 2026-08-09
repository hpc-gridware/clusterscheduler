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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The queues between the application and the connections
 */


#include "comm/lists/cl_lists.h"
#include "comm/cl_data_types.h"

/** @brief One entry of a handle's send or receive queue
 *
 * The same struct serves both queues, and which half of it is filled depends
 * on which queue the element is in - the `rcv_` member for
 * #cl_com_handle::received_message_queue, the `snd_` members for
 * #cl_com_handle::send_message_queue.
 */
typedef struct cl_app_message_queue_elem_t {
   cl_com_connection_t *rcv_connection;   ///< Receive queue: the connection a message arrived on

   cl_com_endpoint_t *snd_destination;    ///< Send queue: who the message goes to
   cl_xml_ack_type_t snd_ack_type;        ///< Send queue: whether and when the peer acknowledges
   cl_byte_t *snd_data;                   ///< Send queue: the payload, owned by the element
   unsigned long snd_size;                ///< Send queue: its length
   unsigned long snd_response_mid;        ///< Send queue: the message id this answers, or 0
   unsigned long snd_tag;                 ///< Send queue: the application's own tag

   cl_raw_list_elem_t *raw_elem;          ///< Back pointer into the raw list
} cl_app_message_queue_elem_t;


/* basic functions */
int cl_app_message_queue_setup(cl_raw_list_t **list_p, const char *list_name, int enable_locking);

int cl_app_message_queue_cleanup(cl_raw_list_t **list_p);


/* thread list functions that will lock the list */
int cl_app_message_queue_append(cl_raw_list_t *list_p,
                                cl_com_connection_t *rcv_connection,
                                cl_com_endpoint_t *snd_destination,
                                cl_xml_ack_type_t snd_ack_type,
                                cl_byte_t *snd_data,
                                unsigned long snd_size,
                                unsigned long snd_response_mid,
                                unsigned long snd_tag,
                                int do_lock);

int cl_app_message_queue_remove(cl_raw_list_t *list_p, cl_com_connection_t *connection, int do_lock,
                                bool remove_all_elements);


/* thread functions that will not lock the list */
cl_app_message_queue_elem_t *cl_app_message_queue_get_first_elem(cl_raw_list_t *list_p);

cl_app_message_queue_elem_t *cl_app_message_queue_get_least_elem(cl_raw_list_t *list_p);

cl_app_message_queue_elem_t *cl_app_message_queue_get_next_elem(cl_app_message_queue_elem_t *elem);

cl_app_message_queue_elem_t *cl_app_message_queue_get_last_elem(cl_app_message_queue_elem_t *elem);
