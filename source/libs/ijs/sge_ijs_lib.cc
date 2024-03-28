/*___INFO__MARK_BEGIN_NEW__*/
/* Portions of this code are Copyright 2011 Univa Inc. */
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
