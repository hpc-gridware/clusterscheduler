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


/** @brief Add one named entry to a bundle
 * @param bundle the bundle to add to
 * @param param_name the entry's name, one of the constants above
 * @param parameter the entry's value, taken over by the bundle
 */
void ocs::ProcedureParameter::add_parameter_bundle(lList *bundle, const std::string& param_name, lList *parameter) {
   SGE_ASSERT(bundle != nullptr);
   ocs::tom::Element<ocs::tom::SPP> bundle_elem = ocs::tom::emplace_back<SPP_name>(&bundle, param_name.c_str());
   ocs::tom::set<SPP_value_list>(bundle_elem, parameter);
}

/** @brief Serialise this object into a CULL SPP_Type parameter bundle for GDI transport.
 *
 * @return the bundle, which the caller owns
 */
lList *
ocs::ProcedureParameter::get_bundle() {
   DENTER(TOP_LAYER);
   lList *bundle = nullptr;

   // procedure name, sub-procedure name
   lList *name_value_list = nullptr;

   ocs::tom::Element<ocs::tom::VA> ep = ocs::tom::emplace_back<VA_variable>(&name_value_list, PROCEDURE);
   ocs::tom::set<VA_value>(ep, procedure_name_.c_str());
   DPRINTF("procedure_name: " SFN "\n", procedure_name_.c_str());

   ocs::tom::Element<ocs::tom::VA> ep_va = ocs::tom::emplace_back<VA_variable>(&name_value_list, SUB_PROCEDURE);
   ocs::tom::set<VA_value>(ep_va, sub_procedure_name_.c_str());
   DPRINTF("sub_procedure_name: " SFN "\n", sub_procedure_name_.c_str());

   ocs::tom::Element<ocs::tom::SPP> ep_spp = ocs::tom::emplace_back<SPP_name>(&bundle, NAME_VALUE_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp, name_value_list);

   // -fmt
   lList *output_format_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&output_format_list, static_cast<uint32_t>(output_format_));
   ocs::tom::Element<ocs::tom::SPP> ep_spp_2 = ocs::tom::emplace_back<SPP_name>(&bundle, OUTPUT_FORMAT);
   ocs::tom::set<SPP_value_list>(ep_spp_2, output_format_list);
   DPRINTF("output_format: %d\n", output_format_);

   // environment
   ocs::tom::Element<ocs::tom::SPP> ep_spp_3 = ocs::tom::emplace_back<SPP_name>(&bundle, ENVIRONMENT);
   ocs::tom::set<SPP_value_list>(ep_spp_3, env_variable_list_->clone(""));
   DPRINTF("env_variable_list: " sge_u32 "\n", static_cast<uint32_t>(env_variable_list_->size()));

   // exec_context can be ignored here. It is evaluated on client side only
   DRETURN(bundle);
}

/** @brief Populate this object from a received CULL SPP_Type parameter bundle.
 *
 * Called on the server side after the GDI request is received.
 *
 * @param bundle the received bundle
 * @return none
 */
void ocs::ProcedureParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // procedure name
   ocs::tom::ConstElement<ocs::tom::SPP> name_value_param = ocs::tom::find_first<SPP_name>(bundle, NAME_VALUE_LIST);
   const lList *name_value_list = ocs::tom::get<SPP_value_list>(name_value_param);
   ocs::tom::ConstElement<ocs::tom::VA> procedure_elem = ocs::tom::find_first<VA_variable>(name_value_list, PROCEDURE);
   procedure_name_ = bundle_string(ocs::tom::get<VA_value>(procedure_elem));
   DPRINTF("procedure_name: " SFN "\n", procedure_name_.c_str());

   // sub-procedure name
   ocs::tom::ConstElement<ocs::tom::VA> sub_procedure_elem = ocs::tom::find_first<VA_variable>(name_value_list, SUB_PROCEDURE);
   sub_procedure_name_ = bundle_string(ocs::tom::get<VA_value>(sub_procedure_elem));
   DPRINTF("sub_procedure_name: " SFN "\n", sub_procedure_name_.c_str());

   // -fmt
   ocs::tom::ConstElement<ocs::tom::SPP> output_format_param = ocs::tom::find_first<SPP_name>(bundle, OUTPUT_FORMAT);
   const lList *output_format_list = ocs::tom::get<SPP_value_list>(output_format_param);
   output_format_ = static_cast<OutputFormat>(ocs::tom::get<ULNG_value>(lFirst(output_format_list)));
   DPRINTF("output_format: %d\n", output_format_);

   // environment
   // the bundle is an untyped value list, so the list taken out of it is not checked, as before
   ocs::tom::Element<ocs::tom::SPP> env_param = ocs::tom::find_first_rw<SPP_name>(bundle, ENVIRONMENT);
   lList *env_variable_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(env_param, &env_variable_list_received);
   env_variable_list_ = ocs::tom::ListPtr<ocs::tom::VA>::from_typed(env_variable_list_received);
   DPRINTF("env_variable_list: " sge_u32 "\n", static_cast<uint32_t>(env_variable_list_->size()));

   DRETURN_VOID;
}

/** @brief Read the command name out of a bundle without building a parameter object
 *
 * This is what lets the server pick the right subclass before it has one.
 *
 * @param parameter_bundle the received bundle
 * @return the procedure name
 */
std::string ocs::ProcedureParameter::get_procedure_from_bundle(const lList *parameter_bundle) {
   ocs::tom::ConstElement<ocs::tom::SPP> name_value_param = ocs::tom::find_first<SPP_name>(parameter_bundle, NAME_VALUE_LIST);
   const lList *name_value_list = ocs::tom::get<SPP_value_list>(name_value_param);
   ocs::tom::ConstElement<ocs::tom::VA> procedure_elem = ocs::tom::find_first<VA_variable>(name_value_list, PROCEDURE);
   return ocs::tom::get<VA_value>(procedure_elem);
}

/** @brief Read the sub-command name out of a bundle
 * @param parameter_bundle the received bundle
 * @return the sub-procedure name, empty when there is none
 */
std::string ocs::ProcedureParameter::get_sub_procedure_from_bundle(const lList *parameter_bundle) {
   ocs::tom::ConstElement<ocs::tom::SPP> name_value_param = ocs::tom::find_first<SPP_name>(parameter_bundle, NAME_VALUE_LIST);
   const lList *name_value_list = ocs::tom::get<SPP_value_list>(name_value_param);

   // Not every request contains sub-procedure requests
   if (ocs::tom::ConstElement<ocs::tom::VA> sub_procedure_elem = ocs::tom::find_first<VA_variable>(name_value_list, SUB_PROCEDURE); sub_procedure_elem != nullptr) {
      return bundle_string(ocs::tom::get<VA_value>(sub_procedure_elem));
   }
   return "";
}

/** @brief Add one environment variable
 *
 * The list is created with the first variable, as the free emplace_back() creates it.
 *
 * @param name the variable
 * @param value its value
 * @return none
 */
void ocs::ProcedureParameter::add_variable(const char *name, const char *value) {
   DENTER(TOP_LAYER);
   lList *env_variable_list = env_variable_list_.release();
   const auto elem = ocs::tom::emplace_back<VA_variable>(&env_variable_list, name);
   env_variable_list_ = ocs::tom::ListPtr<ocs::tom::VA>::from_typed(env_variable_list);
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
