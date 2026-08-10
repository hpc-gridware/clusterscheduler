#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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
 * @brief What the two reporting file writers share
 */

#include <utility>

#include "ocs_ReportingFileWriter.h"

namespace ocs {
   /** @brief What the two reporting writers share
    *
    * Reporting files, unlike accounting files, also carry the share tree
    * snapshot, which is written on its own interval rather than per event.
    */
   class BaseReportingFileWriter : public ReportingFileWriter {
   protected:
      bool do_joblog;             ///< Whether job life-cycle records are wanted
      bool log_consumables;       ///< Whether consumable usage is recorded
      uint64_t sharelog_interval; ///< How often a share tree snapshot is written; 0 for never
      uint64_t next_sharelog;     ///< When the next snapshot falls due
   public:
      /** @brief Build a reporting writer
       * @param filename the file to append to
       * @param write_comment_header whether to start it with a column header
       */
      explicit BaseReportingFileWriter(std::string filename, bool write_comment_header)
      : ReportingFileWriter(std::move(filename), write_comment_header),
         do_joblog(false), log_consumables(false), sharelog_interval(0), next_sharelog(0) {
      }

      uint64_t
      trigger(monitoring_t *monitor) override;

      void update_config() override;

      /** @brief Write a share tree snapshot in this writer's format
       * @param monitor the thread monitoring
       */
      virtual void
      create_sharelog_record(monitoring_t *monitor) = 0;
   };
}
