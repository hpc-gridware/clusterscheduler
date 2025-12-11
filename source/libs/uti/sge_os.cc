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
#include <filesystem>
#include <string>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef SIGTSTP

#   include <sys/file.h>

#endif

#if defined(SOLARIS)
#   include <sys/termios.h>
#endif

#include "uti/msg_utilib.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sig_handlers.h"

#include "msg_common.h"

static void sge_close_fd(int fd);

/****** uti/os/sge_get_pids() *************************************************
*  NAME
*     sge_get_pids() -- Return all "pids" of a running processes 
*
*  SYNOPSIS
*     int sge_get_pids(pid_t *pids, int max_pids, const char *name, 
*                      const char *pscommand) 
*
*  FUNCTION
*     Return all "pids" of a running processes with given "name". 
*     Only first 8 characters of "name" are significant.
*     Checks only basename of command after "/".
*
*  INPUTS
*     pid_t *pids           - pid array
*     int max_pids          - size of pid array
*     const char *name      - name 
*     const char *pscommand - ps commandline
*
*  RESULT
*     int - Result 
*         0 - No program with given name found
*        >0 - Number of processes with "name" 
*        -1 - Error
*
*  NOTES
*     MT-NOTES: sge_get_pids() is not MT safe
******************************************************************************/
int sge_get_pids(pid_t *pids, int max_pids, const char *name,
                 const char *pscommand) {
   FILE *fp_in, *fp_out, *fp_err;
   char buf[10000], *ptr;
   int num_of_pids = 0, last, len;
   pid_t pid, command_pid;

   DENTER(TOP_LAYER);

   command_pid = sge_peopen("/bin/sh", 0, pscommand, nullptr, nullptr,
                            &fp_in, &fp_out, &fp_err, false);

   if (command_pid == -1) {
      DRETURN(-1);
   }

   while (!feof(fp_out) && num_of_pids < max_pids) {
      if ((fgets(buf, sizeof(buf), fp_out))) {
         if ((len = strlen(buf))) {

            /* handles first line of ps command */
            if ((pid = (pid_t) atoi(buf)) <= 0)
               continue;

            /* strip off trailing white spaces */
            last = len - 1;
            while (last >= 0 && isspace((int) buf[last])) {
               buf[last] = '\0';
               last--;
            }

            /* set pointer to first character of process name */
            while (last >= 0 && !isspace((int) buf[last]))
               last--;
            last++;

            /* DPRINTF("pid: %d - progname: >%s<\n", pid, &buf[last]); */

            /* get basename of program */
            ptr = strrchr(&buf[last], '/');
            if (ptr)
               ptr++;
            else
               ptr = &buf[last];

            /* check if process has given name */
            if (!strncmp(ptr, name, 8))
               pids[num_of_pids++] = pid;
         }
      }
   }

   sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);
   DRETURN(num_of_pids);
}

/****** uti/os/sge_contains_pid() *********************************************
*  NAME
*     sge_contains_pid() -- Checks whether pid array contains pid 
*
*  SYNOPSIS
*     int sge_contains_pid(pid_t pid, pid_t *pids, int npids) 
*
*  FUNCTION
*     whether pid array contains pid 
*
*  INPUTS
*     pid_t pid   - process id 
*     pid_t *pids - pid array 
*     int npids   - number of pids in array 
*
*  RESULT
*     int - result state
*         0 - pid was not found
*         1 - pid was found
*
*  NOTES
*     MT-NOTES: sge_contains_pid() is MT safe
******************************************************************************/
int sge_contains_pid(pid_t pid, pid_t *pids, int npids) {
   int i;

   for (i = 0; i < npids; i++) {
      if (pids[i] == pid) {
         return 1;
      }
   }
   return 0;
}

/****** uti/os/sge_checkprog() ************************************************
*  NAME
*     sge_checkprog() -- Has "pid" of a running process the given "name" 
*
*  SYNOPSIS
*     int sge_checkprog(pid_t pid, const char *name, 
*                       const char *pscommand) 
*
*  FUNCTION
*     Check if "pid" of a running process has given "name".
*     Only first 8 characters of "name" are significant.
*     Check only basename of command after "/". 
*
*  INPUTS
*     pid_t pid             - process id 
*     const char *name      - process name 
*     const char *pscommand - ps commandline 
*
*  RESULT
*     int - result state
*         0 - Process with "pid" has "name"
*         1 - No such pid or pid has other name
*        -1 - error occurred (mostly sge_peopen() failed) 
*
*  NOTES
*     MT-NOTES: sge_checkprog() is not MT safe
******************************************************************************/
int sge_checkprog(pid_t pid, const char *name, const char *pscommand) {
   FILE *fp_in, *fp_out, *fp_err;
   char buf[1000], *ptr;
   pid_t command_pid, pidfound;
   int len, last, notfound;

   DENTER(TOP_LAYER);

   command_pid = sge_peopen("/bin/sh", 0, pscommand, nullptr, nullptr,
                            &fp_in, &fp_out, &fp_err, false);

   if (command_pid == -1) {
      DRETURN(-1);
   }

   notfound = 1;
   while (!feof(fp_out)) {
      if ((fgets(buf, sizeof(buf), fp_out))) {
         if ((len = strlen(buf))) {
            pidfound = (pid_t) atoi(buf);

            if (pidfound == pid) {
               last = len - 1;
               DPRINTF("last pos in line: %d\n", last);
               while (last >= 0 && isspace((int) buf[last])) {
                  buf[last] = '\0';
                  last--;
               }

               /* DPRINTF("last pos in line now: %d\n", last); */

               while (last >= 0 && !isspace((int) buf[last]))
                  last--;
               last++;

               /* DPRINTF("pid: %d - progname: >%s<\n", pid, &buf[last]); */

               /* get basename of program */
               ptr = strrchr(&buf[last], '/');
               if (ptr)
                  ptr++;
               else
                  ptr = &buf[last];

               if (!strncmp(ptr, name, 8)) {
                  notfound = 0;
                  break;
               } else
                  break;
            }
         }
      }
   }

   sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);

   DRETURN(notfound);
}

/****** uti/os/redirect_to_dev_null() ******************************************
*  NAME
*     redirect_to_dev_null() -- redirect a channel to /dev/null
*
*  SYNOPSIS
*     int redirect_to_dev_null(int target, int mode) 
*
*  FUNCTION
*     Attaches a certain filedescriptor to /dev/null.
*
*  INPUTS
*     int target - file descriptor
*     int mode   - mode for open
*
*  RESULT
*     int - target fd number if there was an error
*           else -1
*
*  NOTES
*     MT-NOTE: redirect_to_dev_null() is MT safe 
*
*******************************************************************************/
int redirect_to_dev_null(int target, int mode) {
   SGE_STRUCT_STAT buf{};

   if (SGE_FSTAT(target, &buf)) {
      if ((open("/dev/null", mode, 0)) != target) {
         return target;
      }
   }

   return -1;
}

/****** uti/os/sge_occupy_first_three() ***************************************
*  NAME
*     sge_occupy_first_three() -- Open descriptor 0, 1, 2 to /dev/null
*
*  SYNOPSIS
*     int sge_occupy_first_three()
*
*  FUNCTION
*     Occupy the first three filedescriptors, if not available. This is done
*     to be sure that a communication by a socket will not get any "forgotten"
*     print output from code.
*
*  RESULT
*     int - error state
*        -1 - OK
*         0 - there are problems with stdin
*         1 - there are problems with stdout
*         2 - there are problems with stderr
*
*  NOTES
*     MT-NOTE: sge_occupy_first_three() is MT safe
*
*  SEE ALSO
*     uti/os/redirect_to_dev_null()
*     uti/os/sge_close_all_fds()
******************************************************************************/
int sge_occupy_first_three() {
   int ret = -1;

   DENTER(TOP_LAYER);

   ret = redirect_to_dev_null(0, O_RDONLY);

   if (ret == -1) {
      ret = redirect_to_dev_null(1, O_WRONLY);
   }

   if (ret == -1) {
      ret = redirect_to_dev_null(2, O_WRONLY);
   }

   DRETURN(ret);
}

#ifdef __INSURE__
extern int _insure_is_internal_fd(int);
#endif

/****** uti/os/sge_get_max_fd() ************************************************
*  NAME
*     sge_get_max_fd() -- get max filedescriptor count
*
*  SYNOPSIS
*     int sge_get_max_fd() 
*
*  FUNCTION
*     This function returns the nr of file descriptors which are available 
*     (Where fd 0 is the first one).
*     So the highest file descriptor value is: max_fd - 1.
*
*  INPUTS
*     void - no input parameters
*
*  RESULT
*     int - max. possible open file descriptor count on this system
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
int sge_get_max_fd() {
   return sysconf(_SC_OPEN_MAX);
}

#if not defined(LINUX) && not defined(SOLARIS)
/****** uti/os/fd_compare() ****************************************************
*  NAME
*     fd_compare() -- file descriptor compare function for qsort()
*
*  SYNOPSIS
*     static int fd_compare(const void* fd1, const void* fd2) 
*
*  FUNCTION
*     qsort() needs a callback function to compare two filedescriptors for
*     sorting them. This is the implementation to value the difference of two
*     file descriptors. If one parameter is nullptr, only the pointers are
*     used for the comparsion.
*     Used by sge_close_all_fds().
*
*  INPUTS
*     const void* fd1 - pointer to an int (file descriptor 1)
*     const void* fd2 - pointer to an int (file descriptor 2)
*
*  RESULT
*     static int - compare result (1, 0 or -1)
*                  1  : fd1 > fd2
*                  0  : fd1 == fd2
*                  -1 : fd1 < fd2
*
*  NOTES
*     MT-NOTE: fd_compare() is MT safe 
*
*  SEE ALSO
*     uti/os/sge_close_all_fds()
*******************************************************************************/
static int fd_compare(const void *fd1, const void *fd2) {
   int *i1 = (int *) fd1;
   int *i2 = (int *) fd2;

   /* If there are nullptr pointer we also try to compare them */
   if (i1 == nullptr || i2 == nullptr) {
      if (i1 > i2) {
         return 1;
      }
      if (i1 < i2) {
         return -1;
      }
      return 0;
   }

   if (*i1 > *i2) {
      return 1;
   }
   if (*i1 < *i2) {
      return -1;
   }
   return 0;
}
#endif

#if defined(LINUX) || defined(SOLARIS)
/**
 * @brief Get all open file descriptors for a process
 *
 * Retrieves all currently open file descriptors for the specified process
 * by reading the /proc/[pid]/fdinfo directory (or /proc/self/fdinfo if pid is 0).
 * The function uses opendir() to iterate over the directory entries and
 * automatically excludes the file descriptor used by the directory stream itself.
 *
 * @param pid Process id (0 for current process/self)
 * @return std::set<int> Set containing all open file descriptor numbers
 * @note MT-SAFE: get_all_fds() is MT safe
 * @see sge_close_all_fds()
 */
std::set<int> get_all_fds(pid_t pid) {
   DENTER(TOP_LAYER);

   std::set<int> fds;

   std::filesystem::path proc_path = "/proc/";
   if (pid == 0) {
      proc_path += "self/fd";
   } else {
      proc_path += std::to_string(pid) + "/fd";
   }
#if 0
   // Unfortunately, we cannot use the C++ directory_iterator for iterating over /proc/*/fdinfo:
   // It opens itself a file handle which shows up in the /proc/*/fdinfo - but we cannot figure
   // out which one belongs to the iterator!
   // With opendir() we can use dirfd() to get the fd of the directory stream itself and can ignore it.
   try {
      std::filesystem::directory_iterator iter{proc_path};
      for (const auto &entry : iter) {
         int fd = std::stoi(entry.path().filename());
         fds.insert(fd);
      }
   } catch (std::filesystem::filesystem_error &e) {
      // this should never happen
   }
#else
   DIR *cwd = opendir(proc_path.c_str());
   if (cwd == nullptr) {
      DPRINTF("get_all_fds(): cannot open directory %s: %s", proc_path.c_str(), strerror(errno));
   } else {
      // Iterate over the directory stream.
      // Do not use readdir.2 or readdir_r - they are deprecated.
      // See readdir.3 man page.
      int cwd_fd = dirfd(cwd);
      dirent *dent;
      while ((dent = readdir(cwd)) != nullptr) {
         if (dent->d_name[0] == '\0') {
            DPRINTF("get_all_fds(): empty filename in directory %s", proc_path.c_str());
         } else if (strcmp(dent->d_name, "..") == 0 || strcmp(dent->d_name, ".") == 0) {
            DPRINTF("get_all_fds(): skipping %s", dent->d_name);
         } else {
            int fd = std::stoi(dent->d_name);
            if (fd == cwd_fd) {
               DPRINTF("get_all_fds(): skipping opendir() internal fd %d", fd);
            } else {
               // This is a valid fd, store it in the returned set.
               fds.insert(fd);
            }
         }
      }
      closedir(cwd);
   }

#endif

   DRETURN(fds);
}
#endif

/****** uti/os/sge_close_fd() **************************************************
*  NAME
*     sge_close_fd() -- close a file descriptor
*
*  SYNOPSIS
*     static void sge_close_fd(int fd) 
*
*  FUNCTION
*     This function closes the specified file descriptor on an architecture
*     specific way. If __INSURE__ is defined during compile time and it is an
*     fd used by insure the file descriptor is not closed.
*
*  INPUTS
*     int fd - file descriptor number to close
*
*  RESULT
*     static void - no return value
*
*  SEE ALSO
*     uti/os/sge_close_all_fds()
*******************************************************************************/
static void sge_close_fd(int fd) {
   DENTER(TOP_LAYER);
#ifdef __INSURE__
   if (_insure_is_internal_fd(fd)) {
      return;
   }
#endif
#if 1
   if (close(fd) == -1) {
      DPRINTF("close(%d) failed: %s\n", fd, strerror(errno));
   }
#else
   // @todo When closing all fds is called for fork()/exec(), we could just set the FD_CLOEXEC flag
   // on the file descriptor. This will not close the fd, but just mark it to be closed on any
   // of the various exec() calls.
   // A short test showed that setting the flag instead of closing the fd is about 30% faster.
   // It would be worth doing only if the caller has a high number of file descriptors open,
   // which is not the case on the execution side, but might be in sge_qmaster,
   // e.g., when starting a JSV script in a big cluster.
   if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
      DPRINTF("fcntl(%d, F_SETFD, FD_CLOEXEC) failed: %s\n", fd, strerror(errno));
   }
#endif
   DRETURN_VOID;
}

/**
 * @brief close all open file descriptors
 *
 * This function is used to close all open file descriptors for
 * the current process.
 *
 * It is possible to pass a list (int array) of file descriptors which shall
 * be kept open.
 *
 * The algorithm used depends on the operating system:
 *    * On Linux and Solaris we can figure out the actually open file descriptors
 *      by looping over all files in `/proc/self/fdinfo` and closing them.
 *    * On other OSes we loop from 0 to the maximum possible file descriptor
 *      and (try to) close them.
 *    * On newer Linux versions we could possibly use the `close_range()` function,
 *      ideally using the flag CLOSE_RANGE_CLOEXEC which will not immediately close
 *      the fd but just mark it to be closed on `exec()` calls - which is
 *      a situation where we call `close_all_fds()`.
 *      But it is not yet available on our default build platform, CentOS 8.
 *
 * @param keep_open - optionally: int array of file descriptors to keep open
 * @param nr_of_keep_open_entries - number of entries in keep_open
 * @todo What about fcntl(fd, F_SETFD, FD_CLOEXEC)? See comment in sge_close_fd().
 */
#if defined(LINUX) || defined(SOLARIS)
static bool keep_fd_open(int *keep_open, unsigned long nr_of_keep_open_entries, int fd) {
   for (unsigned long keep_open_array_index = 0; keep_open_array_index < nr_of_keep_open_entries; keep_open_array_index++) {
      if (keep_open[keep_open_array_index] == fd) {
         return true;
      }
   }
   return false;
}

void sge_close_all_fds(int *keep_open, unsigned long nr_of_keep_open_entries) {
   std::set all_fds = get_all_fds();
   for (auto fd : all_fds) {
      if (keep_open == nullptr || !keep_fd_open(keep_open, nr_of_keep_open_entries, fd)) {
         sge_close_fd(fd);
      }
   }
}
#else
void sge_close_all_fds(int *keep_open, unsigned long nr_of_keep_open_entries) {
   int maxfd = sge_get_max_fd();
   if (keep_open == nullptr) {
      /* if we do not have any keep_open we can delete all fds */
      for (int fd = 0; fd < maxfd; fd++) {
         sge_close_fd(fd);
      }
   } else {
      int current_fd_keep_open;
      int current_fd_to_close = 0;

      /* First sort the keep open list */
      qsort((void *) keep_open, nr_of_keep_open_entries, sizeof(int), fd_compare);

      /* Now go over the int array and do a close loop to the current value */
      for (unsigned long keep_open_array_index = 0; keep_open_array_index < nr_of_keep_open_entries; keep_open_array_index++) {

         /* test if keep open fd is a valid fd */
         current_fd_keep_open = keep_open[keep_open_array_index];
         if (current_fd_keep_open < 0 || current_fd_keep_open >= maxfd) {
            continue;
         }

         /* we can close all fds up to current_fd_keep_open */
         for (int fd = current_fd_to_close; fd < current_fd_keep_open; fd++) {
            sge_close_fd(fd);
         }

         /*
          * now we reached current_fd_keep_open, simple set current_fd_to_close to
          * current_fd_keep_open + 1 for the next run (=skip current_fd_keep_open)
          */
         current_fd_to_close = current_fd_keep_open + 1;
      }

      /* Now close up to fd nr (max_fd - 1)  */
      for (int fd = current_fd_to_close; fd < maxfd; fd++) {
         sge_close_fd(fd);
      }
   }
}
#endif

/****** uti/os/sge_dup_fd_above_stderr() **************************************
*  NAME
*     sge_dup_fd_above_stderr() -- Make sure a fd is >=3
*
*  SYNOPSIS
*     int sge_dup_fd_above_stderr(int *fd) 
*
*  FUNCTION
*     This function checks if the given fd is <3, if yes it dups it to be >=3.
*     The fd obtained by open(), socket(), pipe(), etc. can be <3 if stdin, 
*     stdout and/or stderr are closed. As it is difficult for an application
*     to determine if the first three fds are connected to the std-handles or
*     something else and because many programmers just rely on these three fds
*     to be connected to the std-handles, this function makes sure that it 
*     doesn't use these three fds.
*
*  INPUTS
*     int *fd - pointer to the fd which is to be checked and dupped.
*
*  RESULT
*     int - 0: Ok
*          >0: errno
*
*  SEE ALSO
*******************************************************************************/
int sge_dup_fd_above_stderr(int *fd) {
   if (fd == nullptr) {
      return EINVAL;
   }
   /* 
    * make sure the provided *fd is not 0, 1 or 2  - anyone can close
    * stdin, stdout or stderr without checking what these really are
    */
   if (*fd < 3) {
      int tmp_fd;
      if ((tmp_fd = fcntl(*fd, F_DUPFD, 3)) == -1) {
         return errno;
      }
      close(*fd);
      *fd = tmp_fd;
   }
   return 0;
}
