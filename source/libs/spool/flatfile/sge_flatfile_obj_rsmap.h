#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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
#include "uti/sge_dstring.h"

/* RSMAP per-instance-characteristics grammar:
 *    gpu0[device=/dev/nvidia0,memory=80G]
 * The characteristics block is enclosed in RSMAP_CHARACTERISTICS_OPEN /
 * RSMAP_CHARACTERISTICS_CLOSE and its individual name=value entries are
 * separated by RSMAP_CHARACTERISTIC_SEPARATOR. The framework's flatfile
 * value-capture is bracket-depth aware (see sge_flatfile.cc), so the ','
 * inside [...] is preserved even though ',' is the field separator of the
 * enclosing complex_values list.
 */
#define RSMAP_CHARACTERISTICS_OPEN         '['
#define RSMAP_CHARACTERISTICS_CLOSE        ']'
#define RSMAP_CHARACTERISTIC_SEPARATOR     ','
#define RSMAP_CHARACTERISTIC_SEPARATOR_STR ","

int read_CE_stringval_host(lListElem *ep, int nm, const char *buf,
                           lList **alp);

int write_CE_stringval_host(const lListElem *ep, int nm, dstring *buffer,
                            lList **alp);
