/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Carrying a cull answer list as a C++ exception
 */

#include "uti/sge_rmon_macros.h"

#include "ocs_AnswerException.h"

#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"

ocs::AnswerException::Holder::~Holder() {
   lList *list = answer_list.exchange(nullptr);
   lFreeList(&list);
}

/**
 * @brief Find the message of an answer list
 * @param answer_list an answer list, may be an empty handle
 * @return the text of the last entry of quality error or critical, or a
 *         fixed text if there is none
 */
std::string
ocs::AnswerException::get_last_error_message(const ocs::tom::ConstList<ocs::tom::AN> answer_list) {
   DENTER(TOP_LAYER);
   for (auto answer = answer_list.back(); answer; answer = answer.prev()) {
      if (answer_has_quality(answer, ANSWER_QUALITY_ERROR) || answer_has_quality(answer, ANSWER_QUALITY_CRITICAL)) {
         const char *message = ocs::tom::get<AN_text>(answer);
         if (message != nullptr) {
            DRETURN(message);
         }
      }
   }
   DRETURN("No error details available");
}

ocs::AnswerException::AnswerException(ocs::tom::ListPtr<ocs::tom::AN> *answer_list)
   : RuntimeException(get_last_error_message(*answer_list)),
     m_holder(std::make_shared<Holder>(answer_list->release())) {
   // the holder owns the list from here on
}

ocs::tom::ListPtr<ocs::tom::AN>
ocs::AnswerException::take_answer_list() noexcept {
   return ocs::tom::ListPtr<ocs::tom::AN>::from_typed(m_holder->answer_list.exchange(nullptr));
}

ocs::AnswerException::~AnswerException() = default;
