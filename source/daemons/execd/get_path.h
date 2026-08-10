#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Working out where a job's output goes
 *
 * A user may give a path per host, or a directory rather than a file, or use
 * the pseudo variables for job id, task id and host. Resolving all that into
 * one concrete filename is what this does, and it has to happen on the
 * execution host because that is the only place that knows which host it is.
 */

#include "uti/sge_dstring.h"

#include "cull/cull.h"

#include <cinttypes>

/** @name Which of a job's paths is being resolved
 * @{
 */
#define SGE_STDIN           0x00100000   ///< Standard input
#define SGE_STDOUT          0x00200000   ///< Standard output
#define SGE_STDERR          0x00400000   ///< Standard error
#define SGE_SHELL           0x04000000   ///< The shell to start the job with
#define SGE_PAR_STDOUT      0x20000000   ///< Standard output of a PE task
#define SGE_PAR_STDERR      0x40000000   ///< Standard error of a PE task
/** @} */

int sge_get_path(const char *qualified_hostname, const lList *lp, const char *cwd, const char *owner, 
                 const char *job_name, uint32_t job_number,
                 uint32_t task_number, int type, char *path, size_t path_len);
                 
bool sge_get_fs_path(const lList* lp, char* fs_host, size_t fs_host_len,
                                char* fs_path, size_t fs_path_len);

const char *sge_make_ja_task_active_dir(const lListElem *job, const lListElem *ja_task, dstring *err_str);
const char *sge_make_pe_task_active_dir(const lListElem *job, const lListElem *ja_task, const lListElem *pe_task, dstring *err_str);

const char *
expand_path(dstring *dstr_exp_path, const char *path_in, uint32_t job_id, uint32_t ja_task_id,
            const char *job_name, const char *user, const char *fqhost);
