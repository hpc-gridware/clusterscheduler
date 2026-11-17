/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Base of the parameter objects: a command's arguments, format and context
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "ocs_ProcedureParameter.h"


/** @brief add_parameter_bundle() for an untyped caller
 * @param bundle the bundle to add to
 * @param param_name the entry's name, one of the constants above
 * @param parameter the entry's value, taken over by the bundle
 * @return none
 */
void
ocs::ProcedureParameter::add_parameter_bundle(const ocs::tom::List<ocs::tom::SPP> bundle,
                                              const std::string &param_name, lList *parameter) {
   SGE_ASSERT(bundle);
   const auto bundle_elem = bundle.emplace_back<SPP_name>(param_name.c_str());
   ocs::tom::set<SPP_value_list>(bundle_elem, parameter);
}

/** @brief A value list holding one number, as a bundle entry carries it
 * @param value the number
 * @return the list, named as the free emplace_back() named it
 */
ocs::tom::ListPtr<ocs::tom::ULNG>
ocs::ProcedureParameter::ulong_value_list(const uint32_t value) {
   DENTER(TOP_LAYER);
   auto list = ocs::tom::ListPtr<ocs::tom::ULNG>::create("ulong_sublist");
   list->emplace_back<ULNG_value>(value);
   DRETURN(list);
}

/** @brief A value list holding one text, as a bundle entry carries it
 * @param value the text; nullptr gives no list, as the free emplace_back() did
 * @return the list, or an empty handle for nullptr
 */
ocs::tom::ListPtr<ocs::tom::ST>
ocs::ProcedureParameter::string_value_list(const char *value) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::ST> list;
   if (value != nullptr) {
      list = ocs::tom::ListPtr<ocs::tom::ST>::create("");
      list->emplace_back<ST_name>(value);
   }
   DRETURN(list);
}

/** @brief Serialise this object into a CULL SPP_Type parameter bundle for GDI transport.
 *
 * @return the bundle, which the caller owns
 */
lList *
ocs::ProcedureParameter::get_bundle() {
   DENTER(TOP_LAYER);
   // the bundle and its value lists are created with their first entry, named as the free emplace_back()
   // created them before
   auto bundle = ocs::tom::ListPtr<ocs::tom::SPP>::create("");

   // procedure name, sub-procedure name
   auto name_value_list = ocs::tom::ListPtr<ocs::tom::VA>::create("");

   const auto ep = name_value_list->emplace_back<VA_variable>(PROCEDURE);
   ocs::tom::set<VA_value>(ep, procedure_name_.c_str());
   DPRINTF("procedure_name: " SFN "\n", procedure_name_.c_str());

   const auto ep_va = name_value_list->emplace_back<VA_variable>(SUB_PROCEDURE);
   ocs::tom::set<VA_value>(ep_va, sub_procedure_name_.c_str());
   DPRINTF("sub_procedure_name: " SFN "\n", sub_procedure_name_.c_str());

   add_parameter_bundle(*bundle, NAME_VALUE_LIST, std::move(name_value_list));

   // -fmt
   add_parameter_bundle(*bundle, OUTPUT_FORMAT, ulong_value_list(static_cast<uint32_t>(output_format_)));
   DPRINTF("output_format: %d\n", output_format_);

   // environment
   add_parameter_bundle(*bundle, ENVIRONMENT, env_variable_list_->clone(""));
   DPRINTF("env_variable_list: " sge_u32 "\n", static_cast<uint32_t>(env_variable_list_->size()));

   // exec_context can be ignored here. It is evaluated on client side only
   // the virtual interface hands the bundle over untyped: qmaster's response path takes it as a raw list
   DRETURN(bundle.release());
}

/** @brief Populate this object from a received CULL SPP_Type parameter bundle.
 *
 * Called on the server side after the GDI request is received. The value lists of
 * the bundle entries are of any type; they are wrapped without the descriptor check
 * (from_typed), as the other set_bundle() overrides do.
 *
 * @param bundle the received bundle
 * @return none
 */
void ocs::ProcedureParameter::set_bundle(const ocs::tom::ConstList<ocs::tom::SPP> bundle) {
   DENTER(TOP_LAYER);

   // procedure name
   const auto name_value_param = bundle.find_first<SPP_name>(NAME_VALUE_LIST);
   const auto name_value_list = value_list<ocs::tom::VA>(name_value_param);
   const auto procedure_elem = name_value_list.find_first<VA_variable>(PROCEDURE);
   procedure_name_ = bundle_string(ocs::tom::get<VA_value>(procedure_elem));
   DPRINTF("procedure_name: " SFN "\n", procedure_name_.c_str());

   // sub-procedure name
   const auto sub_procedure_elem = name_value_list.find_first<VA_variable>(SUB_PROCEDURE);
   sub_procedure_name_ = bundle_string(ocs::tom::get<VA_value>(sub_procedure_elem));
   DPRINTF("sub_procedure_name: " SFN "\n", sub_procedure_name_.c_str());

   // -fmt
   const auto output_format_param = bundle.find_first<SPP_name>(OUTPUT_FORMAT);
   const auto output_format_value = value_list<ocs::tom::ULNG>(output_format_param).front();
   output_format_ = static_cast<OutputFormat>(ocs::tom::get<ULNG_value>(output_format_value));
   DPRINTF("output_format: %d\n", output_format_);

   // environment
   // the received bundle is taken over in part: the environment list is moved out of it, so its entry is
   // looked up writable; the list taken out of it is not checked, as before
   env_variable_list_ = take_value_list<ocs::tom::VA>(ocs::tom::find_first_rw<SPP_name>(bundle, ENVIRONMENT));
   DPRINTF("env_variable_list: " sge_u32 "\n", static_cast<uint32_t>(env_variable_list_->size()));

   DRETURN_VOID;
}

/** @brief Read the command name out of a bundle without building a parameter object
 *
 * This is what lets the server pick the right subclass before it has one. The
 * bundle comes untyped from qmaster's dispatcher, so the free lookups take the
 * soft path on a list of another type, as before.
 *
 * @param parameter_bundle the received bundle
 * @return the procedure name
 */
std::string ocs::ProcedureParameter::get_procedure_from_bundle(const lList *parameter_bundle) {
   const auto name_value_param = ocs::tom::find_first<SPP_name>(parameter_bundle, NAME_VALUE_LIST);
   // the value list is of any type (CULL_ANY_SUBTYPE): the free lookup checks it
   const lList *name_value_list = ocs::tom::get<SPP_value_list>(name_value_param);
   const auto procedure_elem = ocs::tom::find_first<VA_variable>(name_value_list, PROCEDURE);
   return ocs::tom::get<VA_value>(procedure_elem);
}

/** @brief Read the sub-command name out of a bundle
 * @param parameter_bundle the received bundle, untyped as for get_procedure_from_bundle()
 * @return the sub-procedure name, empty when there is none
 */
std::string ocs::ProcedureParameter::get_sub_procedure_from_bundle(const lList *parameter_bundle) {
   const auto name_value_param = ocs::tom::find_first<SPP_name>(parameter_bundle, NAME_VALUE_LIST);
   // the value list is of any type (CULL_ANY_SUBTYPE): the free lookup checks it
   const lList *name_value_list = ocs::tom::get<SPP_value_list>(name_value_param);

   // Not every request contains sub-procedure requests
   if (const auto sub_procedure_elem = ocs::tom::find_first<VA_variable>(name_value_list, SUB_PROCEDURE);
       sub_procedure_elem) {
      return bundle_string(ocs::tom::get<VA_value>(sub_procedure_elem));
   }
   return "";
}

/** @brief Add one environment variable
 *
 * The list is created with the first variable, as the free emplace_back() created it;
 * a null name adds nothing, as before.
 *
 * @param name the variable
 * @param value its value
 * @return none
 */
void ocs::ProcedureParameter::add_variable(const char *name, const char *value) {
   DENTER(TOP_LAYER);
   if (name == nullptr) {
      DRETURN_VOID;
   }
   if (!env_variable_list_) {
      env_variable_list_ = ocs::tom::ListPtr<ocs::tom::VA>::create("");
   }
   const auto elem = env_variable_list_->emplace_back<VA_variable>(name);
   if (value != nullptr) {
      ocs::tom::set<VA_value>(elem, value);
   }
   DRETURN_VOID;
}

/** @brief Read one environment variable
 * @param name the variable
 * @return its value, or nullptr when it is not set
 */
const char *ocs::ProcedureParameter::get_variable(const char *name) const {
   DENTER(TOP_LAYER);
   // the free form on the raw list: the list came unchecked out of a received bundle, and the free
   // lookup takes the soft path on a list of another type instead of throwing inside qmaster
   const auto elem = ocs::tom::find_first<VA_variable>(env_variable_list_.get(), name);
   if (elem == nullptr) {
      DRETURN(nullptr);
   }
   DRETURN(ocs::tom::get<VA_value>(elem));
}
