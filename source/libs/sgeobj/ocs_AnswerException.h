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
 * @brief Carrying a cull answer list as a C++ exception
 */

#include <string>

#include "uti/ocs_Exception.h"
#include "cull/cull.h"

#include "sgeobj/sge_answer.h"

namespace ocs {
   /**
    * @brief An exception that carries an answer list to the catching code
    *
    * Deep in a request handler the answer list is often the only place an
    * error was recorded. Throwing this instead of returning an error code
    * hands both the list and its last message to the caller unchanged.
    */
   class AnswerException : public Exception {
      lList *m_answer_list = nullptr; //< answer list to be transferred to the catching code

      /** @brief returns the last message in the answer list
       *
       * @param answer_list An AN_Type list
       * @return String with the last message
       */
      static std::string get_last_message(const lList *answer_list) {
         const lListElem *last;
         if (answer_list == nullptr || (last = lLast(answer_list)) == nullptr) {
            return "No error details available";
         }
         const char *message = lGetString(last, AN_text);
         return message;
      }
   public:
      /**
       * @brief Take ownership of an answer list and carry it up to the catching code
       * @param[in,out] answer_list the list to carry; the caller's pointer is cleared
       */
      explicit AnswerException(lList **answer_list) : Exception(get_last_message(*answer_list)), m_answer_list(*answer_list) {
         // this class takes ownership for the CULL list
         *answer_list = nullptr;
      };

      ~AnswerException() override {
         lFreeList(&m_answer_list);
      }

   };
}