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
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <string>

#include "uti/sge_binding_hlp.h"
#include "uti/sge_binding_parse.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/cull/sge_binding_BN_L.h"

#include "msg_common.h"
#include "ocs_BindingType.h"
#include "ocs_BindingUnit.h"
#include "ocs_BindingStart.h"
#include "ocs_BindingEnd.h"
#include "ocs_BindingInstance.h"
#include "ocs_BindingStrategy.h"

/****** sge_binding/binding_print_to_string() **********************************
*  NAME
*     binding_print_to_string() -- Prints the content of a binding list to a string
*
*  SYNOPSIS
*     bool binding_print_to_string(const lListElem *this_elem, dstring *string)
*
*  FUNCTION
*     Prints the binding type and binding strategy of a binding list element
*     into a string.
*
*  INPUTS
*     const lListElem* this_elem - Binding list element
*
*  OUTPUTS
*     const dstring *string      - Output string which must be initialized.
*
*  RESULT
*     bool - true in all cases
*
*  NOTES
*     MT-NOTE: is_starting_point() is MT safe
*
*******************************************************************************/
void
ocs::BindingIo::binding_print_to_string(const lListElem *binding_elem, std::string &binding_string, bool as_switches) {
   DENTER(TOP_LAYER);

   // no binding
   if (binding_elem == nullptr) {
      if (!as_switches) {
         binding_string = NONE_STR;
      }
      DRETURN_VOID;
   }

   // no amount or strategy also causes to hide all other binding parameters because binding will not be done
   auto amount = lGetUlong(binding_elem, BN_new_amount);
   if (amount == 0) {
      if (!as_switches) {
         binding_string = NONE_STR;;
      }
      DRETURN_VOID;
   }

   // -bamount <number>
   if (amount != 0) {
      if (as_switches) {
         binding_string += "-bamount ";
      } else {
         binding_string += "bamount=";
      }
      binding_string += std::to_string(amount);
   }

   // -bend s|c|...
   auto end = static_cast<BindingEnd::End>(lGetUlong(binding_elem, BN_new_end));
   if (end != BindingEnd::End::UNINITIALIZED && end != BindingEnd::End::NONE) {
      if (as_switches) {
         binding_string += " -bend ";
      } else {
         binding_string += ",bend=";
      }
      binding_string += BindingEnd::to_string(end);
   }

   // -binstance set|pe|env
   auto instance_type = static_cast<BindingInstance::Instance>(lGetUlong(binding_elem, BN_new_instance));
   if (instance_type != BindingInstance::Instance::UNINITIALIZED && instance_type != BindingInstance::Instance::NONE) {
      if (as_switches) {
         binding_string += " -binstance ";
      } else {
         binding_string += ",binstance=";
      }
      binding_string += BindingInstance::to_string(instance_type);
   }

   // -bsort S|C|...
   const char *sort = lGetString(binding_elem, BN_new_sort);
   if (sort != nullptr && strcmp(sort, NONE_STR) != 0) {
      if (as_switches) {
         binding_string += " -bsort ";
      } else {
         binding_string += ",bsort=";
      }

      binding_string += sort;
   }

   // -bstart S|C|...
   auto start = static_cast<BindingStart::Start>(lGetUlong(binding_elem, BN_new_start));
   if (start != BindingStart::Start::UNINITIALIZED && start != BindingStart::Start::NONE) {
      if (as_switches) {
         binding_string += " -bstart ";
      } else {
         binding_string += ",bstart=";
      }
      binding_string += BindingStart::to_string(start);
   }

   // -bstrategy pack|linear
   auto strategy = static_cast<BindingStrategy::Strategy>(lGetUlong(binding_elem, BN_new_strategy));
   if (strategy != BindingStrategy::Strategy::UNINITIALIZED && strategy != BindingStrategy::Strategy::NONE) {
      if (as_switches) {
         binding_string += " -bstrategy ";
      } else {
         binding_string += ",bstrategy=";
      }
      binding_string += BindingStrategy::to_string(strategy);
   }

   // -btype host|slot
   auto type = static_cast<BindingType::Type>(lGetUlong(binding_elem, BN_new_type));
   if (type != BindingType::Type::UNINITIALIZED && type != BindingType::Type::NONE) {
      if (as_switches) {
         binding_string += " -btype ";
      } else {
         binding_string += ",btype=";
      }
      binding_string += BindingType::to_string(type);
   }

   // -bunit S|C|T|...
   auto unit = static_cast<BindingUnit::Unit>(lGetUlong(binding_elem, BN_new_unit));
   if (unit != BindingUnit::Unit::UNINITIALIZED && unit != BindingUnit::Unit::NONE) {
      if (as_switches) {
         binding_string += " -bunit ";
      } else {
         binding_string += ",bunit=";
      }
      binding_string += BindingUnit::to_string(unit);
   }

   // -bfilter <topo_string>
   auto filter = lGetString(binding_elem, BN_new_filter);
   if (filter != nullptr && strcasecmp(filter, NONE_STR) != 0) {
      if (as_switches) {
         binding_string += " -bfilter ";
      } else {
         binding_string += ",bfilter=";

      }
      binding_string += filter;
   }

   DRETURN_VOID;
}

bool
ocs::BindingIo::binding_parse_from_string(lListElem *this_elem, lList **answer_list, dstring *string) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (this_elem != nullptr && string != nullptr) {
      int amount = 0;
      int stepsize = 0;
      int first_socket = 0;
      int first_core = 0;
      binding_type_t type = BINDING_TYPE_NONE;
      dstring strategy = DSTRING_INIT;
      dstring socket_core_list = DSTRING_INIT;
      dstring error = DSTRING_INIT;

      if (parse_binding_parameter_string(sge_dstring_get_string(string),
                                         &type, &strategy, &amount, &stepsize, &first_socket, &first_core,
                                         &socket_core_list, &error) != true) {
         dstring parse_binding_error = DSTRING_INIT;

         sge_dstring_append_dstring(&parse_binding_error, &error);

         answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_XOPTIONWRONGARGUMENT_SS, "-binding",
                                 sge_dstring_get_string(&parse_binding_error));

         sge_dstring_free(&parse_binding_error);
         ret = false;
      } else {
         lSetString(this_elem, BN_strategy, sge_dstring_get_string(&strategy));

         lSetUlong(this_elem, BN_type, type);
         lSetUlong(this_elem, BN_parameter_socket_offset, (first_socket >= 0) ? first_socket : 0);
         lSetUlong(this_elem, BN_parameter_core_offset, (first_core >= 0) ? first_core : 0);
         lSetUlong(this_elem, BN_parameter_n, (amount >= 0) ? amount : 0);
         lSetUlong(this_elem, BN_parameter_striding_step_size, (stepsize >= 0) ? stepsize : 0);

         if (strstr(sge_dstring_get_string(&strategy), "explicit") != nullptr) {
            lSetString(this_elem, BN_parameter_explicit, sge_dstring_get_string(&socket_core_list));
         }
      }

      sge_dstring_free(&strategy);
      sge_dstring_free(&socket_core_list);
      sge_dstring_free(&error);
   }

   DRETURN(ret);
}

