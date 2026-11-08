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

#include "ocs_AnswerException.h"

#include "sgeobj/sge_answer.h"

ocs::AnswerException::Holder::~Holder() {
   lList *list = answer_list.exchange(nullptr);
   lFreeList(&list);
}

std::string
ocs::AnswerException::get_last_error_message(const lList *answer_list) {
   for (const lListElem *answer = lLast(answer_list); answer != nullptr; answer = lPrev(answer)) {
      if (answer_has_quality(answer, ANSWER_QUALITY_ERROR) || answer_has_quality(answer, ANSWER_QUALITY_CRITICAL)) {
         const char *message = lGetString(answer, AN_text);
         if (message != nullptr) {
            return message;
         }
      }
   }
   return "No error details available";
}

ocs::AnswerException::AnswerException(lList **answer_list)
   : RuntimeError(get_last_error_message(*answer_list)), m_holder(std::make_shared<Holder>(*answer_list)) {
   // the holder owns the list from here on
   *answer_list = nullptr;
}

lList *
ocs::AnswerException::take_answer_list() noexcept {
   return m_holder->answer_list.exchange(nullptr);
}

ocs::AnswerException::~AnswerException() = default;
