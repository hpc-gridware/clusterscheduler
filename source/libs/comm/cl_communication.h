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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The protocol layer under the public interface
 *
 * What #cl_commlib_send_message and friends are built on: opening and closing
 * a connection, the GMSH/MIH read and write path, host name resolution and
 * its cache, and the accept loop of a service.
 *
 * Nothing here is framework specific - a `cl_com_*` call in this file
 * dispatches on #cl_com_connection_type::framework_type into
 * `cl_tcp_framework.cc` or `cl_ssl_framework.cc`.
 *
 * @see @ref cl_commlib.h for the interface an application uses
 */

#include "comm/lists/cl_list_types.h"
#include "comm/cl_data_types.h"
#include "comm/cl_xml_parsing.h"
#include "comm/cl_connection_list.h"

/** @name Default timeouts, in seconds
 *
 * These are the values a fresh handle starts with; an application can change
 * most of them per handle afterwards.
 * @{
 */
#define CL_DEFINE_READ_TIMEOUT                       30     ///< Give up on a read that makes no progress
#define CL_DEFINE_WRITE_TIMEOUT                      30     ///< Give up on a write that makes no progress
#define CL_DEFINE_ACK_TIMEOUT                        60     ///< Give up waiting for an acknowledgement
#define CL_DEFINE_MESSAGE_TIMEOUT                    3600   ///< Discard a received message the application never took - an hour, because taking it is the application's business
#define CL_DEFINE_GET_CLIENT_CONNECTION_DATA_TIMEOUT 60     ///< Give up on a client that connected but never sent its CM
#define CL_DEFINE_DELETE_MESSAGES_TIMEOUT_AFTER_CCRM 60     ///< How long unread messages survive after the connection closed
#define CL_DEFINE_SYNCHRON_RECEIVE_TIMEOUT           60     ///< How long a synchronous receive waits
#define CL_DEFINE_CLIENT_CONNECTION_LIFETIME         600    ///< Cut a client off after this much silence
#define CL_DEFINE_MESSAGE_DUP_LOG_TIMEOUT            30     ///< Within this window a repeated application error is marked as a duplicate instead of logged again
/** @} */


/** @brief Size of a connection's read and write buffer */
#define CL_DEFINE_DATA_BUFFER_SIZE                   1024 * 4

/** @brief Highest message id before the counter wraps
 *
 * @warning Only 65535, because application code still stores client ids in a
 *          `u_short`. The 32 bit value beside it in the source is disabled
 *          behind `#if 0` and cannot be enabled until that is fixed - a busy
 *          qmaster wraps a 16 bit counter quickly.
 */
#if 0
#define CL_DEFINE_MAX_MESSAGE_ID                     4294967295UL
#else
#define CL_DEFINE_MAX_MESSAGE_ID                     65535
#endif

/** @brief Longest host name the commlib handles
 *
 * The system's `MAXHOSTNAMELEN` when that is larger than 256, otherwise 256 -
 * some systems define it as 64, which is too short for a fully qualified name.
 */
#if defined(MAXHOSTNAMELEN) && MAXHOSTNAMELEN > 256
#define CL_MAXHOSTNAMELEN MAXHOSTNAMELEN
#else
#define CL_MAXHOSTNAMELEN 256
#endif


int cl_com_compare_endpoints(cl_com_endpoint_t *endpoint1, cl_com_endpoint_t *endpoint2);

/** @brief Write an endpoint to the log
 *
 * @param endpoint the endpoint
 * @param text prefix for the log line
 *
 * @note Defined only when `CL_DO_COMMUNICATION_DEBUG` is on, which it is not
 *       by default - so this is documented here rather than at the
 *       definition, which doxygen never reaches.
 */
void cl_com_dump_endpoint(cl_com_endpoint_t *endpoint, const char *text);

int cl_com_endpoint_list_refresh(cl_raw_list_t *endpoint_list);

/* debug client functions */
int cl_com_add_debug_message(cl_com_connection_t *connection, const char *message, cl_com_message_t *ms);

int cl_com_gethostname(char **unique_hostname, struct in_addr *copy_addr, struct hostent **he_copy,
                       int *system_error_value);

int cl_com_host_list_refresh(cl_raw_list_t *host_list);

int cl_com_cached_gethostbyname(const char *hostname, char **unique_hostname, struct in_addr *copy_addr,
                                struct hostent **he_copy, int *system_error_value);

int cl_com_cached_gethostbyaddr(struct in_addr *addr, char **unique_hostname, struct hostent **he_copy,
                                int *system_error_val);

char *cl_com_get_h_error_string(int h_error);

int cl_com_compare_hosts(const char *host1, const char *host2);

int cl_com_set_resolve_method(cl_host_resolve_method_t method, char *local_domain_name);

int cl_com_free_handle_statistic(cl_com_handle_statistic_t **statistic);

int cl_com_free_hostent(cl_com_hostent_t **hostent_p);                    /* CR check */
int cl_com_free_hostspec(cl_com_host_spec_t **hostspec);

/** @brief Write a resolver result to the log
 *
 * @param hostent_p the result
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 *
 * @note Like #cl_com_dump_endpoint, defined only behind
 *       `CL_DO_COMMUNICATION_DEBUG`.
 */
int cl_com_print_host_info(cl_com_hostent_t *hostent_p);


int cl_com_create_debug_client_setup(cl_debug_client_setup_t **new_setup,
                                     cl_debug_client_t dc_mode,
                                     bool dc_dump_flag,
                                     int dc_app_log_level);

int cl_com_free_debug_client_setup(cl_debug_client_setup_t **new_setup);

#if defined(SECURE)
int cl_com_create_ssl_setup(cl_ssl_setup_t **new_setup,
                            cl_ssl_cert_mode_t ssl_cert_mode,
                            cl_ssl_method_t ssl_method,
                            const char *ssl_CA_cert_pem_file,
                            const char *ssl_CA_key_pem_file,
                            const char *ssl_cert_pem_file,
                            const char *ssl_key_pem_file,
                            const char *ssl_rand_file,
                            const char *ssl_reconnect_file,
                            const char *ssl_crl_file,
                            unsigned long ssl_refresh_time,
                            const char *ssl_password,
                            cl_ssl_verify_func_t ssl_verify_func);
#endif
#if defined(OCS_WITH_OPENSSL)
int cl_com_create_ssl_setup(cl_ssl_setup_t **new_setup,
                            cl_ssl_cert_mode_t ssl_cert_mode,
                            cl_ssl_method_t ssl_method,
                            const char *ssl_client_cert_file,
                            const char *ssl_server_cert_file,
                            const char *ssl_server_key_file,
                            bool allow_incomplete = false, bool needs_client_cert = true);
#endif

int cl_com_dup_ssl_setup(cl_ssl_setup_t **new_setup, cl_ssl_setup_t *source);

int cl_com_free_ssl_setup(cl_ssl_setup_t **del_setup);

/** @name Turning a connection's enum fields into names
 *
 * One per enum in #cl_com_connection_type. Their only caller is the
 * diagnostic output - `qping -info` and the debug client - which is why they
 * all return a static string rather than filling a buffer.
 * @{
 */
const char *cl_com_get_framework_type(cl_com_connection_t *connection);        ///< Name of the transport
const char *cl_com_get_connection_type(cl_com_connection_t *connection);       ///< Name of the direction
const char *cl_com_get_service_handler_flag(cl_com_connection_t *connection);  ///< Whether it is the listening socket
const char *cl_com_get_data_write_flag(cl_com_connection_t *connection);       ///< Whether there is data to write
const char *cl_com_get_data_read_flag(cl_com_connection_t *connection);        ///< Whether there is data to read
const char *cl_com_get_connection_state(cl_com_connection_t *connection);      ///< Name of the state
const char *cl_com_get_connection_sub_state(cl_com_connection_t *connection);  ///< Name of the sub state
const char *cl_com_get_data_flow_type(cl_com_connection_t *connection);        ///< Stream or message oriented
/** @} */

void cl_com_ignore_timeouts(bool flag);

bool cl_com_get_ignore_timeouts_flag();


/* message functions */
int
cl_com_setup_message(cl_com_message_t **message, cl_com_connection_t *connection, cl_byte_t *data, unsigned long size,
                     cl_xml_ack_type_t ack_type, unsigned long response_id,
                     unsigned long tag);   /* *message must be zero */
int cl_com_create_message(cl_com_message_t **message);

int cl_com_free_message(cl_com_message_t **message);

int cl_com_create_connection(cl_com_connection_t **connection);

/** @cond doxygen_note
 * There is no `cl_com_free_connection()`; #cl_com_close_connection releases
 * the connection as well.
 * @endcond
 */


int cl_com_connection_complete_accept(cl_com_connection_t *connection,
                                      long timeout);

int cl_com_connection_complete_shutdown(cl_com_connection_t *connection);


int cl_com_open_connection(cl_com_connection_t *connection,
                           int timeout,
                           cl_com_endpoint_t *remote_endpoint,
                           cl_com_endpoint_t *local_endpoint);    /* CR check */

int cl_com_close_connection(cl_com_connection_t **connection);  /* CR check */

int cl_com_read_GMSH(cl_com_connection_t *connection, unsigned long *only_one_read);

int cl_com_read(cl_com_connection_t *connection, cl_byte_t *message, unsigned long size, unsigned long *only_one_read);

int
cl_com_write(cl_com_connection_t *connection, cl_byte_t *message, unsigned long size, unsigned long *only_one_write);



/** @name Only valid on a service connection
 *
 * These need a connection that went through
 * #cl_com_connection_request_handler_setup - the listening socket, not a
 * connection to a peer.
 * @{
 */

int cl_com_connection_get_connect_port(cl_com_connection_t *connection, int *port);

int cl_com_connection_set_connect_port(cl_com_connection_t *connection, int port);

int cl_com_connection_get_service_port(cl_com_connection_t *connection, int *port);

int cl_com_connection_get_fd(cl_com_connection_t *connection, int *fd);
const char *cl_com_connection_get_ip(cl_com_connection_t *connection, dstring *dstr);

int cl_com_connection_get_client_socket_in_port(cl_com_connection_t *connection, int *port);


/* setup service */
int cl_com_connection_request_handler_setup(cl_com_connection_t *connection,
                                            cl_com_endpoint_t *local_endpoint);

/* check for new service connection clients */
int cl_com_connection_request_handler(cl_com_connection_t *connection,
                                      cl_com_connection_t **new_connection);

/* cleanup service */
int cl_com_connection_request_handler_cleanup(cl_com_connection_t *connection);

int cl_com_open_connection_request_handler(cl_com_poll_t *poll_handle,
                                           cl_com_handle_t *handle,
                                           int timeout_val_sec,
                                           int timeout_val_usec,
                                           cl_select_method_t select_mode);

int cl_com_free_poll_array(cl_com_poll_t *poll_handle);

int cl_com_malloc_poll_array(cl_com_poll_t *poll_handle, unsigned long nr_of_malloced_connections);

int cl_com_connection_complete_request(cl_raw_list_t *connection_list, cl_connection_list_elem_t *elem, long timeout,
                                       cl_select_method_t select_mode);
/** @} */
