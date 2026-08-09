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
 * @brief Assembling and sending the execution daemon's periodic reports
 *
 * The daemon tells qmaster what this host looks like - its load, its
 * configuration, the jobs on it - on a schedule. Each kind of report has its
 * own function and its own next-send time, so a load report every interval
 * does not drag the configuration report along with it.
 */

#include "cull/cull.h"
#include "sgeobj/sge_daemonize.h"

/** @brief Fills in one kind of report and says when it is next due
 *
 * The `next_send` out parameter is how a report sets its own cadence.
 */
typedef int (*report_func_type)(lList *, uint64_t now, uint64_t *next_send);

/** @brief One kind of report the daemon sends, and when it is next due */
typedef struct report_source {
  int type;                 ///< Which report this is, a `NUM_REP_REPORT_*` value
  report_func_type func;    ///< Fills it in
  uint64_t next_send;       ///< When it is next due
} report_source;

int sge_send_all_reports(uint64_t now, int which, report_source *report_sources);

int sge_add_double2load_report(lList **lpp, const char *name, double value, const char *host, const char *units);
int sge_add_int2load_report(lList **lpp, const char *name, int value, const char *host);
int sge_add_str2load_report(lList **lpp, const char *name, const char *value, const char *host);
