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
 * @brief The attribute switches of `qconf` (CS-2986)
 *
 * `-mattr`/`-aattr`/`-rattr`/`-dattr` with the attribute on the command line and
 * `-Mattr`/`-Aattr`/`-Rattr`/`-Dattr` with the attributes in a file. They send a
 * delta of the named objects rather than the whole objects, so they lose no
 * concurrent change and stay GDI requests; qmaster decides their permission in
 * the GDI layer.
 */

#include "ocs_qconf_common.h"

/**
 * @brief Run one attribute operation, if it is one
 *
 * What qmaster answers is printed here, on standard output for a success, as
 * these switches always did.
 *
 * @param op   the operation to run
 * @param alpp the answer list of the operation, receives every message
 * @return what became of the operation
 */
QConfOpResult
qconf_run_attr(const qconf_op &op, lList **alpp);
