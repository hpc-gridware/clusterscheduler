/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Key functions of @ref ocs::LogicError and @ref ocs::RuntimeError
 *
 * The destructors are the only out-of-line members of the classes. Defining
 * them here makes the compiler emit vtable and type information once, in this
 * library, instead of as a weak copy in every translation unit. Under libc++,
 * which compares type information by address, that single copy is what lets a
 * handler in one module catch an exception thrown in another.
 */

#include "ocs_Exception.h"

ocs::LogicError::~LogicError() = default;

ocs::RuntimeError::~RuntimeError() = default;
