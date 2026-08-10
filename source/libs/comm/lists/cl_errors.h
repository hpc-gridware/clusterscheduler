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
 * @brief The commlib return codes and their texts
 *
 * Every commlib function returns one of these rather than `errno`, so that a
 * caller can tell a communication failure from a system one. #cl_get_error_text
 * turns a code into the sentence a user sees.
 *
 * The descriptions below are the message texts themselves, taken from
 * `msg_commlistslib.h` through the mapping in `cl_errors.cc` - not written by
 * hand, so they cannot drift from what the program actually prints.
 */

/** @brief Where the commlib code range starts
 *
 * The codes are offsets from here so that they never collide with `errno`
 * values, which #cl_is_commlib_error relies on.
 */
#define CL_RETVAL_START_ID 1000
#define CL_RETVAL_OK                          (CL_RETVAL_START_ID + 0)   ///< No error happened
#define CL_RETVAL_MALLOC                      (CL_RETVAL_START_ID + 1)   ///< Can't allocate memory
#define CL_RETVAL_PARAMS                      (CL_RETVAL_START_ID + 2)   ///< Got unexpected parameters
#define CL_RETVAL_UNKNOWN                     (CL_RETVAL_START_ID + 3)   ///< Can't report a reason
#define CL_RETVAL_MUTEX_ERROR                 (CL_RETVAL_START_ID + 4)   ///< Got general mutex error
#define CL_RETVAL_MUTEX_CLEANUP_ERROR         (CL_RETVAL_START_ID + 5)   ///< Can't cleanup mutex
#define CL_RETVAL_MUTEX_LOCK_ERROR            (CL_RETVAL_START_ID + 6)   ///< Can't lock mutex
#define CL_RETVAL_MUTEX_UNLOCK_ERROR          (CL_RETVAL_START_ID + 7)   ///< Can't unlock mutex
#define CL_RETVAL_CONDITION_ERROR             (CL_RETVAL_START_ID + 8)   ///< Got general thread condition error
#define CL_RETVAL_CONDITION_CLEANUP_ERROR     (CL_RETVAL_START_ID + 9)   ///< Can't cleanup thread condition
#define CL_RETVAL_CONDITION_WAIT_TIMEOUT      (CL_RETVAL_START_ID + 10)   ///< Timeout while waiting for thread condition
#define CL_RETVAL_CONDITION_SIGNAL_ERROR      (CL_RETVAL_START_ID + 11)   ///< Received a signal while waiting for thread condition
#define CL_RETVAL_THREAD_CREATE_ERROR         (CL_RETVAL_START_ID + 12)   ///< Can't create thread
#define CL_RETVAL_THREAD_START_TIMEOUT        (CL_RETVAL_START_ID + 13)   ///< Timeout while waiting for thread start
#define CL_RETVAL_THREAD_NOT_FOUND            (CL_RETVAL_START_ID + 14)   ///< Can't find thread
#define CL_RETVAL_THREAD_JOIN_ERROR           (CL_RETVAL_START_ID + 15)   ///< Got thread join error
#define CL_RETVAL_THREAD_CANCELSTATE_ERROR    (CL_RETVAL_START_ID + 16)   ///< Got unexpected thread cancel state
#define CL_RETVAL_LOG_NO_LOGLIST              (CL_RETVAL_START_ID + 17)   ///< No log list found
#define CL_RETVAL_CONNECTION_NOT_FOUND        (CL_RETVAL_START_ID + 18)   ///< Can't find connection
#define CL_RETVAL_HANDLE_NOT_FOUND            (CL_RETVAL_START_ID + 19)   ///< Can't find handle
#define CL_RETVAL_THREADS_ENABLED             (CL_RETVAL_START_ID + 20)   ///< Threads are enabled
#define CL_RETVAL_NO_MESSAGE                  (CL_RETVAL_START_ID + 21)   ///< Got no message
#define CL_RETVAL_CREATE_SOCKET               (CL_RETVAL_START_ID + 22)   ///< Can't create socket
#define CL_RETVAL_CONNECT_ERROR               (CL_RETVAL_START_ID + 23)   ///< Can't connect to service
#define CL_RETVAL_CONNECT_TIMEOUT             (CL_RETVAL_START_ID + 24)   ///< Got connect timeout
#define CL_RETVAL_NOT_OPEN                    (CL_RETVAL_START_ID + 25)   ///< Not open error
#define CL_RETVAL_SEND_ERROR                  (CL_RETVAL_START_ID + 26)   ///< Got send error
#define CL_RETVAL_BIND_SOCKET                 (CL_RETVAL_START_ID + 27)   ///< Can't bind socket
#define CL_RETVAL_SELECT_ERROR                (CL_RETVAL_START_ID + 28)   ///< Got select error
/* #define CL_RETVAL_RECEIVE_ERROR removed ! */
#define CL_RETVAL_PIPE_ERROR                  (CL_RETVAL_START_ID + 29)   ///< Got pipe error
#define CL_RETVAL_GETHOSTNAME_ERROR           (CL_RETVAL_START_ID + 30)   ///< Can't resolve host name
#define CL_RETVAL_SEND_TIMEOUT                (CL_RETVAL_START_ID + 31)   ///< Got send timeout
#define CL_RETVAL_READ_TIMEOUT                (CL_RETVAL_START_ID + 32)   ///< Got read timeout
#define CL_RETVAL_UNDEFINED_FRAMEWORK         (CL_RETVAL_START_ID + 33)   ///< Framework is not defined
#define CL_RETVAL_NOT_SERVICE_HANDLER         (CL_RETVAL_START_ID + 34)   ///< Handle is not defined as service handler
#define CL_RETVAL_NO_FRAMEWORK_INIT           (CL_RETVAL_START_ID + 35)   ///< Framework is not initialized
#define CL_RETVAL_SETSOCKOPT_ERROR            (CL_RETVAL_START_ID + 36)   ///< Can't set socket options
#define CL_RETVAL_FCNTL_ERROR                 (CL_RETVAL_START_ID + 37)   ///< Got fcntl error
#define CL_RETVAL_LISTEN_ERROR                (CL_RETVAL_START_ID + 38)   ///< Got listen error
#define CL_RETVAL_NEED_EMPTY_FRAMEWORK        (CL_RETVAL_START_ID + 39)   ///< Framework is not uninitalized
#define CL_RETVAL_LOCK_ERROR                  (CL_RETVAL_START_ID + 40)   ///< Can't lock error
#define CL_RETVAL_UNLOCK_ERROR                (CL_RETVAL_START_ID + 41)   ///< Can't unlock error
#define CL_RETVAL_WRONG_FRAMEWORK             (CL_RETVAL_START_ID + 42)   ///< Used wrong framework
#define CL_RETVAL_READ_ERROR                  (CL_RETVAL_START_ID + 43)   ///< Got read error
#define CL_RETVAL_MAX_READ_SIZE               (CL_RETVAL_START_ID + 44)   ///< Max read size reached
#define CL_RETVAL_CLIENT_WELCOME_ERROR        (CL_RETVAL_START_ID + 45)   ///< Got client welcome error
#define CL_RETVAL_UNKNOWN_HOST_ERROR           (CL_RETVAL_START_ID + 46)   ///< Unknown host error
#define CL_RETVAL_LOCAL_HOSTNAME_ERROR        (CL_RETVAL_START_ID + 47)   ///< Local host name error
#define CL_RETVAL_UNKNOWN_ENDPOINT            (CL_RETVAL_START_ID + 48)   ///< Unknown endpoint error
#define CL_RETVAL_UNCOMPLETE_WRITE            (CL_RETVAL_START_ID + 49)   ///< Couldn't write all data
#define CL_RETVAL_UNCOMPLETE_READ             (CL_RETVAL_START_ID + 50)   ///< Couldn't read all data
#define CL_RETVAL_LIST_DATA_NOT_EMPTY         (CL_RETVAL_START_ID + 51)   ///< List data is not empty
#define CL_RETVAL_LIST_NOT_EMPTY              (CL_RETVAL_START_ID + 52)   ///< List is not empty
#define CL_RETVAL_LIST_DATA_IS_NULL           (CL_RETVAL_START_ID + 53)   ///< List data is not initalized
#define CL_RETVAL_THREAD_SETSPECIFIC_ERROR    (CL_RETVAL_START_ID + 54)   ///< Got error setting thread specific data
#define CL_RETVAL_NOT_THREAD_SPECIFIC_INIT    (CL_RETVAL_START_ID + 55)   ///< Could not initialize thread specific data
#define CL_RETVAL_ALLREADY_CONNECTED          (CL_RETVAL_START_ID + 56)   ///< Already connected error
#define CL_RETVAL_STREAM_BUFFER_OVERFLOW      (CL_RETVAL_START_ID + 57)   ///< Got stream buffer overflow
#define CL_RETVAL_GMSH_ERROR                  (CL_RETVAL_START_ID + 58)   ///< Can't read general message size header (GMSH)
#define CL_RETVAL_MESSAGE_ACK_ERROR           (CL_RETVAL_START_ID + 59)   ///< Got message acknowledge error
#define CL_RETVAL_MESSAGE_WAIT_FOR_ACK        (CL_RETVAL_START_ID + 60)   ///< Message is not acknowledged
#define CL_RETVAL_ENDPOINT_NOT_UNIQUE         (CL_RETVAL_START_ID + 61)   ///< Endpoint is not unique error
#define CL_RETVAL_SYNC_RECEIVE_TIMEOUT        (CL_RETVAL_START_ID + 62)   ///< Got syncron message receive timeout error
#define CL_RETVAL_MAX_MESSAGE_LENGTH_ERROR    (CL_RETVAL_START_ID + 63)   ///< Reached max message length
#define CL_RETVAL_RESOLVING_SETUP_ERROR       (CL_RETVAL_START_ID + 64)   ///< Resolve setup error
#define CL_RETVAL_IP_NOT_RESOLVED_ERROR       (CL_RETVAL_START_ID + 65)   ///< Can't resolve ip address
#define CL_RETVAL_MESSAGE_IN_BUFFER           (CL_RETVAL_START_ID + 66)   ///< Still messages in buffer
#define CL_RETVAL_CONNECTION_GOING_DOWN       (CL_RETVAL_START_ID + 67)   ///< Connection is going down
#define CL_RETVAL_CONNECTION_STATE_ERROR      (CL_RETVAL_START_ID + 68)   ///< General connection state error
#define CL_RETVAL_SELECT_TIMEOUT              (CL_RETVAL_START_ID + 69)   ///< Got select timeout
#define CL_RETVAL_SELECT_INTERRUPT            (CL_RETVAL_START_ID + 70)   ///< Select was interrupted
#define CL_RETVAL_NO_SELECT_DESCRIPTORS       (CL_RETVAL_START_ID + 71)   ///< No file descriptors for select available
#define CL_RETVAL_ALIAS_EXISTS                (CL_RETVAL_START_ID + 72)   ///< Alias is already existing
#define CL_RETVAL_NO_ALIAS_FILE               (CL_RETVAL_START_ID + 73)   ///< No alias file specified
#define CL_RETVAL_ALIAS_FILE_NOT_FOUND        (CL_RETVAL_START_ID + 74)   ///< Could not get alias file
#define CL_RETVAL_OPEN_ALIAS_FILE_FAILED      (CL_RETVAL_START_ID + 75)   ///< Could not open alias file
#define CL_RETVAL_ALIAS_VERSION_ERROR         (CL_RETVAL_START_ID + 76)   ///< Wrong alias file version
#define CL_RETVAL_SECURITY_ANNOUNCE_FAILED    (CL_RETVAL_START_ID + 77)   ///< Security announce failed
#define CL_RETVAL_SECURITY_SEND_FAILED        (CL_RETVAL_START_ID + 78)   ///< Security send failed
#define CL_RETVAL_SECURITY_RECEIVE_FAILED     (CL_RETVAL_START_ID + 79)   ///< Security receive failed
#define CL_RETVAL_ACCESS_DENIED               (CL_RETVAL_START_ID + 80)   ///< Access denied
#define CL_RETVAL_MAX_CON_COUNT_REACHED       (CL_RETVAL_START_ID + 81)   ///< Max. connection count reached
#define CL_RETVAL_GETHOSTADDR_ERROR           (CL_RETVAL_START_ID + 82)   ///< Can't resolve ip address
#define CL_RETVAL_NO_PORT_ERROR               (CL_RETVAL_START_ID + 83)   ///< No valid port number
#define CL_RETVAL_PROTOCOL_ERROR              (CL_RETVAL_START_ID + 84)   ///< Can't send response for this message id - protocol error
#define CL_RETVAL_LOCAL_ENDPOINT_NOT_UNIQUE   (CL_RETVAL_START_ID + 85)   ///< Local endpoint is not unique
#define CL_RETVAL_TO_LESS_FILEDESCRIPTORS     (CL_RETVAL_START_ID + 86)   ///< Operating system provides to less file descriptors
#define CL_RETVAL_DEBUG_CLIENTS_NOT_ENABLED   (CL_RETVAL_START_ID + 87)   ///< Debug client mode not active
#define CL_RETVAL_CREATE_RESERVED_PORT_SOCKET (CL_RETVAL_START_ID + 88)   ///< Can't create reserved port socket
#define CL_RETVAL_NO_RESERVED_PORT_CONNECTION (CL_RETVAL_START_ID + 89)   ///< Client did not use reserved port < 1024
#define CL_RETVAL_NO_LOCAL_HOST_CONNECTION    (CL_RETVAL_START_ID + 90)   ///< Client is not connected from local host
#define CL_RETVAL_UNEXPECTED_CHARACTERS       (CL_RETVAL_START_ID + 91)   ///< Got unexpected characters or values
#define CL_RETVAL_SSL_COULD_NOT_SET_METHOD         (CL_RETVAL_START_ID + 92)   ///< Can't set ssl method
#define CL_RETVAL_SSL_COULD_NOT_CREATE_CONTEXT     (CL_RETVAL_START_ID + 93)   ///< Can't create ssl context
#define CL_RETVAL_SSL_COULD_NOT_SET_CA_CHAIN_FILE  (CL_RETVAL_START_ID + 94)   ///< Can't set CA chain file
#define CL_RETVAL_SSL_CANT_SET_CA_KEY_PEM_FILE     (CL_RETVAL_START_ID + 95)   ///< Can't set private key pem file
#define CL_RETVAL_SSL_CANT_READ_CA_LIST            (CL_RETVAL_START_ID + 96)   ///< Can't read trusted CA certificates file(s)
#define CL_RETVAL_SSL_NO_SYMBOL_TABLE              (CL_RETVAL_START_ID + 97)   ///< No symbol table declared
#define CL_RETVAL_SSL_SYMBOL_TABLE_ALREADY_LOADED  (CL_RETVAL_START_ID + 98)   ///< Symbol table already loaded
#define CL_RETVAL_SSL_DLOPEN_SSL_LIB_FAILED        (CL_RETVAL_START_ID + 99)   ///< Can't open ssl library
#define CL_RETVAL_SSL_CANT_LOAD_ALL_FUNCTIONS      (CL_RETVAL_START_ID + 100)   ///< Can't load ssl library function
#define CL_RETVAL_SSL_SHUTDOWN_ERROR               (CL_RETVAL_START_ID + 101)   ///< Ssl shutdown error
#define CL_RETVAL_SSL_CANT_CREATE_SSL_OBJECT       (CL_RETVAL_START_ID + 102)   ///< Can't create ssl object
#define CL_RETVAL_SSL_CANT_CREATE_BIO_SOCKET       (CL_RETVAL_START_ID + 103)   ///< Can't create bio socket
#define CL_RETVAL_SSL_ACCEPT_HANDSHAKE_TIMEOUT     (CL_RETVAL_START_ID + 104)   ///< Ssl accept handshake timeout
#define CL_RETVAL_SSL_ACCEPT_ERROR                 (CL_RETVAL_START_ID + 105)   ///< Ssl accept error
#define CL_RETVAL_SSL_CONNECT_HANDSHAKE_TIMEOUT    (CL_RETVAL_START_ID + 106)   ///< Ssl connect handshake timeout
#define CL_RETVAL_SSL_CONNECT_ERROR                (CL_RETVAL_START_ID + 107)   ///< Ssl connect error
#define CL_RETVAL_SSL_CERTIFICATE_ERROR            (CL_RETVAL_START_ID + 108)   ///< Ssl certificate error
#define CL_RETVAL_SSL_PEER_CERTIFICATE_ERROR       (CL_RETVAL_START_ID + 109)   ///< Ssl peer certificate error
#define CL_RETVAL_SSL_GET_SSL_ERROR                (CL_RETVAL_START_ID + 110)   ///< Ssl error
#define CL_RETVAL_SSL_NO_SERVICE_PEER_NAME         (CL_RETVAL_START_ID + 111)   ///< Got no expected peer name for service certificate check
#define CL_RETVAL_SSL_RAND_SEED_FAILURE            (CL_RETVAL_START_ID + 112)   ///< PRNG hasn't been seeded with enough data
#define CL_RETVAL_SSL_NOT_SUPPORTED                (CL_RETVAL_START_ID + 113)   ///< SSL module not compiled with -DSECURE (aimk -secure) option
#define CL_RETVAL_ERROR_SETTING_CIPHER_LIST        (CL_RETVAL_START_ID + 114)   ///< Error setting cipher list
#define CL_RETVAL_REACHED_FILEDESCRIPTOR_LIMIT     (CL_RETVAL_START_ID + 115)   ///< File descriptor exeeds FD_SETSIZE of this system
#define CL_RETVAL_HOSTNAME_LENGTH_ERROR            (CL_RETVAL_START_ID + 116)   ///< Hostname exeeds hostname length(MAXHOSTNAMELEN) on this system
#define CL_RETVAL_HANDLE_SHUTDOWN_IN_PROGRESS      (CL_RETVAL_START_ID + 117)   ///< Handle shutdown in progress
#define CL_RETVAL_COMMLIB_SETUP_ALREADY_CALLED     (CL_RETVAL_START_ID + 118)   ///< Cl_com_setup_commlib() processed twice
#define CL_RETVAL_DO_IGNORE                        (CL_RETVAL_START_ID + 119)   ///< Value is ignored
#define CL_RETVAL_CLOSE_ALIAS_FILE_FAILED          (CL_RETVAL_START_ID + 120)   ///< Could not close alias file
#define CL_RETVAL_SSL_CANT_SET_CERT_PEM_BYTE       (CL_RETVAL_START_ID + 121)   ///< Can't set certificate bytes
#define CL_RETVAL_SSL_SET_CERT_PEM_BYTE_IS_NULL    (CL_RETVAL_START_ID + 122)   ///< Certificate bytes are nullptr
#define CL_RETVAL_SSL_CANT_SET_KEY_PEM_BYTE        (CL_RETVAL_START_ID + 123)   ///< Can't set key bytes
#define CL_RETVAL_UNKNOWN_PARAMETER                (CL_RETVAL_START_ID + 124)   ///< Parameter not found
/** @warning Has no case in #cl_get_error_text, so a caller that hits it is
 *           told `"undefined commlib error code"` and nothing more. It is
 *           returned from eight places - `cl_tcp_framework.cc`,
 *           `cl_ssl_framework.cc` and `cl_commlib.cc` - whenever `dup()` on a
 *           socket fails.
 */
#define CL_RETVAL_DUP_SOCKET_FD_ERROR              (CL_RETVAL_START_ID + 125)

/** @warning Has no case in #cl_get_error_text either, same consequence.
 *           Returned by `cl_ssl_framework.cc` when the OpenSSL library path
 *           cannot be determined.
 */
#define CL_RETVAL_SSL_CANT_GET_LIB_PATH            (CL_RETVAL_START_ID + 126)

/** @brief One past the highest code; the end of the range #cl_is_commlib_error tests
 *
 * @warning Must stay the last number + 1.
 */
#define CL_RETVAL_LAST_ID                     (CL_RETVAL_START_ID + 127)

/** @brief What #cl_get_error_text returns for a code it has no case for */
#define CL_RETVAL_UNDEFINED_STR "undefined commlib error code"

int cl_is_commlib_error(int error_id);

const char *cl_get_error_text(int error_id);
