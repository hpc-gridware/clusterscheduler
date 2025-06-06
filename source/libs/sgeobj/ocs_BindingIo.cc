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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_binding_hlp.h"
#include "uti/sge_binding_parse.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/cull/sge_binding_BN_L.h"

#include "msg_common.h"

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
bool
ocs::BindingIo::binding_print_to_string(const lListElem *this_elem, dstring *string) { bool ret = true;
   DENTER(TOP_LAYER);
   if (this_elem != nullptr && string != nullptr) {
      const char *const strategy = lGetString(this_elem, BN_strategy);
      binding_type_t type = (binding_type_t)lGetUlong(this_elem, BN_type);

      switch (type) {
         case BINDING_TYPE_SET:
            sge_dstring_append(string, "set ");
            break;
         case BINDING_TYPE_PE:
            sge_dstring_append(string, "pe ");
            break;
         case BINDING_TYPE_ENV:
            sge_dstring_append(string, "env ");
            break;
         case BINDING_TYPE_NONE:
            sge_dstring_append(string, "NONE");
      }

      if (strcmp(strategy, "linear_automatic") == 0) {
         sge_dstring_sprintf_append(string, "%s:" sge_u32,
                                    "linear", lGetUlong(this_elem, BN_parameter_n));
      } else if (strcmp(strategy, "linear") == 0) {
         sge_dstring_sprintf_append(string, "%s:" sge_u32 ":" sge_u32 "," sge_u32,
                                    "linear", lGetUlong(this_elem, BN_parameter_n),
                                    lGetUlong(this_elem, BN_parameter_socket_offset),
                                    lGetUlong(this_elem, BN_parameter_core_offset));
      } else if (strcmp(strategy, "striding_automatic") == 0) {
         sge_dstring_sprintf_append(string, "%s:" sge_u32 ":" sge_u32,
                                    "striding", lGetUlong(this_elem, BN_parameter_n),
                                    lGetUlong(this_elem, BN_parameter_striding_step_size));
      } else if (strcmp(strategy, "striding") == 0) {
         sge_dstring_sprintf_append(string, "%s:" sge_u32 ":" sge_u32 ":" sge_u32 "," sge_u32,
                                    "striding", lGetUlong(this_elem, BN_parameter_n),
                                    lGetUlong(this_elem, BN_parameter_striding_step_size),
                                    lGetUlong(this_elem, BN_parameter_socket_offset),
                                    lGetUlong(this_elem, BN_parameter_core_offset));
      } else if (strcmp(strategy, "explicit") == 0) {
         sge_dstring_sprintf_append(string, "%s", lGetString(this_elem, BN_parameter_explicit));
      }
   }
   DRETURN(ret);
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

