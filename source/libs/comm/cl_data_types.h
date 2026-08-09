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
 * @brief The commlib's types: handles, connections, messages and their states
 *
 * ## The protocol, in nine acronyms
 *
 * Half the enumerators below name a message of the commlib's own wire
 * protocol, and the names are opaque until the acronyms are known. They are
 * defined in `cl_xml_parsing.h`, which is where the XML for each lives:
 *
 * | | |
 * |---|---|
 * | **GMSH** | a length prefix - `<gmsh><dl>N</dl></gmsh>` - giving the byte length of the XML struct that follows. Every message is preceded by one, which is why reading always starts with #CL_COM_READ_GMSH |
 * | **MIH**  | Message Information Header: message id, length, data format, acknowledge type, tag, response id |
 * | **CM**   | Connect Message - a client announcing itself |
 * | **CRM**  | Connect Response Message - the service accepting or denying it |
 * | **AM**   | Acknowledge Message |
 * | **CCM**  | Connection Close Message |
 * | **CCRM** | Connection Close Response Message |
 * | **SIM**  | Status Information Message - what `qping` sends |
 * | **SIRM** | Status Information Response Message |
 *
 * ## Two state machines, not one
 *
 * A connection has a #cl_connection_state_t and, inside it, a
 * #cl_connection_sub_state_type. The sub state is only meaningful together
 * with the state it belongs to - the same reason its enumerators are grouped
 * by parent state below. A message has a state of its own,
 * #cl_message_state_t, which tracks how far one message has got through the
 * GMSH/MIH/payload sequence.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>

#include <cinttypes>
#include "comm/lists/cl_lists.h"
#include "comm/cl_xml_parsing.h"

#include "uti/ocs_OpenSSL.h"

/** @brief What a line in the debug client stream describes */
typedef enum cl_com_debug_message_tag_type {
   CL_DMT_MESSAGE = 1,   ///< Commlib message traffic; the default
   CL_DMT_APP_MESSAGE,   ///< A message the application logged
   CL_DMT_MAX_TYPE       ///< Not a tag: the count. @warning Must stay last
} cl_com_debug_message_tag_t;

/** @brief Which local port a TCP connect uses */
typedef enum cl_tcp_connect_def {
   CL_TCP_DEFAULT = 1,     ///< Any ephemeral port
   CL_TCP_RESERVED_PORT    ///< A port below 1024, which only root can bind - the peer can therefore trust that the caller was privileged
} cl_tcp_connect_t;

/** @brief The commlib's global on/off switches */
typedef enum cl_global_settings_params_def {
   CL_COMMLIB_DELAYED_LISTEN = 1   ///< Do not accept connections until the application says it is ready
} cl_global_settings_params_t;

/** @brief Which transport a handle and its connections use
 *
 * The framework is what decides which `cl_*_framework.cc` implements the
 * reading, writing and connecting for a connection.
 */
typedef enum cl_framework_def {
   CL_CT_UNDEFINED = 0,   ///< Unset
   CL_CT_TCP,             ///< Plain TCP, `cl_tcp_framework.cc`
   CL_CT_SSL,             ///< The older SSL framework, `cl_ssl_framework.cc`
   CL_CT_SSL_TLS          ///< The newer SSL/TLS framework
} cl_framework_t;


/** @brief What a file descriptor is being watched for */
typedef enum cl_select_method_def {
   CL_RW_SELECT,   ///< Readable or writable
   CL_R_SELECT,    ///< Readable only
   CL_W_SELECT     ///< Writable only
} cl_select_method_t;

/** @brief What a service does when it has as many connections as it allows
 *
 * See #cl_com_handle::max_open_connections. The problem this solves is that a
 * qmaster which simply stops accepting looks dead to every new client, so the
 * first option makes room instead by dropping clients that said they may be
 * closed.
 */
typedef enum cl_max_count_def {
   CL_ON_MAX_COUNT_CLOSE_AUTOCLOSE_CLIENTS = 5,   ///< Close connections whose peer allowed autoclose, making room
   CL_ON_MAX_COUNT_DISABLE_ACCEPT,                ///< Stop accepting until a connection goes away
   CL_ON_MAX_COUNT_OFF                            ///< Do nothing, keep accepting
} cl_max_count_t;


/** @brief Whether host names are compared short or fully qualified */
typedef enum cl_host_resolve_method_def {
   CL_SHORT = 1,   ///< Compare up to the first dot
   CL_LONG = 2     ///< Compare the fully qualified name
} cl_host_resolve_method_t;

/** @brief Whether the commlib runs threads of its own
 *
 * This is the single biggest decision an application makes about the
 * commlib, because it decides who drives the I/O.
 */
typedef enum cl_thread_mode_def {
   CL_NO_THREAD,   ///< No commlib threads; the application must call `cl_commlib_trigger()` from its main loop or nothing moves
   CL_RW_THREAD    ///< The commlib runs its own read, write, trigger and service threads
} cl_thread_mode_t;


typedef unsigned char cl_byte_t;   ///< One byte of message payload

/** @brief Which directions a connection carries, see #cl_com_connection_type::connection_type */
typedef enum cl_connection_type {
   CL_COM_RECEIVE = 1,    ///< Incoming messages only
   CL_COM_SEND,           ///< Outgoing messages only
   CL_COM_SEND_RECEIVE,   ///< Both
   CL_COM_UNDEFINED       ///< Unset
} cl_connection_t;

/** @brief Whether there is data to move, see #cl_com_connection_type::data_read_flag */
typedef enum cl_data_ready_flag_type {
   CL_COM_DATA_READY = 1,   ///< There is data waiting
   CL_COM_DATA_NOT_READY    ///< There is not
} cl_data_ready_flag_t;

/** @brief Whether a connection is the listening socket or a real peer
 *
 * A service handle holds one #CL_COM_SERVICE_HANDLER connection - the
 * listening socket, which carries no messages - plus one #CL_COM_CONNECTION
 * per accepted client.
 */
typedef enum cl_service_handler_type {
   CL_COM_SERVICE_HANDLER = 1,   ///< The listening socket
   CL_COM_CONNECTION,            ///< A connection to a peer
   CL_COM_SERVICE_UNDEFINED      ///< Unset
} cl_service_handler_t;

/** @brief The state of a connection
 *
 * Each of these has its own set of sub states, see
 * #cl_connection_sub_state_type. #CL_OPENING is the outgoing path and
 * #CL_ACCEPTING the incoming one; both end in #CL_CONNECTING, which is where
 * the CM/CRM handshake happens, and then #CL_CONNECTED.
 */
typedef enum cl_connection_state_type {
   CL_DISCONNECTED = 1,   ///< No socket
   CL_OPENING,            ///< Opening outwards: `connect()`, and the SSL handshake if any
   CL_ACCEPTING,          ///< Accepting inwards, from the listening socket
   CL_CONNECTING,         ///< Socket is up, the CM/CRM handshake is running
   CL_CONNECTED,          ///< Usable for messages
   CL_CLOSING             ///< Shutting down
} cl_connection_state_t;

/** @brief Where inside its #cl_connection_state_t a connection has got to
 *
 * A sub state is only meaningful together with its parent state, which is why
 * the values are grouped below. The two handshakes are worth reading as
 * sequences:
 *
 * - **accepting a client** (`CL_ACCEPTING` then `CL_CONNECTING`): accept the
 *   socket, read the GMSH length prefix, read the CM the client sent, decide
 *   whether to admit it, send the CRM back.
 * - **opening outwards** (`CL_OPENING` then `CL_CONNECTING`): connect, do the
 *   SSL handshake if the framework needs one, send our CM, then read the
 *   GMSH and the CRM that answer it.
 *
 * Closing is symmetric in the same way: whoever wants to close sends a CCM
 * and waits for the CCRM, and the other side answers it.
 */
typedef enum cl_connection_sub_state_type {
   /** @name When #CL_DISCONNECTED
    * @{ */
   CL_COM_SUB_STATE_UNDEFINED = 1,    ///< Nothing in progress
   /** @} */

   /** @name When #CL_OPENING - connecting outwards
    * @{ */
   CL_COM_OPEN_INIT,                  ///< About to create the socket
   CL_COM_OPEN_CONNECT,               ///< About to call `connect()`
   CL_COM_OPEN_CONNECT_IN_PROGRESS,   ///< `connect()` returned `EINPROGRESS`; waiting for the socket to become writable
   CL_COM_OPEN_CONNECTED,             ///< The socket is up
   CL_COM_OPEN_SSL_CONNECT_INIT,      ///< About to start the SSL handshake
   CL_COM_OPEN_SSL_CONNECT,           ///< SSL handshake running
   /** @} */

   /** @name When #CL_ACCEPTING - taking an incoming connection
    * @{ */
   CL_COM_ACCEPT_INIT,                ///< About to accept
   CL_COM_ACCEPT,                     ///< Accepting, including the SSL handshake if any
   /** @} */

   /** @name When #CL_CONNECTING - the CM/CRM handshake
    *
    * The `READ_*` values are the service's side, the `SEND_*` values the
    * client's.
    * @{ */
   CL_COM_READ_INIT,                  ///< Service: about to read the client's connect message
   CL_COM_READ_GMSH,                  ///< Service: reading the GMSH length prefix
   CL_COM_READ_CM,                    ///< Service: reading the CM itself
   CL_COM_READ_INIT_CRM,              ///< Service: building the CRM, having decided whether to admit the client
   CL_COM_READ_SEND_CRM,              ///< Service: sending the CRM
   CL_COM_SEND_INIT,                  ///< Client: about to send its connect message
   CL_COM_SEND_CM,                    ///< Client: sending the CM
   CL_COM_SEND_READ_GMSH,             ///< Client: reading the GMSH prefix of the answer
   CL_COM_SEND_READ_CRM,              ///< Client: reading the CRM, which says whether it was admitted
   /** @} */

   /** @name When #CL_CONNECTED
    * @{ */
   CL_COM_WORK,                       ///< Carrying messages; the normal state
   CL_COM_RECEIVED_CCM,               ///< The peer asked to close
   CL_COM_SENDING_CCM,                ///< We asked to close
   CL_COM_WAIT_FOR_CCRM,              ///< Waiting for the peer to confirm our close
   CL_COM_SENDING_CCRM,               ///< Confirming the peer's close
   CL_COM_DONE,                       ///< Both sides agreed; nothing more will be carried
   /** @} */

   /** @name When #CL_CLOSING
    * @{ */
   CL_COM_DO_SHUTDOWN,                ///< Tearing the socket down
   CL_COM_SHUTDOWN_DONE               ///< Torn down
   /** @} */

} cl_connection_sub_state_type;


/** @brief How far one message has got
 *
 * Sending and receiving are the same three steps in the same order: the GMSH
 * length prefix, the MIH describing the message, then the payload. A message
 * can be half sent when a socket blocks, which is what these states exist to
 * remember.
 */
typedef enum cl_message_state_type {
   CL_MS_UNDEFINED = 1,   ///< Unset
   CL_MS_INIT_SND,        ///< About to send
   CL_MS_SND_GMSH,        ///< Sending the GMSH length prefix
   CL_MS_SND_MIH,         ///< Sending the message information header
   CL_MS_SND,             ///< Sending the payload
   CL_MS_INIT_RCV,        ///< About to receive
   CL_MS_RCV_GMSH,        ///< Reading the GMSH length prefix
   CL_MS_RCV_MIH,         ///< Reading the message information header
   CL_MS_RCV,             ///< Reading the payload
   CL_MS_READY,           ///< Complete; waiting for the application to take it
   CL_MS_PROTOCOL         ///< A protocol message the commlib handles itself, not one for the application. @warning Must stay the highest value
} cl_message_state_t;


/** @brief Which SSL method is handed to `SSL_CTX_new()` */
typedef enum cl_ssl_method_type {
   CL_SSL_v23 = 1,   ///< The negotiating method, accepting whatever the peer offers
   CL_SSL_TLS = 2    ///< TLS
} cl_ssl_method_t;

/** @brief What a #cl_ssl_verify_func_t is being asked to check */
typedef enum cl_ssl_verify_mode_type {
   CL_SSL_PEER_NAME = 1,   ///< The host name in the peer's certificate
   CL_SSL_USER_NAME = 2    ///< The user name in it
} cl_ssl_verify_mode_t;

/** @brief Whether certificates come from files or from memory
 *
 * Reading them from memory is what lets a process hold a certificate it never
 * wrote to disk.
 */
typedef enum cl_ssl_cert_mode_type {
   CL_SSL_PEM_FILE = 1,   ///< The `ssl_*_pem_file` members name files
   CL_SSL_PEM_BYTE = 2    ///< They hold the PEM data itself
} cl_ssl_cert_mode_t;

/** @brief Application hook deciding whether a peer's certificate is acceptable
 *
 * The commlib can check that a certificate is valid; only the application
 * knows whether *this* peer may connect.
 *
 * @param mode         what `value` is - a host name or a user name
 * @param service_mode true when we are the service being connected to
 * @param value        the name out of the peer's certificate
 *
 * @return true to admit the peer
 */
typedef bool (*cl_ssl_verify_func_t)(cl_ssl_verify_mode_t mode, bool service_mode, const char *value);

/** @brief How much a `qping -dump` debug client is sent */
typedef enum cl_debug_client_def {
   CL_DEBUG_CLIENT_OFF = 0,   ///< No debug client attached
   CL_DEBUG_CLIENT_ALL,       ///< Both message traffic and application messages
   CL_DEBUG_CLIENT_MSG,       ///< Message traffic only
   CL_DEBUG_CLIENT_APP        ///< Application messages only
} cl_debug_client_t;


/** @brief What an attached `qping -dump` client is being fed
 *
 * Created with `cl_com_create_debug_client_setup()` and released with
 * `cl_com_free_debug_client_setup()`.
 */
typedef struct cl_debug_client_setup_type {
   cl_debug_client_t dc_mode;      ///< How much to send
   bool dc_dump_flag;              ///< Include the message payload, not just the headers
   int dc_app_log_level;           ///< Application log level to forward
   cl_raw_list_t *dc_debug_list;   ///< Queue of lines waiting to go to the debug client
} cl_debug_client_setup_t;
/** @brief The certificates and keys a secure handle uses
 *
 * Created with `cl_com_create_ssl_setup()`, copied with
 * `cl_com_dup_ssl_setup()`, released with `cl_com_free_ssl_setup()`.
 *
 * Two generations of members live here side by side: the `SECURE` block is
 * the older SSL framework, the `OCS_WITH_OPENSSL` block the newer one. Which
 * is compiled depends on how the tree was built.
 *
 * @note The comment that used to stand here explained
 *       #cl_ssl_cert_mode_t by giving two branches with **identical**
 *       contents - `if PEM_FILE` and `else` said the same thing - so it
 *       explained nothing. What the mode really selects is documented on
 *       #cl_ssl_cert_mode_t itself.
 *
 * @todo `cl_com_create_ssl_setup()` does not fail when required parameters
 *       are missing, and does not check them.
 */
typedef struct cl_ssl_setup_type {
   cl_ssl_cert_mode_t ssl_cert_mode;   ///< Whether the members below are file names or PEM data
   cl_ssl_method_t ssl_method;         ///< Passed to `SSL_CTX_new()`
#if defined(SECURE)
   char *ssl_CA_cert_pem_file;   ///< CA certificate
   char *ssl_CA_key_pem_file;    ///< Private key of the CA. @warning Not used
   char *ssl_cert_pem_file;      ///< Our certificate
   char *ssl_key_pem_file;       ///< Our key
   char *ssl_rand_file;          ///< Entropy file, used when `RAND_status()` reports the pool is not seeded
   char *ssl_reconnect_file;     ///< Reconnect data. @warning Not used
   char *ssl_crl_file;           ///< Certificate revocation list
   unsigned long ssl_refresh_time;   ///< How long a service keeps a key alive. @warning Not used
   char *ssl_password;           ///< Password for an encrypted key file. @warning Not used
   cl_ssl_verify_func_t ssl_verify_func;   ///< Application hook checking the peer's name
#endif
#if defined(OCS_WITH_OPENSSL)
   char *ssl_client_cert_file;   ///< Client certificate used to verify the peer, if set
   char *ssl_server_cert_file;   ///< Server certificate identifying us
   char *ssl_server_key_file;    ///< Server key identifying us
   bool needs_client_cert;       ///< Fail handle creation when the client certificate cannot be read. Default true; the qmaster may set it false, because `act_qmaster` can name a host for which no certificate exists
#endif
} cl_ssl_setup_t;


/** @brief What a handle reports about itself
 *
 * This is what `qping -info` prints. The counters are per interval, reset at
 * `last_update`, not totals.
 *
 * @note `bytes_*` and `real_bytes_*` are not the same measurement:
 *       the plain ones count message payload, the `real_` ones count what
 *       actually went over the socket, protocol headers included.
 */
typedef struct cl_com_handle_statistic_type {
   struct timeval last_update;             ///< When the counters below were last reset
   unsigned long new_connections;          ///< Connections opened since then
   unsigned long access_denied;            ///< Connections refused since then
   unsigned long nr_of_connections;        ///< Connections open right now - a level, not a counter
   unsigned long bytes_sent;               ///< Payload bytes sent since then
   unsigned long bytes_received;           ///< Payload bytes received since then
   unsigned long real_bytes_sent;          ///< Bytes on the wire, headers included
   unsigned long real_bytes_received;      ///< Bytes off the wire, headers included
   unsigned long unsend_message_count;     ///< Messages queued to send
   unsigned long unread_message_count;     ///< Messages received and waiting for the application to take them
   unsigned long application_status;       ///< Whatever the application's #cl_app_status_func_t returned
   char *application_info;                 ///< The text that came with it
} cl_com_handle_statistic_t;

typedef struct cl_com_connection_type cl_com_connection_t;   ///< One connection, see #cl_com_connection_type

/** @brief One participant in the cluster's communication
 *
 * A handle is what an application creates to take part: the qmaster has one
 * that provides a service, every client has one that does not. It owns the
 * connections, the message queues, the threads if there are any, and every
 * timeout that governs them.
 *
 * `connect_port` and `service_port` are the giveaway for which kind it is -
 * exactly one of them is non-zero.
 */
typedef struct cl_com_handle {
   cl_ssl_setup_t *ssl_setup;         ///< Certificates and keys, for a secure framework
#if defined(OCS_WITH_OPENSSL)
   ocs::uti::OpenSSL::OpenSSLContext *ssl_server_context;   ///< SSL context for incoming connections, where we are the server
   ocs::uti::OpenSSL::OpenSSLContext *ssl_client_context;   ///< SSL context for outgoing connections, where we are the client
#endif

   cl_debug_client_setup_t *debug_client_setup;   ///< Set while a `qping -dump` client is attached

   cl_framework_t framework;                  ///< Transport this handle and its connections use
   cl_tcp_connect_t tcp_connect_mode;         ///< Whether outgoing connects use a reserved port
   cl_xml_connection_type_t data_flow_type;   ///< Stream or message oriented
   bool service_provider;                     ///< This handle listens for clients

   int connect_port;   ///< Port we connect to. @warning Exactly one of this and #service_port is non-zero
   int service_port;   ///< Port we listen on. @warning Exactly one of this and #connect_port is non-zero

   cl_com_endpoint_t *local;               ///< Who we are: component name, id and host
   cl_com_handle_statistic_t *statistic;   ///< What `qping -info` reports

   /** @name Only used in #CL_RW_THREAD mode
    * @{ */
   cl_thread_condition_t *app_condition;    ///< Signalled by the read thread when a message is ready for the application
   cl_thread_condition_t *read_condition;   ///< Wakes the read thread
   cl_thread_condition_t *write_condition;  ///< Wakes the write thread
   cl_thread_settings_t *service_thread;    ///< Accepts connections and does the periodic housekeeping
   cl_thread_settings_t *read_thread;       ///< Reads from connections
   cl_thread_settings_t *write_thread;      ///< Writes to connections
   /** @} */

   pthread_mutex_t *messages_ready_mutex;   ///< Guards #messages_ready_for_read
   unsigned long messages_ready_for_read;   ///< How many received messages are waiting, so the application need not walk the lists to find out

   pthread_mutex_t *connection_list_mutex;  ///< Guards #connection_list
   cl_raw_list_t *connection_list;          ///< Every connection of this handle
   cl_raw_list_t *allowed_host_list;        ///< Host names allowed to connect; empty means no restriction
   cl_raw_list_t *file_descriptor_list;     ///< Application descriptors registered with #cl_com_fd_data_t
   unsigned long next_free_client_id;       ///< Id handed to the next accepted client

   cl_raw_list_t *send_message_queue;       ///< Messages handed over by `cl_commlib_send_message()`, not yet on a connection
   cl_raw_list_t *received_message_queue;   ///< Messages ready for `cl_commlib_receive_message()`. @warning Entries reference connections and must be cleaned up when one is destroyed - the service thread does this periodically

   /** @name Limits
    * @{ */
   unsigned long max_open_connections;   ///< How many connections this handle accepts at once
   cl_max_count_t max_con_close_mode;    ///< What to do when that number is reached
   cl_xml_connection_autoclose_t auto_close_mode;   ///< Whether connections this handle opens may be closed by the service to make room
   int max_write_threads;   ///< Upper bound on write threads
   int max_read_threads;    ///< Upper bound on read threads
   /** @} */

   /** @name Timeouts, all in seconds unless the name says otherwise
    * @{ */
   int select_sec_timeout;         ///< How long one `poll()` waits
   int select_usec_timeout;        ///< Microseconds added to that
   int connection_timeout;         ///< Drop a connected client that sends nothing for this long
   int close_connection_timeout;   ///< How long unread messages survive after a connection closed
   int read_timeout;               ///< Give up on a read that makes no progress
   int write_timeout;              ///< Give up on a write that makes no progress
   int open_connection_timeout;    ///< Give up on a connect
   int acknowledge_timeout;        ///< Give up waiting for an acknowledgement
   int message_timeout;            ///< Discard a received message the application never took
   /** @} */
   int synchron_receive_timeout;   ///< How long a synchronous receive waits
   int last_heard_from_timeout;    ///< @warning Do not use; kept for compatibility only

   /** @name Service state
    * @{ */
   int do_shutdown;                            ///< Set when this handle is shutting down
   bool max_connection_count_reached;          ///< #max_open_connections is exhausted
   bool max_connection_count_found_connection_to_close;   ///< A closable connection was found while the limit was reached
   cl_com_connection_t *last_receive_message_connection;  ///< Where `cl_commlib_receive_message()` stopped last time, so the next call continues round-robin instead of always starting at the first connection
   long shutdown_timeout;                      ///< How long the shutdown may take
   cl_com_connection_t *service_handler;       ///< The listening connection, when #service_provider is set
   struct timeval start_time;                  ///< When the handle was created
   struct timeval last_statistic_update_time;  ///< Service thread: when the statistics were last recalculated
   struct timeval last_message_queue_cleanup_time;   ///< Service thread: when the received queue was last swept
   /** @} */
} cl_com_handle_t;

/** @brief The `poll()` arrays, kept allocated between calls
 *
 * Two parallel arrays: `poll_array[i]` is the descriptor and `poll_con[i]`
 * the connection it belongs to, so the result of `poll()` maps straight back
 * without a search. They are grown rather than rebuilt, which is what
 * `poll_fd_count` remembers.
 */
typedef struct cl_com_poll {
   struct pollfd *poll_array;        ///< The descriptors handed to `poll()`
   cl_com_connection_t **poll_con;   ///< The connection each entry belongs to
   unsigned long poll_fd_count;      ///< How many entries are allocated in both arrays
} cl_com_poll_t;

/** @brief A `struct hostent` the commlib owns and frees itself
 *
 * A wrapper so that a resolver result can be put in a list and released like
 * anything else; `struct hostent` from `netdb.h` has no owner of its own.
 */
typedef struct cl_com_hostent {
   struct hostent *he;   ///< The resolver result

} cl_com_hostent_t;

/** @brief One entry of the host name cache
 *
 * Name resolution is on the path of every connection, so results are cached -
 * including the failures, which is what `resolve_error` is for: a host that
 * does not resolve must not be looked up again on every attempt.
 */
typedef struct cl_com_host_spec_type {
   cl_com_hostent_t *hostent;   ///< The resolver result, if the lookup succeeded
   struct in_addr *in_addr;     ///< The address
   char *unresolved_name;       ///< The name as the caller gave it
   char *resolved_name;         ///< The name the resolver returned
   int resolve_error;           ///< The `CL_RETVAL_*` code from `cl_com_gethostbyname()`
   long last_resolve_time;      ///< When the lookup was last done, for expiry
   long creation_time;          ///< When this entry was created

} cl_com_host_spec_t;

/** @brief Application hook for a descriptor the commlib does not own
 *
 * Lets an application put its own descriptors into the commlib's `poll()`
 * instead of running a second event loop beside it.
 *
 * @param fd          the descriptor
 * @param read_ready  it is readable
 * @param write_ready it is writable
 * @param user_data   whatever was registered along with it
 * @param err_val     non-zero when `poll()` reported an error on it
 *
 * @return #CL_RETVAL_OK to keep the descriptor registered
 *
 * @warning Any other return value **removes the descriptor from the list**.
 */
typedef int   (*cl_fd_func_t)(int fd, bool read_ready, bool write_ready, void *user_data, int err_val);

/** @brief One registered application descriptor */
typedef struct cl_com_fd_data_type {
   int fd;                            ///< The descriptor
   cl_select_method_t select_mode;    ///< What it is watched for
   bool read_ready;                   ///< `poll()` says it is readable
   bool write_ready;                  ///< `poll()` says it is writable
   bool ready_for_writing;            ///< The application says it has something to write
   cl_fd_func_t callback;             ///< Called when it becomes ready
   void *user_data;                   ///< Passed back to the callback
} cl_com_fd_data_t;


/** @brief One message, in flight or waiting
 *
 * The same struct is used in both directions. `message_snd_pointer` and
 * `message_rcv_pointer` are how a half-transferred message survives a socket
 * that blocked: they say how much of `message` has already gone out or come
 * in.
 */
typedef struct cl_com_message_type {
   cl_message_state_t message_state;      ///< How far it has got
   cl_xml_mih_data_format_t message_df;   ///< Data format from the MIH: binary, XML, or one of the protocol messages
   cl_xml_ack_type_t message_mat;         ///< Acknowledge type: none, on receipt, or after the application processed it
   int message_ack_flag;                  ///< Set once the acknowledgement is done
   cl_com_SIRM_t *message_sirm;           ///< Set when this message is the response to a SIM, i.e. a `qping` answer
   unsigned long message_tag;             ///< The application's own tag
   unsigned long message_id;              ///< Id of this message
   unsigned long message_response_id;     ///< The id this message answers, if it answers one
   unsigned long message_length;          ///< Payload length
   unsigned long message_snd_pointer;     ///< How much has been written out so far
   unsigned long message_rcv_pointer;     ///< How much has been read in so far
   struct timeval message_receive_time;   ///< When it arrived
   struct timeval message_remove_time;    ///< When it may be discarded unread
   struct timeval message_send_time;      ///< When it went out
   struct timeval message_insert_time;    ///< When it was queued
   cl_byte_t *message;                    ///< The payload
} cl_com_message_t;


/** @brief The same counters as #cl_com_handle_statistic_t, per connection */
typedef struct cl_com_con_statistic_type {
   struct timeval last_update;        ///< When the counters below were last reset
   unsigned long bytes_sent;          ///< Payload bytes sent since then
   unsigned long bytes_received;      ///< Payload bytes received since then
   unsigned long real_bytes_sent;     ///< Bytes on the wire, headers included
   unsigned long real_bytes_received; ///< Bytes off the wire, headers included
} cl_com_con_statistic_t;


/** @brief One connection of a #cl_com_handle_t
 *
 * Three endpoints rather than two: #local and #remote are the connection as
 * it exists, while #client_dst is who the client *asked* for in its CM. They
 * differ when a client reached us under a name we do not call ourselves,
 * which is what makes the mismatch diagnosable instead of just a refusal.
 */
struct cl_com_connection_type {

   bool check_endpoint_flag;   ///< A SIM is out asking whether the peer is still there; set means one is already in flight, so a second is not sent
   unsigned long check_endpoint_mid;   ///< Message id of that SIM

   cl_error_func_t error_func;         ///< Application hook called on errors, may be nullptr
   cl_tag_name_func_t tag_name_func;   ///< Application hook turning a numeric tag into a name for debug clients, may be nullptr

   cl_com_endpoint_t *remote;      ///< The peer
   cl_com_endpoint_t *local;       ///< Us
   cl_com_endpoint_t *client_dst;  ///< Who the client addressed in its CM, which need not be #local

   unsigned long last_send_message_id;       ///< Id given to the last message sent
   unsigned long last_received_message_id;   ///< Id of the last message received

   cl_raw_list_t *received_message_list;   ///< Messages read off this connection
   cl_raw_list_t *send_message_list;       ///< Messages queued for this connection

   cl_com_handle_t *handler;   ///< The handle this connection belongs to


   cl_framework_t framework_type;               ///< Transport, which decides whose read/write functions are used
   cl_tcp_connect_t tcp_connect_mode;           ///< Whether the socket was created on a reserved port
   cl_connection_t connection_type;             ///< Which directions this connection carries
   cl_service_handler_t service_handler_flag;   ///< Whether this is the listening socket or a peer
   cl_data_ready_flag_t data_write_flag;        ///< There is something to write
   cl_data_ready_flag_t fd_ready_for_write;     ///< ...and `poll()` says the socket will take it. Set by `cl_com_open_connection_request_handler()`
   cl_data_ready_flag_t data_read_flag;         ///< There is something to read

   cl_connection_state_t connection_state;              ///< Where the connection is
   cl_connection_sub_state_type connection_sub_state;   ///< Where inside that state; only meaningful together with it

   bool was_accepted;    ///< Came in through `accept()`
   bool was_opened;      ///< Went out through `connect()`
   char *client_host_name;   ///< The peer's resolved host name
   cl_xml_connection_status_t crm_state;   ///< What we answered, or were answered, in the CRM
   char *crm_state_error;    ///< Why, when #crm_state is a denial - this is the text a rejected client is told

   /** @name Data flow
    * @{ */
   cl_xml_connection_type_t data_flow_type;   ///< Stream or message oriented
   cl_xml_data_format_t data_format_type;     ///< Binary or XML payload
   /** @} */

   /** @name The read and write buffers
    *
    * Both buffers carry three positions rather than one, because filling and
    * consuming are separate steps: bytes arrive into `_pos`, and `_processed`
    * follows behind as they are parsed out.
    * @{ */
   unsigned long data_buffer_size;      ///< Size of both buffers
   cl_byte_t *data_read_buffer;         ///< Buffer bytes are read into
   cl_byte_t *data_write_buffer;        ///< Buffer bytes are written from
   cl_com_GMSH_t *read_gmsh_header;     ///< The length prefix of the message currently being read

   long read_buffer_timeout_time;       ///< When the read in progress gives up
   long write_buffer_timeout_time;      ///< When the write in progress gives up

   unsigned long data_write_buffer_pos;         ///< How far the buffer is filled
   unsigned long data_write_buffer_processed;   ///< How much of that has gone out
   unsigned long data_write_buffer_to_send;     ///< Position of the last byte that has to go
   unsigned long data_read_buffer_pos;          ///< How far the buffer is filled
   unsigned long data_read_buffer_processed;    ///< How much of that has been parsed out
   /** @} */

   struct timeval last_transfer_time;        ///< Last time anything moved, which is what #cl_com_handle::connection_timeout is measured against
   struct timeval connection_close_time;     ///< When the CCRM arrived
   struct timeval connection_connect_time;   ///< When the CM/CRM handshake completed
   long shutdown_timeout;                    ///< How long this connection's shutdown may take

   cl_com_con_statistic_t *statistic;   ///< Byte counters for this connection
   cl_xml_connection_autoclose_t auto_close_type;   ///< Whether the service may close this connection to make room
   bool is_read_selected;    ///< A read thread is working on it. @warning While set the connection must not be deleted
   bool is_write_selected;   ///< A write thread is working on it. @warning While set the connection must not be deleted

   void *com_private;   ///< The framework's own per connection data - the socket for TCP, the `SSL *` for SSL
};
