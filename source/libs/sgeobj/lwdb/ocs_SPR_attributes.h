#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/SPR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   SPR_name = 8150,
   SPR_url,
   SPR_option_func,
   SPR_startup_func,
   SPR_shutdown_func,
   SPR_maintenance_func,
   SPR_trigger_func,
   SPR_transaction_func,
   SPR_list_func,
   SPR_read_func,
   SPR_read_keys_func,
   SPR_write_func,
   SPR_delete_func,
   SPR_validate_func,
   SPR_validate_list_func,
   SPR_clientdata
};

constexpr const int SPR_Type[] = {
   SPR_name,
   SPR_url,
   SPR_option_func,
   SPR_startup_func,
   SPR_shutdown_func,
   SPR_maintenance_func,
   SPR_trigger_func,
   SPR_transaction_func,
   SPR_list_func,
   SPR_read_func,
   SPR_read_keys_func,
   SPR_write_func,
   SPR_delete_func,
   SPR_validate_func,
   SPR_validate_list_func,
   SPR_clientdata,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define SPR_ATTRIBUTES \
   {SPR_name, "SPR_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {SPR_url, "SPR_url", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_option_func, "SPR_option_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_startup_func, "SPR_startup_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_shutdown_func, "SPR_shutdown_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_maintenance_func, "SPR_maintenance_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_trigger_func, "SPR_trigger_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_transaction_func, "SPR_transaction_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_list_func, "SPR_list_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_read_func, "SPR_read_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_read_keys_func, "SPR_read_keys_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_write_func, "SPR_write_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_delete_func, "SPR_delete_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_validate_func, "SPR_validate_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_validate_list_func, "SPR_validate_list_func", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPR_clientdata, "SPR_clientdata", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

