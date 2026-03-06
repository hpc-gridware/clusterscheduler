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

#include "basis_types.h"

#include "ocs_qhost_print.h"

namespace ocs {
   class QHostReportHandlerBase {
      u_long32 full_listing_;
   protected:
      size_t indent_ = 0;
   public:
      [[nodiscard]] bool show_binding() const {
         return (full_listing_ & QHOST_DISPLAY_BINDING) == QHOST_DISPLAY_BINDING;
      }
   public:
      QHostReportHandlerBase(u_long32 full_listing) : full_listing_(full_listing) {}
      virtual ~QHostReportHandlerBase() = default;

      virtual void start(std::ostream &os) = 0;
      virtual void end(std::ostream &os) = 0;

      virtual void host_start(std::ostream &os, const char *host_name) = 0;
      virtual void host_end(std::ostream &os) = 0;
      virtual void host_value(std::ostream &os, const char *format_str, const char *name, const char *value) = 0;
      virtual void host_value(std::ostream &os, const char *format_str, const char* name, u_long32 value) = 0;

      virtual void queue_start(std::ostream &os, const char *format_str, const char* qname) = 0;
      virtual void queue_end(std::ostream &os) = 0;
      virtual void queue_value(std::ostream &os, const char *qname, const char *format_str, const char* name, const char *value) = 0;
      virtual void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, u_long32 value) = 0;

      // @todo jid why as string
      virtual void job_start(std::ostream &os, const char *format_str, u_long32 jid) = 0;
      virtual void job_end(std::ostream &os) = 0;
      virtual void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, const char *value) = 0;
      virtual void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, u_long64 value) = 0;
      virtual void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, double value) = 0;

      virtual void resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details) = 0;
   };
}
