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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The binding request stored on a job
 */

#include <cstring>
#include <utility>

#include "uti/ocs_Systemd.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_log.h"
#include "uti/sge.h"

#include "sgeobj/ocs_BindingObj.h"
#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "ocs_Binding.h"
#include "ocs_BindingType.h"
#include "ocs_AdvanceReservation.h"
#include "sge_answer.h"

#include "msg_common.h"
#include "sge_conf.h"

/**
 * @brief Remove the binding request from an object
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object to clear
 */
template <auto F>
void
ocs::Binding::clear(const ocs::tom::Element<Parent<F>> parent) {
   DENTER(TOP_LAYER);
   parent.template set<F>(nullptr);
   DRETURN_VOID;
}

/**
 * @brief The binding sub-object, created on first access
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object to read or extend
 * @param[out] answer_list receives the reason when it could not be created
 *
 * @return the binding sub-object, or an empty handle on error
 */
template <auto F>
ocs::tom::Element<ocs::tom::BN>
ocs::Binding::binding_get_or_create_elem(const ocs::tom::Element<Parent<F>> parent,
                                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);
   if (!parent) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFN, MSG_PARSE_NULLPOINTERRECEIVED);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   // Check if the binding element already exists
   if (const auto binding_elem = parent.template get<F>()) {
      DRETURN(binding_elem);
   }

   auto new_elem = ocs::tom::ElementPtr<ocs::tom::BN>::create();

   // Set defaults. If not changed job-modify (qalter)  will be able to detect what was not set.
   new_elem->set<BN_new_type>(BindingType::UNINITIALIZED);
   new_elem->set<BN_instance>(BindingInstance::UNINITIALIZED);
   new_elem->set<BN_amount>(0);
   new_elem->set<BN_unit>(BindingUnit::UNINITIALIZED);
   new_elem->set<BN_filter>(nullptr);
   new_elem->set<BN_sort>(nullptr);
   new_elem->set<BN_start>(BindingStart::UNINITIALIZED);
   new_elem->set<BN_stop>(BindingStop::UNINITIALIZED);
   new_elem->set<BN_strategy>(BindingStrategy::UNINITIALIZED);

   const auto binding_elem = new_elem.get();
   parent.template set<F>(std::move(new_elem));
   DRETURN(binding_elem);
}

/**
 * @brief Who applies the binding
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested @ref ocs::BindingType::Type
 */
template <auto F>
ocs::BindingType::Type
ocs::Binding::binding_get_type(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   constexpr BindingType::Type default_type = BindingType::SLOT; // default binding type is slot
   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(default_type);
   }

   const auto binding_type = static_cast<BindingType::Type>(binding_elem.template get<BN_new_type>());
   if (binding_type == BindingType::NONE || binding_type == BindingType::UNINITIALIZED) {
      DRETURN(default_type);
   }

   DRETURN(binding_type);
}

/**
 * @brief The hardware unit the binding counts in
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested @ref ocs::BindingUnit::Unit
 */
template <auto F>
ocs::BindingUnit::Unit
ocs::Binding::binding_get_unit(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   const BindingUnit::Unit default_binding_unit = mconf_get_default_binding_unit();
   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(default_binding_unit);
   }

   const auto binding_unit = static_cast<BindingUnit::Unit>(binding_elem.template get<BN_unit>());
   if (binding_unit == BindingUnit::NONE || binding_unit == BindingUnit::UNINITIALIZED) {
      DRETURN(default_binding_unit);
   }

   DRETURN(binding_unit);
}

/**
 * @brief Where on the topology the binding starts
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested @ref ocs::BindingStart::Start
 */
template <auto F>
ocs::BindingStart::Start
ocs::Binding::binding_get_start(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   constexpr BindingStart::Start default_start = BindingStart::NONE;
   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(default_start);
   }

   const auto binding_start = static_cast<BindingStart::Start>(binding_elem.template get<BN_start>());
   if (binding_start == BindingStart::NONE || binding_start == BindingStart::UNINITIALIZED) {
      DRETURN(default_start);
   }

   DRETURN(binding_start);
}

/**
 * @brief Where the binding stops
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested @ref ocs::BindingStop::Stop
 */
template <auto F>
ocs::BindingStop::Stop
ocs::Binding::binding_get_end(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   constexpr BindingStop::Stop default_end = BindingStop::NONE;
   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(default_end);
   }

   const auto binding_end = static_cast<BindingStop::Stop>(binding_elem.template get<BN_stop>());
   if (binding_end == BindingStop::NONE || binding_end == BindingStop::UNINITIALIZED) {
      DRETURN(default_end);
   }

   DRETURN(binding_end);
}

/**
 * @brief How the binding walks the topology
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested @ref ocs::BindingStrategy::Strategy
 */
template <auto F>
ocs::BindingStrategy::Strategy
ocs::Binding::binding_get_strategy(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   constexpr BindingStrategy::Strategy default_strategy = BindingStrategy::PACKED; // default binding strategy is packed
   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(default_strategy);
   }

   const auto binding_strategy = static_cast<BindingStrategy::Strategy>(binding_elem.template get<BN_strategy>());
   if (binding_strategy == BindingStrategy::NONE || binding_strategy == BindingStrategy::UNINITIALIZED) {
      DRETURN(default_strategy);
   }

   DRETURN(binding_strategy);
}

/**
 * @brief Which instance applies the binding
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested @ref ocs::BindingInstance::Instance
 */
template <auto F>
ocs::BindingInstance::Instance
ocs::Binding::binding_get_instance(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   constexpr BindingInstance::Instance default_instance = BindingInstance::SET; // default binding instance is SET
   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(default_instance);
   }

   const auto binding_instance = static_cast<BindingInstance::Instance>(binding_elem.template get<BN_instance>());
   if (binding_instance == BindingInstance::NONE || binding_instance == BindingInstance::UNINITIALIZED) {
      DRETURN(default_instance);
   }

   DRETURN(binding_instance);
}

/**
 * @brief How the selected hardware is ordered
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the sort specification, NONE when none was given
 */
template <auto F>
std::string
ocs::Binding::binding_get_sort(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(NONE_STR);
   }

   const char *binding_sort = binding_elem.template get<BN_sort>();
   if (binding_sort == nullptr) {
      DRETURN(NONE_STR);
   }

   DRETURN(binding_sort);
}

/**
 * @brief Which hardware the binding is restricted to
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the filter expression, NONE when no binding was requested
 */
template <auto F>
std::string
ocs::Binding::binding_get_filter(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   const auto binding_elem = parent.template get<F>();
   if (!binding_elem) {
      DRETURN(NONE_STR);
   }
   DRETURN(binding_elem.template get<BN_filter>());
}

/**
 * @brief How many units the binding asks for
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object carrying the binding request
 *
 * @return the requested amount
 */
template <auto F>
uint32_t
ocs::Binding::binding_get_amount(const ocs::tom::ConstElement<Parent<F>> parent) {
   DENTER(TOP_LAYER);

   const auto binding_elem = parent.template get<F>();
   uint32_t amount = binding_elem.template get<BN_amount>();
   if (amount == static_cast<uint32_t>(-1)) {
      amount = 0;
   }
   DRETURN(amount);
}

/**
 * @brief Fill in the binding fields the request left out
 *
 * @tparam F the field the binding sub-object lives in (JB_binding, AR_binding)
 * @param parent the object whose binding request is completed
 * @param[out] answer_list receives the reason on failure
 */
template <auto F>
void
ocs::Binding::binding_set_missing_defaults(const ocs::tom::Element<Parent<F>> parent,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   // if the job has no new binding, we do not need to set defaults
   if (!parent) {
      DRETURN_VOID;
   }

   // no binding element, so we do not need to set defaults for binding (unless implicit binding is enabled)
   auto binding_elem = parent.template get<F>();
   if (!binding_elem && !mconf_do_implicit_binding()) {
      DRETURN_VOID;
   }

   // create implicit binding if no binding is given (implicit binding is enabled)
   if (!binding_elem) {
      // binding_get_or_create_elem() attaches the new element to the parent already
      binding_elem = binding_get_or_create_elem<F>(parent, answer_list);
      if (!binding_elem) {
         DRETURN_VOID;
      }
      binding_elem.template set<BN_amount>(1);
   }

   // ensure that the binding element is set to nullptr if the amount is 0
   if (binding_elem.template get<BN_amount>() == 0) {
      parent.template set<F>(nullptr);
      DRETURN_VOID;
   }

   // ensure that the binding element is initialized with default values
   const auto instance = static_cast<BindingInstance::Instance>(binding_elem.template get<BN_instance>());
   if (instance == BindingInstance::Instance::UNINITIALIZED ||
       instance == BindingInstance::Instance::NONE) {
      binding_elem.template set<BN_instance>(BindingInstance::Instance::SET);
   }
   const auto unit = static_cast<BindingUnit::Unit>(binding_elem.template get<BN_unit>());
   if (unit == BindingUnit::Unit::UNINITIALIZED ||
       unit == BindingUnit::Unit::NONE) {
      const BindingUnit::Unit default_binding_unit = mconf_get_default_binding_unit();
      binding_elem.template set<BN_unit>(default_binding_unit);
   }
   const auto type = static_cast<BindingType::Type>(binding_elem.template get<BN_new_type>());
   if (type == BindingType::Type::UNINITIALIZED ||
       type == BindingType::Type::NONE) {
      binding_elem.template set<BN_new_type>(BindingType::Type::SLOT);
   }
   const char *filter = binding_elem.template get<BN_filter>();
   if (filter == nullptr) {
      binding_elem.template set<BN_filter>(NONE_STR);
   }
   const char *sort = binding_elem.template get<BN_sort>();
   if (sort == nullptr) {
      binding_elem.template set<BN_sort>(NONE_STR);
   }
   const auto start = static_cast<BindingStart::Start>(binding_elem.template get<BN_start>());
   if (start == BindingStart::Start::UNINITIALIZED) {
      binding_elem.template set<BN_start>(BindingStart::Start::NONE);
   }
   const auto end = static_cast<BindingStop::Stop>(binding_elem.template get<BN_stop>());
   if (end == BindingStop::Stop::UNINITIALIZED) {
      binding_elem.template set<BN_stop>(BindingStop::Stop::NONE);
   }
   const auto strategy = static_cast<BindingStrategy::Strategy>(binding_elem.template get<BN_strategy>());
   if (strategy == BindingStrategy::Strategy::UNINITIALIZED ||
       strategy == BindingStrategy::Strategy::NONE) {
      binding_elem.template set<BN_strategy>(BindingStrategy::Strategy::PACKED);
   }

   DRETURN_VOID;
}

// the two objects that carry a binding request
template ocs::tom::Element<ocs::tom::BN>
ocs::Binding::binding_get_or_create_elem<JB_binding>(ocs::tom::Element<ocs::tom::JB> parent,
                                                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template void ocs::Binding::clear<JB_binding>(ocs::tom::Element<ocs::tom::JB> parent);
template ocs::BindingType::Type ocs::Binding::binding_get_type<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template ocs::BindingUnit::Unit ocs::Binding::binding_get_unit<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template std::string ocs::Binding::binding_get_sort<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template ocs::BindingStart::Start
ocs::Binding::binding_get_start<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template ocs::BindingStop::Stop ocs::Binding::binding_get_end<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template ocs::BindingStrategy::Strategy
ocs::Binding::binding_get_strategy<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template std::string ocs::Binding::binding_get_filter<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template uint32_t ocs::Binding::binding_get_amount<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template ocs::BindingInstance::Instance
ocs::Binding::binding_get_instance<JB_binding>(ocs::tom::ConstElement<ocs::tom::JB> parent);
template void
ocs::Binding::binding_set_missing_defaults<JB_binding>(ocs::tom::Element<ocs::tom::JB> parent,
                                                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template ocs::tom::Element<ocs::tom::BN>
ocs::Binding::binding_get_or_create_elem<AR_binding>(ocs::tom::Element<ocs::tom::AR> parent,
                                                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template void ocs::Binding::clear<AR_binding>(ocs::tom::Element<ocs::tom::AR> parent);
template ocs::BindingType::Type ocs::Binding::binding_get_type<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template ocs::BindingUnit::Unit ocs::Binding::binding_get_unit<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template std::string ocs::Binding::binding_get_sort<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template ocs::BindingStart::Start
ocs::Binding::binding_get_start<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template ocs::BindingStop::Stop ocs::Binding::binding_get_end<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template ocs::BindingStrategy::Strategy
ocs::Binding::binding_get_strategy<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template std::string ocs::Binding::binding_get_filter<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template uint32_t ocs::Binding::binding_get_amount<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template ocs::BindingInstance::Instance
ocs::Binding::binding_get_instance<AR_binding>(ocs::tom::ConstElement<ocs::tom::AR> parent);
template void
ocs::Binding::binding_set_missing_defaults<AR_binding>(ocs::tom::Element<ocs::tom::AR> parent,
                                                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
