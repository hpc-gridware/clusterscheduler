/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "uti/sge_mtutil.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_RequestLimits.h"

#include "msg_qmaster.h"
#include "msg_common.h"

// The one and ony instance
static const char *RequestLimitMutexName = "RequestLimitMutex";
ocs::RequestLimits *ocs::RequestLimits::instance = nullptr;
pthread_mutex_t ocs::RequestLimits::mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief Returns the instance of the RequestLimits class.
 * @return Reference to the RequestLimits instance.
 */
ocs::RequestLimits&
ocs::RequestLimits::get_instance() {
   sge_mutex_lock(RequestLimitMutexName, __func__, __LINE__,  &mutex);

   // create the instance if it does not exist
   if (instance == nullptr) {
      instance = new RequestLimits();
   }

   sge_mutex_unlock(RequestLimitMutexName, __func__, __LINE__,  &mutex);

   // return the instance as reference
   return *instance;
}

bool
ocs::RequestLimits::parse(const std::string &request_limits, lList **answer_list) {
   DENTER(TOP_LAYER);
   if (strcasecmp(request_limits.c_str(), "none") != 0) {
      answer_list_add_sprintf(answer_list, STATUS_ELIMIT, ANSWER_QUALITY_ERROR, MSG_REQLIMIT_NOTSUPPORTED);
      answer_list_add_sprintf(answer_list, STATUS_ELIMIT, ANSWER_QUALITY_ERROR, MSG_CONTACT_HPC_GRIDWARE);
      DRETURN(false);
   }
   DRETURN(true);
}

bool
ocs::RequestLimits::parse_from_config(lList **answer_list) {
   DENTER(TOP_LAYER);
   DRETURN(false);
}

bool
ocs::RequestLimits::will_exceed_limit(gdi::Packet *packet, gdi::Task *task, lList **answer_list) {
   DENTER(TOP_LAYER);
   DRETURN(false);
}
