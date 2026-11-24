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
#include <type_traits>

#ifndef __SGE_H
#   include "uti/sge.h"
#endif

#include "sge_orders.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CTI.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_LDR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RQL.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_answer.h"

/**
 * Minimum number of jobs in a category before the caches that are shared per
 * category - the skip host list, the skip queue list and the soft violations
 * - are worth maintaining.
 */
#define MIN_JOBS_IN_CATEGORY 1

bool
sge_select_queue(ocs::tom::List<ocs::tom::CE> requested_attr, ocs::tom::ConstElement<ocs::tom::QU> queue,
                 ocs::tom::ConstElement<ocs::tom::EH> host, ocs::tom::List<ocs::tom::EH> exechost_list,
                 ocs::tom::ConstList<ocs::tom::CE> centry_list, bool allow_non_requestable, int slots,
                 ocs::tom::ConstList<ocs::tom::ST> queue_user_list, ocs::tom::ConstList<ocs::tom::US> acl_list,
                 ocs::tom::ConstElement<ocs::tom::JB> job);

/* --- is there a load alarm on this queue ---------------------------- */

int sge_load_alarm(char *reason, size_t reason_size, ocs::tom::ConstElement<ocs::tom::QU> queue,
                   ocs::tom::ConstList<ocs::tom::CE> threshold, ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                   ocs::tom::ConstList<ocs::tom::CE> complex_list, ocs::tom::ConstList<ocs::tom::CE> load_adjustments,
                   ocs::tom::ConstList<ocs::tom::CE> consumable_load_adjustments, bool is_check_consumable);

ocs::tom::ListPtr<ocs::tom::LDR>
sge_create_load_list(ocs::tom::List<ocs::tom::QU> queue_list, ocs::tom::ConstList<ocs::tom::EH> host_list,
                     ocs::tom::ConstList<ocs::tom::CE> centry_list);

bool sge_load_list_alarm(bool monitor_next_run, ocs::tom::ConstList<ocs::tom::LDR> load_list,
                         ocs::tom::ConstList<ocs::tom::EH> host_list, ocs::tom::ConstList<ocs::tom::CE> centry_list);

void sge_remove_queue_from_load_list(ocs::tom::ListPtr<ocs::tom::LDR> &load_list,
                                     ocs::tom::ConstList<ocs::tom::QU> queue_list);

char *sge_load_alarm_reason(ocs::tom::Element<ocs::tom::QU> queue, ocs::tom::ConstList<ocs::tom::CE> threshold,
                            ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                            ocs::tom::ConstList<ocs::tom::CE> complex_list,
                            ocs::tom::ConstList<ocs::tom::CE> load_adjustments, char *reason, int reason_size,
                            const char *type);

int sge_split_queue_load(bool monitor_next_run, ocs::tom::List<ocs::tom::QU> unloaded,
                         ocs::tom::ListPtr<ocs::tom::QU> *overloaded, ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         ocs::tom::ConstList<ocs::tom::CE> load_adjustments, ocs::tom::ConstList<ocs::tom::JG> granted,
                         bool is_consumable_load_alarm, bool is_comprehensive, uint32_t ttype);

int sge_split_queue_slots_free(bool monitor_next_run, ocs::tom::List<ocs::tom::QU> free,
                               ocs::tom::ListPtr<ocs::tom::QU> &full);

int sge_split_cal_disabled(bool monitor_next_run, ocs::tom::ListPtr<ocs::tom::QU> &queue_list,
                           ocs::tom::ListPtr<ocs::tom::QU> &disabled);
int sge_split_disabled(bool monitor_next_run, ocs::tom::ListPtr<ocs::tom::QU> &queue_list,
                       ocs::tom::ListPtr<ocs::tom::QU> &disabled);

int sge_split_suspended(bool monitor_next_run, ocs::tom::ListPtr<ocs::tom::QU> &queue_list,
                        ocs::tom::ListPtr<ocs::tom::QU> &suspended);


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
 * @brief The trivially copyable part of #sge_assignment_t
 *
 * Everything of the assignment except the lists it owns: the values, the
 * caches of plain values and the non-owning handles. assignment_copy()
 * copies this part by assignment.
 */
struct sge_assignment_base_t {
   // -------------------- this section determines the assignment --------------------
   uint32_t    job_id;          ///< job id (convenience reasons)
   uint32_t    ja_task_id;      ///< job array task id (convenience reasons)
   uint32_t    ar_id;           ///< ar id if the job requested to run in an AR, else 0
   ocs::tom::Element<ocs::tom::JB> job;      ///< the job
   ocs::tom::Element<ocs::tom::JAT> ja_task; ///< the task; empty means only the
                                             ///< reschedule unknown verification is missing
   ocs::tom::Element<ocs::tom::AR> ar;       ///< the advance reservation if requested
   const char* user;            ///< username (JB_owner)
   const char* group;           ///< group name (JB_group)
   ocs::tom::ConstList<ocs::tom::ST> grp_list; ///< supplementary grp list (JB_grp_list)
   const char* project;         ///< project name (JB_project)
   ocs::tom::ConstElement<ocs::tom::CK> ckpt; ///< the checkpoint interface
   ocs::tom::Element<ocs::tom::EH> gep;      ///< the global host
   uint64_t duration;           ///< jobs time of the assignment
   ocs::tom::List<ocs::tom::CE> load_adjustments;  ///< job load adjustments
   ocs::tom::List<ocs::tom::EH> host_list;         ///< the hosts
   ocs::tom::List<ocs::tom::QU> queue_list;        ///< the queues
   ocs::tom::ConstList<ocs::tom::CE> centry_list;  ///< the complex entries, read only; in qmaster the master list
   ocs::tom::ConstList<ocs::tom::US> acl_list;     ///< the user sets
   ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list;  ///< the host group list
   ocs::tom::List<ocs::tom::RQS> rqs_list;         ///< the resource quota set list
   ocs::tom::List<ocs::tom::AR> ar_list;           ///< the advance reservation list
   bool       is_reservation;   ///< true, if a reservation for this job should be done
   bool       is_advance_reservation; ///< true for advance reservation scheduling
   bool       is_job_verify;    ///< true, if job verification (-w ev) (in qmaster)
   bool       is_schedule_based; ///< true, if resource reservation is enabled
   bool       is_soft;          ///< true, if job has soft requests
   uint64_t   now;              ///< now time for immediate jobs
   bool is_binding_enabled;     ///< cached value of the corresponding configuration parameter
   bool do_binding_on_any_host; ///< cached value of the corresponding configuration parameter
   // -------------------- this section are parallel job related settings --------------------
   ocs::tom::Element<ocs::tom::PE> pe;       ///< the parallel environment
   const char* pe_name;         ///< name of the PE
   const char *allocation_rule; ///< the allocation rule of the PE or overwritten globally/for slaves
   int allocation_parsed;       ///< the allocation rule of the PE parsed with a->slots as the maximum
   const char *mallocation_rule; ///< the allocation rule for the master task (or nullptr)
   int mallocation_parsed;      ///< the allocation rule for the master task parsed with a->slots as the maximum (or nullptr)
   // -------------------- this section is the resulting assignment --------------------
   int        slots;            ///< total number of slots we do matching against
   uint64_t   start;            ///< jobs start time
   int        soft_violations;  ///< number of soft request violations
   ocs::tom::ListPtr<ocs::tom::AN> *monitor_alpp;   ///< place scheduler diagnosis here if non-nullptr
   bool       monitor_next_run; ///< controls qconf -tsm scheduler diagnosis
   bool       filter_first_core; ///< globals binding filter: true if first core of first socket should not be used
   // -------------------- scheduler profiling index as picky pack data -------------------------
   sched_prof_t *pi;            ///< Profiling counters, or nullptr when profiling is off
};

// assignment_copy() copies the base part by assignment
static_assert(std::is_trivially_copyable_v<sge_assignment_base_t>,
              "sge_assignment_base_t must stay trivially copyable");

/**
 * @brief Everything one matching run needs and everything it produces
 *
 * Passed through the whole matching machinery as `a`. It has four parts: what
 * determines the assignment (the job and all the lists), caches of
 * intermediate results, the parallel job settings, and the resulting
 * assignment itself. The values and the non-owning handles are in the base
 * #sge_assignment_base_t, the lists the assignment owns are here.
 *
 * Initialize with `sge_assignment_t a{};` and assignment_init(), release with
 * assignment_release().
 */
struct sge_assignment_t : sge_assignment_base_t {
   // -------------------- this section is for caching of intermediate results --------------------
   ocs::tom::ListPtr<ocs::tom::RQL> limit_list;       ///< the resource quota limit list
   ocs::tom::ListPtr<ocs::tom::CTI> skip_cqueue_list; ///< cluster queues that need not be checked anymore
   ocs::tom::ListPtr<ocs::tom::CTI> skip_host_list;   ///< hosts that need not be checked anymore
   ocs::tom::ListPtr<ocs::tom::VA> rsmap_job_keys;    ///< per resource map, the instance group a scope=job request
                                                      ///< binds the whole job to (VA_variable the map name,
                                                      ///< VA_value the group key), see rsmap_job_scope_key()
   ocs::tom::ElementPtr<ocs::tom::CE> default_request; ///< scratch request a default request is evaluated in,
                                                      ///< created on first use; the complex entries stay read only
   ocs::tom::ElementPtr<ocs::tom::CE> limit_offer;     ///< scratch offer a non consumable resource quota limit is
                                                      ///< compared in, created on first use
   ocs::tom::ElementPtr<ocs::tom::CE> implicit_slots_request; ///< the implicit request slots=1 every job makes,
                                                      ///< created on first use, see
                                                      ///< ocs_assignment_implicit_slots_request()
   ocs::tom::ListPtr<ocs::tom::CE> implicit_total_list; ///< scratch capacity list holding the slots of the parallel
                                                      ///< environment, created on first use
   // -------------------- this section is the resulting assignment --------------------
   ocs::tom::ListPtr<ocs::tom::JG> gdil;              ///< the resources
   ocs::tom::ListPtr<ocs::tom::BN> binding_to_use;    ///< Core/thread binding information
   ocs::tom::ListPtr<ocs::tom::GRU> granted_rsmaps;   ///< resource map ids granted, keyed by GRU_name and GRU_host
};

void assignment_init(sge_assignment_t *a, ocs::tom::Element<ocs::tom::JB> job, ocs::tom::Element<ocs::tom::JAT> ja_task,
                     ocs::tom::List<ocs::tom::CE> load_adjustments);
void assignment_init_ar(sge_assignment_t *a, ocs::tom::List<ocs::tom::AR> ar_list);
void assignment_copy(sge_assignment_t *dst, sge_assignment_t *src, bool move_gdil);
void assignment_release(sge_assignment_t *a);
void assignment_clear_cache(sge_assignment_t *a);

ocs::tom::ConstElement<ocs::tom::CE> ocs_assignment_implicit_slots_request(sge_assignment_t *a);

bool rsmap_request_has_job_scope(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::CE> req);

const char *rsmap_job_scope_key(sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::CE> req, const char *same_key,
                                const char *id_expr, ocs::tom::ConstList<ocs::tom::CE> required_props);

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
sge_select_parallel_environment(sge_assignment_t *best, ocs::tom::List<ocs::tom::PE> pe_list);

/* -------------------------------------------------------------------------------- */

bool is_requested(ocs::tom::ConstList<ocs::tom::CE> req, const char *attr);
bool is_requested(ocs::tom::ConstElement<ocs::tom::JB> job, const char *attr);

dispatch_t sge_queue_match_static(const sge_assignment_t *a, ocs::tom::Element<ocs::tom::QU> queue, bool need_master);

dispatch_t
sge_host_match_static(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::EH> host);

/* ------ DEBUG / output methods --------------------------------------------------- */

/* not used */
/* int sge_get_ulong_qattr(uint32_t *uvalp, char *attrname, ocs::tom::ElementData *q, ocs::tom::ListData *exechost_list,
 * ocs::tom::ListData *complex_list); */

int sge_get_double_qattr(double *dvalp, const char *attrname, ocs::tom::ConstElement<ocs::tom::QU> q,
                         ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         ocs::tom::ConstList<ocs::tom::CE> load_adjustments, bool *has_value_from_object);

int sge_get_string_qattr(char *dst, int dst_len, const char *attrname, ocs::tom::ConstElement<ocs::tom::QU> q,
                         ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         ocs::tom::ConstList<ocs::tom::CE> load_adjustments);

dispatch_t
parallel_rc_slots_by_time(sge_assignment_t *a, int *slots, ocs::tom::ConstList<ocs::tom::CE> total_list,
                          ocs::tom::ConstList<ocs::tom::RUE> rue_list, ocs::tom::ConstList<ocs::tom::HL> load_attr,
                          bool force_slots, ocs::tom::ConstElement<ocs::tom::EH> host,
                          ocs::tom::Element<ocs::tom::QU> queue, uint32_t layer, double lc_factor, uint32_t tag,
                          bool need_master, bool is_master_host, bool &found_master_host, bool allow_non_requestable,
                          const char *object_name, bool isRQ);

dispatch_t
ri_time_by_slots(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::CE> request,
                 ocs::tom::ConstList<ocs::tom::HL> load_attr, ocs::tom::ConstList<ocs::tom::CE> config_attr,
                 ocs::tom::ConstList<ocs::tom::RUE> actual_attr, ocs::tom::ConstElement<ocs::tom::EH> host,
                 ocs::tom::ConstElement<ocs::tom::QU> queue, dstring *reason, bool allow_non_requestable, int slots,
                 uint32_t layer, double lc_factor, uint64_t *start_time, const char *object_name,
                 ocs::TopologyString& binding_inuse);

dispatch_t cqueue_match_static(const char *cqname, sge_assignment_t *a);

void
sge_ar_swap_resource_lists(sge_assignment_t &a);

dispatch_t
parallel_limit_slots_by_time(sge_assignment_t *a, int *slots, ocs::tom::ConstElement<ocs::tom::CE> centry,
                             ocs::tom::ConstElement<ocs::tom::RQRL> limit, dstring *rue_name,
                             ocs::tom::Element<ocs::tom::QU> qep, bool need_master, bool is_master_queue);
