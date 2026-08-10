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
 * @brief Plain text rendering of `qrstat`
 */

#include <ostream>

#include "ocs_QRStatViewBase.h"
#include "cull/cull.h"

namespace ocs {
   /** @brief Renders the `qrstat` report as the plain text a terminal expects
    *
    * The nested lists are printed as comma separated values on one line, so the
    * view has to know whether it is at the first entry of a list - that is what
    * the `first_*` flags are for.
    *
    * @ingroup libprocedure
    */
   class QRStatViewPlain : public QRStatViewBase {
      bool header_printed = false;     ///< Whether the summary table already has its header line
      bool first_resource = false;     ///< Whether no resource has been written for this reservation yet
      bool first_exec_queue = false;   ///< Whether no granted queue has been written yet
      bool first_mail = false;         ///< Whether no mail recipient has been written yet
      bool first_acl = false;          ///< Whether no allowed user has been written yet
      bool first_xacl = false;         ///< Whether no barred user has been written yet
   public:
      QRStatViewPlain(const QRStatParameter &parameter);
      ~QRStatViewPlain() override = default;

      void report_start(std::ostream &os) override;
      void report_finish(std::ostream &os) override;
      void report_ar_start(std::ostream &os) override;
      void report_ar_finish(std::ostream &os) override;
      void report_ar_node_ulong(std::ostream &os, const char *name, uint32_t value) override;

      void report_ar_node_duration(std::ostream &os, const char *name, uint64_t value) override;
      void report_ar_node_string(std::ostream &os, const char *name, const char *value) override;
      void report_ar_node_time(std::ostream &os, const char *name, uint64_t value) override;
      void report_ar_node_state(std::ostream &os, const char *name, uint32_t state) override;
      void report_resource_list_start(std::ostream &os) override;
      void report_resource_list_finish(std::ostream &os) override;
      void report_resource_list_node_str(std::ostream &os, const char *name, const char *value) override;
      void report_resource_list_node_double(std::ostream &os, const char *name, double value) override;
      void report_resource_list_node_uint64(std::ostream &os, const char *name, uint64_t value) override;
      void report_resource_list_node_bool(std::ostream &os, const char *name, bool value) override;
      void report_ar_node_boolean(std::ostream &os, const char *name, bool value) override;
      void report_exec_queue_list_start(std::ostream &os) override;
      void report_exec_queue_list_finish(std::ostream &os) override;
      void report_exec_queue_list_node(std::ostream &os, const char *name, uint32_t value) override;
      void report_exec_binding_list_start(std::ostream &os) override;
      void report_exec_binding_list_finish(std::ostream &os) override;
      void report_exec_binding_list_node(std::ostream &os, const char *name, const char *value) override;
      void report_granted_parallel_environment_start(std::ostream &os) override;
      void report_granted_parallel_environment_finish(std::ostream &os) override;
      void report_granted_parallel_environment_node(std::ostream &os, const char *name, const char *slots_range) override;
      void report_mail_list_start(std::ostream &os) override;
      void report_mail_list_finish(std::ostream &os) override;
      void report_mail_list_node(std::ostream &os, const char *name, const char *host) override;
      void report_acl_list_start(std::ostream &os) override;
      void report_acl_list_finish(std::ostream &os) override;
      void report_acl_list_node(std::ostream &os, const char *name) override;
      void report_xacl_list_start(std::ostream &os) override;
      void report_xacl_list_finish(std::ostream &os) override;
      void report_xacl_list_node(std::ostream &os, const char *name) override;
   };
}