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

#include "basis_types.h"
#include "cull/cull_list.h"
#include "uti/sge_dstring.h"

bool centry_check_rsmap(lList **answer_list, u_long32 status, const char* attrname);

bool centry_check_rsmap_characteristics(lList **answer_list, lListElem *centry,
                                        const lList *master_centry_list);

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

bool centry_rsmap_job_has_same_constraint(const lListElem *job);

const char *centry_rsmap_best_free_group(const lListElem *resource_definition,
                                         const lListElem *resource_utilization,
                                         const char *key_name, u_long32 *free_amount);

bool centry_rsmap_select_group_instances(const lListElem *resource_definition,
                                         const lListElem *resource_utilization,
                                         const lList *already, const char *key_name,
                                         u_long32 amount, lList **selected);

const char *centry_rsmap_best_free_id(const lListElem *resource_definition,
                                      const lListElem *resource_utilization,
                                      u_long32 *free_amount);


bool centry_rsmap_expand_implicit_ids(lList **answer_list, lListElem *centry, u_long32 max_ids);

bool centry_list_rsmap_expand_implicit_ids(lList **answer_list, lList *centry_list);

