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

#include "tom/ocs_Types.h"

#include "uti/sge_rmon_macros.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/sge_object.h"

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

template <auto Hard, auto Soft = NoName>
   requires ocs::tom::FieldTraits<Hard>::has_sub
void parse_list_hardsoft(ocs::tom::List<ocs::tom::SPA> cmdline, const char *option, ocs::tom::Element<ocs::tom::JB> job,
                         uint32_t scope);

int cull_parse_definition_list(char *str, lList **lpp, const char *name, const lDescr *descr, int *interpretation_rule);


int cull_compress_definition_list(ocs::tom::RawList lp, int nm_var, int nm_value, int double_keys);

int cull_parse_string_list(char **pstrlist, const char *listname, const lDescr *descr, int *interpretation_rule,
                           lList **pplist);

int uni_print_list(std::ostream &os, ocs::tom::ConstRawList lp, const int *which_elements_rule, const char *pdelis[],
                   unsigned long flags);
int uni_print_list(FILE *fp, char *buff, uint32_t buff_size, ocs::tom::ConstRawList lp, const int *which_elements_rule,
                   const char *pdelis[], unsigned long flags);

namespace ocs::detail {
   /**
    * @brief Take the next occurrence of a switch out of the command line with its list argument
    * @tparam Sub the object type of the switch's list
    * @param cmdline the parsed option list to consume from
    * @param option the switch to look for
    * @param[out] list receives the switch's list, an empty handle if the switch has none
    * @return true if an occurrence was found and taken out
    */
   template <ocs::tom::ObjectType Sub>
   bool take_list_option(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *option,
                         ocs::tom::ListPtr<Sub> &list) {
      const auto ep = cmdline.find_first<SPA_switch_val>(option);
      if (!ep) {
         return false;
      }
      lList *lp = nullptr;
      ocs::tom::exchange<SPA_argval_lListT>(ep, &lp);
      // the option parser builds the switch's list with the sub-type of the field it is meant for
      list = ocs::tom::ListPtr<Sub>::from_typed(lp);
      cmdline.erase(ep);
      return true;
   }
}

/**
 * @brief Parse a switch's list argument into a list attribute, appending to what it holds
 *
 * Every occurrence of the switch is taken out of the command line, and its entries
 * are appended to the attribute; a repeated key is merged by ocs_object_list_add_unique().
 *
 * @tparam F the list field the entries are stored in, of the object type of @p job
 * @param cmdline the parsed option list to consume from
 * @param option the switch to look for
 * @param[in,out] job the object the list is stored in, a job or an advance reservation
 */
template <auto F>
   requires (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::List && ocs::tom::FieldTraits<F>::has_sub)
void parse_list_append(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *option,
                       const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> job) {
   DENTER(TOP_LAYER);
   using Sub = typename ocs::tom::FieldTraits<F>::Sub;

   // ocs_object_list_add_unique() is key generic and works on the raw lists
   lList *destlist = job.template get<F>().clone("job_sublist").release();
   ocs::tom::ListPtr<Sub> list;
   while (ocs::detail::take_list_option(cmdline, option, list)) {
      DPRINTF("OPTION: %s\n", option);
      if (list) {
         // merged without ever holding a key twice (N-H-18)
         lList *lp = list.release();
         ocs_object_list_add_unique(&destlist, &lp);
      }
   }
   job.template set<F>(ocs::tom::ListPtr<Sub>::from_typed(destlist));
   DRETURN_VOID;
}

/**
 * @brief Parse a switch's list argument into a list attribute, merging by key
 *
 * Every occurrence of the switch is taken out of the command line; its entries are
 * merged into the attribute by @p Key: an entry whose key the attribute holds sets the
 * value of @p Value there, a new key is appended (ocs::tom::merge_by_key()).
 *
 * @tparam F the list field the entries are stored in, of the object type of @p job
 * @tparam Key the key field of the list's entries
 * @tparam Value the value field of the list's entries
 * @param cmdline the parsed option list to consume from
 * @param option the switch to look for
 * @param[in,out] job the object the list is stored in, a job or an advance reservation
 */
template <auto F, auto Key, auto Value>
   requires (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::List && ocs::tom::FieldTraits<F>::has_sub)
void parse_list_merge(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *option,
                      const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> job) {
   DENTER(TOP_LAYER);
   using Sub = typename ocs::tom::FieldTraits<F>::Sub;

   auto destlist = job.template get<F>().clone("job_sublist");
   ocs::tom::ListPtr<Sub> list;
   while (ocs::detail::take_list_option(cmdline, option, list)) {
      DPRINTF("OPTION: %s\n", option);
      if (!list) {
         continue;
      }
      if (!destlist) {
         destlist = std::move(list);
      } else {
         destlist.get().template merge_by_key<Key, Value>(list);
         list.reset();
      }
   }
   job.template set<F>(std::move(destlist));
   DRETURN_VOID;
}

std::pair<int, ocs::tom::ListPtr<ocs::tom::PN>> cull_parse_path_list(const char *path_str);

std::pair<int, ocs::tom::ListPtr<ocs::tom::ST>> cull_parse_jid_hold_list(const char *str);

int sge_parse_hold_list(const char *hold_str, uint32_t prog_number);
