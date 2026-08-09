#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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
 * @brief TODO describe this file
 */

#include <utility>

#include "ocs_ReportingFileWriter.h"

namespace ocs {
   /** @brief What the two accounting writers share
    *
    * An accounting record may be worth flushing immediately rather than on the
    * timer: a site that reads the file to bill on wants the record there when
    * the job ends, not up to a minute later.
    */
   class BaseAccountingFileWriter : public ReportingFileWriter {
   protected:
      bool accounting_immediate_flush;   ///< Flush after every record rather than on the timer
   public:
      /** @brief Build an accounting writer
       * @param filename the file to append to
       * @param write_comment_header whether to start it with a column header
       */
      explicit BaseAccountingFileWriter(std::string filename, bool write_comment_header)
      : ReportingFileWriter(std::move(filename), write_comment_header),
         accounting_immediate_flush(false) {
      }

      void update_config() override;
   };
}
