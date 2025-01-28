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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>

#include "uti/msg_utilib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"

#include "sge.h"

typedef struct {
   pthread_mutex_t mutex;
   const char *log_file;
   u_long32 log_level;
   int log_as_admin_user;
   int verbose;
} log_state_t;

static log_state_t Log_State = {PTHREAD_MUTEX_INITIALIZER, TMP_ERR_FILE_SNBU, LOG_WARNING, 0, 1} ;

static void
sge_do_log(u_long32 prog_number, const char *prog_or_thread_name, int thread_id,
           const char *unqualified_hostname, int level, const char *msg) {
   if (prog_number == QMASTER || prog_number == EXECD || prog_number == SCHEDD || prog_number == SHADOWD) {
      int fd = SGE_OPEN3(log_state_get_log_file(), O_WRONLY | O_APPEND | O_CREAT, 0666);
      if (fd  >= 0) {
         // initialize static dstring
         DSTRING_STATIC(msg_dstr, 4 * MAX_STRING_SIZE);

         // write log message to dstring
         append_time(sge_get_gmt64(), &msg_dstr, false);
         const char *msg_str = sge_dstring_sprintf_append(&msg_dstr, "|%12.12s|%02d|%s|%c|%s\n", prog_or_thread_name, thread_id, unqualified_hostname, level, msg);

         // write the buffer to file
         ssize_t len = sge_dstring_strlen(&msg_dstr);
         if (write(fd, msg_str, len) != len) {
            // write to stderr if logging failed
            fprintf(stderr, "can't log to file %s: %s\n", log_state_get_log_file(), sge_strerror(errno, &msg_dstr));
         }
         close(fd);
      }
   }
}

/****** uti/log/log_state_get_log_level() ******************************************
*  NAME
*     log_state_get_log_level() -- Return log level.
*
*  SYNOPSIS
*     u_long32 log_state_get_log_level() 
*
*  FUNCTION
*     Return log level
*
*  RESULT
*     u_long32
*
******************************************************************************/
u_long32 log_state_get_log_level() {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   u_long32 level = Log_State.log_level;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   return level;
}

/****** uti/sge_log/log_state_get_log_file() ***********************************
*  NAME
*     log_state_get_log_file() -- get log file name
*
*  SYNOPSIS
*     const char* log_state_get_log_file() 
*
*  FUNCTION
*     Return name of current log file. The string returned may or may not 
*     contain a path.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     const char* - log file name (with relative or absolute path)
*
*  NOTES
*     MT-NOTE: log_state_get_log_file() is not MT safe.
*     MT-NOTE:
*     MT-NOTE: It is safe, however, to call this function from within multiple
*     MT-NOTE: threads as long as no other thread does change 'Log_File'.
*
*  BUGS
*     BUGBUG-AD: This function should use something like a barrier for
*     BUGBUG-AD: synchronization.
*
*******************************************************************************/
const char *log_state_get_log_file() {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   const char *file = Log_State.log_file;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   return file;
}

/****** uti/log/log_state_get_log_verbose() ******************************************
*  NAME
*     log_state_get_log_verbose() -- Is verbose logging enabled?
*
*  SYNOPSIS
*     int log_state_get_log_verbose() 
*
*  FUNCTION
*     Is verbose logging enabled? 
*     With verbose logging enabled not only ERROR/CRITICAL messages are 
*     printed to stderr but also WARNING/INFO.
*
*  RESULT
*     int - 0 or 1 
*
*  SEE ALSO
*     uti/log/log_state_set_log_verbose()
******************************************************************************/
int log_state_get_log_verbose() {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   int verbose = Log_State.verbose;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   return verbose;
}

/****** uti/log/log_state_set_log_level() *****************************************
*  NAME
*     log_state_set_log_level() -- Set log level to be used.
*
*  SYNOPSIS
*     void log_state_set_log_level(int i) 
*
*  FUNCTION
*     Set log level to be used.
*
*  INPUTS
*     u_long32 
*
*  SEE ALSO
*     uti/log/log_state_get_log_level() 
******************************************************************************/
void log_state_set_log_level(u_long32 theLevel) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.log_level = theLevel;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

void log_state_set_log_file(const char *theFile) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.log_file = theFile;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

/****** uti/log/log_state_set_log_verbose() *****************************************
*  NAME
*     log_state_set_log_verbose() -- Enable/disable verbose logging 
*
*  SYNOPSIS
*     void log_state_set_log_verbose(int i) 
*
*  FUNCTION
*     Enable/disable verbose logging 
*
*  INPUTS
*     int i - 0 or 1  
*
*  SEE ALSO
*     uti/log/log_state_get_log_verbose() 
******************************************************************************/
void log_state_set_log_verbose(int i) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.verbose = i;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

/****** uti/log/log_state_set_log_as_admin_user() *****************************
*  NAME
*     log_state_set_log_as_admin_user() -- Enable/Disable logging as admin user 
*
*  SYNOPSIS
*     void log_state_set_log_as_admin_user(int i)
*
*  FUNCTION
*     This function enables/disables logging as admin user. This 
*     means that the function/macros switches from start user to 
*     admin user before any messages will be written. After that 
*     they will switch back to 'start' user. 
*
*  INPUTS
*     int i - 0 or 1 
*  
*  SEE ALSO
*     uti/uidgid/sge_switch2admin_user
*     uti/uidgid/sge_switch2start_user
******************************************************************************/
void log_state_set_log_as_admin_user(int i) {
   sge_mutex_lock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
   Log_State.log_as_admin_user = i;
   sge_mutex_unlock("Log_State_Lock", __func__, __LINE__, &Log_State.mutex);
}

void
sge_log(u_long32 log_level, const char *msg, const char *file, int line) {
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
   if (log_level > MAX(log_state_get_log_level(), LOG_WARNING)) {
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
   u_long32 me = component_get_component_id();
   const char *thread_name = component_get_thread_name();
   const int thread_id = component_get_thread_id();
   const char *unqualified_hostname = component_get_unqualified_hostname();
   sge_do_log(me, thread_name, thread_id, unqualified_hostname, level_char, msg);
}

