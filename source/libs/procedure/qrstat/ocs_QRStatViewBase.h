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

#include <ostream>

#include "cull/cull.h"

#include "ocs_QRStatParameter.h"

namespace ocs {
   class QRStatViewBase {
   public:
      bool show_summary = false;
   public:
      explicit QRStatViewBase(const QRStatParameter &parameter);
      virtual ~QRStatViewBase() = default;

      virtual void report_start(std::ostream &os) = 0;
      virtual void report_finish(std::ostream &os) = 0;
      virtual void report_start_ar(std::ostream &os) = 0;
      virtual void report_finish_ar(std::ostream &os) = 0;

      virtual void report_start_unknown_ar(std::ostream &os) = 0;
      virtual void report_finish_unknown_ar(std::ostream &os) = 0;
      virtual void report_ar_node_ulong_unknown(std::ostream &os, const char *name, u_long32 value) = 0;

      virtual void report_ar_node_ulong(std::ostream &os, const char *name, u_long32 value) = 0;
      virtual void report_ar_node_duration(std::ostream &os, const char *name, u_long64 value) = 0;
      virtual void report_ar_node_string(std::ostream &os, const char *name, const char *value) = 0;
      virtual void report_ar_node_time(std::ostream &os, const char *name, u_long64 value) = 0;
      virtual void report_ar_node_state(std::ostream &os, const char *name, u_long32 state) = 0;
      virtual void report_start_resource_list(std::ostream &os) = 0;
      virtual void report_finish_resource_list(std::ostream &os) = 0;
      virtual void report_resource_list_node(std::ostream &os, const char *name, const char *value) = 0;
      virtual void report_ar_node_boolean(std::ostream &os, const char *name, bool value) = 0;
      virtual void report_start_exec_queue_list(std::ostream &os) = 0;
      virtual void report_finish_exec_queue_list(std::ostream &os) = 0;
      virtual void report_exec_queue_list_node(std::ostream &os, const char *name, u_long32 value) = 0;
      virtual void report_start_exec_binding_list(std::ostream &os) = 0;
      virtual void report_finish_exec_binding_list(std::ostream &os) = 0;
      virtual void report_exec_binding_list_node(std::ostream &os, const char *name, const char *value) = 0;
      virtual void report_start_granted_parallel_environment(std::ostream &os) = 0;
      virtual void report_finish_granted_parallel_environment(std::ostream &os) = 0;
      virtual void report_granted_parallel_environment_node(std::ostream &os, const char *name, const char *slots_range) = 0;
      virtual void report_start_mail_list(std::ostream &os) = 0;
      virtual void report_finish_mail_list(std::ostream &os) = 0;
      virtual void report_mail_list_node(std::ostream &os, const char *name, const char *host) = 0;
      virtual void report_start_acl_list(std::ostream &os) = 0;
      virtual void report_finish_acl_list(std::ostream &os) = 0;
      virtual void report_acl_list_node(std::ostream &os, const char *name) = 0;
      virtual void report_start_xacl_list(std::ostream &os) = 0;
      virtual void report_finish_xacl_list(std::ostream &os) = 0;
      virtual void report_xacl_list_node(std::ostream &os, const char *name) = 0;
      virtual void report_newline(std::ostream &os) = 0;
   };
}