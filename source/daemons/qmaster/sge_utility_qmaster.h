#pragma once
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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Attribute modification helpers shared by the configuration objects
 */


#include "cull/cull.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_daemonize.h"
#include "uti/sge_rmon_macros.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_AR.h"

template <auto F>
int attr_mod_procedure(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                       ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> qep,
                       ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> new_ep,
                       const char *attr_name, const char *variables[]);

bool cqueue_mod_sublist(lListElem *this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lListElem *reduced_elem,
                        ocs::gdi::Command command, ocs::gdi::SubCommand sub_command,
                        int attribute_name, int sublist_host_name, int sublist_value_name, int subsub_key,
                        const char *attribute_name_str, const char *object_name_str);

template <auto F>
int multiple_occurances(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                        ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> lp1,
                        ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> lp2, const char *name,
                        const char *obj_name);

/**
 * @brief Drop an empty sublist so it is not spooled as an empty element
 * @tparam F the sublist field, of the element's object type
 * @param ep the object
 */
template <auto F>
   requires (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::List && ocs::tom::FieldTraits<F>::has_sub)
void normalize_sublist(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> ep) {
   if (const auto lp = ocs::tom::get<F>(ep); lp && lp.empty()) {
      ocs::tom::set<F>(ep, nullptr);
   }
}

bool attr_mod_sub_list(ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *this_elem, int this_elem_name,
                       int this_elem_primary_key,
                       const lListElem *delta_elem, ocs::gdi::Command, ocs::gdi::SubCommand sub_command,
                       const char *sub_list_name, const char *object_name, int no_info, bool *changed);

bool
attr_mod_obj_binding(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::AR> new_ar,
                     ocs::tom::ConstElement<ocs::tom::AR> ar);

