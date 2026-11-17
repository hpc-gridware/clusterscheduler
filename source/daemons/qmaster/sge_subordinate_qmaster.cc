/*___INFO__MARK_BEGIN__*/
/************************************************************************
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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Subordinate queues, including the slot-wise suspension tree
 */

#include <cstring>

#include "uti/sge_bitfield.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_sl.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_ja_task.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "tom/ocs_List.h"

#include "evm/sge_event_master.h"
#include "sge_qmod_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "msg_qmaster.h"

static bool
qinstance_x_on_subordinate(ocs::tom::Element<ocs::tom::QU> this_elem, bool suspend,
                           bool send_event, monitoring_t *monitor, uint64_t gdi_session);


/**
 * @brief Retrieves the "threshold" value of the slotwise sos configuration
 *
 * Retrieves the "threshold" value of the slotwise sos configuration. This
 * is the value after the "slots=" keyword.
 *
 * @param qinstance The qinstance from which the threshold is to be retrieved.
 *
 * @return The "threshold" value. 0 if no slotwise suspend on subordinate is defined.
 *
 * @note MT-NOTE: get_slotwise_sos_threshold() is MT safe
 */
static uint32_t
get_slotwise_sos_threshold(const ocs::tom::ConstElement<ocs::tom::QU> qinstance) {
   uint32_t slots_sum = 0;

   if (qinstance) {
      ocs::tom::ConstList<ocs::tom::SO> so_list = ocs::tom::get<QU_subordinate_list>(qinstance);
      if (so_list != nullptr) {
         ocs::tom::ConstElement<ocs::tom::SO> so = so_list.front();
         if (so != nullptr) {
            slots_sum = ocs::tom::get<SO_slots_sum>(so);
         }
      }
   }
   return slots_sum;
}

/**
 * @brief Execute the (un)suspend
 *
 * Executes the (un)suspend of the specified task.
 *
 * @param qinstance_where_task_is_running QU_Type Element of the qinstance in which the task to (un)suspend is running/suspended.
 * @param job_id Job ID of the task to (un)suspend
 * @param task_id Task ID of the task to (un)suspend
 * @param suspend suspend or unsuspend
 * @param monitor monitor
 *
 * @return true:  Task is (un)suspended. false: An error occcurred.
 *
 * @note MT-NOTE: slotwise_x_on_subordinate() is not MT safe, the global lock
 *       must be set outside before calling this function
 */
static bool
slotwise_x_on_subordinate(const ocs::tom::Element<ocs::tom::QU> qinstance_where_task_is_running,
                          const uint32_t job_id, const uint32_t task_id, const bool suspend, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool ret = false;
   uint32_t state = 0;
   const ocs::tom::ConstList<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list(SGE_TYPE_JOB));

   ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_id);
   if (jep != nullptr) {
      ocs::tom::Element<ocs::tom::JAT> jatep = ocs::tom::find_first_in_rw<JB_ja_tasks, JAT_task_number>(jep, task_id);
      if (jatep != nullptr) {
         sge_signal_queue(suspend ? SGE_SIGSTOP : SGE_SIGCONT,
                          qinstance_where_task_is_running, jep, jatep, monitor);
         /* Set status */
         state = ocs::tom::get<JAT_state>(jatep);

         if (suspend) {
            SETBIT(JSUSPENDED_ON_SLOTWISE_SUBORDINATE, state);
            DPRINTF("Setting status JSUSPENDED_ON_SLOTWISE_SUBORDINATE for job %lu.%lu\n", job_id, task_id);
         } else {
            CLEARBIT(JSUSPENDED_ON_SLOTWISE_SUBORDINATE, state);
            DPRINTF("Clearing status JSUSPENDED_ON_SLOTWISE_SUBORDINATE for job %lu.%lu\n", job_id, task_id);
         }
         ocs::tom::set<JAT_state>(jatep, state);
         sge_add_event(0, sgeE_JATASK_MOD, job_id, task_id, nullptr, nullptr,
                       nullptr, jatep, 0);
         ret = true;
      } else {
         /* TODO: HP: Add error handling! */
      }
   } else {
      /* TODO: HP: Add error handling! */
   }
   DRETURN(ret);
}

/**
 * @brief Gets the root qinstance of the slotwise
 *
 * Returns the qinstance that is the root of the slotwise suspend on
 * subordinate tree where the provided qinstance is a member of.
 * Returns nullptr if the give qinstance is not a member of a slotwise suspend
 * on subordinate tree.
 *
 * @param node_queue_instance For this queue instance the slotwise suspend on subordinate tree root is searched.
 *
 * @return The root node of the slotwise suspend on subordinate tree, node_queue_instance if it is the root node, or nullptr if node_queue_instance is not part of any slotwise suspend on subordinate definition.
 *
 * @note MT-NOTE: get_slotwise_sos_tree_root() is not MT safe
 */
/* TODO: HP: Use get_slotwise_super_qinstance() recursively instead of this function */
static ocs::tom::Element<ocs::tom::QU>
get_slotwise_sos_tree_root(const ocs::tom::Element<ocs::tom::QU> node_queue_instance) {
   DENTER(TOP_LAYER);

   const char *node_queue_name = nullptr;
   const char *node_host_name = nullptr;
   ocs::tom::Element<ocs::tom::QU> root_qinstance = nullptr;
   const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE));

   if (node_queue_instance) {
      if (get_slotwise_sos_threshold(node_queue_instance) > 0) {
         /* For now, assume this qinstance is the root node of the slotwise sos tree. */
         root_qinstance = node_queue_instance;
      }

      /* Search the superordinated queue instance of our node_queue_instance.
       * Then search the superordinated queue instance of this superordinated
       * queue instance, and so on.
       * We have to search in queue instances, not in queues, because possibly
       * for our current host there is no superordinated queue instance of our
       * node_queue_instance, while there might be one on another host.
       */
      node_queue_name = ocs::tom::get<QU_qname>(node_queue_instance);
      node_host_name = ocs::tom::get<QU_qhostname>(node_queue_instance);

      for (const auto cqueue : master_cqueue_list) {
         ocs::tom::Element<ocs::tom::QU> qinstance = cqueue_locate_qinstance(cqueue, node_host_name);

         if (qinstance == nullptr) {
            /* There is no instance of this cluster queue on this host. Continue
             * with another branch of the tree.
             */
            continue;
         }

         ocs::tom::Element<ocs::tom::SO> sub = ocs::tom::find_first_in_rw<QU_subordinate_list, SO_name>(qinstance, node_queue_name);
         if (sub != nullptr && ocs::tom::get<SO_slots_sum>(sub) != 0) {
            /* Our node queue is mentioned in the subordinate_list of
             * this queue. This queue is our superordinated queue,
             * i.e. the parent in the slotwise sos tree!
             * Now we can look for the parent of our parent.
             */
            root_qinstance = get_slotwise_sos_tree_root(qinstance);
         }
      }
   }
   DRETURN(root_qinstance);
}

/**
 * @brief Get the superordinate of the
 *
 * Returns the slotwise superordinated queue instance of the queue instance
 * with the provided name.
 *
 * @param queue_name cluster queue name of the subordinated qeueue instance
 * @param hostname host name of the subordinated queue instance
 *
 * @return The slotwise superordinated queue instance (QU_Type) of the provided queue instance, or nullptr if there is none.
 *
 * @note MT-NOTE: get_slotwise_suspend_superordinate() is not MT safe
 */
static ocs::tom::Element<ocs::tom::QU>
get_slotwise_suspend_superordinate(const char *queue_name, const char *hostname) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::QU> super_qinstance = nullptr;
   const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE));

   for (const auto cqueue : master_cqueue_list) {
      ocs::tom::Element<ocs::tom::QU> qinstance = cqueue_locate_qinstance(cqueue, hostname);

      if (qinstance != nullptr) {
         if (get_slotwise_sos_threshold(qinstance) > 0) {
            ocs::tom::Element<ocs::tom::SO> so = ocs::tom::find_first_in_rw<QU_subordinate_list, SO_name>(qinstance, queue_name);
            if (so != nullptr && ocs::tom::get<SO_slots_sum>(so) != 0) {
               /* the queue_name is listed in the subordinate list of this
                * queue instance, so it's our superordinated queue instance.
                */
               super_qinstance = qinstance;
               break;
            }
         }
      }
   }
   DRETURN(super_qinstance);
}

/**
 * @brief Get the superordinate of the
 *
 * Returns the slotwise superordinated queue instance of the provided queue
 * instance.
 *
 * @param qinstance The subordinated queue instance (QU_Type)
 *
 * @return The slotwise superordinated queue instance (QU_Type) of the provided queue instance, or nullptr if there is none.
 *
 * @note MT-NOTE: get_slotwise_sos_super_qinstance() is not MT safe
 */
static ocs::tom::Element<ocs::tom::QU>
get_slotwise_sos_super_qinstance(const ocs::tom::ConstElement<ocs::tom::QU> qinstance) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::QU> super_qinstance = nullptr;
   const char *qinstance_name = nullptr;
   const char *qinstance_host = nullptr;

   if (qinstance) {
      qinstance_name = ocs::tom::get<QU_qname>(qinstance);
      qinstance_host = ocs::tom::get<QU_qhostname>(qinstance);
      super_qinstance = get_slotwise_suspend_superordinate(qinstance_name, qinstance_host);
   }
   DRETURN(super_qinstance);
}

/** @brief One running task that slot-wise suspension may act on */
struct ssos_task_t {
   uint32_t job_id = 0;                    ///< The job the task belongs to
   ocs::tom::Element<ocs::tom::JAT> task;  ///< the task
};

/** @brief One queue instance in the slot-wise suspension tree
 *
 * Slot-wise subordination is a *tree*, not a pair: a queue instance may be
 * subordinate to one queue and superordinate to another, so suspending one
 * slot can cascade downwards. #depth is how far down this instance sits, which
 * decides the order the tree is walked in.
 */
struct ssos_qinstance_t {
   uint32_t depth = 0;                         ///< How deep in the subordination tree this instance sits
   uint32_t seq_no = 0;                        ///< from the parents QU_subordinate_list
   uint32_t action = 0;                        ///< from the parents QU_subordinate_list
   ocs::tom::Element<ocs::tom::QU> qinstance;  ///< the queue instance
   ocs::tom::Element<ocs::tom::QU> parent;     ///< the superordinate instance, empty at the root
   sge_sl_list_t *tasks = nullptr;             ///< The tasks running here, as #ssos_task_t
};

/**
 * @brief Destructor for the elements of a sge simple list of ssos_task_t elements
 *
 * Destroys the data members of sge simple list of ssos_task_t elements.
 * The element was allocated with new, since it holds a typed handle.
 *
 * @param ssos_task The ssos_task_t element to destroy, set to nullptr
 *
 * @return Always true to continue the destruction of all list elements.
 *
 * @note MT-NOTE: destroy_slotwise_sos_task_elem() is MT safe
 */
static bool
destroy_slotwise_sos_task_elem(ssos_task_t **ssos_task) {
   DENTER(TOP_LAYER);
   if (ssos_task != nullptr && *ssos_task != nullptr) {
      delete *ssos_task;
      *ssos_task = nullptr;
   }
   DRETURN(true);
}

/**
 * @brief Destructor for the elements of a sge simple list of ssos_qinstance_t elements
 *
 * Destroys the data members of a sge simple list of ssos_qinstance_t
 * elements. Takes care of the destruction of the sge simple list sublist of
 * ssos_task_t members. The element was allocated with new, since it holds
 * typed handles.
 *
 * @param ssos_qinstance The ssos_qinstance_t element to destroy, set to nullptr
 *
 * @return Always true to continue the destruction of all list elements.
 *
 * @note MT-NOTE: destroy_slotwise_sos_tree_elem() is MT safe
 */
static bool
destroy_slotwise_sos_tree_elem(ssos_qinstance_t **ssos_qinstance) {
   DENTER(TOP_LAYER);
   if (ssos_qinstance != nullptr && *ssos_qinstance != nullptr) {
      if ((*ssos_qinstance)->tasks != nullptr) {
         sge_sl_destroy(&((*ssos_qinstance)->tasks), (sge_sl_destroy_f) destroy_slotwise_sos_task_elem);
      }
      delete *ssos_qinstance;
      *ssos_qinstance = nullptr;
   }

   DRETURN(true);
}

/**
 * @brief Checks if a task is suspended by slotwise subordination
 *
 * If only_ssos is true, this function checks if a task is suspended by
 * slotwise subordination and not suspended otherwise at the same time, too.
 * If only_ssos is false, this function checks if a task is suspended by
 * slotwise subordination. This task may additionally be suspended by
 * another suspend method.
 * A task that is suspended by slotwise subordination could also be suspended
 * manually, or by threshold, or by queue wise subordination or by calendar,
 * too.
 *
 * @param only_ssos If true, checks if task is only suspended by slotwise suspend on subordinate, if false, checks if task is suspended by slotwise suspend on subordinate, but it might be also suspended by some other suspend method at the same time.
 * @param task The task to check. CULL type "JAT_Type".
 *
 * @return true if the task is (only) suspended by slotwise suspend on subordinate. false else.
 *
 * @note MT-NOTE: is_ssos() is MT safe
 */
static bool
is_ssos(const bool only_ssos, const ocs::tom::ConstElement<ocs::tom::JAT> task) {
   bool ret = false;
   uint32_t state = 0;

   if (task) {
      state = ocs::tom::get<JAT_state>(task);
      if (only_ssos) {
         ret = (bool) (!ISSET(state, JSUSPENDED) &&
                       !ISSET(state, JSUSPENDED_ON_THRESHOLD) &&
                       !ISSET(state, JSUSPENDED_ON_SUBORDINATE) &&
                       ISSET(state, JSUSPENDED_ON_SLOTWISE_SUBORDINATE));
      } else {
         ret = (bool) ISSET(state, JSUSPENDED_ON_SLOTWISE_SUBORDINATE);
      }
   }
   return ret;
}

/**
 * @brief Checks if a qinstance has tasks that are suspended
 *
 * If only_ssos is true, this function checks if this queue has at least one
 * task that is suspended by slotwise subordination and not suspended
 * otherwise at the same time, too.
 * If only_ssos is false, this function checks if this queue has at least one
 * task that is suspended by slotwise subordination. This task may
 * additionally by suspended by some other suspend method.
 * A task that is suspended by slotwise subordination could also be suspended
 * manually, or by threshold, or by queue wise subordination or by calendar,
 * too.
 *
 * @param only_ssos If true, checks if task is only suspended by ssos, if false, checks if task is suspended by ssos, but it might also be suspended by other methods at the same time.
 * @param ssos_qinstance The qinstance to check.
 *
 * @return true if the qinstance has at least one task that is suspended by slotwise suspend on subordinate (only). false if there is no such task.
 *
 * @note MT-NOTE: has_ssos_task() is MT safe
 *
 * @see #is_ssos
 */
static bool
has_ssos_task(bool only_ssos_task, ssos_qinstance_t *ssos_qinstance) {
   bool ret = false;
   sge_sl_elem_t *ssos_task_elem = nullptr;
   ssos_task_t *ssos_task = nullptr;

   for_each_sl(ssos_task_elem, ssos_qinstance->tasks) {
      ssos_task = (ssos_task_t *) sge_sl_elem_data(ssos_task_elem);
      ret = is_ssos(only_ssos_task, ssos_task->task);
      if (ret) {
         break;
      }
   }
   return ret;
}

/**
 * @brief Searches the slotwise subordinate tree in a
 *
 * Searches in the provided slotwise subordination tree in a specific depth
 * for a task to (un)suspend.
 *
 * @param slotwise_sos_tree_qinstances The slotwise suspend on subordinate tree as a list
 * @param depth The depth in this tree where the task is to be searched
 * @param suspend Are we going to suspend or unsuspend a task?
 * @param only_slotwise_suspended Are we looking for tasks to unsuspend that are only slotwise suspended, or are we looking for tasks that are also suspended manually, by threshold or by queue wise subordination? Ignored if suspend is true.
 * @param ssos_qinstance_to_x The queue instance where the found task is running
 * @param ssos_task_to_x The task we found
 *
 * @note MT-NOTE: get_task_to_x_in_depth() is MT safe
 */
static void
get_task_to_x_in_depth(sge_sl_list_t *slotwise_sos_tree_qinstances, uint32_t depth, bool suspend,
                       bool only_slotwise_suspended, ssos_qinstance_t **ssos_qinstance_to_x,
                       ssos_task_t **ssos_task_to_x) {
   sge_sl_elem_t *ssos_tree_elem = nullptr;
   uint32_t extreme_seq_no = suspend ? 0 : (uint32_t) -1;
   uint64_t oldest_start_time = (uint64_t) -1;
   uint64_t youngest_start_time = 0;

   for_each_sl(ssos_tree_elem, slotwise_sos_tree_qinstances) {
      ssos_qinstance_t *ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(ssos_tree_elem);

      /* Get highest/lowest seq_no of queues with running/suspended tasks
       * jobs among all queues of current depth
       */
      if (ssos_qinstance->depth == depth &&
          ssos_qinstance->tasks != nullptr &&
          ((suspend && ssos_qinstance->seq_no > extreme_seq_no) ||
           (!suspend && ssos_qinstance->seq_no < extreme_seq_no &&
            has_ssos_task(only_slotwise_suspended, ssos_qinstance)))) {
         extreme_seq_no = ssos_qinstance->seq_no;
      }
   }

   for_each_sl(ssos_tree_elem, slotwise_sos_tree_qinstances) {
      ssos_qinstance_t *ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(ssos_tree_elem);


      /* Search in all queues in current depth, with highest/lowest seq_no
       * and with running/suspended tasks for the oldest/youngest task
       */
      if (ssos_qinstance->depth == depth &&
          ssos_qinstance->seq_no == extreme_seq_no) {
         sge_sl_elem_t *ssos_task_elem = nullptr;
         bool oldest = (bool) (ssos_qinstance->action == SO_ACTION_LR);

         /* If we have to unsuspend and if we would look for the youngest job
          * for suspend, we have to look for the oldest job to unsuspend.
          */
         if (!suspend) {
            oldest = (bool) !oldest;
         }

         for_each_sl(ssos_task_elem, ssos_qinstance->tasks) {
            uint64_t start_time = 0;
            ssos_task_t *ssos_task = (ssos_task_t *) sge_sl_elem_data(ssos_task_elem);

            if (suspend ||
                (!suspend && is_ssos(only_slotwise_suspended, ssos_task->task))) {
               start_time = ocs::tom::get<JAT_start_time>(ssos_task->task);
               if (oldest && start_time < oldest_start_time) {
                  oldest_start_time = start_time;
                  *ssos_task_to_x = ssos_task;
                  *ssos_qinstance_to_x = ssos_qinstance;
               }
               if (!oldest && start_time > youngest_start_time) {
                  youngest_start_time = start_time;
                  *ssos_task_to_x = ssos_task;
                  *ssos_qinstance_to_x = ssos_qinstance;
               }
            }
         }
      }
   }
}

/**
 * @brief Removes a task from the slotwise
 *
 * Removes a specific task from the slotwiese suspend on subordinate tree
 * list. For this, it searches the task in all queue instances of the
 * list.
 *
 * @param slotwise_sos_tree_qinstances The slotwise suspend on subordinate tree as a list
 * @param job_id The job ID of the task
 * @param task_id The task ID of the task
 *
 * @note MT-NOTE: remove_task_from_slotwise_sos_tree() is not MT safe
 */
static void
remove_task_from_slotwise_sos_tree(sge_sl_list_t *slotwise_sos_tree_qinstances, uint32_t job_id, uint32_t task_id) {
   sge_sl_elem_t *ssos_tree_elem = nullptr;
   sge_sl_elem_t *ssos_task_elem = nullptr;
   ssos_qinstance_t *ssos_qinstance = nullptr;
   ssos_task_t *ssos_task = nullptr;

   for_each_sl(ssos_tree_elem, slotwise_sos_tree_qinstances) {
      ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(ssos_tree_elem);
      for_each_sl(ssos_task_elem, ssos_qinstance->tasks) {
         ssos_task = (ssos_task_t *) sge_sl_elem_data(ssos_task_elem);
         if (ssos_task->job_id == job_id &&
             ocs::tom::get<JAT_task_number>(ssos_task->task) == task_id) {
            sge_sl_dechain(ssos_qinstance->tasks, ssos_task_elem);
            if (sge_sl_get_elem_count(ssos_qinstance->tasks) == 0) {
               sge_sl_destroy(&(ssos_qinstance->tasks), nullptr);
            }
            sge_sl_elem_destroy(&ssos_task_elem, (sge_sl_destroy_f) destroy_slotwise_sos_task_elem);
         }
      }
   }
}

/**
 * @brief (Un)suspend the one task of the slotwise subordination tree that is to go next
 *
 * For suspend, the deepest queue instances are searched first; for unsuspend the
 * shallowest ones, first for tasks suspended only by slotwise subordination, then
 * for tasks suspended in other ways too.
 *
 * @param slotwise_sos_tree_qinstances the slotwise suspend on subordinate tree as a list
 * @param suspend true to suspend a task, false to unsuspend one
 * @param monitor for monitoring qmaster threads
 * @return true if a task was (un)suspended
 *
 * @note MT-NOTE: x_most_extreme_task() is not MT safe
 */
static bool
x_most_extreme_task(sge_sl_list_t *slotwise_sos_tree_qinstances, const bool suspend, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   bool suspended_a_task = false;
   uint32_t depth = 0;
   uint32_t i;
   sge_sl_elem_t *ssos_tree_elem = nullptr;
   ssos_qinstance_t *ssos_qinstance = nullptr;
   ssos_task_t *ssos_task = nullptr;

   /* Walk over the whole list and find biggest depth */
   for_each_sl(ssos_tree_elem, slotwise_sos_tree_qinstances) {
      ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(ssos_tree_elem);
      depth = MAX(ssos_qinstance->depth, depth);
   }

   ssos_qinstance = nullptr;
   if (suspend) {
      /* Walk over list, get oldest (youngest) job from qinstances of biggest depth */
      /* If there was no running job in one of the qinstances of biggest depth, repeat
       * with qinstances of max_depth-1, and so on.
       */
      for (i = depth; i > 0 && ssos_task == nullptr; i--) {
         /* find youngest running task to suspend */
         get_task_to_x_in_depth(slotwise_sos_tree_qinstances, i, suspend, true,
                                &ssos_qinstance, &ssos_task);
      }
   } else {
      /* First we look for the best task to unsuspend that is only slotwise
       * suspended and not manually, by threshold or by queue wise subordination.
       * If there is no such task, we look for the best task that is also
       * otherwise suspended to remove the JSUSPENDED_ON_SLOTWISE_SUBORDINATE
       * flag from this task, so it might be unsuspended as soon as the other
       * suspends are removed.
       */
/* TODO: HP: Optimize this: The first loop could also detect and store a
 *           candidate for the second case, so the second loop could be
 *           removed.
 */
      for (i = 1; i <= depth && ssos_task == nullptr; i++) {
         /* find oldest only slotwise suspended task to unsuspend */
         get_task_to_x_in_depth(slotwise_sos_tree_qinstances, i, suspend, true,
                                &ssos_qinstance, &ssos_task);
      }
      if (ssos_task == nullptr) {
         for (i = 1; i <= depth && ssos_task == nullptr; i++) {
            /* find oldest also otherwise suspended task to unsuspend */
            get_task_to_x_in_depth(slotwise_sos_tree_qinstances, i, suspend, false,
                                   &ssos_qinstance, &ssos_task);
         }
      }
   }

   if (ssos_task != nullptr && ssos_qinstance != nullptr) {
      /* (un)suspend this task */
      suspended_a_task = slotwise_x_on_subordinate(ssos_qinstance->qinstance,
                                                   ssos_task->job_id, ocs::tom::get<JAT_task_number>(ssos_task->task),
                                                   suspend, monitor);
      if (suspended_a_task) {
         remove_task_from_slotwise_sos_tree(slotwise_sos_tree_qinstances, ssos_task->job_id,
                                            ocs::tom::get<JAT_task_number>(ssos_task->task));
      }
   }
   DRETURN(suspended_a_task);
}

/**
 * @brief Collect the queue instances of a slotwise subordination (sub) tree
 *
 * Walks the slotwise subordinates of @p qinstance recursively and appends one
 * ssos_qinstance_t per queue instance to @p tree_qinstances. At depth 0 the
 * list is created and @p qinstance itself is added as the root.
 *
 * @param qinstance the queue instance whose subtree is collected
 * @param tree_qinstances the list of ssos_qinstance_t; must point to nullptr at depth 0
 * @param depth the depth of @p qinstance in the tree
 *
 * @note MT-NOTE: get_slotwise_sos_sub_tree_qinstances() is not MT safe
 */
static void
get_slotwise_sos_sub_tree_qinstances(const ocs::tom::Element<ocs::tom::QU> qinstance,
                                     sge_sl_list_t **tree_qinstances, uint32_t depth) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::QU> sub_qinstance = nullptr;
   const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE));

   /* get all qinstances in the slotwise sos tree excluding the super qinstance
    * (i.e. the root node of the tree), it was already added in the iteration before.
    */
   if (depth == 0) {
      ssos_qinstance_t *ssos_qinstance = nullptr;

      if (tree_qinstances == nullptr || *tree_qinstances != nullptr) {
         DRETURN_VOID;
      }

      /* special handling for the root node of the tree */
      sge_sl_create(tree_qinstances);

      /* first add the super qinstance to the list */
      ssos_qinstance = new ssos_qinstance_t;
      ssos_qinstance->seq_no = 0;    /* the super qinstance has always top priority */
      ssos_qinstance->depth = 0;    /* the super qinstance is on top */
      ssos_qinstance->action = 0;    /* the super qinstances tasks don't get modified */
      ssos_qinstance->qinstance = qinstance;
      ssos_qinstance->parent = nullptr; /* the super qinstance has no parent */
      ssos_qinstance->tasks = nullptr; /* gets filled later */

      sge_sl_insert(*tree_qinstances, ssos_qinstance, SGE_SL_FORWARD);
      depth++;
   }

   ocs::tom::ConstList<ocs::tom::SO> so_list = ocs::tom::get<QU_subordinate_list>(qinstance);
   for (const auto so : so_list) {
      const char *so_name = nullptr;
      const char *so_full_name = nullptr;
      dstring dstr_so_full_name = DSTRING_INIT;
      ssos_qinstance_t *ssos_qinstance = nullptr;

      /* if it is slotwise subordinated, get the pointer to this qinstance list elem */
      if (ocs::tom::get<SO_slots_sum>(so) > 0) {
         so_name = ocs::tom::get<SO_name>(so);
         if (strstr(so_name, "@") == nullptr) {
            const char *host_name = nullptr;

            host_name = ocs::tom::get<QU_qhostname>(qinstance);
            sge_dstring_sprintf(&dstr_so_full_name, "%s@%s", so_name, host_name);
            so_full_name = sge_dstring_get_string(&dstr_so_full_name);
         } else {
            so_full_name = so_name;
         }
         sub_qinstance = cqueue_list_locate_qinstance(master_cqueue_list, so_full_name);
         sge_dstring_free(&dstr_so_full_name);

         if (sub_qinstance != nullptr) {
            ssos_qinstance = new ssos_qinstance_t;
            ssos_qinstance->seq_no = ocs::tom::get<SO_seq_no>(so);
            ssos_qinstance->action = ocs::tom::get<SO_action>(so);
            ssos_qinstance->depth = depth;
            ssos_qinstance->qinstance = sub_qinstance;
            ssos_qinstance->parent = qinstance;
            ssos_qinstance->tasks = nullptr; /* gets filled later */

            sge_sl_insert(*tree_qinstances, ssos_qinstance, SGE_SL_FORWARD);

            get_slotwise_sos_sub_tree_qinstances(sub_qinstance, tree_qinstances, depth + 1);
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Count the running tasks of a slotwise subordination tree and remember them
 *
 * Walks the job list and counts the tasks running unsuspended in the queue
 * instances of the tree. For suspend, they are stored in the tree list; for
 * unsuspend, the tasks suspended by slotwise subordination are stored instead.
 *
 * @param qinstances_in_slotwise_sos_tree the tree as a list of ssos_qinstance_t
 * @param suspend whether tasks to suspend (true) or to unsuspend (false) are collected
 * @return the number of running, unsuspended tasks in the tree
 *
 * @note MT-NOTE: count_running_jobs_in_slotwise_sos_tree() is not MT safe
 */
static uint32_t
count_running_jobs_in_slotwise_sos_tree(sge_sl_list_t *qinstances_in_slotwise_sos_tree, const bool suspend) {
   DENTER(TOP_LAYER);

   /* Walk over job list and get the tasks that are running in the qinstances
    * of the slotwise sos tree. Store informations about these tasks in the tree list.
    * The tasks are stored as modifiable handles, hence the list is taken for writing.
    */
   const ocs::tom::List<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB, true));
   uint32_t sum = 0;

   if (qinstances_in_slotwise_sos_tree != nullptr &&
       sge_sl_get_elem_count(qinstances_in_slotwise_sos_tree) > 0) {
      const char *host_name = nullptr;
      ssos_qinstance_t *first_qinstance = nullptr;

      sge_sl_data(qinstances_in_slotwise_sos_tree, (void **) &first_qinstance, SGE_SL_FORWARD);
      host_name = ocs::tom::get<QU_qhostname>(first_qinstance->qinstance);

      for (const auto job : master_job_list) {
         for (const auto task : ocs::tom::get<JB_ja_tasks>(job)) {
            uint32_t state = 0;
            const void *iterator = nullptr;

            ocs::tom::ConstList<ocs::tom::JG> task_gdi_list = ocs::tom::get<JAT_granted_destin_identifier_list>(task);
            /* Get all destination identifiers of the current task that are on the
             * current host using lGetElemHostFirst()/lGetElemHostNext().
             */
            ocs::tom::ConstElement<ocs::tom::JG> task_gdi = ocs::tom::find_first<JG_qhostname>(task_gdi_list, host_name, &iterator);
            while (task_gdi != nullptr) {
               const char *qinstance_name = nullptr;
               const char *task_gdi_qname = nullptr;
               uint32_t status = 0;
               sge_sl_elem_t *sl_elem = nullptr;

               /* Count all tasks in state JRUNNING and store tasks to suspend. */
               state = ocs::tom::get<JAT_state>(task);
               status = ocs::tom::get<JAT_status>(task);

               if (ISSET(state, JRUNNING) &&
                   !ISSET(state, JSUSPENDED) &&
                   !ISSET(state, JSUSPENDED_ON_THRESHOLD) &&
                   !ISSET(state, JSUSPENDED_ON_SUBORDINATE) &&
                   !ISSET(state, JSUSPENDED_ON_SLOTWISE_SUBORDINATE) &&
                   !ISSET(state, JDELETED) &&
                   !ISSET(status, JEXITING) &&
                   !ISSET(status, JFINISHED)) {
                  /* The current task is in state JRUNNING and not suspended in
                   * any way. 
                   * Check if the qinstance name where the current task is
                   * running is in the list of qinstances in the slotwise sos
                   * tree.
                   */
                  task_gdi_qname = ocs::tom::get<JG_qname>(task_gdi);
                  for_each_sl(sl_elem, qinstances_in_slotwise_sos_tree) {
                     ssos_qinstance_t *ssos_qinstance = nullptr;

                     ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(sl_elem);
                     qinstance_name = ocs::tom::get<QU_full_name>(ssos_qinstance->qinstance);
                     if (strcmp(task_gdi_qname, qinstance_name) == 0) {
                        /* The qinstance of the current task is in our ssos tree, now
                         * we can count this task.
                         */
                        sum++;
                        if (suspend) {
                           /* Store the running tasks in our ssos tree list. */
                           auto *ssos_task = new ssos_task_t;

                           if (ssos_qinstance->tasks == nullptr) {
                              sge_sl_create(&(ssos_qinstance->tasks));
                           }

                           ssos_task->job_id = ocs::tom::get<JB_job_number>(job);
                           ssos_task->task = task;
                           sge_sl_insert(ssos_qinstance->tasks, ssos_task, SGE_SL_FORWARD);
                           break;
                        }
                     }
                  }
               } else if (!suspend &&
                          ISSET(state, JSUSPENDED_ON_SLOTWISE_SUBORDINATE) &&
                          !ISSET(state, JDELETED) &&
                          !ISSET(status, JEXITING)) {

                  /* We have to remember all tasks that are slotwise suspended,
                   * even if they are also manually or by threshold or queue
                   * wise subordination suspended, because it might be that
                   * we don't find a task to unsuspend but can remove the
                   * JSUSPENDED_ON_SLOTWISE_SUBORDINATE flag from a doubly
                   * suspended task.
                   */
                  /* Check if the qinstance where the task is slotwise suspended is
                   * in the slotwise sos tree list.
                   */
                  task_gdi_qname = ocs::tom::get<JG_qname>(task_gdi);
                  for_each_sl(sl_elem, qinstances_in_slotwise_sos_tree) {
                     ssos_qinstance_t *ssos_qinstance = nullptr;

                     ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(sl_elem);
                     qinstance_name = ocs::tom::get<QU_full_name>(ssos_qinstance->qinstance);
                     if (strcmp(task_gdi_qname, qinstance_name) == 0) {
                        /* The qinstance of the current task is in our ssos tree, so
                         * we can store this task in our ssos tree list.
                         */
                        auto *ssos_task = new ssos_task_t;
                        if (ssos_qinstance->tasks == nullptr) {
                           sge_sl_create(&(ssos_qinstance->tasks));
                        }

                        ssos_task->job_id = ocs::tom::get<JB_job_number>(job);
                        ssos_task->task = task;
                        sge_sl_insert(ssos_qinstance->tasks, ssos_task, SGE_SL_FORWARD);
                        break;
                     }
                  }
               }
               task_gdi = ocs::tom::find_next<JG_qhostname>(task_gdi_list, host_name, &iterator);
            }
         }
      }
   }
   DRETURN(sum);
}

/**
 * @brief Unsuspends all slotwise suspended tasks in a subtree
 *
 * Unsuspends all slotwise suspended tasks in the subtree of the provided
 * qinstance.
 *
 * @param qinstance The root of the slotwise preemption sub tree.
 * @param monitor monitor
 *
 * @note MT-NOTE: unsuspend_all_tasks_in_slotwise_sub_tree() is MT safe
 */
void
unsuspend_all_tasks_in_slotwise_sub_tree(const ocs::tom::Element<ocs::tom::QU> qinstance, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const bool suspend = false;
   sge_sl_list_t *qinstances_in_slotwise_sos_tree = nullptr;
   sge_sl_elem_t *ssos_qinstance_elem = nullptr;
   ssos_qinstance_t *ssos_qinstance = nullptr;
   sge_sl_elem_t *ssos_task_elem = nullptr;
   ssos_task_t *ssos_task = nullptr;

   if (qinstance) {
      /* get the slotwise sos sub tree of this qinstance as a list */
      get_slotwise_sos_sub_tree_qinstances(qinstance, &qinstances_in_slotwise_sos_tree, 0);

      /* fill the tree list with all slotwise suspended tasks */
      count_running_jobs_in_slotwise_sos_tree(qinstances_in_slotwise_sos_tree, suspend);

      for_each_sl(ssos_qinstance_elem, qinstances_in_slotwise_sos_tree) {
         ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(ssos_qinstance_elem);
         for_each_sl(ssos_task_elem, ssos_qinstance->tasks) {
            ssos_task = (ssos_task_t *) sge_sl_elem_data(ssos_task_elem);
            slotwise_x_on_subordinate(ssos_qinstance->qinstance, ssos_task->job_id,
                                      ocs::tom::get<JAT_task_number>(ssos_task->task), suspend, monitor);
         }
      }
      sge_sl_destroy(&qinstances_in_slotwise_sos_tree, (sge_sl_destroy_f) destroy_slotwise_sos_tree_elem);
   }
   DRETURN_VOID;
}

/**
 * @brief Checks if the new slotwise subordinate configuration is valid
 *
 * Checks if the slotwise preemption configuration would still be valid if
 * a specific change would be made.
 *
 * @param qinstance For this qinstance, the "subordinate_list" configuration value is to be changed.
 * @param new_so_list These configuration will be added to the existing one.
 * @param answer_list answer list for errors.
 *
 * @return true if the new config will be valid, false otherwise.
 *
 * @note MT-NOTE: check_new_slotwise_subordinate_tree() is MT safe
 */
bool
check_new_slotwise_subordinate_tree(const ocs::tom::Element<ocs::tom::QU> qinstance,
                                    const ocs::tom::ConstList<ocs::tom::SO> new_so_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   bool success = true;

   /*
    * Check if the queues that will be added to the "subordinate_list" are
    * already in the subordinate tree. If they are, there is a loop in the
    * tree, which is not allowed.
    */
   const ocs::tom::Element<ocs::tom::QU> root_qinstance = get_slotwise_sos_tree_root(qinstance);
   if (root_qinstance) {
      uint32_t slots_sum = 0;

      slots_sum = get_slotwise_sos_threshold(root_qinstance);
      if (slots_sum > 0) {
         sge_sl_list_t *qinstances_in_slotwise_sos_tree = nullptr;
         sge_sl_elem_t *ssos_qinstance_elem = nullptr;
         ssos_qinstance_t *ssos_qinstance = nullptr;
         const char *cqueue_name = nullptr;

         /* get the slotwise sos tree as a list */
         get_slotwise_sos_sub_tree_qinstances(root_qinstance,
                                              &qinstances_in_slotwise_sos_tree, 0);

         for (const auto new_so : new_so_list) {
            const char *new_so_name = nullptr;

            new_so_name = ocs::tom::get<SO_name>(new_so);
            if (new_so_name != nullptr) {
               for_each_sl(ssos_qinstance_elem, qinstances_in_slotwise_sos_tree) {
                  ssos_qinstance = (ssos_qinstance_t *) sge_sl_elem_data(ssos_qinstance_elem);
                  cqueue_name = ocs::tom::get<QU_qname>(ssos_qinstance->qinstance);

                  if (strcmp(cqueue_name, new_so_name) == 0) {
                     /*
                      * If this queue will be subordinated to "qinstance",
                      * there will be a loop in the tree -> error!
                      */
                     ERROR(MSG_PARSE_LOOP_IN_SSOS_TREE_SS, new_so_name,
                           static_cast<const char *>(ocs::tom::get<QU_qname>(qinstance)));
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                                     ANSWER_QUALITY_ERROR);
                     success = false;
                     break;
                  }
               }
            }
            if (!success) {
               break;
            }
         }
         sge_sl_destroy(&qinstances_in_slotwise_sos_tree,
                        (sge_sl_destroy_f) destroy_slotwise_sos_tree_elem);
      }
   }
   DRETURN(success);
}

/**
 * @brief 'calculates' and executes all slotwise (un)suspends
 *
 * Calculates and executes all suspends and unsuspends for the tasks in the
 * qinstances on the same host as the provided qinstance for the slotwise
 * subordination tree where the provided qinstance is member of.
 *
 * @param qinstance start from this qinstance doing this check
 * @param suspend calculate for suspend or for unsuspend?
 * @param check_subtree_only check only the subtree of the provided qinstance, not the whole slotwise subordinate tree the provided qinstance is member of
 * @param monitor monitor
 *
 * @return true if the provided qinstance is member of a slotwise subordinate tree, false if it is not.
 *
 * @note MT-NOTE: do_slotwise_x_on_subordinate_check() is MT safe
 */
bool
do_slotwise_x_on_subordinate_check(const ocs::tom::Element<ocs::tom::QU> qinstance, const bool suspend,
                                   const bool check_subtree_only, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   sge_sl_list_t *qinstances_in_slotwise_sos_tree = nullptr;
   ocs::tom::Element<ocs::tom::QU> super_qinstance = nullptr;
   uint32_t running_jobs = 0;
   uint32_t slots_sum = 0;

   if (!check_subtree_only) {
      if (suspend) {
         /* Always check a sub tree from a tree node, don't do checking from 
          * a leaf node.
          */
         if (get_slotwise_sos_threshold(qinstance) == 0) {
            /* qinstance doesn't have a slotwise sos list defined, so it is a leaf node.
             * We begin searching at our parent queue instance.
             */
            super_qinstance = get_slotwise_sos_super_qinstance(qinstance);
         } else {
            super_qinstance = qinstance;
         }
      } else {
         super_qinstance = get_slotwise_sos_tree_root(qinstance);
      }
   } else {
      /*
       * Assume our qinstance is the root node of a slotwise subordination tree.
       */
      super_qinstance = qinstance;
   }

   if (super_qinstance == nullptr) {
      DRETURN(false);
   }

   slots_sum = get_slotwise_sos_threshold(super_qinstance);
   if (slots_sum == 0) {
      /* no slotwise suspend on subordinate! */
      DRETURN(false);
   }

   /* get the slotwise sos tree as a list */
   get_slotwise_sos_sub_tree_qinstances(super_qinstance, &qinstances_in_slotwise_sos_tree, 0);

   /* count the number and store informations about all running tasks in the list */
   running_jobs = count_running_jobs_in_slotwise_sos_tree(qinstances_in_slotwise_sos_tree, suspend);
   if ((suspend && running_jobs > slots_sum) ||
       (!suspend && running_jobs < slots_sum)) {
      bool ret = false;
      int diff = 0;
      /* we have to (un)suspend as many running/suspended jobs as new jobs
       * were scheduled/finished or (un)suspended by other ways.
       */
      diff = running_jobs - slots_sum;
      diff = abs(diff);
      do {
         /* suspend/unsuspend the highest/lowest running/suspended task */
         ret = x_most_extreme_task(qinstances_in_slotwise_sos_tree, suspend, monitor);
      } while (ret && (--diff) > 0);
   }
   sge_sl_destroy(&qinstances_in_slotwise_sos_tree, (sge_sl_destroy_f) destroy_slotwise_sos_tree_elem);

   if (suspend && !check_subtree_only) {
      /* Walk the tree from the leaves to the root */
      const ocs::tom::Element<ocs::tom::QU> super_super = get_slotwise_sos_super_qinstance(super_qinstance);
      if (super_super) {
         do_slotwise_x_on_subordinate_check(super_super, suspend, check_subtree_only, monitor);
      }
   }
   DRETURN(true);
}

/**
 * @brief Checks if the old and the new subordinate list differ
 *
 * Compares the old and the new subordinate list configured in a queue and
 * returns true if they differ. The order of subordinates doesn't matter.
 *
 * @param old_so_list The old subordinate list. SO_Type list.
 * @param new_so_list The new subordinate list. SO_Type list.
 *
 * @return true if the lists differ, false if they are identical. If just the order of subordinates is changed, the lists are considered as identical.
 *
 * @note MT-NOTE: do_slotwise_subordinate_lists_differ() is MT safe
 */
bool
do_slotwise_subordinate_lists_differ(const ocs::tom::ConstList<ocs::tom::SO> old_so_list,
                                     const ocs::tom::ConstList<ocs::tom::SO> new_so_list) {
   DENTER(TOP_LAYER);

   bool ret = false;

   for (const auto old_so : old_so_list) {
      ocs::tom::ConstElement<ocs::tom::SO> new_so = ocs::tom::find_first<SO_name>(new_so_list, ocs::tom::get<SO_name>(old_so));
      /* find all queues that are in the old list but not in the new list */
      if (new_so == nullptr) {
         ret = true;
         break;
      } else {
         /*
          * find all queues that are both in the old and in the new list,
          * but whose values were changed
          */
         uint32_t new_seq_no = ocs::tom::get<SO_seq_no>(new_so);
         uint32_t new_action = ocs::tom::get<SO_action>(new_so);
         uint32_t old_seq_no = ocs::tom::get<SO_seq_no>(old_so);
         uint32_t old_action = ocs::tom::get<SO_action>(old_so);

         if (old_seq_no != new_seq_no || old_action != new_action) {
            ret = true;
            break;
         }
      }
   }

   /* find all queues that are in the new list but weren't in the old list */
   if (!ret) {
      for (const auto new_so : new_so_list) {
         if (ocs::tom::find_first<SO_name>(old_so_list, ocs::tom::get<SO_name>(new_so)) == nullptr) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

/** @brief Suspend or resume the queue instances subordinate to a job's granted list
 *
 * Called when a job starts or ends: the queues subordinate to the ones it now
 * occupies have to follow.
 *
 * @note We assume the associated job is already/still debited on all the
 *       queues that are referenced in gdil.
 *
 * @param master_cqueue_list the cluster queues
 * @param suspend true to suspend, false to resume
 * @param gdil the granted destination list of the job that changed
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 * @return true if every affected queue instance could be signalled
 */
bool
cqueue_list_x_on_subordinate_gdil(const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list, const bool suspend,
                                  const ocs::tom::ConstList<ocs::tom::JG> gdil, monitoring_t *monitor,
                                  const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   for (const auto gdi : gdil) {
      const char *full_name = ocs::tom::get<JG_qname>(gdi);
      const char *hostname = ocs::tom::get<JG_qhostname>(gdi);
      const ocs::tom::Element<ocs::tom::QU> qinstance = cqueue_list_locate_qinstance(master_cqueue_list, full_name);

      if (qinstance) {
         ocs::tom::ConstList<ocs::tom::SO> so_list = ocs::tom::get<QU_subordinate_list>(qinstance);
         uint32_t slots = ocs::tom::get<QU_job_slots>(qinstance);
         uint32_t slots_used = qinstance_slots_used(qinstance);
         uint32_t slots_granted = ocs::tom::get<JG_slots>(gdi);

         do_slotwise_x_on_subordinate_check(qinstance, suspend, false, monitor);
         /*
          * Only if this qinstance has not-slotwise subordinates
          * (SO_slots_sum == 0), we must check for queue wise subordination.
          */
         if (ocs::tom::find_first_in<QU_subordinate_list, SO_slots_sum>(qinstance, 0) != nullptr) {
            ocs::tom::ListPtr<ocs::tom::SO> resolved_so_list;

            /* Do queue wise suspend on subordinate */
            /*
             * Resolve cluster queue names into qinstance names
             */
            so_list_resolve(so_list, nullptr, resolved_so_list, nullptr, hostname, master_cqueue_list);

            for (const auto so : resolved_so_list.get()) {
               const char *so_queue_name = ocs::tom::get<SO_name>(so);

               /* We have to check this because so_list_resolve() didn't. */
               if (strcmp(full_name, so_queue_name) == 0) {
                  /* Queue can't be subordinate to itself. */
                  DPRINTF (("Removing circular reference.\n"));
                  continue;
               }

               /*
                * suspend:
                *    no sos before this job came on this queue AND
                *    sos since job is on this queue
                *
                * unsuspend:
                *    no sos after job gone from this queue AND
                *    sos since job is on this queue
                */
               if (!tst_sos(slots_used - slots_granted, slots, so) &&
                   tst_sos(slots_used, slots, so)) {
                  const ocs::tom::Element<ocs::tom::QU> so_queue =
                          cqueue_list_locate_qinstance(master_cqueue_list, so_queue_name);

                  if (so_queue) {
                     /* Suspend/unsuspend the subordinated queue instance */
                     ret &= qinstance_x_on_subordinate(so_queue, suspend, true, monitor, gdi_session);
                     /* This change could also trigger slotwise (un)suspend on
                      * subordinate in related queue instances. If it was a
                      * queuewise suspend, it must be a slotwise unsuspend,
                      * and vice versa.
                      */
                     do_slotwise_x_on_subordinate_check(so_queue, (bool) !suspend, false, monitor);
                  } else {
                     ERROR(MSG_QINSTANCE_NQIFOUND_SS, so_queue_name, __func__);
                     ret = false;
                  }
               }
            }
         }
      } else {
         /* should never happen */
         ERROR(MSG_QINSTANCE_NQIFOUND_SS, full_name, __func__);
         ret = false;
      }
   }
   DRETURN(ret);
}

/**
 * @brief (Un)suspend one queue instance because of queue wise subordination
 *
 * Counts the suspensions on subordinate of the queue instance; the first one
 * suspends it, the last one resumes it. A signal is only delivered if the
 * queue instance is neither manually nor calendar suspended.
 *
 * @param this_elem the subordinate queue instance
 * @param suspend true to suspend, false to resume
 * @param send_event whether to send a sgeE_QINSTANCE_SOS/USOS event
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 * @return false if the signal could not be delivered
 */
static bool
qinstance_x_on_subordinate(const ocs::tom::Element<ocs::tom::QU> this_elem, const bool suspend,
                           const bool send_event, monitoring_t *monitor, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;
   uint32_t sos_counter;
   bool do_action;
   bool send_qinstance_signal;
   const char *hostname;
   const char *cqueue_name;
   const char *full_name;
   int signal;
   ev_event event;

   /* increment sos counter */
   sos_counter = ocs::tom::get<QU_suspended_on_subordinate>(this_elem);
   if (suspend) {
      sos_counter++;
   } else {
      sos_counter--;
   }
   ocs::tom::set<QU_suspended_on_subordinate>(this_elem, sos_counter);

   /* 
    * prepare for operation
    *
    * suspend:  
    *    send a signal if it is not already suspended by admin or calendar 
    *
    * !suspend:
    *    send a signal if not still suspended by admin or calendar
    */
   hostname = ocs::tom::get<QU_qhostname>(this_elem);
   cqueue_name = ocs::tom::get<QU_qname>(this_elem);
   full_name = ocs::tom::get<QU_full_name>(this_elem);
   send_qinstance_signal = (qinstance_state_is_manual_suspended(this_elem) ||
                            qinstance_state_is_cal_suspended(this_elem)) ? false : true;
   if (suspend) {
      do_action = (sos_counter == 1) ? true : false;
      signal = SGE_SIGSTOP;
      event = sgeE_QINSTANCE_SOS;
   } else {
      do_action = (sos_counter == 0) ? true : false;
      signal = SGE_SIGCONT;
      event = sgeE_QINSTANCE_USOS;
   }

   /*
    * do operation
    */
   DPRINTF("qinstance " SFQ " " SFN " " SFN " on subordinate\n", full_name,
           (do_action ? "" : "already"), (suspend ? "suspended" : "unsuspended"));
   if (do_action) {
      DPRINTF("Due to other suspend states signal will %sbe delivered\n", send_qinstance_signal ? "NOT " : "");
      if (send_qinstance_signal) {
         ret = (sge_signal_queue(signal, this_elem, ocs::tom::Element<ocs::tom::JB>(),
                                 ocs::tom::Element<ocs::tom::JAT>(), monitor) == 0) ? true : false;
      }

      sge_qmaster_qinstance_state_set_susp_on_sub(this_elem, suspend, gdi_session);
      if (send_event) {
         sge_add_event(0, event, 0, 0, cqueue_name, hostname, nullptr, nullptr, gdi_session);
      }
      /*
       * this queue instance was (un)suspended by queue wise suspend on subordinate,
       * now check if it has slotwise subordinates that must be handled.
       */
      do_slotwise_x_on_subordinate_check(this_elem, (bool) !suspend, false, monitor);
   }
   DRETURN(ret);
}

/** @brief Suspend or resume an already resolved set of subordinate queue instances
 * @param master_cqueue_list the cluster queues
 * @param answer_list receives messages for the caller
 * @param suspend true to suspend, false to resume
 * @param resolved_so_list the queue instances to act on
 * @param monitor for monitoring qmaster threads
 * @param gdi_session the session the change belongs to
 * @return true if every affected queue instance could be signalled
 */
bool
cqueue_list_x_on_subordinate_so(const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list, lList **answer_list,
                                const bool suspend, const ocs::tom::ConstList<ocs::tom::SO> resolved_so_list,
                                monitoring_t *monitor, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /*
    * Locate all qinstances which are mentioned in resolved_so_list and 
    * (un)suspend them
    */
   for (const auto so : resolved_so_list) {
      const char *full_name = ocs::tom::get<SO_name>(so);
      const ocs::tom::Element<ocs::tom::QU> qinstance = cqueue_list_locate_qinstance(master_cqueue_list, full_name);

      if (qinstance) {
         ret &= qinstance_x_on_subordinate(qinstance, suspend, true, monitor, gdi_session);
         if (!ret) {
            break;
         }
      }
   }
   DRETURN(ret);
}

/** @brief Collect the queue instances currently suspended because of this one
 *
 * The subordinate list of @p this_elem is resolved into queue instance names;
 * every subordinate that is not actually suspended by the slots @p this_elem
 * uses is removed again. Slotwise subordination is not considered.
 *
 * @param this_elem the superordinate queue instance
 * @param answer_list receives messages for the caller
 * @param master_cqueue_list the cluster queues
 * @return the suspended subordinates (SO_Type); an empty handle if @p this_elem is empty,
 *         has slotwise subordinates or nothing was resolved
 */
ocs::tom::ListPtr<ocs::tom::SO>
qinstance_find_suspended_subordinates(const ocs::tom::ConstElement<ocs::tom::QU> this_elem, lList **answer_list,
                                      const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::SO> resolved_so_list;

   if (this_elem) {
      if (get_slotwise_sos_threshold(this_elem) == 0) {
         ocs::tom::ConstList<ocs::tom::SO> so_list = ocs::tom::get<QU_subordinate_list>(this_elem);
         const char *hostname = ocs::tom::get<QU_qhostname>(this_elem);
         /* Slots calculations */
         uint32_t slots = ocs::tom::get<QU_job_slots>(this_elem);
         uint32_t slots_used = qinstance_slots_used(this_elem);
         /*
          * Resolve cluster queue names into qinstance names
          */
         so_list_resolve(so_list, answer_list, resolved_so_list, nullptr, hostname, master_cqueue_list);
         /* 
          * If the number of used slots on this qinstance is greater than a
          * subordinate's threshold (if it has one), this subordinate should
          * be suspended.
          *
          * Remove all subordinated queues from "resolved_so_list" which
          * are not actually suspended by "this_elem" 
          */
         DTRACE;
         const ocs::tom::List<ocs::tom::SO> resolved = resolved_so_list.get();
         ocs::tom::Element<ocs::tom::SO> next_so = resolved.front();
         ocs::tom::Element<ocs::tom::SO> so;
         while ((so = next_so)) {
            next_so = so.next();
            if (!tst_sos(slots_used, slots, so)) {
               DPRINTF("Removing %s because it's not suspended\n", ocs::tom::get<SO_name>(so));
               resolved.erase(so);
            }
         }
      }
   }

   DRETURN(resolved_so_list);
}

/** @brief Work out whether a queue instance starts life suspended by a superordinate
 *
 * A newly created or reconfigured instance may already be subordinate to a
 * queue that is full, in which case it must not accept jobs.
 *
 * @param this_elem the queue instance (`QU_Type`)
 * @param monitor for monitoring qmaster threads
 * @param master_cqueue_list the cluster queues
 * @param gdi_session the session the change belongs to
 * @return true if the instance starts out suspended by a superordinate queue
 */
bool
qinstance_initialize_sos_attr(const ocs::tom::Element<ocs::tom::QU> this_elem, monitoring_t *monitor,
                              const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                              const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *full_name = nullptr;
   const char *qinstance_name = nullptr;
   const char *hostname = nullptr;

   full_name = ocs::tom::get<QU_full_name>(this_elem);
   qinstance_name = ocs::tom::get<QU_qname>(this_elem);
   hostname = ocs::tom::get<QU_qhostname>(this_elem);

   for (const auto cqueue : master_cqueue_list) {
      ocs::tom::ConstElement<ocs::tom::QU> qinstance = ocs::tom::find_first_in<CQ_qinstances, QU_qhostname>(cqueue, hostname);

      if (qinstance != nullptr) {
         if (get_slotwise_sos_threshold(qinstance) > 0) {
            do_slotwise_x_on_subordinate_check(this_elem, true, false, monitor);
         } else {
            uint32_t slots = 0;
            uint32_t slots_used = 0;
            ocs::tom::ListPtr<ocs::tom::SO> resolved_so_list;
            ocs::tom::ConstList<ocs::tom::SO> so_list = ocs::tom::get<QU_subordinate_list>(qinstance);

            /* queue instance-wise suspend on subordinate */
            slots = ocs::tom::get<QU_job_slots>(qinstance);
            slots_used = qinstance_slots_used(qinstance);

            /*
             * Resolve cluster queue names into qinstance names
             */
            so_list_resolve(so_list, nullptr, resolved_so_list, qinstance_name, hostname, master_cqueue_list);

            for (const auto so : resolved_so_list.get()) {
               const char *so_full_name = ocs::tom::get<SO_name>(so);

               if (!strcmp(full_name, so_full_name)) {
                  /* suspend the queue if neccessary */
                  if (tst_sos(slots_used, slots, so)) {
                     qinstance_x_on_subordinate(this_elem, true, false, monitor, gdi_session);
                  }
               }
            }
         }
      }
   }
   DRETURN(ret);
}
