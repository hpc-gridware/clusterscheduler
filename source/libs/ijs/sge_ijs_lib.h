#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
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

int continue_handler (COMM_HANDLE *comm_handle, char *hostname);
int suspend_handler (COMM_HANDLE *comm_handle, char *hostname, int b_is_rsh, int b_suspend_remote, unsigned int pid, dstring *dbuf);
