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

#include "sgeobj/ocs_BindingObj.h"
#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "ocs_Binding.h"
#include "ocs_BindingType.h"
#include "ocs_AdvanceReservation.h"
#include "sge_answer.h"

#include "msg_common.h"
#include "sge_conf.h"

lListElem *
ocs::Binding::binding_get_or_create_elem(lListElem *parent, const int nm, lList**answer) {
   DENTER(TOP_LAYER);
   if (parent == nullptr) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFN, MSG_PARSE_NULLPOINTERRECEIVED);
      answer_list_add(answer, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   // Check if the binding element already exists
   lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem != nullptr) {
      DRETURN(binding_elem);
   }

   binding_elem = lCreateElem(BN_Type);
   if (binding_elem == nullptr) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      answer_list_add(answer, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   // Set defaults. If not changed job-modify (qalter)  will be able to detect what was not set.
   lSetUlong(binding_elem, BN_new_type, BindingType::UNINITIALIZED);
   lSetUlong(binding_elem, BN_instance, BindingInstance::UNINITIALIZED);
   lSetUlong(binding_elem, BN_amount, -1);
   lSetUlong(binding_elem, BN_unit, BindingUnit::UNINITIALIZED);
   lSetString(binding_elem, BN_filter, nullptr);
   lSetString(binding_elem, BN_sort, nullptr);
   lSetUlong(binding_elem, BN_start, BindingStart::UNINITIALIZED);
   lSetUlong(binding_elem, BN_stop, BindingStop::UNINITIALIZED);
   lSetUlong(binding_elem, BN_strategy, BindingStrategy::UNINITIALIZED);

   lSetObject(parent, nm, binding_elem);
   DRETURN(binding_elem);
}

bool
ocs::Binding::binding_was_requested(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(false);
   }
   DRETURN(true);
}

ocs::BindingType::Type
ocs::Binding::binding_get_type(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   constexpr BindingType::Type default_type = BindingType::SLOT; // default binding type is slot
   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(default_type);
   }

   const auto binding_type = static_cast<BindingType::Type>(lGetUlong(binding_elem, BN_new_type));
   if (binding_type == BindingType::NONE || binding_type == BindingType::UNINITIALIZED) {
      DRETURN(default_type);
   }

   DRETURN(binding_type);
}

ocs::BindingUnit::Unit
ocs::Binding::binding_get_unit(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   const BindingUnit::Unit default_binding_unit = mconf_get_default_binding_unit();
   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(default_binding_unit);
   }

   const auto binding_unit = static_cast<BindingUnit::Unit>(lGetUlong(binding_elem, BN_unit));
   if (binding_unit == BindingUnit::NONE || binding_unit == BindingUnit::UNINITIALIZED) {
      DRETURN(default_binding_unit);
   }

   DRETURN(binding_unit);
}

std::string
ocs::Binding::binding_get_sort(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(NONE_STR);
   }

   const char *binding_sort = lGetString(binding_elem, BN_sort);
   if (binding_sort == nullptr) {
      DRETURN(NONE_STR);
   }

   DRETURN(binding_sort);
}

ocs::BindingStart::Start
ocs::Binding::binding_get_start(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   constexpr BindingStart::Start default_start = BindingStart::NONE;
   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(default_start);
   }

   const auto binding_start = static_cast<BindingStart::Start>(lGetUlong(binding_elem, BN_start));
   if (binding_start == BindingStart::NONE || binding_start == BindingStart::UNINITIALIZED) {
      DRETURN(default_start);
   }
   DRETURN(binding_start);
}

ocs::BindingStop::Stop
ocs::Binding::binding_get_end(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   constexpr BindingStop::Stop default_end = BindingStop::NONE;
   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(default_end);
   }

   const auto binding_end = static_cast<BindingStop::Stop>(lGetUlong(binding_elem, BN_stop));
   if (binding_end == BindingStop::NONE || binding_end == BindingStop::UNINITIALIZED) {
      DRETURN(default_end);
   }
   DRETURN(binding_end);
}

ocs::BindingStrategy::Strategy
ocs::Binding::binding_get_strategy(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   constexpr BindingStrategy::Strategy default_strategy = BindingStrategy::PACKED; // default binding strategy is slot
   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(default_strategy);
   }

   const auto binding_strategy = static_cast<BindingStrategy::Strategy>(lGetUlong(binding_elem, BN_strategy));
   if (binding_strategy == BindingStrategy::NONE || binding_strategy == BindingStrategy::UNINITIALIZED) {
      DRETURN(default_strategy);
   }
   DRETURN(binding_strategy);
}

ocs::BindingInstance::Instance
ocs::Binding::binding_get_instance(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   constexpr BindingInstance::Instance default_instance = BindingInstance::SET; // default binding instance is SET
   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(default_instance);
   }

   const auto binding_instance = static_cast<BindingInstance::Instance>(lGetUlong(binding_elem, BN_instance));
   if (binding_instance == BindingInstance::NONE || binding_instance == BindingInstance::UNINITIALIZED) {
      DRETURN(default_instance);
   }
   DRETURN(binding_instance);
}

std::string
ocs::Binding::binding_get_filter(const lListElem *parent, int nm) {
   DENTER(TOP_LAYER);

   const lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN(NONE_STR);
   }
   DRETURN(lGetString(binding_elem, BN_filter));
}

u_long32
ocs::Binding::binding_get_amount(const lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   const lListElem *binding_elem = lGetObject(parent, nm);
   u_long32 amount = lGetUlong(binding_elem, BN_amount);
   if (amount == static_cast<u_long32>(-1)) {
      amount = 0;
   }
   DRETURN(amount);
}

void ocs::Binding::binding_set_missing_defaults(lListElem *parent, const int nm) {
   DENTER(TOP_LAYER);

   // if the job has no new binding, we do not need to set defaults
   if (parent == nullptr) {
      DRETURN_VOID;
   }

   // no binding element, so we do not need to set defaults for binding
   lListElem *binding_elem = lGetObject(parent, nm);
   if (binding_elem == nullptr) {
      DRETURN_VOID;
   }

   // ensure that the binding element is set to nullptr if the amount is 0
   if (lGetUlong(binding_elem, BN_amount) == 0) {
      lSetObject(parent, JB_binding, nullptr);
      DRETURN_VOID;
   }

   // ensure that the binding element is initialized with default values
   auto instance = static_cast<BindingInstance::Instance>(lGetUlong(binding_elem, BN_instance));
   if (instance == BindingInstance::Instance::UNINITIALIZED ||
       instance == BindingInstance::Instance::NONE) {
      lSetUlong(binding_elem, BN_instance, BindingInstance::Instance::SET);
   }
   auto unit = static_cast<BindingUnit::Unit>(lGetUlong(binding_elem, BN_unit));
   if (unit == BindingUnit::Unit::UNINITIALIZED ||
       unit == BindingUnit::Unit::NONE) {
      const BindingUnit::Unit default_binding_unit = mconf_get_default_binding_unit();
      lSetUlong(binding_elem, BN_unit, default_binding_unit);
   }
   auto type = static_cast<BindingType::Type>(lGetUlong(binding_elem, BN_new_type));
   if (type == BindingType::Type::UNINITIALIZED ||
       type == BindingType::Type::NONE) {
      lSetUlong(binding_elem, BN_new_type, BindingType::Type::SLOT);
   }
   const char *filter = lGetString(binding_elem, BN_filter);
   if (filter == nullptr) {
      lSetString(binding_elem, BN_filter, NONE_STR);
   }
   const char *sort = lGetString(binding_elem, BN_sort);
   if (sort == nullptr) {
      lSetString(binding_elem, BN_sort, NONE_STR);
   }
   auto start = static_cast<BindingStart::Start>(lGetUlong(binding_elem, BN_start));
   if (start == BindingStart::Start::UNINITIALIZED) {
      lSetUlong(binding_elem, BN_start, BindingStart::Start::NONE);
   }
   auto end = static_cast<BindingStop::Stop>(lGetUlong(binding_elem, BN_stop));
   if (end == BindingStop::Stop::UNINITIALIZED) {
      lSetUlong(binding_elem, BN_stop, BindingStop::Stop::NONE);
   }
   auto strategy = static_cast<BindingStrategy::Strategy>(lGetUlong(binding_elem, BN_strategy));
   if (strategy == BindingStrategy::Strategy::UNINITIALIZED ||
       strategy == BindingStrategy::Strategy::NONE) {
      lSetUlong(binding_elem, BN_strategy, BindingStrategy::Strategy::PACKED);
   }
}
