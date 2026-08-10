#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief Base view of `qstat -g c`, and the interface the three output formats implement
 */

#include "qstat/ocs_QStatParameter.h"

#include "ocs_ProcedureView.h"

namespace ocs {
   /** @brief Base view for `qstat -g c`, and the interface the three formats implement
    *
    * `qstat -g c` reports one line per cluster queue rather than per queue
    * instance, so everything it prints is a sum over the instances. The
    * controller works those sums out; the view only prints them.
    *
    * @ingroup libprocedure
    */
   class QStatGroupViewBase : public ProcedureView {
   public:
      /** @brief One cluster queue's slots, added up over its queue instances
       *
       * Every count is a number of slots, not of queue instances, and the
       * states are not disjoint - a queue instance can be both disabled and in
       * load alarm, and then contributes to both.
       */
      struct Summary {
         double load;                        ///< Average load over the instances, valid only when #is_load_available
         bool   is_load_available;           ///< Whether any instance reported a load value
         uint32_t used;                      ///< Slots in use
         uint32_t resv;                      ///< Slots reserved by advance reservations
         uint32_t total;                     ///< Slots configured
         uint32_t temp_disabled;             ///< Slots temporarily unusable, e.g. by a calendar
         uint32_t available;                 ///< Slots that could take a job now
         uint32_t manual_intervention;       ///< Slots needing an administrator before they can be used again
         uint32_t suspend_manual;            ///< Slots suspended by an administrator
         uint32_t suspend_threshold;         ///< Slots suspended because a threshold was exceeded
         uint32_t suspend_on_subordinate;    ///< Slots suspended by a subordinate relationship
         uint32_t suspend_calendar;          ///< Slots suspended by a calendar
         /// Slots on unreachable hosts
         uint32_t unknown, load_alarm;       ///< Slots in load alarm
         uint32_t disabled_manual;           ///< Slots disabled by an administrator
         uint32_t disabled_calendar;         ///< Slots disabled by a calendar
         uint32_t ambiguous;                 ///< Slots whose configuration cannot be resolved
         /// Slots of a deleted queue instance that still hold jobs
         uint32_t orphaned, error;           ///< Slots in error state
      };

      /** @brief Build a view for one `qstat -g c` call
       * @param parameter the call's parameters
       */
      explicit QStatGroupViewBase(const ProcedureParameter &parameter) : ProcedureView(parameter) {} ;

      ~QStatGroupViewBase() override = default;

      /** @brief Begin the report
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_started(std::ostream &os, QStatParameter &parameter) = 0;

      /** @brief End the report
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_finished(std::ostream &os, QStatParameter &parameter) = 0;

      /** @brief Report one cluster queue
       * @param os stream to write to
       * @param qname the cluster queue
       * @param cqueue_summary its slots, added up over its instances
       * @param parameter the call's parameters
       */
      virtual void report_cqueue(std::ostream &os, const char* qname, Summary *cqueue_summary, QStatParameter &parameter) = 0;
   };
}
