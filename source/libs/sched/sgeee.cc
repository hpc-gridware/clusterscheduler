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
 * @brief SGEEE - the ticket policies that turn shares into a job priority
 *
 * Three policies hand out **tickets** to a job, and the sum decides the order
 * in which pending jobs are considered:
 *
 * - the **share tree** policy, which distributes tickets along a tree of
 *   users and projects according to configured shares and accumulated usage,
 * - the **functional** policy, which distributes them by fixed weights per
 *   user, project, department and job, and
 * - the **override** policy, which is what an administrator adds by hand.
 *
 * The computation runs over an array of #sge_ref_t, one per job or array
 * task, each holding references to everything the policies need - the job,
 * the task, the user, the project, the department and the share tree node.
 * Tasks that are not enrolled in `JB_ja_tasks` have no cull element to write
 * to, which is what #sge_task_ref_t and the `REF_*` accessor macros exist
 * for.
 */

/** Include queued jobs in the ticket calculation, not only the running ones */
#define SGE_INCLUDE_QUEUED_JOBS

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "uti/sge_language.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"
#include "cull/ocs_cull_descr.h"

#include "sgeobj/ocs_ShareTree.h"
#include "sgeobj/ocs_Usage.h"
#include "sgeobj/ocs_UserProject.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_userprj.h"

#include "sge_job_schedd.h"
#include "sge_urgency.h"
#include "sgeee.h"
#include "sgeobj/cull/sge_eejob_FCAT_L.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_FCAT.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/*
 * Following fields are necessary for tasks which are not
 * enrolled in the JB_ja_tasks-list. For these jobs we have no
 * reference to the 'ja_task'-CULL-element.
 */
/**
 * @brief The ticket values of an array task that is not enrolled
 *
 * A task that has no element in `JB_ja_tasks` has no cull element the
 * policies could write into, so its values are kept here instead. The `REF_*`
 * macros hide the difference: they write to the task element when there is
 * one and into this struct otherwise.
 */
typedef struct {
   uint32_t job_number;       ///< Job number
   uint32_t ja_task_number;   ///< Array task id
   double ja_task_fticket;    ///< Functional tickets of the task
   double ja_task_sticket;    ///< Share tree tickets of the task
   double ja_task_oticket;    ///< Override tickets of the task
   double ja_task_ticket;     ///< Total tickets of the task
   double ja_task_share;      ///< Share of the task
   uint32_t ja_task_fshare;   ///< Functional share of the task
} sge_task_ref_t;

/*
 * sge_ref_t - this structure is used in the SGEEE
 * scheduler to keep references to all the objects
 * associated with a job.  An array is built containing
 * one of these structures for each job/array we
 * are scheduling.
 */

/**
 * @brief Everything the ticket policies need for one job or array task
 *
 * The scheduler builds an array of these, one entry per job or array task it
 * is scheduling, so that each policy can walk the array instead of resolving
 * the user, project, department and share tree node again for every job.
 * The object members are non-owning handles on elements of the scheduler's
 * lists; only a temporary share tree node made from a "default" node is
 * created here, and the share tree it was appended to owns it.
 */
struct sge_ref_t {
   ocs::tom::Element<ocs::tom::JB> job;       ///< The job
   ocs::tom::Element<ocs::tom::JAT> ja_task;  ///< The array task, empty if not enrolled
   ocs::tom::Element<ocs::tom::UU> user;      ///< The job owner
   ocs::tom::Element<ocs::tom::PR> project;   ///< The project of the job
   ocs::tom::Element<ocs::tom::US> dept;      ///< The department of the job owner
   ocs::tom::Element<ocs::tom::STN> node;     ///< The share tree node the job belongs to
   int queued;                ///< 1 if this is a queued job
   uint32_t  share_tree_type; ///< Which share tree the node comes from
   double user_fshare;        ///< The job's share of the user functional shares
   double dept_fshare;        ///< The job's share of the department functional shares
   double project_fshare;     ///< The job's share of the project functional shares
   double job_fshare;         ///< The job's share of the job functional shares
   double tickets;            ///< The job's pending tickets from the hierarchical policies
   sge_task_ref_t *tref;      ///< Ticket values of a task that is not enrolled, see #sge_task_ref_t
};

/*
 * this allows to build a list from
 * the the ref structure. This is need
 * to build the functional categories
 */
/**
 * @brief Doubly linked list of #sge_ref_t, used to build the functional categories
 */
typedef struct sge_ref_list_t{
   sge_ref_t *ref;                     ///< The entry this list item refers to
   struct sge_ref_list_t *next;        ///< Next list item
   struct sge_ref_list_t *prev;        ///< Previous list item
} sge_ref_list_t;

static void tix_range_set(double min_tix, double max_tix);
static void tix_range_get(double *min_tix, double *max_tix);

/* EB: debug */
#if 0
#define DEBUG_TASK_REF
#endif

#ifdef DEBUG_TASK_REF
static void task_ref_print_table();
static void task_ref_print_table_entry(sge_task_ref_t *tref);
#endif

static void sge_clear_ja_task(ocs::tom::Element<ocs::tom::JAT> ja_task);

static sge_task_ref_t *task_ref_get_first(uint32_t job_number, uint32_t ja_task_number);
static sge_task_ref_t *task_ref_get_first_job_entry();
static sge_task_ref_t *task_ref_get_next_job_entry();
static void task_ref_copy_to_ja_task(sge_task_ref_t *tref, ocs::tom::Element<ocs::tom::JAT> ja_task);

static void sge_do_sgeee_priority(ocs::tom::List<ocs::tom::JB> job_list, double min_tix, double max_tix,
                                  bool do_nprio, bool do_nurg);
static void sgeee_priority(ocs::tom::Element<ocs::tom::JAT> task, uint32_t jobid, double nsu, double npri,
                           double min_tix, double max_tix);
static void recompute_prio(sge_task_ref_t *tref, ocs::tom::Element<ocs::tom::JAT> task, double nurg, double npri);


static void calculate_pending_shared_override_tickets(sge_ref_t *job_ref, uint32_t num_jobs, int dependent);

static double calc_job_tickets(sge_ref_t *ref);

static int sge_calc_tickets(scheduler_all_data_t *lists,
                            ocs::tom::List<ocs::tom::JB> running_jobs,
                            ocs::tom::List<ocs::tom::JB> finished_jobs,
                            ocs::tom::List<ocs::tom::JB> queued_jobs,
                            int do_usage,
                            double *max_tickets);

static ocs::tom::ListPtr<ocs::tom::STN> sge_sort_pending_job_nodes(ocs::tom::Element<ocs::tom::STN> root,
                                                                   ocs::tom::Element<ocs::tom::STN> node,
                                                                   double total_share_tree_tickets);
static int sge_calc_sharetree_targets(ocs::tom::Element<ocs::tom::STN> root, scheduler_all_data_t *lists,
                                      ocs::tom::ConstList<ocs::tom::UA> decay_list, uint64_t curr_time,
                                      u_long seqno);
static int sge_init_share_tree_node_fields( lListElem *node, void *ptr );

static double calc_pjob_override_tickets_shared( sge_ref_t *ref);

static void free_fcategories(ocs::tom::ListPtr<ocs::tom::FCAT> &fcategories, sge_ref_list_t **ref_array);
static uint32_t build_functional_categories(sge_ref_t *job_ref, uint32_t num_jobs,
                                            ocs::tom::ListPtr<ocs::tom::FCAT> &fcategories,
                                            sge_ref_list_t **ref_array, int dependent,
                                            JB_Field job_tickets, UU_Field user_tickets, PR_Field project_tickets,
                                            US_Field dp_tickets);
static void copy_ftickets(sge_ref_list_t *source, sge_ref_list_t *dest);
static void destribute_ftickets(ocs::tom::ConstList<ocs::tom::FCAT> fcategories, int dependent);

static void calc_intern_pending_job_functional_tickets(
                                    ocs::tom::ConstElement<ocs::tom::FCAT> current,
                                    double sum_of_user_functional_shares,
                                    double sum_of_project_functional_shares,
                                    double sum_of_department_functional_shares,
                                    double sum_of_job_functional_shares,
                                    double total_functional_tickets,
                                    double weight[],
                                    bool share_functional_shares);


static void task_ref_initialize_table(uint32_t number_of_tasks);
static void task_ref_destroy_table();
static sge_task_ref_t *task_ref_get_entry(uint32_t index);

static scheduler_all_data_t *all_lists; /* thread local */
static uint32_t sge_scheduling_run = 0; /* thread local */

static sge_task_ref_t *task_ref_table = nullptr; /* thread local */
static uint32_t task_ref_entries = 0;   /* thread local */
static uint32_t task_ref_job_pos = 0;   /* thread local */
static double Master_min_tix = 0.0;     /* thread local */
static double Master_max_tix = 0.0;     /* thread local */
static int last_seqno = 0;              /* stores the last used seqno for the orders  thread_local*/
static uint64_t past = 0;               /* stores the last re-order send time thread local */

/**
 * @brief Store ticket range
 *
 * Stores ticket range in the global variables.
 *
 * @param min_tix Minimum ticket value.
 * @param max_tix Maximum ticket value.
 *
 * @note MT-NOTES: tix_range_set() is not MT safe
 */
static void tix_range_set(double min_tix, double max_tix) {
   Master_min_tix = min_tix;
   Master_max_tix = max_tix;
}

/**
 * @brief Get stored ticket range
 *
 * Get stored ticket range from global variables.
 *
 * @param min_tix Target for minimum value.
 * @param max_tix Target for maximum value.
 *
 * @note MT-NOTES: tix_range_get() is not MT safe
 */
static void tix_range_get(double *min_tix, double *max_tix) {
   if (min_tix)
      *min_tix = Master_min_tix;
   if (max_tix)
      *max_tix = Master_max_tix;
}

static void task_ref_initialize_table(uint32_t number_of_tasks) {
   const size_t size = number_of_tasks * sizeof(sge_task_ref_t);

   if (task_ref_entries != number_of_tasks && number_of_tasks > 0) {
      task_ref_destroy_table();
      task_ref_table = (sge_task_ref_t *) sge_malloc(size);
      task_ref_entries = number_of_tasks;
   }
   if (task_ref_table != nullptr) {
      memset(task_ref_table, 0, size);
   }
}

static void task_ref_destroy_table() {
   if (task_ref_table != nullptr) {
      sge_free(&task_ref_table);
      task_ref_entries = 0;
   }
}

static sge_task_ref_t *task_ref_get_entry(uint32_t index) {
   DENTER(BASIS_LAYER);

   sge_task_ref_t *ret = nullptr;

   if (index < task_ref_entries) {
      ret = &task_ref_table[index];
   }
   DRETURN(ret);
}

#ifdef DEBUG_TASK_REF
static void task_ref_print_table() {
   DENTER(BASIS_LAYER);

   uint32_t i;

   for (i = 0; i < task_ref_entries; i++) {
      sge_task_ref_t *tref = task_ref_get_entry(i);

      if (tref == nullptr) {
         break;
      }

      task_ref_print_table_entry(tref);
   }
   DRETURN_VOID;
}

static void task_ref_print_table_entry(sge_task_ref_t *tref) {
   DENTER(TOP_LAYER);

   if (tref != nullptr) {
      DPRINTF("    @@@ "
         "job: " sge_u32" "
         "ja_task: " sge_u32" "
         "t: %f "
         "st: %f "
         "s: %f "
         "\n",
         tref->job_number,
         tref->ja_task_number,
         tref->ja_task_ticket,
         tref->ja_task_sticket,
         tref->ja_task_share
      );
   }
   DRETURN_VOID;
}
#endif

static sge_task_ref_t *task_ref_get_first(uint32_t job_number, uint32_t ja_task_number) {
   DENTER(TOP_LAYER);

   sge_task_ref_t *ret = nullptr;
   uint32_t i;

   for (i = 0; i < task_ref_entries; i++) {
      sge_task_ref_t *tref = task_ref_get_entry(i);

      if (tref != nullptr && tref->job_number == job_number &&
          tref->ja_task_number == ja_task_number) {
         ret = tref;
         break;
      }
   }

   DRETURN(ret);
}

static sge_task_ref_t *task_ref_get_first_job_entry() {
   DENTER(BASIS_LAYER);

   sge_task_ref_t *ret = nullptr;

   task_ref_job_pos = 0;
   if (task_ref_job_pos < task_ref_entries) {
      ret = task_ref_get_entry(task_ref_job_pos);
   }
   DRETURN(ret);
}

static sge_task_ref_t *task_ref_get_next_job_entry() {
   DENTER(BASIS_LAYER);

   sge_task_ref_t *current_entry = task_ref_get_entry(task_ref_job_pos);
   sge_task_ref_t *ret = nullptr;
   uint32_t pos = task_ref_job_pos;

   if (current_entry != nullptr) {
      uint32_t current_job_number = current_entry->job_number;

      while (++pos < task_ref_entries) {
         sge_task_ref_t *next_entry = task_ref_get_entry(pos);

         if (next_entry != nullptr &&
             current_job_number != next_entry->job_number) {
            task_ref_job_pos = pos;
            ret = next_entry;
            break;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Copy the ticket values of a task reference into an array task element
 *
 * @param tref the task reference, nothing is copied if nullptr
 * @param ja_task the array task that receives the values, nothing is copied if empty
 */
static void
task_ref_copy_to_ja_task(sge_task_ref_t *tref, const ocs::tom::Element<ocs::tom::JAT> ja_task) {
   DENTER(BASIS_LAYER);

   if (ja_task != nullptr && tref != nullptr) {
      ocs::tom::set<JAT_task_number>(ja_task, tref->ja_task_number);

      ocs::tom::set<JAT_tix>(ja_task,                       tref->ja_task_ticket);
      ocs::tom::set<JAT_fticket>(ja_task,                   tref->ja_task_fticket);
      ocs::tom::set<JAT_sticket>(ja_task,                   tref->ja_task_sticket);
      ocs::tom::set<JAT_oticket>(ja_task,                   tref->ja_task_oticket);
      ocs::tom::set<JAT_share>(ja_task,                     tref->ja_task_share);
      ocs::tom::set<JAT_fshare>(ja_task,                     tref->ja_task_fshare);

   }
   DRETURN_VOID;
}


/** Usage assumed for a job that has reported none yet, so it never gets an unlimited share */
#define SGE_MIN_USAGE 1.0

/**
 * @name Accessors for the ticket values of a job
 *
 * Every one of these writes to the array task element when the task is
 * enrolled, and into the #sge_task_ref_t of the entry otherwise - which is
 * the whole point: the policies do not have to know which of the two cases
 * they are in.
 * @{
 */

/**
 * @brief Read one ticket value of a job, from the task element or from the task reference
 *
 * The value in the task reference is named by a member pointer and read only
 * when no array task is enrolled: then the job's task reference exists, and
 * otherwise it may be nullptr, as the macro this replaces relied on.
 *
 * @tparam F the field of the array task (`JAT_Type`)
 * @tparam T the type of the value in the task reference
 * @param ref the job's entry
 * @param ref_attr the member of the task reference that holds the value when no array task is enrolled
 * @return the value
 */
template <auto F, typename T>
static inline T ocs_ref_get(const sge_ref_t *ref, T sge_task_ref_t::*ref_attr) {
   return ref->ja_task != nullptr ? static_cast<T>(ocs::tom::get<F>(ref->ja_task)) : ref->tref->*ref_attr;
}

/**
 * @brief Set one ticket value of a job, on the task element or in the task reference
 *
 * The task reference is touched only when no array task is enrolled, see
 * #ocs_ref_get.
 *
 * @tparam F the field of the array task (`JAT_Type`)
 * @tparam T the type of the value in the task reference
 * @param ref the job's entry
 * @param ref_attr the member of the task reference that holds the value when no array task is enrolled
 * @param value the new value
 */
template <auto F, typename T>
static inline void ocs_ref_set(const sge_ref_t *ref, T sge_task_ref_t::*ref_attr, const T value) {
   if (ref->ja_task != nullptr) {
      ocs::tom::set<F>(ref->ja_task, value);
   } else {
      ref->tref->*ref_attr = value;
   }
}

/** Array task id of the job */
#define REF_GET_JA_TASK_NUMBER(ref) \
   ocs_ref_get<JAT_task_number>((ref), &sge_task_ref_t::ja_task_number)

/** Sets the array task id of the job */
#define REF_SET_JA_TASK_NUMBER(ref, ja_task_id) \
   ocs_ref_set<JAT_task_number>((ref), &sge_task_ref_t::ja_task_number, static_cast<uint32_t>(ja_task_id))

/** Functional tickets of the job */
#define REF_GET_FTICKET(ref) \
   ocs_ref_get<JAT_fticket>((ref), &sge_task_ref_t::ja_task_fticket)

/** Share tree tickets of the job */
#define REF_GET_STICKET(ref) \
   ocs_ref_get<JAT_sticket>((ref), &sge_task_ref_t::ja_task_sticket)

/** Override tickets of the job */
#define REF_GET_OTICKET(ref) \
   ocs_ref_get<JAT_oticket>((ref), &sge_task_ref_t::ja_task_oticket)

/** Total tickets of the job */
#define REF_GET_TICKET(ref) \
   ocs_ref_get<JAT_tix>((ref), &sge_task_ref_t::ja_task_ticket)

/** Share of the job */
#define REF_GET_SHARE(ref) \
   ocs_ref_get<JAT_share>((ref), &sge_task_ref_t::ja_task_share)

/** Functional share of the job */
#define REF_GET_FSHARE(ref) \
   ocs_ref_get<JAT_fshare>((ref), &sge_task_ref_t::ja_task_fshare)


/** Sets the functional tickets of the job */
#define REF_SET_FTICKET(ref, ticket) \
   ocs_ref_set<JAT_fticket>((ref), &sge_task_ref_t::ja_task_fticket, static_cast<double>(ticket))

/** Sets the share tree tickets of the job */
#define REF_SET_STICKET(ref, ticket) \
   ocs_ref_set<JAT_sticket>((ref), &sge_task_ref_t::ja_task_sticket, static_cast<double>(ticket))

/** Sets the override tickets of the job */
#define REF_SET_OTICKET(ref, ticket) \
   ocs_ref_set<JAT_oticket>((ref), &sge_task_ref_t::ja_task_oticket, static_cast<double>(ticket))

/** Sets the total tickets of the job */
#define REF_SET_TICKET(ref, ticket) \
   ocs_ref_set<JAT_tix>((ref), &sge_task_ref_t::ja_task_ticket, static_cast<double>(ticket))

/** Sets the share of the job */
#define REF_SET_SHARE(ref, share) \
   ocs_ref_set<JAT_share>((ref), &sge_task_ref_t::ja_task_share, static_cast<double>(share))

/** Sets the functional share of the job */
#define REF_SET_FSHARE(ref, fshare) \
   ocs_ref_set<JAT_fshare>((ref), &sge_task_ref_t::ja_task_fshare, static_cast<uint32_t>(fshare))
/** @} */



/**
 * @brief Resort pending jobs after assignment
 *
 * Update pending jobs order upon assignement and change ticket amounts
 * in orders previously created.
 * If we dispatch a job sub-task and the job has more sub-tasks, then
 * the job is still first in the job list.
 * We need to remove and reinsert the job back into the sorted job
 * list in case another job is higher priority (i.e. has more tickets)
 * Additionally it is neccessary to update the number of pending tickets
 * for the following pending array task. (The next task will get less
 * tickets than the current one)
 *
 * @param job_list The pending job list. The first job in the list was assigned right before.
 */
void
sgeee_resort_pending_jobs(const ocs::tom::List<ocs::tom::JB> job_list) {
   DENTER(TOP_LAYER);
   const auto next_job = job_list.front();

   if (next_job) {
      uint32_t job_id = ocs::tom::get<JB_job_number>(next_job);
      uint64_t start_time_of_job1 = ocs::tom::get<JB_submission_time>(next_job);
      ocs::tom::ConstElement<ocs::tom::JAT> tmp_task = ocs::tom::get<JB_ja_tasks>(next_job).front();
      ocs::tom::Element<ocs::tom::JB> insert_jep = nullptr;
      double prio = 0.0;

      if (tmp_task == nullptr) {
         double nurg = 0.5;
         double npri = 0.5;

         ocs::tom::ConstList<ocs::tom::RN> range_list = ocs::tom::get<JB_ja_n_h_ids>(next_job);
         uint32_t ja_task_id = range_list_get_first_id(range_list, nullptr);
         sge_task_ref_t *tref = task_ref_get_first(job_id, ja_task_id);
         const auto ja_task_template = ocs::tom::get<JB_ja_template>(next_job).front();

         /*
          * If no further sge_task_ref_t element was prepared during
          * ticket computation phase we can safe the effort of both
          * resorting the job list and changing ticket amount kept in
          * the order list. This is because the best approximation we
          * could use instead is the ticket number of the previous
          * task that was assigned from the very same array task.
          * But as a matter of course this would not change the position
          * within the job list or the ticket amount kept in the order
          * list. The only way for actually improving the behaviour is to
          * increase the 'max_pending_tasks_per_job' in sched_conf(5).
          */
         if (tref) {
            bool report_priority = sconf_get_report_pjob_tickets();
            bool do_nurg = (sconf_get_weight_urgency()  != 0 || report_priority) ? true : false;
            bool do_npri = (sconf_get_weight_priority() != 0 || report_priority) ? true : false;

            tmp_task = ja_task_template;

            /*
             * job's normalized urgency and priority did not change,
             * but it's needed for updating prio
             */

            if (do_nurg) {
               nurg = ocs::tom::get<JB_nurg>(next_job);
            }
            if (do_npri) {
               npri = ocs::tom::get<JB_nppri>(next_job);
            }

            /*
             * Update pending tickets in template element
             */
            DPRINTF("task_ref_copy_to_ja_task(tref = " sge_u32 ", template_task = " sge_u32 ")\n",
                    tref->ja_task_number, ocs::tom::get<JAT_task_number>(ja_task_template));
            task_ref_copy_to_ja_task(tref, ja_task_template);
            recompute_prio(tref, ja_task_template, nurg, npri);
         }
      }

      /*
       * Re-Insert job at the correct possition
       */
      if (tmp_task) {
         auto dechained_job = job_list.extract(next_job);
         prio = ocs::tom::get<JAT_prio>(tmp_task);
         for (const auto jep : job_list) {
            uint64_t start_time_of_job2 = ocs::tom::get<JB_submission_time>(jep);
            uint32_t job_id2 = ocs::tom::get<JB_job_number>(jep);
            ocs::tom::ConstElement<ocs::tom::JAT> tmp_task2 = ocs::tom::get<JB_ja_tasks>(jep).front();
            double prio2;

            if (tmp_task2 == nullptr) {
               tmp_task2 = ocs::tom::get<JB_ja_template>(jep).front();
            }
            prio2 = ocs::tom::get<JAT_prio>(tmp_task2);
	    /*
	     * In case of equal priority we select the most senior job
	     * in the system. This guy is marked either by smaller job
	     * ID (the regular case) or earlier submission time (the
	     * wraparound case). Note: we force a one second separation
	     * in time for the latter case to ensure that this assumption
	     * holds.
	     */
            if (prio > prio2 ||
                  (prio == prio2 &&
		  (start_time_of_job1 < start_time_of_job2 || /* JobID rollover */
		  (start_time_of_job1 == start_time_of_job2 && job_id < job_id2)))) { /*Regular case */
               break;
            }
            insert_jep = jep;
         }

         if (insert_jep) {
            job_list.insert_after(insert_jep, std::move(dechained_job));
         } else {
            job_list.push_front(std::move(dechained_job));
         }
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Recompute JAT prio based on changed ticket amount
 *
 * Each time when the ticket amount for in a JAT_Type element is changed
 * the JAT_prio needs to be updated. The new ticket value is normalized
 * and the priorty value is computed.
 *
 * @param tref The tref element that is related to the ticket change
 * @param task The JAT_Type task element.
 * @param nurg The normalized urgency assumed for the job.
 * @param npri The normalized POSIX priority assumed for the job.
 */
static void
recompute_prio(sge_task_ref_t *tref, const ocs::tom::Element<ocs::tom::JAT> task, const double nurg,
               const double npri) {
   DENTER(TOP_LAYER);

   double min_tix, max_tix, prio;
   double ntix = 0.5;
   double weight_ticket = 0.0;
   double weight_urgency = 0.0;
   double weight_priority = 0.0;

   sconf_get_weight_ticket_urgency_priority(&weight_ticket, &weight_urgency, &weight_priority);

   /* need to know min/max tix values to normalize new ticket value */
   tix_range_get(&min_tix, &max_tix);
   ntix = sge_normalize_value(tref->ja_task_ticket, min_tix, max_tix);
   ocs::tom::set<JAT_ntix>(task, ntix);

   /* prio changes, but only due to ntix update */
   prio = weight_urgency * nurg + weight_priority * npri + weight_ticket * ntix;
   ocs::tom::set<JAT_prio>(task, prio);

   DPRINTF("%f tickets for task " sge_u32 ": ntix = %f (min/max %f/%f), prio = %f\n",
           tref->ja_task_ticket, tref->ja_task_number, ntix, min_tix, max_tix, prio);

   DRETURN_VOID;
}

/**
 * @brief Whether an array task is active
 *
 * Active is a task that is running, migrating or transferring and that is not
 * suspended.
 *
 * @param job the job of the task, unused
 * @param ja_task the array task
 * @return 1 if the task is active, 0 otherwise
 */
static int
job_is_active([[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::JB> job,
              const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(BASIS_LAYER);
   u_long job_status = ocs::tom::get<JAT_status>(ja_task);
#ifdef SGE_INCLUDE_QUEUED_JOBS
   DRETURN((job_status == JIDLE ||
       job_status & (JRUNNING | JMIGRATING | JQUEUED | JTRANSFERING)) &&
       !(ocs::tom::get<JAT_state>(ja_task)
         & (JSUSPENDED|JSUSPENDED_ON_THRESHOLD)));
#else
   DRETURN((job_status & (JRUNNING | JMIGRATING | JTRANSFERING)) &&
       !(ocs::tom::get<JAT_state>(ja_task) &
         (JSUSPENDED|JSUSPENDED_ON_THRESHOLD)));
#endif
}

/**
 * @brief Locate a department by its name
 *
 * @param dept_list the departments
 * @param name the name of the department, may be nullptr
 * @return the department, empty if @p name is nullptr or no department has that name
 */
static ocs::tom::Element<ocs::tom::US>
locate_department(const ocs::tom::List<ocs::tom::US> dept_list, const char *name) {
   DENTER(BASIS_LAYER);
   if (name == nullptr) {
      DRETURN(nullptr);
   }

   DRETURN(ocs::tom::find_first_rw<US_name>(dept_list, name));
}



/**
 * @brief Set the object references of a job entry
 *
 * Resets the entry, then locates the user, project, department and share tree
 * node of the job and zeroes their job counts. A "default" share tree node
 * found for the user is replaced by a temporary copy named after the user,
 * which is appended to the children of the default node's parent.
 *
 * @param job the job
 * @param ja_task the array task
 * @param ref the entry to fill
 * @param tref the task reference of a task that is not enrolled, nullptr for an enrolled task
 * @param lists the scheduler's lists
 * @param queued 1 if the job is pending, 0 if it is running
 */
static void
sge_set_job_refs(const ocs::tom::Element<ocs::tom::JB> job,
                 const ocs::tom::Element<ocs::tom::JAT> ja_task,
                 sge_ref_t *ref,
                 sge_task_ref_t *tref,
                 scheduler_all_data_t *lists,
                 const int queued) {
   DENTER(BASIS_LAYER);
   int is_enrolled_ja_task = (ja_task != nullptr && tref == nullptr) ? 1 : 0;

   *ref = sge_ref_t{};

   if (tref != nullptr) {
      memset(tref, 0, sizeof(sge_task_ref_t));

      tref->job_number = ocs::tom::get<JB_job_number>(job);
      tref->ja_task_number = ocs::tom::get<JAT_task_number>(ja_task);
      ref->tref = tref;
   }

   /*-------------------------------------------------------------
    * save job reference
    *-------------------------------------------------------------*/

   ref->job = job;

   /*-------------------------------------------------------------
    * save task reference
    *-------------------------------------------------------------*/

   ref->ja_task = is_enrolled_ja_task ? ja_task : nullptr;

   /*-------------------------------------------------------------
    * save job state
    *-------------------------------------------------------------*/

   ref->queued = queued;

   /*-------------------------------------------------------------
    * locate user object and save off reference
    *-------------------------------------------------------------*/

   ref->user = ocs::tom::Element<ocs::tom::UU>(user_list_locate(*lists->user_list, ocs::tom::get<JB_owner>(job)));
   if (ref->user) {
      ocs::tom::set<UU_job_cnt>(ref->user, 0);
      ocs::tom::set<UU_pending_job_cnt>(ref->user, 0);
   }

   /*-------------------------------------------------------------
    * locate project object and save off reference
    *-------------------------------------------------------------*/

   ref->project = ocs::tom::Element<ocs::tom::PR>(prj_list_locate(*lists->project_list,
                                                                   ocs::tom::get<JB_project>(job)));
   if (ref->project) {
      ocs::tom::set<PR_job_cnt>(ref->project, 0);
      ocs::tom::set<PR_pending_job_cnt>(ref->project, 0);
   }

   /*-------------------------------------------------------------
    * locate share tree node and save off reference
    *-------------------------------------------------------------*/

   if (const auto root = lists->share_tree->front(); root) {
      lListElem *pnode = nullptr;

      ref->share_tree_type = ocs::tom::get<STN_type>(root);

      if (ref->user || ref->project) {
         ref->node = ocs::tom::Element<ocs::tom::STN>(ocs::ShareTree::search_user_project_node(root,
               ref->user ? ocs::tom::get<UU_name>(ref->user) : nullptr,
               ref->project ? ocs::tom::get<PR_name>(ref->project) : nullptr,
               &pnode, root));

         /*
          * if the found node is a "default" node, then create a
          * temporary sibling node using the "default" node as a
          * template
          */

         if (ref->user && ref->node && pnode &&
             strcmp(ocs::tom::get<STN_name>(ref->node), "default") == 0) {
            auto temp_node = ref->node.clone();
            ocs::tom::set<STN_name>(*temp_node, ocs::tom::get<UU_name>(ref->user));
            ocs::tom::set<STN_temp>(*temp_node, 1);
            ref->node = ocs::tom::get<STN_children>(ocs::tom::Element<ocs::tom::STN>(pnode))
                              .push_back(std::move(temp_node));
         }
      } else
         ref->node = nullptr;
   }

   /*-------------------------------------------------------------
    * locate department object and save off reference in job entry
    *-------------------------------------------------------------*/

   ref->dept = locate_department(lists->dept_list.get(),
                                 ocs::tom::get<JB_department>(job));
   if (ref->dept) {
      ocs::tom::set<US_job_cnt>(ref->dept, 0);
      ocs::tom::set<US_pending_job_cnt>(ref->dept, 0);
   }
   DRETURN_VOID;
}


/**
 * @brief Count a job in the job counts of its user, project and department
 *
 * @param ref the job's entry
 * @param queued nonzero to count it as pending, 0 to count it as running
 */
static void
sge_set_job_cnts(sge_ref_t *ref, const int queued) {
   DENTER(BASIS_LAYER);
   int prj_job_cnt_nm = queued ? PR_pending_job_cnt : PR_job_cnt;
   int user_job_cnt_nm = queued ? UU_pending_job_cnt : UU_job_cnt;
   int us_job_cnt_nm = queued ? US_pending_job_cnt : US_job_cnt;
   if (ref->user) {
      lAddUlong(ref->user, user_job_cnt_nm, 1);
   }
   if (ref->project) {
      lAddUlong(ref->project, prj_job_cnt_nm, 1);
   }
   if (ref->dept) {
      lAddUlong(ref->dept, us_job_cnt_nm, 1);
   }
   DRETURN_VOID;
}


/**
 * @brief Take a job out of the job counts of its user, project and department
 *
 * @param ref the job's entry
 * @param queued nonzero to uncount it as pending, 0 to uncount it as running
 */
static void
sge_unset_job_cnts(sge_ref_t *ref, const int queued) {
   DENTER(BASIS_LAYER);
   int prj_job_cnt_nm = queued ? PR_pending_job_cnt : PR_job_cnt;
   int user_job_cnt_nm = queued ? UU_pending_job_cnt : UU_job_cnt;
   int us_job_cnt_nm = queued ? US_pending_job_cnt : US_job_cnt;
   if (ref->user) {
      lAddUlong(ref->user, user_job_cnt_nm, -1);
   }
   if (ref->project) {
      lAddUlong(ref->project, prj_job_cnt_nm, -1);
   }
   if (ref->dept) {
      lAddUlong(ref->dept, us_job_cnt_nm, -1);
   }
   DRETURN_VOID;
}


/**
 * @brief Calculates the modified share of a share tree node and its descendants
 *
 * Called with the root node it covers every active node of the tree. The
 * value is deliberately **not** recomputed every scheduling interval: it is
 * computed when the scheduler comes up and whenever the share tree changes,
 * and adjusted incrementally in `adjust_m_shares()` whenever a job becomes
 * active or inactive.
 *
 * @param[in,out] parent_node the node whose subtree is recalculated
 */
void
calculate_m_shares(const ocs::tom::Element<ocs::tom::STN> parent_node) {
   DENTER(TOP_LAYER);
   u_long sum_of_child_shares = 0;
   double parent_m_share;

   const auto children = ocs::tom::get<STN_children>(parent_node);
   if (!children) {
      DRETURN_VOID;
   }

   /*-------------------------------------------------------------
    * Sum child shares
    *-------------------------------------------------------------*/

   for (const auto child_node : children) {

      if (ocs::tom::get<STN_job_ref_count>(child_node) > 0)
         sum_of_child_shares += ocs::tom::get<STN_shares>(child_node);
   }

   /*-------------------------------------------------------------
    * Calculate m_shares for each child and descendants
    *-------------------------------------------------------------*/

   parent_m_share = ocs::tom::get<STN_m_share>(parent_node);

   for (const auto child_node : children) {
      if (ocs::tom::get<STN_job_ref_count>(child_node) > 0)
         ocs::tom::set<STN_m_share>(child_node,
                    parent_m_share *
                    (sum_of_child_shares ?
                    ((double)(ocs::tom::get<STN_shares>(child_node)) /
                    (double)sum_of_child_shares) : 0));
      else
         ocs::tom::set<STN_m_share>(child_node, 0);

      calculate_m_shares(child_node);
   }

   DRETURN_VOID;
}


/**
 * @brief Recomputes the job reference count of a node and its descendants
 *
 * @param[in,out] node the node whose subtree is recounted
 *
 * @return the number of jobs below and at the node
 */
u_long
update_job_ref_count(const ocs::tom::Element<ocs::tom::STN> node) {
   DENTER(BASIS_LAYER);
   int job_count=0;

   const auto children = ocs::tom::get<STN_children>(node);
   if (children) {
      for (const auto child : children) {
         job_count += update_job_ref_count(child);
      }
      ocs::tom::set<STN_job_ref_count>(node, job_count);
   }

   DRETURN(ocs::tom::get<STN_job_ref_count>(node));
}

/**
 * @brief Recomputes the active job reference count of a node and its descendants
 *
 * @param[in,out] node the node whose subtree is recounted
 *
 * @return the number of active jobs below and at the node
 */
u_long
update_active_job_ref_count(const ocs::tom::Element<ocs::tom::STN> node) {
   DENTER(BASIS_LAYER);
   int active_job_count=0;

   const auto children = ocs::tom::get<STN_children>(node);
   if (children) {
      for (const auto child : children) {
         active_job_count += update_active_job_ref_count(child);
      }
      ocs::tom::set<STN_active_job_ref_count>(node, active_job_count);
   }

   DRETURN(ocs::tom::get<STN_active_job_ref_count>(node));
}


/**
 * @brief Zero the share tree node fields that sge_calc_tickets sets and uses
 *
 * Called for every node of the share tree by ocs::ShareTree::foreach_call_func().
 *
 * The signature is the one of the walk's callback, so the node comes untyped
 * and is checked here.
 *
 * @param node_raw the share tree node (`STN_Type`)
 * @param ptr unused
 * @return 0, so the walk continues
 */
static int
sge_init_share_tree_node_fields(lListElem *node_raw,
                                [[maybe_unused]] void *ptr) {
   DENTER(BASIS_LAYER);
   const auto node = ocs::tom::Element<ocs::tom::STN>(node_raw);
   ocs::tom::set<STN_m_share>(node, 0);
   ocs::tom::set<STN_last_actual_proportion>(node, 0);
   ocs::tom::set<STN_adjusted_current_proportion>(node, 0);
   ocs::tom::set<STN_actual_proportion>(node, 0);
   ocs::tom::set<STN_job_ref_count>(node, 0);
   ocs::tom::set<STN_active_job_ref_count>(node, 0);
   ocs::tom::set<STN_usage_list>(node, nullptr);
   ocs::tom::set<STN_sum_priority>(node, 0);
   ocs::tom::set<STN_stt>(node, 0);
   ocs::tom::set<STN_ostt>(node, 0);
   ocs::tom::set<STN_ltt>(node, 0);
   ocs::tom::set<STN_oltt>(node, 0);
   ocs::tom::set<STN_shr>(node, 0);
   ocs::tom::set<STN_ref>(node, 0);
   DRETURN(0);
}

/**
 * @brief Zeroes the share tree node fields that a ticket calculation fills
 *
 * Has to run before `sge_calc_tickets()`, otherwise the values of the
 * previous run would be added to.
 *
 * @param[in,out] root the root of the share tree
 *
 * @return 0 on success
 */
int
sge_init_share_tree_nodes(const ocs::tom::Element<ocs::tom::STN> root) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::ShareTree::foreach_call_func(root, sge_init_share_tree_node_fields, nullptr));
}

/**
 * @brief Combine the usage of a job's user and project into one value of its share tree node
 *
 * The usage comes from the user's usage of the project if the job has both,
 * from the user if it has no project, and from the project if it has no user.
 * Each usage value is weighted by the configured usage weights.
 *
 * @param ref the job's entry; nothing is done if it has no share tree node
 */
static void
combine_usage(sge_ref_t *ref) {
   DENTER(BASIS_LAYER);
   double usage_value = 0;

   /*-------------------------------------------------------------
    * Get usage from associated user/project object
    *-------------------------------------------------------------*/
   if (ref->node) {
      ocs::tom::ConstList<ocs::tom::UA> usage_list = nullptr;
      double sum_of_usage_weights = 0;
      const char *usage_name;

      /*-------------------------------------------------------------
       * Sum usage weighting factors
       *-------------------------------------------------------------*/

      const auto usage_weight_list = ocs::tom::ListPtr<ocs::tom::UA>(sconf_get_usage_weight_list());
      if (usage_weight_list) {
         for (const auto usage_weight : *usage_weight_list) {
            sum_of_usage_weights += ocs::tom::get<UA_value>(usage_weight);
         }
      }

      /*-------------------------------------------------------------
       * Combine user/project usage based on usage weighting factors
       *-------------------------------------------------------------*/

      if (usage_weight_list) {

         if (ref->user) {
            if (ref->project) {
               const auto upp_list = ocs::tom::get<UU_project>(ref->user);
               if (upp_list) {
                  const auto upp = ocs::tom::find_first<UPP_name>(upp_list, ocs::tom::get<PR_name>(ref->project));
                  if (upp) {
                     usage_list = ocs::tom::get<UPP_usage>(upp);
                  }
               }
            }
            else {
               usage_list = ocs::tom::get<UU_usage>(ref->user);
            }
         }
         else if (ref->project) { /* not sure about this, use it when? */
            usage_list = ocs::tom::get<PR_usage>(ref->project);
         }

         for (const auto usage_elem : usage_list) {
            usage_name = ocs::tom::get<UA_name>(usage_elem);
            const auto usage_weight = ocs::tom::find_first<UA_name>(*usage_weight_list, usage_name);
            if (usage_weight && sum_of_usage_weights>0) {
               usage_value += ocs::tom::get<UA_value>(usage_elem) *
                              (ocs::tom::get<UA_value>(usage_weight) / sum_of_usage_weights);
            }
         }
      }

      /*-------------------------------------------------------------
       * Set combined usage in the node
       *-------------------------------------------------------------*/

      ocs::tom::set<STN_combined_usage>(ref->node, usage_value);
   }

   DRETURN_VOID;
}


/**
 * @brief Pass 1 of the share tree tickets of a job: add the job's shares to its share tree node
 *
 * The sum is what pass 2 divides the node's tickets by.
 *
 * @param ref the job's entry; nothing is done if it has no share tree node
 */
static void
calc_job_share_tree_tickets_pass1(sge_ref_t *ref) {
   DENTER(BASIS_LAYER);
   const auto job = ref->job;
   const auto node = ref->node;

   if (!node) {
      DRETURN_VOID;
   }

   /*-------------------------------------------------------
    * sum job shares for each job for use in pass 2
    *-------------------------------------------------------*/

   ocs::tom::set<STN_sum_priority>(node,
             ocs::tom::get<STN_sum_priority>(node) +
             ocs::tom::get<JB_jobshare>(job));
   DRETURN_VOID;
}


/**
 * @brief Pass 2 of the share tree tickets of a job: set the job's share tree tickets
 *
 * The job gets its part of the tickets of its share tree node, in proportion
 * to its shares if the jobs of the node have shares, otherwise an equal part.
 *
 * @param ref the job's entry; nothing is done if it has no share tree node
 * @param total_share_tree_tickets the tickets of the share tree policy
 */
static void
calc_job_share_tree_tickets_pass2(sge_ref_t *ref, const double total_share_tree_tickets) {
   DENTER(BASIS_LAYER);
   double share_tree_tickets;
   const auto job = ref->job;
   const auto node = ref->node;

   if (!node) {
      DRETURN_VOID;
   }

   /*-------------------------------------------------------
    * calculate the number of share tree tickets for this job
    *-------------------------------------------------------*/

   share_tree_tickets = ocs::tom::get<STN_adjusted_current_proportion>(node) *
                        total_share_tree_tickets;

   if (ocs::tom::get<STN_sum_priority>(node)) {
      REF_SET_STICKET(ref,
                (u_long)((double)ocs::tom::get<JB_jobshare>(job) *
                share_tree_tickets /
                ocs::tom::get<STN_sum_priority>(node)));
   } else {
      REF_SET_STICKET(ref,
                share_tree_tickets / ocs::tom::get<STN_job_ref_count>(node));
   }
   DRETURN_VOID;
}

/**
 * @brief Copy the ftix from one job to an other one
 *
 * Copy the functional tickets and ref fields used for ftix calculation
 * from one job to an other job.
 *
 * @param source source job
 * @param dest dest job
 *
 * @bug ???
 */
static void copy_ftickets(sge_ref_list_t *source, sge_ref_list_t *dest){
   if (source != nullptr && dest != nullptr) {
      sge_ref_t *dest_r = dest->ref;
      sge_ref_t *source_r = source->ref;

      dest_r->user_fshare = source_r->user_fshare;
      dest_r->project_fshare = source_r->project_fshare;
      dest_r->dept_fshare = source_r->dept_fshare;
      dest_r->job_fshare = source_r->job_fshare;

      REF_SET_FSHARE(dest_r, ocs::tom::get<JB_jobshare>(source_r->job));
      REF_SET_FTICKET(dest_r, REF_GET_FTICKET(source_r));
   }
}

/**
 * @brief Ensures, that all jobs have ftix asoziated with them
 *
 * After the functional tickets are calculated, only the first job in the fcategory
 * job list has ftix. This function copies the result from the first job to all
 * other jobs in the same list and sums the job ticket count with the ftix.
 *
 * @param fcategories the functional categories
 * @param dependent does the final ticket count depend on ftix?
 *
 * @note - This function is only needed, because not all functional tickets are calculated
 *       and to give a best guess result, all jobs in one category with no ftix get the
 *       same amount of ftix.
 */
static void
destribute_ftickets(const ocs::tom::ConstList<ocs::tom::FCAT> fcategories, const int dependent) {
   DENTER(TOP_LAYER);
   sge_ref_list_t *current = nullptr;
   sge_ref_list_t *first = nullptr;

   for (const auto elem : fcategories) {
      first = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(elem));
/*      if(!first)
         continue;*/
      current = first->next;

      while(current != nullptr) {
         copy_ftickets(first, current);
         if(dependent)
            current->ref->tickets += REF_GET_FTICKET(current->ref);
         current = current->next;
      }
   }
   DRETURN_VOID;
}

/*
 * job classes are ignored.
 */
/**
 * @brief Sorts the pending jobs into functional categories
 *
 * Generates a list of functional categories. Each category contains a list of jobs
 * which belongs to this category. A functional category is assembled of:
 * - job shares
 * - user shares
 * - department shares
 * - project shares
 * Alljobs with the same job, user,... shares are put in the same fcategory.
 *
 * @param job_ref array of the job reference structures
 * @param num_jobs amount of elements in the job_ref array
 * @param fcategories the functional category list; created here when the first category is added
 * @param ref_array has to be a pointer to nullptr pointer. The memory will be allocated in this function and freed
 *                  with free_fcategories.
 * @param dependent does the functional tickets depend on prior computed tickets?
 * @param job_tickets job field, which has the tickets (JB_jobshare, JB_override_tickets)
 * @param user_tickets source for the user tickets/shares (UU_fshare, UU_oticket)
 * @param project_tickets source for the project tickets/shares (PR_fshare, PR_oticket)
 * @param dp_tickets source for the department tickets/shares (US_fshare, US_oticket)
 * @return number of jobs in the categories
 *
 * @note - job classes are ignored.
 *
 *       IMPROVEMENTS:
 *       - the stored values in the functional category structure can be used to speed up the
 *       ticket calculation. This will avoid unnecessary CULL accesses in the function
 *       calc_job_functional_tickets_pass1
 *       - A further improvement can be done by:
 *       - limiting the job list length in each category to the max nr of jobs calculated
 *       - Sorting the jobs in each functional category by its job category. Each resulting
 *       job list can be of max size of open slots. This will result in a correct ftix result
 *       for all jobs, which might be scheduled.
 *
 * @bug ???
 */
static uint32_t
build_functional_categories(sge_ref_t *job_ref, const uint32_t num_jobs,
                            ocs::tom::ListPtr<ocs::tom::FCAT> &fcategories,
                            sge_ref_list_t **ref_array, const int dependent,
                            const JB_Field job_tickets, const UU_Field user_tickets, const PR_Field project_tickets,
                            const US_Field dp_tickets) {
   DENTER(TOP_LAYER);

   uint32_t job_ndx;
   int ref_array_pos = 0;
   uint32_t job_counter = 0;

   *ref_array = (sge_ref_list_t *)sge_malloc(sizeof(sge_ref_list_t) * num_jobs);
   if (*ref_array == nullptr) {
      DRETURN(0);
   }

   memset(*ref_array, 0, sizeof(sge_ref_list_t) * num_jobs);

   for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
      sge_ref_t *jref = &job_ref[job_ndx];

      if (jref->queued) { /* only work on pending jobs */
         uint32_t job_shares = 0;
         uint32_t user_shares = 0;
         uint32_t dept_shares = 0;
         uint32_t project_shares = 0;

         auto user_el = jref->user;
         auto dept_el = jref->dept;
         auto project_el = jref->project;

         if(user_el){
            user_shares = user_el.get<uint32_t>(user_tickets);
            if (user_shares == 0) {
               user_el = nullptr;
            }
         }
         if(project_el){
            project_shares = project_el.get<uint32_t>(project_tickets);
            if (project_shares == 0) {
               project_el = nullptr;
            }

         }
         if(dept_el){
            dept_shares = dept_el.get<uint32_t>(dp_tickets);
            if (dept_shares == 0) {
               dept_el = nullptr;
            }

         }

         job_shares = jref->job.get<uint32_t>(job_tickets);

         /* jobs in this category will never get tickets, so why compute them...? */
         if ( (job_shares == 0) &&
              (user_shares == 0) &&
              (dept_shares == 0) &&
              (project_shares == 0)) {
           continue;
         }

         job_counter++;

         /* locate the right category */
         ocs::tom::Element<ocs::tom::FCAT> current;
         for (const auto fcategory : *fcategories) {
            if ((ocs::tom::get<FCAT_job_share>(fcategory) == job_shares) &&
                (ocs::tom::get<FCAT_user>(fcategory) == user_el.raw()) &&
                (ocs::tom::get<FCAT_dept>(fcategory) == dept_el.raw()) &&
                (ocs::tom::get<FCAT_project>(fcategory) == project_el.raw())) {
               current = fcategory;
               break;
            }
         }

         if (current == nullptr) {   /* create new category */
            if (!fcategories) {
               // the name and the hashing are those of the list lAddElemUlong() created here before
               fcategories = ocs::tom::ListPtr<ocs::tom::FCAT>::create("ulong_sublist");
            }
            current = fcategories->emplace_back<FCAT_job_share>(job_shares);
            ocs::tom::set<FCAT_user_share>(current, user_shares);
            ocs::tom::set<FCAT_user>(current, user_el.raw());
            ocs::tom::set<FCAT_dept_share>(current, dept_shares);
            ocs::tom::set<FCAT_dept>(current, dept_el.raw());
            ocs::tom::set<FCAT_project_share>(current, project_shares);
            ocs::tom::set<FCAT_project>(current, project_el.raw());
         }

         {  /* create new job entry */
            sge_ref_list_t  *cur_ref_entry = nullptr;
            sge_ref_list_t *new_elem = &((*ref_array)[ref_array_pos++]);

            new_elem->ref = jref;

            /* find the location for the new element */
            cur_ref_entry = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_last>(current));

            if (dependent) {
               while (cur_ref_entry != nullptr) {
                  sge_ref_t *ref = cur_ref_entry->ref;

                  if (ref->tickets > jref->tickets) {
                     break;
                  }
                  else if (ref->tickets == jref->tickets){
                     uint64_t ref_time = ocs::tom::get<JB_submission_time>(ref->job);
                     uint64_t jref_time = ocs::tom::get<JB_submission_time>(jref->job);
                     uint32_t ref_jid = ocs::tom::get<JB_job_number>(ref->job);
                     uint32_t jref_jid = ocs::tom::get<JB_job_number>(jref->job);
		     if (ref_time < jref_time ||
                           (ref_time == jref_time && ref_jid < jref_jid)) {
                        break;
                     }
                     else if ((ref_jid == jref_jid)
                           && (REF_GET_JA_TASK_NUMBER(ref) < REF_GET_JA_TASK_NUMBER(jref)))
                        break;
                  }

                  cur_ref_entry = cur_ref_entry->prev;
               }
            }

            /* insert element into the list */
            if (cur_ref_entry == nullptr) {
               if (ocs::tom::get<FCAT_jobrelated_ticket_first>(current) == nullptr) {
                  /* first element, no elements in the list */
                  ocs::tom::set<FCAT_jobrelated_ticket_first>(current, new_elem);
                  ocs::tom::set<FCAT_jobrelated_ticket_last>(current, new_elem);
               }
               else {
                  /* insert element at the beginning */
                  cur_ref_entry = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(current));
                  cur_ref_entry->prev = new_elem;
                  new_elem->next = cur_ref_entry;
                  ocs::tom::set<FCAT_jobrelated_ticket_first>(current, new_elem);

               }
            }
            else {
               /* insert element somewhere in the list */
               new_elem->prev = cur_ref_entry;
               new_elem->next = cur_ref_entry->next;
               if(cur_ref_entry->next != nullptr){
                  cur_ref_entry->next->prev = new_elem;
               }
               else {
                  /* element is the last in the list */
                  ocs::tom::set<FCAT_jobrelated_ticket_last>(current, new_elem);
               }
               cur_ref_entry->next = new_elem;
            }
         }
      }

   }
   DRETURN(job_counter);
}

/**
 * @brief Frees all fcategories and their job lists
 *
 * frees all fcategories and their job lists.
 *
 * @param fcategories the functional categories, empty afterwards
 * @param ref_array memory for internal structures, allocated with build_functional_categories. Needs to be freed
 *                  as well.
 *
 * @note - it does not delete the sge_ref_t structures, which are stored in
 *       in the job lists.
 */
static void
free_fcategories(ocs::tom::ListPtr<ocs::tom::FCAT> &fcategories, sge_ref_list_t **ref_array) {
   DENTER(TOP_LAYER);

   for (const auto fcategory : *fcategories) {
      ocs::tom::set<FCAT_jobrelated_ticket_last>(fcategory, nullptr);
      ocs::tom::set<FCAT_jobrelated_ticket_first>(fcategory, nullptr);
      ocs::tom::set<FCAT_user>(fcategory, nullptr);
      ocs::tom::set<FCAT_dept>(fcategory, nullptr);
      ocs::tom::set<FCAT_project>(fcategory, nullptr);
   }

   fcategories.reset();
   sge_free(ref_array);

   DRETURN_VOID;
}

/**
 * @brief Pass 1 of the functional tickets of a job: the job's functional shares and their sums
 *
 * @param ref the job's entry; receives the job's user, project, department and job functional shares
 * @param[in,out] sum_of_user_functional_shares the sum of the user functional shares, the job's is added
 * @param[in,out] sum_of_project_functional_shares the sum of the project functional shares, the job's is added
 * @param[in,out] sum_of_department_functional_shares the sum of the department functional shares, the job's
 *                is added
 * @param[in,out] sum_of_job_functional_shares the sum of the job functional shares, the job's is added
 * @param shared nonzero if the functional shares of an object are shared between its jobs
 * @param sum_shares nonzero to add the user, project and department shares in any case, 0 to add them
 *                   only if the object has at most one job
 */
static void
calc_job_functional_tickets_pass1(sge_ref_t *ref,
                                  double *sum_of_user_functional_shares,
                                  double *sum_of_project_functional_shares,
                                  double *sum_of_department_functional_shares,
                                  double *sum_of_job_functional_shares,
                                  const int shared,
                                  const int sum_shares) {
   DENTER(BASIS_LAYER);
   double job_cnt;

   /*-------------------------------------------------------------
    * Sum user functional shares
    *-------------------------------------------------------------*/

   if (ref->user) {
      job_cnt = ocs::tom::get<UU_job_cnt>(ref->user);
      ref->user_fshare = shared ?
            (double)ocs::tom::get<UU_fshare>(ref->user) / job_cnt :
            ocs::tom::get<UU_fshare>(ref->user);
      if(sum_shares || job_cnt<=1)
         *sum_of_user_functional_shares += ref->user_fshare;

   }

   /*-------------------------------------------------------------
    * Sum project functional shares
    *-------------------------------------------------------------*/

   if (ref->project) {
      job_cnt = ocs::tom::get<PR_job_cnt>(ref->project);
      ref->project_fshare = shared ?
            (double)ocs::tom::get<PR_fshare>(ref->project) / job_cnt :
            ocs::tom::get<PR_fshare>(ref->project);
      if(sum_shares || job_cnt<=1)
         *sum_of_project_functional_shares += ref->project_fshare;
   }

   /*-------------------------------------------------------------
    * Sum department functional shares
    *-------------------------------------------------------------*/

   if (ref->dept) {
      job_cnt = ocs::tom::get<US_job_cnt>(ref->dept);
      ref->dept_fshare = shared ?
            (double)ocs::tom::get<US_fshare>(ref->dept) / job_cnt :
            ocs::tom::get<US_fshare>(ref->dept);
      if(sum_shares || job_cnt<=1)
         *sum_of_department_functional_shares += ref->dept_fshare;
   }

   /*-------------------------------------------------------------
    * Sum job functional shares
    *-------------------------------------------------------------*/

   REF_SET_FSHARE(ref, ocs::tom::get<JB_jobshare>(ref->job));

   *sum_of_job_functional_shares += REF_GET_FSHARE(ref);
   DRETURN_VOID;
}

/**
 * @brief The four categories the functional policy weights against each other
 *
 * Index into the array of weighting parameters; #k_last is the number of
 * categories and the loop bound.
 */
enum {
   k_user=0,        ///< Weight of the user category
   k_department,    ///< Weight of the department category
   k_project,       ///< Weight of the project category
   k_job,           ///< Weight of the job category
   k_last           ///< Number of categories
};


/*--------------------------------------------------------------------
 * get_functional_weighting_parameters - get the weighting parameters for
 *      the functional policy categories from the configuration
 *--------------------------------------------------------------------*/


static void
get_functional_weighting_parameters(double sum_of_user_functional_shares,
                                    double sum_of_project_functional_shares,
                                    double sum_of_department_functional_shares,
                                    double sum_of_job_functional_shares,
                                    double weight[]) {
   double k_sum;
   memset(weight, 0, sizeof(double)*k_last);

   if (sconf_is()) {
      if (sum_of_user_functional_shares > 0)
         weight[k_user] = sconf_get_weight_user();
      if (sum_of_department_functional_shares > 0)
         weight[k_department] = sconf_get_weight_department();
      if (sum_of_project_functional_shares > 0)
         weight[k_project] = sconf_get_weight_project();
      if (sum_of_job_functional_shares > 0)
         weight[k_job] = sconf_get_weight_job();
      k_sum = weight[k_user] + weight[k_department] + weight[k_project] +
              weight[k_job];
   } else {
      weight[k_user] = 1;
      weight[k_department] = 1;
      weight[k_project] = 1;
      weight[k_job] = 1;
      k_sum = weight[k_user] + weight[k_department] + weight[k_project] +
              weight[k_job];
   }

   if (k_sum>0) {
      weight[k_user] /= k_sum;
      weight[k_department] /= k_sum;
      weight[k_project] /= k_sum;
      weight[k_job] /= k_sum;
   }

   return;
}


/**
 * @brief Pass 2 of the functional tickets of a job: set the job's functional tickets
 *
 * @param ref the job's entry, with the functional shares of pass 1; receives the functional tickets
 * @param sum_of_user_functional_shares the sum of the user functional shares
 * @param sum_of_project_functional_shares the sum of the project functional shares
 * @param sum_of_department_functional_shares the sum of the department functional shares
 * @param sum_of_job_functional_shares the sum of the job functional shares
 * @param total_functional_tickets the tickets of the functional policy
 * @param weight the weights of the categories, indexed by k_user, k_department, k_project and k_job
 * @param shared unused
 * @return the job functional tickets of the job, before weighting
 */
static double
calc_job_functional_tickets_pass2(sge_ref_t *ref,
                                  const double sum_of_user_functional_shares,
                                  const double sum_of_project_functional_shares,
                                  const double sum_of_department_functional_shares,
                                  const double sum_of_job_functional_shares,
                                  const double total_functional_tickets,
                                  double weight[],
                                  [[maybe_unused]] const int shared) {
   DENTER(BASIS_LAYER);
   double user_functional_tickets=0,
          project_functional_tickets=0,
          department_functional_tickets=0,
          job_functional_tickets=0,
          total_job_functional_tickets;

   /*-------------------------------------------------------
    * calculate user functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->user && sum_of_user_functional_shares) {
      user_functional_tickets = (ref->user_fshare *
                                total_functional_tickets /
                                sum_of_user_functional_shares);
   }

   /*-------------------------------------------------------
    * calculate project functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->project && sum_of_project_functional_shares)
      project_functional_tickets = (ref->project_fshare *
                                 total_functional_tickets /
                                 sum_of_project_functional_shares);

   /*-------------------------------------------------------
    * calculate department functional tickets for this job
    *-------------------------------------------------------*/

   if (ref->dept && sum_of_department_functional_shares)
      department_functional_tickets = (ref->dept_fshare *
                                 total_functional_tickets /
                                 sum_of_department_functional_shares);

   /*-------------------------------------------------------
    * calculate job functional tickets for this job
    *-------------------------------------------------------*/

   if (sum_of_job_functional_shares)
      job_functional_tickets = ((double)ocs::tom::get<JB_jobshare>(ref->job) *
                                 (double)total_functional_tickets /
                                  sum_of_job_functional_shares);

   /*-------------------------------------------------------
    * calculate functional tickets for this job
    *-------------------------------------------------------*/

   total_job_functional_tickets = weight[k_user] * user_functional_tickets +
             weight[k_department] * department_functional_tickets +
             weight[k_project] * project_functional_tickets +
             weight[k_job] * job_functional_tickets;

   REF_SET_FTICKET(ref, total_job_functional_tickets);

   DRETURN(job_functional_tickets);
}


/**
 * @brief Calculate the override tickets of a job
 *
 * The job's override tickets plus those of its user, project and department.
 *
 * @param ref the job's entry; receives the override tickets
 * @param shared nonzero if the override tickets of an object are shared between its jobs
 * @return the override tickets of the job
 */
static double
calc_job_override_tickets(sge_ref_t *ref, const int shared) {
   DENTER(TOP_LAYER);

   double job_override_tickets = 0;
   double otickets, job_cnt;

   /*-------------------------------------------------------
    * job.override_tickets = user.override_tickets +
    *                        project.override_tickets +
    *                        department.override_tickets +
    *                        job.override_tickets;
    *-------------------------------------------------------*/

   if (shared) {
      if (ref->user) {
         job_cnt = ocs::tom::get<UU_job_cnt>(ref->user);
         if (((otickets = ocs::tom::get<UU_oticket>(ref->user)) &&
             (job_cnt )))
            job_override_tickets += (otickets / job_cnt);
      }

      if (ref->project) {
         job_cnt = ocs::tom::get<PR_job_cnt>(ref->project);
         if (((otickets = ocs::tom::get<PR_oticket>(ref->project)) &&
             (job_cnt )))
            job_override_tickets += (otickets / job_cnt);
      }

      if (ref->dept) {
         job_cnt = ocs::tom::get<US_job_cnt>(ref->dept);
         if (((otickets = ocs::tom::get<US_oticket>(ref->dept)) &&
             (job_cnt )))
            job_override_tickets += (otickets / job_cnt);
      }
   }
   else {
      if (ref->user) {
         job_override_tickets += ocs::tom::get<UU_oticket>(ref->user);
      }

      if (ref->project) {
         job_override_tickets += ocs::tom::get<PR_oticket>(ref->project);
      }

      if (ref->dept) {
         job_override_tickets += ocs::tom::get<US_oticket>(ref->dept);
      }
   }

   job_override_tickets += ocs::tom::get<JB_override_tickets>(ref->job);

   REF_SET_OTICKET(ref, job_override_tickets);

   DRETURN(job_override_tickets);
}

/**
 * @brief Calculate the shared override tickets of a pending job as if it were running
 *
 * The override tickets of the user, project and department are shared
 * between their running jobs and this one.
 *
 * @param ref the pending job's entry; receives the override tickets
 * @return the override tickets of the job
 */
static double
calc_pjob_override_tickets_shared(sge_ref_t *ref) {
   DENTER(TOP_LAYER);

   double job_override_tickets = 0;
   double otickets, job_cnt;

   /*-------------------------------------------------------
    * job.override_tickets = user.override_tickets +
    *                        project.override_tickets +
    *                        department.override_tickets +
    *                        job.override_tickets;
    *-------------------------------------------------------*/

   if (ref->user) {
      if ((otickets = ocs::tom::get<UU_oticket>(ref->user)) > 0) {
         job_cnt = ocs::tom::get<UU_job_cnt>(ref->user) + 1;
         job_override_tickets += (otickets / job_cnt);
      }
   }

   if (ref->project) {
      if ((otickets = ocs::tom::get<PR_oticket>(ref->project)) > 0) {
         job_cnt = ocs::tom::get<PR_job_cnt>(ref->project) + 1;
         job_override_tickets += (otickets / job_cnt);
      }
   }

   if (ref->dept) {
      if ((otickets = ocs::tom::get<US_oticket>(ref->dept)) > 0) {
         job_cnt = ocs::tom::get<US_job_cnt>(ref->dept) + 1;
         job_override_tickets += (otickets / job_cnt);
      }
   }

   job_override_tickets += ocs::tom::get<JB_override_tickets>(ref->job);

   REF_SET_OTICKET(ref, job_override_tickets);

   DRETURN(job_override_tickets);
}

/**
 * @brief Calculate the total tickets of a job
 *
 * Sums the share tree, functional and override tickets of the job. For a
 * job of a parallel environment that controls its slaves, the tickets are
 * also distributed onto the granted queues.
 *
 * @param ref the job's entry; receives the total tickets
 * @return the total tickets of the job
 */
static double
calc_job_tickets(sge_ref_t *ref) {
   DENTER(BASIS_LAYER);
   ocs::tom::List<ocs::tom::JG> granted;
   const auto job = ref->job;
   const auto ja_task = ref->ja_task;
   double job_tickets;
   ocs::tom::ConstElement<ocs::tom::PE> pe;
   const char *pe_str;

   /*-------------------------------------------------------------
    * Sum up all tickets for job
    *-------------------------------------------------------------*/

   job_tickets = REF_GET_STICKET(ref) + REF_GET_FTICKET(ref) +
                 REF_GET_OTICKET(ref);

   REF_SET_TICKET(ref, job_tickets);

   /* for PE slave-controlled jobs, set tickets for each granted queue */

   if (ja_task && (pe_str=ocs::tom::get<JAT_granted_pe>(ja_task))
         && (pe = ocs::tom::ConstElement<ocs::tom::PE>(pe_list_locate(*all_lists->pe_list, pe_str)))
         && ocs::tom::get<PE_control_slaves>(pe)
         && (granted=ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task))) {

      double job_tickets_per_slot, job_otickets_per_slot,
             job_ftickets_per_slot, job_stickets_per_slot, nslots, active_nslots;

      /* Here we get the total number of slots granted on all queues (nslots) and the
         total number of slots granted on all queues with active subtasks or
         on which no subtasks have started yet (active_nslots). */

      nslots = nslots_granted(granted, nullptr);
      active_nslots = active_nslots_granted(job, granted, nullptr);

      /* If there are some active_nslots, then calculate the number of tickets
         per slot based on the active_nslots */

      if (active_nslots > 0)
         nslots = active_nslots;

      if (nslots > 0) {
         job_stickets_per_slot = REF_GET_STICKET(ref)/nslots;
      } else {
         job_stickets_per_slot = 0;
      }

      for (const auto granted_el : granted) {
         double slots;

         /* Only give tickets to queues with active sub-tasks or sub-tasks
            which have not started yet. */

         if (active_nslots > 0 && !active_subtasks(job, ocs::tom::get<JG_qname>(granted_el)))
            slots = 0;
         else
            slots = ocs::tom::get<JG_slots>(granted_el);

         if (nslots > 0) {
            job_ftickets_per_slot = (double)(REF_GET_FTICKET(ref))/nslots;
            job_otickets_per_slot = (double)(REF_GET_OTICKET(ref))/nslots;
            job_tickets_per_slot = job_stickets_per_slot + job_ftickets_per_slot + job_otickets_per_slot;
         } else {
            job_ftickets_per_slot = 0;
            job_otickets_per_slot = 0;
            job_tickets_per_slot = 0;
         }

         ocs::tom::set<JG_fticket>(granted_el, job_ftickets_per_slot*slots + ocs::tom::get<JG_jcfticket>(granted_el));
         ocs::tom::set<JG_oticket>(granted_el, job_otickets_per_slot*slots + ocs::tom::get<JG_jcoticket>(granted_el));
         ocs::tom::set<JG_sticket>(granted_el, job_stickets_per_slot*slots);
         ocs::tom::set<JG_ticket>(granted_el, job_tickets_per_slot*slots +
                   ocs::tom::get<JG_jcoticket>(granted_el) + ocs::tom::get<JG_jcfticket>(granted_el));

      }
   }

   DRETURN(job_tickets);
}


/**
 * @brief Clears the ticket values of a job
 *
 * @param[in,out] job          the job
 * @param[in]     is_clear_all true to also clear the urgency contributions,
 *                             not only the tickets
 */
void
sge_clear_job(const ocs::tom::Element<ocs::tom::JB> job, const bool is_clear_all) {
   DENTER(BASIS_LAYER);
   if (is_clear_all) {
      ocs::tom::set<JB_urg>(job,0.0);
      ocs::tom::set<JB_nurg>(job,0.0);
      ocs::tom::set<JB_dlcontr>(job,0.0);
      ocs::tom::set<JB_wtcontr>(job,0.0);
      ocs::tom::set<JB_rrcontr>(job,0.0);

      for (ocs::tom::Element<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_template>(job)) {
         sge_clear_ja_task(ja_task);
         ocs::tom::set<JAT_task_number>(ja_task, 1);
      }
   }

   for (ocs::tom::Element<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job))
      sge_clear_ja_task(ja_task);
   DRETURN_VOID;
}

/**
 * @brief Clear the tickets and the priority of an array task and of its granted queues
 *
 * @param ja_task the array task
 */
static void
sge_clear_ja_task(const ocs::tom::Element<ocs::tom::JAT> ja_task) {
   DENTER(BASIS_LAYER);
   ocs::tom::set<JAT_prio>(ja_task, 0);
   ocs::tom::set<JAT_ntix>(ja_task, 0);
   ocs::tom::set<JAT_tix>(ja_task, 0);
   ocs::tom::set<JAT_oticket>(ja_task, 0);
   ocs::tom::set<JAT_fticket>(ja_task, 0);
   ocs::tom::set<JAT_sticket>(ja_task, 0);
   ocs::tom::set<JAT_share>(ja_task, 0);
   for (ocs::tom::Element<ocs::tom::JG> granted_el : ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task)) {
      ocs::tom::set<JG_ticket>(granted_el, 0);
      ocs::tom::set<JG_oticket>(granted_el, 0);
      ocs::tom::set<JG_fticket>(granted_el, 0);
      ocs::tom::set<JG_sticket>(granted_el, 0);
      ocs::tom::set<JG_jcoticket>(granted_el, 0);
      ocs::tom::set<JG_jcfticket>(granted_el, 0);
   }
   DRETURN_VOID;
}

/**
 * @brief Calc ftix for pending jobs
 *
 * This is an optimized and incomplete version of calc_pending_job_functional_tickets.
 * It is good enough to get the order right within the inner loop of the ftix
 * calculation.
 *
 * The tickets are calculated for the first job of the category only.
 *
 * @param current the functional category
 * @param sum_of_user_functional_shares the sum of the user functional shares
 * @param sum_of_project_functional_shares the sum of the project functional shares
 * @param sum_of_department_functional_shares the sum of the department functional shares
 * @param sum_of_job_functional_shares the sum of the job functional shares
 * @param total_functional_tickets the tickets of the functional policy
 * @param weight destribution of the shares to each other
 * @param share_functional_shares true if the functional shares of an object are shared between its jobs
 *
 * @note be carefull using it
 *
 * @bug ???
 */
static void
calc_intern_pending_job_functional_tickets(const ocs::tom::ConstElement<ocs::tom::FCAT> current,
                                           double sum_of_user_functional_shares,
                                           double sum_of_project_functional_shares,
                                           double sum_of_department_functional_shares,
                                           double sum_of_job_functional_shares,
                                           const double total_functional_tickets,
                                           double weight[],
                                           const bool share_functional_shares) {
   DENTER(BASIS_LAYER);
   auto *tmp = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(current));
   sge_ref_t *ref = tmp->ref;

   /*-------------------------------------------------------------
    * Sum user functional shares
    *-------------------------------------------------------------*/

   if (ref->user) {
      ref->user_fshare = ocs::tom::get<FCAT_user_share>(current);
      if(share_functional_shares) {
         ref->user_fshare /= ocs::tom::get<UU_job_cnt>(ref->user) +1 ;
      }
      else {
         sum_of_user_functional_shares += ref->user_fshare;
      }
   }

   /*-------------------------------------------------------------
    * Sum project functional shares
    *-------------------------------------------------------------*/

   if (ref->project) {
      ref->project_fshare = ocs::tom::get<FCAT_project_share>(current);
      if(share_functional_shares) {
         ref->project_fshare /= (ocs::tom::get<PR_job_cnt>(ref->project) +1);
      }
      else {
         sum_of_project_functional_shares += ref->project_fshare;
      }
   }


   /*-------------------------------------------------------------
    * Sum department functional shares
    *-------------------------------------------------------------*/

   if (ref->dept) {
      ref->dept_fshare = ocs::tom::get<FCAT_dept_share>(current);
      if(share_functional_shares) {
            ref->dept_fshare /= ocs::tom::get<US_job_cnt>(ref->dept) +1;
      }
      else {
         sum_of_department_functional_shares += ref->dept_fshare;
      }
   }

   /*-------------------------------------------------------------
    * Sum job functional shares
    *-------------------------------------------------------------*/

   REF_SET_FSHARE(ref, ocs::tom::get<FCAT_job_share>(current));
   sum_of_job_functional_shares += REF_GET_FSHARE(ref);

   calc_job_functional_tickets_pass2(ref,
                                     sum_of_user_functional_shares,
                                     sum_of_project_functional_shares,
                                     sum_of_department_functional_shares,
                                     sum_of_job_functional_shares,
                                     total_functional_tickets,
                                     weight,
                                     share_functional_shares);

   DRETURN_VOID;
}

/**
 * @brief Calculate the functional tickets of a pending job, adding its shares to the sums
 *
 * @param ref the pending job's entry; receives the functional shares and tickets
 * @param[in,out] sum_of_user_functional_shares the sum of the user functional shares
 * @param[in,out] sum_of_project_functional_shares the sum of the project functional shares
 * @param[in,out] sum_of_department_functional_shares the sum of the department functional shares
 * @param[in,out] sum_of_job_functional_shares the sum of the job functional shares
 * @param total_functional_tickets the tickets of the functional policy
 * @param weight the weights of the categories
 * @param shared nonzero if the functional shares of an object are shared between its jobs
 */
static void
calc_pending_job_functional_tickets(sge_ref_t *ref,
                                    double *sum_of_user_functional_shares,
                                    double *sum_of_project_functional_shares,
                                    double *sum_of_department_functional_shares,
                                    double *sum_of_job_functional_shares,
                                    const double total_functional_tickets,
                                    double weight[],
                                    const int shared) {
   DENTER(BASIS_LAYER);
   /*
    * If the functional shares of a given object are shared between jobs
    * (schedd_param SHARE_FUNCTIONAL_SHARES=true), then we need to reduce
    * the total number of shares for each category to include the object
    * shares only one time. If the object reference count is greater than
    * one, the object's shares were already included in the total before
    * calc_job_functional_tickets_pass1 was called, so we subtract them
    * out here.
   */
   calc_job_functional_tickets_pass1(ref,
                                     sum_of_user_functional_shares,
                                     sum_of_project_functional_shares,
                                     sum_of_department_functional_shares,
                                     sum_of_job_functional_shares,
                                     shared,
                                     !shared);

   calc_job_functional_tickets_pass2(ref,
                                     *sum_of_user_functional_shares,
                                     *sum_of_project_functional_shares,
                                     *sum_of_department_functional_shares,
                                     *sum_of_job_functional_shares,
                                     total_functional_tickets,
                                     weight,
                                     shared);
   DRETURN_VOID;
}

/**
 * @brief Calculate proportional shares in terms of tickets for all active jobs
 *
 * @param lists all the lists of this scheduling run
 * @param running_jobs all currently executing jobs, may be empty
 * @param finished_jobs all completed jobs, may be empty
 * @param queued_jobs all pending jobs to be scheduled, may be empty
 * @param do_usage nonzero to sum the usage of the running jobs into their users and projects
 * @param[in,out] max_tickets the highest ticket amount of a job, raised to the highest one found here
 * @return the sequence number of this ticket calculation
 */
static int
sge_calc_tickets(scheduler_all_data_t *lists,
                 const ocs::tom::List<ocs::tom::JB> running_jobs,
                 const ocs::tom::List<ocs::tom::JB> finished_jobs,
                 const ocs::tom::List<ocs::tom::JB> queued_jobs,
                 const int do_usage,
                 double *max_tickets) {
   DENTER(TOP_LAYER);

   double sum_of_user_functional_shares = 0,
          sum_of_project_functional_shares = 0,
          sum_of_department_functional_shares = 0,
          sum_of_job_functional_shares = 0,
          sum_of_active_tickets = 0,
          sum_of_pending_tickets = 0,
          sum_of_active_override_tickets = 0;

   uint32_t num_jobs, num_queued_jobs, job_ndx;

   uint32_t num_unenrolled_tasks = 0;

   std::unique_ptr<sge_ref_t[]> job_ref;

   double prof_init=0, prof_pass0=0, prof_pass1=0, prof_pass2=0, prof_calc=0;

   double total_share_tree_tickets = sconf_get_weight_tickets_share();
   double total_functional_tickets = sconf_get_weight_tickets_functional();

   bool share_functional_shares = sconf_get_share_functional_shares();
   uint32_t max_pending_tasks_per_job = sconf_get_max_pending_tasks_per_job();
   uint32_t free_qslots = 0;


   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   all_lists = lists;

   sge_scheduling_run++;

   const auto decay_list = ocs::tom::ListPtr<ocs::tom::UA>(ocs::Usage::get_decay_list());

   /* the configured usage weights decide which usage attributes are
    * relevant for share tree usage and may be booked into the
    * user/project usage lists (CS-1385) */
   const auto usage_weight_list = ocs::tom::ListPtr<ocs::tom::UA>(sconf_get_usage_weight_list());


   /*-------------------------------------------------------------
    * Init job_ref_count in each share tree node to zero
    *-------------------------------------------------------------*/

   const auto root = lists->share_tree->front();
   if (root) {
      sge_init_share_tree_nodes(root);
      ocs::ShareTree::set_node_project_flag(root, *lists->project_list);
   }

   for (const auto qep : ocs::tom::ConstList<ocs::tom::QU>(*lists->queue_list)) {
      free_qslots += MAX(0, ((int)ocs::tom::get<QU_job_slots>(qep)) - qinstance_slots_used(qep));
   }

   /*-----------------------------------------------------------------
    * Create and build job reference array.  The job reference array
    * contains all the references of the jobs to the various objects
    * needed for scheduling. It exists so that the ticket calculation
    * function only has to look up the various objects once.
    *-----------------------------------------------------------------*/

   num_jobs = num_queued_jobs = 0;
   if (queued_jobs) {
      for (const auto job : queued_jobs) {
         uint32_t max = std::min(max_pending_tasks_per_job, free_qslots+1);
         uint32_t num_unenrolled_tasks_for_job = 0;
         uint32_t task_cnt = 0;

         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            if (++task_cnt > max) {
               break;
            }
            if (job_is_active(job, ja_task)) {
               num_queued_jobs++;
               num_jobs++;
            }
         }
         if (!(task_cnt > max)) {
            int space = max - task_cnt;
            int tasks = job_get_not_enrolled_ja_tasks(job);

            num_unenrolled_tasks_for_job = std::min(space, tasks);
            num_unenrolled_tasks += num_unenrolled_tasks_for_job;
            num_queued_jobs += num_unenrolled_tasks_for_job;
            num_jobs += num_unenrolled_tasks_for_job;
         }
      }
   }
   if (running_jobs) {
      for (const auto job : running_jobs) {
         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job))
            if (job_is_active(job, ja_task))
               num_jobs++;
      }
   }
   if (num_jobs > 0) {
      job_ref = std::make_unique<sge_ref_t[]>(num_jobs);
   }
   if (num_unenrolled_tasks > 0) {
      task_ref_initialize_table(num_unenrolled_tasks);
   }

   /*-----------------------------------------------------------------
    * Note: use of the job_ref_t array assumes that no jobs
    * will be removed from the job lists and the job list order will
    * be maintained during the execution of sge_calc_tickets.
    *-----------------------------------------------------------------*/

   job_ndx = 0;

   if (running_jobs) {
      for (const auto job : running_jobs) {
         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            if (!job_is_active(job, ja_task)) {
               sge_clear_ja_task(ja_task);
            } else {
               sge_set_job_refs(job, ja_task, &job_ref[job_ndx], nullptr, lists, 0);
               if (job_ref[job_ndx].node) {
                  ocs::tom::set<STN_job_ref_count>(job_ref[job_ndx].node,
                            ocs::tom::get<STN_job_ref_count>(job_ref[job_ndx].node)+1);
                  ocs::tom::set<STN_active_job_ref_count>(job_ref[job_ndx].node,
                            ocs::tom::get<STN_active_job_ref_count>(job_ref[job_ndx].node)+1);
               }
               job_ndx++;
            }
         }
      }
   }

   if (queued_jobs) {
      uint32_t ja_task_ndx = 0;

      for (const auto job : queued_jobs) {
         uint32_t id;
         uint32_t task_cnt = 0;

         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            sge_ref_t *jref = &job_ref[job_ndx];

            if (++task_cnt > std::min(max_pending_tasks_per_job, free_qslots+1) ||
                !job_is_active(job, ja_task)) {
               sge_clear_ja_task(ja_task);
            } else {
               sge_set_job_refs(job, ja_task, jref, nullptr, lists, 1);
               if (jref->node)
                  ocs::tom::set<STN_job_ref_count>(jref->node,
                            ocs::tom::get<STN_job_ref_count>(jref->node)+1);
               job_ndx++;
            }
         }

         for (const auto range : ocs::tom::get<JB_ja_n_h_ids>(job)) {
            for(id = ocs::tom::get<RN_min>(range);
                id <= ocs::tom::get<RN_max>(range);
                id += ocs::tom::get<RN_step>(range)) {
               sge_ref_t *jref = &job_ref[job_ndx];
               sge_task_ref_t *tref = task_ref_get_entry(ja_task_ndx);

               if (++task_cnt > std::min(max_pending_tasks_per_job, free_qslots+1)) {
                  break;
               }
               const auto ja_task = ocs::tom::Element<ocs::tom::JAT>(job_get_ja_task_template_pending(job, id));
               sge_set_job_refs(job, ja_task, jref, tref, lists, 1);
               if (jref->node) {
                  ocs::tom::set<STN_job_ref_count>(jref->node,
                            ocs::tom::get<STN_job_ref_count>(jref->node)+1);
               }
               ja_task_ndx++;
               job_ndx++;
            }
         }
      }
   }

   num_jobs = job_ndx;

   /*-------------------------------------------------------------
    * Calculate the m_shares in the share tree.  The m_share
    * is a hierarchichal share value used in the "classic
    * scheduling" mode. It is calculated by subdividing the shares
    * at each active level in the share tree.
    *-------------------------------------------------------------*/

   if (root) {
      update_job_ref_count(root);
      update_active_job_ref_count(root);
      ocs::tom::set<STN_m_share>(root, 1.0);
      calculate_m_shares(root);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_init = prof_get_measurement_wallclock(SGE_PROF_SCHEDLIB4, false, nullptr);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   /*-----------------------------------------------------------------
    * PASS 0
    *-----------------------------------------------------------------*/

   DPRINTF("=====================[Pass 0]======================\n");

   for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {

      sge_set_job_cnts(&job_ref[job_ndx], job_ref[job_ndx].queued);

      /*-------------------------------------------------------------
       * Handle usage
       *-------------------------------------------------------------*/

   // @todo CS-272: move this to online usage reporting in report handling
#if 1
      if (do_usage) {
         /* CS-1239: scheduler no longer decays locally - the TET share-tree
          * tick is the sole decay site and ships the decayed UU_/PR_usage
          * to every mirror via sgeE_USER_MOD / sgeE_PROJECT_MOD. PASS 0
          * only sums the in-flight running-job projection onto whatever
          * mirror currently holds. sum_usage = book_user_project_usage
          * with do_decay=false. */
         ocs::Usage::sum_usage(job_ref[job_ndx].job, job_ref[job_ndx].ja_task,
                               job_ref[job_ndx].user, job_ref[job_ndx].project, *usage_weight_list);
      }
#endif

      combine_usage(&job_ref[job_ndx]);
   }

   if (root) {
      /* CS-1239: pass curr_time=0 so the calc_node_usage call inside
       * sge_calc_sharetree_targets does NOT decay UU_/PR_usage locally.
       * Decay is owned by the TET share-tree tick handler which ships
       * decayed values via sgeE_USER_MOD / sgeE_PROJECT_MOD; scheduler
       * just reads what mirror has. seqno is irrelevant when now=0
       * because the per-userprj decay branch is gated on now != 0. */
      sge_calc_sharetree_targets(root, lists, *decay_list, 0, 0);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_pass0 = prof_get_measurement_wallclock(SGE_PROF_SCHEDLIB4,false, nullptr);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   /*-----------------------------------------------------------------
    * PASS 1
    *-----------------------------------------------------------------*/

   DPRINTF("=====================[Pass 1]======================\n");

   for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
      if (job_ref[job_ndx].queued)
         break;

      if (total_share_tree_tickets > 0)
         calc_job_share_tree_tickets_pass1(&job_ref[job_ndx]);

      if (total_functional_tickets > 0)
         calc_job_functional_tickets_pass1(&job_ref[job_ndx],
                                          &sum_of_user_functional_shares,
                                          &sum_of_project_functional_shares,
                                          &sum_of_department_functional_shares,
                                          &sum_of_job_functional_shares,
                                          share_functional_shares, 1);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_pass1= prof_get_measurement_wallclock(SGE_PROF_SCHEDLIB4,false, nullptr);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   /*-----------------------------------------------------------------
    * PASS 2
    *-----------------------------------------------------------------*/

   DPRINTF("=====================[Pass 2]======================\n");
   {
      double weight[k_last];
      bool share_override_tickets = sconf_get_share_override_tickets();

      if(total_functional_tickets > 0) {
         get_functional_weighting_parameters(sum_of_user_functional_shares,
                                       sum_of_project_functional_shares,
                                       sum_of_department_functional_shares,
                                       sum_of_job_functional_shares,
                                       weight);
      }

      for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {

         /* stop, when teh first pending is found */
         if (job_ref[job_ndx].queued) {
            break;
         }

         if (total_share_tree_tickets > 0) {
            calc_job_share_tree_tickets_pass2(&job_ref[job_ndx],
                                           total_share_tree_tickets);
         }

         if (total_functional_tickets > 0) {
            calc_job_functional_tickets_pass2(&job_ref[job_ndx],
                                           sum_of_user_functional_shares,
                                           sum_of_project_functional_shares,
                                           sum_of_department_functional_shares,
                                           sum_of_job_functional_shares,
                                           total_functional_tickets,
                                           weight,
                                           share_functional_shares);
         }

         sum_of_active_override_tickets +=
                  calc_job_override_tickets(&job_ref[job_ndx], share_override_tickets);

         sum_of_active_tickets += calc_job_tickets(&job_ref[job_ndx]);

      }
   }

   /* set scheduler configuration information to go back to GUI */
   if (sconf_is()) {
      sconf_set_weight_tickets_override(sum_of_active_override_tickets);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4);
   prof_pass2 = prof_get_measurement_wallclock(SGE_PROF_SCHEDLIB4,false, nullptr);
   PROF_START_MEASUREMENT(SGE_PROF_SCHEDLIB4);

   /*-----------------------------------------------------------------
    * NEW PENDING JOB TICKET CALCULATIONS
    *-----------------------------------------------------------------*/

   if (queued_jobs) {
      policy_hierarchy_t hierarchy[POLICY_VALUES];
      int policy_ndx;

      sconf_ph_fill_array(hierarchy);
      sconf_ph_print_array(hierarchy);

      for(policy_ndx = 0;
          policy_ndx < POLICY_VALUES;
          policy_ndx++) {

         switch(hierarchy[policy_ndx].policy) {

         case SHARE_TREE_POLICY:
         DPRINTF("calc share tree pending tickets: %.3f\n", total_share_tree_tickets);
      /*-----------------------------------------------------------------
       * Calculate pending share tree tickets
       *
       *    The share tree tickets for pending jobs are calculated
       *    by adding each job to the share tree as leaf nodes
       *    under the associated user/project node. The jobs are
       *    then sorted based on their respective short term
       *    entitlements to ensure that the overall goals of the
       *    share tree are met.
       *
       *-----------------------------------------------------------------*/

      if (total_share_tree_tickets > 0 && root) {

         for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
            sge_ref_t *jref = &job_ref[job_ndx];
            const auto node = jref->node;
            if (jref->queued) {
               if (node) {
                  /* add each job to the share tree as a temporary sibling node */
                  char tmpstr[64];

                  const auto job = jref->job;
                  snprintf(tmpstr, sizeof(tmpstr), sge_u32 "." sge_u32, ocs::tom::get<JB_job_number>(job),
                           REF_GET_JA_TASK_NUMBER(jref));
                  const auto child = ocs::tom::emplace_back_in<STN_children, STN_name>(node, tmpstr);
                  ocs::tom::set<STN_jobid>(child, ocs::tom::get<JB_job_number>(job));
                  ocs::tom::set<STN_taskid>(child, REF_GET_JA_TASK_NUMBER(jref));
                  ocs::tom::set<STN_temp>(child, 1);
                  /* save the job reference, so we can refer to it later to set job fields */
                  ocs::tom::set<STN_ref>(child, job_ndx+1);
                  if (hierarchy[policy_ndx].dependent) {
                     /* set the sort value based on tickets of higher level policy */
                     ocs::tom::set<STN_tickets>(child, jref->tickets);
                     ocs::tom::set<STN_sort>(child,
                                jref->tickets + (0.01 * (double)ocs::tom::get<JB_jobshare>(job)));
                  } else
                     /* set the sort value based on the priority of the job */
                     ocs::tom::set<STN_sort>(child, (double)ocs::tom::get<JB_jobshare>(job));
               }
            }
         }

         const auto sorted_job_node_list = sge_sort_pending_job_nodes(root, root, total_share_tree_tickets);
         if (sorted_job_node_list) {
            /*
             * set share tree tickets of each pending job
             * based on the returned sorted node list
             */
            for (const auto job_node : *sorted_job_node_list) {
               sge_ref_t *jref = &job_ref[ocs::tom::get<STN_ref>(job_node)-1];
               REF_SET_STICKET(jref,
                     ocs::tom::get<STN_shr>(job_node) * total_share_tree_tickets);
               if (hierarchy[policy_ndx].dependent)
                  jref->tickets += REF_GET_STICKET(jref);
            }
         }
      }

            break;

         case FUNCTIONAL_POLICY:

      /*-----------------------------------------------------------------
       * Calculate pending functional tickets
       *
       *    We use a somewhat brute force method where we order the pending
       *    jobs based on which job would get the largest number of
       *    functional tickets if it were to run next. Because of the
       *    overhead, we limit the number of pending jobs that we will
       *    order based on the configurable schedd parameter called
       *    max_functional_jobs_to_schedule. The default is 100.
       *
       *    The algorithm works like:
       *    - order all pending jobs by functional categories. A functional
       *      category consits of: user_shares, dept_shares, project_shares,
       *      and job_shares.
       *    - For all max_functional_jobs_to_schedule compute the functional
       *      ticket for the first job of each fcategory. Our currently used
       *      algorithm computes for all jobs under the same fcategory the
       *      same functional tickets. Therefore we have only to compute
       *      the first job of each fcategory. The job list under each category
       *      has to be sorted by time and previous computed tickets.
       *
       *    - When the job of one functional category is found, increase the
       *      counter for running jobs in the user, dept, proj, and other structures.
       *      Afterwards, compute the real functional tickets.
       *
       *      When this is done, undo all the changes to the different structures
       *      (running job counters) and free the fcategory data-structure.
       *-----------------------------------------------------------------*/
      DPRINTF("calc funktional tickets %.3f\n", total_functional_tickets);
      if (total_functional_tickets > 0 && num_queued_jobs > 0) {
         sge_ref_t **sort_list=nullptr;
         ocs::tom::ListPtr<ocs::tom::FCAT> fcategories;

         uint32_t i=0;

         double weight[k_last];

         sge_ref_list_t *ref_array = nullptr;
         double pending_user_fshares = sum_of_user_functional_shares;
         double pending_proj_fshares = sum_of_project_functional_shares;
         double pending_dept_fshares = sum_of_department_functional_shares;
         double pending_job_fshares = sum_of_job_functional_shares;

         uint32_t max =   sconf_get_max_functional_jobs_to_schedule();
         uint32_t pjobs = build_functional_categories(job_ref.get(), num_jobs, fcategories,
                                     &ref_array, hierarchy[policy_ndx].dependent,
                                     JB_jobshare, UU_fshare, PR_fshare, US_fshare);


         max = std::min(max, pjobs);

         if (max > 0) {
            sort_list = (sge_ref_t **)sge_malloc(max * sizeof(sge_ref_t *));

            if(sort_list == nullptr){
               /* error message to come */

               sge_free(&sort_list);

               if (fcategories != nullptr) {
                  free_fcategories(fcategories, &ref_array);
               }

               DRETURN(sge_scheduling_run);
            }

            get_functional_weighting_parameters(1, 1, 1, 1, weight);

            /* Loop through all the jobs calculating the functional tickets and
               find the job with the most functional tickets.  Move it to the
               top of the list and start the process all over again with the
               remaining jobs. */

            for(i=0; i<max; i++) {
               double ftickets, max_ftickets=-1;
               uint32_t jid, save_jid=0, save_tid=0;
	            uint64_t submission_time = 0, save_submission_time = 0;
               ocs::tom::Element<ocs::tom::FCAT> max_current;

               /* loop over all first jobs from the different categories */
               for (const auto current : *fcategories) {
                  auto *tmp = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(current));
                  sge_ref_t *jref = tmp->ref;
                  double user_fshares = pending_user_fshares + 0.001;
                  double proj_fshares = pending_proj_fshares + 0.001;
                  double dept_fshares = pending_dept_fshares + 0.001;
                  double job_fshares = pending_job_fshares + 0.001;

                  /* calc the tickets */

                  calc_intern_pending_job_functional_tickets(current,
                                                      user_fshares,
                                                      proj_fshares,
                                                      dept_fshares,
                                                      job_fshares,
                                                      total_functional_tickets,
                                                      weight,
                                                      share_functional_shares);

                  ftickets = REF_GET_FTICKET(jref);

                  if (hierarchy[policy_ndx].dependent)
                     ftickets += jref->tickets;

                  /* controll, if the current job has the most tickets */
                  if(ftickets >= max_ftickets){
                     jid = ocs::tom::get<JB_job_number>(jref->job);
                     submission_time = ocs::tom::get<JB_submission_time>(jref->job);
                     if (max_current == nullptr ||
                           (ftickets > max_ftickets) ||
			   (submission_time < save_submission_time) ||
			   (submission_time == save_submission_time && jid < save_jid) ||
                           (jid == save_jid && REF_GET_JA_TASK_NUMBER(jref) < save_tid)) {
                        max_ftickets = ftickets;
                        save_jid = ocs::tom::get<JB_job_number>(jref->job);
                        save_submission_time = ocs::tom::get<JB_submission_time>(jref->job);
                        save_tid = REF_GET_JA_TASK_NUMBER(jref);
                        sort_list[i] = jref;
                        max_current = current;
                     }
                  }
               }

               /* - set the results from first entry of the winning category to the second
                  - remove the winning element
                  - if it was the last element, remove the fcategory*/
               if (max_current != nullptr) {
                  auto *tmp = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(max_current));
                  copy_ftickets (tmp, tmp->next);

                  /* switch to next elem */
                  ocs::tom::set<FCAT_jobrelated_ticket_first>(max_current, tmp->next);

                  /* remove category */
                  if (ocs::tom::get<FCAT_jobrelated_ticket_first>(max_current) == nullptr) {
                     ocs::tom::set<FCAT_jobrelated_ticket_last>(max_current,  nullptr);
                     ocs::tom::set<FCAT_user>(max_current, nullptr);
                     ocs::tom::set<FCAT_dept>(max_current, nullptr);
                     ocs::tom::set<FCAT_project>(max_current, nullptr);

                     fcategories->erase(max_current);
                  }

               /* This is the job with the most functional tickets, consider it active */
               sge_set_job_cnts(sort_list[i], 0);

               /* recompute the results afterwards for the winnnig entry */
               calc_pending_job_functional_tickets(sort_list[i],
                                                   &pending_user_fshares,
                                                   &pending_proj_fshares,
                                                   &pending_dept_fshares,
                                                   &pending_job_fshares,
                                                   total_functional_tickets,
                                                   weight,
                                                   share_functional_shares);
               }
               else { /* we reached an end */
                  break;
               }
            }

            /* Reset the pending jobs to inactive */
            {
               int depend = hierarchy[policy_ndx].dependent;
               for(job_ndx=0; job_ndx < max; job_ndx++) {
                  sge_ref_t *jref = sort_list[job_ndx];
                  sge_unset_job_cnts(jref, 0);

                  /* sum up the tickets, if there are dependencies */
                  if (depend) {
                     jref->tickets += REF_GET_FTICKET(jref);
                  }
               }

               destribute_ftickets(*fcategories, depend);
            }
         }

         /* free the allocated memory */
         sge_free(&sort_list);
         free_fcategories(fcategories, &ref_array);

      }

            break;

         case OVERRIDE_POLICY:
               /*-----------------------------------------------------------------
                * Calculate the pending override tickets
                *-----------------------------------------------------------------*/
               {
                  bool share_override_tickets = sconf_get_share_override_tickets();
                  if (share_override_tickets) {
                     calculate_pending_shared_override_tickets(job_ref.get(), num_jobs,
                                                               hierarchy[policy_ndx].dependent);
                  }
                  else {
                     for(job_ndx=0; job_ndx<num_jobs; job_ndx++) {
                        sge_ref_t *jref = &job_ref[job_ndx];
                        if (jref->queued) {
                           calc_job_override_tickets(jref, share_override_tickets);
                           if (hierarchy[policy_ndx].dependent) {
                              jref->tickets += REF_GET_OTICKET(jref);
                           }
                        }
                     } /* for */
                  }
               } /* case */

            break;

         default:
            break;
         }
      } /* switch */

      /*-----------------------------------------------------------------
       * Combine the ticket values into the total tickets for the job
       *-----------------------------------------------------------------*/

      for (job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         calc_job_tickets(&job_ref[job_ndx]);
      }

      /* calculate the ticket % of each job */

      sum_of_active_tickets = 0;
      sum_of_pending_tickets = 0;
      for (job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         double tickets = REF_GET_TICKET(jref);

         if (*max_tickets < tickets) {
            *max_tickets = tickets;
         }

         if (jref->queued) {
            sum_of_pending_tickets += tickets;
         } else {
            sum_of_active_tickets += tickets;
         }
      }

      for (job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         double ticket_sum = jref->queued ? sum_of_pending_tickets : sum_of_active_tickets;

         if (REF_GET_TICKET(jref) > 0) {
            REF_SET_SHARE(jref, REF_GET_TICKET(jref) / ticket_sum);
         }
         else {
            REF_SET_SHARE(jref, 0);
         }
      }

   }
   else {
      for (job_ndx=0; job_ndx<num_jobs; job_ndx++) {
         sge_ref_t *jref = &job_ref[job_ndx];
         double tickets = REF_GET_TICKET(jref);

         if (*max_tickets < tickets) {
            *max_tickets = tickets;
         }

         if (tickets > 0) {
            REF_SET_SHARE(jref, tickets / sum_of_active_tickets);
         }
         else {
            REF_SET_SHARE(jref, 0);
         }

      }

   }

   // @todo CS-272: should not be required as soon as deletion of job specific
   // usage can be moved from sge_follow_order to report handling
#if 1
   /* update share tree node for finished jobs */
   /* NOTE: what purpose does this serve? */
   if (do_usage && finished_jobs) {
      for (const auto job : finished_jobs) {
         sge_ref_t jref;

         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            jref = sge_ref_t{};
            sge_set_job_refs(job, ja_task, &jref, nullptr, lists, 0);
            if (jref.node) {
               if (sge_scheduling_run != ocs::tom::get<STN_pass2_seqno>(jref.node)) {
                  ocs::ShareTree::zero_node_fields(jref.node, nullptr);
                  ocs::tom::set<STN_pass2_seqno>(jref.node, sge_scheduling_run);
               }
            }
         }
      }
   }
#endif

   /*
    * copy tickets
    *
    * Tickets for unenrolled pending tasks were stored an internal table.
    * Now it is necessary to find the ja_task of a job which got the
    * most tickets. These ticket numbers will be stored in the template
    * element within the job.
    */
   if (queued_jobs != nullptr) {
      sge_task_ref_t *tref = task_ref_get_first_job_entry();

      while (tref != nullptr) {
         const auto job = queued_jobs.find_first<JB_job_number>(tref->job_number);

         if (job) {
            const auto ja_task_template = ocs::tom::get<JB_ja_template>(job).front();

            task_ref_copy_to_ja_task(tref, ja_task_template);
         }
         tref = task_ref_get_next_job_entry();
      }
   }

   if (prof_is_active(SGE_PROF_SCHEDLIB4)){
      prof_stop_measurement(SGE_PROF_SCHEDLIB4, nullptr);
      prof_calc = prof_get_measurement_wallclock(SGE_PROF_SCHEDLIB4, false, nullptr);

      PROFILING("PROF: job ticket calculation: init: %.3f s, pass 0: %.3f s, pass 1: %.3f, pass2: %.3f, calc: %.3f s",
         prof_init, prof_pass0, prof_pass1, prof_pass2, prof_calc);
   }

   DRETURN(sge_scheduling_run);
}


/**
 * @brief Sorted list of the pending job nodes below a share tree node, with the pending priority set in STN_sort
 *
 * The temporary job nodes that sge_calc_tickets() added below @p node are
 * copied into the list and freed in the share tree.
 *
 * @param root the root of the share tree
 * @param node the node whose subtree is collected
 * @param total_share_tree_tickets the tickets of the share tree policy
 * @return the job nodes, sorted; empty if there are none
 */
static ocs::tom::ListPtr<ocs::tom::STN>
sge_sort_pending_job_nodes(const ocs::tom::Element<ocs::tom::STN> root,
                           const ocs::tom::Element<ocs::tom::STN> node,
                           const double total_share_tree_tickets) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::STN> job_node_list;
   double node_stt;
   double job_count=0;
   int job_nodes = 0;

   if(root == node){
      int active_nodes = 0;
      ocs::tom::Element<ocs::tom::STN> temp_root;

      for (const auto child : ocs::tom::get<STN_children>(node)) {
         if (ocs::tom::get<STN_ref>(child)) {
            active_nodes++;
            break;
         } else if ((ocs::tom::get<STN_job_ref_count>(child)-ocs::tom::get<STN_active_job_ref_count>(child))>0) {
            temp_root = child;
            active_nodes ++;
         }
         if (active_nodes >1)
               break;
      }
      if (active_nodes == 1 && temp_root) {
         DRETURN(sge_sort_pending_job_nodes(temp_root, temp_root, total_share_tree_tickets));
      }
   }
   /* get the child job nodes in a single list */

   for (const auto child : ocs::tom::get<STN_children>(node)) {
      if (ocs::tom::get<STN_ref>(child)) {
         /* this is a job node, chain it onto our list */
         if (!job_node_list)
            job_node_list = ocs::tom::ListPtr<ocs::tom::STN>::create("sorted job node list");
         job_node_list->push_back(child.clone());
         job_nodes++;
      } else if ((ocs::tom::get<STN_job_ref_count>(child)-ocs::tom::get<STN_active_job_ref_count>(child))>0) {
         /* recursively get all the child job nodes onto our list */
         auto child_job_node_list = sge_sort_pending_job_nodes(root, child, total_share_tree_tickets);
         if (child_job_node_list) {
            if (job_node_list == nullptr)
               job_node_list = std::move(child_job_node_list);
            else
               job_node_list->splice(std::move(child_job_node_list));
         }
      }
   }
   /* free the temporary job nodes */
   if (job_nodes)
      ocs::tom::set<STN_children>(node, nullptr);

   /* sort the job nodes based on the calculated pending priority */
   if (root != node || job_nodes) {
      ocs::tom::ConstElement<ocs::tom::UA> u;

      if (job_node_list && job_node_list->size()>1)
         job_node_list->sort<ocs::tom::desc<STN_sort>, ocs::tom::asc<STN_jobid>, ocs::tom::asc<STN_taskid>>();

      /* calculate a new pending priority -
         The priority of each pending job associated with this node is the
         node's short term entitlement (STN_stt) divided by the number of jobs
         which are scheduled ahead of this node times the number of share tree
         tickets. If we are dependent on another higher-level policy, we also add
         the tickets from those policies. */

      job_count = ocs::tom::get<STN_active_job_ref_count>(node);
      if ((u=ocs::tom::find_first<UA_name>(ocs::tom::get<STN_usage_list>(node), "finished_jobs")))
         job_count += ocs::tom::get<UA_value>(u);
      node_stt = ocs::tom::get<STN_stt>(node);
      for (const auto job_node : *job_node_list) {
         job_count++;
         ocs::tom::set<STN_shr>(job_node, node_stt / job_count);
         ocs::tom::set<STN_sort>(job_node, ocs::tom::get<STN_tickets>(job_node) +
                    ((node_stt / job_count) * total_share_tree_tickets));
      }

   }

   /* return the new list */

   DRETURN(job_node_list);
}


/**
 * @brief Calculate the targeted proportional share of each node in the share tree
 *
 * @param root the root of the share tree
 * @param lists all the lists of this scheduling run
 * @param decay_list the decay constants of the usage
 * @param curr_time the time to decay the usage to; 0 for no decay
 * @param seqno the sequence number of this calculation
 * @return 0
 */
static int
sge_calc_sharetree_targets(const ocs::tom::Element<ocs::tom::STN> root,
                           scheduler_all_data_t *lists,
                           const ocs::tom::ConstList<ocs::tom::UA> decay_list,
                           const uint64_t curr_time,
                           const u_long seqno) {
   DENTER(TOP_LAYER);

   /* decay and store user and project usage into sharetree nodes */

   ocs::ShareTree::calc_node_usage(root,
                       *lists->user_list,
                       *lists->project_list,
                       decay_list,
                       curr_time,
                       nullptr,
                       seqno);

   /* Calculate targeted proportions for each node */

   sge_calc_node_targets(root, root);

   DRETURN(0);
}


/**
 * @brief Calculates the targeted proportions of the subtree below a node
 *
 * The target is what the node *should* get according to its shares; the
 * difference to what it actually got is what the compensation factor works
 * against.
 *
 * @param[in]     root the root of the share tree
 * @param[in,out] node the node whose subtree is calculated
 *
 * @return 0 on success
 */
int
sge_calc_node_targets(const ocs::tom::ConstElement<ocs::tom::STN> root,
                      const ocs::tom::Element<ocs::tom::STN> node) {
   DENTER(TOP_LAYER);

   double sum_shares, sum, compensation_factor;
   /* char *node_name = lGetString(node, STN_name); */

   /*---------------------------------------------------------------
    * if this is the root node, initialize proportions
    *    ltt - long term targeted % among siblings
    *    oltt - overall long term targeted % among entire share tree
    *    stt - short term targeted % among siblings
    *    ostt - overall short term targeted % among entire share tree
    *---------------------------------------------------------------*/

   if (node == root) {
      ocs::tom::set<STN_stt>(node, 1.0);
      ocs::tom::set<STN_ltt>(node, 1.0);
      ocs::tom::set<STN_ostt>(node, 1.0);
      ocs::tom::set<STN_oltt>(node, 1.0);
   }

   /*---------------------------------------------------------------
    * if this node has no children, there's nothing to do
    *---------------------------------------------------------------*/

   const auto children = ocs::tom::get<STN_children>(node);
   if (children.empty()) {
      DRETURN(0);
   }

/*
   do for each active sub-node
      sum_shares += subnode.shares
      subnode.shr = subnode.shares * subnode.shares / subnode.usage
      sum += subnode.shr
   end do
*/

   /*---------------------------------------------------------------
    * calculate the shr value of each child node
    * calculate the short term targeted proportions
    *---------------------------------------------------------------*/

   sum_shares = 0;
   for (const auto child : children) {
      if (ocs::tom::get<STN_job_ref_count>(child)>0) {
         sum_shares += ocs::tom::get<STN_shares>(child);
      }
   }

   sum = 0;
   for (const auto child : children) {
      if (ocs::tom::get<STN_job_ref_count>(child)>0) {
         double shares, shr, ltt, oltt;
         shares = ocs::tom::get<STN_shares>(child);
         if (sum_shares > 0) {
            ltt = shares / (double)sum_shares;
         } else {
            ltt = 0;
         }
         oltt = ocs::tom::get<STN_oltt>(node) * ltt;
         ocs::tom::set<STN_ltt>(child, ltt);
         ocs::tom::set<STN_oltt>(child, oltt);
         if (oltt > 0) {
            shr = shares * shares /
                MAX(ocs::tom::get<STN_combined_usage>(child), SGE_MIN_USAGE*oltt);
         } else {
            shr = 0;
         }
         ocs::tom::set<STN_shr>(child, shr);
         sum += shr;
      }
   }

/*
   do for each active sub-node
      subnode.ltt = subnode.shares / sum_shares;
      subnode.oltt = node.oltt * subnode.ltt;
      subnode.stt = subnode.shr / sum;
      subnode.ostt = node.stt * subnode.stt;
   end do
*/

   /*---------------------------------------------------------------
    * calculate the short term targeted proportions
    *---------------------------------------------------------------*/

   for (const auto child : children) {
      if (ocs::tom::get<STN_job_ref_count>(child)>0) {
         double stt;

         if (sum > 0) {
            stt = ocs::tom::get<STN_shr>(child) / sum;
         } else {
            stt = 0;
         }

         ocs::tom::set<STN_stt>(child, stt);
         ocs::tom::set<STN_ostt>(child, ocs::tom::get<STN_ostt>(node) * stt);
         ocs::tom::set<STN_adjusted_current_proportion>(child, ocs::tom::get<STN_ostt>(child));
      }
   }

/* psydo code:

   if compensation_factor != 0

      sum = 0
      do for each active sub-node
         if subnode.ostt > (compensation_factor * subnode.oltt)
            subnode.shr = subnode.shares * subnode.shares /
                  (subnode.usage * (subnode.ostt / (compensation_factor * subnode.oltt)))
            sum += subnode.shr
         endif
      enddo

      do for each active sub-node
         subnode.stt = subnode.shr / sum;
         subnode.ostt = node.stt * subnode.stt;
      end do

   endif
*/

   /*---------------------------------------------------------------
    * adjust targeted proportion based on compensation factor
    *---------------------------------------------------------------*/

   compensation_factor = sconf_get_compensation_factor();

   if (compensation_factor != 0) {
      u_long compensation = 0;

      /*---------------------------------------------------------------
       * recalculate the shr value of each child node based on compensation factor
       *---------------------------------------------------------------*/

      sum = 0;
      for (const auto child : children) {
         if (ocs::tom::get<STN_job_ref_count>(child)>0) {
            double ostt = ocs::tom::get<STN_ostt>(child);
            double oltt = ocs::tom::get<STN_oltt>(child);
            if (ostt > (compensation_factor * oltt)) {
               double shares = ocs::tom::get<STN_shares>(child);
               double shr = shares * shares /
                     (MAX(ocs::tom::get<STN_combined_usage>(child), SGE_MIN_USAGE*oltt) *
                     (ostt / (compensation_factor * oltt)));
               ocs::tom::set<STN_shr>(child, shr);
               compensation = 1;
            }
            sum += ocs::tom::get<STN_shr>(child);
         }
      }

      /*---------------------------------------------------------------
       * reset short term targeted proportions (if necessary)
       *---------------------------------------------------------------*/

      if (compensation) {
         for (const auto child : children) {
            if (ocs::tom::get<STN_job_ref_count>(child)>0) {
               double stt = ocs::tom::get<STN_shr>(child) / sum;
               ocs::tom::set<STN_stt>(child, stt);
               ocs::tom::set<STN_ostt>(child, ocs::tom::get<STN_ostt>(node) * stt);
               ocs::tom::set<STN_adjusted_current_proportion>(child, ocs::tom::get<STN_ostt>(child));
            }
         }
      }

   }

/*
   do for each active sub-node
      call sge_calc_node_targets(subnode, lists);
   end do
*/

   /*---------------------------------------------------------------
    * recursively call self for all active child nodes
    *---------------------------------------------------------------*/

   for (const auto child : children) {
      if (ocs::tom::get<STN_job_ref_count>(child)>0) {
         sge_calc_node_targets(root, child);
      }
   }

   DRETURN(0);
}

/**
 * @brief Build orders for updating qmaster
 *
 * Builds generates the orderlist for sending the scheduling decisions
 * to the qmaster. The following orders are generated:
 * - running job tickets (ORT_tickets)
 * - pending job tickets (ORT_ptickets / ORT_clear_pri_info)
 * - update scheduler configuration order (ORT_sched_conf)
 * Most orders are generated by using the sge_create_orders function.
 *
 * @param lists all the lists of this scheduling run
 * @param running_jobs list of running jobs
 * @param queued_jobs list of queued jobs (should be sorted by tickets)
 * @param finished_jobs list of finished jobs, unused
 * @param orders existing order list; the new orders are added to it
 * @param update_usage_and_configuration if true, the update usage orders are generated
 * @param seqno a seqno, changed with each scheduling run
 * @param update_execd if true, the queue information is sent along with the
 *                     running job tickets
 *
 * @note Whether the pending tickets are reported to qmaster is not a
 *       parameter - it is read from the scheduler configuration inside, see
 *       `sconf_get_report_pjob_tickets()`.
 */
void
sge_build_sgeee_orders(scheduler_all_data_t *lists, const ocs::tom::List<ocs::tom::JB> running_jobs,
                       const ocs::tom::List<ocs::tom::JB> queued_jobs,
                       [[maybe_unused]] const ocs::tom::List<ocs::tom::JB> finished_jobs, order_t *orders,
                       const bool update_usage_and_configuration, const int seqno, const bool update_execd) {
   DENTER(TOP_LAYER);

   int norders = 0;
   uint32_t max_pending_tasks_per_job = sconf_get_max_pending_tasks_per_job();

   bool max_queued_ticket_orders = sconf_get_report_pjob_tickets();

   if (orders->pendingOrderList == nullptr) {
      orders->pendingOrderList = ocs::tom::ListPtr<ocs::tom::OR>::create("orderlist").release();
   }

   auto order_list = ocs::tom::List<ocs::tom::OR>(orders->pendingOrderList);

   /*-----------------------------------------------------------------
    * build ticket orders for running jobs
    *-----------------------------------------------------------------*/

   if (running_jobs) {
      norders = static_cast<int>(order_list.size());

      DPRINTF("   got %d running jobs\n", static_cast<int>(running_jobs.size()));

      for (const auto job : running_jobs) {
         const char *pe_str = nullptr;
         ocs::tom::ConstList<ocs::tom::JG> granted = nullptr;
         ocs::tom::ConstElement<ocs::tom::PE> pe;

         for (ocs::tom::Element<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            if ((pe_str=ocs::tom::get<JAT_granted_pe>(ja_task))
                  && (pe = ocs::tom::ConstElement<ocs::tom::PE>(pe_list_locate(*all_lists->pe_list, pe_str)))
                  && ocs::tom::get<PE_control_slaves>(pe)) {

               granted=ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task);
            }
            else {
               granted=nullptr;
            }

            order_list = ocs::tom::List<ocs::tom::OR>(sge_create_orders(order_list, ORT_tickets, job, ja_task,
                                                                        granted, update_execd));
         }
      }
      DPRINTF("   added %d ticket orders for running jobs\n",
               static_cast<int>(order_list.size()) - norders);
   }


/*
 * check for the amount of pending ticket orders should be checked.
 * If the report_pjob_ticktes has changed to false, we need to send
 * ORT_clear_pri_info to the qmaster to remove the pticket values.
 * This prevents qstat from reporting wrong pticket values. Its only done
 * once after the config change.
 */
   if ((queued_jobs != nullptr) && (max_queued_ticket_orders || sconf_is_new_config())) {
      uint32_t free_qslots = 0;
      norders = static_cast<int>(order_list.size());
      for (const auto qep : ocs::tom::ConstList<ocs::tom::QU>(*lists->queue_list)) {
         free_qslots += MAX(0, ((int)ocs::tom::get<QU_job_slots>(qep) - qinstance_slots_used(qep)));
      }

      for (const auto job : queued_jobs) {
         uint32_t tasks = 0;
         if (max_queued_ticket_orders) {

            /* NOTE: only send pending tickets for the first sub-task */
            /* when the first sub-task gets scheduled, then the other sub-tasks didn't have
               any tickets specified */
            for (ocs::tom::Element<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
               if (++tasks > std::min(max_pending_tasks_per_job, free_qslots+1)) {
                  break;
               }
               order_list = ocs::tom::List<ocs::tom::OR>(sge_create_orders(order_list, ORT_ptickets, job, ja_task,
                                                                           ocs::tom::ConstList<ocs::tom::JG>(),
                                                                           false));
            }

            if (job_get_not_enrolled_ja_tasks(job) > 0) {
               const auto task_template = ocs::tom::get<JB_ja_template>(job).front();
               if (task_template) {
                  order_list = ocs::tom::List<ocs::tom::OR>(sge_create_orders(order_list, ORT_ptickets, job,
                                                                              task_template,
                                                                              ocs::tom::ConstList<ocs::tom::JG>(),
                                                                              false));
               }
            }
         } else {
            order_list = ocs::tom::List<ocs::tom::OR>(sge_create_orders(order_list, ORT_clear_pri_info, job,
                                                                        ocs::tom::ConstElement<ocs::tom::JAT>(),
                                                                        ocs::tom::ConstList<ocs::tom::JG>(),
                                                                        false));
         }
      }
      DPRINTF("   added %d ticket orders for queued jobs\n",
               static_cast<int>(order_list.size()) - norders);

   }

   if (update_usage_and_configuration) {

      /*-----------------------------------------------------------------
       * build update scheduler configuration order
       *-----------------------------------------------------------------*/

      if (sconf_is()) {
         // built once and never released, so every use only takes a reference
         static const lDescr *const schedConfDesc = ocs_cull_descr_reduce(SC_Type, {SC_weight_tickets_override});

         // node and up_list stay raw: their reduced descriptor does not start with the first SC field, so no
         // typed handle accepts them; the order takes over up_list (and node with it)
         lListElem *node = lCreateElem(schedConfDesc);
         lList *up_list = lCreateList("sched_conf_update", schedConfDesc);

         lAppendElem(up_list, node);

         ocs::tom::set<SC_weight_tickets_override>(node,
                   sconf_get_weight_tickets_override());

         auto order = ocs::tom::ElementPtr<ocs::tom::OR>::create();
         ocs::tom::set<OR_type>(order, ORT_sched_conf);
         ocs::tom::set<OR_joker>(order, up_list);
         order_list.push_back(std::move(order));

         DPRINTF("   added 1 order for scheduler configuration\n");

      }
      last_seqno = seqno;
   }

   DRETURN_VOID;
}

/*--------------------------------------------------------------------
 * sge_scheduler
 *--------------------------------------------------------------------*/


/**
 * @brief Calc tickets, send orders, and sort job list
 *
 * - calculates the running and pending job tickets.
 * - send the orders to the qmaster about the job tickets
 * - order the pending job list according the the job tickets
 * On a "normal" scheduling interval:
 * - calculate tickets for new and running jobs
 * - don't decay and sum usage
 * - don't update qmaster
 * On a SGEEE scheduling interval:
 * - calculate tickets for new and running jobs
 * - decay and sum usage
 * - handle finished jobs
 * - update qmaster
 *
 * @param lists a ref to all lists in this scheduler
 * @param running_jobs a list of all running jobs
 * @param finished_jobs a list of all finished jobs, unused
 * @param pending_jobs a list of all pending jobs
 * @param orders the orders produced by this run are added here
 *
 * @return 0 if everthing went fine, -1 if not
 */
int
sgeee_scheduler(scheduler_all_data_t *lists,
                const ocs::tom::List<ocs::tom::JB> running_jobs,
                [[maybe_unused]] const ocs::tom::List<ocs::tom::JB> finished_jobs,
                const ocs::tom::List<ocs::tom::JB> pending_jobs,
                order_t *orders) {
   DENTER(TOP_LAYER);

   uint64_t now = sge_get_gmt64();
   int seqno;
   double min_tix  = 0;
   double max_tix  = -1;

   bool report_priority = sconf_get_report_pjob_tickets();
   bool do_nurg, do_nprio;

   /* skip computation of ntix, nurg and nprio
      but only if it is irrelevant and not monitored */
   do_nurg  = (sconf_get_weight_urgency() != 0  || report_priority) ? true : false;
   do_nprio = (sconf_get_weight_priority() != 0 || report_priority) ? true : false;

   /* clear SGEEE fields for queued jobs */
   for (const auto job : pending_jobs) {
      sge_clear_job(job, false);
   }
   for (const auto job : running_jobs) {
      sge_clear_job(job, false);
   }

   /* calculate per job static urgency values */
   if (do_nurg) {
      PROF_START_MEASUREMENT(SGE_PROF_CUSTOM3);
      sge_do_urgency(now, pending_jobs, running_jobs, lists);

      if (prof_is_active(SGE_PROF_CUSTOM3)) {
         prof_stop_measurement(SGE_PROF_CUSTOM3, nullptr);

         PROFILING("PROF: static urgency took %.3f s", prof_get_measurement_wallclock(SGE_PROF_CUSTOM3, false, nullptr));
      }

   }

   min_tix = 0;
   max_tix = -1;

   /* calculate tickets for pending jobs and compute their shares.
    * CS-1239: finished-job booking moved to worker thread - pass nullptr so
    * the (now inert) finished-jobs branch in sge_calc_tickets is skipped. */
   sge_calc_tickets(lists, running_jobs, nullptr, pending_jobs, 1, &max_tix);

   /* correct the shares for the running jobs */
   seqno = sge_calc_tickets(lists, running_jobs, nullptr, nullptr, 0, &max_tix);

   if (max_tix == -1) { /* we have no running jobs and the tickets are disabled. */
      max_tix = 0;
   }

   PROF_START_MEASUREMENT(SGE_PROF_CUSTOM3);

   /* use min/max tix for normalizing
      - now to determine initial normalized ticket amount
      - but also later on after tickets recomputation when a job was assigned */
   tix_range_set(min_tix, max_tix);


#ifdef DEBUG_TASK_REF
   task_ref_print_table();
#endif

   /*
    * now combine ticket amount and static urgency scheme into
    * absolute priority
    */
   DPRINTF("Normalizing tickets using %f/%f as min_tix/max_tix\n", min_tix, max_tix);
   sge_do_sgeee_priority(running_jobs, min_tix, max_tix, do_nprio, do_nurg);
   sge_do_sgeee_priority(pending_jobs, min_tix, max_tix, do_nprio, do_nurg);

   if (prof_is_active(SGE_PROF_CUSTOM3)) {
      prof_stop_measurement(SGE_PROF_CUSTOM3, nullptr);

      PROFILING("PROF: normalizing job tickets took %.3f s", prof_get_measurement_wallclock(SGE_PROF_CUSTOM3, false, nullptr));
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SCHEDLIB4)

   /* somebody might have played with the system clock. */
   if (now < past) {
      past = now;
   }

   {
      uint64_t reprioritize_interval = sge_gmt32_to_gmt64(sconf_get_reprioritize_interval());
      bool update_execd = (reprioritize_interval == 0 || (now >= (past + reprioritize_interval))) ? true : false;
      if (update_execd){
         past = now;
      }
      /* we are not calculation pending tickets here, only the running, finished jobs,
         and the sharetree changes are processed. The pending tickets are calculated at the
         end of the scheduler cycle.
         CS-1239: finished-job booking + delete order moved to worker thread - pass
         empty handles so the (now inert) finished-jobs branch is skipped. */
      sge_build_sgeee_orders(lists, running_jobs, ocs::tom::List<ocs::tom::JB>(), ocs::tom::List<ocs::tom::JB>(),
                             orders, true, seqno, update_execd);
   }

   if (prof_is_active(SGE_PROF_SCHEDLIB4)) {
      prof_stop_measurement(SGE_PROF_SCHEDLIB4, nullptr);

      PROFILING("PROF: create active job orders: %.3f s", prof_get_measurement_wallclock(SGE_PROF_SCHEDLIB4, false, nullptr));
   }

   DRETURN(0);
}

/**
 * @brief Determine GEEE priority for a list of jobs
 *
 * Determines for a list of jobs the GEEE priority. Prior
 * sge_do_sgeee_priority() can be called the normalized urgency value must
 * already be known for each job. The ticket range passed is used for
 * normalizing ticket amount.
 *
 * @param job_list The job list
 * @param min_tix Minumum ticket amount
 * @param max_tix Maximum ticket amount
 * @param do_nprio Needs norm. priority be determined
 * @param do_nurg Needs norm. urgency be determined
 *
 * @note MT-NOTE: sge_do_sgeee_priority() is MT safe
 */
static void
sge_do_sgeee_priority(const ocs::tom::List<ocs::tom::JB> job_list, const double min_tix, const double max_tix,
                      const bool do_nprio, const bool do_nurg) {
   DENTER(TOP_LAYER);

   uint32_t jobid;
   bool enrolled;
   double nsu = 0.5, npri = 0.5;

   for (const auto job : job_list) {
      jobid = ocs::tom::get<JB_job_number>(job);

      if (do_nurg)
         nsu = ocs::tom::get<JB_nurg>(job);
      if (do_nprio)
         npri = ocs::tom::get<JB_nppri>(job);

      enrolled = false;

      for (ocs::tom::Element<ocs::tom::JAT> task : ocs::tom::get<JB_ja_tasks>(job)) {
         sgeee_priority(task, jobid, nsu, npri, min_tix, max_tix);
         enrolled = true;
      }

      if (!enrolled) {
         const auto task = ocs::tom::get<JB_ja_template>(job).front();
         sgeee_priority(task, jobid, nsu, npri, min_tix, max_tix);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Compute final GEEE priority
 *
 * The GEEE priority is computed for the task based on the already known
 * ticket amount and already normalized urgency value. The ticket amount
 * is normalized based on the ticket range passed. The weights for
 * ticket and urgency value are applied.
 *
 * @param task The task whose priority is computed
 * @param jobid The jobs id, only for the disabled debug output
 * @param nsu The normalized urgency value that applies to all tasks of the job.
 * @param npri The normalized POSIX priority that applies to all tasks of the job.
 * @param min_tix minimum ticket amount
 * @param max_tix maximum ticket amount
 *
 * @note MT-NOTE: sgeee_priority() is MT safe
 */
static void
sgeee_priority(const ocs::tom::Element<ocs::tom::JAT> task, [[maybe_unused]] const uint32_t jobid, const double nsu,
               const double npri, const double min_tix, const double max_tix) {
   DENTER(TOP_LAYER);

   double nta, geee_priority;
   double weight_ticket = 0.0;
   double weight_urgency = 0.0;
   double weight_priority = 0.0;

   sconf_get_weight_ticket_urgency_priority(&weight_ticket, &weight_urgency, &weight_priority);

   /* now compute normalized ticket amount (NTA) for each job/task */
   nta = sge_normalize_value(ocs::tom::get<JAT_tix>(task), min_tix, max_tix);
   ocs::tom::set<JAT_ntix>(task, nta);

   /* combine per task NTA with per job normalized static urgency
      (NSU) and per job normalized POSIX priority (NPRI) into
      SGEEE priority */
   geee_priority = weight_urgency * nsu + weight_ticket * nta +
                   weight_priority * npri;
/*
   DPRINTF("SGEEE priority (" sge_u32 "." sge_u32 ") %f = %f * %f + %f * %f + %f * %f\n",
      jobid, ocs::tom::get<JAT_task_number>(task), geee_priority,
      weight_urgency, nsu, weight_ticket, nta, weight_priority, npri);
*/
   ocs::tom::set<JAT_ntix>(task, nta);
   ocs::tom::set<JAT_prio>(task, geee_priority);

   DRETURN_VOID;
}


/**
 * @brief Calculate shared override tickets
 *
 * We calculate the override tickets for pending jobs, which are shared. The basic
 * algorithm looks like this:
 * do for each pending job
 *    do for each pending job which isn't yet considered active
 *          consider the job active
 *          calculate override tickets for that job
 *          consider the job not active
 *      end do
 *      consider the job with the highest priority (taking into account all previous polices + override tickets) as active
 * end do
 * set all pending jobs none active
 * Since this algorithm is very expensive, we split all pending jobs into fcategories. The algorithm changes to:
 * max_jobs = build fcategories and ignore jobs, which would get 0 override tickets
 * do for max_jobs pending job
 *    do for each fcategory
 *       take take first job from category
 *       consider the job active
 *       calculate override tickets for that job
 *       consider the job not active
 *       store job with the most override tickets = job_max
 *    end do
 *    set job_max active and remove it from its fcategory.
 *    remove job_max fcategory, if job_max was the last job
 * end;
 * set all pending jobs none active
 * That's it. It is very simillar to the functional ticket calculation, except, that we are working with tickts and
 * not with shares.
 *
 * @param job_ref an array of job structures (first running, than pennding)
 * @param num_jobs number of jobs in the array
 * @param dependent do other ticket policies depend on this one?
 *
 * @note MT-NOTE: calculate_pending_shared_override_tickets() is MT safe
 */
static void
calculate_pending_shared_override_tickets(sge_ref_t *job_ref, const uint32_t num_jobs, const int dependent) {
         DENTER(TOP_LAYER);

         ocs::tom::ListPtr<ocs::tom::FCAT> fcategories;
         sge_ref_list_t *ref_array = nullptr;
         sge_ref_t **sort_list=nullptr;
         uint32_t i;
         uint32_t max;
         uint32_t job_ndx;

         max = build_functional_categories(job_ref, num_jobs, fcategories, &ref_array, dependent,
                                     JB_override_tickets, UU_oticket, PR_oticket, US_oticket);

         if (max > 0) {
            sort_list = (sge_ref_t **)sge_malloc(max * sizeof(sge_ref_t *));

            if (sort_list == nullptr) {
               /* error message to come */

               sge_free(&sort_list);

               if (fcategories != nullptr) {
                  free_fcategories(fcategories, &ref_array);
               }
               DRETURN_VOID;
            }

            for (i = 0; i < max; i++) {
               double max_otickets=-1;
               uint32_t jid, tid, max_jid=0, max_tid=0;
               ocs::tom::Element<ocs::tom::FCAT> max_current;
               /* loop over all first jobs from the different categories */
               for (const auto current : *fcategories) {
                  auto *tmp = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(current));
                  sge_ref_t *jref = tmp->ref;
                  double otickets;

                  otickets = calc_pjob_override_tickets_shared(jref);
                  jid = ocs::tom::get<JB_job_number>(jref->job);
                  tid = REF_GET_JA_TASK_NUMBER(jref);

                  /* get the job with the max otickets */
                  if ((max_otickets < otickets) ||
                      (max_current == nullptr)) {
                     max_current = current;
                     max_otickets = otickets;
                     max_jid = jid;
                     max_tid = tid;
                     sort_list[i] = jref;
                  }
                  else if (max_otickets == otickets) {
                     if ((max_jid > jid) ||
                        ((max_jid == jid) && (max_tid > tid))) {
                           max_current = current;
                           max_otickets = otickets;
                           max_jid = jid;
                           max_tid = tid;
                           sort_list[i] = jref;
                     }
                  }
               }

               if (max_current != nullptr) {
                  auto *tmp = static_cast<sge_ref_list_t *>(ocs::tom::get<FCAT_jobrelated_ticket_first>(max_current));
                  sge_set_job_cnts(tmp->ref, 0); /* set job active */
                  ocs::tom::set<FCAT_jobrelated_ticket_first>(max_current, tmp->next);/* next job in that category */
                  sort_list[i]->tickets += max_otickets;

                  /* remove category */
                  if(ocs::tom::get<FCAT_jobrelated_ticket_first>(max_current) == nullptr){
                     ocs::tom::set<FCAT_jobrelated_ticket_last>(max_current,  nullptr);
                     ocs::tom::set<FCAT_user>(max_current, nullptr);
                     ocs::tom::set<FCAT_dept>(max_current, nullptr);
                     ocs::tom::set<FCAT_project>(max_current, nullptr);

                     fcategories->erase(max_current);
                  }
               }
               else { /* we reached an end. */
                  break;
               }
            }

            /* set pending jobs to be not active */
            for(job_ndx=0; job_ndx < max; job_ndx++) {
               sge_unset_job_cnts(sort_list[job_ndx], 0);
            }
            sge_free(&sort_list);
         }
         /* free the allocated memory */

         free_fcategories(fcategories, &ref_array);

   DRETURN_VOID;
}


#ifdef MODULE_TEST

int main(int argc, char **argv) {
   int job_number;
   sge_Sdescr_t *lists;
   lList *child_list = nullptr;
   lList *group_list = nullptr;
   lListElem *job, *config, *node;

   lListElem *ep;

   lists = (sge_Sdescr_t *)sge_malloc(sizeof(sge_Sdescr_t));
   memset(lists, 0, sizeof(sge_Sdescr_t));


#ifdef __SGE_COMPILE_WITH_GETTEXT__
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(nullptr,nullptr);
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */


   lInit(nmv);

   /* build host list */
   lAddElemHost(&(lists->host_list), EH_name, "racerx", EH_Type);
   lAddElemHost(&(lists->host_list), EH_name, "yosemite_sam", EH_Type);
   lAddElemHost(&(lists->host_list), EH_name, "fritz", EH_Type);

   /* build queue list */
   ep = lAddElemStr(&(lists->all_queue_list), QU_full_name, "racerx.q", QU_Type);
   lSetHost(ep, QU_qhostname, "racerx");

   ep = lAddElemStr(&(lists->all_queue_list), QU_full_name, "yosemite_sam.q", QU_Type);
   lSetHost(ep, QU_qhostname, "yosemite_sam");

   ep = lAddElemStr(&(lists->all_queue_list), QU_full_name, "fritz.q", QU_Type);
   lSetHost(ep, QU_qhostname, "fritz");

   /* build configuration */

   config = lCreateElem(SC_Type);
   lSetUlong(config, SC_halftime, 60*60);
   lSetList(config, SC_usage_weight_list,
       build_usage_list("usageweightlist", nullptr));
   for_each_ep_lv(usage, lGetList(config, SC_usage_weight_list))
      lSetDouble(usage, UA_value, 1.0/3.0);
   lSetDouble(config, SC_compensation_factor, 2);
   lSetDouble(config, SC_weight_user, 0.25);
   lSetDouble(config, SC_weight_project, 0.25);
   lSetDouble(config, SC_weight_job, 0.25);
   lSetDouble(config, SC_weight_department, 0.25);
   lSetUlong(config, SC_weight_tickets_functional, 10000);
   lSetUlong(config, SC_weight_tickets_share, 10000);
   lists->config_list = lCreateList("config_list", SC_Type);
   lAppendElem(lists->config_list, config);

   /* build user list */

   ep = lAddElemStr(&(lists->user_list), UU_name, "davidson", UU_Type);
   lSetUlong(ep, UU_oticket, 0);
   lSetUlong(ep, UU_fshare, 200);

   ep = lAddElemStr(&(lists->user_list), UU_name, "garrenp", UU_Type);
   lSetUlong(ep, UU_oticket, 0);
   lSetUlong(ep, UU_fshare, 100);

   ep = lAddElemStr(&(lists->user_list), UU_name, "stair", UU_Type);
   lSetUlong(ep, UU_oticket, 0);
   lSetUlong(ep, UU_fshare, 100);

   /* build project list */

   ep = lAddElemStr(&(lists->project_list), PR_name, "sgeee", PR_Type);
   lSetUlong(ep, PR_oticket, 0);
   lSetUlong(ep, PR_fshare, 200);

   ep = lAddElemStr(&(lists->project_list), PR_name, "ms", PR_Type);
   lSetUlong(ep, PR_oticket, 0);
   lSetUlong(ep, PR_fshare, 100);


   /* build department list */

   ep = lAddElemStr(&(lists->dept_list), US_name, "software", US_Type);
   lSetUlong(ep, US_oticket, 0);
   lSetUlong(ep, US_fshare, 200);

   ep = lAddElemStr(&(lists->dept_list), US_name, "hardware", US_Type);
   lSetUlong(ep, US_oticket, 0);
   lSetUlong(ep, US_fshare, 100);

   /* build share tree list */

   child_list = lCreateList("childlist", STN_Type);

   group_list = lCreateList("grouplist", STN_Type);

   node = lAddElemStr(&child_list, STN_name, "davidson", STN_Type);
   lSetUlong(node, STN_shares, 100);

   node = lAddElemStr(&child_list, STN_name, "garrenp", STN_Type);
   lSetUlong(node, STN_shares, 100);

   node = lAddElemStr(&group_list, STN_name, "groupA", STN_Type);
   lSetUlong(node, STN_shares, 1000);
   lSetList(node, STN_children, child_list);


   child_list = lCreateList("childlist", STN_Type);

   node = lAddElemStr(&child_list, STN_name, "stair", STN_Type);
   lSetUlong(node, STN_shares, 100);

   node = lAddElemStr(&group_list, STN_name, "groupB", STN_Type);
   lSetUlong(node, STN_shares, 100);
   lSetList(node, STN_children, child_list);

   node = lAddElemStr(&(lists->share_tree), STN_name, "root", STN_Type);
   lSetUlong(node, STN_type, STT_USER);
   lSetList(node, STN_children, group_list);

   /* build job list */

   job_number = 1;


   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++,
                           JB_Type);
   lSetUlong(job, JB_jobshare, 0);
   lSetString(job, JB_owner, "davidson");
   lSetString(job, JB_project, "sgeee");
   lSetString(job, JB_department, "software");
   lSetUlong64(job, JB_submission_time, sge_get_gmt64() - sge_gmt32_to_gmt64(60*2));
   lSetUlong64(job, JB_deadline, 0);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_uJB_ownersage_list("jobusagelist", nullptr));
   for_each_ep_lv(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, rand());

   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++,
                           JB_Type);
   lSetUlong(job, JB_jobshare, 1000);
   lSetString(job, JB_owner, "davidson");
   lSetString(job, JB_project, "ms");
   lSetString(job, JB_department, "software");
   lSetUlong64(job, JB_submission_time, sge_get_gmt64() - sge_gmt32_to_64(60*2));
   lSetUlong64(job, JB_deadline, 0);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", nullptr));
   for_each_ep_lv(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, rand());

   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++,
                           JB_Type);
   lSetUlong(job, JB_job_number, job_number++);
   lSetUlong(job, JB_jobshare, 0);
   lSetString(job, JB_owner, "stair");
   lSetString(job, JB_project, "sgeee");
   lSetString(job, JB_department, "hardware");
   lSetUlong64(job, JB_submission_time, sge_get_gmt64() - sge_gmt32_to_gmt64(60*2));
   lSetUlong64(job, JB_deadline, 0);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", nullptr));
   for_each_ep_lv(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, rand());

   job = lAddElemUlong(&(lists->job_list), JB_job_number, job_number++,
                           JB_Type);
   lSetUlong(job, JB_jobshare, 0);
   lSetString(job, JB_owner, "garrenp");
   lSetString(job, JB_project, "sgeee");
   lSetString(job, JB_department, "software");
   lSetUlong64(job, JB_submission_time, sge_get_gmt64() - sge_gmt32_to_gmt64(60*2));
   lSetUlong64(job, JB_deadline, sge_get_gmt64() + 60*2 * 1000000);
   lSetHost(job, JB_host, "racerx");
   lSetUlong(job, JB_override_tickets, 0);
   lSetList(job, JB_scaled_usage_list, build_usage_list("jobusagelist", nullptr));
   for_each_ep_lv(usage, lGetList(job, JB_scaled_usage_list))
      lSetDouble(usage, UA_value, rand());


   /* call the SGEEE scheduler */

   sge_setup_lists(lists, lists->job_list, nullptr);

   sge_calc_tickets(lists, lists->job_list, nullptr, nullptr);

   sge_calc_tickets(lists, lists->job_list, nullptr, nullptr);

/*   lWriteListTo(lists->job_list, stdout);*/

   sge_calc_share_tree_proportions( lists->share_tree,
                                    lists->user_list,
                                    lists->project_list,
                                    lists->config_list, sge_get_gmt64());

/*   lWriteListTo(lists->share_tree, stdout);*/

   return 0;
}

#endif


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/


