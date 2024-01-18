#pragma once
/* Portions of this code are Copyright 2011 Univa Inc. */

#ifdef __cplusplus
extern "C" {
#endif

int continue_handler (COMM_HANDLE *comm_handle, char *hostname); 
int suspend_handler (COMM_HANDLE *comm_handle, char *hostname, int b_is_rsh, int b_suspend_remote, unsigned int pid, dstring *dbuf);

#ifdef __cplusplus
}
#endif
