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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The commlib's wire protocol: nine XML messages and their parsers
 *
 * Everything the two sides of a connection say to each other. Each message
 * appears three times in this file: as the format string that builds it, as
 * the struct a parser fills, and as the `cl_xml_parse_*` function that does
 * the filling.
 *
 * Reading order on the wire is always the same - a #CL_GMSH_MESSAGE giving
 * the byte length, then the message itself. That is why #cl_com_read_GMSH is
 * the first step of every read.
 *
 * @see @ref cl_communication.h, which drives these parsers
 */

#include <arpa/inet.h>

#define CL_DEFINE_MAX_MESSAGE_LENGTH                 1024 * 1024 * 1024   ///< Largest message the commlib accepts, 1 GB - a longer one is refused rather than allocated



/** @name GMSH - Length prefix - every message is preceded by one
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <gmsh>
 *       <dl>GMSH_DL</dl>
 *    </gmsh>
 *
 *    GMSH_DL:     Data Length of the following XML struct in byte. (1 Byte = 8 bit)
 * @endcode
 * @{
 */
#define CL_GMSH_MESSAGE      "<gmsh><dl>%ld</dl></gmsh>"   ///< The GMSH as a format string
#define CL_GMSH_MESSAGE_SIZE 22   ///< Length of the GMSH format string without its parameters, for sizing the buffer
/** @} */

/** @name MIH - Message Information Header - what follows the prefix
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <mih version="MIH_VERSION">
 *       <mid>MIH_MID</mid>
 *       <dl>MIH_DL</dl>
 *       <df>MIH_DF</df>
 *       <mat>MIH_MAT</mat>
 *       <tag>MIH_TAG</tag>
 *       <rid>MIH_RID</rid>
 *    </mih>
 *
 *    MIH_VERSION: Version number of MIH (e.g. "0.4")
 *    MIH_MID:     Message ID (unsigned)
 *    MIH_DL:      Data Length of the following XML struct in byte
 *    MIH_DF:      Data Format of the following message:
 *                    "bin"  = binary message  "xml" (Binary or XML data encoding)
 *                    "xml"  = general xml message
 *                    "am"   = acknowledge message (AM)
 *                    "sim"  = status information message (SIM)
 *                    "sirm" = status information response message (SIRM)
 *                    "ccm"  = connection close message (CCM)
 *                    "ccrm" = connection close response message (CCRM)
 *    MIH_MAT:     Message Acknowledge Type:
 *                    "nak"  = not acknowledged
 *                    "ack"  = acknowledged after commlib has read the message
 *                    "sync" = acknowledged after application has read the message from NGC AND processed the message
 *    MIH_TAG:     User defined
 *    MIH_RID:     Response ID
 * @endcode
 * @{
 */
#define CL_MIH_MESSAGE          "<mih version=\"%s\"><mid>%ld</mid><dl>%ld</dl><df>%s</df><mat>%s</mat><tag>%ld</tag><rid>%ld</rid></mih>"   ///< The MIH as a format string
#define CL_MIH_MESSAGE_VERSION  "0.1"   ///< Protocol version this side writes into the MIH
#define CL_MIH_MESSAGE_SIZE     87   ///< Length of the MIH format string without its parameters, for sizing the buffer
#define CL_MIH_MESSAGE_ACK_TYPE_NAK     "nak"   ///< Acknowledge type `nak`: not acknowledged at all
#define CL_MIH_MESSAGE_ACK_TYPE_ACK     "ack"   ///< Acknowledge type `ack`: acknowledged once the commlib has read it
#define CL_MIH_MESSAGE_ACK_TYPE_SYNC    "sync"   ///< Acknowledge type `sync`: acknowledged only after the application took **and** processed it
#define CL_MIH_MESSAGE_DATA_FORMAT_BIN  "bin"   ///< Payload is binary
#define CL_MIH_MESSAGE_DATA_FORMAT_XML  "xml"   ///< Payload is XML
#define CL_MIH_MESSAGE_DATA_FORMAT_AM   "am"   ///< Not payload: this message is an AM
#define CL_MIH_MESSAGE_DATA_FORMAT_SIM  "sim"   ///< Not payload: this message is a SIM
#define CL_MIH_MESSAGE_DATA_FORMAT_SIRM "sirm"   ///< Not payload: this message is a SIRM
#define CL_MIH_MESSAGE_DATA_FORMAT_CCM  "ccm"   ///< Not payload: this message is a CCM
#define CL_MIH_MESSAGE_DATA_FORMAT_CCRM "ccrm"   ///< Not payload: this message is a CCRM
#define CL_MIH_MESSAGE_DATA_FORMAT_CM   "cm"   ///< Not payload: this message is a CM
#define CL_MIH_MESSAGE_DATA_FORMAT_CRM  "crm"   ///< Not payload: this message is a CRM
/** @} */

/** @name AM - Acknowledge Message
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <am version="AM_VERSION">
 *       <mid>AM_MID</mid>
 *    </am>
 *
 *    AM_VERSION: Version number of AM (e.g. "0.4")
 *    AM_MID:     Acknowledged message id
 * @endcode
 * @{
 */
#define CL_AM_MESSAGE       "<am version=\"%s\"><mid>%ld</mid></am>"   ///< The AM as a format string
#define CL_AM_MESSAGE_VERSION "0.1"   ///< Protocol version this side writes into the AM
#define CL_AM_MESSAGE_SIZE  34   ///< Length of the AM format string without its parameters, for sizing the buffer
/** @} */

/** @name CM - Connect Message - a client announcing itself
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <cm version="CM_VERSION">
 *       <df>CM_DF</df>
 *       <ct>CM_CT</ct>
 *       <src host="CM_SRC_HOST" comp="CM_SRC_COMP" id="CM_SRC_ID"></src>
 *       <dst host="CM_DST_HOST" comp="CM_DST_COMP" id="CM_DST_ID"></dst>
 *       <rdata host="CM_RDATA_HOST" comp="CM_RDATA_COMP" id="CM_RDATA_ID"></rdata>
 *       <port>CM_PORT</port>
 *       <ac>CM_AC</ac>
 *    </cm>
 *
 *    CM_VERSION:     Version number of CM (e.g. "0.4")
 *    CM_DF:          "bin" or "xml" (connection Binary or XML data encoding)
 *    CM_CT:          "stream" or "message" (connection data flow control)
 *    CM_SRC_HOST:    Host name of source endpoint
 *    CM_SRC_COMP:    Component name of source endpoint
 *    CM_SRC_ID:      Component id of source endpoint
 *    CM_DST_HOST:    Host name of destination endpoint
 *    CM_DST_COMP:    Component name of destination endpoint
 *    CM_DST_ID:      Component id of destination endpoint
 *    CM_RDATA_HOST:  Host name of routing host endpoint
 *    CM_RDATA_COMP:  Component name of routing host endpoint
 *    CM_RDATA_ID:    Component id of routing host endpoint
 *    CM_PORT:        port where client is reachable (e.g. "0" not reachable, "5000" for port 5000 )
 *    CM_AC:          "enabled" or "disabled", used for auto close of connected clients when max. connection
 *                    count is reached at service component.
 * @endcode
 * @{
 */
#define CL_CONNECT_MESSAGE         "<cm version=\"%s\"><df>%s</df><ct>%s</ct><dst host=\"%s\" comp=\"%s\" id=\"%ld\"></dst><rdata host=\"%s\" comp=\"%s\" id=\"%ld\"></rdata><port>%ld</port><ac>%s</ac></cm>"   ///< The CM as a format string
#define CL_CONNECT_MESSAGE_VERSION "0.4"   ///< Protocol version this side writes into the CM
#define CL_CONNECT_MESSAGE_SIZE   100 + 33   ///< Length of the CM format string without its parameters, for sizing the buffer
#define CL_CONNECT_MESSAGE_DATA_FORMAT_BIN    "bin"   ///< The client asks for binary payload
#define CL_CONNECT_MESSAGE_DATA_FORMAT_XML    "xml"   ///< The client asks for XML payload
#define CL_CONNECT_MESSAGE_DATA_FLOW_STREAM   "stream"   ///< The client asks for a stream oriented connection
#define CL_CONNECT_MESSAGE_DATA_FLOW_MESSAGE  "message"   ///< The client asks for a message oriented connection
#define CL_CONNECT_MESSAGE_AUTOCLOSE_ENABLED  "enabled"   ///< The client allows the service to close this connection to make room
#define CL_CONNECT_MESSAGE_AUTOCLOSE_DISABLED "disabled"   ///< The client does not allow that
/** @} */

/** @name CRM - Connect Response Message - admitted or refused
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <crm version="CRM_VERSION">
 *       <cs condition="CRM_CS_CONDITION">CRM_CS_TEXT</cs>
 *       <src host="CRM_SRC_HOST" comp="CRM_SRC_COMP" id="CRM_SRC_ID"></src>
 *       <dst host="CRM_DST_HOST" comp="CRM_DST_COMP" id="CRM_DST_ID"></dst>
 *       <rdata host="CRM_RDATA_HOST" comp="CRM_RDATA_COMP" id="CRM_RDATA_ID"></rdata>
 *       <params>CRM_QMASTER_PARAMS</params>
 *    </crm>
 *
 *    CRM_VERSION:        Version number of CRM (e.g. "0.4")
 *    CRM_CS_CONDITION:   Connection Status:
 *                           "connected"                -> No Errors
 *                           "access denied"            -> Service doesn't allow client to connect
 *                           "unsupported data format"  -> Message Format error
 *                           "endpoint not unique"      -> Client is already connected
 *    CRM_CS_TEXT:        User defined connection status error text
 *    CRM_SRC_HOST:       Host name of source endpoint
 *    CRM_SRC_COMP:       Component name of source endpoint
 *    CRM_SRC_ID:         Component id of source endpoint
 *    CRM_DST_HOST:       Host name of destination endpoint
 *    CRM_DST_COMP:       Component name of destination endpoint
 *    CRM_DST_ID:         Component id of destination endpoint
 *    CRM_RDATA_HOST:     Host name of routing host endpoint
 *    CRM_RDATA_COMP:     Component name of routing host endpoint
 *    CRM_RDATA_ID:       Component id of routing host endpoint
 *    CRM_QMASTER_PARAMS: Qmaster params eg: name=value:name=value:.....
 * @endcode
 * @{
 */
#define CL_CONNECT_RESPONSE_MESSAGE                              "<crm version=\"%s\"><cs condition=\"%s\">%s</cs><rdata host=\"%s\" comp=\"%s\" id=\"%ld\"></rdata><params>%s</params></crm>"   ///< The CRM as a format string
#define CL_CONNECT_RESPONSE_MESSAGE_VERSION                      "0.3"   ///< Protocol version this side writes into the CRM
#define CL_CONNECT_RESPONSE_MESSAGE_SIZE                         101   ///< Length of the CRM format string without its parameters, for sizing the buffer
#define CL_CONNECT_RESPONSE_MESSAGE_CONNECTION_STATUS_OK         "connected"   ///< The service admits the client
#define CL_CONNECT_RESPONSE_MESSAGE_CONNECTION_STATUS_DENIED     "access denied"   ///< Refused: the client is not on the allowed host list
#define CL_CONNECT_RESPONSE_MESSAGE_CONNECTION_STATUS_NOT_UNIQUE "endpoint not unique"   ///< Refused: a component of that name and id is already connected
#define CL_CONNECT_RESPONSE_MESSAGE_CONNECTION_UNSUP_DATA_FORMAT "unsupported data format"   ///< Refused: the service does not speak the data format the client asked for
/** @} */

/** @name SIM - Status Information Message - what `qping` sends
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <sim version="SIM_VERSION">
 *    </sim>
 *
 *    SIM_VERSION: version number of SIM (e.g. "0.4")
 * @endcode
 * @{
 */
#define CL_SIM_MESSAGE               "<sim version=\"%s\"></sim>"   ///< The SIM as a format string
#define CL_SIM_MESSAGE_VERSION       "0.1"   ///< Protocol version this side writes into the SIM
#define CL_SIM_MESSAGE_SIZE          25   ///< Length of the SIM format string without its parameters, for sizing the buffer
/** @} */

/** @name SIRM - Status Information Response Message
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <sirm version="SIRM_VERSION">
 *       <mid>SIRM_MID/mid>
 *       <starttime>SIRM_STARTTIME</starttime>
 *       <runtime>SIRM_RUNTIME</runtime>
 *       <application>
 *          <messages>
 *             <brm>SIRM_APPLICATION_MESSAGES_BRM</brm>
 *             <bwm>SIRM_APPLICATION_MESSAGES_BWM</bwm>
 *          </messages>
 *          <connections>
 *             <noc>SIRM_APPLICATION_CONNECTIONS_NOC</noc>
 *          </connections>
 *          <status>SIRM_APPLICATION_STATUS</status>
 *       </application>
 *       <info>SIRM_INFO</info>
 *    </sirm>
 *
 *    SIRM_VERSION:                       Version number of CCM (e.g. "0.4")
 *    SIRM_MID:                           Message Id of SIRM
 *    SIRM_STARTTIME:                     (unsigned Integer) Starttime of service (Unix timestamp)
 *    SIRM_RUNTIME:                       (unsigned Integer) Runtime since starttime (in seconds)
 *    SIRM_APPLICATION_MESSAGES_BRM:      Buffered read messages for application (service)
 *    SIRM_APPLICATION_MESSAGES_BWM:      Buffered write messages from application (service)
 *    SIRM_APPLICATION_CONNECTIONS_NOC:   No. of connected clients
 *    SIRM_APPLICATION_STATUS:            (Unsigned Integer) Application status
 *    SIRM_INFO:                          Application status information string
 * @endcode
 * @{
 */
#define CL_SIRM_MESSAGE            "<sirm version=\"%s\"><mid>%ld</mid><starttime>%ld</starttime><runtime>%ld</runtime><application><messages><brm>%ld</brm><bwm>%ld</bwm></messages><connections><noc>%ld</noc></connections><status>%ld</status></application><info>%s</info></sirm>"   ///< The SIRM as a format string
#define CL_SIRM_MESSAGE_VERSION    "0.1"   ///< Protocol version this side writes into the SIRM
#define CL_SIRM_MESSAGE_SIZE       218   ///< Length of the SIRM format string without its parameters, for sizing the buffer
/** @} */

/** @name CCM - Connection Close Message
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <ccm version="CCM_VERSION"></ccm>
 *
 *    CCM_VERSION: version number of CCM (e.g. "0.4")
 * @endcode
 * @{
 */
#define CL_CCM_MESSAGE                              "<ccm version=\"%s\"></ccm>"   ///< The CCM as a format string
#define CL_CCM_MESSAGE_VERSION                      "0.1"   ///< Protocol version this side writes into the CCM
#define CL_CCM_MESSAGE_SIZE                         25   ///< Length of the CCM format string without its parameters, for sizing the buffer
/** @} */

/** @name CCRM - Connection Close Response Message
 *
 * The XML it is sent as, and what each placeholder stands for:
 *
 * @code
 *    <ccrm version="CCRM_VERSION"></ccrm>
 *
 *    CCRM_VERSION: version number of CCRM (e.g. "0.4")
 * @endcode
 * @{
 */
#define CL_CCRM_MESSAGE                              "<ccrm version=\"%s\"></ccrm>"   ///< The CCRM as a format string
#define CL_CCRM_MESSAGE_VERSION                      "0.1"   ///< Protocol version this side writes into the CCRM
#define CL_CCRM_MESSAGE_SIZE                         27   ///< Length of the CCRM format string without its parameters, for sizing the buffer
/** @} */

/** @brief Who a participant is
 *
 * The identification triple `comp_host`, `comp_name`, `comp_id` is what makes
 * an endpoint unique in a cluster - two components of the same name and id on
 * the same host are the same endpoint, and a service refuses the second with
 * #CL_CRM_CS_ENDPOINT_NOT_UNIQUE.
 */
typedef struct cl_com_endpoint {
   char *comp_host;          ///< Host the component runs on
   const char *comp_name;    ///< Component name, e.g. `"qmaster"`
   unsigned long comp_id;    ///< Component id
   struct in_addr addr;      ///< Resolved address of `comp_host`
   char *hash_id;            ///< The triple as one string, used as the hash key
} cl_com_endpoint_t;


/** @brief Payload encoding a client asks for in its CM */
typedef enum cl_xml_data_format_def {
   CL_CM_DF_UNDEFINED = 1,   ///< Unset
   CL_CM_DF_BIN,             ///< Binary
   CL_CM_DF_XML              ///< XML
} cl_xml_data_format_t;
/** @brief What follows a MIH
 *
 * Two things at once, which is worth noticing: the first values say how the
 * application's payload is encoded, the rest say that there is no application
 * payload at all and the message is one of the protocol's own. That is how a
 * reader knows whether to hand the message up or handle it itself.
 */
typedef enum cl_xml_mih_data_format_def {
   CL_MIH_DF_UNDEFINED = 1,   ///< Unset
   CL_MIH_DF_BIN,             ///< Application payload, binary
   CL_MIH_DF_XML,             ///< Application payload, XML
   CL_MIH_DF_AM,              ///< Protocol message: acknowledgement
   CL_MIH_DF_SIM,             ///< Protocol message: status request
   CL_MIH_DF_SIRM,            ///< Protocol message: status answer
   CL_MIH_DF_CCM,             ///< Protocol message: close request
   CL_MIH_DF_CCRM,            ///< Protocol message: close answer
   CL_MIH_DF_CM,              ///< Protocol message: connect request
   CL_MIH_DF_CRM              ///< Protocol message: connect answer
} cl_xml_mih_data_format_t;

/** @brief What an acknowledgement promises the sender
 *
 * The difference between the last two is the whole point: #CL_MIH_MAT_ACK
 * says the bytes arrived, #CL_MIH_MAT_SYNC says the application acted on
 * them.
 */
typedef enum cl_xml_ack_type_def {
   CL_MIH_MAT_UNDEFINED = 1,   ///< Unset
   CL_MIH_MAT_NAK,             ///< No acknowledgement is sent
   CL_MIH_MAT_ACK,             ///< Acknowledged when the commlib has read it
   CL_MIH_MAT_SYNC             ///< Acknowledged only after the application took **and** processed it
} cl_xml_ack_type_t;

/** @brief Whether a connection carries a byte stream or discrete messages */
typedef enum cl_xml_connection_type_def {
   CL_CM_CT_UNDEFINED = 1,   ///< Unset
   CL_CM_CT_STREAM,          ///< A byte stream
   CL_CM_CT_MESSAGE          ///< Discrete messages
} cl_xml_connection_type_t;

/** @brief Whether a service may close this connection to make room
 *
 * A client says so in its CM. It is what lets a service at its connection
 * limit drop the clients that can stand it instead of refusing everyone -
 * see #CL_ON_MAX_COUNT_CLOSE_AUTOCLOSE_CLIENTS.
 */
typedef enum cl_xml_connection_autoclose_def {
   CL_CM_AC_UNDEFINED = 1,   ///< Unset
   CL_CM_AC_ENABLED,         ///< The service may close it
   CL_CM_AC_DISABLED         ///< It may not
} cl_xml_connection_autoclose_t;

/** @brief What a service answers in its CRM
 *
 * Everything but #CL_CRM_CS_CONNECTED is a refusal, and the reason is what
 * the rejected client is told - which is why they are distinguished rather
 * than collapsed into one "no".
 */
typedef enum cl_xml_connection_status_def {
   CL_CRM_CS_UNDEFINED = 1,        ///< Unset
   CL_CRM_CS_CONNECTED,            ///< Admitted
   CL_CRM_CS_DENIED,               ///< Refused: not on the allowed host list
   CL_CRM_CS_ENDPOINT_NOT_UNIQUE,  ///< Refused: a component of that name and id is already connected
   CL_CRM_CS_UNSUPPORTED           ///< Refused: the data format asked for is not supported
} cl_xml_connection_status_t;


/* XML data types */
/** @brief A parsed GMSH - the length prefix */
typedef struct cl_com_GMSH_type {
   unsigned long dl;   ///< Byte length of the message that follows
} cl_com_GMSH_t;

/** @brief A parsed CM - what a client says when it connects */
typedef struct cl_com_CM_type {
   char *version;                       ///< Protocol version the client speaks
   cl_xml_data_format_t df;             ///< Payload encoding it asks for
   cl_xml_connection_type_t ct;         ///< Stream or message oriented
   cl_xml_connection_autoclose_t ac;    ///< Whether the service may close this connection
   unsigned long port;                  ///< The port the client itself listens on, if any
   cl_com_endpoint_t *dst;              ///< Who the client thinks it is connecting to - need not be what the service calls itself
   cl_com_endpoint_t *rdata;            ///< Who the client is
} cl_com_CM_t;


/** @brief A parsed CRM - the service's answer to a CM */
typedef struct cl_com_CRM_type {
   char *version;                          ///< Protocol version the service speaks
   cl_xml_connection_status_t cs_condition; ///< Admitted, or why not
   char *cs_text;                          ///< The refusal in words, shown to the user
   char *formats;                          ///< Comma separated list of data formats the service accepts. @todo Not implemented - the field is parsed but never filled
   cl_com_endpoint_t *rdata;               ///< Who the service is
   char *params;                           ///< Free-form parameters the service passes back
} cl_com_CRM_t;

/** @brief A parsed MIH - the header of every message */
typedef struct cl_com_MIH_type {
   char *version;                 ///< Protocol version
   unsigned long mid;             ///< Message id
   unsigned long dl;              ///< Byte length of the payload
   cl_xml_mih_data_format_t df;   ///< Whether payload follows, and in what form
   cl_xml_ack_type_t mat;         ///< What acknowledgement the sender wants
   unsigned long tag;             ///< The application's own tag, passed through untouched
   unsigned long rid;             ///< The message id this one answers, or 0
} cl_com_MIH_t;

/** @brief A parsed AM - an acknowledgement */
typedef struct cl_com_AM_type {
   char *version;       ///< Protocol version
   unsigned long mid;   ///< The message being acknowledged
} cl_com_AM_t;

/** @brief A parsed SIM - a status request, carrying nothing but its version */
typedef struct cl_com_SIM_type {
   char *version;   ///< Protocol version
} cl_com_SIM_t;

/** @brief A parsed SIRM - what `qping` prints
 *
 * The counters come from the answering side's #cl_com_handle_statistic_t.
 */
typedef struct cl_com_SIRM_type {
   char *version;                              ///< Protocol version
   unsigned long mid;                          ///< The SIM being answered
   unsigned long starttime;                    ///< When the answering component started
   unsigned long runtime;                      ///< How long it has been running
   unsigned long application_messages_brm;     ///< Buffered read messages: received and waiting for the application
   unsigned long application_messages_bwm;     ///< Buffered write messages: queued and not yet sent
   unsigned long application_connections_noc;  ///< Number of open connections
   unsigned long application_status;           ///< Whatever the application's #cl_app_status_func_t returned
   char *info;                                 ///< The text that came with it
} cl_com_SIRM_t;

/** @brief A parsed CCM - a request to close, carrying nothing but its version */
typedef struct cl_com_CCM_type {
   char *version;   ///< Protocol version
} cl_com_CCM_t;

/** @brief A parsed CCRM - the answer confirming a close */
typedef struct cl_com_CCRM_type {
   char *version;   ///< Protocol version
} cl_com_CCRM_t;


const char *cl_com_get_mih_df_string(cl_xml_mih_data_format_t df);

const char *cl_com_get_mih_mat_string(cl_xml_ack_type_t mat);


/* endpoint helper functions */
cl_com_endpoint_t *cl_com_create_endpoint(const char *comp_host,
                                          const char *comp_name,
                                          unsigned long comp_id,
                                          const struct in_addr *in_addr);

cl_com_endpoint_t *cl_com_dup_endpoint(cl_com_endpoint_t *endpoint);

int cl_com_free_endpoint(cl_com_endpoint_t **endpoint);


/* message functions */
int cl_com_free_gmsh_header(cl_com_GMSH_t **header);

int cl_com_free_cm_message(cl_com_CM_t **message);

int cl_com_free_crm_message(cl_com_CRM_t **message);

int cl_com_free_mih_message(cl_com_MIH_t **message);

int cl_com_free_am_message(cl_com_AM_t **message);

int cl_com_free_sim_message(cl_com_SIM_t **message);

int cl_com_free_sirm_message(cl_com_SIRM_t **message);

int cl_com_free_ccm_message(cl_com_CCM_t **message);

int cl_com_free_ccrm_message(cl_com_CCRM_t **message);


/* xml parsing functions */
int cl_xml_parse_GMSH(unsigned char *buffer, unsigned long buffer_length, cl_com_GMSH_t *header,
                      unsigned long *used_buffer_length);

int cl_xml_parse_CM(unsigned char *buffer, unsigned long buffer_length, cl_com_CM_t **connection_message);

int cl_xml_parse_CRM(unsigned char *buffer, unsigned long buffer_length, cl_com_CRM_t **connection_message);

int cl_xml_parse_MIH(unsigned char *buffer, unsigned long buffer_length, cl_com_MIH_t **message);

int cl_xml_parse_AM(unsigned char *buffer, unsigned long buffer_length, cl_com_AM_t **message);

int cl_xml_parse_SIM(unsigned char *buffer, unsigned long buffer_length, cl_com_SIM_t **message);

int cl_xml_parse_SIRM(unsigned char *buffer, unsigned long buffer_length, cl_com_SIRM_t **message);

int cl_xml_parse_CCM(unsigned char *buffer, unsigned long buffer_length, cl_com_CCM_t **message);

int cl_xml_parse_CCRM(unsigned char *buffer, unsigned long buffer_length, cl_com_CCRM_t **message);

int cl_com_transformString2XML(const char *input, char **output);

int cl_com_transformXML2String(const char *input, char **output);
