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

/*
 * This code was generated from file source/libs/sgeobj/json/HGRP.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_HOST(HGRP_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(HGRP_host_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(HGRP_cqueue_list) - @todo add summary
*    @todo add description
*
*/

enum {
   HGRP_name = HGRP_LOWERBOUND,
   HGRP_host_list,
   HGRP_cqueue_list
};

LISTDEF(HGRP_Type)
   SGE_HOST(HGRP_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(HGRP_host_list, HR_Type, CULL_SPOOL)
   SGE_LIST(HGRP_cqueue_list, CQ_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(HGRPN)
   NAME("HGRP_name")
   NAME("HGRP_host_list")
   NAME("HGRP_cqueue_list")
NAMEEND

#define HGRP_SIZE sizeof(HGRPN)/sizeof(char *)


