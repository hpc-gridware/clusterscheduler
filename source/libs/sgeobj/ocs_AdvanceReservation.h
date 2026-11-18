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
 * @brief Advance reservations: resources booked for a future time window
 */

#include <string>

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "ocs_BindingType.h"
#include "ocs_BindingUnit.h"
#include "ocs_BindingStart.h"
#include "ocs_BindingStop.h"
#include "ocs_BindingStrategy.h"
#include "ocs_BindingInstance.h"
#include "sgeobj/sge_answer.h"

namespace ocs {
   /**
    * @brief Operations on the advance reservation object
    *
    * The binding accessors mirror those on `ocs::Job`: a reservation can
    * carry the same binding request a job can, so the hardware it books is
    * described the same way.
    */
   class AdvanceReservation {
   public:
      static ocs::tom::Element<ocs::tom::BN> binding_get_or_create_elem(ocs::tom::Element<ocs::tom::AR> ar,
                                                                        ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
      static BindingType::Type binding_get_type(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static BindingUnit::Unit binding_get_unit(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static std::string binding_get_sort(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static BindingStart::Start binding_get_start(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static BindingStop::Stop binding_get_stop(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static BindingStrategy::Strategy binding_get_strategy(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static std::string binding_get_filter(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static uint32_t binding_get_amount(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static BindingInstance::Instance binding_get_instance(ocs::tom::ConstElement<ocs::tom::AR> ar);
      static void binding_set_missing_defaults(ocs::tom::Element<ocs::tom::AR> ar,
                                               ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
   };
}
