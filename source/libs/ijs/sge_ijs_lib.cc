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

#if defined(DARWIN)
#  include <sys/ioctl.h>
#elif defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
#  include <stropts.h>
#  include <termio.h>
#elif defined(FREEBSD) || defined(NETBSD)
#  include <termios.h>
#else
#  include <termio.h>
#endif

#include "uti/sge_rmon_macros.h"

#include "sge_ijs_comm.h"

int continue_handler (COMM_HANDLE *comm_handle, char *hostname) {
  DENTER(TOP_LAYER);
  DRETURN(0);
}

int suspend_handler (COMM_HANDLE *comm_handle, char *hostname, int b_is_rsh, int b_suspend_remote, unsigned int pid, dstring *dbuf) {
  DENTER(TOP_LAYER);
  DRETURN(1);
}
