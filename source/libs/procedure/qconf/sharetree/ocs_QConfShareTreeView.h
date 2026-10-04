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
 * @brief The rendering of `qconf -sst`, `-bonsai`, `-sstnode` and `-rsstnode` (CS-2820)
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

#include "sgeobj/sge_sharetree.h"

namespace ocs {
   /** @brief `qconf -sst`, `-bonsai`, `-sstnode`, `-rsstnode`: the share tree
    *
    * Two renderings of one object, which is why they share a view: the indented
    * tree of `-sst` and `-bonsai`, and the one-path-per-node form of `-sstnode`
    * and `-rsstnode`. Both renderers already live in `libs/sgeobj`, so nothing
    * about the drawing had to move - only the fetch and the path checking did.
    *
    * `-fmt json` is not honoured, exactly as before: these four write their text
    * form whatever the format says. `-sstree` is the switch that renders the
    * share tree as an object, and it has had JSON since CS-2313a.
    *
    * @ingroup libprocedure
    */
   class QConfShareTreeView : public QConfViewBase {
   public:
      explicit QConfShareTreeView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfShareTreeView() override = default;

      /** @brief Not reached: the tree is written by show_tree() or show_paths() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      /** @brief Write the indented tree view of `-sst` and `-bonsai`
       * @param os   the stream to write to
       * @param root the root node
       */
      void show_tree(std::ostream &os, const lListElem *root) {
         show_sharetree(os, root, nullptr);
      }

      /** @brief Write one full path per node, for `-sstnode` and `-rsstnode`
       *
       * A path that names no node renders nothing; the model has already put the
       * reason into the answer list, which is where it has to come from now that
       * this runs in qmaster - the renderer's own stderr line would never reach
       * the user (CS-2820).
       *
       * @param os    the stream to write to
       * @param root  the root node
       * @param paths the paths that were asked for, in the order given
       */
      void show_paths(std::ostream &os, lListElem *root, const std::vector<std::string> &paths) {
         for (const auto &path : paths) {
            show_sharetree_path(os, root, path.c_str());
         }
      }
   };
}
