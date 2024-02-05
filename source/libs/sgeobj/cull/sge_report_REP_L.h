#ifndef SGE_REP_L_H
#define SGE_REP_L_H
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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Report
*
* A report from sge_execd to sge_qmaster, e.g. a load report, a job report, ...
*
*    SGE_ULONG(REP_type) - Report Type
*    The type of the report, defined in source/libs/sgeobj/sge_report.h, e.g.
*    - NUM_REP_REPORT_LOAD: a load report
*    - NUM_REP_REPORT_JOB: a job report
*    - ...
*
*    SGE_HOST(REP_host) - Host Name
*    Hostname as it is seen by sender of report.
*
*    SGE_LIST(REP_list) - Report List
*    A list of report items, depending on the report type, e.g.
*    a list of load values (HL_Type) objects for load reports or
*    a list of job usage values (UA_Type) objects for job reports.
*
*    SGE_ULONG(REP_version) - Software Version
*    Used to report software version (the GDI version) of execd.
*    Only components with the same GDI version can communicate.
*
*    SGE_ULONG(REP_seqno) - Report Sequence Number
*    Used to recognize old reports sent by execd.
*
*/

enum {
   REP_type = REP_LOWERBOUND,
   REP_host,
   REP_list,
   REP_version,
   REP_seqno
};

LISTDEF(REP_Type)
   SGE_ULONG(REP_type, CULL_DEFAULT)
   SGE_HOST(REP_host, CULL_DEFAULT)
   SGE_LIST(REP_list, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(REP_version, CULL_DEFAULT)
   SGE_ULONG(REP_seqno, CULL_DEFAULT)
LISTEND

NAMEDEF(REPN)
   NAME("REP_type")
   NAME("REP_host")
   NAME("REP_list")
   NAME("REP_version")
   NAME("REP_seqno")
NAMEEND

#define REP_SIZE sizeof(REPN)/sizeof(char *)


#endif
