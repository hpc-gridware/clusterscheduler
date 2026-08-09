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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Starting and controlling remote tasks of a parallel job
 */

#include "sgeobj/cull/sge_qexec_RT_L.h"

/// Identifier of a remote task, as returned by #sge_qexecve
typedef char *sge_tid_t;

/// Life cycle of a remote task, as held in its `RT_state` field
enum {
   RT_STATE_WAIT4ACK,  ///< started; waiting for the task's exit message
   RT_STATE_EXITED,    ///< the exit message arrived; kept in the list so the task id is not reused
   RT_STATE_WAITED     ///< the caller reaped the task with #sge_qwaittid
};

#if 0
/* put these values into task environment list 'envlp' in order 
   to overwrite default behaviour */ 
#define OVERWRITE_TASK_ID_NAME   "TASK_ID"
#define OVERWRITE_PROGRAM_NAME   "PROGRAM_NAME"
#define OVERWRITE_STDOUT         "STDOUT_PATH"
#define OVERWRITE_STDERR         "STDERR_PATH"
#define OVERWRITE_MERGE          "STDOUTERR_MERGE"
#define OVERWRITE_QUEUE          "QUEUE_NAME"
#define OVERWRITE_NO_ACK         "NO_ACK"

/* meaning should be analog to macros that come with waitpid(2) */ 
#define QEXITSTATUS(status) (status)
#define QIFEXITED(status)   (0)
#define QIFSIGNALED(status) (0)
#define QTERMSIG(status)    (0)
#endif
sge_tid_t sge_qexecve(const char *hostname, const char *queuename, const char *cwd,
                      const lList *environment, const lList *path_aliases, const char *cert);

int sge_qwaittid(sge_tid_t tid, int *status, int options);

const char *qexec_last_err();
