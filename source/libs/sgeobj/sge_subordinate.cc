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
 * @brief Subordinate queues: one queue suspends another when busy
 *
 * The relation may be queue wise - the whole subordinate queue is suspended -
 * or slot wise, where only as many jobs are suspended as the superordinate
 * queue needs slots for.
 *
 * @see sge_subordinate.h
 */

#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "uti/sge.h"
#include "sgeobj/sge_answer.h"

/* -----------------------------------------------

   test suspend on subordinate (nice neuron)

    A1            C1
      \          /
       \        /
        \      /
  A2--------->B-------> C2
        /      \
       /        \
      /          \
    A3            C3

   a queue C subordinated by B must be suspended if
   the used slots of queue B meet the thresold for C

*/
/**
 * @brief Is a subordinate queue's suspend threshold reached?
 *
 * A subordinate relation suspends the subordinate queue once the
 * superordinate one is busy enough. The threshold is a slot count, and 0 means
 * "all slots".
 *
 * @param used slots in use in the superordinate queue B
 * @param total slots the superordinate queue B has
 * @param so the subordinate relation (SO_Type) referencing a queue C, holding the threshold
 * @return true when the subordinate queue C should be suspended
 */
bool tst_sos(const int used, const int total, const ocs::tom::ConstElement<ocs::tom::SO> so) {
   DENTER(TOP_LAYER);
   uint32_t threshold;
   bool     ret = false;

   /*
    * then check if B's usage meets the threshold
    * for suspension of the subordinated queue C
    */
   if ((threshold=ocs::tom::get<SO_threshold>(so)) == 0) {
      /* queue must be full for suspend of queue C */
      DPRINTF("TSTSOS: %sfull -> %ssuspended\n", (used>=total)?"":"not ",
              (used>=total)?"":"not ");
      ret = (bool)(used >= total);
   } else {
      /* used slots greater or equal threshold */
      DPRINTF("TSTSOS: %d slots used (limit " sge_u32") -> %ssuspended\n", used, threshold, ((uint32_t)(used) >= threshold)?"":"not ");
      ret = (bool)((uint32_t)used >= threshold);
   }
   DRETURN(ret);
}


/**
 * @brief Render a subordinate list as an administrator writes it
 *
 * @param this_list the subordinate relations to render, may be an empty handle
 * @param[out] string receives the text, appended
 * @return the resulting text
 */
const char *so_list_append_to_dstring(const ocs::tom::ConstList<ocs::tom::SO> this_list, dstring *string) {
   DENTER(BASIS_LAYER);

   const char *ret = nullptr;

   if (string != nullptr) {
      bool printed = false;
      ocs::tom::ConstElement<ocs::tom::SO> so;
      uint32_t slots_sum = 0;

      if (this_list && (so = this_list.front())) {
         slots_sum = ocs::tom::get<SO_slots_sum>(so);

         if (slots_sum > 0) {
            /*
             * slot-wise suspend on subordinate
             */
            sge_dstring_sprintf_append(string, "slots=" sge_u32"(", slots_sum);

            for (const auto elem : this_list) {
               const char *action_str = "sr";

               if (ocs::tom::get<SO_action>(elem) == SO_ACTION_LR) {
                  action_str = "lr";
               }

               sge_dstring_sprintf_append(string, "%s:" sge_u32":%s%s",
                  ocs::tom::get<SO_name>(elem),
                  ocs::tom::get<SO_seq_no>(elem),
                  action_str,
                  elem.next() ? ", " : "");
            }
            sge_dstring_sprintf_append(string, ")");
            printed = true;
         } else {
            /*
             * queue instance-wise suspend on subordinate
             */
            for (const auto elem : this_list) {
               if (printed) {
                  sge_dstring_append (string, " ");
               }
               
               sge_dstring_append(string, ocs::tom::get<SO_name>(elem));
               if (ocs::tom::get<SO_threshold>(elem)) {
                  sge_dstring_sprintf_append(string, "=" sge_u32"%s",
                                             ocs::tom::get<SO_threshold>(elem),
                                             elem.next() ? "," : "");
               }
               printed = true;
            }
         }
      }
      if (!printed) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }
   DRETURN(ret);
}


/**
 * @brief Add one subordinate relation to a list
 *
 * If the list holds a relation to the queue already, its values are
 * updated instead: a lower threshold, a lower slots_sum, a higher seq_no and
 * another action replace the ones held.
 *
 * @param[in,out] this_list the list to extend; created named "" when it is an empty handle
 * @param[out] answer_list not written
 * @param so_name the subordinate queue's name
 * @param threshold slots that have to be in use before it is suspended; 0 means all
 * @param slots_sum for a slot wise relation, how many slots the two queues share
 * @param seq_no the order the relations are evaluated in
 * @param action one of the `SO_ACTION_*` values
 * @return true when the relation was added
 */
bool so_list_add(ocs::tom::ListPtr<ocs::tom::SO> &this_list,
                 [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                 const char *so_name, const uint32_t threshold, const uint32_t slots_sum, const uint32_t seq_no,
                 const uint32_t action) {
   DENTER(TOP_LAYER);

   if (so_name != nullptr) {
      ocs::tom::Element<ocs::tom::SO> elem;
      if (this_list) {
         elem = this_list->find_first<SO_name>(so_name);
      }

      if (elem != nullptr) {
         uint32_t current_threshold = ocs::tom::get<SO_threshold>(elem);
         uint32_t current_slots_sum = ocs::tom::get<SO_slots_sum>(elem);
         uint32_t current_seq_no    = ocs::tom::get<SO_seq_no>(elem);
         uint32_t current_action    = ocs::tom::get<SO_action>(elem);

         if (threshold != 0 && threshold < current_threshold) {
            DPRINTF("Replacing entry with higher threshold: %d => %d\n", current_threshold, threshold);
            ocs::tom::set<SO_threshold>(elem, threshold);
         }
         if (slots_sum != 0 && slots_sum < current_slots_sum) {
            DPRINTF("Replacing entry with higher slots_sum: %d => %d\n", current_slots_sum, slots_sum);
            ocs::tom::set<SO_slots_sum>(elem, slots_sum);
         }
         if (seq_no != 0 && seq_no > current_seq_no) {
            DPRINTF("Replacing entry with lower seq_no: %d => %d\n", current_seq_no, seq_no);
            ocs::tom::set<SO_seq_no>(elem, seq_no);
         }
         if (action != current_action) {
            DPRINTF("Replacing entry with different action: %d => %d\n", current_action, action);
            ocs::tom::set<SO_action>(elem, action);
         }
      } else {
         DPRINTF("Adding new entry with threshold: %d, slots_sum: %d, seq_no: %d\n", threshold, slots_sum, seq_no);
         if (!this_list) {
            this_list = ocs::tom::ListPtr<ocs::tom::SO>::create("");
         }
         ocs::tom::Element<ocs::tom::SO> elem = this_list->emplace_back<SO_name>(so_name);
         ocs::tom::set<SO_threshold>(elem, threshold);
         ocs::tom::set<SO_slots_sum>(elem, slots_sum);
         ocs::tom::set<SO_seq_no>(elem,    seq_no);
         ocs::tom::set<SO_action>(elem, action);
      }
   }
   
   DRETURN(true);
}

/**
 * @brief Resolve a generic list of subordinates into their
 *
 * Goes through every entry in the so_list, retrieves the corresponding
 * cqueue, gets the qinstance for hostname from the cqueue, and adds the
 * qinstance's full name to the resolved_so_list.  If qi_name is given, the
 * subordinate list will be checked to make sure that it doesn't contain the
 * queue to which the list is subordinate.
 *
 * @param so_list the list of subordinates to resolve, may be an empty handle
 * @param answer_list answer list for errors
 * @param[in,out] resolved_so_list the destination list for resolved subordinates; created by the first one
 * @param cq_name the queue name of the qinstance to which the subordinate list is subordinate
 * @param hostname the hostname for the queue to which the subordinate list is subordinate
 * @param master_cqueue_list the cluster queues a relation may name
 *
 * @return error state true  - success false - error
 */
bool so_list_resolve(const ocs::tom::ConstList<ocs::tom::SO> so_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                     ocs::tom::ListPtr<ocs::tom::SO> &resolved_so_list, const char *cq_name, const char *hostname,
                     const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (so_list && hostname != nullptr) {
      if (cq_name != nullptr) {
         DPRINTF("Finding subordinates for %s on %s\n", cq_name, hostname);
      } else {
         DPRINTF("Finding subordinates on host %s\n", hostname);
      }
      
      /* Get the list of resolved qinstances for each subordinate. */
      for (const auto so : so_list) {
         const char *qinstance_name = nullptr;
         const char *cq_name_str = ocs::tom::get<SO_name>(so);

         const auto cqueue = cqueue_list_locate(master_cqueue_list, cq_name_str);

         if (cqueue) {
            const auto qinstance = cqueue_locate_qinstance(cqueue, hostname);

            /* If this cqueue doesn't have a qinstance on this host,
             * just skip it. */
            if (qinstance) {
               uint32_t threshold   = ocs::tom::get<SO_threshold>(so);
               uint32_t slots_sum   = ocs::tom::get<SO_slots_sum>(so);
               uint32_t seq_no      = ocs::tom::get<SO_seq_no>(so);
               uint32_t action      = ocs::tom::get<SO_action>(so);
               qinstance_name     = ocs::tom::get<QU_full_name>(qinstance);

               so_list_add(resolved_so_list, answer_list, qinstance_name,
                           threshold, slots_sum, seq_no, action);
               continue;
            }
         }
         if (cq_name && strcmp(cq_name, cq_name_str) == 0){
            dstring buffer = DSTRING_INIT;
            uint32_t threshold     = ocs::tom::get<SO_threshold>(so);
            uint32_t slots_sum     = ocs::tom::get<SO_slots_sum>(so);
            uint32_t seq_no        = ocs::tom::get<SO_seq_no>(so);
            uint32_t action        = ocs::tom::get<SO_action>(so);
            
            qinstance_name = sge_dstring_sprintf(&buffer, "%s@%s", cq_name, hostname);
            so_list_add(resolved_so_list, answer_list, qinstance_name, threshold,
                        slots_sum, seq_no, action);
            sge_dstring_free(&buffer);
         }
      }
   }

   DRETURN(ret);
}

