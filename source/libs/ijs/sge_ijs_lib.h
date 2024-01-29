#pragma once
/*___INFO__MARK_BEGIN__*/
/* Portions of this code are Copyright 2011 Univa Inc. */
/*___INFO__MARK_END__*/

int continue_handler (COMM_HANDLE *comm_handle, char *hostname);
int suspend_handler (COMM_HANDLE *comm_handle, char *hostname, int b_is_rsh, int b_suspend_remote, unsigned int pid, dstring *dbuf);
