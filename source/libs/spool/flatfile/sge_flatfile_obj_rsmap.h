#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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
 * @brief Reading and writing the per-instance characteristics of an RSMAP
 */

#include "cull/cull_list.h"
#include "uti/sge_dstring.h"

/** @name The grammar of an RSMAP characteristics block
 *
 * An RSMAP instance may carry name/value characteristics:
 *
 *     gpu0[device=/dev/nvidia0,memory=80G]
 *
 * The block is enclosed in #RSMAP_CHARACTERISTICS_OPEN and
 * #RSMAP_CHARACTERISTICS_CLOSE, its entries separated by
 * #RSMAP_CHARACTERISTIC_SEPARATOR.
 *
 * @note That separator is a comma, and so is the field separator of the
 *       `complex_values` list this whole thing sits inside. The flatfile
 *       value capture is bracket-depth aware for exactly this reason, so the
 *       inner commas survive.
 * @{
 */
#define RSMAP_CHARACTERISTICS_OPEN         '['   ///< Opens the characteristics block
#define RSMAP_CHARACTERISTICS_CLOSE        ']'   ///< Closes the characteristics block
#define RSMAP_CHARACTERISTIC_SEPARATOR     ','   ///< Between two characteristics
#define RSMAP_CHARACTERISTIC_SEPARATOR_STR ","   ///< The same separator, as a string, for `strtok`-style use
/** @} */

int read_CE_stringval_host(lListElem *ep, int nm, const char *buf,
                           lList **alp);

int write_CE_stringval_host(const lListElem *ep, int nm, dstring *buffer,
                            lList **alp);
