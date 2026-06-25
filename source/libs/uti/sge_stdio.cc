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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include <sys/wait.h>
#include <pwd.h>
#include <csignal>
#include <fcntl.h>

#include "uti/msg_utilib.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include <cinttypes>

#include "msg_common.h"

#if defined(SOLARIS)
#   include "sge_smf.h"
#endif

#ifdef NO_SGE_COMPILE_DEBUG
#   undef SGE_EXIT
#   define sge_exit(x)     exit(x)
#endif

static void addenv(const char *, const char *);

static void addenv(const char *key, const char *value) {
   char *str;

   str = sge_malloc(strlen(key) + strlen(value) + 2);
   if (!str) {
      return;
   }

   strcpy(str, key);
   strcat(str, "=");
   strcat(str, value);

   putenv(str);
   /* there is intentionally no sge_free(&str) */
}

/* 
 * TODO: CLEANUP
 *
 * This function is DEPRECATED and should be removed with the next
 * major release.
 *
 * This function can't be used in multi threaded environments because it
 * might cause a deadlock in the executing qmaster thread.
 * Use sge_peopen_r() instead.
 */
pid_t sge_peopen(const char *shell, int login_shell, const char *command,
                 const char *user, char **env, FILE **fp_in, FILE **fp_out,
                 FILE **fp_err, bool null_stderr) {
   pid_t pid;
   int pipefds[3][2];
   const char *could_not = MSG_SYSTEM_EXECBINSHFAILED;
   const char *not_root = MSG_SYSTEM_NOROOTRIGHTSTOSWITCHUSER;
   int i;
   char arg0[256];
   char err_str[256];
   int res;
   uid_t myuid;

   DENTER(TOP_LAYER);

   /* open pipes - close on failure */
   for (i = 0; i < 3; i++) {
      if (pipe(pipefds[i]) != 0) {
         while (--i >= 0) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
         }
         ERROR(SGE_EVENT, MSG_SYSTEM_FAILOPENPIPES_SS, command, strerror(errno));
         DRETURN(-1);
      }
   }

#if defined(SOLARIS)
   pid = sge_smf_contract_fork(err_str, 256);
#else
   pid = fork();
#endif
   if (pid == 0) {  /* child */
      int keep_open[6];
      keep_open[0] = 0;
      keep_open[1] = 1;
      keep_open[2] = 2;
      keep_open[3] = pipefds[0][0];
      keep_open[4] = pipefds[1][1];
      keep_open[5] = pipefds[2][1];
      sge_close_all_fds(keep_open, 6);
      /* shall we redirect stderr to /dev/null? */
      if (null_stderr) {
         /* open /dev/null */
         int fd = open("/dev/null", O_WRONLY);
         if (fd == -1) {
            snprintf(err_str, sizeof(err_str), MSG_ERROROPENINGFILEFORWRITING_SS, "/dev/null", strerror(errno));
            snprintf(err_str, sizeof(err_str), "\n");
            if (write(2, err_str, strlen(err_str)) != (ssize_t)strlen(err_str)) {
               /* nothing we can do here - we are anyway about to exit */
            }
            sge_exit(1);
         }

         /* set stderr to /dev/null */
         close(2);
         if (dup(fd) == -1) {
            /* we have a serious problem here - nothing we can do but exit */
            sge_exit(1);
         }

         /* we don't need the stderr the pipe - close it */
         close(pipefds[2][1]);
      } else {
         /* redirect stderr to the pipe */
         close(2);
         if (dup(pipefds[2][1]) == -1) {
            /* we have a serious problem here - nothing we can do but exit */
            sge_exit(1);
         }
      }

      /* redirect stdin and stdout to the pipes */
      close(0);
      close(1);
      if (dup(pipefds[0][0]) == -1 ||
          dup(pipefds[1][1]) == -1) {
         /* we have a serious problem here - nothing we can do but exit */
         sge_exit(1);
      }

      if (user) {
         struct passwd *pw;
         struct passwd pw_struct{};
         char *buffer;
         int size;

         size = get_pw_buffer_size();
         buffer = sge_malloc(size);
         SGE_ASSERT(buffer != nullptr);
         if (!(pw = sge_getpwnam_r(user, &pw_struct, buffer, size))) {
            snprintf(err_str, sizeof(err_str), MSG_SYSTEM_NOUSERFOUND_SS, user, strerror(errno));
            snprintf(err_str, sizeof(err_str), "\n");
            if (write(2, err_str, strlen(err_str)) != (ssize_t)strlen(err_str)) {
               /* nothing we can do here - we are anyway about to exit */
            }
            sge_free(&buffer);
            sge_exit(1);
         }

         myuid = geteuid();
         if (myuid != pw->pw_uid) {
            /* Only change user if we differ from the wanted user */
            if (myuid != SGE_SUPERUSER_UID) {
               // strlen, not sizeof(const char*) which is the 8-byte pointer
               // size and truncates the message (CS-2360, CWE-467).
               const size_t not_root_len = strlen(not_root);
               if (write(2, not_root, not_root_len) != static_cast<ssize_t>(not_root_len)) {
                  /* nothing we can do here - we are anyway about to exit */
               }
               sge_free(&buffer);
               sge_exit(1);
            }
            snprintf(err_str, sizeof(err_str), "%s %d\n", pw->pw_name, (int) pw->pw_gid);
            if (write(2, err_str, strlen(err_str)) != (ssize_t)strlen(err_str)) {
               /* TODO: required protocol step? If sending fails, exit? */
            }
            res = initgroups(pw->pw_name, pw->pw_gid);
            if (res) {
               snprintf(err_str, sizeof(err_str), MSG_SYSTEM_INITGROUPSFORUSERFAILED_ISS, res, user, strerror(errno));
               snprintf(err_str, sizeof(err_str), "\n");
               if (write(2, err_str, strlen(err_str)) != (ssize_t)strlen(err_str)) {
                  /* nothing we can do here - we are anyway about to exit */
               }
               sge_free(&buffer);
               sge_exit(1);
            }

            /* drop the primary group before the uid - initgroups() only sets the
             * supplementary groups, so without this setgid() the child would keep
             * the parent's (root) gid (CS-2333). Must precede setuid() while
             * we are still privileged. */
            if (setgid(pw->pw_gid)) {
               snprintf(err_str, sizeof(err_str), MSG_SYSTEM_SETGIDFAILED_g, pw->pw_gid);
               snprintf(err_str, sizeof(err_str), "\n");
               if (write(2, err_str, strlen(err_str)) != (ssize_t)strlen(err_str)) {
                  /* nothing we can do here - we are anyway about to exit */
               }
               sge_free(&buffer);
               sge_exit(1);
            }

            if (setuid(pw->pw_uid)) {
               snprintf(err_str, sizeof(err_str), MSG_SYSTEM_SWITCHTOUSERFAILED_SS, user, strerror(errno));
               snprintf(err_str, sizeof(err_str), "\n");
               if (write(2, err_str, strlen(err_str)) != (ssize_t)strlen(err_str)) {
                  /* nothing we can do here - we are anyway about to exit */
               }
               sge_free(&buffer);
               sge_exit(1);
            }
         }

         addenv("HOME", pw->pw_dir);
         addenv("SHELL", pw->pw_shell);
         addenv("USER", pw->pw_name);
         addenv("LOGNAME", pw->pw_name);
         addenv("PATH", SGE_DEFAULT_PATH);

         sge_free(&buffer);
      }

      // bounded build of argv[0]; shell is conceptually a configurable path, so
      // never strcat it into the fixed arg0[256] (CS-2356, CWE-121). A
      // pathological shell path is truncated instead of overflowing the stack.
      snprintf(arg0, sizeof(arg0), "%s%s", login_shell ? "-" : "", shell);

      if (env) {
         for (; *env; env++) {
            putenv(*env);
         }
      }

      execlp(shell, arg0, "-c", command, nullptr);

      // strlen, not sizeof(const char*) which is the 8-byte pointer size and
      // truncates the message (CS-2360, CWE-467).
      const size_t could_not_len = strlen(could_not);
      if (write(2, could_not, could_not_len) != static_cast<ssize_t>(could_not_len)) {
         /* nothing we can do here - we are anyway about to exit */
      }
      sge_exit(1);
   } /* end child */

   if (pid < 0) {
      for (i = 0; i < 3; i++) {
         close(pipefds[i][0]);
         close(pipefds[i][1]);
      }
#if defined(SOLARIS)
      if (pid < -1 && err_str) {
          ERROR(MSG_SMF_FORK_FAILED_SS, "sge_peopen()", err_str);
      }
#endif
      /* fork could have failed, report it */
      ERROR(MSG_SMF_FORK_FAILED_SS, "sge_peopen()", strerror(errno));
      DRETURN(-1);
   }

   /* close the childs ends of the pipes */
   close(pipefds[0][0]);
   close(pipefds[1][1]);
   close(pipefds[2][1]);

   /* return filehandles for stdin and stdout */
   *fp_in = fdopen(pipefds[0][1], "a");
   *fp_out = fdopen(pipefds[1][0], "r");

   /* is stderr redirected to /dev/null? */
   if (null_stderr) {
      /* close the pipe and return nullptr as filehandle */
      close(pipefds[2][0]);
      *fp_err = nullptr;
   } else {
      /* return filehandle for stderr */
      *fp_err = fdopen(pipefds[2][0], "r");
   }

   DRETURN(pid);
}

/**
 * @brief Advanced popen() variant: run @p command under @p shell with pipes.
 *
 * Like sge_peopen() but reentrant as long as @p env is not provided, so it can
 * be used from multi-threaded processes (e.g. qmaster) in that case. Forks a
 * child that execs `shell -c command`, optionally as a login shell and/or as a
 * different user (root only), and returns the pipe streams. File descriptors
 * must be closed with sge_peclose().
 *
 * @note MT-NOTE: sge_peopen_r() is MT safe when @p env is nullptr.
 * @note DO NOT ADD ASYNC-SIGNAL-UNSAFE FUNCTIONS BETWEEN FORK AND EXEC: this is
 *       used in the multithreaded qmaster and could otherwise deadlock a thread.
 *
 * @param[in]  shell       shell to exec (argv[0] is built bounded into arg0[256])
 * @param[in]  login_shell if true, prefix argv[0] with '-' to make it a login shell
 * @param[in]  command     command string passed as `-c command`
 * @param[in]  user        user to switch to before exec (root only); may be nullptr
 * @param[in]  env         extra environment for the child; nullptr keeps it reentrant
 * @param[out] fp_in       child stdin stream
 * @param[out] fp_out      child stdout stream
 * @param[out] fp_err      child stderr stream
 * @param[in]  null_stderr if true, redirect the child's stderr to /dev/null
 * @return child process id on success, -1 on error
 *
 * @see sge_peclose(), sge_peopen()
 */
pid_t sge_peopen_r(const char *shell, int login_shell, const char *command,
                   const char *user, char **env, FILE **fp_in, FILE **fp_out,
                   FILE **fp_err, bool null_stderr) {
   pid_t pid;
   int pipefds[3][2];
   int i;
   char arg0[256];
#if defined(SOLARIS)
   char err_str[256];
#endif
   struct passwd *pw = nullptr;
   uid_t myuid;
   uid_t tuid;

   DENTER(TOP_LAYER);

   if (sge_has_admin_user()) {
      sge_switch2start_user();
   }
   myuid = geteuid();
   tuid = myuid;

   /* 
    * open pipes - close on failure 
    */
   for (i = 0; i < 3; i++) {
      if (pipe(pipefds[i]) != 0) {
         while (--i >= 0) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
         }
         ERROR(MSG_SYSTEM_FAILOPENPIPES_SS, command, strerror(errno));
         if (sge_has_admin_user()) {
            sge_switch2admin_user();
         }
         DRETURN(-1);
      }
   }

   /*
    * set arg0 for exec call correctly to that
    * either a normal shell or a login shell will be started
    */
   // bounded build of argv[0]; shell is conceptually a configurable path, so
   // never strcat it into the fixed arg0[256] (CS-2356, CWE-121). A pathological
   // shell path is truncated instead of overflowing the stack.
   snprintf(arg0, sizeof(arg0), "%s%s", login_shell ? "-" : "", shell);
   DPRINTF("arg0 = %s\n", arg0);
   DPRINTF("arg1 = -c\n");
   DPRINTF("arg2 = %s\n", command);


   /*
    * prepare the change of the user which might be done after fork()
    * if a user name is provided.
    *
    * this has to be done before the fork() afterwards it might cause
    * a deadlock of the child because getpwnam() is not async-thread safe.
    */
   if (user) {
      struct passwd pw_struct {};
      int size = get_pw_buffer_size();
      char *buffer = sge_malloc(size);

      /*
       * get information about the target user
       */
      if (buffer != nullptr) {
         pw = sge_getpwnam_r(user, &pw_struct, buffer, size);
         if (pw == nullptr) {
            ERROR(MSG_SYSTEM_NOUSERFOUND_SS, user, strerror(errno));
            sge_free(&buffer);
            if (sge_has_admin_user()) {
               sge_switch2admin_user();
            }
            DRETURN(-1);
         }
      } else {
         ERROR(SFNMAX, MSG_UTI_MEMPWNAM);
         sge_free(&buffer);
         if (sge_has_admin_user()) {
            sge_switch2admin_user();
         }
         DRETURN(-1);
      }

      DPRINTF("was able to resolve user\n");

      /* 
       * only prepare change of user if target user is different from current one
       */
      if (myuid != pw->pw_uid) {
         int res;

         if (myuid != SGE_SUPERUSER_UID) {
            DPRINTF("only root is allowed to switch to a different user\n");
            ERROR(SFNMAX, MSG_SYSTEM_NOROOTRIGHTSTOSWITCHUSER);
            sge_free(&buffer);
            DRETURN(-2);
         }

         DPRINTF("Before initgroups\n");

         res = initgroups(pw->pw_name, pw->pw_gid);
         if (res) {
            ERROR(MSG_SYSTEM_INITGROUPSFORUSERFAILED_ISS, res, user, strerror(errno));
            sge_free(&buffer);
            sge_exit(1);
         }

         DPRINTF("Initgroups was successful\n");
      }
      DPRINTF("user = %s\n", user);
      DPRINTF("myuid = %d\n", (int) myuid);
      if (pw != nullptr) {
         tuid = pw->pw_uid;
         DPRINTF("target uid = %d\n", (int) tuid);
      }

      sge_free(&buffer);
   }

   DPRINTF("Now process will fork\n");

#if defined(SOLARIS)
   pid = sge_smf_contract_fork(err_str, 256);
#else
   pid = fork();
#endif

   /* 
    * in the child pid is 0 
    */
   if (pid == 0) {
      /*
       * close all fd's except that ones mentioned in keep_open 
       */
      int keep_open[6];
      keep_open[0] = 0;
      keep_open[1] = 1;
      keep_open[2] = 2;
      keep_open[3] = pipefds[0][0];
      keep_open[4] = pipefds[1][1];
      keep_open[5] = pipefds[2][1];
      sge_close_all_fds(keep_open, 6);

      /* 
       * shall we redirect stderr to /dev/null? Then
       *    - open "/dev/null"
       *    - set stderr to "dev/null"
       *    - close the stderr-pipe
       * otherwise
       *    - redirect stderr to the pipe
       */
      if (null_stderr) {
         int fd = open("/dev/null", O_WRONLY);

         if (fd != -1) {
            close(2);
            if (dup(fd) == -1) {
               sge_exit(1);
            }
            close(pipefds[2][1]);
         } else {
            sge_exit(1);
         }
      } else {
         close(2);
         if (dup(pipefds[2][1]) == -1) {
            sge_exit(1);
         }
      }

      /* 
       * redirect stdin and stdout to the pipes 
       */
      close(0);
      close(1);
      if (dup(pipefds[0][0]) == -1 ||
          dup(pipefds[1][1]) == -1) {
         sge_exit(1);
      }

      if (pw != nullptr) {
         /* drop the primary group before the uid - initgroups() (called in the
          * parent) only sets the supplementary groups, so without this setgid()
          * the child would keep the parent's (root) gid (CS-2333). Must
          * precede setuid() while we are still privileged. */
         if (setgid(pw->pw_gid)) {
            sge_exit(1);
         }
         int lret = setuid(tuid);
         if (lret) {
            sge_exit(1);
         }
      }

      /*
       * set the environment if we got one as argument
       */
      if (env != nullptr) {
         if (pw != nullptr) {
            addenv("HOME", pw->pw_dir);
            addenv("SHELL", pw->pw_shell);
            addenv("USER", pw->pw_name);
            addenv("LOGNAME", pw->pw_name);
         }
         addenv("PATH", SGE_DEFAULT_PATH);
         for (; *env; env++) {
            putenv(*env);
         }
      }
      execlp(shell, arg0, "-c", command, nullptr);
   }

   if (pid < 0) {
      for (i = 0; i < 3; i++) {
         close(pipefds[i][0]);
         close(pipefds[i][1]);
      }
#if defined(SOLARIS)
      if (pid < -1 && err_str) {
          ERROR(MSG_SMF_FORK_FAILED_SS, "sge_peopen()", err_str);
      }
#endif
      if (sge_has_admin_user()) {
         sge_switch2admin_user();
      }
      DRETURN(-1);
   }

   /* close the childs ends of the pipes */
   close(pipefds[0][0]);
   close(pipefds[1][1]);
   close(pipefds[2][1]);

   /* return filehandles for stdin and stdout */
   *fp_in = fdopen(pipefds[0][1], "a");
   *fp_out = fdopen(pipefds[1][0], "r");

   /* is stderr redirected to /dev/null? */
   if (null_stderr) {
      /* close the pipe and return nullptr as filehandle */
      close(pipefds[2][0]);
      *fp_err = nullptr;
   } else {
      /* return filehandle for stderr */
      *fp_err = fdopen(pipefds[2][0], "r");
   }

   if (sge_has_admin_user()) {
      sge_switch2admin_user();
   }
   DRETURN(pid);
}

/****** uti/stdio/sge_peclose() ***********************************************
*  NAME
*     sge_peclose() -- pclose() call which is suitable for sge_peopen()
*
*  SYNOPSIS
*     int sge_peclose(pid_t pid, FILE *fp_in, FILE *fp_out, 
*                     FILE *fp_err, struct timeval *timeout)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     pid_t pid               - pid returned by peopen()
*     FILE *fp_in
*     FILE *fp_out
*     FILE *fp_err
*     struct timeval *timeout
*
*  RESULT
*     int - exit code of command or -1 in case of errors
*
*  SEE ALSO
*     uti/stdio/peopen()
*
*  NOTES
*     MT-NOTE: sge_peclose() is MT safe
******************************************************************************/
int sge_peclose(pid_t pid, FILE *fp_in, FILE *fp_out, FILE *fp_err,
                struct timeval *timeout) {
   int i, status;

   DENTER(TOP_LAYER);

   if (fp_in != nullptr) {
      FCLOSE(fp_in);
   }
   if (fp_out != nullptr) {
      FCLOSE(fp_out);
   }
   if (fp_err != nullptr) {
      FCLOSE(fp_err);
   }

   do {
      i = waitpid(pid, &status, timeout ? WNOHANG : 0);
      if (i == -1) {
         DRETURN(-1);
      }
      if (i == 0) { /* not yet exited */
         if (timeout->tv_sec == 0) {
            DPRINTF("killing\n");
            timeout = nullptr;
            kill(pid, SIGKILL);
         } else {
            DPRINTF("%d seconds waiting for exit\n", timeout->tv_sec);
            sleep(1);
            timeout->tv_sec -= 1;
         }
      }
   } while (i != pid);

   if (status & 0xff) {    /* terminated by signal */
      DRETURN(-1);
   }

   DRETURN((status & 0xff00) >> 8);  /* return exitcode */
   FCLOSE_ERROR:
   return -1;
}

void
print_option_syntax(FILE *fp, const char *option, const char *meaning) {
   if (!meaning)
      fprintf(fp, "   %s\n", option);
   else
      fprintf(fp, "   %-40.40s %s\n", option, meaning);
}

bool sge_check_stdout_stream(FILE *file, int fd) {
   if (fileno(file) != fd) {
      return false;
   }

   if (fprintf(file, "%s", "") < 0) {
      return false;
   }

   return true;
}

