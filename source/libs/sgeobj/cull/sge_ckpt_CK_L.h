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
 * This code was generated from file source/libs/sgeobj/json/CK.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(CK_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_interface) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_ckpt_command) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_migr_command) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_rest_command) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_ckpt_dir) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_when) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_signal) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CK_job_pid) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CK_clean_command) - @todo add summary
*    @todo add description
*
*/

enum {
   CK_name = CK_LOWERBOUND,
   CK_interface,
   CK_ckpt_command,
   CK_migr_command,
   CK_rest_command,
   CK_ckpt_dir,
   CK_when,
   CK_signal,
   CK_job_pid,
   CK_clean_command
};

LISTDEF(CK_Type)
   SGE_STRING(CK_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(CK_interface, CULL_SPOOL)
   SGE_STRING(CK_ckpt_command, CULL_SPOOL)
   SGE_STRING(CK_migr_command, CULL_SPOOL)
   SGE_STRING(CK_rest_command, CULL_SPOOL)
   SGE_STRING(CK_ckpt_dir, CULL_SPOOL)
   SGE_STRING(CK_when, CULL_SPOOL)
   SGE_STRING(CK_signal, CULL_SPOOL)
   SGE_ULONG(CK_job_pid, CULL_DEFAULT)
   SGE_STRING(CK_clean_command, CULL_SPOOL)
LISTEND

NAMEDEF(CKN)
   NAME("CK_name")
   NAME("CK_interface")
   NAME("CK_ckpt_command")
   NAME("CK_migr_command")
   NAME("CK_rest_command")
   NAME("CK_ckpt_dir")
   NAME("CK_when")
   NAME("CK_signal")
   NAME("CK_job_pid")
   NAME("CK_clean_command")
NAMEEND

#define CK_SIZE sizeof(CKN)/sizeof(char *)


