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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>

#if defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
#  include <stropts.h>
#  include <termio.h>
#endif

#include "comm/commlib.h"

#include "uti/msg_utilib.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sge_daemonize.h"

#include "msg_common.h"

/* pipe for sge_daemonize_prepare() and sge_daemonize_finalize() */
static int fd_pipe[2];

/****** uti/os/sge_daemonize_prepare() *****************************************
*  NAME
*     sge_daemonize_prepare() -- prepare daemonize of process
*
*  SYNOPSIS
*     int sge_daemonize_prepare(void) 
*
*  FUNCTION
*     The parent process will wait for the child's successful daemonizing.
*     The client process will report successful daemonizing by a call to
*     sge_daemonize_finalize().
*     The parent process will exit with one of the following exit states:
*
*     typedef enum uti_deamonize_state_type {
*        SGE_DEAMONIZE_OK           = 0,  ok 
*        SGE_DAEMONIZE_DEAD_CHILD   = 1,  child exited before sending state 
*        SGE_DAEMONIZE_TIMEOUT      = 2   timeout whild waiting for state 
*     } uti_deamonize_state_t;
*
*     Daemonize the current application. Throws ourself into the
*     background and dissassociates from any controlling ttys.
*     Don't close filedescriptors mentioned in 'keep_open'.
*      
*     sge_daemonize_prepare() and sge_daemonize_finalize() will replace
*     sge_daemonize() for multithreaded applications.
*     
*     sge_daemonize_prepare() must be called before starting any thread. 
*
*
*  INPUTS
*     void - none
*
*  RESULT
*     int - true on success, false on error
*
*  SEE ALSO
*     uti/os/sge_daemonize_finalize()
*******************************************************************************/
bool sge_daemonize_prepare() {
   pid_t pid;
   int fd;

   int is_daemonized = component_is_daemonized();

   DENTER(TOP_LAYER);

#ifndef NO_SGE_COMPILE_DEBUG
   if (TRACEON) {
      DRETURN(false);
   }
#endif

   if (is_daemonized) {
      DRETURN(true);
   }

   /* create pipe */
   if (pipe(fd_pipe) < 0) {
      CRITICAL(SFNMAX, MSG_UTI_DAEMONIZE_CANT_PIPE);
      DRETURN(false);
   }

   if (fcntl(fd_pipe[0], F_SETFL, O_NONBLOCK) != 0) {
      CRITICAL(SFNMAX, MSG_UTI_DAEMONIZE_CANT_FCNTL_PIPE);
      DRETURN(false);
   }

   /* close all fd's except pipe and first 3 */
   {
      int keep_open[5];
      keep_open[0] = 0;
      keep_open[1] = 1;
      keep_open[2] = 2;
      keep_open[3] = fd_pipe[0];
      keep_open[4] = fd_pipe[1];
      sge_close_all_fds(keep_open, 5);
   }

   /* first fork */
   pid = fork();
   if (pid < 0) {
      CRITICAL(MSG_PROC_FIRSTFORKFAILED_S, strerror(errno));
      exit(1);
   }

   if (pid > 0) {
      char line[256];
      int line_p = 0;
      int retries = 60;
      int exit_status = SGE_DAEMONIZE_TIMEOUT;

      /* close send pipe */
      close(fd_pipe[1]);

      /* check pipe for message from child */
      while (line_p < 4 && retries-- > 0) {
         errno = 0;
         ssize_t back = read(fd_pipe[0], &line[line_p], 1);
         int errno_value = errno;
         if (back > 0) {
            line_p++;
         } else {
            if (back != -1) {
               if (errno_value != EAGAIN) {
                  retries = 0;
                  exit_status = SGE_DAEMONIZE_DEAD_CHILD;
               }
            }
            DPRINTF(("back=%d errno=%d\n", (int)back, errno_value));
            sleep(1);
         }
      }

      if (line_p >= 4) {
         line[3] = 0;
         exit_status = atoi(line);
         DPRINTF(("received: \"%d\"\n", exit_status));
      }

      switch (exit_status) {
         case SGE_DEAMONIZE_OK:
            INFO(SFNMAX, MSG_UTI_DAEMONIZE_OK);
            break;
         case SGE_DAEMONIZE_DEAD_CHILD:
            WARNING(SFNMAX, MSG_UTI_DAEMONIZE_DEAD_CHILD);
            break;
         case SGE_DAEMONIZE_TIMEOUT:
            WARNING(SFNMAX, MSG_UTI_DAEMONIZE_TIMEOUT);
            break;
         default:
            break;
      }
      /* close read pipe */
      close(fd_pipe[0]);
      exit(exit_status); /* parent exit */
   }

   /* child */
   SETPGRP;

   if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
      /* disassociate contolling tty */
      ioctl(fd, TIOCNOTTY, (char *) nullptr);
      close(fd);
   }


   /* second fork */
   pid = fork();
   if (pid < 0) {
      CRITICAL(MSG_PROC_SECONDFORKFAILED_S, strerror(errno));
      exit(1);
   }
   if (pid > 0) {
      /* close read and write pipe for second child and exit */
      close(fd_pipe[0]);
      close(fd_pipe[1]);
      exit(0);
   }

   /* child of child */

   /* close read pipe */
   close(fd_pipe[0]);

   DRETURN(true);
}

/****** uti/os/sge_daemonize_finalize() ****************************************
*  NAME
*     sge_daemonize_finalize() -- finalize daemonize process
*
*  SYNOPSIS
*     int sge_daemonize_finalize(fd_set *keep_open) 
*
*  FUNCTION
*     report successful daemonizing to the parent process and close
*     all file descriptors. Set file descirptors 0, 1 and 2 to /dev/null 
*
*     sge_daemonize_prepare() and sge_daemonize_finalize() will replace
*     sge_daemonize() for multithreades applications.
*
*     sge_daemonize_finalize() must be called by the thread who have called
*     sge_daemonize_prepare().
*
*  INPUTS
*     fd_set *keep_open - file descriptor set to keep open
*
*  RESULT
*     int - true on success
*
*  SEE ALSO
*     uti/os/sge_daemonize_prepare()
*******************************************************************************/
void
sge_daemonize_finalize() {
   int failed_fd;
   char tmp_buffer[4];
   int is_daemonized = component_is_daemonized();

   DENTER(TOP_LAYER);

   /* don't call this function twice */
   if (is_daemonized) {
      DRETURN_VOID;
   }

   /* The response id has 4 byte, send it to father process */
   snprintf(tmp_buffer, 4, "%3d", SGE_DEAMONIZE_OK);
   if (write(fd_pipe[1], tmp_buffer, 4) != 4) {
      dstring ds = DSTRING_INIT;
      CRITICAL(MSG_FILE_CANNOT_WRITE_SS, "fd_pipe[1]", sge_strerror(errno, &ds));
      sge_dstring_free(&ds);
   }

   sleep(2); /* give father time to read the status */

   /* close write pipe */
   close(fd_pipe[1]);

   /* close first three file descriptors */
#ifndef __INSURE__
   close(0);
   close(1);
   close(2);

   /* new descriptors acquired for stdin, stdout, stderr should be 0,1,2 */
   failed_fd = sge_occupy_first_three();
   if (failed_fd != -1) {
      CRITICAL(MSG_CANNOT_REDIRECT_STDINOUTERR_I, failed_fd);
      sge_exit(0);
   }
#endif

   SETPGRP;

   /* now have finished daemonizing */
   component_set_daemonized(true);
   DRETURN_VOID;
}

/****** uti/os/sge_daemonize() ************************************************
*  NAME
*     sge_daemonize() -- Daemonize the current application
*
*  SYNOPSIS
*     int sge_daemonize(fd_set *keep_open)
*
*  FUNCTION
*     Daemonize the current application. Throws ourself into the
*     background and dissassociates from any controlling ttys.
*     Don't close filedescriptors mentioned in 'keep_open'.
*
*  INPUTS
*     fd_set *keep_open - bitmask
*     args   optional args
*
*  RESULT
*     int - Successfull?
*         1 - Yes
*         0 - No
*
*  NOTES
*     MT-NOTES: sge_daemonize() is not MT safe
******************************************************************************/
int sge_daemonize(int *keep_open, unsigned long nr_of_fds) {

   int fd;
   pid_t pid;
   int failed_fd;

   DENTER(TOP_LAYER);

#ifndef NO_SGE_COMPILE_DEBUG
   if (TRACEON) {
      DRETURN(0);
   }
#endif

   if (component_is_daemonized()) {
      DRETURN(1);
   }

   if ((pid = fork()) != 0) {             /* 1st child not pgrp leader */
      if (pid < 0) {
         CRITICAL(MSG_PROC_FIRSTFORKFAILED_S, strerror(errno));
      }
      exit(0);
   }

   SETPGRP;

   if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
      /* disassociate contolling tty */
      ioctl(fd, TIOCNOTTY, (char *) nullptr);
      close(fd);
   }

   if ((pid = fork()) != 0) {
      if (pid < 0) {
         CRITICAL(MSG_PROC_SECONDFORKFAILED_S, strerror(errno));
      }
      exit(0);
   }

   /* close all file descriptors */
   sge_close_all_fds(keep_open, nr_of_fds);

   /* new descriptors acquired for stdin, stdout, stderr should be 0,1,2 */
   failed_fd = sge_occupy_first_three();
   if (failed_fd != -1) {
      CRITICAL(MSG_CANNOT_REDIRECT_STDINOUTERR_I, failed_fd);
      sge_exit(0);
   }

   SETPGRP;

   component_set_daemonized(true);

   DRETURN(1);
}
