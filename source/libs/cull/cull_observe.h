#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2023-2024 HPC-Gridware GmbH
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

#include "cull/cull_list.h"

#ifdef OBSERVE

void lObserveInit(void);
void lObserveStart(void);
void lObserveEnd(void);

void lObserveAdd(const void *pointer, const void *owner, bool is_list);
void lObserveRemove(const void *pointer);

void lObserveSwitchOwner(const void *pointer_a, const void *pointer_b, const void *owner_a, const void *owner_b, int nm);
void lObserveChangeOwner(const void *pointer, const void *new_owner, const void *old_owner, int nm);
void lObserveChangeValue(const void *pointer, bool has_hash, int nm);
void lObserveChangeListType(const void *pointer, bool is_master_list, const char *list_name);

void lObserveGetInfoString(dstring *dstr);
long lObserveGetSize(void);

#endif 
