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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_manop.h"

/****** sgeobj/manop/manop_is_manager() ***************************************
*  NAME
*     manop_is_manager() -- is a certain user manager?
*
*  SYNOPSIS
*     bool manop_is_manager(const char *user_name) 
*
*  FUNCTION
*     Checks if the user given by user name is a manager.
*
*  INPUTS
*     const char *user_name - user name
*
*  RESULT
*     bool - true or false
*
*  NOTES
*     Operator/Manager should be a property of a user.
*     Then the function would be user_is_manager - much more plausible
*
*  SEE ALSO
*     gdi/manop/manop_is_operator()
******************************************************************************/
bool manop_is_manager(const char *user_name, const lList *master_manager_list) 
{
   bool ret = false;

   DENTER(TOP_LAYER);
   if (user_name == nullptr) {
      ret = false;
   } else if (lGetElemStr(master_manager_list, UM_name, user_name) != nullptr) {
      ret = true;
   }
   DRETURN(ret);

}

/****** sgeobj/manop/manop_is_operator() **************************************
*  NAME
*     manop_is_operator() -- is a certain user operator?
*
*  SYNOPSIS
*     bool manop_is_operator(const char *user_name) 
*
*  FUNCTION
*     Checks if the user given by user name is a operator.
*     A manager is implicitly also an operator.
*
*  INPUTS
*     const char *user_name - user name
*
*  RESULT
*     bool - true or false
*
*  NOTES
*     Operator/Manager should be a property of a user.
*     Then the function would be user_is_operator - much more plausible
*
*  SEE ALSO
*     gdi/manop/manop_is_manager()
******************************************************************************/
bool manop_is_operator(const char *user_name, const lList *master_manager_list, const lList *master_operator_list) 
{
   bool ret = false;

   DENTER(TOP_LAYER);
   if (user_name == nullptr) {
      ret = false;
   } else if(lGetElemStr(master_operator_list, UO_name, user_name) != nullptr) {
      ret = true;
   } else if (lGetElemStr(master_manager_list, UM_name, user_name) != nullptr) {
      ret = true;
   }
   DRETURN(ret);
}

