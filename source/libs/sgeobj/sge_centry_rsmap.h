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
 * @brief Declarations for checks specific to RSMAP resources
 *
 * @see sge_centry_rsmap.cc
 */

#include <cinttypes>
#include "cull/cull_list.h"
#include "uti/sge_dstring.h"

bool centry_check_rsmap(lList **answer_list, uint32_t status, const char* attrname);

bool centry_check_rsmap_characteristics(lList **answer_list, lListElem *centry,
                                        const lList *master_centry_list);

bool centry_rsmap_expand_implicit_ids(lList **answer_list, lListElem *centry, uint32_t max_ids);

/**
 * The parameters a resource map request may carry in brackets after its amount. The names are
 * reserved: a complex of one of these names cannot be matched as a characteristic, which is why
 * they are also refused as complex names when a complex is created or modified.
 */
extern const char *const RSMAP_REQUEST_PARAM_ID;
extern const char *const RSMAP_REQUEST_PARAM_SAME;
extern const char *const RSMAP_REQUEST_PARAM_SCOPE;
extern const char *const RSMAP_REQUEST_PARAM_DISTINCT;
extern const char *const RSMAP_REQUEST_PARAM_BIND;

bool centry_rsmap_is_reserved_param(const char *name);

bool centry_rsmap_check_request_params(lList **answer_list, const lListElem *centry,
                                       const lList *master_centry_list);

bool centry_rsmap_get_request_param(const lListElem *centry, const char *param,
                                    dstring *value);


bool centry_rsmap_resolve_request_properties(const lListElem *centry,
                                             const lList *master_centry_list, lList **required);

lList *centry_rsmap_group_keys(const lListElem *resource_definition, const char *key_name);

uint32_t centry_rsmap_free(const lListElem *resource_definition, const lList *taken,
                           const char *id_expr, const lList *required_props = nullptr);

uint32_t centry_rsmap_group_free(const lListElem *resource_definition, const lList *taken,
                                 const char *key_name, const char *key,
                                 const char *id_expr = nullptr,
                                 const lList *required_props = nullptr);

const char *centry_rsmap_best_free_group(const lListElem *resource_definition,
                                         const lList *taken,
                                         const char *key_name, uint32_t *free_amount,
                                         const char *id_expr = nullptr,
                                         const lList *required_props = nullptr);

bool centry_rsmap_select_instances(const lListElem *resource_definition,
                                   const lList *taken, const lList *already,
                                   uint32_t amount, lList **selected,
                                   const char *id_expr = nullptr,
                                   const lList *required_props = nullptr);

bool centry_rsmap_select_group_instances(const lListElem *resource_definition,
                                         const lList *taken,
                                         const lList *already, const char *key_name,
                                         uint32_t amount, lList **selected,
                                         const char *id_expr = nullptr,
                                         const lList *required_props = nullptr);

const char *centry_rsmap_best_free_id(const lListElem *resource_definition,
                                      const lList *taken,
                                      uint32_t *free_amount, const char *id_expr = nullptr,
                                      const lList *required_props = nullptr);


bool centry_rsmap_expand_implicit_ids(lList **answer_list, lListElem *centry, uint32_t max_ids);

bool centry_list_rsmap_expand_implicit_ids(lList **answer_list, lList *centry_list);

