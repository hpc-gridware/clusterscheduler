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
* @brief Checkpoint
*
* This is the list type to hold the checkpointing object
* for the interfaces to the various supported checkpointing mechanisms.
*
*    SGE_STRING(CK_name) - Name
*    @todo add description
*
*    SGE_STRING(CK_interface) - Interface
*    The type of checkpointing to be used, e.g.
*     -hibernator
*     -cpr @todo no longer supported SGI kernel level checkpointing, remove
*     -cray-ckpt @todo no longer supported Cray kernel level checkpointing, remove
*     -transparent, using a checkpointing library like Condor
*     -userdefined, some user defined method
*     -application-level, same as userdefined, all interface commands except restart are used, @todo verify
*
*    SGE_STRING(CK_ckpt_command) - Checkpoint Command
*    Commandline to be executed to initiate a checkpoint.
*
*    SGE_STRING(CK_migr_command) - Migration Command
*    Commandline to be executed to initiate a migration of a job (from one host to another one).
*
*    SGE_STRING(CK_rest_command) - Restart Command
*    Commandline to be executed to restart a checkpointed application.
*
*    SGE_STRING(CK_ckpt_dir) - Checkpoint Directory
*    A directory to which checkpoints shall be written.
*
*    SGE_STRING(CK_when) - When
*    When a checkpoint shall be written, one or a combination of:
*     n: no checkpoint is performed
*     s: when the execution service is shutdown
*     m: checkpoint every minimum CPU interval (see queue configuration)
*     x: when the job gets suspended
*     an interval: in the specified interval
*
*    SGE_STRING(CK_signal) - Signal
*    A Unix signal to be sent to the jobs process(es) in order to initiate a checkpoint.
*
*    SGE_ULONG(CK_job_pid) - Job Pid
*    @todo not used and can be removed?
*
*    SGE_STRING(CK_clean_command) - Clean Command
*    Commandline to be executed after checkpointing to clean up.
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


