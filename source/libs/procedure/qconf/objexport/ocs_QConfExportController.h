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
 * @brief Rendering one file per object for `qconf -S<obj>` (CS-2826)
 */

#include <ostream>

#include "../ocs_QConfController.h"
#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -S<obj>`: render each object into a text of its own
    *
    * The only read whose result is not one text. An export writes one file per
    * object, so what the caller needs is a text per object together with the
    * name that names its file - and the one place that can pair the two is the
    * side that renders, because after the rendering the boundaries are gone.
    *
    * The pairs are therefore collected here rather than written to the output
    * stream, and the response carries them as a structured entry, see
    * ProcedureParameter::EXPORT_OBJECTS. The stream stays empty on purpose: an
    * export has nothing to say on the terminal that the client does not say
    * itself, one line per file it wrote.
    *
    * @ingroup libprocedure
    */
   class QConfExportController : public QConfController {
   protected:
      lList *objects_ = nullptr;   ///< `VA_Type` pairs: object name -> file text

   public:
      /** @brief Bind a controller to an output stream
       * @param out the stream, unused by this controller - see the class comment
       */
      explicit QConfExportController(std::ostream &out) : QConfController(out) {}

      /** @brief Free what was not taken
       *
       * The list is handed over with take_export_objects() when the response is
       * built. A call that failed before that never asks, so the controller owns
       * it to the end of the request either way.
       */
      ~QConfExportController() override { lFreeList(&objects_); }

      void process_request(QConfParameter &parameter, QConfModelBase &model,
                           QConfViewBase &view) override;

      /** @brief Hand the rendered pairs to the caller
       *
       * Ownership moves with them, because the response bundle takes the list as
       * it is rather than copying it.
       *
       * @return the pairs, nullptr when nothing was rendered
       */
      lList *take_export_objects() {
         lList *taken = objects_;
         objects_ = nullptr;
         return taken;
      }
   };
}
