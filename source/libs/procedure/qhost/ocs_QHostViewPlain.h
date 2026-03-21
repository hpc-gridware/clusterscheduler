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

#include <basis_types.h>

#include "ocs_QHostViewBase.h"
#include "ocs_QHostParameter.h"

namespace ocs {
   class QHostViewPlain : public QHostViewBase {
      bool print_host_header = true;
      bool print_job_header = true;
   public:
      explicit QHostViewPlain(const QHostParameter &parameter) : QHostViewBase(parameter) {}
      ~QHostViewPlain() override = default;

      void start(std::ostream &os) override;
      void end(std::ostream &os) override;

      void host_start(std::ostream &os, const char *host_name) override;
      void host_end(std::ostream &os) override;
      void host_value(std::ostream &os, const char *format_str, const char *name, const char *value) override;
      void host_value(std::ostream &os, const char *format_str, const char* name, u_long32 value) override;

      void queue_start(std::ostream &os, const char *format_str, const char* qname) override;
      void queue_end(std::ostream &os) override;
      void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const char *value) override;
      void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, u_long32 value) override;

      void job_start(std::ostream &os, const char *format_str, u_long32 jid) override;
      void job_end(std::ostream &os) override;
      void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, const char *value) override;
      void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, u_long64 value) override;
      void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, double value) override;

      void resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details) override;
   };
}
