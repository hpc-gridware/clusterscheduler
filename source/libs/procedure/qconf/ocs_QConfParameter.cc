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
 * @brief Parameters of `qconf`: which switch this call carries, and how it renders
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_QConfParameter.h"

#include "sgeobj/cull/sge_param_SPP_L.h"

/**
 * @brief The operation as its own argument vector
 *
 * `{ token, arg0, arg1, ..., nullptr }`, which is the shape the option blocks
 * of `sge_parse_qconf()` have always read: `*spp` is the switch,
 * `sge_parser_get_next()` walks forward and `sge_next_is_an_opt()` sees the
 * terminator. Giving each operation its own slice is what lets the execution
 * phase iterate the parsed list without rewriting all 163 blocks (CS-2798).
 *
 * The pointers are into this operation's own strings, so the vector is valid
 * until the operation is changed or destroyed.
 *
 * @return the argument vector, never nullptr
 */
char **
qconf_op::as_argv() {
   argv_.clear();
   argv_.reserve(args.size() + 2);
   argv_.push_back(token.data());
   for (std::string &a : args) {
      argv_.push_back(a.data());
   }
   argv_.push_back(nullptr);
   return argv_.data();
}

/**
 * @brief Build the parameters on the server, from a received bundle
 *
 * @param bundle the marshalled parameters, as `get_bundle()` produced them
 * @param packet the originating client packet, for user, host and permissions
 */
ocs::QConfParameter::QConfParameter(const lList *bundle, gdi::Packet *packet)
   : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   QConfParameter::set_bundle(bundle);

   DRETURN_VOID;
}

/**
 * @brief Marshal the parameters for transport in a GDI request
 *
 * The switches that apply to the whole command line travel as one bitmask
 * rather than as four entries, because that is what they are: properties of the
 * invocation, read once and never again.
 *
 * @return the bundle, which the caller owns
 */
lList *
ocs::QConfParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // procedure name, output format (-fmt), environment
   lList *bundle = ProcedureParameter::get_bundle();

   // the switch that was typed, which for the prefix matched ones carries its
   // suffix (-ke, -kej) and is therefore not the same as the table's name
   lList *operation_list = nullptr;
   lAddElemStr(&operation_list, ST_name, operation_.token.c_str(), ST_Type);
   add_parameter_bundle(bundle, OPERATION, operation_list);

   // its arguments, in order
   lList *args_list = nullptr;
   for (const std::string &arg : operation_.args) {
      lAddElemStr(&args_list, ST_name, arg.c_str(), ST_Type);
   }
   add_parameter_bundle(bundle, OPERATION_ARGS, args_list);

   // -cb, -dry, -f, -strict and the client's SGE_SINGLE_LINE and MORE_INFO
   uint32_t flags = 0;
   flags |= binding_ ? FLAG_BINDING : 0;
   flags |= dry_run_ ? FLAG_DRY_RUN : 0;
   flags |= force_ ? FLAG_FORCE : 0;
   flags |= strict_ ? FLAG_STRICT : 0;
   flags |= single_line_ ? FLAG_SINGLE_LINE : 0;
   flags |= more_info_ ? FLAG_MORE_INFO : 0;
   flags |= for_edit_ ? FLAG_FOR_EDIT : 0;
   flags |= for_export_ ? FLAG_FOR_EXPORT : 0;
   lList *flags_list = nullptr;
   lAddElemUlong(&flags_list, ULNG_value, flags, ULNG_Type);
   add_parameter_bundle(bundle, INVOCATION_FLAGS, flags_list);

   // -fmtval
   lList *value_format_list = nullptr;
   lAddElemUlong(&value_format_list, ULNG_value, static_cast<uint32_t>(value_format_), ULNG_Type);
   add_parameter_bundle(bundle, VALUE_FORMAT, value_format_list);

   DRETURN(bundle);
}

/**
 * @brief Rebuild the parameters from a received bundle
 *
 * @param bundle the marshalled parameters
 * @return none
 */
void
ocs::QConfParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // procedure name, output format, environment
   ProcedureParameter::set_bundle(bundle);

   operation_ = qconf_op{};

   const lListElem *operation_param = lGetElemStr(bundle, SPP_name, OPERATION);
   if (operation_param != nullptr) {
      const lListElem *token = lFirst(lGetList(operation_param, SPP_value_list));
      if (token != nullptr) {
         operation_.token = bundle_string(lGetString(token, ST_name));
      }
   }

   const lListElem *args_param = lGetElemStr(bundle, SPP_name, OPERATION_ARGS);
   if (args_param != nullptr) {
      const lListElem *arg;
      for_each_ep(arg, lGetList(args_param, SPP_value_list)) {
         /* an argument that was empty in the client arrives as a null pointer,
          * see bundle_string() - this is the spot that took qmaster down (CS-2840) */
         operation_.args.emplace_back(bundle_string(lGetString(arg, ST_name)));
      }
   }

   const lListElem *flags_param = lGetElemStr(bundle, SPP_name, INVOCATION_FLAGS);
   if (flags_param != nullptr) {
      const lListElem *value = lFirst(lGetList(flags_param, SPP_value_list));
      if (value != nullptr) {
         const uint32_t flags = lGetUlong(value, ULNG_value);
         binding_ = (flags & FLAG_BINDING) != 0;
         dry_run_ = (flags & FLAG_DRY_RUN) != 0;
         force_ = (flags & FLAG_FORCE) != 0;
         strict_ = (flags & FLAG_STRICT) != 0;
         single_line_ = (flags & FLAG_SINGLE_LINE) != 0;
         more_info_ = (flags & FLAG_MORE_INFO) != 0;
         for_edit_ = (flags & FLAG_FOR_EDIT) != 0;
         for_export_ = (flags & FLAG_FOR_EXPORT) != 0;
      }
   }

   const lListElem *value_format_param = lGetElemStr(bundle, SPP_name, VALUE_FORMAT);
   if (value_format_param != nullptr) {
      const lListElem *value = lFirst(lGetList(value_format_param, SPP_value_list));
      if (value != nullptr) {
         value_format_ = static_cast<ValueFormat>(lGetUlong(value, ULNG_value));
      }
   }

   DRETURN_VOID;
}
