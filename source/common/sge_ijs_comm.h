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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface of the interactive job support transport
 */

#if defined(LINUX)
#include <pty.h>
#endif

#include "uti/sge_dstring.h"

#include "comm/cl_data_types.h"
#include "comm/cl_commlib.h"
#include "comm/cl_connection_list.h"

/** @brief Size of the read and write buffers of the interactive job transport */
#define BUFSIZE              64*1024
/** @brief The commlib handle type, under the name this module uses for it */
#define COMM_HANDLE cl_com_handle_t

#define STDIN_DATA_MSG               0   ///< client→shepherd: data typed by the user, to be written to the job's stdin
#define STDOUT_DATA_MSG              1   ///< shepherd→client: data the job wrote to stdout
#define STDERR_DATA_MSG              2   ///< shepherd→client: data the job wrote to stderr
#define WINDOW_SIZE_CTRL_MSG         3   ///< client→shepherd: the terminal was resized; the new size for the slave PTY
#define REGISTER_CTRL_MSG            4   ///< shepherd→client: the shepherd is up and ready to carry traffic
#define UNREGISTER_CTRL_MSG          5   ///< shepherd→client: the job ended; the shepherd is about to go away
#define UNREGISTER_RESPONSE_CTRL_MSG 6   ///< client→shepherd: acknowledges the unregister, so the shepherd may exit
#define SETTINGS_CTRL_MSG            7   ///< client→shepherd: the client's terminal settings, to be applied to the slave PTY
#define SUSPEND_CTRL_MSG             8   ///< client→shepherd: stop forwarding; the user suspended the client
#define UNSUSPEND_CTRL_MSG           9   ///< client→shepherd: resume forwarding
#define STDIN_CLOSE_MSG              10   ///< client→shepherd: the user's stdin reached end of file
#define X11_AUTH_MSG                 11  ///< client→shepherd: real MIT-MAGIC-COOKIE-1 hex string
#define X11_OPEN_MSG                 12  ///< shepherd→client: 2-byte big-endian conn_id (new X11 connection)
#define X11_DATA_MSG                 13  ///< bidirectional: 2-byte big-endian conn_id followed by X11 data
#define X11_CLOSE_MSG                14  ///< bidirectional: 2-byte big-endian conn_id (connection closed)
#define KEEPALIVE_MSG                15  ///< client→shepherd: keepalive probe (are you there?)
#define KEEPALIVE_ACK_MSG            16  ///< shepherd→client: keepalive acknowledgement (yes, I'm here)
#define RECONNECT_REQUEST_MSG        17  ///< shepherd→client (reconnect): identity proof carrying the one-time token
#define RECONNECT_ACCEPT_MSG         18  ///< client→shepherd: token validated, take over the session
#define RECONNECT_REJECT_MSG         19  ///< client→shepherd: token mismatch — shepherd must give up and kill the job
#define NOECHO_CTRL_MSG              20  ///< client→shepherd: disable ECHO on the slave PTY (sent when the client's local stdin is not a tty, so the shepherd's forwarded input is not echoed back into the job's output)

#define COMM_RETVAL_OK                    0   ///< no error
#define COMM_INVALID_PARAMETER            1   ///< invalid parameter
#define COMM_CANT_SETUP_COMMLIB           2   ///< could not setup commlib
#define COMM_CANT_CLEANUP_COMMLIB         3   ///< could not cleanup commlib
#define COMM_CANT_CREATE_HANDLE           4   ///< could not create handle
#define COMM_CANT_SHUTDOWN_HANDLE         5   ///< could not shutdown handle
#define COMM_CANT_OPEN_CONNECTION         6   ///< could not open connection
#define COMM_CANT_CLOSE_CONNECTION        7   ///< could not close connection
#define COMM_CANT_SETUP_SSL               8   ///< could not setup ssl
#define COMM_CANT_SET_CONNECTION_PARAM    9   ///< could not set connection param
#define COMM_CANT_SET_IGNORE_TIMEOUTS    10   ///< could not set ignore timeouts
#define COMM_GOT_TIMEOUT                 11   ///< got timeout
#define COMM_CANT_TRIGGER                12   ///< could not trigger
#define COMM_CANT_SEARCH_ENDPOINT        13   ///< could not search endpoint
#define COMM_CANT_LOCK_CONNECTION_LIST   14   ///< could not lock connection list
#define COMM_CANT_UNLOCK_CONNECTION_LIST 15   ///< could not unlock connection list
#define COMM_CANT_RECEIVE_MESSAGE        16   ///< could not receive message
#define COMM_CANT_FREE_MESSAGE           17   ///< could not free message
#define COMM_CANT_GET_CLIENT_STATUS      18   ///< could not get client status
#define COMM_NO_SELECT_DESCRIPTORS       19   ///< no select descriptors
#define COMM_CONNECTION_NOT_FOUND        20   ///< connection not found
#define COMM_NO_SECURITY_COMPILED_IN     21   ///< no security compiled in
#define COMM_SELECT_INTERRUPT            22   ///< select interrupt
#define COMM_ENDPOINT_NOT_UNIQUE         23   ///< endpoint not unique
#define COMM_ACCESS_DENIED               24   ///< access denied
#define COMM_SYNC_RECEIVE_TIMEOUT        25   ///< sync receive timeout
#define COMM_NO_MESSAGE_AVAILABLE        26   ///< no message available

/** @brief One message taken off the interactive job connection
 *
 * The payload is only ever one of `data` or `ws`, chosen by `type`, and neither
 * is owned by this struct: both point into `cl_message`, which is why
 * #comm_free_message has to be called and the payload must not be used after
 * it.
 */
typedef struct recv_message_s {
   unsigned char type;        ///< one of the `*_MSG` values above
   char *data;                ///< the payload of a data message
   struct winsize ws;         ///< the new size, for a `WINDOW_SIZE_CTRL_MSG`
   cl_com_message_t *cl_message;   ///< the commlib message the two above point into
} recv_message_t;


int comm_init_lib(dstring *err_msg, cl_log_func_t commlib_log_func = nullptr);
int comm_cleanup_lib(dstring *err_msg);

int comm_open_connection(bool                 b_server,
                         cl_framework_t communication_framework,
                         const char           *this_component,
                         int                  port,
                         const char           *other_component,
                         const char *hostname,
                         const char           *user_name,
                         COMM_HANDLE          **handle,
                         dstring              *err_msg);

int comm_get_application_error(dstring *err_msg);
void comm_set_suppress_bind_errors(bool suppress);
void comm_reset_application_error();
int comm_shutdown_connection(COMM_HANDLE *handle,
                             const char *component_name,
                             char *hostname,
                             dstring *err_msg);


int comm_set_connection_param(COMM_HANDLE *handle, int param, int value,
                              dstring *err_msg);
int comm_ignore_timeouts(bool b_ignore, dstring *err_msg);

int comm_wait_for_connection(COMM_HANDLE *handle, const char *component,
                             int wait_secs, const char **host, dstring *err_msg);
int comm_get_connection_count(const COMM_HANDLE *handle, dstring *err_msg);

unsigned long comm_write_message(COMM_HANDLE *handle,
                  const char *unresolved_hostname,
                  const char *component_name,
                  unsigned long component_id,
                  unsigned char *buffer,
                  unsigned long size,
                  unsigned char type,
                  dstring *err_msg);

int comm_wait_for_all_messages_sent(COMM_HANDLE *handle, dstring *err_msg);

int comm_recv_message(COMM_HANDLE *handle, recv_message_t *recv_mess, dstring *err_msg);

int comm_free_message(recv_message_t *recv_mess, dstring *err_msg);
int check_client_alive(COMM_HANDLE *handle, const char *component_name, char *hostname, dstring *err_msg);
