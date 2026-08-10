#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Process Element
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
*    IO characters for the running process.
*
*    SGE_ULONG(PRO_ioops) - IOOPS
*    IO operations for the running process.
*
*    SGE_ULONG64(PRO_iow) - IOW
*    IO wait time in clock ticks for the running process.
*
*/

enum {
   PRO_pid = PRO_LOWERBOUND,   ///< Pid
   PRO_utime,   ///< User Time
   PRO_stime,   ///< System Time
   PRO_vsize,   ///< Virtual Memory
   PRO_rss,   ///< Resident Set Size
   PRO_groups,   ///< Groups
   PRO_rel,   ///< Related to Cluster Scheduler Job
   PRO_run,   ///< Running
   PRO_io,   ///< IO
   PRO_ioops,   ///< IOOPS
   PRO_iow   ///< IOW
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
   SGE_ULONG(PRO_ioops, CULL_DEFAULT)
   SGE_ULONG64(PRO_iow, CULL_DEFAULT)
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
   NAME("PRO_ioops")
   NAME("PRO_iow")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define PRO_SIZE sizeof(PRON)/sizeof(char *)


