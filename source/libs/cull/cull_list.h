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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The list and element functions of the old interface; not public
 *
 * Included through cull/cull_old_api.h. The navigation part (lFirst(), lNext(), the
 * for_each_* macros, lGetElemDescr(), lGetListDescr(), lGetListName()) moved to the
 * transitional cull/cull_transitional.h, which this header includes.
 */

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <type_traits>
#include <utility>

#include <cinttypes>
#include "tom/cull_types.h"
#include "cull/pack.h"
#include "cull/cull_transitional.h"
#include "uti/sge_dstring.h"

/* LIST SPECIFIC FUNCTIONS */
uint32_t lGetNumberOfElem(const lList *lp);

uint32_t lGetNumberOfRemainingElem(const lListElem *ep);

int lGetElemIndex(const lListElem *ep, const lList *lp);

bool lIsFreeElem(const lListElem *ep);

void lWriteElem(const lListElem *ep);

void lWriteElemTo(const lListElem *ep, FILE *fp);

void lWriteElemToStr(const lListElem *ep, dstring *buffer);

void lWriteListTo(const lList *lp, FILE *fp);

lListElem *lCreateElem(const lDescr *dp);

lList *lCreateList(const char *listname, const lDescr *descr);

lList *lCreateListHash(const char *listname, const lDescr *descr, bool hash);

lList *lCreateElemList(const char *listname, const lDescr *descr, int nr_elem);

void lFreeElem(lListElem **ep);

void lFreeList(lList **ilp);

int lAddList(lList *lp0, lList **lp1);

int lAppendList(lList *lp0, lList *lp1);

int lOverrideStrList(lList *lp0, lList *lp1, int nm, const char *str);

lList *lAddSubList(lListElem *ep, int nm, lList *to_add);

int lCompListDescr(const lDescr *dp0, const lDescr *dp1);

lList *lCopyList(const char *name, const lList *src);

lList *lCopyListHash(const char *name, const lList *src, bool hash, bool skip_no_transfer = false);

lListElem *lCopyElem(const lListElem *src);

lListElem *lCopyElemHash(const lListElem *src, bool isHash, bool skip_no_transfer = false);

int
lCopyElemPartialPack(lListElem *dst, const lListElem *src,
                     const lEnumeration *ep, bool isHash, sge_pack_buffer *pb,
                     bool skip_no_transfer = false);

int
lCopySwitchPack(const lListElem *sep, lListElem *dep, int src_idx, int dst_idx,
                bool isHash, lEnumeration *ep, bool skip_no_transfer = false);

int lAppendElem(lList *lp, lListElem *ep);

lListElem *lDechainElem(lList *lp, lListElem *ep);

void lDechainList(lList *source, lList **target, lListElem *ep);

lListElem *lDechainObject(lListElem *parent, int name);

int lRemoveElem(lList *lp, lListElem **ep);

int lInsertElem(lList *lp, lListElem *ep, lListElem *new_elem);

namespace ocs::cull {

   /**
    * @brief An owning element handle moved into a call that takes its element over, such as ocs::tom::ElementPtr
    *
    * The overloads of the old functions that take an element over accept it
    * only as an rvalue, so the transfer shows at the call: `lAppendElem(lp, std::move(ep))`.
    *
    * @tparam P the handle's type, as deduced for a forwarding reference
    */
   template <typename P>
   concept MovedElement = !std::is_lvalue_reference_v<P> && requires(P &p) {
      { p.release() } -> std::same_as<lListElem *>;
      static_cast<lListElem *>(p.get());
   };

   /**
    * @brief An owning list handle moved into a call that takes its list over, such as ocs::tom::ListPtr
    * @tparam P the handle's type, as deduced for a forwarding reference
    */
   template <typename P>
   concept MovedList = !std::is_lvalue_reference_v<P> && requires(P &p) {
      { p.release() } -> std::same_as<lList *>;
      static_cast<lList *>(p.get());
   };

} // namespace ocs::cull

/**
 * @brief Append the element of an owning handle to a list, which takes it over
 *
 * As lAppendElem() on the raw pointer. The handle gives up its element only
 * when it was appended; on an error it keeps and later frees it.
 *
 * @tparam P the owning handle's type
 * @param lp the list
 * @param ep the owning handle, moved in
 * @return 0 on success, -1 on an error, as lAppendElem()
 */
template <typename P>
   requires ocs::cull::MovedElement<P>
int lAppendElem(lList *lp, P &&ep) {
   const int ret = lAppendElem(lp, static_cast<lListElem *>(ep.get()));
   if (ret == 0) {
      static_cast<void>(ep.release());
   }
   return ret;
}

/**
 * @brief Insert the element of an owning handle into a list after another element; the list takes it over
 *
 * As lInsertElem() on the raw pointer. The handle gives up its element only
 * when it was inserted; on an error it keeps and later frees it.
 *
 * @tparam P the owning handle's type
 * @param lp the list
 * @param ep the element to insert after, nullptr for the head
 * @param new_elem the owning handle, moved in
 * @return 0 on success, -1 on an error, as lInsertElem()
 */
template <typename P>
   requires ocs::cull::MovedElement<P>
int lInsertElem(lList *lp, lListElem *ep, P &&new_elem) {
   const int ret = lInsertElem(lp, ep, static_cast<lListElem *>(new_elem.get()));
   if (ret == 0) {
      static_cast<void>(new_elem.release());
   }
   return ret;
}

int lPSortList(lList *lp, const char *fmt, ...);

int lSortList(lList *lp, const lSortOrder *sp);

int lUniqStr(lList *lp, int keyfield);

int lUniqHost(lList *lp, int keyfield);

lListElem *lFindNextRW(const lListElem *ep, const lCondition *cp);

lListElem *lFindPrevRW(const lListElem *ep, const lCondition *cp);

lListElem *lFindFirstRW(const lList *lp, const lCondition *cp);

lListElem *lFindLastRW(const lList *lp, const lCondition *cp);

