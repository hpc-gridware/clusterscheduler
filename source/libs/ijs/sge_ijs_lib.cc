/*___INFO__MARK_BEGIN_NEW__*/
/*************************************************************************
 *
 *  The contents of this file are made available subject to the terms of the
 *  Apache Software License 2.0 ('The License').
 *  You may not use this file except in compliance with The License.
 *  You may obtain a copy of The License at
 *  http://www.apache.org/licenses/LICENSE-2.0.html
 *
 *  Copyright (c) 2011 Univa Corporation.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief Hooks the interactive job support calls on suspend and continue
 *
 * Two extension points for `qsh`/`qrsh`: one when the session receives
 * `SIGCONT`, one before terminal input is forwarded while a suspend is in
 * progress. Both ship as stubs that return the pass-through answer, so a site
 * or a future build can change the behaviour without touching the client.
 */

#include <termios.h>
#if defined(DARWIN)
#  include <sys/ioctl.h>
#elif defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
#  include <stropts.h>
#endif

#include "uti/sge_rmon_macros.h"

#include "sge_ijs_comm.h"

/** @brief Called when the interactive session receives `SIGCONT`
 *
 * @param comm_handle the connection to the shepherd
 * @param hostname host the job runs on
 *
 * @return 1 to end the session, 0 to carry on
 *
 * @note A stub: it always returns 0, so the session carries on and the
 *       terminal is put back into raw mode. The hook exists so that a build
 *       can abort the session instead.
 */
int continue_handler (COMM_HANDLE *comm_handle, char *hostname) {
  DENTER(TOP_LAYER);
  DRETURN(0);
}

/** @brief Called before terminal input is forwarded to the job
 *
 * @param comm_handle the connection to the shepherd
 * @param hostname host the job runs on
 * @param b_is_rsh true for `qrsh`, false for `qsh`
 * @param b_suspend_remote true when the remote side is to be suspended too
 * @param pid pid of the local client
 * @param dbuf buffer for a message to the user
 *
 * @return 1 to forward the input, 0 to swallow it
 *
 * @note A stub: it always returns 1, so input is always forwarded. The hook
 *       exists so that a build can hold input back while a suspend is in
 *       progress.
 */
int suspend_handler (COMM_HANDLE *comm_handle, char *hostname, int b_is_rsh, int b_suspend_remote, unsigned int pid, dstring *dbuf) {
  DENTER(TOP_LAYER);
  DRETURN(1);
}
