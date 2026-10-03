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
 * A type that is not registered as an object cannot have its envelope derived
 * from the first element, so the descriptor may name it (CS-2820). Whether that
 * name is still given for an *empty* list is a property of the switch rather
 * than a choice: the writer falls back to a bare `{"names": []}` without it,
 * and that is what -sh and -ss printed where -sm and -so printed the full
 * envelope.
 *
 * @param os     the stream to write to
 * @param list   the objects, already sorted
 * @param key_nm the field holding the name
 * @param desc   the descriptor of the type the names belong to
 */
void
ocs::QConfViewJSON::show_name_list(std::ostream &os, const lList *list, int key_nm,
                                   const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   const char *type_name = desc.read.json_name_id;
   if (type_name != nullptr && lFirst(list) == nullptr && !desc.read.json_name_id_when_empty) {
      type_name = nullptr;
   }

   lList *alp = nullptr;
   dstring out = DSTRING_INIT;
   spool_json_write_name_list_ex(&alp, list, key_nm, type_name, &out);
   const char *text = sge_dstring_get_string(&out);
   if (text != nullptr) {
      os << text;
   }
   sge_dstring_free(&out);
   lFreeList(&alp);

   DRETURN_VOID;
}
