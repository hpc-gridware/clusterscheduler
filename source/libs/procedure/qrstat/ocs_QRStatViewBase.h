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
 * @brief Base view of `qrstat`, and the interface the three output formats implement
 */

#include <ostream>

#include "ocs_ProcedureView.h"
#include "ocs_QRStatParameterClient.h"

namespace ocs {
   /** @brief Base view for `qrstat`, and the interface the three formats implement
    *
    * The controller walks the advance reservations and reports what it finds
    * through these hooks; #QRStatViewPlain, #QRStatViewXML and #QRStatViewJSON
    * turn the events into their own syntax. Adding a format means implementing
    * the hooks, not touching the walk.
    *
    * The hooks come in `_start` / `_finish` pairs around each nested list, with
    * `_node` hooks for the entries. The node hooks are split by value type
    * rather than taking pre-formatted text, so that XML and JSON can emit a
    * number as a number and a time as a timestamp.
    *
    * @ingroup libprocedure
    */
   class QRStatViewBase : public ProcedureView{
   public:
      bool show_summary = false;   ///< Whether one line per reservation is wanted rather than full detail
   public:
      explicit QRStatViewBase(const QRStatParameter &parameter);
      ~QRStatViewBase() override = default;

      /** @brief Begin the report

       * @param os stream to write to

       */

      virtual void report_start(std::ostream &os) = 0;
      /** @brief End the report
       * @param os stream to write to
       */
      virtual void report_finish(std::ostream &os) = 0;

      /** @brief Begin one advance reservation

       * @param os stream to write to

       */

      virtual void report_ar_start(std::ostream &os) = 0;
      /** @brief End the current advance reservation
       * @param os stream to write to
       */
      virtual void report_ar_finish(std::ostream &os) = 0;
      /** @brief Report an integer attribute of the current reservation
       * @param os stream to write to
       * @param name the attribute
       * @param value its value
       */
      virtual void report_ar_node_ulong(std::ostream &os, const char *name, uint32_t value) = 0;
      /** @brief Report a duration attribute of the current reservation
       * @param os stream to write to
       * @param name the attribute
       * @param value the duration in seconds
       */
      virtual void report_ar_node_duration(std::ostream &os, const char *name, uint64_t value) = 0;
      /** @brief Report a string attribute of the current reservation
       * @param os stream to write to
       * @param name the attribute
       * @param value its value
       */
      virtual void report_ar_node_string(std::ostream &os, const char *name, const char *value) = 0;
      /** @brief Report a point in time attribute of the current reservation
       * @param os stream to write to
       * @param name the attribute
       * @param value the time, as an epoch timestamp
       */
      virtual void report_ar_node_time(std::ostream &os, const char *name, uint64_t value) = 0;
      /** @brief Report the state of the current reservation
       * @param os stream to write to
       * @param name the attribute
       * @param state the state, as an `AR_STATE` value
       */
      virtual void report_ar_node_state(std::ostream &os, const char *name, uint32_t state) = 0;
      /** @brief Report a boolean attribute of the current reservation
       * @param os stream to write to
       * @param name the attribute
       * @param value its value
       */
      virtual void report_ar_node_boolean(std::ostream &os, const char *name, bool value) = 0;

      /** @brief Begin the resources the reservation requested

       * @param os stream to write to

       */

      virtual void report_resource_list_start(std::ostream &os) = 0;
      /** @brief End the resource list
       * @param os stream to write to
       */
      virtual void report_resource_list_finish(std::ostream &os) = 0;
      /** @brief Report a string valued resource
       * @param os stream to write to
       * @param name the resource
       * @param value its value
       */
      virtual void report_resource_list_node_str(std::ostream &os, const char *name, const char *value) = 0;
      /** @brief Report a floating point resource
       * @param os stream to write to
       * @param name the resource
       * @param value its value
       */
      virtual void report_resource_list_node_double(std::ostream &os, const char *name, double value) = 0;
      /** @brief Report an integer valued resource
       * @param os stream to write to
       * @param name the resource
       * @param value its value
       */
      virtual void report_resource_list_node_uint64(std::ostream &os, const char *name, uint64_t value) = 0;
      /** @brief Report a boolean resource
       * @param os stream to write to
       * @param name the resource
       * @param value its value
       */
      virtual void report_resource_list_node_bool(std::ostream &os, const char *name, bool value) = 0;

      /** @brief Begin the queue instances the reservation was granted

       * @param os stream to write to

       */

      virtual void report_exec_queue_list_start(std::ostream &os) = 0;
      /** @brief End the granted queue list
       * @param os stream to write to
       */
      virtual void report_exec_queue_list_finish(std::ostream &os) = 0;
      /** @brief Report one granted queue instance
       * @param os stream to write to
       * @param name the queue instance
       * @param value the slots reserved in it
       */
      virtual void report_exec_queue_list_node(std::ostream &os, const char *name, uint32_t value) = 0;

      /** @brief Begin the core binding of the reservation

       * @param os stream to write to

       */

      virtual void report_exec_binding_list_start(std::ostream &os) = 0;
      /** @brief End the core binding list
       * @param os stream to write to
       */
      virtual void report_exec_binding_list_finish(std::ostream &os) = 0;
      /** @brief Report the core binding on one host
       * @param os stream to write to
       * @param name the host
       * @param value the binding
       */
      virtual void report_exec_binding_list_node(std::ostream &os, const char *name, const char *value) = 0;

      /** @brief Begin the granted parallel environment

       * @param os stream to write to

       */

      virtual void report_granted_parallel_environment_start(std::ostream &os) = 0;
      /** @brief End the granted parallel environment
       * @param os stream to write to
       */
      virtual void report_granted_parallel_environment_finish(std::ostream &os) = 0;
      /** @brief Report the granted parallel environment
       * @param os stream to write to
       * @param name the parallel environment
       * @param slots_range the slot range it was granted
       */
      virtual void report_granted_parallel_environment_node(std::ostream &os, const char *name, const char *slots_range) = 0;

      /** @brief Begin the addresses the reservation mails to

       * @param os stream to write to

       */

      virtual void report_mail_list_start(std::ostream &os) = 0;
      /** @brief End the mail list
       * @param os stream to write to
       */
      virtual void report_mail_list_finish(std::ostream &os) = 0;
      /** @brief Report one mail recipient
       * @param os stream to write to
       * @param name the user
       * @param host the host to mail to
       */
      virtual void report_mail_list_node(std::ostream &os, const char *name, const char *host) = 0;

      /** @brief Begin the users allowed to use the reservation

       * @param os stream to write to

       */

      virtual void report_acl_list_start(std::ostream &os) = 0;
      /** @brief End the allow list
       * @param os stream to write to
       */
      virtual void report_acl_list_finish(std::ostream &os) = 0;
      /** @brief Report one allowed user or user set
       * @param os stream to write to
       * @param name the user or user set
       */
      virtual void report_acl_list_node(std::ostream &os, const char *name) = 0;

      /** @brief Begin the users barred from the reservation

       * @param os stream to write to

       */

      virtual void report_xacl_list_start(std::ostream &os) = 0;
      /** @brief End the deny list
       * @param os stream to write to
       */
      virtual void report_xacl_list_finish(std::ostream &os) = 0;
      /** @brief Report one barred user or user set
       * @param os stream to write to
       * @param name the user or user set
       */
      virtual void report_xacl_list_node(std::ostream &os, const char *name) = 0;
   };
}