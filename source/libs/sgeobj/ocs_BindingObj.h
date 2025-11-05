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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <string>

#include "cull/cull.h"
#include "ocs_BindingType.h"
#include "ocs_BindingUnit.h"
#include "ocs_BindingStart.h"
#include "ocs_BindingStop.h"
#include "ocs_BindingStrategy.h"
#include "ocs_BindingInstance.h"

namespace ocs {
   class Binding {
   public:
      static lListElem *binding_get_or_create_elem(lListElem *parent, int nm, lList **answer);
      static void clear(lListElem *parent, int nm);
      static bool binding_was_requested(const lListElem *parent, int nm);
      static BindingType::Type binding_get_type(const lListElem *parent, int nm);
      static BindingUnit::Unit binding_get_unit(const lListElem *parent, int nm);
      static std::string binding_get_sort(const lListElem *parent, int nm);
      static BindingStart::Start binding_get_start(const lListElem *parent, int nm);
      static BindingStop::Stop binding_get_end(const lListElem *parent, int nm);
      static BindingStrategy::Strategy binding_get_strategy(const lListElem *parent, int nm);
      static std::string binding_get_filter(const lListElem *parent, int nm);
      static u_long32 binding_get_amount(const lListElem *parent, int nm);
      static BindingInstance::Instance binding_get_instance(const lListElem *parent, int nm);
      static void binding_set_missing_defaults(lListElem *parent, int nm);

   };
}
