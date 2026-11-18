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
 * @brief The object types of #sge_object_type as a compile-time table, and a dispatch on a type known at run time
 *
 * Every #sge_object_type value names an object type of the typed object model and its key field, or nothing for
 * the message types that carry no object (shutdown, scheduler monitoring, ...). #ocs_object_types holds that
 * mapping once, as a tuple whose entries carry the type and the key field as template arguments; the entry at
 * index `V` is the one of the value `V`. The run-time table behind object_type_get_name(),
 * object_type_get_descr() and object_type_get_key_nm() (#ocs_object_descriptions) is generated from it, so there
 * is one source.
 *
 * Code that gets an #sge_object_type at run time and then works on the objects of that type calls
 * ocs_with_object_type(): it calls a generic callable with the entry of the given type, so the body is compiled
 * per object type with that type's handles and key field instead of a descriptor and a field number looked up at
 * run time.
 *
 * @code
 * const auto found = ocs_with_object_type(type, [&](const auto &entry) {
 *    using E = ocs_object_type_entry_t<decltype(entry)>;
 *    if constexpr (E::has_key) {
 *       return static_cast<bool>(ocs::tom::find_first<E::key>(list, key));
 *    } else {
 *       return false;
 *    }
 * }).value_or(false);
 * @endcode
 */

#include <array>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "tom/ocs_FieldTraits.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_SU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

/**
 * @brief One #sge_object_type value: its name, its object type and its key field
 *
 * The object type and the key field are template arguments, so code holding an entry knows both at compile time.
 *
 * @tparam V the #sge_object_type value
 * @tparam T the struct of the object type, e.g. ocs::tom::JB; void for a value that carries no object
 * @tparam K the key field, e.g. JB_job_number; `NoName` for a type without a key or without an object
 */
template <sge_object_type V, typename T = void, auto K = NoName>
struct OcsObjectType {
   /// the #sge_object_type value
   static constexpr sge_object_type value = V;
   /// the struct of the object type, void if the value carries no object
   using type = T;
   /// whether the value carries an object
   static constexpr bool has_object = !std::is_void_v<T>;
   /// whether the object type has a key field
   static constexpr bool has_key = ocs::tom::TypedField<decltype(K)>;
   /// the key field, `NoName` if there is none
   static constexpr auto key = K;

   static_assert(has_object || !has_key, "a value without an object has no key");

   const char *name;   ///< the type name, e.g. "JOB"
};

/**
 * @brief The key field of an entry with a key: a field of the entry's object type that lookups by key accept
 *
 * @tparam E the entry type
 * @return true; a mismatch fails the static assertions
 */
template <typename E>
consteval bool ocs_object_type_key_fits() {
   if constexpr (E::has_key) {
      using Traits = ocs::tom::FieldTraits<E::key>;
      static_assert(std::is_same_v<typename Traits::Type, typename E::type>,
                    "the key field is a field of the object type");
      static_assert(ocs::tom::KeyFieldType<Traits::type>, "the key field is a number, a string or a host name");
   }
   return true;
}

/* *INDENT-OFF* */

/**
 * @brief Every #sge_object_type value with its object type and key field, at the index of the value
 *
 * The order is the order of the enumeration; ocs_object_type_entry_at() checks it at compile time.
 */
inline constexpr std::tuple ocs_object_types{
   OcsObjectType<SGE_TYPE_CALENDAR,            ocs::tom::CAL,  CAL_name>{"CALENDAR"},
   OcsObjectType<SGE_TYPE_CKPT,                ocs::tom::CK,   CK_name>{"CKPT"},
   OcsObjectType<SGE_TYPE_CONFIG,              ocs::tom::CONF, CONF_name>{"CONFIG"},
   OcsObjectType<SGE_TYPE_EXECHOST,            ocs::tom::EH,   EH_name>{"EXECHOST"},
   OcsObjectType<SGE_TYPE_JATASK,              ocs::tom::JAT,  JAT_task_number>{"JATASK"},
   OcsObjectType<SGE_TYPE_PETASK,              ocs::tom::PET,  PET_id>{"PETASK"},
   OcsObjectType<SGE_TYPE_JOB,                 ocs::tom::JB,   JB_job_number>{"JOB"},
   OcsObjectType<SGE_TYPE_JOB_SCHEDD_INFO,     ocs::tom::SME>{"JOB_SCHEDD_INFO"},
   OcsObjectType<SGE_TYPE_SHARETREE,           ocs::tom::STN,  STN_name>{"SHARETREE"},
   OcsObjectType<SGE_TYPE_PE,                  ocs::tom::PE,   PE_name>{"PE"},
   OcsObjectType<SGE_TYPE_PROJECT,             ocs::tom::PR,   PR_name>{"PROJECT"},
   OcsObjectType<SGE_TYPE_CQUEUE,              ocs::tom::CQ,   CQ_name>{"CQUEUE"},
   OcsObjectType<SGE_TYPE_QINSTANCE,           ocs::tom::QU,   QU_qname>{"QINSTANCE"},
   OcsObjectType<SGE_TYPE_SCHEDD_CONF,         ocs::tom::SC>{"SCHEDD_CONF"},
   OcsObjectType<SGE_TYPE_SCHEDD_MONITOR>{"SCHEDD_MONITOR"},
   OcsObjectType<SGE_TYPE_SHUTDOWN>{"SHUTDOWN"},
   OcsObjectType<SGE_TYPE_MARK_4_REGISTRATION>{"QMASTER_GOES_DOWN"},
   OcsObjectType<SGE_TYPE_USER,                ocs::tom::UU,   UU_name>{"USER"},
   OcsObjectType<SGE_TYPE_USERSET,             ocs::tom::US,   US_name>{"USERSET"},
   OcsObjectType<SGE_TYPE_HGROUP,              ocs::tom::HGRP, HGRP_name>{"HOSTGROUP"},
   OcsObjectType<SGE_TYPE_CENTRY,              ocs::tom::CE,   CE_name>{"COMPLEX_ENTRY"},
   OcsObjectType<SGE_TYPE_SUSER,               ocs::tom::SU,   SU_name>{"SUBMIT_USER"},
   OcsObjectType<SGE_TYPE_RQS,                 ocs::tom::RQS,  RQS_name>{"RQS"},
   OcsObjectType<SGE_TYPE_AR,                  ocs::tom::AR,   AR_id>{"AR"},
   OcsObjectType<SGE_TYPE_JOBSCRIPT,           ocs::tom::STU,  STU_name>{"JOBSCRIPT"},
   OcsObjectType<SGE_TYPE_CATEGORY,            ocs::tom::CT,   CT_id>{"CATEGORY"},
   OcsObjectType<SGE_TYPE_PROCEDURE>{"PROCEDURE"},
   OcsObjectType<SGE_TYPE_RL,                  ocs::tom::RL,   RL_name>{"ROLE"},
};

/* *INDENT-ON* */

static_assert(std::tuple_size_v<std::remove_cv_t<decltype(ocs_object_types)>> == static_cast<std::size_t>(SGE_TYPE_ALL),
              "ocs_object_types holds one entry per sge_object_type value");

/**
 * @brief The type of the entry the dispatch passes to its callable, for its compile-time members
 *
 * Inside a generic lambda `[](const auto &entry)` the entry's members are constants of its type, not of the
 * parameter: write `ocs_object_type_entry_t<decltype(entry)>::key`.
 *
 * @tparam E the declared type of the lambda parameter
 */
template <typename E>
using ocs_object_type_entry_t = std::remove_cvref_t<E>;

/**
 * @brief The entry of #ocs_object_types at an index, checked to be the entry of that #sge_object_type value
 *
 * @tparam I the index, an #sge_object_type value
 * @return the entry
 */
template <std::size_t I>
consteval const auto &ocs_object_type_entry_at() {
   using E = std::tuple_element_t<I, std::remove_cv_t<decltype(ocs_object_types)>>;
   static_assert(static_cast<std::size_t>(E::value) == I, "ocs_object_types is in the order of sge_object_type");
   static_assert(ocs_object_type_key_fits<E>());
   return std::get<I>(ocs_object_types);
}

/**
 * @brief The entry type of an #sge_object_type value known at compile time
 * @tparam V the value
 */
template <sge_object_type V>
using ocs_object_type_t = ocs_object_type_entry_t<decltype(ocs_object_type_entry_at<static_cast<std::size_t>(V)>())>;

/**
 * @brief The row of the run-time type table for one entry
 *
 * @tparam I the index of the entry
 * @return name, descriptor (nullptr without an object) and key field (`NoName` without a key)
 */
template <std::size_t I>
consteval object_description ocs_object_description_at() {
   const auto &entry = ocs_object_type_entry_at<I>();
   using E = ocs_object_type_entry_t<decltype(entry)>;
   const lDescr *descr = nullptr;
   if constexpr (E::has_object) {
      descr = E::type::descriptor.e;
   }
   return {entry.name, descr, static_cast<int>(E::key)};
}

/**
 * @brief Build the run-time type table from #ocs_object_types
 * @tparam I the indices 0 .. SGE_TYPE_ALL - 1
 * @return one row per #sge_object_type value, at the index of the value
 */
template <std::size_t... I>
consteval std::array<object_description, sizeof...(I)> ocs_make_object_descriptions(std::index_sequence<I...>) {
   return {ocs_object_description_at<I>()...};
}

/// The run-time type table, generated from #ocs_object_types, indexed by #sge_object_type
inline constexpr std::array<object_description, static_cast<std::size_t>(SGE_TYPE_ALL)> ocs_object_descriptions =
   ocs_make_object_descriptions(std::make_index_sequence<static_cast<std::size_t>(SGE_TYPE_ALL)>{});

/**
 * @brief What ocs_with_object_type() returns for a callable returning R
 *
 * An optional R that is empty when the type carries no object; for a callable returning nothing, whether it was
 * called.
 *
 * @tparam R the result type of the callable
 */
template <typename R>
using ocs_object_type_result_t = std::conditional_t<std::is_void_v<R>, bool, std::optional<R>>;

/**
 * @brief One step of ocs_with_object_type(): call the callable if the type is the one at index I
 *
 * @tparam I the index of the entry to try
 * @tparam R the result type of the callable
 * @tparam F the callable
 * @param type the type to dispatch
 * @param f the callable
 * @param[out] result written with the callable's result when it is called
 * @return true if the type is the one at index I, whether the callable was called or the type carries no object
 */
template <std::size_t I, typename R, typename F>
bool ocs_with_object_type_at(const sge_object_type type, [[maybe_unused]] F &f,
                             [[maybe_unused]] ocs_object_type_result_t<R> &result) {
   if (static_cast<std::size_t>(type) != I) {
      return false;
   }
   const auto &entry = ocs_object_type_entry_at<I>();
   if constexpr (ocs_object_type_entry_t<decltype(entry)>::has_object) {
      if constexpr (std::is_void_v<R>) {
         f(entry);
         result = true;
      } else {
         result = f(entry);
      }
   }
   return true;
}

/**
 * @brief The dispatch of ocs_with_object_type() over all indices
 *
 * @tparam R the result type of the callable
 * @tparam F the callable
 * @tparam I the indices 0 .. SGE_TYPE_ALL - 1
 * @param type the type to dispatch
 * @param f the callable
 * @return as ocs_with_object_type()
 */
template <typename R, typename F, std::size_t... I>
ocs_object_type_result_t<R> ocs_with_object_type_impl(const sge_object_type type, F &f, std::index_sequence<I...>) {
   ocs_object_type_result_t<R> result{};
   // stops at the matching index; the result of the fold itself is not needed
   static_cast<void>((ocs_with_object_type_at<I, R>(type, f, result) || ...));
   return result;
}

/**
 * @brief Call a callable with the entry of #ocs_object_types for an #sge_object_type known only at run time
 *
 * The callable, usually a generic lambda taking `const auto &entry`, is compiled once per object type; inside it
 * `ocs_object_type_entry_t<decltype(entry)>` gives the object type (`::type`), the key field (`::key`, valid if
 * `::has_key`) and the value (`::value`) as compile-time constants. It is called only for a value that carries an
 * object; a value without one (shutdown, scheduler monitoring, registration mark, procedure) and a value outside
 * the enumeration (`SGE_TYPE_ALL`, `SGE_TYPE_NONE`) do not call it.
 *
 * The result type of the callable is the one for the first entry; the other instantiations must return a type
 * convertible to it. The dispatch is a chain of integer comparisons, without allocation.
 *
 * @tparam F the callable, taking `const OcsObjectType<...> &`
 * @param type the type to dispatch
 * @param f the callable
 * @return the callable's result as an optional, empty if it was not called; for a callable returning nothing,
 *         true if it was called
 */
template <typename F>
auto ocs_with_object_type(const sge_object_type type, F &&f) {
   using First = decltype(ocs_object_type_entry_at<static_cast<std::size_t>(SGE_TYPE_FIRST)>());
   static_assert(ocs_object_type_entry_t<First>::has_object, "the first type carries an object");
   using R = std::invoke_result_t<F &, First>;
   return ocs_with_object_type_impl<R>(type, f, std::make_index_sequence<static_cast<std::size_t>(SGE_TYPE_ALL)>{});
}
