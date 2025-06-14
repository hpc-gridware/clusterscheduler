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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <pwd.h>
#include <cerrno>

#include "uti/sge_string.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_signal.h"
#include "uti/sge_unistd.h"
#include "uti/sge_arch.h"
#include "uti/config_file.h"
#include "uti/sge_uidgid.h"

#include "setosjobid.h"

#include "msg_common.h"

#include "builtin_starter.h"
#include "err_trace.h"
#include "setrlimits.h"
#include "get_path.h"
#include "basis_types.h"
#include "execution_states.h"
#include "qlogin_starter.h"

/* The maximum number of env variables we can export. */
#define MAX_NUMBER_OF_ENV_VARS 1023

extern bool g_new_interactive_job_support;
extern int  g_noshell;
extern int  g_newpgrp;

static char* shepherd_env[MAX_NUMBER_OF_ENV_VARS + 1];
static int shepherd_env_index = -1;
static int inherit_environ = -1;

/* static functions */
static char **read_job_args(char **args, int extra_args);
static char *parse_script_params(char **script_file);
static void setup_environment ();
static bool inherit_env();
static void start_qlogin_job(const char *shell_path);
static void start_qrsh_job();
#if 0 /* Not currently used, but looks kinda useful... */
static void set_inherit_env (bool inherit);
#endif
extern int  shepherd_state;
extern char shepherd_job_dir[];
extern char **environ;

/* Copy from clients/qrsh/qrsh_starter.c 
 * Trying to include it caused dependency problems
 * TODO: Move it to a common file
 * This TODO should not be necessary any more if the command is
 * transferred in the job object.
 */
static int count_command(char *command) {
   /* count number of arguments */
   char *s = command;
   int argc;
   int i,end;
   char delimiter[2];

   snprintf(delimiter, 2, "%c", 0xff);

   /* count arguments */
   argc = 1;

   /* do not use strtok(), strtok() is seeing 2 or more delimiters as one !!! */
   end =  strlen(command);
   for(i = 0; i < end ; i++) {
      if (s[i] == delimiter[0]) {
         argc++;
      }
   } 

   return argc;
}

/************************************************************************
 This is the shepherds buitin starter.

 It is also used to start the external starter command .. 
 ************************************************************************/
void son(const char *childname, char *script_file, int truncate_stderr_out)
{
   int   in, out, err;          /* hold fds */
   int   i;
   int   merge_stderr;
   int   fs_stdin, fs_stdout, fs_stderr;
   int   min_gid, min_uid;
   int   use_login_shell;
   int   use_qsub_gid;
   int   ret;
   int   is_interactive  = 0;
   int   is_qlogin  = 0;
   int   is_rsh = 0;
   int   is_rlogin = 0;
   int   is_qlogin_starter = 0;   /* Caution! There is a function qlogin_starter(), too! */
   char  *shell_path = nullptr;
   char  *stdin_path = nullptr;
   char  *stdout_path = nullptr;
   char  *stderr_path = nullptr;
   char  *stdin_path_for_fs = nullptr;
   const char  *target_user = nullptr;
   char  *intermediate_user = nullptr;
   char  fs_stdin_tmp_path[SGE_PATH_MAX] = "\"\"";
   char  fs_stdout_tmp_path[SGE_PATH_MAX] = "\"\"";
   char  fs_stderr_tmp_path[SGE_PATH_MAX] = "\"\"";
   char  fs_stdin_path[SGE_PATH_MAX] = "\"\"";
   char  fs_stdout_path[SGE_PATH_MAX] = "\"\"";
   char  fs_stderr_path[SGE_PATH_MAX] = "\"\"";
   char  err_str[MAX_STRING_SIZE];
   const char  *shell_start_mode;
   char  *cp, *shell_basename, argv0[256];
   char  str_title[512]="";
   char  *tmp_str;
   char  *starter_method;
   int   use_starter_method;
   char  *tmpdir;
   char  *cwd;
   const char *fs_stdin_file="";
   const char *fs_stdout_file="";
   const char *fs_stderr_file="";
   pid_t pid, pgrp, newpgrp;
   gid_t add_grp_id = 0;
   gid_t gid;
   struct passwd *pw=nullptr;
   struct passwd pw_struct;
   char *buffer;
   int size;
   bool skip_silently = false;
   int pty;

   foreground = 0; /* VX sends SIGTTOU if trace messages go to foreground */

   /* From here only the son --------------------------------------*/
   if (!script_file) {
      /* output error and exit */
      shepherd_error(1, "received nullptr als script file");
   }   

   /*
   ** interactive jobs have script_file name interactive and
   ** as exec_file the configuration value for xterm
   */
   
   if (!strcasecmp(script_file, "INTERACTIVE")) {
      is_interactive = 1;

      shepherd_trace("processing interactive job");
      script_file = get_conf_val("exec_file");
   }

   /*
   ** login or rsh or rlogin jobs have the qlogin_starter as their job script
   */
   
   if( !strcasecmp(script_file, "QLOGIN") 
    || !strcasecmp(script_file, "QRSH")
    || !strcasecmp(script_file, "QRLOGIN")) {
      shepherd_trace("processing qlogin job");
      is_qlogin_starter = 1;
      is_qlogin = 1;

      if(!strcasecmp(script_file, "QRSH")) {
         is_rsh = 1;
      } else if(!strcasecmp(script_file, "QRLOGIN")) {
         is_rlogin = 1;
      }
      /* must force to run the qlogin starter as root, since it needs
         access to /dev/something */
      if (!g_new_interactive_job_support) {
         target_user = "root";
      } else { /* g_new_interactive_job_support is true */
         /*
          * Make sure target_user is nullptr and will thus be
          * set to get_conf_val("job_owner") below.
          */
         target_user = nullptr;
      }
   }

   pid = getpid();
   pgrp = GETPGRP;

#ifdef SOLARIS
   if(!is_qlogin_starter || is_rsh)
#endif
   /* 
    * g_newpgrp is != -1 if setsid() was already called in pty.c, fork_pty(),
    * in case of new IJS when a pty was created.
    * For all other jobs and cases we have to setsid() here, if it's not already set.
    */
   if (g_newpgrp == -1) {
      /* assume we are already in a new session */
      newpgrp = getpgrp();
      /* check if our assumption is true */
      if (newpgrp != getpid()) {
         /* were not in a new session - create a new one */
         newpgrp = setsid();
         if (newpgrp < 0) {
            shepherd_error(1, "setsid() failed, errno=%d, %s", errno, strerror(errno));
         }
      }
   } else {
      newpgrp = g_newpgrp;
   }

   /* run these procedures under a different user ? */
   if (!strcmp(childname, "pe_start") ||
       !strcmp(childname, "pe_stop") ||
       !strcmp(childname, "prolog") ||
       !strcmp(childname, "epilog")) {
      /* syntax is [<user>@]<path> <arguments> */
      target_user = parse_script_params(&script_file);
   }

   if (!target_user)
      target_user = get_conf_val("job_owner");
   else {
      /* 
       *  The reason for using the job owner as intermediate user
       *  is that for output of prolog/epilog the same files are
       *  used as for the job. This causes problems if the output
       *  file is created by a prolog running under a user different
       *  from the job owner, because the output files are then owned
       *  by the prolog user and then the job owner has no permissions
       *  to write into this file...
       *  
       *  We work around this problem by opening output files as 
       *  job owner and then changing to the prolog user. 
       *
       *  Additionally it prevents that a root procedures write to
       *  files which may not be accessable by the job owner 
       *  (e.g. /etc/passwd)
       *
       *  This workaround doesn't work for Interix - we have to find
       *  another solution here!
       */
      intermediate_user = get_conf_val("job_owner");
   }

   shepherd_trace("pid=" pid_t_fmt " pgrp=" pid_t_fmt " sid=" pid_t_fmt " old pgrp="
                  pid_t_fmt " getlogin()=%s", pid, newpgrp, newpgrp, pgrp,
                  (cp = getlogin()) ? cp : "<no login set>");

   shepherd_trace("reading passwd information for user '%s'",
                  target_user ? target_user : "<nullptr>");

   size = get_pw_buffer_size();
   buffer = sge_malloc(size);
   pw = sge_getpwnam_r(target_user, &pw_struct, buffer, size);
   if (!pw) {
      shepherd_error(1, "can't get password entry for user \"%s\"", target_user);
   }

   umask(022);

   if (!strcmp(childname, "job")) {
      char *write_osjob_id = get_conf_val("write_osjob_id");
      if(write_osjob_id != nullptr && atoi(write_osjob_id) != 0) {
         setosjobid(newpgrp, &add_grp_id, pw);
      }   
   }
   
   shepherd_trace("setting limits");
   setrlimits(!strcmp(childname, "job"));

   shepherd_trace("setting environment");
   sge_set_environment();

	/* Create the "error" and the "exit" status file here.
	 * The "exit_status" file indicates that the son is started.
	 *
	 * We are here (normally) uid=root, euid=admin_user, but we give the
	 * file ownership to the job owner immediately after opening the file, 
	 * so the job owner can reopen the file if the exec() fails.
	 */
   shepherd_trace("Initializing error file");
   shepherd_error_init( );

   min_gid = atoi(get_conf_val("min_gid"));
   min_uid = atoi(get_conf_val("min_uid"));

   /** 
    ** Set uid and gid and
    ** (add additional group id), switches to start user (root) 
    **/
   tmp_str = search_conf_val("qsub_gid");
   if (strcmp(tmp_str, "no") != 0) {
      use_qsub_gid = 1;   
      gid = atol(tmp_str);
   } else {
      use_qsub_gid = 0;
      gid = 0;
   }
   tmp_str = search_conf_val("skip_ngroups_max_silently");
   if (tmp_str != nullptr && strcmp(tmp_str, "yes") == 0) {
      skip_silently = true;
   }

   /* --- switch to intermediate user */
   shepherd_trace("switching to intermediate/target user");
   if(is_qlogin_starter && !g_new_interactive_job_support) {
      /* 
       * In the old IJS, we didn't have to set the additional group id,
       * because our custom rshd did it for us.
       * The add_grp_id is necessary to obtain online usage and to
       * kill subprocesses.
       */
      ret = sge_set_uid_gid_addgrp(target_user, intermediate_user,
               0, 0, 0, err_str, sizeof(err_str), use_qsub_gid, gid, skip_silently);
   } else { /* if (!is_qlogin_starter || g_new_interactive_job_support is true) */
      /*
       * In not-interactive jobs and in the new IJS we must set the 
       * additional group id here, because there is no custum rshd that will
       * do this for us.
       */
      ret = sge_set_uid_gid_addgrp(target_user, intermediate_user,
               min_gid, min_uid, add_grp_id, err_str, sizeof(err_str), use_qsub_gid, gid, skip_silently);
   }

   if (ret < 0) {
      shepherd_trace(err_str);
      shepherd_trace("try running further with uid=%d", (int)getuid());
   } else if (ret > 0) {
      if (ret == 5) {
         shepherd_state = SSTATE_ADD_GRP_SET_ERROR;
      }

      // violation of min_gid or min_uid
      shepherd_error(1, err_str);
   }

   shell_start_mode = get_conf_val("shell_start_mode");

   shepherd_trace("closing all filedescriptors");
   shepherd_trace("further messages are in \"error\" and \"trace\"");
   fflush(stdin);
   fflush(stdout);
   fflush(stderr);

   {
		/* Close all file descriptors except the ones used by tracing
		 * (for the files "trace", "error" and "exit_status") and,
       * in interactive job case, the ones used to redirect stdin,
       * stdout and stderr.
		 * These files will be closed automatically in the exec() call
		 * due to the FD_CLOEXEC flag.
		 */
      int fdmax = sysconf(_SC_OPEN_MAX);
      pty = atoi(get_conf_val("pty"));

      /* For batch jobs with not pty, also close stdin, stdout and stderr.
       * For new interactive jobs or batch jobs with pty, keep stdin, 
       * stdout and stderr open, they are already connected to the pty 
       * and/or the pipes.
       */
      if ((g_new_interactive_job_support && is_qlogin_starter)
            || (!g_new_interactive_job_support && pty == 1)) {
         i=3;
      } else {
         i=0;
      }
      for ( ; i < fdmax; i++) {
         if (!is_shepherd_trace_fd(i)) {
         	SGE_CLOSE(i);
         }
      }
   }
   foreground = 0;

   /* We have different possiblities to start the job script:
    * - We can start it as login shell or not
    *   Login shell start means that argv[0] starts with a '-'
    * - We can try to be posix compliant and not to evaluate #!/bin/sh
    *   as it is done by normal shellscript starts
    * - We can feed the shellscript to stdin of a started shell
    */

   tmpdir = get_conf_val("tmpdir");

   merge_stderr = atoi(get_conf_val("merge_stderr"));
   fs_stdin  = atoi(get_conf_val("fs_stdin_file_staging"));
   fs_stdout = atoi(get_conf_val("fs_stdout_file_staging"));
   fs_stderr = atoi(get_conf_val("fs_stderr_file_staging"));
   
   if (strcmp(childname, "pe_start") == 0
       || strcmp(childname, "pe_stop") == 0
       || strcmp(childname, "prolog") == 0
       || strcmp(childname, "epilog") == 0) {

      /* use login shell */
      use_login_shell = 1; 

      /* take shell from passwd */
      shell_path = strdup(pw->pw_shell);
      shepherd_trace("using \"%s\" as shell of user \"%s\"", pw->pw_shell, target_user);
      
      /* unix_behaviour */
      shell_start_mode = "unix_behaviour";

      if (!strcmp(childname, "prolog") || !strcmp(childname, "epilog")) {
         stdin_path = strdup("/dev/null");
         if (fs_stdin) {
            /* we need the jobs stdin_path here in prolog,
               because the file must be copied */
            stdin_path_for_fs = build_path(SGE_STDIN);
         }
         stdout_path = build_path(SGE_STDOUT);
         if (!merge_stderr) {
            stderr_path = build_path(SGE_STDERR);
         }
      } else {
         /* childname is "pe_start" or "pe_stop" */
         stdin_path = strdup("/dev/null");
         stdin_path_for_fs = build_path(SGE_STDIN);

         stdout_path  = build_path(SGE_PAR_STDOUT);
         if (!merge_stderr) {
            stderr_path  = build_path(SGE_PAR_STDERR);
         }
      }
   } else { /* the job itself */
      if (!is_rsh && g_new_interactive_job_support) {
         shell_path = strdup(pw->pw_shell);
         sge_chdir(pw->pw_dir);
      } else {
         shell_path = get_conf_val("shell_path");
      }
      use_login_shell = atoi(get_conf_val("use_login_shell"));

      stdin_path = build_path(SGE_STDIN);
      stdin_path_for_fs = strdup(stdin_path);
      stdout_path = build_path(SGE_STDOUT);
      if (!merge_stderr) {
         stderr_path = build_path(SGE_STDERR);
      }
   }

   if (fs_stdin) {
      if (stdin_path_for_fs && strlen(stdin_path_for_fs) > 0) {
         /* Generate fs_input_tmp_path (etc.) from tmpdir+stdin_path */
         fs_stdin_file = strrchr(stdin_path_for_fs, '/') + 1;
         snprintf(fs_stdin_tmp_path, sizeof(fs_stdin_tmp_path), "%s/%s", tmpdir, fs_stdin_file);

         /* Set fs_input_path to old stdin_path and then
            stdin_path to just generated fs_input_tmp_path */
         strcpy(fs_stdin_path, stdin_path_for_fs);
      }

      /* Modify stdin_path, stdout_path and stderr_path only if
         file staging is wanted! */
      /* prolog, pe_start, epilog, pe_stop always get /dev/null as stdin_path,
       only the job gets the file staged stdin file */
      if (strcmp(childname, "prolog") &&
          strcmp(childname, "pe_start") &&
          strcmp(childname, "epilog") &&
          strcmp(childname, "pe_stop")) {
         stdin_path = (char *)sge_realloc(stdin_path, strlen(fs_stdin_tmp_path) + 1, 1);
         strcpy(stdin_path, fs_stdin_tmp_path);
      }
   }

   if (fs_stdout) {
      if (stdout_path && strlen(stdout_path) > 0) {
         fs_stdout_file = strrchr(stdout_path, '/') + 1;
         snprintf(fs_stdout_tmp_path, sizeof(fs_stdout_tmp_path), "%s/%s", tmpdir, fs_stdout_file);
      }

      if (stdout_path && strlen(stdout_path) > 0) {
         strcpy(fs_stdout_path, stdout_path);
      }

      /* prolog, pe_start, job, epilog and pe_stop get the
         file staged output (and error) file */
      stdout_path = (char *)sge_realloc(stdout_path, strlen(fs_stdout_tmp_path) + 1, 1);
      strcpy(stdout_path, fs_stdout_tmp_path);
   }

   if (fs_stderr) {
      if (stderr_path && strlen(stderr_path) > 0) {
         fs_stderr_file = strrchr( stderr_path, '/') + 1;
         snprintf(fs_stderr_tmp_path, sizeof(fs_stderr_tmp_path), "%s/%s", tmpdir, fs_stderr_file);
      }

      if (stderr_path && strlen(stderr_path) > 0) {
         strcpy(fs_stderr_path, stderr_path);
      }
  
      if (!merge_stderr) {
         stderr_path = (char *)sge_realloc(stderr_path, strlen(fs_stderr_tmp_path) + 1, 1);
         strcpy( stderr_path, fs_stderr_tmp_path );
      }
   }

   /* Now we have all paths generated, so we must
      write it to the config struct */
   set_conf_val("stdin_path", stdin_path);
   set_conf_val("stdout_path", stdout_path);
   set_conf_val("stderr_path", stderr_path);
   set_conf_val("fs_stdin_path", fs_stdin_path);
   set_conf_val("fs_stdout_path", fs_stdout_path);
   set_conf_val("fs_stderr_path", fs_stderr_path);
   set_conf_val("fs_stdin_tmp_path", fs_stdin_tmp_path);
   set_conf_val("fs_stdout_tmp_path", fs_stdout_tmp_path);
   set_conf_val("fs_stderr_tmp_path", fs_stderr_tmp_path);
  
#if 0
   /* <DEBUGGING> */
   shepherd_trace("## stdin_path=%s", get_conf_val("stdin_path"));
   shepherd_trace("## stdout_path=%s", get_conf_val("stdout_path"));
   if (!merge_stderr) {
      shepherd_trace("## stderr_path=%s", get_conf_val("stderr_path"));
   }      
   shepherd_trace("## fs_stdin_path=%s", get_conf_val("fs_stdin_path"));
   shepherd_trace("## fs_stdout_path=%s", get_conf_val("fs_stdout_path"));
   shepherd_trace("## fs_stderr_path=%s", get_conf_val("fs_stderr_path"));
   shepherd_trace("## fs_stdin_host=%s", get_conf_val("fs_stdin_host"));
   shepherd_trace("## fs_stdout_host=%s", get_conf_val("fs_stdout_host"));
   shepherd_trace("## fs_stderr_host=%s", get_conf_val("fs_stderr_host"));
   shepherd_trace("## fs_stdin_tmp_path=%s", get_conf_val("fs_stdin_tmp_path"));
   shepherd_trace("## fs_stdout_tmp_path=%s", get_conf_val("fs_stdout_tmp_path"));
   shepherd_trace("## fs_stderr_tmp_path=%s", get_conf_val("fs_stderr_tmp_path"));
   shepherd_trace("## merge_stderr=%s", get_conf_val("merge_stderr"));
   shepherd_trace("## fs_stdin_file_staging=%s", get_conf_val("fs_stdin_file_staging"));
   shepherd_trace("## fs_stdout_file_staging=%s", get_conf_val("fs_stdout_file_staging"));
   shepherd_trace("## fs_stderr_file_staging=%s", get_conf_val("fs_stderr_file_staging"));
   /* </DEBUGGING> */
#endif

   /* Now we have to generate the command line for prolog, pe_start, epilog
    * or pe_stop from the config values
    */
   if (!strcmp(childname, "prolog")) {
      replace_params( get_conf_val("prolog"),
         script_file, sizeof(script_file)-1, prolog_epilog_variables );
      target_user = parse_script_params(&script_file);
   } else if (!strcmp(childname, "epilog")) {
      replace_params( get_conf_val("epilog"),
         script_file, sizeof(script_file)-1, prolog_epilog_variables );
      target_user = parse_script_params(&script_file);
   } else if (!strcmp(childname, "pe_start")) {
      replace_params( get_conf_val("pe_start"),
         script_file, sizeof(script_file)-1, pe_variables );
      target_user = parse_script_params(&script_file);
   } else if (!strcmp(childname, "pe_stop")) {
      replace_params( get_conf_val("pe_stop"),
         script_file, sizeof(script_file)-1, pe_variables );
      target_user = parse_script_params(&script_file);
   }

   cwd = get_conf_val("cwd");

   sge_set_env_value("SGE_STDIN_PATH", stdin_path);
   sge_set_env_value("SGE_STDOUT_PATH", stdout_path);
   sge_set_env_value("SGE_STDERR_PATH", merge_stderr?stdout_path:stderr_path);
   sge_set_env_value("SGE_CWD_PATH", cwd);

   /*
   ** for interactive jobs, we disregard the current shell_start_mode
   */
   if (is_interactive || is_qlogin)
      shell_start_mode = "unix_behaviour";

   in = 0;
   if (!strcasecmp(shell_start_mode, "script_from_stdin")) {
      in = SGE_OPEN2(script_file, O_RDONLY);
      if (in == -1) {
         shepherd_error(1, "error: can't open %s script file \"%s\": %s", 
                        childname, script_file, strerror(errno));
      }
   } else {
      /* 
       * Opening a stdin file doesnt make sense for any interactive 
       * job (except qsh) and qsub -pty 
       */
      if (!is_qlogin_starter && pty == 0) {
         /* need to open a file as fd0 for qsub jobs */
         in = SGE_OPEN2(stdin_path, O_RDONLY); 

         if (in == -1) {
            shepherd_state = SSTATE_OPEN_OUTPUT;
            shepherd_error(1, "error: can't open %s as dummy input file", 
                           stdin_path);
         }
      }
   }

   if (in != 0) {
      shepherd_error(1, "error: fd for in is not 0");
   }

   if(!is_qlogin_starter) {
      /* -cwd or from pw->pw_dir */
      if (sge_chdir(cwd)) {
         shepherd_state = SSTATE_NO_CWD;
         shepherd_error(1, "error: can't chdir to %s: %s", cwd, strerror(errno));
      }
   }
   /* open stdout - not for interactive jobs */
   if (!is_interactive && !is_qlogin) {
      /* open stdout - not for qsub -pty yes */
      if (pty == 0) {
         if (truncate_stderr_out) {
            out = SGE_OPEN3(stdout_path, O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, 0644);
         } else {
            out = SGE_OPEN3(stdout_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
         }
         
         if (out==-1) {
            shepherd_state = SSTATE_OPEN_OUTPUT;
            shepherd_error(1, "error: can't open output file \"%s\": %s", 
                           stdout_path, strerror(errno));
         }
         
         if (out!=1) {
            shepherd_error(1, "error: fd out is not 1");
         }   

         /* open stderr */
         if (merge_stderr) {
            shepherd_trace("using stdout as stderr");
            dup2(1, 2);
         } else {
            if (truncate_stderr_out) {
               err = SGE_OPEN3(stderr_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
            } else {
               err = SGE_OPEN3(stderr_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            }

            if (err == -1) {
               shepherd_state = SSTATE_OPEN_OUTPUT;
               shepherd_error(1, "error: can't open output file \"%s\": %s", 
                              stderr_path, strerror(errno));
            }

#ifndef __INSURE__
            if (err!=2) {
               shepherd_trace("unexpected fd");
               shepherd_error(1, "error: fd err is not 2");
            }
#endif
         }
      }
   }

   /*
    * Use starter_method if it is supplied
    */

   if (!is_interactive && !is_qlogin && !is_qlogin_starter &&
       !strcmp(childname, "job") &&
       (starter_method = get_conf_val("starter_method")) &&
       strcasecmp(starter_method, "none")) {

      sge_set_env_value("SGE_STARTER_SHELL_PATH", shell_path);
      shell_path = starter_method;
      use_starter_method = 1;
      sge_set_env_value("SGE_STARTER_SHELL_START_MODE", shell_start_mode);
      if (!strcasecmp("unix_behavior", shell_start_mode))
         shell_start_mode = "posix_compliant";
      if (use_login_shell)
         sge_set_env_value("SGE_STARTER_USE_LOGIN_SHELL", "true");
    } else {
       use_starter_method = 0;
    }      

   /* get basename of shell for building argv[0] */
   cp = strrchr(shell_path, '/');
   if (!cp)
      shell_basename = shell_path;
   else
      shell_basename = cp+1;

   {
      SGE_STRUCT_STAT sbuf;

      if ((!strcasecmp("script_from_stdin", shell_start_mode) ||
           !strcasecmp("posix_compliant", shell_start_mode) ||
           !strcasecmp("start_as_command", shell_start_mode)) && 
           !is_interactive && !is_qlogin) { 
         if (SGE_STAT(shell_path, &sbuf)) {
            shepherd_state = SSTATE_NO_SHELL;
            shepherd_error(1, "unable to find shell \"%s\"", shell_path);
         }
      }
   }
   if (use_login_shell) {
      strcpy(argv0, "-");
      strcat(argv0, shell_basename);
   } else {
      strcpy(argv0, shell_basename);
   }

   sge_set_def_sig_mask(nullptr, nullptr);
   sge_unblock_all_signals();
   
   /*
   ** prepare xterm title for interactive jobs
   */
   if (is_interactive) {
      char *queue;
      char *host;
      char *job_id;

      queue = get_conf_val("queue");
      host = get_conf_val("host");
      job_id = get_conf_val("job_id");
      snprintf(str_title, sizeof(str_title), "SGE Interactive Job %s on %s in Queue %s", job_id, host, queue);
   }

/* ---- switch to target user */
   if (intermediate_user) {
      if (is_qlogin_starter) {
         ret = sge_set_uid_gid_addgrp(target_user, nullptr, 0, 0, 0,
                                      err_str, sizeof(err_str), use_qsub_gid, gid, skip_silently);
      } else {
         ret = sge_set_uid_gid_addgrp(target_user, nullptr, min_gid, min_uid,
                                      add_grp_id, err_str, sizeof(err_str), use_qsub_gid, gid, skip_silently);
      }
      if (ret < 0) {
         shepherd_trace(err_str);
         shepherd_trace("try running further with uid=%d", (int)getuid());
      } else if (ret > 0) {
         shepherd_error(1, err_str);
      }
   }
   shepherd_trace("now running with uid=" uid_t_fmt ", euid=" uid_t_fmt,
                  (int)getuid(), (int)geteuid());

   /*
   ** if we dont check if the script_file exists, then in case of
   ** "start_as_command" the result is an error report
   ** saying that the script exited with 1
   */
   if (!is_qlogin && !atoi(get_conf_val("handle_as_binary"))) {
      if (strcasecmp(shell_start_mode, "raw_exec")) {
         SGE_STRUCT_STAT sbuf;
         char file[SGE_PATH_MAX+1];
         char *pc;
   
         sge_strlcpy(file, script_file, SGE_PATH_MAX);
         pc = strchr(file, ' ');
         if (pc) {
            *pc = 0;
         }
  
         if (SGE_STAT(file, &sbuf)) {
            /*
            ** generate a friendly error message especially for interactive jobs
            */
            if (is_interactive) {
               shepherd_error(1, "unable to find xterm executable \"%s\" for "
                              "interactive job", file);
            } else {
               shepherd_error(1, "unable to find %s file \"%s\"", childname, file);
            }
         }

         if (!(sbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            shepherd_error(1, "%s file \"%s\" is not executable", childname, file);
         }
      }
   }
   start_command(childname, shell_path, script_file, argv0, shell_start_mode, 
                 is_interactive, is_qlogin, is_rsh, is_rlogin, str_title, 
                 use_starter_method);

   sge_free(&buffer);
   return;
}

/****** Shepherd/sge_set_environment() *****************************************
*  NAME
*     sge_set_environment () -- Read the environment from the "environment" file
*     and store it in the appropriate environment, inherited or internal.
*
*  SYNOPSIS
*      int sge_set_environment()
*
*  FUNCTION
*     This function reads the "environment" file written out by the execd and
*     stores each environment variable entry in the appropriate environment,
*     either internal or inherited.
*
*  RESULTS
*     int - error code: 0: good, 1: bad
*
*  NOTES
*      MT-NOTE: sge_set_environment() is not MT safe
*******************************************************************************/
int sge_set_environment()
{
   const char *const filename = "environment";
   FILE *fp;
   char buf[10000], *name, *value, err_str[10000];
   int line=0;
   const char *new_value = nullptr;

   setup_environment();
   
   if (!(fp = fopen(filename, "r"))) {
      shepherd_error(1, "can't open environment file: %s", strerror(errno));
   }

   while (fgets(buf, sizeof(buf), fp)) {

      line++;

      if (strlen(buf) <= 1) {   /* empty line or lastline */
         continue;
      }

      name = strtok(buf, "=");
      if (name == nullptr) {
         FCLOSE(fp);
         shepherd_error(1, "error reading environment file: line=%d, contents:%s", line, buf);
      }

      value = strtok(nullptr, "\n");
      if (value == nullptr) {
         value = (char *)"";
      }

      new_value = sge_replace_substring(value, "\\n", "\n");
      if (new_value == nullptr) {
         sge_set_env_value(name, value);
      } else {
         sge_set_env_value(name, new_value);
         sge_free(&new_value);
      }
   }

   FCLOSE(fp);
   return 0;
FCLOSE_ERROR:
   snprintf(err_str, sizeof(err_str), MSG_FILE_ERRORCLOSEINGXY_SS, filename, strerror(errno));
   return 1;
}

/****** Shepherd/setup_environment() *******************************************
*  NAME
*     setup_environment () -- Set up the internal environment
*
*  SYNOPSIS
*     void setup_environment()
*
*  FUNCTION
*     This function initializes the variables used to store the internal
*     environment.
*
*  NOTES
*      MT-NOTE: setup_environment() is not MT safe
*******************************************************************************/
static void setup_environment()
{
   /* Bugfix: Issuezilla 1300
    * Because this fix could break pre-existing installations, it was made
    * optional. */
   if (!inherit_env()) {
      if (shepherd_env_index < 0) {
         shepherd_env[0] = nullptr;
      } else {
         int index = 0;

         while (shepherd_env[index] != nullptr) {
            sge_free(&(shepherd_env[index]));
            shepherd_env[index] = nullptr;
            index++;
         }
      }
      
      shepherd_env_index = 0;
   }
}

/****** Shepherd/sge_get_environment() *****************************************
*  NAME
*     sge_get_environment () -- Get a pointer to the current environment
*
*  SYNOPSIS
*     char **sge_get_environment()
*
*  FUNCTION
*     This function returns a point to the current environment, inherited or
*     internal.
*
*  RESULTS
*     char ** - pointer to the current environment
*
*  NOTES
*      MT-NOTE: sge_get_environment() is not MT safe
*******************************************************************************/
char** sge_get_environment()
{
   /* Bugfix: Issuezilla 1300
    * Because this fix could break pre-existing installations, it was made
    * optional. */
   if (!inherit_env()) {
      return shepherd_env;
   } else {
      return environ;
   }
}

/****** Shepherd/sge_set_env_value() *******************************************
*  NAME
*     sge_set_env_value () -- Set the value for the given environment variable
*
*  SYNOPSIS
*     const int sge_set_env_value(const char *name, const char *value)
*
*  FUNCTION
*     This function sets the value of the given environment variable in
*     the appropriate environment, inherited or internal.
*
*  INPUT
*     const char *name - the name of the environment variable
*     const char *value - the value of the environment variable
*
*  RESULTS
*     int - error code: -1: bad, 0: good
*
*  NOTES
*      MT-NOTE: sge_set_env_value() is not MT safe
*******************************************************************************/
int sge_set_env_value(const char *name, const char* value)
{
   int ret = -1;
   
   /* Bugfix: Issuezilla 1300
    * Because this fix could break pre-existing installations, it was made
    * optional. */
   if (!inherit_env()) {
      char *entry = nullptr;
      int entry_size = 0;

      if (shepherd_env_index < 0) {
         setup_environment();
      } else if (shepherd_env_index < MAX_NUMBER_OF_ENV_VARS) {
         /* entry = name + value + '=' + '\0' */
         entry_size = strlen(name) + strlen(value) + 2;
         entry = sge_malloc(sizeof (char) * entry_size);

         if (entry != nullptr) {
            snprintf(entry, entry_size, "%s=%s", name, value);
            shepherd_env[shepherd_env_index] = entry;
            shepherd_env_index++;
            ret = 0;
         }
      }
   } else {
      ret = sge_setenv(name, value);
   }
   
   return ret;
}

/****** Shepherd/sge_get_env_value() *******************************************
*  NAME
*     sge_get_env_value () -- Get the value for the given environment variable
*
*  SYNOPSIS
*     const char *sge_get_env_value(const char *name)
*
*  FUNCTION
*     This function returns the value of the given environment variable from
*     the appropriate environment, inherited or internal.
*
*  INPUT
*     const char *name - the name of the environment variable
* 
*  RESULTS
*     const char * - the value of the environment variable
*
*  NOTES
*      MT-NOTE: sge_get_env_value() is not MT safe
*******************************************************************************/
const char *sge_get_env_value(const char *name)
{
   const char *ret = nullptr;
   /* Bugfix: Issuezilla 1300
    * Because this fix could break pre-existing installations, it was made
    * optional. */
   if (!inherit_env()) {
      if (shepherd_env_index >= 0) {
         int index = 0;

         while (shepherd_env[index] != nullptr) {
            char *entry = shepherd_env[index++];
            char *equals = strchr(entry, '=');

            if (equals != nullptr) {
               equals[0] = '\0';

               if (strcmp(entry, name) == 0) {
                  equals[0] = '=';
                  ret = (const char*)&equals[1];
                  break;
               }
            }
         }
      }
   } else {
      ret = sge_getenv (name);
   }
   
   return ret;
}

#define PROC_ARG_DELIM " \t"
static char **disassemble_proc_args(const char *script_file, char **preargs, int extra_args)
{
   char *s;
   unsigned long n_preargs = 0, n_procargs = 0, i;
   char **args, **pstr;

   /* count preargs */
   for (pstr = preargs; pstr && *pstr; pstr++)
      n_preargs++;

   /* count procedure args */
   for (s = sge_strtok(script_file, PROC_ARG_DELIM); s; s = sge_strtok(nullptr, PROC_ARG_DELIM))
      n_procargs++;

   if (!(n_preargs + n_procargs))
      return nullptr;

   /* malloc new argv */
   args = (char **)sge_malloc((n_preargs + n_procargs + extra_args + 1)*sizeof(char *));
   if (!args)
      return nullptr;

   /* copy preargs */
   for (i = 0; i < n_preargs; i++)
      args[i] = strdup(preargs[i]);
   args[i] = nullptr;

   /* add procedure args */
   for (s = sge_strtok(script_file, PROC_ARG_DELIM); s; s = sge_strtok(nullptr, PROC_ARG_DELIM)) {
      args[i++] = strdup(s);
   }
   args[i] = nullptr;
   return args;
}


static char **read_job_args(char **preargs, int extra_args)
{
   int n_preargs = 0;
   int n_job_args = 0;
   int i;
   char **pstr;
   char **args;
   char conf_val[256];
   char *cp;

   /* count preargs */
   for (pstr = preargs; pstr && *pstr; pstr++)
      n_preargs++;
  
   /* get number of job args */ 
   n_job_args = atoi(get_conf_val("njob_args"));

   if (!(n_preargs + n_job_args))
      return nullptr;
  
   /* malloc new argv */ 
   args = (char **)sge_malloc((n_preargs + n_job_args + extra_args + 1)*sizeof(char *));
   if (!args)
      return nullptr;
  
   /* copy preargs */ 
   for (i = 0; i < n_preargs; i++) {
      args[i] = strdup(preargs[i]);
   }

   args[i] = nullptr;

   for (i = 0; i < n_job_args; i++) {
      snprintf(conf_val, sizeof(conf_val), "job_arg%d", i + 1);
      cp = get_conf_val(conf_val);
     
      if(cp != nullptr) {
         args[i + n_preargs] = strdup(cp);
      } else {
         args[i + n_preargs] = (char *)"";
      }
   }
   args[i + n_preargs] = nullptr;
   return args;
}


/*--------------------------------------------------------------------
 * set_shepherd_signal_mask
 * set signal mask that shepherd can handle signals from execd
 *--------------------------------------------------------------------*/
void start_command(
const char *childname,
char *shell_path,
char *script_file,
char *argv0,
const char *shell_start_mode,
int is_interactive,
int is_qlogin,
int is_rsh,
int is_rlogin,
const char *str_title,
int use_starter_method /* If this flag is set the shellpath contains the
                        * starter_method */
) {
   char **args;
   char **pstr;
   char *pc;
   char *pre_args[10];
   char **pre_args_ptr;
   char err_str[2048];

   pre_args_ptr = &pre_args[0];
   
#if 0
   shepherd_trace("childname = %s", childname? childname : "nullptr");
   shepherd_trace("shell_path = %s", shell_path ? shell_path : "nullptr");
   shepherd_trace("script_file = %s", script_file ? script_file : "nullptr");
   shepherd_trace("argv0 = %s", argv0 ? argv0 : "nullptr");
   shepherd_trace("shell_start_mode = %s", shell_start_mode ? shell_start_mode : "nullptr");
   shepherd_trace("is_interactive = %d", is_interactive);
   shepherd_trace("is_qlogin = %d", is_qlogin);
   shepherd_trace("is_rsh = %d", is_rsh);
   shepherd_trace("is_rlogin = %d", is_rlogin);
   shepherd_trace("str_title = %s", str_title ? str_title : "nullptr");
#endif

   /*
   ** prepare job arguments
   */
   if ((atoi(get_conf_val("handle_as_binary")) == 1) &&
       (atoi(get_conf_val("no_shell")) == 0) &&
       !is_rsh && !is_qlogin && !strcmp(childname, "job") && use_starter_method != 1 ) {
      int arg_id = 0;
      dstring arguments = DSTRING_INIT;
      int n_job_args;
      int i;
      char *cp;

#if 0
      shepherd_trace("Case 1: handle_as_binary, shell, no rsh, no qlogin, starter_method=none");
#endif

      pre_args_ptr[arg_id++] = argv0;
      n_job_args = atoi(get_conf_val("njob_args"));
      pre_args_ptr[arg_id++] = (char*)"-c";
      sge_dstring_append(&arguments, script_file);
     
      sge_dstring_append(&arguments, " ");
      for (i=0; i<n_job_args; i++) {
         char conf_val[256];

         snprintf(conf_val, sizeof(conf_val), "job_arg%d", i + 1);
         cp = get_conf_val(conf_val);

         if(cp != nullptr) {
            sge_dstring_append(&arguments, cp);
            sge_dstring_append(&arguments, " ");
         } else {
            sge_dstring_append(&arguments, "\"\"");
         }
      }
      
      pre_args_ptr[arg_id++] = strdup(sge_dstring_get_string(&arguments));
      pre_args_ptr[arg_id++] = nullptr;
      sge_dstring_free(&arguments);
      args = pre_args;
   /* No need to test for binary since this option excludes binary */
   } else if (!strcasecmp("script_from_stdin", shell_start_mode)) {

#if 0
      shepherd_trace("Case 2: script_from_stdin");
#endif

      /*
      ** -s makes it possible to make the shell read from stdin
      ** and yet have job arguments
      ** problem: if arguments are options that shell knows it
      ** will interpret and eat them
      */
      pre_args_ptr[0] = argv0;
      pre_args_ptr[1] = (char *)"-s";
      pre_args_ptr[2] = nullptr;
      args = read_job_args(pre_args, 0);
   /* Binary, noshell jobs have to make it to the else */
   } else if ( (!strcasecmp("posix_compliant", shell_start_mode) &&
              (atoi(get_conf_val("handle_as_binary")) == 0)) || (use_starter_method == 1)) {
                 
#if 0
      shepherd_trace("Case 3: posix_compliant, no binary or starter_method!=none" );
#endif

      pre_args_ptr[0] = argv0;
      pre_args_ptr[1] = script_file;
      pre_args_ptr[2] = nullptr;
      args = read_job_args(pre_args, 0);
   /* No need to test for binary since this option excludes binary */
   } else if (!strcasecmp("start_as_command", shell_start_mode)) {
      
#if 0
      shepherd_trace("Case 4: start_as_command" );
#endif

      pre_args_ptr[0] = argv0;
      shepherd_trace("start_as_command: pre_args_ptr[0] = argv0; \"%s\""
                     " shell_path = \"%s\"", argv0, shell_path); 
      pre_args_ptr[1] = (char *)"-c";
      pre_args_ptr[2] = script_file;
      pre_args_ptr[3] = nullptr;
      args = pre_args;
   /* No need to test for binary since this option excludes binary */
   } else if (is_interactive) {
      int njob_args;
      
#if 0
      shepherd_trace("Case 5: interactive");
#endif

      pre_args_ptr[0] = script_file;
      pre_args_ptr[1] = (char *)"-display";
      pre_args_ptr[2] = get_conf_val("display");
      pre_args_ptr[3] = (char *)"-n";
      pre_args_ptr[4] = (char *)str_title;
      pre_args_ptr[5] = nullptr;
      args = read_job_args(pre_args, 2);
      njob_args = atoi(get_conf_val("njob_args"));
      args[njob_args + 5] = (char *)"-e";
      args[njob_args + 6] = get_conf_val("shell_path");
      args[njob_args + 7] = nullptr;
   /* No need to test for binary since qlogin handles that itself */
   } else if (is_qlogin) {
      const char *conf_name = nullptr;
#if 0
     shepherd_trace("Case 6: qlogin");
#endif
      pre_args_ptr[0] = script_file;
      if (is_rsh) {
         conf_name = "rsh_daemon";
      } else {
         if (is_rlogin) {
            conf_name = "rlogin_daemon";
         } else {
            conf_name = "qlogin_daemon";
         }
      }
      pre_args_ptr[1] = get_conf_val(conf_name);

      if (check_configured_method(pre_args_ptr[1], conf_name, err_str, sizeof(err_str)) != 0
          && !g_new_interactive_job_support) {
         shepherd_state = SSTATE_CHECK_DAEMON_CONFIG;
         shepherd_error(1, err_str);
      }

      pre_args_ptr[2] = (char *)"-d";
      pre_args_ptr[3] = nullptr;
      args = read_job_args(pre_args, 0);
   /* Here we finally deal with binary, noshell jobs */
   } else {
      
#if 0
     shepherd_trace("Case 7: unix_behaviour/raw_exec" );
#endif
      /*
      ** unix_behaviour/raw_exec
      */
      if (!strcmp(childname, "job")) {
         
         int arg_id = 0;
#if 0
         shepherd_trace("Case 7.1: job" );
#endif
         if( use_starter_method ) {
           pre_args_ptr[arg_id++] = shell_path;
         }
         pre_args_ptr[arg_id++] = script_file;
         pre_args_ptr[arg_id++] = nullptr;
         
         args = read_job_args(pre_args, 0);
      } else {
#if 0
         shepherd_trace("Case 7.2: no job" );
#endif
         
         /* the script file also contains procedure arguments
            need to disassemble the string and put into args vector */
         if( use_starter_method ) {
            pre_args_ptr[0] = shell_path;
            pre_args_ptr[1] = nullptr;
         } else {
            pre_args_ptr[0] = nullptr;
         }
         args = disassemble_proc_args(script_file, pre_args, 0);
         
         script_file = args[0];
      }
   }

   if (is_qlogin) { /* qlogin, qrsh (without command), qrsh <command> */
      if (!g_new_interactive_job_support) {
         shepherd_trace("start qlogin");
         
         /* build trace string */
         shepherd_trace("calling qlogin_starter(%s, %s);", shepherd_job_dir, args[1]);
#if defined (SOLARIS)
         if (is_rlogin) {
            if (strstr(args[1], "sshd") != nullptr) {
               /* workaround for CR 6215730 */ 
               shepherd_trace("staring an sshd on SOLARIS, do a SETPGRP to be "
                              "able to kill it (qdel)");
               SETPGRP;
            }
         }
#endif
         qlogin_starter(shepherd_job_dir, args[1], sge_get_environment());
      } else { /* g_new_interactive_job_support is true */
         if (!is_rsh) { /* qlogin, qrsh (without command) */
            start_qlogin_job(shell_path);
         } else { /* qrsh <command> */
            start_qrsh_job();
         }
      }
   } else { /* batch job */
      char *filename = nullptr;

      if( use_starter_method ) {
         filename = shell_path;
      } else if( pre_args_ptr[0] == argv0 ) {
         filename = shell_path;
      } else {
         filename = script_file;
      }

      /* build trace string */
      pc = err_str;
      snprintf(pc, sizeof(err_str), "execvp(%s,", filename);
      pc += strlen(pc);
      for (pstr = args; pstr && *pstr; pstr++) {
      
         /* calculate rest length in string - 15 is just lazyness for blanks,
          * "..." string, etc.
          */
         if (strlen(*pstr) < ((sizeof(err_str)  - (pc - err_str) - 15))) {
            snprintf(pc, sizeof(err_str) - (pc - err_str)," \"%s\"", *pstr);
            pc += strlen(pc);
         }
         else {
            snprintf(pc, sizeof(err_str) - (pc - err_str), " ...");
            pc += strlen(pc);
            break;
         }      
      }

      snprintf(pc, sizeof(err_str) - (pc - err_str), ")");
      shepherd_trace(err_str);

      /* Bugfix: Issuezilla 1300
       * Because this fix could break pre-existing installations, it was made
       * optional. */

      {
         if (!inherit_env()) {
            /* The closest thing to execvp that takes an environment pointer is
             * execve.  The problem is that execve does not resolve the path.
             * As there is no reasonable way to resolve the path ourselves, we
             * have to resort to this ugly hack.  Don't try this at home. */
            char **tmp = environ;

            environ = sge_get_environment();
            execvp(filename, args);
            environ = tmp;
         } else {
            execvp(filename, args);
         }

         // execvp() failed
         {
            char failed_str[2048+128];
            snprintf(failed_str, sizeof(failed_str), "%s failed: %s", err_str, strerror(errno));

            if (strcmp(childname, "prolog") == 0) {
               shepherd_state = SSTATE_PROLOG_FAILED;
            } else if (strcmp(childname, "epilog") == 0) {
               shepherd_state = SSTATE_EPILOG_FAILED;
            } else if (strcmp(childname, "pe_start") == 0) {
               shepherd_state = SSTATE_PESTART_FAILED;
            } else if (strcmp(childname, "pe_stop") == 0) {
               shepherd_state = SSTATE_PESTOP_FAILED;
            } else {
               /* most of the problems here are related to the shell i.e. -S /etc/passwd */
               shepherd_state = SSTATE_NO_SHELL;
            }

            /* EXIT HERE IN CASE IF FAILURE */
            shepherd_error(1, failed_str);
         }
      }
   }
}

int
check_configured_method(const char *method, const char *name, char *err_str, size_t err_str_size) {
   SGE_STRUCT_STAT     statbuf;
   unsigned int        file_perm = S_IXOTH;
   int                 ret = 0;
   char                *command;
   struct saved_vars_s *context = nullptr;

   /*
    * The configured method can include some pameters, e.g.
    * qlogin_daemon                /usr/sbin/in.telnetd -i
    * Only the command itself must be checked.
    */

   command = sge_strtok_r(method, " ", &context);

   if (strncmp(command, "/", 1) != 0) {
      snprintf(err_str, err_str_size, "%s \"%s\" is not an absolute path", name, command);
      ret = -1;
   } else {
      if (SGE_STAT(command, &statbuf) != 0) {
         snprintf(err_str, err_str_size, "%s \"%s\" can't be read: %s (%d)", name, command, strerror(errno), errno);
         ret = -1;
      } else {
         if (getuid() == statbuf.st_uid) {
            file_perm = file_perm | S_IXUSR;
         }
         if (getgid() == statbuf.st_gid) {
            file_perm = file_perm | S_IXGRP;
         }
         if ((statbuf.st_mode & file_perm) == 0) {
            snprintf(err_str, err_str_size, "%s \"%s\" is not executable", name, command);
            ret = -1;
         }
      }
   }

   sge_free_saved_vars(context);
   return ret;
}

char *
build_path(int type) {
   SGE_STRUCT_STAT statbuf{};
   char *path, *base;
   char *job_id, *job_name, *ja_task_id;
   const char *name;
   const char *postfix;
   int pathlen;
   char err_str[SGE_PATH_MAX+128];

   switch (type) {
   case SGE_PAR_STDOUT:
      name = "pe_stdout_path";
      postfix = "po";
      break;
   case SGE_PAR_STDERR:
      name = "pe_stderr_path";
      postfix = "pe";
      break;
   case SGE_STDOUT:
      name = "stdout_path";
      postfix = "o";
      break;
   case SGE_STDERR:
      name = "stderr_path";
      postfix = "e";
      break;
   case SGE_STDIN:
      name = "stdin_path";
      postfix = "i";
      break;
   default:
      return nullptr;
   }

   base = get_conf_val(name);

   /* Try to get information about 'base' */
   if( SGE_STAT(base, &statbuf)) {
      /* An error occurred */
      if (errno != ENOENT) {
         char *t;
         snprintf(err_str, sizeof(err_str), "can't stat() \"%s\" as %s: %s", base, name, strerror(errno));
         snprintf(err_str + strlen(err_str), sizeof(err_str) - strlen(err_str), " KRB5CCNAME=%s uid=" uid_t_fmt " gid=" uid_t_fmt " ",
                 (t=getenv("KRB5CCNAME"))?t:"none", getuid(), getgid());
         {
            gid_t groups[10];
            int i, ngid;
            ngid = getgroups(10, groups);
            for(i=0; i<ngid; i++) {
               size_t fill_size = strlen(err_str);
               snprintf(err_str + fill_size, sizeof(err_str) - fill_size, uid_t_fmt " ", groups[i]);
            }
         }
         shepherd_state = SSTATE_OPEN_OUTPUT; /* job's failure */
         shepherd_error(1, err_str);
      }
      return base; /* does not exist - must be path of file to be created */
   }

   if( !(S_ISDIR(statbuf.st_mode))) {
      return base;
   } else {
      /* 'base' is a existing directory, but not a file! */
      if (type == SGE_STDIN) {
         shepherd_state = SSTATE_OPEN_OUTPUT; /* job's failure */
         shepherd_error(1, SFQ " is a directory not a file", base);
         return (char*)"/dev/null";
      } else {
         job_name = get_conf_val("job_name");
         job_id = get_conf_val("job_id");
         ja_task_id = get_conf_val("ja_task_id");

         pathlen = strlen(base) + strlen(job_name) + strlen(job_id) + strlen(ja_task_id) + 25;
         path = sge_malloc(pathlen);
         if (path == nullptr) {
            shepherd_error(1, "malloc(%d) failed for %s@", pathlen, name);
         }

         if (atoi(ja_task_id)) {
            snprintf(path, pathlen, "%s/%s.%s%s.%s", base, job_name, postfix, job_id, ja_task_id);
         } else {
            snprintf(path, pathlen, "%s/%s.%s%s", base, job_name, postfix, job_id);
         }
      }
   }

   return path;
}

/****** Shepherd/parse_script_params() *****************************************
*  NAME
*     parse_script_params() -- Parse prolog/epilog/pe_start/pe_stop line from
*                              config
*
*  SYNOPSIS
*     static char *parse_script_params(char **script_file)
*
*  FUNCTION
*     Parses the config value for prolog/epilog/pe_start or pe_stop.
*     Retrieves the target user (as whom the script should be run) and
*     eats the target user from the config value string, so that the path of
*     the script file remains.
*
*  INPUTS
*     char **script_file - Pointer to the string containing the conf value.
*                          syntax: [<user>@]<path>, e.g. joe@/home/joe/script
*
*  OUTPUT
*     char **script_file - Pointer to the string containing the script path.
* 
*  RESULT
*     char* - If one is given, the name of the user.
*             Else nullptr.
*******************************************************************************/
static char*
parse_script_params(char **script_file) 
{
   char* target_user = nullptr;
   char* s;

   /* syntax is [<user>@]<path> <arguments> */
   s = strpbrk(*script_file, "@ ");
   if (s && *s == '@') {
      *s = '\0';
      target_user = *script_file; 
      *script_file = &s[1];
   }
   return target_user;
}

/****** Shepherd/inherit_env() *************************************************
*  NAME
*     inherit_env() -- Test whether the evironment should be inherited from the
*                       parent process or not
*
*  SYNOPSIS
*     static bool inherit_env()
*
*  FUNCTION
*     Tests the INHERIT_ENV execd param to see if the job should inherit the
*     environment of the execd that spawned this shepherd.
*
*  RESULT
*     bool - whether the environment should be inherited
* 
*  NOTES
*      MT-NOTE: inherit_env() is not MT safe
*******************************************************************************/
static bool inherit_env()
{
   if (inherit_environ == -1) {
      /* We have to use search_conf_val() instead of get_conf_val() because this
       * change is happening in a patch, and we can't break backward
       * compatibility.  In a later release, this should probably be changed to
       * use get_conf_val() instead. */
      char *inherit = search_conf_val("inherit_env");
      
      if (inherit != nullptr) {
         inherit_environ = (strcmp(inherit, "1") == 0 ? 1 : 0);
      } else {
         /* This should match the default set in sgeobj/sge_conf.c. */
         inherit_environ = true;
      }
   }
   
   return (inherit_environ == 1) ? true : false;
}

#if 0 /* Not currently used, but looks kinda useful... */
/****** Shepherd/set_inherit_env() *********************************************
*  NAME
*     set_inherit_env() -- Set whether the evironment should be inherited from
*                           the parent process or not
*
*  SYNOPSIS
*     static void set_inherit_env (bool inherit)
*
*  FUNCTION
*     Internally sets whether the job should inherit the
*     environment of the execd that spawned this shepherd.  Overrides the
*     INHERIT_ENV execd param in inherit_env()
*
*  INPUT
*     bool inherit - whether the environment should be inherited
* 
*  NOTES
*      MT-NOTE: set_inherit_env() is not MT safe
*******************************************************************************/
static void set_inherit_env (bool inherit)
{
   inherit_environ = inherit ? 1 : 0;
}
#endif

/****** builtin_starter/start_qlogin_job() *************************************
*  NAME
*     start_qlogin_job() -- starts the qlogin/qrsh (without command) job
*
*  SYNOPSIS
*     static void start_qlogin_job(const char *shell_path) 
*
*  FUNCTION
*     Prepares the argument list, the environment and starts the qlogin/qrsh
*     (without command) job, i.e. usually starts the users login shell.
*     This function doesn't return if it succeeds, because it exec's the job.
*
*  INPUTS
*     const char *shell_path - The path of the users login shell,
*                              i.e. "/bin/bash".
*
*  RESULT
*     static void - no result. If this function succeeds, it doesn't return,
*                   because it execs the job.
*
*  NOTES
*     MT-NOTE: start_qlogin_job() is not MT safe 
*
*  SEE ALSO
*     builtin_starter/start_qrsh_job
*******************************************************************************/
static void start_qlogin_job(const char *shell_path)
{
   char minusname[50];

   // generate e.g. "-bash" from the shell path "/bin/bash"
   snprintf(minusname, sizeof(minusname), "-%s", sge_basename(shell_path, '/'));

   shepherd_trace("execle(%s, %s, nullptr, env)", shell_path, minusname);
   execle(shell_path, minusname, nullptr, sge_get_environment());
}

/****** builtin_starter/start_qrsh_job() ***************************************
*  NAME
*     start_qrsh_job() -- starts the "qrsh <command>" job
*
*  SYNOPSIS
*     static void start_qrsh_job() 
*
*  FUNCTION
*     Prepares the argument list and starts the qrsh <command> job, i.e.
*     usually starts the qrsh_starter binary. If this function succeeds, it
*     doesn't return, because it execs the job. If it returns, an error occurred.
*
*  INPUTS
*     void - no input 
*
*  RESULT
*     void - no result. If this function succeeds, it doesn't return, because
*            it execs the job.
*
*  NOTES
*     MT-NOTE: start_qrsh_job() is not MT safe 
*
*  SEE ALSO
*     builtin_starter/start_qlogin_job()
*******************************************************************************/
static void start_qrsh_job()
{
   /* TODO: RFE: Make a minimal qrsh_starter for new IJS */
   const char *sge_root = nullptr;
   const char *arch = nullptr;
   char *command = nullptr;
   char *buf = nullptr;
   char *args[9];
   int  nargs;
   int  i=0;

   args[0] = nullptr;

   command = (char*)sge_get_env_value("QRSH_COMMAND");
   nargs = count_command(command);

   if (nargs > 0) {
      buf = sge_malloc(strlen(command)+2049);

      sge_root = sge_get_root_dir(0, nullptr, 0, 1);
      arch = sge_get_arch();
      if (sge_root == nullptr || arch == nullptr) {
         shepherd_trace("reading environment SGE_ROOT and ARC failed");
         return;
      }
      snprintf(buf, 2048, "%s/utilbin/%s/qrsh_starter", sge_root, arch);

      args[0] = buf;
      args[1] = shepherd_job_dir;
      if (g_noshell == 1) {
         args[2] = (char *)"noshell";
         args[3] = nullptr;
      } else {
         args[2] = nullptr;
      }
   }

   for (i=0; args[i] != nullptr; i++) {
      shepherd_trace("args[%d] = \"%s\"", i, args[i]);
   }

   shepherd_trace("execvp(%s, ...);", args[0]);
   execvp(args[0], args);
}
