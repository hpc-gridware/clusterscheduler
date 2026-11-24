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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The job object: array tasks, request sets and the category string
 *
 * Three things in here are easy to get wrong and worth knowing before editing:
 *
 * - **A pending array task usually has no element.** Tasks are stored as id
 *   ranges and only get a `JAT` element once they are about to run
 *   ("enrolled"). The `job_get_ja_task_template*` functions hand out a
 *   throwaway element describing an unenrolled task.
 * - **Requests are grouped by scope.** A sequential job only has the global
 *   set; a parallel job may state different requests for its master task and
 *   its slave tasks. See the `JRS_SCOPE_*` values.
 * - **State and status overlap.** `JAT_state` and `JAT_status` can disagree,
 *   and one bit value means different things depending on which field it
 *   appears in. #jatask_combine_state_and_status_for_output is what output
 *   should use, never either field alone.
 *
 * @see sge_job.h
 */

#include <cstring>
#include <unistd.h>
#include <sstream>
#include <limits>
#include <tuple>
#include <array>
#include <utility>

#include "uti/ocs_Pattern.h"
#include "uti/sge.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CX.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "sgeobj/tom/ocs_tom_JB.h"

#include "symbols.h"
#include "msg_common.h"
#include "uti/sge_hostname.h"


/**
 * @brief Create a ja task template
 *
 * The function returns a pointer to a template array task element.
 * This task represents a currently submitted pending task
 * (no hold state).
 * The task number (JAT_task_number) of this template task will be
 * initialized with the value given by 'ja_task_id'.
 *
 * The template element belongs to the job and is reinitialised by every
 * call; it is writable although the job is passed read only, as before.
 *
 * @param job JB
 * @param ja_task_id array task number
 *
 * @return template task (JAT), an empty handle if the job has no template
 *
 * @note MT-NOTE: job_get_ja_task_template_pending() is MT safe
 *
 * @see #job_get_ja_task_template_pending, #job_get_ja_task_template_hold
 */
ocs::tom::Element<ocs::tom::JAT> job_get_ja_task_template_pending(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                                  const uint32_t ja_task_id) {
   DENTER(BASIS_LAYER);

   // the template list of a read only job hands out a writable element
   const auto template_task = ocs::tom::Element<ocs::tom::JAT>::from_typed(
      const_cast<lListElem *>(job.get<JB_ja_template>().front().raw()));

   if (!template_task) {
      ERROR("unable to retrieve template task");
   } else {
      template_task.set<JAT_state>(JQUEUED | JWAITING);
      template_task.set<JAT_task_number>(ja_task_id);
   }
   DRETURN(template_task);
}


/**
 * @brief Create a ja task template
 *
 * The function returns a pointer to a template array task element.
 * This task represents a currently submitted pending task.
 * The task number (JAT_task_number) of this template task will be
 * initialized with the value given by 'ja_task_id'.
 * The hold state of the task (JAT_hold) will be initialized with
 * the 'hold_state' value.
 *
 * @param job JB
 * @param ja_task_id array task number
 * @param hold_state the hold state the template task is initialized with
 *
 * @return template task (JAT), an empty handle if the job has no template
 *
 * @see #job_get_ja_task_template_pending
 */
ocs::tom::Element<ocs::tom::JAT> job_get_ja_task_template_hold(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                               const uint32_t ja_task_id,
                                                               const uint32_t hold_state) {
   DENTER(BASIS_LAYER);

   const auto template_task = job_get_ja_task_template_pending(job, ja_task_id);
   if (template_task) {
      template_task.set<JAT_task_number>(ja_task_id);
      template_task.set<JAT_hold>(hold_state);
      template_task.set<JAT_status>(JIDLE);
      uint32_t state = JQUEUED | JWAITING;
      if (template_task.get<JAT_hold>()) {
         state |= JHELD;
      }
      template_task.set<JAT_state>(state);
   }
   DRETURN(template_task);
}

/**
 * @brief Hold state of unenrolled task
 *
 * Returns the hold state of a task which is not enrolled in
 * the JB_ja_tasks list of 'job'
 *
 * @param job JB
 * @param ja_task_id valid ja task id
 *
 * @return hold state
 */
static uint32_t job_accumulate_hold_flags(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t ja_task_id) {
   DENTER(TOP_LAYER);
   uint32_t ret = 0;

   if (range_list_is_id_within(job.get<JB_ja_u_h_ids>(), ja_task_id)) {
      ret |= MINUS_H_TGT_USER;
   }
   if (range_list_is_id_within(job.get<JB_ja_o_h_ids>(), ja_task_id)) {
      ret |= MINUS_H_TGT_OPERATOR;
   }
   if (range_list_is_id_within(job.get<JB_ja_s_h_ids>(), ja_task_id)) {
      ret |= MINUS_H_TGT_SYSTEM;
   }
   if (range_list_is_id_within(job.get<JB_ja_a_h_ids>(), ja_task_id)) {
      ret |= MINUS_H_TGT_JA_AD;
   }
   DRETURN(ret);
}

/**
 * @brief Intersection of two id range lists, owned
 *
 * @param list1 the first list (RN), may be an empty handle
 * @param list2 the second list (RN), may be an empty handle
 * @return the intersection, an empty handle if it is empty
 */
static ocs::tom::ListPtr<ocs::tom::RN> ocs_range_list_intersection(const ocs::tom::ConstList<ocs::tom::RN> list1,
                                                                   const ocs::tom::ConstList<ocs::tom::RN> list2) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::RN> result;
   range_list_calculate_intersection_set(result, nullptr, list1, list2);
   DRETURN(result);
}

/**
 * @brief Difference of two id range lists, owned
 *
 * @param list1 the list to subtract from (RN), may be an empty handle
 * @param list2 the ids to subtract (RN), may be an empty handle
 * @return list1 - list2, an empty handle if it is empty
 */
static ocs::tom::ListPtr<ocs::tom::RN> ocs_range_list_difference(const ocs::tom::ConstList<ocs::tom::RN> list1,
                                                                 const ocs::tom::ConstList<ocs::tom::RN> list2) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::RN> result;
   range_list_calculate_difference_set(result, nullptr, list1, list2);
   DRETURN(result);
}

/**
 * @brief Lists for hold combinations
 *
 * This function creates sixteen id lists, one per hold state combination.
 * Tasks whose id is contained in an id list have the hold state combination
 * stored next to it.
 *
 * The lists used to be out-parameters, to be freed with
 * job_destroy_hold_id_lists(); they are returned now and free themselves.
 *
 * @param job JB
 *
 * @return per hold combination the hold state (see MINUS_H_TGT_*) and the id list (RN, an empty
 *         handle if no task has that combination); index 0 is the combination without any hold
 *
 * @see #job_get_ja_task_template_hold
 */
std::array<std::pair<uint32_t, ocs::tom::ListPtr<ocs::tom::RN>>, 16>
job_create_hold_id_lists(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   constexpr uint32_t U = MINUS_H_TGT_USER;
   constexpr uint32_t O = MINUS_H_TGT_OPERATOR;
   constexpr uint32_t S = MINUS_H_TGT_SYSTEM;
   constexpr uint32_t A = MINUS_H_TGT_JA_AD;
   std::array<std::pair<uint32_t, ocs::tom::ListPtr<ocs::tom::RN>>, 16> ret;
   constexpr uint32_t hold_state[16] = {
      0, U, O, S, A, U | O, U | S, U | A, O | S, O | A, S | A, U | O | S, U | O | A, U | S | A, O | S | A, U | O | S | A
   };
   for (int i = 0; i < 16; i++) {
      ret[i].first = hold_state[i];
   }

   const auto u_ids = job.get<JB_ja_u_h_ids>();
   const auto o_ids = job.get<JB_ja_o_h_ids>();
   const auto s_ids = job.get<JB_ja_s_h_ids>();
   const auto a_ids = job.get<JB_ja_a_h_ids>();
   const auto intersection = ocs_range_list_intersection;
   const auto difference = ocs_range_list_difference;

   /* uo, us, ua, os, oa, sa, uos, uoa, usa, osa */
   const auto uo = intersection(u_ids, o_ids);
   const auto us = intersection(u_ids, s_ids);
   const auto ua = intersection(u_ids, a_ids);
   const auto os = intersection(o_ids, s_ids);
   const auto oa = intersection(o_ids, a_ids);
   const auto sa = intersection(s_ids, a_ids);
   const auto uos = intersection(*uo, *os);
   const auto uoa = intersection(*uo, *oa);
   const auto usa = intersection(*us, *sa);
   const auto osa = intersection(*os, *sa);

   /* uosa -> 15 */
   ret[15].second = intersection(*uos, *uoa);

   /* osaU -> 14 */
   ret[14].second = difference(*osa, *ret[15].second);

   /* usaO -> 13 */
   ret[13].second = difference(*usa, *ret[15].second);

   /* uoaS -> 12 */
   ret[12].second = difference(*uoa, *ret[15].second);

   /* uosA -> 11 */
   ret[11].second = difference(*uos, *ret[15].second);

   /* saUO -> 10 */
   ret[10].second = difference(*difference(*sa, *usa), *ret[14].second);

   /* oaUS -> 9 */
   const auto oa_u = difference(*oa, *uoa);
   ret[9].second = difference(*oa_u, *ret[14].second);

   /* osUA -> 8 */
   const auto os_u = difference(*os, *uos);
   ret[8].second = difference(*os_u, *ret[14].second);

   /* uaOS -> 7 */
   ret[7].second = difference(*difference(*ua, *uoa), *ret[13].second);

   /* usOA -> 6 */
   const auto us_o = difference(*us, *uos);
   ret[6].second = difference(*us_o, *ret[13].second);

   /* uoSA -> 5 */
   ret[5].second = difference(*difference(*uo, *uos), *ret[12].second);

   /* aUOS -> 4 */
   ret[4].second = difference(*difference(*difference(a_ids, *ua), *oa_u), *ret[10].second);

   /* sUOA -> 3 */
   ret[3].second = difference(*difference(*difference(s_ids, *us), *os_u), *ret[10].second);

   /* oUSA -> 2 */
   ret[2].second = difference(*difference(*difference(o_ids, *uo), *os_u), *ret[9].second);

   /* uOSA -> 1 */
   ret[1].second = difference(*difference(*difference(u_ids, *uo), *us_o), *ret[7].second);

   /* UOSA -> 0 */
   ret[0].second = job.get<JB_ja_n_h_ids>().clone("task_id_range");

   DRETURN(ret);
}



/**
 * @brief Is a certain array task enrolled
 *
 * This function will return true (1) if the array task with
 * 'task_number' is not enrolled in the JB_ja_tasks sublist
 * of 'job'.
 *
 * @param job JB
 * @param task_number task_number
 *
 * @return true or false
 */
bool job_is_enrolled(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t task_number) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (range_list_is_id_within(job.get<JB_ja_n_h_ids>(), task_number) ||
       range_list_is_id_within(job.get<JB_ja_u_h_ids>(), task_number) ||
       range_list_is_id_within(job.get<JB_ja_o_h_ids>(), task_number) ||
       range_list_is_id_within(job.get<JB_ja_s_h_ids>(), task_number) ||
       range_list_is_id_within(job.get<JB_ja_a_h_ids>(), task_number)) {
      ret = false;
   }
   DRETURN(ret);
}


/**
 * @brief Was this task submitted
 *
 * This function will return true (1) if the task with
 * 'ja_task_number' is defined within the array 'job'. The task
 * is defined when 'ja_task_number' is enclosed in the task id
 * range which was specified during submit time (qsub -t).
 *
 * @param job JB
 * @param ja_task_number task number
 *
 * @return true or false
 *
 * @note MT-NOTE: job_is_ja_task_defined() is MT safe
 */
bool job_is_ja_task_defined(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t ja_task_number) {
   return range_list_is_id_within(job.get<JB_ja_structure>(), ja_task_number);
}


/**
 * @brief Returns number of array tasks
 *
 * This function returns the overall amount of tasks in an array job.
 *
 * @param job JB
 *
 * @return number of tasks
 *
 * @see #job_get_ja_tasks, #job_get_enrolled_ja_tasks, #job_get_not_enrolled_ja_tasks, #job_get_submit_ja_tasks
 */
uint32_t job_get_ja_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   uint32_t ret = 0;

   uint32_t n = job_get_not_enrolled_ja_tasks(job);
   ret += n;
   DPRINTF("Not enrolled ja_tasks: " sge_u32 "\n", n);
   n = job_get_enrolled_ja_tasks(job);
   ret += n;
   DPRINTF("Enrolled ja_tasks: " sge_u32 "\n", n);
   DRETURN(ret);
}


/**
 * @brief Num. of unenrolled tasks
 *
 * This function returns the amount of tasks not enrolled in the
 * JB_ja_tasks sublist.
 *
 * @param job JB
 *
 * @return number of tasks
 *
 * @see #job_get_ja_tasks, #job_get_enrolled_ja_tasks, #job_get_not_enrolled_ja_tasks, #job_get_submit_ja_tasks
 */
uint32_t job_get_not_enrolled_ja_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   ocs::tom::ListPtr<ocs::tom::RN> uo_ids;
   ocs::tom::ListPtr<ocs::tom::RN> uos_ids;
   ocs::tom::ListPtr<ocs::tom::RN> uosa_ids;
   uint32_t ret = 0;

   range_list_calculate_union_set(uo_ids, &answer_list,
                                  job.get<JB_ja_u_h_ids>(),
                                  job.get<JB_ja_o_h_ids>());
   range_list_calculate_union_set(uos_ids, &answer_list, uo_ids, job.get<JB_ja_s_h_ids>());
   range_list_calculate_union_set(uosa_ids, &answer_list, uos_ids, job.get<JB_ja_a_h_ids>());

   ret += range_list_get_number_of_ids(job.get<JB_ja_n_h_ids>());
   ret += range_list_get_number_of_ids(uosa_ids.get());

   // the answer list was never freed before
   answer_list.reset();
   DRETURN(ret);
}


/**
 * @brief Num. of enrolled array tasks
 *
 * This function returns the amount of tasks enrolled in the
 * JB_ja_tasks sublist.
 *
 * @param job JB
 *
 * @return number of tasks
 *
 * @see #job_get_ja_tasks, #job_get_enrolled_ja_tasks, #job_get_not_enrolled_ja_tasks, #job_get_submit_ja_tasks
 */
uint32_t job_get_enrolled_ja_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return static_cast<uint32_t>(job.get<JB_ja_tasks>().size());
}


/**
 * @brief Array size during job submittion
 *
 * The function returns the ammount of tasks the job had during
 * it's submittion
 *
 * @param job JB
 *
 * @return number of tasks
 *
 * @see #job_get_ja_tasks, #job_get_enrolled_ja_tasks, #job_get_not_enrolled_ja_tasks, #job_get_submit_ja_tasks
 */
uint32_t job_get_submit_ja_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   const auto [start, end, step] = job_get_submit_task_ids(job);
   return ((end - start) / step + 1);
}

/**
 * @brief Enrolls a task into the JB_ja_tasks list of a job
 *
 * @param job the job (JB)
 * @param answer_list the answer list
 * @param ja_task_number the task number
 *
 * @return the enrolled task (JAT)
 */
ocs::tom::Element<ocs::tom::JAT> job_enroll(const ocs::tom::Element<ocs::tom::JB> job,
                                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                            const uint32_t ja_task_number) {
   DENTER(TOP_LAYER);

   object_delete_range_id<JB_ja_n_h_ids>(job, answer_list, ja_task_number);
   auto ja_task = job.find_first_in_rw<JB_ja_tasks, JAT_task_number>(ja_task_number);
   if (!ja_task) {
      auto ja_task_list = job.get<JB_ja_tasks>();
      const auto template_task = job_get_ja_task_template_pending(job, ja_task_number);
      if (!template_task) {
         // nothing to enroll from: the old code ended up with nullptr here as well
         DRETURN(ocs::tom::Element<ocs::tom::JAT>());
      }

      if (!ja_task_list) {
         // the list gets the layout of the (possibly reduced) template task
         auto created = ocs::tom::ListPtr<ocs::tom::JAT>::create_like("ulong_sublist", template_task);
         ja_task_list = created.get();
         job.set<JB_ja_tasks>(std::move(created));
      }
      ja_task = ja_task_list.push_back(template_task.clone());
   }

   DRETURN(ja_task);
}

/**
 * @brief Unenrolls a task from the JB_ja_tasks list of a job
 *
 * The task ID is added to the list of unenrolled tasks not in hold state (n_h)
 *
 * The task stays an untyped in-out pointer: it is freed and set to nullptr,
 * and the caller relies on that.
 *
 * @param job the job (JB), may be an empty handle
 * @param answer_list the answer list
 * @param[in,out] ja_task the task to unenroll (JAT); freed and set to nullptr
 */
void job_unenroll(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                  lListElem **ja_task) {
   DENTER(TOP_LAYER);
   if (!job || ja_task == nullptr || *ja_task == nullptr) {
      // nothing to do
      DRETURN_VOID;
   }

   const auto ja_task_list = job.get<JB_ja_tasks>();
   if (!ja_task_list) {
      // no enrolled task - should never happen
      DRETURN_VOID;
   }

   // add the task id to the list of unenrolled tasks; the list is taken out of the job and put back
   const uint32_t ja_task_id = ocs::tom::ConstElement<ocs::tom::JAT>(*ja_task).get<JAT_task_number>();
   auto ids = job.exchange<JB_ja_n_h_ids>(nullptr);
   range_list_insert_id(ids, answer_list, ja_task_id);
   range_list_compress(ids);
   static_cast<void>(job.exchange<JB_ja_n_h_ids>(std::move(ids)));

   // remove and free the enrolled task, then clear the caller's pointer, as lRemoveElem() did
   ja_task_list.erase(ocs::tom::Element<ocs::tom::JAT>::from_typed(*ja_task));
   *ja_task = nullptr;

   // trash the list if it was the last task
   if (ja_task_list.empty()) {
      job.set<JB_ja_tasks>(nullptr);
   }
   DRETURN_VOID;
}

/**
 * @brief Count rescheduled tasks
 *
 * Returns number of rescheduled tasks in JB_ja_tasks of a job. The
 * 'count_all' flag can be used to cause a quick exit if merely the
 * existence of rescheduled tasks is of interest.
 *
 * @param job the job (JB)
 * @param count_all quick exit flag
 *
 * @return number of tasks resp. 0/1
 *
 * @note MT-NOTE: job_count_rescheduled_ja_tasks() is MT safe
 */
static int job_count_rescheduled_ja_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job, const bool count_all) {
   int n = 0;

   for (const auto ja_task : job.get<JB_ja_tasks>()) {
      const uint32_t state = ja_task.get<JAT_state>();
      if ((ja_task.get<JAT_status>() == JIDLE) && ((state & JQUEUED) != 0) && ((state & JWAITING) != 0)) {
         n++;
         if (!count_all)
            break;
      }
   }
   return n;
}

/**
 * @brief Count number of pending tasks
 *
 * This function returns the number of pending tasks of a job.
 *
 * @param job JB
 * @param count_all false to stop at the first pending task, so the result is
 *                  only 0 or 1 and the whole range does not have to be walked
 *
 * @return number of tasks or simply 0/1 if count_all is 'false'
 */
uint32_t job_count_pending_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job, const bool count_all) {
   DENTER(TOP_LAYER);

   if (count_all) {
      uint32_t n = range_list_get_number_of_ids(job.get<JB_ja_n_h_ids>());
      n += job_count_rescheduled_ja_tasks(job, true);
      DRETURN(n);
   }

   if (job.get<JB_ja_n_h_ids>() || job_count_rescheduled_ja_tasks(job, false)) {
      DRETURN(1);
   }

   DRETURN(0);
}


/**
 * @brief Remove unenrolled task
 *
 * Removes an unenrolled pending task from the id lists.
 *
 * @param job JB
 * @param answer_list AN
 * @param ja_task_number Task to be removed
 */
void job_delete_not_enrolled_ja_task(const ocs::tom::Element<ocs::tom::JB> job,
                                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const uint32_t ja_task_number) {
   DENTER(TOP_LAYER);

   object_delete_range_id<JB_ja_n_h_ids>(job, answer_list, ja_task_number);
   object_delete_range_id<JB_ja_u_h_ids>(job, answer_list, ja_task_number);
   object_delete_range_id<JB_ja_o_h_ids>(job, answer_list, ja_task_number);
   object_delete_range_id<JB_ja_s_h_ids>(job, answer_list, ja_task_number);
   object_delete_range_id<JB_ja_a_h_ids>(job, answer_list, ja_task_number);
   DRETURN_VOID;
}


/**
 * @brief Has the job soft requests?
 *
 * True (1) will be returned if the job has soft requests.
 *
 * @param job JB
 *
 * @return true or false
 *
 * @note MT-NOTES: job_has_soft_requests() is MT safe
 */
bool job_has_soft_requests(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   bool ret = false;
   for (const auto jrs : job.get<JB_request_set_list>()) {
      if (jrs.get<JRS_soft_resource_list>() || jrs.get<JRS_soft_queue_list>()) {
         ret = true;
         break;
      }
   }

   return ret;
}

/**
 * @brief Adds an id to or removes it from one hold id list of a job
 *
 * The list is taken out of the job, changed, compressed and put back.
 *
 * @tparam F the hold id list field (JB_ja_n_h_ids, JB_ja_u_h_ids, ...)
 * @param job JB
 * @param answer_list AN
 * @param ja_task_id the array task id
 * @param insert true to insert the id, false to remove it
 */
template <auto F>
static void ocs_job_change_hold_ids(const ocs::tom::Element<ocs::tom::JB> job,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    const uint32_t ja_task_id, const bool insert) {
   DENTER(TOP_LAYER);
   auto range_list = job.exchange<F>(nullptr);
   if (insert) {
      range_list_insert_id(range_list, answer_list, ja_task_id);
   } else {
      range_list_remove_id(range_list, answer_list, ja_task_id);
   }
   range_list_compress(range_list);
   static_cast<void>(job.exchange<F>(std::move(range_list)));
   DRETURN_VOID;
}

/**
 * @brief Changes the hold state of a task
 *
 * This function changes the hold state of a job/array-task.
 *
 * For an unenrolled task the id is removed from the "no hold" list and
 * inserted into the list of each hold type set in new_hold_state, and the
 * other way round for a cleared state.
 *
 * @param job JB
 * @param answer_list AN
 * @param ja_task_id Array task id
 * @param new_hold_state hold state (see MINUS_H_TGT_*)
 */
void job_set_hold_state(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const uint32_t ja_task_id,
                        const uint32_t new_hold_state) {
   DENTER(TOP_LAYER);
   if (!job_is_enrolled(job, ja_task_id)) {
      ocs_job_change_hold_ids<JB_ja_n_h_ids>(job, answer_list, ja_task_id,
                                             (new_hold_state & MINUS_H_TGT_ALL) == 0);
      ocs_job_change_hold_ids<JB_ja_u_h_ids>(job, answer_list, ja_task_id,
                                             (new_hold_state & MINUS_H_TGT_USER) != 0);
      ocs_job_change_hold_ids<JB_ja_o_h_ids>(job, answer_list, ja_task_id,
                                             (new_hold_state & MINUS_H_TGT_OPERATOR) != 0);
      ocs_job_change_hold_ids<JB_ja_s_h_ids>(job, answer_list, ja_task_id,
                                             (new_hold_state & MINUS_H_TGT_SYSTEM) != 0);
      ocs_job_change_hold_ids<JB_ja_a_h_ids>(job, answer_list, ja_task_id,
                                             (new_hold_state & MINUS_H_TGT_JA_AD) != 0);
   } else {
      const auto ja_task = job_search_task(job, nullptr, ja_task_id);

      if (ja_task) {
         ja_task.set<JAT_hold>(new_hold_state);
         if (new_hold_state) {
            ja_task.set<JAT_state>(ja_task.get<JAT_state>() | JHELD);
         } else {
            ja_task.set<JAT_state>(ja_task.get<JAT_state>() & ~JHELD);
         }
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Returns the hold state of a task
 *
 * Returns the hold state of a job/array-task
 *
 * @param job JB
 * @param ja_task_id array task id
 *
 * @return hold state (see MINUS_H_TGT_*)
 */
uint32_t job_get_hold_state(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t ja_task_id) {
   DENTER(TOP_LAYER);
   uint32_t ret = 0;

   if (job_is_enrolled(job, ja_task_id)) {
      const auto ja_task = job_search_task(job, nullptr, ja_task_id);

      if (ja_task) {
         ret = ja_task.get<JAT_hold>() & MINUS_H_TGT_ALL;
      } else {
         ret = 0;
      }
   } else {
      ret = job_accumulate_hold_flags(job, ja_task_id);
   }
   DRETURN(ret);
}


/**
 * @brief Search an array task
 *
 * This function return the array task with the id 'ja_task_id' if
 * it exists in the JB_ja_tasks-sublist of 'job'. If the task
 * is not found in the sublist, nullptr is returned.
 *
 * @param job JB, may be an empty handle
 * @param answer_list AN
 * @param ja_task_id array task id
 *
 * @return JAT element, writable although the job is passed read only, as before; an empty handle if
 *         there is none
 *
 * @note In case of errors, the function should return a message in a
 *       given answer_list (answer_list != nullptr).
 *       MT-NOTE: job_search_task() is MT safe
 */
ocs::tom::Element<ocs::tom::JAT> job_search_task(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                 [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                 const uint32_t ja_task_id) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JAT> ja_task = nullptr;

   if (job) {
      ja_task = job.find_first_in_rw<JB_ja_tasks, JAT_task_number>(ja_task_id);
   }
   DRETURN(ja_task);
}


/**
 * @brief Create an array task
 *
 * This function return the array task with the id 'ja_task_id' if
 * it exists in the JB_ja_tasks-sublist of 'job'.
 * A new element will be created in the sublist
 * of 'job' and a pointer to the new element will be returned.
 * Errors may be found in the 'answer_list'
 *
 * @param job JB, may be an empty handle
 * @param answer_list AN
 * @param ja_task_id array task id
 *
 * @return JAT element, an empty handle if the task is not defined
 *
 * @note In case of errors, the function should return a message in a
 *       given answer_list (answer_list != nullptr).
 */
ocs::tom::Element<ocs::tom::JAT> job_create_task(const ocs::tom::Element<ocs::tom::JB> job,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                 const uint32_t ja_task_id) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JAT> ja_task = nullptr;

   if (job && job_is_ja_task_defined(job, ja_task_id)) {
      ja_task = job_enroll(job, answer_list, ja_task_id);
   }

   DRETURN(ja_task);
}

/**
 * @brief Get shell start mode for 'job'
 *
 * Returns a string identifying the shell start mode for 'job'.
 *
 * @param queue QU element
 * @param conf_shell_start_mode shell start mode of configuration
 *
 * @return shell start mode
 */
const char *job_get_shell_start_mode(const ocs::tom::ConstElement<ocs::tom::QU> queue,
                                     const char *conf_shell_start_mode) {
   DENTER(TOP_LAYER);
   if (const char *queue_start_mode = queue.get<QU_shell_start_mode>();
       queue_start_mode && strcasecmp(queue_start_mode, "none") != 0) {
      DRETURN(queue_start_mode);
   }
   DRETURN(conf_shell_start_mode);
}

/**
 * @brief Creates a joblist and adds an job into it
 *
 * A 'job_list' will be created by this function if it does not
 * already exist and 'job' will be inserted into this 'job_list'.
 * 'name' will be the name of the new list.
 * If 'check' is true (1) than the function will test whether
 * there is already an element in 'job_list' which has the same
 * 'JB_job_number' like 'job'. If this is true than -1 will be
 * returned by this function.
 *
 * The list stays an untyped in-out reference, the callers pass master list
 * slots. The job is taken over by the list unless an error is returned.
 *
 * @param job_list JB, created if *job_list is nullptr
 * @param name name of the list
 * @param job JB element, may be an empty handle; on success the list owns it, and it is freed when
 *        adding it throws, so the caller hands it over in every case
 * @param check Does the element already exist?
 *
 * @return error code 1 => invalid parameter -1 => check failed: element already exists 0 => OK
 */
int job_list_add_job(lList **job_list, const char *name, const ocs::tom::Element<ocs::tom::JB> job,
                     const int check) {
   DENTER(TOP_LAYER);

   if (!job_list) {
      ERROR(SFNMAX, MSG_JOB_JLPPNULL);
      DRETURN(1);
   }
   if (!job) {
      ERROR(SFNMAX, MSG_JOB_JEPNULL);
      DRETURN(1);
   }

   if (!*job_list) {
      *job_list = ocs::tom::ListPtr<ocs::tom::JB>::create(name).release();
   }

   // the slot holds a job list; the master list and the spooling lists are not checked again
   const auto list = ocs::tom::List<ocs::tom::JB>::from_typed(*job_list);
   if (check && list && list.find_first<JB_job_number>(job.get<JB_job_number>())) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_JOBALREADYEXISTS_S,
            job_get_id_string(job.get<JB_job_number>(), 0, nullptr, &id_dstring));
      sge_dstring_free(&id_dstring);
      DRETURN(-1);
   }

   // the list takes the job over; every caller hands its job to this function, so when push_back throws the
   // temporary owning handle frees the job instead of leaking it
   list.push_back(ocs::tom::ElementPtr<ocs::tom::JB>::from_typed(job.raw()));

   DRETURN(0);
}


/**
 * @brief Is "job" an array job or not?
 *
 * The function returns true (1) if "job" is an array job with more
 * than one task.
 *
 * @param job JB element
 *
 * @return true or false
 *
 * @see #job_is_tight_parallel
 */
bool job_is_array(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   const uint32_t job_type = job.get<JB_type>();

   return JOB_TYPE_IS_ARRAY(job_type) ? true : false;
}


/**
 * @brief Is "job" a tightly integrated par. job?
 *
 * This function returns true if "job" is really a tightly
 * integrated parallel job.
 *
 * @param job JB element
 * @param pe_list PE list with all existing PEs
 *
 * @return true or false
 *
 * @see #job_is_array, #job_might_be_tight_parallel
 */
bool job_is_tight_parallel(const ocs::tom::ConstElement<ocs::tom::JB> job,
                           const ocs::tom::ConstList<ocs::tom::PE> pe_list) {
   DENTER(TOP_LAYER);

   bool ret = false;

   const char *pe_name = job.get<JB_pe>();
   if (pe_name != nullptr) {
      int found_pe = 0;
      int all_are_tight = 1;

      for (const auto pe : pe_list) {
         if (pe_is_matching(pe, pe_name)) {
            found_pe = 1;
            all_are_tight &= pe.get<PE_control_slaves>();
         }
      }

      if (found_pe && all_are_tight) {
         ret = true;
      }
   }
   DRETURN(ret);
}


/**
 * @brief Tightly integrated job?
 *
 * This functions returns true  if "job" might be a tightly
 * integrated job. True will be returned if (at least one) pe
 * matching the requested (wildcard) pe of a job has
 * "contol_slaves=true" in its configuration.
 *
 * @param job JB element
 * @param pe_list PE list with all existing PEs
 *
 * @return true or false
 *
 * @see #job_is_array, #job_is_tight_parallel, #job_might_be_tight_parallel
 */
bool job_might_be_tight_parallel(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                 const ocs::tom::ConstList<ocs::tom::PE> pe_list) {
   DENTER(TOP_LAYER);

   bool ret = false;

   const char *pe_name = job.get<JB_pe>();
   if (pe_name != nullptr) {
      for (const auto pe : pe_list) {
         if (pe_is_matching(pe, pe_name) && pe.get<PE_control_slaves>()) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}


/**
 * @brief Submit time task specification
 *
 * The function returns the "start", "end" and "step" numbers
 * which where used to create "job" (qsub -t `range`).
 *
 * The three numbers used to be out-parameters; they are returned now.
 *
 * @param job JB element
 *
 * @return first id, last id and step size (>=1); 1, 1, 1 if the job has no task range
 */
std::tuple<uint32_t, uint32_t, uint32_t> job_get_submit_task_ids(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   const auto range_elem = job.get<JB_ja_structure>().front();
   if (range_elem) {
      const uint32_t tmp_step = range_elem.get<RN_step>();

      return {range_elem.get<RN_min>(), range_elem.get<RN_max>(), tmp_step ? tmp_step : 1};
   }
   return {1, 1, 1};
}


/**
 * @brief Store the initial range ids in "job"
 *
 * The function stores the initial range id values ("start", "end"
 * and "step") in "job". It should only be used in functions
 * initializing new jobs.
 *
 * @param job JB job
 * @param start first id
 * @param end last id
 * @param step step size
 *
 * @return 0 -> OK 1 -> no memory
 *
 * @note MT-NOTE: job_set_submit_task_ids() is MT safe
 */
int job_set_submit_task_ids(const ocs::tom::Element<ocs::tom::JB> job, const uint32_t start, const uint32_t end,
                            const uint32_t step) {
   return object_set_range_id<JB_ja_structure>(job, start, end, step);
}


/**
 * @brief Get the smallest or the biggest unenrolled id
 *
 * Returns the smallest or the biggest task id currently existing in a job
 * which is not enrolled in the JB_ja_tasks sublist of 'job'.
 * If all tasks are enrolled 0 will be returned.
 *
 * @param job JB element
 * @param find_min true for the smallest id, false for the biggest
 *
 * @return task id or 0
 */
static uint32_t job_get_extreme_unenrolled_task_id(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                   const bool find_min) {
   DENTER(TOP_LAYER);
   const ocs::tom::ConstList<ocs::tom::RN> lists[5] = {
      job.get<JB_ja_n_h_ids>(), job.get<JB_ja_u_h_ids>(), job.get<JB_ja_o_h_ids>(),
      job.get<JB_ja_s_h_ids>(), job.get<JB_ja_a_h_ids>()};
   uint32_t ids[5];

   for (int i = 0; i < 5; i++) {
      ids[i] = find_min ? range_list_get_first_id(lists[i], nullptr) : range_list_get_last_id(lists[i], nullptr);
   }

   // all five lists the same way: the array dependency hold list (a_h) had its condition inverted,
   // so its first or last id replaced the result instead of being compared with it (CS-3123)
   uint32_t ret = ids[0];
   for (int i = 1; i < 5; i++) {
      if (ret > 0 && ids[i] > 0) {
         ret = find_min ? std::min(ret, ids[i]) : std::max(ret, ids[i]);
      } else if (ids[i] > 0) {
         ret = ids[i];
      }
   }
   DRETURN(ret);
}

/**
 * @brief The lowest task id of an array job that has no task element yet
 *
 * Pending tasks of an array job are stored as ranges rather than as elements;
 * they only get an element when they are about to run.
 *
 * @param job the array job to search (JB)
 * @return the id, or 0 when every task is enrolled
 */
uint32_t job_get_smallest_unenrolled_task_id(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_extreme_unenrolled_task_id(job, true);
}


/**
 * @brief Find smallest enrolled tid
 *
 * Returns the smallest task id currently existing in a job
 * which is enrolled in the JB_ja_tasks sublist of 'job'.
 * If no task is enrolled 0 will be returned.
 *
 * @param job JB element
 *
 * @return task id or 0
 */
uint32_t job_get_smallest_enrolled_task_id(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   uint32_t ret = 0;

   // initialize ret with the first task, then try to find a smaller task id
   bool first = true;
   for (const auto ja_task : job.get<JB_ja_tasks>()) {
      const uint32_t task_number = ja_task.get<JAT_task_number>();
      ret = first ? task_number : std::min(ret, task_number);
      first = false;
   }
   return ret;
}


/**
 * @brief Find biggest unenrolled id
 *
 * Returns the biggest task id currently existing in a job
 * which is not enrolled in the JB_ja_tasks sublist of 'job'.
 * If no task is enrolled 0 will be returned.
 *
 * @param job JB element
 *
 * @return task id or 0
 */
uint32_t job_get_biggest_unenrolled_task_id(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_extreme_unenrolled_task_id(job, false);
}


/**
 * @brief Find biggest enrolled tid
 *
 * Returns the biggest task id currently existing in a job
 * which is enrolled in the JB_ja_tasks sublist of 'job'.
 * If no task is enrolled 0 will be returned.
 *
 * @param job JB element
 *
 * @return task id or 0
 */
uint32_t job_get_biggest_enrolled_task_id(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   uint32_t ret = 0;

   // every task number is > 0, so the maximum does not depend on the direction of the walk
   for (const auto ja_task : job.get<JB_ja_tasks>()) {
      ret = std::max(ret, ja_task.get<JAT_task_number>());
   }
   return ret;
}


/**
 * @brief Try to register a new job
 *
 * This function checks whether a new job would exceed the maximum
 * of allowed jobs per cluster ("max_jobs"). If the limit would be
 * exceeded then the function will return 1 otherwise 0. In some
 * situations it may be necessary to force the registration
 * of a new job (reading jobs from spool area). This may be done
 * with "force_registration".
 *
 * @param job_list the jobs already in the system (JB)
 * @param max_jobs maximum number of allowed jobs per user
 * @param force_registration force job registration
 *
 * @return 1 => limit would be exceeded 0 => otherwise
 *
 * @see `suser_register_new_job()`
 */
int job_list_register_new_job(const ocs::tom::ConstList<ocs::tom::JB> job_list, const uint32_t max_jobs,
                              const int force_registration) {
   DENTER(TOP_LAYER);

   int ret = 1;

   if (max_jobs > 0 && !force_registration && max_jobs <= job_list.size()) {
      ret = 1;
   } else {
      ret = 0;
   }
   DRETURN(ret);
}


/**
 * @brief Initialize task id range lists
 *
 * Initialize the task id range lists within "job". All tasks within
 * the JB_ja_structure element of job will be added to the
 * JB_ja_n_h_ids list. All other id lists stored in the "job" will
 * be deleted.
 *
 * @param job JB element
 * @param answer_list AN list pointer
 *
 * @return return state -1 - error 0 - OK
 *
 * @note MT-NOTE: job_initialize_id_lists() is MT safe
 */
int job_initialize_id_lists(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   auto n_h_list = job.get<JB_ja_structure>().clone("task_id_range");
   if (n_h_list == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      DRETURN(-1);
   } else {
      job.set<JB_ja_n_h_ids>(std::move(n_h_list));
      job.set<JB_ja_u_h_ids>(nullptr);
      job.set<JB_ja_o_h_ids>(nullptr);
      job.set<JB_ja_s_h_ids>(nullptr);
      job.set<JB_ja_a_h_ids>(nullptr);
   }
   DRETURN(0);
}


/**
 * @brief Initialize environment (partially)
 *
 * Initialize the environment sublist (JB_env_list) of "job".
 * Path aliasing ("path_alias_list") has to be initialized before
 * this function might be called.
 * Following enironment variables will be added:
 *    `VAR_PREFIX`O_HOME
 *    `VAR_PREFIX`O_LOGNAME
 *    `VAR_PREFIX`O_PATH
 *    `VAR_PREFIX`O_SHELL
 *    `VAR_PREFIX`O_TZ
 *    `VAR_PREFIX`O_HOST
 *    `VAR_PREFIX`O_WORKDIR
 *    `VAR_PREFIX`O_MAIL
 * This function will be used in SGE/EE client applications.
 * Clients do not know which prefix should be used for job
 * environment variables ("SGE_" or other). Therefore
 * we use the define `VAR_PREFIX` which will be replaced shortly
 * before the job is started.
 *
 * @param job JB element
 * @param answer_list AN list pointer
 * @param path_alias_list PA list
 * @param unqualified_hostname the submit host without its domain
 * @param qualified_hostname the submit host including its domain
 */
void job_initialize_env(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const ocs::tom::ConstList<ocs::tom::PA> path_alias_list,
                        const char *unqualified_hostname,
                        const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   dstring buffer = DSTRING_INIT;

   // the environment is taken out of the job and put back at the end
   auto env_list = job.exchange<JB_env_list>(nullptr);

   // Copy specific environment variables into the job environment
   // as SGE_O_* variables.
   int i = -1;
   const char* env_name[] = {"HOME", "LOGNAME", "PATH",
                             "SHELL", "TZ", "MAIL", nullptr};

   while (env_name[++i] != nullptr) {
      const char *env_value = sge_getenv(env_name[i]);
      const char *env_var = sge_dstring_sprintf(&buffer, "%s%s%s", VAR_PREFIX, "O_", env_name[i]);
      var_list_set_string(env_list, env_var, env_value);
   }

   // Add TERM to the job environment.
   // If TERM is not set, it will be inherited from sge_execd's environment (unless execd_param INHERIT_ENV=FALSE).
   const uint32_t job_type = job.get<JB_type>();
   if (JOB_TYPE_IS_QLOGIN(job_type) || JOB_TYPE_IS_QRLOGIN(job_type) || JOB_TYPE_IS_QRSH(job_type)) {
      const char *term = sge_getenv("TERM");
      if (term != nullptr) {
         var_list_set_string(env_list, "TERM", term);
      }
   }


   // Add SGE_O_HOST either from environment HOST or set it to the unqualified host name.
   const char* host = sge_getenv("HOST");
   if (host == nullptr) {
      host = unqualified_hostname;
   }
   var_list_set_string(env_list, VAR_PREFIX "O_HOST", host);

   // Add SGE_O_WORKDIR from the current working directory.
   char tmp_str[SGE_PATH_MAX + 1];
   if (getcwd(tmp_str, sizeof(tmp_str)) == nullptr) {
      answer_list_add(answer_list, MSG_ANSWER_GETCWDFAILED, STATUS_EDISK, ANSWER_QUALITY_ERROR);
   } else {
      path_alias_list_get_path(path_alias_list, nullptr, tmp_str, qualified_hostname, &buffer);
      var_list_set_string(env_list, VAR_PREFIX "O_WORKDIR", sge_dstring_get_string(&buffer));
   }

   sge_dstring_free(&buffer);
   static_cast<void>(job.exchange<JB_env_list>(std::move(env_list)));

   DRETURN_VOID;
}


/**
 * @brief Get value of certain job env variable
 *
 * Return the string value of the job environment "variable".
 * Please note: The "*_O_*" env variables get their final
 *              name shortly before job execution. Find more
 *              information in the comment of
 *              job_initialize_env()
 *
 * @param job JB element
 * @param variable environment variable name
 *
 * @return value of "variable"
 *
 * @see #job_initialize_env
 */
const char *job_get_env_string(const ocs::tom::ConstElement<ocs::tom::JB> job, const char *variable) {
   DENTER(TOP_LAYER);

   const char *ret = var_list_get_string(job.get<JB_env_list>(), variable);
   DRETURN(ret);
}

/**
 * @brief Test JB_ja_* sublists
 *
 * Test following elements of "job" whether they are correct:
 *    JB_ja_structure, JB_ja_n_h_ids, JB_ja_u_h_ids,
 *    JB_ja_s_h_ids, JB_ja_o_h_ids, JB_ja_a_h_ids, JB_ja_z_ids
 * The function will try to correct errors within this lists. If
 * this is not possible an error will be returned in "answer_list".
 *
 * @param job JB element
 * @param answer_list AN list
 */
void job_check_correct_id_sublists(const ocs::tom::Element<ocs::tom::JB> job,
                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   /*
    * Is 0 contained in one of the range lists
    */
   {
      // the first list, JB_ja_structure, is not corrected
      const ocs::tom::List<ocs::tom::RN> range_lists[] = {
         job.get<JB_ja_structure>(),
         job.get<JB_ja_n_h_ids>(),
         job.get<JB_ja_u_h_ids>(),
         job.get<JB_ja_s_h_ids>(),
         job.get<JB_ja_o_h_ids>(),
         job.get<JB_ja_a_h_ids>()
      };
      bool is_structure = true;

      for (const auto range_list : range_lists) {
         for (const auto range : range_list) {
            if (!is_structure)
               range_correct_end(range);
            if (range_is_id_within(range, 0)) {
               ERROR(SFNMAX, MSG_JOB_NULLNOTALLOWEDT);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN_VOID;
            }
         }
         is_structure = false;
      }
   }

   /*
    * JB_ja_structure and one of the JB_ja_?_h_ids has
    * to comprise at least one id.
    */
   {
      const ocs::tom::ConstList<ocs::tom::RN> range_lists[] = {
         job.get<JB_ja_n_h_ids>(),
         job.get<JB_ja_u_h_ids>(),
         job.get<JB_ja_s_h_ids>(),
         job.get<JB_ja_o_h_ids>(),
         job.get<JB_ja_a_h_ids>()
      };
      int has_structure = 0;
      int has_x_ids = 0;

      for (const auto range_list : range_lists) {
         if (!range_list_is_empty(range_list)) {
            has_x_ids = 1;
         }
      }
      has_structure = !range_list_is_empty(job.get<JB_ja_structure>());
      if (!has_structure) {
         ERROR(SFNMAX, MSG_JOB_NOIDNOTALLOWED);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
      } else if (!has_x_ids) {
         job_initialize_id_lists(job, answer_list);
      }
   }

   DRETURN_VOID;
}


/**
 * @brief Get an id string for a job/jatask/petask
 *
 * Returns an id string for a certain job, ja task or pe task.
 * The function should be used in any code that outputs ids, e.g.
 * in error strings to ensure we have the same output format
 * everywhere. If the ja_task_id is 0, only the job id is output.
 *
 * @param job_id the job id
 * @param ja_task_id the ja task id or 0 to output only job_id
 * @param pe_task_id optionally the pe task id
 * @param buffer a buffer to be used for printing the id string
 *
 * @return pointer to a static buffer. It is valid until the next call of the function.
 *
 * @note MT-NOTE: job_get_id_string() is MT safe
 */
const char *job_get_id_string(uint32_t job_id, uint32_t ja_task_id,
                              const char *pe_task_id, dstring *buffer) {
   DENTER(TOP_LAYER);

   if (job_id == 0) {
      sge_dstring_sprintf(buffer, "");
   } else {
      if (ja_task_id == 0) {
         sge_dstring_sprintf(buffer, MSG_JOB_JOB_ID_U, job_id);
      } else {
         if (pe_task_id == nullptr) {
            sge_dstring_sprintf(buffer, MSG_JOB_JOB_JATASK_ID_UU, job_id, ja_task_id);
         } else {
            sge_dstring_sprintf(buffer, MSG_JOB_JOB_JATASK_PETASK_ID_UUS, job_id, ja_task_id, pe_task_id);
         }
      }
   }

   DRETURN(sge_dstring_get_string(buffer));
}

/**
 * @brief Does job reference the given PE?
 *
 * The function returns true if "job" references the "pe".
 * This is also the case if job requests a wildcard PE and
 * the wildcard name matches the given pe name.
 *
 * @param job JB element
 * @param pe PE object
 *
 * @return true or false
 */
bool job_is_pe_referenced(const ocs::tom::ConstElement<ocs::tom::JB> job,
                          const ocs::tom::ConstElement<ocs::tom::PE> pe) {
   const char *ref_pe_name = job.get<JB_pe>();
   bool ret = false;

   if(ref_pe_name != nullptr) {
      if (pe_is_matching(pe, ref_pe_name)) {
         ret = true;
      }
   }
   return ret;
}

/**
 * @brief Does job reference the given CKPT?
 *
 * The function returns true if "job" references the
 * checkpointing object "ckpt".
 *
 * @param job JB element
 * @param ckpt CK object
 *
 * @return true or false
 */
bool job_is_ckpt_referenced(const ocs::tom::ConstElement<ocs::tom::JB> job,
                            const ocs::tom::ConstElement<ocs::tom::CK> ckpt) {
   const char *ckpt_name = ckpt.get<CK_name>();
   const char *ref_ckpt_name = job.get<JB_checkpoint_name>();
   bool ret = false;

   if(ckpt_name != nullptr && ref_ckpt_name != nullptr) {
      if (!strcmp(ref_ckpt_name, ckpt_name)) {
         ret = true;
      }
   }
   return ret;
}

/**
 * @brief Write job state flags into a string
 *
 * This function writes the state flags given by 'op' into the
 * string 'str'
 *
 * @param str containes the state flags for 'qstat'/'qhost'
 * @param op job state bitmask
 */
/* JG: TODO: use dstring! */
void job_get_state_string(char *str, uint32_t op) {
   DENTER(TOP_LAYER);

   int count = 0;

   if (VALID(JDELETED, op)) {
      str[count++] = DISABLED_SYM;
   }

   if (VALID(JERROR, op)) {
      str[count++] = ERROR_SYM;
   }

   if (VALID(JSUSPENDED_ON_SUBORDINATE, op) ||
       VALID(JSUSPENDED_ON_SLOTWISE_SUBORDINATE, op)) {
      str[count++] = SUSPENDED_ON_SUBORDINATE_SYM;
   }

   if (VALID(JSUSPENDED_ON_THRESHOLD, op)) {
      str[count++] = SUSPENDED_ON_THRESHOLD_SYM;
   }

   if (VALID(JHELD, op)) {
      str[count++] = HELD_SYM;
   }

   if (VALID(JMIGRATING, op)) {
      str[count++] = RESTARTING_SYM;
   }

   if (VALID(JQUEUED, op)) {
      str[count++] = QUEUED_SYM;
   }

   if (VALID(JRUNNING, op)) {
      str[count++] = RUNNING_SYM;
   }

   if (VALID(JSUSPENDED, op)) {
      str[count++] = SUSPENDED_SYM;
   }

   if (VALID(JTRANSFERING, op)) {
      str[count++] = TRANSISTING_SYM;
   }

   if (VALID(JWAITING, op)) {
      str[count++] = WAITING_SYM;
   }

   if (VALID(JEXITING, op)) {
      str[count++] = EXITING_SYM;
   }

   /* CS-1908 finished-job retention: JFINISHED_DISPLAY is a synthetic bit
    * set by jatask_combine_state_and_status_for_output when JAT_status is
    * JFINISHED so retained rows render as 'f' rather than the transient 'x'
    * (JEXITING) the pre-retention world used. Bit is disjoint from every
    * real JAT_state bit, so this check does not fire on any legacy state. */
   if (VALID(JFINISHED_DISPLAY, op)) {
      str[count++] = FINISHED_SYM;
   }

   str[count++] = '\0';

   DRETURN_VOID;
}

/**
 * @brief Add parent jobid to job context
 *
 * If we have JOB_ID in environment implicitly put it into the
 * job context variable PARENT if was not explicitly set using
 * "-sc PARENT=$JOBID". By doing this we preserve information
 * about the relationship between these two jobs.
 *
 * The variable is added to the context change request JB_context_change as a
 * group of its own, so that a preceding -dc group cannot delete it (CS-3058).
 *
 * @param job JB element
 */
void job_add_parent_id_to_context(const ocs::tom::Element<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const char *job_id_string = sge_getenv("JOB_ID");
   if (job_id_string != nullptr && !job.find_first_in<JB_context_change, CX_variable>(CONTEXT_PARENT)) {
      // emplace_back_in() creates the list if the job has none yet
      job.emplace_back_in<JB_context_change, CX_variable>("+");
      const auto parent = job.emplace_back_in<JB_context_change, CX_variable>(CONTEXT_PARENT);
      parent.set<CX_value>(job_id_string);
   }
   DRETURN_VOID;
}


/**
 * @brief Check DISPLAY variable for qsh jobs
 *
 * Checks the DISPLAY variable for qsh jobs:
 * - existence
 * - empty string
 * - local variable
 * In each error cases, an appropriate error message is generated.
 * If output_warning is set to true, an error message is output.
 * In each case, an error message is written into answer_list.
 *
 * @param job the job to check
 * @param answer_list answer list to take error messages, if nullptr, no answer is passed back.
 * @param output_warning output error messages to stderr?
 *
 * @return STATUS_OK, if function call succeeds, else STATUS_EUNKNOWN.
 *
 * @note To fully hide the data representation of the DISPLAY settings,
 *       functions job_set_qsh_display and job_get_qsh_display would
 *       be usefull.
 */
int job_check_qsh_display(const ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          const bool output_warning) {
   DENTER(TOP_LAYER);

   dstring id_dstring = DSTRING_INIT;
   const char *id_str = job_get_id_string(job.get<JB_job_number>(), 0, nullptr, &id_dstring);
   int ret = STATUS_OK;

   /* check for existence of DISPLAY */
   if (const auto display_ep = job.get<JB_env_list>().find_first<VA_variable>("DISPLAY"); !display_ep) {
      if (output_warning) {
         WARNING(MSG_JOB_NODISPLAY_S, id_str);
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NODISPLAY_S, id_str);
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   } else {
      /* check value of display variable, if it is an empty string,
       * it is useless in a grid environment.
       */
      if (const char *display = display_ep.get<VA_value>(); display == nullptr || strlen(display) == 0) {
         if (output_warning) {
            WARNING(MSG_JOB_EMPTYDISPLAY_S, id_str);
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_EMPTYDISPLAY_S, id_str);
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      } else if (*display == ':') {
         /* check value of display variable, if it has the form :<id> (local display)
          * it is useless in a grid environment.
          */
         if (output_warning) {
            WARNING(MSG_JOB_LOCALDISPLAY_SS, display, id_str);
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_LOCALDISPLAY_SS, display, id_str);
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   sge_dstring_free(&id_dstring);
   DRETURN(ret);
}


/**
 * @brief Check the owner of a job
 *
 * Checks whether the user the request came from owns the job. An operator or
 * manager owns every job for this purpose.
 *
 * @param packet the request, which carries the requesting user
 * @param job_id the job number
 * @param master_job_list the master job list (JB)
 *
 * @return -1, if the job cannot be found 0, if the user is the job owner 1, if the user is not the job owner
 */
int job_check_owner(const ocs::gdi::Packet *packet, const uint32_t job_id,
                    const ocs::tom::ConstList<ocs::tom::JB> master_job_list) {
   DENTER(TOP_LAYER);

   if (manop_is_operator(packet)) {
      DRETURN(0);
   }

   const auto job = master_job_list.find_first<JB_job_number>(job_id);
   if (!job) {
      DRETURN(-1);
   }

   if (strcmp(packet->user, job.get<JB_owner>()) != 0) {
      DRETURN(1);
   }

   DRETURN(0);
}


/**
 * @brief Create a unique key
 *
 * Creates a unique key consisting of the job_id.
 * The job id can reread by calling job_parse_key().
 *
 * @param job_id job id
 * @param[out] buffer receives the key; nothing is written when it is nullptr
 *
 * @return pointer to a static buffer containing the key. The result is only valid until the next call of the function.
 *
 * @note MT-NOTE: job_get_job_key() is MT safe
 *
 * @see #job_get_key, #job_parse_key
 */
const char *job_get_job_key(uint32_t job_id, dstring *buffer) {
   DENTER(TOP_LAYER);

   const char *ret = nullptr;
   if (buffer != nullptr) {
      ret = sge_dstring_sprintf(buffer, "%d", job_id);
   }

   DRETURN(ret);
}

/**
 * @brief Create a unique key
 *
 * Creates a unique key consisting of job_id, ja_task_id and
 * pe_task_id. This key can again be split into its components
 * by a call to job_parse_key().
 *
 * @param job_id job id
 * @param ja_task_id ja task id
 * @param pe_task_id pe task id
 * @param buffer dstring buffer used to generate the key
 *
 * @return pointer to the key, stored within buffer
 *
 * @note MT-NOTE: job_get_key() is MT safe
 *
 * @see #job_parse_key
 */
const char *job_get_key(uint32_t job_id, uint32_t ja_task_id,
                        const char *pe_task_id, dstring *buffer) {
   DENTER(TOP_LAYER);

   const char *ret = nullptr;
   if (buffer != nullptr) {
      if (ja_task_id == 0) {
         ret = sge_dstring_sprintf(buffer, "%d", job_id);
      } else if (pe_task_id != nullptr) {
         ret = sge_dstring_sprintf(buffer, "%d.%d %s",
                                   job_id, ja_task_id, pe_task_id);
      } else {
         ret = sge_dstring_sprintf(buffer, "%d.%d",
                                   job_id, ja_task_id);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Parse a key generated by job_get_key()
 *
 * Parse a key generated by job_get_key() and split it into its
 * components.
 *
 * @param key key to be parsed
 * @param job_id pointer to job_id
 * @param ja_task_id pointer to ja_task_id
 * @param pe_task_id pointer to pe_task_id
 * @param only_job true, if only a job id is contained in key, else false.
 *
 * @return true, if the key could be parsed, else false
 *
 * @note MT-NOTE: job_get_key() is MT safe
 *
 *       The pe_task_id is only valid until the passed key is deleted!
 *
 * @see #job_get_key
 */
bool job_parse_key(char *key, uint32_t *job_id, uint32_t *ja_task_id,
                   char **pe_task_id, bool *only_job) {
   DENTER(TOP_LAYER);

   const char *ja_task_id_str;
   char *lasts = nullptr;

   *job_id = atoi(strtok_r(key, ".", &lasts));
   ja_task_id_str = strtok_r(nullptr, " ", &lasts);
   if (ja_task_id_str == nullptr) {
      *ja_task_id = 0;
      *pe_task_id = nullptr;
      *only_job  = true;
   } else {
      *ja_task_id = atoi(ja_task_id_str);
      *pe_task_id = strtok_r(nullptr, " ", &lasts);
      *only_job = false;
   }

   if(*pe_task_id != nullptr && strlen(*pe_task_id) == 0) {
      *pe_task_id = nullptr;
   }

   DRETURN(true);
}

/**
 * @brief Create a unique key
 *
 * Creates a unique key consisting of job_id and jobscript name
 * This key can again be split into its components
 * by a call to jobscript_parse_key()
 *
 * @param jep the job whose id and script name make up the key
 * @param[out] buffer receives the key
 *
 * @return pointer to a static buffer containing the key. The result is only valid until the next call of the function.
 *
 * @note MT-NOTE: jobscript_get_key() is MT safe
 *
 * @see #jobscript_parse_key
 */
const char *jobscript_get_key(const ocs::tom::ConstElement<ocs::tom::JB> jep, dstring *buffer) {
   DENTER(TOP_LAYER);

   const char *ret = nullptr;

   if (buffer != nullptr) {
      ret = sge_dstring_sprintf(buffer, "%s:" sge_u32 ".%s",
                                object_type_get_name(SGE_TYPE_JOBSCRIPT),
                                jep.get<JB_job_number>(), jep.get<JB_exec_file>());
   }
   DRETURN(ret);
}



/**
 * @brief Parse a key generated by job_get_key()
 *
 * Parse a key generated by jobscript_get_key()
 *
 * @param key key to be parsed
 * @param exec_file exec_file name to unlink
 *
 * @return const char * the database job key
 *
 * @note MT-NOTE: jobscript_parse_key() is MT safe
 *
 * @see #jobscript_get_key
 */
char *jobscript_parse_key(char *key, const char **exec_file) {
   DENTER(TOP_LAYER);

   char *lasts = nullptr;
   char *ret = nullptr;
   ret = strtok_r(key, ".", &lasts);
   *exec_file = strtok_r(nullptr, ".", &lasts);
   DRETURN(ret);
}

/**
 * @brief Check that a job's script file name is the one qmaster gives it
 *
 * qmaster stores the script of a job as `job_scripts/<job id>`, relative to the spool directory of qmaster and
 * of execd (`EXEC_DIR`). No client sets `JB_exec_file`, so any other name was not set by qmaster, and neither
 * qmaster nor execd may read, write or delete a file under it (CS-3134).
 *
 * @param exec_file the script file name (`JB_exec_file`)
 * @param job_id the job number
 * @return true if `exec_file` is exactly `job_scripts/<job_id>`, false otherwise, also for `nullptr` and job id 0
 *
 * @note MT-NOTE: ocs_job_exec_file_is_valid() is MT safe
 */
bool ocs_job_exec_file_is_valid(const char *exec_file, const uint32_t job_id) {
   if (exec_file == nullptr || job_id == 0) {
      return false;
   }
   char expected[sizeof(EXEC_DIR) + 16];
   snprintf(expected, sizeof(expected), "%s/" sge_u32, EXEC_DIR, job_id);
   return strcmp(exec_file, expected) == 0;
}

/**
 * @brief Check the script file name taken from a job script spooling key
 *
 * The key built by jobscript_get_key() and split by jobscript_parse_key() is `JOBSCRIPT:<job id>` plus the
 * script file name. The job id is taken from the key and the name checked with ocs_job_exec_file_is_valid().
 *
 * @param db_key the first part of the key, `JOBSCRIPT:<job id>`
 * @param exec_file the script file name from the key
 * @return true if the name is `job_scripts/<job id>` of the key's job
 *
 * @note MT-NOTE: ocs_jobscript_key_is_valid() is MT safe
 *
 * @see #jobscript_parse_key, #ocs_job_exec_file_is_valid
 */
bool ocs_jobscript_key_is_valid(const char *db_key, const char *exec_file) {
   if (db_key == nullptr) {
      return false;
   }
   const char *colon = strchr(db_key, ':');
   if (colon == nullptr || colon[1] < '1' || colon[1] > '9') {
      return false;
   }
   char *end = nullptr;
   const unsigned long job_id = strtoul(colon + 1, &end, 10);
   if (end == nullptr || *end != '\0' || job_id > UINT32_MAX) {
      return false;
   }
   return ocs_job_exec_file_is_valid(exec_file, static_cast<uint32_t>(job_id));
}

/**
 * @brief Resolves hostnames in path lists
 *
 * Resolves hostnames in path lists.
 *
 * @param path_list a path list of a job (JB_stderr_path_list, JB_stdout_path_list, ...), may be an empty handle;
 *        its host names are rewritten in place
 * @param answer_list AN element
 *
 * @return error code (STATUS_OK, or ...)
 */
int job_resolve_host_for_path_list(const ocs::tom::List<ocs::tom::PN> path_list,
                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   bool ret_error=false;

   for (const auto ep : path_list) {
      int res = ocs_resolve_host<PN_host>(ep);
      DPRINTF("after ocs_resolve_host() which returned %s\n", cl_get_error_text(res));
      if (res != CL_RETVAL_OK) {
         const char *hostname = ep.get<PN_host>();
         if (hostname != nullptr) {
            ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, hostname);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret_error=true;
         } else if (res != CL_RETVAL_PARAMS) {
            ERROR(SFNMAX, MSG_PARSE_NULLPOINTERRECEIVED);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret_error=true;
         }
      }
      DPRINTF("after ocs_resolve_host() - II\n");

      /* ensure, that each hostname is only specified once */
      if( !ret_error ){
         const char *hostname = ep.get<PN_host>();

         for (auto temp = ep.prev(); temp; temp = temp.prev()) {
            const char *temp_hostname = temp.get<PN_host>();

            if(hostname == nullptr){
               if(temp_hostname == nullptr){
                  ERROR(SFNMAX, MSG_PARSE_DUPLICATEHOSTINFILESPEC);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                  ret_error=true;
               }
            }
            else if( temp_hostname && strcmp(hostname, temp_hostname)==0){
               ERROR(SFNMAX, MSG_PARSE_DUPLICATEHOSTINFILESPEC);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret_error=true;
            }
            if(ret_error)
               break;
         }/* end for */
      }

      if(ret_error) {
         break;
      }
   }/*end for*/

   if(ret_error) {
      DRETURN(STATUS_EUNKNOWN);
   } else {
      DRETURN(STATUS_OK);
   }
}


/**
 * @brief Returns the requested centry name
 *
 * Returns the requested centry name if it is requested by the give
 * job (JB).
 *
 * @param job JB element
 * @param centry_name name
 *
 * @return CE element, an empty handle if neither list holds it
 *
 * @note MT-NOTE: job_get_request() is MT safe
 */
ocs::tom::ConstElement<ocs::tom::CE> job_get_request(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                     const char *centry_name) {
   DENTER(TOP_LAYER);

   auto ret = job_get_hard_resource_list(job).find_first<CE_name>(centry_name);
   if (!ret) {
      ret = job_get_soft_resource_list(job).find_first<CE_name>(centry_name);
   }
   DRETURN(ret);
}


/**
 * @brief A job's hard request for one resource in one scope
 *
 * This looks only in the scope named and does not fall back to the global
 * set.
 *
 * @param job the job to search (JB)
 * @param name the resource to look for
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the request (CE), an empty handle when that scope does not name the resource
 */
ocs::tom::ConstElement<ocs::tom::CE>
job_get_hard_request(const ocs::tom::ConstElement<ocs::tom::JB> job, const char *name, const uint32_t scope) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::CE> ret = nullptr;

   // a request can be either global
   const auto request_list = job_get_hard_resource_list(job, scope);
   if (request_list) {
      // @todo can we rely on CE_name or can it be the shortcut? Use centry_list_locate?
      ret = request_list.find_first<CE_name>(name);
   }

   DRETURN(ret);
}

/**
 * @brief How much of a consumable one scope of the job needs
 *
 * The amount stays an out-parameter: it is also written when false is
 * returned for a default request of 0.
 *
 * @param job the job to read (JB)
 * @param[out] answer_list receives the message when the value cannot be parsed
 * @param name the consumable to ask about
 * @param[out] value receives the amount
 * @param complex_definition the resource's complex entry (CE), for its default
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return true when a value was determined
 *
 * @note A default request of 0 is reported as failure, so the caller books
 *       nothing rather than booking zero.
 */
bool
job_get_contribution_by_scope(const ocs::tom::ConstElement<ocs::tom::JB> job,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name,
                              double *value, const ocs::tom::ConstElement<ocs::tom::CE> complex_definition,
                              const uint32_t scope) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *value_string = nullptr;
   char error_str[256];
   bool is_default_request = false;

   // we only consider *hard* requests (consumables), there are no soft consumables
   if (const auto centry = job_get_hard_request(job, name, scope); centry) {
      // CS-537: the amount was parsed when the request was built and is kept in CE_doubleval.
      // Parsing CE_stringval again would also be wrong for a resource map, whose string carries
      // a bracketed parameter list after the amount.
      *value = centry.get<CE_doubleval>();
   } else {
      // if the job did not request the consumable, there might still be a default request
      // @todo CE-459 if there was a CE_default_doubleval we wouldn't have to parse the string
      value_string = complex_definition.get<CE_defaultval>();
      is_default_request = true;
      const auto valtype = static_cast<ocs::CEntry::Type>(complex_definition.get<CE_valtype>());
      if (!(parse_ulong_val(value, nullptr, valtype, value_string, error_str, sizeof(error_str)-1))) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_ATTRIB_PARSATTRFAILED_SS, name, error_str);
         ret = false;
      }
   }
   if (is_default_request && *value == 0) {
      ret = false;
   }

   DRETURN(ret);
}

// adjust the slot count used for debiting of slave tasks
// called when we just debited the master task
// we need to reduce the slot count by one
// exception:
//    - the pe setting job_is_first_task = false (in this case there was no slot for the master task)
//    - unless the slot count is already +-1, then we had a single master task without slave task
//      @todo really? What if there is the master task and one slave task in the qinstance?
//        unless slots == +-1, then we are only booking the master task here
//        ==> reason for the alleged bug CS-547 we will always have a slave task with the master task
//            when job_is_first_task = false?
// for JOB and HOST variables debit_slots was already +-1, so we will not book them for slave again
/**
 * @brief Take the master task's slot out of the slave slot count
 *
 * Called right after the master task was debited. The master task normally
 * occupies one of the job's slots, so the count left for the slave tasks is
 * one lower - unless the parallel environment sets `job_is_first_task = false`,
 * in which case the master task got no slot of its own.
 *
 * @param pe the job's parallel environment (PE); an empty handle is treated as
 *           `job_is_first_task = true`
 * @param[in,out] slave_debit_slots the slot count to correct; its sign is kept,
 *                                  so it works for booking and unbooking alike
 */
void
adjust_slave_task_debit_slots(const ocs::tom::ConstElement<ocs::tom::PE> pe, int &slave_debit_slots) {
   bool job_is_first_task = true;
   if (pe) {
      job_is_first_task = pe.get<PE_job_is_first_task>();
   }

   if (job_is_first_task /* || abs(slave_debit_slots) == 1 */) {
      if (slave_debit_slots > 0) {
         slave_debit_slots--;
      } else {
         slave_debit_slots++;
      }
   }
}

/**
 * @brief Creates a string from the access lists and user
 *
 * @param category_str target string
 * @param owner job owner
 * @param group group owner
 * @param grp_list the owner's supplementary groups (ST)
 * @param acl_list a list of all access lists (US)
 * @param option string option to put in infront of the generated string
 *
 * @return true, if everything was fine
 *
 * @note MT-NOTE: sge_unparse_acl_dstring() is MT safe
 */
bool sge_unparse_acl_dstring(dstring *category_str, const char *owner, const char *group,
                             const ocs::tom::ConstList<ocs::tom::ST> grp_list,
                             const ocs::tom::ConstList<ocs::tom::US> acl_list, const char *option) {
   DENTER(TOP_LAYER);

   bool first = true;

   for (const auto elem : acl_list) {
      if (elem.get<US_consider_with_categories>() &&
          sge_contained_in_access_list(owner, group, grp_list, elem)) {
         if (first) {
            sge_dstring_append(category_str, option);
            sge_dstring_append_char(category_str, ' ');
            first = false;
         } else {
            sge_dstring_append_char(category_str, ',');
         }
         sge_dstring_append(category_str, elem.get<US_name>());
      }
   }
   if (!first) {
      sge_dstring_append_char(category_str, ' ');
   }

   DRETURN(true);
}


/**
 * @brief Creates a string from a queue name list
 *
 * The list is sorted by queue name first, so two jobs requesting the same
 * queues in a different order end up in the same category.
 *
 * @param category_str target string
 * @param queue_list the queue requests to render (QR), sorted in place; an empty handle appends nothing
 * @param option string option to put in infront of the generated string
 *
 * @return true, if everything was fine
 *
 * @note MT-NOTE: sge_unparse_queue_list_dstring() is MT safe
 */
bool sge_unparse_queue_list_dstring(dstring *category_str, const ocs::tom::List<ocs::tom::QR> queue_list,
                                    const char *option) {
   DENTER(TOP_LAYER);

   if (queue_list) {
      queue_list.sort<ocs::tom::asc<QR_name>>();

      bool first = true;
      for (const auto sub_elem : queue_list) {
         if (first) {
            sge_dstring_append(category_str, option);
            sge_dstring_append_char(category_str, ' ');
            first = false;
         } else {
            sge_dstring_append_char(category_str, ',');
         }
         sge_dstring_append(category_str, sub_elem.get<QR_name>());
      }
      sge_dstring_append_char(category_str, ' ');
   }

   DRETURN(true);
}

/**
 * @brief Creates a string from resource requests
 *
 * @param category_str target string
 * @param resource_list the resource requests to render (CE), sorted in place; an empty handle appends nothing
 * @param option string option to put in infront of the generated string
 *
 * @return true, if everything was fine
 *
 * @note MT-NOTE: sge_unparse_resource_list_dstring() is MT safe
 */
bool sge_unparse_resource_list_dstring(dstring *category_str, const ocs::tom::List<ocs::tom::CE> resource_list,
                                       const char *option) {
   DENTER(TOP_LAYER);

   if (resource_list) {
      resource_list.sort<ocs::tom::asc<CE_name>>();

       bool first = true;
       for (const auto sub_elem : resource_list) {
         if (first) {
            sge_dstring_append(category_str, option);
            sge_dstring_append(category_str, " ");
            first = false;
         } else {
            sge_dstring_append(category_str, ",");
         }
         /* CS-2014: the category has to be identical for requests that differ
          * only in the notation they were written with -- "-l h_vmem=1G" and
          * "-l h_vmem=1073741824" are the same request, and so are
          * "-l h_rt=1:00:00" and "-l h_rt=3600". MEM and TIME values therefore
          * go into the category in their canonical form (bytes, seconds).
          *
          * Done here and not in centry_fill_and_check(): that one rewrites
          * CE_stringval, the stored value, and plain client output has to show
          * the value as the user wrote it.
          */
         switch (static_cast<ocs::CEntry::Type>(sub_elem.get<CE_valtype>())) {
            case ocs::CEntry::Type::MEM:
            case ocs::CEntry::Type::TIME:
               sge_dstring_sprintf_append(category_str, "%s=%.0f",
                                          sub_elem.get<CE_name>(),
                                          sub_elem.get<CE_doubleval>());
               break;
            default:
               sge_dstring_append(category_str, sub_elem.get<CE_name>());
               sge_dstring_append(category_str, "=");
               sge_dstring_append(category_str, sub_elem.get<CE_stringval>());
               break;
         }
      }
      sge_dstring_append(category_str, " ");
   }

   DRETURN(true);
}

/**
 * @brief Creates a string from a pe request
 *
 * @param category_str target string
 * @param job the job, holding JB_pe and JB_pe_range
 * @param option string option to put in infront of the generated string
 *
 * @return true, if everything was fine; false for a parallel environment without ranges
 *
 * @note MT-NOTE: sge_unparse_pe_dstring() is MT safe
 */
bool sge_unparse_pe_dstring(dstring *category_str, const ocs::tom::ConstElement<ocs::tom::JB> job, const char *option) {
   DENTER(TOP_LAYER);

   if (const char *pe_name = job.get<JB_pe>(); pe_name != nullptr) {
      const auto range_list = job.get<JB_pe_range>();
      if (!range_list) {
         DPRINTF("Job has parallel environment with no ranges\n");
         DRETURN(false);
      } else {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(range_list, &range_string, true, false, false);
         sge_dstring_append(category_str, option);
         sge_dstring_append_char(category_str, ' ');
         sge_dstring_append(category_str, pe_name);
         sge_dstring_append_char(category_str, ' ');
         sge_dstring_append_dstring(category_str, &range_string);
         sge_dstring_append_char(category_str, ' ');

         sge_dstring_free(&range_string);
      }

   }

   DRETURN(true);
}

/**
 * @brief Copies a string into a dstring
 *
 * Copies a string into a dstring. Used for category string building
 *
 * @param category_str target string
 * @param value the value of the string attribute, e.g. the job's JB_project; nullptr appends nothing
 * @param option string option to put in in front of the generated string
 *
 * @return always true
 *
 * @note MT-NOTE: sge_unparse_string_option_dstring() is MT safe
 *
 * @see #sge_unparse_ulong_option_dstring
 */
bool sge_unparse_string_option_dstring(dstring *category_str, const char *value, const char *option) {
   DENTER(TOP_LAYER);
   if (value != nullptr) {
      sge_dstring_append(category_str, option);
      sge_dstring_append_char(category_str, ' ');
      sge_dstring_append(category_str, value);
      sge_dstring_append_char(category_str, ' ');
   }
   DRETURN(true);
}

/**
 * @brief Copies a number into a dstring
 *
 * Copies a number into a dstring. Used for category string building
 *
 * @param category_str target string
 * @param value the value of the number attribute, e.g. the job's JB_ar; 0 appends nothing
 * @param option string option to put in front of the generated string
 *
 * @return always true
 *
 * @note MT-NOTE: sge_unparse_ulong_option_dstring() is MT safe
 *
 * @see #sge_unparse_string_option_dstring
 */
bool
sge_unparse_ulong_option_dstring(dstring *category_str, const uint32_t value, const char *option) {
   DENTER(TOP_LAYER);

   if (value != 0) {
      sge_dstring_append(category_str, option);
      sge_dstring_append_char(category_str, ' ');
      sge_dstring_sprintf_append(category_str, sge_u32, value);
      sge_dstring_append_char(category_str, ' ');
   }
   DRETURN(true);
}

/**
 * @brief Append a job's binding request in the form the user typed it
 *
 * Nothing is appended when the job has no binding request.
 *
 * @param[out] category_str receives the `-binding` switch and its value
 * @param binding_elem the job's binding request (JB_binding), may be an empty handle
 * @return always true
 */
bool sge_unparse_binding_dstring(dstring *category_str, const ocs::tom::ConstElement<ocs::tom::BN> binding_elem) {
   DENTER(TOP_LAYER);
   if (binding_elem) {
      std::string binding_switches_str;
      ocs::BindingIo::binding_print_to_string(binding_elem, binding_switches_str, true);
      sge_dstring_append(category_str, binding_switches_str.c_str());
      sge_dstring_append_char(category_str, ' ');
   }
   DRETURN(true);
}

static ocs::tom::ConstList<ocs::tom::CX>
job_get_context_change(ocs::tom::ConstElement<ocs::tom::JB> job);

/**
 * @brief Verify a job object
 *
 * Verifies structure and contents of a job object.
 * As a job object may look quite different depending on its state,
 * additional functions are provided, calling this function doing
 * general tests and doing additional verification themselves.
 *
 * @param job the job object to verify (JB), may be an empty handle
 * @param answer_list answer list to pass back error messages
 * @param do_structure_verify verify the structure against the JB descriptor.
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: job_verify() is MT safe
 *
 * @bug The function is far from being complete.
 *      Currently, only the structure is verified, not the contents.
 *
 * @see `object_verify_structure()`, #job_verify_submitted_job, `job_verify_execd_job()`
 */
bool
job_verify(const ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
           const bool do_structure_verify) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (!job) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_NULLELEMENTPASSEDTO_S,
                              "job_verify");
      DRETURN(false);
   }

   if (do_structure_verify && !object_verify_structure(job, ocs::tom::JB::descriptor.e)) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_STRUCTURE_ERROR);
      DRETURN(false);
   }

   if (const char *name = job.get<JB_job_name>(); name != nullptr) {
      if (strlen(name) >= MAX_VERIFY_STRING) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_JOB_NAMETOOLONG_I,
                                 MAX_VERIFY_STRING);
         DRETURN(false);
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_JOB_NOJOBNAME);
      DRETURN(false);
   }

   if (const char *cwd = job.get<JB_cwd>(); cwd != nullptr) {
      /*
       * cwd needn't be an absolute path, we also accept
       * relative paths, e.g. via -wd switch,
       * or even pseudo variables, e.g. $HOME used by drmaa submit
       */
      ret = path_verify(cwd, answer_list, "cwd", false);
      if (!ret) {
         DRETURN(false);
      }
   }

   if (const auto path_aliases = job.get<JB_path_aliases>(); path_aliases) {
      ret = path_alias_verify(path_aliases, answer_list);
      if (!ret) {
         DRETURN(false);
      }
   }

   if (const auto env_list = job.get<JB_env_list>(); env_list) {
      ret = var_list_verify(env_list, answer_list);
      if (!ret) {
         DRETURN(false);
      }
   }

   if (const auto context_list = job.get<JB_context>(); context_list) {
      ret = var_list_verify(context_list, answer_list);
      if (!ret) {
         DRETURN(false);
      }
   }

   // the names of the context instructions, markers included, must not be empty either
   for (const auto change_ep : job_get_context_change(job)) {
      const char *variable = change_ep.get<CX_variable>();
      if (variable == nullptr || variable[0] == '\0') {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, SFNMAX,
                                 MSG_OBJECT_VARIABLENAME_NOT_EMPTY);
         DRETURN(false);
      }
   }

   const struct { ocs::tom::ConstList<ocs::tom::PN> list; const char *name; } path_lists[] = {
      {job.get<JB_stdout_path_list>(), "stdout path"},
      {job.get<JB_stderr_path_list>(), "stderr path"},
      {job.get<JB_stdin_path_list>(),  "stdin path"},
   };
   for (const auto &[list, name] : path_lists) {
      ret = path_list_verify(list, answer_list, name);
      if (!ret) {
         DRETURN(false);
      }
   }

   DRETURN(true);
}

/**
 * @brief Verify a just submitted job
 *
 * Verifies a just submitted job object.
 * Does generic tests by calling job_verify, like verifying the list
 * structure, and makes sure a number of job attributes are set
 * correctly.
 *
 * @param job the job to verify (JB)
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: job_verify_submitted_job() is MT safe
 *
 * @bug The function is far from being complete.
 *      Currently, only the structure is verified, not the contents.
 *
 * @see #job_verify
 */
bool job_verify_submitted_job(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = job_verify(job, answer_list, true);

   /* JB_job_number must be 0 */
   if (ret) {
      ret = object_verify_ulong_null<JB_job_number>(job, answer_list);
   }

   /* JB_version must be 0 */
   if (ret) {
      ret = object_verify_ulong_null<JB_version>(job, answer_list);
   }

   /* TODO: JB_jid_request_list */
   /* TODO: JB_jid_predecessor_l */
   /* TODO: JB_jid_sucessor_list */

   /* JB_session must be a valid string */
   if (ret) {
      const char *name = job.get<JB_session>();
      if (name != nullptr) {
         if (verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               ocs::tom::FieldTraits<JB_session>::info.name, KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* JB_project must be a valid string */
   if (ret) {
      const char *name = job.get<JB_project>();
      if (name != nullptr) {
         if (verify_str_key(
            answer_list, name, MAX_VERIFY_STRING,
            ocs::tom::FieldTraits<JB_project>::info.name, KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* JB_department must be a valid string */
   if (ret) {
      const char *name = job.get<JB_department>();
      if (name != nullptr) {
         if (verify_str_key(
            answer_list, name, MAX_VERIFY_STRING,
            ocs::tom::FieldTraits<JB_department>::info.name, KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* TODO: JB_directive_prefix can be any string, verify_str_key is too restrictive */

   /* JB_exec_file must be a valid directory string */
   if (ret) {
      const char *name = job.get<JB_exec_file>();
      if (name != nullptr) {
         ret = path_verify(name, answer_list, "exec_file", false);
      }
   }

   /* JB_script_file must be a string and a valid directory string */
   if (ret) {
      const char *name = job.get<JB_script_file>();
      if (name != nullptr) {
         ret = path_verify(name, answer_list, "script_file", false);
      }
   }

   /* JB_script_ptr must be any string */
   if (ret) {
      const char *name = job.get<JB_script_ptr>();
      if (name == nullptr) {
         /* JB_script_size must not 0 */
         ret = object_verify_ulong_null<JB_script_size>(job, answer_list);
      } else {
         /* JB_script_size must be size of JB_script_ptr */
         /* TODO: define a max script size */
         if (strlen(name) != job.get<JB_script_size>()) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_JOB_SCRIPTLENGTHDOESNOTMATCH);
            ret = false;
         }
      }
   }

   /* JB_submission_time is overwritten by qmaster */

   /* JB_execution_time can be any value */

   /* JB_deadline can be any value */

   /* JB_owner is overwritten by qmaster */

   /* JB_uid is overwritten by qmaster */

   /* JB_group is overwritten by qmaster */

   /* JB_gid is overwritten by qmaster */

   /* JB_account must be a valid string */
   if (ret) {
      const char *name = job.get<JB_account>();
      if (name != nullptr) {
         if(verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               ocs::tom::FieldTraits<JB_account>::info.name, QSUB_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* JB_notify boolean value */
   /* JB_type any ulong value */
   /* JB_reserve boolean value */

   /*
    * Job priority has a valid range from -1023 to 1024.
    * As JB_priority is an unsigned long, it is raised by BASE_PRIORITY at
    * job submission time.
    * Therefore a valid range is from 1 to 2 * BASE_PRIORITY.
    */
   if (ret) {
      uint32_t priority = job.get<JB_priority>();
      if (priority < 1 || priority > 2 * BASE_PRIORITY) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDPRIORITYMUSTBEINNEG1023TO1024);
         ret = false;
      }
      job_normalize_priority(job, priority);
   }

   /* JB_jobshare any ulong value */

   /* TODO JB_shell_list */
   /* JB_verify any ulong value */
   /* TODO JB_job_args */
   /* JB_checkpoint_attr any ulong */

   /* JB_checkpoint_name */
   if (ret) {
      const char *name = job.get<JB_checkpoint_name>();
      if (name != nullptr) {
         if (verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               ocs::tom::FieldTraits<JB_checkpoint_name>::info.name, KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* JB_checkpoint_object */
   if (ret) {
      if (job.get<JB_checkpoint_object>() != nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALIDJOB_REQUEST_S, "checkpoint object");
            ret = false;
      }
   }

   /* JB_checkpoint_interval any ulong */

   /* JB_restart can be 0, 1 or 2 */
   if (ret) {
      uint32_t value = job.get<JB_restart>();
      if (value != 0 && value != 1 && value != 2) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALIDJOB_REQUEST_S, "restart");
            ret = false;
      }
   }

   /* TODO: JB_stdout_path_list */
   /* TODO: JB_stderr_path_list */
   /* TODO: JB_stdin_path_list */
   /* JB_merge_stderr boolean value */
   /* TODO: JB_hard_resource_list */
   /* TODO: JB_soft_resource_list */
   /* TODO: JB_hard_queue_list */
   /* TODO: JB_soft_queue_list */
   /* TODO: JB_mail_options */
   /* JB_mail_list any ulong */

   /* JB_pe must be a valid string */
   if (ret) {
      const char *name = job.get<JB_pe>();
      if (name != nullptr) {
         if (verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               ocs::tom::FieldTraits<JB_pe>::info.name, KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* TODO: JB_pe_range */
   /* TODO: JB_master_hard_queue */

   /* JB_tgt can be any string value */
   /* JB_cred can be any string value */

   /* TODO: JB_ja_structure */
   /* TODO: JB_ja_n_h_ids */
   /* TODO: JB_ja_u_h_ids */
   /* TODO: JB_ja_s_h_ids */
   /* TODO: JB_ja_o_h_ids */
   /* TODO: JB_ja_a_h_ids */
   /* TODO: JB_ja_z_ids */
   /* TODO: JB_ja_template */
   /* TODO: JB_ja_tasks */

   /* JB_host must be nullptr */
   if (ret) {
      if (job.get<JB_host>() != nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALIDJOB_REQUEST_S, "host");
            ret = false;
      }
   }

   /* TODO: JB_category */
   /* TODO: JB_user_list */
   /* TODO: JB_job_identifier_list */

   /* JB_verify_suitable_queues must be in range of OPTION_VERIFY_STR */
   if (ret) {
      if (job.get<JB_verify_suitable_queues>() >= (sizeof(OPTION_VERIFY_STR)/sizeof(char)-1)) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALIDJOB_REQUEST_S, "verify");
            ret = false;
      }
   }

   /* JB_soft_wallclock_gmt must be 0 */
   if (ret) {
      ret = object_verify_ulong64_null<JB_soft_wallclock_gmt>(job, answer_list);
    }

   /* JB_hard_wallclock_gmt must be 0 */
   if (ret) {
      ret = object_verify_ulong64_null<JB_hard_wallclock_gmt>(job, answer_list);
    }

   /* JB_override_tickets must be 0 */
   if (ret) {
      ret = object_verify_ulong_null<JB_override_tickets>(job, answer_list);
    }

   /* JB_urg must be 0 */
   if (ret) {
      ret = object_verify_double_null<JB_urg>(job, answer_list);
    }
   /* JB_nurg must be 0 */
   if (ret) {
      ret = object_verify_double_null<JB_nurg>(job, answer_list);
   }
   /* JB_rrcontr must be 0 */
   if (ret) {
      ret = object_verify_double_null<JB_rrcontr>(job, answer_list);
    }
   /* JB_dlcontr must be 0 */
   if (ret) {
      ret = object_verify_double_null<JB_dlcontr>(job, answer_list);
    }
   /* JB_wtcontr must be 0 */
   if (ret) {
      ret = object_verify_double_null<JB_wtcontr>(job, answer_list);
    }
   // JB_ja_task_concurrency may only be given for array jobs
   if (ret) {
      uint32_t task_concurrency = job.get<JB_ja_task_concurrency>();
      if (task_concurrency > 0 && !job_is_array(job)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALIDJOB_REQUEST_S, "task concurrency");
         ret = false;
      }
   }

   /* JB_env_list
    * The loader and interpreter variables of a submission used to be removed here
    * (Issue GE-3761). They are not any more: the job runs as its owner, who may set
    * what they like, and what has to be protected are the administrative procedures
    * which can run as another user. The execution daemon's shepherd drops them for
    * those, see dangerous_env_vars[] in builtin_starter.cc (CS-2764).
    */

   DRETURN(ret);
}


/**
 * @brief Computes jobs wallclock limit
 *
 * Compute the jobs wallclock limit depending on requested h_rt, s_rt.
 * If no limit was requested the maximal ulong32 value is returned
 *
 * @param limit store for the value
 * @param jep jobs ep (JB)
 *
 * @return true on success false if no value was requested
 *
 * @note MT-NOTE: job_get_wallclock_limit() is not MT safe
 */
bool job_get_wallclock_limit(uint64_t *limit, const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);
   ocs::tom::ConstElement<ocs::tom::CE> ep;
   double d_ret = 0, d_tmp;
   const char *s;
   bool got_duration = false;
   char error_str[1024];

   const auto hard_resource_list = job_get_hard_resource_list(jep);
   if ((ep = hard_resource_list.find_first<CE_name>(SGE_ATTR_H_RT))) {
      if (parse_ulong_val(&d_tmp, nullptr, ocs::CEntry::Type::TIME, (s=ep.get<CE_stringval>()), error_str,
                          sizeof(error_str)-1)==0) {
         ERROR(MSG_CPLX_WRONGTYPE_SSS, SGE_ATTR_H_RT, s, error_str);
         DRETURN(false);
      }
      d_ret = d_tmp;
      got_duration = true;
   }

   if ((ep = hard_resource_list.find_first<CE_name>(SGE_ATTR_S_RT))) {
      if (parse_ulong_val(&d_tmp, nullptr, ocs::CEntry::Type::TIME, (s=ep.get<CE_stringval>()), error_str,
                          sizeof(error_str)-1)==0) {
         ERROR(MSG_CPLX_WRONGTYPE_SSS, SGE_ATTR_H_RT, s, error_str);
         DRETURN(false);
      }

      if (got_duration) {
         d_ret = std::min(d_ret, d_tmp);
      } else {
         d_ret = d_tmp;
         got_duration = true;
      }
   }

   if (got_duration) {
      if (d_ret > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
         *limit = std::numeric_limits<uint64_t>::max();
      } else {
         *limit = sge_gmt32_to_gmt64(d_ret);
      }
   } else {
      *limit = std::numeric_limits<uint64_t>::max();
   }

   DRETURN(got_duration);
}


/**
 * @brief Was "job" job submitted with -b y?
 *
 * This function returns true if "job" is a "binary" job
 * which was e.g. submitted with qsub -b y
 *
 * @param job JB element
 *
 * @return true or false
 *
 * @see #job_is_array, #job_is_binary, #job_is_tight_parallel
 */
bool
job_is_binary(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return (JOB_TYPE_IS_BINARY(job.get<JB_type>()) ? true : false);
}

/**
 * @brief Mark the job as submitted with a binary rather than a script
 *
 * @param[in,out] job the job to change
 * @param is_binary true to set #JOB_TYPE_BINARY, false to clear it
 * @return always true
 */
bool
job_set_binary(const ocs::tom::Element<ocs::tom::JB> job, const bool is_binary) {
   bool ret = true;
   uint32_t type = job.get<JB_type>();

   if (is_binary) {
      JOB_TYPE_SET_BINARY(type);
   } else {
      JOB_TYPE_CLEAR_BINARY(type);
   }
   job.set<JB_type>(type);
   return ret;
}

/**
 * @brief Was the job submitted with `qsub -noshell`?
 *
 * @param job the job to check
 * @return true when #JOB_TYPE_NO_SHELL is set
 */
bool
job_is_no_shell(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return (JOB_TYPE_IS_NO_SHELL(job.get<JB_type>()) ? true : false);
}

/**
 * @brief Mark the job as one to exec without a shell
 *
 * @param[in,out] job the job to change
 * @param is_binary true to set #JOB_TYPE_NO_SHELL, false to clear it
 * @return always true
 *
 * @note The parameter is called `is_no_shell` in the header; it has nothing to
 *       do with #JOB_TYPE_BINARY despite the name used here.
 */
bool
job_set_no_shell(const ocs::tom::Element<ocs::tom::JB> job, const bool is_binary) {
   bool ret = true;
   uint32_t type = job.get<JB_type>();

   if (is_binary) {
      JOB_TYPE_SET_NO_SHELL(type);
   } else {
      JOB_TYPE_CLEAR_NO_SHELL(type);
   }
   job.set<JB_type>(type);
   return ret;
}


/**
 * @brief Record who submitted the job
 *
 * @param[in,out] job the job to change
 * @param uid the submitter's numeric user id
 * @param gid the submitter's numeric primary group id
 * @param user the submitter's user name
 * @param group the submitter's primary group name
 * @param amount how many entries `grp_array` has
 * @param grp_array the submitter's supplementary groups
 * @return always true
 */
bool
job_set_owner_and_group(const ocs::tom::Element<ocs::tom::JB> job, const uint32_t uid, const uint32_t gid,
                        const char *user, const char *group, const int amount, ocs_grp_elem_t *grp_array) {
   DENTER(TOP_LAYER);

   bool ret = true;

   job.set<JB_owner>(user);
   job.set<JB_uid>(uid);
   job.set<JB_group>(group);
   job.set<JB_gid>(gid);
   job.set<JB_grp_list>(grp_list_array2list(amount, grp_array));

   DRETURN(ret);
}


/**
 * @brief Apply a context change request to a job's context
 *
 * The request is a `CX` list divided into groups. A group starts with a
 * marker element naming the operation, and any combination of groups may
 * appear in one request:
 *
 * | Marker | Effect on the following elements |
 * |---|---|
 * | `+` | appended to the job's context, or replacing an existing value |
 * | `-` | removed from the job's context |
 * | `=` | replacing the job's context entirely |
 *
 * `=` is the default when the list starts without a marker. The job's context
 * `JB_context` is a `VA` list with a unique name, so a variable given
 * again keeps its last value (CS-3058).
 *
 * @param change the request, a `CX` list; an empty request or an empty handle clears the context
 * @param[in,out] job the job whose `JB_context` is changed (JB)
 */
void set_context(const ocs::tom::ConstList<ocs::tom::CX> change, const ocs::tom::Element<ocs::tom::JB> job) {
   // if the incoming list is empty, or it starts without a marker, the context is replaced
   if (change.empty()) {
      job.set<JB_context>(nullptr);
      return;
   }
   const char *first = change.front().get<CX_variable>();
   if (first == nullptr || (*first != '+' && *first != '-' && *first != '=')) {
      job.set<JB_context>(nullptr);
   }

   char mode = '+';
   for (const auto change_ep : change) {
      const char *name = change_ep.get<CX_variable>();
      if (name == nullptr) {
         continue;
      }
      switch (*name) {
         case '+':
            mode = '+';
            break;
         case '-':
            mode = '-';
            break;
         case '=':
            job.set<JB_context>(nullptr);
            mode = '+';
            break;
         default:
            if (mode == '+') {
               auto context = job.get<JB_context>();
               if (!context) {
                  auto created = ocs::tom::ListPtr<ocs::tom::VA>::create("context_list");
                  context = created.get();
                  job.set<JB_context>(std::move(created));
               }
               auto var = context.find_first<VA_variable>(name);
               if (!var) {
                  var = context.emplace_back<VA_variable>(name);
               }
               var.set<VA_value>(change_ep.get<CX_value>());
            } else {
               // frees JB_context when its last variable goes, so the list is looked up again above
               job.erase_in<JB_context, VA_variable>(name);
            }
            break;
      }
   }
}


/**
 * @brief Append the instructions of one context switch to a context change request
 *
 * The parsers build one `VA` list per `-ac`, `-sc` or `-dc` switch, its
 * marker first. The request concatenates them as `CX` elements in the
 * order given, where markers and names repeat by design (CS-3058).
 *
 * The callers take the field out of a job or pass a local list; a request
 * that does not exist yet is created with its first instruction.
 *
 * @param[in,out] change the `CX` request, created if it is empty and the switch holds a variable
 * @param option_list the list of one switch, a `VA` list; an empty handle is accepted
 */
void ocs_context_change_add(ocs::tom::ListPtr<ocs::tom::CX> &change,
                            const ocs::tom::ConstList<ocs::tom::VA> option_list) {
   for (const auto var : option_list) {
      if (!change) {
         change = ocs::tom::ListPtr<ocs::tom::CX>::create("");
      }
      const auto ep = change->emplace_back<CX_variable>(var.get<VA_variable>());
      ep.set<CX_value>(var.get<VA_value>());
   }
}


/**
 * @brief The context change request of a job, also of a reduced one
 *
 * A reduced job, such as the one qstat -j works on, need not contain
 * `JB_context_change`; reading the field from it would abort.
 *
 * @param job the job (JB), full or reduced
 * @return the `CX` request, an empty handle if the job has none or does not contain the field
 */
static ocs::tom::ConstList<ocs::tom::CX>
job_get_context_change(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   if (!job.has<JB_context_change>()) {
      return nullptr;
   }
   return job.get<JB_context_change>();
}

/**
 * @brief The context a job has once its context change request is applied
 *
 * Before the qmaster has processed a job, its context is `JB_context`, empty
 * unless a GDI client set it, with the instructions of `JB_context_change`
 * still to be applied. Afterwards `JB_context_change` is empty and the result
 * is `JB_context` itself (CS-3058).
 *
 * @param job the job (JB), full or reduced
 * @return a new `VA` list, an empty handle if the context is empty
 */
ocs::tom::ListPtr<ocs::tom::VA> ocs_context_get_effective(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   auto tmp_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   if (job.has<JB_context>() && job.get<JB_context>()) {
      tmp_job->set<JB_context>(job.get<JB_context>().clone("context_list"));
   }
   if (const auto change = job_get_context_change(job); change) {
      set_context(change, *tmp_job);
   }
   return (*tmp_job).exchange<JB_context>(nullptr);
}


/**
 * @brief Write the checkpointing occasions as the letters a user wrote
 *
 * @param os the stream to append to
 * @param op the `JB_checkpoint_attr` bit field
 */
void
job_get_ckpt_attr(std::ostream &os, uint32_t op) {
   DENTER(TOP_LAYER);
   if (VALID(CHECKPOINT_AT_MINIMUM_INTERVAL, op)) {
      os << CHECKPOINT_AT_MINIMUM_INTERVAL_SYM;
   }
   if (VALID(CHECKPOINT_AT_SHUTDOWN, op)) {
      os << CHECKPOINT_AT_SHUTDOWN_SYM;
   }
   if (VALID(CHECKPOINT_SUSPEND, op)) {
      os << CHECKPOINT_SUSPEND_SYM;
   }
   if (VALID(NO_CHECKPOINT, op)) {
      os << NO_CHECKPOINT_SYM;
   }
   DRETURN_VOID;
}

/**
 * @brief Write the checkpointing occasions into a dstring
 *
 * @param op the `JB_checkpoint_attr` bit field
 * @param[out] string receives the letters, appended
 * @return always true
 */
bool job_get_ckpt_attr(uint32_t op, dstring *string) {
   DENTER(TOP_LAYER);
   std::stringstream ss;
   job_get_ckpt_attr(ss, op);
   sge_dstring_append(string, ss.str().c_str());
   DRETURN(true);
}

/**
 * @brief Write the verification level as the letter a user wrote
 *
 * @param op one of the `*_VERIFY` values
 * @param[out] string receives the letter, appended; an unknown value yields `n`
 * @return always true
 */
bool job_get_verify_attr(uint32_t op, dstring *string) {
   DENTER(TOP_LAYER);

   bool success = true;

   if (ERROR_VERIFY == op) {
      sge_dstring_append_char(string, 'e');
   } else if (WARNING_VERIFY == op) {
      sge_dstring_append_char(string, 'w');
   } else if (JUST_VERIFY == op) {
      sge_dstring_append_char(string, 'v');
   } else if (POKE_VERIFY == op) {
      sge_dstring_append_char(string, 'p');
   } else {
      sge_dstring_append_char(string, 'n');
   }
   DRETURN(success);
}

bool
/**
 * @brief Turn a `-w` letter into a verification level
 *
 * `qrsub` accepts fewer letters than `qsub` does, so the caller's program
 * decides which are valid and which yield an error.
 *
 * @param[out] level receives one of the `*_VERIFY` or `AR_*_VERIFY` values
 * @param input the letter the user wrote
 * @param prog_number the calling program, e.g. `QRSUB`
 * @param[out] answer_list receives the message naming the rejected letter
 * @return true when the letter is valid for that program
 */
job_parse_validation_level(int *level, const char *input, int prog_number,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (strcmp("e", input) == 0) {
      if (prog_number == QRSUB) {
         *level = AR_ERROR_VERIFY;
      } else {
         *level = ERROR_VERIFY;
      }
   } else if (strcmp("w", input) == 0) {
      if (prog_number == QRSUB) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
         ret = false;
      } else {
         *level = WARNING_VERIFY;
      }
   } else if (strcmp("n", input) == 0) {
      if (prog_number == QRSUB) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
         ret = false;
      } else {
         *level = SKIP_VERIFY;
      }
   } else if (strcmp("v", input) == 0) {
      if (prog_number == QRSUB) {
         *level = AR_JUST_VERIFY;
      } else {
         *level = JUST_VERIFY;
      }
   } else if (strcmp("p", input) == 0) {
      if (prog_number == QRSUB) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
         ret = false;
      } else {
         *level = POKE_VERIFY;
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Is job requesting resources of type
 *
 * This function returns true if "job" is requesting a resource with type
 * CONSUMABLE_JOB.
 *
 * @param jep JB element
 * @param resource_name Name of resource
 *
 * @return true or false
 */
bool job_is_requesting_consumable(const ocs::tom::ConstElement<ocs::tom::JB> jep, const char *resource_name) {
   const auto request_list = job_get_hard_resource_list(jep);

   if (request_list) {
      const auto cep = centry_list_locate(request_list, resource_name);
      if (cep) {
         const uint32_t consumable = cep.get<CE_consumable>();
         if (consumable == CONSUMABLE_JOB) {
            return true;
         }
      }
   }
   return false;
}

/**
 * @brief Turn a scope name into its id
 *
 * @param scope `global`, `master` or `slave`, case insensitively
 * @param[out] scope_id receives the id; `JRS_SCOPE_GLOBAL` when the name was
 *                      not recognised
 * @return true when the name was recognised
 */
bool job_parse_scope_string(const char *scope, char &scope_id) {
   bool ret = true;

   if (sge_strnullcasecmp(scope, "global") == 0) {
      scope_id = JRS_SCOPE_GLOBAL;
   } else if (sge_strnullcasecmp(scope, "master") == 0) {
      scope_id = JRS_SCOPE_MASTER;
   } else if (sge_strnullcasecmp(scope, "slave") == 0) {
      scope_id = JRS_SCOPE_SLAVE;
   } else {
      scope_id = JRS_SCOPE_GLOBAL;
      ret = false;
   }

   return ret;
}

/**
 * @brief The request set of one scope
 *
 * A job may state different requests for its global, master and slave part.
 *
 * @param job the job to read (JB); a reduced job without the attribute is handled
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the request set (JRS), an empty handle when the job has none for that scope
 */
ocs::tom::ConstElement<ocs::tom::JRS> job_get_request_set(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                          const uint32_t scope) {
   // job might be reduced
   if (!job || !job.has<JB_request_set_list>()) {
      return nullptr;
   }
   return job.find_first_in<JB_request_set_list, JRS_scope>(scope);
}

/**
 * @brief The request set of one scope, writable
 *
 * @param job the job to read (JB); a reduced job without the attribute is handled
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the request set (JRS), an empty handle when the job has none for that scope
 */
ocs::tom::Element<ocs::tom::JRS> job_get_request_setRW(const ocs::tom::Element<ocs::tom::JB> job,
                                                       const uint32_t scope) {
   // job might be reduced
   if (!job || !job.has<JB_request_set_list>()) {
      return nullptr;
   }
   return job.find_first_in_rw<JB_request_set_list, JRS_scope>(scope);
}

/**
 * @brief The request set of one scope, created when it does not exist
 *
 * @param[in,out] job the job to read or extend (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the request set (JRS), never empty
 */
ocs::tom::Element<ocs::tom::JRS> job_get_or_create_request_setRW(const ocs::tom::Element<ocs::tom::JB> job,
                                                                 const uint32_t scope) {
   auto jrs = job.find_first_in_rw<JB_request_set_list, JRS_scope>(scope);
   if (!jrs) {
      jrs = job.emplace_back_in<JB_request_set_list, JRS_scope>(scope);
   }

   return jrs;
}


/**
 * @brief Remove duplicate resource requests from the job request set list.
 *
 * @param job the job to work on (JB)
 * @return true if the job has any resource requests, else false
 */
bool job_request_set_remove_duplicates(const ocs::tom::Element<ocs::tom::JB> job) {
   bool requests_found = false;

   for (const auto jrs : job.get<JB_request_set_list>()) {
      if (const auto lp = jrs.get<JRS_hard_resource_list>(); lp) {
         requests_found = true;
         centry_list_remove_duplicates(lp);
      }
      if (const auto lp_ce = jrs.get<JRS_soft_resource_list>(); lp_ce) {
         requests_found = true;
         centry_list_remove_duplicates(lp_ce);
      }
   }

   return requests_found;
}


/**
 * @brief Does the job request a queue in any scope?
 *
 * @param job the job to check (JB)
 * @return true when any request set has a hard or soft queue list
 */
bool job_request_set_has_queue_requests(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   bool ret = false;
   for (const auto jrs : job.get<JB_request_set_list>()) {
      if (jrs.get<JRS_hard_queue_list>() || jrs.get<JRS_soft_queue_list>()) {
         ret = true;
         break;
      }
   }

   return ret;
}

/**
 * @brief The largest hard request for one resource across all scopes
 *
 * @param job the job to search (JB)
 * @param request_name the resource to look for
 * @return the request with the highest value (CE), an empty handle when none asks for it
 */
ocs::tom::ConstElement<ocs::tom::CE> job_get_highest_hard_request(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                                  const char *request_name) {
   ocs::tom::ConstElement<ocs::tom::CE> ret = nullptr;
   double max_request = 0.0;

   for (const auto jrs : job.get<JB_request_set_list>()) {
      const auto request = jrs.find_first_in<JRS_hard_resource_list, CE_name>(request_name);
      if (request) {
         const double request_value = request.get<CE_doubleval>();
         if (request_value > max_request) {
            ret = request;
            max_request = request_value;
         }
      }
   }

   return ret;
}

/**
 * @brief The resource requests of one request set
 *
 * A job may state different requests for its global, master and slave
 * part; `scope` selects which of the three request sets is meant.
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param hard true for the hard list, false for the soft one
 * @return the list (CE), an empty handle when the job has no such request set
 */
ocs::tom::ConstList<ocs::tom::CE> job_get_resource_list(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                        const uint32_t scope, const bool hard) {
   ocs::tom::ConstList<ocs::tom::CE> ret = nullptr;
   const auto jrs = job_get_request_set(job, scope);
   if (jrs) {
      ret = hard ? jrs.get<JRS_hard_resource_list>() : jrs.get<JRS_soft_resource_list>();
   }
   return ret;
}

/**
 * @brief The global hard resource requests of a job
 *
 * @param job the job to read (JB)
 * @return the list (CE), an empty handle when the job has no global request set
 *
 * @see #job_get_resource_list
 */
ocs::tom::ConstList<ocs::tom::CE> job_get_hard_resource_list(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_resource_list(job, JRS_SCOPE_GLOBAL, true);
}


/**
 * @brief The hard resource requests of one scope
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the list (CE), an empty handle when the job has no such request set
 *
 * @see #job_get_resource_list
 */
ocs::tom::ConstList<ocs::tom::CE> job_get_hard_resource_list(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                             const uint32_t scope) {
   return job_get_resource_list(job, scope, true);
}


/**
 * @brief The global soft resource requests of a job
 *
 * @param job the job to read (JB)
 * @return the list (CE), an empty handle when the job has no global request set
 *
 * @see #job_get_resource_list
 */
ocs::tom::ConstList<ocs::tom::CE> job_get_soft_resource_list(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_resource_list(job, JRS_SCOPE_GLOBAL, false);
}


/**
 * @brief The global hard queue requests of a job
 *
 * @param job the job to read (JB)
 * @return the list (QR), an empty handle when the job has no global request set
 *
 * @see #job_get_queue_list
 */
ocs::tom::ConstList<ocs::tom::QR> job_get_hard_queue_list(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_hard_queue_list(job, JRS_SCOPE_GLOBAL);
}


/**
 * @brief The queue requests of one request set
 *
 * A job may state different requests for its global, master and slave
 * part; `scope` selects which of the three request sets is meant.
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param hard true for the hard list, false for the soft one
 * @return the list (QR), an empty handle when the job has no such request set
 */
ocs::tom::ConstList<ocs::tom::QR> job_get_queue_list(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                     const uint32_t scope, const bool hard) {
   ocs::tom::ConstList<ocs::tom::QR> ret = nullptr;
   const auto jrs = job_get_request_set(job, scope);

   if (jrs) {
      ret = hard ? jrs.get<JRS_hard_queue_list>() : jrs.get<JRS_soft_queue_list>();
   }
   return ret;
}

/**
 * @brief The hard queue requests of one scope
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the list (QR), an empty handle when the job has no such request set
 *
 * @see #job_get_queue_list
 */
ocs::tom::ConstList<ocs::tom::QR> job_get_hard_queue_list(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                          const uint32_t scope) {
   return job_get_queue_list(job, scope, true);
}


/**
 * @brief The global soft queue requests of a job
 *
 * @param job the job to read (JB)
 * @return the list (QR), an empty handle when the job has no global request set
 *
 * @see #job_get_queue_list
 */
ocs::tom::ConstList<ocs::tom::QR> job_get_soft_queue_list(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_queue_list(job, JRS_SCOPE_GLOBAL, false);
}

/**
 * @brief The soft queue requests of one scope
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the list (QR), an empty handle when the job has no such request set
 *
 * @see #job_get_queue_list
 */
ocs::tom::ConstList<ocs::tom::QR> job_get_soft_queue_list(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                          const uint32_t scope) {
   return job_get_queue_list(job, scope, false);
}

/**
 * @brief The hard queue requests of a job's master task
 *
 * @param job the job to read (JB)
 * @return the list (QR), an empty handle when the job has no master request set
 */
ocs::tom::ConstList<ocs::tom::QR> job_get_master_hard_queue_list(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return job_get_queue_list(job, JRS_SCOPE_MASTER, true);
}

/**
 * @brief The resource requests of one request set
 *
 * A job may state different requests for its global, master and slave
 * part; `scope` selects which of the three request sets is meant.
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param hard true for the hard list, false for the soft one
 * @return the list (CE), an empty handle when the job has no such request set
 */
ocs::tom::List<ocs::tom::CE> job_get_resource_listRW(const ocs::tom::Element<ocs::tom::JB> job, const uint32_t scope,
                                                     const bool hard) {
   ocs::tom::List<ocs::tom::CE> ret = nullptr;
   const auto jrs = job_get_request_setRW(job, scope);
   if (jrs) {
      ret = hard ? jrs.get<JRS_hard_resource_list>() : jrs.get<JRS_soft_resource_list>();
   }
   return ret;
}

/**
 * @brief The global hard resource requests of a job
 *
 * @param job the job to read (JB)
 * @return the list (CE), an empty handle when the job has no global request set
 *
 * @see #job_get_resource_listRW
 */
ocs::tom::List<ocs::tom::CE> job_get_hard_resource_listRW(const ocs::tom::Element<ocs::tom::JB> job) {
   return job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, true);
}


/**
 * @brief The global soft resource requests of a job
 *
 * @param job the job to read (JB)
 * @return the list (CE), an empty handle when the job has no global request set
 *
 * @see #job_get_resource_listRW
 */
ocs::tom::List<ocs::tom::CE> job_get_soft_resource_listRW(const ocs::tom::Element<ocs::tom::JB> job) {
   return job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, false);
}

/**
 * @brief The queue requests of one request set
 *
 * A job may state different requests for its global, master and slave
 * part; `scope` selects which of the three request sets is meant.
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param hard true for the hard list, false for the soft one
 * @return the list (QR), an empty handle when the job has no such request set
 */
ocs::tom::List<ocs::tom::QR> job_get_queue_listRW(const ocs::tom::Element<ocs::tom::JB> job, const uint32_t scope,
                                                  const bool hard) {
   ocs::tom::List<ocs::tom::QR> ret = nullptr;
   const auto jrs = job_get_request_setRW(job, scope);
   if (jrs) {
      ret = hard ? jrs.get<JRS_hard_queue_list>() : jrs.get<JRS_soft_queue_list>();
   }
   return ret;
}

/**
 * @brief Replace the resource requests of one request set
 *
 * The request set is created when the job does not have it yet.
 *
 * @param[in,out] job the job to change (JB)
 * @param resource_list the new list (CE), consumed; ownership passes to the job
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param hard true for the hard list, false for the soft one
 */
void job_set_resource_list(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::CE> &&resource_list,
                           const uint32_t scope, const bool hard) {
   // consumed on every path
   auto list = std::move(resource_list);
   const auto jrs = job_get_or_create_request_setRW(job, scope);
   if (jrs) {
      if (hard) {
         jrs.set<JRS_hard_resource_list>(std::move(list));
      } else {
         jrs.set<JRS_soft_resource_list>(std::move(list));
      }
   }
}


/**
 * @brief Replace the global hard resource requests of a job
 *
 * @param[in,out] job the job to change (JB)
 * @param resource_list the new list (CE), consumed; ownership passes to the job
 *
 * @see #job_set_resource_list
 */
void job_set_hard_resource_list(const ocs::tom::Element<ocs::tom::JB> job,
                                ocs::tom::ListPtr<ocs::tom::CE> &&resource_list) {
   job_set_resource_list(job, std::move(resource_list), JRS_SCOPE_GLOBAL, true);
}


/**
 * @brief Replace the hard resource requests of one scope
 *
 * @param[in,out] job the job to change (JB)
 * @param resource_list the new list (CE), consumed; ownership passes to the job
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 *
 * @see #job_set_resource_list
 */
void job_set_hard_resource_list(const ocs::tom::Element<ocs::tom::JB> job,
                                ocs::tom::ListPtr<ocs::tom::CE> &&resource_list, const uint32_t scope) {
   job_set_resource_list(job, std::move(resource_list), scope, true);
}


/**
 * @brief Replace the global soft resource requests of a job
 *
 * @param[in,out] job the job to change (JB)
 * @param resource_list the new list (CE), consumed; ownership passes to the job
 *
 * @see #job_set_resource_list
 */
void job_set_soft_resource_list(const ocs::tom::Element<ocs::tom::JB> job,
                                ocs::tom::ListPtr<ocs::tom::CE> &&resource_list) {
   job_set_resource_list(job, std::move(resource_list), JRS_SCOPE_GLOBAL, false);
}


/**
 * @brief Replace the soft resource requests of one scope
 *
 * @param[in,out] job the job to change (JB)
 * @param resource_list the new list (CE), consumed; ownership passes to the job
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 *
 * @see #job_set_resource_list
 */
void job_set_soft_resource_list(const ocs::tom::Element<ocs::tom::JB> job,
                                ocs::tom::ListPtr<ocs::tom::CE> &&resource_list, const uint32_t scope) {
   job_set_resource_list(job, std::move(resource_list), scope, false);
}


/**
 * @brief Replace the queue requests of one request set
 *
 * The request set is created when the job does not have it yet.
 *
 * @param[in,out] job the job to change (JB)
 * @param queue_list the new list (QR), consumed; ownership passes to the job
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param hard true for the hard list, false for the soft one
 */
void job_set_queue_list(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::QR> &&queue_list,
                        const uint32_t scope, const bool hard) {
   // consumed on every path
   auto list = std::move(queue_list);
   const auto jrs = job_get_or_create_request_setRW(job, scope);
   if (jrs) {
      if (hard) {
         jrs.set<JRS_hard_queue_list>(std::move(list));
      } else {
         jrs.set<JRS_soft_queue_list>(std::move(list));
      }
   }
}


/**
 * @brief Replace the global hard queue requests of a job
 *
 * @param[in,out] job the job to change (JB)
 * @param queue_list the new list (QR), consumed; ownership passes to the job
 *
 * @see #job_set_queue_list
 */
void job_set_hard_queue_list(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::QR> &&queue_list) {
   job_set_queue_list(job, std::move(queue_list), JRS_SCOPE_GLOBAL, true);
}


/**
 * @brief Replace the hard queue requests of one scope
 *
 * @param[in,out] job the job to change (JB)
 * @param queue_list the new list (QR), consumed; ownership passes to the job
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 *
 * @see #job_set_queue_list
 */
void job_set_hard_queue_list(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::QR> &&queue_list,
                             const uint32_t scope) {
   job_set_queue_list(job, std::move(queue_list), scope, true);
}


/**
 * @brief Replace the soft queue requests of one scope
 *
 * @param[in,out] job the job to change (JB)
 * @param queue_list the new list (QR), consumed; ownership passes to the job
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 *
 * @see #job_set_queue_list
 */
void job_set_soft_queue_list(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::QR> &&queue_list,
                             const uint32_t scope) {
   job_set_queue_list(job, std::move(queue_list), scope, false);
}


/**
 * @brief Replace the hard queue requests of a job's master task
 *
 * @param[in,out] job the job to change (JB)
 * @param queue_list the new list (QR), consumed; ownership passes to the job
 *
 * @see #job_set_queue_list
 */
void job_set_master_hard_queue_list(const ocs::tom::Element<ocs::tom::JB> job,
                                    ocs::tom::ListPtr<ocs::tom::QR> &&queue_list) {
   job_set_queue_list(job, std::move(queue_list), JRS_SCOPE_MASTER, true);
}


/**
 * @brief Set the allocation rule of one request set
 *
 * @param[in,out] job the job to change (JB)
 * @param allocation_rule the rule to store
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 */
void job_set_allocation_rule(const ocs::tom::Element<ocs::tom::JB> job, const char *allocation_rule,
                             const uint32_t scope) {
   const auto jrs = job_get_or_create_request_setRW(job, scope);
   if (jrs) {
      jrs.set<JRS_allocation_rule>(allocation_rule);
   }
}


/**
 * @brief The allocation rule of one request set
 *
 * @param job the job to read (JB)
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return the rule, or nullptr when the scope has none
 */
const char *job_get_allocation_rule(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t scope) {
   const char *ret = nullptr;
   const auto jrs = job_get_request_set(job, scope);
   if (jrs) {
      ret = jrs.get<JRS_allocation_rule>();
   }
   return ret;
}


static void
job_add_str_to_command_line(dstring *dstr, const char *str) {
   if (str != nullptr) {
      bool do_quote = sge_has_whitespace(str) || ocs::is_pattern(str);
      if (do_quote) {
         sge_dstring_append_char(dstr, '\'');
      }
      sge_dstring_append(dstr, str);
      if (do_quote) {
         sge_dstring_append_char(dstr, '\'');
      }
   }
}

static void
job_add_opt_to_comand_line(dstring *dstr, const char *opt, const char *value) {
   if (opt != nullptr) {
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
   }
   if (value != nullptr) {
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, value);
   }
}

/**
 * @brief Append a boolean option to a command line
 *
 * @tparam F the boolean field of the job
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-j`
 * @param print_value the field value for which the option is appended
 * @return true if the option was appended
 */
template <auto F>
static bool
job_add_bool_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr, const char *opt,
                                 const bool print_value) {
   DENTER(TOP_LAYER);
   bool ret = false;
   const bool b = job.get<F>();
   if (b == print_value) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, b ? "yes" : "no");
   }

   DRETURN(ret);
}

/**
 * @brief Append a numeric option to a command line
 *
 * @tparam F the ulong field of the job
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-js`, or nullptr for the value alone
 * @param offset added to the field value before it is printed
 * @return true if the option was appended, which it is unless the value is 0
 */
template <auto F>
static bool
job_add_ulong_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr, const char *opt,
                                  const long offset) {
   DENTER(TOP_LAYER);
   bool ret = false;
   const long l = job.get<F>() + offset;
   if (l != 0) {
      ret = true;
      if (opt != nullptr) {
         sge_dstring_append_char(dstr, ' ');
         sge_dstring_append(dstr, opt);
      }
      sge_dstring_sprintf_append(dstr, " %ld", l);
   }

   DRETURN(ret);
}

/**
 * @brief Append a time option to a command line
 *
 * @tparam F the ulong64 time field of the job
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-a`, or nullptr for the value alone
 * @return always true
 */
template <auto F>
static bool
job_add_time_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr, const char *opt) {
   DENTER(TOP_LAYER);
   bool ret = true;
   const uint64_t time64 = job.get<F>();
   if (time64 != 0) {
      ret = true;
      if (opt != nullptr) {
         sge_dstring_append_char(dstr, ' ');
         sge_dstring_append(dstr, opt);
      }
      sge_dstring_append_char(dstr, ' ');
      DSTRING_STATIC(dstr_time, 64);
      sge_dstring_append(dstr, sge_ctime64_date_time(time64, &dstr_time));
   }

   DRETURN(ret);
}

/**
 * @brief Append a string option to a command line
 *
 * @tparam F the string field of the job
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-N`, or nullptr for the value alone
 * @return true if the option was appended, which it is unless the field is nullptr
 */
template <auto F>
static bool
job_add_str_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr, const char *opt) {
   DENTER(TOP_LAYER);
   bool ret = false;
   const char *str = job.get<F>();
   if (str != nullptr) {
      ret = true;
      if (opt != nullptr) {
         sge_dstring_append_char(dstr, ' ');
         sge_dstring_append(dstr, opt);
      }
      sge_dstring_append_char(dstr, ' ');
      job_add_str_to_command_line(dstr, str);
   }

   DRETURN(ret);
}

/**
 * @brief Append a resource request option to a command line
 *
 * @tparam F the CE list field of the request set
 * @param scope_ep the request set (JRS)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-hard -l`
 * @return true if the option was appended, which it is unless the list is nullptr
 */
template <auto F>
static bool
job_add_ce_list_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JRS> scope_ep, dstring *dstr,
                                    const char *opt) {
   DENTER(TOP_LAYER);
   bool ret = false;
   if (const auto ce_list = scope_ep.get<F>(); ce_list) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      dstring local_dstr = DSTRING_INIT;
      job_add_str_to_command_line(dstr, centry_list_append_to_dstring(ce_list, &local_dstr));
      sge_dstring_free(&local_dstr);
   }

   DRETURN(ret);
}

/**
 * @brief Append an option with a comma separated list of names to a command line
 *
 * @tparam F the list field of the element
 * @tparam LF the string field of the list's elements that is printed
 * @param ep the element holding the list
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-hold_jid`
 * @return true if the option was appended, which it is unless the list is nullptr
 */
template <auto F, auto LF>
static bool
job_add_list_opt_to_command_line(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                                 dstring *dstr, const char *opt) {
   DENTER(TOP_LAYER);
   bool ret = false;
   if (const auto lp = ep.template get<F>(); lp) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      bool first = true;
      for (const auto list_ep : lp) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(dstr, ',');
         }
         sge_dstring_append(dstr, list_ep.template get<LF>());
      }
   }

   DRETURN(ret);
}

/**
 * @brief Append an option with a comma separated list of `[host:]path` entries to a command line
 *
 * @tparam F the PN list field of the job
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-o`
 * @return true if the option was appended, which it is unless the list is nullptr
 */
template <auto F>
static bool
job_add_host_str_list_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr,
                                          const char *opt) {
   DENTER(TOP_LAYER);
   bool ret = false;
   if (const auto lp = job.get<F>(); lp) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      bool first = true;
      for (const auto ep : lp) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(dstr, ',');
         }
         const char *host = ep.template get<PN_host>();
         const char *str = ep.template get<PN_path>();
         if (str != nullptr) {
            if (host != nullptr) {
               sge_dstring_append(dstr, host);
               sge_dstring_append_char(dstr, ':');
            }
            sge_dstring_append(dstr, str);
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Append an option with a comma separated list of `name=value` entries to a command line
 *
 * @tparam F the VA list field of the job
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 * @param opt the option, e.g. `-v`
 * @param filter entries whose name starts with it are skipped, nullptr for none
 * @return true if the option was appended, which it is when at least one entry was printed
 */
template <auto F>
static bool
job_add_name_value_list_opt_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr,
                                            const char *opt, const char *filter = nullptr) {
   DENTER(TOP_LAYER);
   bool ret = false;
   if (const auto lp = job.get<F>(); lp) {
      bool first = true;
      for (const auto ep : lp) {
         const char *name = ep.template get<VA_variable>();
         // apply filter to name, if name starts with filter, then skip this entry
         if (filter != nullptr && strstr(name, filter) == name) {
            continue;
         }
         if (first) {
            first = false;
            // if we get here for the first time then print the option string
            sge_dstring_append_char(dstr, ' ');
            sge_dstring_append(dstr, opt);
            sge_dstring_append_char(dstr, ' ');
            ret = true;
         } else {
            // additional variable, print the separator
            sge_dstring_append_char(dstr, ',');
         }
         if (name != nullptr) {
            sge_dstring_append(dstr, name);
            sge_dstring_append_char(dstr, '=');

            const char *value = ep.template get<VA_value>();
            if (value != nullptr) {
               sge_dstring_append(dstr, value);
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Append the request lists of one request set to a command line
 *
 * @param scope_ep the request set (JRS)
 * @param[out] dstr the command line to append to
 */
static void
job_add_resource_set_list_scope_to_command_line(const ocs::tom::ConstElement<ocs::tom::JRS> scope_ep, dstring *dstr) {
   DENTER(TOP_LAYER);
   job_add_ce_list_opt_to_command_line<JRS_hard_resource_list>(scope_ep, dstr, "-hard -l");
   job_add_ce_list_opt_to_command_line<JRS_soft_resource_list>(scope_ep, dstr, "-soft -l");
   job_add_list_opt_to_command_line<JRS_hard_queue_list, QR_name>(scope_ep, dstr, "-hard -q");
   job_add_list_opt_to_command_line<JRS_soft_queue_list, QR_name>(scope_ep, dstr, "-soft -q");
   DRETURN_VOID;
}

/**
 * @brief The name of a request scope
 *
 * @param scope_id one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @return `global`, `master`, `slave`, or `unknown` for anything else
 */
const char *job_scope_name(uint32_t scope_id) {
   const char *ret = "unknown";

   switch (scope_id) {
      case JRS_SCOPE_GLOBAL:
         ret = "global";
         break;
      case JRS_SCOPE_MASTER:
         ret = "master";
         break;
      case JRS_SCOPE_SLAVE:
         ret = "slave";
         break;
   }

   return ret;
}

/**
 * @brief The attribute name a scope and list combination is written as
 *
 * Builds the name users see and type, e.g. `master_hard_queue_list`. The
 * global scope contributes no prefix, and the allocation rule has no
 * hard/soft distinction.
 *
 * @param scope one of `JRS_SCOPE_GLOBAL`, `JRS_SCOPE_MASTER`, `JRS_SCOPE_SLAVE`
 * @param nm the `JRS_*` list within the request set
 * @param with_colon true to append a colon, for use as a label
 * @return the assembled name
 */
std::string get_scope_list_name(const uint32_t scope, const int nm, const bool with_colon) {
   std::string name;
   if (scope > JRS_SCOPE_GLOBAL) {
      name += job_scope_name(scope);
      name += "_";
   }

   if (nm != JRS_allocation_rule) {
      const bool is_hard = (nm == JRS_hard_resource_list || nm == JRS_hard_queue_list);
      name += is_hard ? "hard_" : "soft_";
   }

   switch (nm) {
      case JRS_hard_resource_list:
      case JRS_soft_resource_list:
         name += "resource_list";
         break;
      case JRS_soft_queue_list:
      case JRS_hard_queue_list:
         name += "queue_list";
         break;
      case JRS_allocation_rule:
         name += "allocation_rule";
         break;
      default:
         name += "unknown";
   }

   if (with_colon) {
      name += ":";
   }
   return name;
}


/**
 * @brief The name of the scope a request set belongs to
 *
 * @param scope_ep the request set element (JRS); an empty handle yields `unknown`
 * @return `global`, `master`, `slave`, or `unknown`
 */
const char *
job_scope_name(const ocs::tom::ConstElement<ocs::tom::JRS> scope_ep) {
   const char *ret = "unknown";

   if (scope_ep) {
      ret = job_scope_name(scope_ep.get<JRS_scope>());
   }

   return ret;
}

/**
 * @brief Append the request sets of a job to a command line
 *
 * @param job the job (JB)
 * @param[out] dstr the command line to append to
 */
static void
job_add_resource_set_list_to_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr) {
   const auto request_set_list = job.get<JB_request_set_list>();
   if (request_set_list) {
      for (const auto scope_ep : request_set_list) {
         job_add_opt_to_comand_line(dstr, "-scope", job_scope_name(scope_ep));
         const char *allocation_rule = scope_ep.get<JRS_allocation_rule>();
         if (allocation_rule != nullptr) {
            job_add_opt_to_comand_line(dstr, "-par", allocation_rule);
         }
         job_add_resource_set_list_scope_to_command_line(scope_ep, dstr);
      }
   }
}

/**
 * @brief Generate the command line that submits an identical job
 *
 * Based on the attributes of the job object the function generates a command line
 * which can be used to submit an identical job (if no sge_request files and/or JSV are active).
 * The command line is stored in the given dstring.
 *
 * @param[in] job      job used as template for the command line (JB)
 * @param[in] dstr     dstring which will contain the command line
 * @param[in] client   client name, this will become the first word in the command line
 * @return the command line as string
 */
const char *
job_get_effective_command_line(const ocs::tom::ConstElement<ocs::tom::JB> job, dstring *dstr, const char *client) {
   DENTER(TOP_LAYER);
   // the submit client name
   sge_dstring_copy_string(dstr, client);

   // the submit options
   job_add_str_opt_to_command_line<JB_account>(job, dstr, "-A");
   job_add_time_opt_to_command_line<JB_execution_time>(job, dstr, "-a");
   // -ac is covered by -sc
   job_add_ulong_opt_to_command_line<JB_ar>(job, dstr, "-ar", 0);
   if (JOB_TYPE_IS_BINARY(job.get<JB_type>())) {
      job_add_opt_to_comand_line(dstr, "-b", "yes");
   }

   // -btype, -bunit, -bstrategy, -bstart, -bstop, -bamount ...
   if (const auto binding = job.get<JB_binding>(); binding) {
      std::string binding_str;
      ocs::BindingIo::binding_print_to_string(binding, binding_str, true);
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, binding_str.c_str());
   }

   job_add_str_opt_to_command_line<JB_directive_prefix>(job, dstr, "-C");

   // checkpointing
   if (job_add_str_opt_to_command_line<JB_checkpoint_name>(job, dstr, "-ckpt")) {
      u_long ckpt_attr = job.get<JB_checkpoint_attr>();
      u_long ckpt_interval = job.get<JB_checkpoint_interval>();
      if (ckpt_attr != 0 || ckpt_interval != 0) {
         sge_dstring_append(dstr, " -c ");
         if (ckpt_attr != 0) {
            job_get_ckpt_attr(ckpt_attr, dstr);
         }
         if (ckpt_interval != 0) {
            sge_dstring_sprintf_append(dstr, sge_u32, ckpt_interval);
         }
      }
   }

   // -clear is not reflected in the job object
   // -cwd is covered by -wd
   // -dc is not reflected in the job object
   job_add_time_opt_to_command_line<JB_deadline>(job, dstr, "-dl");
   job_add_host_str_list_opt_to_command_line<JB_stderr_path_list>(job, dstr, "-e");
   job_add_host_str_list_opt_to_command_line<JB_stdout_path_list>(job, dstr, "-o");
   job_add_host_str_list_opt_to_command_line<JB_stdin_path_list>(job, dstr, "-i");
   job_add_list_opt_to_command_line<JB_jid_request_list, JRE_job_name>(job, dstr, "-hold_jid");
   job_add_list_opt_to_command_line<JB_ja_ad_request_list, JRE_job_name>(job, dstr, "-hold_jid_ad");
   job_add_bool_opt_to_command_line<JB_merge_stderr>(job, dstr, "-j", true);
   job_add_ulong_opt_to_command_line<JB_jobshare>(job, dstr, "-js", 0);
   // -jsv is not reflected in the job object

   uint32_t mailopt = job.get<JB_mail_options>();
   if (mailopt > 0) {
      sge_dstring_sprintf_append(dstr, " -m ");
      sge_dstring_append_mailopt(dstr, mailopt);
   }
   if (const auto mail_list = job.get<JB_mail_list>(); mail_list) {
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, "-M");
      sge_dstring_append_char(dstr, ' ');
      bool first = true;
      for (const auto ep : mail_list) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(dstr, ',');
         }
         sge_dstring_sprintf_append(dstr, "%s@%s", ep.get<MR_user>(), ep.get<MR_host>());
      }
   }

   if (job.get<JB_notify>()) {
      job_add_opt_to_comand_line(dstr, "-notify", nullptr);
   }
   if (JOB_TYPE_IS_IMMEDIATE(job.get<JB_type>())) {
      job_add_opt_to_comand_line(dstr, "-now", "yes");
   }
   job_add_str_opt_to_command_line<JB_job_name>(job, dstr, "-N");
   job_add_str_opt_to_command_line<JB_project>(job, dstr, "-P");
   job_add_ulong_opt_to_command_line<JB_priority>(job, dstr, "-p", -1024);
   if (job_add_str_opt_to_command_line<JB_pe>(job, dstr, "-pe")) {
      sge_dstring_append_char(dstr, ' ');
      DSTRING_STATIC(dstr_pe, 64);
      range_list_print_to_string(job.get<JB_pe_range>(), &dstr_pe, true, false, false);
      sge_dstring_append_dstring(dstr, &dstr_pe);
   }
   uint32_t pty = job.get<JB_pty>();
   if (pty < 2) { // 2 means: do not specify it but use the default for the client
      job_add_opt_to_comand_line(dstr, "-pty", pty == 0 ? "no" : "yes");
   }

   // -sync <options_flags>
   // -sync n which is the default will be silently ignored
   uint32_t sync_options = job.get<JB_sync_options>();
   if (sync_options != SYNC_NO) {
      std::string sync_flags = job_get_sync_options_string(job);
      job_add_opt_to_comand_line(dstr, "-sync", sync_flags.c_str());
   }

   job_add_bool_opt_to_command_line<JB_reserve>(job, dstr, "-R", true);
   if (job.get<JB_restart>() == 2) {
      job_add_opt_to_comand_line(dstr, "-r", "no");
   } else {
      job_add_opt_to_comand_line(dstr, "-r", "yes");
   }
   // the context as the job will have it, also before the qmaster applied the instructions (CS-3058)
   if (!job_get_context_change(job)) {
      job_add_name_value_list_opt_to_command_line<JB_context>(job, dstr, "-sc");
   } else {
      auto context_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      context_job->set<JB_context>(ocs_context_get_effective(job));
      job_add_name_value_list_opt_to_command_line<JB_context>(*context_job, dstr, "-sc");
   }
   job_add_host_str_list_opt_to_command_line<JB_shell_list>(job, dstr, "-shell");
   // -sync is not part of the job - it is only client behaviour
   // -t option
   if (job_is_array(job)) {
      const auto [start, end, step] = job_get_submit_task_ids(job);
      sge_dstring_sprintf_append(dstr, " -t " sge_u32 "-" sge_u32 ":" sge_u32, start, end, step);
   }
   job_add_ulong_opt_to_command_line<JB_ja_task_concurrency>(job, dstr, "-tc", 0);
   job_add_name_value_list_opt_to_command_line<JB_env_list>(job, dstr, "-v", VAR_PREFIX);
   // -w has no effect on the later job execution
   job_add_str_opt_to_command_line<JB_cwd>(job, dstr, "-wd");
   // -@ is not reflected in the job object

   // requests
   job_add_resource_set_list_to_command_line(job, dstr);

   // command line to execute
   job_add_str_opt_to_command_line<JB_script_file>(job, dstr, nullptr);
   for (const auto ep : job.get<JB_job_args>()) {
      sge_dstring_append_char(dstr, ' ');
      job_add_str_to_command_line(dstr, ep.get<ST_name>());
   }

   DRETURN(sge_dstring_get_string(dstr));
}

/**
 * @brief Store the effective command line in the job
 *
 * Generates the effective command line from the job attributes
 * and stores it in the job attribute JB_submission_command_line.
 *
 * @param[in] job      job used as template for the command line (JB)
 * @param[in] client   client name, this will become the first word in the command line
 */
void job_set_command_line(const ocs::tom::Element<ocs::tom::JB> job, const char *client) {
   dstring dstr = DSTRING_INIT;
   job.set<JB_submission_command_line>(job_get_effective_command_line(job, &dstr, client));
   sge_dstring_free(&dstr);
}


/**
 * @brief Store a given command line in the job
 *
 * Formats the command line given as argument count and argument vector into one string
 * and stores it in the job attribute JB_submission_command_line.
 *
 * @param[in] job    we store the result here (JB)
 * @param[in] argc   the argument count
 * @param[in] argv   the argument vector
 */
void
job_set_command_line(const ocs::tom::Element<ocs::tom::JB> job, const int argc, const char *argv[]) {
   dstring dstr = DSTRING_INIT;
   job.set<JB_submission_command_line>(sge_dstring_from_argv(&dstr, argc, argv, true, true));
   sge_dstring_free(&dstr);
}


/**
 * @brief Sets the sync options for a job.
 *
 * @param job the job to change (JB)
 * @param sync_options the `SYNC_*` value to store
 */
void
job_set_sync_options(const ocs::tom::Element<ocs::tom::JB> job, const uint32_t sync_options) {
   job.set<JB_sync_options>(sync_options);
}


/**
 * @brief get the letter combination that represents the sync options
 *
 * @param job the job to read (JB)
 * @return the letter combination
 */
std::string
job_get_sync_options_string(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   uint32_t sync_options = job.get<JB_sync_options>();
   if (sync_options == SYNC_NO) {
      DRETURN("n");
   }

   std::string ret = "";
   if (sync_options == SYNC_JOB_START) {
      ret += "r";
   }
   if (sync_options == SYNC_JOB_END) {
      ret += "x";
   }
   DRETURN(ret);
}

/** @brief Checks if a job should be visible to the current user.
 *
 * @param packet the request, which carries the requesting user on the server
 *               side; nullptr means the client side, where the component
 *               module knows the user
 * @param owner the owner of the job
 * @param is_manager true if the current user is a manager
 * @return true if the job should be visible, false otherwise
 */
bool
job_is_visible(const ocs::gdi::Packet *packet, const char *owner, const bool is_manager) {
   DENTER(BASIS_LAYER);

   // manager can see everything
   if (is_manager) {
      DRETURN(true);
   }

   // On the server side, when executed as stored procedure then we have a packet containing the
   // owner of the request. If executed on the client-side, then the component module contains the user
   // that started the client command.
   const char *username;
   if (packet != nullptr) {
      username = packet->user;
   } else {
      username = component_get_username();
   }

   // if the request owner and the job owner are identical
   if (strcmp(username, owner) == 0) {
      DRETURN(true);
   }

   DRETURN(false);
}

/**
 * @brief Store a POSIX priority and its normalised form
 *
 * `JB_nppri` is the priority mapped onto [0,1] over the whole storable range,
 * which is what the priority formula works with.
 *
 * @param[in,out] jep the job to change (JB)
 * @param priority the priority in its stored unsigned form, see #PRI_ITOU
 */
void job_normalize_priority(const ocs::tom::Element<ocs::tom::JB> jep, const uint32_t priority) {
   constexpr double min_priority = 0.0;
   constexpr double max_priority = 2048.0;

   jep.set<JB_priority>(priority);
   jep.set<JB_nppri>(sge_normalize_value(priority, min_priority, max_priority));
}


/**
 * @brief Summarizes the slots of the given gdil list.
 *
 * The function will return a new list with one entry for each host.
 * The slots of all entries for the same host will be accumulated.
 *
 * @param gdil_list the input list (JG), may be an empty handle
 * @return a new list with summarized entries, an empty handle for an empty input handle
 */
ocs::tom::ListPtr<ocs::tom::JG>
gdil_make_host_unique(const ocs::tom::ConstList<ocs::tom::JG> gdil_list) {
   DENTER(TOP_LAYER);

   // no input -> nothing to do
   if (!gdil_list) {
      DRETURN(nullptr);
   }

   // just one entry -> return a copy of the input
   auto gdil_out = gdil_list.clone("gdil_summarize_hosts");
   if (gdil_out->size() == 1) {
      DRETURN(gdil_out);
   }

   // first gdil that we will use to summarize the slots and the hostname
   auto first_gdil = gdil_out->front();
   const char *first_hostname = first_gdil.get<JG_qhostname>();

   // iterate over the rest of the list and summarize the slots for entries with the same hostname
   auto next_gdil = first_gdil.next();
   while (next_gdil) {
      const auto gdil = next_gdil;
      next_gdil = gdil.next();

      // if the next gdil has a different hostname then it is the first gdil for this new hostname
      // otherwise we summarize the slots of the current gdil into the first gdil
      if (const char *next_gdil_hostname = gdil.get<JG_qhostname>();
          sge_hostcmp(first_hostname, next_gdil_hostname) != 0) {
         first_gdil = gdil;
         first_hostname = next_gdil_hostname;
      } else {
         first_gdil.add<JG_slots>(gdil.get<JG_slots>());
         gdil_out->erase(gdil);
      }
   }

   DRETURN(gdil_out);
}

/**
 * @brief The state bits a task is displayed with
 *
 * `JAT_state` and `JAT_status` overlap and can disagree, so neither alone is
 * what qstat should show. This folds the status into the state and resolves
 * the contradictions - a running task loses the transferring bit, a suspended
 * one loses the running bit, and a finished one is shown as finished rather
 * than exiting.
 *
 * @param job the job the task belongs to (JB)
 * @param jatep the array task (JAT)
 * @return the state bits to render, which are not stored anywhere
 */
uint32_t
jatask_combine_state_and_status_for_output(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                           const ocs::tom::ConstElement<ocs::tom::JAT> jatep) {
   DENTER(TOP_LAYER);

   uint32_t status = jatep.get<JAT_status>();
   uint32_t state = jatep.get<JAT_state>();
   if (status == JRUNNING) {
      state |= JRUNNING;
      state &= ~JTRANSFERING;
   } else if (status == JTRANSFERING) {
      state |= JTRANSFERING;
      state &= ~JRUNNING;
   } else if (status == JFINISHED) {
      /* CS-1908 finished-job retention: retained rows carry JAT_status ==
       * JFINISHED. Pre-retention this transient state rendered as 'x' via
       * JEXITING (see EXITING_SYM). Under retention the row lives on
       * master_job_list for finished_jobs_keep_time seconds, so 'x' is
       * misleading — it means "in the process of exiting", but the JAT
       * has actually already exited and is being kept as history. Emit the
       * dedicated JFINISHED_DISPLAY bit instead so job_get_state_string
       * renders FINISHED_SYM ('f'), matching the `-s f` filter letter.
       * Also clear JEXITING so any stale bit left from the JAT's earlier
       * lifetime doesn't leak an extra 'x' into the state string. */
      state |= JFINISHED_DISPLAY;
      state &= ~(JRUNNING | JTRANSFERING | JEXITING);
   }

   // correct running state if the job is suspended (remove the 'r')
   if (ISSET(state, JSUSPENDED_ON_SUBORDINATE) ||
       ISSET(state, JSUSPENDED) ||
       ISSET(state, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
      state &= ~JRUNNING;
   }

   // show hold state if the job is held or has predecessors
   if (job.get<JB_jid_predecessor_list>() ||
       jatep.get<JAT_hold>()) {
      state |= JHELD;
   }

   // show 'R' state if the job is restarted
   if (jatep.get<JAT_job_restarted>()) {
      state &= ~JWAITING;
      state |= JMIGRATING;
   }
   DRETURN(state);
}


/**
 * @brief Parse when string for job alteration
 *
 * Parses a string to determine when a job alteration should occur.
 * Valid values are "on_reschedule" and "now". If an invalid value is
 * provided, the function defaults to QALTER_WHEN_ON_RESCHEDULE and
 * returns false.
 *
 * @param input string to parse ("on_reschedule" or "now")
 * @param when reference to store the parsed value
 * @return true if input was valid, false otherwise
 */
bool job_parse_when_string(const char *input, qalter_when_t &when) {
   bool ret = true;

   if (sge_strnullcasecmp(input, "on_reschedule") == 0) {
      when = QALTER_WHEN_ON_RESCHEDULE;
   } else if (sge_strnullcasecmp(input, "now") == 0) {
      when = QALTER_WHEN_NOW;
   } else {
      when = QALTER_WHEN_ON_RESCHEDULE;
      ret = false;
   }

   return ret;
}

/**
 * @brief Check if a job has "when now" attribute set
 *
 * Checks whether the job has the JB_when attribute set to
 * QALTER_WHEN_NOW, indicating that job alterations should be
 * applied immediately.
 * The job can be a reduced element as it is sent by the qalter command.
 *
 * @param job JB element, full or reduced
 * @return true if JB_when is set to QALTER_WHEN_NOW, false otherwise
 */
bool
job_is_when_now(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   bool ret = false;
   if (job.has<JB_when>()) {
      ret = job.get<JB_when>() == QALTER_WHEN_NOW;
   }
   return ret;
}

