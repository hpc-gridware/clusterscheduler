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
 * CS-2313a: selects how numeric TIME/MEM values are rendered in JSON. COMPACT (the
 * default) keeps the human-readable unit/colon strings ("2.000G", "0:5:0"); NUMERIC
 * renders them as native numbers (bytes / seconds). INT/DOUBLE are always numbers and
 * unlimited is always "INFINITY", in either mode. qconf sets this from -fmtval;
 * programmatic callers that need machine values (the event interface feeding the
 * python-api) pin NUMERIC so a user preference cannot break their contract.
 */
enum ocs_json_value_format {
   OCS_JSON_VALUES_COMPACT,
   OCS_JSON_VALUES_NUMERIC
};
extern ocs_json_value_format ocs_json_value_format_opt;

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

/* Write a bare name list (e.g. managers, admin hosts) as a JSON array of the @p keynm
 * field of each element, wrapped in the same $schema/$id envelope as the object lists. */
bool
spool_json_write_name_list(lList **answer_list, const lList *list, int keynm, dstring *out);

/* Like spool_json_write_list but with an explicit @p type_name for the envelope $id and
 * the array key (for lists whose element type is not a registered object, e.g. -stl). */
bool
spool_json_write_typed_list(lList **answer_list, const lList *list,
                            const spooling_field *fields, const char *type_name, dstring *out);

/* Like spool_json_write_typed_list but with separate @p id_name (for the schema $id) and
 * @p envelope_name (the array key), for an object queried both as a list and individually
 * (e.g. the categories: array key "category" but "category-list" schema id). */
bool
spool_json_write_typed_list_ex(lList **answer_list, const lList *list,
                               const spooling_field *fields, const char *id_name,
                               const char *envelope_name, dstring *out);

/* Like spool_json_write_object but with an explicit @p type_name for the envelope $id
 * (for objects whose type cannot be resolved by content, e.g. a sharetree STN node). */
bool
spool_json_write_typed_object(lList **answer_list, const lListElem *object,
                              const spooling_field *fields, const char *type_name, dstring *out);

/*
 * Parse a JSON document (produced by the writers above) back into a CULL element
 * or list. @p fields_out (if non-null) receives the set of field ids found, for
 * the caller's unprocessed-field check (as the ASCII reader does).
 */
lListElem *
spool_json_read_object(lList **answer_list, const lDescr *descr,
                       const spooling_field *fields, int fields_out[], const char *json_text);

lList *
spool_json_read_list(lList **answer_list, const lDescr *descr,
                     const spooling_field *fields, int fields_out[], const char *json_text);
