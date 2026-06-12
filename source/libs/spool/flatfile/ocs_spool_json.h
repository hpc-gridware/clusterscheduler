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

#include "cull/cull.h"
#include "uti/sge_dstring.h"
#include "spool/sge_spooling_utilities.h"

/*
 * CS-2313a: generic JSON serialization (qconf -fmt json), driven by the same
 * spooling_field descriptors the flatfile (plain) writer uses. The format
 * dispatch in libs/spool/flatfile delegates the SP_FORM_JSON case here.
 *
 * Both functions append a pretty-printed JSON document (with a $schema/$id
 * envelope) to @p out and return true on success.
 */
bool
spool_json_write_object(lList **answer_list, const lListElem *object,
                        const spooling_field *fields, dstring *out);

bool
spool_json_write_list(lList **answer_list, const lList *list,
                      const spooling_field *fields, dstring *out);
