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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull_list.h"
#include "uti/ocs_Ternary.h"

#define COMM_SERVER "qrsh_ijs"
#define COMM_CLIENT "shepherd_ijs"

void set_signal_handlers();
void* tty_to_commlib(void *t_conf);
void* commlib_to_tty(void *t_conf);

int start_ijs_server(cl_framework_t communication_framework,
                     const char *hostname, const char *username,
                     const lList *port_range,
                     COMM_HANDLE **phandle, dstring *p_err_msg);

int run_ijs_server(COMM_HANDLE *phandle, const char *remote_host,
   int nostdin, int noshell, int is_rsh, int is_qlogin,
   ocs::Ternary force_pty, ocs::Ternary suspend_remote, int *p_exit_status,
   dstring *p_err_msg, bool forward_x11 = false, char escape_char = '~');

int stop_ijs_server(COMM_HANDLE **phandle, dstring *p_err_msg);

/**
 * @brief Switch run_ijs_server into reconnect mode (CS-2144).
 *
 * When set to a non-null token, commlib_to_tty expects the first inbound message
 * from the shepherd to be RECONNECT_REQUEST_MSG carrying this exact token.  A match
 * causes RECONNECT_ACCEPT_MSG to be sent and the session to be marked connected
 * without re-issuing X11_AUTH_MSG/SETTINGS_CTRL_MSG (the shell is already running).
 * A mismatch causes RECONNECT_REJECT_MSG to be sent and the session to be torn down.
 *
 * Pass nullptr to clear (default — normal qsub/qrsh fresh-session mode).
 * The pointer must outlive run_ijs_server's invocation.
 */
void set_expected_reconnect_token(const char *token);

/**
 * @brief Did run_ijs_server exit because the user pressed the ~. escape?
 *
 * Set during tty_to_commlib's processing of the ~. escape sequence (and also
 * on keepalive-detected dead connections, which take the same disconnect-not-
 * stdin-close path). Cleared at the start of each run_ijs_server invocation.
 *
 * Used by ocs_qsh.cc after run_ijs_server returns: when this returns true
 * the user explicitly disconnected, so qrsh must NOT tell qmaster to delete
 * the job — the shepherd's reconnect grace period (CS-2118 / CS-2155) keeps
 * the job alive for `ijs_reconnect_timeout` seconds so the user can reattach.
 */
bool ijs_was_escape_disconnect();

int force_ijs_server_shutdown(COMM_HANDLE **phandle,
   const char *this_component, dstring *err_msg);

