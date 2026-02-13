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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 * Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>

#include <termios.h>
#if defined(DARWIN)
#  include <sys/ioctl.h>
#  include <grp.h>
#elif defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
#  include <stropts.h>
#elif defined(FREEBSD) || defined(NETBSD)
#  include <libutil.h>
#endif

#include "uti/sge_pty.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "err_trace.h"
#include "ocs_shepherd_pty.h"

int g_newpgrp = -1;

#if not defined(FREEBSD)
/****** uti/pty/ptym_open() ****************************************************
*  NAME
*     ptym_open() -- Opens a pty master device
*
*  SYNOPSIS
*     int ptym_open(char *pts_name)
*
*  FUNCTION
*     Searches for a free pty master device and opens it.
*
*  INPUTS
*     char *pts_name - A buffer that is to receive the name of the
*                      pty master device. Must be at least 12 bytes large.
*
*  RESULT
*     int - The file descriptor of the pty master device.
*           -1 in case of error.
*
*  NOTES
*     MT-NOTE: ptym_open() is not MT safe
*
*  SEE ALSO
*     pty/ptys_open()
*******************************************************************************/
#if defined(DARWIN)
static int
ptym_open(char *pts_name, uid_t uid)
{
   char ptr1[] = "pqrstuvwxyzPQRST";
   char ptr2[] = "0123456789abcdef";
   int  fdm, i, j;

   strcpy(pts_name, "/dev/ptyXY");

   /*
    * iterate over all possible pty names: /dev/ptyXY
    * X = ptr1, Y = ptr2
    */
   for (i=0; ptr1[i] != '\0'; i++) {
      pts_name[8] = ptr1[i];
      for (j=0; ptr2[j] != '\0'; j++) {
         pts_name[9] = ptr2[j];

         /* try to open master */
         if ((fdm = open(pts_name, O_RDWR)) < 0) {
            if (errno == ENOENT) { /* different from EIO */
               return -1;        /* out of pty devices */
            } else {
               continue;         /* try next pty device */
            }
         }

         pts_name[5] = 't';   /* change "pty" to "tty" */
         return fdm;      /* got it, return fd of master */
      }
   }
   return -1;  /* out of pty devices */
}
#else
static int
ptym_open(char *pts_name, uid_t uid) {
   // Flags:
   //    O_RDWR   Open the device for both reading and writing.  It is usual to specify this flag.
   //    O_NOCTTY Do not make this device the controlling terminal for the process.
   //
   // The parent process does not want the PTY as controlling terminal.
   // The child process explicitly opens the slave side and makes it the controlling terminal for its (newly created)
   // process group.
   int fdm = posix_openpt(O_RDWR | O_NOCTTY);
   if (fdm < 0) {
      shepherd_trace("posix_openpt() failed %d: %s", errno, strerror(errno));
      return -1;
   }

   char *ptr;
   if ((ptr = ptsname(fdm)) == nullptr) {   /* get slave's name */
      // Does ptsname() set errno? Doesn't look so ...
      shepherd_trace("ptsname() failed");
      close(fdm);
      return -2;
   }

   strcpy(pts_name, ptr);  /* return name of slave */

   // We now want to use grantpt() to set ownership and permissions of the slave pseudo device.
   // BUT: For grantpt() the real user id needs to be set to the uid of the job user,
   //      but once we changed that to != 0 we cannot get back to the original uid/euid settings.
   //      On Linux there would be setresuid which should allow us to swap uid and euid, but this does not
   //      exist on all OSes.
   // Therefore, we need to change owner explicitly.
   if (grantpt(fdm) < 0) {    /* grant access to slave */
      shepherd_trace("grantpt() failed %d: %s", errno, strerror(errno));
      close(fdm);
      return -3;
   }
   if (uid != 0) {
      if (chown(pts_name, uid, -1) < 0) {
         shepherd_trace("chown(%s, %d) failed %d: %s", pts_name, uid, errno, strerror(errno));
         close(fdm);
         return -3;
      }
   }

   if (unlockpt(fdm) < 0) {   /* clear slave's lock flag */
      shepherd_trace("unlockpt() failed %d: %s", errno, strerror(errno));
      close(fdm);
      return -4;
   }

   return fdm;             /* return fd of master */
}

#endif

/****** uti/pty/ptys_open() ****************************************************
*  NAME
*     ptys_open() -- Opens a pty slave device.
*
*  SYNOPSIS
*     int ptys_open(int fdm, char *pts_name)
*
*  FUNCTION
*     Opens a pty slave device that matches to a given pty master device.
*
*  INPUTS
*     int fdm        - File descriptor of the pty master device.
*     char *pts_name - The name of the master slave device.
*
*  RESULT
*     int - File descriptor of the pty slave device.
*           -1 in case of error.
*
*  NOTES
*     MT-NOTE: ptys_open() is not MT safe
*
*  SEE ALSO
*     pty/ptym_open
*******************************************************************************/
#if defined(DARWIN)
static int
ptys_open(int fdm, char *pts_name)
{
   struct group gr_struct{};
   struct group *grptr;
   gid_t gid;
   int fds;
   char *gr_buffer;
   size_t gr_buffer_size;

   gr_buffer_size = get_group_buffer_size();
   gr_buffer = sge_malloc(gr_buffer_size);

   if (getgrnam_r("tty", &gr_struct, gr_buffer, gr_buffer_size, &grptr) == 0) {
      gid = grptr->gr_gid;
   } else {
      gid = -1;      /* group tty is not in the group file */
   }

   sge_free(&gr_buffer);

   /* following two functions don't work unless we're root */
   chown(pts_name, getuid(), gid);
   chmod(pts_name, S_IRUSR | S_IWUSR | S_IWGRP);

   if ((fds = open(pts_name, O_RDWR)) < 0) {
      close(fdm);
      return -1;
   }
   return fds;
}
#else

static int
ptys_open(int fdm, char *pts_name) {
   int fds;

   shepherd_trace("child_process: before opening %s: isatty(STDIN_FILENO) = %d", pts_name, isatty(STDIN_FILENO));
   /* following should allocate controlling terminal */
   if ((fds = open(pts_name, O_RDWR)) < 0) {
      shepherd_trace("open(%s) failed %d: %s", pts_name, errno, strerror(errno));
      close(fdm);
      return -5;
   }
   shepherd_trace("child_process: isatty(fds) = %d", isatty(fds));
#if defined(SOLARIS64) || defined(SOLARIS86) || defined(SOLARISAMD64)
   if (ioctl(fds, I_PUSH, "ptem") < 0) {
      shepherd_trace("ioctl(ptem) failed %d: %s", errno, strerror(errno));
      close(fdm);
      close(fds);
      return -6;
   }
   if (ioctl(fds, I_PUSH, "ldterm") < 0) {
      shepherd_trace("ioctl(ldterm) failed %d: %s", errno, strerror(errno));
      close(fdm);
      close(fds);
      return -7;
   }
   if (ioctl(fds, I_PUSH, "ttcompat") < 0) {
      shepherd_trace("ioctl(ttcompat) failed %d: %s", errno, strerror(errno));
      close(fdm);
      close(fds);
      return -8;
   }
#endif

   return fds;
}
#endif

#endif

/****** uti/pty/fork_pty() *****************************************************
*  NAME
*     fork_pty() -- Opens a pty, forks and redirects the std handles
*
*  SYNOPSIS
*     pid_t fork_pty(int *ptrfdm, int *fd_pipe_err, dstring *err_msg)
*
*  FUNCTION
*     Opens a pty, forks and redirects stdin, stdout and stderr of the child
*     to the pty.
*
*  INPUTS
*     int *ptrfdm      - Receives the file descriptor of the master side of
*                        the pty.
*     int *fd_pipe_err - A int[2] array that receives the file descriptors
*                        of a pipe to separately redirect stderr.
*                        To achieve the same behaviour like rlogin/rsh, this
*                        is normally disabled, compile with
*                        -DUSE_PTY_AND_PIPE_ERR to enable this feature.
*     dstring *err_msg - Receives an error string in case of error.
*
*  RESULT
*     pid_t - -1 in case of error,
*              0 in the child process,
*              or the pid of the child process in the parent process.
*
*  NOTES
*     MT-NOTE: fork_pty() is not MT safe
*
*  SEE ALSO
*     pty/fork_no_pty
*******************************************************************************/
pid_t fork_pty(int *ptrfdm, int *fd_pipe_err, dstring *err_msg) {
   pid_t pid;
   int fdm, fds;
   struct termios tio{};
   char pts_name[64];

   /*
    * We run this either as root with euid="sge admin user" or as an unprivileged
    * user.  If we are root with euid="sge admin user", we must change our
    * euid back to root for this function.
    */
   uid_t old_euid = geteuid();
   bool changed_uid = false;
   if (old_euid != SGE_SUPERUSER_UID) {
      seteuid(SGE_SUPERUSER_UID);
      changed_uid = true;
   }
#if defined(FREEBSD) || defined(NETBSD)
   if (openpty(&fdm, &fds, pts_name, nullptr, nullptr) < 0) {
      sge_dstring_sprintf(err_msg, "can't open master and slave pty: %d, %s", errno, strerror(errno));
      return -1;
   }
#else
   if ((fdm = ptym_open(pts_name, old_euid)) < 0) {
      sge_dstring_sprintf(err_msg, "can't open master pty \"%s\": %d, %s", pts_name, errno, strerror(errno));
      return -1;
   }
   shepherd_trace("created pty with slave device %s", pts_name);
#endif
   if (changed_uid) {
      seteuid(old_euid);
   }
#if defined(USE_PTY_AND_PIPE_ERR)
   if (pipe(fd_pipe_err) == -1) {
      sge_dstring_sprintf(err_msg, "can't create pipe for stderr: %d, %s",
                          errno, strerror(errno));
      close(fdm);
      return -1;
   }
#endif
   if ((pid = fork()) < 0) {
      close(fdm);
      return -1;
   } else if (pid == 0) {     /* child */
      // Create a new session and process group.
      if ((g_newpgrp = setsid()) < 0) {
         sge_dstring_sprintf(err_msg, "setsid() error: %d, %s", errno, strerror(errno));
         return -1;
      }

#if defined(FREEBSD) || defined(NETBSD)
      // fds is already the slave that was created by openpty
#else
      /* Open pty slave */
      if ((fds = ptys_open(fdm, pts_name)) < 0) {
         sge_dstring_sprintf(err_msg, "can't open slave pty: %d", fds);
         return -1;
      }
#endif
      close(fdm);

      // Set raw mode = without buffering.
      // When we received job input from qrsh we want the IJS thread (commlib_to_pty) to wait until the job has
      // actually started and reads the input. Otherwise qrsh might read all input (e.g. redirected from a file)
      // and on EOF send a STDIN_CLOSE_MSG => commlib_to_pty will close stdin, the job will get a SIGHUP before
      // it even fully started up.
      // And reading from a buffered terminal will partly duplicate the data read by pty_to_commlib thread.
#if 0
      if (terminal_enter_raw_mode() != 0) {
         shepherd_trace("setting terminal to raw mode failed");
      }
#endif

      /*
       * Set the remote pty to break lines with NL, not CR NL. This ensures
       * line breaks are not modified when e.g.  "cat file.txt" is run in
       * the qrsh session.
       */
      if (tcgetattr(fds, &tio) == 0) {
         tio.c_oflag &= ~ONLCR;
         tcsetattr(fds, TCSANOW, &tio);
      }

#if   defined(TIOCSCTTY) && !defined(CIBAUD)
      /* 44BSD way to acquire controlling terminal */
      /* !CIBAUD to avoid doing this under SunOS */
      if (ioctl(fds, TIOCSCTTY, (char *) nullptr) < 0) {
         sge_dstring_sprintf(err_msg, "TIOCSCTTY error: %d, %s", errno, strerror(errno));
         return -1;
      }
#endif
      /* slave becomes stdin/stdout/stderr of child */
      if ((dup2(fds, STDIN_FILENO)) != STDIN_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stdin error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
      if ((dup2(fds, STDOUT_FILENO)) != STDOUT_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stdout error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
#if defined(USE_PTY_AND_PIPE_ERR)
      close(fd_pipe_err[0]); fd_pipe_err[0] = -1;
      if ((dup2(fd_pipe_err[1], STDERR_FILENO)) != STDERR_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stderr error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
      close(fd_pipe_err[1]); fd_pipe_err[1] = -1;
#else
      if ((dup2(fds, STDERR_FILENO)) != STDERR_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stderr error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
#endif

      if (fds > STDERR_FILENO) {
         close(fds);
      }

      shepherd_trace("child_process: after dup2: isatty(STDIN_FILENO) = %d", isatty(STDIN_FILENO));

      return 0;      /* child returns 0 just like fork() */
   } else {          /* parent */
      shepherd_trace("parent_process: isatty(STDIN_FILENO) = %d", isatty(STDIN_FILENO));
      *ptrfdm = fdm; /* return fd of master */
      close(fd_pipe_err[1]);
      fd_pipe_err[1] = -1;
      return pid;    /* parent returns pid of child */
   }
}

/****** uti/pty/fork_no_pty() **************************************************
*  NAME
*     fork_no_pty() -- Opens pipes, forks and redirects the std handles
*
*  SYNOPSIS
*     pid_t fork_no_pty(int *fd_pipe_in, int *fd_pipe_out, int *fd_pipe_err,
*     dstring *err_msg)
*
*  FUNCTION
*     Opens three pipes, forks and redirects stdin, stdout and stderr of the
*     child to the pty.
*
*  INPUTS
*     int *fd_pipe_in  - int[2] array for the two stdin pipe file descriptors
*     int *fd_pipe_out - int[2] array for the two stdout pipe file descriptors
*     int *fd_pipe_err - int[2] array for the two stderr pipe file descriptors
*     dstring *err_msg - Receives an error string in case of error.
*
*  RESULT
*     pid_t - -1 in case of error,
*              0 in the child process,
*              or the pid of the child process in the parent process.
*
*  NOTES
*     MT-NOTE: fork_no_pty() is not MT safe
*
*  SEE ALSO
*     pty/fork_pty()
*******************************************************************************/
pid_t fork_no_pty(int *fd_pipe_in, int *fd_pipe_out,
                  int *fd_pipe_err, dstring *err_msg) {
   int ret;
   pid_t pid;

   DENTER(TOP_LAYER);

   ret = pipe(fd_pipe_in);
   if (ret == -1) {
      sge_dstring_sprintf(err_msg, "can't create pipe for stdin: %d: %s",
                          errno, strerror(errno));
      return -1;
   }

   ret = pipe(fd_pipe_out);
   if (ret == -1) {
      sge_dstring_sprintf(err_msg, "can't create pipe for stdout: %d: %s",
                          errno, strerror(errno));
      return -1;
   }

   ret = pipe(fd_pipe_err);
   if (ret == -1) {
      sge_dstring_sprintf(err_msg, "can't create pipe for stderr: %d: %s",
                          errno, strerror(errno));
      return -1;
   }

   if ((pid = fork()) < 0) {
      return -1;
   } else if (pid == 0) {     /* child */
      if (setsid() < 0) {
         sge_dstring_sprintf(err_msg, "setsid() error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }

      /* attach pipes to stdin/stdout/stderr of child */
      close(fd_pipe_in[1]);
      fd_pipe_in[1] = -1;
      if ((dup2(fd_pipe_in[0], STDIN_FILENO)) != STDIN_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stdin error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
      close(fd_pipe_in[0]);
      fd_pipe_in[0] = -1;

      close(fd_pipe_out[0]);
      fd_pipe_out[0] = -1;
      if ((dup2(fd_pipe_out[1], STDOUT_FILENO)) != STDOUT_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stdout error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
      close(fd_pipe_out[1]);
      fd_pipe_out[1] = -1;

      close(fd_pipe_err[0]);
      fd_pipe_out[0] = -1;
      if ((dup2(fd_pipe_err[1], STDERR_FILENO)) != STDERR_FILENO) {
         sge_dstring_sprintf(err_msg, "dup2 to stderr error: %d, %s",
                             errno, strerror(errno));
         return -1;
      }
      close(fd_pipe_err[1]);
      fd_pipe_err[1] = -1;
   } else {  /* parent */
      close(fd_pipe_in[0]);
      fd_pipe_in[0] = -1;
      close(fd_pipe_out[1]);
      fd_pipe_out[1] = -1;
      close(fd_pipe_err[1]);
      fd_pipe_err[1] = -1;
   }
   DRETURN(pid);
}


