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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The single-character symbols used in configuration values, and their bit values
 */

/* Checkpoint/Restart Constants */
#define CHECKPOINT_AT_MINIMUM_INTERVAL_SYM 'm'   ///< the letter for "checkpoint at the minimum interval of the checkpointing environment"
#define CHECKPOINT_AT_MINIMUM_INTERVAL     0x00000001   ///< bit value: checkpoint at the minimum interval of the checkpointing environment
#define CHECKPOINT_AT_SHUTDOWN_SYM         's'   ///< the letter for "checkpoint before the execution daemon is shut down"
#define CHECKPOINT_AT_SHUTDOWN             0x00000002   ///< bit value: checkpoint before the execution daemon is shut down
#define CHECKPOINT_SUSPEND_SYM             'x'   ///< the letter for "checkpoint when the job is suspended"
#define CHECKPOINT_SUSPEND                 0x00000004   ///< bit value: checkpoint when the job is suspended
#define NO_CHECKPOINT_SYM                  'n'   ///< the letter for "never checkpoint; this one excludes all the others"
#define NO_CHECKPOINT                      0x00000008   ///< bit value: never checkpoint; this one excludes all the others
#define CHECKPOINT_AT_AUTO_RES_SYM         'r'   ///< the letter for "checkpoint before a rescheduling caused by a queue reschedule"
#define CHECKPOINT_AT_AUTO_RES             0x00000010   ///< bit value: checkpoint before a rescheduling caused by a queue reschedule


/* Hold Type Constants */
#define NO_HOLD_SYM                        'n'   ///< the letter for "no hold at all"
#define NO_HOLD                            0x00000010   ///< bit value: no hold at all
#define OTHER_SYM                          'o'   ///< the letter for "hold set by an operator"
#define OTHER                              0x00000020   ///< bit value: hold set by an operator
#define SYSTEM_SYM                         's'   ///< the letter for "hold set by a manager"
#define SYSTEM                             0x00000040   ///< bit value: hold set by a manager
#define USER_SYM                           'u'   ///< the letter for "hold set by the job owner"
#define USER                               0x00000080   ///< bit value: hold set by the job owner

/* EB: TODO: remove obsolete definitions */

#define ALARM_SYM                          'a'   ///< the letter for "a load threshold is exceeded"
#define SUSPEND_ALARM_SYM                  'A'   ///< the letter for "a suspend threshold is exceeded"
#define SUSPEND_ON_COMP_SYM                'c'   ///< the letter for "suspended by the calendar of a competing queue" (an extension, not in POSIX 1003.15D12)
#define SUSPENDED_ON_CALENDAR_SYM          'C'   ///< the letter for "suspended by its queue calendar" (an extension, not in POSIX 1003.15D12)
#define DISABLED_SYM                       'd'   ///< the letter for "disabled by an administrator"
#define DISABLED_ON_CALENDAR_SYM           'D'   ///< the letter for "disabled by its queue calendar" (an extension, not in POSIX 1003.15D12)
#define ENABLED_SYM                        'e'   ///< the letter for "enabled"
#define HELD_SYM                           'h'   ///< the letter for "held back; the hold has to be released before it can run"
#define MIGRATING_SYM                      'm'   ///< the letter for "being migrated to another queue" (an extension, not in POSIX 1003.15D12)
#define QUEUED_SYM                         'q'   ///< the letter for "waiting to be scheduled"
#define RESTARTING_SYM                     'R'   ///< the letter for "being restarted after a checkpoint" (an extension, not in POSIX 1003.15D12)
#define RUNNING_SYM                        'r'   ///< the letter for "running"
#define SUSPENDED_SYM                      's'   ///< the letter for "suspended" (an extension, not in POSIX 1003.15D12)
#define SUSPENDED_ON_SUBORDINATE_SYM       'S'   ///< the letter for "suspended because a superordinate queue is full"
#define SUSPENDED_ON_THRESHOLD_SYM         'T'   ///< the letter for "suspended because a suspend threshold is exceeded" (an extension, not in POSIX 1003.15D12)
#define TRANSISTING_SYM                    't'   ///< the letter for "in transit to an execution host"
#define UNKNOWN_SYM                        'u'   ///< the letter for "the execution host has stopped reporting"
#define WAITING_SYM                        'w'   ///< the letter for "waiting for its start time"
#define EXITING_SYM                        'x'   ///< the letter for "finishing; the execution daemon is collecting the usage" (an extension, not in POSIX 1003.15D12)
#define ERROR_SYM                          'E'   ///< the letter for "in error state; it will not be scheduled again until cleared"
#define FINISHED_SYM                       'f'   ///< the letter for "finished but retained for the configured window (CS-1908)"

/* Keep_list Constants */
#define KEEP_NONE_SYM                      'n'   ///< the letter for "discard both streams"
#define KEEP_NONE                          0x00000000   ///< bit value: discard both streams
#define KEEP_STD_ERROR_SYM                 'e'   ///< the letter for "keep the standard error stream"
#define KEEP_STD_ERROR                     0x00010000   ///< bit value: keep the standard error stream
#define KEEP_STD_OUTPUT_SYM                'o'   ///< the letter for "keep the standard output stream"
#define KEEP_STD_OUTPUT                    0x00020000   ///< bit value: keep the standard output stream

/* Mail Option Constants */
#define MAIL_AT_ABORT_SYM                  'a'   ///< the letter for "mail when the job is aborted"
#define MAIL_AT_ABORT                      0x00040000   ///< bit value: mail when the job is aborted
#define MAIL_AT_BEGINNING_SYM              'b'   ///< the letter for "mail when the job starts"
#define MAIL_AT_BEGINNING                  0x00080000   ///< bit value: mail when the job starts
#define MAIL_AT_EXIT_SYM                   'e'   ///< the letter for "mail when the job ends"
#define MAIL_AT_EXIT                       0x00100000   ///< bit value: mail when the job ends
#define NO_MAIL_SYM                        'n'   ///< the letter for "send no mail; this one excludes all the others"
#define NO_MAIL                            0x00200000   ///< bit value: send no mail; this one excludes all the others
#define MAIL_AT_SUSPENSION_SYM             's'   ///< the letter for "mail when the job is suspended" (an extension, not in POSIX 1003.15D12)
#define MAIL_AT_SUSPENSION                 0x00400000   ///< bit value: mail when the job is suspended
