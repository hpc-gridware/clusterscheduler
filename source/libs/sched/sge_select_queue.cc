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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cfloat>
#include <climits>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_userset.h"

#include "basis_types.h"
#include "schedd_message.h"
#include "schedd_monitor.h"
#include "sge_complex_schedd.h"
#include "sge_pe_schedd.h"
#include "sge_qeti.h"
#include "sge_resource_quota_schedd.h"
#include "sge_resource_utilization.h"
#include "sge_schedd_text.h"
#include "sge_select_queue.h"
#include "uti/sge.h"
#include "valid_queue_user.h"

#include "sgeobj/cull/sge_select_queue_LDR_L.h"
#include "sgeobj/cull/sge_select_queue_QRL_L.h"
#include "sgeobj/cull/sge_message_MES_L.h"

#include "msg_common.h"
#include "msg_schedd.h"

/* -- these implement helpers for the category optimization -------- */

/* make sure that this is in sync with libs/sgeobj/json/LDR.json */
enum {
   LDR_queue_ref_list_pos = 0,
   LDR_limit_pos,
   LDR_global_pos,
   LDR_host_pos,
   LDR_queue_pos
};

typedef struct {
   lListElem *category;           /* ref to the category */
   lListElem *cache;              /* ref to the cache object in the category */
   bool       use_category;       /* if true: use the category
                                   * with immediate dispatch runs only and only if there is more than a single job of that category
                                   * prevents 'skip_host_list' and 'skip_queue_list' be used with reservation
                                   */
   bool       mod_category;       /* if true: update the category with new messages, queues, and hosts */
   u_long32  *possible_pe_slots;  /* stores all possible slots settings for a pe job with ranges
                                   * it is stored in the job category cache, attribute CCT_pe_job_slots
                                   * and is *not* freed with the category_use_t object
                                   * (unless is_pe_slots_rev == false which means: it is not referenced in the cache)
                                   */
   bool      is_pe_slots_rev;     /* if it is true, the possible_pe_slots are stored in the category */
} category_use_t;

static void
fill_category_use_t(const sge_assignment_t *a, category_use_t *use_category, const char *pe_name);

static bool
add_pe_slots_to_category(category_use_t *use_category, u_long32 *max_slotsp, lListElem *pe,
                         int min_slots, int max_slots, lList *pe_range);
/* -- these implement parallel assignment ------------------------- */

static dispatch_t
parallel_reservation_max_time_slots(sge_assignment_t *best, int *available_slots);

static dispatch_t
parallel_maximize_slots_pe(sge_assignment_t *best, int *available_slots);

static dispatch_t
parallel_assignment(sge_assignment_t *a, category_use_t *use_category, int *available_slots);

#ifdef SOLARIS
#pragma no_inline(parallel_assignment)
#endif

static dispatch_t
parallel_available_slots(const sge_assignment_t *a, int *slots, int *slots_qend);

static dispatch_t
parallel_tag_queues_suitable4job(sge_assignment_t *assignment, category_use_t *use_category, int *available_slots);

static dispatch_t
parallel_global_slots(const sge_assignment_t *a, int *slots, int *slots_qend);

static dispatch_t
parallel_tag_hosts_queues(sge_assignment_t *a, lListElem *hep, int *slots, int *slots_qend, bool need_master,
                          bool is_master_host, bool *master_host, category_use_t *use_category,
                          lList **unclear_cqueue_list);

#ifdef SOLARIS
#pragma no_inline(parallel_tag_hosts_queues)
#endif

static int
parallel_max_host_slots(sge_assignment_t *a, lListElem *host);


static dispatch_t
parallel_queue_slots(sge_assignment_t *a, lListElem *qep, int *slots, int *slots_qend, bool need_master,
                     bool is_master_queue, bool allow_non_requestable);

static
void clean_up_parallel_job(sge_assignment_t *a);

/* -- these implement sequential assignment ---------------------- */

static dispatch_t
sequential_tag_queues_suitable4job(sge_assignment_t *a);

static dispatch_t
sequential_queue_time(u_long64 *start, const sge_assignment_t *a, int *violations, lListElem *qep);

static dispatch_t
sequential_host_time(u_long64 *start, const sge_assignment_t *a, int *violations, const lListElem *hep);

static dispatch_t
sequential_global_time(u_long64 *start_time, const sge_assignment_t *a, int *violations);

static dispatch_t
match_static_advance_reservation(const sge_assignment_t *a);

static int
sequential_update_host_order(lList *host_list, lList *queues);

/* -- base functions ---------------------------------------------- */

static int
compute_soft_violations(const sge_assignment_t *a, lListElem *queue, int violation,
                        const lList *load_attr, const lList *config_attr, const lList *actual_attr,
                        u_long32 layer, double lc_factor, u_long32 tag);

static dispatch_t
rc_time_by_slots(const sge_assignment_t *a, lList *requested, const lList *load_attr, const lList *config_attr, const lList *actual_attr,
                 lListElem *queue, bool allow_non_requestable, dstring *reason, int slots,
                 u_long32 layer, double lc_factor, u_long32 tag, u_long64 *start_time, const char *object_name);



static dispatch_t
ri_slots_by_time(const sge_assignment_t *a, int *slots, int *slots_qend, const lList *rue_list, lListElem *request,
                 const lList *load_attr, const lList *total_list, const lList *additional_usage, lListElem *queue,
                 u_long32 layer, double lc_factor, dstring *reason, bool allow_non_requestable, bool no_centry,
                 const char *object_name);

static dispatch_t
match_static_resource(int slots, lListElem *req_cplx, lListElem *src_cplx, dstring *reason, bool allow_non_requestable);

static int
resource_cmp(u_long32 relop, double req, double src_dl);

static bool
job_is_forced_centry_missing(const sge_assignment_t *a, const lListElem *queue_or_host, bool is_qinstance);

static void
clear_resource_tags(lList *resources, u_long32 max_tag);
static void
clear_resource_tags(lListElem *job, u_long32 max_tag);

static dispatch_t
find_best_result(dispatch_t r1, dispatch_t r2);

/* ---- helpers for load computation ---------------------------------------------------------- */

static lListElem
*load_locate_elem(lList *load_list, lListElem *global_consumable, lListElem *host_consumable,
                  lListElem *queue_consumable, const char *limit);

static int
load_check_alarm(char *reason, size_t reason_size, const char *name, const char *load_value, const char *limit_value,
                     u_long32 relop, u_long32 type, lListElem *hep, const lListElem *hlep, double lc_host,
                     double lc_global, const lList *load_adjustments, int load_is_value);

static int
load_np_value_adjustment(const char* name, lListElem *hep, double *load_correction);

static void
print_tagged4schedule(const lListElem *qinstance) {
   DENTER(TOP_LAYER);

   if (DPRINTF_IS_ACTIVE) {
      if (qinstance != nullptr) {
         u_long32 bitmask = lGetUlong(qinstance, QU_tagged4schedule);
         DSTRING_STATIC(dstr, 100);
         if (ISSET(bitmask, TAG4SCHED_MASTER)) {
            sge_dstring_append(&dstr, " MASTER");
         }
         if (ISSET(bitmask, TAG4SCHED_SLAVE)) {
            sge_dstring_append(&dstr, " SLAVE");
         }
         if (ISSET(bitmask, TAG4SCHED_MASTER_LATER)) {
            sge_dstring_append(&dstr, " LATER_MASTER");
         }
         if (ISSET(bitmask, TAG4SCHED_SLAVE_LATER)) {
            sge_dstring_append(&dstr, " LATER_SLAVE");
         }

         DPRINTF("      QU_tagged4schedule: %s\n", sge_dstring_get_string(&dstr));
      } else {
         // this is a regular case when matching hosts, no need for an error message
         // DPRINTF("      !!! nullptr passed as qinstance to print_tagged4schedule !!!\n");
      }
   }

   DRETURN_VOID;
}

/* ---- Implementation ------------------------------------------------------------------------- */

void assignment_init(sge_assignment_t *a, lListElem *job, lListElem *ja_task, lList *load_adjustments)
{
   if (job != nullptr) {
      a->job         = job;
      a->user        = lGetString(job, JB_owner);
      a->group       = lGetString(job, JB_group);
      a->grp_list    = lGetList(job, JB_grp_list);
      a->project     = lGetString(job, JB_project);
      a->job_id      = lGetUlong(job, JB_job_number);
      a->is_soft     = job_has_soft_requests(job);
   }

   a->load_adjustments = load_adjustments;

   if (ja_task != nullptr) {
      a->ja_task     = ja_task;
      a->ja_task_id  = lGetUlong(ja_task, JAT_task_number);
   }
}

void assignment_copy(sge_assignment_t *dst, sge_assignment_t *src, bool move_gdil)
{
   if (dst == nullptr || src == nullptr) {
      return;
   }

   if (move_gdil) {
      lFreeList(&(dst->gdil));
      lFreeList(&(dst->limit_list));
      lFreeList(&(dst->skip_cqueue_list));
      lFreeList(&(dst->skip_host_list));
   }

   memcpy(dst, src, sizeof(sge_assignment_t));

   if (src->load_adjustments != nullptr) {
      dst->load_adjustments = src->load_adjustments;
   }

   if (move_gdil) {
      src->gdil = src->limit_list = src->skip_cqueue_list = src->skip_host_list = nullptr;
   } else {
      dst->gdil = dst->limit_list = dst->skip_cqueue_list = dst->skip_host_list = nullptr;
   }
}

void assignment_release(sge_assignment_t *a)
{
   lFreeList(&(a->gdil));
   lFreeList(&(a->limit_list));
   lFreeList(&(a->skip_cqueue_list));
   lFreeList(&(a->skip_host_list));
}

void assignment_clear_cache(sge_assignment_t *a)
{
   lFreeList(&(a->limit_list));
   lFreeList(&(a->skip_cqueue_list));
   lFreeList(&(a->skip_host_list));
}

static dispatch_t
find_best_result(dispatch_t r1, dispatch_t r2)
{
   DENTER(BASIS_LAYER);

   if (r1 == DISPATCH_NEVER ||
            r2 == DISPATCH_NEVER) {
      DRETURN(DISPATCH_NEVER);
   }
   else if (r1 == DISPATCH_OK ||
       r2 == DISPATCH_OK) {
      DRETURN(DISPATCH_OK);
   }
   else if (r1 == DISPATCH_NOT_AT_TIME ||
            r2 == DISPATCH_NOT_AT_TIME) {
      DRETURN(DISPATCH_NOT_AT_TIME);
   }
   else if (r1 == DISPATCH_NEVER_JOB ||
            r2 == DISPATCH_NEVER_JOB) {
      DRETURN(DISPATCH_NEVER_JOB);
   }
   else if (r1 ==  DISPATCH_NEVER_CAT ||
            r2 == DISPATCH_NEVER_CAT) {
      DRETURN(DISPATCH_NEVER_CAT);
   }
   else if (r1 == DISPATCH_MISSING_ATTR  ||
            r2 == DISPATCH_MISSING_ATTR ) {
      DRETURN(DISPATCH_MISSING_ATTR);
   }

   CRITICAL(SFNMAX, MSG_JOBMATCHINGUNEXPECTEDRESULT);
   DRETURN(DISPATCH_NEVER);
}


static bool
is_not_better(sge_assignment_t *a, u_long32 viola_best, u_long64 tt_best, u_long32 viola_this, u_long64 tt_this)
{
   /* earlier start time has higher preference than lower soft violations */
   if (a->is_reservation && tt_this >= tt_best)
      return true;
   if (!a->is_reservation && viola_this >= viola_best)
      return true;
   return false;
}

#if 0
static bool
is_acceptable_result(sge_assignment_t *a) {
   bool ret = false;

   const lList *pe_range = lGetList(a->job, JB_pe_range);
   int last = range_list_get_last_id(pe_range, nullptr);
   if (a->slots == last) {
      // we have the maximum number of slots we requested

      if (!a->is_soft || a->soft_violations == 0) {
         // we either have no soft request or all of them could be matched

         // we have a perfectly matching pe
         ret = true;
      }
   }

   /* @todo (CS-452) need a way to break out of the loop, e.g. if
    * + there are no soft requests
    * + we have 0 soft violations (or reached a configurable acceptable number)
    * + we request a fixed number of PE slots and we got an assignment
    * + we request a range of PE slots and already got the maximum
    */

   return ret;
}
#endif

/****** scheduler/sge_select_parallel_environment() ****************************
*  NAME
*     sge_select_parallel_environment() -- Decide about a PE assignment
*
*  SYNOPSIS
*     static dispatch_t sge_select_parallel_environment(sge_assignment_t *best, lList
*     *pe_list)
*
*  FUNCTION
*     When users use wildcard PE request such as -pe <pe_range> 'mpi8_*'
*     more than a single parallel environment can match the wildcard expression.
*     In case of 'now' assignments the PE that gives us the largest assignment
*     is selected. When scheduling a reservation we search for the earliest
*     assignment for each PE and then choose that one that finally gets us the
*     maximum number of slots.
*
* IMPORTANT
*     The scheduler info messages are not cached. They are added globally and have
*     to be added for each job in the category. When the messages are updated
*     this has to be changed.
*
*  INPUTS
*     sge_assignment_t *best - herein we keep all important in/out information
*     lList *pe_list         - the list of all parallel environments (PE_Type)
*
*  RESULT
*     dispatch_t - 0 ok got an assignment
*                  1 no assignment at the specified time (???)
*                 -1 assignment will never be possible for all jobs of that category
*                 -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: sge_select_parallel_environment() is not MT safe
*******************************************************************************/
dispatch_t
sge_select_parallel_environment(sge_assignment_t *best, const lList *pe_list)
{
   DENTER(TOP_LAYER);
   int matched_pe_count = 0;
   lListElem *pe;
   lListElem *queue;
   const char *pe_request, *pe_name;
   dispatch_t result, best_result = DISPATCH_NEVER_CAT;
   int old_logging = 0;
   DSTRING_STATIC(time_str, 64);

   pe_request = lGetString(best->job, JB_pe);

   DPRINTF("handling parallel job " sge_u32"." sge_u32"\n", best->job_id, best->ja_task_id);

   /* make sure our queue list is sorted according to host order (load formula) */
   sequential_update_host_order(best->host_list, best->queue_list);

   /* initialize all tags */
   for_each_rw(queue, best->queue_list) {
      lSetUlong(queue, QU_tagged4schedule, TAG4SCHED_ALL); // = can be used as master and slave for now and reservation
   }

   if (best->is_reservation) { /* reservation scheduling */
      if (!best->is_advance_reservation) {
         old_logging = schedd_mes_get_logging();
         schedd_mes_set_logging(0);
         sconf_set_mes_schedd_info(false);
      }

      for_each_rw(pe, pe_list) {
         int available_slots = 0;

         if (!pe_is_matching(pe, pe_request)) {
            continue;
         }

         matched_pe_count++;

         pe_name = lGetString(pe, PE_name);
         if (best->gdil == nullptr) { /* first pe run */
            best->pe = pe;
            best->pe_name = pe_name;

            /* determine the earliest start time with that PE */
            result = parallel_reservation_max_time_slots(best, &available_slots);
            if (result != DISPATCH_OK) {
               schedd_mes_add(best->monitor_alpp, best->monitor_next_run,
                              best->job_id, SCHEDD_INFO_PESLOTSNOTINRANGE_SI,
                              pe_name, available_slots);
               best_result = find_best_result(best_result, result);
               continue;
            }
            DPRINTF("### first ### reservation in PE \"%s\" at %s with %d soft violations\n",
                    pe_name, sge_ctime64(best->start, &time_str), best->soft_violations);
         } else { /* test with all other pes */
            sge_assignment_t tmp = SGE_ASSIGNMENT_INIT;

            assignment_copy(&tmp, best, false);
            tmp.pe = pe;
            tmp.pe_name = pe_name;

            /* try to find earlier assignment again with minimum slot amount */
            tmp.slots = 0;
            result = parallel_reservation_max_time_slots(&tmp, &available_slots);

            if (result != DISPATCH_OK) {
               schedd_mes_add(best->monitor_alpp, best->monitor_next_run,
                              best->job_id, SCHEDD_INFO_PESLOTSNOTINRANGE_SI,
                              pe_name, available_slots);
               best_result = find_best_result(best_result, result);
               assignment_release(&tmp);
               continue;
            }

            if (tmp.start < best->start ||
                  (tmp.start == best->start && tmp.soft_violations < best->soft_violations)) {
               assignment_copy(best, &tmp, true);
               DPRINTF("### better ### reservation in PE \"%s\" at " sge_u32" with %d soft violations\n",
                       pe_name, best->start, best->soft_violations);
            }

            assignment_release(&tmp);

            /* @todo (CS-452) need a way to break out of the loop, e.g. if
             * - there are no soft requests
             * - we have 0 soft violations (or reached a configurable acceptable number)
             * - in case of AR: the reservation time is what was requested
             * - in case of RR: we got a reservation time which is acceptable (due to some configuration parameter)
             */
         }
      }
   } else {
      /* now assignments */
      u_long32 ar_id = lGetUlong(best->job, JB_ar);

      if (ar_id != 0) {
         // we have an advance reservation
         result = match_static_advance_reservation(best);
         if (result != DISPATCH_OK) {
            DRETURN(result);
         }

         const lListElem *ar = lGetElemUlong(best->ar_list, AR_id, ar_id);
         pe = lGetElemStrRW(pe_list, PE_name, lGetString(ar, AR_granted_pe));

         if (pe == nullptr) {
            DPRINTF("Critical Error, ar references non existing PE\n");
         } else {
            int available_slots = 0;

            matched_pe_count++;
            best->pe = pe;
            best->pe_name = lGetString(pe, PE_name);

            best_result = parallel_maximize_slots_pe(best, &available_slots);
            if (best_result != DISPATCH_OK) {
               schedd_mes_add(best->monitor_alpp, best->monitor_next_run,
                              best->job_id, SCHEDD_INFO_PESLOTSNOTINRANGE_SI,
                              best->pe_name, available_slots);
            }
            DPRINTF("### AR ### assignment in PE \"%s\" with %d soft violations and %d available slots\n",
                    best->pe_name, best->soft_violations, available_slots);
         }
      } else {
         // we have no advance reservation
         for_each_rw(pe, pe_list) {
            if (!pe_is_matching(pe, pe_request)) {
               continue;
            }

            pe_name = lGetString(pe, PE_name);
            matched_pe_count++;

            if (best->gdil == nullptr) {
               // first PE matching the requested name
               int available_slots = 0;
               best->pe = pe;
               best->pe_name = pe_name;
               result = parallel_maximize_slots_pe(best, &available_slots);

               if (result != DISPATCH_OK) {
                  schedd_mes_add(best->monitor_alpp, best->monitor_next_run,
                                 best->job_id, SCHEDD_INFO_PESLOTSNOTINRANGE_SI,
                                 pe_name, available_slots);
                  best_result = find_best_result(best_result, result);
                  continue;
               }
               DPRINTF("### first ### assignment in PE \"%s\" with %d soft violations\n", pe_name, best->soft_violations);
               /* CS-452 shortcut
               if (is_acceptable_result(best)) {
                  break;
               } */
            } else {
               // another PE matching the requested name
               int available_slots = 0;
               sge_assignment_t tmp = SGE_ASSIGNMENT_INIT;
               assignment_copy(&tmp, best, false);
               tmp.pe = pe;
               tmp.pe_name = pe_name;

               result = parallel_maximize_slots_pe(&tmp, &available_slots);

               if (result != DISPATCH_OK) {
                  assignment_release(&tmp);
                  schedd_mes_add(best->monitor_alpp, best->monitor_next_run,
                                 best->job_id, SCHEDD_INFO_PESLOTSNOTINRANGE_SI,
                                 pe_name, available_slots);
                  best_result = find_best_result(best_result, result);
                  continue;
               }

               if ((tmp.slots > best->slots) ||
                   (tmp.start == best->start &&
                    tmp.soft_violations < best->soft_violations)) {
                  assignment_copy(best, &tmp, true);
                  DPRINTF("### better ### assignment in PE \"%s\" with %d soft violations\n",
                          pe_name, best->soft_violations);
               }
               assignment_release(&tmp);
               /* CS-452 shortcut
               if (is_acceptable_result(best)) {
                  break;
               } */
            }
         }
      }
   }

   if (matched_pe_count == 0) {
      schedd_mes_add(best->monitor_alpp, best->monitor_next_run,
                     best->job_id, SCHEDD_INFO_NOPEMATCH_ );
      best_result = DISPATCH_NEVER_CAT;
   } else if (best->is_reservation && best->gdil) {
      int available_slots = 0;
      // we searched for the earliest start time with the minimum slot count
      // now try to maximize the slot count at the given time
      result = parallel_maximize_slots_pe(best, &available_slots);
      if (result != DISPATCH_OK) { /* ... should never happen */
         best_result = DISPATCH_NEVER_CAT;
      }
   }

   if (best->gdil) {
      best_result = DISPATCH_OK;
   }

   switch (best_result) {
   case DISPATCH_OK:
      DPRINTF("SELECT PE(" sge_u32"." sge_u32") returns PE %s %d slots at " sge_u32")\n",
              best->job_id, best->ja_task_id, best->pe_name, best->slots, best->start);
      break;
   case DISPATCH_NOT_AT_TIME:
      DPRINTF("SELECT PE(" sge_u32"." sge_u32") returns <later>\n", best->job_id, best->ja_task_id);
      break;
   case DISPATCH_NEVER_CAT:
      DPRINTF("SELECT PE(" sge_u32"." sge_u32") returns <category_never>\n", best->job_id, best->ja_task_id);
      break;
   case DISPATCH_NEVER_JOB:
      DPRINTF("SELECT PE(" sge_u32"." sge_u32") returns <job_never>\n", best->job_id, best->ja_task_id);
      break;
   case DISPATCH_MISSING_ATTR:
   default:
      DPRINTF("!!! SELECT PE(" sge_u32"." sge_u32") returns unexpected %d\n", best->job_id, best->ja_task_id, best_result);
      break;
   }

   if (best->is_reservation && !best->is_advance_reservation) {
      sconf_set_mes_schedd_info(true);
      schedd_mes_set_logging(old_logging);
   }

   /* clean up */
   clean_up_parallel_job(best);

   DRETURN(best_result);
}

/****** sge_select_queue/clean_up_parallel_job() *******************************
*  NAME
*     clean_up_parallel_job() -- removes tags
*
*  SYNOPSIS
*     static void clean_up_parallel_job(sge_assignment_t *a)
*
*  FUNCTION
*     during pe job dispatch are man queues and hosts tagged. This
*     function removes the tags.
*
*  INPUTS
*     sge_assignment_t *a - the resource structure
*
*
*
*  NOTES
*     MT-NOTE: clean_up_parallel_job() is not MT safe
*
*******************************************************************************/
static
void clean_up_parallel_job(sge_assignment_t *a)
{
   qinstance_list_set_tag(a->queue_list, 0);
}

/****** scheduler/parallel_reservation_max_time_slots() *****************************************
*  NAME
*     parallel_reservation_max_time_slots() -- Search earliest possible assignment
*
*  SYNOPSIS
*     static dispatch_t parallel_reservation_max_time_slots(sge_assignment_t *best)
*
*  FUNCTION
*     The earliest possible assignment is searched for a job assuming a
*     particular parallel environment be used with a particular slot
*     number. If the slot number passed is 0 we start with the minimum
*     possible slot number for that job. The search starts with the
*     latest queue end time if DISPATCH_TIME_QUEUE_END was specified
*     rather than a real time value.
*
*  INPUTS
*     sge_assignment_t *best - herein we keep all important in/out information
*
*  RESULT
*     dispatch_t - 0 ok got an assignment
*                  1 no assignment at the specified time (???)
*                 -1 assignment will never be possible for all jobs of that category
*                 -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: parallel_reservation_max_time_slots() is not MT safe
*******************************************************************************/
static dispatch_t
parallel_reservation_max_time_slots(sge_assignment_t *best, int *available_slots)
{
   u_long64 pe_time, first_time;
   sge_assignment_t tmp_assignment = SGE_ASSIGNMENT_INIT;
   dispatch_t result = DISPATCH_NEVER_CAT;
   sge_qeti_t *qeti = nullptr;
   bool is_first = true;
   int old_logging = 0;
   category_use_t use_category;
   DSTRING_STATIC(time_str, 64);

   DENTER(TOP_LAYER);

   /* assemble job category information */
   fill_category_use_t(best, &use_category, best->pe_name);

   qeti = sge_qeti_allocate(best);
   if (qeti == nullptr) {
      ERROR("could not allocate qeti object needed reservation " "scheduling of parallel job " sge_U32CFormat, sge_u32c(best->job_id));
      DRETURN(DISPATCH_NEVER_CAT);
   }

   assignment_copy(&tmp_assignment, best, false);
   if (best->slots == 0) {
      tmp_assignment.slots = range_list_get_first_id(lGetList(best->job, JB_pe_range), nullptr);
   }

   if (best->start == DISPATCH_TIME_QUEUE_END) {
      first_time = sge_qeti_first(qeti);
      if (first_time == 0) { /* we need at least one reservation run */
         first_time = best->now;
      }
   } else {
      /* the first iteration will be done using best->start
         further ones will use earlier times */
      first_time = best->start;
      sge_qeti_next_before(qeti, best->start);
   }

   old_logging = schedd_mes_get_logging(); /* store logging mode */
   for (pe_time = first_time ; pe_time; pe_time = sge_qeti_next(qeti)) {
      DPRINTF("SELECT PE TIME(%s, " sge_u32") tries at %s\n",
               best->pe_name, best->job_id, sge_ctime64(pe_time, &time_str));
      tmp_assignment.start = pe_time;

      /* this is an additional run, we have already at least one possible match,
         all additional scheduling information is not important, since we can
         start the job */
      if (is_first) {
         is_first = false;
      } else {
         use_category.mod_category = false;
         schedd_mes_set_logging(0);
         sconf_set_mes_schedd_info(false);
      }

      result = parallel_assignment(&tmp_assignment, &use_category, available_slots);
      assignment_clear_cache(&tmp_assignment);

      if (result == DISPATCH_OK) {
         if (tmp_assignment.gdil) {
            DPRINTF("SELECT PE TIME: earlier assignment at %s\n", sge_ctime64(pe_time, &time_str));
         }
         assignment_copy(best, &tmp_assignment, true);
         assignment_release(&tmp_assignment);
      } else {
         DPRINTF("SELECT PE TIME: no earlier assignment at %s\n", sge_ctime64(pe_time, &time_str));
         break;
      }
   }
   schedd_mes_set_logging(old_logging); /* restore logging mode */
   sconf_set_mes_schedd_info(true);

   sge_qeti_release(&qeti);
   assignment_release(&tmp_assignment);

   if (best->gdil) {
      result = DISPATCH_OK;
   }

   switch (result) {
   case DISPATCH_OK:
      DPRINTF("SELECT PE TIME(%s, %d) returns %s\n", best->pe_name, best->slots, sge_ctime64(best->start, &time_str));
      break;
   case DISPATCH_NEVER_CAT:
      DPRINTF("SELECT PE TIME(%s, %d) returns <category_never>\n", best->pe_name, best->slots);
      break;
   case DISPATCH_NEVER_JOB:
      DPRINTF("SELECT PE TIME(%s, %d) returns <job_never>\n", best->pe_name, best->slots);
      break;
   default:
      DPRINTF("!!! SELECT PE TIME(%s, %d) returns unexpected %d\n", best->pe_name, best->slots, result);
      break;
   }

   DRETURN(result);
}

/****** scheduler/parallel_maximize_slots_pe() *****************************************
*  NAME
*     parallel_maximize_slots_pe() -- Maximize number of slots for an assignment
*
*  SYNOPSIS
*     static int parallel_maximize_slots_pe(sge_assignment_t *best, lList *host_list,
*     lList *queue_list, lList *centry_list, lList *acl_list)
*
*  FUNCTION
*     The largest possible slot amount is searched for a job assuming a
*     particular parallel environment be used at a particular start time.
*     If the slot number passed is 0 we start with the minimum
*     possible slot number for that job.
*
*     To search most efficiently for the right slot value, it has three search
*     strategies implemented:
*     - binary search
*     - least slot value first
*     - highest slot value first
*
*     To be able to use binary search all possible slot values are stored in
*     one array. The slot values in this array are sorted ascending. After the
*     right slot value was found, it is very easy to compute the best strategy
*     from the result. For each strategy it will compute how many iterations
*     would have been needed to compute the correct result. These steps will
*     be stored for the next run and used to figure out the best algorithm.
*     To ensure that we can adapt to rapid changes and also ignore spikes we
*     are using the running average algorithm in a 80-20 setting. This means
*     that the algorithm will need 4 (max 5) iterations to adopt to a new
*     scenario.
*
*  Further enhancements:
*     It might be a good idea to store the derived values with the job categories
*     and allow to find the best strategy per category.
*
*  INPUTS
*     sge_assignment_t *best - herein we keep all important in/out information
*     int *available_slots   -
*
*  RESULT
*     int - 0 ok got an assignment (maybe without maximizing it)
*           1 no assignment at the specified time
*          -1 assignment will never be possible for all jobs of that category
*          -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: parallel_maximize_slots_pe() is MT safe as long as the provided
*              lists are owned be the caller
*
*  SEE ALSO:
*     sconf_best_pe_alg
*     sconf_update_pe_alg
*     add_pe_slots_to_category
*
*******************************************************************************/
static dispatch_t
parallel_maximize_slots_pe(sge_assignment_t *best, int *available_slots)
{
   int min_slots, max_slots;
   int max_pe_slots;
   int first, last;
   lList *pe_range;
   lListElem *pe;
   sge_assignment_t tmp = SGE_ASSIGNMENT_INIT;
   dispatch_t result = DISPATCH_NEVER_CAT;
   const char *pe_name = best->pe_name;
   bool is_first = true;
   int old_logging = 0;
   category_use_t use_category{};
   u_long32 max_slotsp = 0;
   int current = 0;
   int match_current = 0;
   int runs = 0;
   schedd_pe_algorithm alg =  sconf_best_pe_alg();

   DENTER(TOP_LAYER);

   if (best == nullptr ||
       (pe_range=lGetListRW(best->job, JB_pe_range)) == nullptr ||
       (pe=best->pe) == nullptr) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   // assemble job category information
   fill_category_use_t(best, &use_category, pe_name);

   // requested pe slot range
   first = range_list_get_first_id(pe_range, nullptr);
   last  = range_list_get_last_id(pe_range, nullptr);
   max_pe_slots = lGetUlong(pe, PE_slots);

   // we already have a possible assignment with a certain slot count - use it as minimum slot count
   if (best->slots) {
      min_slots = best->slots;
   } else {
      min_slots = first;
   }

   // shortcut: this PE cannot give us more slots - skip it
   // @todo the calling code assumes that we might optimize soft violations - which is not the case here
   if (best->gdil && best->slots == max_pe_slots) { /* already found maximum */
      DRETURN(DISPATCH_OK);
   }

   DPRINTF("MAXIMIZE SLOT: FIRST %d LAST %d MAX SLOT %d\n", first, last, max_pe_slots);

   // cap the max range number RANGE_INFINITY (i.e. -pe pename 1-) to the maximum the pe can provide
   max_slots = MIN(last, max_pe_slots);

   DPRINTF("MAXIMIZE SLOT FOR " sge_u32" using \"%s\" FROM %d TO %d\n",
      best->job_id, pe_name, min_slots, max_slots);

   old_logging = schedd_mes_get_logging(); /* store logging mode */

   if ((max_slots < min_slots) ||
      (min_slots <= 0)) {
      ERROR("invalid pe job range setting for job " sge_u32"\n", best->job_id);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* --- prepare the possible slots for the binary search */
   max_slotsp = (max_slots - min_slots+1);
   if (!add_pe_slots_to_category(&use_category, &max_slotsp, pe, min_slots, max_slots, pe_range)) {
      ERROR(SFNMAX, MSG_SGETEXT_NOMEM);
      DRETURN(DISPATCH_NEVER_CAT);
   }
   if (max_slotsp == 0) {
      DPRINTF("no slots in PE %s available for job " sge_u32"\n", pe_name, best->job_id);
      if (!use_category.is_pe_slots_rev) {
         sge_free(&(use_category.possible_pe_slots));
      }
      DRETURN(DISPATCH_NEVER_CAT);
   }

   assignment_copy(&tmp, best, false);

   /* --- work on the different slot ranges and try to find the best one --- */
   if (alg == SCHEDD_PE_BINARY) {
      int min = 0;
      int max = max_slotsp-1;

      do {
         runs++;
         current = (min + max) / 2;

         if (is_first) { /* first run, collect information */
            is_first = false;
         } else { /* this is an additional run, we have already at least one possible match */
            use_category.mod_category = false;
            schedd_mes_set_logging(0);
            sconf_set_mes_schedd_info(false);
         }

         /* we try that slot amount */
         tmp.slots = use_category.possible_pe_slots[current];
         result = parallel_assignment(&tmp, &use_category, available_slots);
         assignment_clear_cache(&tmp);

         if (result == DISPATCH_OK) {
            assignment_copy(best, &tmp, true);
            assignment_release(&tmp);
            match_current = current;
            min = current + 1;
         } else {
            max = current - 1;
         }

      } while (min <= max && max != -1);
   } else {
      /* compute how many runs the bin search might have needed */
      /* This will not give us the correct answer in all cases, but
         it is close enough. And on average it is correct :-) */
      int end = max_slotsp;
      for (runs=1; runs < end; runs++) {
         end = end - runs;
      }
      runs--;

      if (alg == SCHEDD_PE_LOW_FIRST) {
         for (current = 0; current < (int)max_slotsp; current++) {
            if (is_first) { /* first run, collect information */
               is_first = false;
            } else {  /* this is an additional run, we have already at least one possible match */
               use_category.mod_category = false;
               schedd_mes_set_logging(0);
               sconf_set_mes_schedd_info(false);
            }

            /* we try that slot amount */
            tmp.slots = use_category.possible_pe_slots[current];
            result = parallel_assignment(&tmp, &use_category, available_slots);
            assignment_clear_cache(&tmp);

            if (result != DISPATCH_OK) { // this slot count did not work
               break;                    // no need to try higher ones
            }

            // we remember the last working slot count
            match_current = current;
            assignment_copy(best, &tmp, true);
            assignment_release(&tmp);
         }
      } else { // optimistic search
         for (current = max_slotsp-1; current >= 0; current--) {
            if (is_first) { /* first run, collect information */
               is_first = false;
            } else { /* this is an additional run, we have already at least one possible match */
               use_category.mod_category = false;
               schedd_mes_set_logging(0);
               sconf_set_mes_schedd_info(false);
            }

            /* we try that slot amount */
            tmp.slots = use_category.possible_pe_slots[current];
            result = parallel_assignment(&tmp, &use_category, available_slots);
            assignment_clear_cache(&tmp);

            if (result == DISPATCH_OK) {          // we have the best possible match, stop
               assignment_copy(best, &tmp, true);
               assignment_release(&tmp);
               match_current = current;
               break;
            }
         } // end for on slot count
      } // end optimistic search
   } // end non-binary search

   sconf_update_pe_alg(runs, match_current, max_slotsp);

   /* --- we are done --- */
   if (!use_category.is_pe_slots_rev) {
      sge_free(&(use_category.possible_pe_slots));
   }

   assignment_release(&tmp);

   schedd_mes_set_logging(old_logging); /* restore logging mode */
   sconf_set_mes_schedd_info(true);

   if (best->gdil) {
      result = DISPATCH_OK;
   }

   switch (result) {
      case DISPATCH_OK:
         if (!best->is_reservation) {
            sconf_inc_pe_jobs();
         }
         DPRINTF("MAXIMIZE SLOT(%s, %d) returns <ok>\n", pe_name, best->slots);
         break;
      case DISPATCH_NOT_AT_TIME:
         DPRINTF("MAXIMIZE SLOT(%s, %d) returns <later>\n", pe_name, best->slots);
         break;
      case DISPATCH_NEVER_CAT:
         DPRINTF("MAXIMIZE SLOT(%s, %d) returns <category_never>\n", pe_name, best->slots);
         break;
      case DISPATCH_NEVER_JOB:
         DPRINTF("MAXIMIZE SLOT(%s, %d) returns <job_never>\n", pe_name, best->slots);
         break;
      default:
         DPRINTF("!!! MAXIMIZE SLOT(%d, %d) returns unexpected %d\n", best->slots, (int)best->start, result);
         break;
   }

   DRETURN(result);
}

/****** sge_select_queue/sge_select_queue() ************************************
*  NAME
*     sge_select_queue() -- checks whether a job matches a given queue or host
*
*  SYNOPSIS
*     int sge_select_queue(lList *requested_attr, lListElem *queue, lListElem
*     *host, lList *exechost_list, lList *centry_list, bool
*     allow_non_requestable, int slots)
*
*  FUNCTION
*     Takes the requested attributes from a job and checks if it matches the given
*     host or queue. One and only one should be specified. If both, the function
*     assumes, that the queue belongs to the given host.
*
*  INPUTS
*     lList *requested_attr     - list of requested attributes
*     lListElem *queue          - current queue or null if host is set
*     lListElem *host           - current host or null if queue is set
*     lList *exechost_list      - list of all hosts in the system
*     lList *centry_list        - system wide attribute config list
*     bool allow_non_requestable - allow non requestable?
*     int slots                 - number of requested slots
*     lList *queue_user_list    - list of users or null
*     lList *acl_list           - acl_list or null
*     lListElem *job            - job or null
*
*  RESULT
*     int - 1, if okay, QU_tag will be set if a queue is selected
*           0, if not okay
*
*  NOTES
*   The caller is responsible for cleaning tags.
*
*   No range is used. For serial jobs we will need a call for hard and one
*    for soft requests. For parallel jobs we will call this function for each
*   -l request. Because of in serial jobs requests can be simply added.
*   In Parallel jobs each -l requests a different set of queues.
*
*******************************************************************************/
bool
sge_select_queue(lList *requested_attr, lListElem *queue, lListElem *host,
                 lList *exechost_list, lList *centry_list, bool allow_non_requestable,
                 int slots, lList *queue_user_list, lList *acl_list, lListElem *job)
{
   dispatch_t ret;
   const lList *load_attr = nullptr;
   const lList *config_attr = nullptr;
   const lList *actual_attr = nullptr;
   lListElem *global = nullptr;
   const lListElem *qu = nullptr;
   int q_access=1;
   const lList *projects;
   const char *project;

   sge_assignment_t a = SGE_ASSIGNMENT_INIT;
   double lc_factor = 0; /* scaling for load correction */
   u_long32 ulc_factor;
   /* actually we don't care on start time here to this is just a dummy setting */
   u_long64 start_time = DISPATCH_TIME_NOW;

   DENTER(TOP_LAYER);

   clear_resource_tags(requested_attr, MAX_TAG);

   assignment_init(&a, nullptr, nullptr, nullptr);
   a.centry_list      = centry_list;
   a.host_list        = exechost_list;

   if (acl_list != nullptr) {
      /* check if job owner has access rights to the queue */
      DPRINTF("testing queue access lists\n");
      for_each_ep(qu, queue_user_list) {
         const char *name = lGetString(qu, ST_name);
         DPRINTF("-----> checking queue user: %s\n", name );
         q_access |= (name[0]=='@')?
                     sge_has_access(nullptr, &name[1], nullptr, queue, acl_list):
                     sge_has_access(name, nullptr, nullptr, queue, acl_list);
         if (q_access) {
            break;
         }
      }
      if (q_access == 0) {
         DPRINTF("no access\n");
         assignment_release(&a);
         DRETURN(false);
      } else {
         DPRINTF("ok\n");
      }
   }

   if (job != nullptr) {
      /* check if job can run in queue based on project */
      DPRINTF("testing job projects lists\n");
      if ( (project = lGetString(job, JB_project)) ) {
         if ((!(projects = lGetList(queue, QU_projects)))) {
            DPRINTF("no access because queue has no project\n");
            assignment_release(&a);
            DRETURN(false);
         }
         if ((!prj_list_locate(projects, project))) {
            DPRINTF("no access because project not contained in queues project list");
            assignment_release(&a);
            DRETURN(false);
         }
         DPRINTF("ok\n");

         /* check if job can run in queue based on excluded projects */
         DPRINTF("testing job xprojects lists\n");
         if ((projects = lGetList(queue, QU_xprojects))) {
            if (((project = lGetString(job, JB_project)) &&
                 prj_list_locate(projects, project))) {
               DPRINTF("no access\n");
               assignment_release(&a);
               DRETURN(false);
            }
         }
         DPRINTF("ok\n");
      }

      /*
      *is queue contained in hard queue list ?
      */
      DPRINTF("queue contained in jobs hard queue list?\n");
      const lList *qref_list = job_get_hard_queue_list(job); // @todo CS-400 where is sge_select_queue() called?
      if (qref_list != nullptr) {
         const char *qname = nullptr;
         const char *qinstance_name = nullptr;

         qname = lGetString(queue, QU_qname);
         qinstance_name = lGetString(queue, QU_full_name);
         if ((lGetElemStr(qref_list, QR_name, qname) != nullptr) ||
             (lGetElemStr(qref_list, QR_name, qinstance_name) != nullptr)) {
            DPRINTF("ok");
         } else {
            DPRINTF("denied because queue \"%s\" is not contained in the hard "
                    "queue list (-q) that was requested by job %d\n",
                    qname, lGetUlong(job, JB_job_number));
            assignment_release(&a);
            DRETURN(false);
         }
      }
   }

/* global */
   global = host_list_locate(exechost_list, SGE_GLOBAL_NAME);
   load_attr = lGetList(global, EH_load_list);
   config_attr = lGetList(global, EH_consumable_config_list);
   actual_attr = lGetList(global, EH_resource_utilization);

   /* is there a multiplier for load correction (maybe not in qstat, qhost etc.) */
   if (lGetPosViaElem(global, EH_load_correction_factor, SGE_NO_ABORT) >= 0) {
      if ((ulc_factor=lGetUlong(global, EH_load_correction_factor)))
         lc_factor = ((double)ulc_factor)/100;
   }

   ret = rc_time_by_slots(&a, requested_attr, load_attr, config_attr, actual_attr,
            nullptr, allow_non_requestable, nullptr, slots, DOMINANT_LAYER_HOST, lc_factor, GLOBAL_TAG,
            &start_time, SGE_GLOBAL_NAME);

/* host */
   if(ret == DISPATCH_OK || ret == DISPATCH_MISSING_ATTR){
      if(host == nullptr) {
         host = host_list_locate(exechost_list, lGetHost(queue, QU_qhostname));
      }
      load_attr = lGetList(host, EH_load_list);
      config_attr = lGetList(host, EH_consumable_config_list);
      actual_attr = lGetList(host, EH_resource_utilization);

      if (lGetPosViaElem(host, EH_load_correction_factor, SGE_NO_ABORT) >= 0) {
         if ((ulc_factor=lGetUlong(host, EH_load_correction_factor)))
            lc_factor = ((double)ulc_factor)/100;
      }

      ret = rc_time_by_slots(&a, requested_attr, load_attr, config_attr,
                             actual_attr, nullptr, allow_non_requestable, nullptr,
                             slots, DOMINANT_LAYER_HOST, lc_factor, HOST_TAG,
                             &start_time, lGetHost(host, EH_name));

/* queue */
     if((ret == DISPATCH_OK || ret == DISPATCH_MISSING_ATTR) && queue){
         config_attr = lGetList(queue, QU_consumable_config_list);
         actual_attr = lGetList(queue, QU_resource_utilization);

         ret = rc_time_by_slots(&a, requested_attr, nullptr, config_attr, actual_attr,
               queue, allow_non_requestable, nullptr, slots, DOMINANT_LAYER_QUEUE, 0, QUEUE_TAG,
               &start_time,  lGetString(queue, QU_full_name));
      }
   }

   assignment_release(&a);

   DRETURN((ret == DISPATCH_OK) ? true : false);
}


/****** sge_select_queue/rc_time_by_slots() **********************************
*  NAME
*     rc_time_by_slots() -- checks weather all resource requests on one level
*                             are fulfilled
*
*  SYNOPSIS
*     static int rc_time_by_slots(lList *requested, lList *load_attr, lList
*     *config_attr, lList *actual_attr, lList *centry_list, lListElem *queue,
*     bool allow_non_requestable, char *reason, int reason_size, int slots,
*     u_long32 layer, double lc_factor, u_long32 tag)
*
*  FUNCTION
*     Checks, weather all requests, default requests and implicit requests on this
*     this level are fulfilled.
*
*     With reservation scheduling the earliest start time due to resources of the
*     resource container is the maximum of the earliest start times for all
*     resources comprised by the resource container that requested by a job.
*
*  INPUTS
*     lList *requested          - list of attribute requests
*     lList *load_attr          - list of load attributes or null on queue level
*     lList *config_attr        - list of user defined attributes
*     lList *actual_attr        - usage of all consumables (RUE_Type)
*     lList *centry_list        - system wide attribute config. list (CE_Type)
*     lListElem *queue          - current queue or nullptr on global/host level
*     bool allow_non_requestable - allow none requestable?
*     char *reason              - error message
*     int reason_size           - max error message size
*     int slots                 - number of slots the job is looking for
*     u_long32 layer            - current layer flag
*     double lc_factor          - load correction factor
*     u_long32 tag              - current layer tag
*     u_long32 *start_time      - in/out argument for start time
*     u_long32 duration         - jobs estimated total run time
*     const char *object_name   - name of the object used for monitoring purposes
*
*  RESULT
*     dispatch_t -
*
*  NOTES
*     MT-NOTES: is not thread save. uses a static buffer
*
*  Important:
*     we have some special behavior, when slots is set to -1.
*******************************************************************************/
static dispatch_t
rc_time_by_slots(const sge_assignment_t *a, lList *requested, const lList *load_attr, const lList *config_attr,
                 const lList *actual_attr, lListElem *queue, bool allow_non_requestable,
                 dstring *reason, int slots, u_long32 layer, double lc_factor, u_long32 tag,
                 u_long64 *start_time, const char *object_name)
{
   static lListElem *implicit_slots_request = nullptr;
   lListElem *attr;
   u_long64 latest_time = DISPATCH_TIME_NOW;
   u_long64 tmp_start;
   dispatch_t ret;
   bool is_not_found = false;

   DENTER(TOP_LAYER);

   clear_resource_tags(requested, QUEUE_TAG);

   /* ensure availability of implicit slot request */
   if (!implicit_slots_request) {
      implicit_slots_request = lCreateElem(CE_Type);
      lSetString(implicit_slots_request, CE_name, SGE_ATTR_SLOTS);
      lSetString(implicit_slots_request, CE_stringval, "1");
      lSetDouble(implicit_slots_request, CE_doubleval, 1);
   }

   /* match number of free slots */
   if (slots != -1) {
      tmp_start = *start_time;
      ret = ri_time_by_slots(a, implicit_slots_request, load_attr, config_attr, actual_attr, queue,
                       reason, allow_non_requestable, slots, layer, lc_factor, &tmp_start, object_name);

      if (ret == DISPATCH_OK && *start_time == DISPATCH_TIME_QUEUE_END) {
         DPRINTF("%s: \"slot\" request delays start time from " sge_u64
           " to " sge_u64 "\n", object_name, latest_time, MAX(latest_time, tmp_start));
         latest_time = MAX(latest_time, tmp_start);
      }

      /* we don't care if slots are not specified, except at queue level */
      if (ret == DISPATCH_MISSING_ATTR && tag != QUEUE_TAG) {
         ret = DISPATCH_OK;
      }
      if (ret != DISPATCH_OK) {
         DRETURN(ret);
      }

      /* ensure all default requests are fulfilled */
      if (!allow_non_requestable) {
         dispatch_t ff;
         const char *name;
         double dval=0.0;
         u_long32 valtype;

         for_each_rw (attr, actual_attr) {
            name = lGetString(attr, RUE_name);
            if (!strcmp(name, "slots")) {
               continue;
            }

            /* consumable && used in this global/host/queue && not requested */
            if (!is_requested(requested, name)) {
               lListElem *default_request = lGetElemStrRW(a->centry_list, CE_name, name);
               const char *def_req = lGetString(default_request, CE_defaultval);
               valtype = lGetUlong(default_request, CE_valtype);
               parse_ulong_val(&dval, nullptr, valtype, def_req, nullptr, 0);

               /* ignore default request if the value is 0 */
               if ((def_req != nullptr && dval != 0.0) || lGetUlong(default_request, CE_relop) == CMPLXEXCL_OP) {
                  dstring tmp_reason;
                  char tmp_reason_buf[2048];

                  sge_dstring_init(&tmp_reason, tmp_reason_buf, sizeof(tmp_reason_buf));

                  /* build the default request */
                  lSetString(default_request, CE_stringval, def_req);
                  lSetDouble(default_request, CE_doubleval, dval);

                  tmp_start = *start_time;
                  ff = ri_time_by_slots(a, default_request, load_attr, config_attr, actual_attr,
                        queue, &tmp_reason, true, slots, layer, lc_factor, &tmp_start, object_name);

                  if (ff != DISPATCH_OK) {
                     /* useless to continue in these cases */
                     sge_dstring_append(reason, MSG_SCHEDD_FORDEFAULTREQUEST);
                     sge_dstring_append_dstring(reason, &tmp_reason);
                     DRETURN(ff);
                  }

                  if (*start_time == DISPATCH_TIME_QUEUE_END) {
                     DPRINTF("%s: default request \"%s\" delays start time from " sge_u64
                           " to " sge_u64 "\n", object_name, name, latest_time, MAX(latest_time, tmp_start));
                     latest_time = MAX(latest_time, tmp_start);
                  }
               }
            }
         }/* end for*/
      }
   } else {
      slots = 1;
   }
   /* explicit requests */
   for_each_rw (attr, requested) {
      const char *attr_name = lGetString(attr, CE_name);

      tmp_start = *start_time;
      switch (ri_time_by_slots(a, attr, load_attr, config_attr, actual_attr, queue,
               reason, allow_non_requestable, slots, layer, lc_factor, &tmp_start, object_name)) {

         case DISPATCH_NEVER_CAT : /* will never match */
                  DRETURN(DISPATCH_NEVER_CAT);

         case DISPATCH_OK : /* a match was found */
               if (*start_time == DISPATCH_TIME_QUEUE_END) {
                  DPRINTF("%s: explicit request \"%s\" delays start time from " sge_u64
                          " to " sge_u64 "\n", object_name, attr_name, latest_time, MAX(latest_time, tmp_start));
                  latest_time = MAX(latest_time, tmp_start);
               }
               if (lGetUlong(attr, CE_tagged) < tag && tag != RQS_TAG) {
                  lSetUlong(attr, CE_tagged, tag);
               }
            break;

         case DISPATCH_NOT_AT_TIME : /* will match later-on */
                  DPRINTF("%s: request for %s will match later-on\n", object_name, attr_name);
                  DRETURN(DISPATCH_NOT_AT_TIME);

         case DISPATCH_MISSING_ATTR : /* the requested element does not exist */
            if (tag == QUEUE_TAG && lGetUlong(attr, CE_tagged) == NO_TAG) {
               sge_dstring_sprintf(reason, MSG_SCHEDD_JOBREQUESTSUNKNOWNRESOURCE_S, attr_name);
               DRETURN(DISPATCH_NEVER_CAT);
            }

            if (tag != QUEUE_TAG) {
               is_not_found = true;
            }

            break;
         default: /* error */
            break;
      }
   }

   // @todo CS-731 if DISPATCH_OK and for DOMINANT_LAYER_HOST place for additional binding specific checks

   if (*start_time == DISPATCH_TIME_QUEUE_END) {
      *start_time = latest_time;
   }

   if (is_not_found) {
      DRETURN(DISPATCH_MISSING_ATTR);
   }

   DRETURN(DISPATCH_OK);
}

static dispatch_t
match_static_resource(int slots, lListElem *req_cplx, lListElem *src_cplx, dstring *reason,
                      bool allow_non_requestable)
{
   int match;
   dispatch_t ret = DISPATCH_OK;
   char availability_text[2048];

   DENTER(TOP_LAYER);

   /* check whether attrib is requestable */
   if (!allow_non_requestable && lGetUlong(src_cplx, CE_requestable) == REQU_NO) {
      sge_dstring_append(reason, MSG_SCHEDD_JOBREQUESTSNONREQUESTABLERESOURCE);
      sge_dstring_append(reason, lGetString(src_cplx, CE_name));
      sge_dstring_append(reason, "\"");
      DRETURN(DISPATCH_NEVER_CAT);
   }

   match = compare_complexes(slots, req_cplx, src_cplx, availability_text, false, false);

   if (!match) {
      sge_dstring_append(reason, MSG_SCHEDD_ITOFFERSONLY);
      sge_dstring_append(reason, availability_text);
      ret = DISPATCH_NEVER_CAT;
   }

   DRETURN(ret);
}

/****** sge_select_queue/clear_resource_tags() *********************************
*  NAME
*     clear_resource_tags() -- removes the tags from a resource request.
*
*  SYNOPSIS
*     static void clear_resource_tags(lList *resources, u_long32 max_tag)
*
*  FUNCTION
*     Removes the tags from the given resource list. A tag is only removed
*     if it is smaller or equal to the given tag value. The tag value "MAX_TAG" results
*     in removing all existing tags, or the value "HOST_TAG" removes queue and host
*     tags but keeps the global tags.
*
*  INPUTS
*     lList *resources  - list of job requests.
*     u_long32 max_tag - max tag element
*
*******************************************************************************/
static void clear_resource_tags(lList *resources, u_long32 max_tag) {

   lListElem *attr;
   for_each_rw (attr, resources) {
      if (lGetUlong(attr, CE_tagged) <= max_tag) {
         lSetUlong(attr, CE_tagged, NO_TAG);
      }
   }
}

static void clear_resource_tags(lListElem *job, u_long32 max_tag) {
   lListElem *jrs;
   for_each_rw (jrs, lGetListRW(job, JB_request_set_list)) {
      clear_resource_tags(lGetListRW(jrs, JRS_hard_resource_list), max_tag);
   }
}

static void
get_hard_queue_lists(const lListElem *job, const lList *&global_hard_queue_list, const lList *&master_hard_queue_list,
                     const lList *&slave_hard_queue_list) {
   /* we can either have
    * - a global queue request list (can not be combined with master or slave queue request list)
    * - in case of parallel job: a master queue request list and/or a slave queue request list
    */
   global_hard_queue_list = job_get_hard_queue_list(job, JRS_SCOPE_GLOBAL);
   if (/* it is a pe job && */ global_hard_queue_list == nullptr) {
      master_hard_queue_list = job_get_hard_queue_list(job, JRS_SCOPE_MASTER);
      slave_hard_queue_list = job_get_hard_queue_list(job, JRS_SCOPE_SLAVE);
   } else {
      master_hard_queue_list = nullptr;
      slave_hard_queue_list = nullptr;
   }
}

static void
get_soft_queue_list(const lListElem *job, const lList *&soft_queue_list) {
   const lList *master_hard_queue_list = job_get_hard_queue_list(job, JRS_SCOPE_MASTER);
   if (master_hard_queue_list != nullptr) {
      // if we have master queue requests, then work on the soft slave queue requests
      soft_queue_list = job_get_soft_queue_list(job, JRS_SCOPE_SLAVE);
   } else {
      // if we have no master queue requests (or it is a sequential job), then work on the global soft queue requests
      soft_queue_list = job_get_soft_queue_list(job, JRS_SCOPE_GLOBAL);
   }
}
/****** sge_select_queue/sge_queue_match_static() ************************
*  NAME
*     sge_queue_match_static() -- Do matching that depends not on time.
*
*  SYNOPSIS
*     static int sge_queue_match_static(lListElem *queue, lListElem *job,
*     const lListElem *pe, const lListElem *ckpt, lList *centry_list, lList
*     *host_list, lList *acl_list)
*
*  FUNCTION
*     Checks if a job fits on a queue or not. All checks that depend on the
*     current load and resource situation must get handled outside.
*     The queue also gets tagged in QU_tagged4schedule to indicate whether it
*     is specified using -masterq queue_list.
*
*  INPUTS
*     lListElem *queue      - The queue we're matching
*     lListElem *job        - The job
*     const lListElem *pe   - The PE object
*     const lListElem *ckpt - The ckpt object
*     lList *centry_list    - The centry list
*     lList *acl_list       - The ACL list
*
*  RESULT
*     dispatch_t - DISPATCH_OK, ok
*                  DISPATCH_NEVER_CAT, assignment will never be possible for all jobs of that category
*
*  NOTES
*******************************************************************************/

dispatch_t sge_queue_match_static(const sge_assignment_t *a, lListElem *queue)
{
   u_long32 ar_id;
   const lList *projects;
   lListElem *ar_ep;
   const char *qinstance_name = lGetString(queue, QU_full_name);

   DENTER(TOP_LAYER);

   /* check if queue was reserved for AR job */
   ar_id = lGetUlong(a->job, JB_ar);
   ar_ep = lGetElemUlongRW(a->ar_list, AR_id, ar_id);

   if (ar_ep != nullptr) {
      DPRINTF("searching for queue %s\n", qinstance_name);

      if (lGetSubStr(ar_ep, QU_full_name, qinstance_name, AR_reserved_queues) == nullptr) {
         schedd_mes_add_global(a->monitor_alpp, a->monitor_next_run,
                               SCHEDD_INFO_QNOTARRESERVED_SI, qinstance_name, ar_id);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   } else {
      /* this is not advance reservation job, we have to drop queues in orphaned state */
      if (lGetUlong(queue, QU_state) == QI_ORPHANED) {
         schedd_mes_add_global(a->monitor_alpp, a->monitor_next_run, SCHEDD_INFO_QUEUENOTAVAIL_, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /* check if job owner has access rights to the queue */
   if (!sge_has_access(a->user, a->group, a->grp_list, queue, a->acl_list)) {
      DPRINTF("Job %d has no permission for queue %s\n", (int)a->job_id, qinstance_name);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run,
                     a->job_id, SCHEDD_INFO_HASNOPERMISSION_SS,
                     "queue", qinstance_name);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* check if job can run in queue based on project */
   if ((projects = lGetList(queue, QU_projects))) {
      if (!a->project) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_HASNOPRJ_S,
                        "queue", qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
      if ((!prj_list_locate(projects, a->project))) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_HASINCORRECTPRJ_SSS,
                        a->project, "queue", qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /* check if job can run in queue based on excluded projects */
   if ((projects = lGetList(queue, QU_xprojects))) {
      if (a->project && prj_list_locate(projects, a->project)) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_EXCLPRJ_SSS,
                        a->project, "queue", qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   const lList *global_hard_queue_list;
   const lList *master_hard_queue_list;
   const lList *slave_hard_queue_list;
   get_hard_queue_lists(a->job, global_hard_queue_list, master_hard_queue_list, slave_hard_queue_list);
   // if we have any queue request lists then check if queues may be requested at all
   if (global_hard_queue_list != nullptr || master_hard_queue_list != nullptr || slave_hard_queue_list != nullptr) {
      if (!centry_list_are_queues_requestable(a->centry_list)) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_QUEUENOTREQUESTABLE_S, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   // is queue rejected by a global queue list?
   if (global_hard_queue_list != nullptr) {
      if (qref_list_cq_rejected(global_hard_queue_list, lGetString(queue, QU_qname),
                                lGetHost(queue, QU_qhostname), a->hgrp_list)) {
         DPRINTF("Queue " SFQ " is not contained in the global hard queue list (-q) "
                              "that was requested by job " sge_u32 "\n",
                              qinstance_name, a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTINHARDQUEUELST_S, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   } else {
      // if there is no global queue list, we might have master and/or slave queue requests
      bool can_be_master_queue = true;
      bool can_be_slave_queue = true;

      // is this queue a candidate for being the master queue?
      if (master_hard_queue_list != nullptr) {
         if (qref_list_cq_rejected(master_hard_queue_list, lGetString(queue, QU_qname),
                                   lGetHost(queue, QU_qhostname), a->hgrp_list)) {
            DPRINTF("Queue " SFQ " is not contained in the master hard "
                                 "queue list that was requested by job " sge_u32 "\n",
                    qinstance_name, a->job_id);
            can_be_master_queue = false;
            lClearUlongBitMask(queue, QU_tagged4schedule, TAG4SCHED_MASTER | TAG4SCHED_MASTER_LATER);
         }
      }

      // is this queue a candidate for being a slave queue?
      if (slave_hard_queue_list != nullptr) {
         if (qref_list_cq_rejected(slave_hard_queue_list, lGetString(queue, QU_qname),
                                   lGetHost(queue, QU_qhostname), a->hgrp_list)) {
            DPRINTF("Queue " SFQ " is not contained in the slave hard "
                                 "queue list (-q) that was requested by job " sge_u32 "\n",
                    qinstance_name, (int) a->job_id);
            can_be_slave_queue = false;
            // @todo CS-601 slave tags not yet used
            // lClearUlongBitMask(queue, QU_tagged4schedule, TAG4SCHED_SLAVE | TAG4SCHED_SLAVE_LATER);
         }
      }

      // if it can be neither master nor slave queue, then it is not a candidate at all
      if (!can_be_master_queue && !can_be_slave_queue) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTINHARDQUEUELST_S, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /*
   ** different checks for different job types:
   */

   if (a->pe != nullptr) { /* parallel job */
      if (!qinstance_is_parallel_queue(queue)) {
         DPRINTF("Queue \"%s\" is not a parallel queue as requested by job %d\n", qinstance_name, (int)a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTPARALLELQUEUE_S, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }

      /*
       * check if the requested PE is named in the PE reference list of Queue
       */
      if (!qinstance_is_pe_referenced(queue, a->pe)) {
         DPRINTF("Queue " SFQ " does not reference PE " SFQ "\n", qinstance_name, lGetString(a->pe, PE_name));
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTINQUEUELSTOFPE_SS,
                        qinstance_name, lGetString(a->pe, PE_name));
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   if (a->ckpt != nullptr) { /* ckpt job */
      /* is it a ckpt queue ? */
      if (!qinstance_is_checkpointing_queue(queue)) {
         DPRINTF("Queue \"%s\" is not a checkpointing queue as requested by job %d\n", qinstance_name, (int)a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTACKPTQUEUE_SS, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }

      /*
       * check if the requested CKPT is named in the CKPT ref list of Queue
       */
      if (!qinstance_is_ckpt_referenced(queue, a->ckpt)) {
         DPRINTF("Queue \"%s\" does not reference checkpointing object " SFQ "\n", qinstance_name, lGetString(a->ckpt, CK_name));
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTINQUEUELSTOFCKPT_SS,
                        qinstance_name, lGetString(a->ckpt, CK_name));
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /* to be activated as soon as immediate jobs are available */
   if (JOB_TYPE_IS_IMMEDIATE(lGetUlong(a->job, JB_type))) {
      if (!qinstance_is_interactive_queue(queue)) {
         DPRINTF("Queue \"%s\" is not an interactive queue as requested by job %d\n", qinstance_name, (int)a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_QUEUENOTINTERACTIVE_S, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   if (!a->pe && !a->ckpt && !JOB_TYPE_IS_IMMEDIATE(lGetUlong(a->job, JB_type))) { /* serial (batch) job */
      /* is it a batch or transfer queue */
      if (!qinstance_is_batch_queue(queue)) {
         DPRINTF("Queue \"%s\" is not a batch queue as requested by job %d\n", qinstance_name, (int)a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_NOTASERIALQUEUE_S, qinstance_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /* RD: I don't understand this condition */
   if (a->ckpt && !a->pe && !JOB_TYPE_IS_IMMEDIATE(lGetUlong(a->job, JB_type)) &&
       qinstance_is_parallel_queue(queue) && !qinstance_is_batch_queue(queue)) {
      DPRINTF("Queue \"%s\" is not a serial queue as requested by job %d\n", qinstance_name, (int)a->job_id);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                     SCHEDD_INFO_NOTPARALLELJOB_S, qinstance_name);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   if (job_is_forced_centry_missing(a, queue, true)) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   DRETURN(DISPATCH_OK);
}

static bool
job_is_forced_centry_missing(const sge_assignment_t *a, const lListElem *queue_or_host, bool is_qinstance)
{
   DENTER(TOP_LAYER);
   bool ret = false;

   if (a->job != nullptr && a->centry_list != nullptr && queue_or_host != nullptr) {
      const void *iterator = nullptr;
      const lListElem *centry;
      const lListElem *next_centry = lGetElemUlongFirst(a->centry_list, CE_requestable, REQU_FORCED, &iterator);
      while ((centry = next_centry) != nullptr) {
         next_centry = lGetElemUlongNext(a->centry_list, CE_requestable, REQU_FORCED, &iterator);

         // check if the forced centry was requested; if so, we can continue with the next centry
         const char *name = lGetString(centry, CE_name);
         bool skip_further_checks = false;
         const lListElem *jrs;
         for_each_ep (jrs, lGetList(a->job, JB_request_set_list)) {
            const lList *res_list = lGetList(jrs, JRS_hard_resource_list);
            if (is_requested(res_list, name)) {
               // if requested we are fine
               skip_further_checks = true;
               break;
            }
         }
         if (skip_further_checks) {
            continue;
         }

         // if the centry was not requested then we need to complain about it if it is defined in the current object (queue or host)
         if (is_qinstance) {
            if (qinstance_is_centry_a_complex_value(queue_or_host, centry)) {
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_NOTREQFORCEDRES_SS,
                              name, lGetString(queue_or_host, QU_full_name));
               ret = true;
               break;
            }
         } else {
            if (host_is_centry_a_complex_value(queue_or_host, centry)) {
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_NOFORCEDRES_SS,
                              name, lGetHost(queue_or_host, EH_name));
               ret = true;
               break;

            }
         }
      } // end loop over all complex definitions
   }
   DRETURN(ret);
}

/****** sge_select_queue/compute_soft_violations() ********************************
*  NAME
*     compute_soft_violations() -- counts the violations in the request for a given host or queue
*
*  SYNOPSIS
*     static int compute_soft_violations(lListElem *queue, int violation, lListElem *job,lList *load_attr, lList *config_attr,
*                               lList *actual_attr, lList *centry_list, u_long32 layer, double lc_factor, u_long32 tag)
*
*  FUNCTION
*     this function checks if the current resources can satisfy the requests. The resources come from the global host, a
*     given host or the queue. The function returns the number of violations.
*
*  INPUTS
*     const sge_assignment_t *a - job info structure
*     lListElem *queue     - should only be set, when one using this method on queue level
*     int violation        - the number of previous violations. This is needed to get a correct result on queue level.
*     lList *load_attr     - the load attributes, only when used on hosts or global
*     lList *config_attr   - a list of custom attributes  (CE_Type)
*     lList *actual_attr   - a list of custom consumables, they contain the current usage of these attributes (RUE_Type)
*     u_long32 layer       - the current layer flag
*     double lc_factor     - should be set, when load correction has to be done.
*     u_long32 tag         - the current layer tag. (GLOBAL_TAG, HOST_TAG, QUEUE_TAG)
*
*  RESULT
*     static int - the number of violations ( = (prev. violations) + (new violations in this run)).
*
*******************************************************************************/
static int
compute_soft_violations(const sge_assignment_t *a, lListElem *queue, int violation,
                        const lList *load_attr, const lList *config_attr,
                        const lList *actual_attr, u_long32 layer, double lc_factor, u_long32 tag)
{
   const char *queue_name = nullptr;
   DSTRING_STATIC(reason, 1024 + 1);
   unsigned int soft_violation = violation;
   u_long64 start_time = DISPATCH_TIME_NOW;

   DENTER(TOP_LAYER);

   lList *soft_requests = job_get_soft_resource_listRW(a->job);
   clear_resource_tags(soft_requests, tag);

   u_long32 job_id = a->job_id;
   if (queue != nullptr) {
      queue_name = lGetString(queue, QU_full_name);
   }

   /* count number of soft violations for _one_ slot of this job */
   lListElem *attr;
   for_each_rw (attr, soft_requests) {
      switch (ri_time_by_slots(a, attr, load_attr, config_attr, actual_attr, queue, &reason, false, 1, layer,
                               lc_factor, &start_time, queue_name?queue_name:"no queue")) {
            /* no match */
            case DISPATCH_NEVER_CAT:
               soft_violation++;
               break;
            /* element not found */
            case DISPATCH_MISSING_ATTR:
            case DISPATCH_NOT_AT_TIME:
               // we are checking on queue layer (the top layer)
               //                      and the attribute hasn't matched on a lower layer (global, exechost)
               if (tag == QUEUE_TAG && lGetUlong(attr, CE_tagged) == NO_TAG) {
                  soft_violation++;
               }
               break;
            /* everything is fine */
            default:
               // tag the attribute to remember that it has matched on this layer
               if (lGetUlong(attr, CE_tagged) < tag) {
                  lSetUlong(attr, CE_tagged, tag);
               }
      }
   }

   if (queue != nullptr) {
      DPRINTF("queue %s does not fulfill soft %d requests (first: %s)\n", queue_name, soft_violation,
              sge_dstring_get_string(&reason));

       // check whether queue fulfills soft queue request of the job (-q)
      const lList *qref_list;
      get_soft_queue_list(a->job, qref_list);
      if (qref_list != nullptr) {
         const char *qinstance_name = lGetString(queue, QU_full_name);
         const lListElem *qr;
         for_each_ep(qr, qref_list) {
            if (qref_cq_rejected(lGetString(qr, QR_name), lGetString(queue, QU_qname), lGetHost(queue, QU_qhostname), a->hgrp_list)) {
               DPRINTF("Queue \"%s\" is not contained in the soft queue list (-q) that was requested by job " sge_u32 "\n",
                        qinstance_name, job_id);
               soft_violation++;
            }
         }
      }

      /* store number of soft violations in queue */
      lSetUlong(queue, QU_soft_violation, soft_violation);
   }

   DRETURN(soft_violation);
}

/****** sge_select_queue/sge_host_match_static() ********************************
*  NAME
*     sge_host_match_static() -- Static test whether job fits to host
*
*  SYNOPSIS
*     static int sge_host_match_static(lListElem *job, lListElem *ja_task,
*     lListElem *host, lList *centry_list, lList *acl_list)
*
*  FUNCTION
*
*  INPUTS
*     lListElem *job     - ???
*     lListElem *ja_task - ???
*     lListElem *host    - ???
*     lList *centry_list - ???
*     lList *acl_list    - ???
*
*  RESULT
*     int - 0 ok
*          -1 assignment will never be possible for all jobs of that category
*          -2 assignment will never be possible for that particular job
*******************************************************************************/
dispatch_t
sge_host_match_static(const sge_assignment_t *a, const lListElem *host)
{
   const lList *projects;
   const char *eh_name;

   DENTER(TOP_LAYER);

   if (!host) {
      DRETURN(DISPATCH_OK);
   }

   eh_name = lGetHost(host, EH_name);

   /* check if job owner has access rights to the host */
   if (!sge_has_access_(a->user, a->group, a->grp_list, lGetList(host, EH_acl),
         lGetList(host, EH_xacl), a->acl_list)) {
      DPRINTF("Job %d has no permission for host %s\n", (int)a->job_id, eh_name);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                     SCHEDD_INFO_HASNOPERMISSION_SS, "host", eh_name);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* check if job can run on host based on required projects */
   if ((projects = lGetList(host, EH_prj))) {

      if (!a->project) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_HASNOPRJ_S, "host", eh_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }

      if ((!prj_list_locate(projects, a->project))) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_HASINCORRECTPRJ_SSS, a->project, "host", eh_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   /* check if job can run on host based on excluded projects */
   if ((projects = lGetList(host, EH_xprj))) {
      if (a->project && prj_list_locate(projects, a->project)) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_EXCLPRJ_SSS, a->project, "host", eh_name);
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   if (job_is_forced_centry_missing(a, host, false)) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* RU: */
   /*
   ** check if job can run on host based on the list of jids/taskids
   ** contained in the reschedule_unknown-list
   */
   if (a->ja_task) {
      const lListElem *ruep;
      u_long32 task_id = lGetUlong(a->ja_task, JAT_task_number);
      const lList *rulp = lGetList(host, EH_reschedule_unknown_list);

      for_each_ep(ruep, rulp) {
         if (lGetUlong(ruep, RU_job_number) == a->job_id
             && lGetUlong(ruep, RU_task_number) == task_id) {
            DPRINTF("RU: Job " sge_u32"." sge_u32" Host " SFN "\n", a->job_id, task_id, eh_name);
            schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                           SCHEDD_INFO_CLEANUPNECESSARY_S, eh_name);
            DRETURN(DISPATCH_NEVER_JOB);
         }
      }
   }

   DRETURN(DISPATCH_OK);
}

/****** sge_select_queue/is_requested() ****************************************
*  NAME
*     is_requested() -- Returns true if specified resource is requested.
*
*  SYNOPSIS
*     bool is_requested(lList *req, const char *attr)
*
*  FUNCTION
*     Returns true if specified resource is requested. Both long name
*     and shortcut name are checked.
*
*  INPUTS
*     lList *req       - The request list (CE_Type)
*     const char *attr - The resource name.
*
*  RESULT
*     bool - true if requested, otherwise false
*
*  NOTES
*     MT-NOTE: is_requested() is MT safe
*******************************************************************************/
bool is_requested(const lList *req, const char *attr)
{
   if (req != nullptr) {
      if (lGetElemStr(req, CE_name, attr) ||
          lGetElemStr(req, CE_shortcut, attr)) {
         return true;
      }
   }

   return false;
}


bool is_requested(const lListElem *job, const char *attr)
{
   bool ret = false;

   const lList *global_request_list = job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL);
   if (is_requested(global_request_list, attr)) {
      ret = true;
   } else {
      const lList *master_request_list = job_get_hard_resource_list(job, JRS_SCOPE_MASTER);
      const lList *slave_request_list = job_get_hard_resource_list(job, JRS_SCOPE_SLAVE);
      if (is_requested(master_request_list, attr) &&
          is_requested(slave_request_list, attr)) {
         ret = true;
      }
   }

   return ret;
}

static int
load_check_alarm(char *reason, size_t reason_size, const char *name, const char *load_value, const char *limit_value,
                 u_long32 relop, u_long32 type, lListElem *hep, const lListElem *hlep, double lc_host, double lc_global,
                 const lList *load_adjustments, int load_is_value)
{
   const lListElem *job_load;
   double limit, load;
   int match;
#define STR_LC_DIAGNOSIS 1024
   char lc_diagnosis1[STR_LC_DIAGNOSIS], lc_diagnosis2[STR_LC_DIAGNOSIS];

   DENTER(TOP_LAYER);

   switch (type) {
      case TYPE_RSMAP:
      case TYPE_INT:
      case TYPE_TIM:
      case TYPE_MEM:
      case TYPE_BOO:
      case TYPE_DOUBLE:
         if (!parse_ulong_val(&load, nullptr, type, load_value, nullptr, 0)) {
            if (reason)
               snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDINVALIDLOAD_SS, load_value, name);
            DRETURN(1);
         }
         if (!parse_ulong_val(&limit, nullptr, type, limit_value, nullptr, 0)) {
            if (reason)
               snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDINVALIDTHRESHOLD_SS, name, limit_value);
            DRETURN(1);
         }
         if (load_is_value) { /* we got no load - this is just the complex value */
            sge_strlcpy(lc_diagnosis2, MSG_SCHEDD_LCDIAGNOLOAD, STR_LC_DIAGNOSIS);
         } else if (((hlep && lc_host) || lc_global) &&
            (job_load = lGetElemStr(load_adjustments, CE_name, name))) { /* load correction */
            const char *load_correction_str;
            double load_correction;

            load_correction_str = lGetString(job_load, CE_stringval);
            if (!parse_ulong_val(&load_correction, nullptr, type, load_correction_str, nullptr, 0)) {
               if (reason)
                  snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDINVALIDLOADADJUST_SS, name, load_correction_str);
               DRETURN(1);
            }

            if (hlep) {
               int nproc;
               load_correction *= lc_host;

               if ((nproc = load_np_value_adjustment(name, hep,  &load_correction)) > 0) {
                  snprintf(lc_diagnosis1, sizeof(lc_diagnosis1), MSG_SCHEDD_LCDIAGHOSTNP_SFI, load_correction_str, lc_host, nproc);
               }
               else {
                  snprintf(lc_diagnosis1, sizeof(lc_diagnosis1), MSG_SCHEDD_LCDIAGHOST_SF, load_correction_str, lc_host);

               }

            }
            else {
               load_correction *= lc_global;
               snprintf(lc_diagnosis1, sizeof(lc_diagnosis1), MSG_SCHEDD_LCDIAGGLOBAL_SF, load_correction_str, lc_global);
            }
            /* it depends on relop in complex config
            whether load_correction is pos/neg */
            switch (relop) {
            case CMPLXGE_OP:
            case CMPLXGT_OP:
               load += load_correction;
               snprintf(lc_diagnosis2, sizeof(lc_diagnosis2), MSG_SCHEDD_LCDIAGPOSITIVE_SS, load_value, lc_diagnosis1);
               break;

            case CMPLXNE_OP:
            case CMPLXEQ_OP:
            case CMPLXLT_OP:
            case CMPLXLE_OP:
            default:
               load -= load_correction;
               snprintf(lc_diagnosis2, sizeof(lc_diagnosis2), MSG_SCHEDD_LCDIAGNEGATIVE_SS, load_value, lc_diagnosis1);
               break;
            }
         } else  {
            sge_strlcpy(lc_diagnosis2, MSG_SCHEDD_LCDIAGNONE, STR_LC_DIAGNOSIS);
         }

         /* is threshold exceeded ? */
         if (resource_cmp(relop, load, limit)) {
            if (reason) {
               if (type == TYPE_BOO){
                  snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDBOOLVALUE_SSSSS, name,
                           load ? MSG_TRUE : MSG_FALSE, lc_diagnosis2, map_op2str(relop), limit_value);
               }
               else {
                  snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDFLOATVALUE_SFSSS,
                           name, load, lc_diagnosis2, map_op2str(relop), limit_value);
               }
            }
            DRETURN(1);
         }
         break;

      case TYPE_STR:
      case TYPE_CSTR:
      case TYPE_HOST:
      case TYPE_RESTR:
         match = string_base_cmp(type, limit_value, load_value);
         if (!match) {
            if (reason)
               snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDSTRINGVALUE_SSSS, name, load_value, map_op2str(relop), limit_value);
            DRETURN(1);
         }
         break;
      default:
         if (reason)
            snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDCOMPLEXTYPE_S, name);
         DRETURN(1);
   }

   DRETURN(0);
}

/****** sge_select_queue/load_np_value_adjustment() ****************************
*  NAME
*     load_np_value_adjustment() -- adjusts np load values for the number of processors
*
*  SYNOPSIS
*     static int load_np_value_adjustment(const char* name, lListElem *hep,
*     double *load_correction)
*
*  FUNCTION
*     Tests the load value name for "np_*". If this pattern is found, it will
*     retrieve the number of processors and adjusts the load_correction accordingly.
*     If the pattern is not found, it does nothing and returns 0 for number of processors.
*
*  INPUTS
*     const char* name        - load value name
*     lListElem *hep          - host object
*     double *load_correction - current load_correction for further corrections
*
*  RESULT
*     static int - number of processors, or 0 if it was called on a none np load value
*
*  NOTES
*     MT-NOTE: load_np_value_adjustment() is MT safe
*
*******************************************************************************/
static int load_np_value_adjustment(const char* name, lListElem *hep, double *load_correction) {

   int nproc = 1;
   if (!strncmp(name, "np_", 3)) {
      const lListElem *ep_nproc = lGetSubStr(hep, HL_name, LOAD_ATTR_NUM_PROC, EH_load_list);

      if (ep_nproc != nullptr) {
         const char* cp = lGetString(ep_nproc, HL_value);
         if (cp) {
            nproc = atoi(cp);

            if (nproc > 1) {
               *load_correction /= nproc;
            }
         }
      }
   } else {
      nproc = 0;
   }

   return nproc;
}

static int resource_cmp(u_long32 relop, double req, double src_dl)
{
   int match;

   switch(relop) {
   case CMPLXEQ_OP :
      match = ( req==src_dl);
      break;
   case CMPLXLE_OP :
      match = ( req<=src_dl);
      break;
   case CMPLXLT_OP :
      match = ( req<src_dl);
      break;
   case CMPLXGT_OP :
      match = ( req>src_dl);
      break;
   case CMPLXGE_OP :
      match = ( req>=src_dl);
      break;
   case CMPLXNE_OP :
      match = ( req!=src_dl);
      break;
   default:
      match = 0; /* default -> no match */
   }

   return match;
}

/* ----------------------------------------

   sge_load_alarm()

   checks given threshold of the queue;
   centry_list and exechost_list get used
   therefore

   returns boolean:
      1 yes, the threshold is exceeded
      0 no
*/

int
sge_load_alarm(char *reason, size_t reason_size, const lListElem *qep, const lList *threshold,
               const lList *exechost_list, const lList *centry_list,
               const lList *load_adjustments, bool is_check_consumable)
{
   lListElem *hep, *global_hep;
   const lListElem *tep;
   u_long32 ulc_factor;
   const char *load_value = nullptr;
   const char *limit_value = nullptr;
   double lc_host = 0, lc_global = 0;
   int load_is_value = 0;

   DENTER(TOP_LAYER);

   if (!threshold) {
      /* no threshold -> no alarm */
      DRETURN(0);
   }

   hep = host_list_locate(exechost_list, lGetHost(qep, QU_qhostname));

   if(!hep) {
      if (reason)
         snprintf(reason, sizeof(reason_size), MSG_SCHEDD_WHYEXCEEDNOHOST_S, lGetHost(qep, QU_qhostname));
      /* no host for queue -> ERROR */
      DRETURN(1);
   }

   if ((lGetPosViaElem(hep, EH_load_correction_factor, SGE_NO_ABORT) >= 0)
       && (ulc_factor=lGetUlong(hep, EH_load_correction_factor))) {
      lc_host = ((double)ulc_factor)/100;
   }

   if ((global_hep = host_list_locate(exechost_list, SGE_GLOBAL_NAME)) != nullptr) {
      if ((lGetPosViaElem(global_hep, EH_load_correction_factor, SGE_NO_ABORT) >= 0)
          && (ulc_factor=lGetUlong(global_hep, EH_load_correction_factor)))
         lc_global = ((double)ulc_factor)/100;
   }

   for_each_ep(tep, threshold) {
      const lListElem *hlep = nullptr;
      const lListElem *glep = nullptr;
      const lListElem *queue_ep = nullptr;
      lListElem *cep  = nullptr;
      bool need_free_cep = false;
      const char *name;
      u_long32 relop, type;

      name = lGetString(tep, CE_name);
      /* complex attriute definition */

      if (!(cep = centry_list_locate(centry_list, name))) {
         if (reason) {
            snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDNOCOMPLEX_S, name);
         }
         DRETURN(1);
      }
      if (!is_check_consumable && lGetUlong(cep, CE_consumable) != CONSUMABLE_NO) {
         continue;
      }

      if (hep != nullptr) {
         hlep = lGetSubStr(hep, HL_name, name, EH_load_list);
      }

      if (lGetUlong(cep, CE_consumable) == CONSUMABLE_NO) {
         if (hlep != nullptr) {
            load_value = lGetString(hlep, HL_value);
            load_is_value = 0;
         } else if ((global_hep != nullptr) &&
                  ((glep = lGetSubStr(global_hep, HL_name, name, EH_load_list)) != nullptr)) {
               load_value = lGetString(glep, HL_value);
               load_is_value = 0;
         } else {
            queue_ep = lGetSubStr(qep, CE_name, name, QU_consumable_config_list);
            if (queue_ep != nullptr) {
               load_value = lGetString(queue_ep, CE_stringval);
               load_is_value = 1;
            } else {
               if (reason) {
                  snprintf(reason, reason_size, MSG_SCHEDD_NOVALUEFORATTR_S, name);
               }
               DRETURN(1);
            }
         }
      } else {
         /* load thesholds... */
         if ((cep = get_attribute_by_name(global_hep, hep, qep, name, centry_list, load_adjustments, DISPATCH_TIME_NOW, 0)) == nullptr ) {
            if (reason)
               snprintf(reason, reason_size, MSG_SCHEDD_WHYEXCEEDNOCOMPLEX_S, name);
            DRETURN(1);
         }
         need_free_cep = true;

         load_value = lGetString(cep, CE_pj_stringval);
         load_is_value = (lGetUlong(cep, CE_pj_dominant) & DOMINANT_TYPE_MASK) != DOMINANT_TYPE_CLOAD;
      }

      relop = lGetUlong(cep, CE_relop);
      limit_value = lGetString(tep, CE_stringval);
      type = lGetUlong(cep, CE_valtype);

      if (load_check_alarm(reason, reason_size, name, load_value, limit_value, relop, type, hep, hlep, lc_host,
                           lc_global, load_adjustments, load_is_value)) {
         if (need_free_cep) {
            lFreeElem(&cep);
         }
         DRETURN(1);
      }
      if (need_free_cep) {
         lFreeElem(&cep);
      }
   }

   DRETURN(0);
}

/* ----------------------------------------

   sge_load_alarm_reasons()

   checks given threshold of the queue;
   centry_list and exechost_list get used
   therefore

   fills and returns string buffer containing reasons for alarm states
*/

char *sge_load_alarm_reason(lListElem *qep, lList *threshold,
                            const lList *exechost_list, const lList *centry_list,
                            char *reason, int reason_size,
                            const char *threshold_type)
{
   DENTER(TOP_LAYER);

   *reason = 0;

   /* no threshold -> no alarm */
   if (threshold != nullptr) {
      lList *rlp = nullptr;
      const lListElem *tep;
      bool first = true;

      /* get actual complex values for queue */
      queue_complexes2scheduler(&rlp, qep, exechost_list, centry_list);

      /* check all thresholds */
      for_each_ep(tep, threshold) {
         const char *name;             /* complex attrib name */
         const lListElem *cep;         /* actual complex attribute */
         char dom_str[5];              /* dominance as string */
         u_long32 dom_val;             /* dominance as u_long */
         char buffer[MAX_STRING_SIZE]; /* buffer for one line */
         const char *load_value;       /* actual load value */
         const char *limit_value;      /* limit defined by threshold */

         name = lGetString(tep, CE_name);

         if (first) {
            first = false;
         } else {
            strncat(reason, "\n\t", reason_size);
         }

         /* find actual complex attribute */
         if ((cep = lGetElemStr(rlp, CE_name, name)) == nullptr) {
            /* no complex attribute for threshold -> ERROR */
            if (qinstance_state_is_unknown(qep)) {
               snprintf(buffer, MAX_STRING_SIZE, MSG_QINSTANCE_VALUEMISSINGMASTERDOWN_S, name);
            } else {
               snprintf(buffer, MAX_STRING_SIZE, MSG_SCHEDD_NOCOMPLEXATTRIBUTEFORTHRESHOLD_S, name);
            }
            strncat(reason, buffer, reason_size);
            continue;
         }

         limit_value = lGetString(tep, CE_stringval);

         if (!(lGetUlong(cep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            dom_val = lGetUlong(cep, CE_pj_dominant);
            load_value = lGetString(cep, CE_pj_stringval);
         } else {
            dom_val = lGetUlong(cep, CE_dominant);
            load_value = lGetString(cep, CE_stringval);
         }

         monitor_dominance(dom_str, dom_val);

         snprintf(buffer, MAX_STRING_SIZE, "alarm %s:%s=%s %s-threshold=%s",
                 dom_str,
                 name,
                 load_value,
                 threshold_type,
                 limit_value
                );

         strncat(reason, buffer, reason_size);
      }

      lFreeList(&rlp);
   }

   DRETURN(reason);
}

/* ----------------------------------------

   sge_split_queue_load()

   splits the incoming queue list (1st arg) into an unloaded and
   overloaded (2nd arg) list according to the load values contained in
   the execution host list (3rd arg) and with respect to the definitions
   in the complex list (4th arg).

   returns:
      0 successful
     -1 errors in functions called by sge_split_queue_load

*/
int sge_split_queue_load(
bool monitor_next_run,
lList **unloaded,               /* QU_Type */
lList **overloaded,             /* QU_Type */
lList *exechost_list,           /* EH_Type */
const lList *centry_list,             /* CE_Type */
const lList *load_adjustments,  /* CE_Type */
lList *granted,                 /* JG_Type */
bool  is_consumable_load_alarm, /* is true, when the consumable evaluation
                                   set a load alarm */
bool is_comprehensive,          /* do the load evaluation comprehensive (include consumables) */
u_long32 ttype
) {
   const lList *thresholds;
   int nverified = 0;
   char reason[2048];

   DENTER(TOP_LAYER);

   /* a job has been dispatched recently,
      but load correction is not in use at all */
   if (granted && !load_adjustments && !is_consumable_load_alarm) {
      DRETURN(0);
   }

   if (!granted || load_adjustments) {
      lListElem *qep, *next_qep;

      next_qep = lFirstRW(*unloaded);
      while ((qep = next_qep)) {
         bool remove_queue = false;
         next_qep = lNextRW(qep);

         /* do not verify load alarm if a job has been dispatched recently
            but not to the host where this queue resides */
         if (lGetUlong(qep, QU_tagged4schedule) == 1) {
            /* this queue is already tagged for removing */
            remove_queue = true;
            lSetUlong(qep, QU_tagged4schedule, 0);
         } else if (!granted || (granted && (sconf_get_global_load_correction() ||
                              lGetElemHost(granted, JG_qhostname, lGetHost(qep, QU_qhostname))))) {
            thresholds = lGetList(qep, ttype);
            nverified++;

            if (sge_load_alarm(reason, sizeof(reason), qep, thresholds, exechost_list, centry_list, load_adjustments, is_comprehensive) != 0) {
               remove_queue = true;
               if (ttype==QU_suspend_thresholds) {
                  DPRINTF("queue %s tagged to be in suspend alarm: %s\n", lGetString(qep, QU_full_name), reason);
                  schedd_mes_add_global(nullptr, monitor_next_run, SCHEDD_INFO_QUEUEINALARM_SS, lGetString(qep, QU_full_name), reason);
               } else {
                  DPRINTF("queue %s tagged to be overloaded: %s\n", lGetString(qep, QU_full_name), reason);
                  schedd_mes_add_global(nullptr, monitor_next_run, SCHEDD_INFO_QUEUEOVERLOADED_SS, lGetString(qep, QU_full_name), reason);
               }
            }
         }
         if (remove_queue) {
            if (overloaded != nullptr) {
               lDechainElem(*unloaded, qep);
               if (*overloaded == nullptr) {
                  *overloaded = lCreateListHash("", lGetListDescr(*unloaded), false);
               }
               lAppendElem(*overloaded, qep);
            } else {
               lRemoveElem(*unloaded, &qep);
            }
         }
      }
   }

   DPRINTF("verified threshold of %d queues\n", nverified);
   DRETURN(0);
}


/****** sge_select_queue/sge_split_queue_slots_free() **************************
*  NAME
*     sge_split_queue_slots_free() -- ???
*
*  SYNOPSIS
*     int sge_split_queue_slots_free(lList **free, lList **full)
*
*  FUNCTION
*     Split queue list into queues with at least one slots and queues with
*     less than one free slot. The list optioally returned in full gets the
*     QNOSLOTS queue instance state set.
*
*  INPUTS
*     lList **free - Input queue instance list and return free slots.
*     lList **full - If non-nullptr the full queue instances get returned here.
*
*  RESULT
*     int - 0 success
*          -1 error
*******************************************************************************/
int sge_split_queue_slots_free(bool monitor_next_run, lList **free, lList **full)
{
   lList *full_queues = nullptr;
   lListElem *thiz = nullptr;
   lListElem *next = nullptr;

   DENTER(TOP_LAYER);

   if (free == nullptr) {
      DRETURN(-1);
   }

   for (thiz=lFirstRW(*free); (next=lNextRW(thiz)), thiz ; thiz = next) {
      if (qinstance_slots_used(thiz) >= (int)lGetUlong(thiz, QU_job_slots)) {

         thiz = lDechainElem(*free, thiz);

         if (!qinstance_state_is_full(thiz)) {
            schedd_mes_add_global(nullptr, monitor_next_run,
                                  SCHEDD_INFO_QUEUEFULL_,
                                  lGetString(thiz, QU_full_name));
            qinstance_state_set_full(thiz, true);

            if (full_queues == nullptr) {
               full_queues = lCreateListHash("full one", lGetListDescr(*free), false);
            }
            lAppendElem(full_queues, thiz);
         } else if (full != nullptr) {
            if (*full == nullptr) {
               *full = lCreateList("full one", lGetListDescr(*free));
            }
            lAppendElem(*full, thiz);
         } else {
            lFreeElem(&thiz);
         }
      }
   }

   /* dump out the -tsm log and add the new queues to the disabled queue list */
   if (full_queues) {
      schedd_log_list(nullptr, monitor_next_run,
                      MSG_SCHEDD_LOGLIST_QUEUESFULLANDDROPPED,
                      full_queues, QU_full_name);
      if (full != nullptr) {
         if (*full == nullptr) {
            *full = full_queues;
         } else {
            lAddList(*full, &full_queues);
         }
      } else {
         lFreeList(&full_queues);
      }
   }

   DRETURN(0);
}


/* ----------------------------------------

   sge_split_suspended()

   splits the incoming queue list (1st arg) into non suspended queues and
   suspended queues (2nd arg)

   returns:
      0 successful
     -1 error

*/
int sge_split_suspended(
bool monitor_next_run,
lList **queue_list,        /* QU_Type */
lList **suspended         /* QU_Type */
) {
   lCondition *where;
   int ret;
   lList *lp = nullptr;

   DENTER(TOP_LAYER);

   if (!queue_list) {
      DRETURN(-1);
   }

   /* split queues */
   where = lWhere("%T(!(%I m= %u) && !(%I m= %u) && !(%I m= %u) && !(%I m= %u))",
      lGetListDescr(*queue_list),
         QU_state, QI_SUSPENDED,
         QU_state, QI_CAL_SUSPENDED,
         QU_state, QI_CAL_DISABLED,
         QU_state, QI_SUSPENDED_ON_SUBORDINATE);

   ret = lSplit(queue_list, &lp, "full queues", where);
   lFreeWhere(&where);

   if (lp != nullptr) {
      lListElem* mes_queue;

      for_each_rw (mes_queue, lp) {
         if (!qinstance_state_is_manual_suspended(mes_queue)) {
            qinstance_state_set_manual_suspended(mes_queue, true);
            schedd_mes_add_global(nullptr, monitor_next_run,
                                  SCHEDD_INFO_QUEUESUSP_,
                                  lGetString(mes_queue, QU_full_name));
         }
      }

      schedd_log_list(nullptr, monitor_next_run,
                      MSG_SCHEDD_LOGLIST_QUEUESSUSPENDEDANDDROPPED,
                      lp, QU_full_name);

      if (*suspended == nullptr) {
         *suspended = lp;
      } else {
         lAddList(*suspended, &lp);
      }
   }

   DRETURN(ret);
}

/* ----------------------------------------

   sge_split_cal_disabled()

   splits the incoming queue list (1st arg) into non disabled queues and
   cal_disabled queues (2nd arg)

   lList **queue_list,       QU_Type
   lList **disabled          QU_Type

   returns:
      0 successful
     -1 errors in functions called by sge_split_queue_load

*/
int
sge_split_cal_disabled(bool monitor_next_run, lList **queue_list, lList **disabled)
{
   lCondition *where;
   int ret;
   lList *lp = nullptr;

   DENTER(TOP_LAYER);

   if (!queue_list) {
      DRETURN(-1);
   }

   /* split queues */
   where = lWhere("%T(!(%I m= %u))", lGetListDescr(*queue_list),
                  QU_state, QI_CAL_DISABLED);
   ret = lSplit(queue_list, &lp, "full queues", where);
   lFreeWhere(&where);

   if (lp != nullptr) {
      lListElem* mes_queue;

      for_each_rw (mes_queue, lp) {
         schedd_mes_add_global(nullptr, monitor_next_run,
                               SCHEDD_INFO_QUEUEDISABLED_,
                               lGetString(mes_queue, QU_full_name));
      }

      schedd_log_list(nullptr, monitor_next_run,
                      MSG_SCHEDD_LOGLIST_QUEUESDISABLEDANDDROPPED,
                      lp, QU_full_name);

      if (*disabled == nullptr) {
         *disabled = lp;
      } else {
         lAddList(*disabled, &lp);
      }
   }

   DRETURN(ret);
}

/* ----------------------------------------

   sge_split_disabled()

   splits the incoming queue list (1st arg) into non disabled queues and
   disabled queues (2nd arg)

   lList **queue_list,       QU_Type
   lList **disabled          QU_Type

   returns:
      0 successful
     -1 errors in functions called by sge_split_queue_load

*/
int
sge_split_disabled(bool monitor_next_run, lList **queue_list, lList **disabled)
{
   lCondition *where;
   int ret;
   lList *lp = nullptr;

   DENTER(TOP_LAYER);

   if (!queue_list) {
      DRETURN(-1);
   }

   /* split queues */
   where = lWhere("%T(!(%I m= %u) && !(%I m= %u))", lGetListDescr(*queue_list),
                  QU_state, QI_DISABLED, QU_state, QI_CAL_DISABLED);
   ret = lSplit(queue_list, &lp, "full queues", where);
   lFreeWhere(&where);

   if (lp != nullptr) {
      lListElem* mes_queue;

      for_each_rw (mes_queue, lp) {
         schedd_mes_add_global(nullptr, monitor_next_run,
                               SCHEDD_INFO_QUEUEDISABLED_,
                               lGetString(mes_queue, QU_full_name));
      }

      schedd_log_list(nullptr, monitor_next_run,
                      MSG_SCHEDD_LOGLIST_QUEUESDISABLEDANDDROPPED,
                      lp, QU_full_name);

      if (*disabled == nullptr) {
         *disabled = lp;
      } else {
         lAddList(*disabled, &lp);
      }
   }

   DRETURN(ret);
}

/****** sge_select_queue/pe_cq_rejected() **************************************
*  NAME
*     pe_cq_rejected() -- Check, if -pe pe_name rejects cluster queue
*
*  SYNOPSIS
*     static bool pe_cq_rejected(const char *pe_name, const lListElem *cq)
*
*  FUNCTION
*     Match a jobs -pe 'pe_name' with pe_list cluster queue configuration.
*     True is returned if the parallel environment has no access.
*
*  INPUTS
*     const char *project - the pe request of a job (no wildcard)
*     const lListElem *cq - cluster queue (CQ_Type)
*
*  RESULT
*     static bool - True, if rejected
*
*  NOTES
*     MT-NOTE: pe_cq_rejected() is MT safe
*******************************************************************************/
static bool pe_cq_rejected(const char *pe_name, const lListElem *cq)
{
   const lListElem *alist;
   bool rejected;

   DENTER(TOP_LAYER);

   if (!pe_name) {
      DRETURN(false);
   }

   rejected = true;
   for_each_ep(alist, lGetList(cq, CQ_pe_list)) {
      if (lGetSubStr(alist, ST_name, pe_name, ASTRLIST_value)) {
         rejected = false;
         break;
      }
   }

   DRETURN(rejected);
}

/****** sge_select_queue/project_cq_rejected() *********************************
*  NAME
*     project_cq_rejected() -- Check, if -P project rejects cluster queue
*
*  SYNOPSIS
*     static bool project_cq_rejected(const char *project, const lListElem *cq)
*
*  FUNCTION
*     Match a jobs -P 'project' with project/xproject cluster queue configuration.
*     True is returned if the project has no access.
*
*  INPUTS
*     const char *project - the project of a job or nullptr
*     const lListElem *cq - cluster queue (CQ_Type)
*
*  RESULT
*     static bool - True, if rejected
*
*  NOTES
*     MT-NOTE: project_cq_rejected() is MT safe
*******************************************************************************/
static bool project_cq_rejected(const char *project, const lListElem *cq)
{
   const lList *projects;
   const lListElem *alist;
   bool rejected;

   DENTER(TOP_LAYER);

   if (!project) {
      /* without project: rejected, if each "project" profile
         does contain project references */
      for_each_ep(alist, lGetList(cq, CQ_projects)) {
         if (!lGetList(alist, APRJLIST_value)) {
            DRETURN(false);
         }
      }

      DRETURN(true);
   }

   /* with project: rejected, if project is excluded by each "xproject" profile */
   rejected = true;
   for_each_ep(alist, lGetList(cq, CQ_xprojects)) {
      projects = lGetList(alist, APRJLIST_value);
      if (!projects || !prj_list_locate(projects, project)) {
         rejected = false;
         break;
      }
   }
   if (rejected) {
      DRETURN(true);
   }

   /* with project: rejected, if project is not included with each "project" profile */
   rejected = true;
   for_each_ep(alist, lGetList(cq, CQ_projects)) {
      projects = lGetList(alist, APRJLIST_value);
      if (!projects || prj_list_locate(projects, project)) {
         rejected = false;
         break;
      }
   }
   if (rejected) {
      DRETURN(true);
   }

   DRETURN(false);
}

/****** sge_select_queue/interactive_cq_rejected() *****************************
*  NAME
*     interactive_cq_rejected() --  Check, if -now yes rejects cluster queue
*
*  SYNOPSIS
*     static bool interactive_cq_rejected(const lListElem *cq)
*
*  FUNCTION
*     Returns true if -now yes jobs can not be run in cluster queue
*
*  INPUTS
*     const lListElem *cq - cluster queue (CQ_Type)
*
*  RESULT
*     static bool - True, if rejected
*
*  NOTES
*     MT-NOTE: interactive_cq_rejected() is MT safe
*******************************************************************************/
static bool interactive_cq_rejected(const lListElem *cq)
{
   const lListElem *alist;
   bool rejected;

   DENTER(TOP_LAYER);

   rejected = true;
   for_each_ep(alist, lGetList(cq, CQ_qtype)) {
      if ((lGetUlong(alist, AQTLIST_value) & IQ)) {
         rejected = false;
         break;
      }
   }

   DRETURN(rejected);
}

bool sge_contained_in_access_list_(const char * user, const char * group, const lList *grp_list,  const lList * acl, const lList * list, const lList * acl_list);
/****** sge_select_queue/access_cq_rejected() **********************************
*  NAME
*     access_cq_rejected() -- Check, if cluster queue rejects user/project
*
*  SYNOPSIS
*     static bool access_cq_rejected(const char *user, const char *group, const
*     lList *acl_list, const lListElem *cq)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     const char *user      - Username
*     const char *group     - Groupname
*     const lList *acl_list - List of access list definitions
*     const lListElem *cq   - Cluster queue
*
*  RESULT
*     static bool - True, if rejected
*
*  NOTES
*     MT-NOTE: access_cq_rejected() is MT safe
*******************************************************************************/
static bool
access_cq_rejected(const char *user, const char *group, const lList *grp_list,
                   const lList *acl_list, const lListElem *cq)
{
   const lListElem *alist;
   bool rejected;

   DENTER(TOP_LAYER);

   /* rejected, if user/group is excluded by each "xacl" profile */
   rejected = true;
   for_each_ep(alist, lGetList(cq, CQ_xacl)) {
      if (!sge_contained_in_access_list_(user, group, grp_list, lGetList(alist, AUSRLIST_value), acl_list)) {
         rejected = false;
         break;
      }
   }
   if (rejected) {
      DRETURN(true);
   }

   /* rejected, if user/group is not included in any "acl" profile */
   rejected = true;
   for_each_ep(alist, lGetList(cq, CQ_acl)) {
      const lList *t = lGetList(alist, AUSRLIST_value);
      if (!t || sge_contained_in_access_list_(user, group, grp_list, t, acl_list)) {
         rejected = false;
         break;
      }
   }

   DRETURN(rejected);
}

bool cqueue_match_static_resource_list_rejected(sge_assignment_t *a, const char *cqname, const lListElem *cq,
                                                const lList *hard_resource_list, const char *scope_name) {
   DENTER(TOP_LAYER);

   bool rejected = false;

   if (hard_resource_list != nullptr) {
      dstring unsatisfied = DSTRING_INIT;
      if (request_cq_rejected(hard_resource_list, cq, a->centry_list,
                              (!a->pe_name || a->slots == 1)?true:false, &unsatisfied)) {
         rejected = true;
         DPRINTF("Cluster Queue " SFQ " can not fulfill " SFN " resource request (-l " SFN ") that "
                 "was requested by job " sge_u32 "\n", cqname, scope_name, sge_dstring_get_string(&unsatisfied), a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_CANNOTRUNINQUEUE_SSS,
                        sge_dstring_get_string(&unsatisfied),
                        cqname, "of cluster queue");
         sge_dstring_free(&unsatisfied);
      }
   }

   DRETURN(rejected);
}

/****** sge_select_queue/cqueue_match_static() *********************************
*  NAME
*     cqueue_match_static() -- Does cluster queue match the job?
*
*  SYNOPSIS
*     static dispatch_t cqueue_match_static(const char *cqname,
*     sge_assignment_t *a)
*
*  FUNCTION
*     The function tries to find reasons (-q, -l and -P) why the
*     entire cluster is not suited for the job.
*
*  INPUTS
*     const char *cqname  - Cluster queue name
*     sge_assignment_t *a - ???
*
*  RESULT
*     static dispatch_t - Returns DISPATCH_OK  or DISPATCH_NEVER_CAT
*
*  NOTES
*     MT-NOTE: cqueue_match_static() is MT safe
*******************************************************************************/
dispatch_t cqueue_match_static(const char *cqname, sge_assignment_t *a) {
   DENTER(TOP_LAYER);

   const lList *global_hard_queue_list, *master_hard_queue_list, *slave_hard_queue_list;
   get_hard_queue_lists(a->job, global_hard_queue_list, master_hard_queue_list, slave_hard_queue_list);

   bool rejected = false;
   if (global_hard_queue_list != nullptr) {
      if (qref_list_cq_rejected(global_hard_queue_list, cqname, nullptr, nullptr)) {
         DPRINTF("Cluster Queue " SFQ " is not contained in global hard queue requests of job " sge_u32 "\n",
                 cqname, a->job_id);
         rejected = true;
      }
   } else {
      if (a->pe_name != nullptr &&
          master_hard_queue_list != nullptr && slave_hard_queue_list != nullptr &&
          qref_list_cq_rejected(master_hard_queue_list, cqname, nullptr, nullptr) &&
          qref_list_cq_rejected(slave_hard_queue_list, cqname, nullptr, nullptr)) {
         DPRINTF("Cluster Queue " SFQ " is not contained in hard master and slave queue requests of job " sge_u32 "\n",
                 cqname, a->job_id);
         rejected = true;
      }
   }

   if (rejected) {
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                     SCHEDD_INFO_NOTINHARDQUEUELST_S, cqname);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* check if cqueue was reserved for AR job */
   u_long32 ar_id;
   ar_id = lGetUlong(a->job, JB_ar);
   if (ar_id != 0) {
      const lListElem *ar_ep = lGetElemUlong(a->ar_list, AR_id, ar_id);

      if (ar_ep != nullptr) {
         const lListElem *gep = nullptr;

         for_each_ep(gep, lGetList(ar_ep, AR_granted_slots)) {
            char *ar_cqueue = cqueue_get_name_from_qinstance(lGetString(gep, JG_qname));

            if (strcmp(cqname, ar_cqueue) == 0) {
               /* found queue */
               sge_free(&ar_cqueue);
               break;
            }
            sge_free(&ar_cqueue);
         }

         if (gep == nullptr) {

            DPRINTF("Cluster Queue \"%s\" was not reserved by advance reservation %d\n", cqname, ar_id);
            schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                           SCHEDD_INFO_QINOTARRESERVED_SI, cqname, ar_id);
            DRETURN(DISPATCH_NEVER_CAT);
         }
      } else {
         /* should never happen */
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   const lListElem *cq = lGetElemStr(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE), CQ_name, cqname);

   /* detect if entire cluster queue ruled out due to -l */
   const lList *hard_resource_list = job_get_hard_resource_list(a->job, JRS_SCOPE_GLOBAL);
   if (cqueue_match_static_resource_list_rejected(a, cqname, cq, hard_resource_list, "global")) {
      // if global requests do not match, we can rule out the cluster queue
      DRETURN(DISPATCH_NEVER_CAT);
   }
   const lList *master_hard_resource_list = job_get_hard_resource_list(a->job, JRS_SCOPE_MASTER);
   const lList *slave_hard_resource_list = job_get_hard_resource_list(a->job, JRS_SCOPE_SLAVE);
   if (cqueue_match_static_resource_list_rejected(a, cqname, cq, master_hard_resource_list, "master") &&
       cqueue_match_static_resource_list_rejected(a, cqname, cq, slave_hard_resource_list, "slave")) {
      // if both master and slave requests do not match, we cannot run any tasks in this cluster queue
      // @todo CS-601 here we could already tag queue instances
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* detect if entire cluster queue ruled out due to -P */
   const char *project = a->project;
   if (project_cq_rejected(project, cq)) {
      DPRINTF("Cluster queue \"%s\" does not work for -P %s job %d\n", cqname, project?project:"<no project>", (int)a->job_id);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id, SCHEDD_INFO_HASNOPRJ_S, "cluster queue", cqname);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* detect if entire cluster queue ruled out due to user_list/xuser_lists */
   if (access_cq_rejected(a->user, a->group, a->grp_list, a->acl_list, cq)) {
      DPRINTF("Job %d has no permission for cluster queue %s\n", (int)a->job_id, cqname);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id, SCHEDD_INFO_HASNOPERMISSION_SS, "cluster queue", cqname);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* detect if entire cluster queue ruled out due to -pe */
   const char *pe_name;
   if (a->pe && (pe_name=a->pe_name) && pe_cq_rejected(pe_name, cq)) {
      DPRINTF("Cluster queue " SFQ " does not reference PE " SFQ "\n", cqname, pe_name);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id, SCHEDD_INFO_NOTINQUEUELSTOFPE_SS, cqname, pe_name);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* detect if entire cluster queue ruled out due to -I y aka -now yes */
   if (JOB_TYPE_IS_IMMEDIATE(lGetUlong(a->job, JB_type)) && interactive_cq_rejected(cq)) {
      DPRINTF("Queue \"%s\" is not an interactive queue as requested by job %d\n", cqname, (int)a->job_id);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id, SCHEDD_INFO_QUEUENOTINTERACTIVE_S, cqname);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   /* Optimization: Walk through forced_centry_list and reject cq if possible */

   DRETURN(DISPATCH_OK);
}


/****** sge_select_queue/sequential_tag_queues_suitable4job() **************
*  NAME
*     sequential_tag_queues_suitable4job() -- ???
*
*  SYNOPSIS
*
*  FUNCTION
*     The start time of a queue is always returned using the QU_available_at
*     field.
*
*     The overall behaviour of this function is somewhat dependent on the
*     value that gets passed to assignment->start and whether soft requests
*     were specified with the job:
*
*     (1) In case of now assignments (DISPATCH_TIME_NOW) only the first queue
*         suitable for jobs without soft requests is tagged. When soft requests
*         are specified all queues must be verified and tagged in order to find
*         the queue that fits best.
*
*     (2) In case of reservation assignments (DISPATCH_TIME_QUEUE_END) the earliest
*         time is searched when the resources of global/host/queue are sufficient
*         for the job. The time-wise iteration is then done for each single resources
*         instance.
*
*         Actually there are cases when iterating through all queues were not
*         needed: (a) if there was a global limitation search could stop once
*         a queue is found that causes no further delay (b) if job has
*         a soft request search could stop once a queue is found with minimum (=0)
*         soft violations.
*
*  INPUTS
*     sge_assignment_t *assignment - job info structure
*
*  RESULT
*     dispatch_t - 0 ok got an assignment
*                    start time(s) and slots are tagged
*                  1 no assignment at the specified time
*                 -1 assignment will never be possible for all jobs of that category
*                 -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: sequential_tag_queues_suitable4job() is not MT safe
*******************************************************************************/
static dispatch_t
sequential_tag_queues_suitable4job(sge_assignment_t *a)
{
   lList *skip_host_list = nullptr;
   lList *skip_queue_list = nullptr;

   lList *unclear_cqueue_list = nullptr;
   lList *unclear_host_list = nullptr;
   dstring rule_name = DSTRING_INIT;
   dstring rue_string = DSTRING_INIT;
   dstring limit_name = DSTRING_INIT;
   u_long32 ar_id = lGetUlong(a->job, JB_ar);
   const lListElem *ar_ep = lGetElemUlong(a->ar_list, AR_id, ar_id);

   category_use_t use_category;
   bool got_solution = false;
   u_long64 tt_best = U_LONG64_MAX;
   u_long32 violations_best = U_LONG32_MAX;

   dispatch_t result;
   u_long64 tt_global = a->start;
   dispatch_t best_queue_result = DISPATCH_NEVER_CAT;
   int global_violations = 0;
   int queue_violations = 0;
   lListElem *qep;
   lListElem *best_qep = nullptr;
   u_long32 best_qep_violations = U_LONG32_MAX;
   u_long64 best_qep_tt = U_LONG64_MAX;

   DENTER(TOP_LAYER);

   /* assemble job category information */
   fill_category_use_t(a, &use_category, "NONE");

   /* restore job messages from previous dispatch runs of jobs of the same category */
   if (use_category.use_category) {
      schedd_mes_set_tmp_list(use_category.cache, CCT_job_messages, a->job_id);
      skip_host_list = lGetListRW(use_category.cache, CCT_ignore_hosts);
      skip_queue_list = lGetListRW(use_category.cache, CCT_ignore_queues);
   }

   if (ar_ep != nullptr) {
      result = match_static_advance_reservation(a);
      if (result != DISPATCH_OK) {
         DRETURN(result);
      }
   } else {
      SCHED_PROF_INC(a->pi, seq_global);
      result = sequential_global_time(&tt_global, a, &global_violations);
      if (result != DISPATCH_OK && result != DISPATCH_MISSING_ATTR) {
         DRETURN(result);
      }
   }

   for_each_rw(qep, a->queue_list) {
      u_long64 tt_host = a->start;
      u_long64 tt_queue = a->start;
      const char *eh_name;
      const char *qname, *cqname;
      u_long64 tt_rqs = 0;
      const lListElem *hep;

      qname = lGetString(qep, QU_full_name);
      cqname = lGetString(qep, QU_qname);
      eh_name = lGetHost(qep, QU_qhostname);

      /* untag this queues */
      lSetUlong(qep, QU_tag, 0);

      /* try to foreclose the cluster queue */
      if (lGetElemStr(a->skip_cqueue_list, CTI_name, cqname)) {
         DPRINTF("skip cluster queue %s\n", cqname);
         continue;
      }
      if (lGetElemStr(a->skip_host_list, CTI_name, eh_name)) {
         DPRINTF("rqs skip host %s\n", eh_name);
         continue;
      }
      if (skip_host_list && lGetElemStr(skip_host_list, CTI_name, eh_name)){
         DPRINTF("job category skip host %s\n", eh_name);
         continue;
      }

      if (skip_queue_list && lGetElemStr(skip_queue_list, CTI_name, qname)){
         DPRINTF("job category skip queue %s\n", qname);
         continue;
      }

      if (!ar_ep) {
         bool is_global;
         /* resource quota matching */
         if ((result = rqs_by_slots(a, cqname, eh_name, &tt_rqs, &is_global,
                  &rue_string, &limit_name, &rule_name, got_solution?tt_best:U_LONG64_MAX)) != DISPATCH_OK) {
            best_queue_result = find_best_result(result, best_queue_result);
            if (!is_global) {
               DPRINTF("no match due to GLOBAL RQS\n", eh_name);
               continue;
            }
            break; /* hit a global limit */
         }
         if (got_solution && is_not_better(a, violations_best, tt_best, 0, tt_rqs)) {
            DPRINTF("CUT TREE: Due to RQS for \"%s\"\n", qname);
            if (!is_global)
               continue;
            break;
         }
      }

      /* static cqueue matching */
      if (!lGetElemStr(unclear_cqueue_list, CTI_name, cqname)) {
         SCHED_PROF_INC(a->pi, seq_cqstat);
         if (cqueue_match_static(cqname, a) != DISPATCH_OK) {
            lAddElemStr(&(a->skip_cqueue_list), CTI_name, cqname, CTI_Type);
            best_queue_result = find_best_result(DISPATCH_NEVER_CAT, best_queue_result);
            continue;
         }
         lAddElemStr(&unclear_cqueue_list, CTI_name, cqname, CTI_Type);
      }

      /* static host matching */
      if (!(hep = lGetElemHost(a->host_list, EH_name, eh_name))) {
         ERROR(MSG_SCHEDD_UNKNOWN_HOST_SS, qname, eh_name);
         if (skip_queue_list != nullptr) {
            lAddElemStr(&skip_queue_list, CTI_name, qname, CTI_Type);
         }
         continue;
      }
      if (!lGetElemStr(unclear_host_list, CTI_name, eh_name)) {
         SCHED_PROF_INC(a->pi, seq_hstat);
         if (qref_list_eh_rejected(job_get_hard_queue_list(a->job), eh_name, a->hgrp_list)) {
            schedd_mes_add(a->monitor_alpp, a->monitor_next_run,
                           a->job_id, SCHEDD_INFO_NOTINHARDQUEUELST_S, eh_name);
            DPRINTF("Host \"%s\" is not contained in the hard queue list (-q) that "
                  "was requested by job %d\n", eh_name, (int)a->job_id);
            if (skip_host_list) {
               lAddElemStr(&skip_host_list, CTI_name, eh_name, CTI_Type);
            } else {
               lAddElemStr(&(a->skip_host_list), CTI_name, eh_name, CTI_Type);
            }
            best_queue_result = find_best_result(DISPATCH_NEVER_CAT, best_queue_result);
            continue;
         }

         if ((result=sge_host_match_static(a, hep))) {
            if (result == DISPATCH_NEVER_JOB || !skip_host_list) {
               lAddElemStr(&(a->skip_host_list), CTI_name, eh_name, CTI_Type);
            } else {
               lAddElemStr(&(a->skip_host_list), CTI_name, eh_name, CTI_Type);
            }
            best_queue_result = find_best_result(result, best_queue_result);
            continue;
         }
         lAddElemStr(&unclear_host_list, CTI_name, eh_name, CTI_Type);
      }

      /* static queue matching */
      SCHED_PROF_INC(a->pi, seq_qstat);
      if (sge_queue_match_static(a, qep) != DISPATCH_OK) {
         if (skip_queue_list)
            lAddElemStr(&skip_queue_list, CTI_name, qname, CTI_Type);
         best_queue_result = find_best_result(DISPATCH_NEVER_CAT, best_queue_result);
         continue;
      }

      /* ar matching */
      if (ar_ep != nullptr) {
         lListElem *ar_queue = lGetSubStrRW(ar_ep, QU_full_name, qname, AR_reserved_queues);
         // ar_queue can not be nullptr - we already sorted out non AR queues above in cqueue_match_static()
         lListElem *rep;
         dstring reason = DSTRING_INIT;

         /*
          * We are only interested in the static complexes on queue/host level.
          * These are tagged in this function. The result doesn't matter
          */
         for_each_rw(rep, job_get_hard_resource_listRW(a->job)) {
            const char *attrname = lGetString(rep, CE_name);
            lListElem *cplx_el = lGetElemStrRW(a->centry_list, CE_name, attrname);

            if (lGetUlong(cplx_el, CE_consumable) != CONSUMABLE_NO) {
               continue;
            }
            sge_dstring_clear(&reason);
            cplx_el = get_attribute_by_name(a->gep, hep, qep, attrname, a->centry_list, a->load_adjustments, a->start, a->duration);
            if (cplx_el == nullptr) {
               result = DISPATCH_MISSING_ATTR;
               sge_dstring_sprintf(&reason, MSG_ATTRIB_MISSINGATTRIBUTEXINCOMPLEXES_S, attrname);
               break;
            }

            result = match_static_resource(1, rep, cplx_el, &reason, false);
            lFreeElem(&cplx_el);
            if (result != DISPATCH_OK) {
               break;
            }
            lSetUlong(rep, CE_tagged, HOST_TAG);
         }
         if (result != DISPATCH_OK) {
            char buff[1024 + 1];
            centry_list_append_to_string(job_get_hard_resource_listRW(a->job), buff, sizeof(buff) - 1);
            if (*buff && (buff[strlen(buff) - 1] == '\n')) {
               buff[strlen(buff) - 1] = 0;
            }
            schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                           SCHEDD_INFO_CANNOTRUNATHOST_SSS, buff,
                           lGetHost(hep, EH_name), sge_dstring_get_string(&reason));
         } else {
            result = sequential_queue_time(&tt_queue, a, &queue_violations, ar_queue);
            if (result != DISPATCH_OK) {
               DPRINTF("ar queue %s returned %d\n", qname, result);
               if (skip_queue_list) {
                  lAddElemStr(&skip_queue_list, CTI_name, lGetString(ar_queue, QU_full_name), CTI_Type);
               }
               best_queue_result = find_best_result(result, best_queue_result);
               continue;
            }
            tt_global = tt_host = tt_queue;
         }
         sge_dstring_free(&reason);
      } else {
         // not running in an AR
         queue_violations = global_violations;

         /* dynamic host matching */
         SCHED_PROF_INC(a->pi, seq_hdyn);
         result = sequential_host_time(&tt_host, a, &queue_violations, hep);
         if (result != DISPATCH_OK && result != DISPATCH_MISSING_ATTR) {
            if (skip_host_list)
               lAddElemStr(&skip_host_list, CTI_name, eh_name, CTI_Type);
            else
               lAddElemStr(&(a->skip_host_list), CTI_name, eh_name, CTI_Type);
            DPRINTF("host %s returned %d\n", eh_name, result);
            best_queue_result = find_best_result(result, best_queue_result);
            continue;
         }
         if (got_solution && is_not_better(a, violations_best, tt_best, queue_violations, tt_host)) {
            lAddElemStr(&(a->skip_host_list), CTI_name, eh_name, CTI_Type);
            DPRINTF("CUT TREE: Due to HOST for \"%s\"\n", qname);
            continue;
         }

         /* dynamic queue matching */
         SCHED_PROF_INC(a->pi, seq_qdyn);
         result = sequential_queue_time(&tt_queue, a, &queue_violations, qep);
         if (result != DISPATCH_OK) {
            DPRINTF("queue %s returned %d\n", qname, result);
            if (skip_queue_list) {
               lAddElemStr(&skip_queue_list, CTI_name, qname, CTI_Type);
            }
            best_queue_result = find_best_result(result, best_queue_result);
            continue;
         }
         if (got_solution && is_not_better(a, violations_best, tt_best, queue_violations, tt_queue)) {
            DPRINTF("CUT TREE: Due to QUEUE for \"%s\"\n", qname);
            continue;
         }
      }

      if (result == DISPATCH_OK) {
         u_long64 this_tt = 0, this_violations = 0;
         lSetUlong(qep, QU_tag, 1); /* tag number of slots per queue and time when it will be available */


         if (a->start == DISPATCH_TIME_QUEUE_END) {
            DPRINTF("    global " sge_u64" host " sge_u64" queue " sge_u64" rqs " sge_u64"\n",
                  tt_global, tt_host, tt_queue, tt_rqs);
            lSetUlong64(qep, QU_available_at, this_tt = MAX(tt_queue, MAX(tt_host, MAX(tt_rqs, tt_global))));
         }
         if (a->is_soft) {
            this_violations = lGetUlong(qep, QU_soft_violation);
         }
         DPRINTF("    set Q: %s number=1 when=" sge_u64" violations=" sge_u32"\n", qname, this_tt, this_violations);
         best_queue_result = DISPATCH_OK;

         if (!a->is_reservation) {
            if (!a->is_soft || this_violations == 0) {
               /* we found our queue */
               if (best_qep != nullptr) {
                  lSetUlong(best_qep, QU_tag, 0);
               }
               break;
            }
         } else {
            /* further search is pointless, if reservation depends on a global consumable */
            /* earlier start time has higher preference than lower soft violations */
            if (ar_ep == nullptr && (tt_global >= MAX(MAX(tt_queue, tt_host), tt_rqs)) && !a->is_soft) {
               /* we found our queue */
               if (best_qep != nullptr) {
                  lSetUlong(best_qep, QU_tag, 0);
               }
               break;
            }
         }

         if (a->is_soft) {
            if (!a->is_reservation) {
               /* check if this queue is better than our last best queue */
               if (this_violations < best_qep_violations) {
                  lSetUlong(best_qep, QU_tag, 0);
                  best_qep = qep;
                  best_qep_violations = this_violations;
               } else {
                  /* reset QU_tag because another queue is better */
                  lSetUlong(qep, QU_tag, 0);
               }
            } else {
               /* earlier start time has higher preference than lower soft violations */
               if (this_tt < best_qep_tt ||
                   (this_tt == best_qep_tt && this_violations < best_qep_violations)) {
                  lSetUlong(best_qep, QU_tag, 0);
                  best_qep = qep;
                  best_qep_tt = this_tt;
                  best_qep_violations = this_violations;
               } else {
                  lSetUlong(qep, QU_tag, 0);
               }
            }
         }

         got_solution = true;
         violations_best = this_violations;
         tt_best = this_tt;
      } else {
         DPRINTF("queue %s reported %d\n", qname, result);
         if (skip_queue_list) {
            lAddElemStr(&skip_queue_list, CTI_name, qname, CTI_Type);
         }
         best_queue_result = find_best_result(result, best_queue_result);
      }
   } // end loop over all qinstances

   lFreeList(&unclear_cqueue_list);
   lFreeList(&unclear_host_list);

   /* cache so far generated messages with the job category */
   if (use_category.use_category) {
      lList *temp = schedd_mes_get_tmp_list();
      if (temp){
         lSetList(use_category.cache, CCT_job_messages, lCopyList(nullptr, temp));
      }
   }

   sge_dstring_free(&rue_string);
   sge_dstring_free(&limit_name);
   sge_dstring_free(&rule_name);

   DRETURN(best_queue_result);
}



/****** sge_select_queue/add_pe_slots_to_category() ****************************
*  NAME
*     add_pe_slots_to_category() -- defines an array of valid slot values
*
*  SYNOPSIS
*     static bool add_pe_slots_to_category(category_use_t *use_category,
*     u_long32 *max_slotsp, lListElem *pe, int min_slots, int max_slots, lList
*     *pe_range)
*
*  FUNCTION
*     In case of pe ranges does this function alocate memory and filles it wil
*     valid pe slot values. If a category is set, it stores them the category
*     for further jobs.
*
*  INPUTS
*     category_use_t *use_category - category caching structure, must not be nullptr
*     u_long32 *max_slotsp         - number of different slot settings
*     lListElem *pe                - pe, must not be nullptr
*     int min_slots                - min slot setting (pe range)
*     int max_slots                - max slot setting (pe range)
*     lList *pe_range              - pe range, must not be nullptr
*
*  RESULT
*     static bool - true, if successful
*
*  NOTES
*     MT-NOTE: add_pe_slots_to_category() is MT safe
*
*******************************************************************************/
static bool
add_pe_slots_to_category(category_use_t *use_category, u_long32 *max_slotsp, lListElem *pe,
                         int min_slots, int max_slots, lList *pe_range)
{
   if (use_category->cache != nullptr) {
      use_category->possible_pe_slots = (u_long32 *)lGetRef(use_category->cache, CCT_pe_job_slots);
   }

   /* --- prepare the possible slots for the binary search */
   if (use_category->possible_pe_slots == nullptr) {
      int slots;

      *max_slotsp = 0;
      for (slots = min_slots; slots <= max_slots; slots++) {
         // sort out slot numbers that would conflict with allocation rule
         if (sge_pe_slots_per_host(pe, slots) == 0) {
            continue;
         }

         // only slot numbers from jobs PE range request
         if (range_list_is_id_within(pe_range, slots) == 0) {
            continue;
         }

         if (use_category->possible_pe_slots == nullptr) {
            use_category->possible_pe_slots = (u_long32 *)sge_malloc((max_slots - min_slots + 1) * sizeof(u_long32));
            if (use_category->possible_pe_slots == nullptr) {
               return false;
            }
         }
         use_category->possible_pe_slots[(*max_slotsp)] = slots;
         (*max_slotsp)++;
      }

      if (use_category->cache != nullptr) {
         lSetRef(use_category->cache, CCT_pe_job_slots, use_category->possible_pe_slots);
         lSetUlong(use_category->cache, CCT_pe_job_slot_count, *max_slotsp);
         use_category->is_pe_slots_rev = true;
      } else {
         use_category->is_pe_slots_rev = false;

      }
   } else {
      use_category->is_pe_slots_rev = true;
      *max_slotsp = lGetUlong(use_category->cache, CCT_pe_job_slot_count);
   }

   return true;
}

/****** sge_select_queue/fill_category_use_t() **************
*  NAME
*     fill_category_use_t() -- fills the category_use_t structure.
*
*  SYNOPSIS
*     void fill_category_use_t(sge_assignment_t *a, category_use_t
*     *use_category, const char *pe_name)
*
*  FUNCTION
*     If a cache structure for the given PE does not exist, it
*     will generate the necessary data structures.
*
*  INPUTS
*     sge_assignment_t *a          - job info structure (in)
*     category_use_t *use_category - category info structure (out)
*     const char* pe_name          - the current pe name or "NONE"
*
*  NOTES
*     MT-NOTE: fill_category_use_t() is MT safe
*******************************************************************************/
static void fill_category_use_t(const sge_assignment_t *a, category_use_t *use_category, const char *pe_name)
{
   lListElem *job = a->job;

   DENTER(TOP_LAYER);

   use_category->category = (lListElem *)lGetRef(job, JB_category);
   if (use_category->category != nullptr) {
      // the category cache is stored in the job category object and contains an element per pe
      use_category->cache = lGetElemStrRW(lGetList(use_category->category, CT_cache), CCT_pe_name, pe_name);
      if (use_category->cache == nullptr) {
         // there is no cache yet, create it
         use_category->cache = lCreateElem(CCT_Type);

         lSetString(use_category->cache, CCT_pe_name, pe_name);
         lSetList(use_category->cache, CCT_ignore_queues, lCreateList(nullptr, CTI_Type));
         lSetList(use_category->cache, CCT_ignore_hosts, lCreateList(nullptr, CTI_Type));
         lSetList(use_category->cache, CCT_job_messages, lCreateList(nullptr, MES_Type));

         // store the cache in the job category
         if (lGetList(use_category->category, CT_cache) == nullptr) {
            lSetList(use_category->category, CT_cache, lCreateList("pe_cache", CCT_Type));
         }
         lAppendElem(lGetListRW(use_category->category, CT_cache), use_category->cache);
      }

      use_category->mod_category = true;

      use_category->use_category = a->start == DISPATCH_TIME_NOW &&
                                   lGetUlong(use_category->category, CT_refcount) > MIN_JOBS_IN_CATEGORY;
   } else {
      // we have a job without category (does this case exist at all?)
      use_category->cache = nullptr;
      use_category->mod_category = false;
      use_category->use_category = false;
   }
   use_category->possible_pe_slots = nullptr;
   use_category->is_pe_slots_rev = false;

   DRETURN_VOID;
}

static int get_soft_violations(sge_assignment_t *a, lListElem *hep, lListElem *qep)
{
   int violations;

   // @todo CS-483 only compute soft violations for a host if EH_soft_violations == -1
   violations = compute_soft_violations(a, nullptr, 0,
         lGetList(a->gep, EH_load_list),
         lGetList(a->gep, EH_consumable_config_list),
         lGetList(a->gep, EH_resource_utilization),
         DOMINANT_LAYER_GLOBAL, 0, GLOBAL_TAG);

   violations = compute_soft_violations(a, nullptr, violations,
         lGetList(hep, EH_load_list),
         lGetList(hep, EH_consumable_config_list),
         lGetList(hep, EH_resource_utilization),
         DOMINANT_LAYER_HOST, 0, HOST_TAG);

   violations = compute_soft_violations(a, qep, violations,
         nullptr,
         lGetList(qep, QU_consumable_config_list),
         lGetList(qep, QU_resource_utilization),
         DOMINANT_LAYER_QUEUE, 0, QUEUE_TAG);

   return violations;
}


/****** sge_select_queue/parallel_tag_queues_suitable4job() *********
*  NAME
*     parallel_tag_queues_suitable4job() -- Tag queues/hosts for
*        a comprehensive/parallel assignment
*
*  SYNOPSIS
*     static int parallel_tag_queues_suitable4job(sge_assignment_t
*                *assignment)
*
*  FUNCTION
*     We tag the amount of available slots for that job at global, host and
*     queue level under consideration of all constraints of the job. We also
*     mark those queues that are suitable as a master queue as possible master
*     queues and count the number of violations of the job's soft request.
*     The method below is named comprehensive since it does the tagging game
*     for the whole parallel job and under consideration of all available
*     resources that could help to suffice the jobs request. This is necessary
*     to prevent consumable resource limited at host/global level multiple
*     times.
*
*     While tagging we also set queues QU_host_seq_no based on the sort
*     order of each host. Assumption is the host list passed is sorted
*     according the load formula.
*
*  INPUTS
*     sge_assignment_t *assignment - ???
*     category_use_t use_category - information on how to use the job category
*
*  RESULT
*     static dispatch_t - 0 ok got an assignment
*                         1 no assignment at the specified time
*                        -1 assignment will never be possible for all jobs of that category
*                        -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: parallel_tag_queues_suitable4job() is not MT safe
*******************************************************************************/
static dispatch_t
parallel_tag_queues_suitable4job(sge_assignment_t *a, category_use_t *use_category, int *available_slots)
{
   lListElem *job = a->job;
   const lList *global_hard_queue_list, *master_hard_queue_list, *slave_hard_queue_list;
   get_hard_queue_lists(job, global_hard_queue_list, master_hard_queue_list, slave_hard_queue_list);
   bool have_master_and_slave_queue_request = master_hard_queue_list != nullptr && slave_hard_queue_list != nullptr;
#if 0
   bool have_master_requests = job_get_hard_resource_list(job, JRS_SCOPE_MASTER) != nullptr;
   bool have_slave_requests = job_get_hard_resource_list(job, JRS_SCOPE_SLAVE) != nullptr;
#endif

   lListElem *hep;
   dispatch_t best_result = DISPATCH_NEVER_CAT;
   int gslots = a->slots;
   int gslots_qend = 0;

   DENTER(TOP_LAYER);

   clean_up_parallel_job(a);

   if (use_category->use_category) {
      schedd_mes_set_tmp_list(use_category->cache, CCT_job_messages, a->job_id);
   }

   u_long32 ar_id = lGetUlong(a->job, JB_ar);
   if (ar_id == 0) {
      SCHED_PROF_INC(a->pi, par_global);
      parallel_global_slots(a, &gslots, &gslots_qend);
   }

   if (gslots < a->slots) {
      best_result = (gslots_qend < a->slots) ? DISPATCH_NEVER_CAT : DISPATCH_NOT_AT_TIME;
      *available_slots = gslots_qend;

      if (best_result == DISPATCH_NOT_AT_TIME) {
         DPRINTF("GLOBAL will <category_later> get us %d slots (%d)\n", gslots, gslots_qend);
      } else {
         DPRINTF("GLOBAL will <category_never> get us %d slots (%d)\n", gslots, gslots_qend);
      }
   } else {
      // cluster queues which are not rejected at cluster queue level
      // no need to repeat the cluster queue checks but need to check on lower level
      lList *unclear_cqueue_list = nullptr;

      int last_accu_host_slots, last_accu_host_slots_qend;

      lList *skip_host_list = nullptr;
      if (use_category->use_category) {
         skip_host_list = lGetListRW(use_category->cache, CCT_ignore_hosts);
      }

      /* first queue filtering step on cluster queue layer
       * non-eligible queues may have no impact on host preference
       */
      lListElem *qep;
      for_each_rw (qep, a->queue_list) {
         const char *cqname = lGetString(qep, QU_qname);

         /* try to foreclose the cluster queue */
         if (lGetElemStr(a->skip_cqueue_list, CTI_name, cqname)) { // @todo (CS-453) why is the skip_cqueue_list not cached?
            lSetUlong(qep, QU_tag, 0);    // @todo why set it? It is already 0 from clean_up_parallel_job() above. Similar cases below.
            DPRINTF("(1) skip cluster queue %s\n", cqname);
            continue;
         }

         if (!lGetElemStr(unclear_cqueue_list, CTI_name, cqname)) {
            SCHED_PROF_INC(a->pi, par_cqstat);
            if (cqueue_match_static(cqname, a) != DISPATCH_OK) {
               lAddElemStr(&(a->skip_cqueue_list), CTI_name, cqname, CTI_Type);
               /* tag QI as unsuited */
               lSetUlong(qep, QU_tag, 0);
               DPRINTF("add cluster queue %s to skip list\n", cqname);
               continue;
            }
            DPRINTF("cqueue %s is not rejected\n", cqname);
            lAddElemStr(&unclear_cqueue_list, CTI_name, cqname, CTI_Type);
         }
         // the queue can potentially be used - it is not rejected by static cqueue matching
         lSetUlong(qep, QU_tag, 1);
      } // end first queue filtering step

      // sorting queue list, with or without respecting soft requests
      if (a->is_soft) {
         a->soft_violations = 0;
         for_each_rw (qep, a->queue_list) {
            lSetUlong(qep, QU_soft_violation, 0);
            if (lGetUlong(qep, QU_tag) == 0 || !(hep = host_list_locate(a->host_list, lGetHost(qep, QU_qhostname)))) {
               continue;
            }
            get_soft_violations(a, hep, qep);
            DPRINTF("EVALUATE: %s soft violations %d\n", lGetString(qep, QU_full_name), lGetUlong(qep, QU_soft_violation));
         }
         if (sconf_get_queue_sort_method() == QSM_LOAD) {
            /* we sort by
             * 1. QU_tag DESCENDING (QU_tag is 0 for queues that are not suitable according to cqueue_match_static()),
             *    1 for queues which are potentially suitable
             * 2. QU_soft_violation ASCENDING
             * 3. depending on the queue sort method:
             *    - QSM_LOAD: QU_host_seq_no (from load_formula) ASCENDING, QU_seq_no (from config) ASCENDING
             *    - QSM_SEQNUM: QU_seq_no (from config) ASCENDING, QU_host_seq_no (from load_formula) ASCENDING
             */
            lPSortList(a->queue_list, "%I- %I+ %I+ %I+", QU_tag, QU_soft_violation, QU_host_seq_no, QU_seq_no);
         } else {
            lPSortList(a->queue_list, "%I- %I+ %I+ %I+", QU_tag, QU_soft_violation, QU_seq_no, QU_host_seq_no);
         }
      } else {
         int last_dispatch_type = sconf_get_last_dispatch_type();
         if (last_dispatch_type == DISPATCH_TYPE_PE_SOFT_REQ ||
             last_dispatch_type == DISPATCH_TYPE_NONE ||
             sconf_get_host_order_changed()) {
            if (sconf_get_queue_sort_method() == QSM_LOAD) {
               lPSortList(a->queue_list, "%I+ %I+", QU_host_seq_no, QU_seq_no);
            } else {
               lPSortList(a->queue_list, "%I+ %I+", QU_seq_no, QU_host_seq_no);
            }
         }
      }

      // Sort the host list
      // Hosts which are not referenced in the queue list have seq_no U_LONG32_MAX = will land at the end of the list
      for_each_rw (hep, a->host_list) {
         lSetUlong(hep, EH_seq_no, U_LONG32_MAX);
      }
      u_long32 host_seq_no = 0;
      for_each_rw (qep, a->queue_list) {
         /* @todo CS-602 verify: if all.q is disabled or job requests queue != all.q, why is it still on top of the list?
          * AFTER SORT: all.q@ubuntu-22-amd64-1: available: 0, soft violations 0
          * AFTER SORT: all.q@ubuntu-20-amd64-1: available: 0, soft violations 0
          * AFTER SORT: all.q@solaris-11-1: available: 0, soft violations 0
          * AFTER SORT: all.q@centos-7-amd64-1: available: 0, soft violations 0
          * AFTER SORT: all.q@rocky-8-amd64-1: available: 0, soft violations 0
          * AFTER SORT: all.q@ubuntu-24-amd64-1: available: 0, soft violations 0
          * AFTER SORT: test.q@solaris-11-1: available: 1, soft violations 0
          * AFTER SORT: test.q@centos-7-amd64-1: available: 1, soft violations 0
          */
         DPRINTF("AFTER SORT: %s: available: " sge_uu32 ", soft violations %d\n", lGetString(qep, QU_full_name),
                 lGetUlong(qep, QU_tag), lGetUlong(qep, QU_soft_violation));
         if (lGetUlong(qep, QU_tag) == 0) {
            continue;
         }
         const char *eh_name = lGetHost(qep, QU_qhostname);
         if (!(hep = host_list_locate(a->host_list, eh_name)))
            continue;
         if (lGetUlong(hep, EH_seq_no) == U_LONG32_MAX)
            lSetUlong(hep, EH_seq_no, host_seq_no++);
      }
      lPSortList(a->host_list, "%I+", EH_seq_no);
      bool done = false;

      // now work on the host list
      int accu_host_slots = 0, accu_host_slots_qend = 0;
      bool suited_as_master_host;
      bool have_master_host = false, have_master_host_qend = false;

      int allocation_rule, minslots;
      allocation_rule = sge_pe_slots_per_host(a->pe, a->slots);
      minslots = ALLOC_RULE_IS_BALANCED(allocation_rule)?allocation_rule:1;

      dstring rule_name = DSTRING_INIT;
      dstring rue_name = DSTRING_INIT;
      dstring limit_name = DSTRING_INIT;

      const lListElem *master_host = nullptr; // here we remember the master host, it is used when we are in the second round
      // @todo do we need to remember the master queue as well? We also do matching on queues and might have multiple
      //       queues on a host and be matching queue resources. A job might not get these resources.

      /*
       * in case of round-robin we might need to repeat the step as we only allocate 1 slot per host per round
       */
      do {
         last_accu_host_slots = accu_host_slots;
         last_accu_host_slots_qend = accu_host_slots_qend;

         // loop over the sorted host list
         for_each_rw (hep, a->host_list) {
            // we already handled global resources
            if (hep == a->gep) {
               continue;
            }

            /* this host does not work for this category, skip it */
            const char *eh_name = lGetHost(hep, EH_name);
            if (skip_host_list && lGetElemStr(skip_host_list, CTI_name, eh_name)) {
               continue;
            }
            if (lGetElemStr(a->skip_host_list, CTI_name, eh_name)) {
               continue;
            }

            /*
             * do not perform expensive checks for this host if there
             * is not at least one free queue residing at this host:
             * see if there are queues which are not disabled/suspended/calendar;
             * which have at least one free slot, which are not unknown, several alarms
             */
            // @todo (CS-456) cheaper check: lGetUlong(EH_seq_no) == U_LONG32_MAX, we could possibly break out of the host loop
            // @todo suited_as_master_host is never used
            // @todo when we are in an additional round of the do ... while loop, couldn't we skip parallel_tag_hosts_queues()?
            //       the number of available slots should already be in QU_tag / QU_tag_qend?
            //       hslots = sum(QU_tag), and hslots_qend = sum(QU_tag_qend)?
            int hslots = 0, hslots_qend = 0;
            if (lGetElemHost(a->queue_list, QU_qhostname, eh_name)) {
               bool need_master = !have_master_host;
               bool is_master_host = (hep == master_host);
               DPRINTF("===> HOST: %s %p <=== need_master: %d is_master_host: %d (%p)\n", eh_name, (void *)hep, need_master,
                       is_master_host, (void *)master_host);
               parallel_tag_hosts_queues(a, hep, &hslots, &hslots_qend, need_master, is_master_host,
                                         &suited_as_master_host, use_category, &unclear_cqueue_list);

               if (hslots >= minslots) {
                  /* Now RQS limit debitation can be performed for each queue instance based on QU_tag.
                   * While considering allocation_rule and master queue demand we try to get as
                   * much from each queue as needed, but not more than 'maxslots'. When we are through
                   * all queue instances and got not even 'minslots' the slots must be undebited.
                   */
                  // @todo we have the following code twice, identical (?) for now and for qend
                  //       make a function and call it twice with the different parameters
                  bool got_master_queue = have_master_host;
                  int rqs_hslots = 0, maxslots, slots, slots_qend;

                  /* still broken for cases where round-robin requires multiple rounds */
                  if (ALLOC_RULE_IS_BALANCED(allocation_rule)) {
                     maxslots = allocation_rule;
                  } else if (allocation_rule == ALLOC_RULE_FILLUP) {
                     maxslots = MIN(a->slots - accu_host_slots, hslots);
                  } else { // ALLOC_RULE_ROUNDROBIN
                     maxslots = 1;
                  }

                  // @todo if we know here that we still need the master host/queue and master and slave requests
                  //       are disjoint, set maxslots to 1?

                  /* debit on RQS limits */
                  const void *iter = nullptr;
                  for (qep = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &iter); qep;
                       qep = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &iter)) {
                     const char *qname = lGetString(qep, QU_full_name);

                     if (lGetUlong(qep, QU_tag) == 0)
                        continue;

                     DPRINTF("tagged: %d maxslots: %d rqs_hslots: %d\n", (int)lGetUlong(qep, QU_tag), maxslots, rqs_hslots);
                     DPRINTF("SLOT HARVESTING: %s soft violations: %d master: " sge_u32 "\n",
                           lGetString(qep, QU_full_name), (int)lGetUlong(qep, QU_soft_violation), lGetUlong(qep, QU_tagged4schedule));

                     /* how much is still needed */
                     slots = MIN(lGetUlong(qep, QU_tag), maxslots - rqs_hslots);

                     if (!have_master_host && !got_master_queue) {
                        if (!lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_MASTER)) {
                           /*
                              care for slave tasks assignments of -masterq jobs
                              we need at least one slot on the masterq, thus we reduce by one slot
                              if we run out of slots
                             */
                           /* @todo: understand the comment above:
                            * - this qinstance is not suited as master queue
                            * - but there might be another queue on the host which is suited as master queue?
                            * - therefore we reduce the amount of slots we can use in this queue for slave tasks by 1?
                            *    - even if there is no other qinstance which could be used as master queue?
                            *    - this might prevent using the queue only for slave tasks as we now have less slots?
                            */
                           if (accu_host_slots + rqs_hslots + slots == a->slots) {
                              slots--;
                           }
                        } else {
                           // this qinstance is suited as master queue
                           if (have_master_and_slave_queue_request) {
                              /* if the masterq request is not contained in the hard queue request
                               * we need to allocate only one slot for the master task
                               */
#if 1
                              if (qref_list_cq_rejected(slave_hard_queue_list,
                                  lGetString(qep, QU_qname), eh_name, a->hgrp_list)) {
                                 slots = MIN(slots, 1);
                              }
#else
                              if (!lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_SLAVE)) {
                                 slots = MIN(slots, 1);
                              }
#endif
                           }
                           // @todo also resource requests might have disjoint master/slave requests
                           //       shouldn't we see this from queue tagging?
                           //          -> QU_tagged4schedule not containing *SLAVE*
                           //          -> QU_tag being 0 or 1?
#if 0
                           if (have_master_requests && have_slave_requests) {
                              if (!lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_SLAVE)) {
                                 slots = MIN(slots, 1);
                              }
                           }
#endif
                        }
                     }

                     slots_qend = 0;
                     if (ar_id == 0 && !a->is_advance_reservation) {
                        DPRINTF("RQS: trying to debit %d slots in queue " SFQ "\n", slots, qname);
                        parallel_check_and_debit_rqs_slots(a, eh_name, lGetString(qep, QU_qname),
                              &slots, &slots_qend, &rule_name, &rue_name, &limit_name);
                        DPRINTF("RQS: could debiting %d slots in queue " SFQ "\n", slots, qname);
                     }

                     if (slots > 0) {
                        /* add gdil element for this queue */
                        lListElem *gdil_ep;
                        if (!(gdil_ep=lGetElemStrRW(a->gdil, JG_qname, qname))) {
                           gdil_ep = lAddElemStr(&(a->gdil), JG_qname, qname, JG_Type);
                           lSetUlong(gdil_ep, JG_qversion, lGetUlong(qep, QU_version));
                           lSetHost(gdil_ep, JG_qhostname, eh_name);
                           lSetUlong(gdil_ep, JG_slots, slots);

                           /* master queue must be at first position */
                           if (!have_master_host && lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_MASTER)) {
                              lDechainElem(a->gdil, gdil_ep);
                              lInsertElem(a->gdil, nullptr, gdil_ep);
                              got_master_queue = true;
                           }
                        } else {
                           lAddUlong(gdil_ep, JG_slots, slots);
                        }

                        // @todo CS-731 copy task specific binding decision into the JG-element

                        if (a->is_soft)
                           a->soft_violations += slots * lGetUlong(qep, QU_soft_violation);
                     }
                     lSetUlong(qep, QU_tag, slots);

                     rqs_hslots += slots;

                     if (rqs_hslots == maxslots)
                        break;
                  }

                  if (rqs_hslots < minslots) {
                     DPRINTF("reverting debitation since " SFQ " gets us only %d slots while min. %d are needed\n",
                           eh_name, rqs_hslots, minslots);

                     /* must revert all RQS debitations, rollback GDIL and soft violations */
                     for (qep = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &iter); qep;
                          qep = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &iter)) {
                        slots = lGetUlong(qep, QU_tag);
                        if (slots != 0) {
                           if (ar_id == 0 && !a->is_advance_reservation) {
                              parallel_revert_rqs_slot_debitation(a, eh_name, lGetString(qep, QU_qname),
                                    slots, 0, &rule_name, &rue_name, &limit_name);
                           }
                           lListElem *gdil_ep;
                           if ((gdil_ep=lGetElemStrRW(a->gdil, JG_qname, lGetString(qep, QU_full_name)))) {
                              if (lGetUlong(gdil_ep, JG_slots) - slots != 0) {
                                 lSetUlong(gdil_ep, JG_slots, lGetUlong(gdil_ep, JG_slots) - slots);
                              } else {
                                 lDelElemStr(&(a->gdil), JG_qname, lGetString(qep, QU_full_name));
                              }
                           }
                           a->soft_violations -= slots * lGetUlong(qep, QU_soft_violation);
                           lSetUlong(qep, QU_tag, 0);
                        }
                     }
                     hslots = 0;
                  } else {
                     // we get here for every host and re-set have_master_host over and over again
                     if (got_master_queue) {
                        have_master_host = true;
                        // remember the master host only once, otherwise we overwrite it with some different host
                        if (master_host == nullptr) {
                           master_host = hep;
                        }
                     }
                     accu_host_slots += rqs_hslots;
                  }
               } // end: we have enough slots *now* on this host

               if (hslots_qend >= minslots && a->care_reservation && !a->is_reservation) {
                  bool got_master_queue_qend = have_master_host_qend;
                  int rqs_hslots = 0, maxslots, slots, slots_qend;

                  /* still broken for cases where round-robin requires multiple rounds */
                  if (ALLOC_RULE_IS_BALANCED(allocation_rule)) {
                     maxslots = allocation_rule;
                  } else if (allocation_rule == ALLOC_RULE_FILLUP) {
                     maxslots = MIN(a->slots - accu_host_slots_qend, hslots_qend);
                  } else { /* ALLOC_RULE_ROUNDROBIN */
                     maxslots = 1;
                  }

                  /* debit on RQS limits */
                  const void *iter = nullptr;
                  for (qep = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &iter); qep;
                       qep = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &iter)) {

                     if (lGetUlong64(qep, QU_tag_qend) == 0)
                        continue;

                     slots = 0;
                     slots_qend = MIN(lGetUlong64(qep, QU_tag_qend), maxslots - rqs_hslots);

                     if (!have_master_host_qend && !got_master_queue_qend) {
                        if (!lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_MASTER_LATER)) {
                           /*
                              care for slave tasks assignments of -masterq jobs
                              we need at least one slot on the masterq, thus we reduce by one slot
                              if we run out of slots
                             */
                           if (accu_host_slots_qend + rqs_hslots + slots_qend == a->slots) {
                              slots_qend--;
                           }
                        } else {
                           /* care for master tasks assignments of -masterq jobs */
                           if (have_master_and_slave_queue_request) {
                              /* if the masterq request is not contained in the hard queue request
                                 we need to allocate only one slot for the master task */
#if 1
                              if (qref_list_cq_rejected(slave_hard_queue_list,
                                  lGetString(qep, QU_qname), lGetHost(qep, QU_qhostname), a->hgrp_list)) {
                                 slots_qend = MIN(slots_qend, 1);
                              }
#else
                              if (!lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_SLAVE_LATER)) {
                                 slots = MIN(slots, 1);
                              }
#endif
                           }
                           // @todo also resource requests might have disjoint master/slave requests
                           //       shouldn't we see this from queue tagging?
                           //          -> QU_tagged4schedule not containing *SLAVE*
                           //          -> QU_tag being 0 or 1?
#if 0
                           if (have_master_requests && have_slave_requests) {
                              if (!lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_SLAVE_LATER)) {
                                 slots = MIN(slots, 1);
                              }
                           }
#endif
                        }
                     }

                     if (ar_id == 0 && !a->is_advance_reservation) {
                        DPRINTF("trying to debit %d slots_qend in queue " SFQ "\n", slots_qend, lGetString(qep, QU_full_name));
                        parallel_check_and_debit_rqs_slots(a, eh_name, lGetString(qep, QU_qname),
                              &slots, &slots_qend, &rule_name, &rue_name, &limit_name);
                        DPRINTF("could debiting %d slots_qend in queue " SFQ "\n", slots_qend, lGetString(qep, QU_full_name));
                     }

                     if (slots_qend > 0) {
                        if (!have_master_host_qend && lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_MASTER_LATER)) {
                           got_master_queue_qend = true;
                        }
                     }

                     lSetUlong64(qep, QU_tag_qend, slots_qend);
                     rqs_hslots += slots_qend;

                     if (rqs_hslots == maxslots)
                        break;
                  }
                  if (rqs_hslots < minslots) {
                     DPRINTF("reverting debitation since " SFQ " gets us only %d slots while min. %d are needed\n",
                           eh_name, rqs_hslots, minslots);

                     /* must revert all RQS debitations */
                     for (qep = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &iter); qep;
                          qep = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &iter)) {
                        slots_qend = lGetUlong64(qep, QU_tag_qend);
                        if (slots_qend != 0) {
                           if (ar_id == 0 && !a->is_advance_reservation)
                              parallel_revert_rqs_slot_debitation(a, eh_name, lGetString(qep, QU_qname),
                                    0, slots_qend, &rule_name, &rue_name, &limit_name);
                           lSetUlong64(qep, QU_tag_qend, 0);
                        }
                     }
                     hslots_qend = 0;
                  } else {
                     if (got_master_queue_qend) {
                        have_master_host_qend = true;
                     }
                     accu_host_slots_qend += rqs_hslots;
                  }
               } // end: we have enough slots *later* on this host
            } // end: we have at least one qinstance on this host

            /* mark host as not suitable
             * at least when accu_host_slots and accu_host_slots_qend < a->slots
             * and only, if modify category is set
             */
            if (skip_host_list && use_category->mod_category) {
               if (hslots < minslots && hslots_qend < minslots) {
                  // @todo (CS-457) what if the host is already in the skip list? Can probably not happen here.
                  //       but still we should have functions add_to_skip_list(), is_in_skip_list()
                  lAddElemStr(&skip_host_list, CTI_name, eh_name, CTI_Type);
               }
            }

            /* exit is possible when we
             *    (a) got the full slot amount or
             *    (b) got full slot_qend amount and know full slot amount is not achievable anymore
             */
            if ((accu_host_slots >= a->slots && have_master_host) &&
                (!a->care_reservation || a->is_reservation ||
                 (accu_host_slots_qend >= a->slots && have_master_host_qend))) {
               DPRINTF("WE ARE THROUGH!\n");
               done = true;
               break;
            }

            /* no need to collect further when slots_qend are complete */
            if (a->care_reservation && accu_host_slots_qend >= a->slots && have_master_host_qend)
               a->care_reservation = false;

         } /* for each host */
      } while (allocation_rule == ALLOC_RULE_ROUNDROBIN && !done &&
            (last_accu_host_slots != accu_host_slots || last_accu_host_slots_qend != accu_host_slots_qend));

      lFreeList(&unclear_cqueue_list);

      sge_dstring_free(&rule_name);
      sge_dstring_free(&rue_name);
      sge_dstring_free(&limit_name);

      if (accu_host_slots >= a->slots && have_master_host) {
         /* stop looking for smaller slot amounts */
         DPRINTF("-------------->      BINGO %d slots at specified time <--------------\n", a->slots);
         best_result = DISPATCH_OK;
      } else if (accu_host_slots_qend >= a->slots && have_master_host_qend) {
         DPRINTF("-------------->            %d slots later             <--------------\n", a->slots);
         best_result = DISPATCH_NOT_AT_TIME;
      } else {
         DPRINTF("-------------->   NEVER ONLY %d but %d needed              <--------------\n", accu_host_slots, a->slots);
         best_result = DISPATCH_NEVER_CAT;
      }

      /* could be interesting for future diagnosis that unveils what resources the job could get
       * as of now we got only negative diagnosis information i.e. reasons why/where no resources are/available
       */
#if 0
      if (best_result != DISPATCH_OK) {
         dstring sched_info = DSTRING_INIT;
         for_each_ep(gdil_ep, a->gdil) {
            sge_dstring_sprintf(&sched_info, "   HOST: %s QUEUE: %s SLOTS %d",
               lGetHost(gdil_ep, JG_qhostname), lGetString(gdil_ep, JG_qname), (int)lGetUlong(gdil_ep, JG_slots));
            schedd_log(sge_dstring_get_string(&sched_info));
         }
         sge_dstring_free(&sched_info);
      }
#endif

      *available_slots = accu_host_slots;

      if (DPRINTF_IS_ACTIVE) {
         DSTRING_STATIC(dstr, 64);
         switch (best_result) {
         case DISPATCH_OK:
            DPRINTF("COMPREHENSIVE ASSIGNMENT(%d) returns %s\n", a->slots, sge_ctime64(a->start, &dstr));
            break;
         case DISPATCH_NOT_AT_TIME:
            DPRINTF("COMPREHENSIVE ASSIGNMENT(%d) returns <later>\n", a->slots);
            break;
         case DISPATCH_NEVER_CAT:
            DPRINTF("COMPREHENSIVE ASSIGNMENT(%d) returns <category_never>\n", a->slots);
            break;
         case DISPATCH_NEVER_JOB:
            DPRINTF("COMPREHENSIVE ASSIGNMENT(%d) returns <job_never>\n", a->slots);
            break;
         default:
            DPRINTF("!!!!!!!! COMPREHENSIVE ASSIGNMENT(%d) returns unexpected %d\n", best_result);
            break;
         }
      }
   }

   if (use_category->use_category) {
      lList *temp = schedd_mes_get_tmp_list();
      if (temp){
         lSetList(use_category->cache, CCT_job_messages, lCopyList(nullptr, temp));
       }
   }

   if (best_result != DISPATCH_OK) {
      lFreeList(&(a->gdil));
   }

   DRETURN(best_result);
}

static int already_booked_on_host(const lList *gdil, const char *host_name) {
   int ret = 0;

   const void *iterator = nullptr;
   const lListElem *gdil_ep;
   for (gdil_ep = lGetElemHostFirst(gdil, JG_qhostname, host_name, &iterator);
        gdil_ep != nullptr;
        gdil_ep = lGetElemHostNext(gdil, JG_qhostname, host_name, &iterator)) {
      ret += lGetUlong(gdil_ep, JG_slots);
   }

   return ret;
}

static void host_clear_qinstance_tags(lList *queue_list, const char *host_name, u_long32 bitmask) {
   const void *queue_iterator = nullptr;
   lListElem *next_queue, *qep;
   for (next_queue = lGetElemHostFirstRW(queue_list, QU_qhostname, host_name, &queue_iterator);
        (qep = next_queue);
        next_queue = lGetElemHostNextRW(queue_list, QU_qhostname, host_name, &queue_iterator)) {
      lClearUlongBitMask(qep, QU_tagged4schedule, bitmask);
      print_tagged4schedule(qep);
   }
}

static void
host_or_queue_clear_tags(const char *object_name, lListElem *queue, lList *queue_list, u_long32 bitmask) {
   if (queue != nullptr) {
      lClearUlongBitMask(queue, QU_tagged4schedule, bitmask);
      print_tagged4schedule(queue);
   } else {
      // we are coming from host matching, clear tags in all queues on this host
      host_clear_qinstance_tags(queue_list, object_name, bitmask);
   }
}

/****** sge_select_queue/parallel_host_slots() ******************************
*  NAME
*     parallel_host_slots() -- Return host slots available at time period
*
*  SYNOPSIS
*  FUNCTION
*     The maximum amount available at the host for the specified time period
*     is determined.
*
*
*  INPUTS
*
*  RESULT
*******************************************************************************/
// @todo there is a single call to parallel_host_slots() in parallel_tag_hosts_queues() where we pass false as/
//       allow_non_requestable - should we remove this parameter?
static dispatch_t
parallel_host_slots(sge_assignment_t *a, int *slots, int *slots_qend, lListElem *hep, bool need_master,
                    bool is_master_host, bool allow_non_requestable) {
   DENTER(TOP_LAYER);

   int hslots = 0, hslots_qend = 0;
   dispatch_t result = DISPATCH_NEVER_CAT;
   const char *eh_name = lGetHost(hep, EH_name);

   SCHED_PROF_INC(a->pi, par_hstat);

   const lList *global_hard_queue_list, *master_hard_queue_list, *slave_hard_queue_list;
   get_hard_queue_lists(a->job, global_hard_queue_list, master_hard_queue_list, slave_hard_queue_list);
   bool rejected_as_slave = false, rejected_as_master = false;
   if (global_hard_queue_list != nullptr) {
      rejected_as_master = rejected_as_slave = qref_list_eh_rejected(global_hard_queue_list, eh_name, a->hgrp_list);
   } else {
      if (master_hard_queue_list != nullptr) {
         rejected_as_master = qref_list_eh_rejected(master_hard_queue_list, eh_name, a->hgrp_list);
      }
      if (slave_hard_queue_list != nullptr) {
         rejected_as_slave = qref_list_eh_rejected(slave_hard_queue_list, eh_name, a->hgrp_list);
      }
   }
   // @todo CS-601 remove tags from QU_tagged4schedule according to rejected_as_master and rejected_as_slave
   if (!(rejected_as_master && rejected_as_slave) &&
       sge_host_match_static(a, hep) == DISPATCH_OK) {
      // this host is suited at least for slave tasks, possibly also for the master task

      // cause load be raised artificially to reflect load correction when checking job requests
      double lc_factor = 0.0;
      if (lGetPosViaElem(hep, EH_load_correction_factor, SGE_NO_ABORT) >= 0) {
         u_long32 ulc_factor = lGetUlong(hep, EH_load_correction_factor);
         if (ulc_factor > 0) {
            lc_factor = ulc_factor / 100.0;
         }
      }

      SCHED_PROF_INC(a->pi, par_hdyn);

      const lList *load_list = lGetList(hep, EH_load_list);
      const lList *config_attr = lGetList(hep, EH_consumable_config_list);
      const lList *actual_attr = lGetList(hep, EH_resource_utilization);

      // we call this function per host, and we really need to clear the resource tags with every call
      clear_resource_tags(a->job, HOST_TAG);

      result = parallel_rc_slots_by_time(a, &hslots, &hslots_qend,
                                         config_attr, actual_attr, load_list, false, nullptr,
                                         DOMINANT_LAYER_HOST, lc_factor, HOST_TAG, need_master, is_master_host, false,
                                         eh_name, false);

      if (result == DISPATCH_NOT_AT_TIME) {
         host_clear_qinstance_tags(a->queue_list, eh_name, TAG4SCHED_MASTER); // @todo CS-601 also TAG4SCHED_SLAVE?
         result = DISPATCH_OK;
      }

      if (hslots > 0) {
         int t_max = parallel_max_host_slots(a, hep);
         if (t_max<hslots) {
            DPRINTF("\tparallel_host_slots(%s) threshold load adjustment reduces slots"
                  " from %d to %d\n", eh_name, hslots, t_max);
            hslots = t_max;
         }

         // reduce by what we already booked on this host
         hslots -= already_booked_on_host(a->gdil, eh_name);
      }
   }

   *slots = hslots;
   *slots_qend = hslots_qend;

   if (result == DISPATCH_OK) {
      DPRINTF("\tparallel_host_slots(%s) returns %d/%d\n", eh_name, hslots, hslots_qend);
   } else {
      DPRINTF("\tparallel_host_slots(%s) returns <error>\n", eh_name);
   }

   DRETURN(result);
}

/****** sge_select_queue/parallel_tag_hosts_queues() **********************************
*  NAME
*     parallel_tag_hosts_queues() -- Determine host slots and tag queue(s) accordingly
*
*  SYNOPSIS
*
*  FUNCTION
*     For a particular job the maximum number of slots that could be served
*     at that host is determined in accordance with the allocation rule and
*     returned. The time of the assignment can be either DISPATCH_TIME_NOW
*     or a specific time, but never DISPATCH_TIME_QUEUE_END.
*
*     In those cases when the allocation rule allows more than one slot be
*     served per host it is necessary to also consider per queue possibly
*     specified load thresholds. This is because load is global/per host
*     concept while load thresholds are a queue attribute.
*
*     In those cases when the allocation rule gives us neither a fixed amount
*     of slots required nor an upper limit for the number per host slots (i.e.
*     $fill_up and $round_robin) we must iterate through all slot numbers from
*     1 to the maximum number of slots "total_slots" and check with each slot
*     amount whether we can get it or not. Iteration stops when we can't get
*     more slots the host based on the queue limitations and load thresholds.
*
*     As long as only one single queue at the host is eligible for the job
*     it is sufficient to check with each iteration whether the corresponding
*     number of slots can be served by the host and it's queue or not. The
*     really sick case however is when multiple queues are eligible for a host:
*     Here we have to determine in each iteration step also the maximum number
*     of slots each queue could get us by doing a per queue iteration from the
*     1 up to the maximum number of slots we're testing. The optimization in
*     effect here is to check always only if we could get more slots than with
*     the former per host slot amount iteration.
*
*  INPUTS
*     sge_assignment_t *a          -
*     lListElem *hep               - current host
*     lListElem *global            - global host
*     int *slots                   - out: # free slots
*     int *slots_qend              - out: # free slots in the far far future
*     int global_soft_violations   - # of global soft violations
*     bool *master_host            - out: if true, found a master host
*     category_use_t *use_category - int/out : how to use the job category
*
*  RESULT
*     static dispatch_t -  0 ok got an assignment
*                          1 no assignment at the specified time
*                         -1 assignment will never be possible for all jobs of that category
*                         -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: parallel_tag_hosts_queues() is not MT safe
*******************************************************************************/
static dispatch_t
parallel_tag_hosts_queues(sge_assignment_t *a, lListElem *hep, int *slots, int *slots_qend, bool need_master,
                          bool is_master_host, bool *master_host, category_use_t *use_category,
                          lList **unclear_cqueue_list)
{
   bool suited_as_master_host = false;
   int min_host_slots = 1;
   int max_host_slots = a->slots;
   int accu_queue_slots, accu_queue_slots_qend;
   int qslots, qslots_qend;
   int hslots = 0;
   int hslots_qend = 0;
   const char *cqname, *qname, *eh_name = lGetHost(hep, EH_name);
   lListElem *qep, *next_queue;
   dispatch_t result = DISPATCH_OK;
   const void *queue_iterator = nullptr;
   int allocation_rule;

   DENTER(TOP_LAYER);

   allocation_rule = sge_pe_slots_per_host(a->pe, a->slots);

   if (ALLOC_RULE_IS_BALANCED(allocation_rule)) {
      min_host_slots = max_host_slots = allocation_rule;
   }

   if (lGetUlong(a->job, JB_ar) == 0) {
      parallel_host_slots(a, &hslots, &hslots_qend, hep, need_master, is_master_host, false);
   } else {
      // Scheduling a job into its AR
      // @todo have we already made sure that we only get here when the AR actually has this host?
      const lList *config_attr = lGetList(hep, EH_consumable_config_list);
      const lList *actual_attr = lGetList(hep, EH_resource_utilization);
      const lList *load_attr = lGetList(hep, EH_load_list);
      lListElem *rep;
      dstring reason = DSTRING_INIT;

      // in the request list tag requests with HOST_TAG which are available
      lListElem *jrs;
      for_each_rw (jrs, lGetListRW(a->job, JB_request_set_list)) {
         if (!need_master && lGetUlong(jrs, JRS_scope) == JRS_SCOPE_MASTER) {
            continue;
         }
         lList *hard_resource_list = lGetListRW(jrs, JRS_hard_resource_list);
         clear_resource_tags(hard_resource_list, HOST_TAG);
         for_each_rw(rep, hard_resource_list) {
            const char *attrname = lGetString(rep, CE_name);
            lListElem *cplx_el = lGetElemStrRW(a->centry_list, CE_name, attrname);

            if (lGetUlong(cplx_el, CE_consumable) != CONSUMABLE_NO) {
               continue;
            }
            sge_dstring_clear(&reason);
            cplx_el = get_attribute(attrname, config_attr, actual_attr, load_attr, a->centry_list, a->load_adjustments,
                                    nullptr,
                                    DOMINANT_LAYER_HOST, 0, &reason, false, DISPATCH_TIME_NOW, 0);
            if (cplx_el != nullptr) {
               if (match_static_resource(1, rep, cplx_el, &reason, false) == DISPATCH_OK) {
                  lSetUlong(rep, CE_tagged, HOST_TAG);
               }
               lFreeElem(&cplx_el);
            }
         }
      }
      sge_dstring_free(&reason);

      // sum up the number of slots which got granted to the AR as available slots
      // @todo what if multiple jobs are running in the AR? => another reason for CS-430?
      {
         const lListElem *gdil_ep;
         const void *iterator = nullptr;
         const lListElem *ar = ar_list_locate(a->ar_list, lGetUlong(a->job, JB_ar));
         const lList *granted_list = lGetList(ar, AR_granted_slots);

         for (gdil_ep = lGetElemHostFirst(granted_list, JG_qhostname, eh_name, &iterator);
              gdil_ep != nullptr;
              gdil_ep = lGetElemHostNext(granted_list, JG_qhostname, eh_name, &iterator)) {
              hslots += lGetUlong(gdil_ep, JG_slots);
         }
         hslots_qend = hslots;
      }
      // @todo (CS-458) what about putting all queues and hosts which are not in the AR into skip lists?
   } // end checking if job can run in AR


   DPRINTF("HOST %s itself (and queue threshold) will get us %d slots (%d later) ... "
         "we need %d\n", eh_name, hslots, hslots_qend, min_host_slots);

   hslots      = MIN(hslots,      max_host_slots);
   hslots_qend = MIN(hslots_qend, max_host_slots);


   if (hslots >= min_host_slots || hslots_qend >= min_host_slots) {
      lList *skip_queue_list = nullptr;
      if (use_category->use_category) {
         skip_queue_list = lGetListRW(use_category->cache, CCT_ignore_queues);
      }

      accu_queue_slots = accu_queue_slots_qend = 0;

      // check and tag all qinstances on this host
      for (next_queue = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &queue_iterator);
          (qep = next_queue);
           next_queue = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &queue_iterator)) {

         qname = lGetString(qep, QU_full_name);
         cqname = lGetString(qep, QU_qname);

         if (lGetElemStr(a->skip_cqueue_list, CTI_name, cqname)) {
            /* QU_tag is already 0 */
            lSetUlong64(qep, QU_tag_qend, 0);
            DPRINTF("skipped due to cluster queue %s\n", cqname);
            continue;
         }

         if (skip_queue_list && lGetElemStr(skip_queue_list, CTI_name, qname)) {
            DPRINTF("skipped due to queue instance %s\n", qname);
            continue;
         }

         DPRINTF("checking queue %s because cqueue %s is not rejected\n", qname, cqname);
         result = parallel_queue_slots(a, qep, &qslots, &qslots_qend, need_master, is_master_host, false);

         if (result == DISPATCH_OK && (qslots > 0 || qslots_qend > 0)) {
            /* could this host be a master host */
            if (!suited_as_master_host && lMatchUlongBitMask(qep, QU_tagged4schedule, TAG4SCHED_MASTER | TAG4SCHED_MASTER_LATER)) {
               DPRINTF("HOST %s can be master host because of queue %s\n", eh_name, qname);
               suited_as_master_host = true;
            }

            DPRINTF("QUEUE %s TIME: %d + %d -> %d  QEND: %d + %d -> %d (%d soft violations)\n", qname,
                    accu_queue_slots,      qslots,      accu_queue_slots      + qslots,
                    accu_queue_slots_qend, qslots_qend, accu_queue_slots_qend + qslots_qend,
                    (int)lGetUlong(qep, QU_soft_violation));

            accu_queue_slots      += qslots;
            accu_queue_slots_qend += qslots_qend;
            lSetUlong(qep, QU_tag, qslots);
            lSetUlong64(qep, QU_tag_qend, qslots_qend);
         } else {
            lSetUlong(qep, QU_tag, 0);
            if (skip_queue_list) {
               lAddElemStr(&skip_queue_list, CTI_name, qname, CTI_Type);
            }
            DPRINTF("HOST(1.5) %s will get us nothing\n", eh_name);
         }

      } /* for each queue of the host */

      hslots      = MIN(accu_queue_slots,      hslots);
      hslots_qend = MIN(accu_queue_slots_qend, hslots_qend);

      DPRINTF("HOST %s and it's queues will get us %d slots (%d later) ... we need %d\n",
            eh_name, hslots, hslots_qend, min_host_slots);
   }

   *slots       = hslots;
   *slots_qend  = hslots_qend;
   *master_host = suited_as_master_host;

   DRETURN(DISPATCH_OK);
}

/*
 * Determine maximum number of host_slots as limited by queue load thresholds.
 * Goal is to prevent the queue(s) on the host to go into load alarm.
 * The maximum only considers thresholds with load adjustments
 *
 * for each queue Q at this host {
 *    for each threshold T of a queue {
 *       if compare operator > or >=
 *          avail(Q, T) = (threshold - load / adjustment) + 1
 *       else
 *          avail(Q, T) = (threshold - load / -adjustMent) + 1
 *    }
 *    avail(Q) = MIN(all avail(Q, T))
 * }
 * host_slot_max_by_T = MAX(all min(Q))
 */
static int
parallel_max_host_slots(sge_assignment_t *a, lListElem *host) {
   int avail_h = 0;
   const char *eh_name = lGetHost(host, EH_name);

   DENTER(TOP_LAYER);

   const void *queue_iterator = nullptr;
   lListElem *next_queue, *qep;
   for (next_queue = lGetElemHostFirstRW(a->queue_list, QU_qhostname, eh_name, &queue_iterator);
       (qep = next_queue);
        next_queue = lGetElemHostNextRW(a->queue_list, QU_qhostname, eh_name, &queue_iterator)) {
      int avail_q = INT_MAX;

      const lList *load_thresholds = lGetList(qep, QU_load_thresholds);
      const lListElem *tr;
      for_each_ep(tr, load_thresholds) {
         bool is_np_adjustment = false;
         lListElem *centry = nullptr;
         const lListElem *cep;
         const lListElem *lv, *fv, *lc;
         const char *load_value, *limit_value, *adj_value;
         double load, threshold, adjustment;
         const char *name = lGetString(tr, CE_name);
         if ((cep = centry_list_locate(a->centry_list, name)) == nullptr) {
            continue;
         }

         if (lGetUlong(cep, CE_consumable) == CONSUMABLE_NO) { /* work on the load values */
            if ((lc = lGetElemStr(a->load_adjustments, CE_name, name)) == nullptr) {
               continue;
            }
            if ((lv=lGetSubStr(host, HL_name, name, EH_load_list)) != nullptr) {
               load_value = lGetString(lv, HL_value);
            } else if ((lv = lGetSubStr(a->gep, HL_name, name, EH_load_list)) != nullptr) {
               load_value = lGetString(lv, HL_value);
            } else {
               fv = lGetSubStr(qep, CE_name, name, QU_consumable_config_list);
               load_value = lGetString(fv, CE_stringval);
            }
            adj_value = lGetString(lc, CE_stringval);
            is_np_adjustment = true;
            adj_value = lGetString(lc, CE_stringval);

         } else { /* work on a consumable */
            // we might have different requests (global or (master and slave)). Use the highest one to make sure
            // that queues will not go into load alarm.
            // BUT: E.g. in case of a high request for the master and low (no) requests for the slaves this could
            // possibly prevent a job from starting!
            lc = job_get_highest_hard_request(a->job, name);
            if (lc != nullptr) {
               adj_value = lGetString(lc, CE_stringval);
            } else { /* is default value */
               adj_value = lGetString(cep, CE_defaultval);
            }

            if ((centry = get_attribute_by_name(a->gep, host, qep, name, a->centry_list, a->load_adjustments, a->start,
                                                a->duration)) == nullptr) {
                /* no load value, no assigned consumable to queue, host, or global */
                DPRINTF("the consumable " SFN " used in queue " SFN " as load threshold has no instance at queue, host or global level\n",
                         name, lGetString(qep, QU_full_name));
                DRETURN(0);
            }

            load_value = lGetString(centry, CE_pj_stringval);
         }

         limit_value = lGetString(tr, CE_stringval);
         u_long32 type = lGetUlong(cep, CE_valtype);

         /* get the needed values. If the load value is not a number, ignore it */
         switch (type) {
            case TYPE_RSMAP:
            case TYPE_INT:
            case TYPE_TIM:
            case TYPE_MEM:
            case TYPE_BOO:
            case TYPE_DOUBLE:

               if (!parse_ulong_val(&load, nullptr, type, load_value, nullptr, 0) ||
                   !parse_ulong_val(&threshold, nullptr, type, limit_value, nullptr, 0) ||
                   !parse_ulong_val(&adjustment, nullptr, type, adj_value, nullptr, 0)) {
                   lFreeElem(&centry);
                  continue;
               }

               break;

            default:
               lFreeElem(&centry);
               continue;
         }
         /* the string in load_value is not needed anymore, we can free the element */
         lFreeElem(&centry);

         /* an adjustment of nullptr is ignored here. We can dispatch unlimited jobs based
            on the load value */
         if (adjustment == 0) {
            continue;
         }

         switch (lGetUlong(cep, CE_relop)) {
            case CMPLXEQ_OP :
            case CMPLXNE_OP : continue; /* we cannot compute a usefull range */
            case CMPLXLT_OP :
            case CMPLXLE_OP : adjustment *= -1;
               break;
         }

         if (is_np_adjustment) {
            load_np_value_adjustment(name, host, &adjustment);
         }

         /* load alarm is defined in a way, that a host can go with one
            used slot into alarm and not that the dispatching prevents
            the alarm state. For this behavior we have to add 1 to the
            available slots. However, this is not true for consumables
            as load threshold*/
         int avail = (threshold - load)/adjustment;
         if (lGetUlong(cep, CE_consumable) == CONSUMABLE_NO) {
            avail++;
         }
         avail_q = MIN(avail, avail_q);
      } // end loop over all load thresholds

      avail_h = MAX(avail_h, avail_q);
   }
   DRETURN(avail_h);
}

/****** sge_select_queue/sge_sequential_assignment() ***************************
*  NAME
*     sge_sequential_assignment() -- Make an assignment for a sequential job.
*
*  SYNOPSIS
*     int sge_sequential_assignment(sge_assignment_t *assignment)
*
*  FUNCTION
*     For sequential job assignments all the earliest job start time
*     is determined with each queue instance and the earliest one gets
*     chosen. Secondary criterion for queue selection minimizing jobs
*     soft requests.
*
*     The overall behaviour of this function is somewhat dependent on the
*     value that gets passed to assignment->start and whether soft requests
*     were specified with the job:
*
*     (1) In case of now assignemnts (DISPATCH_TIME_NOW) only the first queue
*         suitable for jobs without soft requests is tagged. When soft requests
*         are specified all queues must be verified and tagged in order to find
*         the queue that fits best. On success the start time is set
*
*     (2) In case of queue end assignments (DISPATCH_TIME_QUEUE_END)
*
*
*  INPUTS
*     sge_assignment_t *assignment - ???
*
*  RESULT
*     int - 0 ok got an assignment + time (DISPATCH_TIME_NOW and DISPATCH_TIME_QUEUE_END)
*           1 no assignment at the specified time
*          -1 assignment will never be possible for all jobs of that category
*          -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: sge_sequential_assignment() is not MT safe
*******************************************************************************/
dispatch_t sge_sequential_assignment(sge_assignment_t *a)
{
   dispatch_t result;
   int old_logging = 0;

   DENTER(TOP_LAYER);

   if (a == nullptr) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   if (a->is_reservation && !a->is_advance_reservation){
      /* turn off messages for reservation scheduling */
      old_logging = schedd_mes_get_logging();
      schedd_mes_set_logging(0);
      sconf_set_mes_schedd_info(false);
   }

   sequential_update_host_order(a->host_list, a->queue_list);

   if (sconf_get_qs_state() != QS_STATE_EMPTY) {
      /*------------------------------------------------------------------
       *  There is no need to sort the queues after each dispatch in
       *  case:
       *
       *    1. The last dispatch run did not resort the queue list
       *       currently only DISPATCH_TYPE_PE_SOFT_REQ needs to sort
       *       the list based on the soft violations
       *    2. The hosts sort order has not changed since last dispatch.
       *       Load correction or consumables in the load formula can
       *       change the order of the hosts. We detect changings in the
       *       host order by comparing the host sequence number with the
       *       sequence number from previous run.
       * ------------------------------------------------------------------*/
      int last_dispatch_type = sconf_get_last_dispatch_type();
      if (last_dispatch_type == DISPATCH_TYPE_PE_SOFT_REQ ||
          last_dispatch_type == DISPATCH_TYPE_NONE ||
          sconf_get_host_order_changed()) {
         DPRINTF("SORTING HOSTS!\n");
         if (sconf_get_queue_sort_method() == QSM_LOAD) {
            lPSortList(a->queue_list, "%I+ %I+", QU_host_seq_no, QU_seq_no);
         } else {
            lPSortList(a->queue_list, "%I+ %I+", QU_seq_no, QU_host_seq_no);
         }
      }
      if (a->is_soft) {
         sconf_set_last_dispatch_type(DISPATCH_TYPE_FAST_SOFT_REQ);
      } else {
         sconf_set_last_dispatch_type(DISPATCH_TYPE_FAST);
      }
   }

   result = sequential_tag_queues_suitable4job(a);

   if (result == DISPATCH_OK) {
      const lListElem *qep;
      u_long64 job_start_time = U_LONG64_MAX;
      u_long32 min_soft_violations = U_LONG32_MAX;
      const lListElem *best_queue = nullptr;

      if (a->is_reservation) {
         for_each_ep(qep, a->queue_list) {
            if (lGetUlong(qep, QU_tag) != 0) {
               u_long64 temp_job_start_time = lGetUlong64(qep, QU_available_at);

               DPRINTF("    Q: %s " sge_u32" " sge_u64" (jst: " sge_u64")\n", lGetString(qep, QU_full_name),
                       lGetUlong(qep, QU_tag), temp_job_start_time, job_start_time);

               /* earlier start time has higher preference than lower soft violations */
               if ((job_start_time > temp_job_start_time) ||
                    (a->is_soft &&
                        min_soft_violations > lGetUlong(qep, QU_soft_violation) &&
                        job_start_time == temp_job_start_time)
                  ) {

                  best_queue = qep;
                  job_start_time = temp_job_start_time;
                  min_soft_violations = lGetUlong(qep, QU_soft_violation);
               }
            }
         }
         if (best_queue) {
            DPRINTF("earliest queue \"%s\" at " sge_u64"\n", lGetString(best_queue, QU_full_name), job_start_time);
         } else {
            DPRINTF("no earliest queue found!\n");
         }
      } else {

         for_each_ep(qep, a->queue_list) {
            if (lGetUlong(qep, QU_tag)) {
               job_start_time = lGetUlong64(qep, QU_available_at);
               best_queue = qep;
               break;
            }
         }
      }

      if (!best_queue) {
         DRETURN(DISPATCH_NEVER_CAT); /* should never happen */
      }
      {
         lListElem *gdil_ep;
         lList *gdil = nullptr;
         const char *qname = lGetString(best_queue, QU_full_name);
         const char *eh_name = lGetHost(best_queue, QU_qhostname);

         DPRINTF(sge_u32": 1 slot in queue %s user %s %s for " sge_u64"\n",
            a->job_id, qname, a->user? a->user:"<unknown>",
                  !a->is_reservation?"scheduled":"reserved", job_start_time);

         gdil_ep = lAddElemStr(&gdil, JG_qname, qname, JG_Type);
         lSetUlong(gdil_ep, JG_qversion, lGetUlong(best_queue, QU_version));
         lSetHost(gdil_ep, JG_qhostname, eh_name);
         lSetUlong(gdil_ep, JG_slots, 1);

         if (!a->is_reservation) {
            sconf_inc_fast_jobs();
         }

         lFreeList(&(a->gdil));
         a->gdil = gdil;
         a->slots = 1;
         if (a->start == DISPATCH_TIME_QUEUE_END) {
            a->start = job_start_time;
         }

         // @todo CS-731 copy task specific binding decision into the JG-element
      }
   }

   if (DPRINTF_IS_ACTIVE) {
      DSTRING_STATIC(dstr, 64);
      switch (result) {
         case DISPATCH_OK:
         DPRINTF("SEQUENTIAL ASSIGNMENT(" sge_u32"." sge_u32") returns <time> %s\n", a->job_id, a->ja_task_id,
                 sge_ctime64(a->start, &dstr));
            break;
         case DISPATCH_NOT_AT_TIME:
         DPRINTF("SEQUENTIAL ASSIGNMENT(" sge_u32"." sge_u32") returns <later>\n", a->job_id, a->ja_task_id);
            break;
         case DISPATCH_NEVER_CAT:
         DPRINTF("SEQUENTIAL ASSIGNMENT(" sge_u32"." sge_u32") returns <category_never>\n", a->job_id, a->ja_task_id);
            break;
         case DISPATCH_NEVER_JOB:
         DPRINTF("SEQUENTIAL ASSIGNMENT(" sge_u32"." sge_u32") returns <job_never>\n", a->job_id, a->ja_task_id);
            break;
         case DISPATCH_MISSING_ATTR:
         default:
         DPRINTF("!!!!!!!! SEQUENTIAL ASSIGNMENT(" sge_u32"." sge_u32") returns unexpected %d\n", a->job_id,
                 a->ja_task_id, result);
            break;
      }
   }

   if (a->is_reservation && !a->is_advance_reservation) {
      schedd_mes_set_logging(old_logging);
      sconf_set_mes_schedd_info(true);
   }

   DRETURN(result);
}

/*------------------------------------------------------------------
 *  FAST TRACK FOR SEQUENTIAL JOBS WITHOUT A SOFT REQUEST
 *
 *  It is much faster not to review slots in a comprehensive fashion
 *  for jobs of this type.
 * ------------------------------------------------------------------*/
static int sequential_update_host_order(lList *host_list, lList *queues)
{
   lListElem *hep, *qep;
   double previous_load = 0;
   int previous_load_inited = 0;
   u_long32 host_seqno = 0;
   const char *eh_name;
   const void *iterator = nullptr;
   bool host_order_changed = false;

   DENTER(TOP_LAYER);

   if (!sconf_get_host_order_changed()) {
      DRETURN(0);
   }

   for_each_rw (hep, host_list) { /* in share/load order */

      /* figure out host_seqno
         in case the load of two hosts is equal this
         must be also reflected by the sequence number */
      if (!previous_load_inited) {
         host_seqno = 0;
         previous_load = lGetDouble(hep, EH_sort_value);
         previous_load_inited = 1;
      } else {
         if (previous_load < lGetDouble(hep, EH_sort_value)) {
            host_seqno++;
            previous_load = lGetDouble(hep, EH_sort_value);
         }
      }

      /* set host_seqno for all queues of this host */
      eh_name = lGetHost(hep, EH_name);

      qep = lGetElemHostFirstRW(queues, QU_qhostname, eh_name, &iterator);
      while (qep != nullptr) {
          lSetUlong(qep, QU_host_seq_no, host_seqno);
          qep = lGetElemHostNextRW(queues, QU_qhostname, eh_name, &iterator);
      }

      /* detect whether host_seqno has changed since last dispatch operation */
      if (host_seqno != lGetUlong(hep, EH_seq_no)) {
         DPRINTF("HOST SORT ORDER CHANGED FOR HOST %s FROM %d to %d\n", eh_name, lGetUlong(hep, EH_seq_no), host_seqno);
         host_order_changed = true;
         lSetUlong(hep, EH_seq_no, host_seqno);
      }
   }

   sconf_set_host_order_changed(host_order_changed);

   DRETURN(0);
}

/****** sge_select_queue/parallel_assignment() *****************************
*  NAME
*     parallel_assignment() -- Can we assign with a fixed PE/slot/time
*
*  SYNOPSIS
*     int parallel_assignment(sge_assignment_t *assignment)
*
*  FUNCTION
*     Returns if possible an assignment for a particular PE with a
*     fixed slot at a fixed time.
*
*  INPUTS
*     sge_assignment_t *a -
*     category_use_t *use_category - has information on how to use the job category
*
*  RESULT
*     dispatch_t -  0 ok got an assignment
*                   1 no assignment at the specified time
*                  -1 assignment will never be possible for all jobs of that category
*                  -2 assignment will never be possible for that particular job
*
*  NOTES
*     MT-NOTE: parallel_assignment() is not MT safe
*******************************************************************************/
static dispatch_t
parallel_assignment(sge_assignment_t *a, category_use_t *use_category, int *available_slots)
{
   dispatch_t ret;
   int pslots = a->slots;
   int pslots_qend = 0;

   DENTER(TOP_LAYER);

   if (a == nullptr) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   // check how many slots are available in the PE and if this is enough
   if ((lGetUlong(a->job, JB_ar) == 0) && (ret = parallel_available_slots(a, &pslots, &pslots_qend)) != DISPATCH_OK) {
      *available_slots = MIN(pslots, pslots_qend);
      DRETURN(ret);
   }
   if (a->slots > pslots) {
      *available_slots = MIN(pslots, pslots_qend);
      if (a->slots > pslots_qend) {
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_PESLOTSNOTINRANGE_SI, a->pe_name, pslots_qend);
         DRETURN(DISPATCH_NEVER_CAT);
      }
      DRETURN(DISPATCH_NOT_AT_TIME);
   }

   /* depends on a correct host order */
   ret = parallel_tag_queues_suitable4job(a, use_category, available_slots);

   if (ret != DISPATCH_OK) {
      DRETURN(ret);
   }

   /* must be understood in the context of changing queue sort orders */
   if (a->is_soft) {
      sconf_set_last_dispatch_type(DISPATCH_TYPE_PE_SOFT_REQ);
   } else {
      sconf_set_last_dispatch_type(DISPATCH_TYPE_PE);
   }

   /* DG TODO here ok to create the rankfile list if necessary? */

   DRETURN(ret);
}



/****** sched/select_queue/parallel_queue_slots() *************************
*  NAME
*     parallel_queue_slots() --
*
*  RESULT
*     int - 0 ok got an assignment + set time for DISPATCH_TIME_NOW and
*             DISPATCH_TIME_QUEUE_END (only with fixed_slot equals true)
*           1 no assignment at the specified time
*          -1 assignment will never be possible for all jobs of that category
******************************************************************************/
static dispatch_t
parallel_queue_slots(sge_assignment_t *a, lListElem *qep, int *slots, int *slots_qend, bool need_master,
                     bool is_master_queue, bool allow_non_requestable)
{
   const lList *config_attr = lGetList(qep, QU_consumable_config_list);
   const lList *actual_attr = lGetList(qep, QU_resource_utilization);
   const char *qname = lGetString(qep, QU_full_name);
   int qslots = 0, qslots_qend = 0;
   int lslots = INT_MAX, lslots_qend = INT_MAX;

   dispatch_t result = DISPATCH_NEVER_CAT;

   DENTER(TOP_LAYER);

   SCHED_PROF_INC(a->pi, par_qstat);

   if (sge_queue_match_static(a, qep) == DISPATCH_OK) {
      const lListElem *gdil;

      u_long32 ar_id = lGetUlong(a->job, JB_ar);
      if (ar_id != 0) {
         lListElem *ar_queue;
         lListElem *rep;
         const lList *ar_queue_config_attr;
         const lList *ar_queue_actual_attr;
         const lListElem *ar_ep = lGetElemUlong(a->ar_list, AR_id, ar_id);
         dstring reason = DSTRING_INIT;

         clear_resource_tags(a->job, QUEUE_TAG);

         ar_queue_config_attr = lGetList(qep, QU_consumable_config_list);
         ar_queue_actual_attr = lGetList(qep, QU_resource_utilization);

         lListElem *jrs;
         for_each_rw (jrs, lGetList(a->job, JB_request_set_list)) {
            lList *hard_resource_list = lGetListRW(jrs, JRS_hard_resource_list);
            for_each_rw (rep, hard_resource_list) {
               const char *attrname = lGetString(rep, CE_name);
               lListElem *cplx_el = lGetElemStrRW(a->centry_list, CE_name, attrname);

               if (lGetUlong(cplx_el, CE_consumable)) {
                  continue;
               }
               sge_dstring_clear(&reason);
               cplx_el = get_attribute(attrname, ar_queue_config_attr, ar_queue_actual_attr, nullptr, a->centry_list,
                                       a->load_adjustments, nullptr,
                                       DOMINANT_LAYER_QUEUE, 0, &reason, false, DISPATCH_TIME_NOW, 0);
               if (cplx_el != nullptr) {
                  if (match_static_resource(1, rep, cplx_el, &reason, false) == DISPATCH_OK) {
                     lSetUlong(rep, CE_tagged, PE_TAG);
                  }
                  lFreeElem(&cplx_el);
               }
            }
         }
         sge_dstring_free(&reason);

         ar_queue = lGetSubStrRW(ar_ep, QU_full_name, qname, AR_reserved_queues);
         ar_queue_config_attr = lGetList(ar_queue, QU_consumable_config_list);
         ar_queue_actual_attr = lGetList(ar_queue, QU_resource_utilization);

         DPRINTF("verifying AR queue\n");
         lSetUlong(ar_queue, QU_tagged4schedule, lGetUlong(qep, QU_tagged4schedule));

         result = parallel_rc_slots_by_time(a, &qslots, &qslots_qend,
                                            ar_queue_config_attr, ar_queue_actual_attr, nullptr, true, ar_queue,
                                            DOMINANT_LAYER_QUEUE, 0, QUEUE_TAG, need_master, is_master_queue,
                                            false, lGetString(ar_queue, QU_full_name), false);
         lSetUlong(qep, QU_tagged4schedule, lGetUlong(ar_queue, QU_tagged4schedule));
      } else {
         if (a->is_advance_reservation
            || (((a->pi)?a->pi->par_rqs++:0), result = parallel_rqs_slots_by_time(a, &lslots, &lslots_qend, qep,
                                                                                  need_master, is_master_queue)) == DISPATCH_OK) {
            DPRINTF("verifying normal queue\n");

            SCHED_PROF_INC(a->pi, par_qdyn);

            result = parallel_rc_slots_by_time(a, &qslots, &qslots_qend,
                                               config_attr, actual_attr, nullptr, true, qep,
                                               DOMINANT_LAYER_QUEUE, 0, QUEUE_TAG, need_master, is_master_queue,
                                               false, lGetString(qep, QU_full_name), false);
         }
      }

      // we already have tasks running on this qinstance (round_robin), consider the already booked slots
      // @todo in case of round robin: don't we cache the number of possible slots in QU_tag? Really re-calculate them over and over again?
      if ((gdil=lGetElemStr(a->gdil, JG_qname, lGetString(qep, QU_full_name))))
         qslots -= lGetUlong(gdil, JG_slots);
   }


   *slots = MIN(qslots, lslots);
   *slots_qend = MIN(qslots_qend, lslots_qend);

   if (result == DISPATCH_OK || result == DISPATCH_NOT_AT_TIME) {
      DPRINTF("\tparallel_queue_slots(%s) returns %d/%d\n", qname, qslots, qslots_qend);
      result = DISPATCH_OK;
   } else {
      DPRINTF("\tparallel_queue_slots(%s) returns <error>\n", qname);
   }

   DRETURN(result);
}

/****** sched/select_queue/sequential_queue_time() *************************
*  NAME
*     sequential_queue_time() --
*
*  RESULT
*      dispatch_t - 0 ok got an assignment + set time for DISPATCH_TIME_NOW and
*                     DISPATCH_TIME_QUEUE_END (only with fixed_slot equals true)
*                   1 no assignment at the specified time
*                  -1 assignment will never be possible for all jobs of that category
******************************************************************************/
static dispatch_t
sequential_queue_time(u_long64 *start, const sge_assignment_t *a, int *violations, lListElem *qep)
{
   dstring reason;
   char reason_buf[1024];
   dispatch_t result;
   u_long64 tmp_time = *start;
   lList *hard_requests = job_get_hard_resource_listRW(a->job);
   const lList *config_attr = lGetList(qep, QU_consumable_config_list);
   const lList *actual_attr = lGetList(qep, QU_resource_utilization);
   const char *qname = lGetString(qep, QU_full_name);

   DENTER(TOP_LAYER);

   sge_dstring_init(&reason, reason_buf, sizeof(reason_buf));

   /* match the resources */
   result = rc_time_by_slots(a, hard_requests, nullptr, config_attr, actual_attr,
                            qep, false, &reason, 1, DOMINANT_LAYER_QUEUE,
                            0, QUEUE_TAG, &tmp_time, qname);

   if (result == DISPATCH_OK) {
      if (violations != nullptr) {
         *violations = compute_soft_violations(a, qep, *violations, nullptr, config_attr, actual_attr,
                                           DOMINANT_LAYER_QUEUE, 0, QUEUE_TAG);
      }
   } else {
      char buff[1024 + 1];
      centry_list_append_to_string(hard_requests, buff, sizeof(buff) - 1);
      if (*buff && (buff[strlen(buff) - 1] == '\n')) {
         buff[strlen(buff) - 1] = 0;
      }
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                     SCHEDD_INFO_CANNOTRUNINQUEUE_SSS, buff, qname, reason_buf);
   }

   if (a->is_reservation && result == DISPATCH_OK) {
      *start = tmp_time;
      DPRINTF("queue_time_by_slots(%s) returns earliest start time " sge_u64"\n", qname, *start);
   } else if (result == DISPATCH_OK) {
      DPRINTF("queue_time_by_slots(%s) returns <at specified time>\n", qname);
   } else {
      DPRINTF("queue_time_by_slots(%s) returns <later>\n", qname);
   }

   DRETURN(result);
}




/****** sge_select_queue/sequential_host_time() ******************************
*  NAME
*     sequential_host_time() -- Return time when host slots are available
*
*  SYNOPSIS
*     int sequential_host_time(int slots, u_long32 *start, u_long32 duration,
*     int *host_soft_violations, lListElem *job, lListElem *ja_task, lListElem
*     *hep, lList *centry_list, lList *acl_list)
*
*  FUNCTION
*     The time when the specified slot amount is available at the host
*     is determined. Behaviour depends on input/output parameter start
*
*     DISPATCH_TIME_NOW
*           0 an assignment is possible now
*           1 no assignment now but later
*          -1 assignment never possible for all jobs of the same category
*          -2 assignment never possible for that particular job
*
*     <any other time>
*           0 an assignment is possible at the specified time
*           1 no assignment at specified time but later
*          -1 assignment never possible for all jobs of the same category
*          -2 assignment never possible for that particular job
*
*     DISPATCH_TIME_QUEUE_END
*           0 an assignment is possible and the start time is returned
*          -1 assignment never possible for all jobs of the same category
*          -2 assignment never possible for that particular job
*
*  INPUTS
*     int slots                 - ???
*     u_long32 *start           - ???
*     u_long32 duration         - ???
*     int *host_soft_violations - ???
*     lListElem *job            - ???
*     lListElem *ja_task        - ???
*     lListElem *hep            - ???
*     lList *centry_list        - ???
*     lList *acl_list           - ???
*
*  RESULT
*******************************************************************************/
static dispatch_t
sequential_host_time(u_long64 *start, const sge_assignment_t *a, int *violations, const lListElem *hep)
{
   lList *hard_requests = job_get_hard_resource_listRW(a->job);
   const lList *load_attr = lGetList(hep, EH_load_list);
   const lList *config_attr = lGetList(hep, EH_consumable_config_list);
   const lList *actual_attr = lGetList(hep, EH_resource_utilization);
   double lc_factor = 0;
   u_long32 ulc_factor;
   dispatch_t result;
   u_long64 tmp_time = *start;
   const char *eh_name = lGetHost(hep, EH_name);
   dstring reason; char reason_buf[1024];

   DENTER(TOP_LAYER);

   sge_dstring_init(&reason, reason_buf, sizeof(reason_buf));

   clear_resource_tags(hard_requests, HOST_TAG);

   /* cause load be raised artificially to reflect load correction when
      checking job requests */
   if (lGetPosViaElem(hep, EH_load_correction_factor, SGE_NO_ABORT) >= 0) {
      if ((ulc_factor=lGetUlong(hep, EH_load_correction_factor)))
         lc_factor = ((double)ulc_factor)/100;
   }

   result = rc_time_by_slots(a, hard_requests, load_attr,
         config_attr, actual_attr, nullptr, false,
         &reason, 1, DOMINANT_LAYER_HOST,
         lc_factor, HOST_TAG, &tmp_time, eh_name);

   if (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR) {
      if (violations != nullptr) {
         *violations = compute_soft_violations(a, nullptr, *violations, load_attr, config_attr,
                                           actual_attr, DOMINANT_LAYER_HOST, 0, HOST_TAG);
      }
   } else {
      char buff[1024 + 1];
      centry_list_append_to_string(hard_requests, buff, sizeof(buff) - 1);
      if (*buff && (buff[strlen(buff) - 1] == '\n'))
         buff[strlen(buff) - 1] = 0;
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                     SCHEDD_INFO_CANNOTRUNATHOST_SSS, buff, eh_name, reason_buf);
   }

   if (a->is_reservation && (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR)) {
      *start = tmp_time;
      DPRINTF("sequential_host_time(%s) returns earliest start time " sge_u64"\n", eh_name, *start);
   } else if (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR) {
      DPRINTF("sequential_host_time(%s) returns <at specified time>\n", eh_name);
   } else {
      DPRINTF("sequential_host_time(%s) returns <later>\n", eh_name);
   }

   DRETURN(result);
}

/****** sched/select_queue/sequential_global_time() ***************************
*  NAME
*     sequential_global_time() --
*
*  RESULT
*     int - 0 ok got an assignment + set time for DISPATCH_TIME_QUEUE_END
*           1 no assignment at the specified time
*          -1 assignment will never be possible for all jobs of that category
******************************************************************************/
static dispatch_t
sequential_global_time(u_long64 *start, const sge_assignment_t *a, int *violations)
{
   dstring reason; char reason_buf[1024];
   dispatch_t result = DISPATCH_NEVER_CAT;
   u_long64 tmp_time = *start;
   lList *hard_request = job_get_hard_resource_listRW(a->job);
   const lList *load_attr = lGetList(a->gep, EH_load_list);
   const lList *config_attr = lGetList(a->gep, EH_consumable_config_list);
   const lList *actual_attr = lGetList(a->gep, EH_resource_utilization);
   double lc_factor=0.0;
   u_long32 ulc_factor;

   DENTER(TOP_LAYER);

   sge_dstring_init(&reason, reason_buf, sizeof(reason_buf));

   clear_resource_tags(hard_request, GLOBAL_TAG);

   /* check if job has access to any hosts globally */
   if ((result=sge_host_match_static(a, a->gep)) != 0) {
      DRETURN(result);
   }

   /* cause global load be raised artificially to reflect load correction when
      checking job requests */
   if (lGetPosViaElem(a->gep, EH_load_correction_factor, SGE_NO_ABORT) >= 0) {
      if ((ulc_factor=lGetUlong(a->gep, EH_load_correction_factor)))
         lc_factor = ((double)ulc_factor)/100;
   }

   result = rc_time_by_slots(a, hard_request, load_attr, config_attr, actual_attr, nullptr, false, &reason,
                             1, DOMINANT_LAYER_GLOBAL, lc_factor, GLOBAL_TAG, &tmp_time, SGE_GLOBAL_NAME);

   if ((result == DISPATCH_OK) || (result == DISPATCH_MISSING_ATTR)) {
      if (violations != nullptr) {
         *violations = compute_soft_violations(a, nullptr, *violations, load_attr, config_attr,
                                           actual_attr, DOMINANT_LAYER_GLOBAL, 0, GLOBAL_TAG);
      }
   } else {
      char buff[1024 + 1];
      centry_list_append_to_string(hard_request, buff, sizeof(buff) - 1);
      if (*buff && (buff[strlen(buff) - 1] == '\n')) {
         buff[strlen(buff) - 1] = 0;
      }
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                     SCHEDD_INFO_CANNOTRUNGLOBALLY_SS, buff, reason_buf);
   }

   if (a->is_reservation && (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR)) {
      *start = tmp_time;
      DPRINTF("global_time_by_slots() returns earliest start time " sge_u64"\n", *start);
   }
   else if (result == DISPATCH_OK || result == DISPATCH_MISSING_ATTR) {
      DPRINTF("global_time_by_slots() returns <at specified time>\n");
   }
   else {
      DPRINTF("global_time_by_slots() returns <later>\n");
   }

   DRETURN(result);
}

/****** sched/select_queue/parallel_global_slots() ***************************
*  NAME
*     parallel_global_slots() --
*
*  RESULT
*     dispatch_t -  0 ok got an assignment + set time for DISPATCH_TIME_QUEUE_END
*                   1 no assignment at the specified time
*                  -1 assignment will never be possible for all jobs of that category
******************************************************************************/
static dispatch_t
parallel_global_slots(const sge_assignment_t *a, int *slots, int *slots_qend)
{
   DENTER(TOP_LAYER);

   dispatch_t result = DISPATCH_NEVER_CAT;
   int gslots = 0, gslots_qend = 0;

   /* check if job has access (from acls, projects) to any hosts globally */
   if (sge_host_match_static(a, a->gep) == DISPATCH_OK) {
      /* cause global load be raised artificially to reflect load correction when checking job requests */
      double lc_factor=0.0;
      if (lGetPosViaElem(a->gep, EH_load_correction_factor, SGE_NO_ABORT) >= 0) {
         u_long32 ulc_factor;
         if ((ulc_factor=lGetUlong(a->gep, EH_load_correction_factor))) {
            lc_factor = ((double)ulc_factor)/100;
         }
      }

      const lList *load_attr = lGetList(a->gep, EH_load_list);
      const lList *config_attr = lGetList(a->gep, EH_consumable_config_list);
      const lList *actual_attr = lGetList(a->gep, EH_resource_utilization);

      clear_resource_tags(a->job, GLOBAL_TAG);
      result = parallel_rc_slots_by_time(a, &gslots, &gslots_qend,
                                         config_attr, actual_attr, load_attr, false, nullptr,
                                         DOMINANT_LAYER_GLOBAL,
                                         lc_factor, GLOBAL_TAG, true, false, false, SGE_GLOBAL_NAME, false);
   }

   *slots      = gslots;
   *slots_qend = gslots_qend;

   if (result == DISPATCH_OK) {
      DPRINTF("\tparallel_global_slots() returns %d/%d\n", gslots, gslots_qend);
   } else {
      // parallel_rc_slots_by_time() does only return DISPATCH_OK or DISPATCH_NOT_AT_TIME - not really an error?
      DPRINTF("\tparallel_global_slots() returns <error>\n");
   }

   DRETURN(result);
}

/****** sge_select_queue/parallel_available_slots() **********************************
*  NAME
*     parallel_available_slots() -- Check if number of PE slots is available
*
*  SYNOPSIS
*
*  FUNCTION
*
*  INPUTS
*
*  RESULT
*     dispatch_t - 0 ok got an assignment
*                  1 no assignment at the specified time
*                 -1 assignment will never be possible for all jobs of that category
*
*  NOTES
*     MT-NOTE: parallel_available_slots() is not MT safe
*******************************************************************************/
static dispatch_t
parallel_available_slots(const sge_assignment_t *a, int *slots, int *slots_qend)
{
   dstring reason;
   char reason_buf[1024];
   dispatch_t result;
   int total = lGetUlong(a->pe, PE_slots);
   static lListElem *implicit_slots_request = nullptr;
   static lList *implicit_total_list = nullptr;
   lListElem *tep = nullptr;
   char strbuf[100];
   dstring slots_as_str;

   DENTER(TOP_LAYER);

   sge_dstring_init(&reason, reason_buf, sizeof(reason_buf));

   if ((result=pe_match_static(a)) != DISPATCH_OK) {
      DRETURN(result);
   }

   // @todo CS-482 we should have the implicit slots request once, e.g. in the assignment structure
   if (implicit_slots_request == nullptr) {
      implicit_slots_request = lCreateElem(CE_Type);
      lSetString(implicit_slots_request, CE_name, SGE_ATTR_SLOTS);
      lSetString(implicit_slots_request, CE_stringval, "1");
      lSetDouble(implicit_slots_request, CE_doubleval, 1);
   }

   // @todo CS-482 PE slots should be stored in a PE_consumable_config_list
   if (!implicit_total_list) {
      tep = lAddElemStr(&implicit_total_list, CE_name, SGE_ATTR_SLOTS, CE_Type);
   }

   if (!tep && !(tep = lGetElemStrRW(implicit_total_list, CE_name, SGE_ATTR_SLOTS))) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   total = lGetUlong(a->pe, PE_slots);
   lSetDouble(tep, CE_doubleval, total);
   sge_dstring_init(&slots_as_str, strbuf, sizeof(strbuf));
   sge_dstring_sprintf(&slots_as_str, "%d", total);
   lSetString(tep, CE_stringval, strbuf);

   if (ri_slots_by_time(a, slots, slots_qend,
                        lGetList(a->pe, PE_resource_utilization), implicit_slots_request,
                        nullptr, implicit_total_list, nullptr, nullptr, 0, 0, &reason, true, true, a->pe_name) != DISPATCH_OK) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   DPRINTF("\tparallel_available_slots(%s) returns %d/%d\n", a->pe_name, *slots, *slots_qend);

   DRETURN(DISPATCH_OK);
}


/* ----------------------------------------

   sge_get_double_qattr()

   writes actual value of the queue attriute into *uvalp

   returns:
      0 ok, value in *uvalp is valid
      -1 the queue has no such attribute
      -2 type error: cant compute uval from actual string value

*/
int
sge_get_double_qattr(double *dvalp, const char *attrname, const lListElem *q,
                     const lList *exechost_list, const lList *centry_list,
                     bool *has_value_from_object)
{
   int ret = -1;
   lListElem *ep;
   u_long32 type;
   double tmp_dval;
   char dom_str[4];
   lListElem *global = nullptr;
   lListElem *host = nullptr;

   DENTER(TOP_LAYER);

   global = host_list_locate(exechost_list, SGE_GLOBAL_NAME);
   host = host_list_locate(exechost_list, lGetHost(q, QU_qhostname));

   /* find matching */
   *has_value_from_object = false;
   if (( ep = get_attribute_by_name(global, host, q, attrname, centry_list, nullptr, DISPATCH_TIME_NOW, 0)) &&
       ((type=lGetUlong(ep, CE_valtype)) != TYPE_STR) &&
       (type != TYPE_CSTR) && (type != TYPE_RESTR) && (type != TYPE_HOST) ) {

         if ((lGetUlong(ep, CE_pj_dominant)&DOMINANT_TYPE_MASK)!=DOMINANT_TYPE_VALUE ) {
            parse_ulong_val(&tmp_dval, nullptr, type, lGetString(ep, CE_pj_stringval), nullptr, 0);
            monitor_dominance(dom_str, lGetUlong(ep, CE_pj_dominant));
            *has_value_from_object = true;
         } else {
            parse_ulong_val(&tmp_dval, nullptr, type, lGetString(ep, CE_stringval), nullptr, 0);
            monitor_dominance(dom_str, lGetUlong(ep, CE_dominant));
            *has_value_from_object = ((lGetUlong(ep, CE_dominant) & DOMINANT_TYPE_MASK) == DOMINANT_TYPE_VALUE) ? false : true;
         }
      ret = 0;
      if (dvalp)
         *dvalp = tmp_dval;
      DPRINTF("resource %s: %f\n", dom_str, tmp_dval);
   }

   /* free */
   lFreeElem(&ep);

   DRETURN(ret);
}


/* ----------------------------------------

   sge_get_string_qattr()

   writes string value into dst

   returns:
      -1    if the queue has no such attribute
      0
*/
int sge_get_string_qattr(
char *dst,
int dst_len,
const char *attrname,
lListElem *q,
const lList *exechost_list,
const lList *centry_list
) {
   lListElem *ep;
   lListElem *global = nullptr;
   lListElem *host = nullptr;
   int ret = -1;

   DENTER(TOP_LAYER);

   global = host_list_locate(exechost_list, SGE_GLOBAL_NAME);
   host = host_list_locate(exechost_list, lGetHost(q, QU_qhostname));

   ep = get_attribute_by_name(global, host, q, attrname, centry_list, nullptr, DISPATCH_TIME_NOW, 0);

   /* first copy ... */
   if (ep && dst)
      sge_strlcpy(dst, lGetString(ep, CE_stringval), dst_len);

   if (ep){
      lFreeElem(&ep);
      ret = 0;
   }

   DRETURN(ret);
}

/****** sge_select_queue/ri_time_by_slots() ******************************************
*  NAME
*     ri_time_by_slots() -- Determine availability time through slot number
*
*  SYNOPSIS
*     int ri_time_by_slots(lListElem *rep, lList *load_attr, lList
*     *config_attr, lList *actual_attr, lList *centry_list, lListElem *queue,
*     char *reason, int reason_size, bool allow_non_requestable, int slots,
*     u_long32 layer, double lc_factor)
*
*  FUNCTION
*     Checks for one level, if one request is fulfilled or not.
*
*     With reservation scheduling the earliest start time due to
*     availability of the resource instance is determined by ensuring
*     non-consumable resource requests are fulfilled or by finding the
*     earliest time utilization of a consumable resource is below the
*     threshold required for the request.
*
*  INPUTS
*     sge_assignment_t *a       - assignment object that holds job specific scheduling relevant data
*     lListElem *rep            - requested attribute
*     lList *load_attr          - list of load attributes or null on queue level
*     lList *config_attr        - list of user defined attributes (CE_Type)
*     lList *actual_attr        - usage of user consumables (RUE_Type)
*     lListElem *queue          - the current queue, or null on host level
*     dstring *reason           - target for error message
*     bool allow_non_requestable - allow none requestable attributes?
*     int slots                 - the number of slotes the job is looking for?
*     u_long32 layer            - the current layer
*     double lc_factor          - load correction factor
*     u_long64 *start_time      - in/out argument for start time
*     const char *object_name   - name of the object used for monitoring purposes
*
*  RESULT
*     dispatch_t -
*
*******************************************************************************/
dispatch_t
ri_time_by_slots(const sge_assignment_t *a, lListElem *rep, const lList *load_attr, const lList *config_attr,
                 const lList *actual_attr, const lListElem *queue, dstring *reason, bool allow_non_requestable,
                 int slots, u_long32 layer, double lc_factor, u_long64 *start_time, const char *object_name)
{
   lListElem *cplx_el=nullptr;
   const char *attrname;
   dispatch_t ret = DISPATCH_OK;
   const lListElem *actual_el;
   u_long64 ready_time;
   double util, total, request = 0;
   const lListElem *capacitiy_el;
   bool schedule_based = (a->is_advance_reservation || a->is_schedule_based) ? true : false;
   u_long64 now = a->now;
   int utilized = 0;
   bool is_exclusive = false;

   DENTER(TOP_LAYER);

   attrname = lGetString(rep, CE_name);
   actual_el = lGetElemStr(actual_attr, RUE_name, attrname);
   ready_time = *start_time;

   /*
    * Consumables are treated futher below in schedule based mode
    * thus we always assume zero consumable utilization here
    */

   if (!(cplx_el = get_attribute(attrname, config_attr, actual_attr, load_attr, a->centry_list, a->load_adjustments,
                                 queue, layer,
                                 lc_factor, reason, schedule_based, DISPATCH_TIME_NOW, 0))) {
      DRETURN(DISPATCH_MISSING_ATTR);
   }

   DPRINTF("ri_time_by_slots(%s) consumable = %s\n", attrname, map_consumable2str(lGetUlong(cplx_el, CE_consumable)));

   ret = match_static_resource(slots, rep, cplx_el, reason, allow_non_requestable);
   if (actual_el != nullptr) {
      utilized = lGetNumberOfElem(lGetList(actual_el, RUE_utilized));
   }

   if (ret != DISPATCH_OK || (!schedule_based && utilized == 0 && lGetUlong(cplx_el, CE_relop) != CMPLXEXCL_OP)) {
      DPRINTF("skipping expensive checks, utilized is %d\n", utilized);
      lFreeElem(&cplx_el);
      DRETURN(ret);
   }

   if (lGetUlong(cplx_el, CE_consumable) == CONSUMABLE_NO) {
      if (ready_time == DISPATCH_TIME_QUEUE_END) {
         *start_time = now;
      }
      DPRINTF("%s: ri_time_by_slots(%s) <is no consumable>\n", object_name, attrname);
      lFreeElem(&cplx_el);
      DRETURN(DISPATCH_OK); /* already checked */
   }

   /* we're done if there is no consumable capacity */
   if (!(capacitiy_el = lGetElemStr(config_attr, CE_name, attrname))) {
      DPRINTF("%s: ri_time_by_slots(%s) <does not exist>\n", object_name, attrname);
      lFreeElem(&cplx_el);
      DRETURN(DISPATCH_MISSING_ATTR); /* does not exist */
   }

   /* determine 'total' and 'request' values */
   total = lGetDouble(capacitiy_el, CE_doubleval);

   if (!parse_ulong_val(&request, nullptr, lGetUlong(cplx_el, CE_valtype),
      lGetString(rep, CE_stringval), nullptr, 0)) {
      sge_dstring_append(reason, "wrong type");
      lFreeElem(&cplx_el);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   if (request != 0.0) {
      DPRINTF("exclusive_request\n");
      is_exclusive = true;
   } else {
      DPRINTF("non-exclusive_request\n");
   }

   if (ready_time == DISPATCH_TIME_QUEUE_END) {
      double threshold = total - request * slots;

      /* verify there are enough resources in principle */
      if (threshold < 0) {
         ret = DISPATCH_NEVER_CAT;
      } else {
         /* seek for the time near queue end where resources are sufficient */
         u_long64 when = utilization_below(actual_el, threshold, object_name, is_exclusive);
         if (when == 0) {
            /* may happen only if scheduler code is run outside scheduler with
               DISPATCH_TIME_QUEUE_END time spec */
            *start_time = now;
         } else {
            *start_time = when;
         }

         ret = DISPATCH_OK;

         /* DG TODO  RSMAP host consumable: when it is ok, check if specific ID
          * is also available, if not then return DISPATCH_NOT_AT_TIME ...
          */
      }

      DPRINTF("\t\t%s: time_by_slots: %d of %s=%f can be served %s\n", object_name, slots, attrname, request,
               ret == DISPATCH_OK ? "at time" : "never");

      lFreeElem(&cplx_el);

      DRETURN(ret);
   }

   if (lGetUlong(cplx_el, CE_relop) == CMPLXEXCL_OP) {

      /* here we handle DISPATCH_TIME_NOW + any other time */
      ready_time = *start_time;

      util = utilization_max(actual_el, ready_time, a->duration, is_exclusive);
      if (util == 0.0) {
         if (schedule_based || utilized != 0) {
            /* check for reservations */
            ready_time = now;
            util = utilization_max(actual_el, ready_time, a->duration, is_exclusive);
            if (util == 0.0) {
               ret = DISPATCH_OK;
            } else {
               ret = DISPATCH_NOT_AT_TIME;
            }
         } else {
            ret = DISPATCH_OK;
         }
      } else {
         ret = DISPATCH_NOT_AT_TIME;
      }

      if (ret != DISPATCH_OK) {
         sge_dstring_sprintf(reason, MSG_SCHEDD_EXCLUSIVE_IN_USE_S, attrname);
      }

   } else {
      /* here we handle DISPATCH_TIME_NOW + any other time */
      if (*start_time == DISPATCH_TIME_NOW) {
         ready_time = now;
      } else {
         ready_time = *start_time;
      }

      util = utilization_max(actual_el, ready_time, a->duration, false);

      DPRINTF("\t\t%s: time_by_slots: %s total = %f util = %f from " sge_u64 " plus " sge_u64 " microseconds\n",
              object_name, attrname, total, util, ready_time, a->duration);

      /* ensure resource is sufficient from now until finish */
      if (request * slots > total - util) {
         char dom_str[5];
         dstring availability; char availability_text[2048];

         sge_dstring_init(&availability, availability_text, sizeof(availability_text));

         /* we can't assign right now - maybe later ? */
         if (request * slots > total) {
            DPRINTF("\t\t%s: time_by_slots: %s %f > %f (never)\n", object_name, attrname, request * slots, total);
            ret = DISPATCH_NEVER_CAT; /* surely not */
         } else if (request * slots > total - utilization_queue_end(actual_el, false)) {
            DPRINTF("\t\t%s: time_by_slots: %s %f <= %f (but booked out!!)\n", object_name, attrname, request * slots, total);
            ret = DISPATCH_NEVER_CAT; /* booked out until infinity */
         } else {
            DPRINTF("\t\t%s: time_by_slots: %s %f > %f (later)\n", object_name, attrname, request * slots, total - util);
            ret = DISPATCH_NOT_AT_TIME;
         }

         monitor_dominance(dom_str, DOMINANT_TYPE_CONSUMABLE | layer);
         sge_dstring_sprintf(&availability, "%s:%s=%f", dom_str, attrname, total - util);
         sge_dstring_append(reason, MSG_SCHEDD_ITOFFERSONLY);
         sge_dstring_append(reason, availability_text);

         if ((a->duration != DISPATCH_TIME_NOW) &&
             (request * slots <= total - utilization_max(actual_el, ready_time, DISPATCH_TIME_NOW, false))) {
            sge_dstring_append(reason, MSG_SCHEDD_DUETORR);
         }
      } else {
         ret = DISPATCH_OK;
      }
   }

   lFreeElem(&cplx_el);

   DPRINTF("\t\t%s: time_by_slots: %d of %s=%f can be served %s\n", object_name, slots, attrname, request,
            ret == DISPATCH_OK ? "at time" : ((ret == DISPATCH_NOT_AT_TIME)? "later":"never"));

   DRETURN(ret);
}

/****** sge_select_queue/ri_slots_by_time() ************************************
*  NAME
*     ri_slots_by_time() -- Determine number of slots avail. within time frame
*
*  SYNOPSIS
*     static dispatch_t ri_slots_by_time(const sge_assignment_t *a, int *slots,
*     int *slots_qend, lList *rue_list, lListElem *request, lList *load_attr,
*     lList *total_list, lListElem *queue, u_long32 layer, double lc_factor,
*     dstring *reason, bool allow_non_requestable, bool no_centry, const char
*     *object_name)
*
*  FUNCTION
*     The number of slots available with a resource can be zero for static
*     resources or is determined based on maximum utilization within the
*     specific time frame, the total amount of the resource and the per
*     task request of the parallel job (ri_slots_by_time())
*
*  INPUTS
*     const sge_assignment_t *a  - ???
*     int *slots                 - Returns maximum slots that can be served
*                                  within the specified time frame.
*     int *slots_qend            - Returns the maximum possible number of slots
*     lList *rue_list            - Resource utilization (RUE_Type)
*     lListElem *request         - Job request (CE_Type)
*     lList *load_attr           - Load information for the resource
*     lList *total_list          - Total resource amount (CE_Type)
*     lListElem *queue           - Queue instance (QU_Type) for queue-based resources
*     u_long32 layer             - DOMINANT_LAYER_{GLOBAL|HOST|QUEUE}
*     double lc_factor           - load correction factor
*     dstring *reason            - diagnosis information if no rsrc available
*     bool allow_non_requestable - ???
*     bool no_centry             - ???
*     const char *object_name    - ???
*
*  RESULT
*     static dispatch_t -
*
*  NOTES
*     MT-NOTE: ri_slots_by_time() is not MT safe
*******************************************************************************/
static dispatch_t
ri_slots_by_time(const sge_assignment_t *a, int *slots, int *slots_qend, const lList *rue_list, lListElem *request,
                 const lList *load_attr, const lList *total_list, const lList *additional_usage, lListElem *queue,
                 u_long32 layer, double lc_factor, dstring *reason, bool allow_non_requestable, bool no_centry,
                 const char *object_name)
{
   DENTER(TOP_LAYER);

   const lListElem *tep = nullptr;
   u_long64 start = a->start;
   bool schedule_based = a->is_advance_reservation || a->is_schedule_based;
   int utilized = 0;
   u_long32 consumable = CONSUMABLE_NO;
   bool exclusive_centry = false;

   dispatch_t ret = DISPATCH_OK;
   double used, total, request_val;

   /* always assume zero consumable utilization in schedule based mode */

   const char *name = lGetString(request, CE_name);
   const lListElem *uep = lGetElemStr(rue_list, RUE_name, name);

   DPRINTF("\t\t%s: ri_slots_by_time(%s)\n", object_name, name);

   if (!no_centry) {
      lListElem *cplx_el;
      if (!(cplx_el = get_attribute(name, total_list, rue_list, load_attr, a->centry_list, a->load_adjustments, queue,
                                    layer, lc_factor, reason, schedule_based,
                                    DISPATCH_TIME_NOW, 0))) {
         DRETURN(DISPATCH_MISSING_ATTR); /* does not exist */
      }

      // we match with 1 slot here - we do not know how much it might actually give us, this is calculated below
      ret = match_static_resource(1, request, cplx_el, reason, allow_non_requestable);
      if (ret != DISPATCH_OK) {
         lFreeElem(&cplx_el);
         DRETURN(ret);
      }

      consumable = lGetUlong(cplx_el, CE_consumable);
      if (ret == DISPATCH_OK && consumable == CONSUMABLE_NO) {
         lFreeElem(&cplx_el);
         *slots      = INT_MAX;
         *slots_qend = INT_MAX;
         DRETURN(DISPATCH_OK); /* no limitations */
      }

      /* we're done if there is no consumable capacity */
      if (!(tep=lGetElemStr(total_list, CE_name, name))) {
         lFreeElem(&cplx_el);
         *slots      = INT_MAX;
         *slots_qend = INT_MAX;
         DRETURN(DISPATCH_OK);
      }
      if (lGetUlong(cplx_el, CE_relop) == CMPLXEXCL_OP) {
         exclusive_centry = true;
      }
      lFreeElem(&cplx_el);
   }

   if (!tep && !(tep=lGetElemStr(total_list, CE_name, name))) {
      DRETURN(DISPATCH_NEVER_CAT);
   }

   request_val = lGetDouble(request, CE_doubleval);
   DPRINTF("\t\t%s: ri_slots_by_time: REQUESTED: %s=%f\n", object_name, name, request_val);
   if (exclusive_centry) {
      DPRINTF("\t\t%s: ri_slots_by_time: exclusive request %s\n", object_name, name);
      bool exclusive_request = false;
      if (request_val != 0.0) {
         DPRINTF("exclusive_request\n");
         exclusive_request = true;
      } else {
         DPRINTF("non-exclusive_request\n");
      }

      DPRINTF("schedule_based=%d, is_reservation=%d\n", schedule_based, a->is_reservation);
      if (schedule_based && !a->is_reservation) {
         start = a->now;
      }

      used = utilization_max(uep, start, a->duration, exclusive_request);
      if (used == 0.0) {
         ret = DISPATCH_OK;
         *slots = INT_MAX;
      } else {
         ret = DISPATCH_NOT_AT_TIME;
         *slots = 0;
      }
      used = utilization_queue_end(uep, exclusive_request);
      if (used == 0.0) {
         *slots_qend = INT_MAX;
      } else {
         ret = DISPATCH_NOT_AT_TIME;
         *slots_qend = 0;
      }

      if (ret != DISPATCH_OK) {
         sge_dstring_sprintf(reason, MSG_SCHEDD_EXCLUSIVE_IN_USE_S, name);
      }
   } else {
      DPRINTF("\t\t%s: ri_slots_by_time: non exclusive request %s\n", object_name, name);
      total = lGetDouble(tep, CE_doubleval);

      if (uep != nullptr) {
         utilized = lGetNumberOfElem(lGetList(uep, RUE_utilized));
      }

      if (!a->is_advance_reservation && sconf_get_qs_state() == QS_STATE_EMPTY) {
         DPRINTF("QS_STATE is empty, skipping extensive checks!\n");
         used = 0;
      } else if (schedule_based || utilized != 0) {
         DSTRING_STATIC(time_str, 64);
         if (!a->is_reservation) {
            start = a->now;
         }
         used = utilization_max(uep, start, a->duration, false);
         DPRINTF("\t\t%s: ri_slots_by_time: utilization_max(%s, " sge_u64") returns %f\n",
               object_name, sge_ctime64(start, &time_str), a->duration, used);
         // we have to consider additional usage (e.g. of the master task which has already been matched earlier)
         if (additional_usage != nullptr) {
            const lListElem *add_usage_ep = lGetElemStr(additional_usage, CE_name, name);
            if (add_usage_ep != nullptr) {
               DPRINTF("\t\t%s: ri_slots_by_time: additional usage for %s: %f\n", object_name, name, lGetDouble(add_usage_ep, CE_doubleval));
               used += lGetDouble(add_usage_ep, CE_doubleval);
            }
         }
      } else {
         used = lGetDouble(uep, RUE_utilized_now);
         // we have to consider additional usage (e.g. of the master task which has already been matched earlier)
         if (additional_usage != nullptr) {
            const lListElem *add_usage_ep = lGetElemStr(additional_usage, CE_name, name);
            if (add_usage_ep != nullptr) {
               DPRINTF("\t\t%s: ri_slots_by_time: additional usage for %s: %f\n", object_name, name, lGetDouble(add_usage_ep, CE_doubleval));
               used += lGetDouble(add_usage_ep, CE_doubleval);
            }
         }
      }

      if ((request_val != 0.0) && (total < DBL_MAX)) {
         *slots      = (int)((total - used) / request_val);
         if (uep) {
            *slots_qend = (int)((total - utilization_queue_end(uep, false)) / request_val);
         } else {
            *slots_qend = (int)(total / request_val);
         }
         if (consumable == CONSUMABLE_JOB || consumable == CONSUMABLE_HOST) {
            /* what the function returns is the number of slots which can be assigned to tasks
             * if slots here is at least 1 we know that the consumable request can be granted
             * at least once. We need it just once and can then dispatch any number of tasks (= INT_MAX)
             * to this resource
             */
            if (*slots == 0 || *slots_qend == 0) {
               ret = DISPATCH_NOT_AT_TIME;
            }

            if (layer != DOMINANT_LAYER_GLOBAL) {
               *slots = INT_MAX;
               *slots_qend = INT_MAX;
            } else {
               if (*slots > 0) {
                  *slots = INT_MAX;
               }
               if (*slots_qend > 0) {
                  *slots_qend = INT_MAX;
               }
            }
         }
      } else {
         *slots = INT_MAX;
         *slots_qend = INT_MAX;
      }

      if (*slots == 0 || *slots_qend == 0) {
         char dom_str[5];
         DSTRING_STATIC(availability, 1024);

         monitor_dominance(dom_str, DOMINANT_TYPE_CONSUMABLE | layer);
         sge_dstring_append(reason, MSG_SCHEDD_ITOFFERSONLY);
         sge_dstring_append(reason, sge_dstring_sprintf(&availability, "%s:%s=%f", dom_str, name, (total - used)));

         if ((a->duration != DISPATCH_TIME_NOW) &&
             (*slots != 0 && *slots_qend == 0)) {
            sge_dstring_append(reason, MSG_SCHEDD_DUETORR);
         }
      }

      DSTRING_STATIC(time_str, 64);
      DPRINTF("\t\t%s: ri_slots_by_time: %s=%f has %d (%d) slots at time %s %s (avail: %f total: %f)\n",
              object_name, name, request_val, *slots, *slots_qend, sge_ctime64(start, &time_str),
              !a->is_reservation?" (= now)":"", total - used, total);
   }

   DRETURN(ret);
}


/* Determine maximum number of host_slots as limited
   by job request to this host

   for each resource at this host requested by the job {
      avail(R) = (total - used) / request
   }
   host_slot_max_by_R = MIN(all avail(R))

   host_slot = MIN(host_slot_max_by_T, host_slot_max_by_R)

*/


// @todo CS-601 we should also clear the SLAVE tags in QU_tagged4schedule - we might need this for detecting disjoint
//              master/slave requests
//              also where we filter and sort the queue list
//              have a tag TAG4SCHED_DISJOINT? Here we would have the necessary information. Or a bool &disjoint.
dispatch_t
parallel_rc_slots_by_time(const sge_assignment_t *a, int *slots, int *slots_qend, const lList *total_list,
                          const lList *rue_list, const lList *load_attr, bool force_slots, lListElem *queue,
                          u_long32 layer, double lc_factor, u_long32 tag, bool need_master,
                          bool is_master_host, bool allow_non_requestable, const char *object_name,
                          bool isRQ)
{
   DSTRING_STATIC(reason, 1024);
   int avail = 0;
   int avail_qend = 0;
   int max_slots = INT_MAX, max_slots_qend = INT_MAX;
   const char *name;
   const lListElem *actual;
   lListElem *cep, *req;
   dispatch_t result, ret = DISPATCH_OK;

   DENTER(TOP_LAYER);

   clear_resource_tags(a->job, QUEUE_TAG);

   /* --- implicit slot request */
   name = SGE_ATTR_SLOTS;

   // if slots are not defined on host level
   // @todo what is force_slots?
   const lListElem *tep = lGetElemStr(total_list, CE_name, name);
   if (tep == nullptr && force_slots) {
      DRETURN(DISPATCH_OK); // no slots capacity defined on exec host level, it's the queue instances which limit it
   }

   // if slots is defined on host level then check how much is available
   if (tep != nullptr) {
      // @todo CS-482 not thread safe
      static lListElem *implicit_slots_request = nullptr;
      if (!implicit_slots_request) {
         implicit_slots_request = lCreateElem(CE_Type);
         lSetString(implicit_slots_request, CE_name, SGE_ATTR_SLOTS);
         lSetString(implicit_slots_request, CE_stringval, "1");
         lSetDouble(implicit_slots_request, CE_doubleval, 1);
      }

      result = ri_slots_by_time(a, &avail, &avail_qend,
                                rue_list, implicit_slots_request, load_attr, total_list, nullptr, queue, layer,
                                lc_factor,
                                &reason, allow_non_requestable, false, object_name);
      if (result != DISPATCH_OK) {
         /* If the request is made from the resource quota module, this error message
          * is not informative and should not be displayed */
         if (!isRQ) {
            schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                           SCHEDD_INFO_CANNOTRUNINQUEUE_SSS, "slots=1",
                           object_name, sge_dstring_get_string(&reason));
         }
         // @todo CS-601 remove all tags. This could come from a host (but we don't have the object!)
         //              or a queue - we have it
         DRETURN(result);
      }

      max_slots      = MIN(max_slots,      avail);
      max_slots_qend = MIN(max_slots_qend, avail_qend);
      DPRINTF("%s: parallel_rc_slots_by_time(%s) %d (%d later)\n", object_name, name,
            max_slots, max_slots_qend);
   }


   /* --- default requests except slots which we handled above */
   for_each_ep(actual, rue_list) {
      name = lGetString(actual, RUE_name);
      if (strcmp(name, SGE_ATTR_SLOTS) == 0) {
         continue;
      }
      cep = centry_list_locate(a->centry_list, name);

      // we need to check how much would give us a default request if the attribute is not requested
      // if there are any tasks (master or slave) which have no request
      if (!is_requested(a->job, name)) {
         double request;
         const char *def_req = lGetString(cep, CE_defaultval);
         if (def_req != nullptr) {
            // @todo (CS-459) we should have a CE_default_doubleval which gets filled once and is used here!
            parse_ulong_val(&request, nullptr, lGetUlong(cep, CE_valtype), def_req, nullptr, 0);

            if (request != 0 || lGetUlong(cep, CE_relop) == CMPLXEXCL_OP) {
               lSetString(cep, CE_stringval, def_req);
               lSetDouble(cep, CE_doubleval, request);

               result = ri_slots_by_time(a, &avail, &avail_qend,
                                         rue_list, cep, load_attr, total_list, nullptr, queue, layer, lc_factor,
                                         &reason, allow_non_requestable, false, object_name);
               if (result != DISPATCH_OK) {
                  if (!isRQ) {
                     DSTRING_STATIC(dstr, 1024);
                     const char *req_str = sge_dstring_sprintf(&dstr, "default request %s=%s", name, def_req);
                     schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                                    SCHEDD_INFO_CANNOTRUNINQUEUE_SSS, req_str,
                                    object_name, sge_dstring_get_string(&reason));
                  }
                  // @todo CS-601 remove all tags. This could come from a host (but we don't have the object!)
                  //              or a queue - we have it
                  DRETURN(DISPATCH_NEVER_CAT);
               }
               max_slots      = MIN(max_slots,      avail);
               max_slots_qend = MIN(max_slots_qend, avail_qend);
               DPRINTF("%s: parallel_rc_slots_by_time(%s) %d (%d later)\n", object_name, name,
                     (int)max_slots, (int)max_slots_qend);
            }
         }
      }
   }

   /* --- explicit requests */
   // check all the 3 potential request lists here
   // first global, if it exists then it sets new max_slots
   // second master, if it exists. It does not touch max_slots but might clear MASTER on QU_tagged4schedule
   // third slave, if it exists.

   // when matching the slave requests we have to consider the usage caused by a master task, store it here
   lList *master_usage = nullptr;
   // when calculating the max_slots after matching the slave requests we need to add the one slot we might
   // have got for the master task
   int master_slot = 0, master_slot_qend = 0;
   // we might not have global or master requests at all
   // so lets assume that a master task can run,
   // if global or master request matching fails then revert master_slot / master_slot_qend to 0
   if ((need_master || is_master_host) && lGetBool (a->pe, PE_job_is_first_task)) {
      master_slot = master_slot_qend = 1; // we have a master slot now and later
   }
   lListElem *jrs;
   for_each_rw (jrs, lGetListRW(a->job, JB_request_set_list)) {
      u_long32 scope = lGetUlong(jrs, JRS_scope);
      if (scope == JRS_SCOPE_MASTER) {
         if (!need_master) {
            DPRINTF("%s: parallel_rc_slots_by_time() no need to handle master requests\n", object_name);
            if (is_master_host) {
               // we already have the master host, need to respect the master usage when looking at slave requests
               // in a second round for allocation rule round_robin
               DPRINTF("%s: parallel_rc_slots_by_time() we already have the master task, considering master usage in slave matching\n", object_name);
               master_usage = lGetListRW(jrs, JRS_hard_resource_list);
               // @todo only if we actually have master_usage? We might have a master JRS with just queues
               // @todo is this really correct even if master_usage has different entries than the slave requests?
               //       need to do it per request? Or only if there is some overlap between the master and slave requests?
               //       We have such scenarios in the scope_basic test (-scope master -l int=1 -scope slave -l dbl=2,
               //       and they work fine.
            }
            continue;
         } else {
            if (queue != nullptr) {
               DPRINTF("=====> we are processing master requests and have a queue\n");
               print_tagged4schedule(queue);
               if (!lMatchUlongBitMask(queue, QU_tagged4schedule, TAG4SCHED_MASTER | TAG4SCHED_MASTER_LATER)) {
                  // we still need the master task, but the queue is already tagged not to match (from exechost matching)
                  DPRINTF("%s: parallel_rc_slots_by_time() master queue already tagged not to match, skipping\n", object_name);
                  continue;
               }
            }
         }
      }
      if (scope == JRS_SCOPE_SLAVE && queue != nullptr) {
         DPRINTF("=====> we are processing slave requests and have a queue\n");
         print_tagged4schedule(queue);
         if (!lMatchUlongBitMask(queue, QU_tagged4schedule, TAG4SCHED_SLAVE | TAG4SCHED_SLAVE_LATER)) {
            // we still need slave tasks, but the queue is already tagged not to match (from exechost matching)
            DPRINTF("%s: parallel_rc_slots_by_time() slave queue already tagged not to match, skipping\n", object_name);
            continue;
         }
      }
      // @todo do the same check as above also for the global request

      // consider PE setting ign_sreq_on_mhost if we are on the (potential) master host
      if (scope == JRS_SCOPE_SLAVE &&
          (need_master || is_master_host) &&
          lGetBool(a->pe, PE_ignore_slave_requests_on_master_host)) {
         if (master_slot != 0) {
            // we can run the master task here, ignore slave requests
            DPRINTF("%s: parallel_rc_slots_by_time() ign_sreq_on_mhost TRUE\n", object_name);
            continue;
         } else {
            // we cannot run the master task here, try to use the host/the queue for slave tasks
            DPRINTF("%s: parallel_rc_slots_by_time() ign_sreq_on_mhost TRUE but no master task, potential slave host\n",
                    object_name);
         }
      }

      DPRINTF("%s: parallel_rc_slots_by_time() testing %s requests, master_slot = %d\n", object_name,
              job_scope_name(scope), master_slot);

      lList *requests = lGetListRW(jrs, JRS_hard_resource_list);
      for_each_rw (req, requests) {
         name = lGetString(req, CE_name);
         DPRINTF("    ==> %s: %s = %s = %f\n", object_name, name, lGetString(req, CE_stringval), lGetDouble(req, CE_doubleval));
         result = ri_slots_by_time(a, &avail, &avail_qend,
                                   rue_list, req, load_attr, total_list, master_usage, queue, layer, lc_factor,
                                   &reason, allow_non_requestable, false, object_name);
         DPRINTF("  -> ri_slots_by_time returned %d\n", result);
         if (result == DISPATCH_NEVER_CAT || result == DISPATCH_NEVER_JOB) {
            // if we are checking global requests, and it is a CONSUMABLE_JOB:
            // special handling: the queue can still be used as slave queue
            if (scope == JRS_SCOPE_GLOBAL && lGetUlong(req, CE_consumable) == CONSUMABLE_JOB) {
               DPRINTF("===> CONSUMABLE_JOB %s does not match - can still use %s %s as slave queue\n",
                       name, queue == nullptr ? "host" : "qinstance", object_name);
               host_or_queue_clear_tags(object_name, queue, a->queue_list, TAG4SCHED_MASTER | TAG4SCHED_MASTER_LATER);
               /* misuse of the DISPATCH_MISSING_ATTR
                * add a new DISPATCH result, e.g. DISPATCH_HANDLE_CONSUMABLE_JOB
                */
               result = DISPATCH_MISSING_ATTR;
            } else if (!isRQ) {
               DSTRING_STATIC(dstr, 1024);
               const char *req_str = centry_list_append_to_dstring(requests, &dstr);
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_CANNOTRUNINQUEUE_SSS, req_str, object_name,
                              sge_dstring_get_string(&reason));
            }
            // @todo CS-601 do we want to untag in other situations, e.g. global requests do not match: untag all?
         } else if (result == DISPATCH_NOT_AT_TIME) {
            if (scope == JRS_SCOPE_GLOBAL && lGetUlong(req, CE_consumable) == CONSUMABLE_JOB) {
               DPRINTF("===> CONSUMABLE_JOB %s does not match now - can still use %s %s as slave queue (and master later)\n",
                       name, queue == nullptr ? "host" : "qinstance", object_name);
               host_or_queue_clear_tags(object_name, queue, a->queue_list, TAG4SCHED_MASTER);
            } else {
               DSTRING_STATIC(dstr, 1024);
               const char *req_str = centry_list_append_to_dstring(requests, &dstr);
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_CANNOTRUNINQUEUE_SSS, req_str, object_name,
                              sge_dstring_get_string(&reason));
            }
            // @todo CS-601 do we want to untag in other situations, e.g. global requests do not match: untag all now?
            //       problem: it can be a host (which we don't have) or a queue (which we have)
         }

         // in case of master requests we do not update max_slots and/or return in case of error
         // the queue might still be used as slave queue
         // we just update QU_tagged4schedule
         // @todo CS-601 untag in all cases, make function get_tags4sched(scope, also_later)
         if (scope == JRS_SCOPE_MASTER) {
            switch (result) {
               case DISPATCH_OK:
                  master_usage = requests;            // for slave matching need to consider what the master task would consume
                  // @todo CS-620 sometimes ri_slots_by_time() seems to return 0 (DISPATCH_OK) instead of DISPATCH_NOT_AT_TIME
                  if (avail == 0) {
                     master_slot = 0;
                     master_usage = nullptr;
                     host_or_queue_clear_tags(object_name, queue, a->queue_list, TAG4SCHED_MASTER);
                  }
                  break;
               case DISPATCH_NOT_AT_TIME:
                  // not suitable now, but later
                  host_or_queue_clear_tags(object_name, queue, a->queue_list, TAG4SCHED_MASTER);
                  master_usage = requests; // for slave matching need to consider what the master task would consume
                  DPRINTF("    --> we matched the master requests, -> not at time, clearing master tags and master_slot\n");
                  master_slot = 0;
                  break;
               case DISPATCH_MISSING_ATTR:
                  // we are e.g. on queue level, e.g. a memory consumable does not exist here
                  // but has already been matched on higher levels - fine, we can use the queue as master queue
                  if (tag < lGetUlong(req, CE_tagged)) {
                     DPRINTF("   --> we matched the master requests, -> missing attr, but already satisfied\n");
                  }
               break;
               case DISPATCH_NEVER_CAT:
               case DISPATCH_NEVER_JOB:
                  DPRINTF("    --> we matched the master requests, -> never_cat or never_job, clearing master tags\n");
                  host_or_queue_clear_tags(object_name, queue, a->queue_list, TAG4SCHED_MASTER | TAG4SCHED_MASTER_LATER);
                  master_slot = master_slot_qend = 0; // no master slot
                  // continue with slave tasks, they might match, and we can use the queue as slave only queue
                  continue;
               default:
                  master_slot = master_slot_qend = 0; // no master slot
                  // error states
                  // fall through to the switch below where we possibly return, e.g. on DISPATCH_MISSING_ATTR
                  break;
            }
         }

         // @todo in case of slave request with master_usage and it doesn't give us enough
         // this means that we cannot put one master task + slave tasks onto the host
         // but perhaps only slave tasks would be OK?
         // we'd have to try the slave request again without master_usage?
         // probably would have to be done somewhere in a higher level, e.g. in parallel_tag_queues_suitable4job

         switch (result) {
            case DISPATCH_OK:          // matches now
            case DISPATCH_NOT_AT_TIME: // will match later-on
               ret = result;

               DPRINTF("%s: explicit request for %s gets us %d slots (%d later)\n",
                       object_name, name, avail, avail_qend);
               if (lGetUlong(req, CE_tagged) < tag && tag != RQS_TAG)
                  lSetUlong(req, CE_tagged, tag);

               // slave requests matched we might have to add one slot for the master task
               if (scope != JRS_SCOPE_MASTER) {
                  if (scope == JRS_SCOPE_SLAVE) {
                     if (avail < INT_MAX) {
                        avail += master_slot;
                     }
                     if (avail_qend < INT_MAX) {
                        avail_qend += master_slot_qend;
                     }
                  }
                  max_slots = MIN(max_slots, avail);
                  max_slots_qend = MIN(max_slots_qend, avail_qend);
               }
               DPRINTF("%s: parallel_rc_slots_by_time(%s) %d (%d later)\n", object_name, name,
                       max_slots, max_slots_qend);
               print_tagged4schedule(queue);
               break;

            case DISPATCH_NEVER_CAT: /* the requested element does not exist */
            DPRINTF("%s: parallel_rc_slots_by_time(%s) <never cat>\n", object_name, name);
               print_tagged4schedule(queue);
               *slots = *slots_qend = 0;
               DRETURN(DISPATCH_NEVER_CAT);

            case DISPATCH_NEVER_JOB: /* the requested element does not exist */
            DPRINTF("%s: parallel_rc_slots_by_time(%s) <never job>\n", object_name, name);
               print_tagged4schedule(queue);
               *slots = *slots_qend = 0;
               DRETURN(DISPATCH_NEVER_JOB);

            case DISPATCH_MISSING_ATTR: /* the requested element does not exist */
               if (tag == QUEUE_TAG && lGetUlong(req, CE_tagged) == NO_TAG) {
                  if (lGetUlong(req, CE_consumable) == CONSUMABLE_JOB) {
                     max_slots = MIN(max_slots, avail);
                     max_slots_qend = MIN(max_slots_qend, avail_qend);
                     // only suitable for slave tasks
                     // @todo we did this already above, why repeat it? Can it get overwritten in between?
                     host_or_queue_clear_tags(object_name, queue, a->queue_list, TAG4SCHED_MASTER | TAG4SCHED_MASTER_LATER);
                  } else {
                     DPRINTF("%s: parallel_rc_slots_by_time(%s) <never found>\n", object_name, name);
                     *slots = *slots_qend = 0;
                     DRETURN(DISPATCH_NEVER_CAT);
                  }
               }
               DPRINTF("%s: parallel_rc_slots_by_time(%s) no such resource, but already satisfied\n", object_name,
                       name);
               print_tagged4schedule(queue);
               break;

            case DISPATCH_NEVER:
            default :
            DPRINTF("unexpected return code\n");
         } // end switch result

         if (scope == JRS_SCOPE_GLOBAL) {
            // if we get here we have finished handling global requests and have updated max_slots/max_slots_qend
            // with the results for the global requests
            // if we still search for a master host/queue, then need to remember that this host/queue is suitable
            switch (result) {
               case DISPATCH_OK:
                  break;
               case DISPATCH_NOT_AT_TIME:
                  // if the global requests have matched (and there might not be any master specific requests)
                  // the host / the queue might be suited for the master task - need to consider its slot
                  if (need_master && lGetBool (a->pe, PE_job_is_first_task)) {
                     master_slot = 0;
                  }
                  break;
               case DISPATCH_MISSING_ATTR:
                  // we are e.g. on queue level, e.g. a memory consumable does not exist here
                  // but has already been matched on higher levels - fine, we can use the queue as master queue
                  if (tag < lGetUlong(req, CE_tagged)) {
                     DPRINTF("   --> we matched the global requests, -> missing attr, but already satisfied\n");
                  }
                  break;
               default:
                  // cannot be master host - actually cannot run anything at all, see below
                  master_slot = master_slot_qend = 0;
                  break;
            }
         }
         // @todo if one request of a scope has not matched, we can break out of the request loop (?)
      } // end for each request per scope
      // @todo if global requests have not matched, no need to check other scopes
   } // end for each scope


   // @todo CS-731 if DISPATCH_OK: place for additional binding specific checks

   *slots = max_slots;
   *slots_qend = max_slots_qend;

   DRETURN(ret);
}

/****** sge_select_queue/sge_create_load_list() ********************************
*  NAME
*     sge_create_load_list() -- create the controll structure for consumables as
*                               load thresholds
*
*  SYNOPSIS
*     void sge_create_load_list(const lList *queue_list, const lList
*     *host_list, const lList *centry_list, lList **load_list)
*
*  FUNCTION
*     scanes all queues for consumables as load thresholds. It builds a
*     consumable category for each queue which is using consumables as a load
*     threshold.
*     If no consumables are used, the *load_list is set to nullptr.
*
*  INPUTS
*     const lList *queue_list  - a list of queue instances
*     const lList *host_list   - a list of hosts
*     const lList *centry_list - a list of complex entries
*     lList **load_list        - a ref to the target load list
*
*  NOTES
*     MT-NOTE: sge_create_load_list() is MT safe
*
*  SEE ALSO
*     sge_create_load_list
*     load_locate_elem
*     sge_load_list_alarm
*     sge_remove_queue_from_load_list
*     sge_free_load_list
*
*******************************************************************************/
void sge_create_load_list(const lList *queue_list, const lList *host_list,
                          const lList *centry_list, lList **load_list) {
   lListElem *queue;
   lListElem *load_threshold;
   lListElem *centry;
   const lList * load_threshold_list;
   const char *load_threshold_name;
   const char *limit_value;
   lListElem *global;
   lListElem *host;

   DENTER(TOP_LAYER);

   if (load_list == nullptr){
      CRITICAL("no load_list specified\n");
      abort();
   }

   if (*load_list != nullptr){
      sge_free_load_list(load_list);
   }

   if ((global = host_list_locate(host_list, SGE_GLOBAL_NAME)) == nullptr) {
      ERROR("no global host in sge_create_load_list");
      DRETURN_VOID;
   }

   for_each_rw(queue, queue_list) {
      load_threshold_list = lGetList(queue, QU_load_thresholds);
      for_each_rw(load_threshold, load_threshold_list) {
         load_threshold_name = lGetString(load_threshold, CE_name);
         limit_value = lGetString(load_threshold, CE_stringval);
         if ((centry = centry_list_locate(centry_list, load_threshold_name)) == nullptr) {
            ERROR(MSG_SCHEDD_WHYEXCEEDNOCOMPLEX_S, load_threshold_name);
            goto error;
         }

         if (lGetUlong(centry, CE_consumable) != CONSUMABLE_NO) {
            lListElem *global_consumable = nullptr;
            lListElem *host_consumable = nullptr;
            lListElem *queue_consumable = nullptr;

            lListElem *load_elem = nullptr;
            lListElem *queue_ref_elem = nullptr;
            lList *queue_ref_list = nullptr;


            if ((host = host_list_locate(host_list, lGetHost(queue, QU_qhostname))) == nullptr){
               ERROR(MSG_SGETEXT_INVALIDHOSTINQUEUE_SS, lGetHost(queue, QU_qhostname), lGetString(queue, QU_full_name));
               goto error;
            }

            global_consumable = lGetSubStrRW(global, RUE_name, load_threshold_name, EH_resource_utilization);
            host_consumable = lGetSubStrRW(host, RUE_name, load_threshold_name, EH_resource_utilization);
            queue_consumable = lGetSubStrRW(queue, RUE_name, load_threshold_name, QU_resource_utilization);

            if (*load_list == nullptr) {
               *load_list = lCreateList("load_ref_list", LDR_Type);
               if (*load_list == nullptr) {
                  goto error;
               }
            } else {
               load_elem = load_locate_elem(*load_list, global_consumable,
                                            host_consumable, queue_consumable,
                                            limit_value);
            }
            if (load_elem == nullptr) {
               load_elem = lCreateElem(LDR_Type);
               if (load_elem == nullptr) {
                  goto error;
               }
               lSetPosRef(load_elem, LDR_global_pos, global_consumable);
               lSetPosRef(load_elem, LDR_host_pos, host_consumable);
               lSetPosRef(load_elem, LDR_queue_pos, queue_consumable);
               lSetPosString(load_elem, LDR_limit_pos, limit_value);
               lAppendElem(*load_list, load_elem);
            }

            queue_ref_list = lGetPosList(load_elem, LDR_queue_ref_list_pos);
            if (queue_ref_list == nullptr) {
               queue_ref_list = lCreateList("", QRL_Type);
               if (queue_ref_list == nullptr) {
                  goto error;
               }
               lSetPosList(load_elem, LDR_queue_ref_list_pos, queue_ref_list);
            }

            queue_ref_elem = lCreateElem(QRL_Type);
            if (queue_ref_elem == nullptr) {
               goto error;
            }
            lSetRef(queue_ref_elem, QRL_queue, queue);
            lAppendElem(queue_ref_list, queue_ref_elem);
         }
      }
   }

   DRETURN_VOID;

error:
   DPRINTF("error in sge_create_load_list!");
   ERROR(SFNMAX, MSG_SGETEXT_CONSUMABLE_AS_LOAD);
   sge_free_load_list(load_list);
   DRETURN_VOID;

}

/****** sge_select_queue/load_locate_elem() ************************************
*  NAME
*     load_locate_elem() -- locates a consumable category in the given load list
*
*  SYNOPSIS
*     static lListElem* load_locate_elem(lList *load_list, lListElem
*     *global_consumable, lListElem *host_consumable, lListElem
*     *queue_consumable)
*
*  INPUTS
*     lList *load_list             - the load list to work on
*     lListElem *global_consumable - a ref to the global consumable
*     lListElem *host_consumable   - a ref to the host consumable
*     lListElem *queue_consumable  - a ref to the qeue consumable
*
*  RESULT
*     static lListElem* - nullptr, or the category element from the load list
*
*  NOTES
*     MT-NOTE: load_locate_elem() is MT safe
*
*  SEE ALSO
*     sge_create_load_list
*     load_locate_elem
*     sge_load_list_alarm
*     sge_remove_queue_from_load_list
*     sge_free_load_list
*
*******************************************************************************/
static lListElem *load_locate_elem(lList *load_list, lListElem *global_consumable,
                            lListElem *host_consumable, lListElem *queue_consumable,
                            const char *limit) {
   lListElem *load_elem = nullptr;
   lListElem *load = nullptr;

   for_each_rw(load, load_list) {
      if ((lGetPosRef(load, LDR_global_pos) == global_consumable) &&
          (lGetPosRef(load, LDR_host_pos) == host_consumable) &&
          (lGetPosRef(load, LDR_queue_pos) == queue_consumable) &&
          ( strcmp(lGetPosString(load, LDR_limit_pos), limit) == 0)) {
         load_elem = load;
         break;
      }
   }

   return load_elem;
}

/****** sge_select_queue/sge_load_list_alarm() *********************************
*  NAME
*     sge_load_list_alarm() -- checks if queues went into an alarm state
*
*  SYNOPSIS
*     bool sge_load_list_alarm(lList *load_list, const lList *host_list, const
*     lList *centry_list)
*
*  FUNCTION
*     The function uses the cull bitfield to identify modifications in one of
*     the consumable elements. If the consumption has changed, the load for all
*     queue referencing the consumable is recomputed. If a queue exceeds it
*     load threshold, QU_tagged4schedule is set to 1.
*
*  INPUTS
*     lList *load_list         - ???
*     const lList *host_list   - ???
*     const lList *centry_list - ???
*
*  RESULT
*     bool - true, if at least one queue was set into alarm state
*
*  NOTES
*     MT-NOTE: sge_load_list_alarm() is MT safe
*
*  SEE ALSO
*     sge_create_load_list
*     load_locate_elem
*     sge_load_list_alarm
*     sge_remove_queue_from_load_list
*     sge_free_load_list
*
*******************************************************************************/
bool sge_load_list_alarm(bool monitor_next_run, lList *load_list, const lList *host_list,
                         const lList *centry_list) {
   lListElem *load;
   lListElem *queue;
   lListElem *queue_ref;
   lList *queue_ref_list;
   char reason[2048];
   bool is_alarm = false;

   DENTER(TOP_LAYER);

   if (load_list == nullptr) {
      DRETURN(is_alarm);
   }

   for_each_rw(load, load_list) {
      /* we used to have code here to check if load elements had changed and only then did the alarm calculation.
       * but load elements will constantly change, so no real optimization
       * removed it as part of CS-438
       */
      bool is_category_alarm = false;
      queue_ref_list = lGetPosList(load, LDR_queue_ref_list_pos);
      for_each_rw(queue_ref, queue_ref_list) {
         queue = (lListElem *)lGetRef(queue_ref, QRL_queue);
         if (is_category_alarm) {
            lSetUlong(queue, QU_tagged4schedule, 1);
         } else if (sge_load_alarm(reason, sizeof(reason), queue, lGetList(queue, QU_load_thresholds), host_list, centry_list, nullptr, true)) {

            DPRINTF("queue %s tagged to be overloaded: %s\n", lGetString(queue, QU_full_name), reason);
            schedd_mes_add_global(nullptr, monitor_next_run, SCHEDD_INFO_QUEUEOVERLOADED_SS,
                                  lGetString(queue, QU_full_name), reason);
            lSetUlong(queue, QU_tagged4schedule, 1);
            is_alarm = true;
            is_category_alarm = true;
         } else {
            break;
         }
      }
   }

   DRETURN(is_alarm);
}

/****** sge_select_queue/sge_remove_queue_from_load_list() *********************
*  NAME
*     sge_remove_queue_from_load_list() -- removes queues from the load list
*
*  SYNOPSIS
*     void sge_remove_queue_from_load_list(lList **load_list, const lList
*     *queue_list)
*
*  INPUTS
*     lList **load_list       - load list structure
*     const lList *queue_list - queues to be removed from it.
*
*  NOTES
*     MT-NOTE: sge_remove_queue_from_load_list() is MT safe
*
*  SEE ALSO
*     sge_create_load_list
*     load_locate_elem
*     sge_load_list_alarm
*     sge_remove_queue_from_load_list
*     sge_free_load_list
*
*******************************************************************************/
void sge_remove_queue_from_load_list(lList **load_list, const lList *queue_list){
   const lListElem* queue = nullptr;
   lListElem *load = nullptr;

   DENTER(TOP_LAYER);

   if (load_list == nullptr){
      CRITICAL("no load_list specified\n");
      abort();
   }

   if (*load_list == nullptr) {
      DRETURN_VOID;
   }

   for_each_ep(queue, queue_list) {
      bool is_found = false;
      lList *queue_ref_list = nullptr;
      lListElem *queue_ref = nullptr;

      for_each_rw(load, *load_list) {
         queue_ref_list = lGetPosList(load, LDR_queue_ref_list_pos);
         for_each_rw (queue_ref, queue_ref_list) {
            if (queue == lGetRef(queue_ref, QRL_queue)) {
               is_found = true;
               break;
            }
         }
         if (is_found) {
            lRemoveElem(queue_ref_list, &queue_ref);

            if (lGetNumberOfElem(queue_ref_list) == 0) {
               lRemoveElem(*load_list, &load);
            }
            break;
         }
      }

      if (lGetNumberOfElem(*load_list) == 0) {
         lFreeList(load_list);
         DRETURN_VOID;
      }
   }

   DRETURN_VOID;
}


/****** sge_select_queue/sge_free_load_list() **********************************
*  NAME
*     sge_free_load_list() -- frees the load list and sets it to nullptr
*
*  SYNOPSIS
*     void sge_free_load_list(lList **load_list)
*
*  INPUTS
*     lList **load_list - the load list
*
*  NOTES
*     MT-NOTE: sge_free_load_list() is MT safe
*
*  SEE ALSO
*     sge_create_load_list
*     load_locate_elem
*     sge_load_list_alarm
*     sge_remove_queue_from_load_list
*     sge_free_load_list
*
*******************************************************************************/
void sge_free_load_list(lList **load_list)
{
   DENTER(TOP_LAYER);

   lFreeList(load_list);

   DRETURN_VOID;
}

/****** sge_select_queue/match_static_advance_reservation() ********************
*  NAME
*     match_static_advance_reservation() -- Do matching that depends not on queue
*                                           or host
*
*  SYNOPSIS
*     static dispatch_t match_static_advance_reservation(const sge_assignment_t
*     *a)
*
*  FUNCTION
*     Checks whether a job that requests a advance reservation can be scheduled.
*     The job can be scheduled if the advance reservation is in state "running".
*
*  INPUTS
*     const sge_assignment_t *a - assignment to match
*
*  RESULT
*     static dispatch_t - DISPATCH_OK on success
*                         DISPATCH_NEVER_CAT on error
*
*  NOTES
*     MT-NOTE: match_static_advance_reservation() is MT safe
*******************************************************************************/
static dispatch_t match_static_advance_reservation(const sge_assignment_t *a)
{
   dispatch_t result = DISPATCH_OK;
   const lListElem *ar;
   u_long32 ar_id = lGetUlong(a->job, JB_ar);

   DENTER(TOP_LAYER);


   if (ar_id != 0) {
      if ((ar = lGetElemUlong(a->ar_list, AR_id, ar_id)) != nullptr) {
         const lList *acl_list;

         if (!(a->is_job_verify)) {
            /* is ar in error and error handling is not soft? */
            if (lGetUlong(ar, AR_state) == AR_ERROR && lGetUlong(ar, AR_error_handling) != 0) {
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_ARISINERROR_I, ar_id);
               DRETURN(DISPATCH_NEVER_CAT);
            }

            /* is ar running? */
            if (lGetUlong(ar, AR_state) != AR_RUNNING && lGetUlong(ar, AR_state) != AR_ERROR) {
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_EXECTIME_);
               DRETURN(DISPATCH_NEVER_CAT);
            }
         }

         /* has user access? */
         if ((acl_list = lGetList(ar, AR_xacl_list))) {
            const lListElem *acl_ep;
            for_each_ep(acl_ep, acl_list) {
               const char* user = lGetString(acl_ep, ARA_name);

               if (!is_hgroup_name(user)) {
                  if (strcmp(a->user, user) == 0) {
                     break;
                  }
               } else {
                  /* skip preattached \@ sign */
                  const char *acl_name = ++user;
                  const lListElem *userset_list = lGetElemStr(a->acl_list, US_name, acl_name);

                  if (sge_contained_in_access_list(a->user, a->group, a->grp_list, userset_list) == 1) {
                     break;
                  }
               }
            }
            if (acl_ep != nullptr){
               dstring buffer = DSTRING_INIT;
               sge_dstring_sprintf(&buffer, sge_U32CFormat, sge_u32c(ar_id));
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_HASNOPERMISSION_SS, SGE_OBJ_AR,
                              sge_dstring_get_string(&buffer));
               sge_dstring_free(&buffer);
               DRETURN(DISPATCH_NEVER_CAT);
            }
         }

         if ((acl_list = lGetList(ar, AR_acl_list))) {
            const lListElem *acl_ep;
            for_each_ep(acl_ep, acl_list) {
               const char *user = lGetString(acl_ep, ARA_name);

               if (!is_hgroup_name(user)) {
                  if (strcmp(a->user, user) == 0) {
                     break;
                  }
               } else {
                  /* skip preattached \@ sign */
                  const char *acl_name = ++user;
                  const lListElem *userset_list = lGetElemStr(a->acl_list, US_name, acl_name);

                  if (sge_contained_in_access_list(a->user, a->group, a->grp_list, userset_list) == 1) {
                     break;
                  }
               }
            }
            if (acl_ep == nullptr){
               dstring buffer = DSTRING_INIT;
               sge_dstring_sprintf(&buffer, sge_U32CFormat, sge_u32c(ar_id));
               schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                              SCHEDD_INFO_HASNOPERMISSION_SS, SGE_OBJ_AR,
                              sge_dstring_get_string(&buffer));
               sge_dstring_free(&buffer);
               DRETURN(DISPATCH_NEVER_CAT);
            }
         }
      } else {
         /* should never happen */
         DRETURN(DISPATCH_NEVER_CAT);
      }
   }

   DRETURN(result);
}
