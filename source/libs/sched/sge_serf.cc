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

/** @file
 * @brief Implementation of SERF, the schedule entry recording facility
 */

#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"

#include "sge_serf.h"

/** @brief The callbacks the current recorder registered */
typedef struct {
   record_schedule_entry_func_t record_schedule_entry;   ///< Called once per schedule entry
   new_schedule_func_t new_schedule;                     ///< Called once per scheduling interval
} sge_serf_t;
static sge_serf_t current_serf = { nullptr, nullptr }; /* thread local */


/**
 * @brief Registers the callbacks of a schedule recorder
 *
 * Passing nullptr for both switches recording off again.
 *
 * @param[in] write   called once per schedule entry, see
 *                    #record_schedule_entry_func_t
 * @param[in] newline called once per scheduling interval, see
 *                    #new_schedule_func_t
 *
 * @note MT-NOTE: serf_init() is not MT safe
 */
void serf_init(record_schedule_entry_func_t write, new_schedule_func_t newline)
{
   current_serf.record_schedule_entry = write;
   current_serf.new_schedule          = newline;
}


/**
 * @brief Add a new schedule entry record
 *
 * The entirety of all information passed to this function describes
 * the schedule that was created during a scheduling interval of a
 * Cluster Scheduler scheduler. To reflect multiple resource debitations
 * of a job multiple calls to serf_record_entry() are required. For
 * parallel jobs the serf_record_entry() is called one times with a
 * 'P' as level_char.
 *
 * @param[in] job_id      The job id
 * @param[in] ja_taskid    The task id
 * @param[in] type         Why the utilization is in the schedule, one of the
 *                         SCHEDULING_RECORD_ENTRY_TYPE_* strings
 * @param[in] start_time   Start of the resource utilization
 * @param[in] end_time     End of the resource utilization
 * @param[in] level        The level the resource sits at - queue, host,
 *                         global or parallel environment; converted to the
 *                         `level_char` the callback receives
 * @param[in] object_name  Name of the queue, host, global or PE
 * @param[in] name         Resource name
 * @param[in] utilization  Utilization amount
 *
 * @note MT-NOTE: (1) serf_record_entry() is MT safe if no recording function
 *       MT-NOTE:     was registered via serf_init().
 *       MT-NOTE: (2) Otherwise MT safety of serf_record_entry() depends on
 *       MT-NOTE:     MT safety of registered recording function
 */
void serf_record_entry(uint32_t job_id, uint32_t ja_taskid,
                       const char *type, uint64_t start_time, uint64_t end_time, uint32_t level,
                       const char *object_name, const char *name, double utilization)
{
   DENTER(TOP_LAYER);

   char level_char = CENTRY_LEVEL_TO_CHAR(level);

   /* human-readable format */
   if (DPRINTF_IS_ACTIVE) {
      DSTRING_STATIC(dstr_s, 64);
      DSTRING_STATIC(dstr_e, 64);
      DPRINTF("J=" sge_u32 "." sge_u32 " T=%s S=%s E=%s L=%c O=%s R=%s U=%f\n",
              job_id, ja_taskid, type, sge_ctime64(start_time, &dstr_s), sge_ctime64(end_time, &dstr_e),
              level_char, object_name, name, utilization);
   }

   if (current_serf.record_schedule_entry && serf_get_active()) {
      (current_serf.record_schedule_entry)(job_id, ja_taskid, type, start_time, end_time, 
            level_char, object_name, name, utilization);
   }
   DRETURN_VOID;
}


/**
 * @brief Indicate the end of a  scheduling run
 *
 * When a new scheduling run ended serf_new_interval() shall be
 * called to indicate this. This allows assigning of schedule entry
 * records to different schedule runs.
 *
 * @note MT-NOTE: (1) serf_new_interval() is MT safe if no recording function
 *       MT-NOTE:     was registered via serf_init().
 *       MT-NOTE: (2) Otherwise MT safety of serf_new_interval() depends on
 *       MT-NOTE:     MT safety of registered recording function
 */
void serf_new_interval()
{
   DENTER(TOP_LAYER);

   DPRINTF("================[SCHEDULING-DONE]==================\n");

   if (current_serf.new_schedule && serf_get_active()) {
      (current_serf.new_schedule)();
   }

   DRETURN_VOID;
}


/**
 * @brief Closes SERF
 *
 * All operations requited to cleanly shutdown the SERF are done.
 *
 * @note MT-NOTE: serf_exit() is MT safe
 */
void serf_exit()
{
   memset(&current_serf, 0, sizeof(sge_serf_t)); 
}

