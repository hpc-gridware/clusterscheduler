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

#include <atomic>
#include <memory>
#include <string>

#include "uti/ocs_Exception.h"
#include "cull/cull.h"

namespace ocs {
   /**
    * @brief An exception that carries an answer list to the catching code
    *
    * Deep in a request handler the answer list is often the only place an
    * error was recorded. Throwing this instead of returning an error code
    * hands the whole list, warnings included, to the catching code.
    *
    * The message is the text of the list's last error entry, copied when the
    * exception is constructed. The list itself is shared by all copies of the
    * exception: it is freed once, when the last copy is destroyed, and handed
    * out once, by the first take_answer_list() on any copy.
    */
   class AnswerException : public RuntimeError {
      /** @brief The answer list shared by all copies of one exception */
      struct Holder {
         std::atomic<lList *> answer_list; ///< the list, nullptr once it was taken

         /**
          * @brief Hold an answer list
          * @param list the list to hold; the holder owns it
          */
         explicit Holder(lList *list) : answer_list(list) {}

         /** @brief Free the list unless it was taken */
         ~Holder();

         Holder(const Holder &) = delete;
         Holder &operator=(const Holder &) = delete;
      };

      std::shared_ptr<Holder> m_holder; ///< shared by every copy of this exception

      /**
       * @brief Find the message of an answer list
       * @param answer_list an AN_Type list, may be nullptr
       * @return the text of the last entry of quality error or critical, or a
       *         fixed text if there is none
       */
      static std::string get_last_error_message(const lList *answer_list);

   public:
      /**
       * @brief Take ownership of an answer list and carry it up to the catching code
       * @param[in,out] answer_list the list to carry; the caller's pointer is cleared
       */
      explicit AnswerException(lList **answer_list);

      /**
       * @brief Hand out the answer list
       *
       * The first call on any copy of the exception returns the list, and
       * the caller owns it from then on. Every later call, on this copy or
       * another, returns nullptr.
       *
       * @return the answer list, or nullptr if it was already taken
       */
      lList *take_answer_list() noexcept;

      /** @brief Destroy this copy; the last copy frees the list unless it was taken */
      ~AnswerException() override;
   };
}
