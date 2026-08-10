#pragma once
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

/*
 * This code was generated from file source/libs/sgeobj/json/SPR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Spooling Rule
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of SPR
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   SPR_name = 7650,   ///< Name
   SPR_url,   ///< URL
   SPR_option_func,   ///< Option Function
   SPR_startup_func,   ///< Startup Function
   SPR_shutdown_func,   ///< Shutdown Function
   SPR_maintenance_func,   ///< Maintenance Function
   SPR_trigger_func,   ///< Trigger Function
   SPR_transaction_func,   ///< Transaction Function
   SPR_list_func,   ///< List Function
   SPR_read_func,   ///< Read Function
   SPR_read_keys_func,   ///< Read Keys Function
   SPR_write_func,   ///< Write Function
   SPR_delete_func,   ///< Delete Function
   SPR_validate_func,   ///< Validate Function
   SPR_validate_list_func,   ///< Validate List Function
   SPR_clientdata   ///< Client Data
};

/** @brief The attribute ids of SPR, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of SPR
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
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

