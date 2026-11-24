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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <new>
#include <optional>
#include <utility>

#include "uti/ocs_cond.h"
#include "uti/sge_error_class.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "comm/commlib.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_ClientServerBase.h"
#include "gdi/ocs_gdi_Command.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/sge_event.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_ET.h"
#include "sgeobj/tom/ocs_tom_EV.h"
#include "sgeobj/tom/ocs_tom_EVS.h"
#include "sgeobj/tom/ocs_tom_PACK.h"
#include "sgeobj/tom/ocs_tom_REP.h"

#include "evc/sge_event_client.h"
#include "evc/msg_evclib.h"

#include "msg_common.h"
#include "uti/ocs_Bootstrap.h"
#include "tom/ocs_PackCore.h"

/** @brief Debug layer used by every `DENTER`/`DPRINTF` in this file */
#define EVC_LAYER TOP_LAYER

/** @file
 * @brief Implementation of the event client interface
 *
 * Contains both flavours of event client: the external one that reaches
 * qmaster over the GDI, and the internal one that runs inside qmaster and
 * calls the event master directly. #sge_evc_class_create() picks the flavour
 * and fills the operation table of #sge_evc_class_t with the matching
 * `ec2_*` implementations.
 *
 * The narrative documentation of the interface lives on the @ref evc_interface
 * page below.
 */

/** @page evc_interface The event client interface
 *
 * The event client interface provides a means to connect to qmaster and
 * receive information about objects: their current properties and every
 * subsequent change.
 *
 * It provides a subscribe/unsubscribe mechanism allowing fine grained
 * selection per object type (jobs, queues, hosts, ...) and event type (add,
 * modify, delete). Flushing triggered by individual events can be configured,
 * and policies can be set for how to handle busy event clients. Clients that
 * receive large numbers of events, or that take some time to process them
 * because they depend on other components such as databases, should in any
 * case make use of the busy handling.
 *
 * The interface has much less impact on qmaster performance than polling the
 * same data in regular intervals.
 *
 * @section evc_lifecycle The shape of a session
 *
 * The sections below describe the pieces; this is how they fit together. Two
 * details are easy to miss from the prose and are the whole reason the
 * interface looks the way it does:
 *
 * - **Subscription changes are buffered until `ec_commit()`.** `ec_subscribe()`
 *   and `ec_set_flush()` only change the local copy, so a client can rearrange
 *   its whole subscription and send it as one request.
 * - **Every delivery is acknowledged, and the acknowledgement carries the next
 *   subscription with it.** `ec_ack()`, `ec_set_busy()` and `ec_commit()` go
 *   out together after each batch, which is what lets qmaster tell a slow
 *   client from a dead one. A client that stops acknowledging is eventually
 *   sent `sgeE_ACK_TIMEOUT` and has to register again.
 *
 * @msc
 *   hscale="1.4";
 *   client [label="event client"], qmaster [label="qmaster"];
 *
 *   client=>qmaster  [label="ec_register()"];
 *   qmaster>>client  [label="client id"];
 *   client rbox client [label="ec_subscribe(), ec_set_flush(),\nec_set_edtime() - local only"];
 *   client=>qmaster  [label="ec_commit(): the subscription"];
 *   |||;
 *   --- [label="then, for as long as the session lasts"];
 *   qmaster=>client  [label="events, at the interval or flushed early"];
 *   client rbox client [label="process them"];
 *   client=>qmaster  [label="ec_ack() + ec_set_busy() + ec_commit()"];
 *   |||;
 *   --- [label="at the end"];
 *   client=>qmaster  [label="ec_deregister()"];
 * @endmsc
 *
 * @section evc_ids Id numbers for registration
 *
 * Each event client registered at qmaster has a unique client id. The
 * registration request either names a concrete id the client wants to occupy,
 * or asks qmaster to assign one. The id is set when the client is created, by
 * #sge_evc_class_create().
 *
 * | Id | Meaning |
 * |----|---------|
 * | `EV_ID_ANY` | qmaster assigns a unique id |
 * | `EV_ID_SCHEDD` | register at qmaster as the scheduler |
 *
 * As long as an event client does not expect any special handling inside
 * qmaster, it should let qmaster assign an id. If a client does need special
 * handling, a new id in the range [2;10] has to be created and the handling
 * implemented in qmaster, see `sge_event_master.cc`.
 *
 * @section evc_subscription Subscription
 *
 * An event client is notified when certain event conditions are raised in
 * qmaster. Which events it is interested in is set through the subscription
 * interface.
 *
 * Events have a unique identification that classifies them, for example "a job
 * has been submitted" or "a queue has been disabled". Delivery can be switched
 * on and off for each event id individually, and changed at runtime through
 * #sge_evc_class_str::ec_subscribe and #sge_evc_class_str::ec_unsubscribe.
 *
 * @section evc_flushing Flushing
 *
 * In the standard configuration qmaster delivers events at a fixed interval,
 * configured with #sge_evc_class_str::ec_set_edtime.
 *
 * Sometimes an event client wants to be notified immediately. A scheduler for
 * example benefits from learning at once that resources became available, so
 * it can refill the cluster without waiting out the interval. Flushing can
 * therefore be configured per event id: either switched off, which is the
 * default, or given a delivery time after which qmaster delivers at the
 * latest. A delivery time of 0 means instant delivery.
 *
 * Flushing can be changed at runtime through
 * #sge_evc_class_str::ec_set_flush and #sge_evc_class_str::ec_unset_flush.
 *
 * @section evc_list_filtering List filtering
 *
 * The data sent with an event can be filtered on the master side by setting a
 * what and a where condition. If the where condition removes all data, no
 * event is sent at all.
 *
 * #sge_evc_class_str::ec_mod_subscription_where expects the `lListElem`
 * representation of the `lCondition` and `lEnumeration`; `ocs_what_to_elem()` and
 * `ocs_where_to_elem()` convert them.
 *
 * Master and client both benefit, in speed and in memory, from requesting only
 * the data the client actually needs. Three constraints apply:
 *
 * - Reducing elements via a what condition requires care: all elements must
 *   have a custom descriptor, and elements with different descriptors must not
 *   be mixed in the same list.
 * - All registered events for the same object type need the same what
 *   and where filter.
 * - `JAT` is special. It is subscribable as an event and is at the same
 *   time a sub-structure of `JB`, so setting a `JAT` filter also
 *   filters the JAT lists inside the JB list.
 *
 * @section evc_busy_state Busy state
 *
 * An event client may have periods where it is busy processing and cannot
 * accept new events. Qmaster should then buffer events rather than send them,
 * as otherwise timeouts occur while waiting for acknowledgements.
 *
 * The policy is selected with #sge_evc_class_str::ec_set_busy_handling and can
 * be changed at runtime:
 *
 * | Policy | Behaviour |
 * |--------|-----------|
 * | `EV_BUSY_NO_HANDLING` | busy state is not handled automatically |
 * | `EV_BUSY_UNTIL_ACK` | qmaster marks the client busy when delivering, and clears it when the client acknowledges receipt |
 * | `EV_BUSY_UNTIL_RELEASED` | qmaster marks the client busy when delivering; it stays busy until the client clears it explicitly with `ec_set_busy(0)` |
 *
 * @section evc_session_filtering Session filtering
 *
 * For JAPI event clients, subscription alone is not enough to track only those
 * events that belong to a JAPI session. When a session key is set with
 * #sge_evc_class_str::ec_set_session, two rules decide whether a subscribed
 * event is sent:
 *
 * - An event associated with a specific session is not sent when the session
 *   does not match.
 * - An event not associated with any session is not sent. The only exception
 *   are total update events; if subscribed, those are always sent and are
 *   filtered more finely afterwards.
 */
/** @page evc_ev_type The EV event client object
 *
 * An event client creates and initialises an `EV` object and passes
 * it to qmaster for registration. Qmaster fills in some of the fields, for
 * example `EV_id`, and sends the object back on successful registration.
 * Whenever the client wants to change a configuration parameter it modifies
 * the object and sends it to qmaster with a modification request.
 *
 * Qmaster also uses the object internally to track sequence numbers, delivery
 * times and timeouts.
 *
 * | Field | Meaning |
 * |-------|---------|
 * | `EV_id` | event client id, see @ref evc_ids |
 * | `EV_name` | event client name, any string characterising the client |
 * | `EV_host` | host on which the event client resides |
 * | `EV_commproc` | addressing information |
 * | `EV_commid` | unique id of the event client in the commlib |
 * | `EV_uid` | user id under which the event client is running |
 * | `EV_d_time` | event delivery interval used by qmaster |
 * | `EV_subscribed` | subscription and flushing, see @ref evc_subscription and @ref evc_flushing |
 * | `EV_busy_handling` | busy state policy, see @ref evc_busy_state |
 * | `EV_last_heard_from` | when qmaster last heard from the event client |
 * | `EV_last_send_time` | time of the last delivery of events |
 * | `EV_next_send_time` | next time scheduled for delivery of events |
 * | `EV_next_number` | next sequential number; every event gets a unique number |
 * | `EV_busy` | is the client busy? no events are delivered to a busy client |
 * | `EV_events` | events spooled until the client acknowledges receipt |
 *
 * @see @ref evc_interface, @ref evc_events
 */

/** @page evc_events Events available from qmaster
 *
 * The following events can be raised in qmaster and subscribed by an event
 * client. See @ref evc_subscription for how to subscribe them.
 *
 * | Event | Meaning |
 * |-------|---------|
 * | `sgeE_ALL_EVENTS` | &nbsp; |
 * | `sgeE_CALENDAR_LIST` | send calendar list at registration |
 * | `sgeE_CALENDAR_ADD` | event add calendar |
 * | `sgeE_CALENDAR_DEL` | event delete calendar |
 * | `sgeE_CALENDAR_MOD` | event modify calendar |
 * | `sgeE_CKPT_LIST` | send ckpt list at registration |
 * | `sgeE_CKPT_ADD` | event add ckpt |
 * | `sgeE_CKPT_DEL` | event delete ckpt |
 * | `sgeE_CKPT_MOD` | event modify ckpt |
 * | `sgeE_CENTRY_LIST` | send complex entry list at reg. |
 * | `sgeE_CENTRY_ADD` | event add complex entry |
 * | `sgeE_CENTRY_DEL` | event delete complex entry |
 * | `sgeE_CENTRY_MOD` | event modify complex entry |
 * | `sgeE_CONFIG_LIST` | send config list at registration |
 * | `sgeE_CONFIG_ADD` | event add config |
 * | `sgeE_CONFIG_DEL` | event delete config |
 * | `sgeE_CONFIG_MOD` | event modify config |
 * | `sgeE_EXECHOST_LIST` | send exec host list at registration |
 * | `sgeE_EXECHOST_ADD` | event add exec host |
 * | `sgeE_EXECHOST_DEL` | event delete exec host |
 * | `sgeE_EXECHOST_MOD` | event modify exec host |
 * | `sgeE_JATASK_ADD` | event add array job task |
 * | `sgeE_JATASK_DEL` | event delete array job task |
 * | `sgeE_JATASK_MOD` | event modify array job task |
 * | `sgeE_PETASK_ADD` | event add a new pe task |
 * | `sgeE_PETASK_DEL` | event delete a pe task |
 * | `sgeE_JOB_LIST` | send job list at registration |
 * | `sgeE_JOB_ADD` | event job add (new job) |
 * | `sgeE_JOB_DEL` | event job delete |
 * | `sgeE_JOB_MOD` | event job modify |
 * | `sgeE_JOB_USAGE` | event job online usage |
 * | `sgeE_JOB_FINAL_USAGE` | event job final usage report after job end |
 * | `sgeE_JOB_FINISH` | job finally finished or aborted (user view) |
 * | `sgeE_JOB_SCHEDD_INFO_LIST` | send job schedd info list at registration |
 * | `sgeE_JOB_SCHEDD_INFO_ADD` | event jobs schedd info added |
 * | `sgeE_JOB_SCHEDD_INFO_DEL` | event jobs schedd info deleted |
 * | `sgeE_JOB_SCHEDD_INFO_MOD` | event jobs schedd info modified |
 * | `sgeE_NEW_SHARETREE` | replace possibly existing share tree |
 * | `sgeE_PE_LIST` | send pe list at registration |
 * | `sgeE_PE_ADD` | event pe add |
 * | `sgeE_PE_DEL` | event pe delete |
 * | `sgeE_PE_MOD` | event pe modify |
 * | `sgeE_PROJECT_LIST` | send project list at registration |
 * | `sgeE_PROJECT_ADD` | event project add |
 * | `sgeE_PROJECT_DEL` | event project delete |
 * | `sgeE_PROJECT_MOD` | event project modify |
 * | `sgeE_QMASTER_GOES_DOWN` | qmaster notifies all event clients, before |
 * | `sgeE_QUEUE_LIST` | send queue list at registration |
 * | `sgeE_QUEUE_ADD` | event queue add |
 * | `sgeE_QUEUE_DEL` | event queue delete |
 * | `sgeE_QUEUE_MOD` | event queue modify |
 * | `sgeE_QUEUE_SUSPEND_ON_SUB` | queue is suspended by subordinate mechanism |
 * | `sgeE_QUEUE_UNSUSPEND_ON_SUB` | queue is unsuspended by subordinate mechanism |
 * | `sgeE_SCHED_CONF` | replace existing (sge) scheduler configuration |
 * | `sgeE_SCHEDDMONITOR` | trigger scheduling run |
 * | `sgeE_SHUTDOWN` | request shutdown of an event client |
 * | `sgeE_USER_LIST` | send user list at registration |
 * | `sgeE_USER_ADD` | event user add |
 * | `sgeE_USER_DEL` | event user delete |
 * | `sgeE_USER_MOD` | event user modify |
 * | `sgeE_USERSET_LIST` | send userset list at registration |
 * | `sgeE_USERSET_ADD` | event userset add |
 * | `sgeE_USERSET_DEL` | event userset delete |
 * | `sgeE_USERSET_MOD` | event userset modify |
 * | `sgeE_HGROUP_LIST` | send list of host groups |
 * | `sgeE_HGROUP_ADD` | a host group was added |
 * | `sgeE_HGROUP_DEL` | a host group was deleted |
 * | `sgeE_HGROUP_MOD` | a host group was changed |
 *
 * This list grows as further event situations are identified and interfaced.
 *
 * @note When adding an event here, the event mirror in `libs/mir` has to be
 *       updated as well.
 */

/** @page evc_client Writing an event client
 *
 * The client side of the interface provides functions to register and
 * deregister, to select the data to be sent through subscribe/unsubscribe, and
 * to set the interval at which qmaster sends new events.
 *
 * A client is created with #sge_evc_class_create(), which returns the operation
 * table #sge_evc_class_t. Everything else is reached through that table.
 *
 * `clients/qevent/ocs_qevent.cc` is a simple worked example. The scheduler in
 * `daemons/qmaster/sge_thread_scheduler.cc` is also implemented as a (local)
 * event client and uses every mechanism the interface offers.
 *
 * @see @ref evc_interface
 */

/** @brief Seconds an internal event client waits for new events before looking again */
#define EC_TIMEOUT_S 10

/** @brief Handover point between an internal event client and the event master
 *
 * Only used by qmaster internal clients. The event master fills #new_events and
 * signals #cond_var; the client thread wakes up in `ec2_get_local()`, takes the
 * list over and clears #triggered. All fields are protected by #mutex.
 */
struct ec_control_t {
   pthread_mutex_t mutex;       ///< protects every other field of this struct
   pthread_cond_t  cond_var;    ///< signalled when events arrive or the client must exit
   bool            exit;        ///< true once event delivery shall stop
   bool            triggered;   ///< true while #new_events holds events not yet taken over
   ocs::tom::ListPtr<ocs::tom::ET> new_events; ///< events handed over by the event master, owned by this struct
   bool            new_global_conf; ///< set when the global configuration changed
};

/** @brief Private state of one event client
 *
 * Reached through #sge_evc_class_str::sge_evc_handle. These fields used to be
 * file scope globals, which limited a process to a single event client; they
 * now live per client so one process can connect to several event servers.
 */
struct sge_evc_t {
   bool need_register;          ///< true while the client is not registered at the server
   ocs::tom::ElementPtr<ocs::tom::EV> ec; ///< `EV` object holding the configuration, see @ref evc_ev_type
   uint32_t ec_reg_id;          ///< id used to register at qmaster, see @ref evc_ids
   uint32_t next_event;         ///< sequence number of the next event expected
   ec_control_t event_control;  ///< handover point, internal clients only
};


static bool ck_event_number(ocs::tom::List<ocs::tom::ET> lp, uint32_t *waiting_for, uint32_t *wrong_number);
static void ec2_add_subscriptionElement(sge_evc_class_t *thiz, ev_event event, bool flush, int interval);
static void ec2_remove_subscriptionElement(sge_evc_class_t *thiz, ev_event event);
static void ec2_mod_subscription_flush(sge_evc_class_t *thiz, ev_event event, bool flush, int intervall);
static void ec2_config_changed(sge_evc_class_t *thiz);
static bool sge_evc_setup(sge_evc_class_t *thiz, ev_registration_id id, const char *ec_name);
/** @brief Release the private state of an event client
 *
 * Broadcasts on the condition variable first, so a thread blocked in
 * `ec2_get_local()` leaves before the condition variable is destroyed, then
 * frees the buffered events and the `EV` object.
 *
 * @param[in,out] sge_evc address of the state to free; ignored when nullptr or
 *                        already nullptr
 */
static void sge_evc_destroy(sge_evc_t **sge_evc);
static bool ec2_is_initialized(sge_evc_class_t *thiz);
static ocs::tom::Element<ocs::tom::EV> ec2_get_event_client(sge_evc_class_t *thiz);
static void ec2_mark4registration(sge_evc_class_t *thiz);
static bool ec2_need_new_registration(sge_evc_class_t *thiz);
static bool ec2_set_edtime(sge_evc_class_t *thiz, uint32_t interval);
static uint32_t ec2_get_edtime(sge_evc_class_t *thiz);
static bool ec2_set_busy_handling(sge_evc_class_t *thiz, ev_busy_handling handling);
static ev_busy_handling ec2_get_busy_handling(sge_evc_class_t *thiz);
static bool ec2_register(sge_evc_class_t *thiz, bool exit_on_qmaster_down, ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static bool ec2_deregister(sge_evc_class_t *thiz);
static bool ec2_deregister_local(sge_evc_class_t *thiz);
static bool ec2_register_local(sge_evc_class_t *thiz, [[maybe_unused]] bool exit_on_qmaster_down,
                               ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static bool ec2_subscribe(sge_evc_class_t *thiz, ev_event event);
static bool ec2_mod_subscription_where(sge_evc_class_t *thiz, ev_event event, const lListElem *what,
                                       const lListElem *where);
static bool ec2_subscribe_all(sge_evc_class_t *thiz);
static bool ec2_unsubscribe(sge_evc_class_t *thiz, ev_event event);
static bool ec2_unsubscribe_all(sge_evc_class_t *thiz);
static int ec2_get_flush(sge_evc_class_t *thiz, ev_event event);
static bool ec2_set_flush(sge_evc_class_t *thiz, ev_event event, bool flush, int interval);
static bool ec2_unset_flush(sge_evc_class_t *thiz, ev_event event);
static bool ec2_subscribe_flush(sge_evc_class_t *thiz, ev_event event, int flush);
static bool ec2_set_busy(sge_evc_class_t *thiz, int busy);
static bool ec2_get_busy(sge_evc_class_t *thiz);
static bool ec2_set_session(sge_evc_class_t *thiz, const char *session);
static const char *ec2_get_session(sge_evc_class_t *thiz);
static bool ec2_commit(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static bool ec2_commit_local(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static bool ec2_commit_multi(sge_evc_class_t *thiz, lList **malpp, ocs::gdi::Request *gdi_multi);
static bool ec2_ack(sge_evc_class_t *thiz);
static bool ec2_get(sge_evc_class_t *thiz, lList **event_list, bool exit_on_qmaster_down);
static std::optional<ocs::tom::ListPtr<ocs::tom::REP>> get_event_list(sge_evc_class_t *thiz, int sync,
                                                                     int *commlib_error);
static ev_registration_id ec2_get_id(sge_evc_class_t *thiz);
static bool ec2_get_local(sge_evc_class_t *thiz, lList **elist, bool exit_on_qmaster_down);
static void ec2_wait_local(sge_evc_class_t *thiz);
static int ec2_signal_local(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                            ocs::tom::List<ocs::tom::REP> event_list);
static void ec2_wait(sge_evc_class_t *thiz);
static int ec2_signal(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                      ocs::tom::List<ocs::tom::REP> event_list);
static ec_control_t *ec2_get_event_control(sge_evc_class_t *thiz);
static bool ec2_evco_triggered(sge_evc_class_t *thiz);
static bool ec2_evco_exit(sge_evc_class_t *thiz);

/** @brief Create an event client and prepare it for registration
 *
 * Allocates the handle, fills its operation table and sets up the state needed
 * to register at the event server. Whether the external or the internal set of
 * operations is installed is decided here, from
 * `component_is_qmaster_internal()`; callers use the result identically either
 * way.
 *
 * The events `sgeE_QMASTER_GOES_DOWN`, `sgeE_SHUTDOWN` and `sgeE_ACK_TIMEOUT`
 * are subscribed with immediate flushing, because a client must always react to
 * them. The busy handling is preset to `EV_BUSY_UNTIL_ACK`.
 *
 * No communication happens yet - the client registers on the first
 * #sge_evc_class_str::ec_register or #sge_evc_class_str::ec_get.
 *
 * @param reg_id id to register with, see @ref evc_ids. Must be below
 *               `EV_ID_FIRST_DYNAMIC`; use `EV_ID_ANY` to let qmaster assign one
 * @param alpp   answer list, filled on error
 * @param name   name of the event client, shown by `qconf -sec` and used in
 *               messages. When nullptr, the component name is used
 * @return the new handle, or nullptr on error, in which case @p alpp describes
 *         the failure. Release it with #sge_evc_class_destroy()
 *
 * @see #sge_evc_class_destroy(), @ref evc_client
 */
sge_evc_class_t *
sge_evc_class_create(ev_registration_id reg_id, ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *name) {
   DENTER(EVC_LAYER);

   auto *ret = (sge_evc_class_t *)sge_malloc(sizeof(sge_evc_class_t));
   if (!ret) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      DRETURN(nullptr);
   }

   /*
   ** get type of connection internal/external
   */
   bool is_qmaster_internal = component_is_qmaster_internal();

   DPRINTF("creating %s event client context\n", is_qmaster_internal ? "internal" : "external");

   if (is_qmaster_internal) {
      ret->ec_register = ec2_register_local;
      ret->ec_deregister = ec2_deregister_local;
      ret->ec_commit = ec2_commit_local;
      ret->ec_get = ec2_get_local;
      ret->ec_signal = ec2_signal_local;
      ret->ec_wait = ec2_wait_local;
   } else {
      ret->ec_register = ec2_register;
      ret->ec_deregister = ec2_deregister;
      ret->ec_commit = ec2_commit;
      ret->ec_get = ec2_get;
      ret->ec_signal = ec2_signal;
      ret->ec_wait = ec2_wait;
   }

   ret->ec_is_initialized = ec2_is_initialized;
   ret->ec_get_event_client = ec2_get_event_client;
   ret->ec_subscribe = ec2_subscribe;
   ret->ec_subscribe_all = ec2_subscribe_all;
   ret->ec_unsubscribe = ec2_unsubscribe;
   ret->ec_unsubscribe_all = ec2_unsubscribe_all;
   ret->ec_get_flush = ec2_get_flush;
   ret->ec_set_flush = ec2_set_flush;
   ret->ec_unset_flush = ec2_unset_flush;
   ret->ec_subscribe_flush = ec2_subscribe_flush;
   ret->ec_mod_subscription_where = ec2_mod_subscription_where;
   ret->ec_set_edtime = ec2_set_edtime;
   ret->ec_get_edtime = ec2_get_edtime;
   ret->ec_set_busy_handling = ec2_set_busy_handling;
   ret->ec_get_busy_handling = ec2_get_busy_handling;
   ret->ec_set_busy = ec2_set_busy;
   ret->ec_get_busy = ec2_get_busy;
   ret->ec_set_session = ec2_set_session;
   ret->ec_get_session = ec2_get_session;
   ret->ec_get_id = ec2_get_id;
   ret->ec_commit_multi = ec2_commit_multi;
   ret->ec_mark4registration = ec2_mark4registration;
   ret->ec_need_new_registration = ec2_need_new_registration;
   ret->ec_ack = ec2_ack;
   ret->ec_evco_triggered = ec2_evco_triggered;
   ret->ec_evco_exit = ec2_evco_exit;

   ret->sge_evc_handle = nullptr;

   // value-initialised: the owners of the struct are empty, the plain fields zero
   auto *sge_evc = new (std::nothrow) sge_evc_t{};
   if (!sge_evc) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      sge_evc_class_destroy(&ret);
      DRETURN(nullptr);
   }
   sge_evc->need_register = true;
   sge_evc->ec_reg_id = 0;
   sge_evc->next_event = 1;

   ret->sge_evc_handle = sge_evc;

   if (!sge_evc_setup(ret, reg_id, name)) {
      sge_evc_class_destroy(&ret);
      DRETURN(nullptr);
   }

   DRETURN(ret);
}

/** @brief Destroy an event client and release everything it owns
 *
 * Wakes any thread waiting for events, destroys the condition variable and
 * mutex, frees the buffered events and the `EV` object, and finally the
 * handle itself. @p pst is set to nullptr.
 *
 * This does **not** deregister at the event server; call
 * #sge_evc_class_str::ec_deregister first if the server should stop spooling
 * events immediately rather than after the client times out.
 *
 * Passing nullptr, or a pointer to nullptr, is allowed and does nothing.
 *
 * @param[in,out] pst address of the handle to destroy; set to nullptr on return
 *
 * @see #sge_evc_class_create()
 */
void sge_evc_class_destroy(sge_evc_class_t **pst) {
   DENTER(EVC_LAYER);

   if (pst == nullptr || *pst == nullptr) {
      DRETURN_VOID;
   }

   sge_evc_destroy((sge_evc_t **)&((*pst)->sge_evc_handle));
   sge_free(pst);
   DRETURN_VOID;
}

/**
 * @brief Free the state of an event client: wake up the threads waiting on it, the new events and the client
 * @param sge_evc the state; nullptr afterwards. A null holder or state frees nothing
 */
static void sge_evc_destroy(sge_evc_t **sge_evc) {
   DENTER(EVC_LAYER);

   if (sge_evc == nullptr || *sge_evc == nullptr) {
      DRETURN_VOID;
   }

   /*
   ** signal all threads waiting on condition before destroy
   */
   pthread_mutex_lock(&((*sge_evc)->event_control.mutex));
   pthread_cond_broadcast(&((*sge_evc)->event_control.cond_var));
   pthread_mutex_unlock(&((*sge_evc)->event_control.mutex));

   pthread_cond_destroy(&((*sge_evc)->event_control.cond_var));
   pthread_mutex_destroy(&((*sge_evc)->event_control.mutex));
   (*sge_evc)->event_control.new_events.reset();

   (*sge_evc)->ec.reset();
   delete *sge_evc;
   *sge_evc = nullptr;

   DRETURN_VOID;
}

/**
 * @brief Prepare the client for registration at the event server
 *
 * Initialises the per client state, creates the `EV` object and fills in
 * the data needed to register. No communication happens here.
 *
 * The three events `sgeE_QMASTER_GOES_DOWN`, `sgeE_SHUTDOWN` and
 * `sgeE_ACK_TIMEOUT` are subscribed with immediate flushing, because every
 * client has to react to them. The busy handling is preset to
 * `EV_BUSY_UNTIL_ACK`.
 *
 * @param thiz the event client handle
 * @param id id to register with; must be below `EV_ID_FIRST_DYNAMIC`, see
 *           @ref evc_ids
 * @param ec_name name of the event client. Informational only: it appears in
 *                errors, warnings and infos, and is shown by `qconf -sec`.
 *                When nullptr, the component name is used
 * @return true on success. False if @p id is out of range or the name is
 *         empty, and also if the `EV` object could not be created
 *
 * @see @ref evc_ev_type, @ref evc_ids, @ref evc_subscription
 */
static bool
sge_evc_setup(sge_evc_class_t *thiz, ev_registration_id id, const char *ec_name) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t*)thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /*
   ** event_control setup for internal event clients
   */
   pthread_mutex_init(&(sge_evc->event_control.mutex), nullptr);
   ocs::uti::condition_initialize(&(sge_evc->event_control.cond_var));
   sge_evc->event_control.exit = false;
   sge_evc->event_control.triggered = false;
   sge_evc->event_control.new_events.reset();
   sge_evc->event_control.new_global_conf = false;

   const char *name;
   if (ec_name != nullptr) {
      name = ec_name;
   } else {
      name = component_get_component_name();
   }

   if (id >= EV_ID_FIRST_DYNAMIC || name == nullptr || *name == 0) {
      WARNING(MSG_EVENT_ILLEGAL_ID_OR_NAME_US, static_cast<uint32_t>(id), name != nullptr ? name : "nullptr" );
   } else {
      // the client keeps the configuration object in its private state and frees it in sge_evc_destroy()
      sge_evc->ec = ocs::tom::ElementPtr<ocs::tom::EV>::create();

      if (sge_evc->ec != nullptr) {
         char tmp_string[CL_MAXHOSTNAMELEN + 1];

         /* remember registration id for subsequent registrations */
         sge_evc->ec_reg_id = id;

         /* initialize event client object */
         sge_evc->ec->set<EV_name>(name);
         if (gethostname(tmp_string, CL_MAXHOSTNAMELEN) == 0) {
            sge_evc->ec->set<EV_host>(ocs::tom::Host(tmp_string));
         }
         /*
         ** for internal clients we reuse the data of the gdi context
         */
         sge_evc->ec->set<EV_commproc>(component_get_component_name());
         sge_evc->ec->set<EV_commid>(0);
         sge_evc->ec->set<EV_d_time>(DEFAULT_EVENT_DELIVERY_INTERVAL);

         /* always subscribe this three events */
         ec2_subscribe_flush(thiz, sgeE_QMASTER_GOES_DOWN, 0);
         ec2_subscribe_flush(thiz, sgeE_SHUTDOWN, 0);
         ec2_subscribe_flush(thiz, sgeE_ACK_TIMEOUT, 0);

         ec2_set_busy_handling(thiz, EV_BUSY_UNTIL_ACK);
         sge_evc->ec->set<EV_busy>(0);
         ec2_config_changed(thiz);
         ret = true;
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Has the client been initialized
 *
 * Checks if the event client mechanism has been initialized
 * (if #sge_evc_class_create() has been called).
 *
 * @param thiz the event client handle
 * @return true, if the event client interface has been initialized, else false.
 *
 * @see #sge_evc_class_create()
 */
static bool ec2_is_initialized(sge_evc_class_t *thiz) {
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc == nullptr || sge_evc->ec == nullptr) {
      return false;
   } else {
      return true;
   }
}

/**
 * @brief Return the event client object (EV) holding the configuration
 *
 * @param thiz the event client handle
 * @return the event client object owned by @p thiz, or an empty handle when it is not initialized
 *
 * @see #sge_evc_class_create()
 */
static ocs::tom::Element<ocs::tom::EV> ec2_get_event_client(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   const auto *sge_evc = static_cast<sge_evc_t *>(thiz->sge_evc_handle);

   // created by sge_evc_class_create() and owned by the client
   DRETURN(sge_evc->ec.get());
}

/**
 * @brief New registration is required
 *
 * Tells the event client mechanism, that the connection to the server
 * is broken and it has to reregister.
 *
 * @param thiz the event client handle
 * @note Should be no external interface. The event client mechanism should itself
 *       detect such situations and react accordingly.
 *
 * @see #sge_evc_class_str::ec_need_new_registration
 */
static void
ec2_mark4registration(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   auto *sge_evc = (sge_evc_t*)thiz->sge_evc_handle;
   const char *master_name = ocs::gdi::ClientBase::gdi_get_act_master_host(true);

   // If the client is internal, we do not need to close the connection.
   if (!component_is_qmaster_internal()) {
      cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
      if (handle != nullptr) {
         cl_commlib_close_connection(handle, (char*)master_name, to_cstr(QMASTER), 1, false);
         DPRINTF("closed old connection to qmaster\n");
      }
   }

   sge_evc->need_register = true;
   DPRINTF("*** Need new registration at qmaster ***\n");
   // after a deregistration the client holds no configuration object any more; nothing to mark then
   if (sge_evc->ec) {
      sge_evc->ec->set<EV_changed>(true);
   }
   DRETURN_VOID;
}

/**
 * @brief Is a reregistration neccessary?
 *
 * Function to check, if a new registration at the server is neccessary.
 *
 * @param thiz the event client handle
 * @return true, if the client has to (re)register, else false
 */
static bool
ec2_need_new_registration(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   auto sge_evc = (sge_evc_t*) thiz->sge_evc_handle;
   DRETURN(sge_evc->need_register);
}

/**
 * @brief Set the event delivery interval
 *
 * Set the interval qmaster will use to send events to the client.
 * Any number > 0 is a valid interval in seconds. However the interval
 * my not be larger than the commd commproc timeout. Otherwise the event
 * client encounters receive timeouts and this is returned as an error
 * by #sge_evc_class_str::ec_get.
 *
 * @param thiz the event client handle
 * @param interval interval [s]
 *
 * @return 1, if the value was changed, else 0
 *
 * @note The maximum interval is limited to the commd commproc timeout.
 *       The maximum interval should be limited also by the application.
 *       A too big interval makes qmaster spool lots of events and consume
 *       a lot of memory.
 *
 * @see #sge_evc_class_str::ec_get_edtime
 */
static bool
ec2_set_edtime(sge_evc_class_t *thiz, uint32_t interval) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *)thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      ret = (sge_evc->ec->get<EV_d_time>() != interval);
      if (ret) {
         sge_evc->ec->set<EV_d_time>(std::min(interval, static_cast<uint32_t>(CL_DEFINE_CLIENT_CONNECTION_LIFETIME-5)));
         ec2_config_changed(thiz);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Get the current event delivery interval
 *
 * Get the interval qmaster will use to send events to the client.
 *
 * @param thiz the event client handle
 * @return the interval in seconds
 *
 * @see #sge_evc_class_str::ec_set_edtime
 */
static uint32_t
ec2_get_edtime(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   uint32_t interval = 0;
   auto *sge_evc = (sge_evc_t *)thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      interval = sge_evc->ec->get<EV_d_time>();
   }

   DRETURN(interval);
}

/**
 * @brief Set the event client busy handling
 *
 * The event client interface has a mechanism to handle situations in which
 * an event client is busy and will not accept any new events.
 * The policy to use can be configured using this function.
 * The available policies are listed in @ref evc_busy_state.
 * This parameter can be changed during runtime and will take effect
 * after the next commit or fetch.
 *
 * @param thiz the event client handle
 * @param handling the policy to use
 *
 * @return true, if the value was changed, else false
 *
 * @see #sge_evc_class_str::ec_get_busy_handling, @ref evc_busy_state, #sge_evc_class_str::ec_commit, #sge_evc_class_str::ec_get
 */
static bool
ec2_set_busy_handling(sge_evc_class_t *thiz, ev_busy_handling handling) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      DPRINTF("EVC: change event client to " sge_u32 "\n", static_cast<uint32_t>(handling));

      ret = (sge_evc->ec->get<EV_busy_handling>() != handling) ? true : false;

      if (ret) {
         sge_evc->ec->set<EV_busy_handling>(handling);
         ec2_config_changed(thiz);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Get configured busy handling policy
 *
 * Returns the policy currently configured.
 *
 * @param thiz the event client handle
 * @return the current policy
 *
 * @see #sge_evc_class_str::ec_set_edtime, @ref evc_busy_state
 */
static ev_busy_handling
ec2_get_busy_handling(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   ev_busy_handling handling = EV_BUSY_NO_HANDLING;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      handling = (ev_busy_handling)sge_evc->ec->get<EV_busy_handling>();
   }

   DRETURN(handling);
}

/** @brief Deregister a qmaster internal client from the event master
 *
 * Sets #ec_control_t::exit and signals the condition variable so a thread
 * waiting in `ec2_get_local()` wakes up and stops, then removes the client from
 * the event master and clears the local state so a later registration starts
 * from scratch.
 *
 * @param thiz the event client handle
 * @return true on success, false if the client was never initialised or has no
 *         event control block
 */
static bool
ec2_deregister_local(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /* not yet initialized? Nothing to shutdown */
   if (sge_evc == nullptr || sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      local_t *evc_local = &(thiz->ec_local);
      uint32_t id = sge_evc->ec_reg_id;

      DPRINTF("ec2_deregister_local sge_evc->ec_reg_id %d\n", sge_evc->ec_reg_id);

      /*
      ** signal thread when in ec2_get_local
      */
      ec_control_t *evco = ec2_get_event_control(thiz);
      if (evco == nullptr) {
         DPRINTF("ec2_deregister_local evco IS nullptr\n");
         DRETURN(false);
      }

      sge_mutex_lock("event_control_mutex", __func__, __LINE__, &(evco->mutex));

      evco->exit = true;

      DPRINTF("----> evco->exit = true\n");

      pthread_cond_signal(&(evco->cond_var));
#ifdef EVC_DEBUG
      {
      DSTRING_STATIC(dsbuf, 64);
      printf("EVENT_CLIENT %d has been signaled at %s\n", thiz->ec_get_id(thiz), sge_ctime64(sge_get_gmt64(), &dsbuf));
      }
#endif
      sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &(evco->mutex));

      if (id != 0 && evc_local && evc_local->remove_func) {
         evc_local->remove_func(id);
      }

      /* clear state of this event client instance */
      sge_evc->ec.reset();
      sge_evc->need_register = true;
      sge_evc->ec_reg_id = 0;
      sge_evc->next_event = 1;

      ret = true;
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/** @brief Register a qmaster internal client at the event master
 *
 * Calls the event master directly through #local_t::add_func rather than going
 * through the GDI. Registering an already known client modifies it instead. The
 * busy handling is set to `EV_BUSY_UNTIL_RELEASED` first, so the event master
 * does not deliver again until the client explicitly releases itself.
 *
 * Returns immediately when no new registration is needed.
 *
 * @param thiz the event client handle
 * @param exit_on_qmaster_down unused; an internal client cannot outlive qmaster
 * @param alpp answer list, filled when the event master refuses the client
 * @return true on success, else false
 */
static bool
ec2_register_local(sge_evc_class_t *thiz, [[maybe_unused]] bool exit_on_qmaster_down,
                   ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(EVC_LAYER);

   bool ret = true;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (!thiz->ec_need_new_registration(thiz)) {
      DRETURN(ret);
   }

   sge_evc->next_event = 1;

   DPRINTF("trying to register as internal client with preset %d (0 means EV_ID_ANY)\n", (int)sge_evc->ec_reg_id);

   if (sge_evc->ec == nullptr) {
      WARNING(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
      ret = false;
   } else {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      local_t *evc_local = &(thiz->ec_local);

      sge_evc->ec->set<EV_id>(sge_evc->ec_reg_id);

      /* initialize, we could do a re-registration */
      sge_evc->ec->set<EV_last_heard_from>(0);
      sge_evc->ec->set<EV_last_send_time>(0);
      sge_evc->ec->set<EV_next_send_time>(0);
      sge_evc->ec->set<EV_next_number>(0);

      /*
       *  to add may also mean to modify
       *  - if this event client is already enrolled at qmaster
       */

      if (evc_local && evc_local->add_func) {
         // out-parameter of the add function, which hands the registered client over raw
         lList *eclp_raw = nullptr;

         // for internal request we create a pseudo packet just containing
         // information required for potential error message
         ocs::gdi::Packet pseudo_packet;
         strcpy(pseudo_packet.user, ocs::Bootstrap::get_admin_user());
         strcpy(pseudo_packet.host, ocs::gdi::ClientBase::gdi_get_act_master_host(false));

         /*
         ** set busy handling, sets EV_changed to true if it is really changed
         */
         thiz->ec_set_busy_handling(thiz, EV_BUSY_UNTIL_RELEASED);
         evc_local->add_func(&pseudo_packet, sge_evc->ec->raw(), &alp, &eclp_raw, evc_local->update_func,
                             evc_local->update_func_arg);
         // the registered client (EV) as the event master returns it
         if (const auto eclp = ocs::tom::ListPtr<ocs::tom::EV>::from_typed(eclp_raw)) {
            sge_evc->ec_reg_id = eclp->front().get<EV_id>();
         }
      }

      ocs::tom::ConstElement<ocs::tom::AN> aep;
      if (alp != nullptr) {
         aep = alp->front();
         ret = ((aep.get<AN_status>() == STATUS_OK) ? true : false);
      }

      if (!ret) {
         if (aep.get<AN_quality>() == ANSWER_QUALITY_ERROR) {
            ERROR("%s", aep.get<AN_text>());
            answer_list_add(alpp, aep.get<AN_text>(),
                  aep.get<AN_status>(), (answer_quality_t)aep.get<AN_quality>());

            ret = false;
         }
      } else {
         sge_evc->ec->set<EV_changed>(false);
         sge_evc->need_register = false;
         DPRINTF("registered local event client with id " sge_u32 "\n", sge_evc->ec_reg_id);
      }

      alp.reset();
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);
   DRETURN(ret);
}

/**
 * @brief Register at the event server
 *
 * Registers the event client at the event server (usually the qmaster).
 * This function can be called explicitly in the event client at startup
 * or when the connection to qmaster is down.
 *
 * It is also called implicitly by #sge_evc_class_str::ec_get whenever that
 * detects the client has to (re)register.
 *
 * @param thiz the event client handle
 * @param exit_on_qmaster_down when true, a registration refused with an error
 *                             terminates the process instead of returning false
 * @param alpp answer list, filled when qmaster refuses the registration
 * @return true, if the registration succeeded, else false
 *
 * @see #sge_evc_class_str::ec_deregister, #sge_evc_class_str::ec_get
 */
static bool
ec2_register(sge_evc_class_t *thiz, bool exit_on_qmaster_down, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      WARNING(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      ocs::tom::ConstElement<ocs::tom::AN> aep;
      /*
       *   EV_host, EV_commproc and EV_commid get filled
       *  at qmaster side with more secure commd
       *  informations
       *
       *  EV_uid gets filled with gdi_request
       *  informations
       */

      sge_evc->ec->set<EV_id>(sge_evc->ec_reg_id);

      /* initialize, we could do a re-registration */
      sge_evc->ec->set<EV_last_heard_from>(0);
      sge_evc->ec->set<EV_last_send_time>(0);
      sge_evc->ec->set<EV_next_send_time>(0);
      sge_evc->ec->set<EV_next_number>(0);

      auto registration = ocs::tom::ListPtr<ocs::tom::EV>::create("registration");
      // the configuration object is owned by this client
      registration->push_back(sge_evc->ec->clone());


#if 0
   cl_com_handle_t* com_handle = nullptr;
   const char* progname = nullptr;
   const char* mastername = nullptr;

      progname = ocs::gdi::Client::sge_gdi_ctx->get_progname(sge_gdi_ctx);
      mastername = ocs::gdi::Client::sge_gdi_ctx->get_master(sge_gdi_ctx);

      /* TODO: is this code section really necessary */
      /* closing actual connection to qmaster and reopen new connection. This will delete all
         buffered messages  - CR */
      com_handle = ocs::gdi::Client::sge_gdi_ctx->get_com_handle(sge_gdi_ctx);
      if (com_handle != nullptr) {
         int ngc_error;
         ngc_error = cl_commlib_close_connection(com_handle, mastername, to_cstr(QMASTER), 1, false);
         if (ngc_error == CL_RETVAL_OK) {
            DPRINTF("closed old connection to qmaster\n");
         } else {
            INFO("error closing old connection to qmaster: " SFN4, cl_get_error_text(ngc_error));
         }
         ngc_error = cl_commlib_open_connection(com_handle, mastername, to_cstr(QMASTER), 1);
         if (ngc_error == CL_RETVAL_OK) {
            DPRINTF("opened new connection to qmaster\n");
         } else {
            ERROR("error opening new connection to qmaster: " SFN4, cl_get_error_text(ngc_error));
         }
      }
#endif

      /*
       *  to add may also means to modify
       *  - if this event client is already enrolled at qmaster
       */
      // sge_gdi() replaces the request list with the response, the registered client (EV); freed on every
      // way out of this block
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EV_LIST, ocs::gdi::Command::ADD,
                                      ocs::gdi::SubCommand::RETURN_NEW_VERSION, registration);
      const auto lp = std::move(registration);

      aep = alp->front();

      ret = (aep.get<AN_status>() == STATUS_OK) ? true : false;

      if (ret) {
         uint32_t new_id = 0;

         if (const auto new_ec = lp->front()) {
            new_id = new_ec.get<EV_id>();
         }

         if (new_id != 0) {
            sge_evc->ec->set<EV_id>(new_id);
            DPRINTF("REGISTERED with id " sge_u32 "\n", new_id);
            sge_evc->ec->set<EV_changed>(false);
            sge_evc->need_register = false;

         }
      } else {
         if (aep.get<AN_quality>() == ANSWER_QUALITY_ERROR) {
            ERROR("%s", aep.get<AN_text>());
            answer_list_add(alpp, aep.get<AN_text>(),
                  aep.get<AN_status>(),
                  (answer_quality_t)aep.get<AN_quality>());
            alp.reset();
            /* TODO: remove exit_on_qmaster_down and move to calling code by delivering
                     better return values */
            if (exit_on_qmaster_down) {
               DPRINTF("exiting in ec2_register()\n");
               sge_exit(1);
            } else {
               /*
                * Trigger commlib in case of errors. This is to prevent 100% CPU usage
                * when client does not handle errors and perform a wait before retry
                * in an endless while loop.
                */
               cl_com_handle_t* com_handle = cl_com_get_handle(component_get_component_name(), 0);
               if (com_handle != nullptr) {
                  // @todo is this required in all cases? Only if we are using commlib single threaded, check this?
                  cl_commlib_trigger(com_handle, 1);
               } else {
                  /* We have no commlib handle, do a sleep() */
                  sleep(1);
               }
               DRETURN(false);
            }
         }
      }

      alp.reset();
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Deregister from the event server
 *
 * Deregister from the event server (usually the qmaster).
 * This function should be called when an event client exits.
 *
 * If an event client does not deregister, qmaster will spool events for this
 * client until it times out (it did not acknowledge events sent by qmaster).
 * After the timeout, it will be deleted.
 *
 * @param thiz the event client handle
 * @return true, if the deregistration succeeded, else false
 *
 * @see #sge_evc_class_str::ec_register
 */
static bool ec2_deregister(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /* not yet initialized? Nothing to shutdown */
   if (sge_evc->ec != nullptr) {
      sge_pack_buffer pb;

      if (ocs::tom::try_init_buffer(&pb, sizeof(uint32_t)) == PACK_SUCCESS) {
         /* error message is output from ocs::tom::try_init_buffer() */
         int send_ret;
         ocs::tom::ListPtr<ocs::tom::AN> alp;
         /* TODO: to master only !!!!! */
         const char* commproc = to_cstr(QMASTER);
         const char* rhost = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
         const int commid   = 1;


         ocs::tom::try_put_uint32(&pb, sge_evc->ec->get<EV_id>());

         send_ret = ocs::gdi::ClientServerBase::sge_gdi_send_any_request(0, nullptr, rhost, commproc, commid, &pb, ocs::gdi::ClientServerBase::TAG_EVENT_CLIENT_EXIT, 0, &alp);

         ocs::tom::clear_buffer(&pb);
         answer_list_output (&alp);

         if (send_ret == CL_RETVAL_OK) {
            /* error message is output from sge_send_any_request */
            /* clear state of this event client instance */
            sge_evc->ec.reset();
            sge_evc->need_register = true;
            sge_evc->ec_reg_id = 0;
            sge_evc->next_event = 1;

            ret = true;
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Subscribe an event
 *
 * Subscribe a certain event.
 * See @ref evc_events for the list of all events.
 * The subscription takes effect on the next commit or fetch.
 * Subscribing everything that is wanted and committing once afterwards is
 * both possible and preferable.
 *
 * @param thiz the event client handle
 * @param event the event number
 *
 * @return true on success, else false
 *
 * @see @ref evc_events, #sge_evc_class_str::ec_subscribe_all, #sge_evc_class_str::ec_unsubscribe, #sge_evc_class_str::ec_unsubscribe_all, #sge_evc_class_str::ec_commit, #sge_evc_class_str::ec_get
 */
static bool
ec2_subscribe(sge_evc_class_t *thiz, ev_event event) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event);
   } else {
      if (event == sgeE_ALL_EVENTS) {
         int i;
         for(i = (int)sgeE_ALL_EVENTS + 1; i < (int)sgeE_EVENTSIZE; i++) {
            ec2_add_subscriptionElement(thiz, (ev_event)i, EV_NOT_FLUSHED, -1);
         }
      } else {
         ec2_add_subscriptionElement(thiz, event, EV_NOT_FLUSHED, -1);
      }

      if (sge_evc->ec->get<EV_changed>()) {
         ret = true;
      }
   }
   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/** @brief Add one event to the subscription list
 *
 * Creates the `EVS` element for @p event if it is not subscribed already,
 * and marks the client's configuration as changed so the next commit sends it.
 * An event that is already subscribed is left untouched, including its flush
 * settings.
 *
 * @param thiz the event client handle
 * @param event the event to subscribe; `sgeE_ALL_EVENTS` is ignored here, the
 *              caller iterates
 * @param flush initial flush setting of the new element
 * @param interval initial flush interval of the new element, in seconds
 */
static void
ec2_add_subscriptionElement(sge_evc_class_t *thiz, ev_event event, bool flush, int interval) {
   DENTER(EVC_LAYER);

   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event);
   } else {
      ocs::tom::Element<ocs::tom::EVS> sub_el;
      ocs::tom::List<ocs::tom::EVS> subscribed = sge_evc->ec->get<EV_subscribed>();
      if (event != sgeE_ALL_EVENTS){
         if (!subscribed) {
            subscribed = sge_evc->ec->get_or_create<EV_subscribed>("subscription list");
         } else {
            sub_el = subscribed.find_first<EVS_id>(event);
         }

         if (!sub_el) {
            auto new_sub_el = ocs::tom::ElementPtr<ocs::tom::EVS>::create();
            new_sub_el->set<EVS_id>(event);
            new_sub_el->set<EVS_flush>(flush);
            new_sub_el->set<EVS_interval>(interval);
            subscribed.push_back(std::move(new_sub_el));

            sge_evc->ec->set<EV_changed>(true);
         }
      }
   }
   DRETURN_VOID;
}

/** @brief Change the flush settings of already subscribed events
 *
 * Applies @p flush and @p intervall to one event, or to every subscribed event
 * when passed `sgeE_ALL_EVENTS`, and marks the configuration as changed.
 * Events the client has not subscribed are skipped silently - that is the
 * normal case for a client that does not subscribe everything.
 *
 * @param thiz the event client handle
 * @param event the event to reconfigure, or `sgeE_ALL_EVENTS` for all of them
 * @param flush true to flush the event, false to deliver it with the regular
 *              interval
 * @param intervall flush interval in seconds
 */
static void
ec2_mod_subscription_flush(sge_evc_class_t *thiz, ev_event event, bool flush, int intervall) {
   DENTER(EVC_LAYER);

   auto *sge_evc = static_cast<sge_evc_t *>(thiz->sge_evc_handle);
   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event);
   } else {
      if (const ocs::tom::List<ocs::tom::EVS> subscribed = sge_evc->ec->get<EV_subscribed>()) {
         if (event == sgeE_ALL_EVENTS) {
            for (int e = sgeE_ALL_EVENTS + 1; e < static_cast<int>(sgeE_EVENTSIZE); e++) {
               if (ocs::tom::Element<ocs::tom::EVS> sub_el = subscribed.find_first<EVS_id>(e)) {
                  sub_el.set<EVS_flush>(flush);
                  sub_el.set<EVS_interval>(intervall);
                  sge_evc->ec->set<EV_changed>(true);
               }
            }
         } else {
            if (ocs::tom::Element<ocs::tom::EVS> sub_el = subscribed.find_first<EVS_id>(event)) {
               sub_el.set<EVS_flush>(flush);
               sub_el.set<EVS_interval>(intervall);
               sge_evc->ec->set<EV_changed>(true);
            }
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Adds an element filter to the event
 *
 * Allows to filter the event date on the master side to reduce the
 * date, which is send to the clients.
 *
 * @param thiz the event client handle
 * @param event event type
 * @param what what condition
 * @param where where condition
 *
 * @return true, if everything went fine
 *
 * @see `ocs_what_to_elem()`, `ocs_what_from_elem()`, `ocs_where_to_elem()`, `ocs_where_from_elem()`
 */
static bool ec2_mod_subscription_where(sge_evc_class_t *thiz, ev_event event, const lListElem *what,
                                       const lListElem *where) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event <= sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event);
   } else {
      const ocs::tom::List<ocs::tom::EVS> subscribed = sge_evc->ec->get<EV_subscribed>();
      if (event != sgeE_ALL_EVENTS){
         if (subscribed) {
            ocs::tom::Element<ocs::tom::EVS> sub_el = subscribed.find_first<EVS_id>(event);
            if (sub_el) {
               // the packed selection and condition of ocs_what_to_elem() and ocs_where_to_elem(), so their type
               // is known; a copy that cannot be made (no memory) leaves the field empty, as lCopyElem() gave nullptr
               ocs::tom::ElementPtr<ocs::tom::PACK> what_copy;
               ocs::tom::ElementPtr<ocs::tom::PACK> where_copy;
               try {
                  what_copy = ocs::tom::ConstElement<ocs::tom::PACK>::from_typed(what).clone();
               } catch (const ocs::RuntimeException &) {
               }
               try {
                  where_copy = ocs::tom::ConstElement<ocs::tom::PACK>::from_typed(where).clone();
               } catch (const ocs::RuntimeException &) {
               }
               sub_el.set<EVS_what>(std::move(what_copy));
               sub_el.set<EVS_where>(std::move(where_copy));
               sge_evc->ec->set<EV_changed>(true);
               ret = true;
            }
         }
      }
   }

   DRETURN(ret);
}

/** @brief Remove one event from the subscription list
 *
 * Marks the configuration as changed when an element was actually removed.
 *
 * @param thiz the event client handle
 * @param event the event to unsubscribe; `sgeE_ALL_EVENTS` is ignored here, the
 *              caller iterates
 */
static void
ec2_remove_subscriptionElement(sge_evc_class_t *thiz, ev_event event) {
   DENTER(EVC_LAYER);
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event);
   } else {
      ocs::tom::List<ocs::tom::EVS> subscribed = sge_evc->ec->get<EV_subscribed>();
      if (event != sgeE_ALL_EVENTS) {
         if (subscribed) {
            if (const auto sub_el = subscribed.find_first<EVS_id>(event)) {
               // the element was found in this very list, so the removal cannot fail
               subscribed.erase(sub_el);
               sge_evc->ec->set<EV_changed>(true);
            }
         }
      }
   }
   DRETURN_VOID;
}
/**
 * @brief Subscribe all events
 *
 * Subscribe all possible event.
 * The subscription takes effect on the next commit or fetch.
 *
 * @param thiz the event client handle
 * @return true on success, else false
 *
 * @note Subscribing all events can cause a lot of traffic and may
 *       decrease performance of qmaster.
 *       Only subscribe all events, if you really need them.
 *
 * @see #sge_evc_class_str::ec_subscribe, #sge_evc_class_str::ec_unsubscribe, #sge_evc_class_str::ec_unsubscribe_all, #sge_evc_class_str::ec_commit, #sge_evc_class_str::ec_get
 */
static bool ec2_subscribe_all(sge_evc_class_t *thiz) {
   return ec2_subscribe(thiz, sgeE_ALL_EVENTS);
}

/**
 * @brief Unsubscribe an event
 *
 * Unsubscribe a certain event.
 * See ... for a list of all events.
 * The change takes effect on the next commit or fetch.
 * Unsubscribing everything no longer needed and committing once afterwards
 * is both possible and preferable.
 *
 * @param thiz the event client handle
 * @param event the event to unsubscribe
 *
 * @return true on success, else false
 *
 * @note The events sgeE_QMASTER_GOES_DOWN and sgeE_SHUTDOWN cannot
 *       be unsubscribed. Attempting it logs an error and changes nothing.
 *
 * @see @ref evc_events, #sge_evc_class_str::ec_subscribe, #sge_evc_class_str::ec_subscribe_all, #sge_evc_class_str::ec_unsubscribe_all, #sge_evc_class_str::ec_commit, #sge_evc_class_str::ec_get
 */
static bool
ec2_unsubscribe(sge_evc_class_t *thiz, ev_event event) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event );
   } else {
      if (event == sgeE_ALL_EVENTS) {
         int i;
         for (i = (int)sgeE_ALL_EVENTS + 1; i < (int)sgeE_EVENTSIZE; i++) {
            ec2_remove_subscriptionElement(thiz, (ev_event)i);
         }
         ec2_add_subscriptionElement(thiz, sgeE_QMASTER_GOES_DOWN, EV_FLUSHED, 0);
         ec2_add_subscriptionElement(thiz, sgeE_ACK_TIMEOUT, EV_FLUSHED, 0);
         ec2_add_subscriptionElement(thiz, sgeE_SHUTDOWN, EV_FLUSHED, 0);

      } else {
         if (event == sgeE_QMASTER_GOES_DOWN || event == sgeE_SHUTDOWN || event == sgeE_ACK_TIMEOUT) {
            ERROR(SFNMAX, MSG_EVENT_HAVETOHANDLEEVENTS);
         } else {
            ec2_remove_subscriptionElement(thiz, event);
         }
      }

      if (sge_evc->ec->get<EV_changed>()) {
         ret = true;
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Unsubscribe all events
 *
 * Unsubscribe all possible event.
 * The change takes effect on the next commit or fetch.
 *
 * @param thiz the event client handle
 * @return true on success, else false
 *
 * @note The events sgeE_QMASTER_GOES_DOWN and sgeE_SHUTDOWN will not be
 *       unsubscribed.
 *
 * @see #sge_evc_class_str::ec_subscribe, #sge_evc_class_str::ec_subscribe_all, #sge_evc_class_str::ec_unsubscribe, #sge_evc_class_str::ec_commit, #sge_evc_class_str::ec_get
 */
static bool ec2_unsubscribe_all(sge_evc_class_t *thiz) {
   return ec2_unsubscribe(thiz, sgeE_ALL_EVENTS);
}

/**
 * @brief Get flushing information for an event
 *
 * An event client can request flushing of events from qmaster
 * for any number of the events subscribed.
 * This function returns the flushing information for an
 * individual event.
 *
 * @param thiz the event client handle
 * @param event the event id to query
 *
 * @return EV_NO_FLUSH or the number of seconds used for flushing
 *
 * @see @ref evc_flushing, #sge_evc_class_str::ec_set_flush
 */
static int
ec2_get_flush(sge_evc_class_t *thiz, ev_event event) {
   DENTER(EVC_LAYER);
   int ret = EV_NO_FLUSH;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event );
   } else {
      ocs::tom::ConstElement<ocs::tom::EVS> sub_event = sge_evc->ec->get<EV_subscribed>().find_first<EVS_id>(event);

      if (sub_event == nullptr) {
         WARNING(MSG_EVENT_NOTSUBSCRIBED_I, event);
      } else if (sub_event.get<EVS_flush>()) {
         ret = sub_event.get<EVS_interval>();
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Set flushing information for an event
 *
 * An event client can request flushing of events from qmaster
 * for any number of the events subscribed.
 * This function sets the flushing information for an individual
 * event.
 *
 * @param thiz the event client handle
 * @param event id of the event to configure
 * @param flush true for flushing
 * @param interval flush interval in sec.
 *
 * @return true on success, else false
 *
 * @see @ref evc_flushing, #sge_evc_class_str::ec_get_flush, #sge_evc_class_str::ec_unset_flush,
 *      #sge_evc_class_str::ec_subscribe_flush
 */
static bool
ec2_set_flush(sge_evc_class_t *thiz, ev_event event, bool flush, int interval) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event );
   } else {
      if (!flush ) {
         PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);
         ret = ec2_unset_flush(thiz, event);
         ec2_mod_subscription_flush(thiz, event, EV_NOT_FLUSHED, EV_NO_FLUSH);
         PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);
/*      } else if (interval < 0 || interval > EV_MAX_FLUSH) {
         WARNING(MSG_EVENT_ILLEGALFLUSHTIME_I, interval); */
      } else {
         if (event == sgeE_ALL_EVENTS) {
            // ec2_mod_subscription_flush() walks the subscribed events itself and
            // silently skips the ones that are not subscribed, so there is nothing
            // to iterate over here. The loop that used to be at this place called
            // it once per event id - every call walking the whole list again - and
            // logged an error for each id the client had not subscribed, which is
            // the normal case for a client that does not subscribe everything.
            ec2_mod_subscription_flush(thiz, event, EV_FLUSHED, interval);
            if (sge_evc->ec->get<EV_changed>()) {
               ret = true;
            }
         } else {
            const auto sub_event = sge_evc->ec->get<EV_subscribed>().find_first<EVS_id>(event);

            if (sub_event == nullptr) {
               WARNING(MSG_EVENT_NOTSUBSCRIBED_I, event);
            } else {
               ec2_mod_subscription_flush(thiz, event, EV_FLUSHED, interval);
            }
            if (sge_evc->ec->get<EV_changed>()) {
               ret = true;
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Unset flushing information
 *
 * Switch of flushing of an individual event.
 *
 * @param thiz the event client handle
 * @param event if of the event to configure
 *
 * @return true on success, else false
 *
 * @see @ref evc_flushing, #sge_evc_class_str::ec_set_flush, #sge_evc_class_str::ec_get_flush,
 *      #sge_evc_class_str::ec_subscribe_flush
 */
static bool
ec2_unset_flush(sge_evc_class_t *thiz, ev_event event) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else if (event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING(MSG_EVENT_ILLEGALEVENTID_I, event);
   } else {
      const auto sub_event = sge_evc->ec->get<EV_subscribed>().find_first<EVS_id>(event);

      if (sub_event == nullptr) {
         WARNING(MSG_EVENT_NOTSUBSCRIBED_I, event);
      } else {
         ec2_mod_subscription_flush(thiz, event, EV_NOT_FLUSHED, EV_NO_FLUSH);
      }

      if (sge_evc->ec->get<EV_changed>()) {
         ret = true;
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Subscribe an event and set flushing
 *
 * Subscribes and event and configures flushing for this event.
 *
 * @param thiz the event client handle
 * @param event id of the event to subscribe and flush
 * @param flush number of seconds between event creation and flushing of events
 *
 * @return true on success, else false
 *
 * @see @ref evc_subscription, @ref evc_flushing, #sge_evc_class_str::ec_subscribe, #sge_evc_class_str::ec_set_flush
 */
static bool ec2_subscribe_flush(sge_evc_class_t *thiz, ev_event event, int flush) {
   bool ret;

   ret = ec2_subscribe(thiz, event);
   if (ret) {
      if (flush >= 0)
         ret = ec2_set_flush(thiz, event, true, flush);
      else
         ret = ec2_set_flush(thiz, event, false, flush);
   }

   return ret;
}

/**
 * @brief Set the busy state
 *
 * Sets the busy state of the client. This has to be done if
 * the busy policy has been set to EV_BUSY_UNTIL_RELEASED.
 * An event client can set or unset the busy state at any time.
 * While it is marked busy at the qmaster, qmaster will not
 * deliver events to this client.
 * The changed busy state will be communicated to qmaster with
 * the next commit, which the next fetch performs implicitly.
 *
 * @param thiz the event client handle
 * @param busy true = event client busy, true = event client idle
 *
 * @return true = success, false = failed
 *
 * @see @ref evc_busy_state, #sge_evc_class_str::ec_set_busy_handling, #sge_evc_class_str::ec_get_busy
 */
static bool
ec2_set_busy(sge_evc_class_t *thiz, int busy) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      sge_evc->ec->set<EV_busy>(busy);
      ret = true;
   }

   DRETURN(ret);
}

/**
 * @brief Get the busy state
 *
 * Reads the busy state of the event client.
 *
 * @param thiz the event client handle
 * @return true: the client is busy, false: the client is idle
 *
 * @note The function only returns the local busy state in the event
 *       client itself. If this state changes, it will be reported to
 *       qmaster with the next communication, but not back from
 *       qmaster to the client.
 *
 * @see @ref evc_busy_state, #sge_evc_class_str::ec_set_busy_handling, #sge_evc_class_str::ec_set_busy
 */
static bool ec2_get_busy(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      /* JG: TODO: EV_busy should be boolean datatype */
      ret = (sge_evc->ec->get<EV_busy>() > 0) ? true : false;
   }

   DRETURN(ret);
}

/**
 * @brief Specify session key for event filtering
 *
 * Specifies a session that is used in event master for event
 * filtering.
 *
 * @param thiz the event client handle
 * @param session the session key
 *
 * @return true = success, false = failed
 *
 * @see @ref evc_session_filtering, #sge_evc_class_str::ec_get_session
 */
static bool ec2_set_session(sge_evc_class_t *thiz, const char *session) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      sge_evc->ec->set<EV_session>(session);

      /* force communication to qmaster - we may be out of sync */
      ec2_config_changed(thiz);
      ret = true;
   }

   DRETURN(ret);
}

/**
 * @brief Get session key used for event filtering
 *
 * Returns session key that is used in event master for event
 * filtering.
 *
 * @param thiz the event client handle
 * @return the session key
 *
 * @see @ref evc_session_filtering, #sge_evc_class_str::ec_set_session
 */
static const char *ec2_get_session(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   const char *ret = nullptr;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
   } else {
      ret = sge_evc->ec->get<EV_session>();
   }

   DRETURN(ret);
}

/**
 * @brief Return event client id
 *
 * Return event client id.
 *
 * @param thiz the event client handle
 * @return the event client id
 */
static ev_registration_id
ec2_get_id(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);

   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
      DRETURN(EV_ID_INVALID);
   }

   DRETURN((ev_registration_id)sge_evc->ec->get<EV_id>());
}

/**
 * @brief Tell system the config has changed
 *
 * Checkes whether the configuration has changes.
 * Configuration changes can either be changes in the subscription
 * or change of the event delivery interval.
 *
 * @param thiz the event client handle
 * @see #sge_evc_class_str::ec_subscribe, #sge_evc_class_str::ec_subscribe_all, #sge_evc_class_str::ec_unsubscribe, #sge_evc_class_str::ec_unsubscribe_all, #sge_evc_class_str::ec_set_edtime
 */
static void
ec2_config_changed(sge_evc_class_t *thiz) {
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   if (sge_evc != nullptr && sge_evc->ec != nullptr) {
      sge_evc->ec->set<EV_changed>(true);
   }
}

/** @brief Send configuration changes of an internal client to the event master
 *
 * Counterpart of `ec2_commit()` for qmaster internal clients: passes the
 * `EV` object to #local_t::mod_func directly instead of through the GDI.
 * The update callback and its argument are attached to the object first, so the
 * event master knows where to deliver.
 *
 * @param thiz the event client handle
 * @param alpp answer list, filled by the event master on failure
 * @return true when the change was accepted, false when the client is not
 *         initialised, not registered, or the event master rejected it
 */
static bool
ec2_commit_local(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /* not yet initialized? Cannot send modification to qmaster! */
   if (sge_evc->ec == nullptr) {
      DPRINTF(SFN "\n", MSG_EVENT_UNINITIALIZED_EC);
   } else if (thiz->ec_need_new_registration(thiz)) {
      /* not (yet) registered? Cannot send modification to qmaster! */
      DPRINTF(SFN "\n", MSG_EVENT_NOTREGISTERED);
   } else {
      local_t *evc_local = &(thiz->ec_local);
      const char *ruser = ocs::Bootstrap::get_admin_user();
      const char *rhost = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
      sge_evc->ec->set<EV_update_function>((void *)evc_local->update_func);
      sge_evc->ec->set<EV_update_function_arg>((void *)evc_local->update_func_arg);

      /*
       *  to add may also means to modify
       *  - if this event client is already enrolled at qmaster
       */
      ret = ((evc_local->mod_func(sge_evc->ec->raw(), alpp, (char*)ruser, (char*)rhost) == STATUS_OK) ? true : false);

      if (ret) {
         sge_evc->ec->set<EV_changed>(false);
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);
   DRETURN(ret);
}

/** @brief Acknowledge all events received so far
 *
 * Confirms receipt up to the last event handed to the caller, which lets the
 * event master drop them from its spool and, under `EV_BUSY_UNTIL_ACK`, clears
 * the busy state.
 *
 * @param thiz the event client handle
 * @return true when the acknowledgement was accepted, false when the client is
 *         not initialised, not registered, or has no acknowledge callback
 */
static bool
ec2_ack(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   /* not yet initialized? Cannot send modification to qmaster! */
   if (sge_evc->ec == nullptr) {
      DPRINTF(SFN "\n", MSG_EVENT_UNINITIALIZED_EC);
   } else if (thiz->ec_need_new_registration(thiz)) {
      /* not (yet) registered? Cannot send modification to qmaster! */
      DPRINTF(SFN "\n", MSG_EVENT_NOTREGISTERED);
   } else {
      local_t *evc_local = &(thiz->ec_local);
      if (evc_local && evc_local->ack_func) {
         ret = evc_local->ack_func(sge_evc->ec_reg_id, (ev_event) (sge_evc->next_event-1));
      }
   }
   DRETURN(ret);
}

/**
 * @brief Commit configuration changes
 *
 * Configuration changes (subscription and/or event delivery
 * time) will be sent to the event server.
 * The function should be called after (multiple) configuration
 * changes have been made.
 * If it is not explicitly called by the event client program,
 * the next fetch commits configuration changes before looking for new events.
 *
 * @param thiz the event client handle
 * @param alpp answer list, filled when the client is not initialised, not
 *             registered, or qmaster rejects the change
 * @return true on success, else false
 *
 * @see #sge_evc_class_str::ec_commit_multi, #sge_evc_class_str::ec_get
 */
static bool
ec2_commit(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /* not yet initialized? Cannot send modification to qmaster! */
   if (sge_evc->ec == nullptr) {
      DPRINTF(SFN "\n", MSG_EVENT_UNINITIALIZED_EC);
      answer_list_add(alpp, MSG_EVENT_UNINITIALIZED_EC, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   } else if (thiz->ec_need_new_registration(thiz)) {
      /* not (yet) registered? Cannot send modification to qmaster! */
      DPRINTF(SFN "\n", MSG_EVENT_NOTREGISTERED);
      answer_list_add(alpp, MSG_EVENT_NOTREGISTERED, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   } else {
      ocs::tom::ListPtr<ocs::tom::AN> alp;

      auto request = ocs::tom::ListPtr<ocs::tom::EV>::create("change configuration");
      // the configuration object is owned by this client
      const auto ec = request->push_back(sge_evc->ec->clone());
      if (!sge_evc->ec->get<EV_changed>()) {
         ec.set<EV_subscribed>(nullptr);
      }

      /*
       *  to add may also means to modify
       *  - if this event client is already enrolled at qmaster
       */
      // a MOD is not answered with objects; the request is freed at the end of the block
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::EV_LIST, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE,
                                      request);

      if (alp->front().get<AN_status>() == STATUS_OK) {
         alp.reset();
         ret = true;
      } else {
         if (alpp) {
            *alpp = std::move(alp);
         } else {
            alp.reset();
         }
         ret = false;
      }

      if (ret) {
         sge_evc->ec->set<EV_changed>(false);
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);
   DRETURN(ret);
}

/**
 * @brief Commit configuration changes via gdi multi request
 *
 * Sends the same configuration changes as #sge_evc_class_str::ec_commit, but as
 * part of a GDI multi request so they travel together with other requests
 * instead of on their own.
 *
 * This has to be the last request added to the multi request: it triggers the
 * communication of everything queued up.
 *
 * @param thiz the event client handle
 * @param malpp answer list of the whole GDI multi request
 * @param gdi_multi the multi request this commit is appended to and which it
 *                  triggers
 *
 * @return true on success, else false
 *
 * @see #sge_evc_class_str::ec_commit, #sge_evc_class_str::ec_get
 */
static bool
ec2_commit_multi(sge_evc_class_t *thiz, lList **malpp, ocs::gdi::Request *gdi_multi) {
   DENTER(EVC_LAYER);
   bool ret = false;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /* not yet initialized? Cannot send modification to qmaster! */
   if (sge_evc->ec == nullptr) {
      DPRINTF(SFN "\n", MSG_EVENT_UNINITIALIZED_EC);
   } else if (thiz->ec_need_new_registration(thiz)) {
      /* not (yet) registered? Cannot send modification to qmaster! */
      DPRINTF(SFN "\n", MSG_EVENT_NOTREGISTERED);
   } else {
      int commit_id, gdi_ret;
      ocs::tom::ListPtr<ocs::tom::AN> alp;

      /* do not check, if anything has changed.
       * we have to send the request in any case to finish the
       * gdi multi request
       */
      auto request = ocs::tom::ListPtr<ocs::tom::EV>::create("change configuration");
      // the configuration object is owned by this client
      const auto ec = request->push_back(sge_evc->ec->clone());
      if (!sge_evc->ec->get<EV_changed>()) {
         ec.set<EV_subscribed>(nullptr);
      }

      /*
       * TODO: extend ocs::gdi::Client::sge_gdi_ctx_class_t to support ocs::gdi::Client::sge_gdi_multi()
       *  to add may also means to modify
       *  - if this event client is already enrolled at qmaster
       */
      // the request takes the list over
      commit_id = gdi_multi->request(&alp, ocs::gdi::Mode::SEND, ocs::gdi::Target::EV_LIST,
                                     ocs::gdi::Command::MOD, ocs::gdi::SubCommand::NONE, std::move(request));
      gdi_multi->wait();

      if (alp != nullptr) {
         answer_list_handle_request_answer_list(&alp, stderr);
      } else {
         // the modification answers with no objects
         ocs::tom::ListPtr<ocs::tom::EV> no_objects;
         gdi_multi->get_response(&alp, ocs::gdi::Command::ADD, ocs::gdi::SubCommand::NONE,
                                 ocs::gdi::Target::ORDER_LIST, commit_id, no_objects);

         gdi_ret = answer_list_handle_request_answer_list(&alp, stderr);

         if (gdi_ret == STATUS_OK) {
            sge_evc->ec->set<EV_changed>(false);  /* TODO: call changed method */
            ret = true;
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Look for new events
 *
 * If new events have arrived they are passed back to the caller in
 * @p event_list.
 *
 * A client that is not registered yet registers first.
 *
 * If the configuration changed and was not committed, it is committed before
 * events are looked for.
 *
 * @param thiz the event client handle
 * @param[out] event_list receives the events that arrived; the caller owns and
 *                        must free the list
 * @param exit_on_qmaster_down passed on to the registration, see
 *                             #sge_evc_class_str::ec_register
 *
 * @return true if events, an empty event list, or nothing at all arrived within
 *         the timeout. False on error, in particular on a communication error
 *         or when the client had to be marked for re-registration
 *
 * @see #sge_evc_class_str::ec_register, #sge_evc_class_str::ec_commit
 */
static bool
ec2_get(sge_evc_class_t *thiz, lList **event_list, bool exit_on_qmaster_down) {
   DENTER(EVC_LAYER);
   bool ret = true;
   uint32_t wrong_number;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   if (sge_evc->ec == nullptr) {
      ERROR(SFNMAX, MSG_EVENT_UNINITIALIZED_EC);
      ret = false;
   } else if (thiz->ec_need_new_registration(thiz)) {
      sge_evc->next_event = 1;
      ret = thiz->ec_register(thiz, exit_on_qmaster_down, nullptr);
   }

   if (ret) {
      if (sge_evc->ec->get<EV_changed>()) {
         ret = thiz->ec_commit(thiz, nullptr);
      }
   }

   /* receive event message(s)
    * The following problems exists here:
    * - there might be multiple event reports at commd - so fetching only one
    *   is not sufficient
    * - if we fetch reports until fetching fails, we can run into an endless
    *   loop, if qmaster sends lots of event reports (e.g. due to flushing)
    * - so what number of events shall we fetch?
    *   Let's assume that qmaster will send a maximum of 1 event message per
    *   second. Then our maximum number of messages to fetch is the number of
    *   seconds passed since last fetch.
    *   For the first fetch after registration, we can take the event delivery
    *   interval.
    * - To make sure this algorithm works in all cases, we could restrict both
    *   event delivery time and flush time to intervals greater than 1 second.
    */
   if (ret) {
      static uint64_t last_fetch_time = 0;
      static uint64_t last_fetch_ok_time = 0;
      int commlib_error = CL_RETVAL_UNKNOWN;

      bool done = false;
      bool fetch_ok = false;
      int max_fetch;
      int sync = 1;

      uint64_t now = sge_get_gmt64();

      /* initialize last_fetch_ok_time */
      // this doesn't make sense, last_fetch_ok_time is a timestamp (big number), ed_time is just a few seconds
#if 0
      if (last_fetch_ok_time == 0) {
         last_fetch_ok_time = thiz->ec_get_edtime(thiz);
      }
#endif

      /* initialize the maximum number of fetches
       * - based on ed_time which is the event delivery interval in seconds (??)
       * - assuming that we try one fetch per second (??)
       */
      if (last_fetch_time == 0) {
         max_fetch = thiz->ec_get_edtime(thiz);
      } else {
         max_fetch = sge_gmt64_to_gmt32(now - last_fetch_time);
      }

      last_fetch_time = now;

      DPRINTF("ec2_get retrieving events - will do max %d fetches\n", max_fetch);

      /* fetch data until nothing left or maximum reached */
      while (!done) {
         DPRINTF("doing %s fetch for messages, %d still to do\n", sync ? "sync" : "async", max_fetch);
         if (thiz->ec_need_new_registration(thiz)) {
            ret = false;
            done = true;
            continue;
         }
         auto report_list = get_event_list(thiz, sync, &commlib_error);
         if ((fetch_ok = report_list.has_value())) {
            // REP_list is runtime typed: the events (ET) of the event report, adopted checked; qmaster may have
            // sent no report at all, which gives no events, as the exchange on no report did
            const auto report = (*report_list)->front();
            auto new_events = report ? ocs::tom::adopt<ocs::tom::ET>(
                                          report.exchange<REP_list>(ocs::tom::ListPtr<ocs::tom::ET>{}))
                                     : ocs::tom::ListPtr<ocs::tom::ET>{};
            report_list->reset();
            if (!ck_event_number(*new_events, &(sge_evc->next_event), &wrong_number)) {
               /*
                *  may be we got an old event, that was sent before
                *  reregistration at qmaster
                */
               ocs::tom::ListPtr<ocs::tom::ET>::from_typed(*event_list).reset();
               *event_list = nullptr;
               thiz->ec_mark4registration(thiz);
               ret = false;
               done = true;
               continue;
            }

            DPRINTF("got %zu events till " sge_u32 "\n", new_events->size(), sge_evc->next_event - 1);

            if (*event_list != nullptr) {
               // the caller's list holds the events of the earlier fetches of this call. lAddList() refused
               // a list of another layout softly; splice() throws - not reachable, qmaster always packs full ET
               ocs::tom::List<ocs::tom::ET>::from_typed(*event_list).splice(std::move(new_events));
            } else {
               *event_list = new_events.release();
            }

         } else {
            /* get_event_list failed - we are through */
            done = true;
            continue;
         }

         sync = 0;

         if (--max_fetch <= 0) {
            /* maximum number of fetches reached - stop fetching reports */
            done = true;
         }
      }

      /* if first synchronous get_event_list failed, return error */
      if (sync && !fetch_ok) {
         uint64_t timeout = sge_gmt32_to_gmt64(thiz->ec_get_edtime(thiz) * 10);

         DPRINTF("first syncronous get_event_list failed\n");

         /* we return false when we have reached a timeout or
            on communication error, otherwise we return true */
         ret = true;

         /* check timeout */
         if (last_fetch_ok_time + timeout < now) {
            /* we have a  SGE_EM_TIMEOUT */
            DPRINTF("SGE_EM_TIMEOUT reached\n");
            ret = false;
         } else {
            DPRINTF("SGE_EM_TIMEOUT in " sge_u64 " microseconds\n", last_fetch_ok_time + timeout - now);
         }

         /* check for communication error */
         if (commlib_error != CL_RETVAL_OK) {
            switch (commlib_error) {
               case CL_RETVAL_NO_MESSAGE:
               case CL_RETVAL_SYNC_RECEIVE_TIMEOUT:
                  break;
               default:
                  DPRINTF("COMMUNICATION ERROR: %s\n", cl_get_error_text(commlib_error));
                  ret = false;
                  break;
            }
         }
      } else {
         /* set last_fetch_ok_time, because we had success */
         last_fetch_ok_time = sge_get_gmt64();

         /* send an ack to the qmaster for all received events */
         if (sge_send_ack_to_qmaster(ACK_EVENT_DELIVERY, sge_evc->next_event - 1,
                                     sge_evc->ec->get<EV_id>(), nullptr, &alp)
                                    != CL_RETVAL_OK) {
            answer_list_output(&alp);
            WARNING(SFNMAX, MSG_COMMD_FAILEDTOSENDACKEVENTDELIVERY);
         } else {
            DPRINTF("Sent ack for all events lower or equal %d\n", (sge_evc->next_event - 1));
         }
      }
   }

   // the events handed to the caller (ET), from the network
   const auto events = ocs::tom::ConstList<ocs::tom::ET>::from_typed(*event_list);
   if (events) {
      DPRINTF("ec2_get - received %zu events\n", events.size());
   }

   /* check if we got a QMASTER_GOES_DOWN or sgeE_ACK_TIMEOUT event.
    * if yes, reregister with next event fetch
    */
   for (const auto event : events) {
      const auto tmp_type = event.get<ET_type>();
      if (tmp_type == sgeE_QMASTER_GOES_DOWN || tmp_type == sgeE_ACK_TIMEOUT) {
         ec2_mark4registration(thiz);
         break;
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}


/**
 * @brief Test event numbers
 *
 * Tests list of events if it contains right numbered events.
 *
 * Events with numbers lower than expected get trashed.
 *
 * In cases the master has added no new events to the event list
 * and the acknowledge we sent was lost also a list with events lower
 * than "waiting_for" is correct.
 * But the number of the last event must be at least "waiting_for"-1.
 *
 * On success *waiting_for will contain the next number we wait for.
 *
 * On failure *waiting_for gets not changed and if wrong_number is not
 * nullptr *wrong_number contains the wrong number we got.
 *
 * @param lp event list to check (ET), may be an empty handle; events skipped
 *           are removed from it
 * @param waiting_for next number to wait for
 * @param wrong_number event number that causes a failure
 *
 * @return true on success, else false
 */
static bool
ck_event_number(const ocs::tom::List<ocs::tom::ET> lp, uint32_t *waiting_for, uint32_t *wrong_number) {
   DENTER(EVC_LAYER);
   bool ret = true;
   uint32_t i, j;
   int skipped;

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   i = *waiting_for;

   if (lp.empty()) {
      /* got a dummy event list for alive protocol */
      DPRINTF("received empty event list\n");
   } else {
      DPRINTF("Checking %zu events (" sge_u32 "-" sge_u32 ") while waiting for #" sge_u32 "\n",
              lp.size(), lp.front().get<ET_number>(), lp.back().get<ET_number>(), i);

      /* ensure number of last event is "waiting_for"-1 or higher */
      if ((j=lp.back().get<ET_number>()) < i-1) {
         /* error in event numbers */
         /* could happen if the order of two event messages was exchanged */
         if (wrong_number)
            *wrong_number = j;

         ERROR(MSG_EVENT_HIGHESTEVENTISXWHILEWAITINGFORY_UU , j, i);
      }

      /* ensure number of first event is lower or equal "waiting_for" */
      if ((j=lp.front().get<ET_number>()) > i) {
         /* error in event numbers */
         if (wrong_number) {
            *wrong_number = j;
         }
         ERROR(MSG_EVENT_SMALLESTEVENTXISGRTHYWAITFOR_UU, j, i);
         ret = false;
      } else {

         /* skip leading events till event number is "waiting_for"
            or there are no more events */
         skipped = 0;
         auto ep = lp.front();
         while (ep && ep.get<ET_number>() < i) {
            const auto next = ep.next();
            lp.erase(ep);
            ep = next;
            skipped++;
         }

         if (skipped) {
            DPRINTF("Skipped %d events, still %zu in list\n", skipped, lp.size());
         }

         /* ensure number of events increase */
         for (const auto lep : lp) {
            if ((j=lep.get<ET_number>()) != i++) {
               /*
                  do not change waiting_for because
                  we still wait for this number
               */
               ERROR(SFNMAX, MSG_EVENT_EVENTSWITHNOINCREASINGNUMBERS);
               if (wrong_number) {
                  *wrong_number = j;
               }
               ret = false;
               break;
            }
         }

         if (ret) {
            /* that's the new number we wait for */
            *waiting_for = i;
            DPRINTF("check complete, %zu events in list\n", lp.size());
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/**
 * @brief Get event list via gdi call
 *
 * Tries to retrieve the event list.
 * Returns the incoming data and the commlib status/error code.
 * Used by #sge_evc_class_str::ec_get.
 *
 * @param thiz the event client handle
 * @param sync synchronous transfer
 * @param commlib_error pointer to integer to return communication error
 *
 * @return the report list received (may be an empty handle when qmaster sent none), or std::nullopt on a
 *         communication or unpack error
 *
 * @see #sge_evc_class_str::ec_get
 */
static std::optional<ocs::tom::ListPtr<ocs::tom::REP>>
get_event_list(sge_evc_class_t *thiz, const int sync, int *commlib_error) {
   DENTER(EVC_LAYER);
   std::optional<ocs::tom::ListPtr<ocs::tom::REP>> ret;
   sge_pack_buffer pb;
   int help;
   char rhost[CL_MAXHOSTNAMELEN+1] = "";
   char commproc[CL_MAXHOSTNAMELEN+1] = "";

   PROF_START_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   /* TODO: check if all the functionality of get_event_list has been mapped */

   ocs::gdi::ClientServerBase::ClientServerBaseTag tag = ocs::gdi::ClientServerBase::TAG_REPORT_REQUEST;
   u_short id = 1;

   uint64_t now = sge_get_gmt64();
   DPRINTF("try to get request from %s, id %d\n", to_cstr(QMASTER), id );
   if ((help = ocs::gdi::ClientServerBase::sge_gdi_get_any_request(rhost, commproc, &id, &pb, &tag, sync, 0,
                                                                   nullptr)) != CL_RETVAL_OK) {
      if (help == CL_RETVAL_NO_MESSAGE || help == CL_RETVAL_SYNC_RECEIVE_TIMEOUT) {
         DEBUG("commlib returns after %fs: %s\n", sge_gmt64_to_gmt32_double(sge_get_gmt64() - now),
               cl_get_error_text(help));
      } else {
         WARNING("commlib returns after %fs: %s\n", sge_gmt64_to_gmt32_double(sge_get_gmt64() - now),
                 cl_get_error_text(help));
      }
   } else {
      try {
         // qmaster always sends REP; a list of another type is refused like a broken buffer
         ret = ocs::tom::ListPtr<ocs::tom::REP>::unpack(pb);
      } catch (const ocs::tom::PackException &) {
         // the bytes cut off or no memory, where cull_unpack_list() returned an error and freed what it unpacked
         ERROR(SFNMAX, MSG_EVC_UNPACKREPORTFAILED);
      } catch (const ocs::tom::FieldException &) {
         ERROR(SFNMAX, MSG_EVC_UNPACKREPORTFAILED);
      }
      ocs::tom::clear_buffer(&pb);
   }
   if (commlib_error != nullptr) {
      *commlib_error = help;
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_EVENTCLIENT);

   DRETURN(ret);
}

/** @brief Create an event client and store it in the caller's variable
 *
 * Convenience wrapper around #sge_evc_class_create() for callers that keep the
 * handle in a variable and want a boolean result rather than a pointer.
 *
 * @param[out] evc_ref  receives the new handle; untouched on failure
 * @param reg_id        id to register with, see @ref evc_ids
 * @param alpp          answer list, filled on error
 * @param name          name of the event client, see #sge_evc_class_create()
 * @return true on success, false if @p evc_ref is nullptr or the client could
 *         not be created
 *
 * @see #sge_evc_class_create()
 */
bool
sge_gdi2_evc_setup(sge_evc_class_t **evc_ref, ev_registration_id reg_id, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                   const char * name) {
   DENTER(EVC_LAYER);

   if (evc_ref == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_NULLPOINTER);
      DRETURN(false);
   }

   sge_evc_class_t *evc = sge_evc_class_create(reg_id, alpp, name);
   if (evc == nullptr) {
      DRETURN(false);
   }

   *evc_ref = evc;

   DRETURN(true);
}

/** @brief Event control block of a qmaster internal client
 *
 * @param thiz the event client handle
 * @return the handover point used to exchange events with the event master, or
 *         nullptr when @p thiz is not initialised or is not an internal client
 */
static ec_control_t *ec2_get_event_control(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   ec_control_t *event_control = nullptr;

   if (thiz && thiz->ec_is_initialized(thiz) && component_is_qmaster_internal()) {
      auto *sge_evc = (sge_evc_t*)thiz->sge_evc_handle;
      event_control = &(sge_evc->event_control);
   }
   DRETURN(event_control);
}

/** @brief Wait for and collect events of a qmaster internal client
 *
 * Counterpart of `ec2_get()` for internal clients. Registers first if needed,
 * then blocks on the condition variable until the event master signals new
 * events, until shutdown is requested, or until #EC_TIMEOUT_S seconds have
 * passed. Takes ownership of whatever has accumulated, acknowledges it, marks
 * itself busy and commits that state, so no further events arrive until
 * `ec2_wait_local()` releases it.
 *
 * Receiving `sgeE_ACK_TIMEOUT` marks the client for re-registration.
 *
 * @param thiz the event client handle
 * @param[out] elist receives the events; may be nullptr when none arrived. The
 *                   caller owns and must free the list
 * @param exit_on_qmaster_down passed on to the registration
 * @return true when the wait completed, false when @p thiz is nullptr or the
 *         client has no event control block
 */
static bool ec2_get_local(sge_evc_class_t *thiz, lList **elist, bool exit_on_qmaster_down) {
   DENTER(EVC_LAYER);
   DSTRING_STATIC(ds_buffer, 64);

   if (thiz == nullptr) {
      DRETURN(false);
   }
   ec_control_t *evco = ec2_get_event_control(thiz);
   if (evco == nullptr) {
      DRETURN(false);
   }

   if (thiz->ec_need_new_registration(thiz)) {
      auto *sge_evc = (sge_evc_t *) thiz->sge_evc_handle;
      sge_evc->next_event = 1;
      thiz->ec_register(thiz, exit_on_qmaster_down, nullptr);
   }

   sge_mutex_lock("evco_event_thread_cond_mutex", __func__, __LINE__, &(evco->mutex));

   uint64_t current_time = sge_get_gmt64();
   uint64_t timeout = sge_gmt32_to_gmt64(EC_TIMEOUT_S);
   while (!evco->triggered && !evco->exit &&
          ((sge_get_gmt64() - current_time) < timeout)){
#ifdef EVC_DEBUG
printf("EVENT_CLIENT %d beginning to wait at %s\n", thiz->ec_get_id(thiz), sge_ctime64(sge_get_gmt64(), &ds_buffer));
#endif
      ocs::uti::condition_timedwait(&(evco->cond_var), &(evco->mutex), EC_TIMEOUT_S);
#ifdef EVC_DEBUG
printf("EVENT_CLIENT %d ends to wait at %s\n", thiz->ec_get_id(thiz), sge_ctime64(sge_get_gmt64(), &ds_buffer));
#endif
   }

   /* taking out the new events */
   // the events (ET) the event master queued; taken over from the event control block
   auto event_list = std::move(evco->new_events);
   evco->triggered = false;

   DPRINTF("EVENT_CLIENT id=%d TAKES FROM EVENT QUEUE at %s\n", thiz->ec_get_id(thiz),
           sge_ctime64(sge_get_gmt64(), &ds_buffer));

   sge_mutex_unlock("evco_event_thread_cond_mutex", __func__, __LINE__,
                    &(evco->mutex));

   thiz->ec_ack(thiz);
   thiz->ec_set_busy(thiz, 1);
   thiz->ec_commit(thiz, nullptr);

   const bool ack_timeout = static_cast<bool>(event_list->find_first<ET_type>(sgeE_ACK_TIMEOUT));
   // the caller owns the events now; ec_get keeps its raw out-parameter for its callers in libs/mir and gcs-extensions
   *elist = event_list.release();

   if (ack_timeout) {
      ec2_mark4registration(thiz);
   }
   DRETURN(true);
}

/** @brief Release an internal client's busy state after processing a batch
 *
 * Clears the busy flag set by `ec2_get_local()` and commits it. Without this
 * call the event master keeps the client busy and delivers nothing further.
 *
 * @param thiz the event client handle
 */
static void
ec2_wait_local(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);

   /*
   ** reset busy, important otherwise no new events
   */
   thiz->ec_set_busy(thiz, 0);
   thiz->ec_commit(thiz, nullptr);

   DRETURN_VOID;
}

/** @brief Hand new events to an internal client and wake it up
 *
 * Called by the event master. Moves the events out of @p event_list into the
 * client's queue, appending when a previous batch has not been collected yet,
 * sets #ec_control_t::triggered and broadcasts on the condition variable.
 *
 * Does nothing when the report carries no events.
 *
 * @param thiz the event client handle
 * @param alpp unused
 * @param[in,out] event_list report whose event list is moved into the client's
 *                           queue; emptied on success
 * @return the number of events handed over, or -1 when @p thiz is nullptr or
 *         has no event control block
 */
static int
ec2_signal_local(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                 const ocs::tom::List<ocs::tom::REP> event_list) {
   DENTER(EVC_LAYER);

   if (thiz == nullptr) {
      DPRINTF("EVENT UPDATE FUNCTION thiz IS nullptr\n");
      DRETURN(-1);
   }
   ec_control_t *evco = ec2_get_event_control(thiz);
   if (evco == nullptr) {
      DPRINTF("EVENT UPDATE FUNCTION evco IS nullptr\n");
      DRETURN(-1);
   }

   // the report (REP) the event master built; its event list is runtime typed
   const auto report = event_list.front();
   // the event list of an event report holds events (ET)
   const auto report_events = report.get_as<REP_list, ocs::tom::ET>();
   int num_events = static_cast<int>(report_events.size());
   if (num_events > 0) {
      sge_mutex_lock("event_control_mutex", __func__, __LINE__, &(evco->mutex));
      // the event list of an event report holds events (ET), adopted checked
      auto report_list =
         ocs::tom::adopt<ocs::tom::ET>(report.exchange<REP_list>(ocs::tom::ListPtr<ocs::tom::ET>{}));
      if (evco->new_events) {
         evco->new_events->splice(std::move(report_list));
      } else {
         evco->new_events = std::move(report_list);
      }

      evco->triggered = true;
      DPRINTF("EVENT UPDATE FUNCTION jgdi_event_update_func() HAS BEEN TRIGGERED\n");

      pthread_cond_broadcast(&(evco->cond_var));
#ifdef EVC_DEBUG
{
DSTRING_STATIC(dsbuf, 64);
printf("EVENT_CLIENT %d has been signaled at %s\n", thiz->ec_get_id(thiz), sge_ctime64(sge_get_gmt64(), &dsbuf));
}
#endif
      sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &(evco->mutex));
   }

   DRETURN(num_events);
}

/** @brief Release the busy state - does nothing for external clients
 *
 * External clients acknowledge events as part of `ec2_get()`, so there is no
 * separate release step. Exists so callers can use both flavours identically.
 *
 * @param thiz unused
 */
static void
ec2_wait(sge_evc_class_t *thiz) {
   /* do nothing */
}

/** @brief Deliver events - does nothing for external clients
 *
 * Only the event master pushes events, and only to internal clients. An
 * external client pulls them with `ec2_get()`. Exists so callers can use both
 * flavours identically.
 *
 * @param thiz unused
 * @param alpp unused
 * @param event_list unused
 * @return always 1
 */
static int
ec2_signal(sge_evc_class_t *thiz, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
           const ocs::tom::List<ocs::tom::REP> event_list) {
   /* do nothing */
   return 1;
}

/** @brief Are events waiting to be collected by an internal client?
 *
 * @param thiz the event client handle
 * @return true when the event master queued events that have not been collected
 *         yet; false also when @p thiz is nullptr or is not an internal client
 */
static bool
ec2_evco_triggered(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   if (thiz == nullptr) {
      DRETURN(false);
   }
   ec_control_t *evco = ec2_get_event_control(thiz);
   if (evco == nullptr) {
      DRETURN(false);
   }
   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &(evco->mutex));
   bool ret = evco->triggered;
   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &(evco->mutex));

   DRETURN(ret);
}

/** @brief Has an internal client been asked to shut down?
 *
 * @param thiz the event client handle
 * @return true once deregistration requested the event loop to stop; false also
 *         when @p thiz is nullptr or is not an internal client
 */
static bool
ec2_evco_exit(sge_evc_class_t *thiz) {
   DENTER(EVC_LAYER);
   if (thiz == nullptr) {
      DRETURN(false);
   }
   ec_control_t *evco = ec2_get_event_control(thiz);
   if (evco == nullptr) {
      DRETURN(false);
   }
   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &(evco->mutex));
   bool ret = evco->exit;
   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &(evco->mutex));

   DRETURN(ret);
}

/* function see libs/sgeobj/sge_event.c */

