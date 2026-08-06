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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Thread monitoring: periodic health and throughput output
 */

#include <sys/time.h>
#include <rapidjson/writer.h>

#include <cinttypes>
#include "uti/sge_dstring.h"

/**
 * Monitoring functionality:
 * -------------------------
 *
 * - qping health monitoring
 *
 * - keeping statistics on what is done during a thread loop
 *
 * - outputting the statistics information via message file or
 *   qping
 *
 *
 * Monitoring Usage:
 * -----------------
 *
 * do a normal data definition, call init and free, when you are done.
 * You have to call MONITOR_IDLE_TIME and sge_monitor_output. After that
 * everything is up to you to design...
 *
 * @code
 *    monitoring_t monitor;
 *
 *    sge_monitor_init(&monitor, "THREAD NAME", extension, warning_to, error_to);
 *
 *    while (thread_runs) {
 *
 *       MONITOR_IDLE_TIME(wait_for_something(), (&monitor), monitor_time);
 *
 *       // do your work, and the monitoring calls that go with it
 *
 *       sge_monitor_output(&monitor);
 *    }
 *    sge_monitor_free(&monitor);
 * @endcode
 *
 * Important:
 * ----------
 *  The call to MONITOR_IDLE_TIME has to be the first one after the thread loop otherwise
 *  certain parts of the monitoring structure are not correctly initialized.
 *
 * General statistic methods:
 * --------------------------
 *
 * - MONITOR_IDLE_TIME    : counts idle time, very important, nothing works without it
 * - MONITOR_WAIT_TIME    : counts wait time (wait for a lock usually)
 * - MONITOR_MESSAGES     : counts how many times the thread loop is executed
 * - MONITOR_MESSAGES_OUT : counts how many messages are send
 *
 * GDI statistics methods:
 * -----------------------
 *
 * - MONITOR_GDI  : counts GDI requests
 * - MONITOR_ACK  : counts ACKs
 * - MONITOR_LOAD : counts reports
 */


/** @name qping thread warning thresholds, in seconds
 *
 * A thread that has not reported progress for this long is shown as delayed by
 * `qping`. 0 disables the check for that thread.
 * @{
 */
const long NO_WARNING = 0;   ///< no warning threshold for this thread: warning
const long EVENT_MASTER_THREAD_WARNING = 5;   ///< event master thread: warning
const long TET_WARNING = 10;   ///< timed event thread: warning
const long MT_WARNING = 0;   ///< main thread, no threshold: warning
const long WT_WARNING = 60;   ///< worker thread: warning
const long RT_WARNING = 60;   ///< reader thread: warning
const long ST_WARNING = 0;   ///< scheduler thread, no threshold: warning
const long EXECD_WARNING = 10;   ///< execd: warning
const long SCT_WARNING = 20;   ///< signal/communication thread: warning

/** @} */

/** @name qping thread error thresholds, in seconds
 *
 * As the warning thresholds above, but for the point at which `qping` reports
 * the thread as failed rather than merely slow.
 * @{
 */
const long NO_ERROR = 0;   ///< no error threshold for this thread: error
const long EVENT_MASTER_THREAD_ERROR = 60;   ///< event master thread: error
const long TET_ERROR = 60;   ///< timed event thread: error
const long MT_ERROR = 0;   ///< main thread, no threshold: error
const long WT_ERROR = 600;   ///< worker thread: error
const long RT_ERROR = 60*60*24*365;   ///< reader thread, effectively never: error
const long ST_ERROR = 0;   ///< scheduler thread, no threshold: error
const long EXECD_ERROR = 600;   ///< execd: error
const long SCT_ERROR = 600;   ///< signal/communication thread: error

/** @} */

/** @brief Renders the extension data of one thread into the monitoring output
 *
 * Each extension supplies one of these; #sge_monitor_output calls it.
 */
typedef void (*extension_output)(
        dstring *info_message,    // target memory buffer
        void *monitor_extension,  // contains the monitor extension structure
        double time,              // length of the time interval
        rapidjson::Writer<rapidjson::StringBuffer> *json_writer // json writer
);

/**
 * This enum identifies all available extensions
 */
typedef enum {
   NONE_EXT = -1,       ///< no extension, only the common counters are collected
   GDI_EXT = 0,         ///< request processing thread, data is #m_gdi_t
   EMAT_EXT = 1,        ///< event master thread
   TET_EXT = 2,         ///< timed event thread
   LIS_EXT = 3,         ///< listener thread, data is #m_lis_t
   SCH_EXT = 4          ///< scheduler thread, data is #m_sch_t
} extension_t;

/** @brief Receives the monitoring data of one interval as a JSON document
 *
 * Installed in #monitoring_t::json_output; used when JSON logging is enabled.
 */
typedef bool (*json_output_func)(const char *json_string);

/** @brief Monitoring state of one thread
 *
 * One of these per monitored thread, set up by #sge_monitor_init and released
 * with #sge_monitor_free. The thread updates the counters through the
 * `MONITOR_*` macros, and #sge_monitor_output writes a line per interval.
 *
 * Threads with extra metrics attach an extension: #ext_type says which, and
 * #ext_data points at #m_gdi_t, #m_lis_t or #m_sch_t accordingly.
 */
typedef struct {
   /*--- init data ------------*/
   const char *thread_name;               ///< name shown in the output
   time_t monitor_time;                   ///< length of one measuring interval; 0 disables monitoring
   bool log_monitor_mes;                  ///< write the monitoring info to the message file
   bool log_monitor_json;                 ///< write the monitoring info as JSON
   /*--- output data ----------*/
   dstring *output_line1;                 ///< first output line, the common counters
   dstring *output_line2;                 ///< second output line, the extension counters
   dstring *work_line;                    ///< scratch buffer used while formatting
   int pos;                               ///< line of this thread in the qping output, effectively a thread id
   json_output_func json_output;          ///< where to send the JSON document, may be nullptr
   /*--- work data ------------*/
   struct timeval now;                    ///< start of the current interval
   bool output;                           ///< set when the interval elapsed and output is due
   uint32_t message_in_count;             ///< messages received during the interval
   uint32_t message_out_count;            ///< messages sent during the interval
   double idle;                           ///< seconds spent idle during the interval
   double wait;                           ///< seconds spent waiting during the interval
   /*--- extension data -------*/
   extension_t ext_type;                  ///< which extension #ext_data holds
   void *ext_data;                        ///< the extension counters, or nullptr
   uint32_t ext_data_size;                ///< size of #ext_data in bytes
   extension_output ext_output;           ///< renders #ext_data into the output
} monitoring_t;

void sge_monitor_init(monitoring_t *monitor, const char *thread_name, extension_t ext, long warning_timeout,
                      long error_timeout, json_output_func json_output);

void sge_monitor_free(monitoring_t *monitor);

uint32_t sge_monitor_status(char **info_message, uint32_t monitor_time);

void sge_set_last_wait_time(monitoring_t *monitor, struct timeval after);

void sge_monitor_output(monitoring_t *monitor);

void sge_monitor_reset(monitoring_t *monitor);


/****************
 * MACRO section
 ****************/

/**
 * This macro is used to measure the idle time in a thread loop.
 * @param execute  the code to execute
 * @param monitor  the monitoring structure
 * @param options  a tuple with the following values:
 *                - the time interval for the measurement (qmaster_params MONITOR_TIME=timeval)
 *                - log into message file (qmaster_params LOG_MONITOR_MESSAGE=TRUE|FALSE)
 *                - log as json (reporting_params monitoring=true|false)
 */
#define MONITOR_IDLE_TIME(execute, monitor, options)    { \
                                 struct timeval before{};  \
                                 gettimeofday(&before, nullptr); \
                                 sge_set_last_wait_time((monitor), before); \
                                 if (std::get<0>(options) > 0) { \
                                    struct timeval before1{};  \
                                    struct timeval after1{}; \
                                    double time; \
                                    \
                                    (monitor)->monitor_time = std::get<0>(options); \
                                    (monitor)->log_monitor_mes = std::get<1>(options); \
                                    (monitor)->log_monitor_json = std::get<2>(options); \
                                    gettimeofday(&before1, nullptr); \
                                    if ((monitor)->now.tv_sec == 0) { \
                                       (monitor)->now = before1; \
                                    } \
                                    execute; \
                                    gettimeofday(&after1, nullptr);  \
                                    (monitor)->output = ((after1.tv_sec-(monitor)->now.tv_sec) >= (monitor)->monitor_time)?true:false; \
                                    time = after1.tv_usec - before1.tv_usec; \
                                    time = after1.tv_sec - before1.tv_sec + (time/1000000); \
                                    (monitor)->idle += time; \
                                 } \
                                 else { \
                                    execute; \
                                 } \
                              } \

/**
 * This might pose a problem if it is called with another macro.
 *
 * TODO: it should be customized for read/write locks.
 */
#define MONITOR_WAIT_TIME(execute, monitor)    if (((monitor) != nullptr) && ((monitor)->monitor_time > 0)){ \
                                    struct timeval before{};  \
                                    struct timeval after{}; \
                                    double time; \
                                    \
                                    gettimeofday(&before, nullptr); \
                                    execute; \
                                    gettimeofday(&after, nullptr);  \
                                    time = after.tv_usec - before.tv_usec; \
                                    time = after.tv_sec - before.tv_sec + (time/1000000); \
                                    (monitor)->wait += time; \
                                 } \
                                 else { \
                                    execute; \
                                 } \

/// count one received message
#define MONITOR_MESSAGES(monitor) if ((monitor != nullptr) && ((monitor)->monitor_time > 0)) (monitor)->message_in_count++

/// count one sent message
#define MONITOR_MESSAGES_OUT(monitor) if (((monitor) != nullptr) && ((monitor)->monitor_time > 0)) (monitor)->message_out_count++

/*--------------------------------*/
/*   EXTENSION SECTION            */
/*--------------------------------*/

/**
 * What you need to do to create a new extension:
 *
 * - create a new extension_t in the enum
 * - define a extension data structure
 * - modify the sge_monitor_init method to handle the new extension type
 *   Example:
 *     case GDI_EXT :
 *          monitor->ext_data_size = sizeof(m_gdi_t);
 *          monitor->ext_data = malloc(sizeof(m_gdi_t));
 *          monitor->ext_output = `&ext_gdi_output`;
 *       break;
 *
 * - write the extension output function
 * - write the measurement makros
 * - remember, that the entire extension structure is reset to 0 after the data is printed
 *
 **/


/** @brief Extension counters of the scheduler thread
 *
 * The scheduler reports no extra metrics yet; the struct exists so the
 * extension mechanism has something to attach.
 */
typedef struct {
   uint32_t dummy;    ///< unused placeholder
} m_sch_t;

/** @brief Extension counters of a request processing thread
 *
 * Attached when #monitoring_t::ext_type is #GDI_EXT. Every counter is reset to
 * zero after each interval is printed.
 */
typedef struct {
   uint32_t gdi_add_count;      ///< GDI add requests
   uint32_t gdi_mod_count;      ///< GDI mod requests
   uint32_t gdi_get_count;      ///< GDI get requests
   uint32_t gdi_del_count;      ///< GDI del requests
   uint32_t gdi_cp_count;       ///< GDI copy requests
   uint32_t gdi_trig_count;     ///< GDI trigger requests
   uint32_t gdi_perm_count;     ///< GDI permission requests
   uint32_t gdi_replace_count;  ///< GDI replace requests

   uint32_t eload_count;        ///< load reports received from execd
   uint32_t econf_count;        ///< configuration version requests from execd
   uint32_t ejob_count;         ///< job reports received from execd
   uint32_t eproc_count;        ///< processor reports received from execd
   uint32_t eack_count;         ///< acknowledgements received from execd

   uint32_t queue_length;       ///< length of the main (worker) queue
   uint32_t rqueue_length;      ///< length of the reader queue
   uint32_t wrqueue_length;     ///< length of the waiting reader queue
} m_gdi_t;

/// count one GDI add request
#define MONITOR_GDI_ADD(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_add_count++
/// count one GDI get request
#define MONITOR_GDI_GET(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_get_count++
/// count one GDI mod request
#define MONITOR_GDI_MOD(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_mod_count++
/// count one GDI del request
#define MONITOR_GDI_DEL(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_del_count++
/// count one GDI copy request
#define MONITOR_GDI_CP(monitor)     if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_cp_count++
/// count one GDI trigger request
#define MONITOR_GDI_TRIG(monitor)   if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_trig_count++
/// count one GDI permission request
#define MONITOR_GDI_PERM(monitor)   if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_perm_count++
/// count one GDI replace request
#define MONITOR_GDI_REPLACE(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->gdi_replace_count++

/** @brief Count one acknowledgement
 *
 * @bug Broken and unused. It casts #monitoring_t::ext_data to #m_gdi_t and
 *      accesses `ack_count`, but that field belongs to #m_edt_t - #m_gdi_t has
 *      `eack_count`. Any use of this macro fails to compile, which is why the
 *      defect survived. #MONITOR_EDT_ACK is the working equivalent.
 */
#define MONITOR_ACK(monitor)     if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->ack_count++

/// count one load report from execd
#define MONITOR_ELOAD(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->eload_count++
/// count one configuration version request from execd
#define MONITOR_ECONF(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->econf_count++
/// count one job report from execd
#define MONITOR_EJOB(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->ejob_count++
/// count one processor report from execd
#define MONITOR_EPROC(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->eproc_count++
/// count one acknowledgement from execd
#define MONITOR_EACK(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->eack_count++

/// record the current main queue length
#define MONITOR_SET_QLEN(monitor, qlen)    if ((monitor) != nullptr && (monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->queue_length = (qlen)
/// record the current reader queue length
#define MONITOR_SET_RQLEN(monitor, qlen)    if ((monitor) != nullptr && (monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->rqueue_length = (qlen)
/// record the current waiting reader queue length
#define MONITOR_SET_WRQLEN(monitor, qlen)    if ((monitor) != nullptr && (monitor->monitor_time > 0) && (monitor->ext_type == GDI_EXT)) ((m_gdi_t*)(monitor->ext_data))->wrqueue_length = (qlen)

/** @brief Extension counters of a listener thread
 *
 * Attached when #monitoring_t::ext_type is #LIS_EXT.
 */
typedef struct {
   uint32_t inc_gdi;                     ///< incoming GDI requests
   uint32_t inc_ack;                     ///< incoming acknowledgements
   uint32_t inc_ece;                     ///< incoming event client exits
   uint32_t inc_rep;                     ///< incoming reports

   uint32_t gdi_get_count;      ///< GDI get requests
   uint32_t gdi_trig_count;     ///< GDI trigger requests
   uint32_t gdi_perm_count;     ///< GDI permission requests
} m_lis_t;

/// count one incoming GDI request
#define MONITOR_INC_GDI(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->inc_gdi++
/// count one incoming acknowledgement
#define MONITOR_INC_ACK(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->inc_ack++
/// count one incoming event client exit
#define MONITOR_INC_ECE(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->inc_ece++
/// count one incoming report
#define MONITOR_INC_REP(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->inc_rep++

/// count one GDI get request seen by the listener
#define MONITOR_LIS_GDI_GET(monitor)    if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->gdi_get_count++
/// count one GDI trigger request seen by the listener
#define MONITOR_LIS_GDI_TRIG(monitor)   if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->gdi_trig_count++
/// count one GDI permission request seen by the listener
#define MONITOR_LIS_GDI_PERM(monitor)   if ((monitor->monitor_time > 0) && (monitor->ext_type == LIS_EXT)) ((m_lis_t*)(monitor->ext_data))->gdi_perm_count++

/** @brief Extension counters of the event master thread
 *
 * Attached when #monitoring_t::ext_type is #EMAT_EXT.
 */
typedef struct {
   uint32_t count;                       ///< monitoring runs of this thread
   uint32_t client_count;                ///< currently connected event clients
   uint32_t mod_client_count;            ///< event client modifications
   uint32_t ack_count;                   ///< acknowledgements received
   uint32_t new_event_count;             ///< newly created events
   uint32_t added_event_count;           ///< events handed to event clients
   uint32_t skip_event_count;            ///< events dropped because nobody subscribed them
   uint32_t blocked_client_count;        ///< event clients blocked while sending
   uint32_t busy_client_count;           ///< event clients busy while sending
} m_edt_t;

/// record the number of connected event clients
#define MONITOR_CLIENT_COUNT(monitor, inc)  if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                               ((m_edt_t*) (monitor->ext_data))->client_count += inc

/// count one event master run
#define MONITOR_EDT_COUNT(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*) (monitor->ext_data))->count++

/// count one event client modification
#define MONITOR_EDT_MOD(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*) (monitor->ext_data))->mod_client_count++

/// count one acknowledgement received by the event master
#define MONITOR_EDT_ACK(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*)(monitor->ext_data))->ack_count++

/// count one newly created event
#define MONITOR_EDT_NEW(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*)(monitor->ext_data))->new_event_count++

/// count one event handed to an event client
#define MONITOR_EDT_ADDED(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*)(monitor->ext_data))->added_event_count++

/// count one event dropped because nobody subscribed it
#define MONITOR_EDT_SKIP(monitor) if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*)(monitor->ext_data))->skip_event_count++

/// count one event client blocked while sending
#define MONITOR_EDT_BLOCKED(monitor)  if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*)(monitor->ext_data))->blocked_client_count++

/// count one event client busy while sending
#define MONITOR_EDT_BUSY(monitor)  if ((monitor->monitor_time > 0) && (monitor->ext_type == EMAT_EXT)) \
                                    ((m_edt_t*)(monitor->ext_data))->busy_client_count++

/** @brief Extension counters of the timed event thread
 *
 * Attached when #monitoring_t::ext_type is #TET_EXT.
 */
typedef struct {
   uint32_t count;                       ///< monitoring runs of this thread
   uint32_t event_count;                 ///< timed events still pending
   uint32_t exec_count;                  ///< timed events executed
} m_tet_t;

/// count one timed event thread run
#define MONITOR_TET_COUNT(monitor)  if ((monitor->monitor_time > 0) && (monitor->ext_type == TET_EXT)) \
                                    ((m_tet_t*)(monitor->ext_data))->count++

/// record the number of pending timed events
#define MONITOR_TET_EVENT(monitor, inc)  if ((monitor->monitor_time > 0) && (monitor->ext_type == TET_EXT)) \
                                    ((m_tet_t*)(monitor->ext_data))->event_count += inc

/// count one executed timed event
#define MONITOR_TET_EXEC(monitor)  if ((monitor->monitor_time > 0) && (monitor->ext_type == TET_EXT)) \
                                    ((m_tet_t*)(monitor->ext_data))->exec_count++
