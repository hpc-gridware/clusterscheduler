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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface of the qevent event client
 */

#include "uti/sge_dstring.h"

#define MAX_TRIGGER_SCRIPTS (10)   ///< how many `-trigger` scripts one invocation may register
#define QEVENT_JB_END 1        ///< trigger event: a job ended
#define QEVENT_JB_TASK_END 2   ///< trigger event: an array task ended

/** @brief Everything the command line asked for */
typedef struct qevent_options {
  int          help_option;           ///< `-help` was given
  int          testsuite_option;      ///< `-ts`: run the subscribe/unsubscribe loop
  int          subscribe_option;      ///< subscribe to the event types named on the command line
  int          monitor_all_option;    ///< subscribe to every event type
  int          trigger_option_count;  ///< how many entries of the two arrays below are in use
  int          trigger_option_events[MAX_TRIGGER_SCRIPTS];      ///< the event each script fires on
  const char*  trigger_option_scripts[MAX_TRIGGER_SCRIPTS];     ///< the script to run for it
  dstring      *error_message;        ///< receives the reason when parsing failed
} qevent_options;
