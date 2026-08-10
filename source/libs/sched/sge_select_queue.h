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
 * @brief Matching a job against queues and hosts - the core of the scheduler
 *
 * Everything the scheduler does to answer "can this job run here, and when"
 * lives behind this header. The two entry points are
 * sge_sequential_assignment() for a sequential job and
 * sge_select_parallel_environment() for a parallel one; both fill an
 * #sge_assignment_t, which carries the job, all the lists needed to match it,
 * the caches built along the way and finally the result.
 *
 * A match is made in layers - global, host, queue instance - and in two
 * flavours: **static** matching, which only looks at what cannot change
 * during a scheduling run (access lists, queue type, fixed attributes), and
 * **dynamic** matching, which looks at consumables and therefore at time.
 * The static half is what makes the skip lists and the category rejection of
 * `ocs::CategorySchedd` possible: a static mismatch holds for every job of
 * the same category.
 *
 * With resource reservation enabled the question is not only "now" but "when
 * at the earliest", which is why so many functions take a start time and a
 * duration and return a #dispatch_t rather than a bool.
 */

#include <limits>

#ifndef __SGE_H
#   include "uti/sge.h"
#endif

#include "sge_orders.h"
#include "sgeobj/ocs_TopologyString.h"

/**
 * Minimum number of jobs in a category before the caches that are shared per
 * category - the skip host list, the skip queue list and the soft violations
 * - are worth maintaining.
 */
#define MIN_JOBS_IN_CATEGORY 1

bool
sge_select_queue(lList *requested_attr, lListElem *queue, lListElem *host, lList *exechost_list,
                 lList *centry_list, bool allow_non_requestable, int slots, lList *queue_user_list,
                 lList *acl_list, lListElem *job);

/* --- is there a load alarm on this queue ---------------------------- */

int sge_load_alarm(char *reason, size_t reason_size, const lListElem *queue, const lList *threshold,
                   const lList *exechost_list, const lList *complex_list,
                   const lList *load_adjustments, bool is_check_consumable);

void sge_create_load_list(const lList *queue_list, const lList *host_list,
                          const lList *centry_list, lList **load_list);

bool sge_load_list_alarm(bool monitor_next_run, lList *load_list, const lList *host_list,
                         const lList *centry_list);

void sge_remove_queue_from_load_list(lList **load_list, const lList *queue_list);

void sge_free_load_list(lList **load_list);

char *sge_load_alarm_reason(lListElem *queue, lList *threshold, const lList *exechost_list,
                            const lList *complex_list, char  *reason, int reason_size,
                            const char *type);

int sge_split_queue_load(bool monitor_next_run, lList **unloaded, lList **overloaded, lList *exechost_list,
                         const lList *complex_list, const lList *load_adjustments,
                         lList *granted, bool is_consumable_load_alarm, bool is_comprehensive,
                         uint32_t ttype);

int sge_split_queue_slots_free(bool monitor_next_run, lList **unloaded, lList **overloaded);

int sge_split_cal_disabled(bool monitor_next_run, lList **unloaded, lList **overloaded);
int sge_split_disabled(bool monitor_next_run, lList **unloaded, lList **overloaded);

int sge_split_suspended(bool monitor_next_run, lList **queue_list, lList **suspended);


/* --- job assignment methods ---------------------------- */

/**
 * @brief The two special points in time a match can be made for
 */
enum {
   DISPATCH_TIME_NOW = 0,   ///< Match for right now
   DISPATCH_TIME_QUEUE_END = std::numeric_limits<uint64_t>::max()   ///< Match for the time when everything currently booked has ended
};

/**
 * @brief Counters of the scheduler profiling
 *
 * How often each matching layer was entered, split by sequential (`seq_`) and
 * parallel (`par_`) jobs. Raised through #SCHED_PROF_INC, which is a no-op
 * when no profiling structure is attached to the assignment.
 */
typedef struct {
   int par_global;   ///< Parallel job: matches against the global host
   int par_rqs;      ///< Parallel job: matches against the resource quota sets
   int par_cqstat;   ///< Parallel job: static matches against a cluster queue
   int par_hstat;    ///< Parallel job: static matches against a host
   int par_qstat;    ///< Parallel job: static matches against a queue instance
   int par_hdyn;     ///< Parallel job: dynamic matches against a host
   int par_qdyn;     ///< Parallel job: dynamic matches against a queue instance

   int seq_global;   ///< Sequential job: matches against the global host
   int seq_rqs;      ///< Sequential job: matches against the resource quota sets
   int seq_cqstat;   ///< Sequential job: static matches against a cluster queue
   int seq_hstat ;   ///< Sequential job: static matches against a host
   int seq_qstat;    ///< Sequential job: static matches against a queue instance
   int seq_hdyn;     ///< Sequential job: dynamic matches against a host
   int seq_qdyn;     ///< Sequential job: dynamic matches against a queue instance
} sched_prof_t;

/** Raises one counter of #sched_prof_t, doing nothing when profiling is off */
#define SCHED_PROF_INC(pi, attrib) if (pi != nullptr) pi->attrib++

/**
 * @brief Everything one matching run needs and everything it produces
 *
 * Passed through the whole matching machinery as `a`. It has four parts, in
 * this order: what determines the assignment (the job and all the lists),
 * caches of intermediate results, the parallel job settings, and the
 * resulting assignment itself.
 *
 * Initialize with #SGE_ASSIGNMENT_INIT or assignment_init(), release with
 * assignment_release().
 */
typedef struct {
   // -------------------- this section determines the assignment --------------------
   uint32_t    job_id;          ///< job id (convenience reasons)
   uint32_t    ja_task_id;      ///< job array task id (convenience reasons)
   uint32_t    ar_id;           ///< ar id if the job requested to run in an AR, else 0
   lListElem  *job;             ///< the job (JB_Type)
   lListElem  *ja_task;         ///< the task (JAT_Type); nullptr means only the
                                ///< reschedule unknown verification is missing
   lListElem *ar;               ///< the advance reservation if requested (AR_Type)
   const char* user;            ///< username (JB_owner)
   const char* group;           ///< group name (JB_group)
   const lList *grp_list;       ///< supplementary grp list (JB_grp_list)
   const char* project;         ///< project name (JB_project)
   const lListElem *ckpt;       ///< the checkpoint interface (CK_Type)
   lListElem *gep;              ///< the global host (EH_Type)
   uint64_t duration;           ///< jobs time of the assignment
   lList *load_adjustments;     ///< job load adjustments (CE_Type)
   lList *host_list;            ///< the hosts (EH_Type)
   lList *queue_list;           ///< the queues (QU_Type)
   const lList *centry_list;    ///< the complex entries (CE_Type)
   const lList *acl_list;       ///< the user sets (US_Type)
   const lList *hgrp_list;      ///< the host group list (HGRP_Type)
   lList      *rqs_list;        ///< the resource quota set list (RQS_Type)
   lList      *ar_list;         ///< the advance reservation list (AR_Type)
   bool       is_reservation;   ///< true, if a reservation for this job should be done
   bool       is_advance_reservation; ///< true for advance reservation scheduling
   bool       is_job_verify;    ///< true, if job verification (-w ev) (in qmaster)
   bool       is_schedule_based; ///< true, if resource reservation is enabled
   bool       is_soft;          ///< true, if job has soft requests
   uint64_t   now;              ///< now time for immediate jobs
   bool is_binding_enabled;     ///< cached value of the corresponding configuration parameter
   bool do_binding_on_any_host; ///< cached value of the corresponding configuration parameter
   // -------------------- this section is for caching of intermediate results --------------------
   lList      *limit_list;      ///< the resource quota limit list (RQL_Type)
   lList      *skip_cqueue_list; ///< cluster queues that need not be checked anymore (CTI_Type)
   lList      *skip_host_list;  ///< hosts that need not be checked anymore (CTI_Type)
   // -------------------- this section are parallel job related settings --------------------
   lListElem  *pe;              ///< the parallel environment (PE_Type)
   const char* pe_name;         ///< name of the PE
   const char *allocation_rule; ///< the allocation rule of the PE or overwritten globally/for slaves
   int allocation_parsed;       ///< the allocation rule of the PE parsed with a->slots as the maximum
   const char *mallocation_rule; ///< the allocation rule for the master task (or nullptr)
   int mallocation_parsed;      ///< the allocation rule for the master task parsed with a->slots as the maximum (or nullptr)
   // -------------------- this section is the resulting assignment --------------------
   lList      *gdil;            ///< the resources (JG_Type)
   int        slots;            ///< total number of slots we do matching against
   uint64_t   start;            ///< jobs start time
   int        soft_violations;  ///< number of soft request violations
   lList      **monitor_alpp;   ///< place scheduler diagnosis here if non-nullptr
   bool       monitor_next_run; ///< controls qconf -tsm scheduler diagnosis
   lList      *binding_to_use;  ///< Core/thread binding information
   bool       filter_first_core; ///< globals binding filter: true if first core of first socket should not be used
   // -------------------- scheduler profiling index as picky pack data -------------------------
   sched_prof_t *pi;            ///< Profiling counters, or nullptr when profiling is off
} sge_assignment_t;

/** Initializer for an empty #sge_assignment_t */
#define SGE_ASSIGNMENT_INIT {0, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, \
   0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false, false, false, false, false, 0,    \
   false, false,                                                                                                       \
   nullptr, nullptr, nullptr,                            /* caching */                                                 \
   nullptr, nullptr, nullptr, 0, nullptr, 0,             /* parallel job related settings */                           \
   nullptr, 0, 0, 0, nullptr, false, nullptr, false,     /* resulting assignment */                                    \
   nullptr                                               /* profiling */                                               \
}

void assignment_init(sge_assignment_t *a, lListElem *job, lListElem *ja_task, lList *load_adjustments);
void assignment_init_ar(sge_assignment_t *a, lList *ar_list);
void assignment_copy(sge_assignment_t *dst, sge_assignment_t *src, bool move_gdil);
void assignment_release(sge_assignment_t *a);
void assignment_clear_cache(sge_assignment_t *a);

/* -------------------------------------------------------------------------------- */

/**
 * @brief The outcome of a matching attempt
 *
 * The two negative values are what makes a scheduling run affordable: they
 * say the mismatch is permanent, so the caller may exclude the whole category
 * or the whole job instead of trying the next object.
 */
typedef enum {
   DISPATCH_NEVER = 4,        ///< An error happened, no dispatch will ever work again
   DISPATCH_MISSING_ATTR = 2, ///< The attribute does not exist
   DISPATCH_NOT_AT_TIME = 1,  ///< No assignment at the specified time
   DISPATCH_OK = 0,           ///< Got an assignment; the time is set for #DISPATCH_TIME_QUEUE_END
   DISPATCH_NEVER_CAT = -1,   ///< Will never be possible for any job of that category
   DISPATCH_NEVER_JOB = -2    ///< Will never be possible for this particular job - it is on a host's reschedule unknown list
} dispatch_t;

/**
 * @name Which task of a parallel job a match is made for
 *
 * The master task of a parallel job can have different requests than the
 * slave tasks, so a queue may be usable for one and not for the other.
 * @{
 */
#define TAG4SCHED_NONE                              ///< @warning Empty body and no user in the source tree - it expands to nothing, so a `TAG4SCHED_NONE` written into an expression would silently vanish rather than contribute a zero bit
#define TAG4SCHED_SLAVE          0b0001             ///< Match for the slave tasks
#define TAG4SCHED_MASTER         0b0010             ///< Match for the master task
#define TAG4SCHED_ALL TAG4SCHED_SLAVE|TAG4SCHED_MASTER   ///< Match for both
/** @} */

dispatch_t
sge_sequential_assignment(sge_assignment_t *a);

dispatch_t
sge_select_parallel_environment(sge_assignment_t *best, const lList *pe_list);

/* -------------------------------------------------------------------------------- */

bool is_requested(const lList *req, const char *attr);
bool is_requested(const lListElem *job, const char *attr);

dispatch_t sge_queue_match_static(const sge_assignment_t *a, lListElem *queue, bool need_master);

dispatch_t
sge_host_match_static(const sge_assignment_t *a, const lListElem *host);

/* ------ DEBUG / output methods --------------------------------------------------- */

/* not used */
/* int sge_get_ulong_qattr(uint32_t *uvalp, char *attrname, lListElem *q, lList *exechost_list, lList *complex_list); */

int sge_get_double_qattr(double *dvalp, const char *attrname, const lListElem *q,
                         const lList *exechost_list, const lList *complex_list,
                         bool *has_value_from_object);

int sge_get_string_qattr(char *dst, int dst_len, const char *attrname, lListElem *q, const lList *exechost_list, const lList *complex_list);

dispatch_t
parallel_rc_slots_by_time(sge_assignment_t *a, int *slots, const lList *total_list,
                          const lList *rue_list, const lList *load_attr, bool force_slots,
                          lListElem *host, lListElem *queue,
                          uint32_t layer, double lc_factor, uint32_t tag, bool need_master,
                          bool is_master_host, bool &found_master_host, bool allow_non_requestable,
                          const char *object_name, bool isRQ);

dispatch_t
ri_time_by_slots(const sge_assignment_t *a, lListElem *request, const lList *load_attr, const lList *config_attr,
                 const lList *actual_attr, const lListElem *host, const lListElem *queue, dstring *reason, bool allow_non_requestable,
                 int slots, uint32_t layer, double lc_factor, uint64_t *start_time, const char *object_name, ocs::TopologyString& binding_inuse);

dispatch_t cqueue_match_static(const char *cqname, sge_assignment_t *a);

void
sge_ar_swap_resource_lists(sge_assignment_t &a);

dispatch_t
parallel_limit_slots_by_time(sge_assignment_t *a, int *slots, lListElem *centry,
                             lListElem *limit, dstring *rue_name, lListElem *qep, bool need_master,
                             bool is_master_queue);
