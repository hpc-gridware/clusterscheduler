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
 * This code was generated from file source/libs/sgeobj/json/SME.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_LIST(SME_message_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SME_global_message_list) - @todo add summary
*    @todo add description
*
*/

enum {
   SME_message_list = SME_LOWERBOUND,
   SME_global_message_list
};

LISTDEF(SME_Type)
   SGE_LIST(SME_message_list, MES_Type, CULL_DEFAULT)
   SGE_LIST(SME_global_message_list, MES_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(SMEN)
   NAME("SME_message_list")
   NAME("SME_global_message_list")
NAMEEND

#define SME_SIZE sizeof(SMEN)/sizeof(char *)


