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
 * @brief Implementation of the JSON writing helpers
 */

#include "ocs_JsonUtil.h"

/**
 * @brief Write one key and its an `int` value
 *
 * @param writer the JSON writer to append to
 * @param key the member name
 * @param value the value to write
 */
void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, int value) {
   writer.Key(key);
   writer.Int(value);
}

/**
 * @brief Write one key and its a `uint32_t` value
 *
 * @param writer the JSON writer to append to
 * @param key the member name
 * @param value the value to write
 */
void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, uint32_t value) {
   writer.Key(key);
   writer.Uint64(value);
}

/**
 * @brief Write one key and its a `uint64_t` value
 *
 * @param writer the JSON writer to append to
 * @param key the member name
 * @param value the value to write
 */
void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, uint64_t value) {
   writer.Key(key);
   writer.Uint64(value);
}

/**
 * @brief Write one key and its a `double` value
 *
 * @param writer the JSON writer to append to
 * @param key the member name
 * @param value the value to write
 */
void
   write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, double value) {
      writer.Key(key);
      writer.Double(value);
   }

/**
 * @brief Write one key and its a C string value
 *
 * @param writer the JSON writer to append to
 * @param key the member name
 * @param value the value to write
 */
void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, const char *value) {
   if (value != nullptr) {
      writer.Key(key);
      writer.String(value);
   }
}

