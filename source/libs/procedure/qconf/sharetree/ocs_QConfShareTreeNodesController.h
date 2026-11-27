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
 * @brief Driving the rendering of `qconf -sstnode` and `-rsstnode` (CS-2820)
 */

#include "../ocs_QConfModelBase.h"
#include "ocs_QConfShareTreeView.h"

namespace ocs {
   /** @brief `qconf -sstnode` and `-rsstnode`: hand the asked-for paths to the view
    *
    * The two switches share this controller because they render the same
    * thing, one path per node. They differ in depth as documented: only
    * `-rsstnode` descends into the child nodes, `-sstnode` writes the named
    * nodes alone (CS-572).
    *
    * @ingroup libprocedure
    */
   class QConfShareTreeNodesController {
   protected:
      std::ostream &out_;   ///< where the view writes

   public:
      explicit QConfShareTreeNodesController(std::ostream &out) : out_(out) {}

      /** @brief Write one path per node for every path that was asked for
       * @param parameter the invocation; its switch decides whether the child nodes are
       *                  written, the paths were taken apart by the model
       * @param model     the snapshot, holding the share tree and the paths
       * @param view      the rendering
       * @return none
       */
      void process_request(QConfParameter &parameter, QConfModelBase &model, QConfShareTreeView &view) {
         const bool recursive = (parameter.get_operation().token == "-rsstnode");
         view.show_paths(out_, model.get_sharetree_root(), model.get_sharetree_paths(), recursive);
      }
   };
}
