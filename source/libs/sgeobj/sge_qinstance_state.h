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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The state bits of a queue instance and the transitions that change them
 *
 * @see sge_qinstance_state.cc
 */

#include "cull/cull.h"
#include "uti/sge_dstring.h"

/**
 * @name Queue instance states
 *
 * Bits of `QU_state`. Several can be set at once, and the letters `qstat`
 * prints are derived from them - see #qinstance_state_as_string.
 * @{
 */
#define QI_DEFAULT                     0x00000000 ///< no state bit set; the queue is available
#define QI_ALARM                       0x00000001 ///< a load threshold is exceeded, so no job is dispatched here
#define QI_SUSPEND_ALARM               0x00000002 ///< a suspend threshold is exceeded, so running jobs get suspended
#define QI_DISABLED                    0x00000004 ///< an administrator disabled the queue
#define QI_SUSPENDED                   0x00000100 ///< an administrator suspended the queue
#define QI_UNKNOWN                     0x00000400 ///< the execution host has not reported for too long
#define QI_ERROR                       0x00004000 ///< a job could not be started here and the cause is unclear
#define QI_SUSPENDED_ON_SUBORDINATE    0x00008000 ///< suspended because a subordinate relation demanded it
#define QI_CAL_DISABLED                0x00020000 ///< disabled by the queue's calendar
#define QI_CAL_SUSPENDED               0x00040000 ///< suspended by the queue's calendar
#define QI_AMBIGUOUS                   0x00080000 ///< the cluster queue's configuration does not resolve for this host
#define QI_ORPHANED                    0x00100000 ///< the queue was deleted but still holds jobs
#define QI_FULL                        0x00200000 ///< every slot is in use
/** @} */

/**
 * @name Queue instance state transitions
 *
 * What a `qmod` request asks for. These are not the state bits above: a
 * transition names an action, and several of them deliberately share a value
 * with the state bit they set or clear.
 * @{
 */
#define QI_DO_NOTHING                  0x00000000 ///< no action
#define QI_DO_DISABLE                  0x00000004 ///< disable the queue
#define QI_DO_ENABLE                   0x00000008 ///< enable the queue again
#define QI_DO_UNSUSPEND                0x00000080 ///< resume a suspended queue
#define QI_DO_SUSPEND                  0x00000100 ///< suspend the queue
#define QI_DO_CLEARERROR               0x00004000 ///< clear #QI_ERROR
#define QI_DO_CLEAN                    0x00010000 ///< drop the jobs the queue still holds
#define QI_DO_RESCHEDULE               0x00080000 ///< reschedule the jobs running here
#define QI_DO_CAL_DISABLE              0x00020000 ///< disable because the calendar says so
#define QI_DO_CAL_SUSPEND              0x00040000 ///< suspend because the calendar says so
#define QI_DO_RECONNECT                0x08000000 ///< CS-2144: client GDI request for IJS session reconnect

#ifdef __SGE_QINSTANCE_STATE_DEBUG__
#  define QI_DO_SETERROR               0x00100000 ///< debug only: set #QI_ERROR by hand
#  define QI_DO_SETORPHANED            0x00200000 ///< debug only: set #QI_ORPHANED by hand
#  define QI_DO_CLEARORPHANED          0x00400000 ///< debug only: clear #QI_ORPHANED by hand
#  define QI_DO_SETUNKNOWN             0x00800000 ///< debug only: set #QI_UNKNOWN by hand
#  define QI_DO_CLEARUNKNOWN           0x01000000 ///< debug only: clear #QI_UNKNOWN by hand
#  define QI_DO_SETAMBIGUOUS           0x02000000 ///< debug only: set #QI_AMBIGUOUS by hand
#  define QI_DO_CLEARAMBIGUOUS         0x04000000 ///< debug only: clear #QI_AMBIGUOUS by hand
#endif

/// The request names a job, so the transition applies to that job
#define JOB_DO_ACTION                  0x80000000
/// The request names a queue, so the transition applies to that queue
#define QUEUE_DO_ACTION                0x40000000
/** @} */

/**
 * @name Queue instance transition options
 *
 * Passed alongside a transition to say how far it reaches.
 *
 * @warning Everything from here down to the second
 *          `transition_option_is_valid_for_qinstance` declaration exists
 *          **twice** in this header, verbatim. The values are identical and
 *          the declarations agree, so it compiles; it is an accidental paste,
 *          not a deliberate conditional. Delete one copy when this file is
 *          next touched for a code change.
 * @{
 */
#define QI_TRANSITION_NOTHING          0x00000000 ///< the transition applies to nothing further
#define QI_TRANSITION_OPTION           0x00000001 ///< the transition also applies to the jobs in the queue
/** @} */

/// Is this a transition a queue instance accepts?
bool
transition_is_valid_for_qinstance(uint32_t transition, lList **answer_list);

/// Is this a transition option a queue instance accepts?
bool
transition_option_is_valid_for_qinstance(uint32_t option, lList **answer_list);

#ifdef __SGE_QINSTANCE_STATE_DEBUG__
#  define QI_DO_SETERROR               0x00100000
#endif

/**
 * @name Queue instance transition options (duplicate)
 *
 * @warning An accidental verbatim copy of the block above. See the warning
 *          there.
 * @{
 */
#define QI_TRANSITION_NOTHING          0x00000000 ///< duplicate of the definition above
#define QI_TRANSITION_OPTION           0x00000001 ///< duplicate of the definition above
/** @} */

/// Duplicate declaration; see the block above
bool
transition_is_valid_for_qinstance(uint32_t transition, lList **answer_list);

/// Duplicate declaration; see the block above
bool
transition_option_is_valid_for_qinstance(uint32_t option, lList **answer_list);

bool qinstance_has_state(const lListElem *this_elem, uint32_t bit);

const char * qinstance_state_as_string(uint32_t bit);

uint32_t qinstance_state_from_string(const char* state, lList **answer_list, uint32_t filter);
/* */

bool
qinstance_state_set_alarm(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_suspend_alarm(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_manual_disabled(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_manual_suspended(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_unknown(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_error(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_susp_on_sub(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_cal_disabled(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_cal_suspended(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_full(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_orphaned(lListElem *this_elem, bool set_state);

bool
qinstance_state_set_ambiguous(lListElem *this_elem, bool set_state);

bool 
qinstance_state_is_alarm(const lListElem *this_elem);

bool 
qinstance_state_is_suspend_alarm(const lListElem *this_elem);

bool 
qinstance_state_is_manual_disabled(const lListElem *this_elem);

bool 
qinstance_state_is_manual_suspended(const lListElem *this_elem);

bool 
qinstance_state_is_unknown(const lListElem *this_elem);

bool 
qinstance_state_is_error(const lListElem *this_elem);

bool 
qinstance_state_is_susp_on_sub(const lListElem *this_elem);

bool 
qinstance_state_is_cal_disabled(const lListElem *this_elem);

bool 
qinstance_state_is_cal_suspended(const lListElem *this_elem);

bool
qinstance_state_is_orphaned(const lListElem *this_elem);

bool
qinstance_state_is_ambiguous(const lListElem *this_elem);

bool 
qinstance_state_is_full(const lListElem *this_elem);

bool 
qinstance_state_append_to_dstring(const lListElem *this_elem, dstring *string);

bool
qinstance_set_state(lListElem *this_elem, bool set_state, uint32_t bit);
