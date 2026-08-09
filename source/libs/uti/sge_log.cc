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
 * @brief Message logging: ERROR, WARNING, INFO, DEBUG and the message file
 */

#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <fnmatch.h>
#include <pthread.h>
#include <sys/stat.h>

#include "uti/msg_utilib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sge.h"
#include "ocs_DebugParam.h"
#include "basis_types.h"

/** @brief Process wide logging configuration and open log file */
typedef struct {
   pthread_mutex_t mutex;     ///< protects every other field
   const char *log_file;      ///< path of the message file
   int log_level;             ///< highest severity that is still written
   int log_as_admin_user;     ///< write as the admin user rather than the current one
   int verbose;               ///< also echo messages to stderr
   int log_fd;                ///< persistent log file descriptor; -1 when not open
   uint64_t last_inode_check; ///< gmt64 timestamp of last rotation inode check
} log_state_t;

static log_state_t Log_State = {PTHREAD_MUTEX_INITIALIZER, TMP_ERR_FILE_SNBU, LOG_WARNING, 0, 1, -1, 0};

static void
sge_do_log(uint32_t prog_number, const char *prog_or_thread_name, int thread_id,
           const char *unqualified_hostname, int level, const char *msg) {

   // filter by thread name pattern
   if (const char *thread_name_pattern = ocs::DebugParam::get_thread_name_pattern();
       thread_name_pattern != nullptr && prog_or_thread_name != nullptr && fnmatch(thread_name_pattern, prog_or_thread_name, 0) != 0) {
      return;
   }

   if (prog_number == QMASTER || prog_number == EXECD || prog_number == SCHEDD || prog_number == SHADOWD) {
      // format the log line before acquiring the lock to minimise lock hold time
      DSTRING_STATIC(msg_dstr, 4 * MAX_STRING_SIZE);
      uint64_t now = sge_get_gmt64();
      sge_ctime64(now, &msg_dstr, false, true);
      const char *msg_str = sge_dstring_sprintf_append(&msg_dstr, "|%12.12s|%02d|%s|%c|%s\n", prog_or_thread_name, thread_id, unqualified_hostname, level, msg);
      const ssize_t len = sge_dstring_strlen(&msg_dstr);

      sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);

      // throttled inode check: detect log rotation at most once every 5 seconds
      if (Log_State.log_fd >= 0 && now - Log_State.last_inode_check > 5 * 1000000ULL) {
         Log_State.last_inode_check = now;
         struct stat st_fd{}, st_path{};
         // reopen if the path is gone (mv-based rotation, e.g. logchecker.sh) or
         // resolves to a different inode (rename+recreate rotation, e.g. logrotate)
         if (fstat(Log_State.log_fd, &st_fd) == 0 &&
             (stat(Log_State.log_file, &st_path) != 0 || st_fd.st_ino != st_path.st_ino)) {
            close(Log_State.log_fd);
            Log_State.log_fd = -1;
         }
      }

      // open once and keep open
      if (Log_State.log_fd < 0) {
         Log_State.log_fd = SGE_OPEN3(Log_State.log_file, O_WRONLY | O_APPEND | O_CREAT, 0666);
         Log_State.last_inode_check = now;
      }
      const int fd = Log_State.log_fd;

      sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);

      // write is outside the lock; O_APPEND makes individual write() calls atomic
      if (fd >= 0) {
         if (write(fd, msg_str, len) != len) {
            fprintf(stderr, "can't log to file %s: %s\n", log_state_get_log_file(), sge_strerror(errno, &msg_dstr));
         }
      }
   }
}

/**
 * @brief Return log level
 *
 * Return log level
 *
 * @return uint32_t
 */
int log_state_get_log_level() {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   int level = Log_State.log_level;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   return level;
}

/**
 * @brief Get log file name
 *
 * Return name of current log file. The string returned may or may not
 * contain a path.
 *
 * @return log file name (with relative or absolute path)
 *
 * @note MT-NOTE: log_state_get_log_file() is not MT safe.
 *       MT-NOTE:
 *       MT-NOTE: It is safe, however, to call this function from within multiple
 *       MT-NOTE: threads as long as no other thread does change 'Log_File'.
 *
 * @bug BUGBUG-AD: This function should use something like a barrier for
 *      BUGBUG-AD: synchronization.
 */
const char *log_state_get_log_file() {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   const char *file = Log_State.log_file;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   return file;
}

/**
 * @brief Is verbose logging enabled?
 *
 * Is verbose logging enabled?
 * With verbose logging enabled not only ERROR/CRITICAL messages are
 * printed to stderr but also WARNING/INFO.
 *
 * @return 0 or 1
 *
 * @see #log_state_set_log_verbose
 */
int log_state_get_log_verbose() {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   int verbose = Log_State.verbose;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   return verbose;
}

/**
 * @brief Set log level to be used
 *
 * Set log level to be used.
 *
 * @param theLevel highest severity that should still be written
 *
 * @see #log_state_get_log_level
 */
void log_state_set_log_level(uint32_t theLevel) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.log_level = theLevel;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

/** @brief Set the file log messages are written to
 * @param file path of the message file
 */
void log_state_set_log_file(const char *file) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   // close the persistent fd so it is re-opened against the new path on next use
   if (Log_State.log_fd >= 0) {
      close(Log_State.log_fd);
      Log_State.log_fd = -1;
   }
   Log_State.log_file = file;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

/**
 * @brief Enable/disable verbose logging
 *
 * Enable/disable verbose logging
 *
 * @param i 0 or 1
 *
 * @see #log_state_get_log_verbose
 */
void log_state_set_log_verbose(int i) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.verbose = i;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

/**
 * @brief Enable/Disable logging as admin user
 *
 * This function enables/disables logging as admin user. This
 * means that the function/macros switches from start user to
 * admin user before any messages will be written. After that
 * they will switch back to 'start' user.
 *
 * @param i 0 or 1
 */
void log_state_set_log_as_admin_user(int i) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.log_as_admin_user = i;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

/** @brief Write one message to the message file and, when verbose, to stderr
 *
 * Called by the logging macros; use those rather than this directly.
 *
 * @param log_level severity, one of the `LOG_*` values
 * @param msg the message text
 * @param file source file the message originates from
 * @param line source line the message originates from
 */
void
sge_log(int log_level, const char *msg, const char *file, int line) {
   DENTER_(BASIS_LAYER);
   char buf[128 * 4];

   if (msg == nullptr) {
      snprintf(buf, sizeof(buf), MSG_LOG_CALLEDLOGGINGSTRING_S, MSG_POINTER_NULL);
      msg = buf;
   } else if (msg[0] == '\0') {
      // nothing to do if string is empty
      return;
   }

   DPRINTF("%s %d %s\n", file, line, msg);

   /* quick exit if nothing to log */
   if (log_level > std::max(log_state_get_log_level(), LOG_WARNING)) {
      return;
   }

   int level_char;
   char level_string[32 * 4];
   switch (log_level) {
      case LOG_PROF:
         strcpy(level_string, MSG_LOG_PROFILING);
         level_char = 'P';
         break;
      case LOG_CRIT:
         strcpy(level_string, MSG_LOG_CRITICALERROR);
         level_char = 'C';
         break;
      case LOG_ERR:
         strcpy(level_string, MSG_LOG_ERROR);
         level_char = 'E';
         break;
      case LOG_WARNING:
         strcpy(level_string, "");
         level_char = 'W';
         break;
      case LOG_NOTICE:
         strcpy(level_string, "");
         level_char = 'N';
         break;
      case LOG_INFO:
         strcpy(level_string, "");
         level_char = 'I';
         break;
      case LOG_DEBUG:
         strcpy(level_string, "");
         level_char = 'D';
         break;
      default:
         strcpy(level_string, "");
         level_char = 'L';
         break;
   }

   // avoid double output in debug mode
   bool is_daemonized = component_is_daemonized();
   if (!is_daemonized &&
       !rmon_condition(TOP_LAYER, INFOPRINT) && (log_state_get_log_verbose() || log_level <= LOG_WARNING)) {
      fprintf(stderr, "%s%s\n", level_string, msg);
   }

   // log into the log file
   uint32_t me = component_get_component_id();
   const char *thread_name = component_get_thread_name();
   const int thread_id = component_get_thread_id();
   const char *unqualified_hostname = component_get_unqualified_hostname();
   sge_do_log(me, thread_name, thread_id, unqualified_hostname, level_char, msg);
}

