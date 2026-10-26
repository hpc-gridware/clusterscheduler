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
      bool for_edit_;   ///< render the editable form, see QConfParameter::get_for_edit()
      bool for_export_; ///< render the export form, see QConfParameter::get_for_export()
      bool more_info_;  ///< the client's MORE_INFO, see QConfParameter::get_more_info()

   public:
      /** @brief Build a view for one procedure call
       * @param parameter what the call asked for
       */
      explicit QConfViewBase(const QConfParameter &parameter)
         : ProcedureView(parameter), output_format_(parameter.get_output_format()),
           line_breaks_(parameter.get_single_line() ? SP_LINE_BREAKS_OFF : SP_LINE_BREAKS_ON),
           for_edit_(parameter.get_for_edit()), for_export_(parameter.get_for_export()),
           more_info_(parameter.get_more_info()) {}
      ~QConfViewBase() override = default;

      /** @brief Write the names of the fetched objects
       *
       * Takes the descriptor since CS-2820, as show_object() and
       * show_object_list() always have: the JSON envelope of a list whose
       * elements are not a registered object type has to be named explicitly,
       * and the plain form of the reserved host groups renders matchers last.
       *
       * @param os     the stream to write to
       * @param list   the objects, already sorted
       * @param key_nm the field holding the name
       * @param desc   the descriptor of the type the names belong to
       */
      virtual void show_name_list(std::ostream &os, const lList *list, int key_nm,
                                  const qconf_object_desc &desc) = 0;

      /** @brief Write one object in the layout of its descriptor
       *
       * Virtual since CS-2816: a sub-procedure whose output the descriptor
       * cannot express overrides this. The host group JSON form is the first,
       * because it adds keys derived from the member list.
       *
       * @param os   the stream to write to
       * @param ep   the object
       * @param desc its row in the descriptor table
       */
      virtual void show_object(std::ostream &os, const lListElem *ep, const qconf_object_desc &desc);

      /** @brief Write several objects of one type
       * @param os   the stream to write to
       * @param list the objects, in the order they are to appear
       * @param desc the descriptor of their type
       */
      void show_object_list(std::ostream &os, const lList *list, const qconf_object_desc &desc);
   };
}
