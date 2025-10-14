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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/ocs_Systemd.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_log.h"

#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingEnd.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "ocs_Binding.h"
#include "ocs_BindingType.h"
#include "ocs_AdvanceReservation.h"
#include "sge_answer.h"

#include "msg_common.h"
#include "ocs_BindingObj.h"

lListElem *
ocs::AdvanceReservation::binding_get_or_create_elem(lListElem *ar, lList**answer) {
   return Binding::binding_get_or_create_elem(ar, AR_binding, answer);
}

bool
ocs::AdvanceReservation::binding_was_requested(const lListElem *ar) {
   return Binding::binding_was_requested(ar, AR_binding);
}

ocs::BindingType::Type
ocs::AdvanceReservation::binding_get_type(const lListElem *ar) {
   return Binding::binding_get_type(ar, AR_binding);
}

ocs::BindingUnit::Unit
ocs::AdvanceReservation::binding_get_unit(const lListElem *ar) {
   return Binding::binding_get_unit(ar, AR_binding);
}

std::string
ocs::AdvanceReservation::binding_get_sort(const lListElem *ar) {
   return Binding::binding_get_sort(ar, AR_binding);
}

ocs::BindingStart::Start
ocs::AdvanceReservation::binding_get_start(const lListElem *ar) {
   return Binding::binding_get_start(ar, AR_binding);
}

ocs::BindingStop::Stop
ocs::AdvanceReservation::binding_get_stop(const lListElem *ar) {
   return Binding::binding_get_end(ar, AR_binding);
}

ocs::BindingStrategy::Strategy
ocs::AdvanceReservation::binding_get_strategy(const lListElem *ar) {
   return Binding::binding_get_strategy(ar, AR_binding);
}

ocs::BindingInstance::Instance
ocs::AdvanceReservation::binding_get_instance(const lListElem *ar) {
   return Binding::binding_get_instance(ar, AR_binding);
}

std::string
ocs::AdvanceReservation::binding_get_filter(const lListElem *ar) {
   return Binding::binding_get_filter(ar, AR_binding);
}

u_long32
ocs::AdvanceReservation::binding_get_amount(const lListElem *ar) {
   return Binding::binding_get_amount(ar, AR_binding);
}

void ocs::AdvanceReservation::binding_set_missing_defaults(lListElem *ar) {
   Binding::binding_set_missing_defaults(ar, AR_binding);
}
