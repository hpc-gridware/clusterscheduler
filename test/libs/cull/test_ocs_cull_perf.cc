/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <iostream>

#include "sgeobj/sge_job.h"

int main() {
   auto *elem = lCreateElem(JB_Type);

   // Create elements and store in vector
   for (uint64_t l = 0; l <= 1000000000; ++l) {

      lGetUlong(elem, JB_job_number);
#if 0
      lSetString(elem, JB_job_name, std::to_string(l).c_str());
      lSetUlong64(elem, JB_submission_time, 8724364UL + l);
      lSetBool(elem, JB_notify, true);
#endif

      //lList *jobs = lCreateList("", JB_Type);
      //lAppendElem(jobs, elem);
   }

}











































