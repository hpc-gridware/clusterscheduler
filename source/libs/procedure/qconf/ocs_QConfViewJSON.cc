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
 * @brief The JSON rendering of a `qconf` read operation
 */

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"

#include "spool/flatfile/ocs_spool_json.h"

#include "ocs_QConfViewJSON.h"

/**
 * @brief The names as a JSON array inside the schema envelope
 *
 * CS-2313a put the name lists into the same `$schema`/`$id` envelope as every
 * other JSON output; the writer that does it lives in the spooling code and is
 * called here exactly as the hand written engine called it.
 *
 * @param os     the stream to write to
 * @param list   the objects, already sorted
 * @param key_nm the field holding the name
 */
void
ocs::QConfViewJSON::show_name_list(std::ostream &os, const lList *list, int key_nm) {
   DENTER(TOP_LAYER);

   lList *alp = nullptr;
   dstring out = DSTRING_INIT;
   spool_json_write_name_list(&alp, list, key_nm, &out);
   const char *text = sge_dstring_get_string(&out);
   if (text != nullptr) {
      os << text;
   }
   sge_dstring_free(&out);
   lFreeList(&alp);

   DRETURN_VOID;
}
