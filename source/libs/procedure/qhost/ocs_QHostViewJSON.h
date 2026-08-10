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
 * @brief JSON rendering of `qhost`
 */

#include <iosfwd>

#include "ocs_QHostParameter.h"
#include "ocs_QHostViewBase.h"

namespace ocs {
   /** @brief Renders `qhost` output as JSON
    *
    * JSON needs more state than the other two formats: an array has to be
    * opened before its first element and closed after the last, and a comma
    * belongs between elements but not before the first. The traversal does not
    * announce "this is the last host", so the view remembers what it has
    * already opened and closes it when the next level starts or ends - that is
    * what the `*_open` flags are for.
    *
    * @ingroup libprocedure
    */
   class QHostViewJSON : public QHostViewBase {
      int indent = 0;                     ///< Current indentation depth
      bool host_list_open = false;        ///< The array of hosts has been opened
      bool queue_list_open = false;       ///< The array of queues of the current host has been opened
      bool job_list_open = false;         ///< The array of jobs of the current queue has been opened
      bool resource_list_open = false;    ///< The array of resources of the current host has been opened
      bool host_open = false;             ///< A host object is still open
      bool queue_open = false;            ///< A queue object is still open
      bool job_open = false;              ///< A job object is still open
   public:
      /** @brief Build the JSON view
       * @param parameter the call's parameters
       */
      explicit QHostViewJSON(const QHostParameter &parameter) : QHostViewBase(parameter) {}
      ~QHostViewJSON() override = default;

      void start(std::ostream &os) override;
      void end(std::ostream &os) override;

      void host_start(std::ostream &os, const char *host_name) override;
      void host_end(std::ostream &os) override;
      void host_value(std::ostream &os, const char *format_str, const char *name, const char *value) override;
      void host_value(std::ostream &os, const char *format_str, const char* name, uint64_t value) override;
      void host_value(std::ostream &os, const char *format_str, const char* name, double value) override;

      void queue_start(std::ostream &os, const char *format_str, const char* qname) override;
      void queue_end(std::ostream &os) override;
      void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const char *value) override;
      void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, uint32_t value) override;

      void job_start(std::ostream &os, const char *format_str, uint32_t jid) override;
      void job_end(std::ostream &os) override;
      void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, const char *value) override;
      void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, uint64_t value, bool as_timestamp) override;
      void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, double value) override;

      void resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details, bool as_string) override;
      void resource_value(std::ostream &os, const char* dominance, const char* name, uint64_t value, const char *details, bool as_string) override;
      void resource_value(std::ostream &os, const char* dominance, const char* name, double value, const char *details, bool as_string) override;
   };
}