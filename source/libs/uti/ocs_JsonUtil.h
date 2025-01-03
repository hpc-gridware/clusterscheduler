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

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "basis_types.h"

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, int value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, u_long32 value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, u_long64 value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, double value);

void
write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, const char *value);