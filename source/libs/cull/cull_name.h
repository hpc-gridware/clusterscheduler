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
#pragma once

#include "cull/cull_list.h"

/** @brief Bind the CULL name space used by lNm2Str()/lStr2Nm() on this thread.
 *  @param ns_vector  sentinel-terminated name space array */
void lInit(const lNameSpace *ns_vector);

/** @brief Return the string name for a CULL name id (direct index per entry).
 *  @param nm  CULL name id
 *  @return    the field name, or a placeholder string if unknown */
const char *lNm2Str(int nm);

/** @brief Resolve a field name to its CULL name id (O(1) hash lookup).
 *  @param str  field name to resolve
 *  @return     the CULL name id, or NoName if unknown */
int lStr2Nm(const char *str);

/** @brief Resolve a field name against an explicit name space.
 *  @param str  field name to resolve
 *  @param ns   name space; nullptr uses the thread's bound default
 *  @return     the CULL name id, or NoName if unknown */
int lStr2Nm(const char *str, const lNameSpace *ns);
