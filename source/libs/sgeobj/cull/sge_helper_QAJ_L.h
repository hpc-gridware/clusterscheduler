#ifndef SGE_QAJ_L_H
#define SGE_QAJ_L_H
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
* @brief Accounting Summary
*
* Used in qsub for summing up usage values (?).
* It contains some of the fields which can appear in the qacct -j job_id output.
* See also the sge_accounting(5) man page.
*
*    SGE_HOST(QAJ_host) - Host Name
*    Host where a job ran.
*
*    SGE_STRING(QAJ_queue) - Cluster Queue Name
*    Cluster queue in which a job ran.
*
*    SGE_STRING(QAJ_group) - User Group
*    Unix user group of the job owner.
*
*    SGE_STRING(QAJ_owner) - Owner
*    Unix user name of the job owner.
*
*    SGE_STRING(QAJ_project) - Project
*    Project the job was running for.
*
*    SGE_STRING(QAJ_department) - Department
*    The job owner's department.
*
*    SGE_DOUBLE(QAJ_ru_wallclock) - Rusage Wallclock
*    Wallclock time from rusage (end time - start_time).
*
*    SGE_DOUBLE(QAJ_ru_utime) - Rusage User Time
*    User CPU time from rusage.
*
*    SGE_DOUBLE(QAJ_ru_stime) - Rusage System Time
*    System CPU time from rusage.
*
*    SGE_DOUBLE(QAJ_ru_maxrss) - Rusage Maximum RSS
*    Maximum resident set size of the job's processes from rusage.
*
*    SGE_DOUBLE(QAJ_ru_inblock) - Rusage Block Input
*    Number of block input operations from rusage.
*
*    SGE_STRING(QAJ_granted_pe) - Granted Parallel Environment
*    In case of a parallel job: The PE the job was running in.
*
*    SGE_ULONG(QAJ_slots) - Slots
*    The number of slots the job occupied.
*
*    SGE_DOUBLE(QAJ_cpu) - Cpu Usage
*    The cpu usage of the job reported by sge_execd in seconds.
*
*    SGE_DOUBLE(QAJ_mem) - Integral Memory Usage
*    The integral memory usage of the job reported by sge_execd in Gbytes cpu seconds.
*
*    SGE_DOUBLE(QAJ_io) - IO Usage
*    The IO usage of the job reported by sge_execd (the amount of data transferred in input and output operations.
*
*    SGE_DOUBLE(QAJ_iow) - IO Wait Time
*    The amount of time the job waited for IO operations to happen as reported by sge_execd.
*
*    SGE_DOUBLE(QAJ_maxvmem) - Maximum Virtual Memory
*    The maximum virtual memory usage of the job reported by sge_execd.
*
*    SGE_ULONG(QAJ_arid) - AR Id
*    The id of an advance reservation the job might have run in.
*
*/

enum {
   QAJ_host = QAJ_LOWERBOUND,
   QAJ_queue,
   QAJ_group,
   QAJ_owner,
   QAJ_project,
   QAJ_department,
   QAJ_ru_wallclock,
   QAJ_ru_utime,
   QAJ_ru_stime,
   QAJ_ru_maxrss,
   QAJ_ru_inblock,
   QAJ_granted_pe,
   QAJ_slots,
   QAJ_cpu,
   QAJ_mem,
   QAJ_io,
   QAJ_iow,
   QAJ_maxvmem,
   QAJ_arid
};

LISTDEF(QAJ_Type)
   SGE_HOST(QAJ_host, CULL_DEFAULT)
   SGE_STRING(QAJ_queue, CULL_DEFAULT)
   SGE_STRING(QAJ_group, CULL_DEFAULT)
   SGE_STRING(QAJ_owner, CULL_DEFAULT)
   SGE_STRING(QAJ_project, CULL_DEFAULT)
   SGE_STRING(QAJ_department, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_ru_wallclock, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_ru_utime, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_ru_stime, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_ru_maxrss, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_ru_inblock, CULL_DEFAULT)
   SGE_STRING(QAJ_granted_pe, CULL_DEFAULT)
   SGE_ULONG(QAJ_slots, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_cpu, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_mem, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_io, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_iow, CULL_DEFAULT)
   SGE_DOUBLE(QAJ_maxvmem, CULL_DEFAULT)
   SGE_ULONG(QAJ_arid, CULL_DEFAULT)
LISTEND

NAMEDEF(QAJN)
   NAME("QAJ_host")
   NAME("QAJ_queue")
   NAME("QAJ_group")
   NAME("QAJ_owner")
   NAME("QAJ_project")
   NAME("QAJ_department")
   NAME("QAJ_ru_wallclock")
   NAME("QAJ_ru_utime")
   NAME("QAJ_ru_stime")
   NAME("QAJ_ru_maxrss")
   NAME("QAJ_ru_inblock")
   NAME("QAJ_granted_pe")
   NAME("QAJ_slots")
   NAME("QAJ_cpu")
   NAME("QAJ_mem")
   NAME("QAJ_io")
   NAME("QAJ_iow")
   NAME("QAJ_maxvmem")
   NAME("QAJ_arid")
NAMEEND

#define QAJ_SIZE sizeof(QAJN)/sizeof(char *)


#endif
