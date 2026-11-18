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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations for parsing textual lists into CULL lists
 *
 * @see cull_parse_util.cc
 */

#include <ostream>
#include <utility>

#include "cull/cull.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"

/// Is this CULL type one whose value is printed as a number?
#define L_IS_NUM_TYPE(x) ((x == lDoubleT) || (x == lUlongT) || (x == lLongT)  || (x == lIntT))

/**
 * @name Flags for uni_print_list
 * @{
 */
#define FLG_NO_DELIS_STRINGS 1  ///< do not put the delimiter between string values
#define FLG_NO_DELIS_NUMBERS 2  ///< do not put the delimiter between numeric values
#define FLG_NO_VALUE_AS_EMPTY 4 ///< print an unset value as nothing rather than as `NONE`
/** @} */

/**
 * @name Flags for parse_list_simple
 *
 * What to do when the option already has a value in the target object.
 * @{
 */
#define FLG_LIST_APPEND   1 ///< append the parsed entries to the existing ones, a repeated key merged by #ocs_object_list_add_unique
#define FLG_LIST_REPLACE  2 ///< replace the existing list with the parsed one
#define FLG_LIST_MERGE    4 ///< merge by key; a parsed entry overrides one of the same name
#define FLG_LIST_MERGE_DOUBLE_KEY    8 ///< like #FLG_LIST_MERGE, but the key is a name/value pair
/** @} */

int parse_list_simple(ocs::tom::List<ocs::tom::SPA> cmdline, const char *option, lListElem *job, int field, int nm_var,
                      int nm_value, uint32_t flags);

void parse_list_hardsoft(ocs::tom::List<ocs::tom::SPA> cmdline, const char *option, ocs::tom::Element<ocs::tom::JB> job,
                         uint32_t scope, int hard_field, int soft_field);

int cull_parse_definition_list(char *str, lList **lpp, const char *name, const lDescr *descr, int *interpretation_rule);

int cull_merge_definition_list(lList **lpp_old, lList *lp_new, int nm_var, int nm_value);

int cull_compress_definition_list(lList *lp_new, int nm_var, int nm_value, int double_keys);

int cull_parse_string_list(char **pstrlist, const char *listname, const lDescr *descr, int *interpretation_rule, lList **pplist);

int uni_print_list(std::ostream &os, const lList *lp, const int *which_elements_rule, const char *pdelis[], unsigned long flags);
int uni_print_list(FILE *fp, char *buff, uint32_t buff_size, const lList *lp, const int *which_elements_rule, const char *pdelis[], unsigned long flags);

int 
parse_list_simpler(lList *lp, lList **destlist, const char *option, lListElem *job, int field,
                  int nm_var, int nm_value, uint32_t flags);

std::pair<int, ocs::tom::ListPtr<ocs::tom::PN>> cull_parse_path_list(const char *path_str);

std::pair<int, ocs::tom::ListPtr<ocs::tom::ST>> cull_parse_jid_hold_list(const char *str);

int sge_parse_hold_list(const char *hold_str, uint32_t prog_number);
