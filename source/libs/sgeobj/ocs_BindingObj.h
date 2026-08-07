#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The binding request stored on a job
 */

#include <string>

#include "cull/cull.h"
#include "ocs_BindingType.h"
#include "ocs_BindingUnit.h"
#include "ocs_BindingStart.h"
#include "ocs_BindingStop.h"
#include "ocs_BindingStrategy.h"
#include "ocs_BindingInstance.h"

namespace ocs {
   /**
    * @brief Reading a binding request out of any object that can carry one
    *
    * The field number makes this generic: @ref ocs::Job and
    * @ref ocs::AdvanceReservation are thin wrappers that pass their own field.
    */
   class Binding {
   public:
      /**
       * @brief The binding sub-object, created on first access
       * @param parent the object to read or extend
       * @param[out] answer_list receives the reason when it could not be created
       * @param nm the field the binding sub-object lives in
       * @return the binding sub-object, or nullptr on error
       */
      static lListElem *binding_get_or_create_elem(lListElem *parent, lList **answer_list, int nm);
      /**
       * @brief Remove the binding request from an object
       * @param parent the object to clear
       * @param nm the field the binding sub-object lives in
       */
      static void clear(lListElem *parent, int nm);
      /**
       * @brief Was a binding requested at all?
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return true when the object carries a binding request
       */
      static bool binding_was_requested(const lListElem *parent, int nm);
      /**
       * @brief Who applies the binding
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested @ref ocs::BindingType::Type
       */
      static BindingType::Type binding_get_type(const lListElem *parent, int nm);
      /**
       * @brief The hardware unit the binding counts in
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested @ref ocs::BindingUnit::Unit
       */
      static BindingUnit::Unit binding_get_unit(const lListElem *parent, int nm);
      /**
       * @brief How the selected hardware is ordered
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the sort specification, empty when none was given
       */
      static std::string binding_get_sort(const lListElem *parent, int nm);
      /**
       * @brief Where on the topology the binding starts
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested @ref ocs::BindingStart::Start
       */
      static BindingStart::Start binding_get_start(const lListElem *parent, int nm);
      /**
       * @brief Where the binding stops
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested @ref ocs::BindingStop::Stop
       */
      static BindingStop::Stop binding_get_end(const lListElem *parent, int nm);
      /**
       * @brief How the binding walks the topology
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested @ref ocs::BindingStrategy::Strategy
       */
      static BindingStrategy::Strategy binding_get_strategy(const lListElem *parent, int nm);
      /**
       * @brief Which hardware the binding is restricted to
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the filter expression, empty when none was given
       */
      static std::string binding_get_filter(const lListElem *parent, int nm);
      /**
       * @brief How many units the binding asks for
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested amount
       */
      static uint32_t binding_get_amount(const lListElem *parent, int nm);
      /**
       * @brief Which instance applies the binding
       *
       * @param parent the object carrying the binding request
       * @param nm the field the binding sub-object lives in
       * @return the requested @ref ocs::BindingInstance::Instance
       */
      static BindingInstance::Instance binding_get_instance(const lListElem *parent, int nm);
      /**
       * @brief Fill in the binding fields the request left out
       * @param parent the object whose binding request is completed
       * @param[out] answer_list receives the reason on failure
       * @param nm the field the binding sub-object lives in
       */
      static void binding_set_missing_defaults(lListElem *parent, lList **answer_list, int nm);

   };
}
