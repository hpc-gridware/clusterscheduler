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
 * This code was generated from file source/libs/sgeobj/json/LS.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Load Sensor
*
* The attributes of this element show the state of a load sensor.
* A list of these elements is used in the execd.
*
*    SGE_STRING(LS_name) - Name
*    Name of this load sensor, no hashing, we only have few loadsensors/host.
*    Currently used names:
*      - extern
*      - IDLE_LOADSENSOR_NAME = qidle
*      - GNU_LOADSENSOR_NAME = qloadsensor
*    @todo what's the purpose of the name?
*
*    SGE_STRING(LS_command) - Command
*    Absolute path of the load sensor script / binary.
*
*    SGE_STRING(LS_pid) - Pid
*    Pid of the load sensor process.
*
*    SGE_REF(LS_in) - Stdin File Handle
*    stdin filehandle to the loadsensor process (type FILE *)
*
*    SGE_REF(LS_out) - Stdout File Handle
*    stdout filehandle of the loadsensor process (type FILE *)
*
*    SGE_REF(LS_err) - Stderr File Handle
*    stderr filehandle of the loadsensor process (type FILE *)
*
*    SGE_BOOL(LS_has_to_restart) - Has to restart
*    Do we have to restart the load sensor script?
*
*    SGE_ULONG(LS_tag) - Tag
*    Tag for internal use (@todo 1 means it is running, 0 it isn't running? Verify.)
*
*    SGE_LIST(LS_incomplete) - Incomplete Values
*    Current values we got from the load sensor script.
*
*    SGE_LIST(LS_complete) - Complete Values
*    Last complete set of the load sensor's values.
*
*    SGE_ULONG(LS_last_mod) - Last Modification Time
*    Last modification time of the load sensor script.
*    If the script is modified, then the load sensor will be re-started.
*
*/

enum {
   LS_name = LS_LOWERBOUND,
   LS_command,
   LS_pid,
   LS_in,
   LS_out,
   LS_err,
   LS_has_to_restart,
   LS_tag,
   LS_incomplete,
   LS_complete,
   LS_last_mod
};

LISTDEF(LS_Type)
   SGE_STRING(LS_name, CULL_DEFAULT)
   SGE_STRING(LS_command, CULL_DEFAULT)
   SGE_STRING(LS_pid, CULL_DEFAULT)
   SGE_REF(LS_in, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LS_out, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LS_err, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_BOOL(LS_has_to_restart, CULL_DEFAULT)
   SGE_ULONG(LS_tag, CULL_DEFAULT)
   SGE_LIST(LS_incomplete, LR_Type, CULL_DEFAULT)
   SGE_LIST(LS_complete, LR_Type, CULL_DEFAULT)
   SGE_ULONG(LS_last_mod, CULL_DEFAULT)
LISTEND

NAMEDEF(LSN)
   NAME("LS_name")
   NAME("LS_command")
   NAME("LS_pid")
   NAME("LS_in")
   NAME("LS_out")
   NAME("LS_err")
   NAME("LS_has_to_restart")
   NAME("LS_tag")
   NAME("LS_incomplete")
   NAME("LS_complete")
   NAME("LS_last_mod")
NAMEEND

#define LS_SIZE sizeof(LSN)/sizeof(char *)


