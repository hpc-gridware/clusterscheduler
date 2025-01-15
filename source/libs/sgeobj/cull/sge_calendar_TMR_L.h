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
 * This code was generated from file source/libs/sgeobj/json/TMR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Time Range
*
* Used for time ranges represented by two (begin and end) timestamps (TM_Type).
*
*    SGE_LIST(TMR_begin) - Begin
*    Begin time stamp.
*
*    SGE_LIST(TMR_end) - End
*    End time stamp.
*
*/

enum {
   TMR_begin = TMR_LOWERBOUND,
   TMR_end
};

LISTDEF(TMR_Type)
   SGE_LIST(TMR_begin, TM_Type, CULL_DEFAULT)
   SGE_LIST(TMR_end, TM_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(TMRN)
   NAME("TMR_begin")
   NAME("TMR_end")
NAMEEND

#define TMR_SIZE sizeof(TMRN)/sizeof(char *)


