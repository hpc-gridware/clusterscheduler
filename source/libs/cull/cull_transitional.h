#pragma once
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
 * @brief Transitional, removed by the next CS-3103 step; do not add users
 *
 * The last functions of the old list library that product code may still call
 * while it is converted: walking a list and reading its run-time type, and the
 * pack buffer primitives.
 *
 * - walking: lFirst(), lFirstRW(), lLast(), lLastRW(), lNext(), lNextRW(), lPrev(),
 *   lPrevRW() and the for_each_* macros built on them;
 * - the run-time type: lGetElemDescr(), lGetListDescr(), and lGetListName();
 * - the pack buffer primitives of tom/pack.h (init_packbuffer(), packint(),
 *   unpackstr(), ...).
 *
 * Product code reaches objects through the typed object model and the raw API
 * (tom/ocs_*.h). The next CS-3103 step converts the remaining users of this
 * header and removes it; until then the build lists every file outside
 * libs/cull, libs/tom, libs/sgeobj/tom and the tests that includes it, as a
 * warning (see test/libs/cull/check_cull_api.cmake). The rest of the old
 * interface (cull/cull_old_api.h) is not reachable for product code at all.
 */

#include "tom/cull_types.h"
#include "tom/pack.h"

/**
 * @brief The name of a list
 * @param lp the list, may be nullptr
 * @return the name; "No List specified" for a nullptr list (the cull error is set)
 */
const char *lGetListName(const lList *lp);

/**
 * @brief The descriptor (the run-time type and layout) of the elements of a list
 * @param lp the list, may be nullptr
 * @return the descriptor, nullptr for a nullptr list
 */
const lDescr *lGetListDescr(const lList *lp);

/**
 * @brief The descriptor (the run-time type and layout) of an element
 * @param ep the element, may be nullptr
 * @return the descriptor, nullptr for a nullptr element
 */
const lDescr *lGetElemDescr(const lListElem *ep);

/**
 * @brief The first element of a list, modifiable
 * @param lp the list, may be nullptr
 * @return the first element, nullptr for an empty or a nullptr list
 */
lListElem *lFirstRW(const lList *lp);

/**
 * @brief The first element of a list, read only
 * @param lp the list, may be nullptr
 * @return the first element, nullptr for an empty or a nullptr list
 */
const lListElem *lFirst(const lList *lp);

/**
 * @brief The last element of a list, modifiable
 * @param lp the list, may be nullptr
 * @return the last element, nullptr for an empty or a nullptr list
 */
lListElem *lLastRW(const lList *lp);

/**
 * @brief The last element of a list, read only
 * @param lp the list, may be nullptr
 * @return the last element, nullptr for an empty or a nullptr list
 */
const lListElem *lLast(const lList *lp);

/**
 * @brief The element after an element, modifiable
 * @param ep the element, may be nullptr
 * @return the next element, nullptr at the end of the list or for a nullptr element
 */
lListElem *lNextRW(const lListElem *ep);

/**
 * @brief The element after an element, read only
 * @param ep the element, may be nullptr
 * @return the next element, nullptr at the end of the list or for a nullptr element
 */
const lListElem *lNext(const lListElem *ep);

/**
 * @brief The element before an element, modifiable
 * @param ep the element, may be nullptr
 * @return the previous element, nullptr at the head of the list or for a nullptr element
 */
lListElem *lPrevRW(const lListElem *ep);

/**
 * @brief The element before an element, read only
 * @param ep the element, may be nullptr
 * @return the previous element, nullptr at the head of the list or for a nullptr element
 */
const lListElem *lPrev(const lListElem *ep);

#define for_each_ep(ep, lp) for (ep=lFirst(lp);ep;ep=lNext(ep))                          ///< walk a list front to back, read only
#define for_each_rev(ep, lp) for (ep=lLast(lp);ep;ep=lPrev(ep))                          ///< walk a list back to front, read only
#define for_each_rw(ep, lp) for (ep=lFirstRW(lp);ep;ep=lNextRW(ep))                      ///< walk a list front to back, elements modifiable
#define for_each_rev_rw(ep, lp) for (ep=lLastRW(lp);ep;ep=lPrevRW(ep))                   ///< walk a list back to front, elements modifiable
// same macros as the ones without _lv (local variable) suffix with the difference that the
// object variable does not have to be declared before the loop, but is declared in
// the for loop itself. This allows to use the same variable name in code after the loop.
// The variable takes the type lFirst() and lFirstRW() return: over a list pointer it is the
// element pointer it always was, over a list handle of ocs::tom it is an element handle of the
// list's type, so a field of another object type does not compile in the loop body.
#define for_each_ep_lv(ep, lp) for (auto ep=lFirst(lp);ep;ep=lNext(ep))                   ///< like #for_each_ep, declaring @p ep in the loop itself
#define for_each_rw_lv(ep, lp) for (auto ep=lFirstRW(lp);ep;ep=lNextRW(ep))               ///< like #for_each_rw, declaring @p ep in the loop itself
