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
 * @brief Rendering what a `qconf` read operation fetched
 */

#include <ostream>

#include "cull/cull.h"
#include "spool/flatfile/sge_flatfile.h"

#include "ocs_ProcedureView.h"
#include "ocs_QConfObject.h"
#include "ocs_QConfParameter.h"

namespace ocs {
   /** @brief Renders the result of one `qconf` read operation
    *
    * `qconf` has two output formats, `-fmt plain` and `-fmt json`, and no XML -
    * QConfViewXML exists only because the traits struct in the server dispatch
    * asks every procedure for one. A call that somehow requests XML is refused
    * in QConfModelBase::make_snapshot(), where there is an answer list to say
    * so in; by the time a view is chosen it is too late to report anything.
    *
    * @ingroup libprocedure
    */
   class QConfViewBase : public ProcedureView {
   protected:
      /** @brief The format this call renders in
       *
       * ProcedureView takes the parameter and keeps nothing, so the one thing
       * the qconf views need from it is kept here.
       */
      ProcedureParameter::OutputFormat output_format_;
      spool_flatfile_line_breaks line_breaks_;   ///< the client's SGE_SINGLE_LINE, see QConfParameter::get_single_line()

   public:
      /** @brief Build a view for one procedure call
       * @param parameter what the call asked for
       */
      explicit QConfViewBase(const QConfParameter &parameter)
         : ProcedureView(parameter), output_format_(parameter.get_output_format()),
           line_breaks_(parameter.get_single_line() ? SP_LINE_BREAKS_OFF : SP_LINE_BREAKS_ON) {}
      ~QConfViewBase() override = default;

      /** @brief Write the names of the fetched objects
       * @param os     the stream to write to
       * @param list   the objects, already sorted
       * @param key_nm the field holding the name
       */
      virtual void show_name_list(std::ostream &os, const lList *list, int key_nm) = 0;

      void show_object(std::ostream &os, const lListElem *ep, const qconf_object_desc &desc);
   };
}
