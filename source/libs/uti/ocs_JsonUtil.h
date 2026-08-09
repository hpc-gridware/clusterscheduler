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
 * @brief Typed helpers for writing JSON key/value pairs
 */

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cinttypes>

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, int value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, uint32_t value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, uint64_t value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, double value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, const char *value);
