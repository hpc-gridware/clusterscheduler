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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_ProcedureParameter.h"


void ocs::ProcedureParameter::add_parameter_bundle(lList *bundle, const std::string& param_name, lList *parameter) {
   SGE_ASSERT(bundle != nullptr);
   lListElem *bundle_elem = lAddElemStr(&bundle, SPP_name, param_name.c_str(), SPP_Type);
   lSetList(bundle_elem, SPP_value_list, parameter);
}

lList *
ocs::ProcedureParameter::get_bundle() {
   DENTER(TOP_LAYER);
   lList *bundle = nullptr;

   // procedure name, sub-procedure name
   lList *name_value_list = nullptr;

   lListElem *ep = lAddElemStr(&name_value_list, VA_variable, PROCEDURE, VA_Type);
   lSetString(ep, VA_value, procedure_name_.c_str());
   DPRINTF("procedure_name: " SFN "\n", procedure_name_.c_str());

   ep = lAddElemStr(&name_value_list, VA_variable, SUB_PROCEDURE, VA_Type);
   lSetString(ep, VA_value, sub_procedure_name_.c_str());
   DPRINTF("sub_procedure_name: " SFN "\n", sub_procedure_name_.c_str());

   ep = lAddElemStr(&bundle, SPP_name, NAME_VALUE_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, name_value_list);

   // -fmt
   lList *output_format_list = nullptr;
   lAddElemUlong(&output_format_list, ULNG_value, static_cast<uint32_t>(output_format_), ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, OUTPUT_FORMAT, SPP_Type);
   lSetList(ep, SPP_value_list, output_format_list);
   DPRINTF("output_format: %d\n", output_format_);

   // environment
   ep = lAddElemStr(&bundle, SPP_name, ENVIRONMENT, SPP_Type);
   lSetList(ep, SPP_value_list, lCopyList("", env_variable_list_));
   DPRINTF("env_variable_list: " sge_u32 "\n", lGetNumberOfElem(env_variable_list_));

   // exec_context can be ignored here. It is evaluated on client side only
   DRETURN(bundle);
}

void ocs::ProcedureParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // procedure name
   const lListElem *name_value_param = lGetElemStr(bundle, SPP_name, NAME_VALUE_LIST);
   const lList *name_value_list = lGetList(name_value_param, SPP_value_list);
   const lListElem *procedure_elem = lGetElemStr(name_value_list, VA_variable, PROCEDURE);
   procedure_name_ = lGetString(procedure_elem, VA_value);
   DPRINTF("procedure_name: " SFN "\n", procedure_name_.c_str());

   // sub-procedure name
   const lListElem *sub_procedure_elem = lGetElemStr(name_value_list, VA_variable, SUB_PROCEDURE);
   const char *value = lGetString(sub_procedure_elem, VA_value);
   sub_procedure_name_ = value != nullptr ? value : "";
   DPRINTF("sub_procedure_name: " SFN "\n", sub_procedure_name_.c_str());

   // -fmt
   const lListElem *output_format_param = lGetElemStr(bundle, SPP_name, OUTPUT_FORMAT);
   const lList *output_format_list = lGetList(output_format_param, SPP_value_list);
   output_format_ = static_cast<OutputFormat>(lGetUlong(lFirst(output_format_list), ULNG_value));
   DPRINTF("output_format: %d\n", output_format_);

   // environment
   lListElem *env_param = lGetElemStrRW(bundle, SPP_name, ENVIRONMENT);
   env_variable_list_ = nullptr;
   lXchgList(env_param, SPP_value_list, &env_variable_list_);
   DPRINTF("env_variable_list: " sge_u32 "\n", lGetNumberOfElem(env_variable_list_));

   DRETURN_VOID;
}

std::string ocs::ProcedureParameter::get_procedure_from_bundle(const lList *parameter_bundle) {
   const lListElem *name_value_param = lGetElemStr(parameter_bundle, SPP_name, NAME_VALUE_LIST);
   const lList *name_value_list = lGetList(name_value_param, SPP_value_list);
   const lListElem *procedure_elem = lGetElemStr(name_value_list, VA_variable, PROCEDURE);
   return lGetString(procedure_elem, VA_value);
}

std::string ocs::ProcedureParameter::get_sub_procedure_from_bundle(const lList *parameter_bundle) {
   const lListElem *name_value_param = lGetElemStr(parameter_bundle, SPP_name, NAME_VALUE_LIST);
   const lList *name_value_list = lGetList(name_value_param, SPP_value_list);
   const lListElem *sub_procedure_elem = lGetElemStr(name_value_list, VA_variable, SUB_PROCEDURE);
   return lGetString(sub_procedure_elem, VA_value);
}

void ocs::ProcedureParameter::add_variable(const char *name, const char *value) {
   DENTER(TOP_LAYER);
   lListElem *elem = lAddElemStr(&env_variable_list_, VA_variable, name, VA_Type);
   if (value != nullptr) {
      lSetString(elem, VA_value, value);
   }
   DRETURN_VOID;
}

const char *ocs::ProcedureParameter::get_variable(const char *name) const {
   DENTER(TOP_LAYER);
   const lListElem *elem = lGetElemStr(env_variable_list_, VA_variable, name);
   if (elem == nullptr) {
      DRETURN(nullptr);
   }
   DRETURN(lGetString(elem, VA_value));
}
