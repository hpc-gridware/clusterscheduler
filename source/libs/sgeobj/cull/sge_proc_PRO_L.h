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
 * This code was generated from file source/libs/sgeobj/json/PRO.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Process Element
*
* Used in sge_execd to keep track of all processes on the machine.
*
*    SGE_ULONG(PRO_pid) - Pid
*    The process id.
*
*    SGE_ULONG(PRO_utime) - User Time
*    Number of jiffies that this process has been scheduled in user mode.
*
*    SGE_ULONG(PRO_stime) - System Time
*    Number of jiffies that this process has been scheduled in kernel mode.
*
*    SGE_ULONG64(PRO_vsize) - Virtual Memory
*    Virtual memory size in bytes.
*
*    SGE_ULONG64(PRO_rss) - Resident Set Size
*    Resident Set Size (physical memory) in bytes.
*
*    SGE_LIST(PRO_groups) - Groups
*    GR_Type list with all groups associated with this process.
*
*    SGE_BOOL(PRO_rel) - Related to Cluster Scheduler Job
*    Flag if this process belongs to a GE job.
*
*    SGE_BOOL(PRO_run) - Running
*    Flag if this process is still running.
*
*    SGE_ULONG(PRO_io) - IO
*    IO statistic for the running process.
*
*/

enum {
   PRO_pid = PRO_LOWERBOUND,
   PRO_utime,
   PRO_stime,
   PRO_vsize,
   PRO_rss,
   PRO_groups,
   PRO_rel,
   PRO_run,
   PRO_io
};

LISTDEF(PRO_Type)
   SGE_ULONG(PRO_pid, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(PRO_utime, CULL_DEFAULT)
   SGE_ULONG(PRO_stime, CULL_DEFAULT)
   SGE_ULONG64(PRO_vsize, CULL_DEFAULT)
   SGE_ULONG64(PRO_rss, CULL_DEFAULT)
   SGE_LIST(PRO_groups, GR_Type, CULL_DEFAULT)
   SGE_BOOL(PRO_rel, CULL_DEFAULT)
   SGE_BOOL(PRO_run, CULL_DEFAULT)
   SGE_ULONG(PRO_io, CULL_DEFAULT)
LISTEND

NAMEDEF(PRON)
   NAME("PRO_pid")
   NAME("PRO_utime")
   NAME("PRO_stime")
   NAME("PRO_vsize")
   NAME("PRO_rss")
   NAME("PRO_groups")
   NAME("PRO_rel")
   NAME("PRO_run")
   NAME("PRO_io")
NAMEEND

#define PRO_SIZE sizeof(PRON)/sizeof(char *)


