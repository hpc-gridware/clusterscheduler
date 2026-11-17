
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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Running the site's own load sensors and reading their output
 */
#include <fcntl.h>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

#include "uti/sge.h"
#include "uti/sge_arch.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/cull/sge_loadsensor_LS_L.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_report.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_LR.h"
#include "sgeobj/tom/ocs_tom_LS.h"

#include "execd.h"
#include "sge_load_sensor.h"
#include "sge_report_execd.h"
#include "msg_execd.h"

static int ls_send_command(ocs::tom::ConstElement<ocs::tom::LS> this_ls, const char *command);
static pid_t sge_ls_get_pid(ocs::tom::ConstElement<ocs::tom::LS> this_ls);
static void sge_ls_set_pid(ocs::tom::Element<ocs::tom::LS> this_ls, pid_t pid);
static int sge_ls_status(ocs::tom::ConstElement<ocs::tom::LS> this_ls);
static ocs::tom::ElementPtr<ocs::tom::LS>
sge_ls_create_ls(const char *qualified_hostname, const char *name, const char *scriptfile);
static int sge_ls_start_ls(const char *qualified_hostname, ocs::tom::Element<ocs::tom::LS> this_ls);
static void sge_ls_stop_ls(ocs::tom::Element<ocs::tom::LS> this_ls, int send_no_quit_command);
static int sge_ls_start(const char* qualified_hostname, const char *binary_path, char *scriptfile);
static bool sge_ls_expand_variables(const char *qualified_hostname, const char *scriptfile,
                                    std::string &expanded);

static int read_ls();

/* 
 * time load sensors get to quit cleanly before they get a SIGKILL 
 */
/** @brief Seconds to wait for a load sensor to exit before killing it */
#define LS_QUIT_TIMEOUT (10)

/* 
 * Each element in this list contains elements which show the state
 * of the corresponding load sensor; it lives as long as the process,
 * so it stays a raw pointer and is read through checked handles
 */
static lList *ls_list = nullptr;   /* LS_Type */

/* 
 * should we start the qidle command 
 */
static int has_to_use_qidle = 0;

/* 
 * should we start the (GNU)-load sensor with 
 */
static int has_to_use_gnu_load_sensor = 0;


/**
 * @brief Get pid of a loadsensor
 *
 * Returns the pid which is stored in an CULL element of
 * the type LS_Type. If the corresponding loadsensor was
 * not started until now then -1 will be returned.
 *
 * @param this_ls the load sensor
 *
 * @return returns pid
 */
static pid_t sge_ls_get_pid(const ocs::tom::ConstElement<ocs::tom::LS> this_ls) {
   DENTER(TOP_LAYER);
   pid_t pid = -1;
   const char *pid_string;

   pid_string = ocs::tom::get<LS_pid>(this_ls);
   if (pid_string) {
      sscanf(pid_string, pid_t_fmt, &pid);
   }
   DRETURN(pid);
}

/**
 * @brief Set pid in loadsensor element
 *
 * Set the pid entry in a CULL element of the type LS_Type.
 *
 * @param this_ls the load sensor
 * @param pid pid of the loadsensor process or -1
 *
 * @note [this_ls] - LS_pid entry of the CULL element will be modified
 */
static void sge_ls_set_pid(const ocs::tom::Element<ocs::tom::LS> this_ls, const pid_t pid) {
   DENTER(TOP_LAYER);
   char pid_buffer[256];

   snprintf(pid_buffer, sizeof(pid_buffer), pid_t_fmt, pid);
   ocs::tom::set<LS_pid>(this_ls, pid_buffer);
   DRETURN_VOID;
}

/**
 * @brief Returns the status of a loadsensor
 *
 * The functions detects the status of a londsensor
 * and returns the corresponding integer value.
 * Following values are possible:
 *    LS_OK              - the ls waits for commands
 *    LS_NOT_STARTED     - load sensor not started
 *    LS_BROKEN_PIPE     - ls has exited or is not ready to read
 *
 * @param this_ls the load sensor
 *
 * @return returns the status of the loadsensor
 */
static int sge_ls_status(const ocs::tom::ConstElement<ocs::tom::LS> this_ls) {
   DENTER(TOP_LAYER);

   fd_set writefds;
   int ret;
   int higest_fd;

   if (sge_ls_get_pid(this_ls) == -1) {
      DRETURN(LS_NOT_STARTED);
   }

   /* build writefds */
   FD_ZERO(&writefds);
   higest_fd = fileno((FILE *) ocs::tom::get<LS_in>(this_ls));
   FD_SET(higest_fd, &writefds);

   /* is load sensor ready to read ? */
   ret = select(higest_fd + 1, nullptr, &writefds, nullptr, nullptr);

   if (ret <= 0) {
      DRETURN(LS_BROKEN_PIPE);
   }

   DRETURN(LS_OK);
}

/**
 * @brief Starts a loadsensor
 *
 * An additional loadsensor process will be started. The name
 * of the script has to be stored in the LS_command entry of
 * 'this_ls' before this function will be called.
 * The process environment of the loadsensor will contain
 * the HOST variable. This variable containes the hostname
 * of the execution daemon which calls this function.
 * If 'this_ls' correlates to the 'qidle'-loadsensor then
 * also the XAUTHORITY environment variable will be set.
 *
 * @param qualified_hostname qualified host name
 * @param this_ls the load sensor
 *
 * An additional load sensor process is started; this_ls is modified: LS_pid contains the pid of the
 * process, LS_in, LS_out and LS_err the streams for the communication with it.
 *
 * @return LS_OK on success, LS_CANT_PEOPEN if sge_peopen() fails
 */
static int sge_ls_start_ls(const char *qualified_hostname, const ocs::tom::Element<ocs::tom::LS> this_ls) {
   DENTER(TOP_LAYER);

   pid_t pid = -1;
   FILE *fp_in = nullptr;
   FILE *fp_out = nullptr;
   FILE *fp_err = nullptr;
   char **envp = nullptr;
   DSTRING_STATIC(dstr_host, CL_MAXHOSTNAMELEN);
   DSTRING_STATIC(dstr_spool, SGE_PATH_MAX);

   const char *str_host = sge_dstring_sprintf(&dstr_host, "%s=%s", "HOST", qualified_hostname);
   const char *str_spool = sge_dstring_sprintf(&dstr_spool, "%s=%s/%s", "SGE_JOB_SPOOL_DIR", execd_spool_dir, ACTIVE_DIR);

   // If we are starting the qidle load sensor, we need to export XAUTHORITY.
   // @todo Does anything speak against exporting it always?
   //       We do not really want special handling for one specific load sensor.
   if (has_to_use_qidle && !strcmp(ocs::tom::get<LS_name>(this_ls), IDLE_LOADSENSOR_NAME)) {
      envp = (char **) sge_malloc(sizeof(char *) * 4);
      envp[0] = (char *)str_host;
      envp[1] = (char *)str_spool;
      envp[2] = (char *)"XAUTHORITY=/tmp/.xauthority";
      envp[3] = nullptr;
   } else {
      envp = (char **) sge_malloc(sizeof(char *) * 3);
      envp[0] = (char *)str_host;
      envp[1] = (char *)str_spool;
      envp[2] = nullptr;
   }

   /* we need fds for select() .. */
   pid = sge_peopen("/bin/sh", 0, ocs::tom::get<LS_command>(this_ls), nullptr, envp, &fp_in, &fp_out, &fp_err, true);

   if (envp) {
      sge_free(&envp);
   }
   if (pid == -1) {
      // There is no load sensor process and therefore there are no streams. Whatever the
      // previous run of this sensor left in the refs has been closed by sge_ls_stop_ls().
      ocs::tom::set<LS_in>(this_ls, nullptr);
      ocs::tom::set<LS_out>(this_ls, nullptr);
      ocs::tom::set<LS_err>(this_ls, nullptr);
      DRETURN(LS_CANT_PEOPEN);
   }
   /* we need load reports non blocking */
   fcntl(fileno(fp_out), F_SETFL, O_NONBLOCK);

   sge_ls_set_pid(this_ls, pid);
   ocs::tom::set<LS_in>(this_ls, fp_in);
   ocs::tom::set<LS_out>(this_ls, fp_out);
   ocs::tom::set<LS_err>(this_ls, fp_err);

   DPRINTF("%s: successfully started load sensor \"%s\"\n", __func__, ocs::tom::get<LS_command>(this_ls));

   /* request first load report after starting */
   ls_send_command(this_ls, "\n");

   DRETURN(LS_OK);
}

/**
 * @brief Creates a new CULL loadsensor element
 *
 * The function creates a new element of type LS_Type and
 * returns it. The loadsensor will be
 * started immediately.
 * If it cannot be started then, LS_has_to_restart is set to
 * true so that it will be attempted to be restarted in the next load interval
 *
 * @param qualified_hostname qualified host name
 * @param name pseudo name of the ls "extern" for user defined loadsensors "intern" for qidle and qloadsensor
 * @param scriptfile absolute path to the ls scriptfile
 *
 * @return the new load sensor, owned by the caller, with its process started or due for a restart; an empty
 *         handle if there is no scriptfile or it cannot be stat()ed
 */
static ocs::tom::ElementPtr<ocs::tom::LS>
sge_ls_create_ls(const char *qualified_hostname, const char *name, const char *scriptfile) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::LS> new_ls;
   SGE_STRUCT_STAT st;

   if (scriptfile != nullptr) {
      if (SGE_STAT(scriptfile, &st) != 0) {
         if (strcmp(name, "extern") == 0) {
            WARNING(MSG_LS_NOMODTIME_SS, scriptfile, strerror(errno));
         }
         DRETURN(nullptr);
      }

      new_ls = ocs::tom::ElementPtr<ocs::tom::LS>::create();
      if (new_ls) {
         /* initialize all attributes */
         ocs::tom::set<LS_name>(new_ls, name);
         ocs::tom::set<LS_command>(new_ls, scriptfile);
         sge_ls_set_pid(*new_ls, -1);
         ocs::tom::set<LS_in>(new_ls, nullptr);
         ocs::tom::set<LS_out>(new_ls, nullptr);
         ocs::tom::set<LS_err>(new_ls, nullptr);
         ocs::tom::set<LS_has_to_restart>(new_ls, false);
         ocs::tom::set<LS_tag>(new_ls, 0);
         ocs::tom::set<LS_incomplete>(new_ls, ocs::tom::ListPtr<ocs::tom::LR>::create(""));
         ocs::tom::set<LS_complete>(new_ls, ocs::tom::ListPtr<ocs::tom::LR>::create(""));
         ocs::tom::set<LS_last_mod>(new_ls, st.st_mtime);

         /* start loadsensor, if couldn't set the restart flag so that we
          * restart it in the next load interval
          */
         if (sge_ls_start_ls(qualified_hostname, *new_ls) != LS_OK) {
            ocs::tom::set<LS_has_to_restart>(new_ls, true);
         }
      }
   }
   DRETURN(new_ls);
}

/**
 * @brief Stop a loadsensor process
 *
 * The "quit" command will be send to the loadsensor process.
 * So the loadsensor process can stop itself.
 *
 * @param this_ls the load sensor
 * @param send_no_quit_command 0 sends the quit command, 1 sends none (kill without notification)
 *
 * @note the loadsensor process will be terminated [this_ls] the entries will be reinitialized
 */
static void sge_ls_stop_ls(const ocs::tom::Element<ocs::tom::LS> this_ls, const int send_no_quit_command) {
   DENTER(TOP_LAYER);

   int ret, exit_status;
   struct timeval t;

   if (sge_ls_get_pid(this_ls) == -1) {
      DRETURN_VOID;
   }

   if (!send_no_quit_command) {
      ls_send_command(this_ls, "quit\n");
      ret = sge_ls_status(this_ls);
   } else {
      ret = LS_BROKEN_PIPE;
   }

   memset(&t, 0, sizeof(t));
   if (ret == LS_OK) {
      t.tv_sec = LS_QUIT_TIMEOUT;
   } else {
      t.tv_sec = 0;
   }

   /* close all fds to load sensor */
   if (ret != LS_NOT_STARTED) {
      exit_status = sge_peclose(sge_ls_get_pid(this_ls), (FILE *)ocs::tom::get<LS_in>(this_ls),
                            (FILE *)ocs::tom::get<LS_out>(this_ls), (FILE *)ocs::tom::get<LS_err>(this_ls),
                            (t.tv_sec ? &t : nullptr));
      DPRINTF("%s: load sensor `%s` stopped, exit status from sge_peclose= %d\n",
              __func__, ocs::tom::get<LS_command>(this_ls), exit_status);

      // sge_peclose() has fclose()d the three streams. Their descriptor numbers are free for
      // the process to hand out again, so nothing may be left pointing at them.
      ocs::tom::set<LS_in>(this_ls, nullptr);
      ocs::tom::set<LS_out>(this_ls, nullptr);
      ocs::tom::set<LS_err>(this_ls, nullptr);
   }

   sge_ls_set_pid(this_ls, -1);
   DRETURN_VOID;
}

/**
 * @brief Read sensor output and add it to load report
 *
 * This function loops over all loadsensor elements in
 * the ls_list (LS_Type). It tries to read from the
 * output stream (LS_out). The output will be parsed
 * and stored in the LS_incomplete entry (LR_Type).
 * If the protocol part of the loadsensor is correct
 * then the entries of LS_incomplete will be moved
 * LS_complete.
 * The last complete set of load values (LS_complete)
 * will be added to the load report.
 *
 * @return 0; LS_incomplete and LS_complete of the load sensors are modified.
 */
static int read_ls() {
   DENTER(TOP_LAYER);
   char input[10000];
   char host[1000];
   char name[1000];
   char value[1000];
   bool flag = true;

   for (const auto ls_elem : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      if (sge_ls_get_pid(ls_elem) == -1) {
         continue;
      }

      FILE *file = (FILE *)ocs::tom::get<LS_out>(ls_elem);

      DPRINTF("receiving from %s\n", ocs::tom::get<LS_command>(ls_elem));

      while (flag) {
         if (fscanf(file, "%[^\n]\n", input) != 1) {
            break;
         }
         DPRINTF("received: >>%s<<\n", input);

         if (!strcmp(input, "begin") || !strcmp(input, "start")) {
            /* remove last possibly incomplete load report */
            ocs::tom::set<LS_incomplete>(ls_elem, ocs::tom::ListPtr<ocs::tom::LR>::create(""));
            continue;
         }

         if (!strcmp(input, "end")) {
            /* replace old load report by new one; LS_incomplete is left nullptr, the old one is freed */
            auto tmp_list = ls_elem.exchange<LS_incomplete>(nullptr);
            tmp_list = ls_elem.exchange<LS_complete>(std::move(tmp_list));

            /* request next load report from ls */
            ls_send_command(ls_elem, "\n");
            break;
         }

         /* add a newline for pattern matching in sscanf */
         strcat(input, "\n");
         if (sscanf(input, "%[^:]:%[^:]:%[^\n]", host, name, value) != 3) {
            DPRINTF("format error in line: \"%100s\"\n", input);
            ERROR(MSG_LS_FORMAT_ERROR_SS, ocs::tom::get<LS_command>(ls_elem), input);
         } else {
            // after "end" and before the next "begin" there is no incomplete report: the value is dropped, as it
            // was before when it went into a list created on the fly and lost
            sge_add_str2load_report(ocs::tom::get<LS_incomplete>(ls_elem), name, value, host);
         }
      }
   }

   DRETURN(0);
}

/**
 * @brief Send a command to a loadsensor
 *
 * This function will send a command through the input
 * stream (LS_in) to the loadsensor.
 *
 * @param this_ls the load sensor
 * @param command valid loadsensor command
 *
 * @return 0 on success, -1 on error
 */
static int ls_send_command(const ocs::tom::ConstElement<ocs::tom::LS> this_ls, const char *command) {
   DENTER(TOP_LAYER);

   // A sensor which has been stopped has no streams any more. fileno() on a closed stream
   // reads freed memory, and the descriptor number it used to hold may belong to someone
   // else by now, so refuse here instead of trusting the caller to have checked LS_pid.
   FILE *file = (FILE *)ocs::tom::get<LS_in>(this_ls);
   if (file == nullptr) {
      DPRINTF("load sensor is not running, cannot send command\n");
      DRETURN(-1);
   }

   fd_set writefds;
   FD_ZERO(&writefds);
   int higest_fd = fileno(file);
   FD_SET(higest_fd, &writefds);

   struct timeval timeleft;
   timeleft.tv_sec = 0;
   timeleft.tv_usec = 0;

   /* wait for writing on fd_in */
   int ret = select(higest_fd + 1, nullptr, &writefds, nullptr, &timeleft);
   if (ret == -1) {
      switch (errno) {
      case EINTR:
         DPRINTF("select failed with EINTR\n");
         WARNING("[load_sensor %s] select failed with EINTR", ocs::tom::get<LS_pid>(this_ls));
         break;
      case EBADF:
         DPRINTF("select failed with EBADF\n");
         WARNING("[load_sensor %s] select failed with EBADF", ocs::tom::get<LS_pid>(this_ls));
         break;
      case EINVAL:
         DPRINTF("select failed with EINVAL\n");
         WARNING("[load_sensor %s] select failed with EINVAL", ocs::tom::get<LS_pid>(this_ls));
         break;
      default:
         DPRINTF("select failed with unexpected errno %d", errno);
         WARNING("[load_sensor %s] select failed with [%s]", ocs::tom::get<LS_pid>(this_ls), strerror(errno));
      }
      DRETURN(-1);
   }

   if (!FD_ISSET(higest_fd, &writefds)) {
      DPRINTF("received: cannot read\n");
      WARNING("[load_sensor %s] received: cannot read", ocs::tom::get<LS_pid>(this_ls));
      DRETURN(-1);
   }

   /* send command to load sensor */
   if (fprintf(file, "%s", command) != (int)strlen(command)) {
      WARNING("[load_sensor %s] couldn't send command [%s]", ocs::tom::get<LS_pid>(this_ls), strerror(errno));
      DRETURN(-1);
   }
   if (fflush(file) != 0) {
      WARNING("[load_sensor %s] fflush failed [%s]", ocs::tom::get<LS_pid>(this_ls), strerror(errno));
      DRETURN(-1);
   }

   DRETURN(0);
}

/**
 * @brief Enable or disable the qidle load sensor
 *
 * @param qidle 0 disables it, non-zero enables it
 */
void sge_ls_qidle(int qidle) {
   has_to_use_qidle = qidle;
}

/**
 * @brief Enable or disable the GNU load sensor
 *
 * @param gnu_ls 0 disables it, non-zero enables it
 */
void sge_ls_gnu_ls(int gnu_ls) {
   has_to_use_gnu_load_sensor = gnu_ls;
}

/*!
 * @brief Expand the special variables in the path of a load sensor.
 *
 * The path of a load sensor in the load_sensor parameter of the configuration may contain the
 * special variables $sge_root, $sge_cell and $host. qmaster accepts no other variables.
 *
 * @param qualified_hostname The name of this host, the value of $host.
 * @param scriptfile The path of the load sensor as configured.
 * @param expanded Receives the path with the variables replaced by their values.
 * @return true if the path was expanded, false if it contains an unknown variable.
 */
static bool
sge_ls_expand_variables(const char *qualified_hostname, const char *scriptfile,
                        std::string &expanded) {
   DENTER(TOP_LAYER);

   expanded.clear();
   for (const char *sp = scriptfile; *sp != '\0';) {
      if (*sp != '$') {
         expanded += *sp++;
         continue;
      }

      // the variable name consists of alphanumeric characters and '_', see replace_params()
      const char *name_start = ++sp;
      while (isalnum(static_cast<unsigned char>(*sp)) || *sp == '_') {
         sp++;
      }
      std::string name(name_start, sp - name_start);

      const char *value = nullptr;
      if (name == "sge_root") {
         value = bootstrap_get_sge_root();
      } else if (name == "sge_cell") {
         value = bootstrap_get_sge_cell();
      } else if (name == "host") {
         value = qualified_hostname;
      }
      if (value == nullptr) {
         WARNING(MSG_LS_UNKNOWNVARIABLE_SS, scriptfile, name.c_str());
         DRETURN(false);
      }
      expanded += value;
   }

   DRETURN(true);
}

/**
 * @brief Start/stop/restart loadsensors
 *
 * The 'scriptfiles' parameter will be parsed. Each
 * loadsensor not contained in the global list
 * 'ls_list' (LS_Type) will be added and the process
 * will be started.
 * Loadsensors wich are contained in the global list
 * but not in 'scriptfiles' will be stopped and
 * removed.
 * Depending on global variables additional internal
 * loadsensors will be started:
 *  'has_to_use_gnu_load_sensor' == 1  => start qloadsensor
 *  'has_to_use_qidle' == 1            => start qidle
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param binary_path where to find the load sensor binaries
 * @param scriptfiles comma separated list of scriptfiles
 *
 * @return LS_OK, LS_NOT_STARTED if the list of scriptfiles is too long
 */
static int sge_ls_start(const char *qualified_hostname, const char *binary_path, char *scriptfiles) {
   DENTER(TOP_LAYER);

   char scriptfiles_buffer[MAX_STRING_SIZE];
   SGE_STRUCT_STAT stat_buffer;

   /* tag all elements */
   for (const auto ls_elem : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      ocs::tom::set<LS_tag>(ls_elem, 1);
   }

   /* add / remove load sensors */
   if ((scriptfiles != nullptr) && (strcasecmp(scriptfiles, "NONE") != 0)) {
      char *scriptfile = nullptr;

       if (strlen(scriptfiles) > MAX_STRING_SIZE - 1) {
          DRETURN(LS_NOT_STARTED);
       }
   
      strcpy(scriptfiles_buffer, scriptfiles);
      /* add new load sensors if necessary 
       * and remove tags from the existing load sensors 
       * contained in 'scriptfiles' */
      scriptfile = strtok(scriptfiles_buffer, ",\n");
      while (scriptfile) {
         // the load sensor is known by its path with the special variables expanded
         std::string expanded;
         if (!sge_ls_expand_variables(qualified_hostname, scriptfile, expanded)) {
            scriptfile = strtok(nullptr, ",\n");
            continue;
         }
         auto ls_elem = ocs::tom::List<ocs::tom::LS>(ls_list).find_first<LS_command>(expanded.c_str());

         if (!ls_elem) {
            INFO(MSG_LS_STARTLS_S, expanded.c_str());
            auto new_ls = sge_ls_create_ls(qualified_hostname, "extern", expanded.c_str());

            if (ls_list == nullptr) {
               ls_list = ocs::tom::ListPtr<ocs::tom::LS>::create("").release();
            }
            if (new_ls) {
               ls_elem = ocs::tom::List<ocs::tom::LS>(ls_list).push_back(std::move(new_ls));
            }
         }
         if (ls_elem) {
            ocs::tom::set<LS_tag>(ls_elem, 0);
         }
         scriptfile = strtok(nullptr, ",\n");
      }
   }

   /* QIDLE loadsensor */
   if (has_to_use_qidle) {
      snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s/%s",
               binary_path, sge_get_arch(),
               IDLE_LOADSENSOR_NAME);
      
      if (SGE_STAT(scriptfiles_buffer, &stat_buffer) != 0) {
         snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s",
                  binary_path, IDLE_LOADSENSOR_NAME);
      }
      
      auto ls_elem = ocs::tom::List<ocs::tom::LS>(ls_list).find_first<LS_command>(scriptfiles_buffer);
      if (!ls_elem) {
         auto new_ls = sge_ls_create_ls(qualified_hostname, IDLE_LOADSENSOR_NAME, scriptfiles_buffer);

         if (ls_list == nullptr) {
            ls_list = ocs::tom::ListPtr<ocs::tom::LS>::create("").release();
         }
         if (new_ls) {
            ls_elem = ocs::tom::List<ocs::tom::LS>(ls_list).push_back(std::move(new_ls));
         }
      }
      if (ls_elem) {
         ocs::tom::set<LS_tag>(ls_elem, 0);
      }
   }

   /* GNU loadsensor */
   if (has_to_use_gnu_load_sensor) {
      snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s/%s",
               binary_path, sge_get_arch(),
               GNU_LOADSENSOR_NAME);
      
      if (SGE_STAT(scriptfiles_buffer, &stat_buffer) != 0) {
         snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s",
                  binary_path, GNU_LOADSENSOR_NAME);
      }
      
      auto ls_elem = ocs::tom::List<ocs::tom::LS>(ls_list).find_first<LS_command>(scriptfiles_buffer);
      if (!ls_elem) {
         auto new_ls = sge_ls_create_ls(qualified_hostname, GNU_LOADSENSOR_NAME, scriptfiles_buffer);

         if (ls_list == nullptr) {
            ls_list = ocs::tom::ListPtr<ocs::tom::LS>::create("").release();
         }
         if (new_ls) {
            ls_elem = ocs::tom::List<ocs::tom::LS>(ls_list).push_back(std::move(new_ls));
         }
      }
      if (ls_elem) {
         ocs::tom::set<LS_tag>(ls_elem, 0);
      }
   }

   /* tagged elements are not contained in 'scriptfiles'
    * => we will remove them */
   const ocs::tom::List<ocs::tom::LS> ls_elems(ls_list);
   for (auto ls_elem = ls_elems.front(); ls_elem;) {
      const auto nxt_ls_elem = ls_elem.next();
      if (ocs::tom::get<LS_tag>(ls_elem) == 1) {
         INFO(MSG_LS_STOPLS_S, ocs::tom::get<LS_command>(ls_elem));
         sge_ls_stop_ls(ls_elem, 0);
         ls_elems.erase(ls_elem);
      }
      ls_elem = nxt_ls_elem;
   }

   DRETURN(LS_OK);
}

/**
 * @brief Restart loadsensors
 *
 * Trigger the restart of all loadsensors
 */
void trigger_ls_restart() {
   DENTER(TOP_LAYER);

   for (const auto ls : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      ocs::tom::set<LS_has_to_restart>(ls, true);
   }

   DRETURN_VOID;
}

/**
 * @brief Restart loadsensor with given pid
 *
 * If the given pid is the pid of a loadsensor we started
 * previously, then we will trigger a restart. This is
 * necessary when a loadsensor process dies horribly.
 * The execd notifies this module by invoking this function.
 *
 * @param pid process id
 *
 * @return 0 if pid was not a load sensor, 1 if it was one and its restart was triggered
 */
int sge_ls_stop_if_pid(const pid_t pid) {
   DENTER(TOP_LAYER);

   for (const auto ls : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      if (pid == sge_ls_get_pid(ls)) {
         trigger_ls_restart();
         DRETURN(1);
      }
   }

   DRETURN(0);
}

/**
 * @brief Request a load report
 *
 * This functions starts/stops/restarts all loadsensors
 * contained in the global variable 'conf.load_sensor'.
 * The restart of a loadsensor process will be triggered
 * when the modification time of the scriptfile changed.
 * After that it collects load values by reading the
 * output of each loadsensor process. The last complete
 * list of load values will be added into the given load
 * report list 'lpp'.
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param binary_path where to find the load sensor binaries
 * @param[in,out] lpp the load report the last complete load values of the started loadsensors are added to;
 *        created if it is empty
 *
 * @return OK
 */
int sge_ls_get(const char *qualified_hostname, const char *binary_path, ocs::tom::ListPtr<ocs::tom::LR> &lpp) {
   DENTER(TOP_LAYER);
   char* load_sensor = nullptr;

   load_sensor = mconf_get_load_sensor();
   sge_ls_start(qualified_hostname, binary_path, load_sensor);

   for (const auto ls_elem : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      bool restart = false;
      SGE_STRUCT_STAT st;
      const char *ls_command;
      const char *ls_name;

      ls_command = ocs::tom::get<LS_command>(ls_elem);
      ls_name = ocs::tom::get<LS_name>(ls_elem);

      /* someone triggered the restart */
      if (ocs::tom::get<LS_has_to_restart>(ls_elem)) {
         restart = true;
      }

      /* the modification time of the ls script changed */
      if (!restart) {
         if (ls_command && SGE_STAT(ls_command, &st)) {
            if (!strcmp(GNU_LOADSENSOR_NAME, ls_name) ||
                !strcmp(IDLE_LOADSENSOR_NAME, ls_name)) {
               WARNING(MSG_LS_NOMODTIME_SS, ls_command, strerror(errno));
            }
            continue;
         }
         if ((time_t)ocs::tom::get<LS_last_mod>(ls_elem) != st.st_mtime) {
            restart = true;
            ocs::tom::set<LS_last_mod>(ls_elem, st.st_mtime);
         }
      }

      if (restart) {
         INFO(MSG_LS_RESTARTLS_S, ls_command ? ls_command : "");
         sge_ls_stop_ls(ls_elem, 0);
         /* start the load sensor script, set the restart flag if the load
          * sensor didn't start so that we try to start it again in the next
          * load interval! 
          */
         if (sge_ls_start_ls(qualified_hostname, ls_elem) == LS_OK) {
            ocs::tom::set<LS_has_to_restart>(ls_elem, false);
         }
      }
   }

   read_ls();

   for (const auto ls_elem : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      /* merge external load into existing load report list */
      for (const auto ep : ocs::tom::get<LS_complete>(ls_elem)) {
         sge_add_str2load_report(lpp, ocs::tom::get<LR_name>(ep),
                                 ocs::tom::get<LR_value>(ep),
                                 ocs::tom::get<LR_host>(ep));
      }
   }

   sge_free(&load_sensor);

   DRETURN(0);
}

/**
 * @brief Stop all loadsensor
 *
 * Stop all loadsensors and destroy the complete
 * 'ls_list'.
 *
 * @param exited whether the load sensors have already exited: 0 means notify
 *        them first, 1 means do not - which avoids communicating with a sensor
 *        that is already gone
 */
void sge_ls_stop(int exited) {
   DENTER(TOP_LAYER);

   for (const auto ls_elem : ocs::tom::List<ocs::tom::LS>(ls_list)) {
      sge_ls_stop_ls(ls_elem, exited);
   }
   // the process-lifetime list is given up here
   ocs::tom::ListPtr<ocs::tom::LS>::from_typed(std::exchange(ls_list, nullptr)).reset();

   DRETURN_VOID;
}
