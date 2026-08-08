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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The ids of the scheduler job information messages
 *
 * These are the reasons the scheduler reports for not being able to run a
 * job - what `qstat -j <job_id>` prints under "scheduling info". The message
 * texts belong to the ids and live in `msg_schedd.h`; sge_get_schedd_text()
 * maps one onto the other.
 */

/**
 * @brief The scheduler job information ids
 *
 * Every message exists **twice**: once job related, with the details of the
 * job that could not run, and once generic, as a summary over all jobs. The
 * print functions turn a job related message into either form, depending on
 * what was asked for.
 *
 * The two halves have to be **in the same order**, because the generic id is
 * computed from the job id by adding the constant distance
 * `SCHEDD_INFO_OFFSET` - the first generic id minus the first job id. Adding
 * a message therefore means three edits: the job related id and its text, the
 * generic id and its text, and the two cases in sge_get_schedd_text().
 *
 * The suffix of a job related id names the arguments of its message: `_S` one
 * string, `_SS` two, `_I` an integer, and so on.
 *
 * For testing use `qstat -j` and `qstat -j <job_id>`.
 */
enum { 
   /* job messages */
   SCHEDD_INFO_CANNOTRUNATHOST_SSS = 0 ,             ///< (-l %s) cannot run at host "%s" because %s
   SCHEDD_INFO_HASNOPERMISSION_SS,                   ///< has no permission for %s "%s"
   SCHEDD_INFO_HASINCORRECTPRJ_SSS,                  ///< (project %s) does not have the correct project to run in %s "%s"
   SCHEDD_INFO_HASNOPRJ_S,                           ///< (no project) does not have the correct project to run in %s "%s"
   SCHEDD_INFO_EXCLPRJ_SSS,                          ///< (project %s) is not allowed to run in %s "%s" based on the excluded project list
   SCHEDD_INFO_QUEUENOTREQUESTABLE_S,                ///< cannot run in queue instance "%s" because queues are non requestable
   SCHEDD_INFO_NOTINHARDQUEUELST_S,                  ///< cannot run in queue "%s" because it is not contained in its hard queue list (-q)
   SCHEDD_INFO_NOTPARALLELQUEUE_S,                   ///< cannot run in queue instance "%s" because it is not of parallel type
   SCHEDD_INFO_NOTINQUEUELSTOFPE_SS,                 ///< cannot run in queue "%s" because PE "%s" is not in pe list
   SCHEDD_INFO_NOTACKPTQUEUE_SS,                     ///< cannot run in queue instance "%s" because it is not of type checkpointing
   SCHEDD_INFO_NOTINQUEUELSTOFCKPT_SS,               ///< cannot run in queue instance "%s" because ckpt object "%s" is not in ckpt list of queue
   SCHEDD_INFO_QUEUENOTINTERACTIVE_S,                ///< cannot run in queue "%s" because it is not of type interactive
   SCHEDD_INFO_NOTASERIALQUEUE_S,                    ///< cannot run in queue instance "%s" because it is not of type batch
   SCHEDD_INFO_NOTPARALLELJOB_S,                     ///< cannot run in queue instance "%s" because the job is not parallel
   SCHEDD_INFO_NOTREQFORCEDRES_SS,                   ///< does not request 'forced' resource "%s" of queue instance %s
   /**
    * (%d slots) would set queue instance "%s" in load alarm state
    *
    * @note The suffix says `_DS` while the message takes an integer and a
    *       string, so the id and its text disagree about the first argument.
    */
   SCHEDD_INFO_WOULDSETQEUEINALARM_DS,
   SCHEDD_INFO_NOSLOTSINQUEUE_S,                     ///< cannot run in queue instance "%s" because it has "0" slots
   SCHEDD_INFO_CANNOTRUNINQUEUE_SSS,                 ///< (-l %s) cannot run in queue "%s" because %s
   SCHEDD_INFO_NORESOURCESPE_,                       ///< cannot run because resources requested are not available for parallel job
   SCHEDD_INFO_TOTALPESLOTSNOTINRANGE_S,             ///< cannot run because total slots of pe "%s" not in range of job
   SCHEDD_INFO_CANNOTRUNGLOBALLY_SS,                 ///< (-l %s) cannot run globally because %s
   SCHEDD_INFO_NOFORCEDRES_SS,                       ///< does not request 'forced' resource "%s" of host %s
   SCHEDD_INFO_NOGLOBFORCEDRES_SS,                   ///< does not request globally 'forced' resource "%s"
   SCHEDD_INFO_CKPTNOTFOUND_,                        ///< cannot run because requested ckpt object not found
   SCHEDD_INFO_PESLOTSNOTINRANGE_SI,                 ///< cannot run in PE "%s" because it only offers %d slots
   SCHEDD_INFO_NOACCESSTOPE_S,                       ///< cannot run because no access to pe "%s"
   SCHEDD_INFO_QUEUEINALARM_SS,                      ///< queue instance "%s" is in suspend alarm: %s
   SCHEDD_INFO_QUEUEOVERLOADED_SS,                   ///< queue instance "%s" dropped because it is overloaded: %s
   SCHEDD_INFO_ALLALARMOVERLOADED_,                  ///< All queues dropped because of overload or full
   SCHEDD_INFO_TURNEDOFF_,                           ///< (Collecting of scheduler job information is turned off - use qalter -w p job_id to verify if the job can be scheduled)
   SCHEDD_INFO_JOBLIST_,                             ///< (Scheduler job information not available for every job)
   SCHEDD_INFO_EXECTIME_,                            ///< execution time not reached
   SCHEDD_INFO_JOBINERROR_,                          ///< Job is in error state
   SCHEDD_INFO_JOBHOLD_,                             ///< Job is in hold state
   SCHEDD_INFO_USRGRPLIMIT_,                         ///< job dropped because of user limitations
   SCHEDD_INFO_JOBDEPEND_,                           ///< job dropped because of job dependencies
   SCHEDD_INFO_NOMESSAGE_,                           ///< there are no messages available
   SCHEDD_INFO_QUEUEFULL_,                           ///< queue instance "%s" dropped because it is full
   SCHEDD_INFO_QUEUESUSP_,                           ///< queue instance "%s" dropped because it is suspended
   SCHEDD_INFO_QUEUEDISABLED_,                       ///< queue instance "%s" dropped because it is disabled
   SCHEDD_INFO_QUEUENOTAVAIL_,                       ///< queue instance "%s" dropped because it is temporarily not available
   SCHEDD_INFO_INSUFFICIENTSLOTS_,                   ///< parallel job requires more slots than available
   SCHEDD_INFO_PEALLOCRULE_S,                        ///< pe "%s" dropped because allocation rule is not suitable
   SCHEDD_INFO_NOPEMATCH_,                           ///< no matching pe found
   SCHEDD_INFO_CLEANUPNECESSARY_S,                   ///< cannot run on host "%s" until clean up of a previous run has finished
   SCHEDD_INFO_MAX_AJ_INSTANCES_,                    ///< not all array task may be started due to 'max_aj_instances'
   SCHEDD_INFO_JOB_CATEGORY_FILTER_,                 ///< Job Filter: this job got ignored in the last scheduling run, because to many other jobs with the same resource request are in the pending list before this one.
   SCHEDD_INFO_CANNOTRUNINQUEUECAL_SU,               ///< cannot run in queue instance "%s" because the job runtime of %d sec. is too long
   SCHEDD_INFO_CANNOTRUNRQS_SSS,                     ///< Job cannot run in queue instance "%s@%s" because exceeds limit in rule %s
   SCHEDD_INFO_JOBDYNAMICALLIMIT_SS,                 ///< Job dropped because of invalid dynamical limit %s in rule %s
   SCHEDD_INFO_CANNOTRUNRQSGLOBAL_SS,                ///< cannot run because it exceeds limit "%s" in rule "%s"
   SCHEDD_INFO_QINOTARRESERVED_SI,                   ///< cannot run in queue instance "%s" because it was not reserved by advance reservation %d
   SCHEDD_INFO_QNOTARRESERVED_SI,                    ///< cannot run in queue "%s" because it was not reserved by advance reservation %d
   SCHEDD_INFO_HNOTARRESERVED_SI,                    ///< cannot run on host "%s" because it was not reserved by advance reservation %d
   SCHEDD_INFO_ARISINERROR_I,                        ///< cannot run because requested advance reservation %d is in error state
   SCHEDD_INFO_CONSUMABLENOVALUE_SS,                 ///< cannot run in queue instance "%s" because consumable "%s" used as load threshold has no value at queue, host or global level

   /* global messages*/
   SCHEDD_INFO_CANNOTRUNATHOST,                      ///< Jobs can not run because no host can satisfy the resource requirements
   SCHEDD_INFO_HASNOPERMISSION,                      ///< There could not be found a queue instance with suitable access permissions
   SCHEDD_INFO_HASINCORRECTPRJ,                      ///< Jobs can not run because queue do not provides the jobs assigned project
   SCHEDD_INFO_HASNOPRJ,                             ///< Jobs are not assigned to a project to get a queue instance
   SCHEDD_INFO_EXCLPRJ,                              ///< Jobs can not run because excluded project list of queue does not allow it
   SCHEDD_INFO_QUEUENOTREQUESTABLE,                  ///< Jobs can not run because queues are configured to be non requestable
   SCHEDD_INFO_NOTINHARDQUEUELST,                    ///< Jobs can not run because queue instance is not contained in its hard queue list
   SCHEDD_INFO_NOTPARALLELQUEUE,                     ///< Jobs can not run because queue instance is not a parallel queue
   SCHEDD_INFO_NOTINQUEUELSTOFPE,                    ///< Jobs can not run because queue instance is not in queue list of PE
   SCHEDD_INFO_NOTACKPTQUEUE,                        ///< Jobs can not run because queue instance is not of type checkpointing
   SCHEDD_INFO_NOTINQUEUELSTOFCKPT,                  ///< Jobs can not run because queue instance is not in queue list of ckpt interface defintion
   SCHEDD_INFO_QUEUENOTINTERACTIVE,                  ///< Jobs can not run because queue instance is not interactive
   SCHEDD_INFO_NOTASERIALQUEUE,                      ///< Jobs can not run because queue instance is not of type batch or transfer
   SCHEDD_INFO_NOTPARALLELJOB,                       ///< Jobs can not run in queue instance because the job is not parallel
   SCHEDD_INFO_NOTREQFORCEDRES,                      ///< Jobs can not run because they do not request 'forced' resource
   SCHEDD_INFO_WOULDSETQEUEINALARM,                  ///< Jobs would set queue in load alarm state
   SCHEDD_INFO_NOSLOTSINQUEUE,                       ///< Jobs can not run because queue has 0 slots
   SCHEDD_INFO_CANNOTRUNINQUEUE,                     ///< Jobs can not run because the resource requirements can not be satified
   SCHEDD_INFO_NORESOURCESPE,                        ///< Jobs can not run because resources requested are not available for parallel job
   SCHEDD_INFO_TOTALPESLOTSNOTINRANGE,               ///< Jobs can not run because total slots of pe are not in range of job
   SCHEDD_INFO_CANNOTRUNGLOBALLY,                    ///< Jobs can not run globally because the resource requirements can not be satified
   SCHEDD_INFO_NOFORCEDRES,                          ///< Jobs can not run because they do not request 'forced' resource
   SCHEDD_INFO_NOGLOBFORCEDRES,                      ///< Jobs can not run globally because they do not request 'forced' resource
   SCHEDD_INFO_CKPTNOTFOUND,                         ///< Jobs can not run because requested ckpt object not found
   SCHEDD_INFO_PESLOTSNOTINRANGE,                    ///< Jobs can not run because available slots combined under PE are not in range of job
   SCHEDD_INFO_NOACCESSTOPE,                         ///< Jobs can not run because they have no access to pe
   SCHEDD_INFO_QUEUEINALARM,                         ///< Jobs can not run because queue instances are in alarm starte
   SCHEDD_INFO_QUEUEOVERLOADED,                      ///< Jobs can not run because queue instances are overloaded
   SCHEDD_INFO_ALLALARMOVERLOADED,                   ///< Jobs can not run because all queue instances are overloaded or full
   SCHEDD_INFO_TURNEDOFF,                            ///< (Collecting of scheduler job information is turned off - use qalter -w p job_id to verify if the job can be scheduled)
   SCHEDD_INFO_JOBLIST,                              ///< (Scheduler job information not available for every job)
   SCHEDD_INFO_EXECTIME,                             ///< Jobs can not run because execution time not reached
   SCHEDD_INFO_JOBINERROR,                           ///< Jobs dropped because of error state
   SCHEDD_INFO_JOBHOLD,                              ///< Jobs dropped because of hold state
   SCHEDD_INFO_USRGRPLIMIT,                          ///< Job dropped because of user limitations
   SCHEDD_INFO_JOBDEPEND,                            ///< Job dropped because of job dependencies
   SCHEDD_INFO_NOMESSAGE,                            ///< There are no messages available
   SCHEDD_INFO_QUEUEFULL,                            ///< Queue instances dropped because they are full
   SCHEDD_INFO_QUEUESUSP,                            ///< Queue instances dropped because they are suspended
   SCHEDD_INFO_QUEUEDISABLED,                        ///< Queue instances dropped because they are disabled
   SCHEDD_INFO_QUEUENOTAVAIL,                        ///< Queue instances dropped because they are temporarily not available
   SCHEDD_INFO_INSUFFICIENTSLOTS,                    ///< Parallel jobs dropped because of insufficient slots
   SCHEDD_INFO_PEALLOCRULE,                          ///< PE dropped because allocation rule is not suitable
   SCHEDD_INFO_NOPEMATCH,                            ///< Parallel job dropped because no matching PE found
   SCHEDD_INFO_CLEANUPNECESSARY,                     ///< Jobs can not run because host cleanup has not finished
   SCHEDD_INFO_MAX_AJ_INSTANCES,                     ///< Not all array tasks may be started due to 'max_aj_instances'
   SCHEDD_INFO_JOB_CATEGORY_FILTER,                  ///< Job Filter: Jobs can not run because the resource requirements cannot be satisfied.
   SCHEDD_INFO_CANNOTRUNINQUEUECAL,                  ///< Jobs cannot run because a calendar will disable a queue soon
   SCHEDD_INFO_CANNOTRUNRQS,                         ///< Jobs cannot run because they exceeds limit in resource quota sets
   SCHEDD_INFO_JOBDYNAMICALLIMIT,                    ///< Jobs dropped because of invalid dynamical limit
   SCHEDD_INFO_CANNOTRUNRQSGLOBAL,                   ///< Jobs dropped because exceeds limit in rule
   SCHEDD_INFO_QINOTARRESERVED,                      ///< Jobs can not run because queue instance was not reserved by advance reservation
   SCHEDD_INFO_QNOTARRESERVED,                       ///< Jobs can not run because queue was not reserved by advance reservation
   SCHEDD_INFO_HNOTARRESERVED,                       ///< Jobs can not run because host was not reserved by advance reservation
   SCHEDD_INFO_ARISINERROR,                          ///< Jobs can not run because requested advance reservation is in error state
   SCHEDD_INFO_CONSUMABLENOVALUE,                    ///< Jobs can not run because a consumable used as load threshold has no value

   TOOBIG   ///< Number of ids, must stay in the last position
};
/**
 * Distance between a job related id and its generic counterpart. Adding it to
 * a job related id yields the generic id of the same message, which only
 * works as long as both halves of the enum stay in the same order.
 */
#define SCHEDD_INFO_OFFSET (SCHEDD_INFO_CANNOTRUNATHOST-SCHEDD_INFO_CANNOTRUNATHOST_SSS)

const char *sge_schedd_text(int number);
